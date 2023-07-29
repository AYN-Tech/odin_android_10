// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#ifndef CDM_BASE_WV_CONTENT_DECRYPTION_MODULE_H_
#define CDM_BASE_WV_CONTENT_DECRYPTION_MODULE_H_

#include <map>
#include <memory>
#include <mutex>

#include <utils/RefBase.h>

#include "cdm_identifier.h"
#include "disallow_copy_and_assign.h"
#include "file_store.h"
#include "metrics.pb.h"
#include "timer.h"
#include "wv_cdm_types.h"

namespace wvcdm {

class CdmClientPropertySet;
class CdmEngine;
class WvCdmEventListener;

class WvContentDecryptionModule : public android::RefBase, public TimerHandler {
 public:
  WvContentDecryptionModule();
  virtual ~WvContentDecryptionModule();

  // Static methods
  static bool IsSupported(const std::string& init_data_type);
  static bool IsCenc(const std::string& init_data_type);
  static bool IsWebm(const std::string& init_data_type);
  static bool IsHls(const std::string& init_data_type);
  static bool IsAudio(const std::string& init_data_type);

  // Session related methods
  virtual CdmResponseType OpenSession(const CdmKeySystem& key_system,
                                      CdmClientPropertySet* property_set,
                                      const CdmIdentifier& identifier,
                                      WvCdmEventListener* event_listener,
                                      CdmSessionId* session_id);
  virtual CdmResponseType CloseSession(const CdmSessionId& session_id);
  virtual bool IsOpenSession(const CdmSessionId& session_id);

  // Construct a valid license request.
  virtual CdmResponseType GenerateKeyRequest(
      const CdmSessionId& session_id, const CdmKeySetId& key_set_id,
      const std::string& init_data_type, const CdmInitData& init_data,
      const CdmLicenseType license_type, CdmAppParameterMap& app_parameters,
      CdmClientPropertySet* property_set, const CdmIdentifier& identifier,
      CdmKeyRequest* key_request);

  // Accept license response and extract key info.
  virtual CdmResponseType AddKey(const CdmSessionId& session_id,
                                 const CdmKeyResponse& key_data,
                                 CdmKeySetId* key_set_id);

  // Setup keys for offline usage which were retrived in an earlier key request
  virtual CdmResponseType RestoreKey(const CdmSessionId& session_id,
                                     const CdmKeySetId& key_set_id);

  // Cancel session
  virtual CdmResponseType RemoveKeys(const CdmSessionId& session_id);

  // Query system information
  virtual CdmResponseType QueryStatus(SecurityLevel security_level,
                                      const std::string& key,
                                      std::string* value);
  // Query session information
  virtual CdmResponseType QuerySessionStatus(const CdmSessionId& session_id,
                                             CdmQueryMap* key_info);

  // Query license information
  virtual CdmResponseType QueryKeyStatus(const CdmSessionId& session_id,
                                         CdmQueryMap* key_info);

  // Query OEMCrypto session ID
  virtual CdmResponseType QueryOemCryptoSessionId(
      const CdmSessionId& session_id, CdmQueryMap* response);

  // Query security level support
  static bool IsSecurityLevelSupported(CdmSecurityLevel level);

  // Provisioning related methods
  virtual CdmResponseType GetProvisioningRequest(
      CdmCertificateType cert_type, const std::string& cert_authority,
      const CdmIdentifier& identifier, const std::string& service_certificate,
      CdmProvisioningRequest* request, std::string* default_url);

  virtual CdmResponseType HandleProvisioningResponse(
      const CdmIdentifier& identifier, CdmProvisioningResponse& response,
      std::string* cert, std::string* wrapped_key);

  virtual CdmResponseType Unprovision(CdmSecurityLevel level,
                                      const CdmIdentifier& identifier);

  // Secure stop related methods
  virtual CdmResponseType GetUsageInfo(const std::string& app_id,
                                       const CdmIdentifier& identifier,
                                       CdmUsageInfo* usage_info);
  virtual CdmResponseType GetUsageInfo(const std::string& app_id,
                                       const CdmSecureStopId& ssid,
                                       const CdmIdentifier& identifier,
                                       CdmUsageInfo* usage_info);
  virtual CdmResponseType RemoveAllUsageInfo(const std::string& app_id,
                                             const CdmIdentifier& identifier);
  virtual CdmResponseType RemoveUsageInfo(
      const std::string& app_id,
      const CdmIdentifier& identifier,
      const CdmSecureStopId& secure_stop_id);
  virtual CdmResponseType ReleaseUsageInfo(
      const CdmUsageInfoReleaseMessage& message,
      const CdmIdentifier& identifier);

