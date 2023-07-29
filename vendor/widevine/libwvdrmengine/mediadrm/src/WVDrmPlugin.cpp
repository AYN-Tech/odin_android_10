//
// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//

//#define LOG_NDEBUG 0
#define LOG_TAG "WVCdm"
#include <utils/Log.h>

#include "WVDrmPlugin.h"

#include <endian.h>
#include <string.h>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <vector>

#include "mapErrors-inl.h"
#include "media/stagefright/MediaErrors.h"
#include "utils/Errors.h"
#include "wv_cdm_constants.h"

namespace {
  static const char* const kResetSecurityLevel = "";
  static const char* const kEnable = "enable";
  static const char* const kDisable = "disable";
  static const std::string kPsshTag = "pssh";
  static const char* const kSpecialUnprovisionResponse = "unprovision";
}

namespace wvdrm {

using namespace android;
using namespace std;
using namespace wvcdm;

namespace {

Vector<uint8_t> ToVector(const std::string& str) {
  Vector<uint8_t> vector;
  vector.appendArray(reinterpret_cast<const uint8_t*>(str.data()), str.size());
  return vector;
}

DrmPlugin::KeyRequestType ConvertFromCdmKeyRequestType(
    CdmKeyRequestType keyRequestType) {
  switch (keyRequestType) {
    case kKeyRequestTypeInitial:
      return DrmPlugin::kKeyRequestType_Initial;
    case kKeyRequestTypeRenewal:
      return DrmPlugin::kKeyRequestType_Renewal;
    case kKeyRequestTypeRelease:
      return DrmPlugin::kKeyRequestType_Release;
    default:
      return DrmPlugin::kKeyRequestType_Unknown;
  }
}

DrmPlugin::KeyStatusType ConvertFromCdmKeyStatus(CdmKeyStatus keyStatus) {
  switch (keyStatus) {
    case kKeyStatusUsable:
      return DrmPlugin::kKeyStatusType_Usable;
    case kKeyStatusExpired:
      return DrmPlugin::kKeyStatusType_Expired;
    case kKeyStatusOutputNotAllowed:
      return DrmPlugin::kKeyStatusType_OutputNotAllowed;
    case kKeyStatusPending:
    case kKeyStatusUsableInFuture:
      return DrmPlugin::kKeyStatusType_StatusPending;
    case kKeyStatusInternalError:
    default:
      return DrmPlugin::kKeyStatusType_InternalError;
  }
}

}  // namespace

WVDrmPlugin::WVDrmPlugin(const sp<WvContentDecryptionModule>& cdm,
                         WVGenericCryptoInterface* crypto)
  : mCDM(cdm),
    mCrypto(crypto),
    mCryptoSessions(),
    mCdmIdentifier(kDefaultCdmIdentifier) {}

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
}

status_t WVDrmPlugin::openSession(Vector<uint8_t>& sessionId) {
  CdmSessionId cdmSessionId;
  CdmResponseType res =
      mCDM->OpenSession("com.widevine", &mPropertySet, mCdmIdentifier, this,
                        &cdmSessionId);

  if (!isCdmResponseTypeSuccess(res)) {
    return mapAndNotifyOfCdmResponseType(sessionId, res);
  }

  bool success = false;

  // Construct a CryptoSession
  CdmQueryMap info;
  res = mCDM->QueryOemCryptoSessionId(cdmSessionId, &info);

  if (isCdmResponseTypeSuccess(res) &&
      info.count(QUERY_KEY_OEMCRYPTO_SESSION_ID)) {
    OEMCrypto_SESSION oecSessionId;
    istringstream(info[QUERY_KEY_OEMCRYPTO_SESSION_ID]) >> oecSessionId;
    mCryptoSessions[cdmSessionId] = CryptoSession(oecSessionId);
    success = true;
  } else {
    ALOGE("Unable to query key control info.");
  }

  if (success) {
    // Marshal Session ID
    sessionId = ToVector(cdmSessionId);

    return android::OK;
  } else {
    mCDM->CloseSession(cdmSessionId);

    if (!isCdmResponseTypeSuccess(res)) {
      // We got an error code we can return.
      return mapAndNotifyOfCdmResponseType(sessionId, res);
    } else {
      // We got a failure that did not give us an error code, such as a failure
      // of AttachEventListener() or the key being missing from the map.
      return kErrorCDMGeneric;
    }
  }
}

status_t WVDrmPlugin::closeSession(const Vector<uint8_t>& sessionId) {
  if (!sessionId.size()) {
    return android::BAD_VALUE;
  }
  CdmSessionId cdmSessionId(sessionId.begin(), sessionId.end());
  CdmResponseType res = mCDM->CloseSession(cdmSessionId);
  mCryptoSessions.erase(cdmSessionId);
  if (!isCdmResponseTypeSuccess(res)) {
    return android::ERROR_DRM_SESSION_NOT_OPENED;
  }
  return android::OK;
}

