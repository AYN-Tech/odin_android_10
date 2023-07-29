// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
#ifndef WVCDM_CORE_DEVICE_FILES_H_
#define WVCDM_CORE_DEVICE_FILES_H_

#include <set>
#include <string>
#include <vector>

#include "device_files.pb.h"
#include "disallow_copy_and_assign.h"
#include "platform.h"
#include "wv_cdm_types.h"

#if defined(UNIT_TEST)
#include <gtest/gtest_prod.h>
#endif

namespace wvcdm {

class FileSystem;

class DeviceFiles {
 public:
  typedef enum {
    kLicenseStateActive,
    kLicenseStateReleasing,
    kLicenseStateUnknown,
  } LicenseState;

  // All error response codes start with 5000 to avoid overlap with other error
  // spaces.
  enum ResponseType {
    kNoError = NO_ERROR,
    kResponseTypeBase = 5000,
    kObjectNotInitialized = kResponseTypeBase + 1,
    kParameterNull = kResponseTypeBase + 2,
    kBasePathUnavailable = kResponseTypeBase + 3,
    kFileNotFound = kResponseTypeBase + 4,
    kFileOpenFailed = kResponseTypeBase + 5,
    kFileWriteError = kResponseTypeBase + 6,
    kFileReadError = kResponseTypeBase + 7,
    kInvalidFileSize = kResponseTypeBase + 8,
    kHashComputationFailed = kResponseTypeBase + 9,
    kFileHashMismatch = kResponseTypeBase + 10,
    kFileParseError1 = kResponseTypeBase + 11,
    kFileParseError2 = kResponseTypeBase + 12,
    kUnknownLicenseState = kResponseTypeBase + 13,
    kIncorrectFileType = kResponseTypeBase + 14,
    kIncorrectFileVersion = kResponseTypeBase + 15,
    kLicenseNotPresent = kResponseTypeBase + 16,
  };

  struct CdmUsageData {
    std::string provider_session_token;
    CdmKeyMessage license_request;
    CdmKeyResponse license;
    std::string key_set_id;
    CdmUsageEntry usage_entry;
    uint32_t usage_entry_number;
  };

  DeviceFiles(FileSystem*);
  virtual ~DeviceFiles();

  virtual bool Init(CdmSecurityLevel security_level);
  virtual bool Reset(CdmSecurityLevel security_level) {
    return Init(security_level);
  }

  virtual bool StoreCertificate(const std::string& certificate,
                                const std::string& wrapped_private_key);
  virtual bool RetrieveCertificate(std::string* certificate,
                                   std::string* wrapped_private_key,
                                   std::string* serial_number,
                                   uint32_t* system_id);
  virtual bool HasCertificate();
  virtual bool RemoveCertificate();

  virtual bool StoreLicense(const std::string& key_set_id,
                            const LicenseState state,
                            const CdmInitData& pssh_data,
                            const CdmKeyMessage& key_request,
                            const CdmKeyResponse& key_response,
                            const CdmKeyMessage& key_renewal_request,
                            const CdmKeyResponse& key_renewal_response,
                            const std::string& release_server_url,
                            int64_t playback_start_time,
                            int64_t last_playback_time,
                            int64_t grace_period_end_time,
                            const CdmAppParameterMap& app_parameters,
                            const CdmUsageEntry& usage_entry,
                            uint32_t usage_entry_number,
                            ResponseType* result);

  virtual bool RetrieveLicense(
      const std::string& key_set_id, LicenseState* state,
      CdmInitData* pssh_data, CdmKeyMessage* key_request,
      CdmKeyResponse* key_response, CdmKeyMessage* key_renewal_request,
      CdmKeyResponse* key_renewal_response, std::string* release_server_url,
      int64_t* playback_start_time, int64_t* last_playback_time,
      int64_t* grace_period_end_time, CdmAppParameterMap* app_parameters,
      CdmUsageEntry* usage_entry, uint32_t* usage_entry_number,
      ResponseType* result);
  virtual bool DeleteLicense(const std::string& key_set_id);
  virtual bool ListLicenses(std::vector<std::string>* key_set_ids);
  virtual bool DeleteAllFiles();
  virtual bool DeleteAllLicenses();
  virtual bool LicenseExists(const std::string& key_set_id);
  virtual bool ReserveLicenseId(const std::string& key_set_id);
  virtual bool UnreserveLicenseId(const std::string& key_set_id);

