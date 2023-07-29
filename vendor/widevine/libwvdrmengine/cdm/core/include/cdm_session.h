// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#ifndef WVCDM_CORE_CDM_SESSION_H_
#define WVCDM_CORE_CDM_SESSION_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "crypto_session.h"
#include "device_files.h"
#include "disallow_copy_and_assign.h"
#include "file_store.h"
#include "initialization_data.h"
#include "license.h"
#include "metrics_collections.h"
#include "oemcrypto_adapter.h"
#include "policy_engine.h"
#include "timer_metric.h"
#include "wv_cdm_types.h"

namespace wvcdm {

class CdmClientPropertySet;
class ServiceCertificate;
class WvCdmEventListener;
class UsageTableHeader;

class CdmSession {
 public:
  // Creates a new instance of the CdmSession with the given |file_system|
  // and |metrics| parameters. Both parameters are owned by the caller and
  // must remain in scope througout the scope of the new instance. |metrics|
  // must not be null.
  CdmSession(FileSystem* file_system,
             std::shared_ptr<metrics::SessionMetrics> metrics);
  virtual ~CdmSession();

  void Close() { closed_ = true; }
  bool IsClosed() { return closed_; }

  // Initializes this instance of CdmSession with the given property set.
  // |cdm_client_property_set| MAY be null, is owned by the caller,
  // and must remain in scope throughout the scope of this session.
  virtual CdmResponseType Init(CdmClientPropertySet* cdm_client_property_set);

  // Initializes this instance of CdmSession with the given parmeters.
  // All parameters are owned by the caller.
  // |service_certificate| is caller owned, cannot be null, and must be in
  //     scope as long as the session is in scope.
  // |cdm_client_property_set| is caller owned, may be null, but must be
  //     in scope as long as the session is in scope.
  // |forced_session_id| is caller owned and may be null.
  // |event_listener| is caller owned, may be null, but must be in scope
  //     as long as the session is in scope.
  virtual CdmResponseType Init(CdmClientPropertySet* cdm_client_property_set,
                               const CdmSessionId* forced_session_id,
                               WvCdmEventListener* event_listener);

  // Restores an offline session identified by the |key_set_id| and
  // |license_type|. The |error_detail| will be filled with an internal error
  // code. The |error_detail| may be a CdmResponseType or other error code type.
  // It is only suitable for additional logging or debugging.
  virtual CdmResponseType RestoreOfflineSession(
      const CdmKeySetId& key_set_id, CdmLicenseType license_type,
      int* error_detail);
  // Restores an usage session from the provided |usage_data|.
  // The |error_detail| will be filled with an internal error code. The
  // |error_detail| may be a CdmResponseType or other error code type. It is
  // only suitable for additional logging or debugging.
  virtual CdmResponseType RestoreUsageSession(
      const DeviceFiles::CdmUsageData& usage_data,
      int* error_detail);

  virtual const CdmSessionId& session_id() { return session_id_; }
  virtual const CdmKeySetId& key_set_id() { return key_set_id_; }

  virtual CdmResponseType GenerateKeyRequest(
      const InitializationData& init_data, CdmLicenseType license_type,
      const CdmAppParameterMap& app_parameters, CdmKeyRequest* key_request);

  // AddKey() - Accept license response and extract key info.
  virtual CdmResponseType AddKey(const CdmKeyResponse& key_response);

  // Query session status
  virtual CdmResponseType QueryStatus(CdmQueryMap* query_response);

  // Query license information
  virtual CdmResponseType QueryKeyStatus(CdmQueryMap* query_response);

  // Query allowed usages for key
  virtual CdmResponseType QueryKeyAllowedUsage(const std::string& key_id,
                                               CdmKeyAllowedUsage* key_usage);

  // Query OEMCrypto session ID
  virtual CdmResponseType QueryOemCryptoSessionId(CdmQueryMap* query_response);

  // Decrypt() - Accept encrypted buffer and return decrypted data.
  virtual CdmResponseType Decrypt(const CdmDecryptionParameters& parameters);

  // License renewal
  // GenerateRenewalRequest() - Construct valid renewal request for the current
  // session keys.
  virtual CdmResponseType GenerateRenewalRequest(CdmKeyRequest* key_request);

  // RenewKey() - Accept renewal response and update key info.
  virtual CdmResponseType RenewKey(const CdmKeyResponse& key_response);

  // License release
  // GenerateReleaseRequest() - Construct valid release request for the current
  // session keys.
  virtual CdmResponseType GenerateReleaseRequest(CdmKeyRequest* key_request);

  // ReleaseKey() - Accept response and release key.
  virtual CdmResponseType ReleaseKey(const CdmKeyResponse& key_response);

  virtual CdmResponseType DeleteUsageEntry(uint32_t usage_entry_number);

  virtual bool IsKeyLoaded(const KeyId& key_id);
  virtual int64_t GetDurationRemaining();

  // Used for notifying the Policy Engine of resolution changes
  virtual void NotifyResolution(uint32_t width, uint32_t height);

  virtual void OnTimerEvent(bool update_usage);
  virtual void OnKeyReleaseEvent(const CdmKeySetId& key_set_id);

  virtual void GetApplicationId(std::string* app_id);
  virtual SecurityLevel GetRequestedSecurityLevel() {
    return requested_security_level_;
  }
  virtual CdmSecurityLevel GetSecurityLevel() { return security_level_; }

  // Delete usage information for the list of tokens, |provider_session_tokens|.
  virtual CdmResponseType DeleteMultipleUsageInformation(
      const std::vector<std::string>& provider_session_tokens);
  virtual CdmResponseType UpdateUsageTableInformation();
  virtual CdmResponseType UpdateUsageEntryInformation();