status_t WVDrmPlugin::getKeyRequest(
    const Vector<uint8_t>& scope,
    const Vector<uint8_t>& initData,
    const String8& initDataType,
    KeyType keyType,
    const KeyedVector<String8, String8>& optionalParameters,
    Vector<uint8_t>& request,
    String8& defaultUrl,
    KeyRequestType *keyRequestType) {
  CdmLicenseType cdmLicenseType;
  CdmSessionId cdmSessionId;
  CdmKeySetId cdmKeySetId;
  if (!scope.size()) {
    return android::BAD_VALUE;
  }
  if (keyType == kKeyType_Offline) {
    cdmLicenseType = kLicenseTypeOffline;
    cdmSessionId.assign(scope.begin(), scope.end());
  } else if (keyType == kKeyType_Streaming) {
    cdmLicenseType = kLicenseTypeStreaming;
    cdmSessionId.assign(scope.begin(), scope.end());
  } else if (keyType == kKeyType_Release) {
    cdmLicenseType = kLicenseTypeRelease;
    cdmKeySetId.assign(scope.begin(), scope.end());
  } else {
    return android::ERROR_DRM_CANNOT_HANDLE;
  }

  string cdmInitDataType = initDataType.string();
  // Provide backwards-compatibility for apps that pass non-EME-compatible MIME
  // types.
  if (!WvContentDecryptionModule::IsSupported(cdmInitDataType)) {
    cdmInitDataType = wvcdm::ISO_BMFF_VIDEO_MIME_TYPE;
  }

  CdmInitData processedInitData;
  if (initData.size() > 0 &&
      WvContentDecryptionModule::IsCenc(cdmInitDataType) &&
      !initDataResemblesPSSH(initData)) {
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
    processedInitData.append(reinterpret_cast<const char*>(initData.array()),
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
    processedInitData.assign(reinterpret_cast<const char*>(initData.array()),
                             initData.size());
  }

  CdmAppParameterMap cdmParameters;
  for (size_t i = 0; i < optionalParameters.size(); ++i) {
    const String8& key = optionalParameters.keyAt(i);
    const String8& value = optionalParameters.valueAt(i);

    string cdmKey(key.string(), key.size());
    string cdmValue(value.string(), value.size());

    cdmParameters[cdmKey] = cdmValue;
  }

  CdmKeyRequest keyRequest;
  CdmResponseType res = mCDM->GenerateKeyRequest(
      cdmSessionId, cdmKeySetId, cdmInitDataType, processedInitData,
      cdmLicenseType, cdmParameters, &mPropertySet, mCdmIdentifier,
      &keyRequest);

  *keyRequestType = ConvertFromCdmKeyRequestType(keyRequest.type);

  if (isCdmResponseTypeSuccess(res)) {
    defaultUrl.clear();
    defaultUrl.setTo(keyRequest.url.data(), keyRequest.url.size());

    request = ToVector(keyRequest.message);
  }

  if (keyType == kKeyType_Release) {
    // When releasing keys, we do not have a session ID.
    return mapCdmResponseType(res);
  } else {
    // For all other requests, we have a session ID.
    return mapAndNotifyOfCdmResponseType(scope, res);
  }
}

status_t WVDrmPlugin::provideKeyResponse(
    const Vector<uint8_t>& scope,
    const Vector<uint8_t>& response,
    Vector<uint8_t>& keySetId) {
  if (scope.size() == 0 || response.size() == 0) {
    return android::BAD_VALUE;
  }
  CdmSessionId cdmSessionId;
  CdmKeyResponse cdmResponse(response.begin(), response.end());
  CdmKeySetId cdmKeySetId;

  bool isRequest = (memcmp(scope.array(), SESSION_ID_PREFIX,
                           sizeof(SESSION_ID_PREFIX) - 1) == 0);
  bool isRelease = (memcmp(scope.array(), KEY_SET_ID_PREFIX,
                           sizeof(KEY_SET_ID_PREFIX) - 1) == 0);

  if (isRequest) {
    cdmSessionId.assign(scope.begin(), scope.end());
  } else if (isRelease) {
    cdmKeySetId.assign(scope.begin(), scope.end());
  } else {
    return android::ERROR_DRM_CANNOT_HANDLE;
  }

  CdmResponseType res = mCDM->AddKey(cdmSessionId, cdmResponse, &cdmKeySetId);

  if (isRequest && isCdmResponseTypeSuccess(res)) {
    keySetId = ToVector(cdmKeySetId);
  }

  if (isRelease) {
    // When releasing keys, we do not have a session ID.
    return mapCdmResponseType(res);
  } else {
    // For all other requests, we have a session ID.
    status_t status = mapAndNotifyOfCdmResponseType(scope, res);
    // For "NEED_KEY," we still want to send the notifcation, but then we don't
    // return the error.  This is because "NEED_KEY" from AddKey() is an
    // expected behavior when sending a privacy certificate.
    if (res == wvcdm::NEED_KEY && mPropertySet.use_privacy_mode()) {
      status = android::OK;
    }
    return status;
  }
}

status_t WVDrmPlugin::removeKeys(const Vector<uint8_t>& sessionId) {
  if (!sessionId.size()) {
    return android::BAD_VALUE;
  }
  CdmSessionId cdmSessionId(sessionId.begin(), sessionId.end());

  CdmResponseType res = mCDM->RemoveKeys(cdmSessionId);

  return mapAndNotifyOfCdmResponseType(sessionId, res);
}

status_t WVDrmPlugin::restoreKeys(const Vector<uint8_t>& sessionId,
                                  const Vector<uint8_t>& keySetId) {
  if (sessionId.size() == 0 || keySetId.size() == 0) {
    return android::BAD_VALUE;
  }
  CdmSessionId cdmSessionId(sessionId.begin(), sessionId.end());
  CdmKeySetId cdmKeySetId(keySetId.begin(), keySetId.end());

  CdmResponseType res = mCDM->RestoreKey(cdmSessionId, cdmKeySetId);

  return mapAndNotifyOfCdmResponseType(sessionId, res);
}

status_t WVDrmPlugin::queryKeyStatus(
    const Vector<uint8_t>& sessionId,
    KeyedVector<String8, String8>& infoMap) const {
  if (sessionId.size() == 0) {
    return android::BAD_VALUE;
  }
  CdmSessionId cdmSessionId(sessionId.begin(), sessionId.end());
  CdmQueryMap cdmLicenseInfo;

  CdmResponseType res = mCDM->QueryKeyStatus(cdmSessionId, &cdmLicenseInfo);

  if (isCdmResponseTypeSuccess(res)) {
    infoMap.clear();
    for (CdmQueryMap::const_iterator iter = cdmLicenseInfo.begin();
         iter != cdmLicenseInfo.end();
         ++iter) {
      const string& cdmKey = iter->first;
      const string& cdmValue = iter->second;

      String8 key(cdmKey.data(), cdmKey.size());
      String8 value(cdmValue.data(), cdmValue.size());

      infoMap.add(key, value);
    }
  }

  return mapCdmResponseType(res);
}

status_t WVDrmPlugin::getProvisionRequest(const String8& cert_type,
                                          const String8& cert_authority,
                                          Vector<uint8_t>& request,
                                          String8& defaultUrl) {
  CdmProvisioningRequest cdmProvisionRequest;
  string cdmDefaultUrl;

  CdmCertificateType cdmCertType = kCertificateWidevine;
  if (cert_type == "X.509") {
    cdmCertType = kCertificateX509;
  }

  string cdmCertAuthority = cert_authority.string();

  CdmResponseType res = mCDM->GetProvisioningRequest(
      cdmCertType, cdmCertAuthority, mCdmIdentifier,
      mProvisioningServiceCertificate, &cdmProvisionRequest,
      &cdmDefaultUrl);

  if (isCdmResponseTypeSuccess(res)) {
    request = ToVector(cdmProvisionRequest);
    defaultUrl.clear();
    defaultUrl.setTo(cdmDefaultUrl.data(), cdmDefaultUrl.size());
  }

  return mapCdmResponseType(res);
}

status_t WVDrmPlugin::provideProvisionResponse(
    const Vector<uint8_t>& response,
    Vector<uint8_t>& certificate,
    Vector<uint8_t>& wrapped_key) {
  if (!response.size()) {
    return android::BAD_VALUE;
  }
  CdmProvisioningResponse cdmResponse(response.begin(), response.end());
  if (cdmResponse == kSpecialUnprovisionResponse) {
    if (mCdmIdentifier.IsEquivalentToDefault()) {
      return kErrorNoOriginSpecified;
    }
    return unprovision(mCdmIdentifier);
  } else {
    string cdmCertificate;
    string cdmWrappedKey;
    CdmResponseType res = mCDM->HandleProvisioningResponse(mCdmIdentifier,
                                                           cdmResponse,
                                                           &cdmCertificate,
                                                           &cdmWrappedKey);
    if (isCdmResponseTypeSuccess(res)) {
      certificate = ToVector(cdmCertificate);
      wrapped_key = ToVector(cdmWrappedKey);
    }

    return mapCdmResponseType(res);
  }
}

status_t WVDrmPlugin::unprovisionDevice() {
  return unprovision(kDefaultCdmIdentifier);
}

status_t WVDrmPlugin::getSecureStop(const Vector<uint8_t>& ssid,
                                    Vector<uint8_t>& secureStop) {
  if (!ssid.size()) {
    return android::BAD_VALUE;
  }
  CdmUsageInfo cdmUsageInfo;
  CdmSecureStopId cdmSsid(ssid.begin(), ssid.end());
  CdmResponseType res = mCDM->GetUsageInfo(
      mPropertySet.app_id(), cdmSsid, mCdmIdentifier, &cdmUsageInfo);
  if (isCdmResponseTypeSuccess(res)) {
    secureStop.clear();
    for (CdmUsageInfo::const_iterator iter = cdmUsageInfo.begin();
         iter != cdmUsageInfo.end();
         ++iter) {
      const string& cdmStop = *iter;

      secureStop.appendArray(reinterpret_cast<const uint8_t*>(cdmStop.data()),
                       cdmStop.size());
    }
  }
  return mapCdmResponseType(res);
}

status_t WVDrmPlugin::getSecureStops(List<Vector<uint8_t> >& secureStops) {
  CdmUsageInfo cdmUsageInfo;
  CdmResponseType res =
      mCDM->GetUsageInfo(mPropertySet.app_id(), mCdmIdentifier, &cdmUsageInfo);
  if (isCdmResponseTypeSuccess(res)) {
    secureStops.clear();
    for (CdmUsageInfo::const_iterator iter = cdmUsageInfo.begin();
         iter != cdmUsageInfo.end();
         ++iter) {
      const string& cdmStop = *iter;

      secureStops.push_back(ToVector(cdmStop));
    }
  }
  return mapCdmResponseType(res);
}

status_t WVDrmPlugin::releaseAllSecureStops() {
  CdmResponseType res = mCDM->RemoveAllUsageInfo(mPropertySet.app_id(),
                                                 mCdmIdentifier);
  return mapCdmResponseType(res);
}

status_t WVDrmPlugin::releaseSecureStops(const Vector<uint8_t>& ssRelease) {
  if (!ssRelease.size()) {
    return android::BAD_VALUE;
  }
  CdmUsageInfoReleaseMessage cdmMessage(ssRelease.begin(), ssRelease.end());
  CdmResponseType res = mCDM->ReleaseUsageInfo(cdmMessage, mCdmIdentifier);
  return mapCdmResponseType(res);
}

status_t WVDrmPlugin::getPropertyString(const String8& name,
                                        String8& value) const {
  if (name == "vendor") {
    value = "Google";
  } else if (name == "version") {
    return queryProperty(QUERY_KEY_WVCDM_VERSION, value);
  } else if (name == "description") {
    value = "Widevine CDM";
  } else if (name == "algorithms") {
    value = "AES/CBC/NoPadding,HmacSHA256";
  } else if (name == "securityLevel") {
    string requestedLevel = mPropertySet.security_level();

    if (requestedLevel.length() > 0) {
      value = requestedLevel.c_str();
    } else {
      return queryProperty(QUERY_KEY_SECURITY_LEVEL, value);
    }
  } else if (name == "systemId") {
    return queryProperty(QUERY_KEY_SYSTEM_ID, value);
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
    return queryProperty(QUERY_KEY_CURRENT_HDCP_LEVEL, value);
  } else if (name == "maxHdcpLevel") {
    return queryProperty(QUERY_KEY_MAX_HDCP_LEVEL, value);
  } else if (name == "usageReportingSupport") {
    return queryProperty(QUERY_KEY_USAGE_SUPPORT, value);
  } else if (name == "numberOfOpenSessions") {
    return queryProperty(QUERY_KEY_NUMBER_OF_OPEN_SESSIONS, value);
  } else if (name == "maxNumberOfSessions") {
    return queryProperty(QUERY_KEY_MAX_NUMBER_OF_SESSIONS, value);
  } else if (name == "oemCryptoApiVersion") {
    return queryProperty(QUERY_KEY_OEMCRYPTO_API_VERSION, value);
  } else if (name == "appId") {
    value = mPropertySet.app_id().c_str();
  } else if (name == "origin") {
    value = mCdmIdentifier.origin.c_str();
  } else if (name == "CurrentSRMVersion") {
    return queryProperty(QUERY_KEY_CURRENT_SRM_VERSION, value);
  } else if (name == "SRMUpdateSupport") {
    return queryProperty(QUERY_KEY_SRM_UPDATE_SUPPORT, value);
  } else if (name == "resourceRatingTier") {
    return queryProperty(QUERY_KEY_RESOURCE_RATING_TIER, value);
  } else if (name == "oemCryptoBuildInformation") {
    return queryProperty(QUERY_KEY_OEMCRYPTO_BUILD_INFORMATION, value);
  } else if (name == "decryptHashSupport") {
    return queryProperty(QUERY_KEY_DECRYPT_HASH_SUPPORT, value);
  } else if (name == "decryptHashError") {
    std::string hash_error_string;
    CdmResponseType res =
      mCDM->GetDecryptHashError(mDecryptHashSessionId, &hash_error_string);
    value = hash_error_string.c_str();
    return mapCdmResponseType(res);
  } else {
    ALOGE("App requested unknown string property %s", name.string());
    return android::ERROR_DRM_CANNOT_HANDLE;
  }

  return android::OK;
}

status_t WVDrmPlugin::getPropertyByteArray(const String8& name,
                                           Vector<uint8_t>& value) const {
  if (name == "deviceUniqueId") {
    return queryProperty(QUERY_KEY_DEVICE_ID, value);
  } else if (name == "provisioningUniqueId") {
    return queryProperty(QUERY_KEY_PROVISIONING_ID, value);
  } else if (name == "serviceCertificate") {
    value = ToVector(mPropertySet.service_certificate());
  } else if (name == "provisioningServiceCertificate") {
    value = ToVector(mProvisioningServiceCertificate);
  } else if (name == "metrics") {
    std::string serialized_metrics;
    drm_metrics::WvCdmMetrics metrics;
    mCDM->GetMetrics(mCdmIdentifier, &metrics);
    if (!metrics.SerializeToString(&serialized_metrics)) {
      return android::ERROR_DRM_UNKNOWN;
    } else {
      value = ToVector(serialized_metrics);
    }
  } else {
    ALOGE("App requested unknown byte array property %s", name.string());
    return android::ERROR_DRM_CANNOT_HANDLE;
  }

  return android::OK;
}

status_t WVDrmPlugin::setPropertyString(const String8& name,
                                        const String8& value) {
  if (name == "securityLevel") {
    if (mCryptoSessions.size() == 0) {
      if (value == QUERY_VALUE_SECURITY_LEVEL_L3.c_str()) {
        mPropertySet.set_security_level(QUERY_VALUE_SECURITY_LEVEL_L3);
      } else if (value == QUERY_VALUE_SECURITY_LEVEL_L1.c_str()) {
        // We must be sure we CAN set the security level to L1.
        std::string current_security_level;
        status_t status =
            queryProperty(kLevelDefault, QUERY_KEY_SECURITY_LEVEL,
                          current_security_level);
        if (status != android::OK) return status;

        if (current_security_level != QUERY_VALUE_SECURITY_LEVEL_L1) {
          ALOGE("App requested L1 security on a non-L1 device.");
          return android::BAD_VALUE;
        } else {
          mPropertySet.set_security_level(kResetSecurityLevel);
        }
      } else if (value == kResetSecurityLevel) {
        mPropertySet.set_security_level(kResetSecurityLevel);
      } else {
        ALOGE("App requested invalid security level %s", value.string());
        return android::BAD_VALUE;
      }
    } else {
      ALOGE("App tried to change security level while sessions are open.");
      return kErrorSessionIsOpen;
    }
  } else if (name == "privacyMode") {
    if (value == kEnable) {
      mPropertySet.set_use_privacy_mode(true);
    } else if (value == kDisable) {
      mPropertySet.set_use_privacy_mode(false);
    } else {
      ALOGE("App requested unknown privacy mode %s", value.string());
      return android::BAD_VALUE;
    }
  } else if (name == "sessionSharing") {
    if (mCryptoSessions.size() == 0) {
      if (value == kEnable) {
        mPropertySet.set_is_session_sharing_enabled(true);
      } else if (value == kDisable) {
        mPropertySet.set_is_session_sharing_enabled(false);
      } else {
        ALOGE("App requested unknown sharing type %s", value.string());
        return android::BAD_VALUE;
      }
    } else {
      ALOGE("App tried to change key sharing while sessions are open.");
      return kErrorSessionIsOpen;
    }
  } else if (name == "appId") {
    if (mCryptoSessions.size() == 0) {
      mPropertySet.set_app_id(value.string());
    } else {
      ALOGE("App tried to set the application id while sessions are opened.");
      return kErrorSessionIsOpen;
    }
  } else if (name == "origin") {
    if (mCryptoSessions.size() != 0) {
      ALOGE("App tried to set the origin while sessions are opened.");
      return kErrorSessionIsOpen;
    } else {
      mCdmIdentifier.origin = value.string();
    }
  } else if (name == "decryptHash") {
    CdmSessionId sessionId;
    CdmResponseType res =
      mCDM->SetDecryptHash(value.string(), &sessionId);

    if (wvcdm::NO_ERROR == res) mDecryptHashSessionId = sessionId;

    return mapCdmResponseType(res);
  } else if (name == "decryptHashSessionId") {
    mDecryptHashSessionId = value.string();
  } else {
    ALOGE("App set unknown string property %s", name.string());
    return android::ERROR_DRM_CANNOT_HANDLE;
  }

  return android::OK;
}

status_t WVDrmPlugin::setPropertyByteArray(const String8& name,
                                           const Vector<uint8_t>& value) {
  if (name == "serviceCertificate") {
    std::string cert(value.begin(), value.end());
    if (value.isEmpty() || mCDM->IsValidServiceCertificate(cert)) {
      mPropertySet.set_service_certificate(cert);
    } else {
      return android::BAD_VALUE;
    }
  } else if (name == "provisioningServiceCertificate") {
    std::string cert(value.begin(), value.end());
    if (value.isEmpty() || mCDM->IsValidServiceCertificate(cert)) {
      mProvisioningServiceCertificate = cert;
    } else {
      return android::BAD_VALUE;
    }
  } else {
    ALOGE("App set unknown byte array property %s", name.string());
    return android::ERROR_DRM_CANNOT_HANDLE;
  }

  return android::OK;
}

status_t WVDrmPlugin::setCipherAlgorithm(const Vector<uint8_t>& sessionId,
                                         const String8& algorithm) {
  if (sessionId.size() == 0 || algorithm.size() == 0) {
    return android::BAD_VALUE;
  }
  CdmSessionId cdmSessionId(sessionId.begin(), sessionId.end());
  if (!mCryptoSessions.count(cdmSessionId)) {
    return android::ERROR_DRM_SESSION_NOT_OPENED;
  }

  CryptoSession& cryptoSession = mCryptoSessions[cdmSessionId];

  if (algorithm == "AES/CBC/NoPadding") {
    cryptoSession.setCipherAlgorithm(OEMCrypto_AES_CBC_128_NO_PADDING);
  } else {
    return android::ERROR_DRM_CANNOT_HANDLE;
  }

  return android::OK;
}

status_t WVDrmPlugin::setMacAlgorithm(const Vector<uint8_t>& sessionId,
                                      const String8& algorithm) {
  if (sessionId.size() == 0 || algorithm.size() == 0) {
    return android::BAD_VALUE;
  }
  CdmSessionId cdmSessionId(sessionId.begin(), sessionId.end());
  if (!mCryptoSessions.count(cdmSessionId)) {
    return android::ERROR_DRM_SESSION_NOT_OPENED;
  }

  CryptoSession& cryptoSession = mCryptoSessions[cdmSessionId];

  if (algorithm == "HmacSHA256") {
    cryptoSession.setMacAlgorithm(OEMCrypto_HMAC_SHA256);
  } else {
    return android::ERROR_DRM_CANNOT_HANDLE;
  }

  return android::OK;
}

status_t WVDrmPlugin::encrypt(const Vector<uint8_t>& sessionId,
                              const Vector<uint8_t>& keyId,
                              const Vector<uint8_t>& input,
                              const Vector<uint8_t>& iv,
                              Vector<uint8_t>& output) {
  CdmSessionId cdmSessionId(sessionId.begin(), sessionId.end());
  if (!mCryptoSessions.count(cdmSessionId)) {
    return android::ERROR_DRM_SESSION_NOT_OPENED;
  }

  const CryptoSession& cryptoSession = mCryptoSessions[cdmSessionId];

  if (cryptoSession.cipherAlgorithm() == kInvalidCryptoAlgorithm) {
    return android::NO_INIT;
  }

  OEMCryptoResult res = mCrypto->selectKey(cryptoSession.oecSessionId(),
                                           keyId.array(), keyId.size());

  if (res != OEMCrypto_SUCCESS) {
    ALOGE("OEMCrypto_SelectKey failed with %u", res);
    return mapAndNotifyOfOEMCryptoResult(sessionId, res);
  }

  output.resize(input.size());

  res = mCrypto->encrypt(cryptoSession.oecSessionId(), input.array(),
                         input.size(), iv.array(),
                         cryptoSession.cipherAlgorithm(), output.editArray());

  if (res == OEMCrypto_SUCCESS) {
    return android::OK;
  } else {
    ALOGE("OEMCrypto_Generic_Encrypt failed with %u", res);
    return mapAndNotifyOfOEMCryptoResult(sessionId, res);
  }
}

status_t WVDrmPlugin::decrypt(const Vector<uint8_t>& sessionId,
                              const Vector<uint8_t>& keyId,
                              const Vector<uint8_t>& input,
                              const Vector<uint8_t>& iv,
                              Vector<uint8_t>& output) {
  CdmSessionId cdmSessionId(sessionId.begin(), sessionId.end());
  if (!mCryptoSessions.count(cdmSessionId)) {
    return android::ERROR_DRM_SESSION_NOT_OPENED;
  }

  const CryptoSession& cryptoSession = mCryptoSessions[cdmSessionId];

  if (cryptoSession.cipherAlgorithm() == kInvalidCryptoAlgorithm) {
    return android::NO_INIT;
  }

  OEMCryptoResult res = mCrypto->selectKey(cryptoSession.oecSessionId(),
                                           keyId.array(), keyId.size());

  if (res != OEMCrypto_SUCCESS) {
    ALOGE("OEMCrypto_SelectKey failed with %u", res);
    return mapAndNotifyOfOEMCryptoResult(sessionId, res);
  }

  output.resize(input.size());

  res = mCrypto->decrypt(cryptoSession.oecSessionId(), input.array(),
                         input.size(), iv.array(),
                         cryptoSession.cipherAlgorithm(), output.editArray());

  if (res == OEMCrypto_SUCCESS) {
    return android::OK;
  } else {
    ALOGE("OEMCrypto_Generic_Decrypt failed with %u", res);
    return mapAndNotifyOfOEMCryptoResult(sessionId, res);
  }
}

status_t WVDrmPlugin::sign(const Vector<uint8_t>& sessionId,
                           const Vector<uint8_t>& keyId,
                           const Vector<uint8_t>& message,
                           Vector<uint8_t>& signature) {
  CdmSessionId cdmSessionId(sessionId.begin(), sessionId.end());
  if (!mCryptoSessions.count(cdmSessionId)) {
    return android::ERROR_DRM_SESSION_NOT_OPENED;
  }

  const CryptoSession& cryptoSession = mCryptoSessions[cdmSessionId];

  if (cryptoSession.macAlgorithm() == kInvalidCryptoAlgorithm) {
    return android::NO_INIT;
  }

  OEMCryptoResult res = mCrypto->selectKey(cryptoSession.oecSessionId(),
                                           keyId.array(), keyId.size());

  if (res != OEMCrypto_SUCCESS) {
    ALOGE("OEMCrypto_SelectKey failed with %u", res);
    return mapAndNotifyOfOEMCryptoResult(sessionId, res);
  }

  size_t signatureSize = 0;

  res = mCrypto->sign(cryptoSession.oecSessionId(), message.array(),
                      message.size(), cryptoSession.macAlgorithm(),
                      NULL, &signatureSize);

  if (res != OEMCrypto_ERROR_SHORT_BUFFER) {
    ALOGE("OEMCrypto_Generic_Sign failed with %u when requesting signature "
          "size", res);
    if (res != OEMCrypto_SUCCESS) {
      return mapAndNotifyOfOEMCryptoResult(sessionId, res);
    } else {
      return android::ERROR_DRM_UNKNOWN;
    }
  }

  signature.resize(signatureSize);

  res = mCrypto->sign(cryptoSession.oecSessionId(), message.array(),
                      message.size(), cryptoSession.macAlgorithm(),
                      signature.editArray(), &signatureSize);

  if (res == OEMCrypto_SUCCESS) {
    return android::OK;
  } else {
    ALOGE("OEMCrypto_Generic_Sign failed with %u", res);
    return mapAndNotifyOfOEMCryptoResult(sessionId, res);
  }
}

status_t WVDrmPlugin::verify(const Vector<uint8_t>& sessionId,
                             const Vector<uint8_t>& keyId,
                             const Vector<uint8_t>& message,
                             const Vector<uint8_t>& signature,
                             bool& match) {
  CdmSessionId cdmSessionId(sessionId.begin(), sessionId.end());
  if (!mCryptoSessions.count(cdmSessionId)) {
    return android::ERROR_DRM_SESSION_NOT_OPENED;
  }

  const CryptoSession& cryptoSession = mCryptoSessions[cdmSessionId];

  if (cryptoSession.macAlgorithm() == kInvalidCryptoAlgorithm) {
    return android::NO_INIT;
  }

  OEMCryptoResult res = mCrypto->selectKey(cryptoSession.oecSessionId(),
                                           keyId.array(), keyId.size());

  if (res != OEMCrypto_SUCCESS) {
    ALOGE("OEMCrypto_SelectKey failed with %u", res);
    return mapAndNotifyOfOEMCryptoResult(sessionId, res);
  }

  res = mCrypto->verify(cryptoSession.oecSessionId(), message.array(),
                        message.size(), cryptoSession.macAlgorithm(),
                        signature.array(), signature.size());

  if (res == OEMCrypto_SUCCESS) {
    match = true;
    return android::OK;
  } else if (res == OEMCrypto_ERROR_SIGNATURE_FAILURE) {
    match = false;
    return android::OK;
  } else {
    ALOGE("OEMCrypto_Generic_Verify failed with %u", res);
    return mapAndNotifyOfOEMCryptoResult(sessionId, res);
  }
}

status_t WVDrmPlugin::signRSA(const Vector<uint8_t>& sessionId,
                              const String8& algorithm,
                              const Vector<uint8_t>& message,
                              const Vector<uint8_t>& wrappedKey,
                              Vector<uint8_t>& signature) {
  if (sessionId.size() == 0 || algorithm.size() == 0 ||
      message.size() == 0 || wrappedKey.size() == 0) {
    return android::BAD_VALUE;
  }
  RSA_Padding_Scheme padding_scheme;
  if (algorithm == "RSASSA-PSS-SHA1") {
    padding_scheme = kSign_RSASSA_PSS;
  } else if (algorithm == "PKCS1-BlockType1") {
    padding_scheme = kSign_PKCS1_Block1;
  } else {
    ALOGE("Unknown RSA Algorithm %s", algorithm.string());
    return android::ERROR_DRM_CANNOT_HANDLE;
  }
  OEMCryptoResult res = mCrypto->signRSA(wrappedKey.array(),
                                         wrappedKey.size(),
                                         message.array(), message.size(),
                                         signature,
                                         padding_scheme);

  if (res != OEMCrypto_SUCCESS) {
    ALOGE("OEMCrypto_GenerateRSASignature failed with %u", res);
    return mapOEMCryptoResult(res);
  }

  return android::OK;
}

void WVDrmPlugin::OnSessionRenewalNeeded(const CdmSessionId& cdmSessionId) {
  Vector<uint8_t> sessionId = ToVector(cdmSessionId);
  sendEvent(kDrmPluginEventKeyNeeded, 0, &sessionId, NULL);
}

void WVDrmPlugin::OnSessionKeysChange(const CdmSessionId& cdmSessionId,
                                      const CdmKeyStatusMap& cdmKeysStatus,
                                      bool hasNewUsableKey) {
  bool expired = false;
  Vector<KeyStatus> keyStatusList;
  for (CdmKeyStatusMap::const_iterator it = cdmKeysStatus.begin();
       it != cdmKeysStatus.end(); ++it) {
    const KeyId& keyId = it->first;
    const CdmKeyStatus cdmKeyStatus = it->second;
    if (cdmKeyStatus == kKeyStatusExpired) expired = true;

    keyStatusList.push(
        {ToVector(keyId), ConvertFromCdmKeyStatus(cdmKeyStatus)});
  }

  Vector<uint8_t> sessionId = ToVector(cdmSessionId);
  sendKeysChange(&sessionId, &keyStatusList, hasNewUsableKey);
  // For backward compatibility.
  if (expired) sendEvent(kDrmPluginEventKeyExpired, 0, &sessionId, NULL);
}

void WVDrmPlugin::OnExpirationUpdate(const CdmSessionId& cdmSessionId,
                                     int64_t newExpiryTimeSeconds) {
  Vector<uint8_t> sessionId = ToVector(cdmSessionId);
  int64_t newExpiryTimeMilliseconds = newExpiryTimeSeconds == NEVER_EXPIRES
                                          ? newExpiryTimeSeconds
                                          : newExpiryTimeSeconds * 1000;
  sendExpirationUpdate(&sessionId, newExpiryTimeMilliseconds);
}

status_t WVDrmPlugin::queryProperty(const std::string& property,
                                    std::string& stringValue) const {
    wvcdm::SecurityLevel securityLevel =
      mPropertySet.security_level().compare(QUERY_VALUE_SECURITY_LEVEL_L3) == 0
          ? kLevel3
          : kLevelDefault;
  return queryProperty(securityLevel, property, stringValue);
}

status_t WVDrmPlugin::queryProperty(wvcdm::SecurityLevel securityLevel,
                                    const std::string& property,
                                    std::string& stringValue) const {
  CdmResponseType res =
      mCDM->QueryStatus(securityLevel, property, &stringValue);

  if (res != wvcdm::NO_ERROR) {
    ALOGE("Error querying CDM status: %u", res);
  }
  return mapCdmResponseType(res);
}

status_t WVDrmPlugin::queryProperty(const std::string& property,
                                    String8& string8_value) const {
  std::string string_value;
  status_t status = queryProperty(property, string_value);
  if (status != android::OK) return status;
  string8_value = string_value.c_str();
  return android::OK;
}

status_t WVDrmPlugin::queryProperty(const std::string& property,
                                    Vector<uint8_t>& vector_value) const {
  std::string string_value;
  status_t status = queryProperty(property, string_value);
  if (status != android::OK) return status;
  vector_value = ToVector(string_value);
  return android::OK;
}

status_t WVDrmPlugin::mapAndNotifyOfCdmResponseType(
    const Vector<uint8_t>& sessionId,
    CdmResponseType res) {
  if (res == wvcdm::NEED_PROVISIONING) {
    sendEvent(kDrmPluginEventProvisionRequired, 0, &sessionId, NULL);
  } else if (res == wvcdm::NEED_KEY) {
    sendEvent(kDrmPluginEventKeyNeeded, 0, &sessionId, NULL);
  }

  return mapCdmResponseType(res);
}

status_t WVDrmPlugin::mapAndNotifyOfOEMCryptoResult(
    const Vector<uint8_t>& sessionId,
    OEMCryptoResult res) {
  if (res == OEMCrypto_ERROR_NO_DEVICE_KEY) {
    sendEvent(kDrmPluginEventProvisionRequired, 0, &sessionId, NULL);
  }
  return mapOEMCryptoResult(res);
}

status_t WVDrmPlugin::mapOEMCryptoResult(OEMCryptoResult res) {
  switch (res) {
    case OEMCrypto_SUCCESS:
      return android::OK;
    case OEMCrypto_ERROR_SIGNATURE_FAILURE:
      return android::ERROR_DRM_TAMPER_DETECTED;
    case OEMCrypto_ERROR_SHORT_BUFFER:
      return kErrorIncorrectBufferSize;
    case OEMCrypto_ERROR_NO_DEVICE_KEY:
      return android::ERROR_DRM_NOT_PROVISIONED;
    case OEMCrypto_ERROR_INVALID_SESSION:
      return android::ERROR_DRM_SESSION_NOT_OPENED;
    case OEMCrypto_ERROR_TOO_MANY_SESSIONS:
      return android::ERROR_DRM_RESOURCE_BUSY;
    case OEMCrypto_ERROR_INVALID_RSA_KEY:
      return kErrorInvalidKey;
    case OEMCrypto_ERROR_INSUFFICIENT_RESOURCES:
      return android::ERROR_DRM_RESOURCE_BUSY;
    case OEMCrypto_ERROR_NOT_IMPLEMENTED:
      return android::ERROR_DRM_CANNOT_HANDLE;
    case OEMCrypto_ERROR_UNKNOWN_FAILURE:
    case OEMCrypto_ERROR_OPEN_SESSION_FAILED:
      return android::ERROR_DRM_UNKNOWN;
    default:
      return android::UNKNOWN_ERROR;
  }
}

bool WVDrmPlugin::initDataResemblesPSSH(const Vector<uint8_t>& initData) {
  const uint8_t* const initDataArray = initData.array();

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
  string id(idField, kPsshTag.size());
  return id == kPsshTag;
}

status_t WVDrmPlugin::unprovision(const CdmIdentifier& identifier) {
  CdmResponseType res1 = mCDM->Unprovision(kSecurityLevelL1, identifier);
  CdmResponseType res3 = mCDM->Unprovision(kSecurityLevelL3, identifier);
  if (!isCdmResponseTypeSuccess(res1))
  {
    return mapCdmResponseType(res1);
  }
  else
  {
    return mapCdmResponseType(res3);
  }
}

}  // namespace wvdrm
