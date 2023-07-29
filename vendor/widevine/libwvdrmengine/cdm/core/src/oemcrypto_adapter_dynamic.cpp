// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
// Wrapper of OEMCrypto APIs for platforms that support both Levels 1 and 3.
// This should be used when liboemcrypto.so is dynamically loaded at run
// time and not linked with the CDM code at compile time.
// An implementation should compile either oemcrypto_adapter_dynamic.cpp or
// oemcrypto_adapter_static.cpp, but not both.
//

#include "oemcrypto_adapter.h"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <condition_variable>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "file_store.h"
#include "level3.h"
#include "log.h"
#include "metrics_collections.h"
#include "properties.h"
#include "wv_cdm_constants.h"

using namespace wvoec3;
using wvcdm::kLevelDefault;
using wvcdm::kLevel3;

namespace {

static const size_t kMaxGenericEncryptChunkSize = 100*1024;
const OEMCryptoResult kOemCryptoResultVendorSpecificError1 =
    static_cast<OEMCryptoResult>(10008);

typedef OEMCryptoResult (*L1_SetSandbox_t)(const uint8_t* sandbox_id,
                                           size_t sandbox_id_length);
typedef OEMCryptoResult (*L1_Initialize_t)(void);
typedef OEMCryptoResult (*L1_Terminate_t)(void);
typedef OEMCryptoResult (*L1_OpenSession_t)(OEMCrypto_SESSION* session);
typedef OEMCryptoResult (*L1_CloseSession_t)(OEMCrypto_SESSION session);
typedef OEMCryptoResult (*L1_GenerateDerivedKeys_t)(
    OEMCrypto_SESSION session, const uint8_t* mac_key_context,
    uint32_t mac_key_context_length, const uint8_t* enc_key_context,
    uint32_t enc_key_context_length);
typedef OEMCryptoResult (*L1_GenerateNonce_t)(OEMCrypto_SESSION session,
                                              uint32_t* nonce);
typedef OEMCryptoResult (*L1_GenerateSignature_t)(OEMCrypto_SESSION session,
                                                  const uint8_t* message,
                                                  size_t message_length,
                                                  uint8_t* signature,
                                                  size_t* signature_length);
typedef OEMCryptoResult (*L1_LoadKeys_t)(
    OEMCrypto_SESSION session, const uint8_t* message, size_t message_length,
    const uint8_t* signature, size_t signature_length,
    OEMCrypto_Substring enc_mac_keys_iv, OEMCrypto_Substring enc_mac_keys,
    size_t num_keys, const OEMCrypto_KeyObject* key_array,
    OEMCrypto_Substring pst, OEMCrypto_Substring srm_restriction_data,
    OEMCrypto_LicenseType license_type);
typedef OEMCryptoResult (*L1_LoadKeys_V14_t)(
    OEMCrypto_SESSION session, const uint8_t* message, size_t message_length,
    const uint8_t* signature, size_t signature_length,
    const uint8_t* enc_mac_key_iv, const uint8_t* enc_mac_key, size_t num_keys,
    const OEMCrypto_KeyObject_V14* key_array, const uint8_t* pst,
    size_t pst_length, const uint8_t* srm_requirement,
    OEMCrypto_LicenseType license_type);
typedef OEMCryptoResult (*L1_LoadKeys_V13_t)(
    OEMCrypto_SESSION session, const uint8_t* message, size_t message_length,
    const uint8_t* signature, size_t signature_length,
    const uint8_t* enc_mac_key_iv, const uint8_t* enc_mac_key, size_t num_keys,
    const OEMCrypto_KeyObject_V13* key_array, const uint8_t* pst,
    size_t pst_length, const uint8_t* srm_requirement);
typedef OEMCryptoResult (*L1_LoadKeys_V11_or_V12_t)(
    OEMCrypto_SESSION session, const uint8_t* message, size_t message_length,
    const uint8_t* signature, size_t signature_length,
    const uint8_t* enc_mac_key_iv, const uint8_t* enc_mac_key, size_t num_keys,
    const OEMCrypto_KeyObject_V13* key_array, const uint8_t* pst,
    size_t pst_length);
typedef OEMCryptoResult (*L1_LoadKeys_V9_or_V10_t)(
    OEMCrypto_SESSION session, const uint8_t* message, size_t message_length,
    const uint8_t* signature, size_t signature_length,
    const uint8_t* enc_mac_key_iv, const uint8_t* enc_mac_key, size_t num_keys,
    const OEMCrypto_KeyObject_V10* key_array, const uint8_t* pst,
    size_t pst_length);
typedef OEMCryptoResult (*L1_LoadKeys_V8_t)(
    OEMCrypto_SESSION session, const uint8_t* message, size_t message_length,
    const uint8_t* signature, size_t signature_length,
    const uint8_t* enc_mac_key_iv, const uint8_t* enc_mac_key, size_t num_keys,
    const OEMCrypto_KeyObject_V10* key_array);
typedef OEMCryptoResult (*L1_LoadEntitledContentKeys_t)(
    OEMCrypto_SESSION session, const uint8_t* message, size_t message_length,
    size_t num_keys, const OEMCrypto_EntitledContentKeyObject* key_array);
typedef OEMCryptoResult (*L1_LoadEntitledContentKeys_V14_t)(
    OEMCrypto_SESSION session, size_t num_keys,
    const OEMCrypto_EntitledContentKeyObject_V14* key_array);
typedef OEMCryptoResult (*L1_RefreshKeys_t)(
    OEMCrypto_SESSION session, const uint8_t* message, size_t message_length,
    const uint8_t* signature, size_t signature_length, size_t num_keys,
    const OEMCrypto_KeyRefreshObject* key_array);
typedef OEMCryptoResult (*L1_RefreshKeys_V14_t)(
    OEMCrypto_SESSION session, const uint8_t* message, size_t message_length,
    const uint8_t* signature, size_t signature_length, size_t num_keys,
    const OEMCrypto_KeyRefreshObject_V14* key_array);
typedef OEMCryptoResult (*L1_QueryKeyControl_t)(
    OEMCrypto_SESSION session, const uint8_t* key_id, size_t key_id_length,
    uint8_t* key_control_block, size_t* key_control_block_length);
typedef OEMCryptoResult (*L1_SelectKey_t)(const OEMCrypto_SESSION session,
                                          const uint8_t* content_key_id,
                                          size_t content_key_id_length,
                                          OEMCryptoCipherMode cipher_mode);
typedef OEMCryptoResult (*L1_SelectKey_V13_t)(const OEMCrypto_SESSION session,
                                              const uint8_t* key_id,
                                              size_t key_id_length);
typedef OEMCryptoResult (*L1_DecryptCTR_V10_t)(
    OEMCrypto_SESSION session, const uint8_t* data_addr, size_t data_length,
    bool is_encrypted, const uint8_t* iv, size_t offset,
    const OEMCrypto_DestBufferDesc* out_buffer, uint8_t subsample_flags);
typedef OEMCryptoResult (*L1_DecryptCENC_t)(
    OEMCrypto_SESSION session, const uint8_t* data_addr, size_t data_length,
    bool is_encrypted, const uint8_t* iv, size_t offset,
    OEMCrypto_DestBufferDesc* out_buffer,
    const OEMCrypto_CENCEncryptPatternDesc* pattern, uint8_t subsample_flags);
typedef OEMCryptoResult (*L1_CopyBuffer_V14_t)(const uint8_t* data_addr,
                                           size_t data_length,
                                           OEMCrypto_DestBufferDesc* out_buffer,
                                           uint8_t subsample_flags);
typedef OEMCryptoResult (*L1_CopyBuffer_t)(OEMCrypto_SESSION session,
                                           const uint8_t* data_addr,
                                           size_t data_length,
                                           OEMCrypto_DestBufferDesc* out_buffer,
                                           uint8_t subsample_flags);
typedef OEMCryptoResult (*L1_WrapKeybox_t)(const uint8_t* keybox,
                                           size_t keyBoxLength,
                                           uint8_t* wrappedKeybox,
                                           size_t* wrappedKeyBoxLength,
                                           const uint8_t* transportKey,
                                           size_t transportKeyLength);
typedef OEMCryptoResult (*L1_InstallKeyboxOrOEMCert_t)(const uint8_t* keybox,
                                              size_t keyBoxLength);
typedef OEMCryptoResult (*L1_LoadTestKeybox_t)(const uint8_t *buffer,
                                               size_t length);
typedef OEMCryptoResult (*L1_LoadTestKeybox_V13_t)();
typedef OEMCryptoResult (*L1_IsKeyboxOrOEMCertValid_t)();
typedef OEMCryptoResult (*L1_GetDeviceID_t)(uint8_t* deviceID,
                                            size_t* idLength);
typedef OEMCryptoResult (*L1_GetKeyData_t)(uint8_t* keyData,
                                           size_t* keyDataLength);
typedef OEMCryptoResult (*L1_GetRandom_t)(uint8_t* randomData,
                                          size_t dataLength);
typedef OEMCryptoResult (*L1_RewrapDeviceRSAKey_t)(
    OEMCrypto_SESSION session, const uint8_t* message, size_t message_length,
    const uint8_t* signature, size_t signature_length, const uint32_t* nonce,
    const uint8_t* enc_rsa_key, size_t enc_rsa_key_length,
    const uint8_t* enc_rsa_key_iv, uint8_t* wrapped_rsa_key,
    size_t* wrapped_rsa_key_length);
typedef OEMCryptoResult (*L1_LoadDeviceRSAKey_t)(OEMCrypto_SESSION session,
                                                 const uint8_t* wrapped_rsa_key,
                                                 size_t wrapped_rsa_key_length);
typedef OEMCryptoResult (*L1_LoadTestRSAKey_t)();
typedef OEMCryptoResult (*L1_GenerateRSASignature_t)(
    OEMCrypto_SESSION session, const uint8_t* message, size_t message_length,
    uint8_t* signature, size_t* signature_length,
    RSA_Padding_Scheme padding_scheme);
typedef OEMCryptoResult (*L1_GenerateRSASignature_V8_t)(
    OEMCrypto_SESSION session, const uint8_t* message, size_t message_length,
    uint8_t* signature, size_t* signature_length);
typedef OEMCryptoResult (*L1_DeriveKeysFromSessionKey_t)(
    OEMCrypto_SESSION session, const uint8_t* enc_session_key,
    size_t enc_session_key_length, const uint8_t* mac_key_context,
    size_t mac_key_context_length, const uint8_t* enc_key_context,
    size_t enc_key_context_length);
typedef uint32_t (*L1_APIVersion_t)();
typedef uint8_t (*L1_SecurityPatchLevel_t)();
typedef const char* (*L1_SecurityLevel_t)();
typedef OEMCryptoResult (*L1_GetHDCPCapability_V9_t)(uint8_t* current,
                                                     uint8_t* maximum);
typedef OEMCryptoResult (*L1_GetHDCPCapability_t)(
    OEMCrypto_HDCP_Capability* current, OEMCrypto_HDCP_Capability* maximum);
typedef bool (*L1_SupportsUsageTable_t)();
typedef bool (*L1_IsAntiRollbackHwPresent_t)();
typedef OEMCryptoResult (*L1_GetNumberOfOpenSessions_t)(size_t* count);
typedef OEMCryptoResult (*L1_GetMaxNumberOfSessions_t)(size_t* maximum);
typedef OEMCryptoResult (*L1_Generic_Encrypt_t)(
    OEMCrypto_SESSION session, const uint8_t* in_buffer, size_t buffer_length,
    const uint8_t* iv, OEMCrypto_Algorithm algorithm, uint8_t* out_buffer);
typedef OEMCryptoResult (*L1_Generic_Decrypt_t)(
    OEMCrypto_SESSION session, const uint8_t* in_buffer, size_t buffer_length,
    const uint8_t* iv, OEMCrypto_Algorithm algorithm, uint8_t* out_buffer);
typedef OEMCryptoResult (*L1_Generic_Sign_t)(OEMCrypto_SESSION session,
                                             const uint8_t* in_buffer,
                                             size_t buffer_length,
                                             OEMCrypto_Algorithm algorithm,
                                             uint8_t* signature,
                                             size_t* signature_length);
typedef OEMCryptoResult (*L1_Generic_Verify_t)(OEMCrypto_SESSION session,
                                               const uint8_t* in_buffer,
                                               size_t buffer_length,
                                               OEMCrypto_Algorithm algorithm,
                                               const uint8_t* signature,
                                               size_t signature_length);
typedef OEMCryptoResult (*L1_UpdateUsageTable_t)();
typedef OEMCryptoResult (*L1_DeactivateUsageEntry_V12_t)(const uint8_t* pst,
                                                         size_t pst_length);
typedef OEMCryptoResult (*L1_ReportUsage_t)(OEMCrypto_SESSION session,
                                            const uint8_t* pst,
                                            size_t pst_length,
                                            uint8_t* buffer,
                                            size_t* buffer_length);
typedef OEMCryptoResult (*L1_DeleteUsageEntry_t)(
    OEMCrypto_SESSION session, const uint8_t* pst, size_t pst_length,
    const uint8_t* message, size_t message_length, const uint8_t* signature,
    size_t signature_length);
typedef OEMCryptoResult (*L1_ForceDeleteUsageEntry_t)(const uint8_t* pst,
                                                      size_t pst_length);
typedef OEMCryptoResult (*L1_DeleteOldUsageTable_t)();
typedef OEMCrypto_ProvisioningMethod (*L1_GetProvisioningMethod_t)();
typedef OEMCryptoResult (*L1_GetOEMPublicCertificate_t)(
                                  OEMCrypto_SESSION session,
                                  uint8_t *public_cert,
                                  size_t *public_cert_length);
typedef OEMCryptoResult (*L1_RewrapDeviceRSAKey30_t)(
                               OEMCrypto_SESSION session,
                               const uint32_t *nonce,
                               const uint8_t* encrypted_message_key,
                               size_t encrypted_message_key_length,
                               const uint8_t* enc_rsa_key,
                               size_t enc_rsa_key_length,
                               const uint8_t* enc_rsa_key_iv,
                               uint8_t* wrapped_rsa_key,
                               size_t* wrapped_rsa_key_length);
typedef uint32_t (*L1_SupportedCertificates_t)();
typedef bool (*L1_IsSRMUpdateSupported_t)();
typedef OEMCryptoResult (*L1_GetCurrentSRMVersion_t)(uint16_t* version);
typedef OEMCryptoResult (*L1_LoadSRM_t)(const uint8_t* buffer,
                                        size_t buffer_length);
typedef OEMCryptoResult (*L1_RemoveSRM_t)();
typedef OEMCryptoResult (*L1_CreateUsageTableHeader_t)(
    uint8_t* header_buffer, size_t* header_buffer_length);
typedef OEMCryptoResult (*L1_LoadUsageTableHeader_t)(const uint8_t* buffer,
                                                     size_t buffer_length);
typedef OEMCryptoResult (*L1_CreateNewUsageEntry_t)(
    OEMCrypto_SESSION session, uint32_t *usage_entry_number);
typedef OEMCryptoResult (*L1_LoadUsageEntry_t)(OEMCrypto_SESSION session,
                                               uint32_t index,
                                               const uint8_t *buffer,
                                               size_t buffer_size);
typedef OEMCryptoResult (*L1_UpdateUsageEntry_t)(OEMCrypto_SESSION session,
                                                 uint8_t* header_buffer,
                                                 size_t* header_buffer_length,
                                                 uint8_t* entry_buffer,
                                                 size_t* entry_buffer_length);
typedef OEMCryptoResult (*L1_DeactivateUsageEntry_t)(OEMCrypto_SESSION session,
                                                     const uint8_t* pst,
                                                     size_t pst_length);
typedef OEMCryptoResult (*L1_ShrinkUsageTableHeader_t)(
    uint32_t new_table_size,
    uint8_t* header_buffer,
    size_t* header_buffer_length);
typedef OEMCryptoResult (*L1_MoveEntry_t)(OEMCrypto_SESSION session,
                                          uint32_t new_index);
typedef OEMCryptoResult (*L1_CopyOldUsageEntry_t)(OEMCrypto_SESSION session,
                                                  const uint8_t*pst,
                                                  size_t pst_length);
typedef OEMCryptoResult (*L1_CreateOldUsageEntry_t)(
    uint64_t time_since_license_received, uint64_t time_since_first_decrypt,
    uint64_t time_since_last_decrypt, OEMCrypto_Usage_Entry_Status status,
    uint8_t* server_mac_key, uint8_t* client_mac_key, const uint8_t* pst,
    size_t pst_length);
typedef uint32_t (*L1_GetAnalogOutputFlags_t)(void);
typedef const char* (*L1_BuildInformation_t)(void);
typedef uint32_t (*L1_ResourceRatingTier_t)(void);
typedef uint32_t (*L1_SupportsDecryptHash_t)(void);
typedef OEMCryptoResult (*L1_SetDecryptHash_t)(OEMCrypto_SESSION session,
                                               uint32_t frame_number,
                                               const uint8_t* hash,
                                               size_t hash_length);
typedef OEMCryptoResult (*L1_GetHashErrorCode_t)(OEMCrypto_SESSION session,
                                                 uint32_t* failed_frame_number);

struct FunctionPointers {
  uint32_t version;
  L1_Initialize_t Initialize;
  L1_SetSandbox_t SetSandbox;
  L1_Terminate_t Terminate;
  L1_OpenSession_t OpenSession;
  L1_CloseSession_t CloseSession;
  L1_GenerateDerivedKeys_t GenerateDerivedKeys;
  L1_GenerateNonce_t GenerateNonce;
  L1_GenerateSignature_t GenerateSignature;
  L1_LoadKeys_t LoadKeys;
  L1_LoadEntitledContentKeys_t LoadEntitledContentKeys;
  L1_RefreshKeys_t RefreshKeys;
  L1_QueryKeyControl_t QueryKeyControl;
  L1_SelectKey_t SelectKey;
  L1_DecryptCTR_V10_t DecryptCTR_V10;
  L1_DecryptCENC_t DecryptCENC;
  L1_CopyBuffer_V14_t CopyBuffer_V14;
  L1_CopyBuffer_t CopyBuffer;
  L1_WrapKeybox_t WrapKeybox;
  L1_InstallKeyboxOrOEMCert_t InstallKeyboxOrOEMCert;
  L1_LoadTestKeybox_t LoadTestKeybox;
  L1_IsKeyboxOrOEMCertValid_t IsKeyboxOrOEMCertValid;
  L1_GetDeviceID_t GetDeviceID;
  L1_GetKeyData_t GetKeyData;
  L1_GetRandom_t GetRandom;
  L1_RewrapDeviceRSAKey_t RewrapDeviceRSAKey;
  L1_LoadDeviceRSAKey_t LoadDeviceRSAKey;
  L1_LoadTestRSAKey_t LoadTestRSAKey;
  L1_GenerateRSASignature_t GenerateRSASignature;
  L1_DeriveKeysFromSessionKey_t DeriveKeysFromSessionKey;
  L1_APIVersion_t APIVersion;
  L1_SecurityPatchLevel_t SecurityPatchLevel;
  L1_SecurityLevel_t SecurityLevel;
  L1_GetHDCPCapability_t GetHDCPCapability;
  L1_SupportsUsageTable_t SupportsUsageTable;
  L1_IsAntiRollbackHwPresent_t IsAntiRollbackHwPresent;
  L1_GetNumberOfOpenSessions_t GetNumberOfOpenSessions;
  L1_GetMaxNumberOfSessions_t GetMaxNumberOfSessions;
  L1_Generic_Encrypt_t Generic_Encrypt;
  L1_Generic_Decrypt_t Generic_Decrypt;
  L1_Generic_Sign_t Generic_Sign;
  L1_Generic_Verify_t Generic_Verify;
  L1_UpdateUsageTable_t UpdateUsageTable;
  L1_DeactivateUsageEntry_t DeactivateUsageEntry;
  L1_ReportUsage_t ReportUsage;
  L1_DeleteUsageEntry_t DeleteUsageEntry;
  L1_ForceDeleteUsageEntry_t ForceDeleteUsageEntry;
  L1_DeleteOldUsageTable_t DeleteOldUsageTable;
  L1_GetProvisioningMethod_t GetProvisioningMethod;
  L1_GetOEMPublicCertificate_t GetOEMPublicCertificate;
  L1_RewrapDeviceRSAKey30_t RewrapDeviceRSAKey30;
  L1_SupportedCertificates_t SupportedCertificates;
  L1_IsSRMUpdateSupported_t IsSRMUpdateSupported;
  L1_GetCurrentSRMVersion_t GetCurrentSRMVersion;
  L1_LoadSRM_t LoadSRM;
  L1_RemoveSRM_t RemoveSRM;
  L1_CreateUsageTableHeader_t CreateUsageTableHeader;
  L1_LoadUsageTableHeader_t LoadUsageTableHeader;
  L1_CreateNewUsageEntry_t CreateNewUsageEntry;
  L1_LoadUsageEntry_t LoadUsageEntry;
  L1_UpdateUsageEntry_t UpdateUsageEntry;
  L1_ShrinkUsageTableHeader_t ShrinkUsageTableHeader;
  L1_MoveEntry_t MoveEntry;
  L1_CopyOldUsageEntry_t CopyOldUsageEntry;
  L1_CreateOldUsageEntry_t CreateOldUsageEntry;
  L1_GetAnalogOutputFlags_t GetAnalogOutputFlags;
  L1_BuildInformation_t BuildInformation;
  L1_ResourceRatingTier_t ResourceRatingTier;
  L1_SupportsDecryptHash_t SupportsDecryptHash;
  L1_SetDecryptHash_t SetDecryptHash;
  L1_GetHashErrorCode_t GetHashErrorCode;

