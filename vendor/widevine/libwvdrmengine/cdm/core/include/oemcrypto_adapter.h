// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
#ifndef WVCDM_CORE_OEMCRYPTO_ADAPTER_H_
#define WVCDM_CORE_OEMCRYPTO_ADAPTER_H_

#include "OEMCryptoCENC.h"
#include "wv_cdm_types.h"

namespace wvcdm {

// This attempts to open a session at the desired security level.
// If one level is not available, the other will be used instead.
OEMCryptoResult OEMCrypto_OpenSession(OEMCrypto_SESSION* session,
                                      SecurityLevel level);
OEMCryptoResult OEMCrypto_InstallKeybox(const uint8_t* keybox,
                                        size_t keyBoxLength,
                                        SecurityLevel level);
OEMCryptoResult OEMCrypto_IsKeyboxOrOEMCertValid(SecurityLevel level);
OEMCryptoResult OEMCrypto_GetDeviceID(uint8_t* deviceID, size_t* idLength,
                                      SecurityLevel level);
OEMCryptoResult OEMCrypto_GetKeyData(uint8_t* keyData, size_t* keyDataLength,
                                     SecurityLevel level);
uint32_t OEMCrypto_APIVersion(SecurityLevel level);
const char* OEMCrypto_SecurityLevel(SecurityLevel level);
OEMCryptoResult OEMCrypto_GetHDCPCapability(SecurityLevel level,
                                            OEMCrypto_HDCP_Capability* current,
                                            OEMCrypto_HDCP_Capability* maximum);
bool OEMCrypto_SupportsUsageTable(SecurityLevel level);
bool OEMCrypto_IsAntiRollbackHwPresent(SecurityLevel level);
OEMCryptoResult OEMCrypto_GetNumberOfOpenSessions(SecurityLevel level,
                                                  size_t* count);
OEMCryptoResult OEMCrypto_GetMaxNumberOfSessions(SecurityLevel level,
                                                 size_t* maximum);
uint8_t OEMCrypto_Security_Patch_Level(SecurityLevel level);
OEMCrypto_ProvisioningMethod OEMCrypto_GetProvisioningMethod(
    SecurityLevel level);
uint32_t OEMCrypto_SupportedCertificates(SecurityLevel level);
OEMCryptoResult OEMCrypto_CreateUsageTableHeader(SecurityLevel level,
                                                 uint8_t* header_buffer,
                                                 size_t* header_buffer_length);
OEMCryptoResult OEMCrypto_LoadUsageTableHeader(SecurityLevel level,
                                               const uint8_t* buffer,
                                               size_t buffer_length);
OEMCryptoResult OEMCrypto_ShrinkUsageTableHeader(SecurityLevel level,
                                                 uint32_t new_table_size,
                                                 uint8_t* header_buffer,
                                                 size_t* header_buffer_length);
OEMCryptoResult OEMCrypto_CreateOldUsageEntry(SecurityLevel level,
    uint64_t time_since_license_received, uint64_t time_since_first_decrypt,
    uint64_t time_since_last_decrypt, OEMCrypto_Usage_Entry_Status status,
    uint8_t* server_mac_key, uint8_t* client_mac_key, const uint8_t* pst,
    size_t pst_length);
uint32_t OEMCrypto_GetAnalogOutputFlags(SecurityLevel level);
const char* OEMCrypto_BuildInformation(SecurityLevel level);
uint32_t OEMCrypto_ResourceRatingTier(SecurityLevel level);
uint32_t OEMCrypto_SupportsDecryptHash(SecurityLevel level);
}  // namespace wvcdm

/* The following functions are deprecated in OEMCrypto v13.  They are defined
 * here so that core cdm code may be backwards compatible with an OEMCrypto
 * v12.
 */
