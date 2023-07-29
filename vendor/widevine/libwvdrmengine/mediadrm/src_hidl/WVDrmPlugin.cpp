//
// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//

//#define LOG_NDEBUG 0
#define LOG_TAG "WVCdm"
#include <utils/Log.h>

#include <list>
#include <stdlib.h>

#include "WVDrmPlugin.h"

#include "android-base/macros.h"
#include "hidl_metrics_adapter.h"
#include "mapErrors-inl.h"
#include "media/stagefright/MediaErrors.h"
#include "metrics.pb.h"
#include "openssl/sha.h"
#include "wv_cdm_constants.h"
#include "TypeConvert.h"
#include "HidlTypes.h"

namespace {

static const char* const kResetSecurityLevel = "";
static const char* const kEnable = "enable";
static const char* const kDisable = "disable";
static const std::string kPsshTag = "pssh";
static const char* const kSpecialUnprovisionResponse = "unprovision";

}   // namespace

namespace wvdrm {
namespace hardware {
namespace drm {
namespace V1_2 {
namespace widevine {

using android::hardware::drm::V1_2::widevine::toHidlVec;
using android::hardware::drm::V1_2::widevine::toVector;
using wvcdm::kDefaultCdmIdentifier;
using wvcdm::CdmAppParameterMap;
using wvcdm::CdmCertificateType;
using wvcdm::CdmInitData;
using wvcdm::CdmKeyStatus;
using wvcdm::CdmKeyRequest;
using wvcdm::CdmKeyRequestType;
using wvcdm::CdmKeyResponse;
using wvcdm::CdmKeySetId;
using wvcdm::CdmLicenseType;
using wvcdm::CdmProvisioningRequest;
using wvcdm::CdmProvisioningResponse;
using wvcdm::CdmQueryMap;
using wvcdm::CdmSecureStopId;
using wvcdm::CdmSecurityLevel;
using wvcdm::CdmUsageInfo;
using wvcdm::CdmUsageInfoReleaseMessage;
using wvcdm::KeyId;

namespace {

std::vector<uint8_t> StrToVector(const std::string& str) {
  std::vector<uint8_t> vec(str.begin(), str.end());
  return vec;
}

KeyRequestType ConvertFromCdmKeyRequestType(
    CdmKeyRequestType keyRequestType) {
  switch (keyRequestType) {
    case wvcdm::kKeyRequestTypeInitial:
      return KeyRequestType::INITIAL;
    case wvcdm::kKeyRequestTypeRenewal:
      return KeyRequestType::RENEWAL;
    case wvcdm::kKeyRequestTypeRelease:
      return KeyRequestType::RELEASE;
    default:
      return KeyRequestType::UNKNOWN;
  }
}

KeyRequestType_V1_1 ConvertFromCdmKeyRequestType_1_1(
    CdmKeyRequestType keyRequestType) {
  switch (keyRequestType) {
    case wvcdm::kKeyRequestTypeNone:
      return KeyRequestType_V1_1::NONE;
    case wvcdm::kKeyRequestTypeUpdate:
      return KeyRequestType_V1_1::UPDATE;
    default:
      return static_cast<KeyRequestType_V1_1>(
          ConvertFromCdmKeyRequestType(keyRequestType));
  }
}

KeyRequestType toKeyRequestType_V1_0(KeyRequestType_V1_1 keyRequestType) {
  switch (keyRequestType) {
    case KeyRequestType_V1_1::NONE:
    case KeyRequestType_V1_1::UPDATE:
      return KeyRequestType::UNKNOWN;
    default:
      return static_cast<KeyRequestType>(keyRequestType);
  }
}

Status toStatus_1_0(Status_V1_2 status) {
  switch (status) {
    case Status_V1_2::ERROR_DRM_INSUFFICIENT_SECURITY:
    case Status_V1_2::ERROR_DRM_FRAME_TOO_LARGE:
    case Status_V1_2::ERROR_DRM_SESSION_LOST_STATE:
    case Status_V1_2::ERROR_DRM_RESOURCE_CONTENTION:
      return Status::ERROR_DRM_UNKNOWN;
    default:
      return static_cast<Status>(status);
  }
}

template<typename KST>
KST ConvertFromCdmKeyStatus(CdmKeyStatus keyStatus);

template<>
KeyStatusType ConvertFromCdmKeyStatus<KeyStatusType>(CdmKeyStatus keyStatus) {
  switch (keyStatus) {
    case wvcdm::kKeyStatusUsable:
      return KeyStatusType::USABLE;
    case wvcdm::kKeyStatusExpired:
      return KeyStatusType::EXPIRED;
    case wvcdm::kKeyStatusOutputNotAllowed:
      return KeyStatusType::OUTPUTNOTALLOWED;
    case wvcdm::kKeyStatusPending:
    case wvcdm::kKeyStatusUsableInFuture:
      return KeyStatusType::STATUSPENDING;
    case wvcdm::kKeyStatusInternalError:
    default:
      return KeyStatusType::INTERNALERROR;
  }
}

template<>
KeyStatusType_V1_2 ConvertFromCdmKeyStatus<KeyStatusType_V1_2>(CdmKeyStatus keyStatus) {
  switch (keyStatus) {
    case wvcdm::kKeyStatusUsableInFuture:
      return KeyStatusType_V1_2::USABLEINFUTURE;
    default:
      return static_cast<KeyStatusType_V1_2>(ConvertFromCdmKeyStatus<KeyStatusType>(keyStatus));
  }
}

HdcpLevel mapHdcpLevel(const std::string& level) {
  if (level == wvcdm::QUERY_VALUE_HDCP_V1)
    return HdcpLevel::HDCP_V1;
  else if (level == wvcdm::QUERY_VALUE_HDCP_V2_0)
    return HdcpLevel::HDCP_V2;
  else if (level == wvcdm::QUERY_VALUE_HDCP_V2_1)
    return HdcpLevel::HDCP_V2_1;
  else if (level == wvcdm::QUERY_VALUE_HDCP_V2_2)
    return HdcpLevel::HDCP_V2_2;
  else if (level == wvcdm::QUERY_VALUE_HDCP_NONE)
    return HdcpLevel::HDCP_NONE;
  else if (level == wvcdm::QUERY_VALUE_HDCP_NO_DIGITAL_OUTPUT)
    return HdcpLevel::HDCP_NO_OUTPUT;
  else {
    ALOGE("Invalid HDCP level=%s", level.c_str());
    return HdcpLevel::HDCP_NONE;
  }
}

HdcpLevel_V1_2 mapHdcpLevel_1_2(const std::string level) {
    if (level == wvcdm::QUERY_VALUE_HDCP_V2_3) {
        return HdcpLevel_V1_2::HDCP_V2_3;
    }
    return static_cast<HdcpLevel_V1_2>(mapHdcpLevel(level));
}

HdcpLevel toHdcpLevel_1_1(HdcpLevel_V1_2 level) {
    if (level == HdcpLevel_V1_2::HDCP_V2_3) {
        return HdcpLevel::HDCP_NONE;
    }
    return static_cast<HdcpLevel>(level);
}

}  // namespace

WVDrmPlugin::WVDrmPlugin(const sp<WvContentDecryptionModule>& cdm,
                         const std::string& appPackageName,
                         WVGenericCryptoInterface* crypto,
                         bool useSpoid)
  : mCdmIdentifierBuilder(useSpoid, *this, appPackageName),
    mCDM(cdm),
    mCrypto(crypto),
    mCryptoSessions() {}

WVDrmPlugin::~WVDrmPlugin() {
  typedef map<CdmSessionId, CryptoSession>::iterator mapIterator;
  for (mapIterator iter = mCryptoSessions.begin();
       iter != mCryptoSessions.end();
       ++iter) {
    CdmResponseType res = mCDM->CloseSession(iter->first);
    if (!isCdmResponseTypeSuccess(res)) {
      ALOGE("Failed to close session while destroying WVDrmPlugin");
    }
  }
  mCryptoSessions.clear();
  if (mCdmIdentifierBuilder.is_sealed()) {
    CdmIdentifier identifier;
    Status status = mCdmIdentifierBuilder.getCdmIdentifier(&identifier);
    if (status != Status::OK) {
      ALOGE("Failed to get cdm identifier %d", status);
    } else {
      status = mapCdmResponseType(mCDM->CloseCdm(identifier));
      if (status != Status::OK) {
        ALOGE("Failed to close cdm. status %d", status);
      }
    }
  }
}

Status WVDrmPlugin::openSessionCommon(std::vector<uint8_t>& sessionId) {
  Status status = Status::OK;

  CdmIdentifier identifier;
  status = mCdmIdentifierBuilder.getCdmIdentifier(&identifier);
  if (status != Status::OK) {
    return status;
  }

  CdmSessionId cdmSessionId;
  CdmResponseType res =
      mCDM->OpenSession("com.widevine", &mPropertySet, identifier, this,
                        &cdmSessionId);

  if (!isCdmResponseTypeSuccess(res)) {
    status = mapAndNotifyOfCdmResponseType(sessionId, res);
    return status;
  }

  bool success = false;

  // Construct a CryptoSession
  CdmQueryMap info;
  res = mCDM->QueryOemCryptoSessionId(cdmSessionId, &info);

  if (isCdmResponseTypeSuccess(res) &&
      info.count(wvcdm::QUERY_KEY_OEMCRYPTO_SESSION_ID)) {
    OEMCrypto_SESSION oecSessionId;
    std::istringstream(
        info[wvcdm::QUERY_KEY_OEMCRYPTO_SESSION_ID]) >> oecSessionId;
    mCryptoSessions[cdmSessionId] = CryptoSession(oecSessionId);
    success = true;
  } else {
    ALOGE("Unable to query key control info.");
  }

  if (success) {
    // Marshal Session ID
    sessionId = StrToVector(cdmSessionId);
    return Status::OK;
  } else {
    mCDM->CloseSession(cdmSessionId);

    if (!isCdmResponseTypeSuccess(res)) {
      // We got an error code we can return.
      status = mapAndNotifyOfCdmResponseType(sessionId, res);
    } else {
      // We got a failure that did not give us an error code, such as a failure
      // of AttachEventListener() or the key being missing from the map.
      ALOGW("Returns UNKNOWN error for legacy status kErrorCDMGeneric");
      status = Status::ERROR_DRM_UNKNOWN;
    }
  }

  return status;
}

Return<void> WVDrmPlugin::openSession(openSession_cb _hidl_cb) {
  std::vector<uint8_t> sessionId;
  Status status = openSessionCommon(sessionId);

  _hidl_cb(status, toHidlVec(sessionId));
  return Void();
}

SecurityLevel WVDrmPlugin::mapSecurityLevel(const std::string& level) {
  SecurityLevel hSecurityLevel = SecurityLevel::UNKNOWN;

  if (wvcdm::QUERY_VALUE_SECURITY_LEVEL_L1 == level) {
    hSecurityLevel = SecurityLevel::HW_SECURE_ALL;
  } else if (wvcdm::QUERY_VALUE_SECURITY_LEVEL_L2 == level) {
    hSecurityLevel = SecurityLevel::HW_SECURE_CRYPTO;
  } else if (wvcdm::QUERY_VALUE_SECURITY_LEVEL_L3 == level) {
    hSecurityLevel = SecurityLevel::SW_SECURE_CRYPTO;
  } // else QUERY_VALUE_SECURITY_LEVEL_UNKNOWN returns Security::UNKNOWN

  return hSecurityLevel;
}

Return<void> WVDrmPlugin::openSession_1_1(
    SecurityLevel requestedLevel,
    openSession_1_1_cb _hidl_cb) {
  std::vector<uint8_t> sessionId;
  sessionId.clear();

  if (SecurityLevel::UNKNOWN == requestedLevel) {
    _hidl_cb(Status::ERROR_DRM_CANNOT_HANDLE, toHidlVec(sessionId));
    return Void();
  }

  std::string native_security_level;
  Status status = queryProperty(wvcdm::kLevelDefault,
          wvcdm::QUERY_KEY_SECURITY_LEVEL, native_security_level);
  if (Status::OK != status) {
    _hidl_cb(status, toHidlVec(sessionId));
    return Void();
  }

  if (wvcdm::QUERY_VALUE_SECURITY_LEVEL_L3 == native_security_level &&
      requestedLevel >= SecurityLevel::SW_SECURE_DECODE) {
    _hidl_cb(Status::ERROR_DRM_CANNOT_HANDLE, toHidlVec(sessionId));
    return Void();
  }

  std::string wvcdm_security_level =
      (SecurityLevel::SW_SECURE_CRYPTO == requestedLevel) ?
      wvcdm::QUERY_VALUE_SECURITY_LEVEL_L3 : wvcdm::QUERY_VALUE_SECURITY_LEVEL_DEFAULT;

  setPropertyString(hidl_string(wvcdm::QUERY_KEY_SECURITY_LEVEL),
      hidl_string(wvcdm_security_level));

  status = openSessionCommon(sessionId);
  hidl_vec<uint8_t> hSessionId = toHidlVec(sessionId);
  if (Status::OK == status) {
    Return<void> hResult = getSecurityLevel(hSessionId, [&](Status status, SecurityLevel hSecurityLevel) {
      if (Status::OK != status || requestedLevel != hSecurityLevel) {
        ALOGE("Failed to open session with the requested security level=%d", requestedLevel);
        if (Status::OK != closeSession(hSessionId)) sessionId.clear();
      }
    });
    if (!hResult.isOk()) {
      status = Status::ERROR_DRM_INVALID_STATE;
    }
  }
  _hidl_cb(status, toHidlVec(sessionId));
  return Void();
}

Return<Status> WVDrmPlugin::closeSession(const hidl_vec<uint8_t>& sessionId) {
  if (!sessionId.size()) {
    return Status::BAD_VALUE;
  }
  const std::vector<uint8_t> sId = toVector(sessionId);
  CdmSessionId cdmSessionId(sId.begin(), sId.end());
  CdmResponseType res = mCDM->CloseSession(cdmSessionId);
  mCryptoSessions.erase(cdmSessionId);
  if (!isCdmResponseTypeSuccess(res)) {
    return Status::ERROR_DRM_SESSION_NOT_OPENED;
  }
  return Status::OK;
}

Return<void> WVDrmPlugin::getKeyRequest(
    const hidl_vec<uint8_t>& scope,
    const hidl_vec<uint8_t>& initData,
    const hidl_string& mimeType,
    KeyType keyType,
    const hidl_vec<KeyValue>& optionalParameters,
    getKeyRequest_cb _hidl_cb) {
  hidl_string defaultUrl;
  hidl_vec<uint8_t> request;
  KeyRequestType requestType = KeyRequestType::UNKNOWN;
  Status status = Status::ERROR_DRM_UNKNOWN;

  defaultUrl.clear();
  Return<void> hResult = getKeyRequest_1_1(
          scope, initData, mimeType, keyType, optionalParameters,
          [&](Status statusCode, const hidl_vec<uint8_t>& hRequest,
              KeyRequestType_V1_1 hKeyRequestType, const hidl_string& hDefaultUrl) {
            defaultUrl = hDefaultUrl;
            request = hRequest;
            requestType = toKeyRequestType_V1_0(hKeyRequestType);
            status = statusCode;
          });
  if (!hResult.isOk()) {
    status = Status::ERROR_DRM_INVALID_STATE;
  }
  _hidl_cb(status, request, requestType, defaultUrl);
  return Void();
}

Return<void> WVDrmPlugin::getKeyRequest_1_1(
    const hidl_vec<uint8_t>& scope,
    const hidl_vec<uint8_t>& initData,
    const hidl_string& mimeType,
    KeyType keyType,
    const hidl_vec<KeyValue>& optionalParameters,
    getKeyRequest_1_1_cb _hidl_cb) {
  hidl_string defaultUrl;
  hidl_vec<uint8_t> request;
  KeyRequestType_V1_1 requestType = KeyRequestType_V1_1::UNKNOWN;
  Status status = Status::ERROR_DRM_UNKNOWN;

  defaultUrl.clear();
  Return<void> hResult = getKeyRequest_1_2(
      scope, initData, mimeType, keyType, optionalParameters,
          [&](Status_V1_2 statusCode, const hidl_vec<uint8_t>& hRequest,
              KeyRequestType_V1_1 hKeyRequestType, const hidl_string& hDefaultUrl) {
            defaultUrl = hDefaultUrl;
            request = hRequest;
            requestType = hKeyRequestType;
            status = toStatus_1_0(statusCode);
          });
  if (!hResult.isOk()) {
    status = Status::ERROR_DRM_INVALID_STATE;
  }
  _hidl_cb(status, request, requestType, defaultUrl);
  return Void();
}

Return<void> WVDrmPlugin::getKeyRequest_1_2(
    const hidl_vec<uint8_t>& scope,
    const hidl_vec<uint8_t>& initData,
    const hidl_string& mimeType,
    KeyType keyType,
    const hidl_vec<KeyValue>& optionalParameters,
    getKeyRequest_1_2_cb _hidl_cb) {

  if (!scope.size()) {
    _hidl_cb(Status_V1_2::BAD_VALUE, hidl_vec<uint8_t>(),
        KeyRequestType_V1_1::UNKNOWN, "");
    return Void();
  }
  KeyRequestType_V1_1 requestType = KeyRequestType_V1_1::UNKNOWN;
  Status_V1_2 status = Status_V1_2::OK;
  std::string defaultUrl;
  std::vector<uint8_t> request;
  const std::vector<uint8_t> scopeId = toVector(scope);

  CdmIdentifier identifier;
  status = static_cast<Status_V1_2>(
      mCdmIdentifierBuilder.getCdmIdentifier(&identifier));
  if (status != Status_V1_2::OK) {
    _hidl_cb(status, toHidlVec(request), requestType,
             defaultUrl.c_str());
    return Void();
  }

  CdmLicenseType cdmLicenseType;
  CdmSessionId cdmSessionId;
  CdmKeySetId cdmKeySetId;
  if (keyType == KeyType::OFFLINE) {
    cdmLicenseType = wvcdm::kLicenseTypeOffline;
    cdmSessionId.assign(scopeId.begin(), scopeId.end());
  } else if (keyType == KeyType::STREAMING) {
    cdmLicenseType = wvcdm::kLicenseTypeStreaming;
    cdmSessionId.assign(scopeId.begin(), scopeId.end());
  } else if (keyType == KeyType::RELEASE) {
    cdmLicenseType = wvcdm::kLicenseTypeRelease;
    cdmKeySetId.assign(scopeId.begin(), scopeId.end());
  } else {
    _hidl_cb(Status_V1_2::BAD_VALUE, toHidlVec(request), KeyRequestType_V1_1::UNKNOWN,
             defaultUrl.c_str());
    return Void();
  }

  std::string cdmInitDataType = mimeType;
  // Provide backwards-compatibility for apps that pass non-EME-compatible MIME
  // types.
  if (!WvContentDecryptionModule::IsSupported(cdmInitDataType)) {
    cdmInitDataType = wvcdm::ISO_BMFF_VIDEO_MIME_TYPE;
  }

  CdmInitData processedInitData;
  if (initData.size() > 0 &&
      WvContentDecryptionModule::IsCenc(cdmInitDataType) &&
      !initDataResemblesPSSH(toVector(initData))) {
    // This data was passed in the old format, pre-unwrapped. We need to wrap
    // the init data in a new PSSH header.
    static const uint8_t psshPrefix[] = {
      0, 0, 0, 0,                                     // Total size
      'p', 's', 's', 'h',                             // "PSSH"
      0, 0, 0, 0,                                     // Flags - must be zero
      0xED, 0xEF, 0x8B, 0xA9, 0x79, 0xD6, 0x4A, 0xCE, // Widevine UUID
      0xA3, 0xC8, 0x27, 0xDC, 0xD5, 0x1D, 0x21, 0xED,
      0, 0, 0, 0                                      // Size of initData
    };
    processedInitData.assign(reinterpret_cast<const char*>(psshPrefix),
                             sizeof(psshPrefix) / sizeof(uint8_t));
    processedInitData.append(reinterpret_cast<const char*>(initData.data()),
                             initData.size());
    const size_t kPsshBoxSizeLocation = 0;
    const size_t kInitDataSizeLocation =
        sizeof(psshPrefix) - sizeof(uint32_t);
    uint32_t psshBoxSize = htonl(processedInitData.size());
    uint32_t initDataSize = htonl(initData.size());
    memcpy(&processedInitData[kPsshBoxSizeLocation], &psshBoxSize,
           sizeof(uint32_t));
    memcpy(&processedInitData[kInitDataSizeLocation], &initDataSize,
           sizeof(uint32_t));
  } else {
    // For other formats, we can pass the init data through unmodified.
    processedInitData.assign(reinterpret_cast<const char*>(initData.data()),
                             initData.size());
  }

  CdmAppParameterMap cdmParameters;
  for (size_t i = 0; i < optionalParameters.size(); ++i) {
    const std::string& key(optionalParameters[i].key);
    const std::string& value(optionalParameters[i].value);

    std::string cdmKey(key.c_str(), key.size());
    std::string cdmValue(value.c_str(), value.size());

    cdmParameters[cdmKey] = cdmValue;
  }

  CdmKeyRequest keyRequest;
  CdmResponseType res = mCDM->GenerateKeyRequest(
      cdmSessionId, cdmKeySetId, cdmInitDataType, processedInitData,
      cdmLicenseType, cdmParameters, &mPropertySet, identifier, &keyRequest);

  requestType = ConvertFromCdmKeyRequestType_1_1(keyRequest.type);

  if (isCdmResponseTypeSuccess(res)) {
    defaultUrl.clear();
    defaultUrl.assign(keyRequest.url.data(), keyRequest.url.size());

    request = StrToVector(keyRequest.message);
  }

  if (keyType == KeyType::RELEASE) {
    // When releasing keys, we do not have a session ID.
    status = mapCdmResponseType_1_2(res);
  } else {
    // For all other requests, we have a session ID.
    status = mapAndNotifyOfCdmResponseType_1_2(scopeId, res);
  }
  _hidl_cb(status, toHidlVec(request), requestType,
           defaultUrl.c_str());
  return Void();
}

Return<void> WVDrmPlugin::provideKeyResponse(
    const hidl_vec<uint8_t>& scope,
    const hidl_vec<uint8_t>& response,
    provideKeyResponse_cb _hidl_cb) {

  if (scope.size() == 0 || response.size() == 0) {
    _hidl_cb(Status::BAD_VALUE, hidl_vec<uint8_t>());
    return Void();
  }
  const std::vector<uint8_t> resp = toVector(response);
  const std::vector<uint8_t> scopeId = toVector(scope);

  CdmKeySetId cdmKeySetId;
  CdmSessionId cdmSessionId;
  CdmKeyResponse cdmResponse(resp.begin(), resp.end());

  bool isRequest = (memcmp(scopeId.data(), wvcdm::SESSION_ID_PREFIX,
                           sizeof(wvcdm::SESSION_ID_PREFIX) - 1) == 0);
  bool isRelease = (memcmp(scopeId.data(), wvcdm::KEY_SET_ID_PREFIX,
                           sizeof(wvcdm::KEY_SET_ID_PREFIX) - 1) == 0);

  std::vector<uint8_t> keySetId;

  if (isRequest) {
    cdmSessionId.assign(scopeId.begin(), scopeId.end());
  } else if (isRelease) {
    cdmKeySetId.assign(scopeId.begin(), scopeId.end());
  } else {
    _hidl_cb(Status::ERROR_DRM_CANNOT_HANDLE, toHidlVec(keySetId));
    return Void();
  }

  CdmResponseType res = mCDM->AddKey(cdmSessionId, cdmResponse, &cdmKeySetId);

  if (isRequest && isCdmResponseTypeSuccess(res)) {
    keySetId = StrToVector(cdmKeySetId);
  }

  Status status = Status::OK;
  if (isRelease) {
    // When releasing keys, we do not have a session ID.
    status = mapCdmResponseType(res);
  } else {
    // For all other requests, we have a session ID.
    status = mapAndNotifyOfCdmResponseType(scopeId, res);
    // For "NEED_KEY," we still want to send the notifcation, but then we don't
    // return the error.  This is because "NEED_KEY" from AddKey() is an
    // expected behavior when sending a privacy certificate.
    if (res == wvcdm::NEED_KEY && mPropertySet.use_privacy_mode()) {
      status = Status::OK;
    }
  }
  _hidl_cb(status, toHidlVec(keySetId));
  return Void();
}

Return<Status> WVDrmPlugin::removeKeys(const hidl_vec<uint8_t>& sessionId) {

  if (!sessionId.size()) {
    return Status::BAD_VALUE;
  }
  const std::vector<uint8_t> sId = toVector(sessionId);
  CdmSessionId cdmSessionId(sId.begin(), sId.end());

  CdmResponseType res = mCDM->RemoveKeys(cdmSessionId);

  return mapAndNotifyOfCdmResponseType(sId, res);
}

Return<Status> WVDrmPlugin::restoreKeys(const hidl_vec<uint8_t>& sessionId,
                                        const hidl_vec<uint8_t>& keySetId) {

  if (!sessionId.size() || !keySetId.size()) {
    return Status::BAD_VALUE;
  }
  const std::vector<uint8_t> kId = toVector(keySetId);
  const std::vector<uint8_t> sId = toVector(sessionId);
  CdmSessionId cdmSessionId(sId.begin(), sId.end());
  CdmKeySetId cdmKeySetId(kId.begin(), kId.end());

  CdmResponseType res = mCDM->RestoreKey(cdmSessionId, cdmKeySetId);

  return mapAndNotifyOfCdmResponseType(sId, res);
}

