// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
// OEMCrypto unit tests
//

#include "oec_session_util.h"

#include <openssl/aes.h>
#include <openssl/bio.h>
#include <openssl/cmac.h>
#include <openssl/err.h>
#include <openssl/hmac.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#include <stdint.h>

#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <string>
#include <sstream>
#include <vector>

#include "OEMCryptoCENC.h"
#include "disallow_copy_and_assign.h"
#include "log.h"
#include "oec_device_features.h"
#include "oec_test_data.h"
#include "oemcrypto_types.h"
#include "platform.h"
#include "string_conversions.h"

using namespace std;

// GTest requires PrintTo to be in the same namespace as the thing it prints,
// which is std::vector in this case.
namespace std {
void PrintTo(const vector<uint8_t>& value, ostream* os) {
  *os << wvcdm::b2a_hex(value);
}
}  // namespace std

namespace {
int GetRandBytes(unsigned char* buf, int num) {
  // returns 1 on success, -1 if not supported, or 0 if other failure.
  return RAND_bytes(buf, num);
}

void DeleteX509Stack(STACK_OF(X509)* stack) {
  sk_X509_pop_free(stack, X509_free);
}

}  // namespace

namespace wvoec {

// Increment counter for AES-CTR.  The CENC spec specifies we increment only
// the low 64 bits of the IV counter, and leave the high 64 bits alone.  This
// is different from the BoringSSL implementation, so we implement the CTR loop
// ourselves.
void ctr128_inc64(int64_t increaseBy, uint8_t* iv) {
  ASSERT_NE(static_cast<void*>(NULL), iv);
  uint64_t* counterBuffer = reinterpret_cast<uint64_t*>(&iv[8]);
  (*counterBuffer) =
      wvcdm::htonll64(wvcdm::ntohll64(*counterBuffer) + increaseBy);
}

// Some compilers don't like the macro htonl within an ASSERT_EQ.
uint32_t htonl_fnc(uint32_t x) { return htonl(x); }

void dump_boringssl_error() {
  while (unsigned long err = ERR_get_error()) {
    char buffer[120];
    ERR_error_string_n(err, buffer, sizeof(buffer));
    cout << "BoringSSL Error -- " << buffer << "\n";
  }
}

// A smart pointer for BoringSSL objects.  It uses the specified free function
// to release resources and free memory when the pointer is deleted.
template <typename T, void (*func)(T*)>
class boringssl_ptr {
 public:
  explicit boringssl_ptr(T* p = NULL) : ptr_(p) {}
  ~boringssl_ptr() {
    if (ptr_) func(ptr_);
  }
  T& operator*() const { return *ptr_; }
  T* operator->() const { return ptr_; }
  T* get() const { return ptr_; }
  bool NotNull() const { return ptr_; }

