// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
//  Reference implementation of OEMCrypto APIs
//
//  This file contains oemcrypto engine properties that would be for a
//  level 1 device.
#include "oemcrypto_engine_ref.h"

#include <utility>

namespace wvoec_ref {

class L1CryptoEngine : public CryptoEngine {
 public:
  explicit L1CryptoEngine(std::unique_ptr<wvcdm::FileSystem>&& file_system)
      : CryptoEngine(std::move(file_system)) {}

  bool config_local_display_only() { return true; }

  OEMCrypto_HDCP_Capability config_maximum_hdcp_capability() {
    return HDCP_V2;
  }

  bool config_is_anti_rollback_hw_present() { return true; }

  const char* config_security_level() { return "L1"; }

  // This should start at 0, and be incremented only when a security patch has
  // been applied to the device that fixes a security bug.
  uint8_t config_security_patch_level() { return 3; }
};

CryptoEngine* CryptoEngine::MakeCryptoEngine(
    std::unique_ptr<wvcdm::FileSystem>&& file_system) {
  return new L1CryptoEngine(std::move(file_system));
}

}  // namespace wvoec_ref
