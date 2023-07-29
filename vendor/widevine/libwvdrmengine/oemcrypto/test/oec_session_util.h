#ifndef CDM_OEC_SESSION_UTIL_H_
#define CDM_OEC_SESSION_UTIL_H_

// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
// OEMCrypto unit tests
//
#include <openssl/rsa.h>
#include <time.h>
#include <string>
#include <vector>

#include "oec_device_features.h"
#include "oemcrypto_types.h"
#include "pst_report.h"

using namespace std;

// GTest requires PrintTo to be in the same namespace as the thing it prints,
// which is std::vector in this case.
namespace std {

void PrintTo(const vector<uint8_t>& value, ostream* os);

}  // namespace std

namespace wvoec {

// Make sure this is larger than kMaxKeysPerSession, in oemcrypto_test.cpp
const size_t kMaxNumKeys = 20;

namespace {
#if defined(TEST_SPEED_MULTIPLIER)  // Can slow test time limits when
                                    // debugging is slowing everything.
const int kSpeedMultiplier = TEST_SPEED_MULTIPLIER;
#else
const int kSpeedMultiplier = 1;
#endif
const int kShortSleep = 1 * kSpeedMultiplier;
const int kLongSleep = 2 * kSpeedMultiplier;
const uint32_t kDuration = 2 * kSpeedMultiplier;
const uint32_t kLongDuration = 5 * kSpeedMultiplier;
const int32_t kTimeTolerance = 3 * kSpeedMultiplier;
const time_t kUsageTableTimeTolerance = 10 * kSpeedMultiplier;
}  // namespace

typedef struct {
  uint8_t verification[4];
  uint32_t duration;
  uint32_t nonce;
  uint32_t control_bits;
} KeyControlBlock;

// Note: The API does not specify a maximum key id length.  We specify a
// maximum just for these tests, so that we have a fixed message size.
const size_t kTestKeyIdMaxLength = 16;

// Most content will use a key id that is 16 bytes long.
const int kDefaultKeyIdLength = 16;

const size_t kMaxTestRSAKeyLength = 2000;   // Rough estimate.
const size_t kMaxPSTLength = 255;           // In specification.
const size_t kMaxMessageSize = 8 * 1024;    // In specification.

typedef struct {
  uint8_t key_id[kTestKeyIdMaxLength];
  size_t key_id_length;
  uint8_t key_data[MAC_KEY_SIZE];
  size_t key_data_length;
  uint8_t key_iv[KEY_IV_SIZE];
  uint8_t control_iv[KEY_IV_SIZE];
  KeyControlBlock control;
  // Note: cipher_mode may not be part of a real signed message. For these
  // tests, it is convenient to keep it in this structure anyway.
  OEMCryptoCipherMode cipher_mode;
} MessageKeyData;

// This structure will be signed to simulate a message from the server.
struct MessageData {
  MessageKeyData keys[kMaxNumKeys];
  uint8_t mac_key_iv[KEY_IV_SIZE];
  uint8_t padding[KEY_IV_SIZE];
  uint8_t mac_keys[2 * MAC_KEY_SIZE];
  uint8_t pst[kMaxPSTLength];
};

// This structure will be signed to simulate a provisioning response from the
// server.
struct RSAPrivateKeyMessage {
  uint8_t rsa_key[kMaxTestRSAKeyLength];
  uint8_t rsa_key_iv[KEY_IV_SIZE];
  size_t rsa_key_length;
  uint32_t nonce;
};

struct Test_PST_Report {
  Test_PST_Report(const std::string& pst_in,
                  OEMCrypto_Usage_Entry_Status status_in)
      : status(status_in), pst(pst_in), time_created(time(NULL)) {}

  OEMCrypto_Usage_Entry_Status status;
  int64_t seconds_since_license_received;
  int64_t seconds_since_first_decrypt;
  int64_t seconds_since_last_decrypt;
  std::string pst;
  time_t time_created;
};

struct EntitledContentKeyData {
  uint8_t entitlement_key_id[KEY_SIZE];
  uint8_t content_key_id[KEY_SIZE];
  uint8_t content_key_data_iv[KEY_SIZE];
  uint8_t content_key_data[KEY_SIZE];
};

// Increment counter for AES-CTR.  The CENC spec specifies we increment only
// the low 64 bits of the IV counter, and leave the high 64 bits alone.  This
// is different from the OpenSSL implementation, so we implement the CTR loop
// ourselves.
void ctr128_inc64(int64_t increaseBy, uint8_t* iv);

// Some compilers don't like the macro htonl within an ASSERT_EQ.
uint32_t htonl_fnc(uint32_t x);

// Prints error string from BoringSSL
void dump_boringssl_error();

// Given a message and field, returns an OEMCrypto_Substring with the field's
// offset into the message and its length. If |set_zero| is true, both the
// offset and length will be zero.
OEMCrypto_Substring GetSubstring(const std::string& message = "",
                                 const std::string& field = "",
                                 bool set_zero = false);

class Session {
 public:
  Session();
  ~Session();

