// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
// Clock - Platform independent interface for a time library
//
#ifndef WVCDM_UTIL_CLOCK_H_
#define WVCDM_UTIL_CLOCK_H_

#include <stdint.h>

namespace wvcdm {

// Provides time related information. The implementation is platform dependent.
class Clock {
 public:
  Clock() {}
  virtual ~Clock() {}

  // Provides the number of seconds since an epoch - 01/01/1970 00:00 UTC
  virtual int64_t GetCurrentTime();
};

}  // namespace wvcdm

#endif  // WVCDM_UTIL_CLOCK_H_
