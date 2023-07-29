// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#include "device_files.h"

#include <string.h>
#include <string>

#include "file_store.h"
#include "license_protocol.pb.h"
#include "log.h"
#include "privacy_crypto.h"
#include "properties.h"
#include "string_conversions.h"
#include "wv_cdm_constants.h"

// Protobuf generated classes.
using video_widevine_client::sdk::DeviceCertificate;
using video_widevine_client::sdk::HashedFile;
using video_widevine_client::sdk::HlsAttributes;
using video_widevine_client::sdk::HlsAttributes_Method_AES_128;
using video_widevine_client::sdk::HlsAttributes_Method_SAMPLE_AES;
using video_widevine_client::sdk::License;
using video_widevine_client::sdk::License_LicenseState_ACTIVE;
using video_widevine_client::sdk::License_LicenseState_RELEASING;
using video_widevine_client::sdk::NameValue;
using video_widevine_client::sdk::UsageInfo;
using video_widevine_client::sdk::UsageInfo_ProviderSession;
using video_widevine_client::sdk::UsageTableInfo;
using video_widevine_client::sdk::UsageTableInfo_UsageEntryInfo;
using video_widevine_client::sdk::
    UsageTableInfo_UsageEntryInfo_UsageEntryStorage_LICENSE;
using video_widevine_client::sdk::
    UsageTableInfo_UsageEntryInfo_UsageEntryStorage_USAGE_INFO;
using video_widevine_client::sdk::
    UsageTableInfo_UsageEntryInfo_UsageEntryStorage_UNKNOWN;

using video_widevine::SignedDrmDeviceCertificate;
using video_widevine::DrmDeviceCertificate;

#define RETURN_FALSE_IF_NULL(PARAM) \
if (PARAM == nullptr) { \
  LOGE("|PARAM| not provided"); \
  *result = kParameterNull; \
  return false; \
}

namespace {

const char kCertificateFileName[] = "cert.bin";
const char kHlsAttributesFileNameExt[] = ".hal";
const char kUsageInfoFileNamePrefix[] = "usage";
const char kUsageInfoFileNameExt[] = ".bin";
const char kLicenseFileNameExt[] = ".lic";
const char kEmptyFileName[] = "";
const char kUsageTableFileName[] = "usgtable.bin";
const char kWildcard[] = "*";

}  // namespace

