// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
//  Reference implementation of OEMCrypto APIs
//
//  This file contains oemcrypto engine properties that would be for a
//  level 2 device that does not have persistant storage or a keybox.
//  Note: this is for illustration only.  Production devices are rarely level 2.

#include "oemcrypto_engine_ref.h"

#include <string.h>

#include <utility>

#include "log.h"
#include "oem_cert.h"

namespace wvoec_ref {

class Prov30CryptoEngine : public CryptoEngine {
 public:
  explicit Prov30CryptoEngine(std::unique_ptr<wvcdm::FileSystem>&& file_system)
      : CryptoEngine(std::move(file_system)) {}

  bool config_local_display_only() { return true; }

  // Returns the max HDCP version supported.
  OEMCrypto_HDCP_Capability config_maximum_hdcp_capability() {
    return HDCP_NO_DIGITAL_OUTPUT;
  }

  // Returns true if the client supports persistent storage of
  // offline usage table information.
  bool config_supports_usage_table() {
    return false;
  }

  // Returns true if the client uses a keybox as the root of trust.
  bool config_supports_keybox() {
    return false;
  }

  // This version uses an OEM Certificate.
  OEMCrypto_ProvisioningMethod config_provisioning_method() {
    return OEMCrypto_OEMCertificate;
  }

  OEMCryptoResult get_oem_certificate(SessionContext* session,
                                      uint8_t* public_cert,
                                      size_t* public_cert_length) {
    if (kOEMPublicCertSize == 0) {
      return OEMCrypto_ERROR_NOT_IMPLEMENTED;
    }
    if (public_cert_length == NULL) {
      return OEMCrypto_ERROR_UNKNOWN_FAILURE;
    }
    if (*public_cert_length < kOEMPublicCertSize) {
      *public_cert_length = kOEMPublicCertSize;
      return OEMCrypto_ERROR_SHORT_BUFFER;
    }
    *public_cert_length = kOEMPublicCertSize;
    if (public_cert == NULL) {
      return OEMCrypto_ERROR_SHORT_BUFFER;
    }
    memcpy(public_cert, kOEMPublicCert, kOEMPublicCertSize);
    if (!session->LoadRSAKey(kOEMPrivateKey, kOEMPrivateKeySize)) {
      LOGE("Private RSA Key did not load correctly.");
      return OEMCrypto_ERROR_INVALID_RSA_KEY;
    }
    return OEMCrypto_SUCCESS;
  }

  // Returns "L3" for a software only library.  L1 is for hardware protected
  // keys and data paths.  L2 is for hardware protected keys but no data path
  // protection.
  const char* config_security_level() { return "L2"; }
};

CryptoEngine* CryptoEngine::MakeCryptoEngine(
    std::unique_ptr<wvcdm::FileSystem>&& file_system) {
  return new Prov30CryptoEngine(std::move(file_system));
}

}  // namespace wvoec_ref
