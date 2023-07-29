// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
// Crypto - wrapper classes for OEMCrypto interface
//

#include "crypto_session.h"

#include <string.h>
#include <algorithm>
#include <iostream>
#include <memory>

#include "content_key_session.h"
#include "crypto_key.h"
#include "entitlement_key_session.h"
#include "log.h"
#include "platform.h"
#include "privacy_crypto.h"
#include "properties.h"
#include "pst_report.h"
#include "string_conversions.h"
#include "usage_table_header.h"
#include "wv_cdm_constants.h"

namespace {

// Encode unsigned integer into a big endian formatted string
std::string EncodeUint32(unsigned int u) {
  std::string s;
  s.append(1, (u >> 24) & 0xFF);
  s.append(1, (u >> 16) & 0xFF);
  s.append(1, (u >> 8) & 0xFF);
  s.append(1, (u >> 0) & 0xFF);
  return s;
}

const uint32_t kRsaSignatureLength = 256;
// TODO(b/117112392): adjust chunk size based on resource rating.
const size_t kMaximumChunkSize = 100 * 1024;  // 100 KiB
const size_t kEstimatedInitialUsageTableHeader = 40;
const size_t kOemCryptoApiVersionSupportsBigUsageTables = 13;
// Ability to switch cipher modes in SelectKey() was introduced in this
// OEMCrypto version
const size_t kOemCryptoApiVersionSupportsSwitchingCipherMode = 14;

// Constants and utility objects relating to OEM Certificates
const char* const kWidevineSystemIdExtensionOid = "1.3.6.1.4.1.11129.4.1.1";

}  // namespace