  L1_LoadKeys_V8_t LoadKeys_V8;
  L1_GenerateRSASignature_V8_t GenerateRSASignature_V8;
  L1_GetHDCPCapability_V9_t GetHDCPCapability_V9;
  L1_LoadKeys_V9_or_V10_t LoadKeys_V9_or_V10;
  L1_LoadKeys_V11_or_V12_t LoadKeys_V11_or_V12;
  L1_DeactivateUsageEntry_V12_t DeactivateUsageEntry_V12;
  L1_LoadKeys_V13_t LoadKeys_V13;
  L1_LoadKeys_V14_t LoadKeys_V14;
  L1_SelectKey_V13_t SelectKey_V13;
  L1_LoadTestKeybox_V13_t LoadTestKeybox_V13;
  L1_LoadEntitledContentKeys_V14_t LoadEntitledContentKeys_V14;
  L1_RefreshKeys_V14_t RefreshKeys_V14;
};

// The WatchDog looks after a worker thread that is trying to initialize L3.
// Once in a rare while, the L3 init does not finish and eats up CPU cycles.
// If that happens, the watchdog thread will give up and return an error.
class WatchDog {
 public:
  // Created by main thread.
  WatchDog(std::vector<uint8_t> sandbox_id) {
    status_ = OEMCrypto_SUCCESS;
    gave_up_ = false;
    sandbox_id_ = sandbox_id;
  }

  // Deleted by either thread.
  ~WatchDog() {}

  // Starts worker thread.
  void StartThread() {
    running_ = true;
    thread_ = std::thread(&RunWatchDog, this);
  }

  // Function called by new worker thread.
  static void RunWatchDog(void *watcher) {
    WatchDog* dog = reinterpret_cast<WatchDog *>(watcher);
    dog->DoInit();
    dog->SignalDoneAndCleanUp();
  }

  // Called by worker thread.
  void DoInit() {
    Level3_SetSandbox(&sandbox_id_[0], sandbox_id_.size());
    status_ = Level3_Initialize();
  }

  std::string FailureFilename() {
    std::string path;
    if (!wvcdm::Properties::GetDeviceFilesBasePath(wvcdm::kSecurityLevelL3,
                                                   &path)) {
      LOGW("WatchDog::FailureFilename: Unable to get base path");
      return "/data/l3_failure_file";
    }
    path += "l3_failure_file";
    return path;
  }

  // Check to see if the failure file was created before that last abort.
  void CheckForPreviousFailure(
      wvcdm::metrics::OemCryptoDynamicAdapterMetrics* metrics) {
    wvcdm::FileSystem file_system;
    std::string filename = FailureFilename();
    if (!file_system.Exists(filename)) return;
    auto file = file_system.Open(filename, file_system.kReadOnly);
    if (file) {
      uint32_t flag = 0;
      ssize_t size = sizeof(flag);
      ssize_t size_read = file->Read(reinterpret_cast<char*>(&flag), size);
      file_system.Remove(filename);
      if (size == size_read && flag) {
        LOGE("Previous L3 Init failed.");
        if (metrics == NULL) return;
        metrics
            ->OemCryptoDynamicAdapterMetrics::SetPreviousInitializationFailure(
                wvcdm::metrics::OEMCrypto_INITIALIZED_L3_INITIALIZATION_FAILED);
      }
    }
  }

  // Save the failure file before we abort.
  void SaveFailureInformation() {
    wvcdm::FileSystem file_system;
    std::string filename = FailureFilename();
    LOGD("failure filename = %s", filename.c_str());
    auto file =
        file_system.Open(filename, file_system.kCreate | file_system.kTruncate);
    if (!file) {
      LOGE("Could not create file %s", filename.c_str());
      return;
    }
    uint32_t flag = 0x6261640a;  // bad
    ssize_t size = sizeof(flag);
    ssize_t size_written = file->Write(reinterpret_cast<char*>(&flag), size);
    if (size != size_written) {
      LOGE("Wrote %d bytes, not %d, to file %s", size_written, size,
           filename.c_str());
    } else {
      LOGE("I wrote %d to %s", size_written, filename.c_str());
    }
  }

  // Called by worker thread after DoInit has finshed.
  void SignalDoneAndCleanUp() {
    bool should_delete;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      running_ = false;
      condition_.notify_all();
      // If the main thread gave up, it won't delete this, so we must.
      should_delete = gave_up_;
    }
    // https://isocpp.org/wiki/faq/freestore-mgmt#delete-this
    if (should_delete) delete this;
  }

  // Called by main thread to wait for worker thread.
  OEMCryptoResult WaitForStatusAndCleanUp(
      wvcdm::metrics::OemCryptoDynamicAdapterMetrics* metrics) {
    bool should_delete;
    OEMCryptoResult status;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      if (running_) {
        condition_.wait_for(lock, std::chrono::minutes(2));
      }
      if (running_) {
        gave_up_ = true;
        status_ = OEMCrypto_ERROR_INIT_FAILED;
        LOGE("XXX WATCH DOG ERROR XXX");
        Level3_OutputErrorLogs();
        SaveFailureInformation();
        // This is controversial.  The argument for an abort here is that if we
        // do not abort, we will suck all the life out of the user's battery.
        // By saving information to the file system, above, we can still track
        // metrics.
        abort();
      } else if (metrics != NULL) {
        // Even if we succeeded in calling initialize, there might be failures
        // in the initialize process that we want to record.
        wvoec3::Level3InitializationState state =
            Level3_GetInitializationState();
        switch (state) {
          case wvoec3::LEVEL3_INITIALIZATION_UNKNOWN_FAILURE:
            metrics
                ->OemCryptoDynamicAdapterMetrics::SetLevel3InitializationError(
                    wvcdm::metrics::
                        OEMCrypto_INITIALIZED_L3_INITIALIZATION_FAILED);
            break;
          case wvoec3::LEVEL3_SEED_FAILURE:
            metrics
                ->OemCryptoDynamicAdapterMetrics::SetLevel3InitializationError(
                    wvcdm::metrics::OEMCrypto_INITIALIZED_L3_RNG_FAILED);
            break;
          case wvoec3::LEVEL3_SAVE_DEVICE_KEYS_FAILURE:
            metrics
                ->OemCryptoDynamicAdapterMetrics::SetLevel3InitializationError(
                    wvcdm::metrics::
                        OEMCrypto_INITIALIZED_L3_SAVE_DEVICE_KEYS_FAILED);
            break;
          case wvoec3::LEVEL3_READ_DEVICE_KEYS_FAILURE:
            metrics
                ->OemCryptoDynamicAdapterMetrics::SetLevel3InitializationError(
                    wvcdm::metrics::
                        OEMCrypto_INITIALIZED_L3_READ_DEVICE_KEYS_FAILED);
            break;
          case wvoec3::LEVEL3_VERIFY_DEVICE_KEYS_FAILURE:
            metrics
                ->OemCryptoDynamicAdapterMetrics::SetLevel3InitializationError(
                    wvcdm::metrics::
                        OEMCrypto_INITIALIZED_L3_VERIFY_DEVICE_KEYS_FAILED);
            break;
          default:
            break;
        }
      }
      // If we gave up waiting for init thread, we should not delete the mutex
      // out from under it.
      should_delete = !gave_up_;
      status = status_;
    }
    if (should_delete) {
      thread_.join();
      delete this;
    }
    return status;
  }

  OEMCryptoResult status() { return status_; }

 private:
  OEMCryptoResult status_;
  std::thread thread_;
  std::mutex mutex_;
  std::condition_variable condition_;
  bool running_;
  bool gave_up_;
  std::vector<uint8_t> sandbox_id_;
};

struct LevelSession {
  FunctionPointers* fcn;
  OEMCrypto_SESSION session;
  LevelSession() : fcn(0), session(0) {};
};

