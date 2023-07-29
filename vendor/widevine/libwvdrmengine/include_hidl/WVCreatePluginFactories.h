//
// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//

#ifndef WV_CREATE_PLUGIN_FACTORIES_H_
#define WV_CREATE_PLUGIN_FACTORIES_H_

#include "HidlTypes.h"

namespace wvdrm {
namespace hardware {
namespace drm {
namespace V1_2 {
namespace widevine {

extern "C" {
  IDrmFactory* createDrmFactory();
  ICryptoFactory* createCryptoFactory();
}

}  // namespace widevine
}  // namespace V1_2
}  // namespace drm
}  // namespace hardware
}  // namespace wvdrm
#endif // WV_CREATE_PLUGIN_FACTORIES_H_
