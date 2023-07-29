// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
//  Reference implementation of OEMCrypto APIs
//
#ifndef REF_OEMCRYPTO_SESSION_H_
#define REF_OEMCRYPTO_SESSION_H_

#include <stdint.h>
#include <time.h>
#include <map>
#include <vector>

#include <openssl/rsa.h>

#include "OEMCryptoCENC.h"
#include "oemcrypto_auth_ref.h"
#include "oemcrypto_key_ref.h"
#include "oemcrypto_nonce_table.h"
#include "oemcrypto_rsa_key_shared.h"
#include "oemcrypto_session_key_table.h"
#include "oemcrypto_types.h"
#include "oemcrypto_usage_table_ref.h"

namespace wvoec_ref {

class CryptoEngine;
typedef uint32_t SessionId;

enum SRMVersionStatus { NoSRMVersion, ValidSRMVersion, InvalidSRMVersion };

// TODO(jfore): Is there a better name?
class SessionContextKeys {
 public:
  virtual OEMCrypto_LicenseType type() = 0;
  virtual size_t size() = 0;
  virtual bool Insert(const KeyId& key_id, const Key& key_data) = 0;
  virtual Key* Find(const KeyId& key_id) = 0;
  virtual void Remove(const KeyId& key_id) = 0;
  virtual void UpdateDuration(const KeyControlBlock& control) = 0;

  // Methods supported exclusively for entitlement keys. Returns false if
  // entitlement keys are not found or not supported by the current key table.
  // It is the caller's responsibility to check the context.
  virtual bool SetContentKey(const KeyId& entitlement_id,
                             const KeyId& content_key_id,
                             const std::vector<uint8_t>& content_key) = 0;
  virtual EntitlementKey* GetEntitlementKey(const KeyId& entitlement_id) = 0;

  virtual ~SessionContextKeys() {}

 protected:
  SessionContextKeys() {}

 private:
  CORE_DISALLOW_COPY_AND_ASSIGN(SessionContextKeys);
};

class SessionContext {
 private:
  SessionContext() {}

 public:
  SessionContext(CryptoEngine* ce, SessionId sid, const RSA_shared_ptr& rsa_key)
      : valid_(true),
        ce_(ce),
        id_(sid),
        current_content_key_(NULL),
        session_keys_(NULL),
        rsa_key_(rsa_key),
        allowed_schemes_(kSign_RSASSA_PSS),
        usage_entry_(NULL),
        srm_requirements_status_(NoSRMVersion),
        usage_entry_status_(kNoUsageEntry),
        compute_hash_(false),
        current_hash_(0),
        bad_frame_number_(0),
        hash_error_(OEMCrypto_SUCCESS) {}
  virtual ~SessionContext();

  bool isValid() { return valid_; }

  virtual bool DeriveKeys(const std::vector<uint8_t>& master_key,
                          const std::vector<uint8_t>& mac_context,
                          const std::vector<uint8_t>& enc_context);
  virtual bool RSADeriveKeys(const std::vector<uint8_t>& enc_session_key,
                             const std::vector<uint8_t>& mac_context,
                             const std::vector<uint8_t>& enc_context);
  virtual bool GenerateSignature(const uint8_t* message, size_t message_length,
                                 uint8_t* signature, size_t* signature_length);
  size_t RSASignatureSize();
  virtual OEMCryptoResult GenerateRSASignature(
      const uint8_t* message, size_t message_length, uint8_t* signature,
      size_t* signature_length, RSA_Padding_Scheme padding_scheme);
  virtual bool ValidateMessage(const uint8_t* message, size_t message_length,
                               const uint8_t* signature,
                               size_t signature_length);
  OEMCryptoResult DecryptCENC(const uint8_t* iv, size_t block_offset,
                              const OEMCrypto_CENCEncryptPatternDesc* pattern,
                              const uint8_t* cipher_data,
                              size_t cipher_data_length, bool is_encrypted,
                              uint8_t* clear_data,
                              OEMCryptoBufferType buffer_type,
                              uint8_t subsample_flags);

