// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
// Platform - Abstracts some utilities between platforms.
//
#ifndef WVCDM_UTIL_PLATFORM_H_
#define WVCDM_UTIL_PLATFORM_H_

#include "util_common.h"

#ifdef _WIN32
# include <wtypes.h>
# include <BaseTsd.h>
# include <winsock2.h>  // For htonl and ntohl.
# define __PRETTY_FUNCTION__ __FUNCTION__
# undef NO_ERROR
# undef GetCurrentTime
# undef DeleteFile

using ssize_t = SSIZE_T;

inline void sleep(int seconds) {
  Sleep(seconds * 1000);
}
CORE_UTIL_EXPORT int setenv(const char* key, const char* value, int overwrite);
#else
# include <arpa/inet.h>
# include <sys/types.h>
# include <unistd.h>
#endif

#endif  // WVCDM_UTIL_PLATFORM_H_