 private:
  T* ptr_;
  CORE_DISALLOW_COPY_AND_ASSIGN(boringssl_ptr);
};

OEMCrypto_Substring GetSubstring(const std::string& message,
                                 const std::string& field, bool set_zero) {
  OEMCrypto_Substring substring;
  if (set_zero || field.empty() || message.empty()) {
    substring.offset = 0;
    substring.length = 0;
  } else {
    size_t pos = message.find(field);
    if (pos == std::string::npos) {
      LOGW("GetSubstring : Cannot find offset for %s", field.c_str());
      substring.offset = 0;
      substring.length = 0;
    } else {
      substring.offset = pos;
      substring.length = field.length();
    }
  }
  return substring;
}

Session::Session()
    : open_(false),
      forced_session_id_(false),
      session_id_(0),
      mac_key_server_(MAC_KEY_SIZE),
      mac_key_client_(MAC_KEY_SIZE),
      enc_key_(KEY_SIZE),
      public_rsa_(0),
      message_size_(sizeof(MessageData)),
      // Most tests only use 4 keys. Other tests will explicitly call
      // set_num_keys.
      num_keys_(4) {
  // Stripe the padded message.
  for (size_t i = 0; i < sizeof(padded_message_.padding); i++) {
    padded_message_.padding[i] = i % 0x100;
  }
}

Session::~Session() {
  if (!forced_session_id_ && open_) close();
  if (public_rsa_) RSA_free(public_rsa_);
}

void Session::open() {
  EXPECT_FALSE(forced_session_id_);
  EXPECT_FALSE(open_);
  ASSERT_EQ(OEMCrypto_SUCCESS, OEMCrypto_OpenSession(&session_id_));
  open_ = true;
}

void Session::SetSessionId(uint32_t session_id) {
  EXPECT_FALSE(open_);
  session_id_ = session_id;
  forced_session_id_ = true;
}

void Session::close() {
  EXPECT_TRUE(open_ || forced_session_id_);
  if (open_) {
    ASSERT_EQ(OEMCrypto_SUCCESS, OEMCrypto_CloseSession(session_id_));
  }
  forced_session_id_ = false;
  open_ = false;
}

void Session::GenerateNonce(int* error_counter) {
  // We make one attempt.  If it fails, we assume there was a nonce flood.
  if (OEMCrypto_SUCCESS == OEMCrypto_GenerateNonce(session_id(), &nonce_)) {
    return;
  }
  if (error_counter) {
    (*error_counter)++;
  } else {
    sleep(1);  // wait a second, then try again.
    // The following is after a 1 second pause, so it cannot be from a nonce
    // flood.
    ASSERT_EQ(OEMCrypto_SUCCESS,
              OEMCrypto_GenerateNonce(session_id(), &nonce_));
  }
}

void Session::FillDefaultContext(vector<uint8_t>* mac_context,
                                 vector<uint8_t>* enc_context) {
  /* Context strings
   * These context strings are normally created by the CDM layer
   * from a license request message.
   * They are used to test MAC and ENC key generation.
   */
  *mac_context = wvcdm::a2b_hex(
      "41555448454e5449434154494f4e000a4c08001248000000020000101907d9ff"
      "de13aa95c122678053362136bdf8408f8276e4c2d87ec52b61aa1b9f646e5873"
      "4930acebe899b3e464189a14a87202fb02574e70640bd22ef44b2d7e3912250a"
      "230a14080112100915007caa9b5931b76a3a85f046523e10011a093938373635"
      "34333231180120002a0c31383836373837343035000000000200");
  *enc_context = wvcdm::a2b_hex(
      "454e4352595054494f4e000a4c08001248000000020000101907d9ffde13aa95"
      "c122678053362136bdf8408f8276e4c2d87ec52b61aa1b9f646e58734930aceb"
      "e899b3e464189a14a87202fb02574e70640bd22ef44b2d7e3912250a230a1408"
      "0112100915007caa9b5931b76a3a85f046523e10011a09393837363534333231"
      "180120002a0c31383836373837343035000000000080");
}

// This generates the truth data for deriving one key.  If there are failures in
// this function, then there is something wrong with the test program and its
// dependency on BoringSSL.
void Session::DeriveKey(const uint8_t* key, const vector<uint8_t>& context,
                        int counter, vector<uint8_t>* out) {
  ASSERT_FALSE(context.empty());
  ASSERT_GE(4, counter);
  ASSERT_NE(static_cast<void*>(NULL), out);

  const EVP_CIPHER* cipher = EVP_aes_128_cbc();
  CMAC_CTX* cmac_ctx = CMAC_CTX_new();
  ASSERT_NE(static_cast<void*>(NULL), cmac_ctx);

  ASSERT_EQ(1, CMAC_Init(cmac_ctx, key, KEY_SIZE, cipher, 0));

  std::vector<uint8_t> message;
  message.push_back(counter);
  message.insert(message.end(), context.begin(), context.end());

  ASSERT_EQ(1, CMAC_Update(cmac_ctx, message.data(), message.size()));

  size_t reslen;
  uint8_t res[128];
  ASSERT_EQ(1, CMAC_Final(cmac_ctx, res, &reslen));

  out->assign(res, res + reslen);
  CMAC_CTX_free(cmac_ctx);
}

// This generates the truth data for deriving a set of keys.  If there are
// failures in this function, then there is something wrong with the test
// program and its dependency on BoringSSL.
void Session::DeriveKeys(const uint8_t* master_key,
                         const vector<uint8_t>& mac_key_context,
                         const vector<uint8_t>& enc_key_context) {
  // Generate derived key for mac key
  std::vector<uint8_t> mac_key_part2;
  DeriveKey(master_key, mac_key_context, 1, &mac_key_server_);
  DeriveKey(master_key, mac_key_context, 2, &mac_key_part2);
  mac_key_server_.insert(mac_key_server_.end(), mac_key_part2.begin(),
                         mac_key_part2.end());

  DeriveKey(master_key, mac_key_context, 3, &mac_key_client_);
  DeriveKey(master_key, mac_key_context, 4, &mac_key_part2);
  mac_key_client_.insert(mac_key_client_.end(), mac_key_part2.begin(),
                         mac_key_part2.end());

  // Generate derived key for encryption key
  DeriveKey(master_key, enc_key_context, 1, &enc_key_);
}

// This should only be called if the device uses Provisioning 2.0. A failure in
// this function is probably caused by a bad keybox.
void Session::GenerateDerivedKeysFromKeybox(
    const wvoec::WidevineKeybox& keybox) {
  GenerateNonce();
  vector<uint8_t> mac_context;
  vector<uint8_t> enc_context;
  FillDefaultContext(&mac_context, &enc_context);
  ASSERT_EQ(OEMCrypto_SUCCESS,
            OEMCrypto_GenerateDerivedKeys(
                session_id(), mac_context.data(), mac_context.size(),
                enc_context.data(), enc_context.size()));

  DeriveKeys(keybox.device_key_, mac_context, enc_context);
}

void Session::GenerateDerivedKeysFromSessionKey() {
  // Uses test certificate.
  GenerateNonce();
  vector<uint8_t> session_key;
  vector<uint8_t> enc_session_key;
  if (public_rsa_ == NULL) PreparePublicKey();
  // A failure here probably indicates that there is something wrong with the
  // test program and its dependency on BoringSSL.
  ASSERT_TRUE(GenerateRSASessionKey(&session_key, &enc_session_key));
  vector<uint8_t> mac_context;
  vector<uint8_t> enc_context;
  FillDefaultContext(&mac_context, &enc_context);
  // A failure here is probably caused by having the wrong RSA key loaded.
  ASSERT_EQ(OEMCrypto_SUCCESS,
            OEMCrypto_DeriveKeysFromSessionKey(
                session_id(), enc_session_key.data(), enc_session_key.size(),
                mac_context.data(), mac_context.size(), enc_context.data(),
                enc_context.size()));

  DeriveKeys(session_key.data(), mac_context, enc_context);
}

void Session::LoadTestKeys(const std::string& provider_session_token,
                           bool new_mac_keys) {
  std::string message =
      wvcdm::BytesToString(message_ptr(), sizeof(MessageData));
  OEMCrypto_Substring pst = GetSubstring(message, provider_session_token);
  OEMCrypto_Substring enc_mac_keys_iv = GetSubstring(
      message, wvcdm::BytesToString(encrypted_license().mac_key_iv,
                                    sizeof(encrypted_license().mac_key_iv)));
  OEMCrypto_Substring enc_mac_keys = GetSubstring(
      message, wvcdm::BytesToString(encrypted_license().mac_keys,
                                    sizeof(encrypted_license().mac_keys)));
  if (new_mac_keys) {
    ASSERT_EQ(OEMCrypto_SUCCESS,
              OEMCrypto_LoadKeys(
                  session_id(), message_ptr(), message_size_, signature_.data(),
                  signature_.size(), enc_mac_keys_iv, enc_mac_keys, num_keys_,
                  key_array_, pst, GetSubstring(), OEMCrypto_ContentLicense));
    // Update new generated keys.
    memcpy(mac_key_server_.data(), license_.mac_keys, MAC_KEY_SIZE);
    memcpy(mac_key_client_.data(), license_.mac_keys + MAC_KEY_SIZE,
           MAC_KEY_SIZE);
  } else {
    ASSERT_EQ(OEMCrypto_SUCCESS,
              OEMCrypto_LoadKeys(
                  session_id(), message_ptr(), message_size_, signature_.data(),
                  signature_.size(), GetSubstring(), GetSubstring(), num_keys_,
                  key_array_, pst, GetSubstring(), OEMCrypto_ContentLicense));
  }
  VerifyTestKeys();
}

void Session::LoadEntitlementTestKeys(const std::string& provider_session_token,
                                      bool new_mac_keys,
                                      OEMCryptoResult expected_sts) {
  std::string message =
      wvcdm::BytesToString(message_ptr(), sizeof(MessageData));
  OEMCrypto_Substring pst = GetSubstring(message, provider_session_token);
  OEMCrypto_Substring enc_mac_keys_iv = GetSubstring(
      message, wvcdm::BytesToString(encrypted_license().mac_key_iv,
                                    sizeof(encrypted_license().mac_key_iv)));
  OEMCrypto_Substring enc_mac_keys = GetSubstring(
      message, wvcdm::BytesToString(encrypted_license().mac_keys,
                                    sizeof(encrypted_license().mac_keys)));
  if (new_mac_keys) {
    ASSERT_EQ(
        expected_sts,
        OEMCrypto_LoadKeys(session_id(), message_ptr(), message_size_,
                           signature_.data(), signature_.size(),
                           enc_mac_keys_iv, enc_mac_keys, num_keys_,
                           key_array_, pst, GetSubstring(),
                           OEMCrypto_EntitlementLicense));
    // Update new generated keys.
    memcpy(mac_key_server_.data(), license_.mac_keys, MAC_KEY_SIZE);
    memcpy(mac_key_client_.data(), license_.mac_keys + MAC_KEY_SIZE,
           MAC_KEY_SIZE);
  } else {
    ASSERT_EQ(
        expected_sts,
        OEMCrypto_LoadKeys(session_id(), message_ptr(), message_size_,
                           signature_.data(), signature_.size(), GetSubstring(),
                           GetSubstring(), num_keys_, key_array_, pst,
                           GetSubstring(), OEMCrypto_EntitlementLicense));
  }
}

void Session::FillEntitledKeyArray() {
  int offset = 0;
  entitled_message_.clear();
  for (size_t i = 0; i < num_keys_; ++i) {
    EntitledContentKeyData* key_data = &entitled_key_data_[i];

    entitled_key_array_[i].entitlement_key_id.offset = offset;
    entitled_key_array_[i].entitlement_key_id.length =
        key_array_[i].key_id.length;
    offset += key_array_[i].key_id.length;
    entitled_message_ +=
        wvcdm::BytesToString(message_ptr() + key_array_[i].key_id.offset,
                             key_array_[i].key_id.length);

    EXPECT_EQ(1, GetRandBytes(key_data->content_key_id,
                              sizeof(key_data->content_key_id)));
    entitled_key_array_[i].content_key_id.offset = offset;
    entitled_key_array_[i].content_key_id.length =
        sizeof(key_data->content_key_id);
    offset += sizeof(key_data->content_key_id);
    entitled_message_ += wvcdm::BytesToString(key_data->content_key_id,
                                              sizeof(key_data->content_key_id));

    EXPECT_EQ(1, GetRandBytes(key_data->content_key_data,
                              sizeof(key_data->content_key_data)));
    entitled_key_array_[i].content_key_data.offset = offset;
    entitled_key_array_[i].content_key_data.length =
        sizeof(key_data->content_key_data);
    offset += sizeof(key_data->content_key_data);
    entitled_message_ += wvcdm::BytesToString(
        key_data->content_key_data, sizeof(key_data->content_key_data));

    EXPECT_EQ(1, GetRandBytes(key_data[i].content_key_data_iv,
                              sizeof(key_data[i].content_key_data_iv)));
    entitled_key_array_[i].content_key_data_iv.offset = offset;
    entitled_key_array_[i].content_key_data_iv.length =
        sizeof(key_data->content_key_data_iv);
    offset += sizeof(key_data->content_key_data_iv);
    entitled_message_ += wvcdm::BytesToString(
        key_data->content_key_data_iv, sizeof(key_data->content_key_data_iv));
  }
}

void Session::LoadEntitledContentKeys(OEMCryptoResult expected_sts) {
  encrypted_entitled_message_ = entitled_message_;
  std::vector<OEMCrypto_EntitledContentKeyObject> encrypted_entitled_key_array(
    entitled_key_array_, entitled_key_array_ + num_keys_);

  for (size_t i = 0; i < num_keys_; ++i) {
    // Load the entitlement key from |key_array_|.
    AES_KEY aes_key;
    AES_set_encrypt_key(message_ptr() + key_array_[i].key_data.offset, 256,
                        &aes_key);

    // Encrypt the content key with the entitlement key.
    uint8_t iv[16];
    const uint8_t* content_key_data = reinterpret_cast<const uint8_t*>(
        entitled_message_.data() +
        entitled_key_array_[i].content_key_data.offset);
    const uint8_t* encrypted_content_key_data =
        reinterpret_cast<const uint8_t*>(
            encrypted_entitled_message_.data() +
            encrypted_entitled_key_array[i].content_key_data.offset);
    memcpy(&iv[0],
           message_ptr() +
               encrypted_entitled_key_array[i].content_key_data_iv.offset,
           16);
    AES_cbc_encrypt(content_key_data,
                    const_cast<uint8_t*>(encrypted_content_key_data),
                    encrypted_entitled_key_array[i].content_key_data.length,
                    &aes_key, iv, AES_ENCRYPT);
  }
  ASSERT_EQ(
      expected_sts,
      OEMCrypto_LoadEntitledContentKeys(
          session_id(),
          reinterpret_cast<const uint8_t*>(encrypted_entitled_message_.data()),
          encrypted_entitled_message_.size(), num_keys_,
          encrypted_entitled_key_array.data()));
  if (expected_sts != OEMCrypto_SUCCESS) {
    return;
  }
  VerifyEntitlementTestKeys();
}

// This function verifies that the key control block reported by OEMCrypto agree
// with the truth key control block.  Failures in this function probably
// indicate the OEMCrypto_LoadKeys did not correctly process the key control
// block.
void Session::VerifyTestKeys() {
  for (unsigned int i = 0; i < num_keys_; i++) {
    KeyControlBlock block;
    size_t size = sizeof(block);
    OEMCryptoResult sts = OEMCrypto_QueryKeyControl(
        session_id(), license_.keys[i].key_id, license_.keys[i].key_id_length,
        reinterpret_cast<uint8_t*>(&block), &size);
    if (sts != OEMCrypto_ERROR_NOT_IMPLEMENTED) {
      ASSERT_EQ(OEMCrypto_SUCCESS, sts);
      ASSERT_EQ(sizeof(block), size);
      // control duration and bits stored in network byte order. For printing
      // we change to host byte order.
      ASSERT_EQ((htonl_fnc(license_.keys[i].control.duration)),
                (htonl_fnc(block.duration)))
          << "For key " << i;
      ASSERT_EQ(htonl_fnc(license_.keys[i].control.control_bits),
                htonl_fnc(block.control_bits))
          << "For key " << i;
    }
  }
}

// This function verifies that the key control block reported by OEMCrypto agree
// with the truth key control block.  Failures in this function probably
// indicate the OEMCrypto_LoadEntitledKeys did not correctly process the key
// control block.
void Session::VerifyEntitlementTestKeys() {
  for (unsigned int i = 0; i < num_keys_; i++) {
    KeyControlBlock block;
    size_t size = sizeof(block);
    const uint8_t* content_key_id =
        reinterpret_cast<const uint8_t*>(entitled_message_.data());
    OEMCryptoResult sts = OEMCrypto_QueryKeyControl(
        session_id(),
        content_key_id + entitled_key_array_[i].content_key_id.offset,
        entitled_key_array_[i].content_key_id.length,
        reinterpret_cast<uint8_t*>(&block), &size);
    if (sts != OEMCrypto_ERROR_NOT_IMPLEMENTED) {
      ASSERT_EQ(OEMCrypto_SUCCESS, sts);
      ASSERT_EQ(sizeof(block), size);
      // control duration and bits stored in network byte order. For printing
      // we change to host byte order.
      ASSERT_EQ((htonl_fnc(license_.keys[i].control.duration)),
                (htonl_fnc(block.duration)))
          << "For key " << i;
      ASSERT_EQ(htonl_fnc(license_.keys[i].control.control_bits),
                htonl_fnc(block.control_bits))
          << "For key " << i;
    }
  }
}

void Session::RefreshTestKeys(const size_t key_count, uint32_t control_bits,
                              uint32_t nonce, OEMCryptoResult expected_result) {
  // Note: we store the message in encrypted_license_, but the refresh key
  // message is not actually encrypted.  It is, however, signed.
  // FillRefreshMessage fills the message with a duration of kLongDuration.
  FillRefreshMessage(key_count, control_bits, nonce);
  ServerSignBuffer(reinterpret_cast<const uint8_t*>(&padded_message_),
                   message_size_, &signature_);
  std::vector<OEMCrypto_KeyRefreshObject> key_array(key_count);
  FillRefreshArray(key_array.data(), key_count);
  OEMCryptoResult sts = OEMCrypto_RefreshKeys(
      session_id(), message_ptr(), message_size_, signature_.data(),
      signature_.size(), key_count, key_array.data());
  ASSERT_EQ(expected_result, sts);

  ASSERT_NO_FATAL_FAILURE(TestDecryptCTR());
  //  This should still be valid key, even if the refresh failed, because this
  //  is before the original license duration.
  sleep(kShortSleep);
  ASSERT_NO_FATAL_FAILURE(TestDecryptCTR(false));
  // This should be after duration of the original license, but before the
  // expiration of the refresh message.  This should succeed if and only if the
  // refresh succeeded.
  sleep(kShortSleep + kLongSleep);
  if (expected_result == OEMCrypto_SUCCESS) {
    ASSERT_NO_FATAL_FAILURE(TestDecryptCTR(false, OEMCrypto_SUCCESS));
  } else {
    ASSERT_NO_FATAL_FAILURE(
        TestDecryptCTR(false, OEMCrypto_ERROR_UNKNOWN_FAILURE));
  }
}

void Session::SetKeyId(int index, const string& key_id) {
  MessageKeyData& key = license_.keys[index];
  key.key_id_length = key_id.length();
  ASSERT_LE(key.key_id_length, kTestKeyIdMaxLength);
  memcpy(key.key_id, key_id.data(), key.key_id_length);
}

void Session::FillSimpleMessage(uint32_t duration, uint32_t control,
                                uint32_t nonce, const std::string& pst) {
  EXPECT_EQ(
      1, GetRandBytes(license_.mac_key_iv, sizeof(license_.mac_key_iv)));
  memset(license_.padding, 0, sizeof(license_.padding));
  EXPECT_EQ(1, GetRandBytes(license_.mac_keys, sizeof(license_.mac_keys)));
  for (unsigned int i = 0; i < num_keys_; i++) {
    memset(license_.keys[i].key_id, 0, kTestKeyIdMaxLength);
    license_.keys[i].key_id_length = kDefaultKeyIdLength;
    memset(license_.keys[i].key_id, i, license_.keys[i].key_id_length);
    EXPECT_EQ(1, GetRandBytes(license_.keys[i].key_data,
                                   sizeof(license_.keys[i].key_data)));
    license_.keys[i].key_data_length = KEY_SIZE;
    EXPECT_EQ(1, GetRandBytes(license_.keys[i].key_iv,
                                   sizeof(license_.keys[i].key_iv)));
    EXPECT_EQ(1, GetRandBytes(license_.keys[i].control_iv,
                                   sizeof(license_.keys[i].control_iv)));
    if (global_features.api_version >= 12) {
      // For version 12 and above, we require OEMCrypto to handle kcNN for all
      // licenses.
      std::stringstream stream;
      stream << "kc" << global_features.api_version;
      memcpy(license_.keys[i].control.verification, stream.str().c_str(), 4);
    } else if (control & wvoec::kControlSecurityPatchLevelMask) {
      // For versions before 12, we require the special key control block only
      // when there are newer features present.
      memcpy(license_.keys[i].control.verification, "kc11", 4);
    } else if (control & wvoec::kControlRequireAntiRollbackHardware) {
      memcpy(license_.keys[i].control.verification, "kc10", 4);
    } else if (control & (wvoec::kControlHDCPVersionMask |
                          wvoec::kControlReplayMask)) {
      memcpy(license_.keys[i].control.verification, "kc09", 4);
    } else {
      memcpy(license_.keys[i].control.verification, "kctl", 4);
    }
    license_.keys[i].control.duration = htonl(duration);
    license_.keys[i].control.nonce = htonl(nonce);
    license_.keys[i].control.control_bits = htonl(control);
    license_.keys[i].cipher_mode = OEMCrypto_CipherMode_CTR;
  }
  memcpy(license_.pst, pst.c_str(), min(sizeof(license_.pst), pst.length()));
  pst_ = pst;
}

void Session::FillSimpleEntitlementMessage(
    uint32_t duration, uint32_t control, uint32_t nonce,
    const std::string& pst) {
  EXPECT_EQ(
      1, GetRandBytes(license_.mac_key_iv, sizeof(license_.mac_key_iv)));
  EXPECT_EQ(1, GetRandBytes(license_.mac_keys, sizeof(license_.mac_keys)));
  for (unsigned int i = 0; i < num_keys_; i++) {
    memset(license_.keys[i].key_id, 0, kTestKeyIdMaxLength);
    license_.keys[i].key_id_length = kDefaultKeyIdLength;
    memset(license_.keys[i].key_id, i, license_.keys[i].key_id_length);
    EXPECT_EQ(1, GetRandBytes(license_.keys[i].key_data,
                                   sizeof(license_.keys[i].key_data)));
    license_.keys[i].key_data_length = KEY_SIZE * 2; // AES-256 keys
    EXPECT_EQ(1, GetRandBytes(license_.keys[i].key_iv,
                                   sizeof(license_.keys[i].key_iv)));
    EXPECT_EQ(1, GetRandBytes(license_.keys[i].control_iv,
                                   sizeof(license_.keys[i].control_iv)));
    if (global_features.api_version >= 12) {
      // For version 12 and above, we require OEMCrypto to handle kcNN for all
      // licenses.
      std::stringstream stream;
      stream << "kc" << global_features.api_version;
      memcpy(license_.keys[i].control.verification, stream.str().c_str(), 4);
    } else if (control & wvoec::kControlSecurityPatchLevelMask) {
      // For versions before 12, we require the special key control block only
      // when there are newer features present.
      memcpy(license_.keys[i].control.verification, "kc11", 4);
    } else if (control & wvoec::kControlRequireAntiRollbackHardware) {
      memcpy(license_.keys[i].control.verification, "kc10", 4);
    } else if (control & (wvoec::kControlHDCPVersionMask |
                          wvoec::kControlReplayMask)) {
      memcpy(license_.keys[i].control.verification, "kc09", 4);
    } else {
      memcpy(license_.keys[i].control.verification, "kctl", 4);
    }
    license_.keys[i].control.duration = htonl(duration);
    license_.keys[i].control.nonce = htonl(nonce);
    license_.keys[i].control.control_bits = htonl(control);
    license_.keys[i].cipher_mode = OEMCrypto_CipherMode_CTR;
  }
  memcpy(license_.pst, pst.c_str(), min(sizeof(license_.pst), pst.length()));
  pst_ = pst;
}

void Session::FillRefreshMessage(size_t key_count, uint32_t control_bits,
                                 uint32_t nonce) {
  for (unsigned int i = 0; i < key_count; i++) {
    encrypted_license().keys[i].key_id_length = license_.keys[i].key_id_length;
    memcpy(encrypted_license().keys[i].key_id, license_.keys[i].key_id,
           encrypted_license().keys[i].key_id_length);
    if (global_features.api_version >= 12) {
      // For version 12 and above, we require OEMCrypto to handle kcNN for all
      // licenses.
      std::stringstream stream;
      stream << "kc" << global_features.api_version;
      memcpy(encrypted_license().keys[i].control.verification,
             stream.str().c_str(), 4);
    } else {
      // For versions before 12, we require the special key control block only
      // when there are newer features present.
      memcpy(encrypted_license().keys[i].control.verification, "kctl", 4);
    }
    encrypted_license().keys[i].control.duration = htonl(kLongDuration);
    encrypted_license().keys[i].control.nonce = htonl(nonce);
    encrypted_license().keys[i].control.control_bits = htonl(control_bits);
  }
}

void Session::SetLoadKeysSubstringParams() {
  load_keys_params_.resize(4);
  std::string message =
      wvcdm::BytesToString(message_ptr(), sizeof(MessageData));
  OEMCrypto_Substring* enc_mac_keys_iv = load_keys_params_.data();
  *enc_mac_keys_iv = GetSubstring(
      message, wvcdm::BytesToString(encrypted_license().mac_key_iv,
                                    sizeof(encrypted_license().mac_key_iv)));
  OEMCrypto_Substring* enc_mac_keys = &load_keys_params_[1];
  *enc_mac_keys = GetSubstring(
      message, wvcdm::BytesToString(encrypted_license().mac_keys,
                                    sizeof(encrypted_license().mac_keys)));
  OEMCrypto_Substring* pst = &load_keys_params_[2];
  size_t pst_length =
      strlen(reinterpret_cast<const char*>(encrypted_license().pst));
  *pst = GetSubstring(
      message, wvcdm::BytesToString(encrypted_license().pst, pst_length));
  OEMCrypto_Substring* srm_req = &load_keys_params_[3];
  *srm_req = GetSubstring();
}

void Session::EncryptAndSign() {
  encrypted_license() = license_;

  uint8_t iv_buffer[16];
  memcpy(iv_buffer, &license_.mac_key_iv[0], KEY_IV_SIZE);
  AES_KEY aes_key;
  AES_set_encrypt_key(enc_key_.data(), 128, &aes_key);
  AES_cbc_encrypt(&license_.mac_keys[0], &encrypted_license().mac_keys[0],
                  2 * MAC_KEY_SIZE, &aes_key, iv_buffer, AES_ENCRYPT);

  for (unsigned int i = 0; i < num_keys_; i++) {
    memcpy(iv_buffer, &license_.keys[i].control_iv[0], KEY_IV_SIZE);
    AES_set_encrypt_key(&license_.keys[i].key_data[0], 128, &aes_key);
    AES_cbc_encrypt(
        reinterpret_cast<const uint8_t*>(&license_.keys[i].control),
        reinterpret_cast<uint8_t*>(&encrypted_license().keys[i].control),
        KEY_SIZE, &aes_key, iv_buffer, AES_ENCRYPT);

    memcpy(iv_buffer, &license_.keys[i].key_iv[0], KEY_IV_SIZE);
    AES_set_encrypt_key(enc_key_.data(), 128, &aes_key);
    AES_cbc_encrypt(
        &license_.keys[i].key_data[0], &encrypted_license().keys[i].key_data[0],
        license_.keys[i].key_data_length, &aes_key, iv_buffer, AES_ENCRYPT);
  }
  memcpy(encrypted_license().pst, license_.pst, sizeof(license_.pst));
  ServerSignBuffer(reinterpret_cast<const uint8_t*>(&padded_message_),
                   message_size_, &signature_);
  FillKeyArray(encrypted_license(), key_array_);
  SetLoadKeysSubstringParams();
}

void Session::EncryptProvisioningMessage(
    RSAPrivateKeyMessage* data, RSAPrivateKeyMessage* encrypted,
    const vector<uint8_t>& encryption_key) {
  ASSERT_EQ(encryption_key.size(), KEY_SIZE);
  *encrypted = *data;
  size_t padding = KEY_SIZE - (data->rsa_key_length % KEY_SIZE);
  memset(data->rsa_key + data->rsa_key_length, static_cast<uint8_t>(padding),
         padding);
  encrypted->rsa_key_length = data->rsa_key_length + padding;
  uint8_t iv_buffer[16];
  memcpy(iv_buffer, &data->rsa_key_iv[0], KEY_IV_SIZE);
  AES_KEY aes_key;
  AES_set_encrypt_key(&encryption_key[0], 128, &aes_key);
  AES_cbc_encrypt(&data->rsa_key[0], &encrypted->rsa_key[0],
                  encrypted->rsa_key_length, &aes_key, iv_buffer, AES_ENCRYPT);
}

void Session::ServerSignBuffer(const uint8_t* data, size_t data_length,
                               std::vector<uint8_t>* signature) {
  ASSERT_LE(data_length, kMaxMessageSize);
  signature->assign(SHA256_DIGEST_LENGTH, 0);
  unsigned int md_len = SHA256_DIGEST_LENGTH;
  HMAC(EVP_sha256(), mac_key_server_.data(), mac_key_server_.size(), data,
       data_length, &(signature->front()), &md_len);
}

void Session::ClientSignMessage(const vector<uint8_t>& data,
                                std::vector<uint8_t>* signature) {
  signature->assign(SHA256_DIGEST_LENGTH, 0);
  unsigned int md_len = SHA256_DIGEST_LENGTH;
  HMAC(EVP_sha256(), mac_key_client_.data(), mac_key_client_.size(),
       &(data.front()), data.size(), &(signature->front()), &md_len);
}

void Session::VerifyClientSignature(size_t data_length) {
  // In the real world, a message should be signed by the client and
  // verified by the server.  This simulates that.
  vector<uint8_t> data(data_length);
  for (size_t i = 0; i < data.size(); i++) data[i] = i % 0xFF;
  OEMCryptoResult sts;
  size_t gen_signature_length = 0;
  sts = OEMCrypto_GenerateSignature(session_id(), data.data(), data.size(),
                                    NULL, &gen_signature_length);
  ASSERT_EQ(OEMCrypto_ERROR_SHORT_BUFFER, sts);
  ASSERT_EQ(static_cast<size_t>(32), gen_signature_length);
  vector<uint8_t> gen_signature(gen_signature_length);
  sts = OEMCrypto_GenerateSignature(session_id(), data.data(), data.size(),
                                    gen_signature.data(),
                                    &gen_signature_length);
  ASSERT_EQ(OEMCrypto_SUCCESS, sts);
  std::vector<uint8_t> expected_signature;
  ClientSignMessage(data, &expected_signature);
  ASSERT_EQ(expected_signature, gen_signature);
}

void Session::FillKeyArray(const MessageData& data,
                           OEMCrypto_KeyObject* key_array) {
  const uint8_t* data_ptr = reinterpret_cast<const uint8_t*>(&data);
  std::string message = wvcdm::BytesToString(data_ptr, sizeof(MessageData));
  for (unsigned int i = 0; i < num_keys_; i++) {
    key_array[i].key_id = GetSubstring(
        message,
        wvcdm::BytesToString(data.keys[i].key_id, data.keys[i].key_id_length));
    key_array[i].key_data_iv = GetSubstring(
        message,
        wvcdm::BytesToString(data.keys[i].key_iv, sizeof(data.keys[i].key_iv)));
    key_array[i].key_data = GetSubstring(
        message, wvcdm::BytesToString(data.keys[i].key_data,
                                      data.keys[i].key_data_length));
    key_array[i].key_control_iv = GetSubstring(
        message, wvcdm::BytesToString(data.keys[i].control_iv,
                                      sizeof(data.keys[i].control_iv)));
    const uint8_t* key_control_ptr =
        reinterpret_cast<const uint8_t*>(&data.keys[i].control);
    key_array[i].key_control = GetSubstring(
        message,
        wvcdm::BytesToString(key_control_ptr, sizeof(data.keys[i].control)));
  }
}

void Session::FillRefreshArray(OEMCrypto_KeyRefreshObject* key_array,
                               size_t key_count) {
  std::string message =
      wvcdm::BytesToString(message_ptr(), sizeof(MessageData));
  for (size_t i = 0; i < key_count; i++) {
    key_array[i].key_id = GetSubstring(
        message,
        wvcdm::BytesToString(encrypted_license().keys[i].key_id,
                             sizeof(encrypted_license().keys[i].key_id)),
        key_count <= 1);
    key_array[i].key_control_iv = GetSubstring();
    key_array[i].key_control = GetSubstring(
        message,
        wvcdm::BytesToString(reinterpret_cast<const uint8_t*>(
                                 &encrypted_license().keys[i].control),
                             sizeof(encrypted_license().keys[i].control)));
  }
}

void Session::EncryptCTR(const vector<uint8_t>& in_buffer, const uint8_t* key,
                         const uint8_t* starting_iv,
                         vector<uint8_t>* out_buffer) {
  ASSERT_NE(static_cast<void*>(NULL), key);
  ASSERT_NE(static_cast<void*>(NULL), starting_iv);
  ASSERT_NE(static_cast<void*>(NULL), out_buffer);
  AES_KEY aes_key;
  AES_set_encrypt_key(key, AES_BLOCK_SIZE * 8, &aes_key);
  out_buffer->resize(in_buffer.size());

  uint8_t iv[AES_BLOCK_SIZE];  // Current iv.

  memcpy(iv, &starting_iv[0], AES_BLOCK_SIZE);
  size_t l = 0;  // byte index into encrypted subsample.
  while (l < in_buffer.size()) {
    uint8_t aes_output[AES_BLOCK_SIZE];
    AES_encrypt(iv, aes_output, &aes_key);
    for (size_t n = 0; n < AES_BLOCK_SIZE && l < in_buffer.size(); n++, l++) {
      (*out_buffer)[l] = aes_output[n] ^ in_buffer[l];
    }
    ctr128_inc64(1, iv);
  }
}

void Session::TestDecryptCTR(bool select_key_first,
                             OEMCryptoResult expected_result, int key_index) {
  OEMCryptoResult sts;
  if (select_key_first) {
    // Select the key (from FillSimpleMessage)
    sts = OEMCrypto_SelectKey(session_id(), license_.keys[key_index].key_id,
                              license_.keys[key_index].key_id_length,
                              OEMCrypto_CipherMode_CTR);
    ASSERT_EQ(OEMCrypto_SUCCESS, sts);
  }

  vector<uint8_t> unencryptedData(256);
  for (size_t i = 0; i < unencryptedData.size(); i++)
    unencryptedData[i] = i % 256;
  EXPECT_EQ(1, GetRandBytes(unencryptedData.data(), unencryptedData.size()));
  vector<uint8_t> encryptionIv(KEY_IV_SIZE);
  EXPECT_EQ(1, GetRandBytes(encryptionIv.data(), KEY_IV_SIZE));
  vector<uint8_t> encryptedData(unencryptedData.size());
  EncryptCTR(unencryptedData, license_.keys[key_index].key_data,
             encryptionIv.data(), &encryptedData);

  // Describe the output
  vector<uint8_t> outputBuffer(256);
  OEMCrypto_DestBufferDesc destBuffer;
  destBuffer.type = OEMCrypto_BufferType_Clear;
  destBuffer.buffer.clear.address = outputBuffer.data();
  destBuffer.buffer.clear.max_length = outputBuffer.size();
  OEMCrypto_CENCEncryptPatternDesc pattern;
  pattern.encrypt = 0;
  pattern.skip = 0;
  pattern.offset = 0;
  // Decrypt the data
  sts = OEMCrypto_DecryptCENC(
      session_id(), encryptedData.data(), encryptedData.size(), true,
      encryptionIv.data(), 0, &destBuffer, &pattern,
      OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample);
  // We only have a few errors that we test are reported.
  if (expected_result == OEMCrypto_SUCCESS) {  // No error.
    ASSERT_EQ(OEMCrypto_SUCCESS, sts);
    ASSERT_EQ(unencryptedData, outputBuffer);
  } else {
    ASSERT_NO_FATAL_FAILURE(TestDecryptResult(expected_result, sts));
    ASSERT_NE(unencryptedData, outputBuffer);
  }
}

void Session::TestDecryptResult(OEMCryptoResult expected_result,
                                OEMCryptoResult actual_result) {

  if (expected_result == OEMCrypto_SUCCESS) {  // No error.
    ASSERT_EQ(OEMCrypto_SUCCESS, actual_result);
  } else if (expected_result == OEMCrypto_ERROR_KEY_EXPIRED &&
      global_features.api_version >= 9) {
    // Report stale keys, required in v9 and beyond.
    ASSERT_EQ(OEMCrypto_ERROR_KEY_EXPIRED, actual_result);
  } else if (expected_result == OEMCrypto_ERROR_INSUFFICIENT_HDCP) {
    // Report HDCP errors.
    ASSERT_EQ(OEMCrypto_ERROR_INSUFFICIENT_HDCP, actual_result);
  } else if (expected_result == OEMCrypto_ERROR_ANALOG_OUTPUT) {
    // Report analog errors.
    ASSERT_EQ(OEMCrypto_ERROR_ANALOG_OUTPUT, actual_result);
  } else {
    // OEM's can fine tune other error codes for debugging.
    ASSERT_NE(OEMCrypto_SUCCESS, actual_result);
  }
}

void Session::TestSelectExpired(unsigned int key_index) {
  if (global_features.api_version >= 13) {
    OEMCryptoResult status =
        OEMCrypto_SelectKey(session_id(), license().keys[key_index].key_id,
                            license().keys[key_index].key_id_length,
                            OEMCrypto_CipherMode_CTR);
    // It is OK for SelectKey to succeed with an expired key, but if there is
    // an error, it must be OEMCrypto_ERROR_KEY_EXIRED.
    if (status != OEMCrypto_SUCCESS) {
      ASSERT_EQ(OEMCrypto_ERROR_KEY_EXPIRED, status);
    }
  }
}

void Session::LoadOEMCert(bool verify_cert) {
  // Get the OEM Public Cert from OEMCrypto
  vector<uint8_t> public_cert;
  size_t public_cert_length = 0;
  ASSERT_EQ(OEMCrypto_ERROR_SHORT_BUFFER,
            OEMCrypto_GetOEMPublicCertificate(session_id(), NULL,
                                              &public_cert_length));
  ASSERT_LT(0u, public_cert_length);
  public_cert.resize(public_cert_length);
  ASSERT_EQ(OEMCrypto_SUCCESS,
            OEMCrypto_GetOEMPublicCertificate(session_id(), public_cert.data(),
                                              &public_cert_length));

  // Load the certificate chain into a BoringSSL X509 Stack
  const boringssl_ptr<STACK_OF(X509), DeleteX509Stack> x509_stack(
      sk_X509_new_null());
  ASSERT_TRUE(x509_stack.NotNull()) << "Unable to allocate X509 stack.";

  CBS pkcs7;
  CBS_init(&pkcs7, public_cert.data(), public_cert.size());
  if (!PKCS7_get_certificates(x509_stack.get(), &pkcs7)) {
    dump_boringssl_error();
    FAIL() << "Unable to deserialize certificate chain.";
  }

  STACK_OF(X509)* certs = x509_stack.get();

  // Load the public cert's key into public_rsa_ and verify, if requested
  for (size_t i = 0; certs && i < sk_X509_num(certs); i++) {
    X509* x509_cert = sk_X509_value(certs, i);
    boringssl_ptr<EVP_PKEY, EVP_PKEY_free> pubkey(X509_get_pubkey(x509_cert));
    ASSERT_TRUE(pubkey.NotNull());
    if (i == 0) {
      public_rsa_ = EVP_PKEY_get1_RSA(pubkey.get());
      if (!public_rsa_) {
        cout << "d2i_RSAPrivateKey failed.\n";
        dump_boringssl_error();
        ASSERT_TRUE(NULL != public_rsa_);
      }
    }
    if (verify_cert) {
      vector<char> buffer(80);

      X509_NAME* name = X509_get_subject_name(x509_cert);
      printf("  OEM Certificate Name: %s\n",
             X509_NAME_oneline(name, buffer.data(), buffer.size()));
      boringssl_ptr<X509_STORE, X509_STORE_free> store(X509_STORE_new());
      ASSERT_TRUE(store.NotNull());
      boringssl_ptr<X509_STORE_CTX, X509_STORE_CTX_free> store_ctx(
          X509_STORE_CTX_new());
      ASSERT_TRUE(store_ctx.NotNull());

      X509_STORE_CTX_init(store_ctx.get(), store.get(), x509_cert, NULL);

      // TODO(fredgc): Verify cert is signed by Google.

      int result = X509_verify_cert(store_ctx.get());
      ASSERT_GE(0, result) << "  OEM Cert not valid. " <<
          X509_verify_cert_error_string(
              X509_STORE_CTX_get_error(store_ctx.get()));
      if (result == 0) {
        printf("Cert not verified: %s.\n",
               X509_verify_cert_error_string(
                   X509_STORE_CTX_get_error(store_ctx.get())));
      }
    }
  }
}

void Session::MakeRSACertificate(struct RSAPrivateKeyMessage* encrypted,
                                 size_t message_size,
                                 std::vector<uint8_t>* signature,
                                 uint32_t allowed_schemes,
                                 const vector<uint8_t>& rsa_key,
                                 const vector<uint8_t>* encryption_key) {
  if (encryption_key == NULL) encryption_key = &enc_key_;
  struct RSAPrivateKeyMessage message;
  if (allowed_schemes != kSign_RSASSA_PSS) {
    uint32_t algorithm_n = htonl(allowed_schemes);
    memcpy(message.rsa_key, "SIGN", 4);
    memcpy(message.rsa_key + 4, &algorithm_n, 4);
    memcpy(message.rsa_key + 8, rsa_key.data(), rsa_key.size());
    message.rsa_key_length = 8 + rsa_key.size();
  } else {
    memcpy(message.rsa_key, rsa_key.data(), rsa_key.size());
    message.rsa_key_length = rsa_key.size();
  }
  EXPECT_EQ(1, GetRandBytes(message.rsa_key_iv, KEY_IV_SIZE));
  message.nonce = nonce_;

  EncryptProvisioningMessage(&message, encrypted, *encryption_key);
  ServerSignBuffer(reinterpret_cast<const uint8_t*>(encrypted), message_size,
                   signature);
}

void Session::RewrapRSAKey(const struct RSAPrivateKeyMessage& encrypted,
                           size_t message_size,
                           const std::vector<uint8_t>& signature,
                           vector<uint8_t>* wrapped_key, bool force) {
  size_t wrapped_key_length = 0;
  const uint8_t* message_ptr = reinterpret_cast<const uint8_t*>(&encrypted);
  ASSERT_EQ(OEMCrypto_ERROR_SHORT_BUFFER,
            OEMCrypto_RewrapDeviceRSAKey(
                session_id(), message_ptr, message_size, signature.data(),
                signature.size(), &encrypted.nonce, encrypted.rsa_key,
                encrypted.rsa_key_length, encrypted.rsa_key_iv, NULL,
                &wrapped_key_length));
  wrapped_key->clear();
  wrapped_key->assign(wrapped_key_length, 0);
  OEMCryptoResult sts = OEMCrypto_RewrapDeviceRSAKey(
      session_id(), message_ptr, message_size, signature.data(),
      signature.size(), &encrypted.nonce, encrypted.rsa_key,
      encrypted.rsa_key_length, encrypted.rsa_key_iv, &(wrapped_key->front()),
      &wrapped_key_length);
  if (force) {
    ASSERT_EQ(OEMCrypto_SUCCESS, sts);
  }
  if (OEMCrypto_SUCCESS != sts) {
    wrapped_key->clear();
  }
}

void Session::RewrapRSAKey30(const struct RSAPrivateKeyMessage& encrypted,
                             const std::vector<uint8_t>& encrypted_message_key,
                             vector<uint8_t>* wrapped_key, bool force) {
  size_t wrapped_key_length = 0;
  ASSERT_EQ(OEMCrypto_ERROR_SHORT_BUFFER,
            OEMCrypto_RewrapDeviceRSAKey30(
                session_id(), &nonce_, encrypted_message_key.data(),
                encrypted_message_key.size(), encrypted.rsa_key,
                encrypted.rsa_key_length, encrypted.rsa_key_iv, NULL,
                &wrapped_key_length));
  wrapped_key->clear();
  wrapped_key->assign(wrapped_key_length, 0);
  OEMCryptoResult sts = OEMCrypto_RewrapDeviceRSAKey30(
      session_id(), &nonce_, encrypted_message_key.data(),
      encrypted_message_key.size(), encrypted.rsa_key, encrypted.rsa_key_length,
      encrypted.rsa_key_iv, &(wrapped_key->front()), &wrapped_key_length);
  if (force) {
    ASSERT_EQ(OEMCrypto_SUCCESS, sts);
  }
  if (OEMCrypto_SUCCESS != sts) {
    wrapped_key->clear();
  }
}

void Session::PreparePublicKey(const uint8_t* rsa_key, size_t rsa_key_length) {
  if (rsa_key == NULL) {
    rsa_key = kTestRSAPKCS8PrivateKeyInfo2_2048;
    rsa_key_length = sizeof(kTestRSAPKCS8PrivateKeyInfo2_2048);
  }
  uint8_t* p = const_cast<uint8_t*>(rsa_key);
  boringssl_ptr<BIO, BIO_vfree> bio(BIO_new_mem_buf(p, rsa_key_length));
  ASSERT_TRUE(bio.NotNull());
  boringssl_ptr<PKCS8_PRIV_KEY_INFO, PKCS8_PRIV_KEY_INFO_free> pkcs8_pki(
      d2i_PKCS8_PRIV_KEY_INFO_bio(bio.get(), NULL));
  ASSERT_TRUE(pkcs8_pki.NotNull());
  boringssl_ptr<EVP_PKEY, EVP_PKEY_free> evp(EVP_PKCS82PKEY(pkcs8_pki.get()));
  ASSERT_TRUE(evp.NotNull());
  if (public_rsa_) RSA_free(public_rsa_);
  public_rsa_ = EVP_PKEY_get1_RSA(evp.get());
  if (!public_rsa_) {
    cout << "d2i_RSAPrivateKey failed. ";
    dump_boringssl_error();
    FAIL() << "Could not parse public RSA key.";
  }
  switch (RSA_check_key(public_rsa_)) {
    case 1:  // valid.
      return;
    case 0:  // not valid.
      dump_boringssl_error();
      FAIL() << "[rsa key not valid] ";
    default:  // -1 == check failed.
      dump_boringssl_error();
      FAIL() << "[error checking rsa key] ";
  }
}

bool Session::VerifyPSSSignature(EVP_PKEY* pkey, const uint8_t* message,
                                 size_t message_length,
                                 const uint8_t* signature,
                                 size_t signature_length) {
  EVP_MD_CTX md_ctx_struct;
  EVP_MD_CTX* md_ctx = &md_ctx_struct;
  EVP_MD_CTX_init(md_ctx);
  EVP_PKEY_CTX* pkey_ctx = NULL;

  if (EVP_DigestVerifyInit(md_ctx, &pkey_ctx, EVP_sha1(), NULL /* no ENGINE */,
                           pkey) != 1) {
    LOGE("EVP_DigestVerifyInit failed in VerifyPSSSignature");
    goto err;
  }

  if (EVP_PKEY_CTX_set_signature_md(pkey_ctx,
                                    const_cast<EVP_MD *>(EVP_sha1())) != 1) {
    LOGE("EVP_PKEY_CTX_set_signature_md failed in VerifyPSSSignature");
    goto err;
  }

  if (EVP_PKEY_CTX_set_rsa_padding(pkey_ctx, RSA_PKCS1_PSS_PADDING) != 1) {
    LOGE("EVP_PKEY_CTX_set_rsa_padding failed in VerifyPSSSignature");
    goto err;
  }

  if (EVP_PKEY_CTX_set_rsa_pss_saltlen(pkey_ctx, SHA_DIGEST_LENGTH) != 1) {
    LOGE("EVP_PKEY_CTX_set_rsa_pss_saltlen failed in VerifyPSSSignature");
    goto err;
  }

  if (EVP_DigestVerifyUpdate(md_ctx, message, message_length) != 1) {
    LOGE("EVP_DigestVerifyUpdate failed in VerifyPSSSignature");
    goto err;
  }

  if (EVP_DigestVerifyFinal(md_ctx, const_cast<uint8_t*>(signature),
                            signature_length) != 1) {
    LOGE(
        "EVP_DigestVerifyFinal failed in VerifyPSSSignature. (Probably a bad "
        "signature.)");
    goto err;
  }

  EVP_MD_CTX_cleanup(md_ctx);
  return true;

err:
  dump_boringssl_error();
  EVP_MD_CTX_cleanup(md_ctx);
  return false;
}

void Session::VerifyRSASignature(const vector<uint8_t>& message,
                                 const uint8_t* signature,
                                 size_t signature_length,
                                 RSA_Padding_Scheme padding_scheme) {
  EXPECT_TRUE(NULL != public_rsa_)
      << "No public RSA key loaded in test code.\n";

  EXPECT_EQ(static_cast<size_t>(RSA_size(public_rsa_)), signature_length)
      << "Signature size is wrong. " << signature_length << ", should be "
      << RSA_size(public_rsa_) << "\n";

  if (padding_scheme == kSign_RSASSA_PSS) {
    boringssl_ptr<EVP_PKEY, EVP_PKEY_free> pkey(EVP_PKEY_new());
    ASSERT_EQ(1, EVP_PKEY_set1_RSA(pkey.get(), public_rsa_));

    const bool ok = VerifyPSSSignature(
        pkey.get(), message.data(), message.size(), signature,
        signature_length);
    EXPECT_TRUE(ok) << "PSS signature check failed.";
  } else if (padding_scheme == kSign_PKCS1_Block1) {
    vector<uint8_t> padded_digest(signature_length);
    int size;
    // RSA_public_decrypt decrypts the signature, and then verifies that
    // it was padded with RSA PKCS1 padding.
    size = RSA_public_decrypt(
        signature_length, signature, padded_digest.data(), public_rsa_,
        RSA_PKCS1_PADDING);
    EXPECT_GT(size, 0);
    padded_digest.resize(size);
    EXPECT_EQ(message, padded_digest);
  } else {
    EXPECT_TRUE(false) << "Padding scheme not supported.";
  }
}

bool Session::GenerateRSASessionKey(vector<uint8_t>* session_key,
                                    vector<uint8_t>* enc_session_key) {
  if (!public_rsa_) {
    cout << "No public RSA key loaded in test code.\n";
    return false;
  }
  *session_key = wvcdm::a2b_hex("6fa479c731d2770b6a61a5d1420bb9d1");
  enc_session_key->assign(RSA_size(public_rsa_), 0);
  int status = RSA_public_encrypt(session_key->size(), &(session_key->front()),
                                  &(enc_session_key->front()), public_rsa_,
                                  RSA_PKCS1_OAEP_PADDING);
  int size = static_cast<int>(RSA_size(public_rsa_));
  if (status != size) {
    cout << "GenerateRSASessionKey error encrypting session key.\n";
    dump_boringssl_error();
    return false;
  }
  return true;
}

void Session::InstallRSASessionTestKey(const vector<uint8_t>& wrapped_rsa_key) {
  ASSERT_EQ(OEMCrypto_SUCCESS,
            OEMCrypto_LoadDeviceRSAKey(session_id(), wrapped_rsa_key.data(),
                                       wrapped_rsa_key.size()));
  GenerateDerivedKeysFromSessionKey();
}

void Session::CreateNewUsageEntry(OEMCryptoResult* status) {
  OEMCryptoResult result =
      OEMCrypto_CreateNewUsageEntry(session_id(), &usage_entry_number_);
  if (status) {
    *status = result;
    return;
  }
  ASSERT_EQ(OEMCrypto_SUCCESS, result);
}

void Session::UpdateUsageEntry(std::vector<uint8_t>* header_buffer) {
  size_t header_buffer_length = 0;
  size_t entry_buffer_length = 0;
  ASSERT_EQ(
      OEMCrypto_ERROR_SHORT_BUFFER,
      OEMCrypto_UpdateUsageEntry(session_id(), NULL, &header_buffer_length,
                                 NULL, &entry_buffer_length));
  ASSERT_LT(0u, header_buffer_length);
  header_buffer->resize(header_buffer_length);
  ASSERT_LT(0u, entry_buffer_length);
  encrypted_usage_entry_.resize(entry_buffer_length);
  ASSERT_EQ(OEMCrypto_SUCCESS,
            OEMCrypto_UpdateUsageEntry(
                session_id(), header_buffer->data(), &header_buffer_length,
                encrypted_usage_entry_.data(), &entry_buffer_length));
}

void Session::DeactivateUsageEntry(const std::string& pst) {
  ASSERT_EQ(OEMCrypto_SUCCESS,
            OEMCrypto_DeactivateUsageEntry(
                session_id(), reinterpret_cast<const uint8_t*>(pst.c_str()),
                pst.length()));
}

void Session::LoadUsageEntry(uint32_t index, const vector<uint8_t>& buffer) {
  usage_entry_number_ = index;
  encrypted_usage_entry_ = buffer;
  ASSERT_EQ(
      OEMCrypto_SUCCESS,
      OEMCrypto_LoadUsageEntry(
          session_id(), index, buffer.data(), buffer.size()));
}

void Session::MoveUsageEntry(uint32_t new_index,
                             std::vector<uint8_t>* header_buffer,
                             OEMCryptoResult expect_result) {

  ASSERT_NO_FATAL_FAILURE(open());
  ASSERT_NO_FATAL_FAILURE(ReloadUsageEntry());
  ASSERT_EQ(expect_result, OEMCrypto_MoveEntry(session_id(), new_index));
  if (expect_result == OEMCrypto_SUCCESS) {
    usage_entry_number_ = new_index;
    ASSERT_NO_FATAL_FAILURE(UpdateUsageEntry(header_buffer));
  }
  ASSERT_NO_FATAL_FAILURE(close());
}

void Session::GenerateReport(const std::string& pst,
                             OEMCryptoResult expected_result,
                             Session* other) {
  ASSERT_TRUE(open_);
  if (other) {  // If other is specified, copy mac keys.
    mac_key_server_ = other->mac_key_server_;
    mac_key_client_ = other->mac_key_client_;
  }
  size_t length = 0;
  OEMCryptoResult sts = OEMCrypto_ReportUsage(
      session_id(), reinterpret_cast<const uint8_t*>(pst.c_str()), pst.length(),
      pst_report_buffer_.data(), &length);
  if (expected_result == OEMCrypto_SUCCESS) {
    ASSERT_EQ(OEMCrypto_ERROR_SHORT_BUFFER, sts);
  }
  if (sts == OEMCrypto_ERROR_SHORT_BUFFER) {
    pst_report_buffer_.assign(length, 0xFF);  // Fill with garbage values.
  }
  sts = OEMCrypto_ReportUsage(session_id(),
                              reinterpret_cast<const uint8_t*>(pst.c_str()),
                              pst.length(), pst_report_buffer_.data(), &length);
  ASSERT_EQ(expected_result, sts);
  if (expected_result != OEMCrypto_SUCCESS) {
    return;
  }
  EXPECT_EQ(wvcdm::Unpacked_PST_Report::report_size(pst.length()), length);
  vector<uint8_t> computed_signature(SHA_DIGEST_LENGTH);
  unsigned int sig_len = SHA_DIGEST_LENGTH;
  HMAC(EVP_sha1(), mac_key_client_.data(), mac_key_client_.size(),
       &pst_report_buffer_[SHA_DIGEST_LENGTH], length - SHA_DIGEST_LENGTH,
       computed_signature.data(), &sig_len);
  EXPECT_EQ(0, memcmp(computed_signature.data(), pst_report().signature(),
                      SHA_DIGEST_LENGTH));
  EXPECT_GE(kInactiveUnused, pst_report().status());
  EXPECT_GE(kHardwareSecureClock, pst_report().clock_security_level());
  EXPECT_EQ(pst.length(), pst_report().pst_length());
  EXPECT_EQ(0, memcmp(pst.c_str(), pst_report().pst(), pst.length()));
  // Also, we the session to be able to sign the release message with the
  // correct mac keys from the usage table entry.
  ASSERT_NO_FATAL_FAILURE(VerifyClientSignature());
}

void Session::VerifyPST(const Test_PST_Report& expected) {
  wvcdm::Unpacked_PST_Report computed = pst_report();
  EXPECT_EQ(expected.status, computed.status());
  char* pst_ptr = reinterpret_cast<char *>(computed.pst());
  std::string computed_pst(pst_ptr, pst_ptr + computed.pst_length());
  ASSERT_EQ(expected.pst, computed_pst);
  time_t now = time(NULL);
  int64_t age = now - expected.time_created;  // How old is this report.
  EXPECT_NEAR(expected.seconds_since_license_received + age,
              computed.seconds_since_license_received(),
              kTimeTolerance);
  // Decrypt times only valid on licenses that have been active.
  if (expected.status == kActive || expected.status == kInactiveUsed) {
    EXPECT_NEAR(expected.seconds_since_first_decrypt + age,
                computed.seconds_since_first_decrypt(),
                kUsageTableTimeTolerance);
    EXPECT_NEAR(expected.seconds_since_last_decrypt + age,
                computed.seconds_since_last_decrypt(),
                kUsageTableTimeTolerance);
  }
  std::vector<uint8_t> signature(SHA_DIGEST_LENGTH);
  unsigned int md_len = SHA_DIGEST_LENGTH;
  if (!HMAC(EVP_sha1(), mac_key_client_.data(), mac_key_client_.size(),
            pst_report_buffer_.data() + SHA_DIGEST_LENGTH,
            pst_report_buffer_.size() - SHA_DIGEST_LENGTH,
            signature.data(), &md_len)) {
    cout << "Error computing HMAC.\n";
    dump_boringssl_error();
  }
  EXPECT_EQ(0, memcmp(computed.signature(), signature.data(),
                      SHA_DIGEST_LENGTH));
}

// This might adjust t to be "seconds since now".  If t is small, we assume it
// is "seconds since now", but if the value of t is large, assume it is
// "absolute time" and convert to "seconds since now".
static int64_t MaybeAdjustTime(int64_t t, time_t now) {
  int64_t k10Minutes = 60 * 10;  // in seconds.
  if (t > k10Minutes) return now - t;
  return t;
}

void Session::VerifyReport(Test_PST_Report expected,
                           int64_t time_license_received,
                           int64_t time_first_decrypt,
                           int64_t time_last_decrypt) {
  time_t now = time(NULL);
  expected.seconds_since_license_received =
      MaybeAdjustTime(time_license_received, now);
  expected.seconds_since_first_decrypt =
      MaybeAdjustTime(time_first_decrypt, now);
  expected.seconds_since_last_decrypt = MaybeAdjustTime(time_last_decrypt, now);
  ASSERT_NO_FATAL_FAILURE(VerifyPST(expected));
}

void Session::GenerateVerifyReport(const std::string& pst,
                                   OEMCrypto_Usage_Entry_Status status,
                                   int64_t time_license_received,
                                   int64_t time_first_decrypt,
                                   int64_t time_last_decrypt) {
  ASSERT_NO_FATAL_FAILURE(GenerateReport(pst));
  Test_PST_Report expected(pst, status);
  ASSERT_NO_FATAL_FAILURE(VerifyReport(expected, time_license_received,
                                       time_first_decrypt, time_last_decrypt));
  // The PST report was signed above.  Below we verify that the entire message
  // that is sent to the server will be signed by the right mac keys.
  ASSERT_NO_FATAL_FAILURE(VerifyClientSignature());
}

void Session::CreateOldEntry(const Test_PST_Report& report) {
  OEMCryptoResult result = OEMCrypto_CreateOldUsageEntry(
      report.seconds_since_license_received,
      report.seconds_since_first_decrypt,
      report.seconds_since_last_decrypt,
      report.status, mac_key_server_.data(),
      mac_key_client_.data(),
      reinterpret_cast<const uint8_t*>(report.pst.c_str()),
      report.pst.length());
  if (result == OEMCrypto_ERROR_NOT_IMPLEMENTED) return;
  ASSERT_EQ(OEMCrypto_SUCCESS, result);
}

void Session::CopyAndVerifyOldEntry(const Test_PST_Report& report,
                                    std::vector<uint8_t>* header_buffer) {
  ASSERT_NO_FATAL_FAILURE(CreateNewUsageEntry());
  OEMCryptoResult result = OEMCrypto_CopyOldUsageEntry(
      session_id(), reinterpret_cast<const uint8_t*>(report.pst.c_str()),
      report.pst.length());
  if (result == OEMCrypto_ERROR_NOT_IMPLEMENTED) {
    cout << "WARNING: OEMCrypto CANNOT copy old usage table to new." << endl;
    return;
  }
  ASSERT_NO_FATAL_FAILURE(UpdateUsageEntry(header_buffer));
  ASSERT_NO_FATAL_FAILURE(GenerateReport(report.pst));
  ASSERT_NO_FATAL_FAILURE(VerifyPST(report));
}

const uint8_t* Session::message_ptr() {
  return reinterpret_cast<const uint8_t*>(&encrypted_license());
}

void Session::set_message_size(size_t size) {
  message_size_ = size;
  ASSERT_GE(message_size_, sizeof(MessageData));
  ASSERT_LE(message_size_, kMaxMessageSize);
}

const uint8_t* Session::encrypted_entitled_message_ptr() {
  return reinterpret_cast<const uint8_t*>(encrypted_entitled_message_.data());
}

}  // namespace wvoec