  // Returns the most recently generated nonce.
  // Valid after call to GenerateNonce.
  uint32_t get_nonce() { return nonce_; }
  // Valid after call to open().
  uint32_t session_id() { return (uint32_t)session_id_; }
  // Call OEMCrypto_OpenSession, with GTest ASSERTs.
  void open();
  // Call OEMCrypto_CloseSession, with GTest ASSERTs.
  void close();
  // Artifically set session id without calling OEMCrypto_OpenSession.  This is
  // used in core/test/generic_crypto_unittest.cpp.
  void SetSessionId(uint32_t session_id);
  uint32_t GetOecSessionId() { return session_id_; }
  // Generates one nonce.  If error_counter is null, this will sleep 1 second
  // and try again if a nonce flood has been detected.  If error_counter is
  // not null, it will be incremented when a nonce flood is detected.
  void GenerateNonce(int* error_counter = NULL);
  // Fill the vectors with test context which generate known mac and enc keys.
  void FillDefaultContext(vector<uint8_t>* mac_context,
                          vector<uint8_t>* enc_context);
  // Generate known mac and enc keys using OEMCrypto_GenerateDerivedKeys and
  // also fill out enc_key_, mac_key_server_, and mac_key_client_.
  void GenerateDerivedKeysFromKeybox(const wvoec::WidevineKeybox& keybox);
  // Generate known mac and enc keys using OEMCrypto_DeriveKeysFromSessionKey
  // and also fill out enc_key_, mac_key_server_, and mac_key_client_.
  void GenerateDerivedKeysFromSessionKey();
  // Loads and verifies the keys in the message pointed to by message_ptr()
  // using OEMCrypto_LoadKeys.  This message should have already been created
  // by FillSimpleMessage, modified if needed, and then encrypted and signed by
  // the server's mac key in EncryptAndSign.
  void LoadTestKeys(const std::string& pst = "", bool new_mac_keys = true);
  // Loads the entitlement keys in the message pointed to by message_ptr()
  // using OEMCrypto_LoadKeys.  This message should have already been created
  // by FillSimpleEntitlementMessage, modified if needed, and then encrypted
  // and signed by the server's mac key in EncryptAndSign.
  void LoadEntitlementTestKeys(const std::string& pst = "",
                              bool new_mac_keys = true,
                              OEMCryptoResult expected_sts = OEMCrypto_SUCCESS);
  // Fills an OEMCrypto_EntitledContentKeyObject using the information from
  // the license_ and randomly generated content keys. This method should be
  // called after LoadEntitlementTestKeys.
  void FillEntitledKeyArray();
  // Encrypts and loads the entitled content keys via
  // OEMCrypto_LoadEntitledContentKeys.
  void LoadEntitledContentKeys(
      OEMCryptoResult expected_sts = OEMCrypto_SUCCESS);
  // This uses OEMCrypto_QueryKeyControl to check that the keys in OEMCrypto
  // have the correct key control data.
  void VerifyTestKeys();
  // This uses OEMCrypto_QueryKeyControl to check that the keys in OEMCrypto
  // have the correct key control data.
  void VerifyEntitlementTestKeys();
  // This creates a refresh key or license renewal message, signs it with the
  // server's mac key, and calls OEMCrypto_RefreshKeys.
  void RefreshTestKeys(const size_t key_count, uint32_t control_bits,
                       uint32_t nonce, OEMCryptoResult expected_result);
  // This sets the key id in the current message data to the specified string.
  // This is used to test with different key id lengths.
  void SetKeyId(int index, const string& key_id);
  // This fills the data structure license_ with key information.  This data
  // can be modified, and then should be encrypted and signed in EncryptAndSign
  // before being loaded in LoadTestKeys.
  void FillSimpleMessage(uint32_t duration, uint32_t control, uint32_t nonce,
                         const std::string& pst = "");
  // This fills the data structure license_ with entitlement key information.
  // This data can be modified, and then should be encrypted and signed in
  // EncryptAndSign before being loaded in LoadEntitlementTestKeys.
  void FillSimpleEntitlementMessage(
      uint32_t duration, uint32_t control,
      uint32_t nonce, const std::string& pst = "");
  // Like FillSimpleMessage, this fills encrypted_license_ with data.  The name
  // is a little misleading: the license renewal message is not encrypted, it
  // is just signed.  The signature is computed in RefreshTestKeys, above.
  void FillRefreshMessage(size_t key_count, uint32_t control_bits,
                          uint32_t nonce);
  // Sets the OEMCrypto_Substring parameters of the LoadKeys method.
  // Specifically, it sets the |enc_mac_keys_iv|, |enc_mac_keys|, |pst|, and
  // |srm_restriction_data| in that order. For testing purposes,
  // |srm_restriction_data| will always be NULL.
  void SetLoadKeysSubstringParams();
  // This copies data from license_ to encrypted_license_, and then encrypts
  // each field in the key array appropriately.  It then signes the buffer with
  // the server mac keys. It then fills out the key_array_ so that pointers in
  // that array point to the locations in the encrypted message.
  void EncryptAndSign();
  // This encrypts an RSAPrivateKeyMessage with encryption_key so that it may be
  // loaded with OEMCrypto_RewrapDeviceRSAKey.
  void EncryptProvisioningMessage(RSAPrivateKeyMessage* data,
                                  RSAPrivateKeyMessage* encrypted,
                                  const vector<uint8_t>& encryption_key);
  // Sign the buffer with server's mac key.
  void ServerSignBuffer(const uint8_t* data, size_t data_length,
                        std::vector<uint8_t>* signature);
  // Sign the buffer with client's known mac key.  Known test keys must be
  // installed first.
  void ClientSignMessage(const vector<uint8_t>& data,
                         std::vector<uint8_t>* signature);
  // This checks the signature generated by OEMCrypto_GenerateSignature against
  // that generaged by ClientSignMessage.
  void VerifyClientSignature(size_t data_length = 400);
  // Set the pointers in key_array[*] to point values inside data. This is
  // needed to satisfy range checks in OEMCrypto_LoadKeys.
  void FillKeyArray(const MessageData& data, OEMCrypto_KeyObject* key_array);
  // As in FillKeyArray but for the license renewal message passed to
  // OEMCrypto_RefreshKeys.
  void FillRefreshArray(OEMCrypto_KeyRefreshObject* key_array,
                        size_t key_count);
  // Encrypt a block of data using CTR mode.
  void EncryptCTR(const vector<uint8_t>& in_buffer, const uint8_t* key,
                  const uint8_t* starting_iv, vector<uint8_t>* out_buffer);
  // Encrypt some data and pass to OEMCrypto_DecryptCENC to verify decryption.
  void TestDecryptCTR(bool select_key_first = true,
                      OEMCryptoResult expected_result = OEMCrypto_SUCCESS,
                      int key_index = 0);
  // This compares the actual result with the expected result.  If OEMCrypto is
  // an older version, we allow it to report an equivalent error code.
  void TestDecryptResult(OEMCryptoResult expected_result,
                         OEMCryptoResult actual_result);
  // Verify that an attempt to select an expired key either succeeds, or gives
  // an actionable error code.
  void TestSelectExpired(unsigned int key_index);
  // Calls OEMCrypto_GetOEMPublicCertificate and loads the OEM cert's public
  // rsa key into public_rsa_.
  void LoadOEMCert(bool verify_cert = false);
  // Creates RSAPrivateKeyMessage for the specified rsa_key, encrypts it with
  // the specified encryption key, and then signs it with the server's mac key.
  // If encryption_key is null, use the session's enc_key_.
  void MakeRSACertificate(struct RSAPrivateKeyMessage* encrypted,
                          size_t message_size, std::vector<uint8_t>* signature,
                          uint32_t allowed_schemes,
                          const vector<uint8_t>& rsa_key,
                          const vector<uint8_t>* encryption_key = NULL);
  // Calls OEMCrypto_RewrapDeviceRSAKey with the given provisioning response
  // message.  If force is true, we assert that the key loads successfully.
  void RewrapRSAKey(const struct RSAPrivateKeyMessage& encrypted,
                    size_t message_size, const std::vector<uint8_t>& signature,
                    vector<uint8_t>* wrapped_key, bool force);
  // Loads the specified RSA public key into public_rsa_.  If rsa_key is null,
  // the default test key is loaded.
  void PreparePublicKey(const uint8_t* rsa_key = NULL,
                        size_t rsa_key_length = 0);
  // Verifies the given signature is from the given message and RSA key, pkey.
  static bool VerifyPSSSignature(EVP_PKEY* pkey, const uint8_t* message,
                                 size_t message_length,
                                 const uint8_t* signature,
                                 size_t signature_length);
  // Verify that the message was signed by the private key associated with
  // |public_rsa_| using the specified padding scheme.
  void VerifyRSASignature(const vector<uint8_t>& message,
                          const uint8_t* signature, size_t signature_length,
                          RSA_Padding_Scheme padding_scheme);
  // Encrypts a known session key with public_rsa_ for use in future calls to
  // OEMCrypto_DeriveKeysFromSessionKey or OEMCrypto_RewrapDeviceRSAKey30.
  // The unencrypted session key is stored in session_key.
  bool GenerateRSASessionKey(vector<uint8_t>* session_key,
                             vector<uint8_t>* enc_session_key);
  // Calls OEMCrypto_RewrapDeviceRSAKey30 with the given provisioning response
  // message. If force is true, we assert that the key loads successfully.
  void RewrapRSAKey30(const struct RSAPrivateKeyMessage& encrypted,
                      const std::vector<uint8_t>& encrypted_message_key,
                      vector<uint8_t>* wrapped_key, bool force);
  // Loads the specified wrapped_rsa_key into OEMCrypto, and then runs
  // GenerateDerivedKeysFromSessionKey to install known encryption and mac keys.
  void InstallRSASessionTestKey(const vector<uint8_t>& wrapped_rsa_key);
  // Creates a new usage entry, and keeps track of the index.
  // If status is null, we expect success, otherwise status is set to the
  // return value.
  void CreateNewUsageEntry(OEMCryptoResult *status = NULL);
  // Copy encrypted usage entry from other session, and then load it.
  // This session must already be open.
  void LoadUsageEntry(uint32_t index, const vector<uint8_t>& buffer);
  // Copy encrypted usage entry from other session.
  // This session must already be open.
  void LoadUsageEntry(const Session& other) {
    LoadUsageEntry(other.usage_entry_number(), other.encrypted_usage_entry());
  }
  // Reload previously used usage entry.
  void ReloadUsageEntry() { LoadUsageEntry(*this); }
  // Update the usage entry and save the header to the specified buffer.
  void UpdateUsageEntry(std::vector<uint8_t>* header_buffer);
  // Deactivate this session's usage entry.
  void DeactivateUsageEntry(const std::string& pst);
  // The usage entry number for this session's usage entry.
  uint32_t usage_entry_number() const { return usage_entry_number_; }
  void set_usage_entry_number(uint32_t v) { usage_entry_number_ = v; }
  // The encrypted buffer holding the recently updated and saved usage entry.
  const vector<uint8_t>& encrypted_usage_entry() const {
    return encrypted_usage_entry_;
  }
  // Generates a usage report for the specified pst.  If there is success,
  // the report's signature is verified, and several fields are given sanity
  // checks.  If other is not null, then the mac keys are copied from other in
  // order to verify signatures.
  void GenerateReport(const std::string& pst,
                      OEMCryptoResult expected_result = OEMCrypto_SUCCESS,
                      Session* other = 0);
  // Move this usage entry to a new index.
  void MoveUsageEntry(uint32_t new_index, std::vector<uint8_t>* header_buffer,
                      OEMCryptoResult expect_result = OEMCrypto_SUCCESS);
  // PST used in FillSimpleMesage.
  string pst() const { return pst_; }
  // Returns a pointer-like thing to the usage report generated by the previous
  // call to GenerateReport.
  wvcdm::Unpacked_PST_Report pst_report() {
    return wvcdm::Unpacked_PST_Report(&pst_report_buffer_[0]);
  }
  // Verify the values in the PST report.  The signature should have been
  // verified in GenerateReport, above.
  void VerifyPST(const Test_PST_Report& report);
  // Verify the Usage Report. If any time is greater than 10 minutes, it is
  // assumed to be an absolute time, and time_since will be computed relative to
  // now.
  void VerifyReport(Test_PST_Report report,
                    int64_t time_license_received = 0,
                    int64_t time_first_decrypt = 0,
                    int64_t time_last_decrypt = 0);
  // Same as above, but generates the report with the given status.
  void GenerateVerifyReport(const std::string& pst,
                            OEMCrypto_Usage_Entry_Status status,
                            int64_t time_license_received = 0,
                            int64_t time_first_decrypt = 0,
                            int64_t time_last_decrypt = 0);
  // Create an entry in the old usage table based on the given report.
  void CreateOldEntry(const Test_PST_Report &report);
  // Create a new entry and copy the old entry into it.  Then very the report
  // is right.
  void CopyAndVerifyOldEntry(const Test_PST_Report &report,
                             std::vector<uint8_t>* header_buffer);

