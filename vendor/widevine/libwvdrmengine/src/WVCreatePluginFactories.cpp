//
// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//

#include "WVCreatePluginFactories.h"

#include "WVCryptoFactory.h"
#include "WVDrmFactory.h"

extern "C" {

android::DrmFactory* createDrmFactory() {
  return new wvdrm::WVDrmFactory();
}

android::CryptoFactory* createCryptoFactory() {
  return new wvdrm::WVCryptoFactory();
}

} // extern "C"