namespace wvcdm {

// static
std::set<std::string> DeviceFiles::reserved_license_ids_;

DeviceFiles::DeviceFiles(FileSystem* file_system)
    : file_system_(file_system),
      security_level_(kSecurityLevelUninitialized),
      initialized_(false) {}

DeviceFiles::~DeviceFiles() {}

bool DeviceFiles::Init(CdmSecurityLevel security_level) {
  if (!file_system_) {
    LOGD("DeviceFiles::Init: Invalid FileSystem given.");
    return false;
  }

  std::string path;
  if (!Properties::GetDeviceFilesBasePath(security_level, &path)) {
    LOGW("DeviceFiles::Init: Unsupported security level %d", security_level);
    return false;
  }
  security_level_ = security_level;
  initialized_ = true;
  return true;
}

bool DeviceFiles::StoreCertificate(const std::string& certificate,
                                   const std::string& wrapped_private_key) {
  if (!initialized_) {
    LOGW("DeviceFiles::StoreCertificate: not initialized");
    return false;
  }

  // Fill in file information
  video_widevine_client::sdk::File file;

  file.set_type(video_widevine_client::sdk::File::DEVICE_CERTIFICATE);
  file.set_version(video_widevine_client::sdk::File::VERSION_1);

  DeviceCertificate* device_certificate = file.mutable_device_certificate();
  device_certificate->set_certificate(certificate);
  device_certificate->set_wrapped_private_key(wrapped_private_key);

  std::string serialized_file;
  file.SerializeToString(&serialized_file);

  return
      StoreFileWithHash(GetCertificateFileName(), serialized_file) == kNoError;
}

bool DeviceFiles::RetrieveCertificate(std::string* certificate,
                                      std::string* wrapped_private_key,
                                      std::string* serial_number,
                                      uint32_t* system_id) {
  if (!initialized_) {
    LOGW("DeviceFiles::RetrieveCertificate: not initialized");
    return false;
  }

  video_widevine_client::sdk::File file;
  if (RetrieveHashedFile(GetCertificateFileName(), &file) != kNoError) {
    LOGW("DeviceFiles::RetrieveCertificate: unable to retrieve file");
    return false;
  }

  if (file.type() != video_widevine_client::sdk::File::DEVICE_CERTIFICATE) {
    LOGW("DeviceFiles::RetrieveCertificate: Incorrect file type");
    return false;
  }

  if (file.version() != video_widevine_client::sdk::File::VERSION_1) {
    LOGW("DeviceFiles::RetrieveCertificate: Incorrect file version");
    return false;
  }

  if (!file.has_device_certificate()) {
    LOGW("DeviceFiles::RetrieveCertificate: Certificate not present");
    return false;
  }

  DeviceCertificate device_certificate = file.device_certificate();
  *certificate = device_certificate.certificate();
  *wrapped_private_key = device_certificate.wrapped_private_key();
  return ExtractDeviceInfo(device_certificate.certificate(), serial_number,
                           system_id);
}

bool DeviceFiles::ExtractDeviceInfo(const std::string& device_certificate,
                                    std::string* serial_number,
                                    uint32_t* system_id) {
  LOGI("ExtractDeviceInfo Entry");
  if (!serial_number && !system_id) {
    LOGE("DeviceFiles::ExtractDeviceInfo: invalid parameter.");
    return false;
  }

  // Get serial number and system ID from certificate
  SignedDrmDeviceCertificate signed_drm_device_certificate;
  if (!signed_drm_device_certificate.ParseFromString(device_certificate) ||
      !signed_drm_device_certificate.has_drm_certificate()) {
    LOGE("DeviceFiles::ExtractDeviceInfo: fails parsing signed drm device "
         "certificate.");
    return false;
  }
  DrmDeviceCertificate drm_device_certificate;
  if (!drm_device_certificate.ParseFromString(
          signed_drm_device_certificate.drm_certificate()) ||
      (drm_device_certificate.type() !=
       video_widevine::DrmDeviceCertificate::DRM_USER_DEVICE)) {
    LOGE("DeviceFiles::ExtractDeviceInfo: fails parsing drm device "
         "certificate message.");
    return false;
  }
  if (serial_number != NULL) {
    *serial_number = drm_device_certificate.serial_number();
  }
  if (system_id != NULL) {
    *system_id = drm_device_certificate.system_id();
  }
  return true;
}

bool DeviceFiles::HasCertificate() {
  if (!initialized_) {
    LOGW("DeviceFiles::HasCertificate: not initialized");
    return false;
  }

  return FileExists(GetCertificateFileName());
}

bool DeviceFiles::RemoveCertificate() {
  if (!initialized_) {
    LOGW("DeviceFiles::RemoveCertificate: not initialized");
    return false;
  }

  return RemoveFile(GetCertificateFileName());
}

bool DeviceFiles::StoreLicense(
    const std::string& key_set_id, const LicenseState state,
    const CdmInitData& pssh_data, const CdmKeyMessage& license_request,
    const CdmKeyResponse& license_message,
    const CdmKeyMessage& license_renewal_request,
    const CdmKeyResponse& license_renewal,
    const std::string& release_server_url, int64_t playback_start_time,
    int64_t last_playback_time, int64_t grace_period_end_time,
    const CdmAppParameterMap& app_parameters,
    const CdmUsageEntry& usage_entry,
    const uint32_t usage_entry_number,
    ResponseType* result) {
  if (result == nullptr) {
    LOGE("DeviceFiles::StoreLicense: |result| not provided");
    return false;
  }

  *result = kNoError;

  if (!initialized_) {
    LOGW("DeviceFiles::StoreLicense: not initialized");
    *result = kObjectNotInitialized;
    return false;
  }

  // Fill in file information
  video_widevine_client::sdk::File file;

  file.set_type(video_widevine_client::sdk::File::LICENSE);
  file.set_version(video_widevine_client::sdk::File::VERSION_1);

  License* license = file.mutable_license();
  switch (state) {
    case kLicenseStateActive:
      license->set_state(License_LicenseState_ACTIVE);
      break;
    case kLicenseStateReleasing:
      license->set_state(License_LicenseState_RELEASING);
      break;
    default:
      LOGW("DeviceFiles::StoreLicense: Unknown license state: %u", state);
      *result = kUnknownLicenseState;
      return false;
      break;
  }
  license->set_pssh_data(pssh_data);
  license->set_license_request(license_request);
  license->set_license(license_message);
  license->set_renewal_request(license_renewal_request);
  license->set_renewal(license_renewal);
  license->set_release_server_url(release_server_url);
  license->set_playback_start_time(playback_start_time);
  license->set_last_playback_time(last_playback_time);
  license->set_grace_period_end_time(grace_period_end_time);
  NameValue* app_params;
  for (CdmAppParameterMap::const_iterator iter = app_parameters.begin();
       iter != app_parameters.end(); ++iter) {
    app_params = license->add_app_parameters();
    app_params->set_name(iter->first);
    app_params->set_value(iter->second);
  }
  license->set_usage_entry(usage_entry);
  license->set_usage_entry_number(usage_entry_number);

  std::string serialized_file;
  file.SerializeToString(&serialized_file);

  reserved_license_ids_.erase(key_set_id);
  *result =
      StoreFileWithHash(key_set_id + kLicenseFileNameExt, serialized_file);
  return *result == kNoError;
}

bool DeviceFiles::RetrieveLicense(
    const std::string& key_set_id, LicenseState* state, CdmInitData* pssh_data,
    CdmKeyMessage* license_request, CdmKeyResponse* license_message,
    CdmKeyMessage* license_renewal_request, CdmKeyResponse* license_renewal,
    std::string* release_server_url, int64_t* playback_start_time,
    int64_t* last_playback_time, int64_t* grace_period_end_time,
    CdmAppParameterMap* app_parameters, CdmUsageEntry* usage_entry,
    uint32_t* usage_entry_number, ResponseType* result) {
  if (result == nullptr) {
    LOGE("DeviceFiles::RetrieveLicense: |result| not provided");
    return false;
  }

  if (!initialized_) {
    LOGW("DeviceFiles::RetrieveLicense: not initialized");
    *result = kObjectNotInitialized;
    return false;
  }

  RETURN_FALSE_IF_NULL(state);
  RETURN_FALSE_IF_NULL(pssh_data);
  RETURN_FALSE_IF_NULL(license_request);
  RETURN_FALSE_IF_NULL(license_message);
  RETURN_FALSE_IF_NULL(license_renewal_request);
  RETURN_FALSE_IF_NULL(license_renewal);
  RETURN_FALSE_IF_NULL(release_server_url);
  RETURN_FALSE_IF_NULL(playback_start_time);
  RETURN_FALSE_IF_NULL(last_playback_time);
  RETURN_FALSE_IF_NULL(grace_period_end_time);
  RETURN_FALSE_IF_NULL(app_parameters);
  RETURN_FALSE_IF_NULL(usage_entry);
  RETURN_FALSE_IF_NULL(usage_entry_number);

  video_widevine_client::sdk::File file;
  *result = RetrieveHashedFile(key_set_id + kLicenseFileNameExt, &file);
  if (*result != kNoError) {
    LOGW("DeviceFiles::RetrieveLicense: unable to retrieve file: %d", *result);
    return false;
  }

  if (file.type() != video_widevine_client::sdk::File::LICENSE) {
    LOGW("DeviceFiles::RetrieveLicense: Incorrect file type");
    *result = kIncorrectFileType;
    return false;
  }

  if (file.version() != video_widevine_client::sdk::File::VERSION_1) {
    LOGW("DeviceFiles::RetrieveLicense: Incorrect file version");
    *result = kIncorrectFileVersion;
    return false;
  }

  if (!file.has_license()) {
    LOGW("DeviceFiles::RetrieveLicense: License not present");
    *result = kLicenseNotPresent;
    return false;
  }

  License license = file.license();

  switch (license.state()) {
    case License_LicenseState_ACTIVE:
      *state = kLicenseStateActive;
      break;
    case License_LicenseState_RELEASING:
      *state = kLicenseStateReleasing;
      break;
    default:
      LOGW("DeviceFiles::RetrieveLicense: Unrecognized license state: %u",
           kLicenseStateUnknown);
      *state = kLicenseStateUnknown;
      break;
  }
  *pssh_data = license.pssh_data();
  *license_request = license.license_request();
  *license_message = license.license();
  *license_renewal_request = license.renewal_request();
  *license_renewal = license.renewal();
  *release_server_url = license.release_server_url();
  *playback_start_time = license.playback_start_time();
  *last_playback_time = license.last_playback_time();
  *grace_period_end_time = license.grace_period_end_time();
  for (int i = 0; i < license.app_parameters_size(); ++i) {
    (*app_parameters)[license.app_parameters(i).name()] =
        license.app_parameters(i).value();
  }
  *usage_entry = license.usage_entry();
  *usage_entry_number = license.usage_entry_number();
  return true;
}

bool DeviceFiles::DeleteLicense(const std::string& key_set_id) {
  if (!initialized_) {
    LOGW("DeviceFiles::DeleteLicense: not initialized");
    return false;
  }
  return RemoveFile(key_set_id + kLicenseFileNameExt);
}

bool DeviceFiles::ListLicenses(std::vector<std::string>* key_set_ids) {
  if (!initialized_) {
    LOGW("DeviceFiles::ListLicenses: not initialized");
    return false;
  }

  if (key_set_ids == NULL) {
    LOGW("DeviceFiles::ListLicenses: key_set_ids parameter not provided");
    return false;
  }

  // Get list of filenames
  std::vector<std::string> filenames;
  if (!ListFiles(&filenames)) {
    return false;
  }

  // Scan list of returned filenames, remove extension, and return
  // as a list of key_set_ids.
  key_set_ids->clear();
  for (size_t i = 0; i < filenames.size(); i++) {
    std::string* name = &filenames[i];
    std::size_t pos = name->find(kLicenseFileNameExt);
    if (pos == std::string::npos) {
      // Skip this file - extension does not match
      continue;
    }
    // Store filename (minus extension).  This should be a key set ID.
    key_set_ids->push_back(name->substr(0, pos));
  }
  return true;
}

bool DeviceFiles::DeleteAllLicenses() {
  if (!initialized_) {
    LOGW("DeviceFiles::DeleteAllLicenses: not initialized");
    return false;
  }
  return RemoveFile(std::string(kWildcard) + kLicenseFileNameExt);
}

bool DeviceFiles::DeleteAllFiles() {
  if (!initialized_) {
    LOGW("DeviceFiles::DeleteAllFiles: not initialized");
    return false;
  }

  // We pass an empty string to RemoveFile to delete the device files base
  // directory itself.
  return RemoveFile(kEmptyFileName);
}

bool DeviceFiles::LicenseExists(const std::string& key_set_id) {
  if (!initialized_) {
    LOGW("DeviceFiles::LicenseExists: not initialized");
    return false;
  }
  return reserved_license_ids_.count(key_set_id) ||
         FileExists(key_set_id + kLicenseFileNameExt);
}

bool DeviceFiles::ReserveLicenseId(const std::string& key_set_id) {
  if (!initialized_) {
    LOGW("DeviceFiles::ReserveLicenseId: not initialized");
    return false;
  }
  reserved_license_ids_.insert(key_set_id);
  return true;
}

bool DeviceFiles::UnreserveLicenseId(const std::string& key_set_id) {
  if (!initialized_) {
    LOGW("DeviceFiles::UnreserveLicenseId: not initialized");
    return false;
  }
  reserved_license_ids_.erase(key_set_id);
  return true;
}

bool DeviceFiles::StoreUsageInfo(const std::string& provider_session_token,
                                 const CdmKeyMessage& key_request,
                                 const CdmKeyResponse& key_response,
                                 const std::string& usage_info_file_name,
                                 const std::string& key_set_id,
                                 const std::string& usage_entry,
                                 uint32_t usage_entry_number) {
  if (!initialized_) {
    LOGW("DeviceFiles::StoreUsageInfo: not initialized");
    return false;
  }

  video_widevine_client::sdk::File file;
  if (!FileExists(usage_info_file_name)) {
    file.set_type(video_widevine_client::sdk::File::USAGE_INFO);
    file.set_version(video_widevine_client::sdk::File::VERSION_1);
  } else {
    if (RetrieveHashedFile(usage_info_file_name, &file) != kNoError) {
      LOGW("DeviceFiles::StoreUsageInfo: Unable to retrieve file");
      return false;
    }
  }

  UsageInfo* usage_info = file.mutable_usage_info();
  UsageInfo_ProviderSession* provider_session = usage_info->add_sessions();

  provider_session->set_token(provider_session_token.data(),
                              provider_session_token.size());
  provider_session->set_license_request(key_request.data(), key_request.size());
  provider_session->set_license(key_response.data(), key_response.size());
  provider_session->set_key_set_id(key_set_id.data(), key_set_id.size());
  provider_session->set_usage_entry(usage_entry);
  provider_session->set_usage_entry_number(usage_entry_number);

  std::string serialized_file;
  file.SerializeToString(&serialized_file);
  return StoreFileWithHash(usage_info_file_name, serialized_file) == kNoError;
}

bool DeviceFiles::ListUsageIds(
    const std::string& app_id,
    std::vector<std::string>* ksids,
    std::vector<std::string>* provider_session_tokens) {
  if (!initialized_) {
    LOGW("DeviceFiles::ListUsageIds: not initialized");
    return false;
  }

  if (ksids == NULL && provider_session_tokens == NULL) {
    LOGW("DeviceFiles::ListUsageIds: ksids or pst parameter not provided");
    return false;
  }

  // Empty or non-existent file == no usage records.
  std::string file_name = GetUsageInfoFileName(app_id);
  if (!FileExists(file_name) || GetFileSize(file_name) == 0) {
    if (ksids != NULL) ksids->clear();
    if (provider_session_tokens != NULL) provider_session_tokens->clear();
    return true;
  }

  video_widevine_client::sdk::File file;
  if (RetrieveHashedFile(file_name, &file) != kNoError) {
    LOGW("DeviceFiles::ListUsageRecords: Unable to retrieve file");
    return false;
  }

  if (ksids != NULL) ksids->clear();
  if (provider_session_tokens != NULL) provider_session_tokens->clear();

  size_t num_records = file.usage_info().sessions_size();
  for (size_t i = 0; i < num_records; ++i) {
    if ((ksids != NULL) &&
        !file.usage_info().sessions(i).key_set_id().empty()) {
      ksids->push_back(file.usage_info().sessions(i).key_set_id());
    }
    if ((provider_session_tokens != NULL) &&
        !file.usage_info().sessions(i).token().empty()) {
      provider_session_tokens->push_back(file.usage_info().sessions(i).token());
    }
  }
  return true;
}

bool DeviceFiles::GetProviderSessionToken(const std::string& app_id,
                                          const std::string& key_set_id,
                                          std::string* provider_session_token) {
  if (!initialized_) {
    LOGW("DeviceFiles::GetProviderSessionToken: not initialized");
    return false;
  }

  if (provider_session_token == NULL) {
    LOGW("DeviceFiles::GetProviderSessionToken: NULL return argument pointer");
    return false;
  }

  std::string file_name = GetUsageInfoFileName(app_id);
  if (!FileExists(file_name) || GetFileSize(file_name) == 0) {
    LOGW("DeviceFiles::GetProviderSessionToken: empty file");
    return false;
  }

  video_widevine_client::sdk::File file;
  if (RetrieveHashedFile(file_name, &file) != kNoError) {
    LOGW("DeviceFiles::GetProviderSessionToken: unable to retrieve file");
    return false;
  }

  size_t num_records = file.usage_info().sessions_size();
  for (size_t i = 0; i < num_records; ++i) {
    if (file.usage_info().sessions(i).key_set_id() == key_set_id) {
      *provider_session_token = file.usage_info().sessions(i).token();
      return true;
    }
  }
  return false;
}

bool DeviceFiles::DeleteUsageInfo(const std::string& usage_info_file_name,
                                  const std::string& provider_session_token) {
  if (!initialized_) {
    LOGW("DeviceFiles::DeleteUsageInfo: not initialized");
    return false;
  }
  video_widevine_client::sdk::File file;
  if (RetrieveHashedFile(usage_info_file_name, &file) != kNoError) {
    LOGW("DeviceFiles::DeleteUsageInfo: Unable to retrieve file");
    return false;
  }

  UsageInfo* usage_info = file.mutable_usage_info();
  int index = 0;
  bool found = false;
  for (; index < usage_info->sessions_size(); ++index) {
    if (usage_info->sessions(index).token() == provider_session_token) {
      found = true;
      break;
    }
  }

  if (!found) {
    LOGW(
        "DeviceFiles::DeleteUsageInfo: Unable to find provider session "
        "token: %s",
        b2a_hex(provider_session_token).c_str());
    return false;
  }

  google::protobuf::RepeatedPtrField<UsageInfo_ProviderSession>* sessions =
      usage_info->mutable_sessions();
  if (index < usage_info->sessions_size() - 1) {
    sessions->SwapElements(index, usage_info->sessions_size() - 1);
  }
  sessions->RemoveLast();

  std::string serialized_file;
  file.SerializeToString(&serialized_file);
  return StoreFileWithHash(usage_info_file_name, serialized_file) == kNoError;
}

bool DeviceFiles::DeleteAllUsageInfoForApp(
    const std::string& usage_info_file_name,
    std::vector<std::string>* provider_session_tokens) {
  if (!initialized_) {
    LOGW("DeviceFiles::DeleteAllUsageInfoForApp: not initialized");
    return false;
  }
  if (NULL == provider_session_tokens) {
    LOGW("DeviceFiles::DeleteAllUsageInfoForApp: pst destination not provided");
    return false;
  }
  provider_session_tokens->clear();

  if (!FileExists(usage_info_file_name)) return true;

  video_widevine_client::sdk::File file;
  if (RetrieveHashedFile(usage_info_file_name, &file) == kNoError) {
    for (int i = 0; i < file.usage_info().sessions_size(); ++i) {
      provider_session_tokens->push_back(file.usage_info().sessions(i).token());
    }
  } else {
    LOGW("DeviceFiles::DeleteAllUsageInfoForApp: Unable to retrieve file");
  }
  return RemoveFile(usage_info_file_name);
}

bool DeviceFiles::DeleteAllUsageInfo() {
  if (!initialized_) {
    LOGW("DeviceFiles::DeleteAllUsageInfo: not initialized");
    return false;
  }
  return RemoveFile(kUsageInfoFileNamePrefix + std::string(kWildcard) +
                    kUsageInfoFileNameExt);
}

bool DeviceFiles::RetrieveUsageInfo(
    const std::string& usage_info_file_name,
    std::vector<std::pair<CdmKeyMessage, CdmKeyResponse> >* usage_info) {
  if (!initialized_) {
    LOGW("DeviceFiles::RetrieveUsageInfo: not initialized");
    return false;
  }

  if (NULL == usage_info) {
    LOGW(
        "DeviceFiles::RetrieveUsageInfo: license destination not "
        "provided");
    return false;
  }

  if (!FileExists(usage_info_file_name) ||
      GetFileSize(usage_info_file_name) == 0) {
    usage_info->resize(0);
    return true;
  }

  video_widevine_client::sdk::File file;
  if (RetrieveHashedFile(usage_info_file_name, &file) != kNoError) {
    LOGW("DeviceFiles::RetrieveUsageInfo: Unable to retrieve file");
    return false;
  }

  usage_info->resize(file.usage_info().sessions_size());
  for (int i = 0; i < file.usage_info().sessions_size(); ++i) {
    (*usage_info)[i] =
        std::make_pair(file.usage_info().sessions(i).license_request(),
                       file.usage_info().sessions(i).license());
  }

  return true;
}

bool DeviceFiles::RetrieveUsageInfo(const std::string& usage_info_file_name,
                                    const std::string& provider_session_token,
                                    CdmKeyMessage* license_request,
                                    CdmKeyResponse* license_response,
                                    std::string* usage_entry,
                                    uint32_t* usage_entry_number) {
  if (!initialized_) {
    LOGW("DeviceFiles::RetrieveUsageInfo: not initialized");
    return false;
  }

  video_widevine_client::sdk::File file;
  if (RetrieveHashedFile(usage_info_file_name, &file) != kNoError) {
    LOGW("DeviceFiles::RetrieveUsageInfo: unable to retrieve file");
    return false;
  }

  int index = 0;
  for (; index < file.usage_info().sessions_size(); ++index) {
    if (file.usage_info().sessions(index).token() == provider_session_token) {
      *license_request = file.usage_info().sessions(index).license_request();
      *license_response = file.usage_info().sessions(index).license();
      *usage_entry = file.usage_info().sessions(index).usage_entry();
      *usage_entry_number =
          file.usage_info().sessions(index).usage_entry_number();
      return true;
    }
  }

  return false;
}

bool DeviceFiles::RetrieveUsageInfoByKeySetId(
    const std::string& usage_info_file_name,
    const std::string& key_set_id,
    std::string* provider_session_token,
    CdmKeyMessage* license_request,
    CdmKeyResponse* license_response,
    std::string* usage_entry,
    uint32_t* usage_entry_number) {
  if (!initialized_) {
    LOGW("DeviceFiles::RetrieveUsageInfoByKeySetId: not initialized");
    return false;
  }

  video_widevine_client::sdk::File file;
  if (RetrieveHashedFile(usage_info_file_name, &file) != kNoError) {
    LOGW("DeviceFiles::RetrieveUsageInfoByKeySetId: unable to retrieve file");
    return false;
  }

  int index = 0;
  for (; index < file.usage_info().sessions_size(); ++index) {
    if (file.usage_info().sessions(index).key_set_id() == key_set_id) {
      *provider_session_token = file.usage_info().sessions(index).token();
      *license_request = file.usage_info().sessions(index).license_request();
      *license_response = file.usage_info().sessions(index).license();
      *usage_entry = file.usage_info().sessions(index).usage_entry();
      *usage_entry_number =
          file.usage_info().sessions(index).usage_entry_number();
      return true;
    }
  }

  return false;
}

bool DeviceFiles::StoreUsageInfo(const std::string& usage_info_file_name,
                                 const std::vector<CdmUsageData>& usage_data) {
  if (!initialized_) {
    LOGW("DeviceFiles::StoreUsageInfo: not initialized");
    return false;
  }

  video_widevine_client::sdk::File file;
  file.set_type(video_widevine_client::sdk::File::USAGE_INFO);
  file.set_version(video_widevine_client::sdk::File::VERSION_1);

  UsageInfo* usage_info = file.mutable_usage_info();
  for (size_t i = 0; i < usage_data.size(); ++i) {
    UsageInfo_ProviderSession* provider_session = usage_info->add_sessions();

    provider_session->set_token(usage_data[i].provider_session_token.data(),
                                usage_data[i].provider_session_token.size());
    provider_session->set_license_request(usage_data[i].license_request.data(),
                                          usage_data[i].license_request.size());
    provider_session->set_license(usage_data[i].license.data(),
                                  usage_data[i].license.size());
    provider_session->set_key_set_id(usage_data[i].key_set_id.data(),
                                     usage_data[i].key_set_id.size());
    provider_session->set_usage_entry(usage_data[i].usage_entry);
    provider_session->set_usage_entry_number(usage_data[i].usage_entry_number);
  }

  std::string serialized_file;
  file.SerializeToString(&serialized_file);
  return StoreFileWithHash(usage_info_file_name, serialized_file) == kNoError;
}

bool DeviceFiles::UpdateUsageInfo(const std::string& usage_info_file_name,
                                  const std::string& provider_session_token,
                                  const CdmUsageData& usage_data) {
  if (!initialized_) {
    LOGW("DeviceFiles::UpdateUsageInfo: not initialized");
    return false;
  }

  video_widevine_client::sdk::File file;
  if (!FileExists(usage_info_file_name)) {
    LOGW("DeviceFiles::UpdateUsageInfo: Usage file does not exist");
    return false;
  }


  if (RetrieveHashedFile(usage_info_file_name, &file) != kNoError) {
    LOGW("DeviceFiles::UpdateUsageInfo: Unable to retrieve file");
    return false;
  }

  int index = 0;
  for (; index < file.usage_info().sessions_size(); ++index) {
    if (file.usage_info().sessions(index).token() == provider_session_token) {
      UsageInfo* usage_info = file.mutable_usage_info();
      UsageInfo_ProviderSession* provider_session =
          usage_info->mutable_sessions(index);
      provider_session->set_license_request(usage_data.license_request);
      provider_session->set_license(usage_data.license);
      provider_session->set_key_set_id(usage_data.key_set_id);
      provider_session->set_usage_entry(usage_data.usage_entry);
      provider_session->set_usage_entry_number(usage_data.usage_entry_number);

      std::string serialized_file;
      file.SerializeToString(&serialized_file);
      return
          StoreFileWithHash(usage_info_file_name, serialized_file) == kNoError;
    }
  }

  return false;
}

bool DeviceFiles::RetrieveUsageInfo(const std::string& usage_info_file_name,
                                    std::vector<CdmUsageData>* usage_data) {
  if (!initialized_) {
    LOGW("DeviceFiles::RetrieveUsageInfo: not initialized");
    return false;
  }

  if (usage_data == NULL) {
    LOGW("DeviceFiles::RetrieveUsageInfo: usage_data not provided");
    return false;
  }

  if (!FileExists(usage_info_file_name) ||
      GetFileSize(usage_info_file_name) == 0) {
    usage_data->resize(0);
    return true;
  }

  video_widevine_client::sdk::File file;
  if (RetrieveHashedFile(usage_info_file_name, &file) != kNoError) {
    LOGW("DeviceFiles::RetrieveUsageInfo: unable to retrieve file");
    return false;
  }

  usage_data->resize(file.usage_info().sessions_size());
  for (int i = 0; i < file.usage_info().sessions_size(); ++i) {
    (*usage_data)[i].provider_session_token =
        file.usage_info().sessions(i).token();
    (*usage_data)[i].license_request =
        file.usage_info().sessions(i).license_request();
    (*usage_data)[i].license = file.usage_info().sessions(i).license();
    (*usage_data)[i].key_set_id = file.usage_info().sessions(i).key_set_id();
    (*usage_data)[i].usage_entry = file.usage_info().sessions(i).usage_entry();
    (*usage_data)[i].usage_entry_number =
      file.usage_info().sessions(i).usage_entry_number();
  }

  return true;
}

bool DeviceFiles::RetrieveUsageInfo(const std::string& usage_info_file_name,
                                    const std::string& provider_session_token,
                                    CdmUsageData* usage_data) {
  if (!initialized_) {
    LOGW("DeviceFiles::RetrieveUsageInfo: not initialized");
    return false;
  }

  if (usage_data == NULL) {
    LOGW("DeviceFiles::RetrieveUsageInfo: usage_data not provided");
    return false;
  }

  video_widevine_client::sdk::File file;
  if (RetrieveHashedFile(usage_info_file_name, &file) != kNoError) {
    LOGW("DeviceFiles::RetrieveUsageInfo: unable to retrieve file");
    return false;
  }

  int index = 0;
  for (; index < file.usage_info().sessions_size(); ++index) {
    if (file.usage_info().sessions(index).token() == provider_session_token) {
      usage_data->provider_session_token =
          file.usage_info().sessions(index).token();
      usage_data->license_request =
          file.usage_info().sessions(index).license_request();
      usage_data->license = file.usage_info().sessions(index).license();
      usage_data->key_set_id = file.usage_info().sessions(index).key_set_id();
      usage_data->usage_entry = file.usage_info().sessions(index).usage_entry();
      usage_data->usage_entry_number =
          file.usage_info().sessions(index).usage_entry_number();
      return true;
    }
  }

  return false;
}

bool DeviceFiles::ListUsageInfoFiles(
    std::vector<std::string>* usage_info_file_names) {
  if (!initialized_) {
    LOGW("DeviceFiles::ListUsageInfoFiles: not initialized");
    return false;
  }

  if (usage_info_file_names == NULL) {
    LOGW("DeviceFiles::ListUsageInfoFiles: usage_info_file_names not provided");
    return false;
  }

  // Get list of filenames
  std::vector<std::string> filenames;
  if (!ListFiles(&filenames)) {
    return false;
  }

  // Scan list of all filenames and return only usage info filenames
  usage_info_file_names->clear();
  for (size_t i = 0; i < filenames.size(); i++) {
    std::string* name = &filenames[i];
    std::size_t pos_prefix = name->find(kUsageInfoFileNamePrefix);
    std::size_t pos_suffix = name->find(kUsageInfoFileNameExt);
    if (pos_prefix == std::string::npos ||
        pos_suffix == std::string::npos) {
      // Skip this file - extension does not match
      continue;
    }

    usage_info_file_names->push_back(*name);
  }
  return true;
}

bool DeviceFiles::StoreHlsAttributes(
    const std::string& key_set_id, const CdmHlsMethod method,
    const std::vector<uint8_t>& media_segment_iv) {
  if (!initialized_) {
    LOGW("DeviceFiles::StoreHlsAttributes: not initialized");
    return false;
  }

  // Fill in file information
  video_widevine_client::sdk::File file;

  file.set_type(video_widevine_client::sdk::File::HLS_ATTRIBUTES);
  file.set_version(video_widevine_client::sdk::File::VERSION_1);

  HlsAttributes* hls_attributes = file.mutable_hls_attributes();
  switch (method) {
    case kHlsMethodAes128:
      hls_attributes->set_method(HlsAttributes_Method_AES_128);
      break;
    case kHlsMethodSampleAes:
      hls_attributes->set_method(HlsAttributes_Method_SAMPLE_AES);
      break;
    case kHlsMethodNone:
    default:
      LOGW("DeviceFiles::StoreHlsAttributeInfo: Unknown HLS method: %u",
           method);
      return false;
      break;
  }
  hls_attributes->set_media_segment_iv(&media_segment_iv[0],
                                       media_segment_iv.size());

  std::string serialized_file;
  file.SerializeToString(&serialized_file);

  return StoreFileWithHash(key_set_id + kHlsAttributesFileNameExt,
                           serialized_file) == kNoError;
}

bool DeviceFiles::RetrieveHlsAttributes(
    const std::string& key_set_id, CdmHlsMethod* method,
    std::vector<uint8_t>* media_segment_iv) {
  if (!initialized_) {
    LOGW("DeviceFiles::RetrieveHlsAttributes: not initialized");
    return false;
  }

  video_widevine_client::sdk::File file;
  if (RetrieveHashedFile(key_set_id + kHlsAttributesFileNameExt, &file) !=
      kNoError) {
    LOGW("DeviceFiles::RetrieveHlsAttributes: unable to retrieve file");
    return false;
  }

  if (file.type() != video_widevine_client::sdk::File::HLS_ATTRIBUTES) {
    LOGW("DeviceFiles::RetrieveHlsAttributes: Incorrect file type: %u",
         file.type());
    return false;
  }

  if (file.version() != video_widevine_client::sdk::File::VERSION_1) {
    LOGW("DeviceFiles::RetrieveHlsAttributes: Incorrect file version: %u",
         file.version());
    return false;
  }

  if (!file.has_hls_attributes()) {
    LOGW("DeviceFiles::RetrieveHlsAttributes: HLS attributes not present");
    return false;
  }

  HlsAttributes attributes = file.hls_attributes();

  switch (attributes.method()) {
    case HlsAttributes_Method_AES_128:
      *method = kHlsMethodAes128;
      break;
    case HlsAttributes_Method_SAMPLE_AES:
      *method = kHlsMethodSampleAes;
      break;
    default:
      LOGW("DeviceFiles::RetrieveHlsAttributes: Unrecognized HLS method: %u",
           attributes.method());
      *method = kHlsMethodNone;
      break;
  }
  media_segment_iv->assign(attributes.media_segment_iv().begin(),
                           attributes.media_segment_iv().end());
  return true;
}

bool DeviceFiles::DeleteHlsAttributes(const std::string& key_set_id) {
  if (!initialized_) {
    LOGW("DeviceFiles::DeleteHlsAttributes: not initialized");
    return false;
  }
  return RemoveFile(key_set_id + kHlsAttributesFileNameExt);
}

bool DeviceFiles::StoreUsageTableInfo(
    const CdmUsageTableHeader& usage_table_header,
    const std::vector<CdmUsageEntryInfo>& usage_entry_info) {
  if (!initialized_) {
    LOGW("DeviceFiles::StoreUsageTableInfo: not initialized");
    return false;
  }

  // Fill in file information
  video_widevine_client::sdk::File file;

  file.set_type(video_widevine_client::sdk::File::USAGE_TABLE_INFO);
  file.set_version(video_widevine_client::sdk::File::VERSION_1);

  UsageTableInfo* usage_table_info = file.mutable_usage_table_info();
  usage_table_info->set_usage_table_header(usage_table_header);
  for (size_t i = 0; i < usage_entry_info.size(); ++i) {
    UsageTableInfo_UsageEntryInfo* info =
        usage_table_info->add_usage_entry_info();
    info->set_key_set_id(usage_entry_info[i].key_set_id);
    switch (usage_entry_info[i].storage_type) {
      case kStorageLicense:
        info->set_storage(
            UsageTableInfo_UsageEntryInfo_UsageEntryStorage_LICENSE);
        break;
      case kStorageUsageInfo:
        info->set_storage(
            UsageTableInfo_UsageEntryInfo_UsageEntryStorage_USAGE_INFO);
        info->set_usage_info_file_name(
            usage_entry_info[i].usage_info_file_name);
        break;
      case kStorageTypeUnknown:
      default:
        info->set_storage(
            UsageTableInfo_UsageEntryInfo_UsageEntryStorage_UNKNOWN);
        break;
    }
  }

  std::string serialized_file;
  file.SerializeToString(&serialized_file);

  return
      StoreFileWithHash(GetUsageTableFileName(), serialized_file) == kNoError;
}

bool DeviceFiles::RetrieveUsageTableInfo(
    CdmUsageTableHeader* usage_table_header,
    std::vector<CdmUsageEntryInfo>* usage_entry_info) {
  if (!initialized_) {
    LOGW("DeviceFiles::RetrieveUsageTableInfo: not initialized");
    return false;
  }

  if (usage_table_header == NULL) {
    LOGW(
        "DeviceFiles::RetrieveUsageTableInfo: usage_table_header not provided");
    return false;
  }

  if (usage_entry_info == NULL) {
    LOGW("DeviceFiles::RetrieveUsageTableInfo: usage_entry_info not provided");
    return false;
  }

  video_widevine_client::sdk::File file;
  if (RetrieveHashedFile(GetUsageTableFileName(), &file) != kNoError) {
    LOGW("DeviceFiles::RetrieveUsageTableInfo: unable to retrieve file");
    return false;
  }

  if (file.type() != video_widevine_client::sdk::File::USAGE_TABLE_INFO) {
    LOGW("DeviceFiles::RetrieveUsageTableInfo: Incorrect file type");
    return false;
  }

  if (file.version() != video_widevine_client::sdk::File::VERSION_1) {
    LOGW("DeviceFiles::RetrieveUsageTableInfo: Incorrect file version");
    return false;
  }

  if (!file.has_usage_table_info()) {
    LOGW("DeviceFiles::RetrieveUsageTableInfo: Usage table info not present");
    return false;
  }

  const UsageTableInfo& usage_table_info = file.usage_table_info();

  *usage_table_header = usage_table_info.usage_table_header();
  usage_entry_info->resize(usage_table_info.usage_entry_info_size());
  for (int i = 0; i < usage_table_info.usage_entry_info_size(); ++i) {
    const UsageTableInfo_UsageEntryInfo& info =
        usage_table_info.usage_entry_info(i);
    (*usage_entry_info)[i].key_set_id = info.key_set_id();
    switch (info.storage()) {
      case UsageTableInfo_UsageEntryInfo_UsageEntryStorage_LICENSE:
        (*usage_entry_info)[i].storage_type = kStorageLicense;
        break;
      case UsageTableInfo_UsageEntryInfo_UsageEntryStorage_USAGE_INFO:
        (*usage_entry_info)[i].storage_type = kStorageUsageInfo;
        (*usage_entry_info)[i].usage_info_file_name =
            info.usage_info_file_name();
        break;
      case UsageTableInfo_UsageEntryInfo_UsageEntryStorage_UNKNOWN:
      default:
        (*usage_entry_info)[i].storage_type = kStorageTypeUnknown;
        break;
    }
  }

  return true;
}

bool DeviceFiles::DeleteUsageTableInfo() {
  if (!initialized_) {
    LOGW("DeviceFiles::DeleteUsageTableInfo: not initialized");
    return false;
  }
  return RemoveFile(GetUsageTableFileName());
}

DeviceFiles::ResponseType DeviceFiles::StoreFileWithHash(
    const std::string& name,
    const std::string& serialized_file) {
  std::string hash = Sha256Hash(serialized_file);

  // Fill in hashed file data
  HashedFile hash_file;
  hash_file.set_file(serialized_file);
  hash_file.set_hash(hash);

  std::string serialized_hash_file;
  hash_file.SerializeToString(&serialized_hash_file);

  return StoreFileRaw(name, serialized_hash_file);
}

DeviceFiles::ResponseType DeviceFiles::StoreFileRaw(
    const std::string& name,
    const std::string& serialized_file) {
  std::string path;
  if (!Properties::GetDeviceFilesBasePath(security_level_, &path)) {
    LOGW("DeviceFiles::StoreFileRaw: Unable to get base path");
    return kBasePathUnavailable;
  }

  path += name;

  auto file =
      file_system_->Open(path, FileSystem::kCreate | FileSystem::kTruncate);
  if (!file) {
    LOGW("DeviceFiles::StoreFileRaw: File open failed: %s", path.c_str());
    return kFileOpenFailed;
  }

  ssize_t bytes = file->Write(serialized_file.data(), serialized_file.size());

  if (bytes != static_cast<ssize_t>(serialized_file.size())) {
    LOGW(
        "DeviceFiles::StoreFileRaw: write failed: (actual: %d, "
        "expected: %d)",
        bytes, serialized_file.size());
    return kFileWriteError;
  }

  LOGV("DeviceFiles::StoreFileRaw: success: %s (%db)", path.c_str(),
       serialized_file.size());
  return kNoError;
}

DeviceFiles::ResponseType DeviceFiles::RetrieveHashedFile(
    const std::string& name,
    video_widevine_client::sdk::File* deserialized_file) {
  std::string serialized_file;

  if (!deserialized_file) {
    LOGW("DeviceFiles::RetrieveHashedFile: Unspecified file parameter");
    return kParameterNull;
  }

  std::string path;
  if (!Properties::GetDeviceFilesBasePath(security_level_, &path)) {
    LOGW("DeviceFiles::RetrieveHashedFile: Unable to get base path");
    return kBasePathUnavailable;
  }

  path += name;

  if (!file_system_->Exists(path)) {
    LOGW("DeviceFiles::RetrieveHashedFile: %s does not exist", path.c_str());
    return kFileNotFound;
  }

  ssize_t bytes = file_system_->FileSize(path);
  if (bytes <= 0) {
    LOGW("DeviceFiles::RetrieveHashedFile: File size invalid: %s",
         path.c_str());
    // Remove the corrupted file so the caller will not get the same error
    // when trying to access the file repeatedly, causing the system to stall.
    file_system_->Remove(path);
    return kInvalidFileSize;
  }

  auto file = file_system_->Open(path, FileSystem::kReadOnly);
  if (!file) {
    return kFileOpenFailed;
  }

  std::string serialized_hash_file;
  serialized_hash_file.resize(bytes);
  bytes = file->Read(&serialized_hash_file[0], serialized_hash_file.size());

  if (bytes != static_cast<ssize_t>(serialized_hash_file.size())) {
    LOGW("DeviceFiles::RetrieveHashedFile: read failed: %d", bytes);
    // Remove the corrupted file so the caller will not get the same error
    // when trying to access the file repeatedly, causing the system to stall.
    file_system_->Remove(path);
    return kFileReadError;
  }

  LOGV("DeviceFiles::RetrieveHashedFile: success: %s (%db)", path.c_str(),
       serialized_hash_file.size());

  HashedFile hash_file;
  if (!hash_file.ParseFromString(serialized_hash_file)) {
    LOGW("DeviceFiles::RetrieveHashedFile: Unable to parse hash file");
    // Remove the corrupted file so the caller will not get the same error
    // when trying to access the file repeatedly, causing the system to stall.
    file_system_->Remove(path);
    return kFileParseError1;
  }

  std::string hash = Sha256Hash(hash_file.file());
  if (hash != hash_file.hash()) {
    LOGW("DeviceFiles::RetrieveHashedFile: Hash mismatch");
    // Remove the corrupted file so the caller will not get the same error
    // when trying to access the file repeatedly, causing the system to stall.
    file_system_->Remove(path);
    return kFileHashMismatch;
  }

  if (!deserialized_file->ParseFromString(hash_file.file())) {
    LOGW("DeviceFiles::RetrieveHashedFile: Unable to parse file");
    // Remove the corrupted file so the caller will not get the same error
    // when trying to access the file repeatedly, causing the system to stall.
    file_system_->Remove(path);
    return kFileParseError2;
  }
  return kNoError;
}

bool DeviceFiles::FileExists(const std::string& name) {
  std::string path;
  if (!Properties::GetDeviceFilesBasePath(security_level_, &path)) {
    LOGW("DeviceFiles::FileExists: Unable to get base path");
    return false;
  }
  path += name;

  return file_system_->Exists(path);
}

bool DeviceFiles::ListFiles(std::vector<std::string>* names) {
  std::string path;
  if (!Properties::GetDeviceFilesBasePath(security_level_, &path)) {
    LOGW("DeviceFiles::ListFiles: Unable to get base path");
    return false;
  }
  return file_system_->List(path, names);
}

bool DeviceFiles::RemoveFile(const std::string& name) {
  std::string path;
  if (!Properties::GetDeviceFilesBasePath(security_level_, &path)) {
    LOGW("DeviceFiles::RemoveFile: Unable to get base path");
    return false;
  }
  path += name;

  return file_system_->Remove(path);
}

ssize_t DeviceFiles::GetFileSize(const std::string& name) {
  std::string path;
  if (!Properties::GetDeviceFilesBasePath(security_level_, &path)) {
    LOGW("DeviceFiles::GetFileSize: Unable to get base path");
    return -1;
  }
  path += name;

  return file_system_->FileSize(path);
}

std::string DeviceFiles::GetCertificateFileName() {
  return kCertificateFileName;
}

std::string DeviceFiles::GetUsageTableFileName() {
  return kUsageTableFileName;
}

std::string DeviceFiles::GetHlsAttributesFileNameExtension() {
  return kHlsAttributesFileNameExt;
}

std::string DeviceFiles::GetLicenseFileNameExtension() {
  return kLicenseFileNameExt;
}

std::string DeviceFiles::GetUsageInfoFileName(const std::string& app_id) {
  std::string hash;
  if (app_id != "") {
    hash = GetFileNameSafeHash(app_id);
  }
  return kUsageInfoFileNamePrefix + hash + kUsageInfoFileNameExt;
}

std::string DeviceFiles::GetFileNameSafeHash(const std::string& input) {
  std::string hash = Md5Hash(input);
  return wvcdm::Base64SafeEncode(
      std::vector<uint8_t>(hash.begin(), hash.end()));
}

}  // namespace wvcdm