#define QUOTE_DEFINE(A) #A
#define QUOTE(A) QUOTE_DEFINE(A)
// This macro looks up a function name, but only if the API version is in the
// specified range.
#define LOOKUP(min, max, Name, Function)                                \
  if ((level1_.version >= min) && (level1_.version <= max)) {           \
    level1_.Name = (L1_##Name##_t)dlsym(level1_library_, QUOTE(Function)); \
    if (!level1_.Name) {                                                \
      LOGW("Could not load L1 %s. Falling Back to L3.",                 \
           QUOTE(Function));                                            \
      if (level1_.Terminate) level1_.Terminate();                       \
      return false;                                                     \
    }                                                                   \
  }

// This macro looks up a function name, but only if the API version is above
// the specified minimum.
#define LOOKUP_ALL(min, Name, Function)                                \
  LOOKUP(min, kMaximumVersion, Name, Function)


// The Adapter keeps a block of FunctionPointers for the built-in level 3 and
// the dynamically loaded level 1 oemcrypto.  When initialized, it tries to load
// the level 1 library and verifies that all needed functions are present.  If
// they are not, then it flags the level 1 as invalid.  Later, when the function
// GetFunctionPointers(kLevel3) is called, it returns the level 3 function
// pointers.  When GetFunctionPointers(kLevelDefault) is called, it returns
// level 1 function pointers if level 1 is valid and otherwise returns the level
// 3 function pointers.  The Adapter is a global singleton.
class Adapter {
 public:
  typedef std::map<OEMCrypto_SESSION, LevelSession>::iterator map_iterator;

  Adapter() : level1_valid_(false), level1_library_(NULL) {}

  // The adapter is only destroyed on certain errors, or when the process
  // dies.  It is NOT deleted after each OEMCrypto_Terminate.
  ~Adapter() {
    if (level1_library_) {
      dlclose(level1_library_);
      level1_library_ = NULL;
    }
  }

  void SetSandbox(const uint8_t* sandbox_id,
                  size_t sandbox_id_length) {
    sandbox_id_.assign(sandbox_id, sandbox_id + sandbox_id_length);
  }

  OEMCryptoResult Initialize() {
    /*
     * To avoid changing the function signature and function contract, use a
     * reference to a singleton object for the metrics collected from the
     * dynamic adapter.
     */
    wvcdm::metrics::OemCryptoDynamicAdapterMetrics& metrics =
        wvcdm::metrics::GetDynamicAdapterMetricsInstance();

    level1_ = FunctionPointers();  // start with all null pointers.
    level3_ = FunctionPointers();  // start with all null pointers.
    LoadLevel3();
    WatchDog *watcher = new WatchDog(sandbox_id_);
    watcher->CheckForPreviousFailure(&metrics);
    watcher->StartThread();
    if (level3_.BuildInformation) {
      LOGI("Level 3 Build Info (v%d): %s", level3_.version,
           level3_.BuildInformation());
    }
    OEMCryptoResult result = watcher->WaitForStatusAndCleanUp(&metrics);
    if (Level3_IsInApp()) {
      metrics.SetInitializationMode(
          wvcdm::metrics::OEMCrypto_INITIALIZED_USING_IN_APP);
      return result;
    }
    if (force_level3()) {
      LOGW("Test code. User requested falling back to L3");
      metrics.SetInitializationMode(
          wvcdm::metrics::OEMCrypto_INITIALIZED_FORCING_L3);
      return result;
    }
    LOGI("L3 Initialized. Trying L1.");
    std::string library_name;
    if (!wvcdm::Properties::GetOEMCryptoPath(&library_name)) {
      LOGW("L1 library not specified. Falling back to L3");
      metrics.OemCryptoDynamicAdapterMetrics::SetInitializationMode(
          wvcdm::metrics::OEMCrypto_INITIALIZED_USING_L3_NO_L1_LIBRARY_PATH);
      return result;
    }
    if (level1_library_ == NULL) {
      level1_library_ = dlopen(library_name.c_str(), RTLD_NOW);
      if (level1_library_ == NULL) {
        LOGW("Could not load %s. Falling back to L3.  %s", library_name.c_str(),
             dlerror());
        metrics.OemCryptoDynamicAdapterMetrics::SetInitializationMode(
            wvcdm::metrics::OEMCrypto_INITIALIZED_USING_L3_L1_OPEN_FAILED);
        return result;
      }
    }
    if (LoadLevel1(&metrics)) {
      LOGD("OEMCrypto_Initialize Level 1 success. I will use level 1.");
      if (level1_.BuildInformation) {
        LOGI("Level 1 Build Info (v%d): %s", level1_.version,
             level1_.BuildInformation());
      }
    } else {
      level1_ = FunctionPointers();  // revert to all null pointers.
      level1_valid_ = false;
      // Note: if the function pointers are bad, we do not close the library and
      // try again later.  Instead, we permanently fall back to L3.  This is a
      // debatable choice: I decided the risk of a dlclose resource leak out
      // weighed the possibility that the library would be fixed on the next
      // load.
    }
    return result;
  }

  bool LoadLevel1(wvcdm::metrics::OemCryptoDynamicAdapterMetrics* metrics) {
    if (metrics == NULL) {
      return false;
    }
    level1_valid_ = true;
    const uint32_t kMinimumVersion = 8;
    const uint32_t kMaximumVersion = 15;
    level1_.version = kMinimumVersion;
    LOOKUP_ALL(8, Initialize, OEMCrypto_Initialize);
    LOOKUP_ALL(8, APIVersion, OEMCrypto_APIVersion);
    LOOKUP_ALL(8, Terminate,  OEMCrypto_Terminate);
    if (!level1_valid_) {
      metrics->OemCryptoDynamicAdapterMetrics::SetInitializationMode(
          wvcdm::metrics::OEMCrypto_INITIALIZED_USING_L3_INVALID_L1);
      return false;
    }
    if (!sandbox_id_.empty()) {
      level1_.SetSandbox = (L1_SetSandbox_t)dlsym(level1_library_,
                                                  QUOTE(OEMCrypto_SetSandbox));
      if (level1_.SetSandbox != NULL) {
        level1_.SetSandbox(&sandbox_id_[0], sandbox_id_.size());
      }
    }
    OEMCryptoResult st = level1_.Initialize();
    if (st != OEMCrypto_SUCCESS) {
      LOGW("Could not initialize L1. Falling Back to L3.");
      metrics->OemCryptoDynamicAdapterMetrics::SetInitializationMode(
          wvcdm::metrics::
              OEMCrypto_INITIALIZED_USING_L3_COULD_NOT_INITIALIZE_L1);
      return false;
    }
    level1_.version = level1_.APIVersion();
    metrics->SetL1ApiVersion(level1_.version);
    metrics->SetL1MinApiVersion(kMinimumVersion);

    if (level1_.version < kMinimumVersion) {
      LOGW("liboemcrypto.so is version %d, not %d. Falling Back to L3.",
           level1_.version, kMinimumVersion);
      metrics->OemCryptoDynamicAdapterMetrics::SetInitializationMode(
          wvcdm::metrics::OEMCrypto_INITIALIZED_USING_L3_WRONG_L1_VERSION);
      level1_.Terminate();
      return false;
    }

    // clang-format off
    LOOKUP_ALL( 8, CloseSession,               OEMCrypto_CloseSession);
    LOOKUP(10, 14, CopyBuffer_V14,             OEMCrypto_CopyBuffer_V14);
    LOOKUP_ALL(15, CopyBuffer,                 OEMCrypto_CopyBuffer);
    LOOKUP_ALL(13, CopyOldUsageEntry,          OEMCrypto_CopyOldUsageEntry);
    LOOKUP_ALL(13, CreateNewUsageEntry,        OEMCrypto_CreateNewUsageEntry);
    LOOKUP_ALL(13, CreateOldUsageEntry,        OEMCrypto_CreateOldUsageEntry);
    LOOKUP_ALL(13, CreateUsageTableHeader,     OEMCrypto_CreateUsageTableHeader);
    LOOKUP( 9, 12, DeactivateUsageEntry_V12,   OEMCrypto_DeactivateUsageEntry_V12);
    LOOKUP_ALL(13, DeactivateUsageEntry,       OEMCrypto_DeactivateUsageEntry);
    LOOKUP( 8, 10, DecryptCTR_V10,             OEMCrypto_DecryptCTR_V10);
    LOOKUP_ALL(11, DecryptCENC,                OEMCrypto_DecryptCENC);
    LOOKUP_ALL( 9, DeleteOldUsageTable,        OEMCrypto_DeleteOldUsageTable);
    LOOKUP( 9, 12, DeleteUsageEntry,           OEMCrypto_DeleteUsageEntry);
    LOOKUP_ALL( 8, DeriveKeysFromSessionKey,   OEMCrypto_DeriveKeysFromSessionKey);
    LOOKUP(10, 12, ForceDeleteUsageEntry,      OEMCrypto_ForceDeleteUsageEntry);
    LOOKUP_ALL( 8, GenerateDerivedKeys,        OEMCrypto_GenerateDerivedKeys);
    LOOKUP_ALL( 8, GenerateNonce,              OEMCrypto_GenerateNonce);
    LOOKUP( 8,  8, GenerateRSASignature_V8,    OEMCrypto_GenerateRSASignature_V8);
    LOOKUP_ALL( 9, GenerateRSASignature,       OEMCrypto_GenerateRSASignature);
    LOOKUP_ALL( 8, GenerateSignature,          OEMCrypto_GenerateSignature);
    LOOKUP_ALL( 8, Generic_Decrypt,            OEMCrypto_Generic_Decrypt);
    LOOKUP_ALL( 8, Generic_Encrypt,            OEMCrypto_Generic_Encrypt);
    LOOKUP_ALL( 8, Generic_Sign,               OEMCrypto_Generic_Sign);
    LOOKUP_ALL( 8, Generic_Verify,             OEMCrypto_Generic_Verify);
    LOOKUP_ALL(13, GetCurrentSRMVersion,       OEMCrypto_GetCurrentSRMVersion);
    LOOKUP_ALL( 8, GetDeviceID,                OEMCrypto_GetDeviceID);
    LOOKUP( 9,  9, GetHDCPCapability_V9,       OEMCrypto_GetHDCPCapability_V9);
    LOOKUP_ALL(10, GetHDCPCapability,          OEMCrypto_GetHDCPCapability);
    LOOKUP_ALL(14, GetAnalogOutputFlags,       OEMCrypto_GetAnalogOutputFlags);
    LOOKUP_ALL(15, BuildInformation,           OEMCrypto_BuildInformation);
    LOOKUP_ALL(15, ResourceRatingTier,         OEMCrypto_ResourceRatingTier);
    LOOKUP_ALL( 8, GetKeyData,                 OEMCrypto_GetKeyData);
    LOOKUP_ALL(10, GetMaxNumberOfSessions,     OEMCrypto_GetMaxNumberOfSessions);
    LOOKUP_ALL(10, GetNumberOfOpenSessions,    OEMCrypto_GetNumberOfOpenSessions);
    LOOKUP_ALL(12, GetOEMPublicCertificate,    OEMCrypto_GetOEMPublicCertificate);
    LOOKUP_ALL(12, GetProvisioningMethod,      OEMCrypto_GetProvisioningMethod);
    LOOKUP_ALL( 8, GetRandom,                  OEMCrypto_GetRandom);
    LOOKUP_ALL( 8, InstallKeyboxOrOEMCert,     OEMCrypto_InstallKeyboxOrOEMCert);
    LOOKUP_ALL(10, IsAntiRollbackHwPresent,    OEMCrypto_IsAntiRollbackHwPresent);
    LOOKUP_ALL( 8, IsKeyboxOrOEMCertValid,     OEMCrypto_IsKeyboxOrOEMCertValid);
    LOOKUP_ALL(13, IsSRMUpdateSupported,       OEMCrypto_IsSRMUpdateSupported);
    LOOKUP_ALL( 8, LoadDeviceRSAKey,           OEMCrypto_LoadDeviceRSAKey);
    LOOKUP( 8,  8, LoadKeys_V8,                OEMCrypto_LoadKeys_V8);
    LOOKUP( 9, 10, LoadKeys_V9_or_V10,         OEMCrypto_LoadKeys_V9_or_V10);
    LOOKUP(11, 12, LoadKeys_V11_or_V12,        OEMCrypto_LoadKeys_V11_or_V12);
    LOOKUP(13, 13, LoadKeys_V13,               OEMCrypto_LoadKeys_V13);
    LOOKUP(14, 14, LoadKeys_V14,               OEMCrypto_LoadKeys_V14);
    LOOKUP_ALL(15, LoadKeys,                   OEMCrypto_LoadKeys);
    LOOKUP(14, 14, LoadEntitledContentKeys_V14,OEMCrypto_LoadEntitledContentKeys_V14);
    LOOKUP_ALL(15, LoadEntitledContentKeys,    OEMCrypto_LoadEntitledContentKeys);
    LOOKUP_ALL(13, LoadSRM,                    OEMCrypto_LoadSRM);
    LOOKUP(10, 13, LoadTestKeybox_V13,         OEMCrypto_LoadTestKeybox_V13);
    LOOKUP_ALL(14, LoadTestKeybox,             OEMCrypto_LoadTestKeybox);
    LOOKUP_ALL(10, LoadTestRSAKey,             OEMCrypto_LoadTestRSAKey);
    LOOKUP_ALL(13, LoadUsageEntry,             OEMCrypto_LoadUsageEntry);
    LOOKUP_ALL(13, LoadUsageTableHeader,       OEMCrypto_LoadUsageTableHeader);
    LOOKUP_ALL(13, MoveEntry,                  OEMCrypto_MoveEntry);
    LOOKUP_ALL( 8, OpenSession,                OEMCrypto_OpenSession);
    LOOKUP_ALL(10, QueryKeyControl,            OEMCrypto_QueryKeyControl);
    LOOKUP( 8, 14, RefreshKeys_V14,            OEMCrypto_RefreshKeys_V14);
    LOOKUP_ALL(15, RefreshKeys,                OEMCrypto_RefreshKeys);
    LOOKUP_ALL(13, RemoveSRM,                  OEMCrypto_RemoveSRM);
    LOOKUP_ALL( 9, ReportUsage,                OEMCrypto_ReportUsage);
    LOOKUP_ALL( 8, RewrapDeviceRSAKey,         OEMCrypto_RewrapDeviceRSAKey);
    LOOKUP_ALL(12, RewrapDeviceRSAKey30,       OEMCrypto_RewrapDeviceRSAKey30);
    LOOKUP_ALL( 8, SecurityLevel,              OEMCrypto_SecurityLevel);
    LOOKUP_ALL(11, SecurityPatchLevel,         OEMCrypto_Security_Patch_Level);
    LOOKUP( 8, 13, SelectKey_V13,              OEMCrypto_SelectKey_V13);
    LOOKUP_ALL(14, SelectKey,                  OEMCrypto_SelectKey);
    LOOKUP_ALL(13, ShrinkUsageTableHeader,     OEMCrypto_ShrinkUsageTableHeader);
    LOOKUP_ALL(13, SupportedCertificates,      OEMCrypto_SupportedCertificates);
    LOOKUP_ALL( 9, SupportsUsageTable,         OEMCrypto_SupportsUsageTable);
    LOOKUP_ALL(13, UpdateUsageEntry,           OEMCrypto_UpdateUsageEntry);
    LOOKUP( 9, 12, UpdateUsageTable,           OEMCrypto_UpdateUsageTable);
    LOOKUP_ALL( 8, WrapKeybox,                 OEMCrypto_WrapKeybox);
    LOOKUP_ALL(15, SupportsDecryptHash,        OEMCrypto_SupportsDecryptHash);
    LOOKUP_ALL(15, SetDecryptHash,             OEMCrypto_SetDecryptHash);
    LOOKUP_ALL(15, GetHashErrorCode,           OEMCrypto_GetHashErrorCode);
    // clang-format on

    // TODO(119830252): make the code below available to a static adapter.
    // Check if the keybox or oem certificate is valid, if so, we are finished
    // with initialization.
    OEMCryptoResult root_valid = level1_.IsKeyboxOrOEMCertValid();
    OEMCrypto_ProvisioningMethod provisioning_method =
        level1_.GetProvisioningMethod();
    if (root_valid == OEMCrypto_SUCCESS) {
      // The keybox or certificate is valid -- that means initialization is done
      // and we only have save some metrics and return.
      metrics->OemCryptoDynamicAdapterMetrics::SetInitializationMode(
          (provisioning_method == OEMCrypto_Keybox)
              ? wvcdm::metrics::OEMCrypto_INITIALIZED_USING_L1_WITH_KEYBOX
              : wvcdm::metrics::
                    OEMCrypto_INITIALIZED_USING_L1_WITH_PROVISIONING_3_0);
      return true;
    }
    // At this point, the keybox or cert is not valid.  We look on the file
    // system for one.  If it is there we try to install it.
    wvcdm::FileSystem file_system;
    std::string filename;
    if (!wvcdm::Properties::GetFactoryKeyboxPath(&filename)) {
      // No keybox or cert file found.  Give up.
      LOGW("Bad Level 1 Root of Trust. Falling Back to L3.");
      level1_.Terminate();
      metrics->OemCryptoDynamicAdapterMetrics::SetInitializationMode(
          wvcdm::metrics::OEMCrypto_INITIALIZED_USING_L3_BAD_KEYBOX);
      return false;
    }
    ssize_t size = file_system.FileSize(filename);
    if (size <= 0) {
      // A keybox or cert file was found, but it has size 0.  Give up.
      LOGW("Could not find %s. Falling Back to L3.", filename.c_str());
      level1_.Terminate();
      metrics->OemCryptoDynamicAdapterMetrics::SetInitializationMode(
          wvcdm::metrics::
              OEMCrypto_INITIALIZED_USING_L3_COULD_NOT_OPEN_FACTORY_KEYBOX);
      return false;
    }
    auto file = file_system.Open(filename, file_system.kReadOnly);
    if (!file) {
      // A keybox or cert file was found, but can't open it.  Give up.
      LOGW("Could not open %s. Falling Back to L3.", filename.c_str());
      level1_.Terminate();
      metrics->OemCryptoDynamicAdapterMetrics::SetInitializationMode(
          wvcdm::metrics::
              OEMCrypto_INITIALIZED_USING_L3_COULD_NOT_OPEN_FACTORY_KEYBOX);
      return false;
    }
    std::vector<uint8_t> root_key(size);
    ssize_t size_read = file->Read(reinterpret_cast<char*>(&root_key[0]), size);
    if (level1_.InstallKeyboxOrOEMCert(&root_key[0], size_read) !=
        OEMCrypto_SUCCESS) {
      // A keybox or cert file was read, but I could not install it. Give up.
      LOGE("Could NOT install root key from %s. Falling Back to L3.",
           filename.c_str());
      level1_.Terminate();
      metrics->OemCryptoDynamicAdapterMetrics::SetInitializationMode(
          wvcdm::metrics::
              OEMCrypto_INITIALIZED_USING_L3_COULD_NOT_INSTALL_KEYBOX);
      return false;
    }
    if (level1_.IsKeyboxOrOEMCertValid() != OEMCrypto_SUCCESS) {
      // A keybox or cert file was read and installed, but it is still not
      // valid. Give up.
      LOGE("Installed bad key from %s. Falling Back to L3.",
           filename.c_str());
      level1_.Terminate();
      metrics->OemCryptoDynamicAdapterMetrics::SetInitializationMode(
          wvcdm::metrics::
              OEMCrypto_INITIALIZED_USING_L3_COULD_NOT_INSTALL_KEYBOX);
      return false;
    }
    // A valid keybox or cert file was read and installed. Yay! return success.
    LOGI("Installed root key from %s", filename.c_str());
    metrics->OemCryptoDynamicAdapterMetrics::SetInitializationMode(
        wvcdm::metrics::OEMCrypto_INITIALIZED_USING_L1_INSTALLED_KEYBOX);
    return true;
  }

  void LoadLevel3() {
    // clang-format off
    level3_.Terminate                  = Level3_Terminate;
    level3_.OpenSession                = Level3_OpenSession;
    level3_.CloseSession               = Level3_CloseSession;
    level3_.GenerateDerivedKeys        = Level3_GenerateDerivedKeys;
    level3_.GenerateNonce              = Level3_GenerateNonce;
    level3_.GenerateSignature          = Level3_GenerateSignature;
    level3_.LoadKeys                   = Level3_LoadKeys;
    level3_.LoadEntitledContentKeys    = Level3_LoadEntitledContentKeys;
    level3_.RefreshKeys                = Level3_RefreshKeys;
    level3_.QueryKeyControl            = Level3_QueryKeyControl;
    level3_.SelectKey                  = Level3_SelectKey;
    level3_.DecryptCENC                = Level3_DecryptCENC;
    level3_.CopyBuffer                 = Level3_CopyBuffer;
    level3_.WrapKeybox                 = Level3_WrapKeyboxOrOEMCert;
    level3_.InstallKeyboxOrOEMCert     = Level3_InstallKeyboxOrOEMCert;
    level3_.LoadTestKeybox             = Level3_LoadTestKeybox;
    level3_.IsKeyboxOrOEMCertValid     = Level3_IsKeyboxOrOEMCertValid;
    level3_.GetDeviceID                = Level3_GetDeviceID;
    level3_.GetKeyData                 = Level3_GetKeyData;
    level3_.GetRandom                  = Level3_GetRandom;
    level3_.RewrapDeviceRSAKey         = Level3_RewrapDeviceRSAKey;
    level3_.LoadDeviceRSAKey           = Level3_LoadDeviceRSAKey;
    level3_.LoadTestRSAKey             = Level3_LoadTestRSAKey;
    level3_.GenerateRSASignature       = Level3_GenerateRSASignature;
    level3_.DeriveKeysFromSessionKey   = Level3_DeriveKeysFromSessionKey;
    level3_.APIVersion                 = Level3_APIVersion;
    level3_.SecurityPatchLevel         = Level3_SecurityPatchLevel;
    level3_.SecurityLevel              = Level3_SecurityLevel;
    level3_.GetHDCPCapability          = Level3_GetHDCPCapability;
    level3_.GetAnalogOutputFlags       = Level3_GetAnalogOutputFlags;
    level3_.BuildInformation           = Level3_BuildInformation;
    level3_.ResourceRatingTier         = Level3_ResourceRatingTier;
    level3_.SupportsUsageTable         = Level3_SupportsUsageTable;
    level3_.IsAntiRollbackHwPresent    = Level3_IsAntiRollbackHwPresent;
    level3_.GetNumberOfOpenSessions    = Level3_GetNumberOfOpenSessions;
    level3_.GetMaxNumberOfSessions     = Level3_GetMaxNumberOfSessions;
    level3_.Generic_Decrypt            = Level3_Generic_Decrypt;
    level3_.Generic_Encrypt            = Level3_Generic_Encrypt;
    level3_.Generic_Sign               = Level3_Generic_Sign;
    level3_.Generic_Verify             = Level3_Generic_Verify;
    level3_.DeactivateUsageEntry       = Level3_DeactivateUsageEntry;
    level3_.ReportUsage                = Level3_ReportUsage;
    level3_.DeleteOldUsageTable        = Level3_DeleteOldUsageTable;
    level3_.GetProvisioningMethod      = Level3_GetProvisioningMethod;
    level3_.GetOEMPublicCertificate    = Level3_GetOEMPublicCertificate;
    level3_.RewrapDeviceRSAKey30       = Level3_RewrapDeviceRSAKey30;
    level3_.SupportedCertificates      = Level3_SupportedCertificates;
    level3_.IsSRMUpdateSupported       = Level3_IsSRMUpdateSupported;
    level3_.GetCurrentSRMVersion       = Level3_GetCurrentSRMVersion;
    level3_.LoadSRM                    = Level3_LoadSRM;
    level3_.RemoveSRM                  = Level3_RemoveSRM;
    level3_.CreateUsageTableHeader     = Level3_CreateUsageTableHeader;
    level3_.LoadUsageTableHeader       = Level3_LoadUsageTableHeader;
    level3_.CreateNewUsageEntry        = Level3_CreateNewUsageEntry;
    level3_.LoadUsageEntry             = Level3_LoadUsageEntry;
    level3_.UpdateUsageEntry           = Level3_UpdateUsageEntry;
    level3_.ShrinkUsageTableHeader     = Level3_ShrinkUsageTableHeader;
    level3_.MoveEntry                  = Level3_MoveEntry;
    level3_.CopyOldUsageEntry          = Level3_CopyOldUsageEntry;
    level3_.CreateOldUsageEntry        = Level3_CreateOldUsageEntry;
    level3_.SupportsDecryptHash        = Level3_SupportsDecryptHash;
    level3_.SetDecryptHash             = Level3_SetDecryptHash;
    level3_.GetHashErrorCode           = Level3_GetHashErrorCode;
    // clang-format on

    level3_.version = Level3_APIVersion();
  }

  OEMCryptoResult Terminate() {
    for (map_iterator i = session_map_.begin(); i != session_map_.end(); ++i) {
      if (i->second.fcn) i->second.fcn->CloseSession(i->second.session);
    }
    session_map_.clear();
    OEMCryptoResult result = Level3_Terminate();
    if (level1_valid_) {
      result = level1_.Terminate();
    }
    return result;
  }

  const FunctionPointers* GetFunctionPointers(wvcdm::SecurityLevel level) {
    if (level1_valid_ && level == kLevelDefault) return &level1_;
    return &level3_;
  }

  LevelSession GetSession(OEMCrypto_SESSION session) {
    std::unique_lock<std::mutex> auto_lock(session_map_lock_);
    map_iterator pair = session_map_.find(session);
    if (pair == session_map_.end()) {
      return LevelSession();
    }
    return pair->second;
  }

  OEMCryptoResult OpenSession(OEMCrypto_SESSION* session,
                              wvcdm::SecurityLevel level) {
    LevelSession new_session;
    OEMCryptoResult result;
    if (level == kLevelDefault && level1_valid_) {
      new_session.fcn = &level1_;
      result = level1_.OpenSession(&new_session.session);
      *session = new_session.session;
    } else {
      new_session.fcn = &level3_;
      result = level3_.OpenSession(&new_session.session);
      *session = new_session.session + kLevel3Offset;
    }
    if (result == OEMCrypto_SUCCESS) {
      std::unique_lock<std::mutex> auto_lock(session_map_lock_);
      // Make sure session is not already in my list of sessions.
      while (session_map_.find(*session) != session_map_.end()) {
        (*session)++;
      }
      session_map_[*session] = new_session;
    }

    return result;
  }

  OEMCryptoResult CloseSession(OEMCrypto_SESSION session) {
    std::unique_lock<std::mutex> auto_lock(session_map_lock_);
    map_iterator pair = session_map_.find(session);
    if (pair == session_map_.end()) {
      return OEMCrypto_ERROR_INVALID_SESSION;
    }
    OEMCryptoResult result =
        pair->second.fcn->CloseSession(pair->second.session);
    session_map_.erase(pair);

    return result;
  }

 private:
  bool level1_valid_;
  void* level1_library_;
  struct FunctionPointers level1_;
  struct FunctionPointers level3_;
  std::map<OEMCrypto_SESSION, LevelSession> session_map_;
  std::mutex session_map_lock_;
  std::vector<uint8_t> sandbox_id_;
  // This is just for debugging the map between session ids.
  // If we add this to the level 3 session id, then the external session
  // id will match the internal session id in the last two digits.
  static const OEMCrypto_SESSION kLevel3Offset = 25600;

  // For running the unit tests using the level 3 oemcrypto.  If the user sets
  // the environment FORCE_LEVEL3_OEMCRYPTO, we ignore the level 1 library.
  bool force_level3() {
    const char* var = getenv("FORCE_LEVEL3_OEMCRYPTO");
    if (!var) return false;
    return !strcmp(var, "yes");
  }
};

static std::unique_ptr<Adapter> gAdapter;
}  // namespace

