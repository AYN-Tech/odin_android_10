// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
// Reference implementation of OEMCrypto APIs
//
#include "OEMCryptoCENC.h"

#include <openssl/cmac.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <chrono>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include "file_store.h"
#include "log.h"
#include "oemcrypto_engine_ref.h"
#include "oemcrypto_session.h"
#include "oemcrypto_usage_table_ref.h"
#include "string_conversions.h"

#if defined(_WIN32)
# define OEMCRYPTO_API extern "C" __declspec(dllexport)
#else  // defined(_WIN32)
# define OEMCRYPTO_API extern "C" __attribute__((visibility("default")))
#endif  // defined(_WIN32)

namespace {
const uint8_t kBakedInCertificateMagicBytes[] = {0xDE, 0xAD, 0xBE, 0xEF};

// Return uint32 referenced through a potentially unaligned pointer.
// If the pointer is NULL, return 0.
uint32_t unaligned_dereference_uint32(const void* unaligned_ptr) {
  if (unaligned_ptr == NULL) return 0;
  uint32_t value;
  const uint8_t* src = reinterpret_cast<const uint8_t*>(unaligned_ptr);
  uint8_t* dest = reinterpret_cast<uint8_t*>(&value);
  for (unsigned long i = 0; i < sizeof(value); i++) {
    dest[i] = src[i];
  }
  return value;
}

}  // namespace