  virtual CdmResponseType GetSecureStopIds(const std::string& app_id,
                                           const CdmIdentifier& identifier,
                                           std::vector<CdmSecureStopId>* ssids);

  // Accept encrypted buffer and decrypt data.
  // Decryption parameters that need to be specified are
  // is_encrypted, is_secure, key_id, encrypt_buffer, encrypt_length,
  // iv, block_offset, decrypt_buffer, decrypt_buffer_length,
  // decrypt_buffer_offset and subsample_flags
  virtual CdmResponseType Decrypt(const CdmSessionId& session_id,
                                  bool validate_key_id,
                                  const CdmDecryptionParameters& parameters);

  virtual void NotifyResolution(const CdmSessionId& session_id, uint32_t width,
                                uint32_t height);

  // Validate a passed-in service certificate
  virtual bool IsValidServiceCertificate(const std::string& certificate);

  // Fill the metrics parameter with the metrics data for the CdmEngine
  // associated with the given CdmIdentifier. If the CdmEngine instance does
  // not exist, this will return an error.
  virtual CdmResponseType GetMetrics(const CdmIdentifier& identifier,
                                     drm_metrics::WvCdmMetrics* metrics);

  // Closes the CdmEngine and sessions associated with the given CdmIdentifier.
  virtual CdmResponseType CloseCdm(const CdmIdentifier& identifier);

  virtual CdmResponseType SetDecryptHash(const std::string& hash_data,
                                         CdmSessionId* session_id);
  virtual CdmResponseType GetDecryptHashError(const CdmSessionId& session_id,
                                              std::string* hash_error_string);

  // Return the list of key_set_ids stored on the current (origin-specific)
  // file system.
  virtual CdmResponseType ListStoredLicenses(
      CdmSecurityLevel security_level,
      const CdmIdentifier& identifier,
      std::vector<CdmKeySetId>* key_set_ids);

  // Retrieve offline license state using key_set_id.
  virtual CdmResponseType GetOfflineLicenseState(
      const CdmKeySetId& key_set_id,
      CdmSecurityLevel security_level,
      const CdmIdentifier& identifier,
      CdmOfflineLicenseState* licenseState);

  // Remove offline license using key_set_id.
  virtual CdmResponseType RemoveOfflineLicense(
      const CdmKeySetId& key_set_id,
      CdmSecurityLevel security_level,
      const CdmIdentifier& identifier);

 private:
  struct CdmInfo {
    CdmInfo();

    FileSystem file_system;
    std::unique_ptr<CdmEngine> cdm_engine;
  };

  // Finds the CdmEngine instance for the given identifier, creating one if
  // needed.
  CdmEngine* EnsureCdmForIdentifier(const CdmIdentifier& identifier);
  // Finds the CdmEngine instance for the given session id, returning NULL if
  // not found.
  CdmEngine* GetCdmForSessionId(const std::string& session_id);

  // Close all of the open CdmEngine instances. This is used when ready to close
  // the WvContentDecryptionModule instance.
  void CloseAllCdms();

  uint32_t GenerateSessionSharingId();

  // timer related methods to drive policy decisions
  void EnablePolicyTimer();
  void DisablePolicyTimer();
  void OnTimerEvent();

  static std::mutex session_sharing_id_generation_lock_;
  std::mutex policy_timer_lock_;
  Timer policy_timer_;

  // instance variables
  // This manages the lifetime of the CDM instances.
  std::map<CdmIdentifier, CdmInfo> cdms_;
  std::mutex cdms_lock_;

  // This contains weak pointers to the CDM instances contained in |cdms_|.
  std::map<std::string, CdmEngine*> cdm_by_session_id_;

  CORE_DISALLOW_COPY_AND_ASSIGN(WvContentDecryptionModule);
};

}  // namespace wvcdm

#endif  // CDM_BASE_WV_CONTENT_DECRYPTION_MODULE_H_
