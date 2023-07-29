// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#include <string>
#include <vector>

#include "wv_cdm_types.h"

namespace wvcdm {

const char kCurrentDirectory[] = ".";
const char kParentDirectory[] = "..";
const char kDirectoryDelimiter = '/';
const char kWildcard[] = "*";
bool IsCurrentOrParentDirectory(char* dir);

class FileUtils {
 public:
  static bool Exists(const std::string& src);
  static bool Remove(const std::string& src);
  static bool Copy(const std::string& src, const std::string& dest);
  static bool List(const std::string& path, std::vector<std::string>* files);
  static bool IsRegularFile(const std::string& path);
  static bool IsDirectory(const std::string& path);
  static bool CreateDirectory(const std::string& path);
};

}  // namespace wvcdm