extern "C" {

typedef struct {  // Used for backwards compatibility.
  const uint8_t* key_id;
  size_t key_id_length;
  const uint8_t* key_data_iv;
  const uint8_t* key_data;
  size_t key_data_length;
  const uint8_t* key_control_iv;
  const uint8_t* key_control;
} OEMCrypto_KeyObject_V10;

typedef struct {  // Used for backwards compatibility.
  const uint8_t* key_id;
  size_t key_id_length;
  const uint8_t* key_data_iv;
  const uint8_t* key_data;
  size_t key_data_length;
  const uint8_t* key_control_iv;
  const uint8_t* key_control;
  OEMCryptoCipherMode cipher_mode;
} OEMCrypto_KeyObject_V13;

typedef struct {
  const uint8_t* key_id;
  size_t key_id_length;
  const uint8_t* key_data_iv;
  const uint8_t* key_data;
  size_t key_data_length;
  const uint8_t* key_control_iv;
  const uint8_t* key_control;
} OEMCrypto_KeyObject_V14;

// Backwards compitiblity between v14 and v13.
OEMCryptoResult OEMCrypto_LoadKeys_Back_Compat(
    OEMCrypto_SESSION session, const uint8_t* message, size_t message_length,
    const uint8_t* signature, size_t signature_length,
    OEMCrypto_Substring enc_mac_keys_iv, OEMCrypto_Substring enc_mac_keys,
    size_t num_keys, const OEMCrypto_KeyObject* key_array,
    OEMCrypto_Substring pst, OEMCrypto_Substring srm_restriction_data,
    OEMCrypto_LicenseType license_type, OEMCryptoCipherMode* cipher_modes);

OEMCryptoResult OEMCrypto_UpdateUsageTable();

OEMCryptoResult OEMCrypto_DeactivateUsageEntry_V12(const uint8_t* pst,
                                                  size_t pst_length);
OEMCryptoResult OEMCrypto_DeleteUsageEntry(
    OEMCrypto_SESSION session, const uint8_t* pst, size_t pst_length,
    const uint8_t* message, size_t message_length, const uint8_t* signature,
    size_t signature_length);

OEMCryptoResult OEMCrypto_ForceDeleteUsageEntry(const uint8_t* pst,
                                                size_t pst_length);

typedef struct {
  const uint8_t* entitlement_key_id;
  size_t entitlement_key_id_length;
  const uint8_t* content_key_id;
  size_t content_key_id_length;
  const uint8_t* content_key_data_iv;
  const uint8_t* content_key_data;
  size_t content_key_data_length;
} OEMCrypto_EntitledContentKeyObject_V14;

typedef struct {
  const uint8_t* key_id;
  size_t key_id_length;
  const uint8_t* key_control_iv;
  const uint8_t* key_control;
} OEMCrypto_KeyRefreshObject_V14;

OEMCryptoResult OEMCrypto_LoadKeys_V14(
    OEMCrypto_SESSION session, const uint8_t* message, size_t message_length,
    const uint8_t* signature, size_t signature_length,
    const uint8_t* enc_mac_keys_iv, const uint8_t* enc_mac_keys,
    size_t num_keys, const OEMCrypto_KeyObject_V14* key_array,
    const uint8_t* pst, size_t pst_length, const uint8_t* srm_requirement,
    OEMCrypto_LicenseType license_type);

OEMCryptoResult OEMCrypto_LoadEntitledContentKeys_V14(
    OEMCrypto_SESSION session, size_t num_keys,
    const OEMCrypto_EntitledContentKeyObject_V14* key_array);

OEMCryptoResult OEMCrypto_RefreshKeys_V14(
    OEMCrypto_SESSION session, const uint8_t* message, size_t message_length,
    const uint8_t* signature, size_t signature_length, size_t num_keys,
    const OEMCrypto_KeyRefreshObject_V14* key_array);

OEMCryptoResult OEMCrypto_CopyBuffer_V14(const uint8_t* data_addr,
                                         size_t data_length,
                                         OEMCrypto_DestBufferDesc* out_buffer,
                                         uint8_t subsample_flags);

}  // extern "C"

#endif  // WVCDM_CORE_OEMCRYPTO_ADAPTER_H_