namespace wvcdm {

OEMCryptoResult OEMCrypto_OpenSession(OEMCrypto_SESSION* session,
                                      SecurityLevel level) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_OPEN_SESSION_FAILED;
  return gAdapter->OpenSession(session, level);
}

OEMCryptoResult OEMCrypto_InstallKeyboxOrOEMCert(const uint8_t* keybox,
                                        size_t keyBoxLength,
                                        SecurityLevel level) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  const FunctionPointers* fcn = gAdapter->GetFunctionPointers(level);
  if (!fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (fcn->InstallKeyboxOrOEMCert == NULL)
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  return fcn->InstallKeyboxOrOEMCert(keybox, keyBoxLength);
}

OEMCrypto_ProvisioningMethod OEMCrypto_GetProvisioningMethod(
    SecurityLevel level) {
  if (!gAdapter.get()) return OEMCrypto_ProvisioningError;
  const FunctionPointers* fcn = gAdapter->GetFunctionPointers(level);
  if (!fcn) return OEMCrypto_ProvisioningError;
  if (fcn->version < 12) return OEMCrypto_Keybox;
  if (fcn->GetProvisioningMethod == NULL) return OEMCrypto_Keybox;
  return fcn->GetProvisioningMethod();
}

OEMCryptoResult OEMCrypto_IsKeyboxOrOEMCertValid(SecurityLevel level) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  const FunctionPointers* fcn = gAdapter->GetFunctionPointers(level);
  if (!fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (fcn->IsKeyboxOrOEMCertValid == NULL)
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  return fcn->IsKeyboxOrOEMCertValid();
}

OEMCryptoResult OEMCrypto_GetDeviceID(uint8_t* deviceID, size_t* idLength,
                                      SecurityLevel level) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  const FunctionPointers* fcn = gAdapter->GetFunctionPointers(level);
  if (!fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (fcn->GetDeviceID == NULL) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  return fcn->GetDeviceID(deviceID, idLength);
}

OEMCryptoResult OEMCrypto_GetKeyData(uint8_t* keyData, size_t* keyDataLength,
                                     SecurityLevel level) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  const FunctionPointers* fcn = gAdapter->GetFunctionPointers(level);
  if (!fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (fcn->GetKeyData == NULL) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  return fcn->GetKeyData(keyData, keyDataLength);
}

uint32_t OEMCrypto_APIVersion(SecurityLevel level) {
  if (!gAdapter.get()) return 0;
  const FunctionPointers* fcn = gAdapter->GetFunctionPointers(level);
  if (!fcn) return 0;
  if (fcn->APIVersion == NULL) return 0;
  return fcn->APIVersion();
}

