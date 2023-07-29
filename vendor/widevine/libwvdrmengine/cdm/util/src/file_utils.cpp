// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#include "file_utils.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>

#include "file_store.h"
#include "log.h"
#include "properties.h"

namespace wvcdm {

bool IsCurrentOrParentDirectory(char* dir) {
  return strcmp(dir, kCurrentDirectory) == 0 ||
         strcmp(dir, kParentDirectory) == 0;
}

bool FileUtils::Exists(const std::string& path) {
  struct stat buf;
  int res = stat(path.c_str(), &buf) == 0;
  if (!res) {
    LOGV("File::Exists: stat failed: %d, %s", errno, strerror(errno));
  }
  return res;
}

bool FileUtils::Remove(const std::string& path) {
  if (FileUtils::IsDirectory(path)) {
    // Handle directory deletion
    DIR* dir;
    if ((dir = opendir(path.c_str())) != NULL) {
      // first remove files and dir within it
      struct dirent* entry;
      while ((entry = readdir(dir)) != NULL) {
        if (!IsCurrentOrParentDirectory(entry->d_name)) {
          std::string path_to_remove = path + kDirectoryDelimiter;
          path_to_remove += entry->d_name;
          if (!Remove(path_to_remove)) {
            closedir(dir);
            return false;
          }
        }
      }
      closedir(dir);
    }
    if (rmdir(path.c_str())) {
      LOGW("File::Remove: rmdir failed: %d, %s", errno, strerror(errno));
      return false;
    }
    return true;
  } else {
    size_t wildcard_pos = path.find(kWildcard);
    if (wildcard_pos == std::string::npos) {
      // Handle file deletion
      if (unlink(path.c_str()) && (errno != ENOENT)) {
        LOGW("File::Remove: unlink failed: %d, %s", errno, strerror(errno));
        return false;
      }
    } else {
      // Handle wildcard specified file deletion
      size_t delimiter_pos = path.rfind(kDirectoryDelimiter, wildcard_pos);
      if (delimiter_pos == std::string::npos) {
        LOGW("File::Remove: unable to find path delimiter before wildcard");
        return false;
      }

      DIR* dir;
      std::string dir_path = path.substr(0, delimiter_pos);
      if ((dir = opendir(dir_path.c_str())) == NULL) {
        LOGW("File::Remove: directory open failed for wildcard");
        return false;
      }

      struct dirent* entry;
      std::string ext = path.substr(wildcard_pos + 1);

      while ((entry = readdir(dir)) != NULL) {
        size_t filename_len = strlen(entry->d_name);
        if (filename_len > ext.size()) {
          if (strcmp(entry->d_name + filename_len - ext.size(), ext.c_str()) ==
              0) {
            std::string file_path_to_remove =
                dir_path + kDirectoryDelimiter + entry->d_name;
            if (!Remove(file_path_to_remove)) {
              closedir(dir);
              return false;
            }
          }
        }
      }
      closedir(dir);
    }
    return true;
  }
}

bool FileUtils::Copy(const std::string& src, const std::string& dest) {
  struct stat stat_buf;
  if (stat(src.c_str(), &stat_buf)) {
    LOGV("File::Copy: file %s stat error: %d, %s", src.c_str(), errno,
         strerror(errno));
    return false;
  }

  int fd_src = open(src.c_str(), O_RDONLY);
  if (fd_src < 0) {
    LOGW("File::Copy: unable to open file %s: %d, %s", src.c_str(), errno,
         strerror(errno));
    return false;
  }

  int fd_dest = open(dest.c_str(), O_WRONLY | O_CREAT, stat_buf.st_mode);
  if (fd_dest < 0) {
    LOGW("File::Copy: unable to open file %s: %d, %s", dest.c_str(), errno,
         strerror(errno));
    close(fd_src);
    return false;
  }

  off_t offset = 0;
  bool status = true;
  if (sendfile(fd_dest, fd_src, &offset, stat_buf.st_size) < 0) {
    LOGV("File::Copy: unable to copy %s to %s: %d, %s", src.c_str(),
         dest.c_str(), errno, strerror(errno));
    status = false;
  }

  close(fd_src);
  close(fd_dest);
  return status;
}

bool FileUtils::List(const std::string& path, std::vector<std::string>* files) {
  if (NULL == files) {
    LOGV("File::List: files destination not provided");
    return false;
  }

  if (!FileUtils::Exists(path)) {
    LOGV("File::List: path %s does not exist: %d, %s", path.c_str(), errno,
         strerror(errno));
    return false;
  }

  DIR* dir = opendir(path.c_str());
  if (dir == NULL) {
    LOGW("File::List: unable to open directory %s: %d, %s", path.c_str(), errno,
         strerror(errno));
    return false;
  }

  files->clear();
  struct dirent* entry;
  while ((entry = readdir(dir)) != NULL) {
    if (!IsCurrentOrParentDirectory(entry->d_name)) {
      files->push_back(entry->d_name);
    }
  }
  closedir(dir);

  return true;
}

bool FileUtils::IsRegularFile(const std::string& path) {
  struct stat buf;
  if (stat(path.c_str(), &buf) == 0)
    return buf.st_mode & S_IFREG;
  else
    return false;
}

bool FileUtils::IsDirectory(const std::string& path) {
  struct stat buf;
  if (stat(path.c_str(), &buf) == 0)
    return buf.st_mode & S_IFDIR;
  else
    return false;
}

bool FileUtils::CreateDirectory(const std::string& path_in) {
  std::string path = path_in;
  size_t size = path.size();
  if ((size == 1) && (path[0] == kDirectoryDelimiter)) return true;

  if (size <= 1) return false;

  size_t pos = path.find(kDirectoryDelimiter, 1);
  while (pos < size) {
    path[pos] = '\0';
    if (mkdir(path.c_str(), 0700) != 0) {
      if (errno != EEXIST) {
        LOGW("File::CreateDirectory: mkdir failed: %d, %s\n", errno,
             strerror(errno));
        return false;
      }
    }
    path[pos] = kDirectoryDelimiter;
    pos = path.find(kDirectoryDelimiter, pos + 1);
  }

  if (path[size - 1] != kDirectoryDelimiter) {
    if (mkdir(path.c_str(), 0700) != 0) {
      if (errno != EEXIST) {
        LOGW("File::CreateDirectory: mkdir failed: %d, %s\n", errno,
             strerror(errno));
        return false;
      }
    }
  }
  return true;
}

}  // namespace wvcdm
