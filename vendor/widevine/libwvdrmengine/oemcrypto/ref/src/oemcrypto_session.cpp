// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
//  Reference implementation of OEMCrypto APIs
//
#include "oemcrypto_session.h"

#include <assert.h>
#include <string.h>
#include <algorithm>
#include <iostream>
#include <vector>

#include <openssl/aes.h>
#include <openssl/bio.h>
#include <openssl/cmac.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <openssl/x509.h>

#include "keys.h"
#include "log.h"
#include "oemcrypto_engine_ref.h"
#include "oemcrypto_key_ref.h"
#include "oemcrypto_rsa_key_shared.h"
#include "oemcrypto_types.h"
#include "platform.h"
#include "disallow_copy_and_assign.h"
#include "string_conversions.h"
#include "wvcrc32.h"

static const int kPssSaltLength = 20;

namespace {

// Increment counter for AES-CTR.  The CENC spec specifies we increment only
// the low 64 bits of the IV counter, and leave the high 64 bits alone.
void ctr128_inc64(uint8_t* counter) {
  uint32_t n = 16;
  do {
    if (++counter[--n] != 0) return;
  } while (n > 8);
}
}  // namespace

namespace wvoec_ref {

/***************************************/

class ContentKeysContext : public SessionContextKeys {
 public:
  explicit ContentKeysContext() {}
  ~ContentKeysContext() override {}
  size_t size() override { return session_keys_.size(); }
  bool Insert(const KeyId& key_id, const Key& key_data) override;
  Key* Find(const KeyId& key_id) override;
  void Remove(const KeyId& key_id) override;
  void UpdateDuration(const KeyControlBlock& control) override;

  OEMCrypto_LicenseType type() override { return OEMCrypto_ContentLicense; }

  bool SetContentKey(const KeyId& entitlement_id,
                     const KeyId& content_key_id,
                     const std::vector<uint8_t>& content_key) override;
  EntitlementKey* GetEntitlementKey(const KeyId& entitlement_id) override;

 private:
  SessionKeyTable session_keys_;
  CORE_DISALLOW_COPY_AND_ASSIGN(ContentKeysContext);
};

bool ContentKeysContext::Insert(const KeyId& key_id, const Key& key_data) {
  return session_keys_.Insert(key_id, key_data);
}

Key* ContentKeysContext::Find(const KeyId& key_id) {
  return session_keys_.Find(key_id);
}

void ContentKeysContext::Remove(const KeyId& key_id) {
  session_keys_.Remove(key_id);
}

void ContentKeysContext::UpdateDuration(const KeyControlBlock& control) {
  session_keys_.UpdateDuration(control);
}

bool ContentKeysContext::SetContentKey(
    const KeyId& entitlement_id, const KeyId& content_key_id,
    const std::vector<uint8_t>& content_key) {
  // Unsupported action for this type.
  return false;
}

EntitlementKey* ContentKeysContext::GetEntitlementKey(
    const KeyId& entitlement_id) {
  // Unsupported action for this type.
  return nullptr;
}

/***************************************/

class EntitlementKeysContext : public SessionContextKeys {
 public:
  EntitlementKeysContext() {}
  ~EntitlementKeysContext() override {}
  size_t size() override { return session_keys_.size(); }
  bool Insert(const KeyId& key_id, const Key& key_data) override;
  Key* Find(const KeyId& key_id) override;
  void Remove(const KeyId& key_id) override;
  void UpdateDuration(const KeyControlBlock& control) override;
  bool SetContentKey(const KeyId& entitlement_id,
                     const KeyId& content_key_id,
                     const std::vector<uint8_t>& content_key) override;
  EntitlementKey* GetEntitlementKey(const KeyId& entitlement_id) override;

  OEMCrypto_LicenseType type() override { return OEMCrypto_EntitlementLicense; }

