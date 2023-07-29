#ifndef CDM_OEC_DEVICE_FEATURES_H_
#define CDM_OEC_DEVICE_FEATURES_H_

#include <string>

#include "OEMCryptoCENC.h"
#include "oemcrypto_types.h"

namespace wvoec {

// Keeps track of which features are supported by the version of OEMCrypto being
// tested.  See the integration guide for a list of optional features.
class DeviceFeatures {
 public:
  // There are several possible methods used to derive a set of known session
  // keys.  For example, the test can install a known test keybox, or it can
  // parse the OEM certificate.
  enum DeriveMethod {      // Method to use derive session keys.
    NO_METHOD,             // Cannot derive known session keys.
    LOAD_TEST_KEYBOX,      // Call LoadTestKeybox before deriving keys.
    LOAD_TEST_RSA_KEY,     // Call LoadTestRSAKey before deriving keys.
    FORCE_TEST_KEYBOX,     // User requested calling InstallKeybox.
    TEST_PROVISION_30,     // Device has OEM Certificate installed.
  };

  enum DeriveMethod derive_key_method;
  bool uses_keybox;        // Device uses a keybox to derive session keys.
  bool uses_certificate;   // Device uses a certificate to derive session keys.
  bool loads_certificate;  // Device can load a certificate from the server.
  bool generic_crypto;     // Device supports generic crypto.
  bool cast_receiver;      // Device supports alternate rsa signature padding.
  bool usage_table;        // Device saves usage information.
  bool supports_rsa_3072;  // Device supports 3072 bit RSA keys.
  bool supports_level_1;   // Device supports Level 1 security.
  uint32_t resource_rating; // Device's resource rating tier.
  bool supports_crc;        // Supported decrypt hash type CRC.
  uint32_t api_version;
  OEMCrypto_ProvisioningMethod provisioning_method;

  // This should be called from the test program's main procedure.
  void Initialize(bool is_cast_receiver, bool force_load_test_keybox);
  // Generate a GTest filter of tests that should not be run. This should be
  // called after Initialize.  Tests are filtered out based on which features
  // are not supported.  For example, a device that uses Provisioning 3.0 will
  // have all keybox tests filtered out.
  std::string RestrictFilter(const std::string& initial_filter);

 private:
  // Decide which method should be used to derive session keys, based on
  // supported featuers.
  void PickDerivedKey();
  // Add a GTest filter restriction to the current filter.
  void FilterOut(std::string* current_filter, const std::string& new_filter);
};

// There is one global set of features for the version of OEMCrypto being
// tested.  This should be initialized in the test program's main procedure.
extern DeviceFeatures global_features;
const char* ProvisioningMethodName(OEMCrypto_ProvisioningMethod method);

}  // namespace wvoec

#endif  // CDM_OEC_DEVICE_FEATURES_H_
