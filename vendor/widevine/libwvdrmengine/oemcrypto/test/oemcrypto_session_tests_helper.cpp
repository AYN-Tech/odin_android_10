#include "oemcrypto_session_tests_helper.h"

#include <gtest/gtest.h>
#include "oec_test_data.h"

using namespace std;
using namespace wvoec;

namespace wvoec {

// Make this function available when in Fuzz mode because we are not inheriting
// from OEMCryptoClientTest.
const uint8_t* find(const vector<uint8_t>& message,
                    const vector<uint8_t>& substring) {
  vector<uint8_t>::const_iterator pos = search(
      message.begin(), message.end(), substring.begin(), substring.end());
  if (pos == message.end()) {
    return NULL;
  }
  return &(*pos);
}

// This creates a wrapped RSA key for devices that have the test keybox
// installed.  If force is true, we assert that the key loads successfully.
void SessionUtil::CreateWrappedRSAKeyFromKeybox(uint32_t allowed_schemes,
                                                bool force) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(s.GenerateDerivedKeysFromKeybox(keybox_));
  // Provisioning request would be signed by the client and verified by the
  // server.
  ASSERT_NO_FATAL_FAILURE(s.VerifyClientSignature());
  struct RSAPrivateKeyMessage encrypted;
  std::vector<uint8_t> signature;
  ASSERT_NO_FATAL_FAILURE(
      s.MakeRSACertificate(&encrypted, sizeof(encrypted),
                           &signature, allowed_schemes,
                           encoded_rsa_key_));
  ASSERT_NO_FATAL_FAILURE(s.RewrapRSAKey(
      encrypted, sizeof(encrypted), signature, &wrapped_rsa_key_, force));
  // Verify that the clear key is not contained in the wrapped key.
  // It should be encrypted.
  ASSERT_EQ(NULL, find(wrapped_rsa_key_, encoded_rsa_key_));
}

// This creates a wrapped RSA key for devices using provisioning 3.0. If force
//  is true, we assert that the key loads successfully.
void SessionUtil::CreateWrappedRSAKeyFromOEMCert(
  uint32_t allowed_schemes, bool force) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(s.LoadOEMCert());
  s.GenerateNonce();
  struct RSAPrivateKeyMessage encrypted;
  std::vector<uint8_t> signature;
  std::vector<uint8_t> message_key;
  std::vector<uint8_t> encrypted_message_key;
  s.GenerateRSASessionKey(&message_key, &encrypted_message_key);
  ASSERT_NO_FATAL_FAILURE(
      s.MakeRSACertificate(&encrypted, sizeof(encrypted), &signature,
                           allowed_schemes, encoded_rsa_key_, &message_key));
  ASSERT_NO_FATAL_FAILURE(
      s.RewrapRSAKey30(encrypted, encrypted_message_key,
                       &wrapped_rsa_key_, force));
  // Verify that the clear key is not contained in the wrapped key.
  // It should be encrypted.
  ASSERT_EQ(NULL, find(wrapped_rsa_key_, encoded_rsa_key_));
}

// If force is true, we assert that the key loads successfully.
void SessionUtil::CreateWrappedRSAKey(uint32_t allowed_schemes,
                                      bool force) {
  switch (global_features.provisioning_method) {
    case OEMCrypto_OEMCertificate:
      CreateWrappedRSAKeyFromOEMCert(allowed_schemes, force);
      break;
    case OEMCrypto_Keybox:
      CreateWrappedRSAKeyFromKeybox(allowed_schemes, force);
      break;
    default:
      FAIL() << "Cannot generate wrapped RSA key if provision method = "
             << wvoec::ProvisioningMethodName(
                    global_features.provisioning_method);
  }
}

void SessionUtil::InstallKeybox(const wvoec::WidevineKeybox& keybox,
                                bool good) {
  uint8_t wrapped[sizeof(wvoec::WidevineKeybox)];
  size_t length = sizeof(wvoec::WidevineKeybox);
  keybox_ = keybox;
  ASSERT_EQ(
      OEMCrypto_SUCCESS,
      OEMCrypto_WrapKeybox(reinterpret_cast<const uint8_t*>(&keybox),
                           sizeof(keybox), wrapped, &length, NULL, 0));
  OEMCryptoResult sts = OEMCrypto_InstallKeybox(wrapped, sizeof(keybox));
  if (good) {
    ASSERT_EQ(OEMCrypto_SUCCESS, sts);
  } else {
    // Can return error now, or return error on IsKeyboxValid.
  }
}

void SessionUtil::EnsureTestKeys() {
  switch (global_features.derive_key_method) {
    case DeviceFeatures::LOAD_TEST_KEYBOX:
      keybox_ = kTestKeybox;
      // TODO(fredgc, b/119316243): REMOVE FOLLOWING LINE:
      if (global_features.api_version < 14) keybox_ = kTestKeyboxForV13;
      ASSERT_EQ(OEMCrypto_SUCCESS,
                OEMCrypto_LoadTestKeybox(
                    reinterpret_cast<const uint8_t*>(&keybox_),
                    sizeof(keybox_)));
      break;
    case DeviceFeatures::LOAD_TEST_RSA_KEY:
      ASSERT_EQ(OEMCrypto_SUCCESS, OEMCrypto_LoadTestRSAKey());
      break;
    case DeviceFeatures::FORCE_TEST_KEYBOX:
      keybox_ = kTestKeybox;
      InstallKeybox(keybox_, true);
      break;
    case DeviceFeatures::TEST_PROVISION_30:
      // Can use oem certificate to install test rsa key.
      break;
    default:
      FAIL() << "Cannot run test without test keybox or RSA key installed.";
  }
}

// This makes sure that the derived keys (encryption key and two mac keys)
// are installed in OEMCrypto and in the test session.
void SessionUtil::InstallTestSessionKeys(Session* s) {
  if (global_features.uses_certificate) {
    if (global_features.loads_certificate) {
      if (wrapped_rsa_key_.size() == 0) {
        // If we don't have a wrapped key yet, create one.
        // This wrapped key will be shared by all sessions in the test.
        ASSERT_NO_FATAL_FAILURE(
            CreateWrappedRSAKey(kSign_RSASSA_PSS, true));
      }
      // Load the wrapped rsa test key.
      ASSERT_NO_FATAL_FAILURE(
          s->InstallRSASessionTestKey(wrapped_rsa_key_));
    }
    // Test RSA key should be loaded.
    ASSERT_NO_FATAL_FAILURE(
        s->GenerateDerivedKeysFromSessionKey());
  } else {  // Just uses keybox.  Test keybox should already be installed.
    ASSERT_NO_FATAL_FAILURE(
        s->GenerateDerivedKeysFromKeybox(keybox_));
  }
}

}