  // Use this method to create a |usage_info_file_name| from an |app_id|
  static std::string GetUsageInfoFileName(const std::string& app_id);

  // The UsageInfo methods have been revised to use |usage_info_file_name|
  // rather than |app_id| as a parameter. Use the helper method above to
  // translate.
  // OEMCrypto API 13 introduced big usage tables which required
  // migration from usage tables stored by the TEE to usage table
  // header+usage entries stored in unsecured persistent storage. The upgrade
  // required creation of reverse lookup tables (CdmUsageEntryInfo).
  // |app_id| however was hashed and unextractable, and necessitated the
  // switch to |usage_info_file_name|
  virtual bool StoreUsageInfo(const std::string& provider_session_token,
                              const CdmKeyMessage& key_request,
                              const CdmKeyResponse& key_response,
                              const std::string& usage_info_file_name,
                              const std::string& key_set_id,
                              const CdmUsageEntry& usage_entry,
                              uint32_t usage_entry_number);

  // Retrieve usage identifying information stored on the file system.
  // The caller needs to specify at least one of |ksids| or
  // |provider_session_tokens|
  virtual bool ListUsageIds(
      const std::string& app_id,
      std::vector<std::string>* ksids,
      std::vector<std::string>* provider_session_tokens);

  // Get the provider session token for the given key_set_id.
  virtual bool GetProviderSessionToken(const std::string& app_id,
                                       const std::string& key_set_id,
                                       std::string* provider_session_token);

  virtual bool DeleteUsageInfo(const std::string& usage_info_file_name,
                               const std::string& provider_session_token);

  // Delete usage information from the file system.  Puts a list of all the
  // psts that were deleted from the file into |provider_session_tokens|.
  virtual bool DeleteAllUsageInfoForApp(
      const std::string& usage_info_file_name,
      std::vector<std::string>* provider_session_tokens);

  virtual bool DeleteAllUsageInfo();

  // Retrieve one usage info from the file.  Subsequent calls will retrieve
  // subsequent entries in the table for this app_id.
  virtual bool RetrieveUsageInfo(
      const std::string& usage_info_file_name,
      std::vector<std::pair<CdmKeyMessage, CdmKeyResponse> >* usage_info);

  // Retrieve the usage info entry specified by |provider_session_token|.
  // Returns false if the entry could not be found.
  virtual bool RetrieveUsageInfo(const std::string& usage_info_file_name,
                                 const std::string& provider_session_token,
                                 CdmKeyMessage* license_request,
                                 CdmKeyResponse* license_response,
                                 CdmUsageEntry* usage_entry,
                                 uint32_t* usage_entry_number);
  // Retrieve the usage info entry specified by |key_set_id|.
  // Returns false if the entry could not be found.
  virtual bool RetrieveUsageInfoByKeySetId(
      const std::string& usage_info_file_name,
      const std::string& key_set_id,
      std::string* provider_session_token,
      CdmKeyMessage* license_request,
      CdmKeyResponse* license_response,
      CdmUsageEntry* usage_entry,
      uint32_t* usage_entry_number);

  // These APIs support upgrading from usage tables to usage tabler header +
  // entries introduced in OEMCrypto V13.

  virtual bool ListUsageInfoFiles(std::vector<std::string>* usage_file_names);
  virtual bool RetrieveUsageInfo(const std::string& usage_info_file_name,
                                 std::vector<CdmUsageData>* usage_data);
  virtual bool RetrieveUsageInfo(const std::string& usage_info_file_name,
                                 const std::string& provider_session_token,
                                 CdmUsageData* usage_data);
  // This method overwrites rather than appends data to the usage file
  virtual bool StoreUsageInfo(const std::string& usage_info_file_name,
                              const std::vector<CdmUsageData>& usage_data);
  virtual bool UpdateUsageInfo(const std::string& usage_info_file_name,
                               const std::string& provider_session_token,
                               const CdmUsageData& usage_data);

  virtual bool StoreHlsAttributes(const std::string& key_set_id,
                                  const CdmHlsMethod method,
                                  const std::vector<uint8_t>& media_segment_iv);
  virtual bool RetrieveHlsAttributes(const std::string& key_set_id,
                                     CdmHlsMethod* method,
                                     std::vector<uint8_t>* media_segment_iv);
  virtual bool DeleteHlsAttributes(const std::string& key_set_id);

