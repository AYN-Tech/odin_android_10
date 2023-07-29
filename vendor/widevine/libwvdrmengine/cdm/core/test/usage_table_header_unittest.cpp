// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#include "usage_table_header.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>

#include "crypto_session.h"
#include "device_files.h"
#include "file_store.h"
#include "test_base.h"
#include "test_printers.h"
#include "wv_cdm_constants.h"
#include "wv_cdm_types.h"

namespace wvcdm {

namespace {

const std::string kEmptyString;

const CdmUsageTableHeader kEmptyUsageTableHeader;
const CdmUsageTableHeader kUsageTableHeader = "some usage table header";
const CdmUsageTableHeader kAnotherUsageTableHeader =
    "another usage table header";
const CdmUsageTableHeader kYetAnotherUsageTableHeader =
    "yet another usage table header";
const CdmUsageTableHeader kAndAnotherUsageTableHeader =
    "and another usage table header";
const CdmUsageTableHeader kOneMoreUsageTableHeader =
    "one more usage table header";
const CdmUsageEntry kUsageEntry = "usage entry";
const CdmUsageEntry kAnotherUsageEntry = "another usage entry";
const CdmUsageEntry kYetAnotherUsageEntry = "yet another usage entry";
const CdmUsageEntry kAndAnotherUsageEntry = "and another usage entry";
const CdmUsageEntry kOneMoreUsageEntry = "one more usage entry";
const CdmUsageEntryInfo kUsageEntryInfoOfflineLicense1 = {
    /* storage_type = */ kStorageLicense,
    /* key_set_id = */ "offline_key_set_1",
    /* usage_info_file_name = */ "",
};
const CdmUsageEntryInfo kUsageEntryInfoOfflineLicense2 = {
    /* storage_type = */ kStorageLicense,
    /* key_set_id = */ "offline_key_set_2",
    /* usage_info_file_name = */ "",
};
const CdmUsageEntryInfo kUsageEntryInfoOfflineLicense3 = {
    /* storage_type = */ kStorageLicense,
    /* key_set_id = */ "offline_key_set_3",
    /* usage_info_file_name = */ ""};
const CdmUsageEntryInfo kUsageEntryInfoOfflineLicense4 = {
    /* storage_type = */ kStorageLicense,
    /* key_set_id = */ "offline_key_set_4",
    /* usage_info_file_name = */ ""};
const CdmUsageEntryInfo kUsageEntryInfoOfflineLicense5 = {
    /* storage_type = */ kStorageLicense,
    /* key_set_id = */ "offline_key_set_5",
    /* usage_info_file_name = */ ""};
const CdmUsageEntryInfo kUsageEntryInfoOfflineLicense6 = {
    /* storage_type = */ kStorageLicense,
    /* key_set_id = */ "offline_key_set_6",
    /* usage_info_file_name = */ ""};
const CdmUsageEntryInfo kUsageEntryInfoSecureStop1 = {
    /* storage_type = */ kStorageUsageInfo,
    /* key_set_id = */ "secure_stop_key_set_1",
    /* usage_info_file_name = */ "usage_info_file_1",
};
const CdmUsageEntryInfo kUsageEntryInfoSecureStop2 = {
    /* storage_type = */ kStorageUsageInfo,
    /* key_set_id = */ "secure_stop_key_set_2",
    /* usage_info_file_name = */ "usage_info_file_2",
};
const CdmUsageEntryInfo kUsageEntryInfoSecureStop3 = {
    /* storage_type = */ kStorageUsageInfo,
    /* key_set_id = */ "secure_stop_key_set_3",
    /* usage_info_file_name = */ "usage_info_file_3"};
const CdmUsageEntryInfo kUsageEntryInfoSecureStop4 = {
    /* storage_type = */ kStorageUsageInfo,
    /* key_set_id = */ "secure_stop_key_set_4",
    /* usage_info_file_name = */ "usage_info_file_4"};
const CdmUsageEntryInfo kUsageEntryInfoSecureStop5 = {
    /* storage_type = */ kStorageUsageInfo,
    /* key_set_id = */ "secure_stop_key_set_5",
    /* usage_info_file_name = */ "usage_info_file_5"};
const CdmUsageEntryInfo kUsageEntryInfoStorageTypeUnknown = {
    /* storage_type = */ kStorageTypeUnknown,
    /* key_set_id = */ "",
    /* usage_info_file_name = */ "",
};
const CdmUsageEntryInfo kDummyUsageEntryInfo = {
    /* storage_type = */ kStorageLicense,
    /* key_set_id = */ "DummyKsid",
    /* usage_info_file_name = */ "",
};

const std::vector<std::string> kEmptyLicenseList;

const std::string kLicenseArray[] = {
    kUsageEntryInfoOfflineLicense1.key_set_id,
    kUsageEntryInfoOfflineLicense2.key_set_id,
    kUsageEntryInfoOfflineLicense3.key_set_id,
};
const size_t kLicenseArraySize = sizeof(kLicenseArray)/
    sizeof(kLicenseArray[0]);
std::vector<std::string> kLicenseList;

const std::vector<std::string> kEmptyUsageInfoFilesList;

const std::string kUsageInfoFileArray[] = {
    kUsageEntryInfoSecureStop1.usage_info_file_name,
    kUsageEntryInfoSecureStop2.usage_info_file_name,
    kUsageEntryInfoSecureStop3.usage_info_file_name,
};
const size_t kUsageInfoFileArraySize = sizeof(kUsageInfoFileArray)/
    sizeof(kUsageInfoFileArray[0]);
std::vector<std::string> kUsageInfoFileList;

const DeviceFiles::CdmUsageData kCdmUsageData1 = {
  /* provider_session_token = */ "provider_session_token_1",
  /* license_request = */ "license_request_1",
  /* license = */ "license_1",
  /* key_set_id = */ "key_set_id_1",
  /* usage_entry = */ "usage_entry_1",
  /* usage_entry_number = */ 0,
};
const DeviceFiles::CdmUsageData kCdmUsageData2 = {
  /* provider_session_token = */ "provider_session_token_2",
  /* license_request = */ "license_request_2",
  /* license = */ "license_2",
  /* key_set_id = */ "key_set_id_2",
  /* usage_entry = */ "usage_entry_2",
  /* usage_entry_number = */ 0,
};
const DeviceFiles::CdmUsageData kCdmUsageData3 = {
  /* provider_session_token = */ "provider_session_token_3",
  /* license_request = */ "license_request_3",
  /* license = */ "license_3",
  /* key_set_id = */ "key_set_id_3",
  /* usage_entry = */ "usage_entry_3",
  /* usage_entry_number = */ 0,
};
const std::vector<DeviceFiles::CdmUsageData> kEmptyUsageInfoUsageDataList;

const std::vector<CdmUsageEntryInfo> kEmptyUsageEntryInfoVector;
std::vector<CdmUsageEntryInfo> kUsageEntryInfoVector;
std::vector<CdmUsageEntryInfo> k10UsageEntryInfoVector;
std::vector<CdmUsageEntryInfo> k201UsageEntryInfoVector;

const DeviceFiles::LicenseState kActiveLicenseState =
    DeviceFiles::kLicenseStateActive;
const CdmInitData kPsshData = "pssh data";
const CdmKeyMessage kKeyRequest = "key request";
const CdmKeyResponse kKeyResponse = "key response";
const CdmKeyMessage kKeyRenewalRequest = "key renewal request";
const CdmKeyResponse kKeyRenewalResponse = "key renewal response";
const std::string kReleaseServerUrl = "some url";
const std::string kProviderSessionToken = "provider session token";
const CdmAppParameterMap kEmptyAppParameters;
int64_t kPlaybackStartTime = 1030005;
int64_t kPlaybackDuration = 300;
int64_t kGracePeriodEndTime = 60;

namespace {

void InitVectorConstants() {
  kUsageEntryInfoVector.clear();
  kUsageEntryInfoVector.push_back(kUsageEntryInfoOfflineLicense1);
  kUsageEntryInfoVector.push_back(kUsageEntryInfoSecureStop1);
  kUsageEntryInfoVector.push_back(kUsageEntryInfoStorageTypeUnknown);

  k10UsageEntryInfoVector.clear();
  k10UsageEntryInfoVector.push_back(kUsageEntryInfoOfflineLicense1);
  k10UsageEntryInfoVector.push_back(kUsageEntryInfoSecureStop1);
  k10UsageEntryInfoVector.push_back(kUsageEntryInfoOfflineLicense2);
  k10UsageEntryInfoVector.push_back(kUsageEntryInfoSecureStop2);
  k10UsageEntryInfoVector.push_back(kUsageEntryInfoOfflineLicense3);
  k10UsageEntryInfoVector.push_back(kUsageEntryInfoSecureStop3);
  k10UsageEntryInfoVector.push_back(kUsageEntryInfoOfflineLicense4);
  k10UsageEntryInfoVector.push_back(kUsageEntryInfoSecureStop4);
  k10UsageEntryInfoVector.push_back(kUsageEntryInfoOfflineLicense5);
  k10UsageEntryInfoVector.push_back(kUsageEntryInfoSecureStop5);

  k201UsageEntryInfoVector.clear();
  for (size_t i = 0; i < 201; ++i) {
    switch (i % 4) {
      case 0:
        k201UsageEntryInfoVector.push_back(kUsageEntryInfoOfflineLicense1);
        break;
      case 1:
        k201UsageEntryInfoVector.push_back(kUsageEntryInfoSecureStop1);
        break;
      case 2:
        k201UsageEntryInfoVector.push_back(kUsageEntryInfoOfflineLicense2);
        break;
      case 3:
        k201UsageEntryInfoVector.push_back(kUsageEntryInfoSecureStop2);
        break;
      default:
        k201UsageEntryInfoVector.push_back(kUsageEntryInfoStorageTypeUnknown);
        break;
    }
  }

  kUsageInfoFileList.clear();
  for (size_t i = 0; i < kUsageInfoFileArraySize; i++) {
    kUsageInfoFileList.push_back(kUsageInfoFileArray[i]);
  }

  kLicenseList.clear();
  for (size_t i = 0; i < kLicenseArraySize; i++) {
    kLicenseList.push_back(kLicenseArray[i]);
  }
}

void ToVector(std::vector<CdmUsageEntryInfo>& vec,
              const CdmUsageEntryInfo* arr, size_t total_size) {
  size_t max = total_size / sizeof(CdmUsageEntryInfo);
  vec.clear();
  for (size_t i = 0; i < max; i++) {
    vec.push_back(arr[i]);
  }
}

};  // namespace

class MockDeviceFiles : public DeviceFiles {
 public:
  MockDeviceFiles() : DeviceFiles(&file_system_) { Init(kSecurityLevelL1); }

  MOCK_METHOD2(RetrieveUsageTableInfo,
               bool(CdmUsageTableHeader*, std::vector<CdmUsageEntryInfo>*));
  MOCK_METHOD2(StoreUsageTableInfo,
               bool(const CdmUsageTableHeader&,
                    const std::vector<CdmUsageEntryInfo>&));
  MOCK_METHOD2(DeleteUsageInfo, bool(const std::string&, const std::string&));
  MOCK_METHOD7(RetrieveUsageInfoByKeySetId,
               bool(const std::string&, const std::string&, std::string*,
                    CdmKeyMessage*, CdmKeyResponse*, CdmUsageEntry*,
                    uint32_t*));
  MOCK_METHOD0(DeleteAllLicenses, bool());
  MOCK_METHOD0(DeleteAllUsageInfo, bool());
  MOCK_METHOD0(DeleteUsageTableInfo, bool());
  MOCK_METHOD7(StoreUsageInfo,
               bool(const std::string&, const CdmKeyMessage&,
                    const CdmKeyResponse&, const std::string&,
                    const std::string&, const CdmUsageEntry&, uint32_t));
  MOCK_METHOD2(RetrieveUsageInfo,
               bool(const std::string&, std::vector<CdmUsageData>*));
  MOCK_METHOD1(ListLicenses,
               bool(std::vector<std::string>* key_set_ids));
  MOCK_METHOD1(ListUsageInfoFiles,
               bool(std::vector<std::string>* usage_info_files));

 private:
  FileSystem file_system_;
};

class MockCryptoSession : public TestCryptoSession {
 public:
  MockCryptoSession(metrics::CryptoMetrics* metrics)
      : TestCryptoSession(metrics) {}
  MOCK_METHOD1(Open, CdmResponseType(SecurityLevel));
  MOCK_METHOD1(LoadUsageTableHeader,
               CdmResponseType(const CdmUsageTableHeader&));
  MOCK_METHOD1(CreateUsageTableHeader, CdmResponseType(CdmUsageTableHeader*));
  MOCK_METHOD1(CreateUsageEntry, CdmResponseType(uint32_t*));
  MOCK_METHOD2(LoadUsageEntry, CdmResponseType(uint32_t, const CdmUsageEntry&));
  MOCK_METHOD2(UpdateUsageEntry,
               CdmResponseType(CdmUsageTableHeader*, CdmUsageEntry*));
  MOCK_METHOD1(MoveUsageEntry, CdmResponseType(uint32_t));
  MOCK_METHOD2(ShrinkUsageTableHeader,
               CdmResponseType(uint32_t, CdmUsageTableHeader*));
};

// Partial mock of the UsageTableHeader. This is to test when dependency
// exist on internal methods which would require complex expectations
class MockUsageTableHeader : public UsageTableHeader {
   public:
    MockUsageTableHeader() : UsageTableHeader() {}
    MOCK_METHOD3(DeleteEntry, CdmResponseType(uint32_t, DeviceFiles*,
                                              metrics::CryptoMetrics*));
};

}  // namespace

// gmock methods
using ::testing::_;
using ::testing::AllOf;
using ::testing::AtMost;
using ::testing::ElementsAreArray;
using ::testing::Ge;
using ::testing::Invoke;
using ::testing::Lt;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgPointee;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

class UsageTableHeaderTest : public WvCdmTestBase {
 public:
  static void SetUpTestCase() {
    InitVectorConstants();
  }

  // Useful when UsageTableHeader is mocked
  void DeleteEntry(uint32_t usage_entry_number, DeviceFiles*,
                   metrics::CryptoMetrics*) {
    usage_table_header_->DeleteEntryForTest(usage_entry_number);
  }