  // The unencrypted license response or license renewal response.
  MessageData& license() { return license_; }
  // The encrypted license response or license renewal response.
  MessageData& encrypted_license() { return padded_message_; }

  // A pointer to the buffer holding encrypted_license.
  const uint8_t* message_ptr();

  // An array of key objects for use in LoadKeys.
  OEMCrypto_KeyObject* key_array() { return key_array_; }

  // An array of key objects for LoadEntitledContentKeys.
  OEMCrypto_EntitledContentKeyObject* entitled_key_array() {
    return entitled_key_array_;
  }

  // The last signature generated with the server's mac key.
  std::vector<uint8_t>& signature() { return signature_; }

  // Set the number of keys to use in the license(), encrypted_license()
  // and key_array().
  void set_num_keys(int num_keys) { num_keys_ = num_keys; }
  // The current number of keys to use in the license(), encrypted_license()
  // and key_array().
  unsigned int num_keys() const { return num_keys_; }

  // Set the size of the buffer used the encrypted license.
  // Must be between sizeof(MessageData) and kMaxMessageSize.
  void set_message_size(size_t size);
  // The size of the encrypted message.
  size_t message_size() { return message_size_; }

  // The OEMCrypto_Substrings associated with the encrypted license that are
  // passed to LoadKeys.
  vector<OEMCrypto_Substring> load_keys_params() { return load_keys_params_; }
  OEMCrypto_Substring enc_mac_keys_iv_substr() { return load_keys_params_[0]; }
  OEMCrypto_Substring enc_mac_keys_substr() { return load_keys_params_[1]; }
  OEMCrypto_Substring pst_substr() { return load_keys_params_[2]; }
  OEMCrypto_Substring srm_restriction_data_substr() {
    return load_keys_params_[3];
  }