uint8_t OEMCrypto_Security_Patch_Level(SecurityLevel level) {
  if (!gAdapter.get()) return 0;
  const FunctionPointers* fcn = gAdapter->GetFunctionPointers(level);
  if (!fcn) return 0;
  if (fcn->version < 11) return 0;
  if (fcn->SecurityPatchLevel == NULL) return 0;
  return fcn->SecurityPatchLevel();
}

const char* OEMCrypto_SecurityLevel(SecurityLevel level) {
  if (!gAdapter.get()) return "";
  const FunctionPointers* fcn = gAdapter->GetFunctionPointers(level);
  if (!fcn) return "";
  if (fcn->SecurityLevel == NULL) return "";
  return fcn->SecurityLevel();
}

OEMCryptoResult OEMCrypto_GetHDCPCapability(
    SecurityLevel level, OEMCrypto_HDCP_Capability* current,
    OEMCrypto_HDCP_Capability* maximum) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  const FunctionPointers* fcn = gAdapter->GetFunctionPointers(level);
  if (!fcn) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  if (fcn->version == 8) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  if (fcn->version == 9) {
    if (current == NULL) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
    if (maximum == NULL) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
    uint8_t current_byte, maximum_byte;
    if (fcn->GetHDCPCapability_V9 == NULL)
      return OEMCrypto_ERROR_NOT_IMPLEMENTED;
    OEMCryptoResult sts =
        fcn->GetHDCPCapability_V9(&current_byte, &maximum_byte);
    *current = static_cast<OEMCrypto_HDCP_Capability>(current_byte);
    *maximum = static_cast<OEMCrypto_HDCP_Capability>(maximum_byte);
    return sts;
  } else {
    if (fcn->GetHDCPCapability == NULL) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
    return fcn->GetHDCPCapability(current, maximum);
  }
}

uint32_t OEMCrypto_GetAnalogOutputFlags(SecurityLevel level) {
  if (!gAdapter.get()) return OEMCrypto_Unknown_Analog_Output;
  const FunctionPointers* fcn = gAdapter->GetFunctionPointers(level);
  if (!fcn) return OEMCrypto_Unknown_Analog_Output;
  if (fcn->version < 14) return OEMCrypto_Unknown_Analog_Output;
  if (fcn->GetAnalogOutputFlags == NULL) return OEMCrypto_Unknown_Analog_Output;
  return fcn->GetAnalogOutputFlags();
}

const char* OEMCrypto_BuildInformation(SecurityLevel level) {
  if (!gAdapter.get()) return "<not initialized>";
  const FunctionPointers* fcn = gAdapter->GetFunctionPointers(level);
  if (!fcn) return "<not initialized>";
  if (fcn->version < 14) return "pre v15";
  if (fcn->BuildInformation == NULL) return "unknown";
  return fcn->BuildInformation();
}

uint32_t OEMCrypto_ResourceRatingTier(SecurityLevel level) {
  if (!gAdapter.get()) return 0;
  const FunctionPointers* fcn = gAdapter->GetFunctionPointers(level);
  if (!fcn) return 0;
  if (fcn->version <= 14) return 0;
  if (fcn->ResourceRatingTier == NULL) return 0;
  return fcn->ResourceRatingTier();
}

uint32_t OEMCrypto_SupportsDecryptHash(SecurityLevel level) {
  if (!gAdapter.get()) return OEMCrypto_Hash_Not_Supported;
  const FunctionPointers* fcn = gAdapter->GetFunctionPointers(level);
  if (!fcn) return OEMCrypto_Hash_Not_Supported;
  if (fcn->version < 15) return OEMCrypto_Hash_Not_Supported;
  if (fcn->BuildInformation == NULL) return OEMCrypto_Hash_Not_Supported;
  return fcn->SupportsDecryptHash();
}

bool OEMCrypto_SupportsUsageTable(SecurityLevel level) {
  if (!gAdapter.get()) return false;
  const FunctionPointers* fcn = gAdapter->GetFunctionPointers(level);
  if (!fcn) return false;
  if (fcn->version == 8) return false;
  if (fcn->SupportsUsageTable == NULL) return false;
  return fcn->SupportsUsageTable();
}

bool OEMCrypto_IsAntiRollbackHwPresent(SecurityLevel level) {
  if (!gAdapter.get()) return false;
  const FunctionPointers* fcn = gAdapter->GetFunctionPointers(level);
  if (!fcn) return false;
  if (fcn->version < 10) return false;
  if (fcn->IsAntiRollbackHwPresent == NULL) return false;
  return fcn->IsAntiRollbackHwPresent();
}

OEMCryptoResult OEMCrypto_GetNumberOfOpenSessions(SecurityLevel level,
                                                  size_t* count) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  const FunctionPointers* fcn = gAdapter->GetFunctionPointers(level);
  if (!fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (fcn->version < 10) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  return fcn->GetNumberOfOpenSessions(count);
}

OEMCryptoResult OEMCrypto_GetMaxNumberOfSessions(SecurityLevel level,
                                                 size_t* maximum) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  const FunctionPointers* fcn = gAdapter->GetFunctionPointers(level);
  if (!fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (fcn->version < 10) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  if (fcn->GetMaxNumberOfSessions == NULL)
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  return fcn->GetMaxNumberOfSessions(maximum);
}

uint32_t OEMCrypto_SupportedCertificates(SecurityLevel level) {
  if (!gAdapter.get()) return 0;
  const FunctionPointers* fcn = gAdapter->GetFunctionPointers(level);
  if (!fcn) return 0;
  // Default is to support 2048 bit RSA keys.
  if (fcn->version < 13) return OEMCrypto_Supports_RSA_2048bit;
  if (fcn->SupportedCertificates == NULL) return OEMCrypto_Supports_RSA_2048bit;
  return fcn->SupportedCertificates();
}

OEMCryptoResult OEMCrypto_CreateUsageTableHeader(SecurityLevel level,
                                                 uint8_t* header_buffer,
                                                 size_t* header_buffer_length) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  const FunctionPointers* fcn = gAdapter->GetFunctionPointers(level);
  if (!fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (fcn->version < 13) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  if (fcn->CreateUsageTableHeader == NULL)
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  return fcn->CreateUsageTableHeader(header_buffer, header_buffer_length);
}

OEMCryptoResult OEMCrypto_LoadUsageTableHeader(SecurityLevel level,
                                               const uint8_t* buffer,
                                               size_t buffer_length) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  const FunctionPointers* fcn = gAdapter->GetFunctionPointers(level);
  if (!fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (fcn->version < 13) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  if (fcn->LoadUsageTableHeader == NULL) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  return fcn->LoadUsageTableHeader(buffer, buffer_length);
}

OEMCryptoResult OEMCrypto_ShrinkUsageTableHeader(SecurityLevel level,
                                                 uint32_t new_table_size,
                                                 uint8_t* header_buffer,
                                                 size_t* header_buffer_length) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  const FunctionPointers* fcn = gAdapter->GetFunctionPointers(level);
  if (!fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (fcn->version < 13) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  if (fcn->ShrinkUsageTableHeader == NULL)
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  return fcn->ShrinkUsageTableHeader(new_table_size, header_buffer,
                                     header_buffer_length);
}

OEMCryptoResult OEMCrypto_CreateOldUsageEntry(
    SecurityLevel level, uint64_t time_since_license_received,
    uint64_t time_since_first_decrypt, uint64_t time_since_last_decrypt,
    OEMCrypto_Usage_Entry_Status status, uint8_t* server_mac_key,
    uint8_t* client_mac_key, const uint8_t* pst, size_t pst_length) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  const FunctionPointers* fcn = gAdapter->GetFunctionPointers(level);
  if (!fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (fcn->version < 13) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  if (fcn->CreateOldUsageEntry == NULL) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  return fcn->CreateOldUsageEntry(
      time_since_license_received, time_since_first_decrypt,
      time_since_last_decrypt, status, server_mac_key, client_mac_key, pst,
      pst_length);
}
}  // namespace wvcdm

extern "C" OEMCryptoResult OEMCrypto_SetSandbox(const uint8_t* sandbox_id,
                                                size_t sandbox_id_length) {
  if (!gAdapter.get()) {
    gAdapter.reset(new Adapter());
  }
  gAdapter->SetSandbox(sandbox_id, sandbox_id_length);
  return OEMCrypto_SUCCESS;
}


extern "C" OEMCryptoResult OEMCrypto_Initialize(void) {
  if (!gAdapter.get()) {
    gAdapter.reset(new Adapter());
  }
  return gAdapter->Initialize();
}

extern "C" OEMCryptoResult OEMCrypto_Terminate(void) {
  OEMCryptoResult result = OEMCrypto_SUCCESS;
  if (gAdapter.get()) {
    result = gAdapter->Terminate();
  }
  return result;
}

extern "C" OEMCryptoResult OEMCrypto_OpenSession(OEMCrypto_SESSION* session) {
  return OEMCrypto_OpenSession(session, kLevelDefault);
}

extern "C" OEMCryptoResult OEMCrypto_CloseSession(OEMCrypto_SESSION session) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_CLOSE_SESSION_FAILED;
  return gAdapter->CloseSession(session);
}

extern "C" OEMCryptoResult OEMCrypto_GenerateDerivedKeys(
    OEMCrypto_SESSION session, const uint8_t* mac_key_context,
    uint32_t mac_key_context_length, const uint8_t* enc_key_context,
    uint32_t enc_key_context_length) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  LevelSession pair = gAdapter->GetSession(session);
  if (!pair.fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (pair.fcn->GenerateDerivedKeys == NULL)
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  return pair.fcn->GenerateDerivedKeys(pair.session, mac_key_context,
                                       mac_key_context_length, enc_key_context,
                                       enc_key_context_length);
}

extern "C" OEMCryptoResult OEMCrypto_GenerateNonce(OEMCrypto_SESSION session,
                                                   uint32_t* nonce) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  LevelSession pair = gAdapter->GetSession(session);
  if (!pair.fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (pair.fcn->GenerateNonce == NULL) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  return pair.fcn->GenerateNonce(pair.session, nonce);
}

extern "C" OEMCryptoResult OEMCrypto_GenerateSignature(
    OEMCrypto_SESSION session, const uint8_t* message, size_t message_length,
    uint8_t* signature, size_t* signature_length) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  LevelSession pair = gAdapter->GetSession(session);
  if (!pair.fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (pair.fcn->GenerateSignature == NULL)
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  return pair.fcn->GenerateSignature(pair.session, message, message_length,
                                     signature, signature_length);
}

extern "C" uint32_t OEMCrypto_GetAnalogOutputFlags() {
  return OEMCrypto_GetAnalogOutputFlags(kLevelDefault);
}

extern "C" const char* OEMCrypto_BuildInformation() {
  return OEMCrypto_BuildInformation(kLevelDefault);
}

extern "C" uint32_t OEMCrypto_ResourceRatingTier() {
  return OEMCrypto_ResourceRatingTier(kLevelDefault);
}

// Used for backwards compatibility. If the length is 0, this denotes a NULL
// pointer for OEMCrypto v15.
const uint8_t* PointerOrNull(const uint8_t* pointer, size_t length) {
  return length ? pointer : NULL;
}

