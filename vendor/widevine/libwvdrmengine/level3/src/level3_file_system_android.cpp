#include "level3_file_system_android.h"

#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <cstring>
#include <string>

#include "log.h"
#include "properties.h"
#include "wv_cdm_types.h"

using wvcdm::File;
using wvcdm::FileSystem;

namespace wvoec3 {

OEMCrypto_Level3AndroidFileSystem::OEMCrypto_Level3AndroidFileSystem()
    : file_system_(new FileSystem()) {
  const char kDirectoryDelimiter = '/';
  wvcdm::Properties::GetDeviceFilesBasePath(wvcdm::kSecurityLevelL3,
                                            &base_path_);
  size_t size = base_path_.size();
  if (size < 2) {
    // Default value is set to make sure unit tests pass, running as root.
    base_path_ = "/data/";
    size = base_path_.size();
  } else if (base_path_[size - 1] != kDirectoryDelimiter) {
    base_path_ = base_path_ + "/";
  }
  size_t pos = base_path_.find(kDirectoryDelimiter, 1);
  while (pos < size) {
    base_path_[pos] = '\0';
    if (mkdir(base_path_.c_str(), 0775) != 0 && errno != EEXIST) {
      wvcdm::Log(
          "", "", 0, wvcdm::LOG_ERROR,
          "Could not create base directories for Level3FileSystem, error: %s\n",
          strerror(errno));
    }
    base_path_[pos] = kDirectoryDelimiter;
    pos = base_path_.find(kDirectoryDelimiter, pos + 1);
  }
}

OEMCrypto_Level3AndroidFileSystem::~OEMCrypto_Level3AndroidFileSystem() {}

ssize_t OEMCrypto_Level3AndroidFileSystem::Read(const char *filename,
                                                void *buffer, size_t size) {
  auto file = file_system_->Open(base_path_ + std::string(filename),
                                 FileSystem::kReadOnly);
  ssize_t bytes_read = 0;
  if (file) {
    bytes_read = file->Read(static_cast<char *>(buffer), size);
  }
  return bytes_read;
}

ssize_t OEMCrypto_Level3AndroidFileSystem::Write(const char *filename,
                                                 const void *buffer,
                                                 size_t size) {
  auto file = file_system_->Open(base_path_ + std::string(filename),
                                 FileSystem::kCreate | FileSystem::kTruncate);
  ssize_t bytes_written = 0;
  if (file) {
    bytes_written = file->Write(static_cast<const char *>(buffer), size);
  }
  return bytes_written;
}

bool OEMCrypto_Level3AndroidFileSystem::Exists(const char *filename) {
  return file_system_->Exists(base_path_ + std::string(filename));
}

ssize_t OEMCrypto_Level3AndroidFileSystem::FileSize(const char *filename) {
  return file_system_->FileSize(base_path_ + std::string(filename));
}

bool OEMCrypto_Level3AndroidFileSystem::Remove(const char *filename) {
  return file_system_->Remove(base_path_ + std::string(filename));
}
}  // namespace wvoec3
