//
// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//

#include "WVCreatePluginFactories.h"

#include "WVCryptoFactory.h"
#include "WVDrmFactory.h"

namespace wvdrm {
namespace hardware {
namespace drm {
namespace V1_2 {
namespace widevine {

extern "C" {

IDrmFactory* createDrmFactory() {
  return new WVDrmFactory();
}

ICryptoFactory* createCryptoFactory() {
  return new WVCryptoFactory();
}

}  // extern "C"
}  // namespace widevine
}  // namespace V1_2
}  // namespace drm
}  // namespace hardware
}  // namespace wvdrm
