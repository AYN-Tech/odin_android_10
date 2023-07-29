// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "file_store.h"
#include "file_utils.h"
#include "test_vectors.h"

namespace wvcdm {

namespace {
const std::string kTestDirName = "test_dir";
const std::string kTestFileName = "test.txt";
const std::string kTestFileName2 = "test2.txt";
const std::string kTestFileNameExt = ".txt";
const std::string kWildcard = "*";
}  // namespace

class FileUtilsTest : public testing::Test {
 protected:
  FileUtilsTest() {}
  virtual ~FileUtilsTest() {}

  virtual void SetUp() { CreateTestDir(); }
  virtual void TearDown() { RemoveTestDir(); }

  void CreateTestDir() {
    if (!file_system.Exists(test_vectors::kTestDir)) {
      EXPECT_TRUE(FileUtils::CreateDirectory(test_vectors::kTestDir));
    }
    EXPECT_TRUE(file_system.Exists(test_vectors::kTestDir));
  }

  void RemoveTestDir() {
    EXPECT_TRUE(file_system.Remove(test_vectors::kTestDir));
  }

  std::string GenerateRandomData(uint32_t len) {
    std::string data(len, 0);
    for (size_t i = 0; i < len; i++) {
      data[i] = rand() % 256;
    }
    return data;
  }

  FileSystem file_system;
};

TEST_F(FileUtilsTest, CreateDirectory) {
  std::string dir_wo_delimiter =
      test_vectors::kTestDir.substr(0, test_vectors::kTestDir.size() - 1);
  if (file_system.Exists(dir_wo_delimiter))
    EXPECT_TRUE(file_system.Remove(dir_wo_delimiter));
  EXPECT_FALSE(file_system.Exists(dir_wo_delimiter));
  EXPECT_TRUE(FileUtils::CreateDirectory(dir_wo_delimiter));
  EXPECT_TRUE(file_system.Exists(dir_wo_delimiter));
  EXPECT_TRUE(file_system.Remove(dir_wo_delimiter));
  EXPECT_TRUE(FileUtils::CreateDirectory(test_vectors::kTestDir));
  EXPECT_TRUE(file_system.Exists(test_vectors::kTestDir));
  EXPECT_TRUE(file_system.Remove(test_vectors::kTestDir));
}

TEST_F(FileUtilsTest, IsDir) {
  std::string path = test_vectors::kTestDir + kTestFileName;
  std::unique_ptr<File> file = file_system.Open(path, FileSystem::kCreate);
  EXPECT_TRUE(file);

  EXPECT_TRUE(file_system.Exists(path));
  EXPECT_TRUE(file_system.Exists(test_vectors::kTestDir));
  EXPECT_FALSE(FileUtils::IsDirectory(path));
  EXPECT_TRUE(FileUtils::IsDirectory(test_vectors::kTestDir));
}

TEST_F(FileUtilsTest, IsRegularFile) {
  std::string path = test_vectors::kTestDir + kTestFileName;
  std::unique_ptr<File> file = file_system.Open(path, FileSystem::kCreate);
  EXPECT_TRUE(file);

  EXPECT_TRUE(file_system.Exists(path));
  EXPECT_TRUE(file_system.Exists(test_vectors::kTestDir));
  EXPECT_TRUE(FileUtils::IsRegularFile(path));
  EXPECT_FALSE(FileUtils::IsRegularFile(test_vectors::kTestDir));
}

TEST_F(FileUtilsTest, CopyFile) {
  std::string path = test_vectors::kTestDir + kTestFileName;
  file_system.Remove(path);

  std::string write_data = GenerateRandomData(600);
  size_t write_data_size = write_data.size();
  std::unique_ptr<File> wr_file = file_system.Open(path, FileSystem::kCreate);
  EXPECT_TRUE(wr_file);
  EXPECT_EQ(wr_file->Write(write_data.data(), write_data_size),
            write_data_size);
  ASSERT_TRUE(file_system.Exists(path));

  std::string path_copy = test_vectors::kTestDir + kTestFileName2;
  EXPECT_FALSE(file_system.Exists(path_copy));
  EXPECT_TRUE(FileUtils::Copy(path, path_copy));

  std::string read_data;
  read_data.resize(file_system.FileSize(path_copy));
  size_t read_data_size = read_data.size();
  std::unique_ptr<File> rd_file =
    file_system.Open(path_copy, FileSystem::kReadOnly);
  EXPECT_TRUE(rd_file);
  EXPECT_EQ(rd_file->Read(&read_data[0], read_data_size), read_data_size);
  EXPECT_EQ(write_data, read_data);
  EXPECT_EQ(file_system.FileSize(path), file_system.FileSize(path_copy));
}

TEST_F(FileUtilsTest, ListEmptyDirectory) {
  std::vector<std::string> files;
  EXPECT_TRUE(FileUtils::List(test_vectors::kTestDir, &files));
  EXPECT_EQ(0u, files.size());
}

TEST_F(FileUtilsTest, ListFiles) {
  std::string path = test_vectors::kTestDir + kTestDirName;
  EXPECT_TRUE(FileUtils::CreateDirectory(path));

  path = test_vectors::kTestDir + kTestFileName;
  std::string write_data = GenerateRandomData(600);
  size_t write_data_size = write_data.size();
  std::unique_ptr<File> file = file_system.Open(path, FileSystem::kCreate);
  EXPECT_TRUE(file);
  EXPECT_EQ(file->Write(write_data.data(), write_data_size), write_data_size);
  EXPECT_TRUE(file_system.Exists(path));

  path = test_vectors::kTestDir + kTestFileName2;
  write_data = GenerateRandomData(600);
  write_data_size = write_data.size();
  file = file_system.Open(path, FileSystem::kCreate);
  EXPECT_TRUE(file);
  EXPECT_EQ(file->Write(write_data.data(), write_data_size), write_data_size);
  EXPECT_TRUE(file_system.Exists(path));

  std::vector<std::string> files;
  EXPECT_TRUE(FileUtils::List(test_vectors::kTestDir, &files));
  EXPECT_EQ(3u, files.size());

  for (size_t i = 0; i < files.size(); ++i) {
    EXPECT_TRUE(files[i] == kTestDirName || files[i] == kTestFileName ||
                files[i] == kTestFileName2);
  }
}

}  // namespace wvcdm
