// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#include "cdm_engine.h"

#include <assert.h>
#include <stdlib.h>

#include <iostream>
#include <list>
#include <memory>
#include <sstream>

#include "cdm_session.h"
#include "cdm_session_map.h"
#include "clock.h"
#include "device_files.h"
#include "file_store.h"
#include "log.h"
#include "properties.h"
#include "string_conversions.h"
#include "wv_cdm_constants.h"
#include "wv_cdm_event_listener.h"

namespace {
const uint64_t kReleaseSessionTimeToLive = 60;  // seconds
const uint32_t kUpdateUsageInformationPeriod = 60;  // seconds
const size_t kUsageReportsPerRequest = 1;

wvcdm::CdmOfflineLicenseState MapDeviceFilesLicenseState(
    wvcdm::DeviceFiles::LicenseState state) {
  switch (state) {
    case wvcdm::DeviceFiles::LicenseState::kLicenseStateActive:
      return wvcdm::kLicenseStateActive;
    case wvcdm::DeviceFiles::LicenseState::kLicenseStateReleasing:
      return wvcdm::kLicenseStateReleasing;
    default:
      return wvcdm::kLicenseStateUnknown;
  }
}
}  // namespace

namespace wvcdm {

class UsagePropertySet : public CdmClientPropertySet {
 public:
  UsagePropertySet() {}
  ~UsagePropertySet() override {}
  void set_security_level(SecurityLevel security_level) {
    if (kLevel3 == security_level)
      security_level_ = QUERY_VALUE_SECURITY_LEVEL_L3;
    else
      security_level_.clear();
  }
  const std::string& security_level() const override { return security_level_; }
  bool use_privacy_mode() const override { return false; }
  const std::string& service_certificate() const override { return empty_; }
  void set_service_certificate(const std::string&) override {}
  bool is_session_sharing_enabled() const override { return false; }
  uint32_t session_sharing_id() const override { return 0; }
  void set_session_sharing_id(uint32_t /* id */) override {}
  const std::string& app_id() const override { return app_id_; }
  void set_app_id(const std::string& appId) { app_id_ = appId; }

