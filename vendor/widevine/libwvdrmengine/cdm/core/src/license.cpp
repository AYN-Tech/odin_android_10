// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#include "license.h"

#include <sstream>
#include <vector>

#include "cdm_session.h"
#include "client_identification.h"
#include "clock.h"
#include "crypto_key.h"
#include "crypto_session.h"
#include "device_files.h"
#include "log.h"
#include "platform.h"
#include "policy_engine.h"
#include "privacy_crypto.h"
#include "properties.h"
#include "service_certificate.h"
#include "string_conversions.h"
#include "wv_cdm_constants.h"

namespace {

const uint32_t kFourCcCbc1 = 0x63626331;
const uint32_t kFourCcCbcs = 0x63626373;
const uint32_t kFourCcLittleEndianCbc1 = 0x31636263;
const uint32_t kFourCcLittleEndianCbcs = 0x73636263;
const uint32_t kFourCcCenc = 0x63656e63;

const std::string kEmptyString;

}  // namespace

namespace wvcdm {

// Protobuf generated classes.
using video_widevine::ClientIdentification_ClientCapabilities;
using video_widevine::ClientIdentification_NameValue;
using video_widevine::DrmDeviceCertificate;
using video_widevine::EncryptedClientIdentification;
using video_widevine::License;
using video_widevine::License_KeyContainer;
using video_widevine::LicenseError;
using video_widevine::LicenseIdentification;
using video_widevine::LicenseRequest;
using video_widevine::LicenseRequest_ContentIdentification;
using video_widevine::LicenseRequest_ContentIdentification_CencDeprecated;
using video_widevine::LicenseRequest_ContentIdentification_ExistingLicense;
using video_widevine::LicenseRequest_ContentIdentification_WebmDeprecated;
using video_widevine::SignedDrmDeviceCertificate;
using video_widevine::SignedMessage;

static std::vector<CryptoKey> ExtractEntitlementKeys(const License& license) {
  std::vector<CryptoKey> key_array;

  for (int i = 0; i < license.key_size(); ++i) {
    CryptoKey key;
    size_t length = 0;
    switch (license.key(i).type()) {
      case License_KeyContainer::ENTITLEMENT: {
        // Strip off PKCS#5 padding - since we know the key is 32 or 48 bytes,
        // the padding will always be 16 bytes.
        if (license.key(i).key().size() > 32) {
          length = license.key(i).key().size() - 16;
        } else {
          length = 0;
        }
        key.set_key_data(license.key(i).key().substr(0, length));
        key.set_key_data_iv(license.key(i).iv());
        key.set_key_id(license.key(i).id());
        key.set_track_label(license.key(i).track_label());
        if (license.key(i).has_key_control()) {
          key.set_key_control(license.key(i).key_control().key_control_block());
          key.set_key_control_iv(license.key(i).key_control().iv());
        }
        uint32_t four_cc = kFourCcCenc;
        if (license.has_protection_scheme()) {
          four_cc = license.protection_scheme();
        }
        key.set_track_label(license.key(i).track_label());
        switch (four_cc) {
          // b/30713238: Android N assumed that the "protection scheme" Four
          // CC code, after being extracted from the protobuf, was host byte
          // order dependent. Later versions do not assume this, and thus,
          // for backwards compatibility, must support both byte orders.
          case kFourCcCbc1:
          case kFourCcCbcs:
          case kFourCcLittleEndianCbc1:
          case kFourCcLittleEndianCbcs:
            key.set_cipher_mode(kCipherModeCbc);
            break;
          default:
            key.set_cipher_mode(kCipherModeCtr);
            break;
        }
        key_array.push_back(key);
      } break;

      default:
        // Ignore all but ENTITLEMENT key types.
        break;
    }
  }

  return key_array;
}

static std::vector<CryptoKey> ExtractContentKeys(const License& license) {
  std::vector<CryptoKey> key_array;

  // Extract content key(s)
  for (int i = 0; i < license.key_size(); ++i) {
    CryptoKey key;
    size_t length;
    switch (license.key(i).type()) {
      case License_KeyContainer::CONTENT:
      case License_KeyContainer::OPERATOR_SESSION: {
        key.set_key_id(license.key(i).id());
        // Strip off PKCS#5 padding - since we know the key is 16 or 32 bytes,
        // the padding will always be 16 bytes.
        // TODO(b/111069024): remove this!
        if (license.key(i).key().size() > 16) {
          length = license.key(i).key().size() - 16;
        } else {
          length = 0;
        }
        key.set_key_data(license.key(i).key().substr(0, length));
        key.set_key_data_iv(license.key(i).iv());
        if (license.key(i).has_key_control()) {
          key.set_key_control(license.key(i).key_control().key_control_block());
          key.set_key_control_iv(license.key(i).key_control().iv());
        }
        uint32_t four_cc = kFourCcCenc;
        if (license.has_protection_scheme()) {
          four_cc = license.protection_scheme();
        }
        key.set_track_label(license.key(i).track_label());
        switch (four_cc) {
          // b/30713238: Android N assumed that the "protection scheme" Four
          // CC code, after being extracted from the protobuf, was host byte
          // order dependent. Later versions do not assume this, and thus,
          // for backwards compatibility, must support both byte orders.
          case kFourCcCbc1:
          case kFourCcCbcs:
          case kFourCcLittleEndianCbc1:
          case kFourCcLittleEndianCbcs:
            key.set_cipher_mode(kCipherModeCbc);
            break;
          default:
            key.set_cipher_mode(kCipherModeCtr);
            break;
        }
        key_array.push_back(key);
        break;
      }
      case License_KeyContainer::KEY_CONTROL:
        if (license.key(i).has_key_control()) {
          key.set_key_control(license.key(i).key_control().key_control_block());
          if (license.key(i).key_control().has_iv()) {
            key.set_key_control_iv(license.key(i).key_control().iv());
          }
          key_array.push_back(key);
        }
        break;
      default:
        // Ignore SIGNING and SUB_SESSION key types as they are not content
        // related.
        break;
    }
  }

  return key_array;
}

CdmLicense::CdmLicense(const CdmSessionId& session_id)
    : crypto_session_(NULL),
      policy_engine_(NULL),
      session_id_(session_id),
      initialized_(false),
      renew_with_client_id_(false),
      is_offline_(false),
      use_privacy_mode_(false),
      clock_(new Clock()),
      license_key_type_(kLicenseKeyTypeContent) {}

CdmLicense::CdmLicense(const CdmSessionId& session_id, Clock* clock)
    : crypto_session_(NULL),
      policy_engine_(NULL),
      session_id_(session_id),
      initialized_(false),
      renew_with_client_id_(false),
      is_offline_(false),
      use_privacy_mode_(false),
      license_key_type_(kLicenseKeyTypeContent) {
  clock_.reset(clock);
}

CdmLicense::~CdmLicense() {}

bool CdmLicense::Init(const std::string& client_token,
                      CdmClientTokenType client_token_type,
                      const std::string& device_id, bool use_privacy_mode,
                      const std::string& signed_service_certificate,
                      CryptoSession* session, PolicyEngine* policy_engine) {
  if (clock_.get() == NULL) {
    LOGE("CdmLicense::Init: clock parameter not provided");
    return false;
  }
  if (session_id_.empty()) {
    LOGE("CdmLicense::Init: empty session id provided");
    return false;
  }
  if (client_token.size() == 0) {
    LOGE("CdmLicense::Init: empty client token provided");
    return false;
  }
  if (session == NULL || !session->IsOpen()) {
    LOGE("CdmLicense::Init: crypto session not provided or not open");
    return false;
  }
  if (policy_engine == NULL) {
    LOGE("CdmLicense::Init: no policy engine provided");
    return false;
  }

  if (use_privacy_mode) {
    if (!signed_service_certificate.empty()) {
      if (service_certificate_.Init(signed_service_certificate) != NO_ERROR)
        return false;
    }
    if (!service_certificate_.has_certificate() &&
        !Properties::allow_service_certificate_requests()) {
      LOGE("CdmLicense::Init: Required service certificate not provided");
      return false;
    }
  }

  client_token_ = client_token;
  client_token_type_ = client_token_type;
  device_id_ = device_id;
  crypto_session_ = session;
  policy_engine_ = policy_engine;
  use_privacy_mode_ = use_privacy_mode;
  initialized_ = true;
  return true;
}

CdmResponseType CdmLicense::PrepareKeyRequest(
    const InitializationData& init_data, CdmLicenseType license_type,
    const CdmAppParameterMap& app_parameters, CdmKeyMessage* signed_request,
    std::string* server_url) {
  if (!initialized_) {
    LOGE("CdmLicense::PrepareKeyRequest: not initialized");
    return LICENSE_PARSER_NOT_INITIALIZED_4;
  }
  if (init_data.IsEmpty() && stored_init_data_.get()) {
    InitializationData restored_init_data = *stored_init_data_;
    stored_init_data_.reset();
    return PrepareKeyRequest(restored_init_data, license_type, app_parameters,
                             signed_request, server_url);
  }
  wrapped_keys_ = init_data.ExtractWrappedKeys();
  if (!init_data.is_supported()) {
    LOGE("CdmLicense::PrepareKeyRequest: unsupported init data type (%s)",
         init_data.type().c_str());
    return INVALID_PARAMETERS_LIC_3;
  }
  if (init_data.IsEmpty()) {
    LOGE("CdmLicense::PrepareKeyRequest: empty init data provided");
    return INVALID_PARAMETERS_LIC_4;
  }
  if (!signed_request) {
    LOGE("CdmLicense::PrepareKeyRequest: no signed request provided");
    return INVALID_PARAMETERS_LIC_6;
  }
  if (!server_url) {
    LOGE("CdmLicense::PrepareKeyRequest: no server url provided");
    return INVALID_PARAMETERS_LIC_7;
  }

  // If privacy mode and no service certificate, depending on platform
  // configuration, request service certificate or declare error
  if (use_privacy_mode_ && !service_certificate_.has_certificate()) {
    if (!Properties::allow_service_certificate_requests()) {
      LOGE(
          "CdmLicense::PrepareKeyRequest: failure with privacy mode - "
          "no service certificate.");
      return PRIVACY_MODE_ERROR_1;
    }

    stored_init_data_.reset(new InitializationData(init_data));

    if (!ServiceCertificate::GetRequest(signed_request)) {
      LOGE(
          "CdmLicense::PrepareKeyRequest: failed to prepare a service "
          "certificated request");
      return LICENSE_REQUEST_SERVICE_CERTIFICATE_GENERATION_ERROR;
    }

    return KEY_MESSAGE;
  }

  const std::string& request_id = crypto_session_->request_id();

  LicenseRequest license_request;
  CdmResponseType status;
  status = PrepareClientId(app_parameters, kEmptyString, &license_request);
  if (NO_ERROR != status) return status;

  status =
      PrepareContentId(init_data, license_type, request_id, &license_request);
  if (NO_ERROR != status) return status;

  license_request.set_type(LicenseRequest::NEW);

  license_request.set_request_time(clock_->GetCurrentTime());

  // Get/set the nonce.  This value will be reflected in the Key Control Block
  // of the license response.
  uint32_t nonce;
  status = crypto_session_->GenerateNonce(&nonce);

  switch (status) {
    case NO_ERROR:
      break;
    case SESSION_LOST_STATE_ERROR:
    case SYSTEM_INVALIDATED_ERROR:
      return status;
    default:
      return LICENSE_REQUEST_NONCE_GENERATION_ERROR;
  }
  license_request.set_key_control_nonce(nonce);
  LOGD("PrepareKeyRequest: nonce=%u", nonce);

  license_request.set_protocol_version(video_widevine::VERSION_2_1);

  // License request is complete. Serialize it.
  std::string serialized_license_req;
  license_request.SerializeToString(&serialized_license_req);

  key_request_ = serialized_license_req;

  // Derive signing and encryption keys and construct signature.
  std::string license_request_signature;
  status = crypto_session_->PrepareRequest(serialized_license_req, false,
                                           &license_request_signature);

  if (status != NO_ERROR) {
    signed_request->clear();
    return status;
  }

  if (license_request_signature.empty()) {
    LOGE("CdmLicense::PrepareKeyRequest: License request signature empty");
    signed_request->clear();
    return EMPTY_LICENSE_REQUEST;
  }

  // Put serialized license request and signature together
  SignedMessage signed_message;
  signed_message.set_type(SignedMessage::LICENSE_REQUEST);
  signed_message.set_signature(license_request_signature);
  signed_message.set_msg(serialized_license_req);

  signed_message.SerializeToString(signed_request);

  *server_url = server_url_;
  return KEY_MESSAGE;
}

CdmResponseType CdmLicense::PrepareKeyUpdateRequest(
    bool is_renewal, const CdmAppParameterMap& app_parameters,
    CdmSession* cdm_session, CdmKeyMessage* signed_request,
    std::string* server_url) {
  if (!initialized_) {
    LOGE("CdmLicense::PrepareKeyUpdateRequest: not initialized");
    return LICENSE_PARSER_NOT_INITIALIZED_1;
  }
  if (!signed_request) {
    LOGE("CdmLicense::PrepareKeyUpdateRequest: No signed request provided");
    return INVALID_PARAMETERS_LIC_1;
  }
  if (!server_url) {
    LOGE("CdmLicense::PrepareKeyUpdateRequest: No server url provided");
    return INVALID_PARAMETERS_LIC_2;
  }

  if (is_renewal && !policy_engine_->CanRenew()) {
    LOGE("CdmLicense::PrepareKeyUpdateRequest: license renewal prohibited");
    return LICENSE_RENEWAL_PROHIBITED;
  }

  if (renew_with_client_id_) {
    if (use_privacy_mode_ && !service_certificate_.has_certificate()) {
      LOGE(
          "CdmLicense::PrepareKeyUpdateRequest: failure with privacy mode - "
          "no service certificate.");
      return PRIVACY_MODE_ERROR_2;
    }
  }

  LicenseRequest license_request;
  if (is_renewal)
    license_request.set_type(LicenseRequest::RENEWAL);
  else
    license_request.set_type(LicenseRequest::RELEASE);

  license_request.set_request_time(clock_->GetCurrentTime());

  if (renew_with_client_id_) {
    CdmResponseType status =
        PrepareClientId(app_parameters, provider_client_token_,
                        &license_request);
    if (NO_ERROR != status) return status;
  }

  LicenseRequest_ContentIdentification_ExistingLicense* current_license =
      license_request.mutable_content_id()->mutable_existing_license();
  LicenseIdentification license_id = policy_engine_->license_id();
  current_license->mutable_license_id()->CopyFrom(license_id);

  int64_t seconds_since_started, seconds_since_last_played;
  CryptoSession::UsageDurationStatus usage_duration_status =
      CryptoSession::kUsageDurationsInvalid;
  if (!provider_session_token_.empty()) {
    if (!is_renewal) {
      CdmResponseType status =
          crypto_session_->DeactivateUsageInformation(provider_session_token_);
      if (NO_ERROR != status) return status;
    }

    // TODO(rfrias): Refactor to avoid needing to call CdmSession
    if (cdm_session &&
        cdm_session->get_usage_support_type() == kUsageEntrySupport) {
      CdmResponseType status = cdm_session->UpdateUsageEntryInformation();
      if (NO_ERROR != status) return status;
    }

    std::string usage_report;
    CdmResponseType status = crypto_session_->GenerateUsageReport(
        provider_session_token_, &usage_report, &usage_duration_status,
        &seconds_since_started, &seconds_since_last_played);
    if (!is_renewal) {
      if (NO_ERROR == status)
        current_license->set_session_usage_table_entry(usage_report);
      else
        return GENERATE_USAGE_REPORT_ERROR;
    }
  }

  if (CryptoSession::kUsageDurationsValid != usage_duration_status) {
    if (policy_engine_->GetSecondsSinceStarted(&seconds_since_started) &&
        policy_engine_->GetSecondsSinceLastPlayed(&seconds_since_last_played))
      usage_duration_status = CryptoSession::kUsageDurationsValid;
  }

  if (CryptoSession::kUsageDurationsValid == usage_duration_status) {
    current_license->set_seconds_since_started(seconds_since_started);
    current_license->set_seconds_since_last_played(seconds_since_last_played);
  }

  // Get/set the nonce.  This value will be reflected in the Key Control Block
  // of the license response.
  uint32_t nonce;
  CdmResponseType status = crypto_session_->GenerateNonce(&nonce);

  switch (status) {
    case NO_ERROR:
      break;
    case SESSION_LOST_STATE_ERROR:
    case SYSTEM_INVALIDATED_ERROR:
      return status;
    default:
      return LICENSE_RENEWAL_NONCE_GENERATION_ERROR;
  }
  license_request.set_key_control_nonce(nonce);
  LOGD("PrepareKeyUpdateRequest: nonce=%u", nonce);
  license_request.set_protocol_version(video_widevine::VERSION_2_1);

  // License request is complete. Serialize it.
  std::string serialized_license_req;
  license_request.SerializeToString(&serialized_license_req);

  // Construct signature.
  std::string license_request_signature;
  status = crypto_session_->PrepareRenewalRequest(serialized_license_req,
                                                  &license_request_signature);
  if (status != NO_ERROR) return status;

  if (license_request_signature.empty()) {
    LOGE(
        "CdmLicense::PrepareKeyUpdateRequest: empty license request"
        " signature");
    return EMPTY_LICENSE_RENEWAL;
  }

  // Put serialize license request and signature together
  SignedMessage signed_message;
  signed_message.set_type(SignedMessage::LICENSE_REQUEST);
  signed_message.set_signature(license_request_signature);
  signed_message.set_msg(serialized_license_req);

  signed_message.SerializeToString(signed_request);
  *server_url = server_url_;
  return KEY_MESSAGE;
}

CdmResponseType CdmLicense::HandleKeyResponse(
    const CdmKeyResponse& license_response) {
  if (!initialized_) {
    LOGE("CdmLicense::HandleKeyResponse: not initialized");
    return LICENSE_PARSER_NOT_INITIALIZED_2;
  }
  // Clear the latest service version when we receive a new response.
  latest_service_version_.Clear();

  if (license_response.empty()) {
    LOGE("CdmLicense::HandleKeyResponse: empty license response");
    return EMPTY_LICENSE_RESPONSE_1;
  }

  SignedMessage signed_response;
  if (!signed_response.ParseFromString(license_response)) {
    LOGE(
        "CdmLicense::HandleKeyResponse: unable to parse signed license"
        " response");
    return INVALID_LICENSE_RESPONSE;
  }

  latest_service_version_ = signed_response.service_version_info();

  if (use_privacy_mode_ && Properties::allow_service_certificate_requests() &&
      signed_response.type() == SignedMessage::SERVICE_CERTIFICATE) {
    std::string signed_certificate;
    CdmResponseType status = ServiceCertificate::ParseResponse(
        license_response, &signed_certificate);
    if (status != NO_ERROR) return status;

    status = service_certificate_.Init(signed_certificate);
    return (status == NO_ERROR) ? NEED_KEY : status;
  }

  if (signed_response.type() == SignedMessage::ERROR_RESPONSE)
    return HandleKeyErrorResponse(signed_response);

  if (signed_response.type() != SignedMessage::LICENSE) {
    LOGE("CdmLicense::HandleKeyResponse: unrecognized signed message type: %d",
         signed_response.type());
    return INVALID_LICENSE_TYPE;
  }

  if (!signed_response.has_signature()) {
    LOGE("CdmLicense::HandleKeyResponse: license response is not signed");
    return LICENSE_RESPONSE_NOT_SIGNED;
  }

  License license;
  if (!license.ParseFromString(signed_response.msg())) {
    LOGE("CdmLicense::HandleKeyResponse: unable to parse license response");
    return LICENSE_RESPONSE_PARSE_ERROR_1;
  }

  if (!signed_response.has_session_key()) {
    LOGE("CdmLicense::HandleKeyResponse: no session keys present");
    return SESSION_KEYS_NOT_FOUND;
  }
  CdmResponseType status =
      crypto_session_->GenerateDerivedKeys(key_request_,
                                           signed_response.session_key());

  if (status != NO_ERROR) return status;

  // Extract mac key
  std::string mac_key_iv;
  std::string mac_keys;
  for (int i = 0; i < license.key_size(); ++i) {
    if (license.key(i).type() == License_KeyContainer::SIGNING) {
      mac_key_iv.assign(license.key(i).iv());

      // Strip off PKCS#5 padding
      mac_keys.assign(license.key(i).key().data(), 2 * MAC_KEY_SIZE);
    }
  }
  if (license.policy().can_renew() ||
      (mac_key_iv.size() != 0 || mac_keys.size() != 0)) {
    if (mac_key_iv.size() != KEY_IV_SIZE ||
        mac_keys.size() != 2 * MAC_KEY_SIZE) {
      LOGE(
          "CdmLicense::HandleKeyResponse: mac key/iv size error"
          "(key/iv size expected: %d/%d, actual: %d/%d",
          2 * MAC_KEY_SIZE, KEY_IV_SIZE, mac_keys.size(), mac_key_iv.size());
      return KEY_SIZE_ERROR_1;
    }
  }

  CdmLicenseKeyType key_type = kLicenseKeyTypeEntitlement;
  std::vector<CryptoKey> key_array = ExtractEntitlementKeys(license);
  if (key_array.empty()) {
    key_array = ExtractContentKeys(license);
    key_type = kLicenseKeyTypeContent;
  }
  if (key_array.empty()) {
    LOGE("CdmLicense::HandleKeyResponse : No content keys.");
    return NO_CONTENT_KEY;
  }
  license_key_type_ = key_type;

  if (license.has_provider_client_token())
    provider_client_token_ = license.provider_client_token();

  if (license.has_srm_update()) {
    status = crypto_session_->LoadSrm(license.srm_update());
    switch (status) {
      case NO_ERROR:
        break;
      case SYSTEM_INVALIDATED_ERROR:
        return status;
      default:
        break;  // Ignore
    }
  }

  if (license.id().type() == video_widevine::OFFLINE &&
      license.policy().can_persist())
    is_offline_ = true;

  if (license.id().has_provider_session_token())
    provider_session_token_ = license.id().provider_session_token();

  LOGV("provider_session_token: %s", provider_session_token_.empty()
                                         ? "N/A"
                                         : provider_session_token_.c_str());

  if (license.policy().has_renewal_server_url()) {
    server_url_ = license.policy().renewal_server_url();
  }

  if (license.policy().has_always_include_client_id()) {
    renew_with_client_id_ = license.policy().always_include_client_id();
  }

  CdmResponseType resp = NO_CONTENT_KEY;
  if (kLicenseKeyTypeEntitlement == key_type) {
    resp = HandleEntitlementKeyResponse(signed_response.msg(),
                                        signed_response.signature(), mac_key_iv,
                                        mac_keys, key_array, license);
  } else if (kLicenseKeyTypeContent == key_type) {
    resp = HandleContentKeyResponse(signed_response.msg(),
                                    signed_response.signature(), mac_key_iv,
                                    mac_keys, key_array, license);
  }
  return resp;
}

CdmResponseType CdmLicense::HandleKeyUpdateResponse(
    bool is_renewal, const CdmKeyResponse& license_response) {
  if (!initialized_) {
    LOGE("CdmLicense::HandleKeyUpdateResponse: not initialized");
    return LICENSE_PARSER_NOT_INITIALIZED_3;
  }
  if (license_response.empty()) {
    LOGE("CdmLicense::HandleKeyUpdateResponse : Empty license response.");
    return EMPTY_LICENSE_RESPONSE_2;
  }

  SignedMessage signed_response;
  if (!signed_response.ParseFromString(license_response)) {
    LOGE(
        "CdmLicense::HandleKeyUpdateResponse: Unable to parse signed "
        "message");
    return LICENSE_RESPONSE_PARSE_ERROR_2;
  }

  switch (signed_response.type()) {
    case SignedMessage::LICENSE:
      break;
    case SignedMessage::ERROR_RESPONSE:
      return HandleKeyErrorResponse(signed_response);
    default:
      LOGE(
          "CdmLicense::HandleKeyUpdateResponse: unrecognized signed message "
          "type: %d",
          signed_response.type());
      return INVALID_LICENSE_TYPE;
  }

  if (!signed_response.has_signature()) {
    LOGE("CdmLicense::HandleKeyUpdateResponse: signature missing");
    return SIGNATURE_NOT_FOUND;
  }

  License license;
  if (!license.ParseFromString(signed_response.msg())) {
    LOGE(
        "CdmLicense::HandleKeyUpdateResponse: Unable to parse license"
        " from signed message");
    return LICENSE_RESPONSE_PARSE_ERROR_3;
  }

  if (!license.has_id()) {
    LOGE("CdmLicense::HandleKeyUpdateResponse: license id not present");
    return LICENSE_ID_NOT_FOUND;
  }

  if (license.policy().has_always_include_client_id()) {
    renew_with_client_id_ = license.policy().always_include_client_id();
  }

  if (!is_renewal) {
    if (!license.id().has_provider_session_token()) return KEY_ADDED;

    provider_session_token_ = license.id().provider_session_token();
    CdmResponseType status = crypto_session_->ReleaseUsageInformation(
        signed_response.msg(), signed_response.signature(),
        provider_session_token_);
    return (NO_ERROR == status) ? KEY_ADDED : status;
  }

  if (license.policy().has_renewal_server_url() &&
      license.policy().renewal_server_url().size() > 0) {
    server_url_ = license.policy().renewal_server_url();
  }

  std::vector<CryptoKey> key_array = ExtractContentKeys(license);

  CdmResponseType status =
      crypto_session_->RefreshKeys(signed_response.msg(),
                                   signed_response.signature(),
                                   key_array.size(), &key_array[0]);

  if (status == KEY_ADDED) {
    policy_engine_->UpdateLicense(license);
  }

  return status;
}

CdmResponseType CdmLicense::HandleEmbeddedKeyData(
    const InitializationData& init_data) {
  return HandleNewEntitledKeys(init_data.ExtractWrappedKeys());
}

CdmResponseType CdmLicense::RestoreOfflineLicense(
    const CdmKeyMessage& license_request,
    const CdmKeyResponse& license_response,
    const CdmKeyResponse& license_renewal_response, int64_t playback_start_time,
    int64_t last_playback_time, int64_t grace_period_end_time,
    CdmSession* cdm_session) {
  if (license_request.empty()) {
    LOGE("CdmLicense::RestoreOfflineLicense: empty |license_request|");
    return EMPTY_LICENSE_REQUEST_2;
  }

  if (license_response.empty()) {
    LOGE("CdmLicense::RestoreOfflineLicense: empty |license_response|");
    return EMPTY_LICENSE_RESPONSE_3;
  }

  SignedMessage signed_request;
  if (!signed_request.ParseFromString(license_request)) {
    LOGE("CdmLicense::RestoreOfflineLicense: |license_request| parse failed");
    return PARSE_REQUEST_ERROR_1;
  }

  if (signed_request.type() != SignedMessage::LICENSE_REQUEST) {
    LOGE(
        "CdmLicense::RestoreOfflineLicense: license request type: expected = "
        "%d, actual = %d",
        SignedMessage::LICENSE_REQUEST, signed_request.type());
    return INVALID_LICENSE_REQUEST_TYPE_1;
  }

  key_request_ = signed_request.msg();

  CdmResponseType sts = HandleKeyResponse(license_response);

  if (sts != KEY_ADDED) return sts;

  if (!license_renewal_response.empty()) {
    sts = HandleKeyUpdateResponse(true, license_renewal_response);

    if (sts != KEY_ADDED) return sts;
  }

  if (!provider_session_token_.empty()) {
    if (cdm_session &&
        cdm_session->get_usage_support_type() == kUsageEntrySupport) {
      CdmResponseType status = cdm_session->UpdateUsageEntryInformation();
      if (NO_ERROR != status) return sts;
    }

    std::string usage_report;
    CryptoSession::UsageDurationStatus usage_duration_status =
        CryptoSession::kUsageDurationsInvalid;

    int64_t seconds_since_started, seconds_since_last_played;
    sts = crypto_session_->GenerateUsageReport(
        provider_session_token_, &usage_report, &usage_duration_status,
        &seconds_since_started, &seconds_since_last_played);

    if (NO_ERROR == sts) {
      switch (usage_duration_status) {
        case CryptoSession::kUsageDurationPlaybackNotBegun:
          playback_start_time = 0;
          last_playback_time = 0;
          break;
        case CryptoSession::kUsageDurationsValid: {
          int64_t current_time = clock_->GetCurrentTime();
          if (current_time - seconds_since_started > 0)
            playback_start_time = current_time - seconds_since_started;
          if (current_time - last_playback_time > 0)
            last_playback_time = current_time - seconds_since_last_played;
          break;
        }
        default:
          break;
      }
    }
  }

  policy_engine_->RestorePlaybackTimes(playback_start_time, last_playback_time,
                                       grace_period_end_time);
  return NO_ERROR;
}

CdmResponseType CdmLicense::RestoreLicenseForRelease(
    const CdmKeyMessage& license_request,
    const CdmKeyResponse& license_response) {
  if (license_request.empty()) {
    LOGE("CdmLicense::RestoreLicenseForRelease: empty |license_request|");
    return EMPTY_LICENSE_REQUEST_3;
  }

  if (license_response.empty()) {
    LOGE("CdmLicense::RestoreLicenseForRelease: empty |license_response|");
    return EMPTY_LICENSE_RESPONSE_4;
  }

  SignedMessage signed_request;
  if (!signed_request.ParseFromString(license_request)) {
    LOGE("CdmLicense::RestoreLicenseForRelease: license_request parse failed");
    return PARSE_REQUEST_ERROR_2;
  }

  if (signed_request.type() != SignedMessage::LICENSE_REQUEST) {
    LOGE(
        "CdmLicense::RestoreLicenseForRelease: license request type: "
        "expected "
        "= %d, actual = %d",
        SignedMessage::LICENSE_REQUEST, signed_request.type());
    return INVALID_LICENSE_REQUEST_TYPE_2;
  }

  key_request_ = signed_request.msg();

  SignedMessage signed_response;
  if (!signed_response.ParseFromString(license_response)) {
    LOGE(
        "CdmLicense::RestoreLicenseForRelease: unable to parse signed license"
        " response");
    return LICENSE_RESPONSE_PARSE_ERROR_4;
  }

  if (SignedMessage::LICENSE != signed_response.type()) {
    LOGE(
        "CdmLicense::RestoreLicenseForRelease: unrecognized signed message "
        "type: %d",
        signed_response.type());
    return INVALID_LICENSE_TYPE_2;
  }

  if (!signed_response.has_signature()) {
    LOGE(
        "CdmLicense::RestoreLicenseForRelease: license response is not"
        " signed");
    return SIGNATURE_NOT_FOUND_2;
  }

  License license;
  if (!license.ParseFromString(signed_response.msg())) {
    LOGE(
        "CdmLicense::RestoreLicenseForRelease: unable to parse license"
        " response");
    return LICENSE_RESPONSE_PARSE_ERROR_5;
  }

  if (license.has_provider_client_token())
    provider_client_token_ = license.provider_client_token();

  if (license.id().has_provider_session_token())
    provider_session_token_ = license.id().provider_session_token();

  if (license.policy().has_always_include_client_id())
    renew_with_client_id_ = license.policy().always_include_client_id();

  if (!signed_response.has_session_key()) {
    LOGE("CdmLicense::RestoreLicenseForRelease: no session keys present");
    return SESSION_KEYS_NOT_FOUND_2;
  }

  if (!license.id().has_provider_session_token()) {
    CdmResponseType result = HandleKeyResponse(license_response);
    return result == KEY_ADDED ? NO_ERROR : result;
  }

  if (license.policy().has_renewal_server_url())
    server_url_ = license.policy().renewal_server_url();

  // If the policy engine already has keys, they will now expire.
  // If the policy engine does not already have keys, this will not add any.
  policy_engine_->SetLicenseForRelease(license);
  return NO_ERROR;
}

bool CdmLicense::IsKeyLoaded(const KeyId& key_id) {
  return loaded_keys_.find(key_id) != loaded_keys_.end();
}

bool CdmLicense::ExtractProviderSessionToken(
    const CdmKeyResponse& license_response,
    std::string* provider_session_token) {
  if (license_response.empty()) {
    LOGE("CdmLicense::ExtractProviderSessionToken: empty license response");
    return false;
  }

  SignedMessage signed_response;
  if (!signed_response.ParseFromString(license_response)) {
    LOGE(
        "CdmLicense::ExtractProviderSessionToken: unable to parse signed "
        "license response");
    return false;
  }

  if (signed_response.type() != SignedMessage::LICENSE) {
    LOGE(
        "CdmLicense::ExtractProviderSessionToken: unrecognized signed message "
        "type: %d",
        signed_response.type());
    return false;
  }

  License license;
  if (!license.ParseFromString(signed_response.msg())) {
    LOGE(
        "CdmLicense::ExtractProviderSessionToken: unable to parse license "
        "response");
    return false;
  }

  if (license.id().has_provider_session_token() &&
      !license.id().provider_session_token().empty()) {
    *provider_session_token = license.id().provider_session_token();
    return true;
  }

  return false;
}

CdmResponseType CdmLicense::HandleKeyErrorResponse(
    const SignedMessage& signed_message) {
  LicenseError license_error;
  if (!license_error.ParseFromString(signed_message.msg())) {
    LOGE("CdmLicense::HandleKeyErrorResponse: Unable to parse license error");
    return KEY_ERROR;
  }

  switch (license_error.error_code()) {
    case LicenseError::INVALID_DRM_DEVICE_CERTIFICATE:
      return NEED_PROVISIONING;
    case LicenseError::REVOKED_DRM_DEVICE_CERTIFICATE:
      return DEVICE_REVOKED;
    case LicenseError::SERVICE_UNAVAILABLE:
    default:
      LOGW("CdmLicense::HandleKeyErrorResponse: Unknown error type = %d",
           license_error.error_code());
      return KEY_ERROR;
  }
}

CdmResponseType CdmLicense::PrepareClientId(
    const CdmAppParameterMap& app_parameters,
    const std::string& provider_client_token,
    LicenseRequest* license_request) {
  wvcdm::ClientIdentification id;
  CdmResponseType status = id.Init(client_token_, device_id_, crypto_session_);
  if (status != NO_ERROR) return status;

  video_widevine::ClientIdentification* client_id =
      license_request->mutable_client_id();
  status = id.Prepare(app_parameters, provider_client_token, client_id);
  if (status != NO_ERROR) return status;

  if (Properties::UsePrivacyMode(session_id_)) {
    if (!service_certificate_.has_certificate()) {
      LOGE("CdmLicense::PrepareClientId: Service Certificate not staged");
      return PRIVACY_MODE_ERROR_3;
    }
    EncryptedClientIdentification* encrypted_client_id =
        license_request->mutable_encrypted_client_id();
    status = service_certificate_.EncryptClientId(crypto_session_, client_id,
                                                  encrypted_client_id);
    if (NO_ERROR == status) {
      license_request->clear_client_id();
    } else {
      license_request->clear_encrypted_client_id();
    }
    return status;
  }
  return NO_ERROR;
}

CdmResponseType CdmLicense::PrepareContentId(
    const InitializationData& init_data, CdmLicenseType license_type,
    const std::string& request_id, LicenseRequest* license_request) {
  // Content Identification may be a cenc_id, a webm_id or a license_id
  LicenseRequest_ContentIdentification* content_id =
      license_request->mutable_content_id();

  if (init_data.is_cenc() || init_data.is_hls()) {
    LicenseRequest_ContentIdentification_CencDeprecated* cenc_content_id =
        content_id->mutable_cenc_id_deprecated();

    if (!init_data.IsEmpty()) {
      cenc_content_id->add_pssh(init_data.data());
    } else {
      LOGE("CdmLicense::PrepareContentId: ISO-CENC init data not available");
      return CENC_INIT_DATA_UNAVAILABLE;
    }

    if (!SetTypeAndId(license_type, request_id, cenc_content_id)) {
      return PREPARE_CENC_CONTENT_ID_FAILED;
    }
  } else if (init_data.is_webm()) {
    LicenseRequest_ContentIdentification_WebmDeprecated* webm_content_id =
        content_id->mutable_webm_id_deprecated();

    if (!init_data.IsEmpty()) {
      webm_content_id->set_header(init_data.data());
    } else {
      LOGE("CdmLicense::PrepareContentId: WebM init data not available");
      return WEBM_INIT_DATA_UNAVAILABLE;
    }

    if (!SetTypeAndId(license_type, request_id, webm_content_id)) {
      return PREPARE_WEBM_CONTENT_ID_FAILED;
    }
  } else {
    LOGE("CdmLicense::PrepareContentId: no support for init data type (%s)",
         init_data.type().c_str());
    return UNSUPPORTED_INIT_DATA_FORMAT;
  }
  return NO_ERROR;
}

CdmResponseType CdmLicense::HandleContentKeyResponse(
    const std::string& msg, const std::string& signature,
    const std::string& mac_key_iv, const std::string& mac_key,
    const std::vector<CryptoKey>& key_array,
    const video_widevine::License& license) {
  if (key_array.empty()) {
    LOGE("CdmLicense::HandleKeyResponse : No content keys.");
    return NO_CONTENT_KEY;
  }
  CdmResponseType resp = crypto_session_->LoadKeys(
      msg, signature, mac_key_iv, mac_key, key_array, provider_session_token_,
      license.srm_requirement(), kLicenseKeyTypeContent);

  if (KEY_ADDED == resp) {
    loaded_keys_.clear();
    for (std::vector<CryptoKey>::const_iterator it = key_array.begin();
         it != key_array.end(); ++it) {
      loaded_keys_.insert(it->key_id());
    }
    policy_engine_->SetLicense(license);
  }
  return resp;
}

CdmResponseType CdmLicense::HandleEntitlementKeyResponse(
    const std::string& msg, const std::string& signature,
    const std::string& mac_key_iv, const std::string& mac_key,
    const std::vector<CryptoKey>& key_array,
    const video_widevine::License& license) {
  if (key_array.empty()) {
    LOGE("CdmLicense::HandleKeyResponse : No entitlement keys.");
    return NO_CONTENT_KEY;
  }
  CdmResponseType resp = crypto_session_->LoadKeys(
      msg, signature, mac_key_iv, mac_key, key_array, provider_session_token_,
      license.srm_requirement(), kLicenseKeyTypeEntitlement);
  if (KEY_ADDED != resp) {
    return resp;
  }

  // Save the entitlement keys for future use to handle key changes.
  entitlement_keys_.CopyFrom(license.key());
  policy_engine_->SetLicense(license);

  return HandleNewEntitledKeys(wrapped_keys_);
}

CdmResponseType CdmLicense::HandleNewEntitledKeys(
    const std::vector<WidevinePsshData_EntitledKey>& wrapped_keys) {
  std::vector<CryptoKey> entitled_key_array;
  entitled_key_array.reserve(wrapped_keys.size());

  for (RepeatedPtrField<License_KeyContainer>::const_iterator kc =
           entitlement_keys_.begin();
       kc != entitlement_keys_.end(); ++kc) {
    if (kc->type() != video_widevine::License::KeyContainer::ENTITLEMENT) {
      continue;
    }
    for (std::vector<WidevinePsshData_EntitledKey>::const_iterator wk =
             wrapped_keys.begin();
         wk != wrapped_keys.end(); wk++) {
      if (wk->entitlement_key_id() == kc->id()) {
        // Add a new entry to the key array to load oemcrypto.
        entitled_key_array.resize(entitled_key_array.size() + 1);

        // Strip PKCS#5 padding from entitled content keys.
        std::string content_key = wk->key();
        if (content_key.size() < CONTENT_KEY_SIZE) {
          LOGE("Entitled Key too small, %lu bytes", content_key.size());
          return KEY_SIZE_ERROR_2;
        } else if (content_key.size() > CONTENT_KEY_SIZE) {
          content_key.resize(CONTENT_KEY_SIZE);
        }

        CryptoKey& this_entry = entitled_key_array.back();
        this_entry.set_key_id(wk->key_id());
        this_entry.set_key_data_iv(wk->iv());
        this_entry.set_entitlement_key_id(wk->entitlement_key_id());
        this_entry.set_key_data(content_key);
      }
    }
  }

  CdmResponseType resp =
      crypto_session_->LoadEntitledContentKeys(entitled_key_array);
  if (KEY_ADDED == resp) {
    for (std::vector<WidevinePsshData_EntitledKey>::const_iterator it =
             wrapped_keys.begin();
         it != wrapped_keys.end(); ++it) {
      loaded_keys_.insert(it->key_id());
    }

    policy_engine_->SetEntitledLicenseKeys(wrapped_keys);
  }
  return resp;
}

template <typename T>
bool CdmLicense::SetTypeAndId(CdmLicenseType license_type,
                              const std::string& request_id, T* content_id) {
  switch (license_type) {
    case kLicenseTypeOffline:
      content_id->set_license_type(video_widevine::OFFLINE);
      break;
    case kLicenseTypeStreaming:
    case kLicenseTypeTemporary:
      content_id->set_license_type(video_widevine::STREAMING);
      break;
    default:
      LOGD("CdmLicense::PrepareKeyRequest: Unknown license type = %d",
           license_type);
      return false;
  }

  content_id->set_request_id(request_id);
  return true;
}

}  // namespace wvcdm
