//
// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//

#include "WVUUID.h"

#include <string.h>

namespace wvdrm {

bool isWidevineUUID(const uint8_t uuid[16]) {
  static const uint8_t kWidevineUUID[16] = {
    0xED,0xEF,0x8B,0xA9,0x79,0xD6,0x4A,0xCE,
    0xA3,0xC8,0x27,0xDC,0xD5,0x1D,0x21,0xED
  };

  static const uint8_t kOldNetflixWidevineUUID[16] = {
    0x29,0x70,0x1F,0xE4,0x3C,0xC7,0x4A,0x34,
    0x8C,0x5B,0xAE,0x90,0xC7,0x43,0x9A,0x47
  };

  return !memcmp(uuid, kWidevineUUID, 16) ||
      !memcmp(uuid, kOldNetflixWidevineUUID, 16);
}

} // namespace wvdrm