 private:
  EntitlementKeyTable session_keys_;
  CORE_DISALLOW_COPY_AND_ASSIGN(EntitlementKeysContext);
};

bool EntitlementKeysContext::Insert(const KeyId& key_id, const Key& key_data) {
  return session_keys_.Insert(key_id, key_data);
}

Key* EntitlementKeysContext::Find(const KeyId& key_id) {
  return session_keys_.Find(key_id);
}

void EntitlementKeysContext::Remove(const KeyId& key_id) {
  session_keys_.Remove(key_id);
}

void EntitlementKeysContext::UpdateDuration(const KeyControlBlock& control) {
  session_keys_.UpdateDuration(control);
}

bool EntitlementKeysContext::SetContentKey(
    const KeyId& entitlement_id, const KeyId& content_key_id,
    const std::vector<uint8_t>& content_key) {
  return session_keys_.SetContentKey(entitlement_id, content_key_id,
                                     content_key);
}

EntitlementKey* EntitlementKeysContext::GetEntitlementKey(
    const KeyId& entitlement_id) {
  return session_keys_.GetEntitlementKey(entitlement_id);
}

/***************************************/

SessionContext::~SessionContext() {
  if (usage_entry_) {
    delete usage_entry_;
    usage_entry_ = NULL;
  }
  if (session_keys_) {
    delete session_keys_;
    session_keys_ = NULL;
  }
}

// Internal utility function to derive key using CMAC-128
bool SessionContext::DeriveKey(const std::vector<uint8_t>& key,
                               const std::vector<uint8_t>& context, int counter,
                               std::vector<uint8_t>* out) {
  if (key.empty() || counter > 4 || context.empty() || out == NULL) {
    LOGE("[DeriveKey(): OEMCrypto_ERROR_INVALID_CONTEXT]");
    return false;
  }

  const EVP_CIPHER* cipher = EVP_aes_128_cbc();
  CMAC_CTX* cmac_ctx = CMAC_CTX_new();

  if (!cmac_ctx) {
    LOGE("[DeriveKey(): OEMCrypto_ERROR_CMAC_FAILURE]");
    return false;
  }

  if (!CMAC_Init(cmac_ctx, &key[0], key.size(), cipher, 0)) {
    LOGE("[DeriveKey(): OEMCrypto_ERROR_CMAC_FAILURE]");
    CMAC_CTX_free(cmac_ctx);
    return false;
  }

  std::vector<uint8_t> message;
  message.push_back(counter);
  message.insert(message.end(), context.begin(), context.end());

  if (!CMAC_Update(cmac_ctx, &message[0], message.size())) {
    LOGE("[DeriveKey(): OEMCrypto_ERROR_CMAC_FAILURE]");
    CMAC_CTX_free(cmac_ctx);
    return false;
  }

  size_t reslen;
  uint8_t res[128];
  if (!CMAC_Final(cmac_ctx, res, &reslen)) {
    LOGE("[DeriveKey(): OEMCrypto_ERROR_CMAC_FAILURE]");
    CMAC_CTX_free(cmac_ctx);
    return false;
  }

  out->assign(res, res + reslen);

  CMAC_CTX_free(cmac_ctx);

  return true;
}

bool SessionContext::DeriveKeys(const std::vector<uint8_t>& master_key,
                                const std::vector<uint8_t>& mac_key_context,
                                const std::vector<uint8_t>& enc_key_context) {
  // Generate derived key for mac key
  std::vector<uint8_t> mac_key_server;
  std::vector<uint8_t> mac_key_client;
  std::vector<uint8_t> mac_key_part2;
  if (!DeriveKey(master_key, mac_key_context, 1, &mac_key_server)) {
    return false;
  }
  if (!DeriveKey(master_key, mac_key_context, 2, &mac_key_part2)) {
    return false;
  }
  mac_key_server.insert(mac_key_server.end(), mac_key_part2.begin(),
                        mac_key_part2.end());

  if (!DeriveKey(master_key, mac_key_context, 3, &mac_key_client)) {
    return false;
  }
  if (!DeriveKey(master_key, mac_key_context, 4, &mac_key_part2)) {
    return false;
  }
  mac_key_client.insert(mac_key_client.end(), mac_key_part2.begin(),
                        mac_key_part2.end());

  // Generate derived key for encryption key
  std::vector<uint8_t> enc_key;
  if (!DeriveKey(master_key, enc_key_context, 1, &enc_key)) {
    return false;
  }

  set_mac_key_server(mac_key_server);
  set_mac_key_client(mac_key_client);
  set_encryption_key(enc_key);
  return true;
}

bool SessionContext::RSADeriveKeys(
    const std::vector<uint8_t>& enc_session_key,
    const std::vector<uint8_t>& mac_key_context,
    const std::vector<uint8_t>& enc_key_context) {
  if (!rsa_key()) {
    LOGE("[RSADeriveKeys(): no RSA key set]");
    return false;
  }
  if (enc_session_key.size() != static_cast<size_t>(RSA_size(rsa_key()))) {
    LOGE("[RSADeriveKeys(): encrypted session key wrong size:%zu, expected %d]",
         enc_session_key.size(), RSA_size(rsa_key()));
    dump_boringssl_error();
    return false;
  }
  session_key_.resize(RSA_size(rsa_key()));
  int decrypted_size =
      RSA_private_decrypt(enc_session_key.size(), &enc_session_key[0],
                          &session_key_[0], rsa_key(), RSA_PKCS1_OAEP_PADDING);
  if (-1 == decrypted_size) {
    LOGE("[RSADeriveKeys(): error decrypting session key.]");
    dump_boringssl_error();
    return false;
  }
  session_key_.resize(decrypted_size);
  if (decrypted_size != static_cast<int>(wvoec::KEY_SIZE)) {
    LOGE("[RSADeriveKeys(): error. Session key is wrong size: %d.]",
         decrypted_size);
    dump_boringssl_error();
    session_key_.clear();
    return false;
  }
  return DeriveKeys(session_key_, mac_key_context, enc_key_context);
}

// Utility function to generate a message signature
bool SessionContext::GenerateSignature(const uint8_t* message,
                                       size_t message_length,
                                       uint8_t* signature,
                                       size_t* signature_length) {
  if (message == NULL || message_length == 0 || signature == NULL ||
      signature_length == 0) {
    LOGE("[OEMCrypto_GenerateSignature(): OEMCrypto_ERROR_INVALID_CONTEXT]");
    return false;
  }

  if (mac_key_client_.size() != wvoec::MAC_KEY_SIZE) {
    return false;
  }

  if (*signature_length < SHA256_DIGEST_LENGTH) {
    *signature_length = SHA256_DIGEST_LENGTH;
    return false;
  }

  unsigned int md_len = *signature_length;
  if (HMAC(EVP_sha256(), &mac_key_client_[0], wvoec::MAC_KEY_SIZE, message,
           message_length, signature, &md_len)) {
    *signature_length = md_len;
    return true;
  }
  return false;
}

size_t SessionContext::RSASignatureSize() {
  if (!rsa_key()) {
    LOGE("[GenerateRSASignature(): no RSA key set]");
    return 0;
  }
  return static_cast<size_t>(RSA_size(rsa_key()));
}

OEMCryptoResult SessionContext::GenerateRSASignature(
    const uint8_t* message, size_t message_length, uint8_t* signature,
    size_t* signature_length, RSA_Padding_Scheme padding_scheme) {
  if (message == NULL || message_length == 0 || signature == NULL ||
      signature_length == 0) {
    LOGE("[GenerateRSASignature(): OEMCrypto_ERROR_INVALID_CONTEXT]");
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }
  if (!rsa_key()) {
    LOGE("[GenerateRSASignature(): no RSA key set]");
    return OEMCrypto_ERROR_INVALID_RSA_KEY;
  }
  if (*signature_length < static_cast<size_t>(RSA_size(rsa_key()))) {
    *signature_length = RSA_size(rsa_key());
    return OEMCrypto_ERROR_SHORT_BUFFER;
  }
  if ((padding_scheme & allowed_schemes_) != padding_scheme) {
    LOGE("[GenerateRSASignature(): padding_scheme not allowed]");
    return OEMCrypto_ERROR_INVALID_RSA_KEY;
  }
  // This is the standard padding scheme used for license requests.
  if (padding_scheme == kSign_RSASSA_PSS) {
    // Hash the message using SHA1.
    uint8_t hash[SHA_DIGEST_LENGTH];
    if (!SHA1(message, message_length, hash)) {
      LOGE("[GeneratRSASignature(): error creating signature hash.]");
      dump_boringssl_error();
      return OEMCrypto_ERROR_UNKNOWN_FAILURE;
    }

    // Add PSS padding.
    std::vector<uint8_t> padded_digest(*signature_length);
    int status = RSA_padding_add_PKCS1_PSS_mgf1(
        rsa_key(), &padded_digest[0], hash, EVP_sha1(), NULL, kPssSaltLength);
    if (status == -1) {
      LOGE("[GeneratRSASignature(): error padding hash.]");
      dump_boringssl_error();
      return OEMCrypto_ERROR_UNKNOWN_FAILURE;
    }

    // Encrypt PSS padded digest.
    status = RSA_private_encrypt(*signature_length, &padded_digest[0],
                                 signature, rsa_key(), RSA_NO_PADDING);
    if (status == -1) {
      LOGE("[GeneratRSASignature(): error in private encrypt.]");
      dump_boringssl_error();
      return OEMCrypto_ERROR_UNKNOWN_FAILURE;
    }
    // This is the alternate padding scheme used by cast receivers only.
  } else if (padding_scheme == kSign_PKCS1_Block1) {
    if (message_length > 83) {
      LOGE("[GeneratRSASignature(): RSA digest too large.]");
      return OEMCrypto_ERROR_SIGNATURE_FAILURE;
    }
    // Pad the message with PKCS1 padding, and then encrypt.
    size_t status = RSA_private_encrypt(message_length, message, signature,
                                        rsa_key(), RSA_PKCS1_PADDING);
    if (status != *signature_length) {
      LOGE("[GeneratRSASignature(): error in RSA private encrypt. status=%d]",
           status);
      dump_boringssl_error();
      return OEMCrypto_ERROR_UNKNOWN_FAILURE;
    }
  } else {  // Bad RSA_Padding_Scheme
    return OEMCrypto_ERROR_INVALID_RSA_KEY;
  }
  return OEMCrypto_SUCCESS;
}

// Validate message signature
bool SessionContext::ValidateMessage(const uint8_t* given_message,
                                     size_t message_length,
                                     const uint8_t* given_signature,
                                     size_t signature_length) {
  if (signature_length != SHA256_DIGEST_LENGTH) {
    return false;
  }
  uint8_t computed_signature[SHA256_DIGEST_LENGTH];
  memset(computed_signature, 0, SHA256_DIGEST_LENGTH);
  unsigned int md_len = SHA256_DIGEST_LENGTH;
  if (!HMAC(EVP_sha256(), mac_key_server_.data(), mac_key_server_.size(),
            given_message, message_length, computed_signature, &md_len)) {
    LOGE("ValidateMessage: Could not compute signature.");
    return false;
  }
  if (memcmp(given_signature, computed_signature, signature_length)) {
    LOGE("Invalid signature    given: %s",
         wvcdm::HexEncode(given_signature, signature_length).c_str());
    LOGE("Invalid signature computed: %s",
         wvcdm::HexEncode(computed_signature, signature_length).c_str());
    return false;
  }
  return true;
}

OEMCryptoResult SessionContext::CheckStatusOnline(uint32_t nonce,
                                                  uint32_t control) {
  if (!(control & wvoec::kControlNonceEnabled)) {
    LOGE("LoadKeys: Server provided Nonce_Required but Nonce_Enabled = 0.");
    // Server error. Continue, and assume nonce required.
  }
  if (!CheckNonce(nonce)) return OEMCrypto_ERROR_INVALID_NONCE;
  switch (usage_entry_status_) {
    case kNoUsageEntry:
      LOGE("LoadKeys: Session did not create usage entry.");
      return OEMCrypto_ERROR_INVALID_CONTEXT;
    case kUsageEntryLoaded:
      LOGE("LoadKeys: Session reloaded existing entry.");
      return OEMCrypto_ERROR_INVALID_CONTEXT;
    case kUsageEntryNew:
      return OEMCrypto_SUCCESS;
    default:  // invalid status.
      return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
}

OEMCryptoResult SessionContext::CheckStatusOffline(uint32_t nonce,
                                                   uint32_t control) {
  if (control & wvoec::kControlNonceEnabled) {
    LOGE("KCB: Server provided NonceOrEntry but Nonce_Enabled = 1.");
    // Server error. Continue, and assume nonce required.
  }
  switch (usage_entry_status_) {
    case kNoUsageEntry:
      LOGE("LoadKeys: Session did not create or load usage entry.");
      return OEMCrypto_ERROR_INVALID_CONTEXT;
    case kUsageEntryLoaded:
      // Repeat load.  Calling function will verify pst and keys.
      return OEMCrypto_SUCCESS;
    case kUsageEntryNew:
      // First load.  Verify nonce.
      if (!CheckNonce(nonce)) return OEMCrypto_ERROR_INVALID_NONCE;
      return OEMCrypto_SUCCESS;
    default:  // invalid status.
      return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
}

OEMCryptoResult SessionContext::CheckNonceOrEntry(
    const KeyControlBlock& key_control_block) {
  switch (key_control_block.control_bits() & wvoec::kControlReplayMask) {
    case wvoec::kControlNonceRequired:  // Online license. Nonce always required.
      return CheckStatusOnline(key_control_block.nonce(),
                               key_control_block.control_bits());
      break;
    case wvoec::kControlNonceOrEntry:  // Offline license. Nonce required on first use.
      return CheckStatusOffline(key_control_block.nonce(),
                                key_control_block.control_bits());
      break;
    default:
      if ((key_control_block.control_bits() & wvoec::kControlNonceEnabled) &&
          (!CheckNonce(key_control_block.nonce()))) {
        LOGE("LoadKeys: BAD Nonce");
        return OEMCrypto_ERROR_INVALID_NONCE;
      }
  }
  return OEMCrypto_SUCCESS;
}

void SessionContext::StartTimer() { timer_start_ = ce_->OnlineTime(); }

uint32_t SessionContext::CurrentTimer() {
  time_t now = ce_->OnlineTime();
  return now - timer_start_;
}

OEMCryptoResult SessionContext::LoadKeys(
    const uint8_t* message, size_t message_length, const uint8_t* signature,
    size_t signature_length, OEMCrypto_Substring enc_mac_keys_iv,
    OEMCrypto_Substring enc_mac_keys, size_t num_keys,
    const OEMCrypto_KeyObject* key_array, OEMCrypto_Substring pst,
    OEMCrypto_Substring srm_restriction_data,
    OEMCrypto_LicenseType license_type) {
  // Validate message signature
  if (!ValidateMessage(message, message_length, signature, signature_length)) {
    return OEMCrypto_ERROR_SIGNATURE_FAILURE;
  }

  if (!session_keys_) {
    switch (license_type) {
      case OEMCrypto_ContentLicense:
        session_keys_ = new ContentKeysContext();
        break;

      case OEMCrypto_EntitlementLicense:
        session_keys_ = new EntitlementKeysContext();
        break;

      default:
        return OEMCrypto_ERROR_INVALID_CONTEXT;
    }
  } else {
    if (session_keys_->type() != license_type) {
      return OEMCrypto_ERROR_INVALID_CONTEXT;
    }
  }

  StartTimer();

  if (srm_restriction_data.length != 0) {
    const std::string kSRMVerificationString = "HDCPDATA";
    if (memcmp(message + srm_restriction_data.offset,
               kSRMVerificationString.c_str(), kSRMVerificationString.size())) {
      LOGE("SRM Requirement Data has bad verification string: %8s",
           message + srm_restriction_data.offset);
      return OEMCrypto_ERROR_INVALID_CONTEXT;
    }
    uint32_t minimum_version = htonl(*reinterpret_cast<const uint32_t*>(
        message + srm_restriction_data.offset + 8));
    uint16_t current_version = 0;
    if (OEMCrypto_SUCCESS != ce_->current_srm_version(&current_version)) {
      LOGW("[LoadKeys: SRM Version not available.");
      srm_requirements_status_ = InvalidSRMVersion;
    } else if (current_version < minimum_version) {
      LOGW("[LoadKeys: SRM Version is too small %d, required: %d",
           current_version, minimum_version);
      srm_requirements_status_ = InvalidSRMVersion;
    } else if (ce_->srm_blacklisted_device_attached()) {
      LOGW("[LoadKeys: SRM blacklisted device attached]");
      srm_requirements_status_ = InvalidSRMVersion;
    } else {
      LOGI("[LoadKeys: SRM Versions is %d, required: %d]", current_version,
           minimum_version);
      srm_requirements_status_ = ValidSRMVersion;
    }
  }

  // Decrypt and install keys in key object
  // Each key will have a key control block.  They will all have the same nonce.
  OEMCryptoResult status = OEMCrypto_SUCCESS;
  std::vector<uint8_t> key_id;
  std::vector<uint8_t> enc_key_data;
  std::vector<uint8_t> key_data_iv;
  std::vector<uint8_t> key_control;
  std::vector<uint8_t> key_control_iv;
  for (unsigned int i = 0; i < num_keys; i++) {
    key_id.assign(
        message + key_array[i].key_id.offset,
        message + key_array[i].key_id.offset + key_array[i].key_id.length);
    enc_key_data.assign(
        message + key_array[i].key_data.offset,
        message + key_array[i].key_data.offset + key_array[i].key_data.length);
    key_data_iv.assign(
        message + key_array[i].key_data_iv.offset,
        message + key_array[i].key_data_iv.offset + wvoec::KEY_IV_SIZE);
    if (key_array[i].key_control.length == 0) {
      status = OEMCrypto_ERROR_UNKNOWN_FAILURE;
      break;
    }
    key_control.assign(
        message + key_array[i].key_control.offset,
        message + key_array[i].key_control.offset + wvoec::KEY_CONTROL_SIZE);
    key_control_iv.assign(
        message + key_array[i].key_control_iv.offset,
        message + key_array[i].key_control_iv.offset + wvoec::KEY_IV_SIZE);

    OEMCryptoResult result =
        InstallKey(key_id, enc_key_data, key_data_iv, key_control,
                   key_control_iv);
    if (result != OEMCrypto_SUCCESS) {
      status = result;
      break;
    }
  }
  FlushNonces();
  if (status != OEMCrypto_SUCCESS) return status;

  // enc_mac_key can be NULL if license renewal is not supported
  if (enc_mac_keys.length != 0) {
    // V2.1 license protocol: update mac keys after processing license response
    const std::vector<uint8_t> enc_mac_keys_str = std::vector<uint8_t>(
        message + enc_mac_keys.offset,
        message + enc_mac_keys.offset + 2 * wvoec::MAC_KEY_SIZE);
    const std::vector<uint8_t> enc_mac_key_iv_str = std::vector<uint8_t>(
        message + enc_mac_keys_iv.offset,
        message + enc_mac_keys_iv.offset + wvoec::KEY_IV_SIZE);

    if (!UpdateMacKeys(enc_mac_keys_str, enc_mac_key_iv_str)) {
      LOGE("Failed to update mac keys.\n");
      return OEMCrypto_ERROR_UNKNOWN_FAILURE;
    }
  }
  if (usage_entry_) {
    OEMCryptoResult result = OEMCrypto_SUCCESS;
    switch (usage_entry_status_) {
      case kNoUsageEntry:
        if (pst.length > 0) {
          LOGE("LoadKeys: PST specified but no usage entry loaded.");
          return OEMCrypto_ERROR_INVALID_CONTEXT;
        }
        break;  // no extra check.
      case kUsageEntryNew:
        result = usage_entry_->SetPST(message + pst.offset, pst.length);
        if (result != OEMCrypto_SUCCESS) {
          return result;
        }
        if (!usage_entry_->SetMacKeys(mac_key_server_, mac_key_client_)) {
          LOGE("LoadKeys: Usage table can't set keys.\n");
          return OEMCrypto_ERROR_UNKNOWN_FAILURE;
        }
        break;
      case kUsageEntryLoaded:
        if (!usage_entry_->VerifyPST(message + pst.offset, pst.length)) {
          return OEMCrypto_ERROR_WRONG_PST;
        }
        if (!usage_entry_->VerifyMacKeys(mac_key_server_, mac_key_client_)) {
          LOGE("LoadKeys: Usage table entry mac keys do not match.\n");
          return OEMCrypto_ERROR_WRONG_KEYS;
        }
        if (usage_entry_->Inactive()) return OEMCrypto_ERROR_LICENSE_INACTIVE;
        break;
    }
  }
  encryption_key_.clear();
  return OEMCrypto_SUCCESS;
}

OEMCryptoResult SessionContext::LoadEntitledContentKeys(
    const uint8_t* message, size_t message_length, size_t num_keys,
    const OEMCrypto_EntitledContentKeyObject* key_array) {
  if (!key_array) {
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (!session_keys_ || session_keys_->type() != OEMCrypto_EntitlementLicense) {
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }
  for (size_t i = 0; i < num_keys; ++i) {
    const OEMCrypto_EntitledContentKeyObject* key_data = &key_array[i];
    std::vector<uint8_t> entitlement_key_id;
    entitlement_key_id.assign(message + key_data->entitlement_key_id.offset,
                              message + key_data->entitlement_key_id.offset +
                                  key_data->entitlement_key_id.length);

    EntitlementKey* entitlement_key =
        session_keys_->GetEntitlementKey(entitlement_key_id);
    if (entitlement_key == nullptr) {
      return OEMCrypto_KEY_NOT_ENTITLED;
    }
    std::vector<uint8_t> content_key;
    std::vector<uint8_t> iv;
    std::vector<uint8_t> encrypted_content_key;
    std::vector<uint8_t> content_key_id;

    iv.assign(message + key_data->content_key_data_iv.offset,
              message + key_data->content_key_data_iv.offset + 16);
    encrypted_content_key.assign(message + key_data->content_key_data.offset,
                                 message + key_data->content_key_data.offset +
                                     key_data->content_key_data.length);
    content_key_id.assign(message + key_data->content_key_id.offset,
                          message + key_data->content_key_id.offset +
                              key_data->content_key_id.length);
    if (!DecryptMessage(entitlement_key->entitlement_key(), iv,
                        encrypted_content_key, &content_key,
                        256 /* key size */)) {
      return OEMCrypto_ERROR_UNKNOWN_FAILURE;
    }
    if (!session_keys_->SetContentKey(entitlement_key_id, content_key_id,
                                      content_key)) {
      return OEMCrypto_ERROR_UNKNOWN_FAILURE;
    }
  }
  return OEMCrypto_SUCCESS;
}

OEMCryptoResult SessionContext::InstallKey(
    const KeyId& key_id, const std::vector<uint8_t>& key_data,
    const std::vector<uint8_t>& key_data_iv,
    const std::vector<uint8_t>& key_control,
    const std::vector<uint8_t>& key_control_iv) {
  // Decrypt encrypted key_data using derived encryption key and offered iv
  std::vector<uint8_t> content_key;
  std::vector<uint8_t> key_control_str;

  if (!DecryptMessage(encryption_key_, key_data_iv, key_data, &content_key,
                      128 /* key size */)) {
    LOGE("[Installkey(): Could not decrypt key data]");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }

  // Key control must be supplied by license server
  if (key_control.empty()) {
    LOGE("[Installkey(): WARNING: No Key Control]");
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }
  if (key_control_iv.empty()) {
    LOGE("[Installkey(): ERROR: No Key Control IV]");
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }
  if (!DecryptMessage(content_key, key_control_iv, key_control,
                      &key_control_str, 128 /* key size */)) {
    LOGE("[Installkey(): ERROR: Could not decrypt content key]");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }

  KeyControlBlock key_control_block(key_control_str);
  if (!key_control_block.valid()) {
    LOGE("Error parsing key control.");
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }
  if ((key_control_block.control_bits() &
       wvoec::kControlRequireAntiRollbackHardware) &&
      !ce_->config_is_anti_rollback_hw_present()) {
    LOGE("Anti-rollback hardware is required but hardware not present.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  uint8_t minimum_patch_level =
      (key_control_block.control_bits() & wvoec::kControlSecurityPatchLevelMask) >>
      wvoec::kControlSecurityPatchLevelShift;
  if (minimum_patch_level > OEMCrypto_Security_Patch_Level()) {
    LOGE("[InstallKey(): security patch level: %d.  Minimum:%d]",
         OEMCrypto_Security_Patch_Level(), minimum_patch_level);
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  OEMCryptoResult result = CheckNonceOrEntry(key_control_block);
  if (result != OEMCrypto_SUCCESS) {
    LOGE("LoadKeys: Failed Nonce/PST check.");
    return result;
  }
  if (key_control_block.control_bits() & wvoec::kControlSRMVersionRequired) {
    if (srm_requirements_status_ == NoSRMVersion) {
      LOGE("[LoadKeys: control bit says SRM version required]");
      return OEMCrypto_ERROR_INVALID_CONTEXT;
    }
    if (srm_requirements_status_ == InvalidSRMVersion) {
      // If the SRM version is too small, treat this key as local display only.
      key_control_block.RequireLocalDisplay();
    }
  }

  Key key(content_key, key_control_block);
  if (!session_keys_) {
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }
  session_keys_->Insert(key_id, key);
  return OEMCrypto_SUCCESS;
}

bool SessionContext::InstallRSAEncryptedKey(
    const uint8_t* encrypted_message_key, size_t encrypted_message_key_length) {
  encryption_key_.resize(RSA_size(rsa_key()));
  int decrypted_size = RSA_private_decrypt(
      encrypted_message_key_length, encrypted_message_key, &encryption_key_[0],
      rsa_key(), RSA_PKCS1_OAEP_PADDING);
  if (-1 == decrypted_size) {
    LOGE("[RSADeriveKeys(): error decrypting session key.]");
    dump_boringssl_error();
    return false;
  }
  encryption_key_.resize(decrypted_size);
  if (decrypted_size != static_cast<int>(wvoec::KEY_SIZE)) {
    LOGE("[RSADeriveKeys(): error. Session key is wrong size: %d.]",
         decrypted_size);
    dump_boringssl_error();
    encryption_key_.clear();
    return false;
  }
  return true;
}

OEMCryptoResult SessionContext::RefreshKey(
    const KeyId& key_id, const std::vector<uint8_t>& key_control,
    const std::vector<uint8_t>& key_control_iv) {
  if (!session_keys_) {
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }
  if (key_id.empty()) {
    // Key control is not encrypted if key id is NULL
    KeyControlBlock key_control_block(key_control);
    if (!key_control_block.valid()) {
      LOGE("Parse key control error.");
      return OEMCrypto_ERROR_INVALID_CONTEXT;
    }
    if ((key_control_block.control_bits() & wvoec::kControlNonceEnabled) &&
        (!CheckNonce(key_control_block.nonce()))) {
      LOGE("KCB: BAD Nonce");
      return OEMCrypto_ERROR_INVALID_NONCE;
    }
    // Apply duration to all keys in this session
    session_keys_->UpdateDuration(key_control_block);
    return OEMCrypto_SUCCESS;
  }

  Key* content_key = session_keys_->Find(key_id);

  if (NULL == content_key) {
    LOGE("Key ID not found.");
    return OEMCrypto_ERROR_NO_CONTENT_KEY;
  }

  if (key_control.empty()) {
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }

  const std::vector<uint8_t> content_key_value = content_key->value();

  // Decrypt encrypted key control block
  std::vector<uint8_t> control;
  if (key_control_iv.empty()) {
    control = key_control;
  } else {
    if (!DecryptMessage(content_key_value, key_control_iv, key_control,
                        &control, 128 /* key size */)) {
      return OEMCrypto_ERROR_UNKNOWN_FAILURE;
    }
  }

  KeyControlBlock key_control_block(control);
  if (!key_control_block.valid()) {
    LOGE("Error parsing key control.");
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }
  if ((key_control_block.control_bits() & wvoec::kControlNonceEnabled) &&
      (!CheckNonce(key_control_block.nonce()))) {
    return OEMCrypto_ERROR_INVALID_NONCE;
  }
  content_key->UpdateDuration(key_control_block);
  return OEMCrypto_SUCCESS;
}

bool SessionContext::DecryptRSAKey(const uint8_t* enc_rsa_key,
                                   size_t enc_rsa_key_length,
                                   const uint8_t* enc_rsa_key_iv,
                                   uint8_t* pkcs8_rsa_key) {
  // Decrypt rsa key with keybox.
  uint8_t iv_buffer[wvoec::KEY_IV_SIZE];
  memcpy(iv_buffer, enc_rsa_key_iv, wvoec::KEY_IV_SIZE);
  AES_KEY aes_key;
  AES_set_decrypt_key(&encryption_key_[0], 128, &aes_key);
  AES_cbc_encrypt(enc_rsa_key, pkcs8_rsa_key, enc_rsa_key_length, &aes_key,
                  iv_buffer, AES_DECRYPT);
  return true;
}

bool SessionContext::EncryptRSAKey(const uint8_t* pkcs8_rsa_key,
                                   size_t enc_rsa_key_length,
                                   const uint8_t* enc_rsa_key_iv,
                                   uint8_t* enc_rsa_key) {
  // Encrypt rsa key with keybox.
  uint8_t iv_buffer[wvoec::KEY_IV_SIZE];
  memcpy(iv_buffer, enc_rsa_key_iv, wvoec::KEY_IV_SIZE);
  AES_KEY aes_key;
  AES_set_encrypt_key(&encryption_key_[0], 128, &aes_key);
  AES_cbc_encrypt(pkcs8_rsa_key, enc_rsa_key, enc_rsa_key_length, &aes_key,
                  iv_buffer, AES_ENCRYPT);
  return true;
}

bool SessionContext::LoadRSAKey(const uint8_t* pkcs8_rsa_key,
                                size_t rsa_key_length) {
  rsa_key_.reset();
  if (rsa_key_length < 8) {
    LOGE("[LoadRSAKey(): Very Short Buffer]");
    return false;
  }
  if ((memcmp(pkcs8_rsa_key, "SIGN", 4) == 0)) {
    uint32_t schemes_n;
    memcpy((uint8_t*)&schemes_n, pkcs8_rsa_key + 4, sizeof(uint32_t));
    allowed_schemes_ = htonl(schemes_n);
    pkcs8_rsa_key += 8;
    rsa_key_length -= 8;
  } else {
    allowed_schemes_ = kSign_RSASSA_PSS;
  }
  return rsa_key_.LoadPkcs8RsaKey(pkcs8_rsa_key, rsa_key_length);
}

OEMCryptoResult SessionContext::CheckKeyUse(const std::string& log_string,
                                            uint32_t use_type,
                                            OEMCryptoBufferType buffer_type) {
  const KeyControlBlock& control = current_content_key()->control();
  if (use_type && (!(control.control_bits() & use_type))) {
    LOGE("[%s(): control bit says not allowed.", log_string.c_str());
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (control.control_bits() & wvoec::kControlDataPathSecure) {
    if (!ce_->config_closed_platform() &&
        buffer_type == OEMCrypto_BufferType_Clear) {
      LOGE("[%s(): Secure key with insecure buffer]", log_string.c_str());
      return OEMCrypto_ERROR_DECRYPT_FAILED;
    }
  }
  if (control.control_bits() & wvoec::kControlReplayMask) {
    if (!CheckUsageEntry()) {
      LOGE("[%s(): usage entry not valid]", log_string.c_str());
      return OEMCrypto_ERROR_UNKNOWN_FAILURE;
    }
  }
  if (control.duration() > 0) {
    if (control.duration() < CurrentTimer()) {
      LOGE("[%s(): key expired.", log_string.c_str());
      return OEMCrypto_ERROR_KEY_EXPIRED;
    }
  }
  if (!ce_->config_local_display_only()) {
    // Only look at HDCP restrictions if the display can be non-local.
    if (control.control_bits() & wvoec::kControlHDCPRequired) {
      uint8_t required_hdcp =
          (control.control_bits() & wvoec::kControlHDCPVersionMask) >>
          wvoec::kControlHDCPVersionShift;
      if (ce_->srm_blacklisted_device_attached()) {
        required_hdcp = HDCP_NO_DIGITAL_OUTPUT;
      }
      // For reference implementation, we pretend we can handle the current
      // HDCP version.
      if (required_hdcp > ce_->config_current_hdcp_capability() ||
          ce_->config_current_hdcp_capability() == 0) {
        return OEMCrypto_ERROR_INSUFFICIENT_HDCP;
      }
    }
  }
  // If the output buffer is clear, then we cannot control whether the output is
  // an active analog display.  In that case, return an error if analog displays
  // should be disabled.
  if ((control.control_bits() & wvoec::kControlDisableAnalogOutput) &&
      (ce_->analog_display_active() ||
       (buffer_type == OEMCrypto_BufferType_Clear))) {
    LOGE("[%s(): control bit says disable analog.", log_string.c_str());
    return OEMCrypto_ERROR_ANALOG_OUTPUT;
  }
  // Check if CGMS is required.
  if (control.control_bits() & wvoec::kControlCGMSMask) {
    // We can't control CGMS for a clear buffer.
    if (buffer_type == OEMCrypto_BufferType_Clear) {
      LOGE("[%s(): CGMS required, but buffer is clear.", log_string.c_str());
      return OEMCrypto_ERROR_ANALOG_OUTPUT;
    }
    if ( ce_->analog_display_active() && !ce_->cgms_a_active()) {
      LOGE("[%s(): control bit says CGMS required.", log_string.c_str());
      return OEMCrypto_ERROR_ANALOG_OUTPUT;
    }
  }
  return OEMCrypto_SUCCESS;
}

OEMCryptoResult SessionContext::Generic_Encrypt(const uint8_t* in_buffer,
                                                size_t buffer_length,
                                                const uint8_t* iv,
                                                OEMCrypto_Algorithm algorithm,
                                                uint8_t* out_buffer) {
  // Check there is a content key
  if (current_content_key() == NULL) {
    LOGE("[Generic_Encrypt(): OEMCrypto_ERROR_NO_CONTENT_KEY]");
    return OEMCrypto_ERROR_NO_CONTENT_KEY;
  }
  const std::vector<uint8_t>& key = current_content_key()->value();
  // Set the AES key.
  if (static_cast<int>(key.size()) != AES_BLOCK_SIZE) {
    LOGE("[Generic_Encrypt(): CONTENT_KEY has wrong size: %d", key.size());
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  OEMCryptoResult result = CheckKeyUse("Generic_Encrypt", wvoec::kControlAllowEncrypt,
                                       OEMCrypto_BufferType_Clear);
  if (result != OEMCrypto_SUCCESS) return result;
  if (algorithm != OEMCrypto_AES_CBC_128_NO_PADDING) {
    LOGE("[Generic_Encrypt(): algorithm bad.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (buffer_length % AES_BLOCK_SIZE != 0) {
    LOGE("[Generic_Encrypt(): buffers size bad.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  const uint8_t* key_u8 = &key[0];
  AES_KEY aes_key;
  if (AES_set_encrypt_key(key_u8, AES_BLOCK_SIZE * 8, &aes_key) != 0) {
    LOGE("[Generic_Encrypt(): FAILURE]");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  uint8_t iv_buffer[wvoec::KEY_IV_SIZE];
  memcpy(iv_buffer, iv, wvoec::KEY_IV_SIZE);
  AES_cbc_encrypt(in_buffer, out_buffer, buffer_length, &aes_key, iv_buffer,
                  AES_ENCRYPT);
  return OEMCrypto_SUCCESS;
}

OEMCryptoResult SessionContext::Generic_Decrypt(const uint8_t* in_buffer,
                                                size_t buffer_length,
                                                const uint8_t* iv,
                                                OEMCrypto_Algorithm algorithm,
                                                uint8_t* out_buffer) {
  // Check there is a content key
  if (current_content_key() == NULL) {
    LOGE("[Generic_Decrypt(): OEMCrypto_ERROR_NO_CONTENT_KEY]");
    return OEMCrypto_ERROR_NO_CONTENT_KEY;
  }
  const std::vector<uint8_t>& key = current_content_key()->value();
  // Set the AES key.
  if (static_cast<int>(key.size()) != AES_BLOCK_SIZE) {
    LOGE("[Generic_Decrypt(): CONTENT_KEY has wrong size.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  OEMCryptoResult result = CheckKeyUse("Generic_Decrypt", wvoec::kControlAllowDecrypt,
                                       OEMCrypto_BufferType_Clear);
  if (result != OEMCrypto_SUCCESS) return result;

  if (algorithm != OEMCrypto_AES_CBC_128_NO_PADDING) {
    LOGE("[Generic_Decrypt(): bad algorithm.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (buffer_length % AES_BLOCK_SIZE != 0) {
    LOGE("[Generic_Decrypt(): bad buffer size.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  const uint8_t* key_u8 = &key[0];
  AES_KEY aes_key;
  if (AES_set_decrypt_key(key_u8, AES_BLOCK_SIZE * 8, &aes_key) != 0) {
    LOGE("[Generic_Decrypt(): FAILURE]");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  uint8_t iv_buffer[wvoec::KEY_IV_SIZE];
  memcpy(iv_buffer, iv, wvoec::KEY_IV_SIZE);
  AES_cbc_encrypt(in_buffer, out_buffer, buffer_length, &aes_key, iv_buffer,
                  AES_DECRYPT);
  return OEMCrypto_SUCCESS;
}

OEMCryptoResult SessionContext::Generic_Sign(const uint8_t* in_buffer,
                                             size_t buffer_length,
                                             OEMCrypto_Algorithm algorithm,
                                             uint8_t* signature,
                                             size_t* signature_length) {
  // Check there is a content key
  if (current_content_key() == NULL) {
    LOGE("[Generic_Sign(): OEMCrypto_ERROR_NO_CONTENT_KEY]");
    return OEMCrypto_ERROR_NO_CONTENT_KEY;
  }
  if (*signature_length < SHA256_DIGEST_LENGTH) {
    *signature_length = SHA256_DIGEST_LENGTH;
    LOGE("[Generic_Sign(): bad signature length.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  const std::vector<uint8_t>& key = current_content_key()->value();
  if (static_cast<int>(key.size()) != SHA256_DIGEST_LENGTH) {
    LOGE("[Generic_Sign(): CONTENT_KEY has wrong size; %d", key.size());
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  OEMCryptoResult result = CheckKeyUse("Generic_Sign", wvoec::kControlAllowSign,
                                       OEMCrypto_BufferType_Clear);
  if (result != OEMCrypto_SUCCESS) return result;
  if (algorithm != OEMCrypto_HMAC_SHA256) {
    LOGE("[Generic_Sign(): bad algorithm.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  unsigned int md_len = *signature_length;
  if (HMAC(EVP_sha256(), &key[0], key.size(), in_buffer, buffer_length,
           signature, &md_len)) {
    *signature_length = md_len;
    return OEMCrypto_SUCCESS;
  }
  LOGE("[Generic_Sign(): hmac failed.");
  dump_boringssl_error();
  return OEMCrypto_ERROR_UNKNOWN_FAILURE;
}

OEMCryptoResult SessionContext::Generic_Verify(const uint8_t* in_buffer,
                                               size_t buffer_length,
                                               OEMCrypto_Algorithm algorithm,
                                               const uint8_t* signature,
                                               size_t signature_length) {
  // Check there is a content key
  if (current_content_key() == NULL) {
    LOGE("[Decrypt_Verify(): OEMCrypto_ERROR_NO_CONTENT_KEY]");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (signature_length < SHA256_DIGEST_LENGTH) {
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  const std::vector<uint8_t>& key = current_content_key()->value();
  if (static_cast<int>(key.size()) != SHA256_DIGEST_LENGTH) {
    LOGE("[Generic_Verify(): CONTENT_KEY has wrong size: %d", key.size());
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  OEMCryptoResult result = CheckKeyUse("Generic_Verify", wvoec::kControlAllowVerify,
                                       OEMCrypto_BufferType_Clear);
  if (result != OEMCrypto_SUCCESS) return result;
  if (algorithm != OEMCrypto_HMAC_SHA256) {
    LOGE("[Generic_Verify(): bad algorithm.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  unsigned int md_len = signature_length;
  uint8_t computed_signature[SHA256_DIGEST_LENGTH];
  if (HMAC(EVP_sha256(), &key[0], key.size(), in_buffer, buffer_length,
           computed_signature, &md_len)) {
    if (0 == memcmp(signature, computed_signature, SHA256_DIGEST_LENGTH)) {
      return OEMCrypto_SUCCESS;
    } else {
      return OEMCrypto_ERROR_SIGNATURE_FAILURE;
    }
  }
  LOGE("[Generic_Verify(): HMAC failed.");
  dump_boringssl_error();
  return OEMCrypto_ERROR_UNKNOWN_FAILURE;
}

bool SessionContext::UpdateMacKeys(const std::vector<uint8_t>& enc_mac_keys,
                                   const std::vector<uint8_t>& iv) {
  // Decrypt mac key from enc_mac_key using device_keya
  std::vector<uint8_t> mac_keys;
  if (!DecryptMessage(encryption_key_, iv, enc_mac_keys, &mac_keys,
                      128 /* key size */)) {
    return false;
  }
  mac_key_server_ = std::vector<uint8_t>(
      mac_keys.begin(), mac_keys.begin() + wvoec::MAC_KEY_SIZE);
  mac_key_client_ = std::vector<uint8_t>(mac_keys.begin() + wvoec::MAC_KEY_SIZE,
                                         mac_keys.end());
  return true;
}

bool SessionContext::QueryKeyControlBlock(const KeyId& key_id, uint32_t* data) {
  if (!session_keys_) {
    return false;
  }
  const Key* content_key = session_keys_->Find(key_id);
  if (NULL == content_key) {
    LOGE("[QueryKeyControlBlock(): No key matches key id]");
    return false;
  }
  data[0] = 0;  // verification optional.
  data[1] = htonl(content_key->control().duration());
  data[2] = 0;  // nonce optional.
  data[3] = htonl(content_key->control().control_bits());
  return true;
}

OEMCryptoResult SessionContext::SelectContentKey(
    const KeyId& key_id, OEMCryptoCipherMode cipher_mode) {
  if (!session_keys_) {
    LOGE("Select Key: no session keys.");
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }
  Key* content_key = session_keys_->Find(key_id);
  if (NULL == content_key) {
    LOGE("No key matches key id");
    return OEMCrypto_ERROR_NO_CONTENT_KEY;
  }
  content_key->set_ctr_mode(cipher_mode == OEMCrypto_CipherMode_CTR);
  current_content_key_ = content_key;
  const KeyControlBlock& control = current_content_key()->control();

  if (control.duration() > 0) {
    if (control.duration() < CurrentTimer()) {
      LOGE("[SelectContentKey(): KEY_EXPIRED %d versus %d]", control.duration(),
           CurrentTimer());
      return OEMCrypto_ERROR_KEY_EXPIRED;
    }
  }
  return OEMCrypto_SUCCESS;
}

void SessionContext::AddNonce(uint32_t nonce) { nonce_table_.AddNonce(nonce); }

bool SessionContext::CheckNonce(uint32_t nonce) {
  return nonce_table_.CheckNonce(nonce);
}

void SessionContext::FlushNonces() { nonce_table_.Flush(); }

bool SessionContext::CheckUsageEntry() {
  if (!usage_entry_) return false;
  return usage_entry_->CheckForUse();
}

OEMCryptoResult SessionContext::CreateNewUsageEntry(
    uint32_t* usage_entry_number) {
  OEMCryptoResult result = ce_->usage_table().CreateNewUsageEntry(
      this, &usage_entry_, usage_entry_number);
  if (usage_entry_) {
    usage_entry_status_ = kUsageEntryNew;
  }
  return result;
}

OEMCryptoResult SessionContext::LoadUsageEntry(
    uint32_t index, const std::vector<uint8_t>& buffer) {
  OEMCryptoResult result =
      ce_->usage_table().LoadUsageEntry(this, &usage_entry_, index, buffer);
  if (usage_entry_) {
    usage_entry_status_ = kUsageEntryLoaded;
    // Copy the mac keys to the current session.
    mac_key_server_ = std::vector<uint8_t>(
        usage_entry_->mac_key_server(),
        usage_entry_->mac_key_server() + wvoec::MAC_KEY_SIZE);
    mac_key_client_ = std::vector<uint8_t>(
        usage_entry_->mac_key_client(),
        usage_entry_->mac_key_client() + wvoec::MAC_KEY_SIZE);
  }
  return result;
}

OEMCryptoResult SessionContext::UpdateUsageEntry(uint8_t* header_buffer,
                                                 size_t* header_buffer_length,
                                                 uint8_t* entry_buffer,
                                                 size_t* entry_buffer_length) {
  if (!usage_entry_) {
    LOGE("UpdateUsageEntry: Session has no entry.");
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }
  return ce_->usage_table().UpdateUsageEntry(this, usage_entry_, header_buffer,
                                             header_buffer_length, entry_buffer,
                                             entry_buffer_length);
}

OEMCryptoResult SessionContext::DeactivateUsageEntry(
    const std::vector<uint8_t>& pst) {
  if (!usage_entry_) return OEMCrypto_ERROR_INVALID_CONTEXT;
  usage_entry_->Deactivate(pst);
  return OEMCrypto_SUCCESS;
}

OEMCryptoResult SessionContext::ReportUsage(const std::vector<uint8_t>& pst,
                                            uint8_t* buffer,
                                            size_t* buffer_length) {
  if (!usage_entry_) return OEMCrypto_ERROR_INVALID_CONTEXT;
  return usage_entry_->ReportUsage(pst, buffer, buffer_length);
}

OEMCryptoResult SessionContext::MoveEntry(uint32_t new_index) {
  if (!usage_entry_) return OEMCrypto_ERROR_INVALID_CONTEXT;
  return ce_->usage_table().MoveEntry(usage_entry_, new_index);
}

OEMCryptoResult SessionContext::CopyOldUsageEntry(
    const std::vector<uint8_t>& pst) {
  if (!usage_entry_) return OEMCrypto_ERROR_INVALID_CONTEXT;
  return usage_entry_->CopyOldUsageEntry(pst);
}

// Internal utility function to decrypt the message
bool SessionContext::DecryptMessage(const std::vector<uint8_t>& key,
                                    const std::vector<uint8_t>& iv,
                                    const std::vector<uint8_t>& message,
                                    std::vector<uint8_t>* decrypted,
                                    uint32_t key_size) {
  if (key.empty() || iv.empty() || message.empty() || !decrypted) {
    LOGE("[DecryptMessage(): OEMCrypto_ERROR_INVALID_CONTEXT]");
    return false;
  }
  decrypted->resize(message.size());
  uint8_t iv_buffer[16];
  memcpy(iv_buffer, &iv[0], 16);
  AES_KEY aes_key;
  AES_set_decrypt_key(&key[0], key_size, &aes_key);
  AES_cbc_encrypt(&message[0], &(decrypted->front()), message.size(), &aes_key,
                  iv_buffer, AES_DECRYPT);
  return true;
}

OEMCryptoResult SessionContext::DecryptCENC(
    const uint8_t* iv, size_t block_offset,
    const OEMCrypto_CENCEncryptPatternDesc* pattern, const uint8_t* cipher_data,
    size_t cipher_data_length, bool is_encrypted, uint8_t* clear_data,
    OEMCryptoBufferType buffer_type, uint8_t subsample_flags) {
  OEMCryptoResult result =
      ChooseDecrypt(iv, block_offset, pattern, cipher_data, cipher_data_length,
                    is_encrypted, clear_data, buffer_type);
  if (compute_hash_) {
    if (current_content_key() == NULL ||
        (current_content_key()->control().control_bits() &
         wvoec::kControlAllowHashVerification) == 0) {
      LOGE("[DecryptCENC(): OEMCrypto_ERROR_UNKNOWN_FAILURE]");
      hash_error_ = OEMCrypto_ERROR_UNKNOWN_FAILURE;
      compute_hash_ = false;
      current_hash_ = 0;
      current_frame_number_ = 0;
    } else {
      if (OEMCrypto_FirstSubsample & subsample_flags) {
        current_hash_ = wvcrc32Init();
      }
      current_hash_ =
          wvcrc32Cont(clear_data, cipher_data_length, current_hash_);
      if (OEMCrypto_LastSubsample & subsample_flags) {
        if (current_hash_ != given_hash_) {
          LOGE("CRC for frame %d is %08x, should be %08x\n",
               current_frame_number_, current_hash_, given_hash_);
          // Update bad_frame_number_ only if this is the first bad frame.
          if (hash_error_ == OEMCrypto_SUCCESS) {
            bad_frame_number_ = current_frame_number_;
            hash_error_ = OEMCrypto_ERROR_BAD_HASH;
          }
        }
        compute_hash_ = false;
      }
    }
  }
  return result;
}

OEMCryptoResult SessionContext::ChooseDecrypt(
    const uint8_t* iv, size_t block_offset,
    const OEMCrypto_CENCEncryptPatternDesc* pattern, const uint8_t* cipher_data,
    size_t cipher_data_length, bool is_encrypted, uint8_t* clear_data,
    OEMCryptoBufferType buffer_type) {
  // If the data is clear, we do not need a current key selected.
  if (!is_encrypted) {
    if (buffer_type != OEMCrypto_BufferType_Direct) {
      memmove(reinterpret_cast<uint8_t*>(clear_data), cipher_data,
              cipher_data_length);
      return OEMCrypto_SUCCESS;
    }
    // For reference implementation, we quietly drop the clear direct video.
    return OEMCrypto_SUCCESS;
  }

  // Check there is a content key
  if (current_content_key() == NULL) {
    LOGE("[DecryptCTR(): OEMCrypto_ERROR_NO_CONTENT_KEY]");
    return OEMCrypto_ERROR_DECRYPT_FAILED;
  }

  OEMCryptoResult result = CheckKeyUse("DecryptCENC", 0, buffer_type);
  if (result != OEMCrypto_SUCCESS) return result;

  const std::vector<uint8_t>& content_key = current_content_key()->value();

  // Set the AES key.
  if (static_cast<int>(content_key.size()) != AES_BLOCK_SIZE) {
    LOGE("[DecryptCTR(): CONTENT_KEY has wrong size: %d", content_key.size());
    return OEMCrypto_ERROR_DECRYPT_FAILED;
  }
  const uint8_t* key_u8 = &content_key[0];

  if (buffer_type == OEMCrypto_BufferType_Direct) {
    // For reference implementation, we quietly drop the decrypted direct video.
    return OEMCrypto_SUCCESS;
  }

  if (!current_content_key()->ctr_mode()) {
    if (block_offset > 0) return OEMCrypto_ERROR_INVALID_CONTEXT;
    return DecryptCBC(key_u8, iv, pattern, cipher_data, cipher_data_length,
                      clear_data);
  }
  if (pattern->skip > 0) {
    return PatternDecryptCTR(key_u8, iv, block_offset, pattern, cipher_data,
                             cipher_data_length, clear_data);
  }
  return DecryptCTR(key_u8, iv, block_offset, cipher_data, cipher_data_length,
                    clear_data);
}

OEMCryptoResult SessionContext::DecryptCBC(
    const uint8_t* key, const uint8_t* initial_iv,
    const OEMCrypto_CENCEncryptPatternDesc* pattern, const uint8_t* cipher_data,
    size_t cipher_data_length, uint8_t* clear_data) {
  AES_KEY aes_key;
  AES_set_decrypt_key(&key[0], AES_BLOCK_SIZE * 8, &aes_key);
  uint8_t iv[AES_BLOCK_SIZE];
  uint8_t next_iv[AES_BLOCK_SIZE];
  memcpy(iv, &initial_iv[0], AES_BLOCK_SIZE);

  size_t l = 0;
  size_t pattern_offset = pattern->offset;
  while (l < cipher_data_length) {
    size_t size =
        std::min(cipher_data_length - l, static_cast<size_t>(AES_BLOCK_SIZE));
    size_t pattern_length = pattern->encrypt + pattern->skip;
    bool skip_block =
        (pattern_offset >= pattern->encrypt) && (pattern_length > 0);
    if (pattern_length > 0) {
      pattern_offset = (pattern_offset + 1) % pattern_length;
    }
    if (skip_block || (size < AES_BLOCK_SIZE)) {
      memmove(&clear_data[l], &cipher_data[l], size);
    } else {
      uint8_t aes_output[AES_BLOCK_SIZE];
      // Save the iv for the next block, in case cipher_data is in the same
      // buffer as clear_data.
      memcpy(next_iv, &cipher_data[l], AES_BLOCK_SIZE);
      AES_decrypt(&cipher_data[l], aes_output, &aes_key);
      for (size_t n = 0; n < AES_BLOCK_SIZE; n++) {
        clear_data[l + n] = aes_output[n] ^ iv[n];
      }
      memcpy(iv, next_iv, AES_BLOCK_SIZE);
    }
    l += size;
  }
  return OEMCrypto_SUCCESS;
}

OEMCryptoResult SessionContext::PatternDecryptCTR(
    const uint8_t* key, const uint8_t* initial_iv, size_t block_offset,
    const OEMCrypto_CENCEncryptPatternDesc* pattern, const uint8_t* cipher_data,
    size_t cipher_data_length, uint8_t* clear_data) {
  AES_KEY aes_key;
  AES_set_encrypt_key(&key[0], AES_BLOCK_SIZE * 8, &aes_key);
  uint8_t iv[AES_BLOCK_SIZE];
  memcpy(iv, &initial_iv[0], AES_BLOCK_SIZE);

  size_t l = 0;
  size_t pattern_offset = pattern->offset;
  while (l < cipher_data_length) {
    size_t size =
        std::min(cipher_data_length - l, AES_BLOCK_SIZE - block_offset);
    size_t pattern_length = pattern->encrypt + pattern->skip;
    bool skip_block =
        (pattern_offset >= pattern->encrypt) && (pattern_length > 0);
    if (pattern_length > 0) {
      pattern_offset = (pattern_offset + 1) % pattern_length;
    }
    if (skip_block) {
      memmove(&clear_data[l], &cipher_data[l], size);
    } else {
      uint8_t aes_output[AES_BLOCK_SIZE];
      AES_encrypt(iv, aes_output, &aes_key);
      for (size_t n = 0; n < size; n++) {
        clear_data[l + n] = aes_output[n + block_offset] ^ cipher_data[l + n];
      }
      ctr128_inc64(iv);
    }
    l += size;
    block_offset = 0;
  }
  return OEMCrypto_SUCCESS;
}

// This is a special case of PatternDecryptCTR with no skip pattern. It uses
// more optimized versions of openssl's implementation of AES CTR mode.
OEMCryptoResult SessionContext::DecryptCTR(const uint8_t* key_u8,
                                           const uint8_t* iv,
                                           size_t block_offset,
                                           const uint8_t* cipher_data,
                                           size_t cipher_data_length,
                                           uint8_t* clear_data) {
  // Local copy (will be modified).
  // Allocated as 64-bit ints to enforce 64-bit alignment for later access as a
  // 64-bit value.
  uint64_t aes_iv[2];
  assert(sizeof(aes_iv) == AES_BLOCK_SIZE);
  // The double-cast is needed to comply with strict aliasing rules.
  uint8_t* aes_iv_u8 =
      reinterpret_cast<uint8_t*>(reinterpret_cast<void*>(aes_iv));
  memcpy(aes_iv_u8, &iv[0], AES_BLOCK_SIZE);

  // The CENC spec specifies we increment only the low 64 bits of the IV
  // counter, and leave the high 64 bits alone.  This is different from the
  // OpenSSL implementation, which increments the entire 128 bit iv. That is
  // why we implement the CTR loop ourselves.
  size_t l = 0;
  if (block_offset > 0 && l < cipher_data_length) {
    // Encrypt the IV.
    uint8_t ecount_buf[AES_BLOCK_SIZE];

    AES_KEY aes_key;
    if (AES_set_encrypt_key(key_u8, AES_BLOCK_SIZE * 8, &aes_key) != 0) {
      LOGE("[DecryptCTR(): FAILURE]");
      return OEMCrypto_ERROR_DECRYPT_FAILED;
    }
    AES_encrypt(aes_iv_u8, ecount_buf, &aes_key);
    for (int n = block_offset; n < AES_BLOCK_SIZE && l < cipher_data_length;
         ++n, ++l) {
      clear_data[l] = cipher_data[l] ^ ecount_buf[n];
    }
    ctr128_inc64(aes_iv_u8);
    block_offset = 0;
  }

  uint64_t remaining = cipher_data_length - l;
  int out_len = 0;

  while (remaining) {
    EVP_CIPHER_CTX* evp_cipher_ctx = EVP_CIPHER_CTX_new();
    EVP_CIPHER_CTX_set_padding(evp_cipher_ctx, 0);
    if (!EVP_DecryptInit_ex(evp_cipher_ctx, EVP_aes_128_ctr(), NULL, key_u8,
                            aes_iv_u8)) {
      LOGE("[DecryptCTR(): EVP_INIT ERROR]");
      EVP_CIPHER_CTX_free(evp_cipher_ctx);
      return OEMCrypto_ERROR_DECRYPT_FAILED;
    }

    // Test the MSB of the counter portion of the initialization vector. If the
    // value is 0xFF the counter is near wrapping. In this case we calculate
    // the number of bytes we can safely decrypt before the counter wraps.
    uint64_t decrypt_length = 0;
    if (aes_iv_u8[8] == 0xFF) {
      uint64_t bottom_64_bits = wvcdm::ntohll64(aes_iv[1]);
      uint64_t bytes_before_iv_wrap = (~bottom_64_bits + 1) * AES_BLOCK_SIZE;
      decrypt_length =
          bytes_before_iv_wrap < remaining ? bytes_before_iv_wrap : remaining;
    } else {
      decrypt_length = remaining;
    }

    if (!EVP_DecryptUpdate(evp_cipher_ctx, &clear_data[l], &out_len,
                           &cipher_data[l], decrypt_length)) {
      LOGE("[DecryptCTR(): EVP_UPDATE_ERROR]");
      EVP_CIPHER_CTX_free(evp_cipher_ctx);
      return OEMCrypto_ERROR_DECRYPT_FAILED;
    }
    l += decrypt_length;
    remaining = cipher_data_length - l;

    int final;
    if (!EVP_DecryptFinal_ex(evp_cipher_ctx,
                             &clear_data[cipher_data_length - remaining],
                             &final)) {
      LOGE("[DecryptCTR(): EVP_FINAL_ERROR]");
      EVP_CIPHER_CTX_free(evp_cipher_ctx);
      return OEMCrypto_ERROR_DECRYPT_FAILED;
    }
    EVP_CIPHER_CTX_free(evp_cipher_ctx);

    // If remaining is not zero, reset the iv before the second pass.
    if (remaining) {
      memcpy(aes_iv_u8, &iv[0], AES_BLOCK_SIZE);
      memset(&aes_iv_u8[8], 0, AES_BLOCK_SIZE / 2);
    }
  }
  return OEMCrypto_SUCCESS;
}

OEMCryptoResult SessionContext::SetDecryptHash(uint32_t frame_number,
                                               const uint8_t* hash,
                                               size_t hash_length) {
  if (hash_length < sizeof(uint32_t)) {
    LOGE("[SetDecryptHash(): short buffer]");
    return OEMCrypto_ERROR_SHORT_BUFFER;
  }
  if (hash_length > sizeof(uint32_t)) {
    LOGE("[SetDecryptHash(): long buffer]");
    return OEMCrypto_ERROR_BUFFER_TOO_LARGE;
  }
  compute_hash_ = true;
  current_frame_number_ = frame_number;
  given_hash_ = *reinterpret_cast<const uint32_t*>(hash);
  return OEMCrypto_SUCCESS;
}

OEMCryptoResult SessionContext::GetHashErrorCode(
    uint32_t* failed_frame_number) {
  if (failed_frame_number == NULL) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  if (hash_error_ != OEMCrypto_SUCCESS)
    *failed_frame_number = bad_frame_number_;
  return hash_error_;
}

}  // namespace wvoec_ref
