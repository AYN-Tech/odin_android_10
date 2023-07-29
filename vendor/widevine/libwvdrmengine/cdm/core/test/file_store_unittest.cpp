// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "file_store.h"
#include "test_vectors.h"

namespace wvcdm {

namespace {
const std::string kTestDirName = "test_dir";
const std::string kTestFileName = "test.txt";
const std::string kTestFileName2 = "test2.txt";
const std::string kTestFileName3 = "test3.other";
const std::string kTestFileNameExt = ".txt";
const std::string kTestFileNameExt3 = ".other";
const std::string kWildcard = "*";
}  // namespace

class FileTest : public testing::Test {
 protected:
  FileTest() {}

  void TearDown() override { RemoveTestDir(); }

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

TEST_F(FileTest, FileExists) {
  EXPECT_TRUE(file_system.Exists(test_vectors::kExistentFile));
  EXPECT_TRUE(file_system.Exists(test_vectors::kExistentDir));
  EXPECT_FALSE(file_system.Exists(test_vectors::kNonExistentFile));
  EXPECT_FALSE(file_system.Exists(test_vectors::kNonExistentDir));
}

TEST_F(FileTest, RemoveDir) {
  EXPECT_TRUE(file_system.Remove(test_vectors::kTestDir));
  EXPECT_FALSE(file_system.Exists(test_vectors::kTestDir));
}

TEST_F(FileTest, OpenFile) {
  std::string path = test_vectors::kTestDir + kTestFileName;
  EXPECT_TRUE(file_system.Remove(path));

  std::unique_ptr<File> file = file_system.Open(path, FileSystem::kCreate);
  ASSERT_TRUE(file);

  EXPECT_TRUE(file_system.Exists(path));
}

TEST_F(FileTest, RemoveDirAndFile) {
  std::string path = test_vectors::kTestDir + kTestFileName;

  std::unique_ptr<File> file = file_system.Open(path, FileSystem::kCreate);
  ASSERT_TRUE(file);

  EXPECT_TRUE(file_system.Exists(path));
  EXPECT_TRUE(file_system.Remove(path));
  EXPECT_FALSE(file_system.Exists(path));

  file = file_system.Open(path, FileSystem::kCreate);
  ASSERT_TRUE(file);

  EXPECT_TRUE(file_system.Exists(path));
  RemoveTestDir();
  EXPECT_FALSE(file_system.Exists(test_vectors::kTestDir));
  EXPECT_FALSE(file_system.Exists(path));
}

TEST_F(FileTest, RemoveWildcardFiles) {
  std::string path1 = test_vectors::kTestDir + kTestFileName;
  std::string path2 = test_vectors::kTestDir + kTestFileName2;
  std::string wildcard_path =
      test_vectors::kTestDir + kWildcard + kTestFileNameExt;

  std::unique_ptr<File> file = file_system.Open(path1, FileSystem::kCreate);
  ASSERT_TRUE(file);
  file = file_system.Open(path2, FileSystem::kCreate);
  ASSERT_TRUE(file);

  EXPECT_TRUE(file_system.Exists(path1));
  EXPECT_TRUE(file_system.Exists(path2));
  EXPECT_TRUE(file_system.Remove(wildcard_path));
  EXPECT_FALSE(file_system.Exists(path1));
  EXPECT_FALSE(file_system.Exists(path2));
}

TEST_F(FileTest, FileSize) {
  std::string path = test_vectors::kTestDir + kTestFileName;
  file_system.Remove(path);

  std::string write_data = GenerateRandomData(600);
  size_t write_data_size = write_data.size();
  std::unique_ptr<File> file = file_system.Open(path, FileSystem::kCreate);
  ASSERT_TRUE(file);
  EXPECT_EQ(file->Write(write_data.data(), write_data_size), write_data_size);
  EXPECT_TRUE(file_system.Exists(path));

  EXPECT_EQ(static_cast<ssize_t>(write_data_size),
            file_system.FileSize(path));
}

TEST_F(FileTest, WriteReadBinaryFile) {
  std::string path = test_vectors::kTestDir + kTestFileName;
  file_system.Remove(path);

  std::string write_data = GenerateRandomData(600);
  size_t write_data_size = write_data.size();
  std::unique_ptr<File> file = file_system.Open(path, FileSystem::kCreate);
  ASSERT_TRUE(file);
  EXPECT_EQ(file->Write(write_data.data(),write_data_size), write_data_size);
  EXPECT_TRUE(file_system.Exists(path));

  std::string read_data;
  read_data.resize(file_system.FileSize(path));
  size_t read_data_size = read_data.size();
  file = file_system.Open(path, FileSystem::kReadOnly);
  ASSERT_TRUE(file);
  EXPECT_EQ(file->Read(&read_data[0], read_data_size), read_data_size);
  EXPECT_EQ(write_data, read_data);
}

TEST_F(FileTest, ListFiles) {
  std::vector<std::string> names;

  std::string not_path("zzz");
  std::string path1 = test_vectors::kTestDir + kTestFileName;
  std::string path2 = test_vectors::kTestDir + kTestFileName2;
  std::string path3 = test_vectors::kTestDir + kTestFileName3;
  std::string path_dir = test_vectors::kTestDir;

  std::unique_ptr<File> file = file_system.Open(path1, FileSystem::kCreate);
  ASSERT_TRUE(file);
  file = file_system.Open(path2, FileSystem::kCreate);
  ASSERT_TRUE(file);
  file = file_system.Open(path3, FileSystem::kCreate);
  ASSERT_TRUE(file);

  EXPECT_TRUE(file_system.Exists(path1));
  EXPECT_TRUE(file_system.Exists(path2));
  EXPECT_TRUE(file_system.Exists(path3));

  // Ask for non-existent path.
  EXPECT_FALSE(file_system.List(not_path, &names));

  // Valid path, but no way to return names.
  EXPECT_FALSE(file_system.List(path_dir, nullptr));

  // Valid path, valid return.
  EXPECT_TRUE(file_system.List(path_dir, &names));

  // Should find three files. Order not important.
  EXPECT_EQ(3u, names.size());
  EXPECT_THAT(names, ::testing::UnorderedElementsAre(kTestFileName,
                                                     kTestFileName2,
                                                     kTestFileName3));

  std::string wild_card_path = path_dir + kWildcard + kTestFileNameExt;
  EXPECT_TRUE(file_system.Remove(wild_card_path));
  EXPECT_TRUE(file_system.List(path_dir, &names));

  EXPECT_EQ(1u, names.size());
  EXPECT_TRUE(names[0].compare(kTestFileName3) == 0);

  std::string wild_card_path2 = path_dir + kWildcard + kTestFileNameExt3;
  EXPECT_TRUE(file_system.Remove(wild_card_path2));
  EXPECT_TRUE(file_system.List(path_dir, &names));

  EXPECT_EQ(0u, names.size());
}

}  // namespace wvcdm
