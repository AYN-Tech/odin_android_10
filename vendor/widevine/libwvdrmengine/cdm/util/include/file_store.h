// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
// File - Platform independent interface for a File class
//
#ifndef WVCDM_UTIL_FILE_STORE_H_
#define WVCDM_UTIL_FILE_STORE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "disallow_copy_and_assign.h"
#include "platform.h"
#include "util_common.h"

namespace wvcdm {

// File class. The implementation is platform dependent.
class CORE_UTIL_EXPORT File {
 public:
  File() {}
  virtual ~File() {}
  virtual ssize_t Read(char* buffer, size_t bytes) = 0;
  virtual ssize_t Write(const char* buffer, size_t bytes) = 0;

  friend class FileSystem;
  CORE_DISALLOW_COPY_AND_ASSIGN(File);
};

class CORE_UTIL_EXPORT FileSystem {
 public:
  FileSystem();
  FileSystem(const std::string& origin, void* extra_data);
  virtual ~FileSystem();

  class Impl;

  // defines as bit flag
  enum OpenFlags {
    kNoFlags = 0,
    kCreate = 1,
    kReadOnly = 2,  // defaults to read and write access
    kTruncate = 4
  };

  virtual std::unique_ptr<File> Open(const std::string& file_path, int flags);

  virtual bool Exists(const std::string& file_path);
  virtual bool Remove(const std::string& file_path);
  virtual ssize_t FileSize(const std::string& file_path);

  // Return the filenames stored at dir_path.
  // dir_path will be stripped from the returned names.
  virtual bool List(const std::string& dir_path,
                    std::vector<std::string>* names);

  const std::string& origin() const { return origin_; }
  void set_origin(const std::string& origin);

  const std::string& identifier() const { return identifier_; }
  void set_identifier(const std::string& identifier);
  bool IsGlobal() const { return identifier_.empty(); }

 private:
  std::unique_ptr<FileSystem::Impl> impl_;
  std::string origin_;
  std::string identifier_;

  CORE_DISALLOW_COPY_AND_ASSIGN(FileSystem);
};

}  // namespace wvcdm

#endif  // WVCDM_UTIL_FILE_STORE_H_