extern "C" OEMCryptoResult OEMCrypto_LoadKeys_Back_Compat(
    OEMCrypto_SESSION session, const uint8_t* message, size_t message_length,
    const uint8_t* signature, size_t signature_length,
    OEMCrypto_Substring enc_mac_keys_iv, OEMCrypto_Substring enc_mac_keys,
    size_t num_keys, const OEMCrypto_KeyObject* key_array,
    OEMCrypto_Substring pst, OEMCrypto_Substring srm_restriction_data,
    OEMCrypto_LicenseType license_type, OEMCryptoCipherMode* cipher_modes) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  LevelSession pair = gAdapter->GetSession(session);
  if (!pair.fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (pair.fcn->version < 11) {
    if (license_type != OEMCrypto_ContentLicense)
      return OEMCrypto_ERROR_NOT_IMPLEMENTED;
    std::vector<OEMCrypto_KeyObject_V10> key_array_v10(num_keys);
    for (size_t i = 0; i < num_keys; i++) {
      key_array_v10[i].key_id = PointerOrNull(
          message + key_array[i].key_id.offset, key_array[i].key_id.length);
      key_array_v10[i].key_id_length = key_array[i].key_id.length;
      key_array_v10[i].key_data_iv =
          PointerOrNull(message + key_array[i].key_data_iv.offset,
                        key_array[i].key_data_iv.length);
      key_array_v10[i].key_data = PointerOrNull(
          message + key_array[i].key_data.offset, key_array[i].key_data.length);
      key_array_v10[i].key_data_length = key_array[i].key_data.length;
      key_array_v10[i].key_control_iv =
          PointerOrNull(message + key_array[i].key_control_iv.offset,
                        key_array[i].key_control_iv.length);
      key_array_v10[i].key_control =
          PointerOrNull(message + key_array[i].key_control.offset,
                        key_array[i].key_control.length);
      if (cipher_modes[i] == OEMCrypto_CipherMode_CBC) {
        LOGE("CBC Mode not supported.");
        return OEMCrypto_ERROR_NOT_IMPLEMENTED;
      }
    }
    OEMCrypto_KeyObject_V10* key_array_v10_ptr = NULL;
    if (num_keys > 0) key_array_v10_ptr = &key_array_v10[0];
    if (pair.fcn->version == 8) {
      if (pair.fcn->LoadKeys_V8 == NULL) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
      return pair.fcn->LoadKeys_V8(
          pair.session, message, message_length, signature, signature_length,
          PointerOrNull(message + enc_mac_keys_iv.offset,
                        enc_mac_keys_iv.length),
          PointerOrNull(message + enc_mac_keys.offset, enc_mac_keys.length),
          num_keys, key_array_v10_ptr);
    } else {
      if (pair.fcn->LoadKeys_V9_or_V10 == NULL)
        return OEMCrypto_ERROR_NOT_IMPLEMENTED;
      OEMCryptoResult result = pair.fcn->LoadKeys_V9_or_V10(
          pair.session, message, message_length, signature, signature_length,
          PointerOrNull(message + enc_mac_keys_iv.offset,
                        enc_mac_keys_iv.length),
          PointerOrNull(message + enc_mac_keys.offset, enc_mac_keys.length),
          num_keys, key_array_v10_ptr,
          PointerOrNull(message + pst.offset, pst.length), pst.length);
      // Convert a vendor specific error, to make it actionable
      if (result == kOemCryptoResultVendorSpecificError1)
        result = OEMCrypto_ERROR_USAGE_TABLE_UNRECOVERABLE;
      return result;
    }
  } else if (pair.fcn->version < 14) {
    if (license_type != OEMCrypto_ContentLicense)
      return OEMCrypto_ERROR_NOT_IMPLEMENTED;
    std::vector<OEMCrypto_KeyObject_V13> key_array_v13(num_keys);
    for (size_t i = 0; i < num_keys; i++) {
      key_array_v13[i].key_id = PointerOrNull(
          message + key_array[i].key_id.offset, key_array[i].key_id.length);
      key_array_v13[i].key_id_length = key_array[i].key_id.length;
      key_array_v13[i].key_data_iv =
          PointerOrNull(message + key_array[i].key_data_iv.offset,
                        key_array[i].key_data_iv.length);
      key_array_v13[i].key_data = PointerOrNull(
          message + key_array[i].key_data.offset, key_array[i].key_data.length);
      key_array_v13[i].key_data_length = key_array[i].key_data.length;
      key_array_v13[i].key_control_iv =
          PointerOrNull(message + key_array[i].key_control_iv.offset,
                        key_array[i].key_control_iv.length);
      key_array_v13[i].key_control =
          PointerOrNull(message + key_array[i].key_control.offset,
                        key_array[i].key_control.length);
      key_array_v13[i].cipher_mode = cipher_modes[i];
    }
    OEMCrypto_KeyObject_V13* key_array_v13_ptr = NULL;
    if (num_keys > 0) key_array_v13_ptr = &key_array_v13[0];
    if (pair.fcn->version < 13) {
      if (pair.fcn->LoadKeys_V11_or_V12 == NULL)
        return OEMCrypto_ERROR_NOT_IMPLEMENTED;
      OEMCryptoResult result = pair.fcn->LoadKeys_V11_or_V12(
          pair.session, message, message_length, signature, signature_length,
          PointerOrNull(message + enc_mac_keys_iv.offset,
                        enc_mac_keys_iv.length),
          PointerOrNull(message + enc_mac_keys.offset, enc_mac_keys.length),
          num_keys, key_array_v13_ptr,
          PointerOrNull(message + pst.offset, pst.length), pst.length);
      // Convert a vendor specific error, to make it actionable
      if (result == kOemCryptoResultVendorSpecificError1)
        result = OEMCrypto_ERROR_USAGE_TABLE_UNRECOVERABLE;
      return result;
    } else {
      if (pair.fcn->LoadKeys_V13 == NULL)
        return OEMCrypto_ERROR_NOT_IMPLEMENTED;
      return pair.fcn->LoadKeys_V13(
          pair.session, message, message_length, signature, signature_length,
          PointerOrNull(message + enc_mac_keys_iv.offset,
                        enc_mac_keys_iv.length),
          PointerOrNull(message + enc_mac_keys.offset, enc_mac_keys.length),
          num_keys, key_array_v13_ptr,
          PointerOrNull(message + pst.offset, pst.length), pst.length,
          PointerOrNull(message + srm_restriction_data.offset,
                        srm_restriction_data.length));
    }
  } else if (pair.fcn->version < 15) {
    if (pair.fcn->LoadKeys_V14 == NULL) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
    std::vector<OEMCrypto_KeyObject_V14> key_array_v14(num_keys);
    for (size_t i = 0; i < num_keys; i++) {
      key_array_v14[i].key_id = PointerOrNull(
          message + key_array[i].key_id.offset, key_array[i].key_id.length);
      key_array_v14[i].key_id_length = key_array[i].key_id.length;
      key_array_v14[i].key_data_iv =
          PointerOrNull(message + key_array[i].key_data_iv.offset,
                        key_array[i].key_data_iv.length);
      key_array_v14[i].key_data = PointerOrNull(
          message + key_array[i].key_data.offset, key_array[i].key_data.length);
      key_array_v14[i].key_data_length = key_array[i].key_data.length;
      key_array_v14[i].key_control_iv =
          PointerOrNull(message + key_array[i].key_control_iv.offset,
                        key_array[i].key_control_iv.length);
      key_array_v14[i].key_control =
          PointerOrNull(message + key_array[i].key_control.offset,
                        key_array[i].key_control.length);
    }
    OEMCrypto_KeyObject_V14* key_array_v14_ptr = NULL;
    if (num_keys > 0) key_array_v14_ptr = &key_array_v14[0];
    return pair.fcn->LoadKeys_V14(
        pair.session, message, message_length, signature, signature_length,
        PointerOrNull(message + enc_mac_keys_iv.offset, enc_mac_keys_iv.length),
        PointerOrNull(message + enc_mac_keys.offset, enc_mac_keys.length),
        num_keys, key_array_v14_ptr,
        PointerOrNull(message + pst.offset, pst.length), pst.length,
        PointerOrNull(message + srm_restriction_data.offset,
                      srm_restriction_data.length),
        license_type);
  } else {
    if (pair.fcn->LoadKeys == NULL) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
    return pair.fcn->LoadKeys(pair.session, message, message_length, signature,
                              signature_length, enc_mac_keys_iv, enc_mac_keys,
                              num_keys, key_array, pst, srm_restriction_data,
                              license_type);
  }
}

/* The CDM layer should use OEMCrypto_LoadKeys_Back_Compat instead. This is
 * used by unit tests, and forces cipher mode to CTR when used with an older
 * oemcrypto.
 */
extern "C" OEMCryptoResult OEMCrypto_LoadKeys(
    OEMCrypto_SESSION session, const uint8_t* message, size_t message_length,
    const uint8_t* signature, size_t signature_length,
    OEMCrypto_Substring enc_mac_keys_iv, OEMCrypto_Substring enc_mac_keys,
    size_t num_keys, const OEMCrypto_KeyObject* key_array,
    OEMCrypto_Substring pst, OEMCrypto_Substring srm_restriction_data,
    OEMCrypto_LicenseType license_type) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  LevelSession pair = gAdapter->GetSession(session);
  if (!pair.fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (pair.fcn->version < 11) {
    if (license_type != OEMCrypto_ContentLicense)
      return OEMCrypto_ERROR_NOT_IMPLEMENTED;
    std::vector<OEMCrypto_KeyObject_V10> key_array_v10(num_keys);
    for (size_t i = 0; i < num_keys; i++) {
      key_array_v10[i].key_id = PointerOrNull(
          message + key_array[i].key_id.offset, key_array[i].key_id.length);
      key_array_v10[i].key_id_length = key_array[i].key_id.length;
      key_array_v10[i].key_data_iv =
          PointerOrNull(message + key_array[i].key_data_iv.offset,
                        key_array[i].key_data_iv.length);
      key_array_v10[i].key_data = PointerOrNull(
          message + key_array[i].key_data.offset, key_array[i].key_data.length);
      key_array_v10[i].key_data_length = key_array[i].key_data.length;
      key_array_v10[i].key_control_iv =
          PointerOrNull(message + key_array[i].key_control_iv.offset,
                        key_array[i].key_control_iv.length);
      key_array_v10[i].key_control =
          PointerOrNull(message + key_array[i].key_control.offset,
                        key_array[i].key_control.length);
    }
    OEMCrypto_KeyObject_V10* key_array_v10_ptr = NULL;
    if (num_keys > 0) key_array_v10_ptr = &key_array_v10[0];
    if (pair.fcn->version == 8) {
      if (pair.fcn->LoadKeys_V8 == NULL) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
      return pair.fcn->LoadKeys_V8(
          pair.session, message, message_length, signature, signature_length,
          PointerOrNull(message + enc_mac_keys_iv.offset,
                        enc_mac_keys_iv.length),
          PointerOrNull(message + enc_mac_keys.offset, enc_mac_keys.length),
          num_keys, key_array_v10_ptr);
    } else {
      if (pair.fcn->LoadKeys_V9_or_V10 == NULL)
        return OEMCrypto_ERROR_NOT_IMPLEMENTED;
      OEMCryptoResult result = pair.fcn->LoadKeys_V9_or_V10(
          pair.session, message, message_length, signature, signature_length,
          PointerOrNull(message + enc_mac_keys_iv.offset,
                        enc_mac_keys_iv.length),
          PointerOrNull(message + enc_mac_keys.offset, enc_mac_keys.length),
          num_keys, key_array_v10_ptr,
          PointerOrNull(message + pst.offset, pst.length), pst.length);
      // Convert a vendor specific error, to make it actionable
      if (result == kOemCryptoResultVendorSpecificError1)
        result = OEMCrypto_ERROR_USAGE_TABLE_UNRECOVERABLE;
      return result;
    }
  } else if (pair.fcn->version < 14) {
    if (license_type != OEMCrypto_ContentLicense)
      return OEMCrypto_ERROR_NOT_IMPLEMENTED;
    std::vector<OEMCrypto_KeyObject_V13> key_array_v13(num_keys);
    for (size_t i = 0; i < num_keys; i++) {
      key_array_v13[i].key_id = PointerOrNull(
          message + key_array[i].key_id.offset, key_array[i].key_id.length);
      key_array_v13[i].key_id_length = key_array[i].key_id.length;
      key_array_v13[i].key_data_iv =
          PointerOrNull(message + key_array[i].key_data_iv.offset,
                        key_array[i].key_data_iv.length);
      key_array_v13[i].key_data = PointerOrNull(
          message + key_array[i].key_data.offset, key_array[i].key_data.length);
      key_array_v13[i].key_data_length = key_array[i].key_data.length;
      key_array_v13[i].key_control_iv =
          PointerOrNull(message + key_array[i].key_control_iv.offset,
                        key_array[i].key_control_iv.length);
      key_array_v13[i].key_control =
          PointerOrNull(message + key_array[i].key_control.offset,
                        key_array[i].key_control.length);
      key_array_v13[i].cipher_mode = OEMCrypto_CipherMode_CTR;
    }
    OEMCrypto_KeyObject_V13* key_array_v13_ptr = NULL;
    if (num_keys > 0) key_array_v13_ptr = &key_array_v13[0];
    if (pair.fcn->version < 13) {
      if (pair.fcn->LoadKeys_V11_or_V12 == NULL)
        return OEMCrypto_ERROR_NOT_IMPLEMENTED;
      OEMCryptoResult result = pair.fcn->LoadKeys_V11_or_V12(
          pair.session, message, message_length, signature, signature_length,
          PointerOrNull(message + enc_mac_keys_iv.offset,
                        enc_mac_keys_iv.length),
          PointerOrNull(message + enc_mac_keys.offset, enc_mac_keys.length),
          num_keys, key_array_v13_ptr,
          PointerOrNull(message + pst.offset, pst.length), pst.length);
      // Convert a vendor specific error, to make it actionable
      if (result == kOemCryptoResultVendorSpecificError1)
        result = OEMCrypto_ERROR_USAGE_TABLE_UNRECOVERABLE;
      return result;
    } else {
      if (pair.fcn->LoadKeys_V13 == NULL)
        return OEMCrypto_ERROR_NOT_IMPLEMENTED;
      return pair.fcn->LoadKeys_V13(
          pair.session, message, message_length, signature, signature_length,
          PointerOrNull(message + enc_mac_keys_iv.offset,
                        enc_mac_keys_iv.length),
          PointerOrNull(message + enc_mac_keys.offset, enc_mac_keys.length),
          num_keys, key_array_v13_ptr,
          PointerOrNull(message + pst.offset, pst.length), pst.length,
          PointerOrNull(message + srm_restriction_data.offset,
                        srm_restriction_data.length));
    }
  } else if (pair.fcn->version < 15) {
    if (pair.fcn->LoadKeys_V14 == NULL) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
    std::vector<OEMCrypto_KeyObject_V14> key_array_v14(num_keys);
    for (size_t i = 0; i < num_keys; i++) {
      key_array_v14[i].key_id = PointerOrNull(
          message + key_array[i].key_id.offset, key_array[i].key_id.length);
      key_array_v14[i].key_id_length = key_array[i].key_id.length;
      key_array_v14[i].key_data_iv =
          PointerOrNull(message + key_array[i].key_data_iv.offset,
                        key_array[i].key_data_iv.length);
      key_array_v14[i].key_data = PointerOrNull(
          message + key_array[i].key_data.offset, key_array[i].key_data.length);
      key_array_v14[i].key_data_length = key_array[i].key_data.length;
      key_array_v14[i].key_control_iv =
          PointerOrNull(message + key_array[i].key_control_iv.offset,
                        key_array[i].key_control_iv.length);
      key_array_v14[i].key_control =
          PointerOrNull(message + key_array[i].key_control.offset,
                        key_array[i].key_control.length);
    }
    OEMCrypto_KeyObject_V14* key_array_v14_ptr = NULL;
    if (num_keys > 0) key_array_v14_ptr = &key_array_v14[0];
    return pair.fcn->LoadKeys_V14(
        pair.session, message, message_length, signature, signature_length,
        PointerOrNull(message + enc_mac_keys_iv.offset, enc_mac_keys_iv.length),
        PointerOrNull(message + enc_mac_keys.offset, enc_mac_keys.length),
        num_keys, key_array_v14_ptr,
        PointerOrNull(message + pst.offset, pst.length), pst.length,
        PointerOrNull(message + srm_restriction_data.offset,
                      srm_restriction_data.length),
        license_type);
  } else {
    if (pair.fcn->LoadKeys == NULL) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
    return pair.fcn->LoadKeys(pair.session, message, message_length, signature,
                              signature_length, enc_mac_keys_iv, enc_mac_keys,
                              num_keys, key_array, pst, srm_restriction_data,
                              license_type);
  }
}

extern "C" OEMCryptoResult OEMCrypto_LoadEntitledContentKeys(
    OEMCrypto_SESSION session, const uint8_t* message, size_t message_length,
    size_t num_keys, const OEMCrypto_EntitledContentKeyObject* key_array) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  LevelSession pair = gAdapter->GetSession(session);
  if (!pair.fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (pair.fcn->version < 14) {
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  } else if (pair.fcn->version < 15) {
    if (pair.fcn->LoadEntitledContentKeys_V14 == NULL)
      return OEMCrypto_ERROR_NOT_IMPLEMENTED;
    std::vector<OEMCrypto_EntitledContentKeyObject_V14> ecko_array_v14(
        num_keys);
    for (size_t i = 0; i < num_keys; i++) {
      ecko_array_v14[i].entitlement_key_id =
          PointerOrNull(message + key_array[i].entitlement_key_id.offset,
                        key_array[i].entitlement_key_id.length);
      ecko_array_v14[i].entitlement_key_id_length =
          key_array[i].entitlement_key_id.length;
      ecko_array_v14[i].content_key_id =
          PointerOrNull(message + key_array[i].content_key_id.offset,
                        key_array[i].content_key_id.length);
      ecko_array_v14[i].content_key_id_length =
          key_array[i].content_key_id.length;
      ecko_array_v14[i].content_key_data_iv =
          PointerOrNull(message + key_array[i].content_key_data_iv.offset,
                        key_array[i].content_key_data_iv.length);
      ecko_array_v14[i].content_key_data =
          PointerOrNull(message + key_array[i].content_key_data.offset,
                        key_array[i].content_key_data.length);
      ecko_array_v14[i].content_key_data_length =
          key_array[i].content_key_data.length;
    }
    OEMCrypto_EntitledContentKeyObject_V14* ecko_array_v14_ptr = NULL;
    if (num_keys > 0) ecko_array_v14_ptr = &ecko_array_v14[0];
    return pair.fcn->LoadEntitledContentKeys_V14(pair.session, num_keys,
                                                 ecko_array_v14_ptr);
  } else {
    if (pair.fcn->LoadEntitledContentKeys == NULL)
      return OEMCrypto_ERROR_NOT_IMPLEMENTED;
    return pair.fcn->LoadEntitledContentKeys(
        pair.session, message, message_length, num_keys, key_array);
  }
}