  // Pointer to buffer holding |encrypted_entitled_message_|
  const uint8_t* encrypted_entitled_message_ptr();

 private:
  // Generate mac and enc keys give the master key.
  void DeriveKeys(const uint8_t* master_key,
                  const vector<uint8_t>& mac_key_context,
                  const vector<uint8_t>& enc_key_context);
  // Internal utility function to derive key using CMAC-128
  void DeriveKey(const uint8_t* key, const vector<uint8_t>& context,
                 int counter, vector<uint8_t>* out);

  bool open_;
  bool forced_session_id_;
  OEMCrypto_SESSION session_id_;
  vector<uint8_t> mac_key_server_;
  vector<uint8_t> mac_key_client_;
  vector<uint8_t> enc_key_;
  uint32_t nonce_;
  RSA* public_rsa_;
  vector<uint8_t> pst_report_buffer_;
  MessageData license_;
  struct PaddedMessageData : public MessageData {
    uint8_t padding[kMaxMessageSize - sizeof(MessageData)];
  } padded_message_;
  size_t message_size_;  // How much of the padded message to use.
  OEMCrypto_KeyObject key_array_[kMaxNumKeys];
  vector<OEMCrypto_Substring> load_keys_params_;
  std::vector<uint8_t> signature_;
  unsigned int num_keys_;
  vector<uint8_t> encrypted_usage_entry_;
  uint32_t usage_entry_number_;
  string pst_;

  // Clear Entitlement key data. This is the backing data for
  // |entitled_key_array_|.
  EntitledContentKeyData entitled_key_data_[kMaxNumKeys];
  // Message containing data from |key_array| and |entitled_key_data_|.
  std::string entitled_message_;
  // Entitled key object. Pointers are backed by |entitled_key_data_|.
  OEMCrypto_EntitledContentKeyObject entitled_key_array_[kMaxNumKeys];
  std::string encrypted_entitled_message_;
};

}  // namespace wvoec

#endif  // CDM_OEC_SESSION_UTIL_H_
