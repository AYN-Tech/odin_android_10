// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

/*********************************************************************
 * level3.h
 *
 * Reference APIs needed to support Widevine's crypto algorithms.
 *********************************************************************/

#ifndef LEVEL3_OEMCRYPTO_H_
#define LEVEL3_OEMCRYPTO_H_

#include <stddef.h>
#include <stdint.h>

#include "level3_file_system.h"
#include "OEMCryptoCENC.h"
#include "oemcrypto_adapter.h"

namespace wvoec3 {

#ifdef DYNAMIC_ADAPTER
#define Level3_IsInApp                     _lcc00
#define Level3_Initialize                  _lcc01
#define Level3_Terminate                   _lcc02
#define Level3_InstallKeyboxOrOEMCert      _lcc03
#define Level3_GetKeyData                  _lcc04
#define Level3_IsKeyboxOrOEMCertValid      _lcc05
#define Level3_GetRandom                   _lcc06
#define Level3_GetDeviceID                 _lcc07
#define Level3_WrapKeyboxOrOEMCert         _lcc08
#define Level3_OpenSession                 _lcc09
#define Level3_CloseSession                _lcc10
#define Level3_DecryptCENC                 _lcc11
#define Level3_GenerateDerivedKeys         _lcc12
#define Level3_GenerateSignature           _lcc13
#define Level3_GenerateNonce               _lcc14
#define Level3_RewrapDeviceRSAKey          _lcc18
#define Level3_LoadDeviceRSAKey            _lcc19
#define Level3_GenerateRSASignature        _lcc20
#define Level3_DeriveKeysFromSessionKey    _lcc21
#define Level3_APIVersion                  _lcc22
#define Level3_SecurityLevel               _lcc23
#define Level3_Generic_Encrypt             _lcc24
#define Level3_Generic_Decrypt             _lcc25
#define Level3_Generic_Sign                _lcc26
#define Level3_Generic_Verify              _lcc27
#define Level3_GetHDCPCapability           _lcc28
#define Level3_SupportsUsageTable          _lcc29
#define Level3_UpdateUsageTable            _lcc30
#define Level3_DeactivateUsageEntry        _lcc31
#define Level3_ReportUsage                 _lcc32
#define Level3_DeleteUsageEntry            _lcc33
#define Level3_DeleteOldUsageTable         _lcc34
#define Level3_GetMaxNumberOfSessions      _lcc37
#define Level3_GetNumberOfOpenSessions     _lcc38
#define Level3_IsAntiRollbackHwPresent     _lcc39
#define Level3_QueryKeyControl             _lcc41
#define Level3_ForceDeleteUsageEntry       _lcc43
#define Level3_LoadTestRSAKey              _lcc45
#define Level3_SecurityPatchLevel          _lcc46
#define Level3_GetProvisioningMethod       _lcc49
#define Level3_GetOEMPublicCertificate     _lcc50
#define Level3_RewrapDeviceRSAKey30        _lcc51
#define Level3_SupportedCertificates       _lcc52
#define Level3_IsSRMUpdateSupported        _lcc53
#define Level3_GetCurrentSRMVersion        _lcc54
#define Level3_LoadSRM                     _lcc55
#define Level3_RemoveSRM                   _lcc57
#define Level3_CreateUsageTableHeader      _lcc61
#define Level3_LoadUsageTableHeader        _lcc62
#define Level3_CreateNewUsageEntry         _lcc63
#define Level3_LoadUsageEntry              _lcc64
#define Level3_UpdateUsageEntry            _lcc65
#define Level3_ShrinkUsageTableHeader      _lcc67
#define Level3_MoveEntry                   _lcc68
#define Level3_CopyOldUsageEntry           _lcc69
#define Level3_CreateOldUsageEntry         _lcc70
#define Level3_GetAnalogOutputFlags        _lcc71
#define Level3_LoadTestKeybox              _lcc78
#define Level3_SelectKey                   _lcc81
#define Level3_LoadKeys                    _lcc83
#define Level3_SetSandbox                  _lcc84
#define Level3_ResourceRatingTier          _lcc85
#define Level3_SupportsDecryptHash         _lcc86
#define Level3_SetDecryptHash              _lcc88
#define Level3_GetHashErrorCode            _lcc89
#define Level3_BuildInformation            _lcc90
#define Level3_RefreshKeys                 _lcc91
#define Level3_LoadEntitledContentKeys     _lcc92
#define Level3_CopyBuffer                  _lcc93
#else
#define Level3_Initialize                  _oecc01
#define Level3_Terminate                   _oecc02
#define Level3_InstallKeyboxOrOEMCert      _oecc03
#define Level3_GetKeyData                  _oecc04
#define Level3_IsKeyboxOrOEMCertValid      _oecc05
#define Level3_GetRandom                   _oecc06
#define Level3_GetDeviceID                 _oecc07
#define Level3_WrapKeyboxOrOEMCert         _oecc08
#define Level3_OpenSession                 _oecc09
#define Level3_CloseSession                _oecc10
#define Level3_GenerateDerivedKeys         _oecc12
#define Level3_GenerateSignature           _oecc13
#define Level3_GenerateNonce               _oecc14
#define Level3_RewrapDeviceRSAKey          _oecc18
#define Level3_LoadDeviceRSAKey            _oecc19
#define Level3_DeriveKeysFromSessionKey    _oecc21
#define Level3_APIVersion                  _oecc22
#define Level3_SecurityLevel               _oecc23
#define Level3_Generic_Encrypt             _oecc24
#define Level3_Generic_Decrypt             _oecc25
#define Level3_Generic_Sign                _oecc26
#define Level3_Generic_Verify              _oecc27
#define Level3_SupportsUsageTable          _oecc29
#define Level3_UpdateUsageTable            _oecc30
#define Level3_ReportUsage                 _oecc32
#define Level3_DeleteUsageEntry            _oecc33
#define Level3_DeleteOldUsageTable         _oecc34
#define Level3_GenerateRSASignature        _oecc36
#define Level3_GetMaxNumberOfSessions      _oecc37
#define Level3_GetNumberOfOpenSessions     _oecc38
#define Level3_IsAntiRollbackHwPresent     _oecc39
#define Level3_QueryKeyControl             _oecc41
#define Level3_ForceDeleteUsageEntry       _oecc43
#define Level3_GetHDCPCapability           _oecc44
#define Level3_LoadTestRSAKey              _oecc45
#define Level3_SecurityPatchLevel          _oecc46
#define Level3_DecryptCENC                 _oecc48
#define Level3_GetProvisioningMethod       _oecc49
#define Level3_GetOEMPublicCertificate     _oecc50
#define Level3_RewrapDeviceRSAKey30        _oecc51
#define Level3_SupportedCertificates       _oecc52
#define Level3_IsSRMUpdateSupported        _oecc53
#define Level3_GetCurrentSRMVersion        _oecc54
#define Level3_LoadSRM                     _oecc55
#define Level3_RemoveSRM                   _oecc57
#define Level3_CreateUsageTableHeader      _oecc61
#define Level3_LoadUsageTableHeader        _oecc62
#define Level3_CreateNewUsageEntry         _oecc63
#define Level3_LoadUsageEntry              _oecc64
#define Level3_UpdateUsageEntry            _oecc65
#define Level3_DeactivateUsageEntry        _oecc66
#define Level3_ShrinkUsageTableHeader      _oecc67
#define Level3_MoveEntry                   _oecc68
#define Level3_CopyOldUsageEntry           _oecc69
#define Level3_CreateOldUsageEntry         _oecc70
#define Level3_GetAnalogOutputFlags        _oecc71
#define Level3_LoadTestKeybox              _oecc78
#define Level3_SelectKey                   _oecc81
#define Level3_LoadKeys                    _oecc83
#define Level3_SetSandbox                  _oecc84
#define Level3_ResourceRatingTier          _oecc85
#define Level3_SupportsDecryptHash         _oecc86
#define Level3_SetDecryptHash              _oecc88
#define Level3_GetHashErrorCode            _oecc89
#define Level3_BuildInformation            _oecc90
#define Level3_RefreshKeys                 _oecc91
#define Level3_LoadEntitledContentKeys     _oecc92
#define Level3_CopyBuffer                  _oecc93
#endif

#define Level3_GetInitializationState      _oecl3o01

extern "C" {

bool Level3_IsInApp();
OEMCryptoResult Level3_Initialize(void);
OEMCryptoResult Level3_Terminate(void);
OEMCryptoResult Level3_OpenSession(OEMCrypto_SESSION *session);
OEMCryptoResult Level3_CloseSession(OEMCrypto_SESSION session);
OEMCryptoResult Level3_GenerateDerivedKeys(OEMCrypto_SESSION session,
                                           const uint8_t *mac_key_context,
                                           uint32_t mac_key_context_length,
                                           const uint8_t *enc_key_context,
                                           uint32_t enc_key_context_length);
OEMCryptoResult Level3_GenerateNonce(OEMCrypto_SESSION session,
                                     uint32_t* nonce);
OEMCryptoResult Level3_GenerateSignature(OEMCrypto_SESSION session,
                                         const uint8_t* message,
                                         size_t message_length,
                                         uint8_t* signature,
                                         size_t* signature_length);
OEMCryptoResult Level3_QueryKeyControl(OEMCrypto_SESSION session,
                                       const uint8_t* key_id,
                                       size_t key_id_length,
                                       uint8_t* key_control_block,
                                       size_t* key_control_block_length);
OEMCryptoResult Level3_DecryptCENC(OEMCrypto_SESSION session,
                                   const uint8_t *data_addr,
                                   size_t data_length,
                                   bool is_encrypted,
                                   const uint8_t *iv,
                                   size_t block_offset,
                                   OEMCrypto_DestBufferDesc* out_buffer,
                                   const OEMCrypto_CENCEncryptPatternDesc* pattern,
                                   uint8_t subsample_flags);
OEMCryptoResult Level3_InstallKeyboxOrOEMCert(const uint8_t* rot,
                                              size_t rotLength);
OEMCryptoResult Level3_IsKeyboxOrOEMCertValid(void);
OEMCryptoResult Level3_WrapKeyboxOrOEMCert(const uint8_t* rot,
                                           size_t rotLength,
                                           uint8_t* wrappedRot,
                                           size_t* wrappedRotLength,
                                           const uint8_t* transportKey,
                                           size_t transportKeyLength);
OEMCrypto_ProvisioningMethod Level3_GetProvisioningMethod();
OEMCryptoResult Level3_GetOEMPublicCertificate(OEMCrypto_SESSION session,
                                               uint8_t *public_cert,
                                               size_t *public_cert_length);
OEMCryptoResult Level3_GetDeviceID(uint8_t* deviceID,
                                   size_t *idLength);
OEMCryptoResult Level3_GetKeyData(uint8_t* keyData,
                                  size_t *keyDataLength);
OEMCryptoResult Level3_GetRandom(uint8_t* randomData,
                                 size_t dataLength);
OEMCryptoResult Level3_RewrapDeviceRSAKey30(OEMCrypto_SESSION session,
                                            const uint32_t *nonce,
                                            const uint8_t* encrypted_message_key,
                                            size_t encrypted_message_key_length,
                                            const uint8_t* enc_rsa_key,
                                            size_t enc_rsa_key_length,
                                            const uint8_t* enc_rsa_key_iv,
                                            uint8_t* wrapped_rsa_key,
                                            size_t* wrapped_rsa_key_length);
OEMCryptoResult Level3_RewrapDeviceRSAKey(OEMCrypto_SESSION session,
                                          const uint8_t* message,
                                          size_t message_length,
                                          const uint8_t* signature,
                                          size_t signature_length,
                                          const uint32_t *nonce,
                                          const uint8_t* enc_rsa_key,
                                          size_t enc_rsa_key_length,
                                          const uint8_t* enc_rsa_key_iv,
                                          uint8_t* wrapped_rsa_key,
                                          size_t *wrapped_rsa_key_length);
OEMCryptoResult Level3_LoadDeviceRSAKey(OEMCrypto_SESSION session,
                                        const uint8_t* wrapped_rsa_key,
                                        size_t wrapped_rsa_key_length);
OEMCryptoResult Level3_LoadTestRSAKey();
OEMCryptoResult Level3_GenerateRSASignature(OEMCrypto_SESSION session,
                                            const uint8_t* message,
                                            size_t message_length,
                                            uint8_t* signature,
                                            size_t *signature_length,
                                            RSA_Padding_Scheme padding_scheme);
OEMCryptoResult Level3_DeriveKeysFromSessionKey(OEMCrypto_SESSION session,
                                                const uint8_t* enc_session_key,
                                                size_t enc_session_key_length,
                                                const uint8_t *mac_key_context,
                                                size_t mac_key_context_length,
                                                const uint8_t *enc_key_context,
                                                size_t enc_key_context_length);
uint32_t Level3_APIVersion();
uint8_t Level3_SecurityPatchLevel();
const char* Level3_SecurityLevel();
OEMCryptoResult Level3_GetHDCPCapability(OEMCrypto_HDCP_Capability* current,
                                         OEMCrypto_HDCP_Capability* maximum);
bool Level3_SupportsUsageTable();
bool Level3_IsAntiRollbackHwPresent();
OEMCryptoResult Level3_GetNumberOfOpenSessions(size_t* count);
OEMCryptoResult Level3_GetMaxNumberOfSessions(size_t* maximum);
uint32_t Level3_SupportedCertificates();
OEMCryptoResult Level3_Generic_Encrypt(OEMCrypto_SESSION session,
                                       const uint8_t* in_buffer,
                                       size_t buffer_length,
                                       const uint8_t* iv,
                                       OEMCrypto_Algorithm algorithm,
                                       uint8_t* out_buffer);
OEMCryptoResult Level3_Generic_Decrypt(OEMCrypto_SESSION session,
                                       const uint8_t* in_buffer,
                                       size_t buffer_length,
                                       const uint8_t* iv,
                                       OEMCrypto_Algorithm algorithm,
                                       uint8_t* out_buffer);
OEMCryptoResult Level3_Generic_Sign(OEMCrypto_SESSION session,
                                    const uint8_t* in_buffer,
                                    size_t buffer_length,
                                    OEMCrypto_Algorithm algorithm,
                                    uint8_t* signature,
                                    size_t* signature_length);
OEMCryptoResult Level3_Generic_Verify(OEMCrypto_SESSION session,
                                      const uint8_t* in_buffer,
                                      size_t buffer_length,
                                      OEMCrypto_Algorithm algorithm,
                                      const uint8_t* signature,
                                      size_t signature_length);
OEMCryptoResult Level3_UpdateUsageTable();
OEMCryptoResult Level3_DeactivateUsageEntry(OEMCrypto_SESSION session,
                                            const uint8_t *pst,
                                            size_t pst_length);
OEMCryptoResult Level3_ReportUsage(OEMCrypto_SESSION session,
                                   const uint8_t *pst,
                                   size_t pst_length,
                                   uint8_t *buffer,
                                   size_t *buffer_length);
OEMCryptoResult Level3_DeleteUsageEntry(OEMCrypto_SESSION session,
                                        const uint8_t* pst,
                                        size_t pst_length,
                                        const uint8_t *message,
                                        size_t message_length,
                                        const uint8_t *signature,
                                        size_t signature_length);
OEMCryptoResult Level3_ForceDeleteUsageEntry(const uint8_t* pst,
                                             size_t pst_length);
OEMCryptoResult Level3_DeleteOldUsageTable();
bool Level3_IsSRMUpdateSupported();
OEMCryptoResult Level3_GetCurrentSRMVersion(uint16_t* version);
OEMCryptoResult Level3_LoadSRM(const uint8_t* buffer,
                                  size_t buffer_length);
OEMCryptoResult Level3_RemoveSRM();
OEMCryptoResult Level3_CreateUsageTableHeader(uint8_t* header_buffer,
                                              size_t* header_buffer_length);
OEMCryptoResult Level3_LoadUsageTableHeader(const uint8_t* buffer,
                                            size_t buffer_length);
OEMCryptoResult Level3_CreateNewUsageEntry(OEMCrypto_SESSION session,
                                           uint32_t *usage_entry_number);
OEMCryptoResult Level3_LoadUsageEntry(OEMCrypto_SESSION session,
                                      uint32_t index,
                                      const uint8_t *buffer,
                                      size_t buffer_size);
OEMCryptoResult Level3_UpdateUsageEntry(OEMCrypto_SESSION session,
                                        uint8_t* header_buffer,
                                        size_t* header_buffer_length,
                                        uint8_t* entry_buffer,
                                        size_t* entry_buffer_length);
OEMCryptoResult Level3_ShrinkUsageTableHeader(uint32_t new_table_size,
                                              uint8_t* header_buffer,
                                              size_t* header_buffer_length);
OEMCryptoResult Level3_MoveEntry(OEMCrypto_SESSION session,
                                 uint32_t new_index);
OEMCryptoResult Level3_CopyOldUsageEntry(OEMCrypto_SESSION session,
                                         const uint8_t*pst,
                                         size_t pst_length);
OEMCryptoResult Level3_CreateOldUsageEntry(uint64_t time_since_license_received,
                                           uint64_t time_since_first_decrypt,
                                           uint64_t time_since_last_decrypt,
                                           OEMCrypto_Usage_Entry_Status status,
                                           uint8_t *server_mac_key,
                                           uint8_t *client_mac_key,
                                           const uint8_t* pst,
                                           size_t pst_length);
uint32_t Level3_GetAnalogOutputFlags();
OEMCryptoResult Level3_LoadTestKeybox(const uint8_t* buffer, size_t length);
OEMCryptoResult Level3_SelectKey(const OEMCrypto_SESSION session,
                                 const uint8_t* key_id, size_t key_id_length,
                                 OEMCryptoCipherMode cipher_mode);
OEMCryptoResult Level3_LoadKeys(
    OEMCrypto_SESSION session, const uint8_t* message, size_t message_length,
    const uint8_t* signature, size_t signature_length,
    OEMCrypto_Substring enc_mac_keys_iv, OEMCrypto_Substring enc_mac_keys,
    size_t num_keys, const OEMCrypto_KeyObject* key_array,
    OEMCrypto_Substring pst, OEMCrypto_Substring srm_restriction_data,
    OEMCrypto_LicenseType license_type);
OEMCryptoResult Level3_SetSandbox(const uint8_t* sandbox_id,
                                  size_t sandbox_id_length);
uint32_t Level3_ResourceRatingTier();
uint32_t Level3_SupportsDecryptHash();

OEMCryptoResult Level3_SetDecryptHash(OEMCrypto_SESSION session,
                                      uint32_t frame_number,
                                      const uint8_t* hash, size_t hash_length);
OEMCryptoResult Level3_GetHashErrorCode(OEMCrypto_SESSION session,
                                        uint32_t* failed_frame_number);
const char* Level3_BuildInformation();
OEMCryptoResult Level3_RefreshKeys(
    OEMCrypto_SESSION session, const uint8_t* message, size_t message_length,
    const uint8_t* signature, size_t signature_length, size_t num_keys,
    const OEMCrypto_KeyRefreshObject* key_array);
OEMCryptoResult Level3_LoadEntitledContentKeys(
    OEMCrypto_SESSION session, const uint8_t* message, size_t message_length,
    size_t num_keys, const OEMCrypto_EntitledContentKeyObject* key_array);
OEMCryptoResult Level3_CopyBuffer(OEMCrypto_SESSION session,
                                  const uint8_t *data_addr,
                                  size_t data_length,
                                  OEMCrypto_DestBufferDesc* out_buffer,
                                  uint8_t subsample_flags);

// The following are specific to Google's Level 3 implementation and are not
// required.

enum Level3InitializationState {
  LEVEL3_INITIALIZATION_SUCCESS = 0,
  LEVEL3_INITIALIZATION_UNKNOWN_FAILURE = 1,
  LEVEL3_SEED_FAILURE = 2,
  LEVEL3_SAVE_DEVICE_KEYS_FAILURE = 3,
  LEVEL3_READ_DEVICE_KEYS_FAILURE = 4,
  LEVEL3_VERIFY_DEVICE_KEYS_FAILURE = 5,
};

/*
 * Level3_GetInitializationState
 *
 * Description:
 *   Return any warning or error condition which occurred during
 *   initialization.  On some platforms, this value will be logged and metrics
 *   will be gathered on production devices.  This is an optional feature, and
 *   OEMCrypto may always return 0, even if Level3_Initialize failed. This
 *   function may be called whether Level3_Initialize succeeded or not.
 *
 * Parameters:
 *   N/A
 *
 * Threading:
 *   No other function calls will be made while this function is running.
 *
 * Returns:
 *   LEVEL3_INITIALIZATION_SUCCESS - no warnings or errors during initialization
 *   LEVEL3_SEED_FAILURE - error in seeding the software RNG
 *   LEVEL3_SAVE_DEVICE_KEYS_FAILURE - failed to save device keys to file system
 *   LEVEL3_READ_DEVICE_KEYS_FAILURE - failed to read device keys from file
 *   system
 *   LEVEL3_VERIFY_DEVICE_KEYS_FAILURE - failed to verify decrypted device keys
 *
 * Version:
 *   This method is new in API version 14.
 */
Level3InitializationState Level3_GetInitializationState(void);

/*
 * Level3_OutputErrorLogs
 *
 * Description:
 *   Call to output any errors in the Level 3 execution if the Level 3 has
 *   failed. This method should only be called if the Level 3 has failed in
 *   an unrecoverable state, and needs to be reinitialized.
 *
 * Parameters:
 *   N/A
 *
 * Threading:
 *   No other function calls will be made while this function is running.
 *
 * Returns:
 *   N/A
 *
 * Version:
 *   This method is new in API version 15.
 */
void Level3_OutputErrorLogs();

}  // extern "C"

// The following are interfaces needed for Google's Level 3 OEMCrypto
// specifically, which partners are expected to implement.

// Returns a stable, unique identifier for the device. This could be a
// serial number or any other character sequence representing that device.
// The parameter |len| needs to be changed to reflect the length of the
// unique identifier.
const char *getUniqueID(size_t *len);

// Returns a 64-bit unsigned integer to be used as a random seed for RNG.
// If the operation is unsuccessful, this function returns 0.
// We provide a sample implementation under the name generate_entropy_linux.cpp
// which partners should use if they can.
uint64_t generate_entropy();

// Creates and returns an OEMCrypto_Level3FileSystem implementation.
OEMCrypto_Level3FileSystem* createLevel3FileSystem();

// Deletes the pointer retrieved by the function above.
void deleteLevel3FileSystem(OEMCrypto_Level3FileSystem* file_system);

}  // namespace wvoec3

#endif  // LEVEL3_OEMCRYPTO_H_