namespace wvoec_ref {

static std::unique_ptr<CryptoEngine> crypto_engine;

typedef struct {
  uint8_t signature[wvoec::MAC_KEY_SIZE];
  uint8_t context[wvoec::MAC_KEY_SIZE];
  uint8_t iv[wvoec::KEY_IV_SIZE];
  uint8_t enc_rsa_key[];
} WrappedRSAKey;

OEMCRYPTO_API OEMCryptoResult OEMCrypto_Initialize(void) {
  if (crypto_engine != nullptr) {
    LOGE("-------------------------  Calling Initialize without Terminate\n");
    crypto_engine.reset();
  }
  // NOTE: This requires a compatible Filesystem implementation.
  // NOTE: Ownership of the FileSystem object is transferred to CryptoEngine
  std::unique_ptr<wvcdm::FileSystem> fs(new wvcdm::FileSystem());
  crypto_engine.reset(CryptoEngine::MakeCryptoEngine(std::move(fs)));
  if (crypto_engine == nullptr || !crypto_engine->Initialize()) {
    LOGE("[OEMCrypto_Initialize(): failed]");
    return OEMCrypto_ERROR_INIT_FAILED;
  }
  return OEMCrypto_SUCCESS;
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_SetSandbox(
    const uint8_t* /*sandbox_id*/, size_t /*sandbox_id_length*/) {
  return OEMCrypto_ERROR_NOT_IMPLEMENTED;
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_Terminate(void) {
  if (crypto_engine == nullptr) {
    LOGE("[OEMCrypto_Terminate(): not initialized]");
    return OEMCrypto_ERROR_TERMINATE_FAILED;
  }
  crypto_engine->Terminate();
  crypto_engine.reset();
  return OEMCrypto_SUCCESS;
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_OpenSession(
    OEMCrypto_SESSION* session) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_OpenSession: OEMCrypto not initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (crypto_engine->GetNumberOfOpenSessions() >=
      crypto_engine->GetMaxNumberOfSessions()) {
    LOGE("[OEMCrypto_OpenSession(): failed due to too many sessions]");
    return OEMCrypto_ERROR_TOO_MANY_SESSIONS;
  }
  SessionId sid = crypto_engine->OpenSession();
  *session = (OEMCrypto_SESSION)sid;
  return OEMCrypto_SUCCESS;
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_CloseSession(
    OEMCrypto_SESSION session) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_CloseSession: OEMCrypto not initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (!crypto_engine->DestroySession((SessionId)session)) {
    return OEMCrypto_ERROR_CLOSE_SESSION_FAILED;
  } else {
    return OEMCrypto_SUCCESS;
  }
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_GenerateDerivedKeys(
    OEMCrypto_SESSION session, const uint8_t* mac_key_context,
    uint32_t mac_key_context_length, const uint8_t* enc_key_context,
    uint32_t enc_key_context_length) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_GenerateDerivedKeys: OEMCrypto not initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (crypto_engine->config_provisioning_method() != OEMCrypto_Keybox) {
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  }
  if (!crypto_engine->ValidRootOfTrust()) {
    LOGE("[OEMCrypto_GenerateDerivedKeys(): ERROR_KEYBOX_INVALID]");
    return OEMCrypto_ERROR_KEYBOX_INVALID;
  }

  SessionContext* session_ctx = crypto_engine->FindSession(session);
  if (!session_ctx || !session_ctx->isValid()) {
    LOGE("[OEMCrypto_GenerateDerivedKeys(): ERROR_INVALID_SESSION]");
    return OEMCrypto_ERROR_INVALID_SESSION;
  }

  const std::vector<uint8_t> mac_ctx_str(
      mac_key_context, mac_key_context + mac_key_context_length);
  const std::vector<uint8_t> enc_ctx_str(
      enc_key_context, enc_key_context + enc_key_context_length);

  // Generate mac and encryption keys for current session context
  if (!session_ctx->DeriveKeys(crypto_engine->DeviceRootKey(), mac_ctx_str,
                               enc_ctx_str)) {
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  return OEMCrypto_SUCCESS;
}


OEMCRYPTO_API OEMCryptoResult OEMCrypto_GenerateNonce(OEMCrypto_SESSION session,
                                                      uint32_t* nonce) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_GenerateNonce: OEMCrypto not initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  SessionContext* session_ctx = crypto_engine->FindSession(session);
  if (!session_ctx || !session_ctx->isValid()) {
    LOGE("[OEMCrypto_GenerateNonce(): ERROR_INVALID_SESSION]");
    return OEMCrypto_ERROR_INVALID_SESSION;
  }

  // Prevent nonce flood.
  static std::chrono::steady_clock clock;
  const auto now = clock.now().time_since_epoch();
  static auto last_nonce_time = now;
  // For testing, we set nonce_flood_count to 1.  Since count is initialized to
  // 1, the very first nonce after initialization is counted as a flood.
  static int nonce_count = 1;

  if (now - last_nonce_time < std::chrono::seconds(1)) {
    nonce_count++;
    if (nonce_count > crypto_engine->nonce_flood_count()) {
      LOGE("[OEMCrypto_GenerateNonce(): Nonce Flood detected]");
      return OEMCrypto_ERROR_UNKNOWN_FAILURE;
    }
  } else {
    nonce_count = 1;
    last_nonce_time = now;
  }

  uint32_t nonce_value = 0;
  uint8_t* nonce_string = reinterpret_cast<uint8_t*>(&nonce_value);

  while (nonce_value == 0 || crypto_engine->NonceCollision(nonce_value)) {
    // Generate 4 bytes of random data
    if (!RAND_bytes(nonce_string, 4)) {
      LOGE("[OEMCrypto_GenerateNonce(): Random bytes failure]");
      return OEMCrypto_ERROR_UNKNOWN_FAILURE;
    }
  }
  session_ctx->AddNonce(nonce_value);
  *nonce = nonce_value;
  return OEMCrypto_SUCCESS;
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_GenerateSignature(
    OEMCrypto_SESSION session, const uint8_t* message, size_t message_length,
    uint8_t* signature, size_t* signature_length) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_GenerateSignature: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }

  if (*signature_length < SHA256_DIGEST_LENGTH) {
    *signature_length = SHA256_DIGEST_LENGTH;
    return OEMCrypto_ERROR_SHORT_BUFFER;
  }

  if (message == NULL || message_length == 0 || signature == NULL ||
      signature_length == 0) {
    LOGE("[OEMCrypto_GenerateSignature(): OEMCrypto_ERROR_INVALID_CONTEXT]");
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }

  SessionContext* session_ctx = crypto_engine->FindSession(session);
  if (!session_ctx || !session_ctx->isValid()) {
    LOGE("[OEMCrypto_GenerateSignature(): ERROR_INVALID_SESSION]");
    return OEMCrypto_ERROR_INVALID_SESSION;
  }

  if (session_ctx->GenerateSignature(message, message_length, signature,
                                     signature_length)) {
    return OEMCrypto_SUCCESS;
  }
  return OEMCrypto_ERROR_UNKNOWN_FAILURE;
}

bool RangeCheck(const uint8_t* message, uint32_t message_length,
                const uint8_t* field, uint32_t field_length, bool allow_null) {
  if (field == NULL) return allow_null;
  if (field < message) return false;
  if (field + field_length > message + message_length) return false;
  return true;
}

bool RangeCheck(uint32_t message_length, const OEMCrypto_Substring& substring,
                bool allow_null) {
  if (!substring.length) return allow_null;
  if (substring.offset > message_length) return false;
  if (substring.offset + substring.length > message_length) return false;
  return true;
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_LoadKeys(
    OEMCrypto_SESSION session, const uint8_t* message, size_t message_length,
    const uint8_t* signature, size_t signature_length,
    OEMCrypto_Substring enc_mac_keys_iv, OEMCrypto_Substring enc_mac_keys,
    size_t num_keys, const OEMCrypto_KeyObject* key_array,
    OEMCrypto_Substring pst, OEMCrypto_Substring srm_restriction_data,
    OEMCrypto_LicenseType license_type) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_LoadKeys: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (!crypto_engine->ValidRootOfTrust()) {
    LOGE("[OEMCrypto_LoadKeys(): ERROR_KEYBOX_INVALID]");
    return OEMCrypto_ERROR_KEYBOX_INVALID;
  }
  SessionContext* session_ctx = crypto_engine->FindSession(session);
  if (!session_ctx || !session_ctx->isValid()) {
    LOGE("[OEMCrypto_LoadKeys(): ERROR_INVALID_SESSION]");
    return OEMCrypto_ERROR_INVALID_SESSION;
  }
  if (message == NULL || message_length == 0 || signature == NULL ||
      signature_length == 0 || key_array == NULL || num_keys == 0) {
    LOGE("[OEMCrypto_LoadKeys(): OEMCrypto_ERROR_INVALID_CONTEXT]");
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }

  // Range check
  if (!RangeCheck(message_length, enc_mac_keys_iv, true) ||
      !RangeCheck(message_length, enc_mac_keys, true) ||
      !RangeCheck(message_length, pst, true) ||
      !RangeCheck(message_length, srm_restriction_data, true)) {
    LOGE("[OEMCrypto_LoadKeys(): OEMCrypto_ERROR_INVALID_CONTEXT - "
         "range check.]");
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }

  for (unsigned int i = 0; i < num_keys; i++) {
    if (!RangeCheck(message_length, key_array[i].key_id, false) ||
        !RangeCheck(message_length, key_array[i].key_data, false) ||
        !RangeCheck(message_length, key_array[i].key_data_iv, false) ||
        !RangeCheck(message_length, key_array[i].key_control, false) ||
        !RangeCheck(message_length, key_array[i].key_control_iv, false)) {
      LOGE("[OEMCrypto_LoadKeys(): OEMCrypto_ERROR_INVALID_CONTEXT - "
           "range check %d]", i);
      return OEMCrypto_ERROR_INVALID_CONTEXT;
    }
  }
  if (enc_mac_keys.offset >= wvoec::KEY_IV_SIZE && enc_mac_keys.length > 0) {
    if (enc_mac_keys_iv.offset + wvoec::KEY_IV_SIZE == enc_mac_keys.offset) {
      LOGE("[OEMCrypto_LoadKeys(): OEMCrypto_ERROR_INVALID_CONTEXT - "
           "range check iv]");
      return OEMCrypto_ERROR_INVALID_CONTEXT;
    } else {
      if (memcmp(message + enc_mac_keys.offset - wvoec::KEY_IV_SIZE,
                 message + enc_mac_keys_iv.offset, wvoec::KEY_IV_SIZE) == 0) {
        LOGE("[OEMCrypto_LoadKeys(): OEMCrypto_ERROR_INVALID_CONTEXT - "
             "suspicious iv]");
        return OEMCrypto_ERROR_INVALID_CONTEXT;
      }
    }
  }
  return session_ctx->LoadKeys(message, message_length, signature,
                               signature_length, enc_mac_keys_iv, enc_mac_keys,
                               num_keys, key_array, pst, srm_restriction_data,
                               license_type);
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_LoadEntitledContentKeys(
    OEMCrypto_SESSION session, const uint8_t* message, size_t message_length,
    size_t num_keys, const OEMCrypto_EntitledContentKeyObject* key_array) {
  if (num_keys == 0) {
    LOGE("[OEMCrypto_LoadEntitledContentKeys(): key_array is empty.");
    return OEMCrypto_SUCCESS;
  }
  if (!key_array) {
    LOGE("[OEMCrypto_LoadEntitledContentKeys(): missing key_array.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_LoadEntitledContentKeys: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  SessionContext* session_ctx = crypto_engine->FindSession(session);
  if (!session_ctx || !session_ctx->isValid()) {
    LOGE("[OEMCrypto_LoadEntitledContentKeys(): ERROR_INVALID_SESSION]");
    return OEMCrypto_ERROR_INVALID_SESSION;
  }
  for (unsigned int i = 0; i < num_keys; i++) {
    if (!RangeCheck(message_length, key_array[i].entitlement_key_id, false) ||
        !RangeCheck(message_length, key_array[i].content_key_id, false) ||
        !RangeCheck(message_length, key_array[i].content_key_data_iv, false) ||
        !RangeCheck(message_length, key_array[i].content_key_data, false)) {
      LOGE(
          "[OEMCrypto_LoadEntitledContentKeys(): "
          "OEMCrypto_ERROR_INVALID_CONTEXT -range "
          "check %d]",
          i);
      return OEMCrypto_ERROR_INVALID_CONTEXT;
    }
  }

  return session_ctx->LoadEntitledContentKeys(message, message_length, num_keys,
                                              key_array);
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_RefreshKeys(
    OEMCrypto_SESSION session, const uint8_t* message, size_t message_length,
    const uint8_t* signature, size_t signature_length, size_t num_keys,
    const OEMCrypto_KeyRefreshObject* key_array) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_RefreshKeys: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }

  if (!crypto_engine->ValidRootOfTrust()) {
    LOGE("[OEMCrypto_RefreshKeys(): ERROR_KEYBOX_INVALID]");
    return OEMCrypto_ERROR_KEYBOX_INVALID;
  }

  SessionContext* session_ctx = crypto_engine->FindSession(session);
  if (!session_ctx || !session_ctx->isValid()) {
    LOGE("[OEMCrypto_RefreshKeys(): ERROR_INVALID_SESSION]");
    return OEMCrypto_ERROR_INVALID_SESSION;
  }

  if (message == NULL || message_length == 0 || signature == NULL ||
      signature_length == 0 || num_keys == 0) {
    LOGE("[OEMCrypto_RefreshKeys(): OEMCrypto_ERROR_INVALID_CONTEXT]");
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }

  // Range check
  for (unsigned int i = 0; i < num_keys; i++) {
    if (!RangeCheck(message_length, key_array[i].key_id, true) ||
        !RangeCheck(message_length, key_array[i].key_control, false) ||
        !RangeCheck(message_length, key_array[i].key_control_iv, true)) {
      LOGE("[OEMCrypto_RefreshKeys(): Range Check %d]", i);
      return OEMCrypto_ERROR_INVALID_CONTEXT;
    }
  }

  // Validate message signature
  if (!session_ctx->ValidateMessage(message, message_length, signature,
                                    signature_length)) {
    LOGE("[OEMCrypto_RefreshKeys(): signature was invalid]");
    return OEMCrypto_ERROR_SIGNATURE_FAILURE;
  }

  // Decrypt and refresh keys in key refresh object
  OEMCryptoResult status = OEMCrypto_SUCCESS;
  std::vector<uint8_t> key_id;
  std::vector<uint8_t> key_control;
  std::vector<uint8_t> key_control_iv;
  for (unsigned int i = 0; i < num_keys; i++) {
    if (key_array[i].key_id.length != 0) {
      key_id.assign(
          message + key_array[i].key_id.offset,
          message + key_array[i].key_id.offset + key_array[i].key_id.length);
      key_control.assign(
          message + key_array[i].key_control.offset,
          message + key_array[i].key_control.offset + wvoec::KEY_CONTROL_SIZE);
      if (key_array[i].key_control_iv.length == 0) {
        key_control_iv.clear();
      } else {
        key_control_iv.assign(
            message + key_array[i].key_control_iv.offset,
            message + key_array[i].key_control_iv.offset + wvoec::KEY_IV_SIZE);
      }
    } else {
      // key_id could be null if special control key type
      // key_control is not encrypted in this case
      key_id.clear();
      key_control_iv.clear();
      key_control.assign(
          message + key_array[i].key_control.offset,
          message + key_array[i].key_control.offset + wvoec::KEY_CONTROL_SIZE);
    }

    status = session_ctx->RefreshKey(key_id, key_control, key_control_iv);
    if (status != OEMCrypto_SUCCESS) {
      LOGE("[OEMCrypto_RefreshKeys():  error %u in key %i]", status, i);
      break;
    }
  }

  session_ctx->FlushNonces();
  if (status != OEMCrypto_SUCCESS) {
    return status;
  }

  session_ctx->StartTimer();
  return OEMCrypto_SUCCESS;
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_QueryKeyControl(
    OEMCrypto_SESSION session, const uint8_t* key_id, size_t key_id_length,
    uint8_t* key_control_block, size_t* key_control_block_length) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_QueryKeyControl: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  uint32_t* block = reinterpret_cast<uint32_t*>(key_control_block);
  if ((key_control_block_length == NULL) ||
      (*key_control_block_length < wvoec::KEY_CONTROL_SIZE)) {
    LOGE("[OEMCrypto_QueryKeyControl(): OEMCrypto_ERROR_SHORT_BUFFER]");
    return OEMCrypto_ERROR_SHORT_BUFFER;
  }
  *key_control_block_length = wvoec::KEY_CONTROL_SIZE;
  if (key_id == NULL) {
    LOGE(
        "[OEMCrypto_QueryKeyControl(): key_id null. "
        "OEMCrypto_ERROR_UNKNOWN_FAILURE]");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }

  SessionContext* session_ctx = crypto_engine->FindSession(session);
  if (!session_ctx || !session_ctx->isValid()) {
    LOGE("[OEMCrypto_QueryKeyControl(): ERROR_INVALID_SESSION]");
    return OEMCrypto_ERROR_INVALID_SESSION;
  }
  const std::vector<uint8_t> key_id_str =
      std::vector<uint8_t>(key_id, key_id + key_id_length);
  if (!session_ctx->QueryKeyControlBlock(key_id_str, block)) {
    LOGE("[OEMCrypto_QueryKeyControl(): FAIL]");
    return OEMCrypto_ERROR_NO_CONTENT_KEY;
  }
  return OEMCrypto_SUCCESS;
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_SelectKey(
    const OEMCrypto_SESSION session, const uint8_t* key_id,
    size_t key_id_length, OEMCryptoCipherMode cipher_mode) {
#ifndef NDEBUG
  if (!crypto_engine->ValidRootOfTrust()) {
    LOGE("[OEMCrypto_SelectKey(): ERROR_KEYBOX_INVALID]");
    return OEMCrypto_ERROR_KEYBOX_INVALID;
  }
#endif

  SessionContext* session_ctx = crypto_engine->FindSession(session);
  if (!session_ctx || !session_ctx->isValid()) {
    LOGE("[OEMCrypto_SelectKey(): ERROR_INVALID_SESSION]");
    return OEMCrypto_ERROR_INVALID_SESSION;
  }

  const std::vector<uint8_t> key_id_str =
      std::vector<uint8_t>(key_id, key_id + key_id_length);
  return session_ctx->SelectContentKey(key_id_str, cipher_mode);
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_DecryptCENC(
    OEMCrypto_SESSION session, const uint8_t* data_addr, size_t data_length,
    bool is_encrypted, const uint8_t* iv, size_t block_offset,
    OEMCrypto_DestBufferDesc* out_buffer,
    const OEMCrypto_CENCEncryptPatternDesc* pattern, uint8_t subsample_flags) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_DecryptCENC: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (data_addr == NULL || data_length == 0 || iv == NULL ||
      out_buffer == NULL) {
    LOGE("[OEMCrypto_DecryptCENC(): OEMCrypto_ERROR_INVALID_CONTEXT]");
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }
  if (crypto_engine->max_buffer_size() > 0 &&
      data_length > crypto_engine->max_buffer_size()) {
    // For testing reasons only, pretend that this integration only supports
    // the minimum possible buffer size.
    LOGE("[OEMCrypto_DecryptCENC(): OEMCrypto_ERROR_BUFFER_TOO_LARGE]");
    return OEMCrypto_ERROR_BUFFER_TOO_LARGE;
  }
  OEMCryptoResult status =
      crypto_engine->SetDestination(out_buffer, data_length, subsample_flags);
  if (status != OEMCrypto_SUCCESS) {
    LOGE("[OEMCrypto_DecryptCENC(): destination status: %d]", status);
    return status;
  }
#ifndef NDEBUG
  if (!crypto_engine->ValidRootOfTrust()) {
    LOGE("[OEMCrypto_DecryptCENC(): ERROR_KEYBOX_INVALID]");
    return OEMCrypto_ERROR_KEYBOX_INVALID;
  }
#endif

  SessionContext* session_ctx = crypto_engine->FindSession(session);
  if (!session_ctx || !session_ctx->isValid()) {
    LOGE("[OEMCrypto_DecryptCENC(): ERROR_INVALID_SESSION]");
    return OEMCrypto_ERROR_INVALID_SESSION;
  }

  OEMCryptoResult result = session_ctx->DecryptCENC(
      iv, block_offset, pattern, data_addr, data_length, is_encrypted,
      crypto_engine->destination(), out_buffer->type, subsample_flags);
  if (result != OEMCrypto_SUCCESS) return result;
  return crypto_engine->PushDestination(out_buffer, subsample_flags);
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_CopyBuffer(
    OEMCrypto_SESSION session, const uint8_t* data_addr, size_t data_length,
    OEMCrypto_DestBufferDesc* out_buffer, uint8_t subsample_flags) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_CopyBuffer: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (data_addr == NULL || out_buffer == NULL) {
    LOGE("[OEMCrypto_CopyBuffer(): OEMCrypto_ERROR_INVALID_CONTEXT]");
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }
  if (crypto_engine->max_buffer_size() > 0 &&
      data_length > crypto_engine->max_buffer_size()) {
    // For testing reasons only, pretend that this integration only supports
    // the minimum possible buffer size.
    LOGE("[OEMCrypto_CopyBuffer(): OEMCrypto_ERROR_BUFFER_TOO_LARGE]");
    return OEMCrypto_ERROR_BUFFER_TOO_LARGE;
  }
  OEMCryptoResult status =
      crypto_engine->SetDestination(out_buffer, data_length, subsample_flags);
  if (status != OEMCrypto_SUCCESS) return status;
  if (crypto_engine->destination() != NULL) {
    memmove(crypto_engine->destination(), data_addr, data_length);
  }
  return crypto_engine->PushDestination(out_buffer, subsample_flags);
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_WrapKeyboxOrOEMCert(
    const uint8_t* keybox, size_t keyBoxLength, uint8_t* wrappedKeybox,
    size_t* wrappedKeyBoxLength, const uint8_t* transportKey,
    size_t transportKeyLength) {
  if (crypto_engine->config_provisioning_method() != OEMCrypto_Keybox) {
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  }
  if (!keybox || !wrappedKeybox || !wrappedKeyBoxLength ||
      (keyBoxLength != *wrappedKeyBoxLength)) {
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  // This implementation ignores the transport key.  For test keys, we
  // don't need to encrypt the keybox.
  memcpy(wrappedKeybox, keybox, keyBoxLength);
  return OEMCrypto_SUCCESS;
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_InstallKeyboxOrOEMCert(
    const uint8_t* keybox, size_t keyBoxLength) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_InstallKeyboxOrOEMCert: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (crypto_engine->config_provisioning_method() != OEMCrypto_Keybox) {
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  }
  if (crypto_engine->InstallKeybox(keybox, keyBoxLength)) {
    return OEMCrypto_SUCCESS;
  }
  return OEMCrypto_ERROR_WRITE_KEYBOX;
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_LoadTestKeybox(const uint8_t* buffer,
                                                       size_t length) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_LoadTestKeybox: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (crypto_engine->config_provisioning_method() != OEMCrypto_Keybox) {
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  }
  crypto_engine->UseTestKeybox(buffer, length);
  return OEMCrypto_SUCCESS;
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_IsKeyboxOrOEMCertValid(void) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_IsKeyboxOrOEMCertValid: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  switch (crypto_engine->config_provisioning_method()) {
    case OEMCrypto_DrmCertificate:
      return OEMCrypto_SUCCESS;
    case OEMCrypto_Keybox:
      switch (crypto_engine->ValidateKeybox()) {
        case NO_ERROR:
          return OEMCrypto_SUCCESS;
        case BAD_CRC:
          return OEMCrypto_ERROR_BAD_CRC;
        case BAD_MAGIC:
          return OEMCrypto_ERROR_BAD_MAGIC;
        default:
        case OTHER_ERROR:
          return OEMCrypto_ERROR_UNKNOWN_FAILURE;
      }
      break;
    case OEMCrypto_OEMCertificate:
      // TODO(fredgc): verify that the certificate exists and is valid.
      return OEMCrypto_SUCCESS;
      break;
    default:
      LOGE("Invalid provisioning method: %d.",
           crypto_engine->config_provisioning_method());
      return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
}

OEMCRYPTO_API OEMCrypto_ProvisioningMethod OEMCrypto_GetProvisioningMethod() {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_GetProvisioningMethod: OEMCrypto Not Initialized.");
    return OEMCrypto_ProvisioningError;
  }
  return crypto_engine->config_provisioning_method();
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_GetOEMPublicCertificate(
    OEMCrypto_SESSION session, uint8_t* public_cert,
    size_t* public_cert_length) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_GetOEMPublicCertificate: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (crypto_engine->config_provisioning_method() != OEMCrypto_OEMCertificate) {
    LOGE("OEMCrypto_GetOEMPublicCertificate: Provisioning method = %d.",
         crypto_engine->config_provisioning_method());
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  }
  SessionContext* session_ctx = crypto_engine->FindSession(session);
  if (!session_ctx || !session_ctx->isValid()) {
    LOGE("[OEMCrypto_GetOEMPublicCertificate(): ERROR_INVALID_SESSION]");
    return OEMCrypto_ERROR_INVALID_SESSION;
  }
  return crypto_engine->get_oem_certificate(session_ctx, public_cert,
                                            public_cert_length);
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_GetDeviceID(uint8_t* deviceID,
                                                    size_t* idLength) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_GetDeviceID: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (crypto_engine->config_provisioning_method() != OEMCrypto_Keybox) {
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  }
  // Devices that do not support a keybox should use some other method to
  // store the device id.
  const std::vector<uint8_t>& dev_id_string = crypto_engine->DeviceRootId();
  if (dev_id_string.empty()) {
    LOGE("[OEMCrypto_GetDeviceId(): Keybox Invalid]");
    return OEMCrypto_ERROR_KEYBOX_INVALID;
  }

  size_t dev_id_len = dev_id_string.size();
  if (*idLength < dev_id_len) {
    *idLength = dev_id_len;
    LOGE("[OEMCrypto_GetDeviceId(): ERROR_SHORT_BUFFER]");
    return OEMCrypto_ERROR_SHORT_BUFFER;
  }
  memset(deviceID, 0, *idLength);
  memcpy(deviceID, &dev_id_string[0], dev_id_len);
  *idLength = dev_id_len;
  return OEMCrypto_SUCCESS;
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_GetKeyData(uint8_t* keyData,
                                                   size_t* keyDataLength) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_GetKeyData: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (crypto_engine->config_provisioning_method() != OEMCrypto_Keybox) {
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  }
  size_t length = crypto_engine->DeviceRootTokenLength();
  if (keyDataLength == NULL) {
    LOGE("[OEMCrypto_GetKeyData(): null pointer. ERROR_UNKNOWN_FAILURE]");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (*keyDataLength < length) {
    *keyDataLength = length;
    LOGE("[OEMCrypto_GetKeyData(): ERROR_SHORT_BUFFER]");
    return OEMCrypto_ERROR_SHORT_BUFFER;
  }
  if (keyData == NULL) {
    LOGE("[OEMCrypto_GetKeyData(): null pointer. ERROR_UNKNOWN_FAILURE]");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  memset(keyData, 0, *keyDataLength);
  memcpy(keyData, crypto_engine->DeviceRootToken(), length);
  *keyDataLength = length;
  return OEMCrypto_SUCCESS;
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_GetRandom(uint8_t* randomData,
                                                  size_t dataLength) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_GetRandom: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (!randomData) {
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (RAND_bytes(randomData, dataLength)) {
    return OEMCrypto_SUCCESS;
  }
  return OEMCrypto_ERROR_UNKNOWN_FAILURE;
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_RewrapDeviceRSAKey30(
    OEMCrypto_SESSION session, const uint32_t* unaligned_nonce,
    const uint8_t* encrypted_message_key, size_t encrypted_message_key_length,
    const uint8_t* enc_rsa_key, size_t enc_rsa_key_length,
    const uint8_t* enc_rsa_key_iv, uint8_t* wrapped_rsa_key,
    size_t* wrapped_rsa_key_length) {
  uint32_t nonce = unaligned_dereference_uint32(unaligned_nonce);
  if (unaligned_nonce == NULL) {
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_RewrapDeviceRSAKey30: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (wrapped_rsa_key_length == NULL) {
    LOGE("[OEMCrypto_RewrapDeviceRSAKey30(): OEMCrypto_ERROR_INVALID_CONTEXT]");
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }
  // For the reference implementation, the wrapped key and the encrypted
  // key are the same size -- just encrypted with different keys.
  // We add 32 bytes for a context, 32 for iv, and 32 bytes for a signature.
  // Important: This layout must match OEMCrypto_LoadDeviceRSAKey below.
  size_t buffer_size = enc_rsa_key_length + sizeof(WrappedRSAKey);

  if (wrapped_rsa_key == NULL || *wrapped_rsa_key_length < buffer_size) {
    *wrapped_rsa_key_length = buffer_size;
    return OEMCrypto_ERROR_SHORT_BUFFER;
  }
  *wrapped_rsa_key_length = buffer_size;  // Tell caller how much space we used.
  if (!crypto_engine->ValidRootOfTrust()) {
    LOGE("[OEMCrypto_RewrapDeviceRSAKey30(): ERROR_KEYBOX_INVALID]");
    return OEMCrypto_ERROR_KEYBOX_INVALID;
  }
  SessionContext* session_ctx = crypto_engine->FindSession(session);
  if (!session_ctx || !session_ctx->isValid()) {
    LOGE("[OEMCrypto_RewrapDeviceRSAKey30(): ERROR_INVALID_SESSION]");
    return OEMCrypto_ERROR_INVALID_SESSION;
  }
  if (encrypted_message_key == NULL || encrypted_message_key_length == 0 ||
      enc_rsa_key == NULL || enc_rsa_key_iv == NULL ||
      unaligned_nonce == NULL) {
    LOGE("[OEMCrypto_RewrapDeviceRSAKey30(): OEMCrypto_ERROR_INVALID_CONTEXT]");
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }

  // Validate nonce
  if (!session_ctx->CheckNonce(nonce)) {
    return OEMCrypto_ERROR_INVALID_NONCE;
  }
  session_ctx->FlushNonces();

  if (!session_ctx->InstallRSAEncryptedKey(encrypted_message_key,
                                           encrypted_message_key_length)) {
    LOGE(
        "OEMCrypto_RewrapDeviceRSAKey30: "
        "Error loading encrypted_message_key.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }

  // Decrypt RSA key.
  std::vector<uint8_t> pkcs8_rsa_key(enc_rsa_key_length);
  if (!session_ctx->DecryptRSAKey(enc_rsa_key, enc_rsa_key_length,
                                  enc_rsa_key_iv, &pkcs8_rsa_key[0])) {
    return OEMCrypto_ERROR_INVALID_RSA_KEY;
  }
  size_t padding = pkcs8_rsa_key[enc_rsa_key_length - 1];
  if (padding > 16) {
    LOGE(
        "[OEMCrypto_RewrapDeviceRSAKey30(): "
        "Encrypted RSA has bad padding: %d]",
        padding);
    return OEMCrypto_ERROR_INVALID_RSA_KEY;
  }
  size_t rsa_key_length = enc_rsa_key_length - padding;
  if (!session_ctx->LoadRSAKey(&pkcs8_rsa_key[0], rsa_key_length)) {
    LOGE("[OEMCrypto_RewrapDeviceRSAKey30(): Failed to LoadRSAKey.");
    return OEMCrypto_ERROR_INVALID_RSA_KEY;
  }

  // Now we generate a wrapped keybox.
  WrappedRSAKey* wrapped = reinterpret_cast<WrappedRSAKey*>(wrapped_rsa_key);
  // Pick a random context and IV for generating keys.
  if (!RAND_bytes(wrapped->context, sizeof(wrapped->context))) {
    LOGE("[_RewrapDeviceRSAKey30(): RAND_bytes failed.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (!RAND_bytes(wrapped->iv, sizeof(wrapped->iv))) {
    LOGE("[_RewrapDeviceRSAKey30(): RAND_bytes failed.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  const std::vector<uint8_t> context(
      wrapped->context, wrapped->context + sizeof(wrapped->context));
  // Generate mac and encryption keys for encrypting the signature.
  if (!session_ctx->DeriveKeys(crypto_engine->DeviceRootKey(), context,
                               context)) {
    LOGE("[_RewrapDeviceRSAKey30(): DeriveKeys failed.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }

  // Encrypt rsa key with keybox.
  if (!session_ctx->EncryptRSAKey(&pkcs8_rsa_key[0], enc_rsa_key_length,
                                  wrapped->iv, wrapped->enc_rsa_key)) {
    LOGE("[_RewrapDeviceRSAKey30(): EncrypteRSAKey failed.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  // The wrapped keybox must be signed with the same key we verify with. I'll
  // pick the server key, so I don't have to modify LoadRSAKey.
  unsigned int sig_length = sizeof(wrapped->signature);
  if (!HMAC(EVP_sha256(), &session_ctx->mac_key_server()[0],
            session_ctx->mac_key_server().size(), wrapped->context,
            buffer_size - sizeof(wrapped->signature), wrapped->signature,
            &sig_length)) {
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  return OEMCrypto_SUCCESS;
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_RewrapDeviceRSAKey(
    OEMCrypto_SESSION session, const uint8_t* message, size_t message_length,
    const uint8_t* signature, size_t signature_length,
    const uint32_t* unaligned_nonce, const uint8_t* enc_rsa_key,
    size_t enc_rsa_key_length, const uint8_t* enc_rsa_key_iv,
    uint8_t* wrapped_rsa_key, size_t* wrapped_rsa_key_length) {
  uint32_t nonce = unaligned_dereference_uint32(unaligned_nonce);
    if (unaligned_nonce == NULL) {
      return OEMCrypto_ERROR_INVALID_CONTEXT;
    }
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_RewrapDeviceRSAKey: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (crypto_engine->config_provisioning_method() != OEMCrypto_Keybox) {
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  }
  if (wrapped_rsa_key_length == NULL) {
    LOGE("[OEMCrypto_RewrapDeviceRSAKey(): OEMCrypto_ERROR_INVALID_CONTEXT]");
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }
  // For the reference implementation, the wrapped key and the encrypted
  // key are the same size -- just encrypted with different keys.
  // We add 32 bytes for a context, 32 for iv, and 32 bytes for a signature.
  // Important: This layout must match OEMCrypto_LoadDeviceRSAKey below.
  size_t buffer_size = enc_rsa_key_length + sizeof(WrappedRSAKey);

  if (wrapped_rsa_key == NULL || *wrapped_rsa_key_length < buffer_size) {
    *wrapped_rsa_key_length = buffer_size;
    return OEMCrypto_ERROR_SHORT_BUFFER;
  }
  *wrapped_rsa_key_length = buffer_size;  // Tell caller how much space we used.
  if (!crypto_engine->ValidRootOfTrust()) {
    LOGE("[OEMCrypto_RewrapDeviceRSAKey(): ERROR_KEYBOX_INVALID]");
    return OEMCrypto_ERROR_KEYBOX_INVALID;
  }
  SessionContext* session_ctx = crypto_engine->FindSession(session);
  if (!session_ctx || !session_ctx->isValid()) {
    LOGE("[OEMCrypto_RewrapDeviceRSAKey(): ERROR_INVALID_SESSION]");
    return OEMCrypto_ERROR_INVALID_SESSION;
  }
  if (message == NULL || message_length == 0 || signature == NULL ||
      signature_length == 0 || unaligned_nonce == NULL || enc_rsa_key == NULL) {
    LOGE("[OEMCrypto_RewrapDeviceRSAKey(): OEMCrypto_ERROR_INVALID_CONTEXT]");
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }

  // Range check
  if (!RangeCheck(message, message_length,
                  reinterpret_cast<const uint8_t*>(unaligned_nonce),
                  sizeof(uint32_t), true) ||
      !RangeCheck(message, message_length, enc_rsa_key, enc_rsa_key_length,
                  true) ||
      !RangeCheck(message, message_length, enc_rsa_key_iv, wvoec::KEY_IV_SIZE, true)) {
    LOGE("[OEMCrypto_RewrapDeviceRSAKey():  - range check.]");
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }

  // Validate nonce
  if (!session_ctx->CheckNonce(nonce)) {
    return OEMCrypto_ERROR_INVALID_NONCE;
  }
  session_ctx->FlushNonces();

  // Decrypt RSA key.
  std::vector<uint8_t> pkcs8_rsa_key(enc_rsa_key_length);
  if (!session_ctx->DecryptRSAKey(enc_rsa_key, enc_rsa_key_length,
                                  enc_rsa_key_iv, &pkcs8_rsa_key[0])) {
    return OEMCrypto_ERROR_INVALID_RSA_KEY;
  }
  size_t padding = pkcs8_rsa_key[enc_rsa_key_length - 1];
  if (padding > 16) {
    LOGE("[RewrapDeviceRSAKey(): Encrypted RSA has bad padding: %d]", padding);
    return OEMCrypto_ERROR_INVALID_RSA_KEY;
  }
  size_t rsa_key_length = enc_rsa_key_length - padding;
  // verify signature, verify RSA key, and load it.
  if (!session_ctx->ValidateMessage(message, message_length, signature,
                                    signature_length)) {
    LOGE("[RewrapDeviceRSAKey(): Could not verify signature]");
    return OEMCrypto_ERROR_SIGNATURE_FAILURE;
  }
  if (!session_ctx->LoadRSAKey(&pkcs8_rsa_key[0], rsa_key_length)) {
    return OEMCrypto_ERROR_INVALID_RSA_KEY;
  }

  // Now we generate a wrapped keybox.
  WrappedRSAKey* wrapped = reinterpret_cast<WrappedRSAKey*>(wrapped_rsa_key);
  // Pick a random context and IV for generating keys.
  if (!RAND_bytes(wrapped->context, sizeof(wrapped->context))) {
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (!RAND_bytes(wrapped->iv, sizeof(wrapped->iv))) {
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  const std::vector<uint8_t> context(
      wrapped->context, wrapped->context + sizeof(wrapped->context));
  // Generate mac and encryption keys for encrypting the signature.
  if (!session_ctx->DeriveKeys(crypto_engine->DeviceRootKey(), context,
                               context)) {
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }

  // Encrypt rsa key with keybox.
  if (!session_ctx->EncryptRSAKey(&pkcs8_rsa_key[0], enc_rsa_key_length,
                                  wrapped->iv, wrapped->enc_rsa_key)) {
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  // The wrapped keybox must be signed with the same key we verify with. I'll
  // pick the server key, so I don't have to modify LoadRSAKey.
  unsigned int sig_length = sizeof(wrapped->signature);
  if (!HMAC(EVP_sha256(), &session_ctx->mac_key_server()[0],
            session_ctx->mac_key_server().size(), wrapped->context,
            buffer_size - sizeof(wrapped->signature), wrapped->signature,
            &sig_length)) {
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  return OEMCrypto_SUCCESS;
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_LoadDeviceRSAKey(
    OEMCrypto_SESSION session, const uint8_t* wrapped_rsa_key,
    size_t wrapped_rsa_key_length) {
  if (wrapped_rsa_key == NULL) {
    LOGE("[OEMCrypto_LoadDeviceRSAKey(): OEMCrypto_ERROR_INVALID_CONTEXT]");
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_LoadDeviceRSAKey: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (crypto_engine->config_provisioning_method() == OEMCrypto_DrmCertificate) {
    // If we are using a baked in cert, the "wrapped RSA key" should actually be
    // the magic value for baked-in certificates.
    if (wrapped_rsa_key_length != sizeof(kBakedInCertificateMagicBytes) ||
        memcmp(kBakedInCertificateMagicBytes, wrapped_rsa_key,
               wrapped_rsa_key_length) != 0) {
      LOGE("OEMCrypto_LoadDeviceRSAKey: Baked in Cert has wrong size.");
      return OEMCrypto_ERROR_INVALID_RSA_KEY;
    } else {
      return OEMCrypto_SUCCESS;
    }
  }
  const WrappedRSAKey* wrapped =
      reinterpret_cast<const WrappedRSAKey*>(wrapped_rsa_key);
  if (!crypto_engine->ValidRootOfTrust()) {
    LOGE("[OEMCrypto_LoadDeviceRSAKey(): ERROR_KEYBOX_INVALID]");
    return OEMCrypto_ERROR_KEYBOX_INVALID;
  }

  SessionContext* session_ctx = crypto_engine->FindSession(session);
  if (!session_ctx || !session_ctx->isValid()) {
    LOGE("[OEMCrypto_LoadDeviceRSAKey(): ERROR_INVALID_SESSION]");
    return OEMCrypto_ERROR_INVALID_SESSION;
  }
  const std::vector<uint8_t> context(
      wrapped->context, wrapped->context + sizeof(wrapped->context));
  // Generate mac and encryption keys for encrypting the signature.
  if (!session_ctx->DeriveKeys(crypto_engine->DeviceRootKey(), context,
                               context)) {
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  // Decrypt RSA key.
  std::vector<uint8_t> pkcs8_rsa_key(wrapped_rsa_key_length -
                                     sizeof(wrapped->signature));
  size_t enc_rsa_key_length = wrapped_rsa_key_length - sizeof(WrappedRSAKey);
  if (!session_ctx->DecryptRSAKey(wrapped->enc_rsa_key, enc_rsa_key_length,
                                  wrapped->iv, &pkcs8_rsa_key[0])) {
    return OEMCrypto_ERROR_INVALID_RSA_KEY;
  }
  size_t padding = pkcs8_rsa_key[enc_rsa_key_length - 1];
  if (padding > 16) {
    LOGE("[LoadDeviceRSAKey(): Encrypted RSA has bad padding: %d]", padding);
    return OEMCrypto_ERROR_INVALID_RSA_KEY;
  }
  size_t rsa_key_length = enc_rsa_key_length - padding;
  // verify signature.
  if (!session_ctx->ValidateMessage(
          wrapped->context, wrapped_rsa_key_length - sizeof(wrapped->signature),
          wrapped->signature, sizeof(wrapped->signature))) {
    LOGE("[LoadDeviceRSAKey(): Could not verify signature]");
    return OEMCrypto_ERROR_SIGNATURE_FAILURE;
  }
  if (!session_ctx->LoadRSAKey(&pkcs8_rsa_key[0], rsa_key_length)) {
    return OEMCrypto_ERROR_INVALID_RSA_KEY;
  }
  return OEMCrypto_SUCCESS;
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_LoadTestRSAKey() {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_LoadTestRSAKey: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (crypto_engine->LoadTestRsaKey()) return OEMCrypto_SUCCESS;
  return OEMCrypto_ERROR_UNKNOWN_FAILURE;
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_GenerateRSASignature(
    OEMCrypto_SESSION session, const uint8_t* message, size_t message_length,
    uint8_t* signature, size_t* signature_length,
    RSA_Padding_Scheme padding_scheme) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_GenerateRSASignature: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }

  if (signature_length == 0) {
    LOGE("[OEMCrypto_GenerateRSASignature(): OEMCrypto_ERROR_INVALID_CONTEXT]");
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }

  SessionContext* session_ctx = crypto_engine->FindSession(session);
  if (!session_ctx || !session_ctx->isValid()) {
    LOGE("[OEMCrypto_GenerateRSASignature(): ERROR_INVALID_SESSION]");
    return OEMCrypto_ERROR_INVALID_SESSION;
  }

  size_t required_size = session_ctx->RSASignatureSize();
  if (*signature_length < required_size) {
    *signature_length = required_size;
    return OEMCrypto_ERROR_SHORT_BUFFER;
  }

  if (message == NULL || message_length == 0 || signature == NULL ||
      signature_length == 0) {
    LOGE("[OEMCrypto_GenerateRSASignature(): OEMCrypto_ERROR_INVALID_CONTEXT]");
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }

  OEMCryptoResult sts = session_ctx->GenerateRSASignature(
      message, message_length, signature, signature_length, padding_scheme);
  return sts;
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_DeriveKeysFromSessionKey(
    OEMCrypto_SESSION session, const uint8_t* enc_session_key,
    size_t enc_session_key_length, const uint8_t* mac_key_context,
    size_t mac_key_context_length, const uint8_t* enc_key_context,
    size_t enc_key_context_length) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_DeriveKeysFromSessionKey: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (!crypto_engine->ValidRootOfTrust()) {
    LOGE("[OEMCrypto_GenerateDerivedKeys(): ERROR_KEYBOX_INVALID]");
    return OEMCrypto_ERROR_KEYBOX_INVALID;
  }

  SessionContext* session_ctx = crypto_engine->FindSession(session);
  if (!session_ctx || !session_ctx->isValid()) {
    LOGE("[OEMCrypto_GenerateDerivedKeys(): ERROR_INVALID_SESSION]");
    return OEMCrypto_ERROR_INVALID_SESSION;
  }

  if (session_ctx->allowed_schemes() != kSign_RSASSA_PSS) {
    LOGE("[OEMCrypto_GenerateDerivedKeys(): x509 key used to derive keys]");
    return OEMCrypto_ERROR_INVALID_RSA_KEY;
  }

  const std::vector<uint8_t> ssn_key_str(
      enc_session_key, enc_session_key + enc_session_key_length);
  const std::vector<uint8_t> mac_ctx_str(
      mac_key_context, mac_key_context + mac_key_context_length);
  const std::vector<uint8_t> enc_ctx_str(
      enc_key_context, enc_key_context + enc_key_context_length);

  // Generate mac and encryption keys for current session context
  if (!session_ctx->RSADeriveKeys(ssn_key_str, mac_ctx_str, enc_ctx_str)) {
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  return OEMCrypto_SUCCESS;
}

OEMCRYPTO_API uint32_t OEMCrypto_APIVersion() { return 15; }

OEMCRYPTO_API uint8_t OEMCrypto_Security_Patch_Level() {
  uint8_t security_patch_level = crypto_engine->config_security_patch_level();
  return security_patch_level;
}

OEMCRYPTO_API const char* OEMCrypto_SecurityLevel() {
  const char* security_level = crypto_engine->config_security_level();
  return security_level;
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_GetHDCPCapability(
    OEMCrypto_HDCP_Capability* current, OEMCrypto_HDCP_Capability* maximum) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_GetHDCPCapability: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (current == NULL) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  if (maximum == NULL) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  *current = crypto_engine->config_current_hdcp_capability();
  *maximum = crypto_engine->config_maximum_hdcp_capability();
  return OEMCrypto_SUCCESS;
}

OEMCRYPTO_API uint32_t OEMCrypto_GetAnalogOutputFlags() {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_GetAnalogOutputFlags: OEMCrypto Not Initialized.");
    return 0;
  }
  return crypto_engine->analog_output_flags();
}

OEMCRYPTO_API const char* OEMCrypto_BuildInformation() {
  return "OEMCrypto Ref Code " __DATE__ " " __TIME__;
}

OEMCRYPTO_API uint32_t OEMCrypto_ResourceRatingTier() {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_ResourceRatingTier: OEMCrypto Not Initialized.");
    return 0;
  }
  return crypto_engine->resource_rating();
}

OEMCRYPTO_API bool OEMCrypto_SupportsUsageTable() {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_SupportsUsageTable: OEMCrypto Not Initialized.");
    return 0;
  }
  bool supports_usage = crypto_engine->config_supports_usage_table();
  return supports_usage;
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_GetNumberOfOpenSessions(size_t* count) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_GetNumberOfOpenSessions: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (count == NULL) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  *count = crypto_engine->GetNumberOfOpenSessions();
  return OEMCrypto_SUCCESS;
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_GetMaxNumberOfSessions(
    size_t* maximum) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_GetMaxNumberOfSessions: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (maximum == NULL) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  *maximum = crypto_engine->GetMaxNumberOfSessions();
  return OEMCrypto_SUCCESS;
}

OEMCRYPTO_API bool OEMCrypto_IsAntiRollbackHwPresent() {
  bool anti_rollback_hw_present =
      crypto_engine->config_is_anti_rollback_hw_present();

  return anti_rollback_hw_present;
}

OEMCRYPTO_API uint32_t OEMCrypto_SupportedCertificates() {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_GetProvisioningMethod: OEMCrypto Not Initialized.");
    return 0;
  }
  if (crypto_engine->config_provisioning_method() == OEMCrypto_DrmCertificate) {
    return 0;
  }
  return OEMCrypto_Supports_RSA_2048bit | OEMCrypto_Supports_RSA_3072bit |
         OEMCrypto_Supports_RSA_CAST;
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_Generic_Encrypt(
    OEMCrypto_SESSION session, const uint8_t* in_buffer, size_t buffer_length,
    const uint8_t* iv, OEMCrypto_Algorithm algorithm, uint8_t* out_buffer) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_Generic_Encrypt: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (!crypto_engine->ValidRootOfTrust()) {
    LOGE("[OEMCrypto_Generic_Encrypt(): ERROR_KEYBOX_INVALID]");
    return OEMCrypto_ERROR_KEYBOX_INVALID;
  }
  SessionContext* session_ctx = crypto_engine->FindSession(session);
  if (!session_ctx || !session_ctx->isValid()) {
    LOGE("[OEMCrypto_Generic_Encrypt(): ERROR_INVALID_SESSION]");
    return OEMCrypto_ERROR_INVALID_SESSION;
  }
  if (in_buffer == NULL || buffer_length == 0 || iv == NULL ||
      out_buffer == NULL) {
    LOGE("[OEMCrypto_Generic_Encrypt(): OEMCrypto_ERROR_INVALID_CONTEXT]");
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }
  OEMCryptoResult sts = session_ctx->Generic_Encrypt(in_buffer, buffer_length,
                                                     iv, algorithm, out_buffer);
  return sts;
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_Generic_Decrypt(
    OEMCrypto_SESSION session, const uint8_t* in_buffer, size_t buffer_length,
    const uint8_t* iv, OEMCrypto_Algorithm algorithm, uint8_t* out_buffer) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_Generic_Decrypt: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (!crypto_engine->ValidRootOfTrust()) {
    LOGE("[OEMCrypto_Generic_Decrypt(): ERROR_KEYBOX_INVALID]");
    return OEMCrypto_ERROR_KEYBOX_INVALID;
  }
  SessionContext* session_ctx = crypto_engine->FindSession(session);
  if (!session_ctx || !session_ctx->isValid()) {
    LOGE("[OEMCrypto_Generic_Decrypt(): ERROR_INVALID_SESSION]");
    return OEMCrypto_ERROR_INVALID_SESSION;
  }
  if (in_buffer == NULL || buffer_length == 0 || iv == NULL ||
      out_buffer == NULL) {
    LOGE("[OEMCrypto_Generic_Decrypt(): OEMCrypto_ERROR_INVALID_CONTEXT]");
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }
  OEMCryptoResult sts = session_ctx->Generic_Decrypt(in_buffer, buffer_length,
                                                     iv, algorithm, out_buffer);
  return sts;
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_Generic_Sign(
    OEMCrypto_SESSION session, const uint8_t* in_buffer, size_t buffer_length,
    OEMCrypto_Algorithm algorithm, uint8_t* signature,
    size_t* signature_length) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_Generic_Sign: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (!crypto_engine->ValidRootOfTrust()) {
    LOGE("[OEMCrypto_Generic_Sign(): ERROR_KEYBOX_INVALID]");
    return OEMCrypto_ERROR_KEYBOX_INVALID;
  }
  SessionContext* session_ctx = crypto_engine->FindSession(session);
  if (!session_ctx || !session_ctx->isValid()) {
    LOGE("[OEMCrypto_Generic_Sign(): ERROR_INVALID_SESSION]");
    return OEMCrypto_ERROR_INVALID_SESSION;
  }
  if (*signature_length < SHA256_DIGEST_LENGTH) {
    *signature_length = SHA256_DIGEST_LENGTH;
    return OEMCrypto_ERROR_SHORT_BUFFER;
  }
  if (in_buffer == NULL || buffer_length == 0 || signature == NULL) {
    LOGE("[OEMCrypto_Generic_Sign(): OEMCrypto_ERROR_INVALID_CONTEXT]");
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }
  OEMCryptoResult sts = session_ctx->Generic_Sign(
      in_buffer, buffer_length, algorithm, signature, signature_length);
  return sts;
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_Generic_Verify(
    OEMCrypto_SESSION session, const uint8_t* in_buffer, size_t buffer_length,
    OEMCrypto_Algorithm algorithm, const uint8_t* signature,
    size_t signature_length) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_Generic_Verify: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (!crypto_engine->ValidRootOfTrust()) {
    LOGE("[OEMCrypto_Generic_Verify(): ERROR_KEYBOX_INVALID]");
    return OEMCrypto_ERROR_KEYBOX_INVALID;
  }
  SessionContext* session_ctx = crypto_engine->FindSession(session);
  if (!session_ctx || !session_ctx->isValid()) {
    LOGE("[OEMCrypto_Generic_Verify(): ERROR_INVALID_SESSION]");
    return OEMCrypto_ERROR_INVALID_SESSION;
  }
  if (signature_length != SHA256_DIGEST_LENGTH) {
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (in_buffer == NULL || buffer_length == 0 || signature == NULL) {
    LOGE("[OEMCrypto_Generic_Verify(): OEMCrypto_ERROR_INVALID_CONTEXT]");
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }
  return session_ctx->Generic_Verify(in_buffer, buffer_length, algorithm,
                                     signature, signature_length);
}

// TODO(fredgc): remove this.
OEMCRYPTO_API OEMCryptoResult OEMCrypto_UpdateUsageTable() {
  return OEMCrypto_ERROR_NOT_IMPLEMENTED;
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_DeactivateUsageEntry(
    OEMCrypto_SESSION session, const uint8_t* pst, size_t pst_length) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_DeactivateUsageEntry: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (!crypto_engine->config_supports_usage_table()) {
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  }
  SessionContext* session_ctx = crypto_engine->FindSession(session);
  if (!session_ctx || !session_ctx->isValid()) {
    LOGE("[OEMCrypto_DeactivateUsageEntry(): ERROR_INVALID_SESSION]");
    return OEMCrypto_ERROR_INVALID_SESSION;
  }
  std::vector<uint8_t> pstv(pst, pst + pst_length);
  return session_ctx->DeactivateUsageEntry(pstv);
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_ReportUsage(OEMCrypto_SESSION session,
                                                    const uint8_t* pst,
                                                    size_t pst_length,
                                                    uint8_t* buffer,
                                                    size_t* buffer_length) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_ReportUsage: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (!crypto_engine->config_supports_usage_table()) {
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  }
  if (!buffer_length) {
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }
  SessionContext* session_ctx = crypto_engine->FindSession(session);
  if (!session_ctx || !session_ctx->isValid()) {
    LOGE("[OEMCrypto_ReportUsage(): ERROR_INVALID_SESSION]");
    return OEMCrypto_ERROR_INVALID_SESSION;
  }
  std::vector<uint8_t> pstv(pst, pst + pst_length);
  OEMCryptoResult sts = session_ctx->ReportUsage(pstv, buffer, buffer_length);
  return sts;
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_DeleteUsageEntry(
    OEMCrypto_SESSION, const uint8_t*, size_t, const uint8_t*, size_t,
    const uint8_t*, size_t) {
  // TODO(fredgc): delete this.
  return OEMCrypto_ERROR_NOT_IMPLEMENTED;
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_ForceDeleteUsageEntry(const uint8_t*,
                                                              size_t) {
  // TODO(fredgc): delete this.
  return OEMCrypto_ERROR_NOT_IMPLEMENTED;
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_DeleteOldUsageTable() {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_DeleteOldUsageTable: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (!crypto_engine->config_supports_usage_table()) {
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  }
  return crypto_engine->usage_table().DeleteOldUsageTable();
}

OEMCRYPTO_API bool OEMCrypto_IsSRMUpdateSupported() {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_IsSRMUpdateSupported: OEMCrypto Not Initialized.");
    return false;
  }
  bool result = crypto_engine->srm_update_supported();
  return result;
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_GetCurrentSRMVersion(
    uint16_t* version) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_GetCurrentSRMVersion: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (crypto_engine->config_local_display_only()) {
    return OEMCrypto_LOCAL_DISPLAY_ONLY;
  }
  OEMCryptoResult result = crypto_engine->current_srm_version(version);
  return result;
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_LoadSRM(const uint8_t* buffer,
                                                size_t buffer_length) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_LoadSRM: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  return crypto_engine->load_srm(buffer, buffer_length);
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_RemoveSRM() {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_RemoveSRM: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  return crypto_engine->remove_srm();
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_CreateUsageTableHeader(
    uint8_t* header_buffer, size_t* header_buffer_length) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_CreateUsageTableHeader: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (!crypto_engine->config_supports_usage_table()) {
    LOGE("OEMCrypto_CreateUsageTableHeader: Configured without Usage Tables.");
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  }
  return crypto_engine->usage_table().CreateUsageTableHeader(
      header_buffer, header_buffer_length);
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_LoadUsageTableHeader(
    const uint8_t* buffer, size_t buffer_length) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_LoadUsageTableHeader: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (!crypto_engine->config_supports_usage_table()) {
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  }
  if (!buffer) {
    LOGE("OEMCrypto_LoadUsageTableHeader: buffer null.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  std::vector<uint8_t> bufferv(buffer, buffer + buffer_length);
  return crypto_engine->usage_table().LoadUsageTableHeader(bufferv);
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_CreateNewUsageEntry(
    OEMCrypto_SESSION session, uint32_t* usage_entry_number) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_CreateNewUsageEntry: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (!crypto_engine->config_supports_usage_table()) {
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  }
  SessionContext* session_ctx = crypto_engine->FindSession(session);
  if (!session_ctx || !session_ctx->isValid()) {
    LOGE("[OEMCrypto_CreateNewUsageEntry(): ERROR_INVALID_SESSION]");
    return OEMCrypto_ERROR_INVALID_SESSION;
  }
  if (!usage_entry_number) {
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  OEMCryptoResult sts = session_ctx->CreateNewUsageEntry(usage_entry_number);
  return sts;
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_LoadUsageEntry(
    OEMCrypto_SESSION session, uint32_t index, const uint8_t* buffer,
    size_t buffer_size) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_LoadUsageEntry: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (!crypto_engine->config_supports_usage_table()) {
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  }
  SessionContext* session_ctx = crypto_engine->FindSession(session);
  if (!session_ctx || !session_ctx->isValid()) {
    LOGE("[OEMCrypto_LoadUsageEntry(): ERROR_INVALID_SESSION]");
    return OEMCrypto_ERROR_INVALID_SESSION;
  }
  if (!buffer) {
    LOGE("[OEMCrypto_LoadUsageEntry(): buffer null]");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  std::vector<uint8_t> bufferv(buffer, buffer + buffer_size);
  return session_ctx->LoadUsageEntry(index, bufferv);
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_UpdateUsageEntry(
    OEMCrypto_SESSION session, uint8_t* header_buffer,
    size_t* header_buffer_length, uint8_t* entry_buffer,
    size_t* entry_buffer_length) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_UpdateUsageEntry: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (!crypto_engine->config_supports_usage_table()) {
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  }
  if (!header_buffer_length || !entry_buffer_length) {
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }
  SessionContext* session_ctx = crypto_engine->FindSession(session);
  if (!session_ctx || !session_ctx->isValid()) {
    LOGE("[OEMCrypto_UpdateUsageEntry(): ERROR_INVALID_SESSION]");
    return OEMCrypto_ERROR_INVALID_SESSION;
  }
  return session_ctx->UpdateUsageEntry(header_buffer, header_buffer_length,
                                       entry_buffer, entry_buffer_length);
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_ShrinkUsageTableHeader(
    uint32_t new_table_size, uint8_t* header_buffer,
    size_t* header_buffer_length) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_ShrinkUsageTableHeader: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (!crypto_engine->config_supports_usage_table()) {
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  }
  return crypto_engine->usage_table().ShrinkUsageTableHeader(
      new_table_size, header_buffer, header_buffer_length);
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_MoveEntry(OEMCrypto_SESSION session,
                                                  uint32_t new_index) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_MoveEntry: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (!crypto_engine->config_supports_usage_table()) {
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  }
  SessionContext* session_ctx = crypto_engine->FindSession(session);
  if (!session_ctx || !session_ctx->isValid()) {
    LOGE("[OEMCrypto_MoveEntry(): ERROR_INVALID_SESSION]");
    return OEMCrypto_ERROR_INVALID_SESSION;
  }
  return session_ctx->MoveEntry(new_index);
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_CopyOldUsageEntry(
    OEMCrypto_SESSION session, const uint8_t* pst, size_t pst_length) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_CopyOldUsageEntry: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (!crypto_engine->config_supports_usage_table()) {
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  }
  SessionContext* session_ctx = crypto_engine->FindSession(session);
  if (!session_ctx || !session_ctx->isValid()) {
    LOGE("[OEMCrypto_CopyOldUsageEntry(): ERROR_INVALID_SESSION]");
    return OEMCrypto_ERROR_INVALID_SESSION;
  }
  std::vector<uint8_t> pstv(pst, pst + pst_length);
  return session_ctx->CopyOldUsageEntry(pstv);
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_CreateOldUsageEntry(
    uint64_t time_since_license_received, uint64_t time_since_first_decrypt,
    uint64_t time_since_last_decrypt, OEMCrypto_Usage_Entry_Status status,
    uint8_t* server_mac_key, uint8_t* client_mac_key, const uint8_t* pst,
    size_t pst_length) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_CreateOldUsageEntry: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (!crypto_engine->config_supports_usage_table()) {
    return OEMCrypto_ERROR_NOT_IMPLEMENTED;
  }
  return crypto_engine->usage_table().CreateOldUsageEntry(
      time_since_license_received, time_since_first_decrypt,
      time_since_last_decrypt, status, server_mac_key, client_mac_key, pst,
      pst_length);
}

OEMCRYPTO_API uint32_t OEMCrypto_SupportsDecryptHash() {
  return OEMCrypto_CRC_Clear_Buffer;
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_SetDecryptHash(
    OEMCrypto_SESSION session, uint32_t frame_number, const uint8_t* hash,
    size_t hash_length) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_SetDecryptHash: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  SessionContext* session_ctx = crypto_engine->FindSession(session);
  if (!session_ctx || !session_ctx->isValid()) {
    LOGE("[OEMCrypto_SetDecryptHash(): ERROR_INVALID_SESSION]");
    return OEMCrypto_ERROR_INVALID_SESSION;
  }
  return session_ctx->SetDecryptHash(frame_number, hash, hash_length);
}

OEMCRYPTO_API OEMCryptoResult OEMCrypto_GetHashErrorCode(
    OEMCrypto_SESSION session, uint32_t* failed_frame_number) {
  if (crypto_engine == nullptr) {
    LOGE("OEMCrypto_GetHashErrorCode: OEMCrypto Not Initialized.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  SessionContext* session_ctx = crypto_engine->FindSession(session);
  if (!session_ctx || !session_ctx->isValid()) {
    LOGE("[OEMCrypto_GetHashErrorCode(): ERROR_INVALID_SESSION]");
    return OEMCrypto_ERROR_INVALID_SESSION;
  }
  return session_ctx->GetHashErrorCode(failed_frame_number);
}

}  // namespace wvoec_ref