 protected:
  void SetUp() override {
    WvCdmTestBase::SetUp();
    // UsageTableHeader will take ownership of the pointer
    device_files_ = new MockDeviceFiles();
    crypto_session_ = new MockCryptoSession(&crypto_metrics_);
    usage_table_header_ = new UsageTableHeader();

    // usage_table_header_ object takes ownership of these objects
    usage_table_header_->SetDeviceFiles(device_files_);
    usage_table_header_->SetCryptoSession(crypto_session_);
  }

  // UsageTableHeaderTest maintains ownership of returned pointer
  MockUsageTableHeader* SetUpMock() {
    // Release non-mocked usage table header
    if (usage_table_header_ != NULL) {
      delete usage_table_header_;
      usage_table_header_ = NULL;
    }

    // Create new mock objects if using MockUsageTableHeader
    device_files_ = new MockDeviceFiles();
    crypto_session_ = new MockCryptoSession(&crypto_metrics_);
    MockUsageTableHeader* mock_usage_table_header = new MockUsageTableHeader();

    // mock_usage_table_header_ object takes ownership of these objects
    mock_usage_table_header->SetDeviceFiles(device_files_);
    mock_usage_table_header->SetCryptoSession(crypto_session_);

    usage_table_header_ = mock_usage_table_header;
    return mock_usage_table_header;
  }

  void TearDown() override {
    if (usage_table_header_ != NULL) delete usage_table_header_;
  }

  void Init(CdmSecurityLevel security_level,
            const CdmUsageTableHeader& usage_table_header,
            const std::vector<CdmUsageEntryInfo>& usage_entry_info_vector) {
    EXPECT_CALL(*device_files_, RetrieveUsageTableInfo(NotNull(), NotNull()))
        .WillOnce(DoAll(SetArgPointee<0>(usage_table_header),
                        SetArgPointee<1>(usage_entry_info_vector),
                        Return(true)));
    EXPECT_CALL(*crypto_session_, LoadUsageTableHeader(usage_table_header))
        .WillOnce(Return(NO_ERROR));
    EXPECT_TRUE(usage_table_header_->Init(security_level, crypto_session_));
  }

