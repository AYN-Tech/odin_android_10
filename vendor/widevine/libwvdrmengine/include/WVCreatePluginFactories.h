//
// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//

#ifndef WV_CREATE_PLUGIN_FACTORIES_H_
#define WV_CREATE_PLUGIN_FACTORIES_H_

#include "media/drm/DrmAPI.h"
#include "media/hardware/CryptoAPI.h"

extern "C" {
  android::DrmFactory* createDrmFactory();
  android::CryptoFactory* createCryptoFactory();
}

#endif // WV_CREATE_PLUGIN_FACTORIES_H_
