// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#ifndef WVCDM_CORE_TEST_BASE_H_
#define WVCDM_CORE_TEST_BASE_H_

#include <gtest/gtest.h>

#include "cdm_engine.h"
#include "config_test_env.h"
#include "crypto_session.h"
#include "metrics_collections.h"
#include "oec_session_util.h"
#include "string_conversions.h"

namespace wvcdm {
// This is the base class for Widevine CDM integration tests.  It's main use is
// to configure OEMCrypto to use a test keybox.
class WvCdmTestBase : public ::testing::Test {
 public:
  WvCdmTestBase() : config_(default_config_), binary_provisioning_(false) {}
  ~WvCdmTestBase() override {}
  void SetUp() override;
  virtual std::string binary_key_id() const { return a2bs_hex(config_.key_id()); }

  // Returns true if the test program should continue, if false, the caller
  // should exit.  This should be called by main() to allow the user to pass in
  // command line switches.
  static bool Initialize(int argc, char **argv);
  // Install a test keybox, if appropriate.
  static void InstallTestRootOfTrust();

  // Send provisioning request to the server and handle response.
  virtual void Provision();
  // Calls Provision() if not already provisioned.
  virtual void EnsureProvisioned();

  // Fill a buffer with some nonconstant data of the given size. The first byte
  // will be set to <init> to help you find the buffer when debugging.
  static void StripeBuffer(std::vector<uint8_t>* buffer, size_t size,
                           uint8_t init);

  // Helper method for doing cryptography.
  static std::string Aes128CbcEncrypt(std::vector<uint8_t> key,
                                      const std::vector<uint8_t>& clear,
                                      const std::vector<uint8_t> iv);
  // Helper method for doing cryptography.
  static std::string Aes128CbcDecrypt(std::vector<uint8_t> key,
                                      const std::vector<uint8_t>& clear,
                                      const std::vector<uint8_t> iv);
  // Helper method for doing cryptography.
  static std::string SignHMAC(const std::string& message,
                              const std::vector<uint8_t>& key);

  // The default test configuration.  This is influenced by command line
  // arguments before any tests are created.
  static ConfigTestEnv default_config_;

  // Configuration for an individual test.  This is initialized to be the
  // default configuration, but can be modified by the test itself.
  ConfigTestEnv config_;

  // This should be set by test subclasses BEFORE calling SetUp -- i.e. in the
  // tests's constructor.
  bool binary_provisioning_;
};

class TestCryptoSession  : public CryptoSession {
 public:
  explicit TestCryptoSession(metrics::CryptoMetrics* crypto_metrics);
  // This intercepts nonce flood errors, which is useful for tests that request
  // many nonces and are not time critical.
  CdmResponseType GenerateNonce(uint32_t* nonce);
};

// A holder for a license.  Users of this class will first open a session with
// OpenSession, then generate a key request with GenerateKeyRequest, and then
// call CreateDefaultLicense to create a bare-bones license with no keys in it.
// The user may then access the license to adjust the policy, or use AddKey to
// add keys to the license.  The license is then loaded via SignAndLoadLicense.
class TestLicenseHolder {
 public:
  // cdm_engine must exist and outlive the TestLicenseHolder.
  TestLicenseHolder(CdmEngine *cdm_engine);
  ~TestLicenseHolder();
  // Caller must ensure device already provisioned.
  void OpenSession(const std::string& key_system);
  void CloseSession();
  // Use the cdm_engine to generate a key request in the session.  This should
  // be called after OpenSession.  This saves the signed license request, so
  // that the DRM certificate can be extracted in CreateDefaultLicense.
  void GenerateKeyRequest(const std::string& key_id,
                          const std::string& init_data_type_string);
  // Create a bare-bones license from the license request. After this, the user
  // may access and modify the license using license() below.
  void CreateDefaultLicense();
  // Sign the license using the DRM certificate's RSA key.  Then the license is
  // passed to the cdm_engine using AddKey.  After this, the license is loaded
  // and the keys may be used.
  void SignAndLoadLicense();

  // The session id.  This is only valid after a call to OpenSession.
  const std::string& session_id() { return session_id_; }
  // The license protobuf.  This is only valid after CreateDefaultLicense.
  video_widevine::License* license() { return &license_; };
  // Add a key with the given key control block and key data.
  // If the block's verification is empty, it will be set to a valid value.
  // The key data is encrypted correctly.
  video_widevine::License_KeyContainer* AddKey(
      const KeyId& key_id, const std::vector<uint8_t>& key_data,
      const wvoec::KeyControlBlock& block);

 private:
  // Helper method to generate mac keys and encryption keys for the license.
  void DeriveKeysFromSessionKey();
  // Derive a single mac key or encryption key using CMAC.
  bool DeriveKey(const std::vector<uint8_t>& key,
                 const std::vector<uint8_t>& context, int counter,
                 std::vector<uint8_t>* out);
  // Add the mac keys to the license.
  void AddMacKey();

  CdmEngine* cdm_engine_;
  std::string signed_license_request_data_;
  std::string license_request_data_;
  std::string session_id_;
  bool session_opened_;
  RsaPublicKey rsa_key_;  // From the DRM Certificate.
  video_widevine::License license_;
  std::vector<uint8_t> derived_mac_key_server_;
  std::vector<uint8_t> derived_mac_key_client_;
  std::vector<uint8_t> mac_key_server_;
  std::vector<uint8_t> mac_key_client_;
  std::vector<uint8_t> enc_key_;
  std::vector<uint8_t> session_key_;
};

}  // namespace wvcdm

#endif  // WVCDM_CORE_TEST_BASE_H_
