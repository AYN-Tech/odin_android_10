// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
// OEMCrypto device features for unit tests
//
#include "oec_device_features.h"

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>

#include "oec_test_data.h"

namespace wvoec {

DeviceFeatures global_features;

void DeviceFeatures::Initialize(bool is_cast_receiver,
                                bool force_load_test_keybox) {
  cast_receiver = is_cast_receiver;
  uses_keybox = false;
  uses_certificate = false;
  loads_certificate = false;
  generic_crypto = false;
  usage_table = false;
  supports_rsa_3072 = false;
  api_version = 0;
  derive_key_method = NO_METHOD;
  OEMCrypto_SetSandbox(kTestSandbox, sizeof(kTestSandbox));
  if (OEMCrypto_SUCCESS != OEMCrypto_Initialize()) {
    printf("OEMCrypto_Initialize failed. All tests will fail.\n");
    return;
  }
  uint32_t nonce = 0;
  uint8_t buffer[1];
  size_t size = 0;
  provisioning_method = OEMCrypto_GetProvisioningMethod();
  printf("provisioning_method = %s\n",
         ProvisioningMethodName(provisioning_method));
  uses_keybox =
      (OEMCrypto_ERROR_NOT_IMPLEMENTED != OEMCrypto_GetKeyData(buffer, &size));
  printf("uses_keybox = %s.\n", uses_keybox ? "true" : "false");
  OEMCrypto_SESSION session;
  OEMCryptoResult result = OEMCrypto_OpenSession(&session);
  if (result != OEMCrypto_SUCCESS) {
    printf("--- ERROR: Could not open session: %d ----\n", result);
  }
  // If the device uses a keybox, check to see if loading a certificate is
  // installed.
  if (provisioning_method == OEMCrypto_Keybox) {
    loads_certificate =
        (OEMCrypto_ERROR_NOT_IMPLEMENTED !=
         OEMCrypto_RewrapDeviceRSAKey(session, buffer, 0, buffer, 0, &nonce,
                                      buffer, 0, buffer, buffer, &size));
  } else if (provisioning_method == OEMCrypto_OEMCertificate) {
    // If the device says it uses Provisioning 3.0, then it should be able to
    // load a DRM certificate.  These devices must support RewrapDeviceRSAKey30.
    loads_certificate = true;
  } else {
    // Other devices are either broken, or they have a baked in certificate.
    loads_certificate = false;
  }
  printf("loads_certificate = %s.\n", loads_certificate ? "true" : "false");
  uses_certificate = (OEMCrypto_ERROR_NOT_IMPLEMENTED !=
                      OEMCrypto_GenerateRSASignature(session, buffer, 0, buffer,
                                                     &size, kSign_RSASSA_PSS));
  printf("uses_certificate = %s.\n", uses_certificate ? "true" : "false");
  generic_crypto =
      (OEMCrypto_ERROR_NOT_IMPLEMENTED !=
       OEMCrypto_Generic_Encrypt(session, buffer, 0, buffer,
                                 OEMCrypto_AES_CBC_128_NO_PADDING, buffer));
  printf("generic_crypto = %s.\n", generic_crypto ? "true" : "false");
  OEMCrypto_CloseSession(session);
  api_version = OEMCrypto_APIVersion();
  printf("api_version = %d.\n", api_version);
  // These unit tests only work with new usage tables. We do not test v12
  // usage tables.
  if (api_version > 12) usage_table = OEMCrypto_SupportsUsageTable();
  printf("usage_table = %s.\n", usage_table ? "true" : "false");
  if (force_load_test_keybox) {
    derive_key_method = FORCE_TEST_KEYBOX;
  } else {
    PickDerivedKey();
  }
  if (api_version >= 13) {
    uint32_t supported_cert = OEMCrypto_SupportedCertificates();
    if (supported_cert & OEMCrypto_Supports_RSA_CAST) {
      cast_receiver = true;
    }
    if (supported_cert & OEMCrypto_Supports_RSA_3072bit) {
      supports_rsa_3072 = true;
    }
  }
  printf("cast_receiver = %s.\n", cast_receiver ? "true" : "false");
  resource_rating = OEMCrypto_ResourceRatingTier();
  printf("resource_rating = %d, security level %s.\n", resource_rating,
         OEMCrypto_SecurityLevel());
  uint32_t decrypt_hash_type = OEMCrypto_SupportsDecryptHash();
  supports_crc = (decrypt_hash_type == OEMCrypto_CRC_Clear_Buffer);
  if (supports_crc) {
    printf("Decrypt hashes will be tested.\n");
  } else {
    printf("Decrypt hashes will not be tested -- %s.\n",
           decrypt_hash_type == OEMCrypto_Hash_Not_Supported
               ? "not supported"
               : "partner defined hash");
  }
  switch (derive_key_method) {
    case NO_METHOD:
      printf("NO_METHOD: Cannot derive known session keys.\n");
      // Note: cast_receiver left unchanged because set by user on command line.
      uses_keybox = false;
      uses_certificate = false;
      loads_certificate = false;
      generic_crypto = false;
      usage_table = false;
      break;
    case LOAD_TEST_KEYBOX:
      printf("LOAD_TEST_KEYBOX: Call LoadTestKeybox before deriving keys.\n");
      break;
    case LOAD_TEST_RSA_KEY:
      printf("LOAD_TEST_RSA_KEY: Call LoadTestRSAKey before deriving keys.\n");
      break;
    case FORCE_TEST_KEYBOX:
      printf("FORCE_TEST_KEYBOX: User requested calling InstallKeybox.\n");
      break;
    case TEST_PROVISION_30:
      printf("TEST_PROVISION_30: Device provisioed with OEM Cert.\n");
      break;
  }
  std::string security_level = OEMCrypto_SecurityLevel();
  supports_level_1 = (security_level == "L1");
  printf("SecurityLevel is %s (%s)\n",
         supports_level_1 ? "Level 1" : "Not Level 1",
         security_level.c_str());
  OEMCrypto_Terminate();
}

std::string DeviceFeatures::RestrictFilter(const std::string& initial_filter) {
  std::string filter = initial_filter;
  if (!uses_keybox)                   FilterOut(&filter, "*KeyboxTest*");
  if (derive_key_method
      != FORCE_TEST_KEYBOX)           FilterOut(&filter, "*ForceKeybox*");
  if (!uses_certificate)              FilterOut(&filter, "OEMCrypto*Cert*");
  if (!loads_certificate)             FilterOut(&filter, "OEMCryptoLoadsCert*");
  if (!generic_crypto)                FilterOut(&filter, "*GenericCrypto*");
  if (!cast_receiver)                 FilterOut(&filter, "*CastReceiver*");
  if (!usage_table)                   FilterOut(&filter, "*UsageTable*");
  if (derive_key_method == NO_METHOD) FilterOut(&filter, "*SessionTest*");
  if (provisioning_method
      != OEMCrypto_OEMCertificate)    FilterOut(&filter, "*Prov30*");
  if (!supports_rsa_3072)             FilterOut(&filter, "*RSAKey3072*");
  if (api_version <  9)               FilterOut(&filter, "*API09*");
  if (api_version < 10)               FilterOut(&filter, "*API10*");
  if (api_version < 11)               FilterOut(&filter, "*API11*");
  if (api_version < 12)               FilterOut(&filter, "*API12*");
  if (api_version < 13)               FilterOut(&filter, "*API13*");
  if (api_version < 14)               FilterOut(&filter, "*API14*");
  if (api_version < 15)               FilterOut(&filter, "*API15*");
  // Some tests may require root access. If user is not root, filter these tests
  // out.
  if (getuid()) {
    FilterOut(&filter, "UsageTableTest.TimeRollbackPrevention");
  }
  // Performance tests take a long time.  Filter them out if they are not
  // specifically requested.
  if (filter.find("Performance") == std::string::npos) {
    FilterOut(&filter, "*Performance*");
  }
  return filter;
}

void DeviceFeatures::PickDerivedKey() {
  if (api_version >= 12) {
    switch (provisioning_method) {
      case OEMCrypto_OEMCertificate:
        derive_key_method = TEST_PROVISION_30;
        return;
      case OEMCrypto_DrmCertificate:
        if (OEMCrypto_ERROR_NOT_IMPLEMENTED != OEMCrypto_LoadTestRSAKey()) {
          derive_key_method = LOAD_TEST_RSA_KEY;
        }
        return;
      case OEMCrypto_Keybox:
        // Fall through to api_version < 12 case.
        break;
      case OEMCrypto_ProvisioningError:
        printf(
            "ERROR: OEMCrypto_GetProvisioningMethod() returns "
            "OEMCrypto_ProvisioningError\n");
        // Then fall through to api_version < 12 case.
        break;
    }
  }
  if (uses_keybox) {
    // If device uses a keybox, try to load the test keybox.
    if (OEMCrypto_ERROR_NOT_IMPLEMENTED != OEMCrypto_LoadTestKeybox(NULL, 0)) {
      derive_key_method = LOAD_TEST_KEYBOX;
    }
  } else if (OEMCrypto_ERROR_NOT_IMPLEMENTED != OEMCrypto_LoadTestRSAKey()) {
    derive_key_method = LOAD_TEST_RSA_KEY;
  }
}

void DeviceFeatures::FilterOut(std::string* current_filter,
                               const std::string& new_filter) {
  if (current_filter->find('-') == std::string::npos) {
    *current_filter += "-" + new_filter;
  } else {
    *current_filter += ":" + new_filter;
  }
}

const char* ProvisioningMethodName(OEMCrypto_ProvisioningMethod method) {
  switch (method) {
    case OEMCrypto_ProvisioningError:
      return "OEMCrypto_ProvisioningError";
    case OEMCrypto_DrmCertificate:
      return "OEMCrypto_DrmCertificate";
    case OEMCrypto_Keybox:
      return "OEMCrypto_Keybox";
    case OEMCrypto_OEMCertificate:
      return "OEMCrypto_OEMCertificate";
  }
  // Not reachable
  return "";
}

}  // namespace wvoec