  MockDeviceFiles* device_files_;
  metrics::CryptoMetrics crypto_metrics_;
  MockCryptoSession* crypto_session_;
  UsageTableHeader* usage_table_header_;
};

TEST_F(UsageTableHeaderTest, InitError) {
  EXPECT_FALSE(
      usage_table_header_->Init(kSecurityLevelUninitialized, crypto_session_));
  EXPECT_FALSE(usage_table_header_->Init(kSecurityLevelL2, crypto_session_));
  EXPECT_FALSE(
      usage_table_header_->Init(kSecurityLevelUnknown, crypto_session_));
  EXPECT_FALSE(usage_table_header_->Init(kSecurityLevelL1, NULL));
  EXPECT_FALSE(usage_table_header_->Init(kSecurityLevelL2, NULL));
}

class UsageTableHeaderInitializationTest
    : public UsageTableHeaderTest,
      public ::testing::WithParamInterface<CdmSecurityLevel> {
 public:
  static void SetUpTestCase() {
    InitVectorConstants();
  }

};

TEST_P(UsageTableHeaderInitializationTest, CreateUsageTableHeader) {
  EXPECT_CALL(*device_files_, RetrieveUsageTableInfo(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgPointee<0>(kEmptyUsageTableHeader),
                      SetArgPointee<1>(kEmptyUsageEntryInfoVector),
                      Return(false)));
  EXPECT_CALL(*device_files_, ListLicenses(NotNull()))
      .WillOnce(DoAll(SetArgPointee<0>(kEmptyLicenseList),
                      Return(false)));
  EXPECT_CALL(*device_files_, ListUsageInfoFiles(NotNull()))
      .WillOnce(DoAll(SetArgPointee<0>(kEmptyUsageInfoFilesList),
                      Return(false)));
  EXPECT_CALL(*crypto_session_, CreateUsageTableHeader(NotNull()))
      .WillOnce(
          DoAll(SetArgPointee<0>(kEmptyUsageTableHeader), Return(NO_ERROR)));
  EXPECT_CALL(*device_files_, StoreUsageTableInfo(kEmptyUsageTableHeader,
                                                  kEmptyUsageEntryInfoVector))
      .Times(2)
      .WillRepeatedly(Return(true));

  EXPECT_TRUE(usage_table_header_->Init(GetParam(), crypto_session_));
}

TEST_P(UsageTableHeaderInitializationTest, Upgrade_UnableToRetrieveLicenses) {
  EXPECT_CALL(*device_files_, RetrieveUsageTableInfo(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgPointee<0>(kEmptyUsageTableHeader),
                      SetArgPointee<1>(kEmptyUsageEntryInfoVector),
                      Return(false)));
  EXPECT_CALL(*device_files_, ListLicenses(NotNull()))
      .WillOnce(DoAll(SetArgPointee<0>(kLicenseList),
                      Return(true)));
  EXPECT_CALL(*device_files_, ListUsageInfoFiles(NotNull()))
      .WillOnce(DoAll(SetArgPointee<0>(kEmptyUsageInfoFilesList),
                      Return(false)));
  EXPECT_CALL(*crypto_session_, CreateUsageTableHeader(NotNull()))
      .WillOnce(
          DoAll(SetArgPointee<0>(kEmptyUsageTableHeader), Return(NO_ERROR)));
  // TODO: Why not being called?
  //EXPECT_CALL(*device_files_, DeleteAllLicenses()).WillOnce(Return(true));
  EXPECT_CALL(*device_files_, StoreUsageTableInfo(kEmptyUsageTableHeader,
                                                  kEmptyUsageEntryInfoVector))
      .Times(2)
      .WillRepeatedly(Return(true));

  for (size_t i = 0; i < kLicenseList.size(); ++i)
    device_files_->DeleteLicense(kLicenseList[i]);
  EXPECT_TRUE(usage_table_header_->Init(GetParam(), crypto_session_));
}

TEST_P(UsageTableHeaderInitializationTest, Upgrade_UnableToRetrieveUsageInfo) {
  EXPECT_CALL(*device_files_, RetrieveUsageTableInfo(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgPointee<0>(kEmptyUsageTableHeader),
                      SetArgPointee<1>(kEmptyUsageEntryInfoVector),
                      Return(false)));
  EXPECT_CALL(*device_files_, ListLicenses(NotNull()))
      .WillOnce(DoAll(SetArgPointee<0>(kEmptyLicenseList),
                      Return(false)));
  EXPECT_CALL(*device_files_, ListUsageInfoFiles(NotNull()))
      .WillOnce(DoAll(SetArgPointee<0>(kUsageInfoFileList),
                      Return(true)));
  EXPECT_CALL(*crypto_session_, CreateUsageTableHeader(NotNull()))
      .WillOnce(
          DoAll(SetArgPointee<0>(kEmptyUsageTableHeader), Return(NO_ERROR)));
  EXPECT_CALL(*device_files_, StoreUsageTableInfo(kEmptyUsageTableHeader,
                                                  kEmptyUsageEntryInfoVector))
      .Times(2)
      .WillRepeatedly(Return(true));
  for (size_t i = 0; i < kUsageInfoFileList.size(); ++i) {
    EXPECT_CALL(*device_files_,
                RetrieveUsageInfo(kUsageInfoFileList[i], NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>(kEmptyUsageInfoUsageDataList),
                      Return(false)));
  }

  EXPECT_TRUE(usage_table_header_->Init(GetParam(), crypto_session_));
}

TEST_P(UsageTableHeaderInitializationTest, UsageTableHeaderExists) {
  EXPECT_CALL(*device_files_, RetrieveUsageTableInfo(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgPointee<0>(kUsageTableHeader),
                      SetArgPointee<1>(kUsageEntryInfoVector), Return(true)));
  EXPECT_CALL(*crypto_session_, LoadUsageTableHeader(kUsageTableHeader))
      .WillOnce(Return(NO_ERROR));

  EXPECT_TRUE(usage_table_header_->Init(GetParam(), crypto_session_));
}

TEST_P(UsageTableHeaderInitializationTest, 200UsageEntries) {
  std::vector<CdmUsageEntryInfo> usage_entries_200 = k201UsageEntryInfoVector;
  usage_entries_200.resize(200);
  EXPECT_CALL(*device_files_, RetrieveUsageTableInfo(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgPointee<0>(kUsageTableHeader),
                      SetArgPointee<1>(usage_entries_200),
                          Return(true)));
  EXPECT_CALL(*crypto_session_, LoadUsageTableHeader(kUsageTableHeader))
      .WillOnce(Return(NO_ERROR));

  EXPECT_TRUE(usage_table_header_->Init(GetParam(), crypto_session_));
}

TEST_P(UsageTableHeaderInitializationTest,
       201UsageEntries_AddEntryFails_UsageTableHeaderRecreated) {
  EXPECT_CALL(*device_files_, RetrieveUsageTableInfo(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgPointee<0>(kUsageTableHeader),
                      SetArgPointee<1>(k201UsageEntryInfoVector),
                          Return(true)));

  SecurityLevel security_level =
      (GetParam() == kSecurityLevelL3) ? kLevel3 : kLevelDefault;
  EXPECT_CALL(*crypto_session_,
              Open(security_level)).WillOnce(Return(NO_ERROR));
  EXPECT_CALL(*crypto_session_, LoadUsageTableHeader(kUsageTableHeader))
      .WillOnce(Return(NO_ERROR));
  EXPECT_CALL(*crypto_session_, CreateUsageTableHeader(NotNull()))
      .WillOnce(
          DoAll(SetArgPointee<0>(kEmptyUsageTableHeader), Return(NO_ERROR)));
  EXPECT_CALL(*device_files_, DeleteAllLicenses()).WillOnce(Return(true));
  EXPECT_CALL(*device_files_, DeleteAllUsageInfo()).WillOnce(Return(true));
  EXPECT_CALL(*device_files_, DeleteUsageTableInfo()).WillOnce(Return(true));
  EXPECT_CALL(*device_files_, StoreUsageTableInfo(kEmptyUsageTableHeader,
                                                  kEmptyUsageEntryInfoVector))
      .WillOnce(Return(true));

  // Expectations for AddEntry
  uint32_t expect_usage_entry_number = k201UsageEntryInfoVector.size();
  EXPECT_CALL(*crypto_session_, CreateUsageEntry(NotNull()))
      .WillOnce(DoAll(SetArgPointee<0>(expect_usage_entry_number),
                      Return(CREATE_USAGE_ENTRY_UNKNOWN_ERROR)));

  EXPECT_TRUE(usage_table_header_->Init(GetParam(), crypto_session_));
}

TEST_P(UsageTableHeaderInitializationTest,
       201UsageEntries_DeleteEntryFails_UsageTableHeaderRecreated) {
  std::vector<CdmUsageEntryInfo> usage_entries_202 = k201UsageEntryInfoVector;
  usage_entries_202.push_back(kDummyUsageEntryInfo);

  EXPECT_CALL(*device_files_, RetrieveUsageTableInfo(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgPointee<0>(kUsageTableHeader),
                      SetArgPointee<1>(k201UsageEntryInfoVector),
                          Return(true)));
  EXPECT_CALL(*crypto_session_, LoadUsageTableHeader(kUsageTableHeader))
      .WillOnce(Return(NO_ERROR));
  EXPECT_CALL(*crypto_session_, CreateUsageTableHeader(NotNull()))
      .WillOnce(
          DoAll(SetArgPointee<0>(kEmptyUsageTableHeader), Return(NO_ERROR)));
  EXPECT_CALL(*device_files_, DeleteAllLicenses()).WillOnce(Return(true));
  EXPECT_CALL(*device_files_, DeleteAllUsageInfo()).WillOnce(Return(true));
  EXPECT_CALL(*device_files_, DeleteUsageTableInfo()).WillOnce(Return(true));
  EXPECT_CALL(*device_files_, StoreUsageTableInfo(kEmptyUsageTableHeader,
                                                  kEmptyUsageEntryInfoVector))
      .WillOnce(Return(true));

  // Expectations for AddEntry
  uint32_t expect_usage_entry_number = k201UsageEntryInfoVector.size();
  EXPECT_CALL(*crypto_session_, CreateUsageEntry(NotNull()))
      .WillOnce(DoAll(SetArgPointee<0>(expect_usage_entry_number),
                      Return(NO_ERROR)));
  EXPECT_CALL(*device_files_, StoreUsageTableInfo(kUsageTableHeader,
                                                  usage_entries_202))
      .WillOnce(Return(true));

  // Expectations for DeleteEntry
  SecurityLevel security_level =
      (GetParam() == kSecurityLevelL3) ? kLevel3 : kLevelDefault;
  EXPECT_CALL(*crypto_session_,
              Open(security_level))
      .Times(2)
      .WillRepeatedly(Return(NO_ERROR));
  EXPECT_CALL(
      *crypto_session_,
      ShrinkUsageTableHeader(usage_entries_202.size() - 1, NotNull()))
      .WillOnce(Return(SHRINK_USAGE_TABLER_HEADER_UNKNOWN_ERROR));

  EXPECT_TRUE(usage_table_header_->Init(GetParam(), crypto_session_));
}

TEST_P(UsageTableHeaderInitializationTest,
       201UsageEntries_AddDeleteEntrySucceeds) {
  std::vector<CdmUsageEntryInfo> usage_entries_202 = k201UsageEntryInfoVector;
  usage_entries_202.push_back(kDummyUsageEntryInfo);

  EXPECT_CALL(*device_files_, RetrieveUsageTableInfo(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgPointee<0>(kUsageTableHeader),
                      SetArgPointee<1>(k201UsageEntryInfoVector),
                          Return(true)));
  EXPECT_CALL(*crypto_session_, LoadUsageTableHeader(kUsageTableHeader))
      .WillOnce(Return(NO_ERROR));

  // Expectations for AddEntry
  uint32_t expect_usage_entry_number = k201UsageEntryInfoVector.size();
  EXPECT_CALL(*crypto_session_, CreateUsageEntry(NotNull()))
      .WillOnce(DoAll(SetArgPointee<0>(expect_usage_entry_number),
                      Return(NO_ERROR)));
  EXPECT_CALL(*device_files_, StoreUsageTableInfo(kUsageTableHeader,
                                                  usage_entries_202))
      .WillOnce(Return(true));

  // Expectations for DeleteEntry
  SecurityLevel security_level =
      (GetParam() == kSecurityLevelL3) ? kLevel3 : kLevelDefault;

  EXPECT_CALL(*crypto_session_,
              Open(security_level))
      .Times(2)
      .WillRepeatedly(Return(NO_ERROR));
  EXPECT_CALL(
      *crypto_session_,
      ShrinkUsageTableHeader(usage_entries_202.size() - 1, NotNull()))
      .WillOnce(
          DoAll(SetArgPointee<1>(kAnotherUsageTableHeader), Return(NO_ERROR)));
  EXPECT_CALL(*device_files_,
              StoreUsageTableInfo(
                  kAnotherUsageTableHeader,
                  SizeIs(k201UsageEntryInfoVector.size())))
      .WillOnce(Return(true));

  EXPECT_TRUE(usage_table_header_->Init(GetParam(), crypto_session_));
}

INSTANTIATE_TEST_CASE_P(Cdm, UsageTableHeaderInitializationTest,
                        ::testing::Values(kSecurityLevelL1, kSecurityLevelL3));

TEST_F(UsageTableHeaderTest, AddEntry_CreateUsageEntryFailed_UnknownError) {
  Init(kSecurityLevelL1, kUsageTableHeader, kUsageEntryInfoVector);
  uint32_t usage_entry_number;
  uint32_t expect_usage_entry_number = kUsageEntryInfoVector.size();

  EXPECT_CALL(*crypto_session_, CreateUsageEntry(NotNull()))
      .WillOnce(DoAll(SetArgPointee<0>(expect_usage_entry_number),
                      Return(CREATE_USAGE_ENTRY_UNKNOWN_ERROR)));

  EXPECT_NE(NO_ERROR,
            usage_table_header_->AddEntry(
                crypto_session_,
                kUsageEntryInfoOfflineLicense1.storage_type == kStorageLicense,
                kUsageEntryInfoOfflineLicense1.key_set_id,
                kUsageEntryInfoOfflineLicense1.usage_info_file_name,
                &usage_entry_number));
}

TEST_F(UsageTableHeaderTest, AddEntry_UsageEntryTooSmall) {
  Init(kSecurityLevelL1, kUsageTableHeader, kUsageEntryInfoVector);
  uint32_t usage_entry_number;
  uint32_t expect_usage_entry_number = kUsageEntryInfoVector.size() - 1;

  EXPECT_CALL(*crypto_session_, CreateUsageEntry(NotNull()))
      .WillOnce(
          DoAll(SetArgPointee<0>(expect_usage_entry_number), Return(NO_ERROR)));

  EXPECT_NE(NO_ERROR,
            usage_table_header_->AddEntry(
                crypto_session_,
                kUsageEntryInfoOfflineLicense1.storage_type == kStorageLicense,
                kUsageEntryInfoOfflineLicense1.key_set_id,
                kUsageEntryInfoOfflineLicense1.usage_info_file_name,
                &usage_entry_number));
}

TEST_F(UsageTableHeaderTest, AddEntry_NextConsecutiveOfflineUsageEntry) {
  Init(kSecurityLevelL1, kUsageTableHeader, kUsageEntryInfoVector);
  uint32_t usage_entry_number;
  uint32_t expect_usage_entry_number = kUsageEntryInfoVector.size();
  std::vector<CdmUsageEntryInfo> expect_usage_entry_info_vector =
      kUsageEntryInfoVector;

  expect_usage_entry_info_vector.resize(expect_usage_entry_number + 1);
  expect_usage_entry_info_vector[expect_usage_entry_number] =
      kUsageEntryInfoOfflineLicense2;

  EXPECT_CALL(*crypto_session_, CreateUsageEntry(NotNull()))
      .WillOnce(
          DoAll(SetArgPointee<0>(expect_usage_entry_number), Return(NO_ERROR)));

  EXPECT_CALL(*device_files_,
              StoreUsageTableInfo(
                  kUsageTableHeader,
                  UnorderedElementsAreArray(expect_usage_entry_info_vector)))
      .WillOnce(Return(true));

  EXPECT_EQ(NO_ERROR,
            usage_table_header_->AddEntry(
                crypto_session_,
                kUsageEntryInfoOfflineLicense2.storage_type == kStorageLicense,
                kUsageEntryInfoOfflineLicense2.key_set_id,
                kUsageEntryInfoOfflineLicense2.usage_info_file_name,
                &usage_entry_number));
  EXPECT_EQ(expect_usage_entry_number, usage_entry_number);
}

TEST_F(UsageTableHeaderTest, AddEntry_NextConsecutiveSecureStopUsageEntry) {
  Init(kSecurityLevelL1, kUsageTableHeader, kUsageEntryInfoVector);
  uint32_t usage_entry_number;
  uint32_t expect_usage_entry_number = kUsageEntryInfoVector.size();
  std::vector<CdmUsageEntryInfo> expect_usage_entry_info_vector =
      kUsageEntryInfoVector;

  expect_usage_entry_info_vector.resize(expect_usage_entry_number + 1);
  expect_usage_entry_info_vector[expect_usage_entry_number] =
      kUsageEntryInfoSecureStop2;

  EXPECT_CALL(*crypto_session_, CreateUsageEntry(NotNull()))
      .WillOnce(
          DoAll(SetArgPointee<0>(expect_usage_entry_number), Return(NO_ERROR)));

  EXPECT_CALL(*device_files_,
              StoreUsageTableInfo(
                  kUsageTableHeader,
                  UnorderedElementsAreArray(expect_usage_entry_info_vector)))
      .WillOnce(Return(true));

  EXPECT_EQ(NO_ERROR,
            usage_table_header_->AddEntry(
                crypto_session_,
                kUsageEntryInfoSecureStop2.storage_type == kStorageLicense,
                kUsageEntryInfoSecureStop2.key_set_id,
                kUsageEntryInfoSecureStop2.usage_info_file_name,
                &usage_entry_number));
  EXPECT_EQ(expect_usage_entry_number, usage_entry_number);
}

TEST_F(UsageTableHeaderTest, AddEntry_SkipUsageEntries) {
  Init(kSecurityLevelL1, kUsageTableHeader, kUsageEntryInfoVector);
  uint32_t usage_entry_number;
  uint32_t next_usage_entry_number = kUsageEntryInfoVector.size();
  size_t skip_usage_entries = 3;
  uint32_t expect_usage_entry_number =
      next_usage_entry_number + skip_usage_entries;

  EXPECT_CALL(*crypto_session_, CreateUsageEntry(NotNull()))
      .WillOnce(
          DoAll(SetArgPointee<0>(expect_usage_entry_number), Return(NO_ERROR)));

  EXPECT_CALL(
      *device_files_,
      StoreUsageTableInfo(
          kUsageTableHeader,
          UnorderedElementsAre(
              kUsageEntryInfoOfflineLicense1, kUsageEntryInfoSecureStop1,
              kUsageEntryInfoStorageTypeUnknown,
              kUsageEntryInfoStorageTypeUnknown,
              kUsageEntryInfoStorageTypeUnknown,
              kUsageEntryInfoStorageTypeUnknown, kUsageEntryInfoSecureStop2)))
      .WillOnce(Return(true));

  EXPECT_EQ(NO_ERROR,
            usage_table_header_->AddEntry(
                crypto_session_,
                kUsageEntryInfoSecureStop2.storage_type == kStorageLicense,
                kUsageEntryInfoSecureStop2.key_set_id,
                kUsageEntryInfoSecureStop2.usage_info_file_name,
                &usage_entry_number));
  EXPECT_EQ(expect_usage_entry_number, usage_entry_number);
}

TEST_F(UsageTableHeaderTest,
       AddEntry_CreateUsageEntryFailsOnce_SucceedsSecondTime) {
  // Initialize and setup
  MockUsageTableHeader* mock_usage_table_header = SetUpMock();
  Init(kSecurityLevelL1, kUsageTableHeader, k10UsageEntryInfoVector);
  std::vector<CdmUsageEntryInfo> usage_entry_info_vector_at_start =
      k10UsageEntryInfoVector;

  uint32_t usage_entry_number_first_to_be_deleted;  // randomly choosen
  std::vector<CdmUsageEntryInfo> final_usage_entries;

  uint32_t expected_usage_entry_number = k10UsageEntryInfoVector.size() - 1;

  // Setup expectations
  EXPECT_CALL(*mock_usage_table_header,
              DeleteEntry(_, device_files_, NotNull()))
      .WillOnce(DoAll(SaveArg<0>(&usage_entry_number_first_to_be_deleted),
                      Invoke(this, &UsageTableHeaderTest::DeleteEntry),
                      Return(NO_ERROR)));

  EXPECT_CALL(*crypto_session_, CreateUsageEntry(NotNull()))
      .WillOnce(Return(INSUFFICIENT_CRYPTO_RESOURCES_3))
      .WillOnce(
          DoAll(SetArgPointee<0>(expected_usage_entry_number),
                Return(NO_ERROR)));

  EXPECT_CALL(*device_files_, StoreUsageTableInfo(kUsageTableHeader, _))
      .WillOnce(DoAll(SaveArg<1>(&final_usage_entries), Return(true)));

  // Now invoke the method under test
  uint32_t usage_entry_number;
  EXPECT_EQ(NO_ERROR,
            mock_usage_table_header->AddEntry(
                crypto_session_,
                kUsageEntryInfoOfflineLicense6.storage_type == kStorageLicense,
                kUsageEntryInfoOfflineLicense6.key_set_id,
                kUsageEntryInfoOfflineLicense6.usage_info_file_name,
                &usage_entry_number));

  // Verify added/deleted usage entry number and entries
  EXPECT_EQ(expected_usage_entry_number, usage_entry_number);

  EXPECT_LE(0u, usage_entry_number_first_to_be_deleted);
  EXPECT_LE(usage_entry_number_first_to_be_deleted,
            usage_entry_info_vector_at_start.size() - 1);

  std::vector<CdmUsageEntryInfo> expected_usage_entries =
      usage_entry_info_vector_at_start;
  expected_usage_entries[usage_entry_number_first_to_be_deleted] =
      expected_usage_entries[expected_usage_entries.size() - 1];
  expected_usage_entries.resize(expected_usage_entries.size() - 1);
  expected_usage_entries.push_back(kUsageEntryInfoOfflineLicense6);

  EXPECT_EQ(expected_usage_entries, final_usage_entries);
}

TEST_F(UsageTableHeaderTest,
       AddEntry_CreateUsageEntryFailsTwice_SucceedsThirdTime) {
  // Initialize and setup
  MockUsageTableHeader* mock_usage_table_header = SetUpMock();
  Init(kSecurityLevelL1, kUsageTableHeader, k10UsageEntryInfoVector);
  std::vector<CdmUsageEntryInfo> usage_entry_info_vector_at_start =
      k10UsageEntryInfoVector;

  uint32_t usage_entry_number_first_to_be_deleted;  // randomly choosen
  uint32_t usage_entry_number_second_to_be_deleted;  // randomly choosen
  std::vector<CdmUsageEntryInfo> final_usage_entries;

  uint32_t expected_usage_entry_number = k10UsageEntryInfoVector.size() - 2;

  // Setup expectations
  EXPECT_CALL(*mock_usage_table_header,
              DeleteEntry(_, device_files_, NotNull()))
      .WillOnce(DoAll(SaveArg<0>(&usage_entry_number_first_to_be_deleted),
                      Invoke(this, &UsageTableHeaderTest::DeleteEntry),
                      Return(NO_ERROR)))
      .WillOnce(DoAll(SaveArg<0>(&usage_entry_number_second_to_be_deleted),
                      Invoke(this, &UsageTableHeaderTest::DeleteEntry),
                      Return(NO_ERROR)));

  EXPECT_CALL(*crypto_session_, CreateUsageEntry(NotNull()))
      .WillOnce(Return(INSUFFICIENT_CRYPTO_RESOURCES_3))
      .WillOnce(Return(INSUFFICIENT_CRYPTO_RESOURCES_3))
      .WillOnce(
          DoAll(SetArgPointee<0>(expected_usage_entry_number),
                Return(NO_ERROR)));

  EXPECT_CALL(*device_files_, StoreUsageTableInfo(kUsageTableHeader, _))
      .WillOnce(DoAll(SaveArg<1>(&final_usage_entries), Return(true)));

  // Now invoke the method under test
  uint32_t usage_entry_number;
  EXPECT_EQ(NO_ERROR,
            mock_usage_table_header->AddEntry(
                crypto_session_,
                kUsageEntryInfoOfflineLicense6.storage_type == kStorageLicense,
                kUsageEntryInfoOfflineLicense6.key_set_id,
                kUsageEntryInfoOfflineLicense6.usage_info_file_name,
                &usage_entry_number));

  // Verify added/deleted usage entry number and entries
  EXPECT_EQ(expected_usage_entry_number, usage_entry_number);

  EXPECT_LE(0u, usage_entry_number_first_to_be_deleted);
  EXPECT_LE(usage_entry_number_first_to_be_deleted,
            usage_entry_info_vector_at_start.size() - 1);
  EXPECT_LE(0u, usage_entry_number_second_to_be_deleted);
  EXPECT_LE(usage_entry_number_second_to_be_deleted,
            usage_entry_info_vector_at_start.size() - 1);

  std::vector<CdmUsageEntryInfo> expected_usage_entries =
      usage_entry_info_vector_at_start;
  expected_usage_entries[usage_entry_number_first_to_be_deleted] =
      expected_usage_entries[expected_usage_entries.size() - 1];
  expected_usage_entries.resize(expected_usage_entries.size() - 1);
  expected_usage_entries[usage_entry_number_second_to_be_deleted] =
      expected_usage_entries[expected_usage_entries.size() - 1];
  expected_usage_entries.resize(expected_usage_entries.size() - 1);
  expected_usage_entries.push_back(kUsageEntryInfoOfflineLicense6);

  EXPECT_EQ(expected_usage_entries, final_usage_entries);
}

TEST_F(UsageTableHeaderTest, AddEntry_CreateUsageEntryFailsThrice) {
  // Initialize and setup
  MockUsageTableHeader* mock_usage_table_header = SetUpMock();
  Init(kSecurityLevelL1, kUsageTableHeader, k10UsageEntryInfoVector);
  std::vector<CdmUsageEntryInfo> usage_entry_info_vector_at_start =
      k10UsageEntryInfoVector;

  uint32_t usage_entry_number_first_to_be_deleted;   // randomly choosen
  uint32_t usage_entry_number_second_to_be_deleted;  // randomly choosen
  uint32_t usage_entry_number_third_to_be_deleted;   // randomly choosen
  std::vector<CdmUsageEntryInfo> final_usage_entries;

  // Setup expectations
  EXPECT_CALL(*mock_usage_table_header,
              DeleteEntry(_, device_files_, NotNull()))
      .WillOnce(DoAll(SaveArg<0>(&usage_entry_number_first_to_be_deleted),
                      Invoke(this, &UsageTableHeaderTest::DeleteEntry),
                      Return(NO_ERROR)))
      .WillOnce(DoAll(SaveArg<0>(&usage_entry_number_second_to_be_deleted),
                      Invoke(this, &UsageTableHeaderTest::DeleteEntry),
                      Return(NO_ERROR)))
      .WillOnce(DoAll(SaveArg<0>(&usage_entry_number_third_to_be_deleted),
                      Invoke(this, &UsageTableHeaderTest::DeleteEntry),
                      Return(NO_ERROR)));

  EXPECT_CALL(*crypto_session_, CreateUsageEntry(NotNull()))
      .Times(4)
      .WillRepeatedly(Return(INSUFFICIENT_CRYPTO_RESOURCES_3));

  // Now invoke the method under test
  uint32_t usage_entry_number;
  EXPECT_EQ(INSUFFICIENT_CRYPTO_RESOURCES_3,
            mock_usage_table_header->AddEntry(
                crypto_session_,
                kUsageEntryInfoOfflineLicense6.storage_type == kStorageLicense,
                kUsageEntryInfoOfflineLicense6.key_set_id,
                kUsageEntryInfoOfflineLicense6.usage_info_file_name,
                &usage_entry_number));

  // Verify deleted usage entry number and entries
  EXPECT_LE(0u, usage_entry_number_first_to_be_deleted);
  EXPECT_LE(usage_entry_number_first_to_be_deleted,
            usage_entry_info_vector_at_start.size() - 1);
  EXPECT_LE(0u, usage_entry_number_second_to_be_deleted);
  EXPECT_LE(usage_entry_number_second_to_be_deleted,
            usage_entry_info_vector_at_start.size() - 1);
  EXPECT_LE(0u, usage_entry_number_third_to_be_deleted);
  EXPECT_LE(usage_entry_number_third_to_be_deleted,
            usage_entry_info_vector_at_start.size() - 1);
}

TEST_F(UsageTableHeaderTest, LoadEntry_InvalidEntryNumber) {
  Init(kSecurityLevelL1, kUsageTableHeader, kUsageEntryInfoVector);
  uint32_t usage_entry_number = kUsageEntryInfoVector.size() + 3;

  EXPECT_NE(NO_ERROR, usage_table_header_->LoadEntry(
                          crypto_session_, kUsageEntry, usage_entry_number));
}

TEST_F(UsageTableHeaderTest, LoadEntry_CryptoSessionError) {
  Init(kSecurityLevelL1, kUsageTableHeader, kUsageEntryInfoVector);
  uint32_t usage_entry_number = 1;

  EXPECT_CALL(*crypto_session_, LoadUsageEntry(usage_entry_number, kUsageEntry))
      .WillOnce(Return(LOAD_USAGE_ENTRY_GENERATION_SKEW));

  EXPECT_NE(NO_ERROR, usage_table_header_->LoadEntry(
                          crypto_session_, kUsageEntry, usage_entry_number));
}

TEST_F(UsageTableHeaderTest, LoadEntry) {
  Init(kSecurityLevelL1, kUsageTableHeader, kUsageEntryInfoVector);
  uint32_t usage_entry_number = 1;

  EXPECT_CALL(*crypto_session_, LoadUsageEntry(usage_entry_number, kUsageEntry))
      .WillOnce(Return(NO_ERROR));

  EXPECT_EQ(NO_ERROR, usage_table_header_->LoadEntry(
                          crypto_session_, kUsageEntry, usage_entry_number));
}

TEST_F(UsageTableHeaderTest, UpdateEntry_CryptoSessionError) {
  Init(kSecurityLevelL1, kUsageTableHeader, kUsageEntryInfoVector);
  CdmUsageEntry usage_entry;

  EXPECT_CALL(*crypto_session_, UpdateUsageEntry(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgPointee<0>(kUsageTableHeader),
                      SetArgPointee<1>(kUsageEntry),
                      Return(UPDATE_USAGE_ENTRY_UNKNOWN_ERROR)));

  EXPECT_NE(NO_ERROR,
            usage_table_header_->UpdateEntry(crypto_session_, &usage_entry));
}

TEST_F(UsageTableHeaderTest, UpdateEntry) {
  Init(kSecurityLevelL1, kUsageTableHeader, kUsageEntryInfoVector);
  CdmUsageEntry usage_entry;

  EXPECT_CALL(*crypto_session_, UpdateUsageEntry(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgPointee<0>(kUsageTableHeader),
                      SetArgPointee<1>(kUsageEntry), Return(NO_ERROR)));

  EXPECT_CALL(
      *device_files_,
      StoreUsageTableInfo(kUsageTableHeader,
                          UnorderedElementsAreArray(kUsageEntryInfoVector)))
      .WillOnce(Return(true));

  EXPECT_EQ(NO_ERROR,
            usage_table_header_->UpdateEntry(crypto_session_, &usage_entry));
}

TEST_F(UsageTableHeaderTest,
       LoadEntry_LoadUsageEntryFailsOnce_SucceedsSecondTime) {
  // Initialize and setup
  MockUsageTableHeader* mock_usage_table_header = SetUpMock();
  Init(kSecurityLevelL1, kUsageTableHeader, k10UsageEntryInfoVector);

  // We try to load a usage entry from the first 9 entries, since DeleteEntry
  // can't delete an entry if the last one is in use.
  int64_t usage_entry_number_to_load = MockUsageTableHeader::GetRandomInRange(
      k10UsageEntryInfoVector.size() - 1);
  ASSERT_THAT(usage_entry_number_to_load,
              AllOf(Ge(0), Lt((int64_t)k10UsageEntryInfoVector.size() - 1)));
  CdmUsageEntry usage_entry_to_load = kUsageEntry;

  // Setup expectations
  EXPECT_CALL(*mock_usage_table_header,
              DeleteEntry(_, device_files_, NotNull()))
      .Times(1)
      .WillRepeatedly(
          DoAll(Invoke(this, &UsageTableHeaderTest::DeleteEntry),
                Return(NO_ERROR)));

  EXPECT_CALL(*crypto_session_,
              LoadUsageEntry(usage_entry_number_to_load, usage_entry_to_load))
      .WillOnce(Return(INSUFFICIENT_CRYPTO_RESOURCES_3))
      .WillOnce(Return(NO_ERROR));

  // Now invoke the method under test
  EXPECT_EQ(NO_ERROR,
            mock_usage_table_header->LoadEntry(
                crypto_session_,
                usage_entry_to_load,
                usage_entry_number_to_load));
}

TEST_F(UsageTableHeaderTest,
       LoadEntry_LoadUsageEntryFailsTwice_SucceedsThirdTime) {
  // Initialize and setup
  MockUsageTableHeader* mock_usage_table_header = SetUpMock();
  Init(kSecurityLevelL1, kUsageTableHeader, k10UsageEntryInfoVector);

  // We try to load a usage entry from the first 8 entries, since DeleteEntry
  // can't delete an entry if the last one is in use.
  int64_t usage_entry_number_to_load = MockUsageTableHeader::GetRandomInRange(
      k10UsageEntryInfoVector.size() - 2);
  ASSERT_THAT(usage_entry_number_to_load,
              AllOf(Ge(0), Lt((int64_t)k10UsageEntryInfoVector.size() - 2)));
  CdmUsageEntry usage_entry_to_load = kUsageEntry;

  // Setup expectations
  EXPECT_CALL(*mock_usage_table_header,
              DeleteEntry(_, device_files_, NotNull()))
      .Times(2)
      .WillRepeatedly(
          DoAll(Invoke(this, &UsageTableHeaderTest::DeleteEntry),
                Return(NO_ERROR)));

  EXPECT_CALL(*crypto_session_,
              LoadUsageEntry(usage_entry_number_to_load, usage_entry_to_load))
      .WillOnce(Return(INSUFFICIENT_CRYPTO_RESOURCES_3))
      .WillOnce(Return(INSUFFICIENT_CRYPTO_RESOURCES_3))
      .WillOnce(Return(NO_ERROR));

  // Now invoke the method under test
  EXPECT_EQ(NO_ERROR,
            mock_usage_table_header->LoadEntry(
                crypto_session_,
                usage_entry_to_load,
                usage_entry_number_to_load));
}

TEST_F(UsageTableHeaderTest, LoadEntry_LoadUsageEntryFailsThrice) {
  // Initialize and setup
  MockUsageTableHeader* mock_usage_table_header = SetUpMock();
  Init(kSecurityLevelL1, kUsageTableHeader, k10UsageEntryInfoVector);

  // We try to load a usage entry from the first 7 entries, since DeleteEntry
  // can't delete an entry if the last one is in use.
  int64_t usage_entry_number_to_load = MockUsageTableHeader::GetRandomInRange(
      k10UsageEntryInfoVector.size() - 3);
  ASSERT_THAT(usage_entry_number_to_load,
              AllOf(Ge(0), Lt((int64_t)k10UsageEntryInfoVector.size() - 3)));
  CdmUsageEntry usage_entry_to_load = kUsageEntry;

  // Setup expectations
  EXPECT_CALL(*mock_usage_table_header,
              DeleteEntry(_, device_files_, NotNull()))
      .Times(3)
      .WillRepeatedly(
          DoAll(Invoke(this, &UsageTableHeaderTest::DeleteEntry),
                Return(NO_ERROR)));

  EXPECT_CALL(*crypto_session_,
              LoadUsageEntry(usage_entry_number_to_load, usage_entry_to_load))
      .Times(4)
      .WillRepeatedly(Return(INSUFFICIENT_CRYPTO_RESOURCES_3));

  // Now invoke the method under test
  EXPECT_EQ(INSUFFICIENT_CRYPTO_RESOURCES_3,
            mock_usage_table_header->LoadEntry(
                crypto_session_,
                usage_entry_to_load,
                usage_entry_number_to_load));
}

TEST_F(UsageTableHeaderTest, DeleteEntry_InvalidUsageEntryNumber) {
  Init(kSecurityLevelL1, kUsageTableHeader, kUsageEntryInfoVector);
  uint32_t usage_entry_number = kUsageEntryInfoVector.size();
  metrics::CryptoMetrics metrics;

  EXPECT_NE(NO_ERROR, usage_table_header_->DeleteEntry(
                          usage_entry_number, device_files_, &metrics));
}

// Initial Test state:
// 1. Entry to be delete is the last entry and is an Offline license.
//    When attempting to delete the entry a crypto session error
//    will occur.
//
// Attempting to delete the entry in (1) will result in:
// a. The usage entry requested to be deleted will not be deleted.
//
// Storage type             Usage entries
//                       at start     at end
// =============         ========     ======
// Offline License 1            0          0
// Secure Stop 1                1          1
// Storage Type unknown         2          2
// Offline License 2            3          3
//
// # of usage entries           4          4
TEST_F(UsageTableHeaderTest, DeleteEntry_CryptoSessionError) {
  std::vector<CdmUsageEntryInfo> usage_entry_info_vector;
  const CdmUsageEntryInfo usage_entry_info_array[] = {
      kUsageEntryInfoOfflineLicense1, kUsageEntryInfoSecureStop1,
      kUsageEntryInfoStorageTypeUnknown, kUsageEntryInfoOfflineLicense2};
  ToVector(usage_entry_info_vector, usage_entry_info_array,
           sizeof(usage_entry_info_array));

  Init(kSecurityLevelL1, kUsageTableHeader, usage_entry_info_vector);
  uint32_t usage_entry_number_to_be_deleted =
      usage_entry_info_vector.size() - 1;  // kUsageEntryInfoOfflineLicense2
  metrics::CryptoMetrics metrics;

  EXPECT_CALL(*crypto_session_, Open(kLevelDefault)).WillOnce(Return(NO_ERROR));
  EXPECT_CALL(
      *crypto_session_,
      ShrinkUsageTableHeader(usage_entry_info_vector.size() - 1, NotNull()))
      .WillOnce(Return(SHRINK_USAGE_TABLER_HEADER_UNKNOWN_ERROR));

  EXPECT_NE(NO_ERROR,
            usage_table_header_->DeleteEntry(usage_entry_number_to_be_deleted,
                                             device_files_, &metrics));
}

// Initial Test state:
// 1. Entry to be delete is the last entry and is an Offline license.
//
// Attempting to delete the entry in (1) will result in:
// a. The usage entry requested to be deleted will be deleted.
//
// Storage type             Usage entries
//                       at start     at end
// =============         ========     ======
// Offline License 1            0          0
// Secure Stop 1                1          1
// Storage Type unknown         2          2
// Offline License 2            3    Deleted
//
// # of usage entries           4          3
TEST_F(UsageTableHeaderTest, DeleteEntry_LastOfflineEntry) {
  std::vector<CdmUsageEntryInfo> usage_entry_info_vector;
  const CdmUsageEntryInfo usage_entry_info_array[] = {
      kUsageEntryInfoOfflineLicense1, kUsageEntryInfoSecureStop1,
      kUsageEntryInfoStorageTypeUnknown, kUsageEntryInfoOfflineLicense2};
  ToVector(usage_entry_info_vector, usage_entry_info_array,
           sizeof(usage_entry_info_array));

  Init(kSecurityLevelL1, kUsageTableHeader, usage_entry_info_vector);
  uint32_t usage_entry_number_to_be_deleted =
      usage_entry_info_vector.size() - 1;  // kUsageEntryInfoOfflineLicense2
  metrics::CryptoMetrics metrics;

  EXPECT_CALL(*crypto_session_, Open(kLevelDefault)).WillOnce(Return(NO_ERROR));
  EXPECT_CALL(
      *crypto_session_,
      ShrinkUsageTableHeader(usage_entry_info_vector.size() - 1, NotNull()))
      .WillOnce(
          DoAll(SetArgPointee<1>(kAnotherUsageTableHeader), Return(NO_ERROR)));

  EXPECT_CALL(*device_files_,
              StoreUsageTableInfo(
                  kAnotherUsageTableHeader,
                  UnorderedElementsAre(kUsageEntryInfoOfflineLicense1,
                                       kUsageEntryInfoSecureStop1,
                                       kUsageEntryInfoStorageTypeUnknown)))
      .WillOnce(Return(true));

  EXPECT_EQ(NO_ERROR,
            usage_table_header_->DeleteEntry(usage_entry_number_to_be_deleted,
                                             device_files_, &metrics));
}

// Initial Test state:
// 1. Entry to be delete is the last entry and is a secure stop.
//
// Attempting to delete the entry in (1) will result in:
// a. The usage entry requested to be deleted will be deleted.
//
// Storage type             Usage entries
//                       at start     at end
// =============         ========     ======
// Offline License 1            0          0
// Secure Stop 1                1          1
// Storage Type unknown         2          2
// Secure Stop 2                3    Deleted
//
// # of usage entries           4          3
TEST_F(UsageTableHeaderTest, DeleteEntry_LastSecureStopEntry) {
  std::vector<CdmUsageEntryInfo> usage_entry_info_vector;
    const CdmUsageEntryInfo usage_entry_info_array[] = {
      kUsageEntryInfoOfflineLicense1, kUsageEntryInfoSecureStop1,
      kUsageEntryInfoStorageTypeUnknown, kUsageEntryInfoSecureStop2};
  ToVector(usage_entry_info_vector, usage_entry_info_array,
           sizeof(usage_entry_info_array));

  Init(kSecurityLevelL1, kUsageTableHeader, usage_entry_info_vector);
  uint32_t usage_entry_number_to_be_deleted =
      usage_entry_info_vector.size() - 1;  // kUsageEntryInfoSecureStop2
  metrics::CryptoMetrics metrics;

  EXPECT_CALL(*crypto_session_, Open(kLevelDefault)).WillOnce(Return(NO_ERROR));
  EXPECT_CALL(
      *crypto_session_,
      ShrinkUsageTableHeader(usage_entry_info_vector.size() - 1, NotNull()))
      .WillOnce(
          DoAll(SetArgPointee<1>(kAnotherUsageTableHeader), Return(NO_ERROR)));

  EXPECT_CALL(*device_files_,
              StoreUsageTableInfo(
                  kAnotherUsageTableHeader,
                  UnorderedElementsAre(kUsageEntryInfoOfflineLicense1,
                                       kUsageEntryInfoSecureStop1,
                                       kUsageEntryInfoStorageTypeUnknown)))
      .WillOnce(Return(true));

  EXPECT_EQ(NO_ERROR,
            usage_table_header_->DeleteEntry(usage_entry_number_to_be_deleted,
                                             device_files_, &metrics));
}

// Initial Test state:
// 1. Last few entries are offline licenses, but have license files
//    missing from persistent storage.
// 2. Usage entry to be deleted preceeds those in (1).
//
// Attempting to delete the entry in (2) will result in:
// a. Offline entries in (1) will be deleted.
// b. The usage entry requested to be deleted will be deleted.
//
// Storage type             Usage entries
//                       at start     at end
// =============         ========     ======
// Secure Stop 1                0          0
// Storage Type unknown         1          1
// Offline License 1            2    Deleted
// Offline License 2            3    Deleted
// Offline License 3            4    Deleted
//
// # of usage entries           5          2
TEST_F(UsageTableHeaderTest,
       DeleteEntry_LastOfflineEntriesHaveMissingLicenses) {
  std::vector<CdmUsageEntryInfo> usage_entry_info_vector;
  const CdmUsageEntryInfo usage_entry_info_array[] = {
      kUsageEntryInfoSecureStop1, kUsageEntryInfoStorageTypeUnknown,
      kUsageEntryInfoOfflineLicense1, kUsageEntryInfoOfflineLicense2,
      kUsageEntryInfoOfflineLicense3};
  ToVector(usage_entry_info_vector, usage_entry_info_array,
           sizeof(usage_entry_info_array));

  Init(kSecurityLevelL1, kUsageTableHeader, usage_entry_info_vector);
  uint32_t usage_entry_number_to_be_deleted =
      usage_entry_info_vector.size() - 3;  // kUsageEntryInfoOfflineLicense1
  metrics::CryptoMetrics metrics;

  EXPECT_CALL(*crypto_session_, Open(kLevelDefault)).WillOnce(Return(NO_ERROR));
  EXPECT_CALL(
      *crypto_session_,
      ShrinkUsageTableHeader(usage_entry_number_to_be_deleted, NotNull()))
      .WillOnce(
          DoAll(SetArgPointee<1>(kAnotherUsageTableHeader), Return(NO_ERROR)));

  EXPECT_CALL(*device_files_,
              StoreUsageTableInfo(
                  kAnotherUsageTableHeader,
                  UnorderedElementsAre(kUsageEntryInfoSecureStop1,
                                       kUsageEntryInfoStorageTypeUnknown)))
      .WillOnce(Return(true));

  EXPECT_EQ(NO_ERROR,
            usage_table_header_->DeleteEntry(usage_entry_number_to_be_deleted,
                                             device_files_, &metrics));
}

// Initial Test state:
// 1. Last few entries are secure stops, but have entries
//    missing from usage info file in persistent storage.
// 2. Usage entry to be deleted preceeds those in (1).
//
// Attempting to delete the entry in (2) will result in:
// a. Secure stops in (1) will be deleted.
// b. The usage entry requested to be deleted will be deleted.
//
// Storage type             Usage entries
//                       at start     at end
// =============         ========     ======
// Offline License 1            0          0
// Storage Type unknown         1          1
// Secure stop 1                2    Deleted
// Secure stop 2                3    Deleted
// Secure stop 3                4    Deleted
//
// # of usage entries           5          2
TEST_F(UsageTableHeaderTest, DeleteEntry_LastSecureStopEntriesAreMissing) {
  std::vector<CdmUsageEntryInfo> usage_entry_info_vector;
  const CdmUsageEntryInfo usage_entry_info_array[] = {
      kUsageEntryInfoOfflineLicense1, kUsageEntryInfoStorageTypeUnknown,
      kUsageEntryInfoSecureStop1, kUsageEntryInfoSecureStop2,
      kUsageEntryInfoSecureStop3};
  ToVector(usage_entry_info_vector, usage_entry_info_array,
           sizeof(usage_entry_info_array));

  Init(kSecurityLevelL1, kUsageTableHeader, usage_entry_info_vector);
  uint32_t usage_entry_number_to_be_deleted =
      usage_entry_info_vector.size() - 3;  // kUsageEntryInfoSecureStop1
  metrics::CryptoMetrics metrics;

  EXPECT_CALL(*crypto_session_, Open(kLevelDefault)).WillOnce(Return(NO_ERROR));
  EXPECT_CALL(
      *crypto_session_,
      ShrinkUsageTableHeader(usage_entry_number_to_be_deleted, NotNull()))
      .WillOnce(
          DoAll(SetArgPointee<1>(kAnotherUsageTableHeader), Return(NO_ERROR)));

  EXPECT_CALL(*device_files_,
              RetrieveUsageInfoByKeySetId(
                  kUsageEntryInfoSecureStop2.usage_info_file_name,
                  kUsageEntryInfoSecureStop2.key_set_id, NotNull(), NotNull(),
                  NotNull(), NotNull(), NotNull()))
      .WillOnce(Return(false));
  EXPECT_CALL(*device_files_,
              RetrieveUsageInfoByKeySetId(
                  kUsageEntryInfoSecureStop3.usage_info_file_name,
                  kUsageEntryInfoSecureStop3.key_set_id, NotNull(), NotNull(),
                  NotNull(), NotNull(), NotNull()))
      .WillOnce(Return(false));

  EXPECT_CALL(*device_files_,
              StoreUsageTableInfo(
                  kAnotherUsageTableHeader,
                  UnorderedElementsAre(kUsageEntryInfoOfflineLicense1,
                                       kUsageEntryInfoStorageTypeUnknown)))
      .WillOnce(Return(true));

  EXPECT_EQ(NO_ERROR,
            usage_table_header_->DeleteEntry(usage_entry_number_to_be_deleted,
                                             device_files_, &metrics));
}

// Initial Test state:
// 1. Last few entries are offline licenses, but have incorrect usage
//    entry number stored in persistent file store.
// 2. Usage entry to be deleted preceeds those in (1).
//
// Attempting to delete the entry in (2) will result in:
// a. Offline entries in (1) will be deleted.
// b. The usage entry requested to be deleted will be deleted.
//
// Storage type             Usage entries
//                       at start     at end
// =============         ========     ======
// Secure Stop 1                0          0
// Storage Type unknown         1          1
// Offline License 1            2    Deleted
// Offline License 2            3    Deleted
// Offline License 3            4    Deleted
//
// # of usage entries           5          2
TEST_F(UsageTableHeaderTest,
       DeleteEntry_LastOfflineEntriesHaveIncorrectUsageEntryNumber) {
  std::vector<CdmUsageEntryInfo> usage_entry_info_vector;
  const CdmUsageEntryInfo usage_entry_info_array[] = {
      kUsageEntryInfoSecureStop1, kUsageEntryInfoStorageTypeUnknown,
      kUsageEntryInfoOfflineLicense1, kUsageEntryInfoOfflineLicense2,
      kUsageEntryInfoOfflineLicense3};
  ToVector(usage_entry_info_vector, usage_entry_info_array,
           sizeof(usage_entry_info_array));

  Init(kSecurityLevelL1, kUsageTableHeader, usage_entry_info_vector);
  uint32_t usage_entry_number_to_be_deleted =
      usage_entry_info_vector.size() - 3;  // kUsageEntryInfoOfflineLicense1
  metrics::CryptoMetrics metrics;
  DeviceFiles::ResponseType sub_error_code;

  EXPECT_TRUE(device_files_->StoreLicense(
      usage_entry_info_vector[usage_entry_info_vector.size() - 1].key_set_id,
      kActiveLicenseState, kPsshData, kKeyRequest, kKeyResponse,
      kKeyRenewalRequest, kKeyRenewalResponse, kReleaseServerUrl,
      kPlaybackStartTime, kPlaybackStartTime + kPlaybackDuration,
      kGracePeriodEndTime, kEmptyAppParameters, kUsageEntry,
      usage_entry_info_vector.size() - 2, &sub_error_code));
  EXPECT_EQ(DeviceFiles::kNoError, sub_error_code);

  EXPECT_TRUE(device_files_->StoreLicense(
      usage_entry_info_vector[usage_entry_info_vector.size() - 2].key_set_id,
      kActiveLicenseState, kPsshData, kKeyRequest, kKeyResponse,
      kKeyRenewalRequest, kKeyRenewalResponse, kReleaseServerUrl,
      kPlaybackStartTime, kPlaybackStartTime + kPlaybackDuration,
      kGracePeriodEndTime, kEmptyAppParameters, kUsageEntry,
      usage_entry_info_vector.size() - 3, &sub_error_code));
  EXPECT_EQ(DeviceFiles::kNoError, sub_error_code);

  EXPECT_CALL(*crypto_session_, Open(kLevelDefault)).WillOnce(Return(NO_ERROR));
  EXPECT_CALL(
      *crypto_session_,
      ShrinkUsageTableHeader(usage_entry_number_to_be_deleted, NotNull()))
      .WillOnce(
          DoAll(SetArgPointee<1>(kAnotherUsageTableHeader), Return(NO_ERROR)));

  EXPECT_CALL(*device_files_,
              StoreUsageTableInfo(
                  kAnotherUsageTableHeader,
                  UnorderedElementsAre(kUsageEntryInfoSecureStop1,
                                       kUsageEntryInfoStorageTypeUnknown)))
      .WillOnce(Return(true));

  EXPECT_EQ(NO_ERROR,
            usage_table_header_->DeleteEntry(usage_entry_number_to_be_deleted,
                                             device_files_, &metrics));
}

// Initial Test state:
// 1. Last few entries are secure stops, but have incorrect usage
//    entry number stored in persistent file store.
// 2. Usage entry to be deleted preceeds those in (1).
//
// Attempting to delete the entry in (2) will result in:
// a. Secure stops entries in (1) will be deleted.
// b. The usage entry requested to be deleted will be deleted.
//
// Storage type             Usage entries
//                       at start     at end
// =============         ========     ======
// Offline License 1            0          0
// Storage Type unknown         1          1
// Secure stop 1                2    Deleted
// Secure stop 2                3    Deleted
// Secure stop 3                4    Deleted
//
// # of usage entries           5          2
TEST_F(UsageTableHeaderTest,
       DeleteEntry_LastSecureStopEntriesHaveIncorrectUsageEntryNumber) {
  std::vector<CdmUsageEntryInfo> usage_entry_info_vector;
  const CdmUsageEntryInfo usage_entry_info_array[] = {
      kUsageEntryInfoOfflineLicense1, kUsageEntryInfoStorageTypeUnknown,
      kUsageEntryInfoSecureStop1, kUsageEntryInfoSecureStop2,
      kUsageEntryInfoSecureStop3};
  ToVector(usage_entry_info_vector, usage_entry_info_array,
           sizeof(usage_entry_info_array));

  Init(kSecurityLevelL1, kUsageTableHeader, usage_entry_info_vector);
  uint32_t usage_entry_number_to_be_deleted =
      usage_entry_info_vector.size() - 3;  // kUsageEntryInfoSecureStop1
  uint32_t usage_entry_number_after_deleted_entry =
      usage_entry_number_to_be_deleted + 1;
  metrics::CryptoMetrics metrics;

  EXPECT_CALL(*crypto_session_, Open(kLevelDefault)).WillOnce(Return(NO_ERROR));
  EXPECT_CALL(
      *crypto_session_,
      ShrinkUsageTableHeader(usage_entry_number_to_be_deleted, NotNull()))
      .WillOnce(
          DoAll(SetArgPointee<1>(kAnotherUsageTableHeader), Return(NO_ERROR)));

  EXPECT_CALL(*device_files_,
              RetrieveUsageInfoByKeySetId(
                  kUsageEntryInfoSecureStop2.usage_info_file_name,
                  kUsageEntryInfoSecureStop2.key_set_id, NotNull(), NotNull(),
                  NotNull(), NotNull(), NotNull()))
      .WillOnce(DoAll(
          SetArgPointee<2>(kProviderSessionToken),
          SetArgPointee<3>(kKeyRequest), SetArgPointee<4>(kKeyResponse),
          SetArgPointee<5>(kUsageEntry),
          SetArgPointee<6>(usage_entry_number_to_be_deleted), Return(true)));

  EXPECT_CALL(*device_files_,
              RetrieveUsageInfoByKeySetId(
                  kUsageEntryInfoSecureStop3.usage_info_file_name,
                  kUsageEntryInfoSecureStop3.key_set_id, NotNull(), NotNull(),
                  NotNull(), NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgPointee<2>(kProviderSessionToken),
                      SetArgPointee<3>(kKeyRequest),
                      SetArgPointee<4>(kKeyResponse),
                      SetArgPointee<5>(kUsageEntry),
                      SetArgPointee<6>(usage_entry_number_after_deleted_entry),
                      Return(true)));

  EXPECT_CALL(*device_files_,
              StoreUsageTableInfo(
                  kAnotherUsageTableHeader,
                  UnorderedElementsAre(kUsageEntryInfoOfflineLicense1,
                                       kUsageEntryInfoStorageTypeUnknown)))
      .WillOnce(Return(true));

  EXPECT_EQ(NO_ERROR,
            usage_table_header_->DeleteEntry(usage_entry_number_to_be_deleted,
                                             device_files_, &metrics));
}

// Initial Test state:
// 1. Last few entries are of storage type unknown.
// 2. Usage entry to be deleted preceeds those in (1).
//
// Attempting to delete the entry in (2) will result in:
// a. Entries of storage type unknown at the end will be deleted.
// b. The usage entry requested to be deleted will be deleted.
//
// Storage type             Usage entries
//                       at start     at end
// =============         ========     ======
// Secure Stop 1                0          0
// Storage Type unknown         1          1
// Offline License 1            2          2
// Offline License 2            3          3
// Offline License 3            4    Deleted
// Storage Type unknown         5    Deleted
// Storage Type unknown         6    Deleted
//
// # of usage entries           7          4
TEST_F(UsageTableHeaderTest, DeleteEntry_LastEntriesAreStorageTypeUnknown) {
  std::vector<CdmUsageEntryInfo> usage_entry_info_vector;
  const CdmUsageEntryInfo usage_entry_info_array[] = {
      kUsageEntryInfoSecureStop1,        kUsageEntryInfoOfflineLicense1,
      kUsageEntryInfoOfflineLicense2,    kUsageEntryInfoOfflineLicense3,
      kUsageEntryInfoStorageTypeUnknown, kUsageEntryInfoStorageTypeUnknown};
  ToVector(usage_entry_info_vector, usage_entry_info_array,
           sizeof(usage_entry_info_array));

  Init(kSecurityLevelL1, kUsageTableHeader, usage_entry_info_vector);
  uint32_t usage_entry_number_to_be_deleted =
      usage_entry_info_vector.size() - 3;  // kUsageEntryInfoOfflineLicense3
  metrics::CryptoMetrics metrics;

  EXPECT_CALL(*crypto_session_, Open(kLevelDefault)).WillOnce(Return(NO_ERROR));
  EXPECT_CALL(
      *crypto_session_,
      ShrinkUsageTableHeader(usage_entry_number_to_be_deleted, NotNull()))
      .WillOnce(
          DoAll(SetArgPointee<1>(kAnotherUsageTableHeader), Return(NO_ERROR)));

  EXPECT_CALL(
      *device_files_,
      StoreUsageTableInfo(kAnotherUsageTableHeader,
                          UnorderedElementsAre(kUsageEntryInfoSecureStop1,
                                               kUsageEntryInfoOfflineLicense1,
                                               kUsageEntryInfoOfflineLicense2)))
      .WillOnce(Return(true));

  EXPECT_EQ(NO_ERROR,
            usage_table_header_->DeleteEntry(usage_entry_number_to_be_deleted,
                                             device_files_, &metrics));
}

// Initial Test state:
// 1. Usage entry to be deleted is not last.
// 2. Last entry is an offline license and calling
//    OEMCrypto_MoveUsageEntry on it will fail.
//
// Attempting to delete the entry in (1) will result in:
// b. The last offline usage entry will not be deleted/moved if the
//    OEMCrypto_MoveUsageEntry operation fails.
// c. The usage entry requested to be deleted will be marked as
//     storage type unknown.
//
// Storage type             Usage entries
//                       at start     at end
// =============         ========     ======
// Secure Stop 1                0          0
// Storage Type unknown         1          1
// Offline License 1            2  Deleted/storage type unknown
// Offline License 2            3          3
// Offline License 3            4          4
//
// # of usage entries           5          5
TEST_F(UsageTableHeaderTest,
       DeleteEntry_LastEntryIsOffline_MoveOfflineEntryFailed) {
  std::vector<CdmUsageEntryInfo> usage_entry_info_vector;
  const CdmUsageEntryInfo usage_entry_info_array[] = {
      kUsageEntryInfoSecureStop1, kUsageEntryInfoStorageTypeUnknown,
      kUsageEntryInfoOfflineLicense1, kUsageEntryInfoOfflineLicense2,
      kUsageEntryInfoOfflineLicense3};
  ToVector(usage_entry_info_vector, usage_entry_info_array,
           sizeof(usage_entry_info_array));

  Init(kSecurityLevelL1, kUsageTableHeader, usage_entry_info_vector);
  uint32_t usage_entry_number_to_be_deleted =
      usage_entry_info_vector.size() - 3;  // kUsageEntryInfoOfflineLicense1
  uint32_t last_usage_entry_number =
      usage_entry_info_vector.size() - 1;  // kUsageEntryInfoOfflineLicense3
  metrics::CryptoMetrics metrics;

  DeviceFiles::ResponseType sub_error_code;
  EXPECT_TRUE(device_files_->StoreLicense(
      usage_entry_info_vector[last_usage_entry_number].key_set_id,
      kActiveLicenseState, kPsshData, kKeyRequest, kKeyResponse,
      kKeyRenewalRequest, kKeyRenewalResponse, kReleaseServerUrl,
      kPlaybackStartTime, kPlaybackStartTime + kPlaybackDuration,
      kGracePeriodEndTime, kEmptyAppParameters, kUsageEntry,
      last_usage_entry_number, &sub_error_code));
  EXPECT_EQ(DeviceFiles::kNoError, sub_error_code);

  EXPECT_CALL(*crypto_session_, Open(kLevelDefault)).WillOnce(Return(NO_ERROR));
  EXPECT_CALL(*crypto_session_,
              LoadUsageEntry(last_usage_entry_number, kUsageEntry))
      .WillOnce(Return(NO_ERROR));
  EXPECT_CALL(*crypto_session_,
              MoveUsageEntry(usage_entry_number_to_be_deleted))
      .WillOnce(Return(MOVE_USAGE_ENTRY_UNKNOWN_ERROR));

  EXPECT_CALL(
      *device_files_,
      StoreUsageTableInfo(
          kUsageTableHeader,
          UnorderedElementsAre(
              kUsageEntryInfoSecureStop1, kUsageEntryInfoStorageTypeUnknown,
              kUsageEntryInfoStorageTypeUnknown, kUsageEntryInfoOfflineLicense2,
              kUsageEntryInfoOfflineLicense3)))
      .WillOnce(Return(true));

  EXPECT_EQ(NO_ERROR,
            usage_table_header_->DeleteEntry(usage_entry_number_to_be_deleted,
                                             device_files_, &metrics));
}

// Initial Test state:
// 1. Usage entry to be deleted is not last.
// 2. Last entry is an secure stop and calling
//    OEMCrypto_MoveUsageEntry on it will fail.
//
// Attempting to delete the entry in (1) will result in:
// b. The last secure stop usage entry will not be deleted/moved if the
//    OEMCrypto_MoveUsageEntry operation fails.
// c. The usage entry requested to be deleted will be marked as
//     storage type unknown.
//
// Storage type             Usage entries
//                       at start     at end
// =============         ========     ======
// Offline License 1            0          0
// Storage Type unknown         1          1
// Secure stop 1                2  Deleted/storage type unknown
// Secure stop 2                3          3
// Secure stop 3                4          4
//
// # of usage entries           5          5
TEST_F(UsageTableHeaderTest,
       DeleteEntry_LastEntryIsSecureStop_MoveSecureStopEntryFailed) {
  std::vector<CdmUsageEntryInfo> usage_entry_info_vector;
  const CdmUsageEntryInfo usage_entry_info_array[] = {
      kUsageEntryInfoOfflineLicense1, kUsageEntryInfoStorageTypeUnknown,
      kUsageEntryInfoSecureStop1, kUsageEntryInfoSecureStop2,
      kUsageEntryInfoSecureStop3};
  ToVector(usage_entry_info_vector, usage_entry_info_array,
           sizeof(usage_entry_info_array));

  Init(kSecurityLevelL1, kUsageTableHeader, usage_entry_info_vector);
  uint32_t usage_entry_number_to_be_deleted =
      usage_entry_info_vector.size() - 3;  // kUsageEntryInfoSecureStop1
  uint32_t last_usage_entry_number =
      usage_entry_info_vector.size() - 1;  // kUsageEntryInfoSecureStop3
  metrics::CryptoMetrics metrics;

  EXPECT_CALL(*crypto_session_, Open(kLevelDefault)).WillOnce(Return(NO_ERROR));
  EXPECT_CALL(*crypto_session_,
              LoadUsageEntry(last_usage_entry_number, kUsageEntry))
      .WillOnce(Return(NO_ERROR));
  EXPECT_CALL(*crypto_session_,
              MoveUsageEntry(usage_entry_number_to_be_deleted))
      .WillOnce(Return(MOVE_USAGE_ENTRY_UNKNOWN_ERROR));

  EXPECT_CALL(*device_files_,
              RetrieveUsageInfoByKeySetId(
                  kUsageEntryInfoSecureStop3.usage_info_file_name,
                  kUsageEntryInfoSecureStop3.key_set_id, NotNull(), NotNull(),
                  NotNull(), NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgPointee<2>(kProviderSessionToken),
                      SetArgPointee<3>(kKeyRequest),
                      SetArgPointee<4>(kKeyResponse),
                      SetArgPointee<5>(kUsageEntry),
                      SetArgPointee<6>(last_usage_entry_number), Return(true)));

  EXPECT_CALL(
      *device_files_,
      StoreUsageTableInfo(
          kUsageTableHeader,
          UnorderedElementsAre(
              kUsageEntryInfoOfflineLicense1, kUsageEntryInfoStorageTypeUnknown,
              kUsageEntryInfoStorageTypeUnknown, kUsageEntryInfoSecureStop2,
              kUsageEntryInfoSecureStop3)))
      .WillOnce(Return(true));

  EXPECT_EQ(NO_ERROR,
            usage_table_header_->DeleteEntry(usage_entry_number_to_be_deleted,
                                             device_files_, &metrics));
}

// Initial Test state:
// 1. Usage entry to be deleted is not last
// 2. Last few entries are of storage type unknown.
// 3. Entry that preceeds those in (2) is an offline license and calling
//    OEMCrypto_MoveUsageEntry on it will fail.
//
// Attempting to delete the entry in (1) will result in:
// a. Entries of storage type unknown at the end will be deleted.
// b. The offline usage entry that preceeds the entries in (a) will
//    not be deleted/moved if the OEMCrypto_MoveUsageEntry operation fails.
// c. The usage entry requested to be deleted will be marked as
//     storage type unknown.
//
// Storage type             Usage entries
//                       at start     at end
// =============         ========     ======
// Secure Stop 1                0          0
// Storage Type unknown         1          1
// Offline License 1            2  Deleted/storage type unknown
// Offline License 2            3          3
// Offline License 3            4          4
// Storage Type unknown         5    Deleted
// Storage Type unknown         6    Deleted
//
// # of usage entries           7          5
TEST_F(UsageTableHeaderTest,
       DeleteEntry_LastEntriesAreOfflineAndUnknown_MoveOfflineEntryFailed) {
  std::vector<CdmUsageEntryInfo> usage_entry_info_vector;
  const CdmUsageEntryInfo usage_entry_info_array[] = {
      kUsageEntryInfoSecureStop1,       kUsageEntryInfoStorageTypeUnknown,
      kUsageEntryInfoOfflineLicense1,   kUsageEntryInfoOfflineLicense2,
      kUsageEntryInfoOfflineLicense3,   kUsageEntryInfoStorageTypeUnknown,
      kUsageEntryInfoStorageTypeUnknown};
  ToVector(usage_entry_info_vector, usage_entry_info_array,
           sizeof(usage_entry_info_array));

  Init(kSecurityLevelL1, kUsageTableHeader, usage_entry_info_vector);
  uint32_t usage_entry_number_to_be_deleted =
      usage_entry_info_vector.size() - 5;  // kUsageEntryInfoOfflineLicense1
  uint32_t last_valid_usage_entry_number =
      usage_entry_info_vector.size() - 3;  // kUsageEntryInfoOfflineLicense3
  metrics::CryptoMetrics metrics;

  DeviceFiles::ResponseType sub_error_code;
  EXPECT_TRUE(device_files_->StoreLicense(
      usage_entry_info_vector[last_valid_usage_entry_number].key_set_id,
      kActiveLicenseState, kPsshData, kKeyRequest, kKeyResponse,
      kKeyRenewalRequest, kKeyRenewalResponse, kReleaseServerUrl,
      kPlaybackStartTime, kPlaybackStartTime + kPlaybackDuration,
      kGracePeriodEndTime, kEmptyAppParameters, kUsageEntry,
      last_valid_usage_entry_number, &sub_error_code));
  EXPECT_EQ(DeviceFiles::kNoError, sub_error_code);

  EXPECT_CALL(*crypto_session_, Open(kLevelDefault))
      .Times(2)
      .WillRepeatedly(Return(NO_ERROR));
  EXPECT_CALL(*crypto_session_,
              LoadUsageEntry(last_valid_usage_entry_number, kUsageEntry))
      .WillOnce(Return(NO_ERROR));
  EXPECT_CALL(*crypto_session_,
              MoveUsageEntry(usage_entry_number_to_be_deleted))
      .WillOnce(Return(MOVE_USAGE_ENTRY_UNKNOWN_ERROR));
  EXPECT_CALL(
      *crypto_session_,
      ShrinkUsageTableHeader(last_valid_usage_entry_number + 1, NotNull()))
      .WillOnce(
          DoAll(SetArgPointee<1>(kAnotherUsageTableHeader), Return(NO_ERROR)));

  EXPECT_CALL(
      *device_files_,
      StoreUsageTableInfo(
          kAnotherUsageTableHeader,
          UnorderedElementsAre(
              kUsageEntryInfoSecureStop1, kUsageEntryInfoStorageTypeUnknown,
              kUsageEntryInfoStorageTypeUnknown, kUsageEntryInfoOfflineLicense2,
              kUsageEntryInfoOfflineLicense3)))
      .WillOnce(Return(true));

  EXPECT_EQ(NO_ERROR,
            usage_table_header_->DeleteEntry(usage_entry_number_to_be_deleted,
                                             device_files_, &metrics));
}

// Initial Test state:
// 1. Usage entry to be deleted is not last
// 2. Last few entries are of storage type unknown.
// 3. Entry that preceeds those in (2) is an offline license and calling
//    OEMCrypto_MoveUsageEntry on it will fail.
//
// Attempting to delete the entry in (1) will result in:
// a. Entries of storage type unknown at the end will be deleted.
// b. The offline usage entry that preceeds the entries in (a) will
//    not be deleted/moved if the OEMCrypto_MoveUsageEntry operation fails.
// c. The usage entry requested to be deleted will be marked as
//     storage type unknown.
//
// Storage type             Usage entries
//                       at start     at end
// =============         ========     ======
// Offline License 1            0          0
// Storage Type unknown         1          1
// Secure stop 1                2  Deleted/storage type unknown
// Secure stop 2                3          3
// Secure stop 3                4          4
// Storage Type unknown         5    Deleted
// Storage Type unknown         6    Deleted
//
// # of usage entries           7          5
TEST_F(UsageTableHeaderTest,
       DeleteEntry_LastEntriesAreSecureStopAndUnknown_MoveOfflineEntryFailed) {
  std::vector<CdmUsageEntryInfo> usage_entry_info_vector;
  const CdmUsageEntryInfo usage_entry_info_array[] = {
      kUsageEntryInfoOfflineLicense1,   kUsageEntryInfoStorageTypeUnknown,
      kUsageEntryInfoSecureStop1,       kUsageEntryInfoSecureStop2,
      kUsageEntryInfoSecureStop3,       kUsageEntryInfoStorageTypeUnknown,
      kUsageEntryInfoStorageTypeUnknown};
  ToVector(usage_entry_info_vector, usage_entry_info_array,
           sizeof(usage_entry_info_array));

  Init(kSecurityLevelL1, kUsageTableHeader, usage_entry_info_vector);
  uint32_t usage_entry_number_to_be_deleted =
      usage_entry_info_vector.size() - 5;  // kUsageEntryInfoOfflineLicense1
  uint32_t last_valid_usage_entry_number =
      usage_entry_info_vector.size() - 3;  // kUsageEntryInfoOfflineLicense3
  metrics::CryptoMetrics metrics;

  EXPECT_CALL(*crypto_session_, Open(kLevelDefault))
      .Times(2)
      .WillRepeatedly(Return(NO_ERROR));
  EXPECT_CALL(*crypto_session_,
              LoadUsageEntry(last_valid_usage_entry_number, kUsageEntry))
      .WillOnce(Return(NO_ERROR));
  EXPECT_CALL(*crypto_session_,
              MoveUsageEntry(usage_entry_number_to_be_deleted))
      .WillOnce(Return(MOVE_USAGE_ENTRY_UNKNOWN_ERROR));

  EXPECT_CALL(*device_files_,
              RetrieveUsageInfoByKeySetId(
                  kUsageEntryInfoSecureStop3.usage_info_file_name,
                  kUsageEntryInfoSecureStop3.key_set_id, NotNull(), NotNull(),
                  NotNull(), NotNull(), NotNull()))
      .WillOnce(
          DoAll(SetArgPointee<2>(kProviderSessionToken),
                SetArgPointee<3>(kKeyRequest), SetArgPointee<4>(kKeyResponse),
                SetArgPointee<5>(kUsageEntry),
                SetArgPointee<6>(last_valid_usage_entry_number), Return(true)));
  EXPECT_CALL(
      *crypto_session_,
      ShrinkUsageTableHeader(last_valid_usage_entry_number + 1, NotNull()))
      .WillOnce(
          DoAll(SetArgPointee<1>(kAnotherUsageTableHeader), Return(NO_ERROR)));

  EXPECT_CALL(
      *device_files_,
      StoreUsageTableInfo(
          kAnotherUsageTableHeader,
          UnorderedElementsAre(
              kUsageEntryInfoOfflineLicense1, kUsageEntryInfoStorageTypeUnknown,
              kUsageEntryInfoStorageTypeUnknown, kUsageEntryInfoSecureStop2,
              kUsageEntryInfoSecureStop3)))
      .WillOnce(Return(true));

  EXPECT_EQ(NO_ERROR,
            usage_table_header_->DeleteEntry(usage_entry_number_to_be_deleted,
                                             device_files_, &metrics));
}

// Initial Test state:
// 1. Usage entry to be deleted is not last.
// 2. Last entry is an offline license.
//
// Attempting to delete the entry in (1) will result in:
// a. The usage entry requested to be deleted will be replaced with the last
//    entry.
//
// Storage type             Usage entries
//                       at start     at end
// =============         ========     ======
// Secure Stop 1                0          0
// Storage Type unknown         1          1
// Offline License 1            2    Deleted
// Offline License 2            3          3
// Offline License 3            4          2
//
// # of usage entries           5          4
TEST_F(UsageTableHeaderTest, DeleteEntry_LastEntryIsOffline) {
  std::vector<CdmUsageEntryInfo> usage_entry_info_vector;
  const CdmUsageEntryInfo usage_entry_info_array[] = {
      kUsageEntryInfoSecureStop1, kUsageEntryInfoStorageTypeUnknown,
      kUsageEntryInfoOfflineLicense1, kUsageEntryInfoOfflineLicense2,
      kUsageEntryInfoOfflineLicense3};
  ToVector(usage_entry_info_vector, usage_entry_info_array,
           sizeof(usage_entry_info_array));

  Init(kSecurityLevelL1, kUsageTableHeader, usage_entry_info_vector);
  uint32_t usage_entry_number_to_be_deleted =
      usage_entry_info_vector.size() - 3;  // kUsageEntryInfoOfflineLicense1
  uint32_t last_usage_entry_number =
      usage_entry_info_vector.size() - 1;  // kUsageEntryInfoOfflineLicense3
  metrics::CryptoMetrics metrics;

  DeviceFiles::ResponseType sub_error_code;
  EXPECT_TRUE(device_files_->StoreLicense(
      usage_entry_info_vector[last_usage_entry_number].key_set_id,
      kActiveLicenseState, kPsshData, kKeyRequest, kKeyResponse,
      kKeyRenewalRequest, kKeyRenewalResponse, kReleaseServerUrl,
      kPlaybackStartTime, kPlaybackStartTime + kPlaybackDuration,
      kGracePeriodEndTime, kEmptyAppParameters, kUsageEntry,
      last_usage_entry_number, &sub_error_code));
  EXPECT_EQ(DeviceFiles::kNoError, sub_error_code);

  EXPECT_CALL(*crypto_session_, Open(kLevelDefault))
      .Times(2)
      .WillRepeatedly(Return(NO_ERROR));
  EXPECT_CALL(*crypto_session_,
              LoadUsageEntry(last_usage_entry_number, kUsageEntry))
      .WillOnce(Return(NO_ERROR));
  EXPECT_CALL(*crypto_session_,
              MoveUsageEntry(usage_entry_number_to_be_deleted))
      .WillOnce(Return(NO_ERROR));
  EXPECT_CALL(*crypto_session_, UpdateUsageEntry(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgPointee<0>(kAnotherUsageTableHeader),
                      SetArgPointee<1>(kAnotherUsageEntry), Return(NO_ERROR)));
  EXPECT_CALL(*crypto_session_,
              ShrinkUsageTableHeader(last_usage_entry_number, NotNull()))
      .WillOnce(
          DoAll(SetArgPointee<1>(kAnotherUsageTableHeader), Return(NO_ERROR)));

  EXPECT_CALL(
      *device_files_,
      StoreUsageTableInfo(
          kAnotherUsageTableHeader,
          UnorderedElementsAre(
              kUsageEntryInfoSecureStop1, kUsageEntryInfoStorageTypeUnknown,
              kUsageEntryInfoOfflineLicense3, kUsageEntryInfoOfflineLicense2)))
      .WillOnce(Return(true));

  EXPECT_CALL(
      *device_files_,
      StoreUsageTableInfo(
          kAnotherUsageTableHeader,
          UnorderedElementsAre(
              kUsageEntryInfoSecureStop1, kUsageEntryInfoStorageTypeUnknown,
              kUsageEntryInfoOfflineLicense3, kUsageEntryInfoOfflineLicense2,
              kUsageEntryInfoOfflineLicense3)))
      .WillOnce(Return(true));

  EXPECT_EQ(NO_ERROR,
            usage_table_header_->DeleteEntry(usage_entry_number_to_be_deleted,
                                             device_files_, &metrics));

  DeviceFiles::LicenseState license_state = DeviceFiles::kLicenseStateUnknown;
  CdmInitData pssh_data;
  CdmKeyMessage key_request;
  CdmKeyResponse key_response;
  CdmKeyMessage key_renewal_request;
  CdmKeyResponse key_renewal_response;
  std::string release_server_url;
  int64_t playback_start_time;
  int64_t last_playback_time;
  int64_t grace_period_end_time;
  CdmAppParameterMap app_parameters;
  CdmUsageEntry usage_entry;
  uint32_t usage_entry_number = ~0;

  EXPECT_TRUE(device_files_->RetrieveLicense(
      kUsageEntryInfoOfflineLicense3.key_set_id, &license_state, &pssh_data,
      &key_request, &key_response, &key_renewal_request, &key_renewal_response,
      &release_server_url, &playback_start_time, &last_playback_time,
      &grace_period_end_time, &app_parameters, &usage_entry,
      &usage_entry_number, &sub_error_code));
  EXPECT_EQ(kActiveLicenseState, license_state);
  EXPECT_EQ(kPsshData, pssh_data);
  EXPECT_EQ(kKeyRequest, key_request);
  EXPECT_EQ(kKeyResponse, key_response);
  EXPECT_EQ(kKeyRenewalRequest, key_renewal_request);
  EXPECT_EQ(kKeyRenewalResponse, key_renewal_response);
  EXPECT_EQ(kReleaseServerUrl, release_server_url);
  EXPECT_EQ(kPlaybackStartTime, playback_start_time);
  EXPECT_EQ(kPlaybackStartTime + kPlaybackDuration, last_playback_time);
  EXPECT_EQ(kGracePeriodEndTime, grace_period_end_time);
  EXPECT_EQ(kEmptyAppParameters.size(), app_parameters.size());
  EXPECT_EQ(kAnotherUsageEntry, usage_entry);
  EXPECT_EQ(usage_entry_number_to_be_deleted, usage_entry_number);
  EXPECT_EQ(DeviceFiles::kNoError, sub_error_code);
}

// Initial Test state:
// 1. Usage entry to be deleted is not last.
// 2. Last entry is a secure stop.
//
// Attempting to delete the entry in (1) will result in:
// a. The usage entry requested to be deleted will be replaced with the last
//    entry.
//
// Storage type             Usage entries
//                       at start     at end
// =============         ========     ======
// Offline License 1            0          0
// Storage Type unknown         1          1
// Secure stop 1                2    Deleted
// Secure stop 2                3          3
// Secure stop 3                4          2
//
// # of usage entries           5          4
TEST_F(UsageTableHeaderTest, DeleteEntry_LastEntryIsSecureStop) {
  std::vector<CdmUsageEntryInfo> usage_entry_info_vector;
  const CdmUsageEntryInfo usage_entry_info_array[] = {
      kUsageEntryInfoOfflineLicense1, kUsageEntryInfoStorageTypeUnknown,
      kUsageEntryInfoSecureStop1, kUsageEntryInfoSecureStop2,
      kUsageEntryInfoSecureStop3};
  ToVector(usage_entry_info_vector, usage_entry_info_array,
           sizeof(usage_entry_info_array));

  Init(kSecurityLevelL1, kUsageTableHeader, usage_entry_info_vector);
  uint32_t usage_entry_number_to_be_deleted =
      usage_entry_info_vector.size() - 3;  // kUsageEntryInfoSecureStop1
  uint32_t last_usage_entry_number =
      usage_entry_info_vector.size() - 1;  // kUsageEntryInfoSecureStop3
  metrics::CryptoMetrics metrics;

  EXPECT_CALL(*crypto_session_, Open(kLevelDefault))
      .Times(2)
      .WillRepeatedly(Return(NO_ERROR));
  EXPECT_CALL(*crypto_session_,
              LoadUsageEntry(last_usage_entry_number, kUsageEntry))
      .WillOnce(Return(NO_ERROR));
  EXPECT_CALL(*crypto_session_,
              MoveUsageEntry(usage_entry_number_to_be_deleted))
      .WillOnce(Return(NO_ERROR));
  EXPECT_CALL(*crypto_session_, UpdateUsageEntry(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgPointee<0>(kAnotherUsageTableHeader),
                      SetArgPointee<1>(kAnotherUsageEntry), Return(NO_ERROR)));
  EXPECT_CALL(*crypto_session_,
              ShrinkUsageTableHeader(last_usage_entry_number, NotNull()))
      .WillOnce(
          DoAll(SetArgPointee<1>(kAnotherUsageTableHeader), Return(NO_ERROR)));

  EXPECT_CALL(*device_files_,
              RetrieveUsageInfoByKeySetId(
                  kUsageEntryInfoSecureStop3.usage_info_file_name,
                  kUsageEntryInfoSecureStop3.key_set_id, NotNull(), NotNull(),
                  NotNull(), NotNull(), NotNull()))
      .WillRepeatedly(
          DoAll(SetArgPointee<2>(kProviderSessionToken),
                SetArgPointee<3>(kKeyRequest), SetArgPointee<4>(kKeyResponse),
                SetArgPointee<5>(kUsageEntry),
                SetArgPointee<6>(last_usage_entry_number), Return(true)));

  EXPECT_CALL(*device_files_,
              DeleteUsageInfo(kUsageEntryInfoSecureStop3.usage_info_file_name,
                              kProviderSessionToken))
      .WillOnce(Return(true));

  EXPECT_CALL(
      *device_files_,
      StoreUsageInfo(kProviderSessionToken, kKeyRequest, kKeyResponse,
                     kUsageEntryInfoSecureStop3.usage_info_file_name,
                     kUsageEntryInfoSecureStop3.key_set_id, kAnotherUsageEntry,
                     usage_entry_number_to_be_deleted))
      .WillOnce(Return(true));

  EXPECT_CALL(
      *device_files_,
      StoreUsageTableInfo(
          kAnotherUsageTableHeader,
          UnorderedElementsAre(
              kUsageEntryInfoOfflineLicense1, kUsageEntryInfoStorageTypeUnknown,
              kUsageEntryInfoSecureStop3, kUsageEntryInfoSecureStop2)))
      .WillOnce(Return(true));

  EXPECT_CALL(
      *device_files_,
      StoreUsageTableInfo(
          kAnotherUsageTableHeader,
          UnorderedElementsAre(
              kUsageEntryInfoOfflineLicense1, kUsageEntryInfoStorageTypeUnknown,
              kUsageEntryInfoSecureStop3, kUsageEntryInfoSecureStop2,
              kUsageEntryInfoSecureStop3)))
      .WillOnce(Return(true));

  EXPECT_EQ(NO_ERROR,
            usage_table_header_->DeleteEntry(usage_entry_number_to_be_deleted,
                                             device_files_, &metrics));
}

// Initial Test state:
// 1. Usage entry to be deleted is not last.
// 2. Last few entries are of storage type unknown.
// 3. Entry that preceeds those in (2) is an offline license.
//
// Attempting to delete the entry in (1) will result in:
// a. The entry being deleted and replaced with the offline entry in (3).
// b. The entries with unknown storage type in (2) will be deleted.
//
// Storage type             Usage entries
//                       at start     at end
// =============         ========     ======
// Secure Stop 1                0          0
// Storage Type unknown         1          1
// Offline License 1            2    Deleted
// Offline License 2            3          3
// Offline License 3            4          2
// Storage Type unknown         5    Deleted
// Storage Type unknown         6    Deleted
//
// # of usage entries           7          4
TEST_F(UsageTableHeaderTest, DeleteEntry_LastEntriesAreOfflineAndUnknknown) {
  std::vector<CdmUsageEntryInfo> usage_entry_info_vector;
  const CdmUsageEntryInfo usage_entry_info_array[] = {
      kUsageEntryInfoSecureStop1,       kUsageEntryInfoStorageTypeUnknown,
      kUsageEntryInfoOfflineLicense1,   kUsageEntryInfoOfflineLicense2,
      kUsageEntryInfoOfflineLicense3,   kUsageEntryInfoStorageTypeUnknown,
      kUsageEntryInfoStorageTypeUnknown};
  ToVector(usage_entry_info_vector, usage_entry_info_array,
           sizeof(usage_entry_info_array));

  Init(kSecurityLevelL1, kUsageTableHeader, usage_entry_info_vector);
  uint32_t usage_entry_number_to_be_deleted =
      usage_entry_info_vector.size() - 5;  // kUsageEntryInfoOfflineLicense1
  uint32_t last_valid_usage_entry_number =
      usage_entry_info_vector.size() - 3;  // kUsageEntryInfoOfflineLicense3
  metrics::CryptoMetrics metrics;

  DeviceFiles::ResponseType sub_error_code;
  EXPECT_TRUE(device_files_->StoreLicense(
      usage_entry_info_vector[last_valid_usage_entry_number].key_set_id,
      kActiveLicenseState, kPsshData, kKeyRequest, kKeyResponse,
      kKeyRenewalRequest, kKeyRenewalResponse, kReleaseServerUrl,
      kPlaybackStartTime, kPlaybackStartTime + kPlaybackDuration,
      kGracePeriodEndTime, kEmptyAppParameters, kUsageEntry,
      last_valid_usage_entry_number, &sub_error_code));
  EXPECT_EQ(DeviceFiles::kNoError, sub_error_code);

  EXPECT_CALL(*crypto_session_, Open(kLevelDefault))
      .Times(2)
      .WillRepeatedly(Return(NO_ERROR));
  EXPECT_CALL(*crypto_session_,
              LoadUsageEntry(last_valid_usage_entry_number, kUsageEntry))
      .WillOnce(Return(NO_ERROR));
  EXPECT_CALL(*crypto_session_,
              MoveUsageEntry(usage_entry_number_to_be_deleted))
      .WillOnce(Return(NO_ERROR));
  EXPECT_CALL(*crypto_session_, UpdateUsageEntry(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgPointee<0>(kAnotherUsageTableHeader),
                      SetArgPointee<1>(kAnotherUsageEntry), Return(NO_ERROR)));
  EXPECT_CALL(*crypto_session_,
              ShrinkUsageTableHeader(last_valid_usage_entry_number, NotNull()))
      .WillOnce(
          DoAll(SetArgPointee<1>(kAnotherUsageTableHeader), Return(NO_ERROR)));

  EXPECT_CALL(
      *device_files_,
      StoreUsageTableInfo(
          kAnotherUsageTableHeader,
          UnorderedElementsAre(
              kUsageEntryInfoSecureStop1, kUsageEntryInfoStorageTypeUnknown,
              kUsageEntryInfoOfflineLicense3, kUsageEntryInfoOfflineLicense2)))
      .WillOnce(Return(true));

  EXPECT_CALL(
      *device_files_,
      StoreUsageTableInfo(
          kAnotherUsageTableHeader,
          UnorderedElementsAre(
              kUsageEntryInfoSecureStop1, kUsageEntryInfoStorageTypeUnknown,
              kUsageEntryInfoOfflineLicense3, kUsageEntryInfoOfflineLicense2,
              kUsageEntryInfoOfflineLicense3, kUsageEntryInfoStorageTypeUnknown,
              kUsageEntryInfoStorageTypeUnknown)))
      .WillOnce(Return(true));

  EXPECT_EQ(NO_ERROR,
            usage_table_header_->DeleteEntry(usage_entry_number_to_be_deleted,
                                             device_files_, &metrics));

  DeviceFiles::LicenseState license_state = DeviceFiles::kLicenseStateUnknown;
  CdmInitData pssh_data;
  CdmKeyMessage key_request;
  CdmKeyResponse key_response;
  CdmKeyMessage key_renewal_request;
  CdmKeyResponse key_renewal_response;
  std::string release_server_url;
  int64_t playback_start_time;
  int64_t last_playback_time;
  int64_t grace_period_end_time;
  CdmAppParameterMap app_parameters;
  CdmUsageEntry usage_entry;
  uint32_t usage_entry_number = ~0;

  EXPECT_TRUE(device_files_->RetrieveLicense(
      kUsageEntryInfoOfflineLicense3.key_set_id, &license_state, &pssh_data,
      &key_request, &key_response, &key_renewal_request, &key_renewal_response,
      &release_server_url, &playback_start_time, &last_playback_time,
      &grace_period_end_time, &app_parameters, &usage_entry,
      &usage_entry_number, &sub_error_code));
  EXPECT_EQ(kActiveLicenseState, license_state);
  EXPECT_EQ(kPsshData, pssh_data);
  EXPECT_EQ(kKeyRequest, key_request);
  EXPECT_EQ(kKeyResponse, key_response);
  EXPECT_EQ(kKeyRenewalRequest, key_renewal_request);
  EXPECT_EQ(kKeyRenewalResponse, key_renewal_response);
  EXPECT_EQ(kReleaseServerUrl, release_server_url);
  EXPECT_EQ(kPlaybackStartTime, playback_start_time);
  EXPECT_EQ(kPlaybackStartTime + kPlaybackDuration, last_playback_time);
  EXPECT_EQ(kGracePeriodEndTime, grace_period_end_time);
  EXPECT_EQ(kEmptyAppParameters.size(), app_parameters.size());
  EXPECT_EQ(kAnotherUsageEntry, usage_entry);
  EXPECT_EQ(usage_entry_number_to_be_deleted, usage_entry_number);
  EXPECT_EQ(DeviceFiles::kNoError, sub_error_code);
}

// Initial Test state:
// 1. Usage entry to be deleted is not last.
// 2. Last few entries are of storage type unknown.
// 3. Entry that preceeds those in (2) is a secure stop.
//
// Attempting to delete the entry in (1) will result in:
// a. The entry being deleted and replaced with the secure stop entry in (3).
// b. The entries with unknown storage type in (2) will be deleted.
//
// Storage type             Usage entries
//                       at start     at end
// =============         ========     ======
// Offline License 1            0          0
// Storage Type unknown         1          1
// Secure stop 1                2    Deleted
// Secure stop 2                3          3
// Secure stop 3                4          2
// Storage Type unknown         5    Deleted
// Storage Type unknown         6    Deleted
//
// # of usage entries           7          4
TEST_F(UsageTableHeaderTest, DeleteEntry_LastEntriesAreSecureStopAndUnknknown) {
  std::vector<CdmUsageEntryInfo> usage_entry_info_vector;
  const CdmUsageEntryInfo usage_entry_info_array[] = {
      kUsageEntryInfoOfflineLicense1,   kUsageEntryInfoStorageTypeUnknown,
      kUsageEntryInfoSecureStop1,       kUsageEntryInfoSecureStop2,
      kUsageEntryInfoSecureStop3,       kUsageEntryInfoStorageTypeUnknown,
      kUsageEntryInfoStorageTypeUnknown};
  ToVector(usage_entry_info_vector, usage_entry_info_array,
           sizeof(usage_entry_info_array));

  Init(kSecurityLevelL1, kUsageTableHeader, usage_entry_info_vector);
  uint32_t usage_entry_number_to_be_deleted =
      usage_entry_info_vector.size() - 5;  // kUsageEntryInfoSecureStop1
  uint32_t last_valid_usage_entry_number =
      usage_entry_info_vector.size() - 3;  // kUsageEntryInfoSecureStop3
  metrics::CryptoMetrics metrics;

  EXPECT_CALL(*crypto_session_, Open(kLevelDefault))
      .Times(2)
      .WillRepeatedly(Return(NO_ERROR));
  EXPECT_CALL(*crypto_session_,
              LoadUsageEntry(last_valid_usage_entry_number, kUsageEntry))
      .WillOnce(Return(NO_ERROR));
  EXPECT_CALL(*crypto_session_,
              MoveUsageEntry(usage_entry_number_to_be_deleted))
      .WillOnce(Return(NO_ERROR));
  EXPECT_CALL(*crypto_session_, UpdateUsageEntry(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgPointee<0>(kAnotherUsageTableHeader),
                      SetArgPointee<1>(kAnotherUsageEntry), Return(NO_ERROR)));
  EXPECT_CALL(*crypto_session_,
              ShrinkUsageTableHeader(last_valid_usage_entry_number, NotNull()))
      .WillOnce(
          DoAll(SetArgPointee<1>(kAnotherUsageTableHeader), Return(NO_ERROR)));

  EXPECT_CALL(*device_files_,
              RetrieveUsageInfoByKeySetId(
                  kUsageEntryInfoSecureStop3.usage_info_file_name,
                  kUsageEntryInfoSecureStop3.key_set_id, NotNull(), NotNull(),
                  NotNull(), NotNull(), NotNull()))
      .WillRepeatedly(
          DoAll(SetArgPointee<2>(kProviderSessionToken),
                SetArgPointee<3>(kKeyRequest), SetArgPointee<4>(kKeyResponse),
                SetArgPointee<5>(kUsageEntry),
                SetArgPointee<6>(last_valid_usage_entry_number), Return(true)));

  EXPECT_CALL(*device_files_,
              DeleteUsageInfo(kUsageEntryInfoSecureStop3.usage_info_file_name,
                              kProviderSessionToken))
      .WillOnce(Return(true));

  EXPECT_CALL(
      *device_files_,
      StoreUsageInfo(kProviderSessionToken, kKeyRequest, kKeyResponse,
                     kUsageEntryInfoSecureStop3.usage_info_file_name,
                     kUsageEntryInfoSecureStop3.key_set_id, kAnotherUsageEntry,
                     usage_entry_number_to_be_deleted))
      .WillOnce(Return(true));

  EXPECT_CALL(
      *device_files_,
      StoreUsageTableInfo(
          kAnotherUsageTableHeader,
          UnorderedElementsAre(
              kUsageEntryInfoOfflineLicense1, kUsageEntryInfoStorageTypeUnknown,
              kUsageEntryInfoSecureStop3, kUsageEntryInfoSecureStop2)))
      .WillOnce(Return(true));

  EXPECT_CALL(
      *device_files_,
      StoreUsageTableInfo(
          kAnotherUsageTableHeader,
          UnorderedElementsAre(
              kUsageEntryInfoOfflineLicense1, kUsageEntryInfoStorageTypeUnknown,
              kUsageEntryInfoSecureStop3, kUsageEntryInfoSecureStop2,
              kUsageEntryInfoSecureStop3, kUsageEntryInfoStorageTypeUnknown,
              kUsageEntryInfoStorageTypeUnknown)))
      .WillOnce(Return(true));

  EXPECT_EQ(NO_ERROR,
            usage_table_header_->DeleteEntry(usage_entry_number_to_be_deleted,
                                             device_files_, &metrics));
}

// If the crypto session says the usage table header is stale, init should fail.
TEST_F(UsageTableHeaderTest, StaleHeader) {
  std::vector<CdmUsageEntryInfo> usage_entry_info_vector;
  const CdmUsageEntryInfo usage_entry_info_array[] = {
      kUsageEntryInfoOfflineLicense1, kUsageEntryInfoSecureStop1,
      kUsageEntryInfoStorageTypeUnknown, kUsageEntryInfoOfflineLicense2};
  ToVector(usage_entry_info_vector, usage_entry_info_array,
           sizeof(usage_entry_info_array));

  EXPECT_CALL(*device_files_, RetrieveUsageTableInfo(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgPointee<0>(kUsageTableHeader),
                      SetArgPointee<1>(usage_entry_info_vector),
                      Return(true)));
  EXPECT_CALL(*crypto_session_, LoadUsageTableHeader(kUsageTableHeader))
      .WillOnce(Return(LOAD_USAGE_HEADER_GENERATION_SKEW));
  EXPECT_CALL(*crypto_session_, CreateUsageTableHeader(NotNull()))
      .WillOnce(
          DoAll(SetArgPointee<0>(kEmptyUsageTableHeader), Return(NO_ERROR)));
  EXPECT_CALL(*device_files_, DeleteAllLicenses()).WillOnce(Return(true));
  EXPECT_CALL(*device_files_, DeleteAllUsageInfo()).WillOnce(Return(true));
  EXPECT_CALL(*device_files_, DeleteUsageTableInfo()).WillOnce(Return(true));
  EXPECT_CALL(*device_files_, StoreUsageTableInfo(kEmptyUsageTableHeader,
                                                  kEmptyUsageEntryInfoVector))
      .WillOnce(Return(true));

  EXPECT_TRUE(usage_table_header_->Init(kSecurityLevelL1, crypto_session_));
}

}  // namespace wvcdm