namespace wvcdm {
shared_mutex CryptoSession::static_field_mutex_;
shared_mutex CryptoSession::oem_crypto_mutex_;
bool CryptoSession::initialized_ = false;
int CryptoSession::session_count_ = 0;
UsageTableHeader* CryptoSession::usage_table_header_l1_ = NULL;
UsageTableHeader* CryptoSession::usage_table_header_l3_ = NULL;
std::atomic<uint64_t> CryptoSession::request_id_index_source_(0);

size_t GetOffset(std::string message, std::string field) {
  size_t pos = message.find(field);
  if (pos == std::string::npos) {
    LOGE("CryptoSession::GetOffset : Cannot find offset for %s", field.c_str());
    pos = 0;
  }
  return pos;
}

OEMCrypto_Substring GetSubstring(const std::string& message,
                                 const std::string& field, bool set_zero) {
  OEMCrypto_Substring substring;
  if (set_zero || field.empty() || message.empty()) {
    substring.offset = 0;
    substring.length = 0;
  } else {
    size_t pos = message.find(field);
    if (pos == std::string::npos) {
      LOGW("GetSubstring : Cannot find offset for %s", field.c_str());
      substring.offset = 0;
      substring.length = 0;
    } else {
      substring.offset = pos;
      substring.length = field.length();
    }
  }
  return substring;
}

void GenerateMacContext(const std::string& input_context,
                        std::string* deriv_context) {
  if (!deriv_context) {
    LOGE("CryptoSession::GenerateMacContext : No output destination provided.");
    return;
  }

  const std::string kSigningKeyLabel = "AUTHENTICATION";
  const size_t kSigningKeySizeBits = wvcdm::MAC_KEY_SIZE * 8;

  deriv_context->assign(kSigningKeyLabel);
  deriv_context->append(1, '\0');
  deriv_context->append(input_context);
  deriv_context->append(EncodeUint32(kSigningKeySizeBits * 2));
}

void GenerateEncryptContext(const std::string& input_context,
                            std::string* deriv_context) {
  if (!deriv_context) {
    LOGE(
        "CryptoSession::GenerateEncryptContext : No output destination "
        "provided.");
    return;
  }

  const std::string kEncryptionKeyLabel = "ENCRYPTION";
  const size_t kEncryptionKeySizeBits = wvcdm::CONTENT_KEY_SIZE * 8;

  deriv_context->assign(kEncryptionKeyLabel);
  deriv_context->append(1, '\0');
  deriv_context->append(input_context);
  deriv_context->append(EncodeUint32(kEncryptionKeySizeBits));
}

OEMCrypto_LicenseType OEMCryptoLicenseType(CdmLicenseKeyType cdm_license_type) {
  return cdm_license_type == kLicenseKeyTypeContent
             ? OEMCrypto_ContentLicense
             : OEMCrypto_EntitlementLicense;
}

OEMCryptoCipherMode ToOEMCryptoCipherMode(CdmCipherMode cipher_mode) {
  return cipher_mode == kCipherModeCtr
      ? OEMCrypto_CipherMode_CTR : OEMCrypto_CipherMode_CBC;
}

// This maps a few common OEMCryptoResult to CdmResponseType. Many mappings
// are not universal but are OEMCrypto method specific. Those will be
// specified in the CryptoSession method rather than here.
CdmResponseType MapOEMCryptoResult(
    OEMCryptoResult result,
    CdmResponseType default_status,
    const char* error_string) {
  if (result != OEMCrypto_SUCCESS) {
    LOGE("%s: error %d", error_string, result);
  }

  switch (result) {
    case OEMCrypto_SUCCESS:
      return NO_ERROR;
    case OEMCrypto_ERROR_NOT_IMPLEMENTED:
      return NOT_IMPLEMENTED_ERROR;
    case OEMCrypto_ERROR_TOO_MANY_SESSIONS:
      return INSUFFICIENT_CRYPTO_RESOURCES;
    case OEMCrypto_ERROR_SESSION_LOST_STATE:
      return SESSION_LOST_STATE_ERROR;
    case OEMCrypto_ERROR_SYSTEM_INVALIDATED:
      return SYSTEM_INVALIDATED_ERROR;
    default:
      return default_status;
  }
}

CryptoSession::CryptoSession(metrics::CryptoMetrics* metrics)
    : metrics_(metrics),
      system_id_(-1),
      open_(false),
      pre_provision_token_type_(kClientTokenUninitialized),
      update_usage_table_after_close_session_(false),
      is_destination_buffer_type_valid_(false),
      requested_security_level_(kLevelDefault),
      is_usage_support_type_valid_(false),
      usage_support_type_(kUnknownUsageSupport),
      usage_table_header_(NULL),
      cipher_mode_(kCipherModeCtr),
      api_version_(0) {
  assert(metrics);
  Init();
  life_span_.Start();
}

CryptoSession::~CryptoSession() {
  if (open_) {
    Close();
  }
  Terminate();
  M_RECORD(metrics_, crypto_session_life_span_, life_span_.AsMs());
}

CdmResponseType CryptoSession::GetProvisioningMethod(
    SecurityLevel requested_security_level,
    CdmClientTokenType* token_type) {
  OEMCrypto_ProvisioningMethod method;
  WithOecReadLock("GetProvisioningMethod", [&] {
    method = OEMCrypto_GetProvisioningMethod(requested_security_level);
  });
  metrics_->oemcrypto_provisioning_method_.Record(method);
  CdmClientTokenType type;
  switch (method) {
    case OEMCrypto_OEMCertificate:
      type = kClientTokenOemCert;
      break;
    case OEMCrypto_Keybox:
      type = kClientTokenKeybox;
      break;
    case OEMCrypto_DrmCertificate:
      type = kClientTokenDrmCert;
      break;
    case OEMCrypto_ProvisioningError:
    default:
      LOGE("OEMCrypto_GetProvisioningMethod failed. %d", method);
      metrics_->oemcrypto_provisioning_method_.SetError(method);
      return GET_PROVISIONING_METHOD_ERROR;
  }
  *token_type = type;
  return NO_ERROR;
}

void CryptoSession::Init() {
  LOGV("CryptoSession::Init");
  WithStaticFieldWriteLock("Init", [&] {
    session_count_ += 1;
    if (!initialized_) {
      std::string sandbox_id;
      OEMCryptoResult sts;
      WithOecWriteLock("Init", [&] {
        if (Properties::GetSandboxId(&sandbox_id) && !sandbox_id.empty()) {
          sts = OEMCrypto_SetSandbox(
              reinterpret_cast<const uint8_t*>(sandbox_id.c_str()),
              sandbox_id.length());
          metrics_->oemcrypto_set_sandbox_.Record(sandbox_id);
        }
        M_TIME(sts = OEMCrypto_Initialize(), metrics_, oemcrypto_initialize_,
               sts);
      });
      if (OEMCrypto_SUCCESS != sts) {
        LOGE("OEMCrypto_Initialize failed: %d", sts);
        return;
      }
      initialized_ = true;
    }
  });
}

void CryptoSession::Terminate() {
  LOGV("CryptoSession::Terminate");
  WithStaticFieldWriteLock("Terminate", [&] {
    LOGV("initialized_=%d, session_count_=%d", initialized_, session_count_);
    if (session_count_ > 0) {
      session_count_ -= 1;
    } else {
      LOGE("CryptoSession::Terminate error, session count: %d", session_count_);
    }
    if (session_count_ > 0 || !initialized_) return;
    OEMCryptoResult sts;
    WithOecWriteLock("Terminate", [&] {
      sts = OEMCrypto_Terminate();
    });
    if (OEMCrypto_SUCCESS != sts) {
      LOGE("OEMCrypto_Terminate failed: %d", sts);
    }

    if (usage_table_header_l1_ != NULL) {
      delete usage_table_header_l1_;
      usage_table_header_l1_ = NULL;
    }
    if (usage_table_header_l3_ != NULL) {
      delete usage_table_header_l3_;
      usage_table_header_l3_ = NULL;
    }

    initialized_ = false;
  });
}

CdmResponseType CryptoSession::GetTokenFromKeybox(std::string* token) {
  if (token == nullptr) {
    LOGE("CryptoSession::GetTokenFromKeybox: token not provided");
    return PARAMETER_NULL;
  }
  std::string temp_buffer(KEYBOX_KEY_DATA_SIZE, '\0');
  size_t buf_size = temp_buffer.size();
  uint8_t* buf = reinterpret_cast<uint8_t*>(&temp_buffer[0]);

  OEMCryptoResult status;
  WithOecReadLock("GetTokenFromKeybox", [&] {
    M_TIME(
        status =
            OEMCrypto_GetKeyData(buf, &buf_size, requested_security_level_),
        metrics_, oemcrypto_get_key_data_, status,
        metrics::Pow2Bucket(buf_size));
  });

  if (OEMCrypto_SUCCESS == status) {
    token->swap(temp_buffer);
  }

  return MapOEMCryptoResult(
      status, GET_TOKEN_FROM_KEYBOX_ERROR, "GetTokenFromKeybox");
}

CdmResponseType CryptoSession::GetTokenFromOemCert(std::string* token) {
  if (token == nullptr) {
    LOGE("CryptoSession::GetTokenFromOemCert: token not provided ");
    return PARAMETER_NULL;
  }
  OEMCryptoResult status;
  if (!oem_token_.empty()) {
    token->assign(oem_token_);
    return NO_ERROR;
  }
  std::string temp_buffer(CERTIFICATE_DATA_SIZE, '\0');
  bool retrying = false;
  while (true) {
    size_t buf_size = temp_buffer.size();
    uint8_t* buf = reinterpret_cast<uint8_t*>(&temp_buffer[0]);
    WithOecSessionLock("GetTokenFromOemCert", [&] {
      status = OEMCrypto_GetOEMPublicCertificate(oec_session_id_, buf,
                                                 &buf_size);
    });
    metrics_->oemcrypto_get_oem_public_certificate_.Increment(status);

    if (OEMCrypto_SUCCESS == status) {
      temp_buffer.resize(buf_size);
      oem_token_.assign(temp_buffer);
      token->assign(temp_buffer);
      return NO_ERROR;
    }

    if (status == OEMCrypto_ERROR_SHORT_BUFFER && !retrying) {
      temp_buffer.resize(buf_size);
      retrying = true;
      continue;
    }

    return MapOEMCryptoResult(
        status, GET_TOKEN_FROM_OEM_CERT_ERROR, "GetTokenFromOemCert");
  }
}

CdmResponseType CryptoSession::GetProvisioningToken(std::string* token) {
  if (token == nullptr) {
    LOGE("CryptoSession::GetProvisioningToken : No token passed to method.");
    metrics_->crypto_session_get_token_.Increment(PARAMETER_NULL);
    return PARAMETER_NULL;
  }

  if (!IsInitialized()) {
    metrics_->crypto_session_get_token_.Increment(CRYPTO_SESSION_NOT_INITIALIZED);
    return CRYPTO_SESSION_NOT_INITIALIZED;
  }

  CdmResponseType status = UNKNOWN_CLIENT_TOKEN_TYPE;
  if (pre_provision_token_type_ == kClientTokenKeybox) {
    status = GetTokenFromKeybox(token);
  } else if (pre_provision_token_type_ == kClientTokenOemCert) {
    status = GetTokenFromOemCert(token);
  }
  metrics_->crypto_session_get_token_.Increment(status);
  return status;
}

CdmSecurityLevel CryptoSession::GetSecurityLevel() {
  LOGV("CryptoSession::GetSecurityLevel");
  if (!open_) {
    LOGW("CryptoSession::GetSecurityLevel: session not open");
    return kSecurityLevelUninitialized;
  }
  return GetSecurityLevel(requested_security_level_);
}

CdmSecurityLevel CryptoSession::GetSecurityLevel(
    SecurityLevel requested_level) {
  LOGV("CryptoSession::GetSecurityLevel");
  if (!IsInitialized()) {
    LOGW("Not Initialized");
    return kSecurityLevelUninitialized;
  }

  std::string security_level;
  WithOecReadLock("GetSecurityLevel", [&] {
    security_level = OEMCrypto_SecurityLevel(requested_level);
  });

  if ((security_level.size() != 2) || (security_level.at(0) != 'L')) {
    return kSecurityLevelUnknown;
  }

  CdmSecurityLevel cdm_security_level;
  switch (security_level.at(1)) {
    case '1':
      cdm_security_level = kSecurityLevelL1;
      break;
    case '2':
      cdm_security_level = kSecurityLevelL2;
      break;
    case '3':
      cdm_security_level = kSecurityLevelL3;
      break;
    default:
      cdm_security_level = kSecurityLevelUnknown;
      break;
  }

  return cdm_security_level;
}

CdmResponseType CryptoSession::GetInternalDeviceUniqueId(
    std::string* device_id) {
  if (device_id == nullptr) {
    LOGE(
        "CryptoSession::GetInternalDeviceUniqueId : No buffer passed to "
        "method.");
    return PARAMETER_NULL;
  }

  if (!IsInitialized()) {
    LOGE("CryptoSession::GetInternalDeviceUniqueId: not initialized");
    return CRYPTO_SESSION_NOT_INITIALIZED;
  }

  std::vector<uint8_t> id;
  size_t id_length = 32;
  id.resize(id_length);

  OEMCryptoResult sts;
  WithOecReadLock("GetInternalDeviceUniqueId Attempt 1", [&] {
    sts = OEMCrypto_GetDeviceID(&id[0], &id_length, requested_security_level_);
  });
  // Increment the count of times this method was called.
  metrics_->oemcrypto_get_device_id_.Increment(sts);
  if (sts == OEMCrypto_ERROR_SHORT_BUFFER) {
    id.resize(id_length);
    WithOecReadLock("GetInternalDeviceUniqueId Attempt 2", [&] {
      sts = OEMCrypto_GetDeviceID(&id[0], &id_length,
                                  requested_security_level_);
    });
    metrics_->oemcrypto_get_device_id_.Increment(sts);
  }

  if (sts == OEMCrypto_ERROR_NOT_IMPLEMENTED &&
      pre_provision_token_type_ == kClientTokenOemCert) {
    return GetTokenFromOemCert(device_id);
  } else {
    // Either the authentication root is a keybox or the device has transitioned
    // to using OEMCerts.
    // OEMCryptos, like the Level 3, that transition from Provisioning 2.0 to
    // 3.0 would have a new device ID, which would affect SPOID calculation.
    // In order to resolve this, we use OEMCrypto_GetDeviceID if it is
    // implemented, so the OEMCrypto can continue to report the same device ID.
    if (sts == OEMCrypto_SUCCESS) {
      device_id->assign(reinterpret_cast<char*>(&id[0]), id_length);
    }

    return MapOEMCryptoResult(
        sts, GET_DEVICE_ID_ERROR, "GetInternalDeviceUniqueId");
  }
}

CdmResponseType CryptoSession::GetExternalDeviceUniqueId(
    std::string* device_id) {
  if (device_id == NULL) {
    LOGE("CryptoSession::GetExternalDeviceUniqueId: device_id not provided");
    return PARAMETER_NULL;
  }
  std::string temp;
  CdmResponseType status = GetInternalDeviceUniqueId(&temp);

  if (status != NO_ERROR) return status;

  size_t id_length = 0;
  OEMCryptoResult sts;
  WithOecReadLock("GetExternalDeviceUniqueId", [&] {
    sts = OEMCrypto_GetDeviceID(NULL, &id_length, requested_security_level_);
  });
  metrics_->oemcrypto_get_device_id_.Increment(sts);

  if (sts == OEMCrypto_ERROR_NOT_IMPLEMENTED &&
      pre_provision_token_type_ == kClientTokenOemCert) {
    // To keep the size of the value passed back to the application down, hash
    // the large OEM Public Cert to a smaller value.
    temp = Sha256Hash(temp);
  }

  *device_id = temp;
  return NO_ERROR;
}

bool CryptoSession::GetApiVersion(uint32_t* version) {
  LOGV("CryptoSession::GetApiVersion");
  if (!open_) {
    LOGW("CryptoSession::GetApiVersion: session not open");
    return false;
  }
  return GetApiVersion(requested_security_level_, version);
}

bool CryptoSession::GetApiVersion(SecurityLevel security_level,
                                  uint32_t* version) {
  LOGV("CryptoSession::GetApiVersion");
  if (!version) {
    LOGE("CryptoSession::GetApiVersion: No buffer passed to method.");
    return false;
  }

  if (!IsInitialized()) {
    LOGW("CryptoSession::GetApiVersion: not initialized");
    return false;
  }

  WithOecReadLock("GetApiVersion", [&] {
    *version = OEMCrypto_APIVersion(security_level);
  });
  // Record the version into the metrics.
  metrics_->oemcrypto_api_version_.Record(*version);

  return true;
}

bool CryptoSession::GetSystemId(uint32_t* system_id) {
  if (!system_id) {
    LOGE("CryptoSession::GetSystemId: No buffer passed to method.");
    return false;
  }

  if (!IsInitialized() || !open_) {
    return false;
  }
  *system_id = system_id_;
  return true;
}

// This method gets the system id from the keybox key data.
// This method assumes that OEMCrypto has been initialized before making this
// call.
CdmResponseType CryptoSession::GetSystemIdInternal(uint32_t* system_id) {
  if (system_id == nullptr) {
    LOGE("CryptoSession::GetSystemIdInternal: No system_id passed to method.");
    return PARAMETER_NULL;
  }

  if (pre_provision_token_type_ == kClientTokenKeybox) {
    std::string token;
    CdmResponseType status = GetTokenFromKeybox(&token);

    if (status != NO_ERROR) return status;

    if (token.size() < 2*sizeof(uint32_t)) {
      LOGE("CryptoSession::GetSystemIdInternal: Keybox token too small: %d",
           token.size());
      return KEYBOX_TOKEN_TOO_SHORT;
    }

    // Decode 32-bit int encoded as network-byte-order byte array starting at
    // index 4.
    uint32_t* id = reinterpret_cast<uint32_t*>(&token[4]);
    *system_id = ntohl(*id);
    return NO_ERROR;

  } else if (pre_provision_token_type_ == kClientTokenOemCert) {
    // Get the OEM Cert
    std::string oem_cert;
    CdmResponseType status = GetTokenFromOemCert(&oem_cert);

    if (status != NO_ERROR) return status;

    if (!ExtractSystemIdFromOemCert(oem_cert, system_id))
      return EXTRACT_SYSTEM_ID_FROM_OEM_CERT_ERROR;

    return NO_ERROR;

  // TODO(blueeyes): Support loading the system id from a pre-provisioned
  // Drm certificate.
  } else if (pre_provision_token_type_ == kClientTokenDrmCert) {
    return NO_ERROR;
  } else {
    LOGE("CryptoSession::GetSystemIdInternal: "
         "Unsupported pre-provision token type %d", pre_provision_token_type_);
    return UNKNOWN_CLIENT_TOKEN_TYPE;
  }
}

bool CryptoSession::ExtractSystemIdFromOemCert(const std::string& oem_cert,
                                               uint32_t* system_id) {
  return ExtractExtensionValueFromCertificate(
      oem_cert, kWidevineSystemIdExtensionOid, /* cert_index */ 1, system_id);
}

CdmResponseType CryptoSession::GetProvisioningId(std::string* provisioning_id) {
  if (provisioning_id == nullptr) {
    LOGE("CryptoSession::GetProvisioningId : No buffer passed to method.");
    return PARAMETER_NULL;
  }

  if (!IsInitialized()) {
    return CRYPTO_SESSION_NOT_INITIALIZED;
  }

  if (pre_provision_token_type_ == kClientTokenOemCert) {
    // OEM Cert devices have no provisioning-unique ID embedded in them, so we
    // synthesize one by using the External Device-Unique ID and inverting all
    // the bits.
    CdmResponseType status = GetExternalDeviceUniqueId(provisioning_id);

    if (status != NO_ERROR) return status;

    for (size_t i = 0; i < provisioning_id->size(); ++i) {
      char value = (*provisioning_id)[i];
      (*provisioning_id)[i] = ~value;
    }

    return NO_ERROR;
  } else if (pre_provision_token_type_ == kClientTokenKeybox) {
    std::string token;
    CdmResponseType status = GetTokenFromKeybox(&token);

    if (status != NO_ERROR) return status;

    if (token.size() < 24) {
      LOGE("CryptoSession::GetProvisioningId: token size too small: %d",
           token.size());
      return KEYBOX_TOKEN_TOO_SHORT;
    }

    provisioning_id->assign(reinterpret_cast<char*>(&token[8]), 16);
    return NO_ERROR;
  } else {
    LOGE("CryptoSession::GetProvisioningId: unsupported token type: %d",
         pre_provision_token_type_);
    return UNKNOWN_CLIENT_TOKEN_TYPE;
  }
}

uint8_t CryptoSession::GetSecurityPatchLevel() {
  uint8_t patch;
  WithOecReadLock("GetSecurityPatchLevel", [&] {
    patch = OEMCrypto_Security_Patch_Level(requested_security_level_);
  });
  metrics_->oemcrypto_security_patch_level_.Record(patch);
  return patch;
}

CdmResponseType CryptoSession::Open(SecurityLevel requested_security_level) {
  LOGD("CryptoSession::Open: requested_security_level: %s",
       requested_security_level == kLevel3
           ? QUERY_VALUE_SECURITY_LEVEL_L3.c_str()
           : QUERY_VALUE_SECURITY_LEVEL_DEFAULT.c_str());
  if (!IsInitialized()) return UNKNOWN_ERROR;
  if (open_) return NO_ERROR;

  CdmResponseType result =
      GetProvisioningMethod(requested_security_level,
                            &pre_provision_token_type_);
  if (result != NO_ERROR) return result;

  OEMCrypto_SESSION sid;
  requested_security_level_ = requested_security_level;
  OEMCryptoResult sts;
  WithOecWriteLock("Open() calling OEMCrypto_OpenSession", [&] {
    sts = OEMCrypto_OpenSession(&sid, requested_security_level);
  });

  if (sts != OEMCrypto_SUCCESS) {
    WithStaticFieldReadLock(
        "Open() reporting OEMCrypto_OpenSession Failure",
        [&] {
          LOGE("OEMCrypto_Open failed: %d, open sessions: %ld, initialized: %d",
               sts, session_count_, (int)initialized_);
        });
    return MapOEMCryptoResult(sts, OPEN_CRYPTO_SESSION_ERROR, "Open");
  }

  oec_session_id_ = static_cast<CryptoSessionId>(sid);
  LOGV("OpenSession: id= %lu", oec_session_id_);
  open_ = true;

  // Get System ID and save it.
  if (GetSystemIdInternal(&system_id_) == NO_ERROR) {
    metrics_->crypto_session_system_id_.Record(system_id_);
  } else {
    LOGE("CryptoSession::Open: Failed to fetch system id.");
    metrics_->crypto_session_system_id_.SetError(LOAD_SYSTEM_ID_ERROR);
    return LOAD_SYSTEM_ID_ERROR;
  }

  uint64_t request_id_base;
  OEMCryptoResult random_sts;
  WithOecReadLock("Open() calling OEMCrypto_GetRandom", [&] {
    random_sts = OEMCrypto_GetRandom(
        reinterpret_cast<uint8_t*>(&request_id_base), sizeof(request_id_base));
  });
  metrics_->oemcrypto_get_random_.Increment(random_sts);
  uint64_t request_id_index =
      request_id_index_source_.fetch_add(1, std::memory_order_relaxed);
  request_id_ = HexEncode(reinterpret_cast<uint8_t*>(&request_id_base),
                          sizeof(request_id_base)) +
                HexEncode(reinterpret_cast<uint8_t*>(&request_id_index),
                          sizeof(request_id_index));

  if (!GetApiVersion(&api_version_)) {
    LOGE("CryptoSession::Open: GetApiVersion failed");
    return USAGE_SUPPORT_GET_API_FAILED;
  }

  CdmUsageSupportType usage_support_type;
  result = GetUsageSupportType(&usage_support_type);
  if (result == NO_ERROR) {
    metrics_->oemcrypto_usage_table_support_.Record(usage_support_type);
    if (usage_support_type == kUsageEntrySupport) {
      CdmSecurityLevel security_level = GetSecurityLevel();
      if (security_level == kSecurityLevelL1 ||
          security_level == kSecurityLevelL3) {
        {
          // This block cannot use |WithStaticFieldWriteLock| because it needs
          // to unlock the lock partway through.
          LOGV("Static Field Write Lock - Open() Initializing Usage Table");
          std::unique_lock<shared_mutex> auto_lock(static_field_mutex_);

          UsageTableHeader** header = security_level == kSecurityLevelL1
                                          ? &usage_table_header_l1_
                                          : &usage_table_header_l3_;
          if (*header == NULL) {
            *header = new UsageTableHeader();
            // Ignore errors since we do not know when a session is opened,
            // if it is intended to be used for offline/usage session related
            // or otherwise.
            auto_lock.unlock();
            bool is_usage_table_header_inited =
                (*header)->Init(security_level, this);
            auto_lock.lock();
            if (!is_usage_table_header_inited) {
              delete *header;
              *header = NULL;
              usage_table_header_ = NULL;
              return NO_ERROR;
            }
          }
          usage_table_header_ = *header;
          metrics_->usage_table_header_initial_size_.Record((*header)->size());
        }
      }
    }
  } else {
    metrics_->oemcrypto_usage_table_support_.SetError(result);
  }

  WithOecSessionLock("Open() calling key_session_.reset()", [&] {
    key_session_.reset(new ContentKeySession(oec_session_id_, metrics_));
  });

  return NO_ERROR;
}

void CryptoSession::Close() {
  LOGV("CloseSession: id=%lu open=%s", oec_session_id_,
       open_ ? "true" : "false");
  if (!open_) return;

  OEMCryptoResult close_sts;
  bool update_usage_table = false;
  WithOecWriteLock("Close", [&] {
    close_sts = OEMCrypto_CloseSession(oec_session_id_);
  });
  metrics_->oemcrypto_close_session_.Increment(close_sts);
  if (OEMCrypto_SUCCESS == close_sts) open_ = false;
  update_usage_table = update_usage_table_after_close_session_;
  if (close_sts == OEMCrypto_SUCCESS && update_usage_table &&
      usage_support_type_ == kUsageTableSupport) {
    UpdateUsageInformation();
  }
}

CdmResponseType CryptoSession::PrepareRequest(const std::string& message,
                                              bool is_provisioning,
                                              std::string* signature) {
  if (signature == nullptr) {
    LOGE("CryptoSession::PrepareRequest : No output destination provided.");
    return PARAMETER_NULL;
  }

  if (is_provisioning && (pre_provision_token_type_ == kClientTokenKeybox)) {
    CdmResponseType status = GenerateDerivedKeys(message);

    if (status != NO_ERROR) return status;

    return GenerateSignature(message, signature);
  } else {
    return GenerateRsaSignature(message, signature);
  }
}

CdmResponseType CryptoSession::PrepareRenewalRequest(const std::string& message,
                                                     std::string* signature) {
  if (signature == nullptr) {
    LOGE(
        "CryptoSession::PrepareRenewalRequest : No output destination "
        "provided.");
    return PARAMETER_NULL;
  }

  return GenerateSignature(message, signature);
}

CdmResponseType CryptoSession::LoadKeys(
    const std::string& message, const std::string& signature,
    const std::string& mac_key_iv, const std::string& mac_key,
    const std::vector<CryptoKey>& keys,
    const std::string& provider_session_token,
    const std::string& srm_requirement, CdmLicenseKeyType key_type) {
  CdmResponseType result = KEY_ADDED;

  OEMCryptoResult sts;
  WithOecSessionLock("LoadKeys", [&] {
    if (key_type == kLicenseKeyTypeEntitlement &&
        key_session_->Type() != KeySession::kEntitlement) {
      key_session_.reset(new EntitlementKeySession(oec_session_id_, metrics_));
    }

    LOGV("LoadKeys: id=%lu", oec_session_id_);
    sts = key_session_->LoadKeys(
        message, signature, mac_key_iv, mac_key, keys, provider_session_token,
        &cipher_mode_, srm_requirement);
  });

  if (sts != OEMCrypto_SUCCESS) {
    LOGE("CryptoSession::LoadKeys: OEMCrypto_LoadKeys error=%d", sts);
  }

  switch (sts) {
    case OEMCrypto_SUCCESS:
      if (!provider_session_token.empty())
        update_usage_table_after_close_session_ = true;
      result = KEY_ADDED;
      break;
    case OEMCrypto_ERROR_TOO_MANY_KEYS:
      result = INSUFFICIENT_CRYPTO_RESOURCES_4;
      break;
    case OEMCrypto_ERROR_USAGE_TABLE_UNRECOVERABLE:
      // Handle vendor specific error
      return NEED_PROVISIONING;
    case OEMCrypto_ERROR_SESSION_LOST_STATE:
      return SESSION_LOST_STATE_ERROR;
    case OEMCrypto_ERROR_SYSTEM_INVALIDATED:
      return SYSTEM_INVALIDATED_ERROR;
    default:
      result = LOAD_KEY_ERROR;
      break;
  }

  if (!provider_session_token.empty() &&
      usage_support_type_ == kUsageTableSupport) {
    UpdateUsageInformation();
  }
  return result;
}

CdmResponseType CryptoSession::LoadEntitledContentKeys(
    const std::vector<CryptoKey>& key_array) {
  OEMCryptoResult sts;
  WithOecSessionLock("LoadEntitledContentKeys", [&] {
    sts = key_session_->LoadEntitledContentKeys(key_array);
  });

  switch (sts) {
    case OEMCrypto_SUCCESS:
      return KEY_ADDED;
    case OEMCrypto_ERROR_INSUFFICIENT_RESOURCES:
      return INSUFFICIENT_CRYPTO_RESOURCES_6;
    case OEMCrypto_ERROR_INVALID_CONTEXT:
      return NOT_AN_ENTITLEMENT_SESSION;
    case OEMCrypto_KEY_NOT_ENTITLED:
      return NO_MATCHING_ENTITLEMENT_KEY;
    case OEMCrypto_ERROR_SESSION_LOST_STATE:
      return SESSION_LOST_STATE_ERROR;
    case OEMCrypto_ERROR_SYSTEM_INVALIDATED:
      return SYSTEM_INVALIDATED_ERROR;
    default:
      return LOAD_ENTITLED_CONTENT_KEYS_ERROR;
  }
}

CdmResponseType CryptoSession::LoadCertificatePrivateKey(
    std::string& wrapped_key) {
  // Call OEMCrypto_GetOEMPublicCertificate before OEMCrypto_LoadDeviceRSAKey
  // so it caches the OEMCrypto Public Key and then throw away result
  std::string temp_buffer(CERTIFICATE_DATA_SIZE, '\0');
  size_t buf_size = temp_buffer.size();
  uint8_t* buf = reinterpret_cast<uint8_t*>(&temp_buffer[0]);
  OEMCryptoResult sts;
  WithOecSessionLock(
      "LoadCertificatePrivateKey() calling OEMCrypto_GetOEMPublicCertificate",
      [&] {
        sts =
            OEMCrypto_GetOEMPublicCertificate(oec_session_id_, buf, &buf_size);
      });
  metrics_->oemcrypto_get_oem_public_certificate_.Increment(sts);

  LOGV("LoadDeviceRSAKey: id=%lu", oec_session_id_);
  WithOecSessionLock(
      "LoadCertificatePrivateKey() calling OEMCrypto_LoadDeviceRSAKey()",
      [&] {
        M_TIME(sts = OEMCrypto_LoadDeviceRSAKey(
                   oec_session_id_,
                   reinterpret_cast<const uint8_t*>(wrapped_key.data()),
                   wrapped_key.size()),
               metrics_, oemcrypto_load_device_rsa_key_, sts);
      });

  return MapOEMCryptoResult(
      sts, LOAD_DEVICE_RSA_KEY_ERROR, "LoadCertificatePrivateKey");
}

CdmResponseType CryptoSession::RefreshKeys(const std::string& message,
                                           const std::string& signature,
                                           int num_keys,
                                           const CryptoKey* key_array) {
  const uint8_t* msg = reinterpret_cast<const uint8_t*>(message.data());
  std::vector<OEMCrypto_KeyRefreshObject> load_key_array(num_keys);
  for (int i = 0; i < num_keys; ++i) {
    const CryptoKey* ki = &key_array[i];
    OEMCrypto_KeyRefreshObject* ko = &load_key_array[i];
    ko->key_id = GetSubstring(message, ki->key_id());
    bool has_key_control = ki->HasKeyControl();
    ko->key_control_iv =
        GetSubstring(message, ki->key_control_iv(), !has_key_control);
    ko->key_control =
        GetSubstring(message, ki->key_control(), !has_key_control);
  }
  LOGV("RefreshKeys: id=%lu", oec_session_id_);
  OEMCryptoResult refresh_sts;
  WithOecSessionLock("RefreshKeys", [&] {
    M_TIME(refresh_sts = OEMCrypto_RefreshKeys(
               oec_session_id_, msg, message.size(),
               reinterpret_cast<const uint8_t*>(signature.data()),
               signature.size(), num_keys, &load_key_array[0]),
           metrics_, oemcrypto_refresh_keys_, refresh_sts);
  });

  if (refresh_sts == OEMCrypto_SUCCESS) return KEY_ADDED;

  return MapOEMCryptoResult(
      refresh_sts, REFRESH_KEYS_ERROR, "RefreshKeys");
}

CdmResponseType CryptoSession::SelectKey(const std::string& key_id,
                                         CdmCipherMode cipher_mode) {
  OEMCryptoResult sts;
  WithOecSessionLock("SelectKey", [&] {
    sts = key_session_->SelectKey(key_id, cipher_mode);
  });

  switch (sts) {
    case OEMCrypto_SUCCESS:
      return NO_ERROR;
    case OEMCrypto_ERROR_KEY_EXPIRED:
      return NEED_KEY;
    case OEMCrypto_ERROR_INSUFFICIENT_HDCP:
      return INSUFFICIENT_OUTPUT_PROTECTION;
    case OEMCrypto_ERROR_ANALOG_OUTPUT:
      return ANALOG_OUTPUT_ERROR;
    case OEMCrypto_ERROR_INVALID_SESSION:
      return INVALID_SESSION_1;
    case OEMCrypto_ERROR_NO_DEVICE_KEY:
      return NO_DEVICE_KEY_1;
    case OEMCrypto_ERROR_NO_CONTENT_KEY:
      return NO_CONTENT_KEY_2;
    case OEMCrypto_KEY_NOT_LOADED:  // obsolete.
      return NO_CONTENT_KEY_3;
    case OEMCrypto_ERROR_INSUFFICIENT_RESOURCES:
      return INSUFFICIENT_CRYPTO_RESOURCES_2;
    case OEMCrypto_ERROR_UNKNOWN_FAILURE:
      return UNKNOWN_SELECT_KEY_ERROR_1;
    case OEMCrypto_ERROR_SESSION_LOST_STATE:
      return SESSION_LOST_STATE_ERROR;
    case OEMCrypto_ERROR_SYSTEM_INVALIDATED:
      return SYSTEM_INVALIDATED_ERROR;
    case OEMCrypto_ERROR_CONTROL_INVALID:
    case OEMCrypto_ERROR_KEYBOX_INVALID:
    default:
      return UNKNOWN_SELECT_KEY_ERROR_2;
  }
}

CdmResponseType CryptoSession::GenerateDerivedKeys(const std::string& message) {
  OEMCryptoResult sts;
  WithOecSessionLock("GenerateDerivedKeys without session_key", [&] {
    sts = key_session_->GenerateDerivedKeys(message);
  });

  return MapOEMCryptoResult(
      sts, GENERATE_DERIVED_KEYS_ERROR_2, "GenerateDerivedKeys");
}

CdmResponseType CryptoSession::GenerateDerivedKeys(
    const std::string& message, const std::string& session_key) {
  OEMCryptoResult sts;
  WithOecSessionLock("GenerateDerivedKeys with session_key", [&] {
    sts = key_session_->GenerateDerivedKeys(message, session_key);
  });

  return MapOEMCryptoResult(
      sts, GENERATE_DERIVED_KEYS_ERROR, "GenerateDerivedKeys");
}

CdmResponseType CryptoSession::GenerateSignature(const std::string& message,
                                                 std::string* signature) {
  LOGV("GenerateSignature: id=%lu", oec_session_id_);
  if (signature == nullptr) {
    LOGE("GenerateSignature: null signature string");
    return PARAMETER_NULL;
  }

  OEMCryptoResult sts;
  size_t length = signature->size();

  // At most two attempts.
  // The first attempt may fail due to buffer too short
  for (int i = 0; i < 2; ++i) {
    WithOecSessionLock("GenerateSignature", [&] {
      M_TIME(sts = OEMCrypto_GenerateSignature(
                 oec_session_id_,
                 reinterpret_cast<const uint8_t*>(message.data()),
                 message.size(),
                 reinterpret_cast<uint8_t*>(
                     const_cast<char*>(signature->data())),
                 &length),
             metrics_, oemcrypto_generate_signature_, sts,
             metrics::Pow2Bucket(length));
    });
    if (OEMCrypto_SUCCESS == sts) {
      // Trim signature buffer and done
      signature->resize(length);
      return NO_ERROR;
    }
    if (OEMCrypto_ERROR_SHORT_BUFFER != sts) {
      break;
    }

    // Retry with proper-sized signature buffer
    signature->resize(length);
  }

  return MapOEMCryptoResult(
      sts, GENERATE_SIGNATURE_ERROR, "GenerateSignature");
}

CdmResponseType CryptoSession::GenerateRsaSignature(const std::string& message,
                                                    std::string* signature) {
  LOGV("GenerateRsaSignature: id=%lu", oec_session_id_);
  if (signature == nullptr) {
    LOGE("GenerateRsaSignature: null signature string");
    return PARAMETER_NULL;
  }

  OEMCryptoResult sts;
  signature->resize(kRsaSignatureLength);
  size_t length = signature->size();

  // At most two attempts.
  // The first attempt may fail due to buffer too short
  for (int i = 0; i < 2; ++i) {
    WithOecSessionLock("GenerateRsaSignature", [&] {
      M_TIME(sts = OEMCrypto_GenerateRSASignature(
                 oec_session_id_,
                 reinterpret_cast<const uint8_t*>(message.data()),
                 message.size(),
                 reinterpret_cast<uint8_t*>(
                     const_cast<char*>(signature->data())),
                 &length, kSign_RSASSA_PSS),
             metrics_, oemcrypto_generate_rsa_signature_, sts,
             metrics::Pow2Bucket(length));
    });

    if (OEMCrypto_SUCCESS == sts) {
      // Trim signature buffer and done
      signature->resize(length);
      return NO_ERROR;
    }
    if (OEMCrypto_ERROR_SHORT_BUFFER != sts) {
      break;
    }

    // Retry with proper-sized signature buffer
    signature->resize(length);
  }

  return MapOEMCryptoResult(
      sts, RSA_SIGNATURE_GENERATION_ERROR, "OEMCrypto_GenerateRSASignature");
}

CdmResponseType CryptoSession::Decrypt(const CdmDecryptionParameters& params) {
  if (!is_destination_buffer_type_valid_) {
    if (!SetDestinationBufferType()) return UNKNOWN_ERROR;
  }

  OEMCrypto_DestBufferDesc buffer_descriptor;
  buffer_descriptor.type =
      params.is_secure ? destination_buffer_type_ : OEMCrypto_BufferType_Clear;

  if (params.is_secure &&
      buffer_descriptor.type == OEMCrypto_BufferType_Clear) {
    return SECURE_BUFFER_REQUIRED;
  }

  switch (buffer_descriptor.type) {
    case OEMCrypto_BufferType_Clear:
      buffer_descriptor.buffer.clear.address =
          static_cast<uint8_t*>(params.decrypt_buffer) +
          params.decrypt_buffer_offset;
      buffer_descriptor.buffer.clear.max_length =
          params.decrypt_buffer_length - params.decrypt_buffer_offset;
      break;
    case OEMCrypto_BufferType_Secure:
      buffer_descriptor.buffer.secure.handle = params.decrypt_buffer;
      buffer_descriptor.buffer.secure.offset = params.decrypt_buffer_offset;
      buffer_descriptor.buffer.secure.max_length = params.decrypt_buffer_length;
      break;
    case OEMCrypto_BufferType_Direct:
      buffer_descriptor.type = OEMCrypto_BufferType_Direct;
      buffer_descriptor.buffer.direct.is_video = params.is_video;
      break;
  }

  OEMCryptoResult sts = OEMCrypto_ERROR_NOT_IMPLEMENTED;
  if (!params.is_encrypted &&
      params.subsample_flags ==
          (OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample)) {
    WithOecSessionLock("Decrypt() calling CopyBuffer", [&] {
      M_TIME(sts = OEMCrypto_CopyBuffer(oec_session_id_, params.encrypt_buffer,
                                        params.encrypt_length,
                                        &buffer_descriptor,
                                        params.subsample_flags),
             metrics_, oemcrypto_copy_buffer_, sts,
             metrics::Pow2Bucket(params.encrypt_length));
    });

    if (sts == OEMCrypto_ERROR_BUFFER_TOO_LARGE &&
        params.encrypt_length > kMaximumChunkSize) {
      // OEMCrypto_CopyBuffer rejected the buffer as too large, so chunk it up
      // into 100 KiB sections.
      sts = CopyBufferInChunks(params, buffer_descriptor);
    }
  }
  if (api_version_ < kOemCryptoApiVersionSupportsSwitchingCipherMode) {
    if (params.is_encrypted && params.cipher_mode != cipher_mode_) {
      return INCORRECT_CRYPTO_MODE;
    }
  }
  if (params.is_encrypted || sts == OEMCrypto_ERROR_NOT_IMPLEMENTED) {
    OEMCrypto_CENCEncryptPatternDesc pattern_descriptor;
    pattern_descriptor.encrypt = params.pattern_descriptor.encrypt_blocks;
    pattern_descriptor.skip = params.pattern_descriptor.skip_blocks;
    pattern_descriptor.offset = 0;  // Deprecated field
    // Check if key needs to be selected
    if (params.is_encrypted) {
      CdmResponseType result = SelectKey(*params.key_id, params.cipher_mode);
      if (result != NO_ERROR) return result;
    }

    WithOecSessionLock("Decrypt() calling key_session_->Decrypt()", [&] {
      sts = key_session_->Decrypt(params, buffer_descriptor,
                                  pattern_descriptor);
    });

    if (sts == OEMCrypto_ERROR_BUFFER_TOO_LARGE) {
      // OEMCrypto_DecryptCENC rejected the buffer as too large, so chunk it
      // up into sections no more than 100 KiB. The exact chunk size needs to
      // be an even number of pattern repetitions long or else the pattern
      // will get out of sync.
      const size_t pattern_length =
          (pattern_descriptor.encrypt + pattern_descriptor.skip) *
          kAes128BlockSize;
      const size_t chunk_size =
          pattern_length > 0
              ? kMaximumChunkSize - (kMaximumChunkSize % pattern_length)
              : kMaximumChunkSize;

      if (params.encrypt_length > chunk_size) {
        sts = DecryptInChunks(params, buffer_descriptor, pattern_descriptor,
                              chunk_size);
      }
    }
  }

  switch (sts) {
    case OEMCrypto_SUCCESS:
      return NO_ERROR;
    case OEMCrypto_ERROR_INSUFFICIENT_RESOURCES:
      return INSUFFICIENT_CRYPTO_RESOURCES_5;
    case OEMCrypto_ERROR_KEY_EXPIRED:
      return NEED_KEY;
    case OEMCrypto_ERROR_INVALID_SESSION:
      return SESSION_NOT_FOUND_17;
    case OEMCrypto_ERROR_DECRYPT_FAILED:
    case OEMCrypto_ERROR_UNKNOWN_FAILURE:
      return DECRYPT_ERROR;
    case OEMCrypto_ERROR_INSUFFICIENT_HDCP:
      return INSUFFICIENT_OUTPUT_PROTECTION;
    case OEMCrypto_ERROR_ANALOG_OUTPUT:
      return ANALOG_OUTPUT_ERROR;
    case OEMCrypto_ERROR_OUTPUT_TOO_LARGE:
      return OUTPUT_TOO_LARGE_ERROR;
    case OEMCrypto_ERROR_SESSION_LOST_STATE:
      return SESSION_LOST_STATE_ERROR;
    case OEMCrypto_ERROR_SYSTEM_INVALIDATED:
      return SYSTEM_INVALIDATED_ERROR;
    default:
      return UNKNOWN_ERROR;
  }
}

bool CryptoSession::UsageInformationSupport(bool* has_support) {
  LOGV("CryptoSession::UsageInformationSupport");
  if (!open_) {
    LOGW("CryptoSession::UsageInformationSupport: session not open");
    return false;
  }
  return UsageInformationSupport(requested_security_level_, has_support);
}

bool CryptoSession::UsageInformationSupport(SecurityLevel security_level,
                                            bool* has_support) {
  LOGV("CryptoSession::UsageInformationSupport");
  if (!IsInitialized()) {
    LOGW("CryptoSession::UsageInformationSupport: not initialized");
    return false;
  }

  WithOecReadLock("UsageInformationSupport", [&] {
    *has_support = OEMCrypto_SupportsUsageTable(security_level);
  });
  return true;
}

CdmResponseType CryptoSession::UpdateUsageInformation() {
  LOGV("CryptoSession::UpdateUsageInformation: id=%lu",
       oec_session_id_);
  if (!IsInitialized()) return UNKNOWN_ERROR;

  if (usage_table_header_ != NULL) {
    LOGV("UpdateUsageInformation: deprecated for OEMCrypto v13+");
    return NO_ERROR;
  }

  OEMCryptoResult status;
  WithOecWriteLock("UpdateUsageInformation", [&] {
    status = OEMCrypto_UpdateUsageTable();
  });
  metrics_->oemcrypto_update_usage_table_.Increment(status);
  if (status != OEMCrypto_SUCCESS) {
    LOGE("CryptoSession::UpdateUsageInformation: error=%ld", status);
    return UNKNOWN_ERROR;
  }
  return NO_ERROR;
}

CdmResponseType CryptoSession::DeactivateUsageInformation(
    const std::string& provider_session_token) {
  LOGV("DeactivateUsageInformation: id=%lu", oec_session_id_);

  uint8_t* pst = reinterpret_cast<uint8_t*>(
      const_cast<char*>(provider_session_token.data()));

  // TODO(fredgc or rfrias): make sure oec_session_id_ is valid.
  OEMCryptoResult status;
  WithOecWriteLock("DeactivateUsageInformation", [&] {
    status = OEMCrypto_DeactivateUsageEntry(
        oec_session_id_, pst, provider_session_token.length());
  });
  metrics_->oemcrypto_deactivate_usage_entry_.Increment(status);

  if (status != OEMCrypto_SUCCESS) {
    LOGE("CryptoSession::DeactivateUsageInformation: error=%ld", status);
  }

  switch (status) {
    case OEMCrypto_SUCCESS:
      return NO_ERROR;
    case OEMCrypto_ERROR_INVALID_CONTEXT:
      return KEY_CANCELED;
    case OEMCrypto_ERROR_SESSION_LOST_STATE:
      return SESSION_LOST_STATE_ERROR;
    case OEMCrypto_ERROR_SYSTEM_INVALIDATED:
      return SYSTEM_INVALIDATED_ERROR;
    default:
      return DEACTIVATE_USAGE_ENTRY_ERROR;
  }
}

CdmResponseType CryptoSession::GenerateUsageReport(
    const std::string& provider_session_token, std::string* usage_report,
    UsageDurationStatus* usage_duration_status, int64_t* seconds_since_started,
    int64_t* seconds_since_last_played) {
  LOGV("GenerateUsageReport: id=%lu", oec_session_id_);

  if (nullptr == usage_report) {
    LOGE("CryptoSession::GenerateUsageReport: usage_report parameter is null");
    return PARAMETER_NULL;
  }

  uint8_t* pst = reinterpret_cast<uint8_t*>(
      const_cast<char*>(provider_session_token.data()));

  size_t usage_length = 0;
  OEMCryptoResult status;
  WithOecWriteLock("GenerateUsageReport Attempt 1", [&] {
    status = OEMCrypto_ReportUsage(
        oec_session_id_, pst, provider_session_token.length(), NULL,
        &usage_length);
  });
  metrics_->oemcrypto_report_usage_.Increment(status);

  if (status != OEMCrypto_SUCCESS && status != OEMCrypto_ERROR_SHORT_BUFFER) {
    return MapOEMCryptoResult(
        status, GENERATE_USAGE_REPORT_ERROR, "GenerateUsageReport");
  }

  std::vector<uint8_t> buffer(usage_length);

  WithOecWriteLock("GenerateUsageReport Attempt 2", [&] {
    status = OEMCrypto_ReportUsage(oec_session_id_, pst,
                                   provider_session_token.length(), &buffer[0],
                                   &usage_length);
  });
  metrics_->oemcrypto_report_usage_.Increment(status);

  if (status != OEMCrypto_SUCCESS) {
    return MapOEMCryptoResult(
        status, GENERATE_USAGE_REPORT_ERROR, "OEMCrypto_ReportUsage");
  }

  if (usage_length != buffer.size()) {
    buffer.resize(usage_length);
  }
  (*usage_report) =
      std::string(reinterpret_cast<const char*>(&buffer[0]), buffer.size());

  Unpacked_PST_Report pst_report(&buffer[0]);
  *usage_duration_status = kUsageDurationsInvalid;
  if (usage_length < pst_report.report_size()) {
    LOGE("CryptoSession::GenerateUsageReport: usage report too small=%ld",
         usage_length);
    return NO_ERROR;  // usage report available but no duration information
  }

  if (kUnused == pst_report.status()) {
    *usage_duration_status = kUsageDurationPlaybackNotBegun;
    return NO_ERROR;
  }
  LOGV("OEMCrypto_PST_Report.status: %d\n", pst_report.status());
  LOGV("OEMCrypto_PST_Report.clock_security_level: %d\n",
       pst_report.clock_security_level());
  LOGV("OEMCrypto_PST_Report.pst_length: %d\n", pst_report.pst_length());
  LOGV("OEMCrypto_PST_Report.padding: %d\n", pst_report.padding());
  LOGV("OEMCrypto_PST_Report.seconds_since_license_received: %lld\n",
       pst_report.seconds_since_license_received());
  LOGV("OEMCrypto_PST_Report.seconds_since_first_decrypt: %lld\n",
       pst_report.seconds_since_first_decrypt());
  LOGV("OEMCrypto_PST_Report.seconds_since_last_decrypt: %lld\n",
       pst_report.seconds_since_last_decrypt());
  LOGV("OEMCrypto_PST_Report: %s\n", b2a_hex(*usage_report).c_str());

  if (kInactiveUnused == pst_report.status()) {
    *usage_duration_status = kUsageDurationPlaybackNotBegun;
    return NO_ERROR;
  }
  // Before OEMCrypto v13, When usage report state is inactive, we have to
  // deduce whether the license was ever used.
  if (kInactive == pst_report.status() &&
      (0 > pst_report.seconds_since_first_decrypt() ||
       pst_report.seconds_since_license_received() <
           pst_report.seconds_since_first_decrypt())) {
    *usage_duration_status = kUsageDurationPlaybackNotBegun;
    return NO_ERROR;
  }

  *usage_duration_status = kUsageDurationsValid;
  *seconds_since_started = pst_report.seconds_since_first_decrypt();
  *seconds_since_last_played = pst_report.seconds_since_last_decrypt();
  return NO_ERROR;
}

CdmResponseType CryptoSession::ReleaseUsageInformation(
    const std::string& message, const std::string& signature,
    const std::string& provider_session_token) {
  LOGV("ReleaseUsageInformation: id=%lu", oec_session_id_);
  if (usage_table_header_ != NULL) {
    LOGW("ReleaseUsageInformation: deprecated for OEMCrypto v13+");
    return NO_ERROR;
  }

  const uint8_t* msg = reinterpret_cast<const uint8_t*>(message.data());
  const uint8_t* sig = reinterpret_cast<const uint8_t*>(signature.data());
  const uint8_t* pst = msg + GetOffset(message, provider_session_token);

  OEMCryptoResult status;
  WithOecWriteLock("ReleaseUsageInformation", [&] {
    status = OEMCrypto_DeleteUsageEntry(
        oec_session_id_, pst, provider_session_token.length(), msg,
        message.length(), sig, signature.length());
  });
  metrics_->oemcrypto_delete_usage_entry_.Increment(status);

  if (OEMCrypto_SUCCESS != status) {
    LOGE("CryptoSession::ReleaseUsageInformation: Report Usage error=%ld",
         status);
    return UNKNOWN_ERROR;
  }

  if (usage_support_type_ == kUsageTableSupport) UpdateUsageInformation();
  return NO_ERROR;
}

CdmResponseType CryptoSession::DeleteUsageInformation(
    const std::string& provider_session_token) {
  CdmResponseType response = NO_ERROR;
  LOGV("CryptoSession::DeleteUsageInformation");
  OEMCryptoResult status;
  WithOecWriteLock("DeleteUsageInformation", [&] {
    status = OEMCrypto_ForceDeleteUsageEntry(
        reinterpret_cast<const uint8_t*>(provider_session_token.c_str()),
        provider_session_token.length());
  });
  metrics_->oemcrypto_force_delete_usage_entry_.Increment(status);
  if (OEMCrypto_SUCCESS != status) {
    LOGE(
        "CryptoSession::DeleteUsageInformation: Delete Usage Table error "
        "= %ld",
        status);
    response = UNKNOWN_ERROR;
  }
  if (usage_support_type_ == kUsageTableSupport) UpdateUsageInformation();
  return response;
}

CdmResponseType CryptoSession::DeleteMultipleUsageInformation(
    const std::vector<std::string>& provider_session_tokens) {
  LOGV("CryptoSession::DeleteMultipleUsageInformation");
  CdmResponseType response = NO_ERROR;
  WithOecWriteLock("DeleteMultipleUsageInformation", [&] {
    for (size_t i = 0; i < provider_session_tokens.size(); ++i) {
      OEMCryptoResult status = OEMCrypto_ForceDeleteUsageEntry(
          reinterpret_cast<const uint8_t*>(provider_session_tokens[i].c_str()),
          provider_session_tokens[i].length());
      metrics_->oemcrypto_force_delete_usage_entry_.Increment(status);
      if (OEMCrypto_SUCCESS != status) {
        LOGW(
            "CryptoSession::DeleteMultipleUsageInformation: "
            "Delete Usage Table error =%ld",
            status);
        response = UNKNOWN_ERROR;
      }
    }
  });
  if (usage_support_type_ == kUsageTableSupport) UpdateUsageInformation();
  return response;
}

CdmResponseType CryptoSession::DeleteAllUsageReports() {
  LOGV("DeleteAllUsageReports");
  OEMCryptoResult status;
  WithOecWriteLock("DeleteAllUsageReports", [&] {
    status = OEMCrypto_DeleteOldUsageTable();
  });
  metrics_->oemcrypto_delete_usage_table_.Increment(status);
  if (OEMCrypto_SUCCESS != status) {
    LOGE(
        "CryptoSession::DeleteAllUsageReports: Delete Usage Table error "
        "=%ld",
        status);
  }

  if (usage_support_type_ == kUsageTableSupport) UpdateUsageInformation();
  return NO_ERROR;
}

bool CryptoSession::IsAntiRollbackHwPresent() {
  bool is_present;
  WithOecReadLock("IsAntiRollbackHwPresent", [&] {
    is_present = OEMCrypto_IsAntiRollbackHwPresent(requested_security_level_);
  });
  metrics_->oemcrypto_is_anti_rollback_hw_present_.Record(is_present);
  return is_present;
}

CdmResponseType CryptoSession::GenerateNonce(uint32_t* nonce) {
  if (nonce == nullptr) {
    LOGE("input parameter is null");
    return PARAMETER_NULL;
  }

  OEMCryptoResult result;
  WithOecSessionLock("GenerateNonce", [&] {
    result = OEMCrypto_GenerateNonce(oec_session_id_, nonce);
  });
  metrics_->oemcrypto_generate_nonce_.Increment(result);

  return MapOEMCryptoResult(result, NONCE_GENERATION_ERROR, "GenerateNonce");
}

bool CryptoSession::SetDestinationBufferType() {
  if (Properties::oem_crypto_use_secure_buffers()) {
    destination_buffer_type_ = OEMCrypto_BufferType_Secure;
  } else if (Properties::oem_crypto_use_fifo()) {
    destination_buffer_type_ = OEMCrypto_BufferType_Direct;
  } else if (Properties::oem_crypto_use_userspace_buffers()) {
    destination_buffer_type_ = OEMCrypto_BufferType_Clear;
  } else {
    return false;
  }

  is_destination_buffer_type_valid_ = true;
  return true;
}

CdmResponseType CryptoSession::RewrapCertificate(
    const std::string& signed_message,
    const std::string& signature,
    const std::string& nonce,
    const std::string& private_key,
    const std::string& iv,
    const std::string& wrapping_key,
    std::string* wrapped_private_key) {
  LOGV("CryptoSession::RewrapCertificate, session id=%lu", oec_session_id_);

  if (pre_provision_token_type_ == kClientTokenKeybox) {
    return RewrapDeviceRSAKey(signed_message, signature, nonce, private_key, iv,
                              wrapped_private_key);

  } else if (pre_provision_token_type_ == kClientTokenOemCert) {
    return RewrapDeviceRSAKey30(signed_message, nonce, private_key, iv,
                                wrapping_key, wrapped_private_key);

  } else {
    LOGE(
        "CryptoSession::RewrapCertificate, Bad pre-provision type=%d: "
        "session id=%lu",
        pre_provision_token_type_, oec_session_id_);
    return UNKNOWN_CLIENT_TOKEN_TYPE;
  }
}

CdmResponseType CryptoSession::RewrapDeviceRSAKey(
    const std::string& message,
    const std::string& signature,
    const std::string& nonce,
    const std::string& enc_rsa_key,
    const std::string& rsa_key_iv,
    std::string* wrapped_rsa_key) {
  LOGV("CryptoSession::RewrapDeviceRSAKey, session id=%lu", oec_session_id_);

  const uint8_t* signed_msg = reinterpret_cast<const uint8_t*>(message.data());
  const uint8_t* msg_rsa_key = NULL;
  const uint8_t* msg_rsa_key_iv = NULL;
  const uint32_t* msg_nonce = NULL;
  if (enc_rsa_key.size() >= MAC_KEY_SIZE && rsa_key_iv.size() >= KEY_IV_SIZE) {
    msg_rsa_key = signed_msg + GetOffset(message, enc_rsa_key);
    msg_rsa_key_iv = signed_msg + GetOffset(message, rsa_key_iv);
    msg_nonce = reinterpret_cast<const uint32_t*>(signed_msg +
                                                  GetOffset(message, nonce));
  }

  // Gets wrapped_rsa_key_length by passing NULL as uint8_t* wrapped_rsa_key
  // and 0 as wrapped_rsa_key_length.
  size_t wrapped_rsa_key_length = 0;
  OEMCryptoResult status;
  WithOecSessionLock("RewrapDeviceRSAKey Attempt 1", [&] {
    M_TIME(status = OEMCrypto_RewrapDeviceRSAKey(
               oec_session_id_, signed_msg, message.size(),
               reinterpret_cast<const uint8_t*>(signature.data()),
               signature.size(), msg_nonce, msg_rsa_key, enc_rsa_key.size(),
               msg_rsa_key_iv, NULL, &wrapped_rsa_key_length),
           metrics_, oemcrypto_rewrap_device_rsa_key_, status);
  });

  if (status != OEMCrypto_ERROR_SHORT_BUFFER) {
    return MapOEMCryptoResult(
        status, REWRAP_DEVICE_RSA_KEY_ERROR, "RewrapDeviceRSAKey");
  }

  wrapped_rsa_key->resize(wrapped_rsa_key_length);
  WithOecSessionLock("RewrapDeviceRSAKey Attempt 2", [&] {
    M_TIME(status = OEMCrypto_RewrapDeviceRSAKey(
               oec_session_id_, signed_msg, message.size(),
               reinterpret_cast<const uint8_t*>(signature.data()),
               signature.size(), msg_nonce, msg_rsa_key, enc_rsa_key.size(),
               msg_rsa_key_iv,
               reinterpret_cast<uint8_t*>(&(*wrapped_rsa_key)[0]),
               &wrapped_rsa_key_length),
           metrics_, oemcrypto_rewrap_device_rsa_key_, status);
  });

  wrapped_rsa_key->resize(wrapped_rsa_key_length);

  return MapOEMCryptoResult(
      status, REWRAP_DEVICE_RSA_KEY_ERROR, "RewrapDeviceRSAKey");
}

CdmResponseType CryptoSession::RewrapDeviceRSAKey30(
    const std::string& message,
    const std::string& nonce,
    const std::string& private_key,
    const std::string& iv,
    const std::string& wrapping_key,
    std::string* wrapped_private_key) {
  LOGV("CryptoSession::RewrapDeviceRSAKey30, session id=%lu",
       oec_session_id_);

  const uint8_t* signed_msg = reinterpret_cast<const uint8_t*>(message.data());
  const uint8_t* msg_private_key = NULL;
  const uint8_t* msg_iv = NULL;
  const uint32_t* msg_nonce = NULL;
  const uint8_t* msg_wrapping_key = NULL;
  if (private_key.size() >= MAC_KEY_SIZE && iv.size() >= KEY_IV_SIZE) {
    msg_private_key = signed_msg + GetOffset(message, private_key);
    msg_iv = signed_msg + GetOffset(message, iv);
    msg_nonce = reinterpret_cast<const uint32_t*>(signed_msg +
                                                  GetOffset(message, nonce));
    msg_wrapping_key = signed_msg + GetOffset(message, wrapping_key);
  }

  // Gets wrapped_rsa_key_length by passing NULL as uint8_t* wrapped_rsa_key
  // and 0 as wrapped_rsa_key_length.
  size_t wrapped_private_key_length = 0;
  OEMCryptoResult status;
  WithOecSessionLock("RewrapDeviceRSAKey30 Attempt 1", [&] {
    M_TIME(
        status = OEMCrypto_RewrapDeviceRSAKey30(
            oec_session_id_, msg_nonce, msg_wrapping_key, wrapping_key.size(),
            msg_private_key, private_key.size(), msg_iv, NULL,
            &wrapped_private_key_length),
        metrics_,
        oemcrypto_rewrap_device_rsa_key_30_,
        status);
  });

  if (status != OEMCrypto_ERROR_SHORT_BUFFER) {
    return MapOEMCryptoResult(
        status, REWRAP_DEVICE_RSA_KEY_30_ERROR, "RewrapDeviceRSAKey30");
  }

  wrapped_private_key->resize(wrapped_private_key_length);
  WithOecSessionLock("RewrapDeviceRSAKey30 Attempt 2", [&] {
    M_TIME(
        status = OEMCrypto_RewrapDeviceRSAKey30(
            oec_session_id_, msg_nonce, msg_wrapping_key, wrapping_key.size(),
            msg_private_key, private_key.size(), msg_iv,
            reinterpret_cast<uint8_t*>(&(*wrapped_private_key)[0]),
            &wrapped_private_key_length),
        metrics_,
        oemcrypto_rewrap_device_rsa_key_30_,
        status);
  });

  wrapped_private_key->resize(wrapped_private_key_length);

  return MapOEMCryptoResult(
      status, REWRAP_DEVICE_RSA_KEY_30_ERROR, "RewrapDeviceRSAKey30");
}

CdmResponseType CryptoSession::GetHdcpCapabilities(HdcpCapability* current,
                                                   HdcpCapability* max) {
  LOGV("CryptoSession::GetHdcpCapabilities: id=%lu", oec_session_id_);
  if (!open_) {
    LOGW("CryptoSession::GetHdcpCapabilities: session not open");
    return CRYPTO_SESSION_NOT_OPEN;
  }
  return GetHdcpCapabilities(requested_security_level_, current, max);
}

CdmResponseType CryptoSession::GetHdcpCapabilities(SecurityLevel security_level,
                                                   HdcpCapability* current,
                                                   HdcpCapability* max) {
  LOGV("CryptoSession::GetHdcpCapabilities");
  if (!IsInitialized()) {
    LOGW("CryptoSession::GetHdcpCapabilities: not initialized");
    return CRYPTO_SESSION_NOT_INITIALIZED;
  }
  if (current == nullptr || max == nullptr) {
    LOGE(
        "CryptoSession::GetHdcpCapabilities: |current|, |max| cannot be "
        "NULL");
    return PARAMETER_NULL;
  }
  OEMCryptoResult status;
  WithOecReadLock("GetHdcpCapabilities", [&] {
    status = OEMCrypto_GetHDCPCapability(security_level, current, max);
  });

  if (OEMCrypto_SUCCESS == status) {
    metrics_->oemcrypto_current_hdcp_capability_.Record(*current);
    metrics_->oemcrypto_max_hdcp_capability_.Record(*max);
  } else {
    metrics_->oemcrypto_current_hdcp_capability_.SetError(status);
    metrics_->oemcrypto_max_hdcp_capability_.SetError(status);
  }

  return MapOEMCryptoResult(
      status, GET_HDCP_CAPABILITY_FAILED, "GetHDCPCapability");
}

bool CryptoSession::GetSupportedCertificateTypes(
    SupportedCertificateTypes* support) {
  LOGV("GetSupportedCertificateTypes: id=%lu", oec_session_id_);
  if (!IsInitialized()) return false;
  if (support == NULL) {
    LOGE(
        "CryptoSession::GetSupportedCertificateTypes: |support| cannot be "
        "NULL");
    return false;
  }

  uint32_t oec_support;
  WithOecReadLock("GetSupportedCertificateTypes", [&] {
    oec_support = OEMCrypto_SupportedCertificates(requested_security_level_);
  });
  support->rsa_2048_bit = oec_support & OEMCrypto_Supports_RSA_2048bit;
  support->rsa_3072_bit = oec_support & OEMCrypto_Supports_RSA_3072bit;
  support->rsa_cast = oec_support & OEMCrypto_Supports_RSA_CAST;
  return true;
}

CdmResponseType CryptoSession::GetRandom(size_t data_length,
                                         uint8_t* random_data) {
  if (random_data == nullptr) {
    LOGE("CryptoSession::GetRandom: random data destination not provided");
    return PARAMETER_NULL;
  }
  OEMCryptoResult sts;
  WithOecReadLock("GetRandom", [&] {
    sts = OEMCrypto_GetRandom(random_data, data_length);
  });
  metrics_->oemcrypto_get_random_.Increment(sts);

  return MapOEMCryptoResult(sts, RANDOM_GENERATION_ERROR, "GetRandom");
}

CdmResponseType CryptoSession::GetNumberOfOpenSessions(
    SecurityLevel security_level,
    size_t* count) {
  LOGV("GetNumberOfOpenSessions");
  if (!IsInitialized()) {
    LOGW("CryptoSession::GetNumberOfOpenSessions: not initialized");
    return CRYPTO_SESSION_NOT_INITIALIZED;
  }
  if (count == nullptr) {
    LOGE("CryptoSession::GetNumberOfOpenSessions: |count| cannot be NULL");
    return PARAMETER_NULL;
  }

  size_t sessions_count;
  OEMCryptoResult status;
  WithOecReadLock("GetNumberOfOpenSessions", [&] {
    status = OEMCrypto_GetNumberOfOpenSessions(security_level, &sessions_count);
  });

  if (OEMCrypto_SUCCESS == status) {
    metrics_->oemcrypto_number_of_open_sessions_.Record(sessions_count);
    *count = sessions_count;
  } else {
    metrics_->oemcrypto_number_of_open_sessions_.SetError(status);
  }

  return MapOEMCryptoResult(
      status, GET_NUMBER_OF_OPEN_SESSIONS_ERROR, "GetNumberOfOpenSessions");
}

CdmResponseType CryptoSession::GetMaxNumberOfSessions(
    SecurityLevel security_level,
    size_t* max) {
  LOGV("GetMaxNumberOfSessions");
  if (!IsInitialized()) {
    LOGW("CryptoSession::GetMaxNumberOfSessions: not initialized");
    return CRYPTO_SESSION_NOT_INITIALIZED;
  }

  if (max == nullptr) {
    LOGE("CryptoSession::GetMaxNumberOfSessions: |max| cannot be NULL");
    return PARAMETER_NULL;
  }

  size_t max_sessions = 0;
  OEMCryptoResult status;
  WithOecReadLock("GetMaxNumberOfSessions", [&] {
    status = OEMCrypto_GetMaxNumberOfSessions(security_level, &max_sessions);
  });

  if (OEMCrypto_SUCCESS == status) {
    metrics_->oemcrypto_max_number_of_sessions_.Record(max_sessions);
    *max = max_sessions;
  } else {
    metrics_->oemcrypto_max_number_of_sessions_.SetError(status);
  }

  return MapOEMCryptoResult(status, GET_MAX_NUMBER_OF_OPEN_SESSIONS_ERROR,
                            "GetMaxNumberOfOpenSessions");
}

CdmResponseType CryptoSession::GetSrmVersion(uint16_t* srm_version) {
  LOGV("GetSrmVersion");
  if (!IsInitialized()) return CRYPTO_SESSION_NOT_INITIALIZED;
  if (srm_version == nullptr) {
    LOGE("CryptoSession::GetSrmVersion: |srm_version| cannot be NULL");
    return PARAMETER_NULL;
  }

  OEMCryptoResult status;
  WithOecReadLock("GetSrmVersion", [&] {
    status = OEMCrypto_GetCurrentSRMVersion(srm_version);
  });

  // SRM is an optional feature. Whether it is implemented is up to the
  // discretion of OEMs
  if (status == OEMCrypto_ERROR_NOT_IMPLEMENTED) {
    LOGV("CryptoSession::GetSrmVersion: OEMCrypto_GetCurrentSRMVersion not "
         "implemented");
    return NOT_IMPLEMENTED_ERROR;
  }

  return MapOEMCryptoResult(
      status, GET_SRM_VERSION_ERROR, "GetCurrentSRMVersion");
}

bool CryptoSession::IsSrmUpdateSupported() {
  LOGV("IsSrmUpdateSupported");
  if (!IsInitialized()) return false;
  return WithOecReadLock("IsSrmUpdateSupported", [&] {
    return OEMCrypto_IsSRMUpdateSupported();
  });
}

CdmResponseType CryptoSession::LoadSrm(const std::string& srm) {
  LOGV("LoadSrm");
  if (!IsInitialized()) return CRYPTO_SESSION_NOT_INITIALIZED;
  if (srm.empty()) {
    LOGE("CryptoSession::LoadSrm: |srm| cannot be empty");
    return INVALID_SRM_LIST;
  }

  OEMCryptoResult status;
  WithOecWriteLock("LoadSrm", [&] {
    status = OEMCrypto_LoadSRM(
        reinterpret_cast<const uint8_t*>(srm.data()), srm.size());
  });

  return MapOEMCryptoResult(status, LOAD_SRM_ERROR, "LoadSRM");
}

bool CryptoSession::GetResourceRatingTier(uint32_t* tier) {
  LOGV("CryptoSession::GetResourceRatingTier");
  if (!open_) {
    LOGW("CryptoSession::GetResourceRatingTier: session not open");
    return false;
  }
  return GetResourceRatingTier(requested_security_level_, tier);
}

bool CryptoSession::GetResourceRatingTier(SecurityLevel security_level,
                                          uint32_t* tier) {
  LOGV("GetResourceRatingTier");
  if (!IsInitialized()) {
    LOGW("CryptoSession::GetResourceRatingTier: not initialized");
    return false;
  }
  if (tier == nullptr) {
    LOGE("tier destination not provided");
    return false;
  }
  WithOecReadLock("GetResourceRatingTier", [&] {
    *tier = OEMCrypto_ResourceRatingTier(security_level);
    metrics_->oemcrypto_resource_rating_tier_.Record(*tier);
  });
  if (*tier < RESOURCE_RATING_TIER_LOW || *tier > RESOURCE_RATING_TIER_HIGH) {
    uint32_t api_version;
    if (GetApiVersion(security_level, &api_version)) {
      if (api_version >= OEM_CRYPTO_API_VERSION_SUPPORTS_RESOURCE_RATING_TIER) {
        LOGW("GetResourceRatingTier: invalid resource rating tier: %d", *tier);
      }
    }
  }
  return true;
}

bool CryptoSession::GetBuildInformation(std::string* info) {
  LOGV("CryptoSession::GetBuildInformation");
  if (!open_) {
    LOGW("CryptoSession::GetBuildInformation: session not open");
    return false;
  }
  return GetBuildInformation(requested_security_level_, info);
}

bool CryptoSession::GetBuildInformation(SecurityLevel security_level,
                                        std::string* info) {
  LOGV("GetBuildInformation");
  if (!IsInitialized()) {
    LOGW("CryptoSession::GetBuildInformation: not initialized");
    return false;
  }
  if (info == nullptr) {
    LOGE("CryptoSession::GetBuildInformation: |info| cannot be empty");
    return false;
  }
  const char* build_information;
  WithOecReadLock("GetBuildInformation", [&] {
    build_information = OEMCrypto_BuildInformation(security_level);
  });
  if (build_information == nullptr) {
    LOGE("CryptoSession::GetBuildInformation: returned null");
    return false;
  }

  info->assign(build_information);
  return true;
}

uint32_t CryptoSession::IsDecryptHashSupported(SecurityLevel security_level) {
  LOGV("IsDecryptHashSupported");
  if (!IsInitialized()) {
    LOGW("CryptoSession::IsDecryptHashSupported: not initialized");
    return false;
  }

  uint32_t secure_decrypt_support;
  WithOecReadLock("IsDecryptHashSupported", [&] {
    secure_decrypt_support = OEMCrypto_SupportsDecryptHash(security_level);
  });
  switch (secure_decrypt_support) {
    case OEMCrypto_Hash_Not_Supported:
    case OEMCrypto_CRC_Clear_Buffer:
    case OEMCrypto_Partner_Defined_Hash:
      break;
    default:
      LOGW("OEMCrypto_SupportsDecryptHash returned unexpected result: %d",
           secure_decrypt_support);
      secure_decrypt_support = OEMCrypto_Hash_Not_Supported;
      break;
  }
  return secure_decrypt_support;
}

CdmResponseType CryptoSession::SetDecryptHash(
    uint32_t frame_number,
    const std::string& hash) {
  LOGV("SetDecryptHash");
  OEMCryptoResult sts;
  WithOecSessionLock("SetDecryptHash", [&] {
    sts = OEMCrypto_SetDecryptHash(
        oec_session_id_, frame_number,
        reinterpret_cast<const uint8_t*>(hash.data()), hash.size());
    metrics_->oemcrypto_set_decrypt_hash_.Increment(sts);
  });

  return MapOEMCryptoResult(sts, SET_DECRYPT_HASH_ERROR, "SetDecryptHash");
}

CdmResponseType CryptoSession::GetDecryptHashError(std::string* error_string) {
  LOGV("GetDecryptHashError");
  if (error_string == nullptr) {
    LOGE("CryptoSession::GetDecryptHashError: |error_string| not provided");
    return PARAMETER_NULL;
  }
  error_string->clear();

  uint32_t failed_frame_number = 0;
  OEMCryptoResult sts;
  WithOecSessionLock("GetDecryptHashError", [&] {
    sts = OEMCrypto_GetHashErrorCode(oec_session_id_, &failed_frame_number);
  });
  error_string->assign(std::to_string(sts));

  if (sts != OEMCrypto_SUCCESS) {
    LOGE("GetDecryptHashError: failed with error %d", sts);
  }

  switch (sts) {
    case OEMCrypto_SUCCESS:
    case OEMCrypto_ERROR_BAD_HASH:
      error_string->assign(std::to_string(sts));
      error_string->append(",");
      error_string->append(std::to_string(failed_frame_number));
      return NO_ERROR;
    case OEMCrypto_ERROR_SESSION_LOST_STATE:
      return SESSION_LOST_STATE_ERROR;
    case OEMCrypto_ERROR_SYSTEM_INVALIDATED:
      return SYSTEM_INVALIDATED_ERROR;
    case OEMCrypto_ERROR_UNKNOWN_FAILURE:
    case OEMCrypto_ERROR_NOT_IMPLEMENTED:
    default:
      return GET_DECRYPT_HASH_ERROR;
  }
}

CdmResponseType CryptoSession::GenericEncrypt(const std::string& in_buffer,
                                              const std::string& key_id,
                                              const std::string& iv,
                                              CdmEncryptionAlgorithm algorithm,
                                              std::string* out_buffer) {
  LOGV("GenericEncrypt: id=%lu", oec_session_id_);
  if (out_buffer == nullptr) {
    LOGE("CryptoSession::GenericEncrypt: out_buffer not provided");
    return PARAMETER_NULL;
  }

  OEMCrypto_Algorithm oec_algorithm = GenericEncryptionAlgorithm(algorithm);
  if (iv.size() != GenericEncryptionBlockSize(algorithm) ||
      oec_algorithm == kInvalidAlgorithm) {
    return INVALID_PARAMETERS_ENG_13;
  }

  if (out_buffer->size() < in_buffer.size()) {
    out_buffer->resize(in_buffer.size());
  }

  // TODO(jfore): We need to select a key with a cipher mode and algorithm
  // doesn't seem to fit. Is it ok to just use a default value here?
  // Or do we need to pass it in?
  CdmResponseType result = SelectKey(key_id, kCipherModeCbc);
  if (result != NO_ERROR) return result;

  OEMCryptoResult sts;

  WithOecSessionLock("GenericEncrypt", [&] {
    M_TIME(
        sts = OEMCrypto_Generic_Encrypt(
            oec_session_id_, reinterpret_cast<const uint8_t*>(in_buffer.data()),
            in_buffer.size(), reinterpret_cast<const uint8_t*>(iv.data()),
            oec_algorithm,
            reinterpret_cast<uint8_t*>(const_cast<char*>(out_buffer->data()))),
        metrics_, oemcrypto_generic_encrypt_, sts,
        metrics::Pow2Bucket(in_buffer.size()));
  });

  if (OEMCrypto_SUCCESS != sts) {
    LOGE("GenericEncrypt: OEMCrypto_Generic_Encrypt err=%d", sts);
  }

  switch (sts) {
    case OEMCrypto_SUCCESS:
      return NO_ERROR;
    case OEMCrypto_ERROR_KEY_EXPIRED:
      return NEED_KEY;
    case OEMCrypto_ERROR_NO_CONTENT_KEY:
    case OEMCrypto_KEY_NOT_LOADED:  // obsolete in v15.
      return KEY_NOT_FOUND_3;
    case OEMCrypto_ERROR_OUTPUT_TOO_LARGE:
      return OUTPUT_TOO_LARGE_ERROR;
    case OEMCrypto_ERROR_SESSION_LOST_STATE:
      return SESSION_LOST_STATE_ERROR;
    case OEMCrypto_ERROR_SYSTEM_INVALIDATED:
      return SYSTEM_INVALIDATED_ERROR;
    default:
      return UNKNOWN_ERROR;
  }
}

CdmResponseType CryptoSession::GenericDecrypt(const std::string& in_buffer,
                                              const std::string& key_id,
                                              const std::string& iv,
                                              CdmEncryptionAlgorithm algorithm,
                                              std::string* out_buffer) {
  LOGV("GenericDecrypt: id=%lu", oec_session_id_);
  if (out_buffer == nullptr) {
    LOGE("CryptoSession::GenericDecrypt: out_buffer not provided");
    return PARAMETER_NULL;
  }

  OEMCrypto_Algorithm oec_algorithm = GenericEncryptionAlgorithm(algorithm);
  if (iv.size() != GenericEncryptionBlockSize(algorithm) ||
      oec_algorithm == kInvalidAlgorithm) {
    return INVALID_PARAMETERS_ENG_14;
  }

  if (out_buffer->size() < in_buffer.size()) {
    out_buffer->resize(in_buffer.size());
  }

  // TODO(jfore): We need to select a key with a cipher mode and algorithm
  // doesn't seem to fit. Is it ok to just use a default value here?
  // Or do we need to pass it in?
  CdmResponseType result = SelectKey(key_id, kCipherModeCbc);
  if (result != NO_ERROR) return result;

  OEMCryptoResult sts;

  WithOecSessionLock("GenericDecrypt", [&] {
    M_TIME(
        sts = OEMCrypto_Generic_Decrypt(
            oec_session_id_, reinterpret_cast<const uint8_t*>(in_buffer.data()),
            in_buffer.size(), reinterpret_cast<const uint8_t*>(iv.data()),
            oec_algorithm,
            reinterpret_cast<uint8_t*>(const_cast<char*>(out_buffer->data()))),
        metrics_, oemcrypto_generic_decrypt_, sts,
        metrics::Pow2Bucket(in_buffer.size()));
  });

  if (OEMCrypto_SUCCESS != sts) {
    LOGE("GenericDecrypt: OEMCrypto_Generic_Decrypt err=%d", sts);
  }

  switch (sts) {
    case OEMCrypto_SUCCESS:
      return NO_ERROR;
    case OEMCrypto_ERROR_KEY_EXPIRED:
      return NEED_KEY;
    case OEMCrypto_ERROR_NO_CONTENT_KEY:
    case OEMCrypto_KEY_NOT_LOADED:  // obsolete in v15.
      return KEY_NOT_FOUND_4;
    case OEMCrypto_ERROR_OUTPUT_TOO_LARGE:
      return OUTPUT_TOO_LARGE_ERROR;
    case OEMCrypto_ERROR_SESSION_LOST_STATE:
      return SESSION_LOST_STATE_ERROR;
    case OEMCrypto_ERROR_SYSTEM_INVALIDATED:
      return SYSTEM_INVALIDATED_ERROR;
    default:
      return UNKNOWN_ERROR;
  }
}

CdmResponseType CryptoSession::GenericSign(const std::string& message,
                                           const std::string& key_id,
                                           CdmSigningAlgorithm algorithm,
                                           std::string* signature) {
  LOGV("GenericSign: id=%lu", oec_session_id_);
  if (signature == nullptr) {
    LOGE("CryptoSession::GenericSign: signature not provided");
    return PARAMETER_NULL;
  }

  OEMCrypto_Algorithm oec_algorithm = GenericSigningAlgorithm(algorithm);
  if (oec_algorithm == kInvalidAlgorithm) {
    return INVALID_PARAMETERS_ENG_15;
  }

  OEMCryptoResult sts;
  size_t length = signature->size();

  // TODO(jfore): We need to select a key with a cipher mode and algorithm
  // doesn't seem to fit. Is it ok to just use a default value here?
  // Or do we need to pass it in?
  CdmResponseType result = SelectKey(key_id, kCipherModeCbc);
  if (result != NO_ERROR) return result;

  // At most two attempts.
  // The first attempt may fail due to buffer too short
  for (int i = 0; i < 2; ++i) {
    WithOecSessionLock("GenericSign", [&] {
      M_TIME(
          sts = OEMCrypto_Generic_Sign(
              oec_session_id_, reinterpret_cast<const uint8_t*>(message.data()),
              message.size(), oec_algorithm,
              reinterpret_cast<uint8_t*>(const_cast<char*>(signature->data())),
              &length),
          metrics_, oemcrypto_generic_sign_, sts,
          metrics::Pow2Bucket(message.size()));
    });

    if (OEMCrypto_SUCCESS == sts) {
      // Trim signature buffer and done
      signature->resize(length);
      return NO_ERROR;
    }
    if (OEMCrypto_ERROR_SHORT_BUFFER != sts) {
      break;
    }

    // Retry with proper-sized return buffer
    signature->resize(length);
  }

  LOGE("GenericSign: OEMCrypto_Generic_Sign err=%d", sts);

  switch (sts) {
    case OEMCrypto_ERROR_KEY_EXPIRED:
      return NEED_KEY;
    case OEMCrypto_ERROR_NO_CONTENT_KEY:
    case OEMCrypto_KEY_NOT_LOADED:  // obsolete in v15.
      return KEY_NOT_FOUND_5;
    case OEMCrypto_ERROR_OUTPUT_TOO_LARGE:
      return OUTPUT_TOO_LARGE_ERROR;
    case OEMCrypto_ERROR_SESSION_LOST_STATE:
      return SESSION_LOST_STATE_ERROR;
    case OEMCrypto_ERROR_SYSTEM_INVALIDATED:
      return SYSTEM_INVALIDATED_ERROR;
    default:
      return UNKNOWN_ERROR;
  }
}

CdmResponseType CryptoSession::GenericVerify(const std::string& message,
                                             const std::string& key_id,
                                             CdmSigningAlgorithm algorithm,
                                             const std::string& signature) {
  LOGV("GenericVerify: id=%lu", oec_session_id_);

  OEMCrypto_Algorithm oec_algorithm = GenericSigningAlgorithm(algorithm);
  if (oec_algorithm == kInvalidAlgorithm) {
    return INVALID_PARAMETERS_ENG_16;
  }

  // TODO(jfore): We need to select a key with a cipher mode and algorithm
  // doesn't seem to fit. Is it ok to just use a default value here?
  // Or do we need to pass it in?
  CdmResponseType result = SelectKey(key_id, kCipherModeCbc);
  if (result != NO_ERROR) return result;

  OEMCryptoResult sts;
  WithOecSessionLock("GenericVerify", [&] {
    M_TIME(
        sts = OEMCrypto_Generic_Verify(
            oec_session_id_, reinterpret_cast<const uint8_t*>(message.data()),
            message.size(), oec_algorithm,
            reinterpret_cast<const uint8_t*>(signature.data()),
            signature.size()),
        metrics_, oemcrypto_generic_verify_, sts,
        metrics::Pow2Bucket(signature.size()));
  });

  if (OEMCrypto_SUCCESS != sts) {
    LOGE("GenericVerify: OEMCrypto_Generic_Verify err=%d", sts);
  }

  switch (sts) {
    case OEMCrypto_SUCCESS:
      return NO_ERROR;
    case OEMCrypto_ERROR_KEY_EXPIRED:
      return NEED_KEY;
    case OEMCrypto_ERROR_NO_CONTENT_KEY:
    case OEMCrypto_KEY_NOT_LOADED:  // obsolete in v15.
      return KEY_NOT_FOUND_6;
    case OEMCrypto_ERROR_OUTPUT_TOO_LARGE:
      return OUTPUT_TOO_LARGE_ERROR;
    case OEMCrypto_ERROR_SESSION_LOST_STATE:
      return SESSION_LOST_STATE_ERROR;
    case OEMCrypto_ERROR_SYSTEM_INVALIDATED:
      return SYSTEM_INVALIDATED_ERROR;
    default:
      return UNKNOWN_ERROR;
  }
}

CdmResponseType CryptoSession::GetUsageSupportType(
    CdmUsageSupportType* usage_support_type) {
  LOGV("GetUsageSupportType: id=%lu", oec_session_id_);

  if (usage_support_type == NULL) {
    LOGE("GetUsageSupportType: usage_support_type param not provided");
    return INVALID_PARAMETERS_ENG_23;
  }

  if (usage_support_type_ != kUnknownUsageSupport) {
    *usage_support_type = usage_support_type_;
    return NO_ERROR;
  }

  bool has_support = false;
  if (!UsageInformationSupport(&has_support)) {
    LOGE("GetUsageSupportType: UsageInformationSupport failed");
    return USAGE_INFORMATION_SUPPORT_FAILED;
  }

  if (!has_support) {
    *usage_support_type = usage_support_type_ = kNonSecureUsageSupport;
    return NO_ERROR;
  }

  *usage_support_type = usage_support_type_ =
      (api_version_ >= kOemCryptoApiVersionSupportsBigUsageTables)
          ? kUsageEntrySupport
          : kUsageTableSupport;
  return NO_ERROR;
}

CdmResponseType CryptoSession::CreateUsageTableHeader(
    CdmUsageTableHeader* usage_table_header) {
  LOGV("CreateUsageTableHeader: id=%lu", oec_session_id_);

  if (usage_table_header == NULL) {
    LOGE("CreateUsageTableHeader: usage_table_header param not provided");
    return INVALID_PARAMETERS_ENG_17;
  }

  usage_table_header->resize(kEstimatedInitialUsageTableHeader);

  size_t usage_table_header_size = usage_table_header->size();
  OEMCryptoResult result;
  WithOecWriteLock("CreateUsageTableHeader Attempt 1", [&] {
    result = OEMCrypto_CreateUsageTableHeader(
        requested_security_level_,
        reinterpret_cast<uint8_t*>(
            const_cast<char*>(usage_table_header->data())),
        &usage_table_header_size);
    metrics_->oemcrypto_create_usage_table_header_.Increment(result);
  });

  if (result == OEMCrypto_ERROR_SHORT_BUFFER) {
    usage_table_header->resize(usage_table_header_size);
    WithOecWriteLock("CreateUsageTableHeader Attempt 2", [&] {
      result = OEMCrypto_CreateUsageTableHeader(
          requested_security_level_,
          reinterpret_cast<uint8_t*>(
              const_cast<char*>(usage_table_header->data())),
          &usage_table_header_size);
      metrics_->oemcrypto_create_usage_table_header_.Increment(result);
    });
  }

  if (result == OEMCrypto_SUCCESS) {
    usage_table_header->resize(usage_table_header_size);
  }

  return MapOEMCryptoResult(result, CREATE_USAGE_TABLE_ERROR,
                            "CreateUsageTableHeader");
}

CdmResponseType CryptoSession::LoadUsageTableHeader(
    const CdmUsageTableHeader& usage_table_header) {
  LOGV("LoadUsageTableHeader: id=%lu", oec_session_id_);

  OEMCryptoResult result;
  WithOecWriteLock("LoadUsageTableHeader", [&] {
    result = OEMCrypto_LoadUsageTableHeader(
        requested_security_level_,
        reinterpret_cast<const uint8_t*>(usage_table_header.data()),
        usage_table_header.size());
    metrics_->oemcrypto_load_usage_table_header_.Increment(result);
  });

  if (result != OEMCrypto_SUCCESS) {
    if (result == OEMCrypto_WARNING_GENERATION_SKEW) {
      LOGW(
          "LoadUsageTableHeader: OEMCrypto_LoadUsageTableHeader warning: "
          "generation skew");
    } else {
      LOGE("LoadUsageTableHeader: OEMCrypto_LoadUsageTableHeader error: %d",
           result);
    }
  }

  switch (result) {
    case OEMCrypto_SUCCESS:
    case OEMCrypto_WARNING_GENERATION_SKEW:
      return NO_ERROR;
    case OEMCrypto_ERROR_GENERATION_SKEW:
      return LOAD_USAGE_HEADER_GENERATION_SKEW;
    case OEMCrypto_ERROR_SIGNATURE_FAILURE:
      return LOAD_USAGE_HEADER_SIGNATURE_FAILURE;
    case OEMCrypto_ERROR_BAD_MAGIC:
      return LOAD_USAGE_HEADER_BAD_MAGIC;
    case OEMCrypto_ERROR_SYSTEM_INVALIDATED:
      return SYSTEM_INVALIDATED_ERROR;
    case OEMCrypto_ERROR_UNKNOWN_FAILURE:
    default:
      return LOAD_USAGE_HEADER_UNKNOWN_ERROR;
  }
}

CdmResponseType CryptoSession::CreateUsageEntry(uint32_t* entry_number) {
  LOGV("CreateUsageEntry: id=%lu", oec_session_id_);

  if (entry_number == NULL) {
    LOGE("CreateUsageEntry: entry_number param not provided");
    return INVALID_PARAMETERS_ENG_18;
  }

  OEMCryptoResult result;
  WithOecWriteLock("CreateUsageEntry", [&] {
    result = OEMCrypto_CreateNewUsageEntry(oec_session_id_, entry_number);
    metrics_->oemcrypto_create_new_usage_entry_.Increment(result);
  });

  if (result != OEMCrypto_SUCCESS) {
    LOGE("CreateUsageEntry: OEMCrypto_CreateNewUsageEntry error: %d", result);
  }

  switch (result) {
    case OEMCrypto_SUCCESS:
      return NO_ERROR;
    case OEMCrypto_ERROR_INSUFFICIENT_RESOURCES:
      return INSUFFICIENT_CRYPTO_RESOURCES_3;
    case OEMCrypto_ERROR_SESSION_LOST_STATE:
      return SESSION_LOST_STATE_ERROR;
    case OEMCrypto_ERROR_SYSTEM_INVALIDATED:
      return SYSTEM_INVALIDATED_ERROR;
    default:
      return CREATE_USAGE_ENTRY_UNKNOWN_ERROR;
  }
}

CdmResponseType CryptoSession::LoadUsageEntry(
    uint32_t entry_number, const CdmUsageEntry& usage_entry) {
  LOGV("LoadUsageEntry: id=%lu", oec_session_id_);

  OEMCryptoResult result;
  WithOecWriteLock("LoadUsageEntry", [&] {
    result = OEMCrypto_LoadUsageEntry(
        oec_session_id_, entry_number,
        reinterpret_cast<const uint8_t*>(usage_entry.data()),
        usage_entry.size());
    metrics_->oemcrypto_load_usage_entry_.Increment(result);
  });

  if (result != OEMCrypto_SUCCESS) {
    if (result == OEMCrypto_WARNING_GENERATION_SKEW) {
      LOGW("LoadUsageEntry: OEMCrypto_LoadUsageEntry warning: generation skew");
    } else {
      LOGE("LoadUsageEntry: OEMCrypto_LoadUsageEntry error: %d", result);
    }
  }

  switch (result) {
    case OEMCrypto_SUCCESS:
    case OEMCrypto_WARNING_GENERATION_SKEW:
      return NO_ERROR;
    case OEMCrypto_ERROR_GENERATION_SKEW:
      return LOAD_USAGE_ENTRY_GENERATION_SKEW;
    case OEMCrypto_ERROR_SIGNATURE_FAILURE:
      return LOAD_USAGE_ENTRY_SIGNATURE_FAILURE;
    case OEMCrypto_ERROR_INSUFFICIENT_RESOURCES:
      return INSUFFICIENT_CRYPTO_RESOURCES_3;
    case OEMCrypto_ERROR_SESSION_LOST_STATE:
      return SESSION_LOST_STATE_ERROR;
    case OEMCrypto_ERROR_SYSTEM_INVALIDATED:
      return SYSTEM_INVALIDATED_ERROR;
    default:
      return LOAD_USAGE_ENTRY_UNKNOWN_ERROR;
  }
}

CdmResponseType CryptoSession::UpdateUsageEntry(
    CdmUsageTableHeader* usage_table_header, CdmUsageEntry* usage_entry) {
  LOGV("UpdateUsageEntry: id=%lu", oec_session_id_);

  if (usage_table_header == NULL) {
    LOGE("UpdateUsageEntry: usage_table_header param not provided");
    return INVALID_PARAMETERS_ENG_19;
  }

  if (usage_entry == NULL) {
    LOGE("UpdateUsageEntry: usage_entry param not provided");
    return INVALID_PARAMETERS_ENG_20;
  }

  size_t usage_table_header_len = 0;
  size_t usage_entry_len = 0;
  OEMCryptoResult result;
  WithOecWriteLock("UpdateUsageEntry Attempt 1", [&] {
    result = OEMCrypto_UpdateUsageEntry(
        oec_session_id_, NULL, &usage_table_header_len, NULL, &usage_entry_len);
  });
  metrics_->oemcrypto_update_usage_entry_.Increment(result);

  if (result == OEMCrypto_ERROR_SHORT_BUFFER) {
    usage_table_header->resize(usage_table_header_len);
    usage_entry->resize(usage_entry_len);

    WithOecWriteLock("UpdateUsageEntry Attempt 2", [&] {
      result = OEMCrypto_UpdateUsageEntry(
          oec_session_id_,
          reinterpret_cast<uint8_t*>(
              const_cast<char*>(usage_table_header->data())),
          &usage_table_header_len,
          reinterpret_cast<uint8_t*>(const_cast<char*>(usage_entry->data())),
          &usage_entry_len);
    });
    metrics_->oemcrypto_update_usage_entry_.Increment(result);
  }

  if (result == OEMCrypto_SUCCESS) {
    usage_table_header->resize(usage_table_header_len);
    usage_entry->resize(usage_entry_len);
  }

  return MapOEMCryptoResult(result, UPDATE_USAGE_ENTRY_UNKNOWN_ERROR,
                            "UpdateUsageEntry");
}

CdmResponseType CryptoSession::ShrinkUsageTableHeader(
    uint32_t new_entry_count, CdmUsageTableHeader* usage_table_header) {
  LOGV("ShrinkUsageTableHeader: id=%lu", oec_session_id_);

  if (usage_table_header == NULL) {
    LOGE("ShrinkUsageTableHeader: usage_table_header param not provided");
    return INVALID_PARAMETERS_ENG_21;
  }

  size_t usage_table_header_len = 0;
  OEMCryptoResult result;
  WithOecWriteLock("ShrinkUsageTableHeader Attempt 1", [&] {
    result = OEMCrypto_ShrinkUsageTableHeader(
        requested_security_level_, new_entry_count, NULL,
        &usage_table_header_len);
    metrics_->oemcrypto_shrink_usage_table_header_.Increment(result);
  });

  if (result == OEMCrypto_ERROR_SHORT_BUFFER) {
    usage_table_header->resize(usage_table_header_len);

    WithOecWriteLock("ShrinkUsageTableHeader Attempt 2", [&] {
      result = OEMCrypto_ShrinkUsageTableHeader(
          requested_security_level_, new_entry_count,
          reinterpret_cast<uint8_t*>(
              const_cast<char*>(usage_table_header->data())),
          &usage_table_header_len);
      metrics_->oemcrypto_shrink_usage_table_header_.Increment(result);
    });
  }

  if (result == OEMCrypto_SUCCESS) {
    usage_table_header->resize(usage_table_header_len);
  }

  return MapOEMCryptoResult(result, SHRINK_USAGE_TABLER_HEADER_UNKNOWN_ERROR,
                            "ShrinkUsageTableHeader");
}

CdmResponseType CryptoSession::MoveUsageEntry(uint32_t new_entry_number) {
  LOGV("MoveUsageEntry: id=%lu", oec_session_id_);

  OEMCryptoResult result;
  WithOecWriteLock("MoveUsageEntry", [&] {
    result = OEMCrypto_MoveEntry(oec_session_id_, new_entry_number);
    metrics_->oemcrypto_move_entry_.Increment(result);
  });

  return MapOEMCryptoResult(
      result, MOVE_USAGE_ENTRY_UNKNOWN_ERROR, "MoveUsageEntry");
}

bool CryptoSession::CreateOldUsageEntry(
    uint64_t time_since_license_received, uint64_t time_since_first_decrypt,
    uint64_t time_since_last_decrypt, UsageDurationStatus usage_duration_status,
    const std::string& server_mac_key, const std::string& client_mac_key,
    const std::string& provider_session_token) {
  LOGV("CreateOldUsageEntry");

  if (server_mac_key.size() < MAC_KEY_SIZE ||
      client_mac_key.size() < MAC_KEY_SIZE) {
    LOGE(
        "CreateOldUsageEntry: Invalid mac key size: server mac key size %d, "
        "client mac key size: %d",
        server_mac_key.size(), client_mac_key.size());
    return false;
  }

  OEMCrypto_Usage_Entry_Status status = kUnused;
  switch (usage_duration_status) {
    case kUsageDurationsInvalid:
      status = kUnused;
      break;
    case kUsageDurationPlaybackNotBegun:
      status = kInactiveUnused;
      break;
    case kUsageDurationsValid:
      status = kActive;
      break;
    default:
      LOGE("CreateOldUsageEntry: Unrecognized usage duration status: %d",
           usage_duration_status);
      return false;
  }

  OEMCryptoResult result;
  WithOecWriteLock("CreateOldUsageEntry", [&] {
    result = OEMCrypto_CreateOldUsageEntry(
        requested_security_level_, time_since_license_received,
        time_since_first_decrypt, time_since_last_decrypt, status,
        reinterpret_cast<uint8_t*>(const_cast<char*>(server_mac_key.data())),
        reinterpret_cast<uint8_t*>(const_cast<char*>(client_mac_key.data())),
        reinterpret_cast<const uint8_t*>(provider_session_token.data()),
        provider_session_token.size());
    metrics_->oemcrypto_create_old_usage_entry_.Increment(result);
  });

  if (result != OEMCrypto_SUCCESS) {
    LOGE("CreateOldUsageEntry: OEMCrypto_CreateOldUsageEntry error: %d",
         result);
    return false;
  }
  return true;
}

CdmResponseType CryptoSession::CopyOldUsageEntry(
    const std::string& provider_session_token) {
  LOGV("CopyOldUsageEntry: id=%lu", oec_session_id_);

  OEMCryptoResult result;
  WithOecWriteLock("CopyOldUsageEntry", [&] {
    result = OEMCrypto_CopyOldUsageEntry(
        oec_session_id_,
        reinterpret_cast<const uint8_t*>(provider_session_token.data()),
        provider_session_token.size());
    metrics_->oemcrypto_copy_old_usage_entry_.Increment(result);
  });

  return MapOEMCryptoResult(
      result, COPY_OLD_USAGE_ENTRY_UNKNOWN_ERROR, "CopyOldUsageEntry");
}

bool CryptoSession::GetAnalogOutputCapabilities(bool* can_support_output,
                                                bool* can_disable_output,
                                                bool* can_support_cgms_a) {
  LOGV("GetAnalogOutputCapabilities: id=%lu", oec_session_id_);
  uint32_t flags;
  WithOecReadLock("GetAnalogOutputCapabilities", [&] {
    flags = OEMCrypto_GetAnalogOutputFlags(requested_security_level_);
  });

  if ((flags & OEMCrypto_Unknown_Analog_Output) != 0) return false;
  *can_support_cgms_a = flags & OEMCrypto_Supports_CGMS_A;
  *can_support_output = flags & OEMCrypto_Supports_Analog_Output;
  *can_disable_output = flags & OEMCrypto_Can_Disable_Analog_Ouptput;
  return true;
}

OEMCrypto_Algorithm CryptoSession::GenericSigningAlgorithm(
    CdmSigningAlgorithm algorithm) {
  if (kSigningAlgorithmHmacSha256 == algorithm) {
    return OEMCrypto_HMAC_SHA256;
  } else {
    return kInvalidAlgorithm;
  }
}

OEMCrypto_Algorithm CryptoSession::GenericEncryptionAlgorithm(
    CdmEncryptionAlgorithm algorithm) {
  if (kEncryptionAlgorithmAesCbc128 == algorithm) {
    return OEMCrypto_AES_CBC_128_NO_PADDING;
  } else {
    return kInvalidAlgorithm;
  }
}

size_t CryptoSession::GenericEncryptionBlockSize(
    CdmEncryptionAlgorithm algorithm) {
  if (kEncryptionAlgorithmAesCbc128 == algorithm) {
    return kAes128BlockSize;
  } else {
    return 0;
  }
}

OEMCryptoResult CryptoSession::CopyBufferInChunks(
    const CdmDecryptionParameters& params,
    OEMCrypto_DestBufferDesc full_buffer_descriptor) {
  size_t remaining_encrypt_length = params.encrypt_length;
  uint8_t subsample_flags = OEMCrypto_FirstSubsample;

  while (remaining_encrypt_length > 0) {
    // Calculate the size of the next chunk and its offset into the original
    // buffer.
    const size_t chunk_size =
        std::min(remaining_encrypt_length, kMaximumChunkSize);
    const size_t additional_offset =
        params.encrypt_length - remaining_encrypt_length;

    // Update the remaining length of the original buffer only after
    // calculating the new values.
    remaining_encrypt_length -= chunk_size;

    // Update the destination buffer with the new offset. Because OEMCrypto
    // can modify the OEMCrypto_DestBufferDesc during the call to
    // OEMCrypto_CopyBuffer, (and is known to do so on some platforms) a new
    // OEMCrypto_DestBufferDesc must be allocated for each call.
    OEMCrypto_DestBufferDesc buffer_descriptor = full_buffer_descriptor;
    switch (buffer_descriptor.type) {
      case OEMCrypto_BufferType_Clear:
        buffer_descriptor.buffer.clear.address += additional_offset;
        buffer_descriptor.buffer.clear.max_length -= additional_offset;
        break;
      case OEMCrypto_BufferType_Secure:
        buffer_descriptor.buffer.secure.offset += additional_offset;
        break;
      case OEMCrypto_BufferType_Direct:
        // OEMCrypto_BufferType_Direct does not need modification.
        break;
    }

    // Re-add "last subsample" flag if this is the last subsample.
    if (remaining_encrypt_length == 0) {
      subsample_flags |= OEMCrypto_LastSubsample;
    }

    OEMCryptoResult sts;
    WithOecSessionLock("CopyBufferInChunks", [&] {
      M_TIME(sts = OEMCrypto_CopyBuffer(
                 oec_session_id_, params.encrypt_buffer + additional_offset,
                 chunk_size, &buffer_descriptor, subsample_flags),
             metrics_, oemcrypto_copy_buffer_, sts,
             metrics::Pow2Bucket(chunk_size));
    });

    if (sts != OEMCrypto_SUCCESS) {
      return sts;
    }

    // Clear any subsample flags before the next loop iteration.
    subsample_flags = 0;
  }

  return OEMCrypto_SUCCESS;
}

OEMCryptoResult CryptoSession::DecryptInChunks(
    const CdmDecryptionParameters& params,
    const OEMCrypto_DestBufferDesc& full_buffer_descriptor,
    const OEMCrypto_CENCEncryptPatternDesc& pattern_descriptor,
    size_t max_chunk_size) {
  size_t remaining_encrypt_length = params.encrypt_length;
  uint8_t subsample_flags = (params.subsample_flags & OEMCrypto_FirstSubsample)
                                ? OEMCrypto_FirstSubsample
                                : 0;
  std::vector<uint8_t> iv = *params.iv;

  const size_t pattern_length_in_bytes =
      (pattern_descriptor.encrypt + pattern_descriptor.skip) * kAes128BlockSize;

  while (remaining_encrypt_length > 0) {
    // Calculate the size of the next chunk and its offset into the
    // original buffer.
    const size_t chunk_size =
        std::min(remaining_encrypt_length, max_chunk_size);
    const size_t additional_offset =
        params.encrypt_length - remaining_encrypt_length;

    // Update the remaining length of the original buffer only after
    // calculating the new values.
    remaining_encrypt_length -= chunk_size;

    // Update the destination buffer with the new offset. Because OEMCrypto
    // can modify the OEMCrypto_DestBufferDesc during the call to
    // OEMCrypto_DecryptCENC, (and is known to do so on some platforms) a new
    // OEMCrypto_DestBufferDesc must be allocated for each call.
    OEMCrypto_DestBufferDesc buffer_descriptor = full_buffer_descriptor;
    switch (buffer_descriptor.type) {
      case OEMCrypto_BufferType_Clear:
        buffer_descriptor.buffer.clear.address += additional_offset;
        buffer_descriptor.buffer.clear.max_length -= additional_offset;
        break;
      case OEMCrypto_BufferType_Secure:
        buffer_descriptor.buffer.secure.offset += additional_offset;
        break;
      case OEMCrypto_BufferType_Direct:
        // OEMCrypto_BufferType_Direct does not need modification.
        break;
    }

    // Re-add "last subsample" flag if this is the last subsample.
    if (remaining_encrypt_length == 0 &&
        params.subsample_flags & OEMCrypto_LastSubsample) {
      subsample_flags |= OEMCrypto_LastSubsample;
    }

    // block_offset and pattern_descriptor do not need to change because
    // max_chunk_size is guaranteed to be an even multiple of the
    // pattern length long, which is also guaranteed to be an exact number
    // of AES blocks long.
    OEMCryptoResult sts;
    WithOecSessionLock("DecryptInChunks", [&] {
      M_TIME(
          sts = OEMCrypto_DecryptCENC(
              oec_session_id_, params.encrypt_buffer + additional_offset,
              chunk_size, params.is_encrypted, &iv.front(), params.block_offset,
              &buffer_descriptor, &pattern_descriptor, subsample_flags),
          metrics_, oemcrypto_decrypt_cenc_, sts,
          metrics::Pow2Bucket(chunk_size));
    });

    if (sts != OEMCrypto_SUCCESS) {
      return sts;
    }

    // If we are not yet done, update the IV so that it is valid for the next
    // iteration.
    if (remaining_encrypt_length != 0) {
      if (cipher_mode_ == kCipherModeCtr) {
        // For CTR modes, update the IV depending on how many encrypted blocks
        // we passed. Since we calculated the chunk size to be an even number
        // of crypto blocks and pattern repetitions in size, we can do a
        // simplified calculation for this.
        uint64_t encrypted_blocks_passed = 0;
        if (pattern_length_in_bytes == 0) {
          encrypted_blocks_passed = chunk_size / kAes128BlockSize;
        } else {
          const size_t pattern_repetitions_passed =
              chunk_size / pattern_length_in_bytes;
          encrypted_blocks_passed =
              pattern_repetitions_passed * pattern_descriptor.encrypt;
        }
        IncrementIV(encrypted_blocks_passed, &iv);
      } else if (cipher_mode_ == kCipherModeCbc) {
        // For CBC modes, use the previous ciphertext block.

        // Stash the last crypto block in the IV. We don't have to handle
        // partial crypto blocks here because we know we broke the buffer into
        // chunks along even crypto block boundaries.
        const uint8_t* const buffer_end =
            params.encrypt_buffer + additional_offset + chunk_size;

        const uint8_t* block_end = NULL;
        if (pattern_length_in_bytes == 0) {
          // For cbc1, the last encrypted block is the last block of the
          // subsample.
          block_end = buffer_end;
        } else {
          // For cbcs, we must look for the last encrypted block, which is
          // probably not the last block of the subsample. Luckily, since the
          // buffer size is guaranteed to be an even number of pattern
          // repetitions long, we can use the pattern to know how many blocks
          // to look back.
          block_end = buffer_end - kAes128BlockSize * pattern_descriptor.skip;
        }

        iv.assign(block_end - kAes128BlockSize, block_end);
      }
    }

    // Clear any subsample flags before the next loop iteration.
    subsample_flags = 0;
  }

  return OEMCrypto_SUCCESS;
}

void CryptoSession::IncrementIV(uint64_t increase_by,
                                std::vector<uint8_t>* iv_out) {
  std::vector<uint8_t>& iv = *iv_out;
  uint64_t* counter_buffer = reinterpret_cast<uint64_t*>(&iv[8]);
  (*counter_buffer) = htonll64(ntohll64(*counter_buffer) + increase_by);
}

template <class Func>
auto CryptoSession::WithStaticFieldWriteLock(const char* tag, Func body)
    -> decltype(body()) {
  LOGV("Static Field Write Lock - %s", tag);
  std::unique_lock<shared_mutex> auto_lock(static_field_mutex_);
  return body();
}

template <class Func>
auto CryptoSession::WithStaticFieldReadLock(const char* tag, Func body)
    -> decltype(body()) {
  LOGV("Static Field Read Lock - %s", tag);
  shared_lock<shared_mutex> auto_lock(static_field_mutex_);
  return body();
}

template <class Func>
auto CryptoSession::WithOecWriteLock(const char* tag, Func body)
    -> decltype(body()) {
  LOGV("OEMCrypto Write Lock - %s", tag);
  std::unique_lock<shared_mutex> auto_lock(oem_crypto_mutex_);
  return body();
}

template <class Func>
auto CryptoSession::WithOecReadLock(const char* tag, Func body)
    -> decltype(body()) {
  LOGV("OEMCrypto Read Lock - %s", tag);
  shared_lock<shared_mutex> auto_lock(oem_crypto_mutex_);
  return body();
}

template <class Func>
auto CryptoSession::WithOecSessionLock(const char* tag, Func body)
    -> decltype(body()) {
  LOGV("OEMCrypto Session Lock - %s", tag);
  shared_lock<shared_mutex> oec_auto_lock(oem_crypto_mutex_);
  std::unique_lock<std::mutex> session_auto_lock(oem_crypto_session_mutex_);
  return body();
}

bool CryptoSession::IsInitialized() {
  return WithStaticFieldReadLock("IsInitialized", [] { return initialized_; });
}

// CryptoSesssionFactory support
std::mutex CryptoSession::factory_mutex_;
// The factory will either be set by WvCdmTestBase, or a default factory is
// created on the first call to MakeCryptoSession.
std::unique_ptr<CryptoSessionFactory> CryptoSession::factory_ =
    std::unique_ptr<CryptoSessionFactory>();

CryptoSession* CryptoSession::MakeCryptoSession(
    metrics::CryptoMetrics* crypto_metrics) {
  std::unique_lock<std::mutex> auto_lock(factory_mutex_);
  // If the factory_ has not been set, then use a default factory.
  if (factory_.get() == NULL) factory_.reset(new CryptoSessionFactory());
  return factory_->MakeCryptoSession(crypto_metrics);
}

CryptoSession* CryptoSessionFactory::MakeCryptoSession(
    metrics::CryptoMetrics* crypto_metrics) {
  return new CryptoSession(crypto_metrics);
}

}  // namespace wvcdm
