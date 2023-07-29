// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#include "cdm_session.h"

#include <assert.h>
#include <stdlib.h>

#include <iostream>
#include <sstream>

#include "cdm_engine.h"
#include "clock.h"
#include "file_store.h"
#include "log.h"
#include "properties.h"
#include "string_conversions.h"
#include "usage_table_header.h"
#include "wv_cdm_constants.h"
#include "wv_cdm_event_listener.h"

namespace {
const size_t kKeySetIdLength = 14;

// Helper function for setting the error detail value.
template<typename T>
void SetErrorDetail(int* error_detail, T error_code) {
  if (error_detail != nullptr) {
    *error_detail = error_code;
  }
}

}  // namespace

namespace wvcdm {

CdmSession::CdmSession(FileSystem* file_system,
                       std::shared_ptr<metrics::SessionMetrics> metrics)
    : metrics_(metrics),
      initialized_(false),
      closed_(true),
      file_handle_(new DeviceFiles(file_system)),
      license_received_(false),
      is_offline_(false),
      is_release_(false),
      is_temporary_(false),
      security_level_(kSecurityLevelUninitialized),
      requested_security_level_(kLevelDefault),
      is_initial_decryption_(true),
      has_decrypted_since_last_report_(false),
      is_initial_usage_update_(true),
      is_usage_update_needed_(false),
      usage_support_type_(kNonSecureUsageSupport),
      usage_table_header_(NULL),
      usage_entry_number_(0),
      mock_license_parser_in_use_(false),
      mock_policy_engine_in_use_(false) {
  assert(metrics_.get());  // metrics_ must not be null.
  crypto_metrics_ = metrics_->GetCryptoMetrics();
  crypto_session_.reset(CryptoSession::MakeCryptoSession(crypto_metrics_));
  life_span_.Start();
}

CdmSession::~CdmSession() {
  if (usage_support_type_ == kUsageEntrySupport &&
      has_provider_session_token() &&
      usage_table_header_ != NULL &&
      !is_release_) {
    UpdateUsageEntryInformation();
  }

  if (!key_set_id_.empty()) {
    // Unreserve the license ID.
    file_handle_->UnreserveLicenseId(key_set_id_);
  }
  Properties::RemoveSessionPropertySet(session_id_);

  if (metrics_.get()) {
    M_RECORD(metrics_.get(), cdm_session_life_span_, life_span_.AsMs());
    metrics_->SetCompleted();
  }
}

CdmResponseType CdmSession::Init(
    CdmClientPropertySet* cdm_client_property_set) {
  return Init(cdm_client_property_set, NULL, NULL);
}

CdmResponseType CdmSession::Init(CdmClientPropertySet* cdm_client_property_set,
                                 const CdmSessionId* forced_session_id,
                                 WvCdmEventListener* event_listener) {
  if (initialized_) {
    LOGE("CdmSession::Init: Failed due to previous initialization");
    return REINIT_ERROR;
  }

  if (cdm_client_property_set && cdm_client_property_set->security_level() ==
                                     QUERY_VALUE_SECURITY_LEVEL_L3) {
    requested_security_level_ = kLevel3;
    security_level_ = kSecurityLevelL3;
  }
  CdmResponseType sts;
  M_TIME(sts = crypto_session_->Open(requested_security_level_),
         crypto_metrics_, crypto_session_open_, sts, requested_security_level_);
  if (NO_ERROR != sts) return sts;

  security_level_ = crypto_session_->GetSecurityLevel();
  crypto_metrics_->crypto_session_security_level_.Record(security_level_);
  std::string oemcrypto_build;
  if(crypto_session_->GetBuildInformation(requested_security_level_,
                                          &oemcrypto_build)) {
    metrics_->oemcrypto_build_info_.Record(oemcrypto_build);
  } else {
    metrics_->oemcrypto_build_info_.SetError(false);
  }

  if (!file_handle_->Init(security_level_)) {
    LOGE("CdmSession::Init: Unable to initialize file handle");
    return SESSION_FILE_HANDLE_INIT_ERROR;
  }

  if (crypto_session_->GetUsageSupportType(&usage_support_type_) == NO_ERROR) {
    if (usage_support_type_ == kUsageEntrySupport)
      usage_table_header_ = crypto_session_->GetUsageTableHeader();
  } else {
    usage_support_type_ = kNonSecureUsageSupport;
  }

  // Device Provisioning state is not yet known.
  // If not using certificates, then Keybox is client token for license
  // requests.
  // Otherwise, try to fetch device certificate.  If not successful and
  // provisioning is supported, return NEED_PROVISIONING.  Otherwise, return
  // an error.
  // client_token and client_token_type are determined here; they are needed
  // to initialize the license parser.
  std::string client_token;
  std::string serial_number;
  CdmClientTokenType client_token_type =
      crypto_session_->GetPreProvisionTokenType();

  // License server client ID token is a stored certificate. Stage it or
  // indicate that provisioning is needed. Get token from stored certificate
  std::string wrapped_key;
  if (!file_handle_->RetrieveCertificate(&client_token, &wrapped_key,
                                         &serial_number, NULL)) {
    return NEED_PROVISIONING;
  }
  CdmResponseType load_cert_sts;
  M_TIME(
      load_cert_sts = crypto_session_->LoadCertificatePrivateKey(wrapped_key),
      crypto_metrics_, crypto_session_load_certificate_private_key_,
      load_cert_sts);
  switch (load_cert_sts) {
    case NO_ERROR: break;
    case SESSION_LOST_STATE_ERROR:
    case SYSTEM_INVALIDATED_ERROR:
      return load_cert_sts;
    default:
      return NEED_PROVISIONING;
  }

  client_token_type = kClientTokenDrmCert;

  // Session is provisioned with certificate needed to construct
  // license request (or with keybox).

  if (forced_session_id) {
    key_set_id_ = *forced_session_id;
  } else {
    bool ok = GenerateKeySetId(&key_set_id_);
    (void)ok;  // ok is now used when assertions are turned off.
    assert(ok);
  }

  session_id_ =
      Properties::AlwaysUseKeySetIds() ? key_set_id_ : GenerateSessionId();
  metrics_->SetSessionId(session_id_);

  if (session_id_.empty()) {
    LOGE("CdmSession::Init: empty session ID");
    return EMPTY_SESSION_ID;
  }
  if (cdm_client_property_set)
    Properties::AddSessionPropertySet(session_id_, cdm_client_property_set);

  if (!mock_license_parser_in_use_)
    license_parser_.reset(new CdmLicense(session_id_));
  if (!mock_policy_engine_in_use_)
    policy_engine_.reset(
        new PolicyEngine(session_id_, event_listener, crypto_session_.get()));

  std::string service_certificate;
  if (!Properties::GetServiceCertificate(session_id_, &service_certificate))
    service_certificate.clear();

  if (!license_parser_->Init(client_token, client_token_type, serial_number,
                             Properties::UsePrivacyMode(session_id_),
                             service_certificate, crypto_session_.get(),
                             policy_engine_.get()))
    return LICENSE_PARSER_INIT_ERROR;

  license_received_ = false;
  is_initial_decryption_ = true;
  initialized_ = true;
  closed_ = false;
  return NO_ERROR;
}

CdmResponseType CdmSession::RestoreOfflineSession(
    const CdmKeySetId& key_set_id, CdmLicenseType license_type,
    int* error_detail) {
  if (!initialized_) {
    LOGE("CdmSession::RestoreOfflineSession: not initialized");
    return NOT_INITIALIZED_ERROR;
  }
  if (!key_set_id_.empty()) {
    file_handle_->UnreserveLicenseId(key_set_id_);
  }
  key_set_id_ = key_set_id;

  DeviceFiles::LicenseState license_state;
  int64_t playback_start_time;
  int64_t last_playback_time;
  int64_t grace_period_end_time;
  DeviceFiles::ResponseType sub_error_code = DeviceFiles::kNoError;

  if (!file_handle_->RetrieveLicense(
          key_set_id, &license_state, &offline_init_data_, &key_request_,
          &key_response_, &offline_key_renewal_request_,
          &offline_key_renewal_response_, &offline_release_server_url_,
          &playback_start_time, &last_playback_time, &grace_period_end_time,
          &app_parameters_, &usage_entry_, &usage_entry_number_,
          &sub_error_code)) {
    LOGE(
        "CdmSession::RestoreOfflineSession: failed to retrieve license. "
        "sub error: %d, key set id = %s",
        sub_error_code, key_set_id.c_str());
    SetErrorDetail(error_detail, sub_error_code);
    return sub_error_code == DeviceFiles::kFileNotFound
      ? KEYSET_ID_NOT_FOUND_4
      : GET_LICENSE_ERROR;
  }

  // TODO(rfrias, juce) b/133684744: Comment out this functionality to address
  // android test failures. These changes were introduced to address
  // b/113167010 (3c23a071e)
  // Attempts to restore a released offline license are treated as a release
  // retry.
  //if (license_state == DeviceFiles::kLicenseStateReleasing) {
  //  license_type = kLicenseTypeRelease;
  //}

  // Only restore offline licenses if they are active or this is a release
  // retry.
  if (!(license_type == kLicenseTypeRelease ||
        license_state == DeviceFiles::kLicenseStateActive)) {
    LOGE(
        "CdmSession::RestoreOfflineSession: invalid offline license state = "
        "%d, type = %d",
        license_state, license_type);
    return GET_RELEASED_LICENSE_ERROR;
  }

  std::string provider_session_token;
  if (usage_support_type_ == kUsageEntrySupport) {
    if (!license_parser_->ExtractProviderSessionToken(
            key_response_, &provider_session_token) ||
        usage_table_header_ == NULL) {
      provider_session_token.clear();
    } else {
      CdmResponseType sts = usage_table_header_->LoadEntry(
          crypto_session_.get(), usage_entry_, usage_entry_number_);
      crypto_metrics_->usage_table_header_load_entry_.Increment(sts);
      if (sts != NO_ERROR) {
        LOGE(
            "CdmSession::RestoreOfflineSession: failed to load usage entry = "
            "%d",
            sts);
        return sts;
      }
    }
  }

  CdmResponseType result;
  if (license_type == kLicenseTypeRelease) {
    result =
        license_parser_->RestoreLicenseForRelease(key_request_, key_response_);

    if (result != NO_ERROR) {
      SetErrorDetail(error_detail, result);
      return RELEASE_LICENSE_ERROR_1;
    }
  } else {
    result = license_parser_->RestoreOfflineLicense(
        key_request_, key_response_, offline_key_renewal_response_,
        playback_start_time, last_playback_time, grace_period_end_time, this);
    if (result != NO_ERROR) {
      SetErrorDetail(error_detail, result);
      return RESTORE_OFFLINE_LICENSE_ERROR_2;
    }
  }

  if (usage_support_type_ == kUsageEntrySupport &&
      !provider_session_token.empty() && usage_table_header_ != NULL) {
    CdmResponseType sts =
        usage_table_header_->UpdateEntry(crypto_session_.get(), &usage_entry_);
    if (sts != NO_ERROR) {
      LOGE(
          "CdmSession::RestoreOfflineSession failed to update usage entry = "
          "%d",
          sts);
      return sts;
    }
    if (!StoreLicense(license_state, error_detail)) {
      LOGW(
          "CdmSession::RestoreUsageSession: unable to save updated usage "
          "info");
    }
  }

  license_received_ = true;
  is_offline_ = true;
  is_release_ = license_type == kLicenseTypeRelease;
  return KEY_ADDED;
}

CdmResponseType CdmSession::RestoreUsageSession(
    const DeviceFiles::CdmUsageData& usage_data, int* error_detail) {
  if (!initialized_) {
    LOGE("CdmSession::RestoreUsageSession: not initialized");
    return NOT_INITIALIZED_ERROR;
  }
  if (!key_set_id_.empty()) {
    file_handle_->UnreserveLicenseId(key_set_id_);
  }
  key_set_id_ = usage_data.key_set_id;
  key_request_ = usage_data.license_request;
  key_response_ = usage_data.license;
  usage_entry_ = usage_data.usage_entry;
  usage_entry_number_ = usage_data.usage_entry_number;
  usage_provider_session_token_ = usage_data.provider_session_token;

  CdmResponseType sts = NO_ERROR;
  if (usage_support_type_ == kUsageEntrySupport &&
      usage_table_header_ != NULL) {
    sts = usage_table_header_->LoadEntry(
        crypto_session_.get(), usage_entry_, usage_entry_number_);
    crypto_metrics_->usage_table_header_load_entry_.Increment(sts);
    if (sts != NO_ERROR) {
      LOGE("CdmSession::RestoreUsageSession: failed to load usage entry = %d",
           sts);
      return sts;
    }
  }

  sts = license_parser_->RestoreLicenseForRelease(key_request_, key_response_);

  if (sts != NO_ERROR) {
    SetErrorDetail(error_detail, sts);
    return RELEASE_LICENSE_ERROR_2;
  }

  if (usage_support_type_ == kUsageEntrySupport &&
      usage_table_header_ != NULL) {
    sts =
        usage_table_header_->UpdateEntry(crypto_session_.get(), &usage_entry_);
    if (sts != NO_ERROR) {
      LOGE("CdmSession::RestoreUsageSession: failed to update usage entry: %d",
           sts);
      return sts;
    }
    if (!UpdateUsageInfo()) {
      LOGW(
          "CdmSession::RestoreUsageSession: unable to save updated usage "
          "info");
    }
  }

  license_received_ = true;
  is_offline_ = false;
  is_release_ = true;
  return KEY_ADDED;
}

// This is a thin wrapper that initiates the latency metric.
CdmResponseType CdmSession::GenerateKeyRequest(
    const InitializationData& init_data, CdmLicenseType license_type,
    const CdmAppParameterMap& app_parameters, CdmKeyRequest* key_request) {
  CdmResponseType result = GenerateKeyRequestInternal(
      init_data, license_type, app_parameters, key_request);
  // Note that GenerateReleaseRequest and GenerateRenewalRequest will initialize
  // the timer themselves. This is duplicate because there are duplicate paths
  // for calling GenerateReleaseRequest and GenerateRenewalRequest.
  if (result == KEY_MESSAGE) {
    key_request_type_ = key_request->type;
    license_request_latency_.Start(); // Start or restart timer.
  }
  return result;
}

CdmResponseType CdmSession::GenerateKeyRequestInternal(
    const InitializationData& init_data, CdmLicenseType license_type,
    const CdmAppParameterMap& app_parameters,
    CdmKeyRequest* key_request) {

  if (!initialized_) {
    LOGE("CdmSession::GenerateKeyRequest: not initialized");
    return NOT_INITIALIZED_ERROR;
  }

  if (!key_request) {
    LOGE("CdmSession::GenerateKeyRequest: No output destination provided");
    return PARAMETER_NULL;
  }

  switch (license_type) {
    case kLicenseTypeTemporary:
      is_temporary_ = true;
      break;
    case kLicenseTypeStreaming:
      is_offline_ = false;
      break;
    case kLicenseTypeOffline:
      is_offline_ = true;
      break;
    case kLicenseTypeRelease:
      is_release_ = true;
      break;
    // TODO(b/132071885): Once a license has been received, CdmSession assumes
    // that a call to this method is to generate a license renewal/release
    // or key rotation. Key rotation can be indicated by specifing a license
    // type kLicenseTypeEmbeddedKeyData or interrogating the PSSH
    // (See "else if (license_received_)" below). b/132071885 is to evaluate
    // whether both mechanisms are needed.
    case kLicenseTypeEmbeddedKeyData:
      return license_parser_->HandleEmbeddedKeyData(init_data);
    default:
      LOGE("CdmSession::GenerateKeyRequest: unrecognized license type: %ld",
           license_type);
      return INVALID_LICENSE_TYPE;
  }

  if (is_release_) {
    return GenerateReleaseRequest(key_request);
  } else if (license_received_) {
    // A call to GenerateKeyRequest after the initial license has been received
    // is either a renewal/release request or a key rotation event
    if (init_data.contains_entitled_keys()) {
      key_request->message.clear();
      key_request->type = kKeyRequestTypeNone;
      key_request->url.clear();
      return license_parser_->HandleEmbeddedKeyData(init_data);
    } else {
      return GenerateRenewalRequest(key_request);
    }
  } else {
    key_request->type = kKeyRequestTypeInitial;

    if (!init_data.is_supported()) {
      LOGW("CdmSession::GenerateKeyRequest: unsupported init data type (%s)",
           init_data.type().c_str());
      return UNSUPPORTED_INIT_DATA;
    }
    if (init_data.IsEmpty() && !license_parser_->HasInitData()) {
      LOGW("CdmSession::GenerateKeyRequest: init data absent");
      return INIT_DATA_NOT_FOUND;
    }
    if (is_offline_ && key_set_id_.empty()) {
      LOGE("CdmSession::GenerateKeyRequest: Unable to generate key set ID");
      return KEY_REQUEST_ERROR_1;
    }

    app_parameters_ = app_parameters;
    CdmResponseType status = license_parser_->PrepareKeyRequest(
        init_data, license_type, app_parameters, &key_request->message,
        &key_request->url);
    if (status != KEY_MESSAGE) return status;

    key_request_ = key_request->message;
    if (is_offline_) {
      offline_init_data_ = init_data.data();
      offline_release_server_url_ = key_request->url;
    }
    return KEY_MESSAGE;
  }
}

// This thin wrapper allows us to update metrics.
CdmResponseType CdmSession::AddKey(const CdmKeyResponse& key_response) {
  CdmResponseType sts = AddKeyInternal(key_response);
  UpdateRequestLatencyTiming(sts);
  return sts;
}

// AddKeyInternal() - Accept license response and extract key info.
CdmResponseType CdmSession::AddKeyInternal(const CdmKeyResponse& key_response) {
  if (!initialized_) {
    LOGE("CdmSession::AddKey: not initialized");
    return NOT_INITIALIZED_ERROR;
  }

  if (is_release_) {
    CdmResponseType sts = ReleaseKey(key_response);
    return (sts == NO_ERROR) ? KEY_ADDED : sts;
  } else if (license_received_) {  // renewal
    return RenewKey(key_response);
  } else {
    // If usage table header+entries are supported, preprocess the license
    // to see if it has a provider session token. If so a new entry needs
    // to be created.
    CdmResponseType sts;
    std::string provider_session_token;
    if (usage_support_type_ == kUsageEntrySupport &&
        usage_table_header_ != NULL) {
      if (license_parser_->ExtractProviderSessionToken(
              key_response, &provider_session_token) &&
          !provider_session_token.empty()) {
        std::string app_id;
        GetApplicationId(&app_id);
        sts = usage_table_header_->AddEntry(
            crypto_session_.get(), is_offline_, key_set_id_,
            DeviceFiles::GetUsageInfoFileName(app_id), &usage_entry_number_);
        crypto_metrics_->usage_table_header_add_entry_.Increment(sts);
        if (sts != NO_ERROR) return sts;
      }
    }
    sts = license_parser_->HandleKeyResponse(key_response);

    // Update the license sdk and service versions.
    const VersionInfo& version_info = license_parser_->GetServiceVersion();
    metrics_->license_sdk_version_.Record(version_info.license_sdk_version());
    metrics_->license_sdk_version_.Record(version_info.license_service_version());

    // Update or delete entry if usage table header+entries are supported
    if (usage_support_type_ == kUsageEntrySupport &&
        !provider_session_token.empty() && usage_table_header_ != NULL) {
      if (sts != KEY_ADDED) {
        CdmResponseType delete_sts = usage_table_header_->DeleteEntry(
            usage_entry_number_, file_handle_.get(), crypto_metrics_);
        crypto_metrics_->usage_table_header_delete_entry_.Increment(delete_sts);
        if (delete_sts != NO_ERROR) {
          LOGW("CdmSession::AddKey: Delete usage entry failed = %d",
               delete_sts);
        }
      }
    }

    if (sts != KEY_ADDED) return (sts == KEY_ERROR) ? ADD_KEY_ERROR : sts;

    license_received_ = true;
    key_response_ = key_response;

    LOGV("AddKey: provider_session_token (size=%d) =%s",
         license_parser_->provider_session_token().size(),
         license_parser_->provider_session_token().c_str());

    if (is_offline_ || has_provider_session_token()) {
      if (has_provider_session_token() &&
          usage_support_type_ == kUsageEntrySupport &&
          usage_table_header_ != NULL) {
        usage_table_header_->UpdateEntry(crypto_session_.get(), &usage_entry_);
      }

      if (!is_offline_)
        usage_provider_session_token_ =
            license_parser_->provider_session_token();

      sts = StoreLicense();
      if (sts != NO_ERROR) return sts;
    }

    return KEY_ADDED;
  }
}

CdmResponseType CdmSession::QueryStatus(CdmQueryMap* query_response) {
  if (!initialized_) {
    LOGE("CdmSession::QueryStatus: not initialized");
    return NOT_INITIALIZED_ERROR;
  }

  switch (security_level_) {
    case kSecurityLevelL1:
      (*query_response)[QUERY_KEY_SECURITY_LEVEL] =
          QUERY_VALUE_SECURITY_LEVEL_L1;
      break;
    case kSecurityLevelL2:
      (*query_response)[QUERY_KEY_SECURITY_LEVEL] =
          QUERY_VALUE_SECURITY_LEVEL_L2;
      break;
    case kSecurityLevelL3:
      (*query_response)[QUERY_KEY_SECURITY_LEVEL] =
          QUERY_VALUE_SECURITY_LEVEL_L3;
      break;
    case kSecurityLevelUninitialized:
    case kSecurityLevelUnknown:
      (*query_response)[QUERY_KEY_SECURITY_LEVEL] =
          QUERY_VALUE_SECURITY_LEVEL_UNKNOWN;
      break;
    default:
      return INVALID_QUERY_KEY;
  }
  return NO_ERROR;
}

CdmResponseType CdmSession::QueryKeyStatus(CdmQueryMap* query_response) {
  return policy_engine_->Query(query_response);
}

CdmResponseType CdmSession::QueryKeyAllowedUsage(
    const std::string& key_id, CdmKeyAllowedUsage* key_usage) {
  return policy_engine_->QueryKeyAllowedUsage(key_id, key_usage);
}

CdmResponseType CdmSession::QueryOemCryptoSessionId(
    CdmQueryMap* query_response) {
  if (!initialized_) {
    LOGE("CdmSession::QueryOemCryptoSessionId: not initialized");
    return NOT_INITIALIZED_ERROR;
  }

  std::stringstream ss;
  ss << crypto_session_->oec_session_id();
  (*query_response)[QUERY_KEY_OEMCRYPTO_SESSION_ID] = ss.str();
  return NO_ERROR;
}

// Decrypt() - Accept encrypted buffer and return decrypted data.
CdmResponseType CdmSession::Decrypt(const CdmDecryptionParameters& params) {
  if (!initialized_) {
    return NOT_INITIALIZED_ERROR;
  }

  // Encrypted playback may not begin until either the start time passes or the
  // license is updated, so we treat this Decrypt call as invalid.
  // For the clear lead, we allow playback even if the key_id is not found or if
  // the security level is not high enough yet.
  if (params.is_encrypted) {
    if (!policy_engine_->CanDecryptContent(*params.key_id)) {
      if (policy_engine_->IsLicenseForFuture()) return DECRYPT_NOT_READY;
      if (!policy_engine_->IsSufficientOutputProtection(*params.key_id))
        return INSUFFICIENT_OUTPUT_PROTECTION;
      return NEED_KEY;
    }
    if (!policy_engine_->CanUseKeyForSecurityLevel(*params.key_id)) {
      return KEY_PROHIBITED_FOR_SECURITY_LEVEL;
    }
  }

  CdmResponseType status = crypto_session_->Decrypt(params);

  if (status == NO_ERROR) {
    if (is_initial_decryption_) {
      is_initial_decryption_ = !policy_engine_->BeginDecryption();
    }
    has_decrypted_since_last_report_ = true;
    if (!is_usage_update_needed_) {
      is_usage_update_needed_ = has_provider_session_token();
    }
  } else {
    Clock clock;
    int64_t current_time = clock.GetCurrentTime();
    if (policy_engine_->HasLicenseOrPlaybackDurationExpired(current_time)) {
      return NEED_KEY;
    }
  }

  return status;
}

// License renewal
// GenerateRenewalRequest() - Construct valid renewal request for the current
// session keys.
CdmResponseType CdmSession::GenerateRenewalRequest(CdmKeyRequest* key_request) {
  if (!initialized_) {
    LOGE("CdmSession::GenerateRenewalRequest: not initialized");
    return NOT_INITIALIZED_ERROR;
  }
  CdmResponseType status = license_parser_->PrepareKeyUpdateRequest(
      true, app_parameters_, NULL, &key_request->message, &key_request->url);

  key_request->type = kKeyRequestTypeRenewal;

  if (KEY_MESSAGE != status) return status;

  if (is_offline_) {
    offline_key_renewal_request_ = key_request->message;
  }
  key_request_type_ = key_request->type;
  license_request_latency_.Start(); // Start or restart timer.
  return KEY_MESSAGE;
}

// RenewKey() - Accept renewal response and update key info.
CdmResponseType CdmSession::RenewKey(const CdmKeyResponse& key_response) {
  if (!initialized_) {
    LOGE("CdmSession::RenewKey: not initialized");
    return NOT_INITIALIZED_ERROR;
  }
  CdmResponseType sts =
      license_parser_->HandleKeyUpdateResponse(true, key_response);

  // Record the timing on success.
  UpdateRequestLatencyTiming(sts);

  if (sts != KEY_ADDED) return (sts == KEY_ERROR) ? RENEW_KEY_ERROR_1 : sts;

  if (is_offline_) {
    offline_key_renewal_response_ = key_response;
    if (!StoreLicense(DeviceFiles::kLicenseStateActive,
                      nullptr /* error_detail */))
      return RENEW_KEY_ERROR_2;
  }
  return KEY_ADDED;
}

CdmResponseType CdmSession::GenerateReleaseRequest(CdmKeyRequest* key_request) {
  if (!initialized_) {
    LOGE("CdmSession::GenerateReleaseRequest: not initialized");
    return NOT_INITIALIZED_ERROR;
  }
  is_release_ = true;
  license_request_latency_.Clear();
  CdmResponseType status = license_parser_->PrepareKeyUpdateRequest(
      false, app_parameters_, usage_table_header_ == NULL ? NULL : this,
      &key_request->message, &key_request->url);

  key_request->type = kKeyRequestTypeRelease;

  if (KEY_MESSAGE != status) return status;

  if (has_provider_session_token() &&
      usage_support_type_ == kUsageEntrySupport) {
    status =
        usage_table_header_->UpdateEntry(crypto_session_.get(), &usage_entry_);

    if (status != NO_ERROR) {
      LOGE(
          "CdmSession::GenerateReleaseRequest: Update usage entry failed = "
          "%d",
          status);
      return status;
    }
  }

  if (is_offline_) {  // Mark license as being released
    if (!StoreLicense(DeviceFiles::kLicenseStateReleasing, nullptr))
      return RELEASE_KEY_REQUEST_ERROR;
  } else if (!usage_provider_session_token_.empty()) {
    if (usage_support_type_ == kUsageEntrySupport) {
      if (!UpdateUsageInfo()) return RELEASE_USAGE_INFO_FAILED;
    }
  }

  key_request_type_ = key_request->type;
  license_request_latency_.Start(); // Start or restart timer.

  return KEY_MESSAGE;
}

// ReleaseKey() - Accept release response and  release license.
CdmResponseType CdmSession::ReleaseKey(const CdmKeyResponse& key_response) {
  if (!initialized_) {
    LOGE("CdmSession::ReleaseKey: not initialized");
    return NOT_INITIALIZED_ERROR;
  }
  CdmResponseType sts =
      license_parser_->HandleKeyUpdateResponse(false, key_response);
  // Record the timing on success.
  UpdateRequestLatencyTiming(sts);

  if (sts != KEY_ADDED) return (sts == KEY_ERROR) ? RELEASE_KEY_ERROR : sts;

  return RemoveLicense();
}

CdmResponseType CdmSession::DeleteUsageEntry(uint32_t usage_entry_number) {
  if (!initialized_) {
    LOGE("CdmSession::DeleteUsageEntry: not initialized");
    return NOT_INITIALIZED_ERROR;
  }
  if (usage_support_type_ != kUsageEntrySupport) {
    LOGE("CdmSession::DeleteUsageEntry: Unexpected usage type supported: %d",
         usage_support_type_);
    return INCORRECT_USAGE_SUPPORT_TYPE_1;
  }

  // The usage entry cannot be deleted if it has a crypto session handling
  // it, so close and reopen session.
  UpdateUsageEntryInformation();
  CdmResponseType sts;
  crypto_session_->Close();
  crypto_session_.reset(CryptoSession::MakeCryptoSession(crypto_metrics_));
  M_TIME(sts = crypto_session_->Open(requested_security_level_),
         crypto_metrics_, crypto_session_open_, sts, requested_security_level_);
  if (sts != NO_ERROR) return sts;

  usage_table_header_ = NULL;
  if (crypto_session_->GetUsageSupportType(&usage_support_type_) == NO_ERROR) {
    if (usage_support_type_ == kUsageEntrySupport)
      usage_table_header_ = crypto_session_->GetUsageTableHeader();
  } else {
    usage_support_type_ = kNonSecureUsageSupport;
  }

  if (usage_table_header_ == NULL) {
    LOGE("CdmSession::DeleteUsageEntry: Usage table header unavailable");
    return INCORRECT_USAGE_SUPPORT_TYPE_1;
  }

  sts = usage_table_header_->DeleteEntry(usage_entry_number,
                                         file_handle_.get(), crypto_metrics_);
  crypto_metrics_->usage_table_header_delete_entry_.Increment(sts);
  return sts;
}

bool CdmSession::IsKeyLoaded(const KeyId& key_id) {
  return license_parser_->IsKeyLoaded(key_id);
}

int64_t CdmSession::GetDurationRemaining() {
  if (policy_engine_->IsLicenseForFuture()) return 0;
  return policy_engine_->GetLicenseOrPlaybackDurationRemaining();
}

CdmSessionId CdmSession::GenerateSessionId() {
  static int session_num = 1;
  return SESSION_ID_PREFIX + IntToString(++session_num);
}

bool CdmSession::GenerateKeySetId(CdmKeySetId* key_set_id) {
  if (!key_set_id) {
    LOGW("CdmSession::GenerateKeySetId: key set id destination not provided");
    return false;
  }

  std::vector<uint8_t> random_data(
      (kKeySetIdLength - sizeof(KEY_SET_ID_PREFIX)) / 2, 0);

  while (key_set_id->empty()) {
    if (crypto_session_->GetRandom(random_data.size(), &random_data[0])
        != NO_ERROR) {
      return false;
    }

    *key_set_id = KEY_SET_ID_PREFIX + b2a_hex(random_data);

    // key set collision
    if (file_handle_->LicenseExists(*key_set_id)) {
      key_set_id->clear();
    }
  }
  // Reserve the license ID to avoid collisions.
  file_handle_->ReserveLicenseId(*key_set_id);
  return true;
}

CdmResponseType CdmSession::StoreLicense() {
  if (is_temporary_) {
    LOGE("CdmSession::StoreLicense: Session type prohibits storage.");
    return STORAGE_PROHIBITED;
  }

  if (is_offline_) {
    if (key_set_id_.empty()) {
      LOGE("CdmSession::StoreLicense: No key set ID");
      return EMPTY_KEYSET_ID;
    }

    if (!license_parser_->is_offline()) {
      LOGE("CdmSession::StoreLicense: License policy prohibits storage.");
      return OFFLINE_LICENSE_PROHIBITED;
    }

    if (!StoreLicense(DeviceFiles::kLicenseStateActive, nullptr)) {
      LOGE("CdmSession::StoreLicense: Unable to store license");
      return STORE_LICENSE_ERROR_1;
    }
    return NO_ERROR;
  }  // if (is_offline_)

  std::string provider_session_token =
      license_parser_->provider_session_token();
  if (provider_session_token.empty()) {
    LOGE("CdmSession::StoreLicense: No provider session token and not offline");
    return STORE_LICENSE_ERROR_2;
  }

  std::string app_id;
  GetApplicationId(&app_id);
  if (!file_handle_->StoreUsageInfo(
          provider_session_token, key_request_, key_response_,
          DeviceFiles::GetUsageInfoFileName(app_id), key_set_id_, usage_entry_,
          usage_entry_number_)) {
    LOGE("CdmSession::StoreLicense: Unable to store usage info");
    // Usage info file is corrupt. Delete current usage entry and file.
    switch (usage_support_type_) {
      case kUsageEntrySupport:
        DeleteUsageEntry(usage_entry_number_);
        break;
      case kUsageTableSupport:
        crypto_session_->DeleteUsageInformation(provider_session_token);
        crypto_session_->UpdateUsageInformation();
        break;
      default:
        LOGW("CdmSession::StoreLicense: unexpected usage support type: %d",
             usage_support_type_);
        break;
    }
    std::vector<std::string> provider_session_tokens;
    file_handle_->DeleteAllUsageInfoForApp(
        DeviceFiles::GetUsageInfoFileName(app_id),
        &provider_session_tokens);

    return STORE_USAGE_INFO_ERROR;
  }
  return NO_ERROR;
}

bool CdmSession::StoreLicense(DeviceFiles::LicenseState state,
                              int* error_detail) {
  DeviceFiles::ResponseType error_detail_alt = DeviceFiles::kNoError;
  bool result = file_handle_->StoreLicense(
      key_set_id_, state, offline_init_data_, key_request_, key_response_,
      offline_key_renewal_request_, offline_key_renewal_response_,
      offline_release_server_url_, policy_engine_->GetPlaybackStartTime(),
      policy_engine_->GetLastPlaybackTime(),
      policy_engine_->GetGracePeriodEndTime(), app_parameters_, usage_entry_,
      usage_entry_number_, &error_detail_alt);
  if (error_detail != nullptr) {
    *error_detail = error_detail_alt;
  }
  return result;
}

CdmResponseType CdmSession::RemoveKeys() {
  CdmResponseType sts;
  crypto_session_.reset(CryptoSession::MakeCryptoSession(crypto_metrics_));
  // Ignore errors
  M_TIME(
      sts = crypto_session_->Open(requested_security_level_),
      crypto_metrics_,
      crypto_session_open_,
      sts,
      requested_security_level_);
  policy_engine_.reset(new PolicyEngine(
        session_id_, NULL, crypto_session_.get()));
  return NO_ERROR;
}

CdmResponseType CdmSession::RemoveLicense() {
  CdmResponseType sts = NO_ERROR;
  if (is_offline_ || has_provider_session_token()) {

    if (usage_support_type_ == kUsageEntrySupport &&
        has_provider_session_token()) {
      sts = DeleteUsageEntry(usage_entry_number_);
    }
    DeleteLicenseFile();
  }
  return NO_ERROR;
}

bool CdmSession::DeleteLicenseFile() {
  if (!is_offline_ && !has_provider_session_token()) return false;

  if (is_offline_) {
    return file_handle_->DeleteLicense(key_set_id_);
  } else {
    std::string app_id;
    GetApplicationId(&app_id);
    return file_handle_->DeleteUsageInfo(
        DeviceFiles::GetUsageInfoFileName(app_id),
        license_parser_->provider_session_token());
  }
}

void CdmSession::NotifyResolution(uint32_t width, uint32_t height) {
  policy_engine_->NotifyResolution(width, height);
}

void CdmSession::OnTimerEvent(bool update_usage) {
  if (update_usage && has_decrypted_since_last_report_) {
    policy_engine_->DecryptionEvent();
    has_decrypted_since_last_report_ = false;
    if (is_offline_ && !is_release_) {
      StoreLicense(DeviceFiles::kLicenseStateActive, nullptr);
    }
  }
  policy_engine_->OnTimerEvent();
}

void CdmSession::OnKeyReleaseEvent(const CdmKeySetId& key_set_id) {
  if (key_set_id_ == key_set_id) {
    policy_engine_->NotifySessionExpiration();
  }
}

void CdmSession::GetApplicationId(std::string* app_id) {
  if (app_id && !Properties::GetApplicationId(session_id_, app_id)) {
    *app_id = "";
  }
}

CdmResponseType CdmSession::DeleteMultipleUsageInformation(
    const std::vector<std::string>& provider_session_tokens) {
  CdmUsageSupportType usage_support_type;
  CdmResponseType sts =
      crypto_session_->GetUsageSupportType(&usage_support_type);
  if (sts == NO_ERROR && usage_support_type == kUsageTableSupport) {
    for (size_t i = 0; i < provider_session_tokens.size(); ++i) {
      crypto_session_->DeactivateUsageInformation(provider_session_tokens[i]);
      UpdateUsageTableInformation();
    }
  }

  if (sts == NO_ERROR) {
    sts = crypto_session_->DeleteMultipleUsageInformation(
        provider_session_tokens);
    crypto_metrics_->crypto_session_delete_multiple_usage_information_
        .Increment(sts);
  }
  return sts;
}

CdmResponseType CdmSession::UpdateUsageTableInformation() {
  CdmUsageSupportType usage_support_type;
  CdmResponseType sts =
      crypto_session_->GetUsageSupportType(&usage_support_type);

  if (sts == NO_ERROR && usage_support_type == kUsageTableSupport) {
    M_TIME(sts = crypto_session_->UpdateUsageInformation(), crypto_metrics_,
           crypto_session_update_usage_information_, sts);
    return sts;
  }

  return NO_ERROR;  // Ignore
}

CdmResponseType CdmSession::UpdateUsageEntryInformation() {
  if (usage_support_type_ != kUsageEntrySupport ||
      !has_provider_session_token() || usage_table_header_ == NULL) {
    LOGE(
        "CdmSession::UpdateUsageEntryInformation: Unexpected state, "
        "usage support type: %d, PST present: %s, usage table header available"
        ": %s",
        usage_support_type_, has_provider_session_token() ? "yes" : "no",
        usage_table_header_ == NULL ? "no" : "yes");
    return INCORRECT_USAGE_SUPPORT_TYPE_2;
  }

  CdmResponseType sts = NO_ERROR;
  // TODO(blueeyes): Add measurements to all UpdateEntry calls in a way that
  // allos us to isolate this particular use case within
  // UpdateUsageEntryInformation.
  M_TIME(sts = usage_table_header_->UpdateEntry(crypto_session_.get(),
                                                &usage_entry_),
         crypto_metrics_, usage_table_header_update_entry_, sts);

  if (sts != NO_ERROR) return sts;

  if (is_offline_)
    StoreLicense(is_release_ ? DeviceFiles::kLicenseStateReleasing
                             : DeviceFiles::kLicenseStateActive, nullptr);
  else if (!usage_provider_session_token_.empty())
    UpdateUsageInfo();

  return NO_ERROR;
}

CdmResponseType CdmSession::GenericEncrypt(const std::string& in_buffer,
                                           const std::string& key_id,
                                           const std::string& iv,
                                           CdmEncryptionAlgorithm algorithm,
                                           std::string* out_buffer) {
  if (!out_buffer) {
    LOGE("CdmSession::GenericEncrypt: No output destination provided");
    return PARAMETER_NULL;
  }
  CdmResponseType sts;
  M_TIME(sts = crypto_session_->GenericEncrypt(in_buffer, key_id, iv, algorithm,
                                               out_buffer),
         crypto_metrics_, crypto_session_generic_encrypt_, sts,
         metrics::Pow2Bucket(in_buffer.size()), algorithm);
  return sts;
}

CdmResponseType CdmSession::GenericDecrypt(const std::string& in_buffer,
                                           const std::string& key_id,
                                           const std::string& iv,
                                           CdmEncryptionAlgorithm algorithm,
                                           std::string* out_buffer) {
  if (!out_buffer) {
    LOGE("CdmSession::GenericDecrypt: No output destination provided");
    return PARAMETER_NULL;
  }
  CdmResponseType sts;
  M_TIME(sts = crypto_session_->GenericDecrypt(in_buffer, key_id, iv, algorithm,
                                               out_buffer),
         crypto_metrics_, crypto_session_generic_decrypt_, sts,
         metrics::Pow2Bucket(in_buffer.size()), algorithm);
  return sts;
}

CdmResponseType CdmSession::GenericSign(const std::string& message,
                                        const std::string& key_id,
                                        CdmSigningAlgorithm algorithm,
                                        std::string* signature) {
  if (!signature) {
    LOGE("CdmSession::GenericSign: No output destination provided");
    return PARAMETER_NULL;
  }
  CdmResponseType sts;
  M_TIME(
      sts = crypto_session_->GenericSign(message, key_id, algorithm, signature),
      crypto_metrics_, crypto_session_generic_sign_, sts,
      metrics::Pow2Bucket(message.size()), algorithm);
  return sts;
}

CdmResponseType CdmSession::GenericVerify(const std::string& message,
                                          const std::string& key_id,
                                          CdmSigningAlgorithm algorithm,
                                          const std::string& signature) {
  CdmResponseType sts;
  M_TIME(sts = crypto_session_->GenericVerify(message, key_id, algorithm,
                                              signature),
         crypto_metrics_, crypto_session_generic_verify_, sts,
         metrics::Pow2Bucket(message.size()), algorithm);
  return sts;
}

CdmResponseType CdmSession::SetDecryptHash(uint32_t frame_number,
                                           const std::string& hash) {
  return crypto_session_->SetDecryptHash(frame_number, hash);
}

CdmResponseType CdmSession::GetDecryptHashError(
    std::string* error_string) {
  return crypto_session_->GetDecryptHashError(error_string);
}

bool CdmSession::UpdateUsageInfo() {
  std::string app_id;
  GetApplicationId(&app_id);

  DeviceFiles::CdmUsageData usage_data;
  usage_data.provider_session_token = usage_provider_session_token_;
  usage_data.license_request = key_request_;
  usage_data.license = key_response_;
  usage_data.key_set_id = key_set_id_;
  usage_data.usage_entry = usage_entry_;
  usage_data.usage_entry_number = usage_entry_number_;

  return file_handle_->UpdateUsageInfo(
      DeviceFiles::GetUsageInfoFileName(app_id), usage_provider_session_token_,
      usage_data);
}

void CdmSession::UpdateRequestLatencyTiming(CdmResponseType sts) {
  if (sts == KEY_ADDED && license_request_latency_.IsStarted()) {
    metrics_->cdm_session_license_request_latency_ms_.Record(
        license_request_latency_.AsMs(), key_request_type_);
  }
  license_request_latency_.Clear();
}

// For testing only - takes ownership of pointers

void CdmSession::set_license_parser(CdmLicense* license_parser) {
  license_parser_.reset(license_parser);
  mock_license_parser_in_use_ = true;
}

void CdmSession::set_crypto_session(CryptoSession* crypto_session) {
  crypto_session_.reset(crypto_session);
}

void CdmSession::set_policy_engine(PolicyEngine* policy_engine) {
  policy_engine_.reset(policy_engine);
  mock_policy_engine_in_use_ = true;
}

void CdmSession::set_file_handle(DeviceFiles* file_handle) {
  file_handle_.reset(file_handle);
}

}  // namespace wvcdm