  OEMCryptoResult Generic_Encrypt(const uint8_t* in_buffer,
                                  size_t buffer_length, const uint8_t* iv,
                                  OEMCrypto_Algorithm algorithm,
                                  uint8_t* out_buffer);
  OEMCryptoResult Generic_Decrypt(const uint8_t* in_buffer,
                                  size_t buffer_length, const uint8_t* iv,
                                  OEMCrypto_Algorithm algorithm,
                                  uint8_t* out_buffer);
  OEMCryptoResult Generic_Sign(const uint8_t* in_buffer, size_t buffer_length,
                               OEMCrypto_Algorithm algorithm,
                               uint8_t* signature, size_t* signature_length);
  OEMCryptoResult Generic_Verify(const uint8_t* in_buffer, size_t buffer_length,
                                 OEMCrypto_Algorithm algorithm,
                                 const uint8_t* signature,
                                 size_t signature_length);
  void StartTimer();
  uint32_t CurrentTimer();  // (seconds).
  virtual OEMCryptoResult LoadKeys(
      const uint8_t* message, size_t message_length, const uint8_t* signature,
      size_t signature_length, OEMCrypto_Substring enc_mac_keys_iv,
      OEMCrypto_Substring enc_mac_keys, size_t num_keys,
      const OEMCrypto_KeyObject* key_array, OEMCrypto_Substring pst,
      OEMCrypto_Substring srm_restriction_data,
      OEMCrypto_LicenseType license_type);
  OEMCryptoResult LoadEntitledContentKeys(
      const uint8_t* message, size_t message_length, size_t num_keys,
      const OEMCrypto_EntitledContentKeyObject* key_array);
  virtual OEMCryptoResult InstallKey(
      const KeyId& key_id, const std::vector<uint8_t>& key_data,
      const std::vector<uint8_t>& key_data_iv,
      const std::vector<uint8_t>& key_control,
      const std::vector<uint8_t>& key_control_iv);
  bool InstallRSAEncryptedKey(const uint8_t* encrypted_message_key,
                              size_t encrypted_message_key_length);
  bool DecryptRSAKey(const uint8_t* enc_rsa_key, size_t enc_rsa_key_length,
                     const uint8_t* wrapped_rsa_key_iv, uint8_t* pkcs8_rsa_key);
  bool EncryptRSAKey(const uint8_t* pkcs8_rsa_key, size_t enc_rsa_key_length,
                     const uint8_t* enc_rsa_key_iv, uint8_t* enc_rsa_key);
  bool LoadRSAKey(const uint8_t* pkcs8_rsa_key, size_t rsa_key_length);
  virtual OEMCryptoResult RefreshKey(
      const KeyId& key_id, const std::vector<uint8_t>& key_control,
      const std::vector<uint8_t>& key_control_iv);
  virtual bool UpdateMacKeys(const std::vector<uint8_t>& mac_keys,
                             const std::vector<uint8_t>& iv);
  virtual bool QueryKeyControlBlock(const KeyId& key_id, uint32_t* data);
  virtual OEMCryptoResult SelectContentKey(const KeyId& key_id,
                                           OEMCryptoCipherMode cipher_mode);
  virtual OEMCryptoResult SetDecryptHash(uint32_t frame_number,
                                         const uint8_t* hash,
                                         size_t hash_length);
  virtual OEMCryptoResult GetHashErrorCode(uint32_t* failed_frame_number);
  const Key* current_content_key(void) { return current_content_key_; }
  void set_mac_key_server(const std::vector<uint8_t>& mac_key_server) {
    mac_key_server_ = mac_key_server;
  }
  const std::vector<uint8_t>& mac_key_server() { return mac_key_server_; }
  void set_mac_key_client(const std::vector<uint8_t>& mac_key_client) {
    mac_key_client_ = mac_key_client;
  }
  const std::vector<uint8_t>& mac_key_client() { return mac_key_client_; }

  void set_encryption_key(const std::vector<uint8_t>& enc_key) {
    encryption_key_ = enc_key;
  }
  const std::vector<uint8_t>& encryption_key() { return encryption_key_; }
  uint32_t allowed_schemes() const { return allowed_schemes_; }

  void AddNonce(uint32_t nonce);
  bool CheckNonce(uint32_t nonce);
  // Verify that the nonce does not match any in this session's nonce table.
  bool NonceCollision(uint32_t nonce) const {
    return nonce_table_.NonceCollision(nonce);
  }
  void FlushNonces();

  virtual OEMCryptoResult CreateNewUsageEntry(uint32_t* usage_entry_number);
  virtual OEMCryptoResult LoadUsageEntry(uint32_t index,
                                         const std::vector<uint8_t>& buffer);
  virtual OEMCryptoResult UpdateUsageEntry(uint8_t* header_buffer,
                                           size_t* header_buffer_length,
                                           uint8_t* entry_buffer,
                                           size_t* entry_buffer_length);
  virtual OEMCryptoResult DeactivateUsageEntry(const std::vector<uint8_t>& pst);
  virtual OEMCryptoResult ReportUsage(const std::vector<uint8_t>& pst,
                                      uint8_t* buffer, size_t* buffer_length);
  OEMCryptoResult MoveEntry(uint32_t new_index);
  OEMCryptoResult CopyOldUsageEntry(const std::vector<uint8_t>& pst);

