// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#include "platform.h"

#include "stdlib.h"

#ifdef _WIN32

int setenv(const char* key, const char* value, int overwrite) {
  if (!overwrite) {
    size_t size;
    errno_t err = getenv_s(&size, nullptr, 0, key);
    if (err != 0 || size != 0)
      return err;  // Return 0 if it exists, but don't change.
  }
  return _putenv_s(key, value);
}

#endif