 Return<void> WVDrmPlugin::queryKeyStatus(const hidl_vec<uint8_t>& sessionId,
                                          queryKeyStatus_cb _hidl_cb) {

  if (sessionId.size() == 0) {
    _hidl_cb(Status::BAD_VALUE, hidl_vec<KeyValue>());
    return Void();
  }
  const std::vector<uint8_t> sId = toVector(sessionId);
  CdmSessionId cdmSessionId(sId.begin(), sId.end());
  CdmQueryMap cdmLicenseInfo;

  CdmResponseType res = mCDM->QueryKeyStatus(cdmSessionId, &cdmLicenseInfo);

  std::vector<KeyValue> infoMapVec;
  if (isCdmResponseTypeSuccess(res)) {
    infoMapVec.clear();

    KeyValue keyValuePair;
    for (CdmQueryMap::const_iterator iter = cdmLicenseInfo.begin();
         iter != cdmLicenseInfo.end();
         ++iter) {
      const std::string& cdmKey = iter->first;
      const std::string& cdmValue = iter->second;
      keyValuePair.key = std::string(cdmKey.data(), cdmKey.size());
      keyValuePair.value = std::string(cdmValue.data(), cdmValue.size());
      infoMapVec.push_back(keyValuePair);
    }
  }

  _hidl_cb(mapCdmResponseType(res), toHidlVec(infoMapVec));
  return Void();
}

Return<void> WVDrmPlugin::getProvisionRequest(
     const hidl_string& certificateType,
     const hidl_string& certificateAuthority,
     getProvisionRequest_cb _hidl_cb) {
  Status status = Status::OK;
  std::string defaultUrl;
  std::vector<uint8_t> request;

  Return<void> hResult = getProvisionRequest_1_2(
      certificateType, certificateAuthority,
          [&](Status_V1_2 statusCode, const hidl_vec<uint8_t>& hRequest,
              const hidl_string& hDefaultUrl) {
            request = hRequest;
            status = toStatus_1_0(statusCode);
            defaultUrl = hDefaultUrl;
          });
  if (!hResult.isOk()) {
    status = Status::ERROR_DRM_INVALID_STATE;
  }

  _hidl_cb(status, toHidlVec(request), hidl_string(defaultUrl));
  return Void();
}

Return<void> WVDrmPlugin::getProvisionRequest_1_2(
     const hidl_string& certificateType,
     const hidl_string& certificateAuthority,
     getProvisionRequest_1_2_cb _hidl_cb) {
  Status_V1_2 status = Status_V1_2::OK;
  std::string defaultUrl;
  std::vector<uint8_t> request;

  CdmIdentifier identifier;
  status = static_cast<Status_V1_2>(mCdmIdentifierBuilder.getCdmIdentifier(&identifier));
  if (status != Status_V1_2::OK) {
    _hidl_cb(status, toHidlVec(request), hidl_string(defaultUrl));
    return Void();
  }

  CdmProvisioningRequest cdmProvisionRequest;
  std::string cdmDefaultUrl;

  CdmCertificateType cdmCertType = wvcdm::kCertificateWidevine;
  if (certificateType == "X.509") {
    cdmCertType = wvcdm::kCertificateX509;
  }

  std::string cdmCertAuthority = certificateAuthority;

  CdmResponseType res = mCDM->GetProvisioningRequest(
      cdmCertType, cdmCertAuthority, identifier,
      mProvisioningServiceCertificate, &cdmProvisionRequest, &cdmDefaultUrl);
  if (isCdmResponseTypeSuccess(res)) {
    request = StrToVector(cdmProvisionRequest);
    defaultUrl.clear();
    defaultUrl.assign(cdmDefaultUrl.data(), cdmDefaultUrl.size());
  }

  _hidl_cb(mapCdmResponseType_1_2(res), toHidlVec(request),
           hidl_string(defaultUrl));
  return Void();
}

Return<void> WVDrmPlugin::provideProvisionResponse(
    const hidl_vec<uint8_t>& response,
    provideProvisionResponse_cb _hidl_cb) {

  if (!response.size()) {
    _hidl_cb(Status::BAD_VALUE, hidl_vec<uint8_t>(), hidl_vec<uint8_t>());
    return Void();
  }
  const std::vector<uint8_t> resp = toVector(response);
  std::vector<uint8_t> certificate;
  std::vector<uint8_t> wrappedKey;

  CdmIdentifier identifier;
  Status status = mCdmIdentifierBuilder.getCdmIdentifier(&identifier);
  if (status != Status::OK) {
    _hidl_cb(status, toHidlVec(certificate), toHidlVec(wrappedKey));
  }

  CdmProvisioningResponse cdmResponse(resp.begin(), resp.end());
  if (cdmResponse == kSpecialUnprovisionResponse) {
    if (identifier.IsEquivalentToDefault()) {
       ALOGW("Returns UNKNOWN error for legacy status kErrorNoOriginSpecified");
      _hidl_cb(Status::ERROR_DRM_UNKNOWN, toHidlVec(certificate),
               toHidlVec(wrappedKey));
      return Void();
    }
    _hidl_cb(unprovision(identifier),
             toHidlVec(certificate),
             toHidlVec(wrappedKey));
    return Void();
  } else {
    std::string cdmCertificate;
    std::string cdmWrappedKey;
    CdmResponseType res = mCDM->HandleProvisioningResponse(
        identifier, cdmResponse, &cdmCertificate, &cdmWrappedKey);
    if (isCdmResponseTypeSuccess(res)) {
      certificate = StrToVector(cdmCertificate);
      wrappedKey = StrToVector(cdmWrappedKey);
    }

    _hidl_cb(mapCdmResponseType(res), toHidlVec(certificate),
             toHidlVec(wrappedKey));
    return Void();
  }
}

Status WVDrmPlugin::unprovisionDevice() {
  return unprovision(kDefaultCdmIdentifier);
}

Return<void> WVDrmPlugin::getSecureStop(
    const hidl_vec<uint8_t>& secureStopId,
    getSecureStop_cb _hidl_cb) {

  if (!secureStopId.size()) {
    _hidl_cb(Status::BAD_VALUE, SecureStop());
    return Void();
  }

  const std::vector<uint8_t> id = toVector(secureStopId);
  std::vector<uint8_t> cdmStopVec;
  SecureStop secureStop;

  CdmIdentifier identifier;
  Status status = mCdmIdentifierBuilder.getCdmIdentifier(&identifier);
  if (status != Status::OK) {
    _hidl_cb(status, secureStop);
    return Void();
  }

  CdmUsageInfo cdmUsageInfo;
  CdmSecureStopId cdmSsId(id.begin(), id.end());
  CdmResponseType res = mCDM->GetUsageInfo(
      mPropertySet.app_id(), cdmSsId, identifier, &cdmUsageInfo);

  if (isCdmResponseTypeSuccess(res)) {
    for (CdmUsageInfo::const_iterator iter = cdmUsageInfo.begin();
         iter != cdmUsageInfo.end();
         ++iter) {
      const std::string& cdmStop = *iter;
      cdmStopVec = StrToVector(cdmStop);
    }
    secureStop.opaqueData = toHidlVec(cdmStopVec);
  }

  _hidl_cb(mapCdmResponseType(res), secureStop);
  return Void();
}

Return<void> WVDrmPlugin::getSecureStops(getSecureStops_cb _hidl_cb) {

  std::list<std::vector<uint8_t> > secureStops;
  std::vector<SecureStop> secureStopsVec;

  CdmIdentifier identifier;
  Status status = mCdmIdentifierBuilder.getCdmIdentifier(&identifier);
  if (status != Status::OK) {
    _hidl_cb(status, toHidlVec(secureStopsVec));
    return Void();
  }

  CdmUsageInfo cdmUsageInfo;
  CdmResponseType res =
      mCDM->GetUsageInfo(mPropertySet.app_id(), identifier, &cdmUsageInfo);

  if (isCdmResponseTypeSuccess(res)) {
    secureStops.clear();
    for (CdmUsageInfo::const_iterator iter = cdmUsageInfo.begin();
         iter != cdmUsageInfo.end();
         ++iter) {
      const std::string& cdmStop = *iter;
      secureStops.push_back(StrToVector(cdmStop));
    }
  }

  std::list<std::vector<uint8_t> >::iterator iter = secureStops.begin();
  while (iter != secureStops.end()) {
    SecureStop secureStop;
    secureStop.opaqueData = toHidlVec(*iter++);
    secureStopsVec.push_back(secureStop);
  }

  _hidl_cb(mapCdmResponseType(res), toHidlVec(secureStopsVec));
  return Void();
}

Return<Status> WVDrmPlugin::releaseAllSecureStops() {
  return removeAllSecureStops();
}

Return<Status> WVDrmPlugin::releaseSecureStop(
    const hidl_vec<uint8_t>& secureStopId) {

  if (!secureStopId.size()) {
    return Status::BAD_VALUE;
  }

  CdmIdentifier identifier;
  Status status = mCdmIdentifierBuilder.getCdmIdentifier(&identifier);
  if (status != Status::OK) {
    return status;
  }

  const std::vector<uint8_t> ssRelease = toVector(secureStopId);
  CdmUsageInfoReleaseMessage cdmMessage(ssRelease.begin(), ssRelease.end());
  CdmResponseType res = mCDM->ReleaseUsageInfo(cdmMessage, identifier);
  return mapCdmResponseType(res);
}

Return<void> WVDrmPlugin::getMetrics(getMetrics_cb _hidl_cb) {
  hidl_vec<DrmMetricGroup> hidl_metrics;
  CdmIdentifier identifier;
  Status status = mCdmIdentifierBuilder.getCdmIdentifier(&identifier);
  if (status != Status::OK) {
    _hidl_cb(status, hidl_metrics);
    return Void();
  }

  drm_metrics::WvCdmMetrics proto_metrics;
  CdmResponseType result = mCDM->GetMetrics(identifier, &proto_metrics);
  if (result != wvcdm::NO_ERROR) {
    status = mapCdmResponseType(result);
    _hidl_cb(status, hidl_metrics);
    return Void();
  }

  ::wvcdm::HidlMetricsAdapter adapter;
  ::wvcdm::HidlMetricsAdapter::ToHidlMetrics(proto_metrics, &hidl_metrics);
  _hidl_cb(Status::OK, hidl_metrics);
  return Void();
}

Return<void> WVDrmPlugin::getSecureStopIds(getSecureStopIds_cb _hidl_cb) {

  std::vector<SecureStopId> secureStopIds;

  CdmIdentifier identifier;
  Status status = mCdmIdentifierBuilder.getCdmIdentifier(&identifier);
  if (status != Status::OK) {
    _hidl_cb(status, toHidlVec(secureStopIds));
    return Void();
  }

  std::vector<CdmSecureStopId> ssids;
  CdmResponseType res =
      mCDM->GetSecureStopIds(mPropertySet.app_id(), identifier, &ssids);

  if (isCdmResponseTypeSuccess(res)) {
    for (auto itr = ssids.begin(); itr != ssids.end(); ++itr) {
      const CdmSecureStopId& ssid = *itr;
      secureStopIds.push_back(StrToVector(ssid));
    }
  }

  _hidl_cb(mapCdmResponseType(res), toHidlVec(secureStopIds));
  return Void();
}

Return<Status> WVDrmPlugin::releaseSecureStops(const SecureStopRelease& ssRelease) {

  if (ssRelease.opaqueData.size() == 0) {
    return Status::BAD_VALUE;
  }

  CdmIdentifier identifier;
  Status status = mCdmIdentifierBuilder.getCdmIdentifier(&identifier);
  if (status != Status::OK) {
    return status;
  }

  const std::vector<uint8_t> data = toVector(ssRelease.opaqueData);
  CdmUsageInfoReleaseMessage cdmMessage(data.begin(), data.end());
  CdmResponseType res = mCDM->ReleaseUsageInfo(cdmMessage, identifier);
  return mapCdmResponseType(res);
}

Return<Status> WVDrmPlugin::removeSecureStop(const hidl_vec<uint8_t>& secureStopId) {
  if (!secureStopId.size()) {
    return Status::BAD_VALUE;
  }

  CdmIdentifier identifier;
  Status status = mCdmIdentifierBuilder.getCdmIdentifier(&identifier);
  if (status != Status::OK) {
    return status;
  }

  const std::vector<uint8_t> idVec = toVector(secureStopId);
  CdmSecureStopId id(idVec.begin(), idVec.end());
  CdmResponseType res = mCDM->RemoveUsageInfo(mPropertySet.app_id(), identifier, id);
  return mapCdmResponseType(res);
}

Return<Status> WVDrmPlugin::removeAllSecureStops() {
  CdmIdentifier identifier;
  Status status = mCdmIdentifierBuilder.getCdmIdentifier(&identifier);
  if (status != Status::OK) {
    return status;
  }

  CdmResponseType res = mCDM->RemoveAllUsageInfo(mPropertySet.app_id(),
                                                 identifier);
  return mapCdmResponseType(res);
}

Return<void> WVDrmPlugin::getHdcpLevels(getHdcpLevels_cb _hidl_cb) {
    HdcpLevel connectedLevel = HdcpLevel::HDCP_NONE;
    HdcpLevel maxLevel = HdcpLevel::HDCP_NO_OUTPUT;

    Return<void> hResult = getHdcpLevels_1_2(
            [&](Status_V1_2 status, const HdcpLevel_V1_2& hConnected,
                               const HdcpLevel_V1_2& hMax) {
                if (status == Status_V1_2::OK) {
                    connectedLevel = toHdcpLevel_1_1(hConnected);
                    maxLevel = toHdcpLevel_1_1(hMax);
                }
            });

  _hidl_cb(Status::OK, connectedLevel, maxLevel);
  return Void();
}

Return<void> WVDrmPlugin::getHdcpLevels_1_2(getHdcpLevels_1_2_cb _hidl_cb) {
  HdcpLevel_V1_2 connectedLevel = HdcpLevel_V1_2::HDCP_NONE;
  HdcpLevel_V1_2 maxLevel = HdcpLevel_V1_2::HDCP_NO_OUTPUT;

  std::string level;
  Status status = queryProperty(wvcdm::QUERY_KEY_CURRENT_HDCP_LEVEL, level);
  if (status == Status::OK) {
    connectedLevel = mapHdcpLevel_1_2(level);
  } else {
    ALOGE("Failed to query current hdcp level.");
    _hidl_cb(Status_V1_2::ERROR_DRM_INVALID_STATE, connectedLevel, maxLevel);
    return Void();
  }

  status = queryProperty(wvcdm::QUERY_KEY_MAX_HDCP_LEVEL, level);
  if (status == Status::OK) {
    maxLevel = mapHdcpLevel_1_2(level);
  } else {
    ALOGE("Failed to query maximum hdcp level.");
    _hidl_cb(Status_V1_2::ERROR_DRM_INVALID_STATE, connectedLevel, maxLevel);
    return Void();
  }

  _hidl_cb(Status_V1_2::OK, connectedLevel, maxLevel);
  return Void();
}

Return<void> WVDrmPlugin::getNumberOfSessions(getNumberOfSessions_cb _hidl_cb) {
  uint32_t currentSessions = 0;
  uint32_t maxSessions = 1;

  std::string value;
  Status status = queryProperty(wvcdm::QUERY_KEY_NUMBER_OF_OPEN_SESSIONS, value);
  if (status == Status::OK) {
    currentSessions = std::strtoul(value.c_str(), nullptr, 10);
  } else {
    ALOGE("Failed to query currently opened sessions.");
    _hidl_cb(Status::ERROR_DRM_INVALID_STATE, currentSessions, maxSessions);
    return Void();
  }

  status = queryProperty(wvcdm::QUERY_KEY_MAX_NUMBER_OF_SESSIONS, value);
  if (status == Status::OK) {
    maxSessions = std::strtoul(value.c_str(), nullptr, 10);
  } else {
    ALOGE("Failed to query maximum number of sessions that the device can support.");
    _hidl_cb(Status::ERROR_DRM_INVALID_STATE, currentSessions, maxSessions);
    return Void();
  }

  _hidl_cb(Status::OK, currentSessions, maxSessions);
  return Void();
}

Return<void> WVDrmPlugin::getSecurityLevel(
    const hidl_vec<uint8_t>& sessionId,
    getSecurityLevel_cb _hidl_cb) {
  if (sessionId.size() == 0) {
    _hidl_cb(Status::BAD_VALUE, SecurityLevel::UNKNOWN);
    return Void();
  }

  std::vector<uint8_t> sid = toVector(sessionId);
  CdmQueryMap info;
  SecurityLevel hSecurityLevel = SecurityLevel::UNKNOWN;

  CdmResponseType status = mCDM->QuerySessionStatus(
          std::string(sid.begin(), sid.end()), &info);
  if (wvcdm::NO_ERROR == status) {
    std::string level = info[wvcdm::QUERY_KEY_SECURITY_LEVEL];
    hSecurityLevel = mapSecurityLevel(level);
  } else {
    ALOGE("Failed to query security level, status=%d", status);
  }

  _hidl_cb(mapCdmResponseType(status), hSecurityLevel);
  return Void();
}

Return<void> WVDrmPlugin::getOfflineLicenseKeySetIds(
    getOfflineLicenseKeySetIds_cb _hidl_cb) {
  std::vector<std::vector<uint8_t> > keySetIds;
  std::vector<KeySetId> keySetIdsVec;

  CdmIdentifier identifier;
  Status status = mCdmIdentifierBuilder.getCdmIdentifier(&identifier);
  if (status != Status::OK) {
    _hidl_cb(status, toHidlVec(keySetIdsVec));
    return Void();
  }

  std::vector<CdmSecurityLevel> levels = {
      wvcdm::kSecurityLevelL1, wvcdm::kSecurityLevelL3 };

  CdmResponseType res = wvcdm::UNKNOWN_ERROR;

  for (auto level : levels) {
    std::vector<CdmKeySetId> cdmKeySetIds;
    res = mCDM->ListStoredLicenses(level, identifier, &cdmKeySetIds);

    if (isCdmResponseTypeSuccess(res)) {
      keySetIds.clear();
      for (auto id : cdmKeySetIds) {
        keySetIds.push_back(StrToVector(id));
      }
      for (auto id : keySetIds) {
        keySetIdsVec.push_back(id);
      }
    }
  }
  _hidl_cb(mapCdmResponseType(res), toHidlVec(keySetIdsVec));
  return Void();
}

Return<void> WVDrmPlugin::getOfflineLicenseState(const KeySetId &keySetId,
    getOfflineLicenseState_cb _hidl_cb) {
    OfflineLicenseState licenseState = OfflineLicenseState::UNKNOWN;

  if (!keySetId.size()) {
    _hidl_cb(Status::BAD_VALUE, licenseState);
    return Void();
  }

  CdmIdentifier identifier;
  Status status = mCdmIdentifierBuilder.getCdmIdentifier(&identifier);
  if (status != Status::OK) {
    _hidl_cb(status, licenseState);
    return Void();
  }

  CdmResponseType res = wvcdm::UNKNOWN_ERROR;
  CdmKeySetId keySetIdStr(keySetId.begin(), keySetId.end());

  wvcdm::CdmOfflineLicenseState state = wvcdm::kLicenseStateUnknown;
  res = mCDM->GetOfflineLicenseState(
      keySetIdStr,
      wvcdm::kSecurityLevelL1,
      identifier,
      &state);
  if (!isCdmResponseTypeSuccess(res)) {
    // try L3
    res = mCDM->GetOfflineLicenseState(
        keySetIdStr,
        wvcdm::kSecurityLevelL3,
        identifier,
        &state);
    if (!isCdmResponseTypeSuccess(res)) {
      _hidl_cb(Status::BAD_VALUE, licenseState);
      return Void();
    }
  }

  switch(state) {
    case wvcdm::kLicenseStateActive:
      licenseState = OfflineLicenseState::USABLE;
      break;
    case wvcdm::kLicenseStateReleasing:
      licenseState = OfflineLicenseState::INACTIVE;
      break;
    default:
      licenseState = OfflineLicenseState::UNKNOWN;
      ALOGE("Return unknown offline license state for %s", keySetIdStr.c_str());
      break;
  }

  _hidl_cb(mapCdmResponseType(res), licenseState);
  return Void();
}

Return<Status> WVDrmPlugin::removeOfflineLicense(const KeySetId &keySetId) {
  if (!keySetId.size()) {
    return Status::BAD_VALUE;
  }

  CdmIdentifier identifier;
  Status status = mCdmIdentifierBuilder.getCdmIdentifier(&identifier);
  if (status != Status::OK) {
    return status;
  }

  CdmResponseType res = wvcdm::UNKNOWN_ERROR;

  res = mCDM->RemoveOfflineLicense(
      std::string(keySetId.begin(), keySetId.end()),
      wvcdm::kSecurityLevelL1,
      identifier);
  if (!isCdmResponseTypeSuccess(res)) {
    CdmResponseType res = mCDM->RemoveOfflineLicense(
        std::string(keySetId.begin(), keySetId.end()),
        wvcdm::kSecurityLevelL3,
        identifier);
    status = mapCdmResponseType(res);
  }

  return status;
}

Return<void> WVDrmPlugin::getPropertyString(const hidl_string& propertyName,
                                            getPropertyString_cb _hidl_cb) {
  Status status = Status::OK;
  std::string name(propertyName.c_str());
  std::string value;

  if (name == "vendor") {
    value = "Google";
  } else if (name == "version") {
    status = queryProperty(wvcdm::QUERY_KEY_WVCDM_VERSION, value);
  } else if (name == "description") {
    value = "Widevine CDM";
  } else if (name == "algorithms") {
    value = "AES/CBC/NoPadding,HmacSHA256";
  } else if (name == "securityLevel") {
    std::string requestedLevel = mPropertySet.security_level();

    if (requestedLevel.length() > 0) {
      value = requestedLevel.c_str();
    } else {
      status = queryProperty(wvcdm::QUERY_KEY_SECURITY_LEVEL, value);
    }
  } else if (name == "systemId") {
    status = queryProperty(wvcdm::QUERY_KEY_SYSTEM_ID, value);
  } else if (name == "privacyMode") {
    if (mPropertySet.use_privacy_mode()) {
      value = kEnable;
    } else {
      value = kDisable;
    }
  } else if (name == "sessionSharing") {
    if (mPropertySet.is_session_sharing_enabled()) {
      value = kEnable;
    } else {
      value = kDisable;
    }
  } else if (name == "hdcpLevel") {
    status = queryProperty(wvcdm::QUERY_KEY_CURRENT_HDCP_LEVEL, value);
  } else if (name == "maxHdcpLevel") {
    status = queryProperty(wvcdm::QUERY_KEY_MAX_HDCP_LEVEL, value);
  } else if (name == "usageReportingSupport") {
    status = queryProperty(wvcdm::QUERY_KEY_USAGE_SUPPORT, value);
  } else if (name == "numberOfOpenSessions") {
    status = queryProperty(wvcdm::QUERY_KEY_NUMBER_OF_OPEN_SESSIONS, value);
  } else if (name == "maxNumberOfSessions") {
    status = queryProperty(wvcdm::QUERY_KEY_MAX_NUMBER_OF_SESSIONS, value);
  } else if (name == "oemCryptoApiVersion") {
    status = queryProperty(wvcdm::QUERY_KEY_OEMCRYPTO_API_VERSION, value);
  } else if (name == "appId") {
    value = mPropertySet.app_id().c_str();
  } else if (name == "origin") {
    value = mCdmIdentifierBuilder.origin().c_str();
  } else if (name == "CurrentSRMVersion") {
    status = queryProperty(wvcdm::QUERY_KEY_CURRENT_SRM_VERSION, value);
  } else if (name == "SRMUpdateSupport") {
    status = queryProperty(wvcdm::QUERY_KEY_SRM_UPDATE_SUPPORT, value);
  } else if (name == "resourceRatingTier") {
    status = queryProperty(wvcdm::QUERY_KEY_RESOURCE_RATING_TIER, value);
  } else if (name == "oemCryptoBuildInformation") {
    status = queryProperty(wvcdm::QUERY_KEY_OEMCRYPTO_BUILD_INFORMATION, value);
  } else if (name == "decryptHashSupport") {
    status = queryProperty(wvcdm::QUERY_KEY_DECRYPT_HASH_SUPPORT, value);
  } else if (name == "decryptHashError") {
    status = mapCdmResponseType(
        mCDM->GetDecryptHashError(mDecryptHashSessionId, &value));
  } else {
    ALOGE("App requested unknown string property %s", name.c_str());
    status = Status::ERROR_DRM_CANNOT_HANDLE;
  }

  _hidl_cb(status, value.c_str());
  return Void();
}

Return<void> WVDrmPlugin::getPropertyByteArray(
    const hidl_string& propertyName,
    getPropertyByteArray_cb _hidl_cb) {

  Status status = Status::OK;
  std::string name(propertyName.c_str());
  std::vector<uint8_t> value;

  if (name == "deviceUniqueId") {
    std::string id;
    status = mCdmIdentifierBuilder.getDeviceUniqueId(&id);
    if (status == Status::OK) {
      value = StrToVector(id);
    }
  } else if (name == "provisioningUniqueId") {
    std::string id;
    status = mCdmIdentifierBuilder.getProvisioningUniqueId(&id);
    if (status == Status::OK) {
      value = StrToVector(id);
    }
  } else if (name == "serviceCertificate") {
    value = StrToVector(mPropertySet.service_certificate());
  } else if (name == "provisioningServiceCertificate") {
    value = StrToVector(mProvisioningServiceCertificate);
  } else if (name == "metrics") {
    drm_metrics::WvCdmMetrics metrics;
    // If the cdm identifier is not yet sealed, then there are no metrics
    // for that cdm engine. Avoid calling getCdmIdentifier and sealing
    // the identifier builder.
    if (mCdmIdentifierBuilder.is_sealed()) {
      CdmIdentifier identifier;
      status = mCdmIdentifierBuilder.getCdmIdentifier(&identifier);
      if (status != Status::OK) {
        ALOGE("Unexpected error retrieving cdm identifier: %d", status);
      } else {
        status = mapCdmResponseType(mCDM->GetMetrics(identifier, &metrics));
      }
    }
    if (status == Status::OK) {
      std::string serialized_metrics;
      if (!metrics.SerializeToString(&serialized_metrics)) {
        status = Status::ERROR_DRM_UNKNOWN;
      } else {
        value = StrToVector(serialized_metrics);
      }
    }
  } else {
    ALOGE("App requested unknown byte array property %s", name.c_str());
    status = Status::ERROR_DRM_CANNOT_HANDLE;
  }

  _hidl_cb(status, toHidlVec(value));
  return Void();
}

Return<Status> WVDrmPlugin::setPropertyString(const hidl_string& propertyName,
                                              const hidl_string& value) {
  std::string name(propertyName.c_str());
  std::string _value(value.c_str());

  if (name == "securityLevel") {
    if (mCryptoSessions.size() == 0) {
      if (_value == wvcdm::QUERY_VALUE_SECURITY_LEVEL_L3.c_str()) {
        mPropertySet.set_security_level(wvcdm::QUERY_VALUE_SECURITY_LEVEL_L3);
      } else if (_value == wvcdm::QUERY_VALUE_SECURITY_LEVEL_L1.c_str()) {
        // We must be sure we CAN set the security level to L1.
        std::string current_security_level;
        Status status = queryProperty(
            wvcdm::kLevelDefault, wvcdm::QUERY_KEY_SECURITY_LEVEL,
            current_security_level);
        if (status != Status::OK) {
           return status;
        }
        if (current_security_level != wvcdm::QUERY_VALUE_SECURITY_LEVEL_L1) {
          ALOGE("App requested L1 security on a non-L1 device.");
          return Status::BAD_VALUE;
        } else {
          mPropertySet.set_security_level(kResetSecurityLevel);
        }
      } else if (_value == kResetSecurityLevel ||
                 _value == wvcdm::QUERY_VALUE_SECURITY_LEVEL_DEFAULT) {
        mPropertySet.set_security_level(kResetSecurityLevel);
      } else {
        ALOGE("App requested invalid security level %s", _value.c_str());
        return Status::BAD_VALUE;
      }
    } else {
      ALOGE("App tried to change security level while sessions are open.");
      ALOGW("Returns UNKNOWN error for legacy status kErrorSessionIsOpen");
      return Status::ERROR_DRM_UNKNOWN;
    }
  } else if (name == "privacyMode") {
    if (_value == kEnable) {
      mPropertySet.set_use_privacy_mode(true);
    } else if (_value == kDisable) {
      mPropertySet.set_use_privacy_mode(false);
    } else {
      ALOGE("App requested unknown privacy mode %s", _value.c_str());
      return Status::BAD_VALUE;
    }
  } else if (name == "sessionSharing") {
    if (mCryptoSessions.size() == 0) {
      if (_value == kEnable) {
        mPropertySet.set_is_session_sharing_enabled(true);
      } else if (_value == kDisable) {
        mPropertySet.set_is_session_sharing_enabled(false);
      } else {
        ALOGE("App requested unknown sharing type %s", _value.c_str());
        return Status::BAD_VALUE;
      }
    } else {
      ALOGE("App tried to change key sharing while sessions are open.");
      ALOGW("Returns UNKNOWN error for legacy status kErrorSessionIsOpen");
      return Status::ERROR_DRM_UNKNOWN;
    }
  } else if (name == "appId") {
    if (mCryptoSessions.size() == 0) {
      mPropertySet.set_app_id(_value.c_str());
    } else {
      ALOGE("App tried to set the application id while sessions are opened.");
      ALOGW("Returns UNKNOWN error for legacy status kErrorSessionIsOpen");
      return Status::ERROR_DRM_UNKNOWN;
    }
  } else if (name == "origin") {
    if (mCryptoSessions.size() != 0) {
      ALOGE("App tried to set the origin while sessions are opened.");
      ALOGW("Returns UNKNOWN error for legacy status kErrorSessionIsOpen");
      return Status::ERROR_DRM_UNKNOWN;
    } else {
      if (!mCdmIdentifierBuilder.set_origin(_value.c_str())) {
        return Status::BAD_VALUE;
      }
    }
  } else if (name == "decryptHash") {
    wvcdm::CdmSessionId sessionId;
    CdmResponseType res =
      mCDM->SetDecryptHash(_value.c_str(), &sessionId);

    if (wvcdm::NO_ERROR == res) mDecryptHashSessionId = sessionId;

    return mapCdmResponseType(res);
  } else if (name == "decryptHashSessionId") {
    mDecryptHashSessionId = _value.c_str();
  } else {
    ALOGE("App set unknown string property %s", name.c_str());
    return Status::ERROR_DRM_CANNOT_HANDLE;
  }

  return Status::OK;
}

Return<Status> WVDrmPlugin::setPropertyByteArray(
    const hidl_string& propertyName, const hidl_vec<uint8_t>& value) {

  std::string name(propertyName.c_str());
  std::vector<uint8_t> _value = toVector(value);

  if (name == "serviceCertificate") {
    std::string cert(_value.begin(), _value.end());
    if (_value.empty() || mCDM->IsValidServiceCertificate(cert)) {
      mPropertySet.set_service_certificate(cert);
    } else {
      return Status::BAD_VALUE;
    }
  } else if (name == "provisioningServiceCertificate") {
    std::string cert(_value.begin(), _value.end());
    if (_value.empty() || mCDM->IsValidServiceCertificate(cert)) {
      mProvisioningServiceCertificate = cert;
    } else {
      return Status::BAD_VALUE;
    }
  } else {
    ALOGE("App set unknown byte array property %s", name.c_str());
    return Status::ERROR_DRM_CANNOT_HANDLE;
  }

  return Status::OK;
}

Return<Status> WVDrmPlugin::setCipherAlgorithm(
    const hidl_vec<uint8_t>& sessionId, const hidl_string& algorithm) {

  if (sessionId.size() == 0 || algorithm.size() == 0) {
    return Status::BAD_VALUE;
  }
  std::string algo(algorithm.c_str());
  std::vector<uint8_t> sId = toVector(sessionId);

  CdmSessionId cdmSessionId(sId.begin(), sId.end());
  if (!mCryptoSessions.count(cdmSessionId)) {
    return Status::ERROR_DRM_SESSION_NOT_OPENED;
  }

  CryptoSession& cryptoSession = mCryptoSessions[cdmSessionId];

  if (algo == "AES/CBC/NoPadding") {
    cryptoSession.setCipherAlgorithm(OEMCrypto_AES_CBC_128_NO_PADDING);
  } else {
    return Status::ERROR_DRM_CANNOT_HANDLE;
  }

  return Status::OK;
}

Return<Status> WVDrmPlugin::setMacAlgorithm(
    const hidl_vec<uint8_t>& sessionId, const hidl_string& algorithm) {

  if (sessionId.size() == 0 || algorithm.size() == 0) {
    return Status::BAD_VALUE;
  }
  std::string algo(algorithm.c_str());
  std::vector<uint8_t> sId = toVector(sessionId);

  CdmSessionId cdmSessionId(sId.begin(), sId.end());
  if (!mCryptoSessions.count(cdmSessionId)) {
    return Status::ERROR_DRM_SESSION_NOT_OPENED;
  }

  CryptoSession& cryptoSession = mCryptoSessions[cdmSessionId];

  if (algo == "HmacSHA256") {
    cryptoSession.setMacAlgorithm(OEMCrypto_HMAC_SHA256);
  } else {
    return Status::ERROR_DRM_CANNOT_HANDLE;
  }

  return Status::OK;
}

Return<void> WVDrmPlugin::encrypt(
    const hidl_vec<uint8_t>& sessionId,
    const hidl_vec<uint8_t>& keyId,
    const hidl_vec<uint8_t>& input,
    const hidl_vec<uint8_t>& iv,
    encrypt_cb _hidl_cb) {

  const std::vector<uint8_t> sId = toVector(sessionId);
  std::vector<uint8_t> output;

  CdmSessionId cdmSessionId(sId.begin(), sId.end());
  if (!mCryptoSessions.count(cdmSessionId)) {
    _hidl_cb(Status::ERROR_DRM_SESSION_NOT_OPENED, toHidlVec(output));
    return Void();
  }

  const CryptoSession& cryptoSession = mCryptoSessions[cdmSessionId];

  if (cryptoSession.cipherAlgorithm() == kInvalidCryptoAlgorithm) {
    ALOGW("Returns UNKNOWN error for legacy status NO_INIT");
    _hidl_cb(Status::ERROR_DRM_UNKNOWN, toHidlVec(output));
    return Void();
  }

  const std::vector<uint8_t> _keyId = toVector(keyId);
  OEMCryptoResult res = mCrypto->selectKey(cryptoSession.oecSessionId(),
                                           _keyId.data(), _keyId.size());

  if (res != OEMCrypto_SUCCESS) {
    ALOGE("OEMCrypto_SelectKey failed with %u", res);
    _hidl_cb(mapAndNotifyOfOEMCryptoResult(sId, res),
             toHidlVec(output));
    return Void();
  }

  const std::vector<uint8_t> _input = toVector(input);
  const std::vector<uint8_t> _iv = toVector(iv);
  output.resize(_input.size());

  res = mCrypto->encrypt(cryptoSession.oecSessionId(), _input.data(),
                         _input.size(), _iv.data(),
                         cryptoSession.cipherAlgorithm(), output.data());

  if (res == OEMCrypto_SUCCESS) {
    _hidl_cb(Status::OK, toHidlVec(output));
  } else {
    ALOGE("OEMCrypto_Generic_Encrypt failed with %u", res);
    _hidl_cb(mapAndNotifyOfOEMCryptoResult(sId, res),
                      toHidlVec(output));
  }
  return Void();
}

Return<void> WVDrmPlugin::decrypt(
    const hidl_vec<uint8_t>& sessionId,
    const hidl_vec<uint8_t>& keyId,
    const hidl_vec<uint8_t>& input,
    const hidl_vec<uint8_t>& iv,
    decrypt_cb _hidl_cb) {

  const std::vector<uint8_t> sId = toVector(sessionId);
  std::vector<uint8_t> output;

  CdmSessionId cdmSessionId(sId.begin(), sId.end());
  if (!mCryptoSessions.count(cdmSessionId)) {
    _hidl_cb(Status::ERROR_DRM_SESSION_NOT_OPENED, toHidlVec(output));
    return Void();
  }

  const CryptoSession& cryptoSession = mCryptoSessions[cdmSessionId];

  if (cryptoSession.cipherAlgorithm() == kInvalidCryptoAlgorithm) {
    ALOGW("Returns UNKNOWN error for legacy status NO_INIT");
    _hidl_cb(Status::ERROR_DRM_UNKNOWN, toHidlVec(output));
    return Void();
  }

  const std::vector<uint8_t> _keyId = toVector(keyId);
  OEMCryptoResult res = mCrypto->selectKey(cryptoSession.oecSessionId(),
                                           _keyId.data(), _keyId.size());

  if (res != OEMCrypto_SUCCESS) {
    ALOGE("OEMCrypto_SelectKey failed with %u", res);
    _hidl_cb(mapAndNotifyOfOEMCryptoResult(sId, res),
             toHidlVec(output));
    return Void();
  }

  const std::vector<uint8_t> _input = toVector(input);
  const std::vector<uint8_t> _iv = toVector(iv);
  output.resize(_input.size());

  res = mCrypto->decrypt(cryptoSession.oecSessionId(), _input.data(),
                         _input.size(), _iv.data(),
                         cryptoSession.cipherAlgorithm(), output.data());

  if (res == OEMCrypto_SUCCESS) {
    _hidl_cb(Status::OK, toHidlVec(output));
  } else {
    ALOGE("OEMCrypto_Generic_Decrypt failed with %u", res);
    _hidl_cb(mapAndNotifyOfOEMCryptoResult(sId, res),
             toHidlVec(output));
  }
  return Void();
}

Return<void> WVDrmPlugin::sign(
    const hidl_vec<uint8_t>& sessionId,
    const hidl_vec<uint8_t>& keyId,
    const hidl_vec<uint8_t>& message,
    sign_cb _hidl_cb) {

  const std::vector<uint8_t> sId = toVector(sessionId);
  std::vector<uint8_t> signature;

  CdmSessionId cdmSessionId(sId.begin(), sId.end());
  if (!mCryptoSessions.count(cdmSessionId)) {
    _hidl_cb(Status::ERROR_DRM_SESSION_NOT_OPENED, toHidlVec(signature));
    return Void();
  }

  const CryptoSession& cryptoSession = mCryptoSessions[cdmSessionId];

  if (cryptoSession.macAlgorithm() == kInvalidCryptoAlgorithm) {
    ALOGW("Returns UNKNOWN error for legacy status NO_INIT");
    _hidl_cb(Status::ERROR_DRM_UNKNOWN, toHidlVec(signature));
    return Void();
  }

  const std::vector<uint8_t> _keyId = toVector(keyId);
  OEMCryptoResult res = mCrypto->selectKey(cryptoSession.oecSessionId(),
                                           _keyId.data(), _keyId.size());

  if (res != OEMCrypto_SUCCESS) {
    ALOGE("OEMCrypto_SelectKey failed with %u", res);
    _hidl_cb(mapAndNotifyOfOEMCryptoResult(sId, res),
                      toHidlVec(signature));
    return Void();
  }

  size_t signatureSize = 0;

  const std::vector<uint8_t> msg = toVector(message);
  res = mCrypto->sign(cryptoSession.oecSessionId(), msg.data(),
                      msg.size(), cryptoSession.macAlgorithm(),
                      NULL, &signatureSize);

  if (res != OEMCrypto_ERROR_SHORT_BUFFER) {
    ALOGE("OEMCrypto_Generic_Sign failed with %u when requesting signature "
          "size", res);
    if (res != OEMCrypto_SUCCESS) {
      _hidl_cb(mapAndNotifyOfOEMCryptoResult(sId, res),
                        toHidlVec(signature));
    } else {
      _hidl_cb(Status::ERROR_DRM_UNKNOWN, toHidlVec(signature));
    }
    return Void();
  }

  signature.resize(signatureSize);

  res = mCrypto->sign(cryptoSession.oecSessionId(), msg.data(),
                      msg.size(), cryptoSession.macAlgorithm(),
                      signature.data(), &signatureSize);

  if (res == OEMCrypto_SUCCESS) {
    _hidl_cb(Status::OK, toHidlVec(signature));
  } else {
    ALOGE("OEMCrypto_Generic_Sign failed with %u", res);
    _hidl_cb(mapAndNotifyOfOEMCryptoResult(sId, res),
             toHidlVec(signature));
  }
  return Void();
}

Return<void> WVDrmPlugin::verify(
    const hidl_vec<uint8_t>& sessionId,
    const hidl_vec<uint8_t>& keyId,
    const hidl_vec<uint8_t>& message,
    const hidl_vec<uint8_t>& signature,
    verify_cb _hidl_cb) {

  bool match = false;
  const std::vector<uint8_t> sId = toVector(sessionId);

  CdmSessionId cdmSessionId(sId.begin(), sId.end());
  if (!mCryptoSessions.count(cdmSessionId)) {
    _hidl_cb(Status::ERROR_DRM_SESSION_NOT_OPENED, match);
    return Void();
  }

  const CryptoSession& cryptoSession = mCryptoSessions[cdmSessionId];

  if (cryptoSession.macAlgorithm() == kInvalidCryptoAlgorithm) {
    ALOGW("Returns UNKNOWN error for legacy status NO_INIT");
    _hidl_cb(Status::ERROR_DRM_UNKNOWN, match);
    return Void();
  }

  const std::vector<uint8_t> _keyId = toVector(keyId);
  OEMCryptoResult res = mCrypto->selectKey(cryptoSession.oecSessionId(),
                                           _keyId.data(), _keyId.size());

  if (res != OEMCrypto_SUCCESS) {
    ALOGE("OEMCrypto_SelectKey failed with %u", res);
    _hidl_cb(mapAndNotifyOfOEMCryptoResult(sId, res), match);
    return Void();
  }

  const std::vector<uint8_t> _message = toVector(message);
  const std::vector<uint8_t> _signature = toVector(signature);
  res = mCrypto->verify(cryptoSession.oecSessionId(), _message.data(),
                        _message.size(), cryptoSession.macAlgorithm(),
                        _signature.data(), _signature.size());

  if (res == OEMCrypto_SUCCESS) {
    match = true;
    _hidl_cb(Status::OK, match);
  } else if (res == OEMCrypto_ERROR_SIGNATURE_FAILURE) {
    match = false;
    _hidl_cb(Status::OK, match);
  } else {
    ALOGE("OEMCrypto_Generic_Verify failed with %u", res);
    _hidl_cb(mapAndNotifyOfOEMCryptoResult(sId, res), match);
  }
  return Void();
}

Return<void> WVDrmPlugin::signRSA(
    const hidl_vec<uint8_t>& sessionId,
    const hidl_string& algorithm,
    const hidl_vec<uint8_t>& message,
    const hidl_vec<uint8_t>& wrappedKey,
    signRSA_cb _hidl_cb) {

  if (sessionId.size() == 0 || algorithm.size() == 0 ||
      message.size() == 0 || wrappedKey.size() == 0) {
    _hidl_cb(Status::BAD_VALUE, hidl_vec<uint8_t>());
    return Void();
  }
  const std::string algo(algorithm.c_str());
  std::vector<uint8_t> signature;

  RSA_Padding_Scheme padding_scheme;
  if (algo == "RSASSA-PSS-SHA1") {
    padding_scheme = kSign_RSASSA_PSS;
  } else if (algo == "PKCS1-BlockType1") {
    padding_scheme = kSign_PKCS1_Block1;
  } else {
    ALOGE("Unknown RSA Algorithm %s", algo.c_str());
    _hidl_cb(Status::ERROR_DRM_CANNOT_HANDLE, toHidlVec(signature));
    return Void();
  }

  const std::vector<uint8_t>  msg = toVector(message);
  const std::vector<uint8_t>  _wrappedKey = toVector(wrappedKey);
  OEMCryptoResult res = mCrypto->signRSA(_wrappedKey.data(),
                                         _wrappedKey.size(),
                                         msg.data(), msg.size(),
                                         signature,
                                         padding_scheme);

  if (res != OEMCrypto_SUCCESS) {
    ALOGE("OEMCrypto_GenerateRSASignature failed with %u", res);
    _hidl_cb(mapOEMCryptoResult(res), toHidlVec(signature));
    return Void();
  }

  _hidl_cb(Status::OK, toHidlVec(signature));
  return Void();
}

Return<void> WVDrmPlugin::setListener(const sp<IDrmPluginListener>& listener) {
  mListener = listener;
  mListenerV1_2 = IDrmPluginListener_V1_2::castFrom(listener);
  return Void();
}

Return<void> WVDrmPlugin::sendEvent(
    EventType eventType,
    const hidl_vec<uint8_t>& sessionId,
    const hidl_vec<uint8_t>& data) {
  if (mListenerV1_2 != NULL) {
    mListenerV1_2->sendEvent(eventType, sessionId, data);
  } else if (mListener != NULL) {
    mListener->sendEvent(eventType, sessionId, data);
  } else {
    ALOGE("Null event listener, event not sent");
  }
  return Void();
}

Return<void> WVDrmPlugin::sendExpirationUpdate(
    const hidl_vec<uint8_t>& sessionId,
        int64_t expiryTimeInMS) {
  if (mListenerV1_2 != NULL) {
    mListenerV1_2->sendExpirationUpdate(sessionId, expiryTimeInMS);
  } else if (mListener != NULL) {
      mListener->sendExpirationUpdate(sessionId, expiryTimeInMS);
  } else {
    ALOGE("Null event listener, event not sent");
  }
  return Void();
}

template<>
void WVDrmPlugin::_sendKeysChange(
      const hidl_vec<uint8_t>& sessionId,
      const hidl_vec<KeyStatus>& keyStatusList,
      bool hasNewUsableKey) {
  mListener->sendKeysChange(sessionId, keyStatusList, hasNewUsableKey);
}

template<>
void WVDrmPlugin::_sendKeysChange(
      const hidl_vec<uint8_t>& sessionId,
      const hidl_vec<KeyStatus_V1_2>& keyStatusList,
      bool hasNewUsableKey) {
  mListenerV1_2->sendKeysChange_1_2(sessionId, keyStatusList, hasNewUsableKey);
}

Return<void> WVDrmPlugin::sendKeysChange(
     const hidl_vec<uint8_t>& sessionId,
     const hidl_vec<KeyStatus>& keyStatusList, bool hasNewUsableKey) {
  if (mListenerV1_2 != NULL) {
      mListenerV1_2->sendKeysChange(sessionId, keyStatusList, hasNewUsableKey);
  } else if (mListener != NULL) {
      mListener->sendKeysChange(sessionId, keyStatusList, hasNewUsableKey);
  } else {
    ALOGE("Null event listener, event not sent");
  }
  return Void();
}

Return<void> WVDrmPlugin::sendKeysChange_1_2(
     const hidl_vec<uint8_t>& sessionId,
     const hidl_vec<KeyStatus_V1_2>& keyStatusList, bool hasNewUsableKey) {
  if (mListenerV1_2 != NULL) {
      mListenerV1_2->sendKeysChange_1_2(sessionId, keyStatusList, hasNewUsableKey);
  } else {
    ALOGE("Null event listener, event not sent");
  }
  return Void();
}

Return<void> WVDrmPlugin::sendSessionLostState(
    const hidl_vec<uint8_t>& sessionId) {
  if (mListenerV1_2 != NULL) {
    mListenerV1_2->sendSessionLostState(sessionId);
  } else {
    ALOGE("Null event listener, event not sent");
  }
  return Void();
}

void WVDrmPlugin::OnSessionRenewalNeeded(const CdmSessionId& cdmSessionId) {
  const std::vector<uint8_t> sessionId = StrToVector(cdmSessionId);
  const hidl_vec<uint8_t> data;  // data is ignored
  const hidl_vec<uint8_t> sid = toHidlVec(sessionId);
  sendEvent(EventType::KEY_NEEDED, sid, data);
}

void WVDrmPlugin::OnSessionKeysChange(const CdmSessionId& cdmSessionId,
                                      const CdmKeyStatusMap& cdmKeysStatus,
                                      bool hasNewUsableKey) {
  if (mListenerV1_2 != NULL) {
    _OnSessionKeysChange<KeyStatus_V1_2>(cdmSessionId, cdmKeysStatus, hasNewUsableKey);
  } else if (mListener != NULL) {
    _OnSessionKeysChange<KeyStatus>(cdmSessionId, cdmKeysStatus, hasNewUsableKey);
  } else {
    ALOGE("Null event listener, event not sent");
  }
}

template<typename KS>
void WVDrmPlugin::_OnSessionKeysChange(const CdmSessionId& cdmSessionId,
                                       const CdmKeyStatusMap& cdmKeysStatus,
                                       bool hasNewUsableKey) {
  bool expired = false;
  std::vector<KS> keyStatusList;
  for (CdmKeyStatusMap::const_iterator it = cdmKeysStatus.begin();
       it != cdmKeysStatus.end(); ++it) {
    const KeyId& keyId = it->first;
    const CdmKeyStatus cdmKeyStatus = it->second;
    if (cdmKeyStatus == wvcdm::kKeyStatusExpired) expired = true;

    KS keyStatus;
    keyStatus.keyId = toHidlVec(StrToVector(keyId));
    keyStatus.type = ConvertFromCdmKeyStatus<decltype(keyStatus.type)>(cdmKeyStatus);
    keyStatusList.push_back(keyStatus);
  }

  const std::vector<uint8_t> sessionId = StrToVector(cdmSessionId);
  const hidl_vec<uint8_t> data;  // data is ignored
  const hidl_vec<uint8_t> sid = toHidlVec(sessionId);
  _sendKeysChange<KS>(sid, toHidlVec(keyStatusList), hasNewUsableKey);
  // For backward compatibility.
  if (expired) {
    sendEvent(EventType::KEY_EXPIRED, sid, data);
  }
}

void WVDrmPlugin::OnExpirationUpdate(const CdmSessionId& cdmSessionId,
                                     int64_t newExpiryTimeSeconds) {
  const std::vector<uint8_t> sessionId = StrToVector(cdmSessionId);
  int64_t newExpiryTimeMilliseconds =
      newExpiryTimeSeconds == wvcdm::NEVER_EXPIRES
      ? newExpiryTimeSeconds : newExpiryTimeSeconds * 1000;

  sendExpirationUpdate(toHidlVec(sessionId), newExpiryTimeMilliseconds);
}

void WVDrmPlugin::OnSessionLostState(const CdmSessionId& cdmSessionId) {
  const std::vector<uint8_t> sessionId = StrToVector(cdmSessionId);
  sendSessionLostState(toHidlVec(sessionId));
}

Status WVDrmPlugin::queryProperty(const std::string& property,
                                    std::string& stringValue) const {
  wvcdm::SecurityLevel securityLevel =
      mPropertySet.security_level().compare(
          wvcdm::QUERY_VALUE_SECURITY_LEVEL_L3) == 0
          ? wvcdm::kLevel3
          : wvcdm::kLevelDefault;
  return queryProperty(securityLevel, property, stringValue);
}

Status WVDrmPlugin::queryProperty(wvcdm::SecurityLevel securityLevel,
                                    const std::string& property,
                                    std::string& stringValue) const {
  CdmResponseType res =
      mCDM->QueryStatus(securityLevel, property, &stringValue);

  if (res != wvcdm::NO_ERROR) {
    ALOGE("Error querying CDM status: %u", res);
  }
  return mapCdmResponseType(res);
}

Status WVDrmPlugin::queryProperty(const std::string& property,
                                    std::vector<uint8_t>& vector_value) const {
  std::string string_value;
  Status status = queryProperty(property, string_value);
  if (status != Status::OK) return status;
  vector_value = StrToVector(string_value);
  return Status::OK;
}

Status WVDrmPlugin::mapAndNotifyOfCdmResponseType(
    const std::vector<uint8_t>& sessionId, CdmResponseType res) {
  notifyOfCdmResponseType(sessionId, res);
  return mapCdmResponseType(res);
}

Status_V1_2 WVDrmPlugin::mapAndNotifyOfCdmResponseType_1_2(
    const std::vector<uint8_t>& sessionId, CdmResponseType res) {
  notifyOfCdmResponseType(sessionId, res);
  return mapCdmResponseType_1_2(res);
}

void WVDrmPlugin::notifyOfCdmResponseType(
    const std::vector<uint8_t>& sessionId,
    CdmResponseType res) {

  const hidl_vec<uint8_t> data;  // data is ignored
  if (res == wvcdm::NEED_PROVISIONING) {
    sendEvent(EventType::PROVISION_REQUIRED, toHidlVec(sessionId), data);
  } else if (res == wvcdm::NEED_KEY) {
    sendEvent(EventType::KEY_NEEDED, toHidlVec(sessionId), data);
  }
}

Status WVDrmPlugin::mapAndNotifyOfOEMCryptoResult(
    const std::vector<uint8_t>& sessionId,
    OEMCryptoResult res) {

  const hidl_vec<uint8_t> data;  // data is ignored
  if (res == OEMCrypto_ERROR_NO_DEVICE_KEY) {
    sendEvent(EventType::PROVISION_REQUIRED, toHidlVec(sessionId), data);
  }
  return mapOEMCryptoResult(res);
}

Status WVDrmPlugin::mapOEMCryptoResult(OEMCryptoResult res) {
  switch (res) {
    case OEMCrypto_SUCCESS:
      return Status::OK;

    case OEMCrypto_ERROR_SIGNATURE_FAILURE:
      return Status::ERROR_DRM_INVALID_STATE;

    case OEMCrypto_ERROR_NO_DEVICE_KEY:
      return Status::ERROR_DRM_NOT_PROVISIONED;

    case OEMCrypto_ERROR_INVALID_SESSION:
      return Status::ERROR_DRM_SESSION_NOT_OPENED;

    case OEMCrypto_ERROR_TOO_MANY_SESSIONS:
    case OEMCrypto_ERROR_INSUFFICIENT_RESOURCES:
      return Status::ERROR_DRM_RESOURCE_BUSY;

    case OEMCrypto_ERROR_NOT_IMPLEMENTED:
      return Status::ERROR_DRM_CANNOT_HANDLE;

    case OEMCrypto_ERROR_INVALID_RSA_KEY:
    case OEMCrypto_ERROR_SHORT_BUFFER:
    case OEMCrypto_ERROR_UNKNOWN_FAILURE:
    case OEMCrypto_ERROR_OPEN_SESSION_FAILED:
      FALLTHROUGH_INTENDED; /* FALLTHROUGH */
    default:
      ALOGW("Returns UNKNOWN error for legacy status: %d", res);
      return Status::ERROR_DRM_UNKNOWN;
  }
}

bool WVDrmPlugin::initDataResemblesPSSH(const std::vector<uint8_t>& initData) {
  const uint8_t* const initDataArray = initData.data();

  if (sizeof(uint32_t) + kPsshTag.size() > initData.size()) {
    // The init data is so small that it couldn't contain a size and PSSH tag.
    return false;
  }

  // Extract the size field
  const uint8_t* const sizeField = &initDataArray[0];
  uint32_t nboSize;
  memcpy(&nboSize, sizeField, sizeof(nboSize));
  uint32_t size = ntohl(nboSize);

  if (size > initData.size()) {
    return false;
  }

  // Extract the ID field
  const char* const idField =
      reinterpret_cast<const char* const>(&initDataArray[sizeof(nboSize)]);
  std::string id(idField, kPsshTag.size());
  return id == kPsshTag;
}

Status WVDrmPlugin::unprovision(const CdmIdentifier& identifier) {
  CdmResponseType res1 = mCDM->Unprovision(wvcdm::kSecurityLevelL1, identifier);
  CdmResponseType res3 = mCDM->Unprovision(wvcdm::kSecurityLevelL3, identifier);
  if (!isCdmResponseTypeSuccess(res1))
  {
    return mapCdmResponseType(res1);
  }
  else
  {
    return mapCdmResponseType(res3);
  }
}

// Implementation for the CdmIdentifierBuilder inner class
WVDrmPlugin::CdmIdentifierBuilder::CdmIdentifierBuilder(
    bool useSpoid, const WVDrmPlugin& parent, const std::string& appPackageName)
  : mCdmIdentifier(),
    mIsIdentifierSealed(false),
    mUseSpoid(useSpoid),
    mAppPackageName(appPackageName),
    mParent(parent) {
  mCdmIdentifier.app_package_name = mAppPackageName;
  mCdmIdentifier.unique_id = getNextUniqueId();
}

Status WVDrmPlugin::CdmIdentifierBuilder::getCdmIdentifier(
    CdmIdentifier* identifier) {
  if (!mIsIdentifierSealed) {
    Status res = calculateSpoid();
    if (res != Status::OK) return res;
    mIsIdentifierSealed = true;
  }
  *identifier = mCdmIdentifier;
  return Status::OK;
}

Status WVDrmPlugin::CdmIdentifierBuilder::getDeviceUniqueId(std::string* id) {
  if (mUseSpoid) {
    CdmIdentifier identifier;
    Status res = getCdmIdentifier(&identifier);
    if (res != Status::OK) return res;

    *id = identifier.spoid;
    return Status::OK;
  } else {
    return getOemcryptoDeviceId(id);
  }
}

Status WVDrmPlugin::CdmIdentifierBuilder::getProvisioningUniqueId(std::string* id) {
  if (mUseSpoid) {
    // To fake a provisioning-unique ID on SPOID devices where we can't expose
    // the real provisioning-unique ID, we just use the SPOID and invert all the
    // bits.
    Status res = getDeviceUniqueId(id);
    if (res != Status::OK) return res;

    for (char& c : *id) {
      c = ~c;
    }

    return Status::OK;
  } else {
    return mParent.queryProperty(wvcdm::QUERY_KEY_PROVISIONING_ID, *id);
  }
}

bool WVDrmPlugin::CdmIdentifierBuilder::set_origin(const std::string& id) {
  if (mIsIdentifierSealed) return false;
  mCdmIdentifier.origin = id;
  return true;
}

Status WVDrmPlugin::CdmIdentifierBuilder::calculateSpoid() {
  if (mUseSpoid) {
    std::string deviceId;
    Status res = getOemcryptoDeviceId(&deviceId);
    if (res != Status::OK) return res;

    uint8_t hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, deviceId.data(), deviceId.length());
    SHA256_Update(&ctx, mAppPackageName.data(), mAppPackageName.length());
    SHA256_Update(&ctx, origin().data(), origin().length());
    SHA256_Final(hash, &ctx);

    mCdmIdentifier.spoid =
        std::string(reinterpret_cast<char*>(hash), SHA256_DIGEST_LENGTH);
  }
  return Status::OK;
}

Status WVDrmPlugin::CdmIdentifierBuilder::getOemcryptoDeviceId(
    std::string* id) {
  return mParent.queryProperty(wvcdm::QUERY_KEY_DEVICE_ID, *id);
}

uint32_t WVDrmPlugin::CdmIdentifierBuilder::getNextUniqueId() {
  // Start with 1. 0 is reserved for the default cdm identifier.
  static uint32_t unique_id = 1;
  return ++unique_id;
}

}  // namespace widevine
}  // namespace V1_2
}  // namespace drm
}  // namespace hardware
}  // namespace wvdrm