extern "C" OEMCryptoResult OEMCrypto_RefreshKeys(
    OEMCrypto_SESSION session, const uint8_t* message, size_t message_length,
    const uint8_t* signature, size_t signature_length, size_t num_keys,
    const OEMCrypto_KeyRefreshObject* key_array) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  LevelSession pair = gAdapter->GetSession(session);
  if (!pair.fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (pair.fcn->version < 15) {
    if (pair.fcn->RefreshKeys_V14 == NULL)
      return OEMCrypto_ERROR_NOT_IMPLEMENTED;
    std::vector<OEMCrypto_KeyRefreshObject_V14> kro_array_v14(num_keys);
    for (size_t i = 0; i < num_keys; i++) {
      kro_array_v14[i].key_id = PointerOrNull(
          message + key_array[i].key_id.offset, key_array[i].key_id.length);
      kro_array_v14[i].key_id_length = key_array[i].key_id.length;
      kro_array_v14[i].key_control_iv =
          PointerOrNull(message + key_array[i].key_control_iv.offset,
                        key_array[i].key_control_iv.length);
      kro_array_v14[i].key_control =
          PointerOrNull(message + key_array[i].key_control.offset,
                        key_array[i].key_control.length);
    }
    OEMCrypto_KeyRefreshObject_V14* kro_array_v14_ptr = NULL;
    if (num_keys > 0) kro_array_v14_ptr = &kro_array_v14[0];
    return pair.fcn->RefreshKeys_V14(pair.session, message, message_length,
                                     signature, signature_length, num_keys,
                                     kro_array_v14_ptr);
  }
  if (pair.fcn->RefreshKeys == NULL) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  return pair.fcn->RefreshKeys(pair.session, message, message_length, signature,
                               signature_length, num_keys, key_array);
}

extern "C" OEMCryptoResult OEMCrypto_QueryKeyControl(
    OEMCrypto_SESSION session, const uint8_t* key_id, size_t key_id_length,
    uint8_t* key_control_block, size_t* key_control_block_length) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  LevelSession pair = gAdapter->GetSession(session);
  if (!pair.fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (pair.fcn->version < 10) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  if (pair.fcn->QueryKeyControl == NULL) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  return pair.fcn->QueryKeyControl(pair.session, key_id, key_id_length,
                                   key_control_block, key_control_block_length);
}

extern "C" OEMCryptoResult OEMCrypto_SelectKey(const OEMCrypto_SESSION session,
                                               const uint8_t* key_id,
                                               size_t key_id_length,
                                               OEMCryptoCipherMode cipher_mode) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  LevelSession pair = gAdapter->GetSession(session);
  if (!pair.fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (pair.fcn->version < 14) {
    if (pair.fcn->SelectKey_V13 == NULL) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
    return pair.fcn->SelectKey_V13(pair.session, key_id, key_id_length);
  } else {
    if (pair.fcn->SelectKey == NULL) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
    return pair.fcn->SelectKey(pair.session, key_id, key_id_length,
                               cipher_mode);
  }
}

extern "C" OEMCryptoResult OEMCrypto_DecryptCENC(
    OEMCrypto_SESSION session, const uint8_t* data_addr, size_t data_length,
    bool is_encrypted, const uint8_t* iv, size_t offset,
    OEMCrypto_DestBufferDesc* out_buffer,
    const OEMCrypto_CENCEncryptPatternDesc* pattern, uint8_t subsample_flags) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  LevelSession pair = gAdapter->GetSession(session);
  if (!pair.fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (pair.fcn->version < 11) {
    if (pair.fcn->DecryptCTR_V10 == NULL)
      return OEMCrypto_ERROR_NOT_IMPLEMENTED;
    return pair.fcn->DecryptCTR_V10(pair.session, data_addr, data_length,
                                    is_encrypted, iv, offset, out_buffer,
                                    subsample_flags);
  } else {
    if (pair.fcn->DecryptCENC == NULL) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
    return pair.fcn->DecryptCENC(pair.session, data_addr, data_length,
                                 is_encrypted, iv, offset, out_buffer, pattern,
                                 subsample_flags);
  }
}

extern "C" OEMCryptoResult OEMCrypto_CopyBuffer(
    OEMCrypto_SESSION session, const uint8_t* data_addr, size_t data_length,
    OEMCrypto_DestBufferDesc* out_buffer, uint8_t subsample_flags) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  LevelSession pair = gAdapter->GetSession(session);
  if (!pair.fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (pair.fcn->version < 15) {
    if (pair.fcn->CopyBuffer_V14 == NULL)
      return OEMCrypto_ERROR_NOT_IMPLEMENTED;
    return pair.fcn->CopyBuffer_V14(data_addr, data_length, out_buffer,
                                    subsample_flags);
  }
  if (pair.fcn->CopyBuffer == NULL) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  return pair.fcn->CopyBuffer(session, data_addr, data_length, out_buffer,
                              subsample_flags);
}

extern "C" OEMCryptoResult OEMCrypto_WrapKeybox(const uint8_t* keybox,
                                                size_t keyBoxLength,
                                                uint8_t* wrappedKeybox,
                                                size_t* wrappedKeyBoxLength,
                                                const uint8_t* transportKey,
                                                size_t transportKeyLength) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  const FunctionPointers* fcn = gAdapter->GetFunctionPointers(kLevelDefault);
  if (!fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (fcn->WrapKeybox == NULL) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  return fcn->WrapKeybox(keybox, keyBoxLength, wrappedKeybox,
                         wrappedKeyBoxLength, transportKey, transportKeyLength);
}

extern "C" OEMCryptoResult OEMCrypto_InstallKeyboxOrOEMCert(const uint8_t* keybox,
                                                   size_t keyBoxLength) {
  return OEMCrypto_InstallKeyboxOrOEMCert(keybox, keyBoxLength, kLevelDefault);
}

extern "C" OEMCryptoResult OEMCrypto_LoadTestKeybox(const uint8_t* buffer,
                                                    size_t length) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  const FunctionPointers* fcn = gAdapter->GetFunctionPointers(kLevelDefault);
  if (!fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (fcn->version < 10) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  if (fcn->LoadTestKeybox_V13 != NULL) {
    // Old versions might use wrong keybox, but this is the best we can do.
    return fcn->LoadTestKeybox_V13();
  }
  if (fcn->LoadTestKeybox == NULL) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  return fcn->LoadTestKeybox(buffer, length);
}

extern "C" OEMCryptoResult OEMCrypto_IsKeyboxOrOEMCertValid() {
  return OEMCrypto_IsKeyboxOrOEMCertValid(kLevelDefault);
}

extern "C" OEMCrypto_ProvisioningMethod OEMCrypto_GetProvisioningMethod() {
  return OEMCrypto_GetProvisioningMethod(kLevelDefault);
}

extern "C" OEMCryptoResult OEMCrypto_GetOEMPublicCertificate(
    OEMCrypto_SESSION session, uint8_t* public_cert,
    size_t* public_cert_length) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  LevelSession pair = gAdapter->GetSession(session);
  if (!pair.fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (pair.fcn->version < 12) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  if (pair.fcn->GetOEMPublicCertificate == NULL)
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  return pair.fcn->GetOEMPublicCertificate(pair.session, public_cert,
                                           public_cert_length);
}

extern "C" OEMCryptoResult OEMCrypto_GetDeviceID(uint8_t* deviceID,
                                                 size_t* idLength) {
  return OEMCrypto_GetDeviceID(deviceID, idLength, kLevelDefault);
}

extern "C" OEMCryptoResult OEMCrypto_GetKeyData(uint8_t* keyData,
                                                size_t* keyDataLength) {
  return OEMCrypto_GetKeyData(keyData, keyDataLength, kLevelDefault);
}

extern "C" OEMCryptoResult OEMCrypto_GetRandom(uint8_t* randomData,
                                               size_t dataLength) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  const FunctionPointers* fcn = gAdapter->GetFunctionPointers(kLevelDefault);
  if (!fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (fcn->GetRandom == NULL) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  return fcn->GetRandom(randomData, dataLength);
}

extern "C" OEMCryptoResult OEMCrypto_RewrapDeviceRSAKey30(
                               OEMCrypto_SESSION session,
                               const uint32_t *nonce,
                               const uint8_t* encrypted_message_key,
                               size_t encrypted_message_key_length,
                               const uint8_t* enc_rsa_key,
                               size_t enc_rsa_key_length,
                               const uint8_t* enc_rsa_key_iv,
                               uint8_t* wrapped_rsa_key,
                               size_t* wrapped_rsa_key_length) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  LevelSession pair = gAdapter->GetSession(session);
  if (!pair.fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (pair.fcn->version < 12) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  if (pair.fcn->RewrapDeviceRSAKey30 == NULL)
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  return pair.fcn->RewrapDeviceRSAKey30(pair.session, nonce,
                                        encrypted_message_key,
                                        encrypted_message_key_length,
                                        enc_rsa_key, enc_rsa_key_length,
                                        enc_rsa_key_iv, wrapped_rsa_key,
                                        wrapped_rsa_key_length);
}

extern "C" OEMCryptoResult OEMCrypto_RewrapDeviceRSAKey(
    OEMCrypto_SESSION session, const uint8_t* message, size_t message_length,
    const uint8_t* signature, size_t signature_length, const uint32_t* nonce,
    const uint8_t* enc_rsa_key, size_t enc_rsa_key_length,
    const uint8_t* enc_rsa_key_iv, uint8_t* wrapped_rsa_key,
    size_t* wrapped_rsa_key_length) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  LevelSession pair = gAdapter->GetSession(session);
  if (!pair.fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (pair.fcn->RewrapDeviceRSAKey == NULL)
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  return pair.fcn->RewrapDeviceRSAKey(
      pair.session, message, message_length, signature, signature_length, nonce,
      enc_rsa_key, enc_rsa_key_length, enc_rsa_key_iv, wrapped_rsa_key,
      wrapped_rsa_key_length);
}

extern "C" OEMCryptoResult OEMCrypto_LoadDeviceRSAKey(
    OEMCrypto_SESSION session, const uint8_t* wrapped_rsa_key,
    size_t wrapped_rsa_key_length) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  LevelSession pair = gAdapter->GetSession(session);
  if (!pair.fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (pair.fcn->LoadDeviceRSAKey == NULL)
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  return pair.fcn->LoadDeviceRSAKey(pair.session, wrapped_rsa_key,
                                    wrapped_rsa_key_length);
}

