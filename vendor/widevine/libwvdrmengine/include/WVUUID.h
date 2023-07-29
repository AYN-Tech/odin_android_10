//
// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//

#ifndef WV_UUID_H_
#define WV_UUID_H_

#include <stdint.h>

namespace wvdrm {

bool isWidevineUUID(const uint8_t uuid[16]);

} // namespace wvdrm

#endif // WV_UUID_H_
