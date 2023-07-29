// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

/*********************************************************************
 * level3_file_system_android.h
 *
 * File system for Android for OEMCrypto Level3 File Operations.
 *********************************************************************/

#ifndef LEVEL3_FILE_SYSTEM_ANDROID_H_
#define LEVEL3_FILE_SYSTEM_ANDROID_H_

#include "level3_file_system.h"

#include <memory>

#include "file_store.h"

namespace wvoec3 {

class OEMCrypto_Level3AndroidFileSystem : public OEMCrypto_Level3FileSystem {
 public:
  OEMCrypto_Level3AndroidFileSystem();
  ~OEMCrypto_Level3AndroidFileSystem();
  ssize_t Read(const char *filename, void *buffer, size_t size);
  ssize_t Write(const char *filename, const void *buffer, size_t size);
  bool Exists(const char *filename);
  ssize_t FileSize(const char *filename);
  bool Remove(const char *filename);

 private:
  std::string base_path_;
  std::unique_ptr<wvcdm::FileSystem> file_system_;
};

}  // namespace wvoec3

#endif
