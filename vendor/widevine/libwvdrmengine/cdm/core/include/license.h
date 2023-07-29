// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#ifndef WVCDM_CORE_LICENSE_H_
#define WVCDM_CORE_LICENSE_H_

#include <memory>
#include <set>

#include "disallow_copy_and_assign.h"
#include "initialization_data.h"
#include "license_protocol.pb.h"
#include "service_certificate.h"
#include "wv_cdm_types.h"

namespace video_widevine {
class SignedMessage;
class LicenseRequest;
class VersionInfo;
}  // namespace video_widevine

namespace wvcdm {

class Clock;
class CryptoSession;
class PolicyEngine;
class CdmSession;
class CryptoKey;

using ::google::protobuf::RepeatedPtrField;
using video_widevine::License_KeyContainer;
using video_widevine::VersionInfo;
using video_widevine::WidevinePsshData_EntitledKey;

class CdmLicense {
 public:
  CdmLicense(const CdmSessionId& session_id);
  virtual ~CdmLicense();

  virtual bool Init(const std::string& client_token,
                    CdmClientTokenType client_token_type,
                    const std::string& device_id, bool use_privacy_mode,
                    const std::string& signed_service_certificate,
                    CryptoSession* session, PolicyEngine* policy_engine);

  virtual CdmResponseType PrepareKeyRequest(
      const InitializationData& init_data, CdmLicenseType license_type,
      const CdmAppParameterMap& app_parameters, CdmKeyMessage* signed_request,
      std::string* server_url);
  virtual CdmResponseType PrepareKeyUpdateRequest(
      bool is_renewal, const CdmAppParameterMap& app_parameters,
      CdmSession* cdm_session, CdmKeyMessage* signed_request,
      std::string* server_url);
  virtual CdmResponseType HandleKeyResponse(
      const CdmKeyResponse& license_response);
  virtual CdmResponseType HandleKeyUpdateResponse(
      bool is_renewal, const CdmKeyResponse& license_response);
  virtual CdmResponseType HandleEmbeddedKeyData(
      const InitializationData& init_data);

  virtual CdmResponseType RestoreOfflineLicense(
      const CdmKeyMessage& license_request,
      const CdmKeyResponse& license_response,
      const CdmKeyResponse& license_renewal_response,
      int64_t playback_start_time, int64_t last_playback_time,
      int64_t grace_period_end_time, CdmSession* cdm_session);
  virtual CdmResponseType RestoreLicenseForRelease(
      const CdmKeyMessage& license_request,
      const CdmKeyResponse& license_response);
  virtual bool HasInitData() { return stored_init_data_.get(); }
  virtual bool IsKeyLoaded(const KeyId& key_id);

  virtual std::string provider_session_token() {
    return provider_session_token_;
  }

  virtual bool is_offline() { return is_offline_; }

  virtual const VersionInfo& GetServiceVersion() {
    return latest_service_version_;
  }

  static bool ExtractProviderSessionToken(
      const CdmKeyResponse& license_response,
      std::string* provider_session_token);

 private:
  CdmResponseType HandleKeyErrorResponse(
      const video_widevine::SignedMessage& signed_message);

  CdmResponseType PrepareClientId(
      const CdmAppParameterMap& app_parameters,
      const std::string& provider_client_token,
      video_widevine::LicenseRequest* license_request);

  CdmResponseType PrepareContentId(
      const InitializationData& init_data, CdmLicenseType license_type,
      const std::string& request_id,
      video_widevine::LicenseRequest* license_request);

  CdmResponseType HandleContentKeyResponse(
      const std::string& msg, const std::string& signature,
      const std::string& mac_key_iv, const std::string& mac_key,
      const std::vector<CryptoKey>& key_array,
      const video_widevine::License& license);

  // HandleEntitlementKeyResponse loads the entitlement keys in |key_array| into
  // the crypto session. In addition, it also extracts content keys from
  // |wrapped_keys_| and loads them for use.
  CdmResponseType HandleEntitlementKeyResponse(
      const std::string& msg, const std::string& signature,
      const std::string& mac_key_iv, const std::string& mac_key,
      const std::vector<CryptoKey>& key_array,
      const video_widevine::License& license);

  CdmResponseType HandleNewEntitledKeys(
      const std::vector<WidevinePsshData_EntitledKey>& wrapped_keys);

  template <typename T>
  bool SetTypeAndId(CdmLicenseType license_type, const std::string& request_id,
                    T* content_id);

  CryptoSession* crypto_session_;
  PolicyEngine* policy_engine_;
  std::string server_url_;
  std::string client_token_;
  CdmClientTokenType client_token_type_;
  std::string device_id_;
  const CdmSessionId session_id_;
  std::unique_ptr<InitializationData> stored_init_data_;
  bool initialized_;
  std::set<KeyId> loaded_keys_;
  std::string provider_session_token_;
  bool renew_with_client_id_;
  bool is_offline_;

  // Associated with ClientIdentification encryption
  bool use_privacy_mode_;
  ServiceCertificate service_certificate_;

  // Used for certificate based licensing
  CdmKeyMessage key_request_;

  std::unique_ptr<Clock> clock_;

  // For testing
  // CdmLicense takes ownership of the clock.
  CdmLicense(const CdmSessionId& session_id, Clock* clock);

  // For entitlement key licensing. This holds the keys from the init_data.
  // These keys are extracted from the pssh when we generate a license request.
  // It is used to load content keys after we have received a license and
  // entitelement keys. It is also used in updating the key status info.
  std::vector<WidevinePsshData_EntitledKey> wrapped_keys_;

  CdmLicenseKeyType license_key_type_;
  RepeatedPtrField<License_KeyContainer> entitlement_keys_;

  std::string provider_client_token_;
  // This is the latest version info extracted from the SignedMessage in
  // HandleKeyResponse
  VersionInfo latest_service_version_;

#if defined(UNIT_TEST)
  friend class CdmLicenseTestPeer;
#endif

  CORE_DISALLOW_COPY_AND_ASSIGN(CdmLicense);
};

}  // namespace wvcdm

#endif  // WVCDM_CORE_LICENSE_H_