 protected:
  bool DeriveKey(const std::vector<uint8_t>& key,
                 const std::vector<uint8_t>& context, int counter,
                 std::vector<uint8_t>* out);
  bool DecryptMessage(const std::vector<uint8_t>& key,
                      const std::vector<uint8_t>& iv,
                      const std::vector<uint8_t>& message,
                      std::vector<uint8_t>* decrypted,
                      uint32_t key_size);  // AES key size, in bits.
  // Either verify the nonce or usage entry, as required by the key control
  // block.
  OEMCryptoResult CheckNonceOrEntry(const KeyControlBlock& key_control_block);
  // If there is a usage entry, check that it is not inactive.
  // It also updates the status of the entry if needed.
  bool CheckUsageEntry();
  // Check that the usage entry status is valid for online use.
  OEMCryptoResult CheckStatusOnline(uint32_t nonce, uint32_t control);
  // Check that the usage entry status is valid for offline use.
  OEMCryptoResult CheckStatusOffline(uint32_t nonce, uint32_t control);
  OEMCryptoResult ChooseDecrypt(const uint8_t* iv, size_t block_offset,
                                const OEMCrypto_CENCEncryptPatternDesc* pattern,
                                const uint8_t* cipher_data,
                                size_t cipher_data_length, bool is_encrypted,
                                uint8_t* clear_data,
                                OEMCryptoBufferType buffer_type);
  OEMCryptoResult DecryptCBC(const uint8_t* key, const uint8_t* iv,
                             const OEMCrypto_CENCEncryptPatternDesc* pattern,
                             const uint8_t* cipher_data,
                             size_t cipher_data_length, uint8_t* clear_data);
  OEMCryptoResult PatternDecryptCTR(
      const uint8_t* key, const uint8_t* iv, size_t block_offset,
      const OEMCrypto_CENCEncryptPatternDesc* pattern,
      const uint8_t* cipher_data, size_t cipher_data_length,
      uint8_t* clear_data);
  OEMCryptoResult DecryptCTR(const uint8_t* key_u8, const uint8_t* iv,
                             size_t block_offset, const uint8_t* cipher_data,
                             size_t cipher_data_length, uint8_t* clear_data);
  // Checks if the key is allowed for the specified type.  If there is a usage
  // entry, it also checks the usage entry.
  OEMCryptoResult CheckKeyUse(const std::string& log_string, uint32_t use_type,
                              OEMCryptoBufferType buffer_type);
  RSA* rsa_key() { return rsa_key_.get(); }

  bool valid_;
  CryptoEngine* ce_;
  SessionId id_;
  std::vector<uint8_t> mac_key_server_;
  std::vector<uint8_t> mac_key_client_;
  std::vector<uint8_t> encryption_key_;
  std::vector<uint8_t> session_key_;
  const Key* current_content_key_;
  SessionContextKeys* session_keys_;
  NonceTable nonce_table_;
  RSA_shared_ptr rsa_key_;
  uint32_t allowed_schemes_;  // for RSA signatures.
  time_t timer_start_;
  UsageTableEntry* usage_entry_;
  SRMVersionStatus srm_requirements_status_;
  enum UsageEntryStatus {
    kNoUsageEntry,      // No entry loaded for this session.
    kUsageEntryNew,     // After entry was created.
    kUsageEntryLoaded,  // After loading entry or loading keys.
  };
  UsageEntryStatus usage_entry_status_;

  // These are used when doing full decrypt path testing.
  bool compute_hash_;  // True if the current frame needs a hash.
  uint32_t current_hash_;  // Running CRC hash of frame.
  uint32_t given_hash_;    // True CRC hash of frame.
  uint32_t current_frame_number_;  // Current frame for CRC hash.
  uint32_t bad_frame_number_;  // Frame number with bad hash.
  OEMCryptoResult hash_error_;  // Error code for first bad frame.

  CORE_DISALLOW_COPY_AND_ASSIGN(SessionContext);
};

}  // namespace wvoec_ref

#endif  // REF_OEMCRYPTO_SESSION_H_
