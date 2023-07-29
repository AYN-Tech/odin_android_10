// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
// OEMCrypto unit tests - extra tests required for Android platform.
//
// The Widevine CDM system can be built on many platforms, with different
// capabilities. For example, some platforms do not require usage tables,
// and some can have a pre-installed certificate and do not need a keybox.
// On Android, these features are not optional.  This set of unit tests
// verify that these features are implemented.
//
// In the file oemcrypto_test.cpp, the unit tests only verify correct
// functionality for functions that are implemented.  Android devices must pass
// unit tests in both files.

#include <gtest/gtest.h>
#include "oec_test_data.h"
#include "OEMCryptoCENC.h"

namespace wvoec {

// These tests are required for LollyPop Android devices.
class OEMCryptoAndroidLMPTest : public ::testing::Test {
 protected:
  void SetUp() override {
    OEMCrypto_SetSandbox(kTestSandbox, sizeof(kTestSandbox));
    ASSERT_EQ(OEMCrypto_SUCCESS, OEMCrypto_Initialize());
  }

  void TearDown() override { OEMCrypto_Terminate(); }
};

// Android devices must have a keybox, or use provisioning 3.0.
TEST_F(OEMCryptoAndroidLMPTest, GetKeyDataImplemented) {
  uint8_t key_data[256];
  size_t key_data_len = sizeof(key_data);
  if (OEMCrypto_Keybox == OEMCrypto_GetProvisioningMethod()) {
    ASSERT_NE(OEMCrypto_ERROR_NOT_IMPLEMENTED,
              OEMCrypto_GetKeyData(key_data, &key_data_len));
  } else {
    ASSERT_EQ(OEMCrypto_OEMCertificate, OEMCrypto_GetProvisioningMethod());
  }
}

// Android devices must have a valid keybox.
TEST_F(OEMCryptoAndroidLMPTest, ValidKeybox) {
  if (OEMCrypto_GetProvisioningMethod() == OEMCrypto_Keybox) {
    ASSERT_EQ(OEMCrypto_SUCCESS, OEMCrypto_IsKeyboxValid());
  }
}

TEST_F(OEMCryptoAndroidLMPTest, MinVersionNumber9) {
  uint32_t version = OEMCrypto_APIVersion();
  ASSERT_LE(9u, version);
}

TEST_F(OEMCryptoAndroidLMPTest, ValidKeyboxTest) {
  ASSERT_EQ(OEMCrypto_SUCCESS, OEMCrypto_IsKeyboxValid());
}

TEST_F(OEMCryptoAndroidLMPTest, RewrapDeviceRSAKeyImplemented) {
  if (OEMCrypto_Keybox == OEMCrypto_GetProvisioningMethod()) {
    ASSERT_NE(OEMCrypto_ERROR_NOT_IMPLEMENTED,
              OEMCrypto_RewrapDeviceRSAKey(0, NULL, 0, NULL, 0, NULL, NULL, 0,
                                           NULL, NULL, NULL));
  } else {
    ASSERT_NE(OEMCrypto_ERROR_NOT_IMPLEMENTED,
              OEMCrypto_RewrapDeviceRSAKey30(0, NULL, NULL, 0, NULL, 0, NULL,
                                             NULL, NULL));
  }
}

// This verifies that the device can load a DRM Certificate.
TEST_F(OEMCryptoAndroidLMPTest, RSASignatureImplemented) {
  ASSERT_NE(
      OEMCrypto_ERROR_NOT_IMPLEMENTED,
      OEMCrypto_GenerateRSASignature(0, NULL, 0, NULL, NULL, kSign_RSASSA_PSS));
}

// The Generic Crypto API functions are required for Android.
TEST_F(OEMCryptoAndroidLMPTest, GenericCryptoImplemented) {
  ASSERT_NE(OEMCrypto_ERROR_NOT_IMPLEMENTED,
            OEMCrypto_Generic_Encrypt(0, NULL, 0, NULL,
                                      OEMCrypto_AES_CBC_128_NO_PADDING, NULL));
  ASSERT_NE(OEMCrypto_ERROR_NOT_IMPLEMENTED,
            OEMCrypto_Generic_Decrypt(0, NULL, 0, NULL,
                                      OEMCrypto_AES_CBC_128_NO_PADDING, NULL));
  ASSERT_NE(
      OEMCrypto_ERROR_NOT_IMPLEMENTED,
      OEMCrypto_Generic_Sign(0, NULL, 0, OEMCrypto_HMAC_SHA256, NULL, NULL));
  ASSERT_NE(
      OEMCrypto_ERROR_NOT_IMPLEMENTED,
      OEMCrypto_Generic_Verify(0, NULL, 0, OEMCrypto_HMAC_SHA256, NULL, 0));
}

// Android requires support of usage table.  The usage table is used for Secure
// Stops and for offline licenses.
TEST_F(OEMCryptoAndroidLMPTest, SupportsUsageTable) {
  ASSERT_TRUE(OEMCrypto_SupportsUsageTable());
}

// Android devices require L1 OEMCrypto.
TEST_F(OEMCryptoAndroidLMPTest, Level1Required) {
  const char* char_level = OEMCrypto_SecurityLevel();
  std::string security_level(char_level ? char_level : "");
  EXPECT_EQ("L1", security_level)
      << "The security level is " << security_level << ". but we expect L1.\n"
      << "If you are testing a device that should be L3 or L2, please\n"
      << "repeat the tests with the flag --gtest_filter=\"*-*Level1Required\"";
}

// These tests are required for M Android devices.
class OEMCryptoAndroidMNCTest : public OEMCryptoAndroidLMPTest {};

TEST_F(OEMCryptoAndroidMNCTest, MinVersionNumber10) {
  uint32_t version = OEMCrypto_APIVersion();
  ASSERT_GE(version, 10u);
}

// Android devices using Provisioning 2.0 must be able to load a test keybox.
// If they are not using Provisioning 2.0, then they must use Provisioning 3.0.
TEST_F(OEMCryptoAndroidMNCTest, LoadsTestKeyboxImplemented) {
  if (OEMCrypto_Keybox == OEMCrypto_GetProvisioningMethod()) {
    ASSERT_EQ(OEMCrypto_SUCCESS, OEMCrypto_LoadTestKeybox(
        reinterpret_cast<const uint8_t*>(&kTestKeybox),
        sizeof(kTestKeybox)));
  } else {
    // Android should use keybox or provisioning 3.0.
    ASSERT_EQ(OEMCrypto_OEMCertificate, OEMCrypto_GetProvisioningMethod());
  }
}

// Android requires implementation of these functions.
TEST_F(OEMCryptoAndroidMNCTest, NumberOfSessionsImplemented) {
  ASSERT_NE(OEMCrypto_ERROR_NOT_IMPLEMENTED,
            OEMCrypto_GetNumberOfOpenSessions(NULL));
  ASSERT_NE(OEMCrypto_ERROR_NOT_IMPLEMENTED,
            OEMCrypto_GetMaxNumberOfSessions(NULL));
}

// Android requires implementation of these functions.
TEST_F(OEMCryptoAndroidMNCTest, QueryKeyControlImplemented) {
  ASSERT_NE(OEMCrypto_ERROR_NOT_IMPLEMENTED,
            OEMCrypto_QueryKeyControl(0, NULL, 0, NULL, NULL));
}

// These tests are required for N Android devices.
class OEMCryptoAndroidNYCTest : public OEMCryptoAndroidMNCTest {};

TEST_F(OEMCryptoAndroidNYCTest, MinVersionNumber11) {
  uint32_t version = OEMCrypto_APIVersion();
  ASSERT_GE(version, 11u);
}

// These tests are required for O MR1 Android devices.
class OEMCryptoAndroidOCTest : public OEMCryptoAndroidNYCTest {};

TEST_F(OEMCryptoAndroidOCTest, MinVersionNumber13) {
  uint32_t version = OEMCrypto_APIVersion();
  ASSERT_GE(version, 13u);
}

// These tests are required for Q Android devices.
class OEMCryptoAndroidQTest : public OEMCryptoAndroidOCTest {};

TEST_F(OEMCryptoAndroidQTest, MinVersionNumber14) {
  uint32_t version = OEMCrypto_APIVersion();
  ASSERT_GE(version, 15u);
}

}  // namespace wvoec
