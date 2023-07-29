//
// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//

//#define LOG_NDEBUG 0
#define LOG_TAG "WVCdm"
#include <utils/Log.h>

#include "WVCryptoFactory.h"

#include <dlfcn.h>

#include "utils/Errors.h"
#include "WVCDMSingleton.h"
#include "WVCryptoPlugin.h"
#include "WVUUID.h"

namespace wvdrm {

using namespace android;

bool WVCryptoFactory::isCryptoSchemeSupported(const uint8_t uuid[16]) const {
  return isWidevineUUID(uuid);
}

status_t WVCryptoFactory::createPlugin(const uint8_t uuid[16], const void* data,
                                       size_t size, CryptoPlugin** plugin) {
  if (!isCryptoSchemeSupported(uuid)) {
    *plugin = NULL;
    return BAD_VALUE;
  }

  *plugin = new WVCryptoPlugin(data, size, getCDM());
  return OK;
}

} // namespace wvdrm