extern "C" OEMCryptoResult OEMCrypto_LoadTestRSAKey() {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  const FunctionPointers* fcn = gAdapter->GetFunctionPointers(kLevelDefault);
  if (!fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (fcn->version < 10) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  if (fcn->LoadTestRSAKey == NULL) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  return fcn->LoadTestRSAKey();
}

extern "C" OEMCryptoResult OEMCrypto_GenerateRSASignature(
    OEMCrypto_SESSION session, const uint8_t* message, size_t message_length,
    uint8_t* signature, size_t* signature_length,
    RSA_Padding_Scheme padding_scheme) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  LevelSession pair = gAdapter->GetSession(session);
  if (!pair.fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (pair.fcn->version == 8) {
    if (pair.fcn->GenerateRSASignature_V8 == NULL)
      return OEMCrypto_ERROR_NOT_IMPLEMENTED;
    return pair.fcn->GenerateRSASignature_V8(
        pair.session, message, message_length, signature, signature_length);
  } else {
    if (pair.fcn->GenerateRSASignature == NULL)
      return OEMCrypto_ERROR_NOT_IMPLEMENTED;
    return pair.fcn->GenerateRSASignature(pair.session, message, message_length,
                                          signature, signature_length,
                                          padding_scheme);
  }
}

extern "C" OEMCryptoResult OEMCrypto_DeriveKeysFromSessionKey(
    OEMCrypto_SESSION session, const uint8_t* enc_session_key,
    size_t enc_session_key_length, const uint8_t* mac_key_context,
    size_t mac_key_context_length, const uint8_t* enc_key_context,
    size_t enc_key_context_length) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  LevelSession pair = gAdapter->GetSession(session);
  if (!pair.fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (pair.fcn->DeriveKeysFromSessionKey == NULL)
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  return pair.fcn->DeriveKeysFromSessionKey(
      pair.session, enc_session_key, enc_session_key_length, mac_key_context,
      mac_key_context_length, enc_key_context, enc_key_context_length);
}

extern "C" uint32_t OEMCrypto_APIVersion() {
  return OEMCrypto_APIVersion(kLevelDefault);
}

extern "C" uint8_t OEMCrypto_Security_Patch_Level() {
  return OEMCrypto_Security_Patch_Level(kLevelDefault);
}

extern "C" const char* OEMCrypto_SecurityLevel() {
  return OEMCrypto_SecurityLevel(kLevelDefault);
}

extern "C" OEMCryptoResult OEMCrypto_GetHDCPCapability(
    OEMCrypto_HDCP_Capability* current, OEMCrypto_HDCP_Capability* maximum) {
  return OEMCrypto_GetHDCPCapability(kLevelDefault, current, maximum);
}

extern "C" bool OEMCrypto_SupportsUsageTable() {
  return OEMCrypto_SupportsUsageTable(kLevelDefault);
}

extern "C" bool OEMCrypto_IsAntiRollbackHwPresent() {
  return OEMCrypto_IsAntiRollbackHwPresent(kLevelDefault);
}

extern "C" OEMCryptoResult OEMCrypto_GetNumberOfOpenSessions(size_t* count) {
  return OEMCrypto_GetNumberOfOpenSessions(kLevelDefault, count);
}

extern "C" OEMCryptoResult OEMCrypto_GetMaxNumberOfSessions(size_t* maximum) {
  return OEMCrypto_GetMaxNumberOfSessions(kLevelDefault, maximum);
}

extern "C" uint32_t OEMCrypto_SupportedCertificates() {
  return OEMCrypto_SupportedCertificates(kLevelDefault);
}

extern "C" OEMCryptoResult OEMCrypto_Generic_Encrypt(
    OEMCrypto_SESSION session, const uint8_t* in_buffer, size_t buffer_length,
    const uint8_t* iv, OEMCrypto_Algorithm algorithm, uint8_t* out_buffer) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  LevelSession pair = gAdapter->GetSession(session);
  if (!pair.fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (pair.fcn->Generic_Encrypt == NULL) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  OEMCryptoResult status = OEMCrypto_SUCCESS;
  std::vector<uint8_t> current_iv(iv, iv + wvcdm::KEY_IV_SIZE);
  while (buffer_length > 0 && status == OEMCrypto_SUCCESS) {
    const size_t chunk_size =
        std::min(buffer_length, kMaxGenericEncryptChunkSize);
    status = pair.fcn->Generic_Encrypt(pair.session, in_buffer, chunk_size,
                                       &current_iv[0], algorithm, out_buffer);
    buffer_length -= chunk_size;
    in_buffer += chunk_size;
    out_buffer += chunk_size;
    if (buffer_length > 0) {
      current_iv.assign(out_buffer - wvcdm::KEY_IV_SIZE, out_buffer);
    }
  }
  return status;
}

extern "C" OEMCryptoResult OEMCrypto_Generic_Decrypt(
    OEMCrypto_SESSION session, const uint8_t* in_buffer, size_t buffer_length,
    const uint8_t* iv, OEMCrypto_Algorithm algorithm, uint8_t* out_buffer) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  LevelSession pair = gAdapter->GetSession(session);
  if (!pair.fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (pair.fcn->Generic_Decrypt == NULL) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  OEMCryptoResult status = OEMCrypto_SUCCESS;
  std::vector<uint8_t> current_iv(iv, iv + wvcdm::KEY_IV_SIZE);
  while (buffer_length > 0 && status == OEMCrypto_SUCCESS) {
    const size_t chunk_size =
        std::min(buffer_length, kMaxGenericEncryptChunkSize);
    status = pair.fcn->Generic_Decrypt(pair.session, in_buffer, chunk_size,
                                       &current_iv[0], algorithm, out_buffer);
    buffer_length -= chunk_size;
    in_buffer += chunk_size;
    out_buffer += chunk_size;
    if (buffer_length > 0) {
      current_iv.assign(in_buffer - wvcdm::KEY_IV_SIZE, in_buffer);
    }
  }
  return status;
}

extern "C" OEMCryptoResult OEMCrypto_Generic_Sign(OEMCrypto_SESSION session,
                                                  const uint8_t* in_buffer,
                                                  size_t buffer_length,
                                                  OEMCrypto_Algorithm algorithm,
                                                  uint8_t* signature,
                                                  size_t* signature_length) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  LevelSession pair = gAdapter->GetSession(session);
  if (!pair.fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  return pair.fcn->Generic_Sign(pair.session, in_buffer, buffer_length,
                                algorithm, signature, signature_length);
}

extern "C" OEMCryptoResult OEMCrypto_Generic_Verify(
    OEMCrypto_SESSION session, const uint8_t* in_buffer, size_t buffer_length,
    OEMCrypto_Algorithm algorithm, const uint8_t* signature,
    size_t signature_length) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  LevelSession pair = gAdapter->GetSession(session);
  if (!pair.fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (pair.fcn->Generic_Verify == NULL) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  return pair.fcn->Generic_Verify(pair.session, in_buffer, buffer_length,
                                  algorithm, signature, signature_length);
}

extern "C" OEMCryptoResult OEMCrypto_UpdateUsageTable() {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  const FunctionPointers* fcn1 = gAdapter->GetFunctionPointers(kLevelDefault);
  OEMCryptoResult sts = OEMCrypto_ERROR_NOT_IMPLEMENTED;
  if ((fcn1 != NULL) && (fcn1->UpdateUsageTable != NULL) &&
      (fcn1->version > 8) && (fcn1->version < 13)) {
    sts = fcn1->UpdateUsageTable();
  }
  return sts;
}

extern "C" OEMCryptoResult OEMCrypto_DeactivateUsageEntry(
    OEMCrypto_SESSION session, const uint8_t* pst, size_t pst_length) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  LevelSession pair = gAdapter->GetSession(session);
  if (!pair.fcn) {
    const FunctionPointers* fcn1 = gAdapter->GetFunctionPointers(kLevelDefault);
    const FunctionPointers* fcn3 = gAdapter->GetFunctionPointers(kLevel3);
    OEMCryptoResult sts = OEMCrypto_ERROR_INVALID_SESSION;
    if (fcn1) {
      if (fcn1->version < 13) {
        if (fcn1->DeactivateUsageEntry_V12) {
          sts = fcn1->DeactivateUsageEntry_V12(pst, pst_length);
        }
      } else {
        if (fcn1->DeactivateUsageEntry) {
          sts = fcn1->DeactivateUsageEntry(0, pst, pst_length);
        }
      }
    }
    if (OEMCrypto_SUCCESS != sts) {
      if (fcn3 && fcn1 != fcn3) {
        if (fcn3->version < 13) {
          if (fcn3->DeactivateUsageEntry_V12) {
            sts = fcn3->DeactivateUsageEntry_V12(pst, pst_length);
          }
        } else {
          if (fcn3->DeactivateUsageEntry) {
            sts = fcn3->DeactivateUsageEntry(0, pst, pst_length);
          }
        }
      }
    }
    return sts;
    // TODO(fredgc): All of the code above should be removed once the
    // unit tests and cdm code use the new deactivate usage entry.
  }
  if (pair.fcn->version < 9) {
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  }
  if (pair.fcn->version < 13) {
    if (pair.fcn->DeactivateUsageEntry_V12) {
      return pair.fcn->DeactivateUsageEntry_V12(pst, pst_length);
    } else {
      return OEMCrypto_ERROR_INVALID_SESSION;
    }
  }
  // Session's OEMCrypto is v13 or greater.
  if (pair.fcn->DeactivateUsageEntry) {
    return pair.fcn->DeactivateUsageEntry(pair.session, pst, pst_length);
  } else {
    return OEMCrypto_ERROR_INVALID_SESSION;
  }
}

extern "C" OEMCryptoResult OEMCrypto_ReportUsage(OEMCrypto_SESSION session,
                                                 const uint8_t* pst,
                                                 size_t pst_length,
                                                 uint8_t* buffer,
                                                 size_t* buffer_length) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  LevelSession pair = gAdapter->GetSession(session);
  if (!pair.fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (pair.fcn->version > 8) {
    if (pair.fcn->ReportUsage == NULL) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
    return pair.fcn->ReportUsage(pair.session, pst, pst_length, buffer,
                                 buffer_length);
  } else {
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  }
}

extern "C" OEMCryptoResult OEMCrypto_DeleteUsageEntry(
    OEMCrypto_SESSION session, const uint8_t* pst, size_t pst_length,
    const uint8_t* message, size_t message_length, const uint8_t* signature,
    size_t signature_length) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  LevelSession pair = gAdapter->GetSession(session);
  if (!pair.fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (pair.fcn->version > 8 && pair.fcn->version < 13) {
    return pair.fcn->DeleteUsageEntry(pair.session, pst, pst_length, message,
                                      message_length, signature,
                                      signature_length);
  } else {
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  }
}

extern "C" OEMCryptoResult OEMCrypto_ForceDeleteUsageEntry(const uint8_t* pst,
                                                           size_t pst_length) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  const FunctionPointers* fcn1 = gAdapter->GetFunctionPointers(kLevelDefault);
  const FunctionPointers* fcn3 = gAdapter->GetFunctionPointers(kLevel3);
  OEMCryptoResult sts = OEMCrypto_ERROR_NOT_IMPLEMENTED;
  if (fcn3 && (fcn3->version > 9) && (fcn3->version < 13) &&
      fcn3->ForceDeleteUsageEntry != NULL) {
    sts = fcn3->ForceDeleteUsageEntry(pst, pst_length);
  }
  if (fcn1 && fcn1 != fcn3 && (fcn1->version > 9) && (fcn1->version < 13) &&
      (fcn1->ForceDeleteUsageEntry != NULL)) {
    OEMCryptoResult sts1 = fcn1->ForceDeleteUsageEntry(pst, pst_length);
    if ((sts != OEMCrypto_SUCCESS) && (sts1 == OEMCrypto_SUCCESS)) {
      sts = sts1;
    }
  }
  return sts;
}

extern "C" OEMCryptoResult OEMCrypto_DeleteOldUsageTable() {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  const FunctionPointers* fcn1 = gAdapter->GetFunctionPointers(kLevelDefault);
  const FunctionPointers* fcn3 = gAdapter->GetFunctionPointers(kLevel3);
  OEMCryptoResult sts = OEMCrypto_ERROR_NOT_IMPLEMENTED;
  if (fcn3 && (fcn3->version > 8) && (fcn3->DeleteOldUsageTable != NULL)) {
    sts = fcn3->DeleteOldUsageTable();
  }
  if (fcn1 && fcn1 != fcn3 &&
      (fcn1->version > 8) && (fcn1->DeleteOldUsageTable != NULL)) {
    sts = fcn1->DeleteOldUsageTable();
  }
  return sts;
}

extern "C" bool OEMCrypto_IsSRMUpdateSupported() {
  if (!gAdapter.get()) return false;
  // Level 3 can't load an SRM, so this just checkes Level 1.
  const FunctionPointers* fcn = gAdapter->GetFunctionPointers(kLevelDefault);
  if (!fcn) return false;
  if (fcn->version < 13) return false;
  if (fcn->IsSRMUpdateSupported == NULL) return false;
  return fcn->IsSRMUpdateSupported();
}

extern "C" OEMCryptoResult OEMCrypto_GetCurrentSRMVersion(uint16_t* version) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  // Level 3 can't load an SRM, so this just checkes Level 1.
  const FunctionPointers* fcn = gAdapter->GetFunctionPointers(kLevelDefault);
  if (!fcn) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  if (fcn->version < 13) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  if (fcn->GetCurrentSRMVersion == NULL) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  return fcn->GetCurrentSRMVersion(version);
}

extern "C" OEMCryptoResult OEMCrypto_LoadSRM(const uint8_t* buffer,
                                             size_t buffer_length) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  // Level 3 can't load an SRM, so this just checkes Level 1.
  const FunctionPointers* fcn = gAdapter->GetFunctionPointers(kLevelDefault);
  if (!fcn) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  if (fcn->version < 13) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  if (fcn->LoadSRM == NULL) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  return fcn->LoadSRM(buffer, buffer_length);
}

extern "C" OEMCryptoResult OEMCrypto_RemoveSRM() {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  // Level 3 can't load an SRM, so this just checkes Level 1.
  const FunctionPointers* fcn = gAdapter->GetFunctionPointers(kLevelDefault);
  if (!fcn) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  if (fcn->version < 13) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  if (fcn->RemoveSRM == NULL) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  return fcn->RemoveSRM();
}

extern "C" OEMCryptoResult OEMCrypto_CreateUsageTableHeader(
    uint8_t* header_buffer, size_t* header_buffer_length) {
  return OEMCrypto_CreateUsageTableHeader(kLevelDefault, header_buffer,
                                          header_buffer_length);
}

extern "C" OEMCryptoResult OEMCrypto_LoadUsageTableHeader(
    const uint8_t* buffer, size_t buffer_length) {
  return OEMCrypto_LoadUsageTableHeader(kLevelDefault, buffer, buffer_length);
}

extern "C" OEMCryptoResult OEMCrypto_CreateNewUsageEntry(
    OEMCrypto_SESSION session, uint32_t* usage_entry_number) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  LevelSession pair = gAdapter->GetSession(session);
  if (!pair.fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (pair.fcn->version < 13) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  if (pair.fcn->CreateNewUsageEntry == NULL)
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  return pair.fcn->CreateNewUsageEntry(pair.session, usage_entry_number);
}

extern "C" OEMCryptoResult OEMCrypto_LoadUsageEntry(OEMCrypto_SESSION session,
                                                    uint32_t index,
                                                    const uint8_t* buffer,
                                                    size_t buffer_size) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  LevelSession pair = gAdapter->GetSession(session);
  if (!pair.fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (pair.fcn->version < 13) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  if (pair.fcn->LoadUsageEntry == NULL) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  return pair.fcn->LoadUsageEntry(pair.session, index, buffer, buffer_size);
}

extern "C" OEMCryptoResult OEMCrypto_UpdateUsageEntry(
    OEMCrypto_SESSION session, uint8_t* header_buffer,
    size_t* header_buffer_length, uint8_t* entry_buffer,
    size_t* entry_buffer_length) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  LevelSession pair = gAdapter->GetSession(session);
  if (!pair.fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (pair.fcn->version < 13) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  if (pair.fcn->UpdateUsageEntry == NULL)
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  return pair.fcn->UpdateUsageEntry(pair.session, header_buffer,
                                    header_buffer_length, entry_buffer,
                                    entry_buffer_length);
}

extern "C" OEMCryptoResult OEMCrypto_ShrinkUsageTableHeader(
    uint32_t new_table_size, uint8_t* header_buffer,
    size_t* header_buffer_length) {
  return OEMCrypto_ShrinkUsageTableHeader(kLevelDefault, new_table_size,
                                          header_buffer, header_buffer_length);
}

extern "C" OEMCryptoResult OEMCrypto_MoveEntry(OEMCrypto_SESSION session,
                                               uint32_t new_index) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  LevelSession pair = gAdapter->GetSession(session);
  if (!pair.fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (pair.fcn->version < 13) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  if (pair.fcn->MoveEntry == NULL) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  return pair.fcn->MoveEntry(pair.session, new_index);
}

extern "C" OEMCryptoResult OEMCrypto_CopyOldUsageEntry(
    OEMCrypto_SESSION session, const uint8_t* pst, size_t pst_length) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  LevelSession pair = gAdapter->GetSession(session);
  if (!pair.fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (pair.fcn->version < 13) return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  if (pair.fcn->CopyOldUsageEntry == NULL)
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  return pair.fcn->CopyOldUsageEntry(pair.session, pst, pst_length);
}

extern "C" OEMCryptoResult OEMCrypto_CreateOldUsageEntry(
    uint64_t time_since_license_received, uint64_t time_since_first_decrypt,
    uint64_t time_since_last_decrypt, OEMCrypto_Usage_Entry_Status status,
    uint8_t* server_mac_key, uint8_t* client_mac_key, const uint8_t* pst,
    size_t pst_length) {
  return OEMCrypto_CreateOldUsageEntry(
      kLevelDefault, time_since_license_received, time_since_first_decrypt,
      time_since_last_decrypt, status, server_mac_key, client_mac_key, pst,
      pst_length);
}

extern "C" uint32_t OEMCrypto_SupportsDecryptHash(){
  return OEMCrypto_SupportsDecryptHash(kLevelDefault);
}

extern "C" OEMCryptoResult OEMCrypto_SetDecryptHash(OEMCrypto_SESSION session,
                                         uint32_t frame_number,
                                         const uint8_t* hash,
                                         size_t hash_length) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  LevelSession pair = gAdapter->GetSession(session);
  if (!pair.fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (pair.fcn->SetDecryptHash == NULL) {
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  }
  return pair.fcn->SetDecryptHash(pair.session, frame_number, hash, hash_length);
}

extern "C" OEMCryptoResult OEMCrypto_GetHashErrorCode(OEMCrypto_SESSION session,
                                           uint32_t* failed_frame_number) {
  if (!gAdapter.get()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  LevelSession pair = gAdapter->GetSession(session);
  if (!pair.fcn) return OEMCrypto_ERROR_INVALID_SESSION;
  if (pair.fcn->GetHashErrorCode == NULL) {
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  }
  return pair.fcn->GetHashErrorCode(pair.session, failed_frame_number);
}
