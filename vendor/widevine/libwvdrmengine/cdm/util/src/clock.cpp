// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
// Clock - implemented using the standard linux time library

#include "clock.h"

#include <sys/time.h>

namespace wvcdm {

int64_t Clock::GetCurrentTime() {
  struct timeval tv;
  tv.tv_sec = tv.tv_usec = 0;
  gettimeofday(&tv, NULL);
  return tv.tv_sec;
}

}  // namespace wvcdm
