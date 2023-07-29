// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

/*********************************************************************
 * level3_file_system.h
 *
 * File system for OEMCrypto Level3 file operations.
 *********************************************************************/

#ifndef LEVEL3_FILE_SYSTEM_H_
#define LEVEL3_FILE_SYSTEM_H_

#include <stdlib.h>

namespace wvoec3 {

class OEMCrypto_Level3FileSystem {
 public:
  virtual ~OEMCrypto_Level3FileSystem() {}
  virtual ssize_t Read(const char *filename, void *buffer, size_t size) = 0;
  virtual ssize_t Write(const char *filename, const void *buffer,
                        size_t size) = 0;
  virtual bool Exists(const char *filename) = 0;
  virtual ssize_t FileSize(const char *filename) = 0;
  virtual bool Remove(const char *filename) = 0;
};

}  // namespace wvoec3

#endif