  virtual bool is_initial_usage_update() { return is_initial_usage_update_; }
  virtual bool is_usage_update_needed() { return is_usage_update_needed_; }
  virtual void reset_usage_flags() {
    is_initial_usage_update_ = false;
    is_usage_update_needed_ = false;
  }

  virtual bool is_release() { return is_release_; }
  virtual bool is_offline() { return is_offline_; }
  virtual bool is_temporary() { return is_temporary_; }
  virtual bool license_received() { return license_received_; }
  virtual bool has_provider_session_token() {
    return (license_parser_.get() != NULL &&
        license_parser_->provider_session_token().size() > 0);
  }

  virtual CdmUsageSupportType get_usage_support_type()
      { return usage_support_type_; }

  // This method will remove keys by resetting crypto resources and
  // policy information. This renders the session mostly useless and it is
  // preferable to simply delete this object rather than call this method.
  virtual CdmResponseType RemoveKeys();

  // Remove the current offline license and/or matching usage record, if any
  // exist.
  CdmResponseType RemoveLicense();
  // Delete this session's associated license or usage record file. Note that,
  // unlike RemoveLicense(), this method ONLY affects the file system and does
  // not touch the usage table headers.
  bool DeleteLicenseFile();

  // Generate unique ID for each new session.
  CdmSessionId GenerateSessionId();

  // Generic crypto operations - provides basic crypto operations that an
  // application can use outside of content stream processing

  // Encrypts a buffer of app-level data.
  virtual CdmResponseType GenericEncrypt(const std::string& in_buffer,
                                         const std::string& key_id,
                                         const std::string& iv,
                                         CdmEncryptionAlgorithm algorithm,
                                         std::string* out_buffer);

  // Decrypts a buffer of app-level data.
  virtual CdmResponseType GenericDecrypt(const std::string& in_buffer,
                                         const std::string& key_id,
                                         const std::string& iv,
                                         CdmEncryptionAlgorithm algorithm,
                                         std::string* out_buffer);

  // Computes the signature for a message.
  virtual CdmResponseType GenericSign(const std::string& message,
                                      const std::string& key_id,
                                      CdmSigningAlgorithm algorithm,
                                      std::string* signature);

  // Verifies the signature on a buffer of app-level data.
  virtual CdmResponseType GenericVerify(const std::string& message,
                                        const std::string& key_id,
                                        CdmSigningAlgorithm algorithm,
                                        const std::string& signature);

  virtual CdmResponseType SetDecryptHash(uint32_t frame_number,
                                         const std::string& hash);

  virtual CdmResponseType GetDecryptHashError(std::string* hash_error_string);

  virtual metrics::SessionMetrics* GetMetrics() { return metrics_.get(); }

 private:
  friend class CdmSessionTest;

  bool GenerateKeySetId(CdmKeySetId* key_set_id);

  CdmResponseType StoreLicense();

  bool StoreLicense(DeviceFiles::LicenseState state,
                    int* error_detail);

  bool UpdateUsageInfo();

  CdmResponseType GenerateKeyRequestInternal(
      const InitializationData& init_data, CdmLicenseType license_type,
      const CdmAppParameterMap& app_parameters, CdmKeyRequest* key_request);
  virtual CdmResponseType AddKeyInternal(const CdmKeyResponse& key_response);
  void UpdateRequestLatencyTiming(CdmResponseType sts);

  // These setters are for testing only. Takes ownership of the pointers.
  void set_license_parser(CdmLicense* license_parser);
  void set_crypto_session(CryptoSession* crypto_session);
  void set_policy_engine(PolicyEngine* policy_engine);
  void set_file_handle(DeviceFiles* file_handle);

  // instance variables
  std::shared_ptr<metrics::SessionMetrics> metrics_;
  metrics::CryptoMetrics* crypto_metrics_;
  metrics::TimerMetric life_span_;
  metrics::TimerMetric license_request_latency_;
  CdmKeyRequestType key_request_type_;

  bool initialized_;
  bool closed_;  // Session closed, but final shared_ptr has not been released.
  CdmSessionId session_id_;
  FileSystem* file_system_;
  std::unique_ptr<CdmLicense> license_parser_;
  std::unique_ptr<CryptoSession> crypto_session_;
  std::unique_ptr<PolicyEngine> policy_engine_;
  std::unique_ptr<DeviceFiles> file_handle_;
  bool license_received_;
  bool is_offline_;
  bool is_release_;
  bool is_temporary_;
  CdmSecurityLevel security_level_;
  SecurityLevel requested_security_level_;
  CdmAppParameterMap app_parameters_;

  // decryption flags
  bool is_initial_decryption_;
  bool has_decrypted_since_last_report_;  // ... last report to policy engine.

  // Usage related flags and data
  bool is_initial_usage_update_;
  bool is_usage_update_needed_;
  CdmUsageSupportType usage_support_type_;
  UsageTableHeader* usage_table_header_;
  uint32_t usage_entry_number_;
  CdmUsageEntry usage_entry_;
  std::string usage_provider_session_token_;

  // information useful for offline and usage scenarios
  CdmKeyMessage key_request_;
  CdmKeyResponse key_response_;

  // license type offline related information
  CdmInitData offline_init_data_;
  CdmKeyMessage offline_key_renewal_request_;
  CdmKeyResponse offline_key_renewal_response_;
  std::string offline_release_server_url_;

  // license type release and offline related information
  CdmKeySetId key_set_id_;

  bool mock_license_parser_in_use_;
  bool mock_policy_engine_in_use_;

  CORE_DISALLOW_COPY_AND_ASSIGN(CdmSession);
};

}  // namespace wvcdm

#endif  // WVCDM_CORE_CDM_SESSION_H_