  virtual bool StoreUsageTableInfo(
      const CdmUsageTableHeader& usage_table_header,
      const std::vector<CdmUsageEntryInfo>& usage_entry_info);

  virtual bool RetrieveUsageTableInfo(
      CdmUsageTableHeader* usage_table_header,
      std::vector<CdmUsageEntryInfo>* usage_entry_info);

  virtual bool DeleteUsageTableInfo();

 private:
  // Extract serial number and system ID from DRM Device certificate
  bool ExtractDeviceInfo(const std::string& device_certificate,
                         std::string* serial_number,
                         uint32_t* system_id);

  // Helpers that wrap the File interface and automatically handle hashing, as
  // well as adding the device files base path to to the file name.
  ResponseType StoreFileWithHash(const std::string& name,
                                 const std::string& serialized_file);
  ResponseType StoreFileRaw(const std::string& name,
                            const std::string& serialized_file);
  ResponseType RetrieveHashedFile(const std::string& name,
                                  video_widevine_client::sdk::File* file);
  bool FileExists(const std::string& name);
  bool ListFiles(std::vector<std::string>* names);
  bool RemoveFile(const std::string& name);
  ssize_t GetFileSize(const std::string& name);

  static std::string GetCertificateFileName();
  static std::string GetHlsAttributesFileNameExtension();
  static std::string GetLicenseFileNameExtension();
  static std::string GetUsageTableFileName();
  static std::string GetFileNameSafeHash(const std::string& input);

#if defined(UNIT_TEST)
  FRIEND_TEST(DeviceFilesSecurityLevelTest, SecurityLevel);
  FRIEND_TEST(DeviceCertificateStoreTest, StoreCertificate);
  FRIEND_TEST(DeviceCertificateTest, DISABLED_ReadCertificate);
  FRIEND_TEST(DeviceCertificateTest, HasCertificate);
  FRIEND_TEST(DeviceFilesStoreTest, StoreLicense);
  FRIEND_TEST(DeviceFilesHlsAttributesTest, Delete);
  FRIEND_TEST(DeviceFilesHlsAttributesTest, Read);
  FRIEND_TEST(DeviceFilesHlsAttributesTest, Store);
  FRIEND_TEST(DeviceFilesTest, DeleteLicense);
  FRIEND_TEST(DeviceFilesTest, ReserveLicenseIdsDoesNotUseFileSystem);
  FRIEND_TEST(DeviceFilesTest, RetrieveLicenses);
  FRIEND_TEST(DeviceFilesTest, AppParametersBackwardCompatibility);
  FRIEND_TEST(DeviceFilesTest, StoreLicenses);
  FRIEND_TEST(DeviceFilesTest, UpdateLicenseState);
  FRIEND_TEST(DeviceFilesUsageInfoTest, Delete);
  FRIEND_TEST(DeviceFilesUsageInfoTest, DeleteAll);
  FRIEND_TEST(DeviceFilesUsageInfoTest, Read);
  FRIEND_TEST(DeviceFilesUsageInfoTest, Store);
  FRIEND_TEST(DeviceFilesUsageTableTest, Read);
  FRIEND_TEST(DeviceFilesUsageTableTest, Store);
  FRIEND_TEST(WvCdmRequestLicenseTest, UnprovisionTest);
  FRIEND_TEST(WvCdmRequestLicenseTest, ForceL3Test);
  FRIEND_TEST(WvCdmRequestLicenseTest, UsageInfoRetryTest);
  FRIEND_TEST(WvCdmRequestLicenseTest, UsageReleaseAllTest);
  FRIEND_TEST(WvCdmUsageInfoTest, UsageInfo);
  FRIEND_TEST(WvCdmUsageTest, WithClientId);
  FRIEND_TEST(WvCdmExtendedDurationTest, UsageOverflowTest);
#endif

  static std::set<std::string> reserved_license_ids_;

  FileSystem* file_system_;
  CdmSecurityLevel security_level_;
  bool initialized_;

  CORE_DISALLOW_COPY_AND_ASSIGN(DeviceFiles);
};

}  // namespace wvcdm

#endif  // WVCDM_CORE_DEVICE_FILES_H_
