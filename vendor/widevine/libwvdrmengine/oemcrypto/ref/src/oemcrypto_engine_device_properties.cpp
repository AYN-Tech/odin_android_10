// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
//  Reference implementation of OEMCrypto APIs
//

#include "oemcrypto_engine_ref.h"

#include <utility>

namespace wvoec_ref {

CryptoEngine* CryptoEngine::MakeCryptoEngine(
    std::unique_ptr<wvcdm::FileSystem>&& file_system) {
  return new CryptoEngine(std::move(file_system));
}

}  // namespace wvoec_ref