 private:
  std::string app_id_;
  std::string security_level_;
  const std::string empty_;
};

bool CdmEngine::seeded_ = false;

CdmEngine::CdmEngine(FileSystem* file_system,
                     std::shared_ptr<metrics::EngineMetrics> metrics,
                     const std::string& spoid)
    : metrics_(metrics),
      cert_provisioning_(),
      cert_provisioning_requested_security_level_(kLevelDefault),
      file_system_(file_system),
      spoid_(spoid),
      usage_session_(),
      usage_property_set_(),
      last_usage_information_update_time_(0) {
  assert(file_system);
  if (!seeded_) {
    Properties::Init();
    srand(clock_.GetCurrentTime());
    seeded_ = true;
  }
}

CdmEngine::~CdmEngine() {
  usage_session_.reset();
  std::unique_lock<std::recursive_mutex> lock(session_map_lock_);
  session_map_.Terminate();
}

CdmResponseType CdmEngine::OpenSession(
    const CdmKeySystem& key_system, CdmClientPropertySet* property_set,
    const CdmSessionId& forced_session_id, WvCdmEventListener* event_listener) {
  return OpenSession(key_system, property_set, event_listener,
                     &forced_session_id, NULL);
}

CdmResponseType CdmEngine::OpenSession(
    const CdmKeySystem& key_system, CdmClientPropertySet* property_set,
    WvCdmEventListener* event_listener, CdmSessionId* session_id) {
  return OpenSession(key_system, property_set, event_listener, NULL, session_id);
}

CdmResponseType CdmEngine::OpenSession(
    const CdmKeySystem& key_system, CdmClientPropertySet* property_set,
    WvCdmEventListener* event_listener, const CdmSessionId* forced_session_id,
    CdmSessionId* session_id) {
  LOGI("CdmEngine::OpenSession");

  if (!ValidateKeySystem(key_system)) {
    LOGI("CdmEngine::OpenSession: invalid key_system = %s", key_system.c_str());
    return INVALID_KEY_SYSTEM;
  }

  if (!session_id && !forced_session_id) {
    LOGE("CdmEngine::OpenSession: no (forced/)session ID destination provided");
    return PARAMETER_NULL;
  }

  if (forced_session_id) {
    if (session_map_.Exists(*forced_session_id)) {
      return DUPLICATE_SESSION_ID_SPECIFIED;
    }
  }

  CloseExpiredReleaseSessions();

  std::unique_ptr<CdmSession> new_session(
      new CdmSession(file_system_, metrics_->AddSession()));
  CdmResponseType sts = new_session->Init(property_set, forced_session_id,
                                          event_listener);
  if (sts != NO_ERROR) {
    if (sts == NEED_PROVISIONING) {
      cert_provisioning_requested_security_level_ =
          new_session->GetRequestedSecurityLevel();
      // Reserve a session ID so the CDM can return success.
      if (session_id)
        *session_id = new_session->GenerateSessionId();
    } else {
      LOGE("CdmEngine::OpenSession: bad session init: %d", sts);
    }
    return sts;
  }
  CdmSessionId id = new_session->session_id();
  LOGI("CdmEngine::OpenSession: %s", id.c_str());

  std::unique_lock<std::recursive_mutex> lock(session_map_lock_);
  session_map_.Add(id, new_session.release());
  if (session_id) *session_id = id;
  return NO_ERROR;
}

CdmResponseType CdmEngine::OpenKeySetSession(
    const CdmKeySetId& key_set_id, CdmClientPropertySet* property_set,
    WvCdmEventListener* event_listener) {
  LOGI("CdmEngine::OpenKeySetSession: %s", key_set_id.c_str());

  if (key_set_id.empty()) {
    LOGE("CdmEngine::OpenKeySetSession: invalid key set id");
    return EMPTY_KEYSET_ID_ENG_1;
  }

  // If in-use, release key set before re-opening, to avoid leaking
  // resources (CryptoSession etc).
  bool key_set_in_use = false;
  {
    std::unique_lock<std::mutex> lock(release_key_sets_lock_);
    key_set_in_use =
        release_key_sets_.find(key_set_id) != release_key_sets_.end();
  }
  if (key_set_in_use)
    CloseKeySetSession(key_set_id);

  CdmSessionId session_id;
  CdmResponseType sts = OpenSession(KEY_SYSTEM, property_set, event_listener,
                                    NULL /* forced_session_id */, &session_id);

  if (sts != NO_ERROR) return sts;

  std::unique_lock<std::mutex> lock(release_key_sets_lock_);
  release_key_sets_[key_set_id] = std::make_pair(session_id,
      clock_.GetCurrentTime() + kReleaseSessionTimeToLive);

  return NO_ERROR;
}

CdmResponseType CdmEngine::CloseSession(const CdmSessionId& session_id) {
  LOGI("CdmEngine::CloseSession: %s", session_id.c_str());
  std::unique_lock<std::recursive_mutex> lock(session_map_lock_);
  if (!session_map_.CloseSession(session_id)) {
    LOGE("CdmEngine::CloseSession: session not found = %s", session_id.c_str());
    return SESSION_NOT_FOUND_1;
  }
  metrics_->ConsolidateSessions();
  return NO_ERROR;
}

CdmResponseType CdmEngine::CloseKeySetSession(const CdmKeySetId& key_set_id) {
  LOGI("CdmEngine::CloseKeySetSession: %s", key_set_id.c_str());

  CdmSessionId session_id;
  {
    std::unique_lock<std::mutex> lock(release_key_sets_lock_);
    CdmReleaseKeySetMap::iterator iter = release_key_sets_.find(key_set_id);
    if (iter == release_key_sets_.end()) {
      LOGE("CdmEngine::CloseKeySetSession: key set id not found = %s",
           key_set_id.c_str());
      return KEYSET_ID_NOT_FOUND_1;
    }
    session_id = iter->second.first;
  }

  CdmResponseType sts = CloseSession(session_id);

  std::unique_lock<std::mutex> lock(release_key_sets_lock_);
  CdmReleaseKeySetMap::iterator iter = release_key_sets_.find(key_set_id);
  if (iter != release_key_sets_.end()) {
    release_key_sets_.erase(iter);
  }
  return sts;
}

bool CdmEngine::IsOpenSession(const CdmSessionId& session_id) {
  std::unique_lock<std::recursive_mutex> lock(session_map_lock_);
  return session_map_.Exists(session_id);
}

CdmResponseType CdmEngine::GenerateKeyRequest(
    const CdmSessionId& session_id, const CdmKeySetId& key_set_id,
    const InitializationData& init_data, const CdmLicenseType license_type,
    CdmAppParameterMap& app_parameters, CdmKeyRequest* key_request) {
  LOGI("CdmEngine::GenerateKeyRequest: %s", session_id.c_str());

  CdmSessionId id = session_id;
  CdmResponseType sts;

  // NOTE: If AlwaysUseKeySetIds() is true, there is no need to consult the
  // release_key_sets_ map for release licenses.
  if (license_type == kLicenseTypeRelease &&
      !Properties::AlwaysUseKeySetIds()) {
    if (key_set_id.empty()) {
      LOGE("CdmEngine::GenerateKeyRequest: invalid key set ID");
      return EMPTY_KEYSET_ID_ENG_2;
    }
    LOGI("CdmEngine::GenerateKeyRequest: key set ID: %s", key_set_id.c_str());

    if (!session_id.empty()) {
      LOGE("CdmEngine::GenerateKeyRequest: invalid session ID = %s",
           session_id.c_str());
      return INVALID_SESSION_ID;
    }

    std::unique_lock<std::mutex> lock(release_key_sets_lock_);
    CdmReleaseKeySetMap::iterator iter = release_key_sets_.find(key_set_id);
    if (iter == release_key_sets_.end()) {
      LOGE("CdmEngine::GenerateKeyRequest: key set ID not found = %s",
           key_set_id.c_str());
      return KEYSET_ID_NOT_FOUND_2;
    }

    id = iter->second.first;
  }

  std::shared_ptr<CdmSession> session;
  if (!session_map_.FindSession(id, &session)) {
    LOGE("CdmEngine::GenerateKeyRequest: session_id not found = %s",
         id.c_str());
    return SESSION_NOT_FOUND_2;
  }

  if (!key_request) {
    LOGE("CdmEngine::GenerateKeyRequest: output destination provided");
    return PARAMETER_NULL;
  }

  key_request->message.clear();

  if (license_type == kLicenseTypeRelease &&
      !session->license_received()) {
    int error_detail = NO_ERROR;
    sts = session->RestoreOfflineSession(key_set_id, kLicenseTypeRelease,
                                         &error_detail);
    session->GetMetrics()->cdm_session_restore_offline_session_.Increment(
        sts, error_detail);
    if (sts != KEY_ADDED) {
      LOGE("CdmEngine::GenerateKeyRequest: key release restoration failed,"
           "sts = %d", static_cast<int>(sts));
      return sts;
    }
  }

  sts = session->GenerateKeyRequest(init_data, license_type, app_parameters,
                                    key_request);

  if (KEY_ADDED == sts) {
    return sts;
  } else if (KEY_MESSAGE != sts) {
    if (sts == NEED_PROVISIONING) {
      cert_provisioning_requested_security_level_ =
          session->GetRequestedSecurityLevel();
    }
    LOGE("CdmEngine::GenerateKeyRequest: key request generation failed, "
         "sts = %d", static_cast<int>(sts));
    return sts;
  }

  if (license_type == kLicenseTypeRelease) {
    OnKeyReleaseEvent(key_set_id);
  }

  return KEY_MESSAGE;
}

CdmResponseType CdmEngine::AddKey(const CdmSessionId& session_id,
                                  const CdmKeyResponse& key_data,
                                  CdmLicenseType* license_type,
                                  CdmKeySetId* key_set_id) {
  LOGI("CdmEngine::AddKey: %s", session_id.c_str());
  if (license_type == nullptr) {
      LOGE("CdmEngine::AddKey: license_type cannot be null.");
      return PARAMETER_NULL;
  }

  CdmSessionId id = session_id;
  bool license_type_release = session_id.empty();

  if (license_type_release) {
    if (!key_set_id) {
      LOGE("CdmEngine::AddKey: no key set id provided");
      return PARAMETER_NULL;
    }

    if (key_set_id->empty()) {
      LOGE("CdmEngine::AddKey: invalid key set id");
      return EMPTY_KEYSET_ID_ENG_3;
    }

    std::unique_lock<std::mutex> lock(release_key_sets_lock_);
    CdmReleaseKeySetMap::iterator iter = release_key_sets_.find(*key_set_id);
    if (iter == release_key_sets_.end()) {
      LOGE("CdmEngine::AddKey: key set id not found = %s", key_set_id->c_str());
      return KEYSET_ID_NOT_FOUND_3;
    }

    id = iter->second.first;
  }

  std::shared_ptr<CdmSession> session;
  if (!session_map_.FindSession(id, &session)) {
    LOGE("CdmEngine::AddKey: session id not found = %s", id.c_str());
    return SESSION_NOT_FOUND_3;
  }

  if (key_data.empty()) {
    LOGE("CdmEngine::AddKey: no key_data");
    return EMPTY_KEY_DATA_1;
  }


  CdmResponseType sts = (session->AddKey(key_data));

  if (sts == KEY_ADDED) {
    if (session->is_release()) {
      *license_type = kLicenseTypeRelease;
    } else if (session->is_temporary()) {
      *license_type = kLicenseTypeTemporary;
    } else if (session->is_offline()) {
      *license_type = kLicenseTypeOffline;
    } else {
      *license_type = kLicenseTypeStreaming;
    }
  }

  if (key_set_id) {
    if ((session->is_offline() ||
         session->has_provider_session_token()) && !license_type_release) {
      *key_set_id = session->key_set_id();
      LOGI("CdmEngine::AddKey: key set ID: %s", key_set_id->c_str());
    } else {
      key_set_id->clear();
    }
  }

  switch (sts) {
    case KEY_ADDED:
      break;
    case NEED_KEY:
      LOGI("CdmEngine::AddKey: service certificate loaded, no key added");
      break;
    default:
      LOGE("CdmEngine::AddKey: keys not added, result = %d", sts);
      break;
  }

  return sts;
}

CdmResponseType CdmEngine::RestoreKey(const CdmSessionId& session_id,
                                      const CdmKeySetId& key_set_id) {
  LOGI("CdmEngine::RestoreKey: session ID: %s, key set ID: %s",
       session_id.c_str(), key_set_id.c_str());

  if (key_set_id.empty()) {
    LOGI("CdmEngine::RestoreKey: invalid key set id");
    return EMPTY_KEYSET_ID_ENG_4;
  }

  std::shared_ptr<CdmSession> session;
  if (!session_map_.FindSession(session_id, &session)) {
    LOGE("CdmEngine::RestoreKey: session_id not found = %s ",
         session_id.c_str());
    return SESSION_NOT_FOUND_4;
  }

  CdmResponseType sts;
  int error_detail = NO_ERROR;
  sts = session->RestoreOfflineSession(key_set_id, kLicenseTypeOffline,
                                       &error_detail);
  session->GetMetrics()->cdm_session_restore_offline_session_.Increment(
      sts, error_detail);
  if (sts == NEED_PROVISIONING) {
    cert_provisioning_requested_security_level_ =
        session->GetRequestedSecurityLevel();
  }
  if (sts != KEY_ADDED && sts != GET_RELEASED_LICENSE_ERROR) {
    LOGE("CdmEngine::RestoreKey: restore offline session failed = %d", sts);
  }
  return sts;
}

CdmResponseType CdmEngine::RemoveKeys(const CdmSessionId& session_id) {
  LOGI("CdmEngine::RemoveKeys: %s", session_id.c_str());

  std::shared_ptr<CdmSession> session;
  if (!session_map_.FindSession(session_id, &session)) {
    LOGE("CdmEngine::RemoveKeys: session_id not found = %s",
         session_id.c_str());
    return SESSION_NOT_FOUND_5;
  }

  session->RemoveKeys();

  return NO_ERROR;
}

CdmResponseType CdmEngine::RemoveLicense(const CdmSessionId& session_id) {
  LOGI("CdmEngine::RemoveLicense: %s", session_id.c_str());

  std::shared_ptr<CdmSession> session;
  if (!session_map_.FindSession(session_id, &session)) {
    LOGE("session_id not found = %s", session_id.c_str());
    return SESSION_NOT_FOUND_19;
  }

  return session->RemoveLicense();
}

CdmResponseType CdmEngine::GenerateRenewalRequest(
    const CdmSessionId& session_id, CdmKeyRequest* key_request) {
  LOGI("CdmEngine::GenerateRenewalRequest: %s", session_id.c_str());

  std::shared_ptr<CdmSession> session;
  if (!session_map_.FindSession(session_id, &session)) {
    LOGE("CdmEngine::GenerateRenewalRequest: session_id not found = %s",
         session_id.c_str());
    return SESSION_NOT_FOUND_6;
  }

  if (!key_request) {
    LOGE("CdmEngine::GenerateRenewalRequest: no request destination");
    return PARAMETER_NULL;
  }

  key_request->message.clear();

  CdmResponseType sts = session->GenerateRenewalRequest(key_request);

  if (KEY_MESSAGE != sts) {
    LOGE("CdmEngine::GenerateRenewalRequest: key request gen. failed, sts=%d",
         sts);
    return sts;
  }

  return KEY_MESSAGE;
}

CdmResponseType CdmEngine::RenewKey(const CdmSessionId& session_id,
                                    const CdmKeyResponse& key_data) {
  LOGI("CdmEngine::RenewKey: %s", session_id.c_str());

  std::shared_ptr<CdmSession> session;
  if (!session_map_.FindSession(session_id, &session)) {
    LOGE("CdmEngine::RenewKey: session_id not found = %s", session_id.c_str());
    return SESSION_NOT_FOUND_7;
  }

  if (key_data.empty()) {
    LOGE("CdmEngine::RenewKey: no key_data");
    return EMPTY_KEY_DATA_2;
  }

  CdmResponseType sts;
  M_TIME(
      sts = session->RenewKey(
          key_data),
      session->GetMetrics(),
      cdm_session_renew_key_,
      sts);

  if (KEY_ADDED != sts) {
    LOGE("CdmEngine::RenewKey: keys not added, sts=%d", static_cast<int>(sts));
    return sts;
  }

  return KEY_ADDED;
}

CdmResponseType CdmEngine::QueryStatus(SecurityLevel security_level,
                                       const std::string& query_token,
                                       std::string* query_response) {
  LOGI("CdmEngine::QueryStatus");
  std::unique_ptr<CryptoSession> crypto_session(
      CryptoSession::MakeCryptoSession(metrics_->GetCryptoMetrics()));
  CdmResponseType status;

  if (!query_response) {
    LOGE("CdmEngine::QueryStatus: no query response destination");
    return PARAMETER_NULL;
  }

  // Add queries here, that can be answered before a session is opened
  if (query_token == QUERY_KEY_SECURITY_LEVEL) {
    CdmSecurityLevel found_security_level =
        crypto_session->GetSecurityLevel(security_level);
    switch (found_security_level) {
      case kSecurityLevelL1:
        *query_response = QUERY_VALUE_SECURITY_LEVEL_L1;
        break;
      case kSecurityLevelL2:
        *query_response = QUERY_VALUE_SECURITY_LEVEL_L2;
        break;
      case kSecurityLevelL3:
        *query_response = QUERY_VALUE_SECURITY_LEVEL_L3;
        break;
      case kSecurityLevelUninitialized:
      case kSecurityLevelUnknown:
        *query_response = QUERY_VALUE_SECURITY_LEVEL_UNKNOWN;
        break;
      default:
        LOGW("CdmEngine::QueryStatus: Unknown security level: %d",
             found_security_level);
        return UNKNOWN_ERROR;
    }
    return NO_ERROR;
  } else if (query_token == QUERY_KEY_CURRENT_HDCP_LEVEL ||
             query_token == QUERY_KEY_MAX_HDCP_LEVEL) {
    CryptoSession::HdcpCapability current_hdcp;
    CryptoSession::HdcpCapability max_hdcp;
    status = crypto_session->GetHdcpCapabilities(security_level, &current_hdcp,
                                                 &max_hdcp);

    if (status != NO_ERROR) {
      LOGW("CdmEngine::QueryStatus: GetHdcpCapabilities failed: %d", status);
      return status;
    }
    *query_response =
        MapHdcpVersion(query_token == QUERY_KEY_CURRENT_HDCP_LEVEL ?
                       current_hdcp : max_hdcp);
    return NO_ERROR;
  } else if (query_token == QUERY_KEY_USAGE_SUPPORT) {
    bool supports_usage_reporting;
    bool got_info = crypto_session->UsageInformationSupport(
        security_level,
        &supports_usage_reporting);

    if (!got_info) {
      LOGW("CdmEngine::QueryStatus: UsageInformationSupport failed");
      metrics_->GetCryptoMetrics()->crypto_session_usage_information_support_
          .SetError(got_info);
      return UNKNOWN_ERROR;
    }
    metrics_->GetCryptoMetrics()->crypto_session_usage_information_support_
        .Record(supports_usage_reporting);

    *query_response =
        supports_usage_reporting ? QUERY_VALUE_TRUE : QUERY_VALUE_FALSE;
    return NO_ERROR;
  } else if (query_token == QUERY_KEY_NUMBER_OF_OPEN_SESSIONS) {
    size_t number_of_open_sessions;
    status = crypto_session->GetNumberOfOpenSessions(security_level,
                                                     &number_of_open_sessions);
    if (status != NO_ERROR) {
      LOGW("CdmEngine::QueryStatus: GetNumberOfOpenSessions failed: %d",
           status);
      return status;
    }

    *query_response = std::to_string(number_of_open_sessions);
    return NO_ERROR;
  } else if (query_token == QUERY_KEY_MAX_NUMBER_OF_SESSIONS) {
    size_t maximum_number_of_sessions = 0;
    status =
        crypto_session->GetMaxNumberOfSessions(security_level,
                                               &maximum_number_of_sessions);

    if (status != NO_ERROR) {
      LOGW("CdmEngine::QueryStatus: GetMaxNumberOfOpenSessions failed: %d",
           status);
      return status;
    }

    *query_response = std::to_string(maximum_number_of_sessions);
    return NO_ERROR;
  } else if (query_token == QUERY_KEY_OEMCRYPTO_API_VERSION) {
    uint32_t api_version;
    if (!crypto_session->GetApiVersion(security_level, &api_version)) {
      LOGW("CdmEngine::QueryStatus: GetApiVersion failed");
      return UNKNOWN_ERROR;
    }

    *query_response = std::to_string(api_version);
    return NO_ERROR;
  } else if (query_token == QUERY_KEY_CURRENT_SRM_VERSION) {
    uint16_t current_srm_version;
    status = crypto_session->GetSrmVersion(&current_srm_version);
    if (status == NOT_IMPLEMENTED_ERROR) {
      *query_response = QUERY_VALUE_NONE;
      return NO_ERROR;
    } else if (status != NO_ERROR) {
      LOGW("CdmEngine::QueryStatus: GetCurrentSRMVersion failed: %d", status);
      return status;
    }

    *query_response = std::to_string(current_srm_version);
    return NO_ERROR;
  } else if (query_token == QUERY_KEY_SRM_UPDATE_SUPPORT) {
    bool is_srm_update_supported = crypto_session->IsSrmUpdateSupported();
    *query_response =
        is_srm_update_supported ? QUERY_VALUE_TRUE : QUERY_VALUE_FALSE;
    return NO_ERROR;
  } else if (query_token == QUERY_KEY_WVCDM_VERSION) {
    std::string cdm_version;
    if (!Properties::GetWVCdmVersion(&cdm_version)) {
      LOGW("CdmEngine::QueryStatus: GetWVCdmVersion failed");
      return UNKNOWN_ERROR;
    }

    *query_response = cdm_version;
    return NO_ERROR;
  } else if (query_token == QUERY_KEY_RESOURCE_RATING_TIER) {
    uint32_t tier;
    if (!crypto_session->GetResourceRatingTier(security_level, &tier)) {
      LOGW("CdmEngine::QueryStatus: GetResourceRatingTier failed");
      return UNKNOWN_ERROR;
    }

    *query_response = std::to_string(tier);
    return NO_ERROR;
  } else if (query_token == QUERY_KEY_OEMCRYPTO_BUILD_INFORMATION) {
    if (!crypto_session->GetBuildInformation(security_level, query_response)) {
      LOGW("CdmEngine::QueryStatus: GetBuildInformation failed");
      return UNKNOWN_ERROR;
    }
    return NO_ERROR;
  } else if (query_token == QUERY_KEY_DECRYPT_HASH_SUPPORT) {
    *query_response = std::to_string(crypto_session->IsDecryptHashSupported(
        security_level));
    return NO_ERROR;
  }

  M_TIME(
      status = crypto_session->Open(
          security_level),
      metrics_->GetCryptoMetrics(),
      crypto_session_open_,
      status,
      security_level);

  if (status != NO_ERROR) return status;

  // Add queries here, that need an open session before they can be answered
  if (query_token == QUERY_KEY_DEVICE_ID) {
    std::string deviceId;
    status = crypto_session->GetExternalDeviceUniqueId(&deviceId);
    metrics_->GetCryptoMetrics()->crypto_session_get_device_unique_id_
        .Increment(status);
    if (status != NO_ERROR) return status;

    *query_response = deviceId;
  } else if (query_token == QUERY_KEY_SYSTEM_ID) {
    uint32_t system_id;
    bool got_id = crypto_session->GetSystemId(&system_id);
    if (!got_id) {
      LOGW("CdmEngine::QueryStatus: QUERY_KEY_SYSTEM_ID unknown failure");
      return UNKNOWN_ERROR;
    }

    *query_response = std::to_string(system_id);
  } else if (query_token == QUERY_KEY_PROVISIONING_ID) {
    std::string provisioning_id;
    status = crypto_session->GetProvisioningId(&provisioning_id);
    if (status != NO_ERROR) {
      LOGW("CdmEngine::QueryStatus: GetProvisioningId failed: %d", status);
      return status;
    }

    *query_response = provisioning_id;
  } else {
    LOGW("CdmEngine::QueryStatus: Unknown status requested, token = %s",
         query_token.c_str());
    return INVALID_QUERY_KEY;
  }

  return NO_ERROR;
}

CdmResponseType CdmEngine::QuerySessionStatus(const CdmSessionId& session_id,
                                              CdmQueryMap* query_response) {
  LOGI("CdmEngine::QuerySessionStatus: %s", session_id.c_str());
  std::shared_ptr<CdmSession> session;
  if (!session_map_.FindSession(session_id, &session)) {
    LOGE("CdmEngine::QuerySessionStatus: session_id not found = %s",
         session_id.c_str());
    return SESSION_NOT_FOUND_8;
  }
  return session->QueryStatus(query_response);
}

bool CdmEngine::IsReleaseSession(const CdmSessionId& session_id) {
  LOGI("CdmEngine::IsReleaseSession: %s", session_id.c_str());
  std::shared_ptr<CdmSession> session;
  if (!session_map_.FindSession(session_id, &session)) {
    LOGE("CdmEngine::IsReleaseSession: session_id not found = %s",
         session_id.c_str());
    return false;
  }
  return session->is_release();
}

bool CdmEngine::IsOfflineSession(const CdmSessionId& session_id) {
  LOGI("CdmEngine::IsOfflineSession: %s", session_id.c_str());
  std::shared_ptr<CdmSession> session;
  if (!session_map_.FindSession(session_id, &session)) {
    LOGE("CdmEngine::IsOfflineSession: session_id not found = %s",
         session_id.c_str());
    return false;
  }
  return session->is_offline();
}

CdmResponseType CdmEngine::QueryKeyStatus(const CdmSessionId& session_id,
                                          CdmQueryMap* query_response) {
  LOGI("CdmEngine::QueryKeyStatus: %s", session_id.c_str());
  std::shared_ptr<CdmSession> session;
  if (!session_map_.FindSession(session_id, &session)) {
    LOGE("CdmEngine::QueryKeyStatus: session_id not found = %s",
         session_id.c_str());
    return SESSION_NOT_FOUND_9;
  }
  return session->QueryKeyStatus(query_response);
}

CdmResponseType CdmEngine::QueryKeyAllowedUsage(const CdmSessionId& session_id,
                                                const std::string& key_id,
                                                CdmKeyAllowedUsage* key_usage) {
  LOGI("CdmEngine::QueryKeyAllowedUsage: %s", session_id.c_str());
  if (!key_usage) {
    LOGE("CdmEngine::QueryKeyAllowedUsage: no response destination");
    return PARAMETER_NULL;
  }
  std::shared_ptr<CdmSession> session;
  if (!session_map_.FindSession(session_id, &session)) {
    LOGE("CdmEngine::QueryKeyAllowedUsage: session_id not found = %s",
         session_id.c_str());
    return SESSION_NOT_FOUND_12;
  }
  return session->QueryKeyAllowedUsage(key_id, key_usage);
}

CdmResponseType CdmEngine::QueryKeyAllowedUsage(const std::string& key_id,
                                                CdmKeyAllowedUsage* key_usage) {
  LOGI("CdmEngine::QueryKeyAllowedUsage (all sessions)");
  CdmResponseType session_sts;
  CdmKeyAllowedUsage found_in_this_session;
  bool found = false;
  if (!key_usage) {
    LOGE("CdmEngine::QueryKeyAllowedUsage: no response destination");
    return PARAMETER_NULL;
  }
  key_usage->Clear();

  CdmSessionList sessions;
  session_map_.GetSessionList(sessions);

  for (CdmSessionList::iterator iter = sessions.begin();
       iter != sessions.end(); ++iter) {
    session_sts = (*iter)->QueryKeyAllowedUsage(key_id, &found_in_this_session);
    if (session_sts == NO_ERROR) {
      if (found) {
        // Found another key. If usage settings do not match, fail.
        if (!key_usage->Equals(found_in_this_session)) {
          key_usage->Clear();
          return KEY_CONFLICT_1;
        }
      } else {
        *key_usage = found_in_this_session;
        found = true;
      }
    } else if (session_sts != KEY_NOT_FOUND_1) {
      LOGE("CdmEngine::QueryKeyAllowedUsage (all sessions) FAILED = %d",
           session_sts);
      key_usage->Clear();
      return session_sts;
    }
  }
  return (found) ? NO_ERROR : KEY_NOT_FOUND_2;
}

CdmResponseType CdmEngine::QueryOemCryptoSessionId(
    const CdmSessionId& session_id, CdmQueryMap* query_response) {
  LOGI("CdmEngine::QueryOemCryptoSessionId: %s", session_id.c_str());
  std::shared_ptr<CdmSession> session;
  if (!session_map_.FindSession(session_id, &session)) {
    LOGE("CdmEngine::QueryOemCryptoSessionId: session_id not found = %s",
         session_id.c_str());
    return SESSION_NOT_FOUND_10;
  }
  return session->QueryOemCryptoSessionId(query_response);
}

bool CdmEngine::IsSecurityLevelSupported(CdmSecurityLevel level) {
  LOGI("CdmEngine::IsSecurityLevelSupported");
  metrics::CryptoMetrics alternate_crypto_metrics;
  std::unique_ptr<CryptoSession> crypto_session(
      CryptoSession::MakeCryptoSession(&alternate_crypto_metrics));

  switch (level) {
    case kSecurityLevelL1:
      return
          crypto_session->GetSecurityLevel(kLevelDefault) == kSecurityLevelL1;
    case kSecurityLevelL3:
      return crypto_session->GetSecurityLevel(kLevel3) == kSecurityLevelL3;
    default:
      LOGE("CdmEngine::IsSecurityLevelSupported: Invalid security level: %d",
           level);
      return false;
  }
}

/*
 * Composes a device provisioning request and output the request in JSON format
 * in *request. It also returns the default url for the provisioning server
 * in *default_url.
 *
 * Returns NO_ERROR for success and CdmResponseType error code if fails.
 */
CdmResponseType CdmEngine::GetProvisioningRequest(
    CdmCertificateType cert_type, const std::string& cert_authority,
    const std::string& service_certificate, CdmProvisioningRequest* request,
    std::string* default_url) {
  LOGI("CdmEngine::GetProvisioningRequest");
  if (!request) {
    LOGE("CdmEngine::GetProvisioningRequest: invalid output parameters");
    return INVALID_PROVISIONING_REQUEST_PARAM_1;
  }
  if (!default_url) {
    LOGE("CdmEngine::GetProvisioningRequest: invalid output parameters");
    return INVALID_PROVISIONING_REQUEST_PARAM_2;
  }

  DeleteAllUsageReportsUponFactoryReset();

  if (NULL == cert_provisioning_.get()) {
    cert_provisioning_.reset(
        new CertificateProvisioning(metrics_->GetCryptoMetrics()));
    CdmResponseType status = cert_provisioning_->Init(service_certificate);
    if (status != NO_ERROR) return status;
  }
  CdmResponseType ret = cert_provisioning_->GetProvisioningRequest(
      cert_provisioning_requested_security_level_, cert_type, cert_authority,
      file_system_->origin(), spoid_, request, default_url);
  if (ret != NO_ERROR) {
    cert_provisioning_.reset(NULL);  // Release resources.
  }
  return ret;
}

/*
 * The response message consists of a device certificate and the device RSA key.
 * The device RSA key is stored in the T.E.E. The device certificate is stored
 * in the device.
 *
 * Returns NO_ERROR for success and  CdmResponseType error code if fails.
 */
CdmResponseType CdmEngine::HandleProvisioningResponse(
    const CdmProvisioningResponse& response, std::string* cert,
    std::string* wrapped_key) {
  LOGI("CdmEngine::HandleProvisioningResponse");
  if (response.empty()) {
    LOGE("CdmEngine::HandleProvisioningResponse: Empty provisioning response.");
    cert_provisioning_.reset(NULL);
    return EMPTY_PROVISIONING_RESPONSE;
  }
  if (cert == NULL) {
    LOGE(
        "CdmEngine::HandleProvisioningResponse: invalid certificate "
        "destination");
    cert_provisioning_.reset(NULL);
    return INVALID_PROVISIONING_PARAMETERS_1;
  }
  if (wrapped_key == NULL) {
    LOGE("CdmEngine::HandleProvisioningResponse: invalid wrapped key "
         "destination");
    cert_provisioning_.reset(NULL);
    return INVALID_PROVISIONING_PARAMETERS_2;
  }
  if (NULL == cert_provisioning_.get()) {
    // Certificate provisioning object has been released. Check if a concurrent
    // provisioning attempt has succeeded before declaring failure.
    std::unique_ptr<CryptoSession> crypto_session(
        CryptoSession::MakeCryptoSession(metrics_->GetCryptoMetrics()));
    CdmResponseType status;
    M_TIME(
        status = crypto_session->Open(
            cert_provisioning_requested_security_level_),
        metrics_->GetCryptoMetrics(),
        crypto_session_open_,
        status,
        cert_provisioning_requested_security_level_);
    if (NO_ERROR != status) {
      LOGE(
          "CdmEngine::HandleProvisioningResponse: provisioning object "
          "missing and crypto session open failed.");
      return EMPTY_PROVISIONING_CERTIFICATE_2;
    }
    CdmSecurityLevel security_level = crypto_session->GetSecurityLevel();
    if (!IsProvisioned(security_level)) {
      LOGE(
          "CdmEngine::HandleProvisioningResponse: provisioning object "
          "missing.");
      return EMPTY_PROVISIONING_CERTIFICATE_1;
    }
    return NO_ERROR;
  }

  CdmResponseType ret = cert_provisioning_->HandleProvisioningResponse(
      file_system_, response, cert, wrapped_key);
  // Release resources only on success. It is possible that a provisioning
  // attempt was made after this one was requested but before the response was
  // received, which will cause this attempt to fail. Not releasing will
  // allow for the possibility that the later attempt succeeds.
  if (NO_ERROR == ret) cert_provisioning_.reset(NULL);
  return ret;
}

bool CdmEngine::IsProvisioned(CdmSecurityLevel security_level) {
  // To validate whether the given security level is provisioned, we attempt to
  // initialize a CdmSession. This verifies the existence of a certificate and
  // attempts to load it. If this fails, initialization will return an error.
  UsagePropertySet property_set;
  property_set.set_security_level(
    security_level == kSecurityLevelL3 ? kLevel3 : kLevelDefault);

  CdmSession session(file_system_, metrics_->AddSession());

  CdmResponseType status = session.Init(&property_set);
  if (NO_ERROR != status) {
    LOGE("CdmEngine::IsProvisioned: CdmSession::Init returned %lu", status);
  }
  return status == NO_ERROR;
}

CdmResponseType CdmEngine::Unprovision(CdmSecurityLevel security_level) {
  LOGI("CdmEngine::Unprovision: security level: %d", security_level);
  // Devices with baked-in DRM certs cannot be reprovisioned and therefore must
  // not be unprovisioned.
  std::unique_ptr<CryptoSession> crypto_session(
      CryptoSession::MakeCryptoSession(metrics_->GetCryptoMetrics()));
  CdmClientTokenType token_type = kClientTokenUninitialized;
  CdmResponseType res = crypto_session->GetProvisioningMethod(
    security_level == kSecurityLevelL3 ? kLevel3 : kLevelDefault,
    &token_type);
  if (res != NO_ERROR) {
    return res;
  } else if (token_type == kClientTokenDrmCert) {
    return DEVICE_CANNOT_REPROVISION;
  }

  DeviceFiles handle(file_system_);
  if (!handle.Init(security_level)) {
    LOGE("CdmEngine::Unprovision: unable to initialize device files");
    return UNPROVISION_ERROR_1;
  }

  if (!file_system_->IsGlobal()) {
    if (!handle.RemoveCertificate()) {
      LOGE("CdmEngine::Unprovision: unable to delete certificate");
      return UNPROVISION_ERROR_2;
    }
    return NO_ERROR;
  } else {
    if (!handle.DeleteAllFiles()) {
      LOGE("CdmEngine::Unprovision: unable to delete files");
      return UNPROVISION_ERROR_3;
    }
    return DeleteUsageTable(security_level);
  }
}

CdmResponseType CdmEngine::DeleteUsageTable(CdmSecurityLevel security_level) {
  LOGI("CdmEngine::DeleteUsageTable: security level: %d", security_level);
  std::unique_ptr<CryptoSession> crypto_session(
      CryptoSession::MakeCryptoSession(metrics_->GetCryptoMetrics()));
  CdmResponseType status;
  M_TIME(
      status = crypto_session->Open(
          security_level == kSecurityLevelL3 ?
              kLevel3 :
              kLevelDefault),
      metrics_->GetCryptoMetrics(),
      crypto_session_open_,
      status,
      security_level == kSecurityLevelL3 ? kLevel3 : kLevelDefault);
  if (NO_ERROR != status) {
    LOGE("CdmEngine::DeleteUsageTable: error opening crypto session: %d",
       status);
    return UNPROVISION_ERROR_4;
  }
  status = crypto_session->DeleteAllUsageReports();
  metrics_->GetCryptoMetrics()->crypto_session_delete_all_usage_reports_
      .Increment(status);
  if (status != NO_ERROR) {
    LOGE("CdmEngine::DeleteUsageTable: error deleteing usage reports: %d",
         status);
  }
  return status;
}

CdmResponseType CdmEngine::ListStoredLicenses(
    CdmSecurityLevel security_level, std::vector<std::string>* key_set_ids) {
  DeviceFiles handle(file_system_);
  if (!key_set_ids) {
    LOGE("CdmEngine::ListStoredLicenses: no response destination");
    return INVALID_PARAMETERS_ENG_22;
  }
  if (!handle.Init(security_level)) {
    LOGE("CdmEngine::ListStoredLicenses: unable to initialize device files");
    return LIST_LICENSE_ERROR_1;
  }
  if (!handle.ListLicenses(key_set_ids)) {
    LOGE("CdmEngine::ListStoredLicenses: ListLicenses call failed");
    return LIST_LICENSE_ERROR_2;
  }
  return NO_ERROR;
}

CdmResponseType CdmEngine::ListUsageIds(
    const std::string& app_id,
    CdmSecurityLevel security_level,
    std::vector<std::string>* ksids,
    std::vector<std::string>* provider_session_tokens) {
  DeviceFiles handle(file_system_);
  if (!ksids && !provider_session_tokens) {
    LOGE("CdmEngine::ListUsageIds: no response destination");
    return INVALID_PARAMETERS_ENG_23;
  }
  if (!handle.Init(security_level)) {
    LOGE("CdmEngine::ListUsageIds: unable to initialize device files");
    return LIST_USAGE_ERROR_1;
  }
  if (!handle.ListUsageIds(app_id, ksids, provider_session_tokens)) {
    LOGE("CdmEngine::ListUsageIds: ListUsageIds call failed");
    return LIST_USAGE_ERROR_2;
  }
  return NO_ERROR;
}

CdmResponseType CdmEngine::DeleteUsageRecord(const std::string& app_id,
                                             CdmSecurityLevel security_level,
                                             const std::string& key_set_id) {
  LOGI("CdmEngine::DeleteUsageRecord: %s, app ID: %s", key_set_id.c_str(),
       app_id.c_str());
  std::string provider_session_token;

  DeviceFiles handle(file_system_);
  if (!handle.Init(security_level)) {
    LOGE("CdmEngine::DeleteUsageRecord: unable to initialize device files");
    return DELETE_USAGE_ERROR_1;
  }
  if (!handle.GetProviderSessionToken(app_id, key_set_id,
                                      &provider_session_token)) {
    LOGE("CdmEngine::DeleteUsageRecord: GetProviderSessionToken failed");
    return DELETE_USAGE_ERROR_2;
  }

  return RemoveUsageInfo(app_id, provider_session_token);
}

CdmResponseType CdmEngine::GetOfflineLicenseState(
    const CdmKeySetId &key_set_id,
    CdmSecurityLevel security_level,
    CdmOfflineLicenseState* license_state) {
  DeviceFiles handle(file_system_);
  if (!handle.Init(security_level)) {
    LOGE("CdmEngine::GetOfflineLicenseState: cannot initialize device files");
    return GET_OFFLINE_LICENSE_STATE_ERROR_1;
  }

  DeviceFiles::LicenseState state;

  // Dummy arguments to make the RetrieveLicense call,
  // do not care about the return value except for license state.
  CdmAppParameterMap app_parameters;
  CdmKeyMessage key_request;
  CdmKeyResponse key_response;
  CdmInitData offline_init_data;
  CdmKeyMessage offline_key_renewal_request;
  CdmKeyResponse offline_key_renewal_response;
  CdmUsageEntry usage_entry;
  int64_t grace_period_end_time;
  int64_t last_playback_time;
  std::string offline_release_server_url;
  int64_t playback_start_time;
  uint32_t usage_entry_number;
  DeviceFiles::ResponseType sub_error_code = DeviceFiles::kNoError;

  if (handle.RetrieveLicense(key_set_id, &state, &offline_init_data,
          &key_request, &key_response,
          &offline_key_renewal_request, &offline_key_renewal_response,
          &offline_release_server_url,
          &playback_start_time, &last_playback_time, &grace_period_end_time,
          &app_parameters, &usage_entry, &usage_entry_number,
          &sub_error_code)) {
      *license_state = MapDeviceFilesLicenseState(state);
  } else {
    LOGE("CdmEngine::GetOfflineLicenseState:: failed to retrieve license state "
         "key set id = %s",
         key_set_id.c_str());
         return GET_OFFLINE_LICENSE_STATE_ERROR_2;
  }
  return NO_ERROR;
}

CdmResponseType CdmEngine::RemoveOfflineLicense(
    const CdmKeySetId &key_set_id,
    CdmSecurityLevel security_level) {

  UsagePropertySet property_set;
  property_set.set_security_level(
      security_level == kSecurityLevelL3 ? kLevel3 : kLevelDefault);
  DeviceFiles handle(file_system_);
  CdmResponseType sts = OpenKeySetSession(key_set_id,
      &property_set, nullptr /* event listener */);
  if (sts != NO_ERROR) {
    if (!handle.Init(security_level)) {
      LOGE("CdmEngine::RemoveOfflineLicense: cannot initialize device files");
    }
    handle.DeleteLicense(key_set_id);
    return REMOVE_OFFLINE_LICENSE_ERROR_1;
  }

  CdmSessionId session_id;
  CdmAppParameterMap dummy_app_params;
  InitializationData dummy_init_data("", "", "");
  CdmKeyRequest key_request;
  // Calling with no session_id is okay
  sts = GenerateKeyRequest(session_id, key_set_id, dummy_init_data,
          kLicenseTypeRelease, dummy_app_params, &key_request);
  if (sts == KEY_MESSAGE) {
    std::unique_lock<std::mutex> lock(release_key_sets_lock_);
    CdmReleaseKeySetMap::iterator iter = release_key_sets_.find(key_set_id);
    if (iter == release_key_sets_.end()) {
      LOGE("CdmEngine::RemoveOfflineLicense: key set id not found = %s",
           key_set_id.c_str());
      sts = REMOVE_OFFLINE_LICENSE_ERROR_2;
    } else {
      session_id = iter->second.first;
      sts = RemoveLicense(session_id);
    }
  }

  if (sts != NO_ERROR) {
    LOGE("CdmEngine: GenerateKeyRequest error=%d", sts);
    handle.DeleteLicense(key_set_id);
  }
  CloseKeySetSession(key_set_id);

  return sts;
}

CdmResponseType CdmEngine::GetUsageInfo(const std::string& app_id,
                                        const CdmSecureStopId& ssid,
                                        int* error_detail,
                                        CdmUsageInfo* usage_info) {
  LOGI("CdmEngine::GetUsageInfo: %s", ssid.c_str());
  if (NULL == usage_property_set_.get()) {
    usage_property_set_.reset(new UsagePropertySet());
  }
  if (!usage_info) {
    LOGE("CdmEngine::GetUsageInfo: no usage info destination");
    return PARAMETER_NULL;
  }
  usage_property_set_->set_security_level(kLevelDefault);
  usage_property_set_->set_app_id(app_id);
  usage_session_.reset(new CdmSession(file_system_, metrics_->AddSession()));
  CdmResponseType status = usage_session_->Init(usage_property_set_.get());
  if (NO_ERROR != status) {
    LOGE("CdmEngine::GetUsageInfo: session init error: %d", status);
    return status;
  }
  DeviceFiles handle(file_system_);
  if (!handle.Init(usage_session_->GetSecurityLevel())) {
    LOGE("CdmEngine::GetUsageInfo: device file init error");
    return GET_USAGE_INFO_ERROR_1;
  }

  CdmKeyMessage license_request;
  CdmKeyResponse license_response;
  std::string usage_entry;
  DeviceFiles::CdmUsageData usage_data;
  if (!handle.RetrieveUsageInfo(DeviceFiles::GetUsageInfoFileName(app_id),
                                ssid, &usage_data)) {
    usage_property_set_->set_security_level(kLevel3);
    usage_property_set_->set_app_id(app_id);
    usage_session_.reset(new CdmSession(file_system_, metrics_->AddSession()));
    status = usage_session_->Init(usage_property_set_.get());
    if (NO_ERROR != status) {
      LOGE("CdmEngine::GetUsageInfo: session init error");
      return status;
    }
    if (!handle.Reset(usage_session_->GetSecurityLevel())) {
      LOGE("CdmEngine::GetUsageInfo: device file init error");
      return GET_USAGE_INFO_ERROR_2;
    }
    if (!handle.RetrieveUsageInfo(DeviceFiles::GetUsageInfoFileName(app_id),
                                  ssid, &usage_data)) {
      // No entry found for that ssid.
      return USAGE_INFO_NOT_FOUND;
    }
  }

  status = usage_session_->RestoreUsageSession(usage_data, error_detail);

  if (KEY_ADDED != status) {
    LOGE("CdmEngine::GetUsageInfo: restore usage session error %d", status);
    usage_info->clear();
    return status;
  }

  CdmKeyRequest request;
  status = usage_session_->GenerateReleaseRequest(&request);

  usage_info->clear();
  usage_info->push_back(request.message);

  if (KEY_MESSAGE != status) {
    LOGE("CdmEngine::GetUsageInfo: generate release request error: %d", status);
    usage_info->clear();
    return status;
  }

  return KEY_MESSAGE;
}

CdmResponseType CdmEngine::GetUsageInfo(const std::string& app_id,
                                        int* error_detail,
                                        CdmUsageInfo* usage_info) {
  LOGI("CdmEngine::GetUsageInfo: %s", app_id.c_str());
  // Return a random usage report from a random security level
  SecurityLevel security_level = ((rand() % 2) == 0) ? kLevelDefault : kLevel3;
  CdmResponseType status = UNKNOWN_ERROR;
  if (!usage_info) {
    LOGE("CdmEngine::GetUsageInfo: no usage info destination");
    return PARAMETER_NULL;
  }
  do {
    status = GetUsageInfo(app_id, security_level, error_detail, usage_info);

    if (KEY_MESSAGE == status && !usage_info->empty()) {
      return status;
    }
  } while (KEY_CANCELED == status);

  security_level = (kLevel3 == security_level) ? kLevelDefault : kLevel3;
  do {
    status = GetUsageInfo(app_id, security_level, error_detail, usage_info);
    if (NEED_PROVISIONING == status)
      return NO_ERROR;  // Valid scenario that one of the security
                        // levels has not been provisioned
  } while (KEY_CANCELED == status);
  return status;
}

CdmResponseType CdmEngine::GetUsageInfo(const std::string& app_id,
                                        SecurityLevel requested_security_level,
                                        int* error_detail,
                                        CdmUsageInfo* usage_info) {
  LOGI("CdmEngine::GetUsageInfo: %s, security level: %d", app_id.c_str(),
       requested_security_level);
  if (NULL == usage_property_set_.get()) {
    usage_property_set_.reset(new UsagePropertySet());
  }
  usage_property_set_->set_security_level(requested_security_level);
  usage_property_set_->set_app_id(app_id);

  usage_session_.reset(new CdmSession(file_system_, metrics_->AddSession()));

  CdmResponseType status = usage_session_->Init(usage_property_set_.get());
  if (NO_ERROR != status) {
    LOGE("CdmEngine::GetUsageInfo: session init error");
    return status;
  }

  DeviceFiles handle(file_system_);
  if (!handle.Init(usage_session_->GetSecurityLevel())) {
    LOGE("CdmEngine::GetUsageInfo: unable to initialize device files");
    return GET_USAGE_INFO_ERROR_3;
  }

  std::vector<DeviceFiles::CdmUsageData> usage_data;
  if (!handle.RetrieveUsageInfo(DeviceFiles::GetUsageInfoFileName(app_id),
                                &usage_data)) {
    LOGE("CdmEngine::GetUsageInfo: unable to read usage information");
    return GET_USAGE_INFO_ERROR_4;
  }

  if (!usage_info) {
    LOGE("CdmEngine::GetUsageInfo: no usage info destination");
    return PARAMETER_NULL;
  }
  if (0 == usage_data.size()) {
    usage_info->resize(0);
    return NO_ERROR;
  }

  usage_info->resize(kUsageReportsPerRequest);

  uint32_t index = rand() % usage_data.size();
  status = usage_session_->RestoreUsageSession(usage_data[index], error_detail);
  if (KEY_ADDED != status) {
    LOGE("CdmEngine::GetUsageInfo: restore usage session (%d) error %ld", index,
         status);
    usage_info->clear();
    return status;
  }

  CdmKeyRequest request;
  status = usage_session_->GenerateReleaseRequest(&request);

  usage_info->clear();
  usage_info->push_back(request.message);

  switch (status) {
    case KEY_MESSAGE:
      break;
    case KEY_CANCELED:                      // usage information not present in
      usage_session_->DeleteLicenseFile();  // OEMCrypto, delete and try again
      usage_info->clear();
      break;
    default:
      LOGE("CdmEngine::GetUsageInfo: generate release request error: %d",
           status);
      usage_info->clear();
      break;
  }
  return status;
}

CdmResponseType CdmEngine::RemoveAllUsageInfo(
    const std::string& app_id, CdmSecurityLevel cdm_security_level) {
  LOGI("CdmEngine::RemoveAllUsageInfo: %s, security level: %d",
       app_id.c_str(), cdm_security_level);
  if (usage_property_set_.get() == nullptr) {
    usage_property_set_.reset(new UsagePropertySet());
  }
  usage_property_set_->set_app_id(app_id);

  CdmResponseType status = NO_ERROR;
  DeviceFiles handle(file_system_);
  if (handle.Init(cdm_security_level)) {
    SecurityLevel security_level =
        cdm_security_level == kSecurityLevelL3 ? kLevel3 : kLevelDefault;
    usage_property_set_->set_security_level(security_level);
    usage_session_.reset(new CdmSession(file_system_, metrics_->AddSession()));
    usage_session_->Init(usage_property_set_.get());

    switch (usage_session_->get_usage_support_type()) {
      case kUsageEntrySupport: {
        std::vector<DeviceFiles::CdmUsageData> usage_data;
        // Retrieve all usage information but delete only one before
        // refetching. This is because deleting the usage entry
        // might cause other entries to be shifted and information updated.
        do {
          if (!handle.RetrieveUsageInfo(
              DeviceFiles::GetUsageInfoFileName(app_id),
              &usage_data)) {
            LOGW("CdmEngine::RemoveAllUsageInfo: failed to retrieve usage info");
            break;
          }

          if (usage_data.empty()) break;

          CdmResponseType res = usage_session_->DeleteUsageEntry(
            usage_data[0].usage_entry_number);

          if (res != NO_ERROR) {
            LOGW("CdmEngine::RemoveAllUsageInfo: failed to delete usage "
                 "entry: error: %d", res);
            break;
          }

          if (!handle.DeleteUsageInfo(
              DeviceFiles::GetUsageInfoFileName(app_id),
              usage_data[0].provider_session_token)) {
            LOGW("CdmEngine::RemoveAllUsageInfo: failed to delete usage "
                 "info");
            break;
          }
        } while (!usage_data.empty());

        std::vector<std::string> provider_session_tokens;
        if (!handle.DeleteAllUsageInfoForApp(
            DeviceFiles::GetUsageInfoFileName(app_id),
            &provider_session_tokens)) {
          status = REMOVE_ALL_USAGE_INFO_ERROR_5;
        }
        break;
      }
      case kUsageTableSupport: {
        std::vector<std::string> provider_session_tokens;
        if (!handle.DeleteAllUsageInfoForApp(
            DeviceFiles::GetUsageInfoFileName(app_id),
            &provider_session_tokens)) {
          LOGE("CdmEngine::RemoveAllUsageInfo: failed to delete %d secure"
              "stops", cdm_security_level);
          status = REMOVE_ALL_USAGE_INFO_ERROR_1;
        } else {
          CdmResponseType status2 = usage_session_->
              DeleteMultipleUsageInformation(provider_session_tokens);
          if (status2 != NO_ERROR) status = status2;
        }
        break;
      }
      default:
        // Ignore
        break;
    }
  }
  usage_session_.reset(NULL);
  return status;
}

CdmResponseType CdmEngine::RemoveAllUsageInfo(const std::string& app_id) {
  LOGI("CdmEngine::RemoveAllUsageInfo: %s", app_id.c_str());
  CdmResponseType status_l1, status_l3;
  status_l1 = status_l3 = NO_ERROR;
  status_l1 = RemoveAllUsageInfo(app_id, kSecurityLevelL1);
  status_l3 = RemoveAllUsageInfo(app_id, kSecurityLevelL3);
  return (status_l3 == NO_ERROR) ? status_l3 : status_l1;
}

CdmResponseType CdmEngine::RemoveUsageInfo(
    const std::string& app_id,
    const CdmSecureStopId& provider_session_token) {
  LOGI("CdmEngine::RemoveUsageInfo: %s, PST: %s", app_id.c_str(),
       provider_session_token.c_str());
  if (NULL == usage_property_set_.get()) {
    usage_property_set_.reset(new UsagePropertySet());
  }
  usage_property_set_->set_app_id(app_id);

  CdmResponseType status = NO_ERROR;
  for (int j = kSecurityLevelL1; j < kSecurityLevelUnknown; ++j) {
    DeviceFiles handle(file_system_);
    if (handle.Init(static_cast<CdmSecurityLevel>(j))) {
      SecurityLevel security_level =
          static_cast<CdmSecurityLevel>(j) == kSecurityLevelL3
              ? kLevel3
              : kLevelDefault;
      usage_property_set_->set_security_level(security_level);
      usage_session_.reset(new CdmSession(file_system_, metrics_->AddSession()));
      usage_session_->Init(usage_property_set_.get());

      std::vector<DeviceFiles::CdmUsageData> usage_data;
      CdmKeyMessage license_request;
      CdmKeyResponse license_response;
      CdmUsageEntry usage_entry;
      uint32_t usage_entry_number;

      if (!handle.RetrieveUsageInfo(
          DeviceFiles::GetUsageInfoFileName(app_id), provider_session_token,
          &license_request, &license_response, &usage_entry,
          &usage_entry_number)) {
        // Try other security level
        continue;
      }

      switch (usage_session_->get_usage_support_type()) {
        case kUsageEntrySupport: {
          status = usage_session_->DeleteUsageEntry(usage_entry_number);

          if (!handle.DeleteUsageInfo(
                DeviceFiles::GetUsageInfoFileName(app_id),
                provider_session_token)) {
              status = REMOVE_USAGE_INFO_ERROR_1;
          }
          usage_session_.reset(NULL);
          return status;
        }
        case kUsageTableSupport: {
          std::vector<std::string> provider_session_tokens;
          handle.DeleteUsageInfo(
              DeviceFiles::GetUsageInfoFileName(app_id),
              provider_session_token);
          std::unique_ptr<CryptoSession> crypto_session(
              CryptoSession::MakeCryptoSession(metrics_->GetCryptoMetrics()));
          status = crypto_session->Open(
              static_cast<CdmSecurityLevel>(j) == kSecurityLevelL3
              ? kLevel3 : kLevelDefault);
          if (status == NO_ERROR) {
            crypto_session->UpdateUsageInformation();
            status =
                crypto_session->DeleteUsageInformation(provider_session_token);
            crypto_session->UpdateUsageInformation();
          }
          return status;
        }
        default:
          // Ignore
          break;
      }
    } else {
      LOGE("CdmEngine::RemoveUsageInfo: failed to initialize L%d devicefiles",
           j);
      status = REMOVE_USAGE_INFO_ERROR_2;
    }
  }
  usage_session_.reset(NULL);
  return REMOVE_USAGE_INFO_ERROR_3;
}

CdmResponseType CdmEngine::ReleaseUsageInfo(
    const CdmUsageInfoReleaseMessage& message) {
  LOGI("CdmEngine::ReleaseUsageInfo");
  if (NULL == usage_session_.get()) {
    LOGE("CdmEngine::ReleaseUsageInfo: cdm session not initialized");
    return RELEASE_USAGE_INFO_ERROR;
  }

  CdmResponseType status = usage_session_->ReleaseKey(message);
  usage_session_.reset(NULL);
  if (NO_ERROR != status) {
    LOGE("CdmEngine::ReleaseUsageInfo: release key error: %d", status);
  }
  return status;
}

CdmResponseType CdmEngine::LoadUsageSession(const CdmKeySetId& key_set_id,
                                            CdmKeyMessage* release_message) {
  LOGI("CdmEngine::LoadUsageSession: %s", key_set_id.c_str());
  // This method is currently only used by the CE CDM, in which all session IDs
  // are key set IDs.
  assert(Properties::AlwaysUseKeySetIds());

  if (key_set_id.empty()) {
    LOGE("CdmEngine::LoadUsageSession: invalid key set id");
    return EMPTY_KEYSET_ID_ENG_5;
  }

  std::shared_ptr<CdmSession> session;
  if (!session_map_.FindSession(key_set_id, &session)) {
    LOGE("CdmEngine::LoadUsageSession: session_id not found = %s ",
         key_set_id.c_str());
    return SESSION_NOT_FOUND_11;
  }

  if (!release_message) {
    LOGE("CdmEngine::LoadUsageSession: no release message destination");
    return PARAMETER_NULL;
  }

  DeviceFiles handle(file_system_);
  if (!handle.Init(session->GetSecurityLevel())) {
    LOGE("CdmEngine::LoadUsageSession: unable to initialize device files");
    return LOAD_USAGE_INFO_FILE_ERROR;
  }

  std::string app_id;
  session->GetApplicationId(&app_id);

  DeviceFiles::CdmUsageData usage_data;
  if (!handle.RetrieveUsageInfoByKeySetId(
                  DeviceFiles::GetUsageInfoFileName(app_id), key_set_id,
                  &(usage_data.provider_session_token),
                  &(usage_data.license_request),
                  &(usage_data.license), &(usage_data.usage_entry),
                  &(usage_data.usage_entry_number))) {
    LOGE("CdmEngine::LoadUsageSession: unable to find usage information");
    return LOAD_USAGE_INFO_MISSING;
  }

  int error_detail = NO_ERROR;
  usage_data.key_set_id = key_set_id;
  CdmResponseType status = session->RestoreUsageSession(usage_data,
                                                        &error_detail);
  session->GetMetrics()->cdm_session_restore_usage_session_.Increment(
      status, error_detail);
  if (KEY_ADDED != status) {
    LOGE("CdmEngine::LoadUsageSession: usage session error %ld", status);
    return status;
  }

  CdmKeyRequest request;
  status = session->GenerateReleaseRequest(&request);

  *release_message = request.message;

  switch (status) {
    case KEY_MESSAGE:
      break;
    case KEY_CANCELED:
      // usage information not present in OEMCrypto, delete and try again
      session->DeleteLicenseFile();
      break;
    default:
      LOGE("CdmEngine::LoadUsageSession: generate release request error: %d",
           status);
      break;
  }
  return status;
}

CdmResponseType CdmEngine::Decrypt(const CdmSessionId& session_id,
                                   const CdmDecryptionParameters& parameters) {
  if (parameters.key_id == NULL) {
    LOGE("CdmEngine::Decrypt: no key_id");
    return INVALID_DECRYPT_PARAMETERS_ENG_1;
  }

  if (parameters.encrypt_buffer == NULL) {
    LOGE("CdmEngine::Decrypt: no src encrypt buffer");
    return INVALID_DECRYPT_PARAMETERS_ENG_2;
  }

  if (parameters.iv == NULL) {
    LOGE("CdmEngine::Decrypt: no iv");
    return INVALID_DECRYPT_PARAMETERS_ENG_3;
  }

  if (parameters.decrypt_buffer == NULL) {
    if (!parameters.is_secure &&
        !Properties::Properties::oem_crypto_use_fifo()) {
      LOGE("CdmEngine::Decrypt: no dest decrypt buffer");
      return INVALID_DECRYPT_PARAMETERS_ENG_4;
    }
    // else we must be level 1 direct and we don't need to return a buffer.
  }

  std::unique_lock<std::recursive_mutex> lock(session_map_lock_);
  std::shared_ptr<CdmSession> session;
  if (session_id.empty()) {
    CdmSessionList sessions;
    session_map_.GetSessionList(sessions);

    // Loop through the sessions to find the session containing the key_id
    // with the longest remaining license validity.
    int64_t seconds_remaining = 0;
    for (CdmSessionList::iterator iter = sessions.begin();
         iter != sessions.end(); ++iter) {
      if ((*iter)->IsKeyLoaded(*parameters.key_id)) {
        int64_t duration = (*iter)->GetDurationRemaining();
        if (duration > seconds_remaining) {
          session = *iter;
          seconds_remaining = duration;
        }
      }
    }
    if (session.get() == NULL) {
      LOGE("CdmEngine::Decrypt: session not found: Empty session ID");
      return SESSION_NOT_FOUND_FOR_DECRYPT;
    }
  } else {
    if (!session_map_.FindSession(session_id, &session)) {
      LOGE("CdmEngine::Decrypt: session not found: id=%s, id size=%d",
           session_id.c_str(), session_id.size());
      return SESSION_NOT_FOUND_FOR_DECRYPT;
    }
  }

  return session->Decrypt(parameters);
}

CdmResponseType CdmEngine::GenericEncrypt(
      const std::string& session_id, const std::string& in_buffer,
      const std::string& key_id, const std::string& iv,
      CdmEncryptionAlgorithm algorithm, std::string* out_buffer) {
  if (out_buffer == NULL) {
    LOGE("CdmEngine::GenericEncrypt: no out_buffer provided");
    return PARAMETER_NULL;
  }
  std::shared_ptr<CdmSession> session;
  if (!session_map_.FindSession(session_id, &session)) {
    LOGE("CdmEngine::GenericEncrypt: session_id not found = %s ",
         session_id.c_str());
    return SESSION_NOT_FOUND_13;
  }
  return session->GenericEncrypt(in_buffer, key_id, iv, algorithm, out_buffer);
}

CdmResponseType CdmEngine::GenericDecrypt(
      const std::string& session_id, const std::string& in_buffer,
      const std::string& key_id, const std::string& iv,
      CdmEncryptionAlgorithm algorithm,
      std::string* out_buffer) {
  if (out_buffer == NULL) {
    LOGE("CdmEngine::GenericDecrypt: no out_buffer provided");
    return PARAMETER_NULL;
  }
  std::shared_ptr<CdmSession> session;
  if (!session_map_.FindSession(session_id, &session)) {
    LOGE("CdmEngine::GenericDecrypt: session_id not found = %s ",
         session_id.c_str());
    return SESSION_NOT_FOUND_14;
  }
  return session->GenericDecrypt(in_buffer, key_id, iv, algorithm, out_buffer);
}

CdmResponseType CdmEngine::GenericSign(
    const std::string& session_id, const std::string& message,
    const std::string& key_id, CdmSigningAlgorithm algorithm,
    std::string* signature) {
  if (signature == NULL) {
    LOGE("CdmEngine::GenericSign: no signature buffer provided");
    return PARAMETER_NULL;
  }
  std::shared_ptr<CdmSession> session;
  if (!session_map_.FindSession(session_id, &session)) {
    LOGE("CdmEngine::GenericSign: session_id not found = %s ",
         session_id.c_str());
    return SESSION_NOT_FOUND_15;
  }
  return session->GenericSign(message, key_id, algorithm, signature);
}

CdmResponseType CdmEngine::GenericVerify(
    const std::string& session_id, const std::string& message,
    const std::string& key_id, CdmSigningAlgorithm algorithm,
    const std::string& signature) {
  std::shared_ptr<CdmSession> session;
  if (!session_map_.FindSession(session_id, &session)) {
    LOGE("CdmEngine::GenericVerify: session_id not found = %s ",
         session_id.c_str());
    return SESSION_NOT_FOUND_16;
  }
  return session->GenericVerify(message, key_id, algorithm, signature);
}

CdmResponseType CdmEngine::ParseDecryptHashString(
    const std::string& hash_string,
    CdmSessionId* session_id,
    uint32_t* frame_number,
    std::string* hash) {
  if (session_id == nullptr) {
    LOGE("CdmEngine::ParseDecryptHashString: |session_id| was not provided");
    return PARAMETER_NULL;
  }
  if (frame_number == nullptr) {
    LOGE("CdmEngine::ParseDecryptHashString: |frame_number| was not provided");
    return PARAMETER_NULL;
  }
  if (hash == nullptr) {
    LOGE("CdmEngine::ParseDecryptHashString: |hash| was not provided");
    return PARAMETER_NULL;
  }
  std::stringstream ss;
  std::string token;
  std::vector<std::string> tokens;
  ss.str(hash_string);
  while (getline(ss, token, ',')) {
    tokens.push_back(token);
  }
  if (tokens.size() != 3) {
    LOGE("CdmEngine::ParseDecryptHashString: |hash_string| has invalid format, "
         "unexpected number of tokens: %d (%s)",
         tokens.size(), hash_string.c_str());
    return INVALID_DECRYPT_HASH_FORMAT;
  }

  for (size_t i = 0; i < tokens.size(); ++i) {
    if (tokens[i].empty()) {
      LOGE("CdmEngine::ParseDecryptHashString: |hash_string| has invalid "
           "format, token %d of length 0: %s", i, hash_string.c_str());
      return INVALID_DECRYPT_HASH_FORMAT;
    }
  }

  *session_id = tokens[0];
  std::istringstream iss(tokens[1]);
  if (!(iss >> *frame_number)) {
    LOGE("CdmEngine::ParseDecryptHashString: error while trying to convert "
         "frame number to a numeric format: %s", hash_string.c_str());
    return INVALID_DECRYPT_HASH_FORMAT;
  }

  std::vector<uint8_t> hash_vec = wvcdm::a2b_hex(tokens[2]);
  if (hash_vec.empty()) {
    LOGE("CdmEngine::ParseDecryptHashString: malformed hash: %s",
         hash_string.c_str());
    return INVALID_DECRYPT_HASH_FORMAT;
  }
  hash->assign(hash_vec.begin(), hash_vec.end());
  return NO_ERROR;
}

CdmResponseType CdmEngine::SetDecryptHash(
    const CdmSessionId& session_id,
    uint32_t frame_number,
    const std::string& hash) {
  LOGI("CdmEngine::SetDecryptHash: %s", session_id.c_str());
  std::shared_ptr<CdmSession> session;
  if (!session_map_.FindSession(session_id, &session)) {
    return SESSION_NOT_FOUND_20;
  }
  return session->SetDecryptHash(frame_number, hash);
}

CdmResponseType CdmEngine::GetDecryptHashError(
    const CdmSessionId& session_id,
    std::string* error_string) {
  LOGI("CdmEngine::GetDecryptHashError: %s", session_id.c_str());
  std::shared_ptr<CdmSession> session;
  if (!session_map_.FindSession(session_id, &session)) {
    return SESSION_NOT_FOUND_20;
  }
  return session->GetDecryptHashError(error_string);
}

// TODO(gmorgan) Used? Delete if unused.
bool CdmEngine::IsKeyLoaded(const KeyId& key_id) {
  CdmSessionList sessions;
  session_map_.GetSessionList(sessions);
  for (CdmSessionList::iterator iter = sessions.begin();
       iter != sessions.end(); ++iter) {
    if ((*iter)->IsKeyLoaded(key_id)) {
      return true;
    }
  }
  return false;
}

bool CdmEngine::FindSessionForKey(const KeyId& key_id,
                                  CdmSessionId* session_id) {
  if (NULL == session_id) {
    LOGE("CdmEngine::FindSessionForKey: session id not provided");
    return false;
  }

  uint32_t session_sharing_id = Properties::GetSessionSharingId(*session_id);

  std::unique_lock<std::recursive_mutex> lock(session_map_lock_);
  CdmSessionList sessions;
  session_map_.GetSessionList(sessions);

  CdmSessionList::iterator session_iter = sessions.end();

  int64_t seconds_remaining = 0;
  for (CdmSessionList::iterator iter = sessions.begin();
       iter != sessions.end(); ++iter) {
    CdmSessionId id = (*iter)->session_id();
    if (Properties::GetSessionSharingId(id) == session_sharing_id) {
      if ((*iter)->IsKeyLoaded(key_id)) {
        int64_t duration = (*iter)->GetDurationRemaining();
        if (duration > seconds_remaining) {
          session_iter = iter;
          seconds_remaining = duration;
        }
      }
    }
  }

  if (session_iter != sessions.end()) {
    *session_id = (*session_iter)->session_id();
    return true;
  }
  return false;
}

bool CdmEngine::NotifyResolution(const CdmSessionId& session_id, uint32_t width,
                                 uint32_t height) {
  std::shared_ptr<CdmSession> session;
  if (session_map_.FindSession(session_id, &session)) {
    session->NotifyResolution(width, height);
    return true;
  }
  return false;
}

bool CdmEngine::ValidateKeySystem(const CdmKeySystem& key_system) {
  return (key_system.find("widevine") != std::string::npos);
}

void CdmEngine::OnTimerEvent() {
  Clock clock;
  uint64_t current_time = clock.GetCurrentTime();

  bool usage_update_period_expired = false;
  if (current_time - last_usage_information_update_time_ >
      kUpdateUsageInformationPeriod) {
    usage_update_period_expired = true;
    last_usage_information_update_time_ = current_time;
  }

  bool is_initial_usage_update = false;
  bool is_usage_update_needed = false;

  {
    std::unique_lock<std::recursive_mutex> lock(session_map_lock_);
    CdmSessionList sessions;
    session_map_.GetSessionList(sessions);

    while (!sessions.empty()) {
      is_initial_usage_update =
          is_initial_usage_update ||
          sessions.front()->is_initial_usage_update();
      is_usage_update_needed =
          is_usage_update_needed || sessions.front()->is_usage_update_needed();

      sessions.front()->OnTimerEvent(usage_update_period_expired);
      sessions.pop_front();
    }

    if (is_usage_update_needed &&
        (usage_update_period_expired || is_initial_usage_update)) {
      bool has_usage_been_updated = false;

      // Session list may have changed. Rebuild.
      session_map_.GetSessionList(sessions);

      for (CdmSessionList::iterator iter = sessions.begin();
           iter != sessions.end(); ++iter) {
        (*iter)->reset_usage_flags();
        switch ((*iter)->get_usage_support_type()) {
          case kUsageEntrySupport:
            if ((*iter)->has_provider_session_token()) {
              (*iter)->UpdateUsageEntryInformation();
            }
            break;
          case kUsageTableSupport:
            if (!has_usage_been_updated) {
              // usage is updated for all sessions so this needs to be
              // called only once per update usage information period
              CdmResponseType status = (*iter)->UpdateUsageTableInformation();
              if (NO_ERROR != status) {
                LOGW("Update usage information failed: %d", status);
              } else {
                has_usage_been_updated = true;
              }
            }
            break;
          default:
            // Ignore
            break;
        }
      }
    }
  }
  CloseExpiredReleaseSessions();
}

void CdmEngine::OnKeyReleaseEvent(const CdmKeySetId& key_set_id) {
  CdmSessionList sessions;
  session_map_.GetSessionList(sessions);

  while (!sessions.empty()) {
    sessions.front()->OnKeyReleaseEvent(key_set_id);
    sessions.pop_front();
  }
}

CdmResponseType CdmEngine::ValidateServiceCertificate(const std::string& cert) {
  ServiceCertificate certificate;
  return certificate.Init(cert);
}

std::string CdmEngine::MapHdcpVersion(
    CryptoSession::HdcpCapability version) {
  switch (version) {
    case HDCP_NONE:
      return QUERY_VALUE_HDCP_NONE;
    case HDCP_V1:
      return QUERY_VALUE_HDCP_V1;
    case HDCP_V2:
      return QUERY_VALUE_HDCP_V2_0;
    case HDCP_V2_1:
      return QUERY_VALUE_HDCP_V2_1;
    case HDCP_V2_2:
      return QUERY_VALUE_HDCP_V2_2;
    case HDCP_V2_3:
      return QUERY_VALUE_HDCP_V2_3;
    case HDCP_NO_DIGITAL_OUTPUT:
      return QUERY_VALUE_HDCP_NO_DIGITAL_OUTPUT;
    default:
      return QUERY_VALUE_HDCP_LEVEL_UNKNOWN;
  }
}

void CdmEngine::CloseExpiredReleaseSessions() {
  int64_t current_time = clock_.GetCurrentTime();

  std::set<CdmSessionId> close_session_set;
  {
    std::unique_lock<std::mutex> lock(release_key_sets_lock_);
    for (CdmReleaseKeySetMap::iterator iter = release_key_sets_.begin();
         iter != release_key_sets_.end();) {
      if (iter->second.second < current_time) {
        close_session_set.insert(iter->second.first);
        release_key_sets_.erase(iter++);
      } else {
        ++iter;
      }
    }
  }

  for (std::set<CdmSessionId>::iterator iter = close_session_set.begin();
       iter != close_session_set.end(); ++iter) {
    CloseSession(*iter);
  }
}

void CdmEngine::DeleteAllUsageReportsUponFactoryReset() {
  std::string device_base_path_level1 = "";
  std::string device_base_path_level3 = "";
  Properties::GetDeviceFilesBasePath(kSecurityLevelL1,
                                     &device_base_path_level1);
  Properties::GetDeviceFilesBasePath(kSecurityLevelL3,
                                     &device_base_path_level3);

  if (!file_system_->Exists(device_base_path_level1) &&
      !file_system_->Exists(device_base_path_level3)) {
    std::unique_ptr<CryptoSession> crypto_session(
        CryptoSession::MakeCryptoSession(metrics_->GetCryptoMetrics()));
    CdmResponseType status;
    M_TIME(
        status = crypto_session->Open(
            cert_provisioning_requested_security_level_),
        metrics_->GetCryptoMetrics(),
        crypto_session_open_,
        status,
        cert_provisioning_requested_security_level_);
    if (NO_ERROR == status) {
      status = crypto_session->DeleteAllUsageReports();
      metrics_->GetCryptoMetrics()->crypto_session_delete_all_usage_reports_
          .Increment(status);
      if (NO_ERROR != status) {
        LOGW(
            "CdmEngine::DeleteAllUsageReportsUponFactoryReset: "
            "Fails to delete usage reports: %d", status);
      }
    } else {
      LOGW(
          "CdmEngine::DeleteAllUsageReportsUponFactoryReset: "
          "Fails to open crypto session: error=%d.\n"
          "Usage reports are not removed after factory reset.", status);
    }
  }
}

}  // namespace wvcdm
