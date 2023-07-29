// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#include "usage_table_header.h"

#include "crypto_session.h"
#include "license.h"
#include "log.h"
#include "wv_cdm_constants.h"

namespace {
std::string kEmptyString;
size_t kMaxCryptoRetries = 3;
size_t kMinUsageEntriesSupported = 200;
wvcdm::CdmKeySetId kDummyKeySetId = "DummyKsid";
uint64_t kOldUsageEntryTimeSinceLicenseReceived = 0;
uint64_t kOldUsageEntryTimeSinceFirstDecrypt = 0;
uint64_t kOldUsageEntryTimeSinceLastDecrypt = 0;
std::string kOldUsageEntryServerMacKey(wvcdm::MAC_KEY_SIZE, 0);
std::string kOldUsageEntryClientMacKey(wvcdm::MAC_KEY_SIZE, 0);
std::string kOldUsageEntryPoviderSessionToken =
    "nahZ6achSheiqua3TohQuei0ahwohv";
}

namespace wvcdm {

UsageTableHeader::UsageTableHeader()
    : security_level_(kSecurityLevelUninitialized),
      requested_security_level_(kLevelDefault),
      is_inited_(false) {
  file_system_.reset(new FileSystem());
  file_handle_.reset(new DeviceFiles(file_system_.get()));
}

bool UsageTableHeader::Init(CdmSecurityLevel security_level,
                            CryptoSession* crypto_session) {
  LOGI("UsageTableHeader::Init: security level: %d", security_level);
  if (crypto_session == NULL) {
    LOGE("UsageTableHeader::Init: no crypto session provided");
    return false;
  }

  switch (security_level) {
    case kSecurityLevelL1:
    case kSecurityLevelL3:
      break;
    default:
      LOGE("UsageTableHeader::Init: invalid security level provided: %d",
           security_level);
      return false;
  }

  security_level_ = security_level;
  requested_security_level_ =
      security_level_ == kSecurityLevelL3 ? kLevel3 : kLevelDefault;

  if (!file_handle_->Init(security_level)) {
    LOGE("UsageTableHeader::Init: device files initialization failed");
    return false;
  }

  CdmResponseType status = USAGE_INFO_NOT_FOUND;
  metrics::CryptoMetrics* metrics = crypto_session->GetCryptoMetrics();
  if (metrics == NULL) metrics = &alternate_crypto_metrics_;

  if (file_handle_->RetrieveUsageTableInfo(&usage_table_header_,
                                            &usage_entry_info_)) {
    LOGI("UsageTableHeader::Init: number of usage entries: %d",
         usage_entry_info_.size());
    status = crypto_session->LoadUsageTableHeader(usage_table_header_);

    // If the usage table header has been successfully loaded, and is at
    // minimum capacity (>200), we need to make sure we can still add and
    // remove entries. If not, clear files/data and recreate usage header table.
    if (status == NO_ERROR) {
      if (usage_entry_info_.size() > kMinUsageEntriesSupported) {
        uint32_t temporary_usage_entry_number;

        // Create a new temporary usage entry, close the session and then
        // try to delete it.
        CdmResponseType result = NO_ERROR;
        {
          // |local_crypto_session| points to an object whose scope is this
          // method or a test object whose scope is the lifetime of this class
          std::unique_ptr<CryptoSession> scoped_crypto_session;
          CryptoSession* local_crypto_session = test_crypto_session_.get();
          if (local_crypto_session == NULL) {
            scoped_crypto_session.reset(
                (CryptoSession::MakeCryptoSession(metrics)));
            local_crypto_session = scoped_crypto_session.get();
          }

          result = local_crypto_session->Open(requested_security_level_);
          if (result == NO_ERROR) {
            result = AddEntry(local_crypto_session, true,
                              kDummyKeySetId, kEmptyString,
                              &temporary_usage_entry_number);
          }
        }
        if (result == NO_ERROR) {
          result = DeleteEntry(temporary_usage_entry_number,
                               file_handle_.get(), metrics);
        }
        if (result != NO_ERROR) {
          LOGE("UsageTableHeader::Init: Unable to create/delete new entry. "
              "Clear usage entries, security level: %d, usage entries: %d",
              security_level, usage_entry_info_.size());
          status = result;
        }
      }
    }

    if (status != NO_ERROR) {
      LOGE(
          "UsageTableHeader::Init: load usage table failed, security level: %d",
          security_level);
      file_handle_->DeleteAllLicenses();
      file_handle_->DeleteAllUsageInfo();
      file_handle_->DeleteUsageTableInfo();
      usage_entry_info_.clear();
      usage_table_header_.clear();
      status = crypto_session->CreateUsageTableHeader(&usage_table_header_);
      if (status != NO_ERROR) return false;
      file_handle_->StoreUsageTableInfo(usage_table_header_, usage_entry_info_);
    }
  } else {
    status = crypto_session->CreateUsageTableHeader(&usage_table_header_);
    if (status != NO_ERROR) return false;
    file_handle_->StoreUsageTableInfo(usage_table_header_, usage_entry_info_);

    UpgradeFromUsageTable(file_handle_.get(), metrics);
    file_handle_->StoreUsageTableInfo(usage_table_header_, usage_entry_info_);
  }

  is_inited_ = true;
  return true;
}

CdmResponseType UsageTableHeader::AddEntry(
    CryptoSession* crypto_session, bool persistent_license,
    const CdmKeySetId& key_set_id, const std::string& usage_info_file_name,
    uint32_t* usage_entry_number) {
  LOGI("UsageTableHeader::AddEntry: Lock");

  metrics::CryptoMetrics* metrics = crypto_session->GetCryptoMetrics();
  if (metrics == NULL) metrics = &alternate_crypto_metrics_;

  CdmResponseType status = crypto_session->CreateUsageEntry(usage_entry_number);

  // If usage entry creation fails due to insufficient resources, release a
  // random entry and try again.
  for (uint32_t retry_count = 0;
       retry_count < kMaxCryptoRetries &&
           status == INSUFFICIENT_CRYPTO_RESOURCES_3;
       ++retry_count) {
    int64_t entry_number_to_delete =
        GetRandomInRange(usage_entry_info_.size());
    if (entry_number_to_delete < 0) break;
    DeleteEntry(entry_number_to_delete, file_handle_.get(), metrics);

    status = crypto_session->CreateUsageEntry(usage_entry_number);
  }

  if (status != NO_ERROR) return status;

  std::unique_lock<std::mutex> auto_lock(usage_table_header_lock_);
  if (*usage_entry_number < usage_entry_info_.size()) {
    LOGE("UsageTableHeader::AddEntry: new entry %d smaller than table size: %d",
         *usage_entry_number, usage_entry_info_.size());
    return USAGE_INVALID_NEW_ENTRY;
  }

  if (*usage_entry_number > usage_entry_info_.size()) {
    LOGW("UsageTableHeader::AddEntry: new entry %d larger than table size: %d",
         *usage_entry_number, usage_entry_info_.size());
    size_t number_of_entries = usage_entry_info_.size();
    usage_entry_info_.resize(*usage_entry_number + 1);
    for (size_t i = number_of_entries; i < usage_entry_info_.size() - 1; ++i) {
      usage_entry_info_[i].storage_type = kStorageTypeUnknown;
      usage_entry_info_[i].key_set_id.clear();
      usage_entry_info_[i].usage_info_file_name.clear();
    }
  } else /* *usage_entry_number == usage_entry_info_.size() */ {
    usage_entry_info_.resize(*usage_entry_number + 1);
  }

  usage_entry_info_[*usage_entry_number].storage_type =
      persistent_license ? kStorageLicense : kStorageUsageInfo;
  usage_entry_info_[*usage_entry_number].key_set_id = key_set_id;
  if (!persistent_license)
    usage_entry_info_[*usage_entry_number].usage_info_file_name =
        usage_info_file_name;

  LOGI("UsageTableHeader::AddEntry: usage entry #: %d", *usage_entry_number);
  file_handle_->StoreUsageTableInfo(usage_table_header_, usage_entry_info_);
  return NO_ERROR;
}

CdmResponseType UsageTableHeader::LoadEntry(CryptoSession* crypto_session,
                                            const CdmUsageEntry& usage_entry,
                                            uint32_t usage_entry_number) {
  {
    LOGI("UsageTableHeader::LoadEntry: Lock: usage entry #: %d",
         usage_entry_number);
    std::unique_lock<std::mutex> auto_lock(usage_table_header_lock_);

    if (usage_entry_number >= usage_entry_info_.size()) {
      LOGE(
          "UsageTableHeader::LoadEntry: usage entry number %d larger than "
          "table size: %d",
          usage_entry_number, usage_entry_info_.size());
      return USAGE_INVALID_LOAD_ENTRY;
    }
  }
  metrics::CryptoMetrics* metrics = crypto_session->GetCryptoMetrics();
  if (metrics == NULL) metrics = &alternate_crypto_metrics_;

  CdmResponseType status =
      crypto_session->LoadUsageEntry(usage_entry_number, usage_entry);

  // If loading a usage entry fails due to insufficient resources, release a
  // random entry different from |usage_entry_number| and try again. If there
  // are no more entries to release, we fail.
  for (uint32_t retry_count = 0;
       retry_count < kMaxCryptoRetries &&
           status == INSUFFICIENT_CRYPTO_RESOURCES_3;
       ++retry_count) {
    // Get a random entry from the other entries.
    int64_t entry_number_to_delete = GetRandomInRangeWithExclusion(
        usage_entry_info_.size(), usage_entry_number);
    if (entry_number_to_delete < 0) break;
    DeleteEntry(entry_number_to_delete, file_handle_.get(), metrics);
    status = crypto_session->LoadUsageEntry(usage_entry_number, usage_entry);
  }

  return status;
}

CdmResponseType UsageTableHeader::UpdateEntry(CryptoSession* crypto_session,
                                              CdmUsageEntry* usage_entry) {
  LOGI("UsageTableHeader::UpdateEntry: Lock");
  std::unique_lock<std::mutex> auto_lock(usage_table_header_lock_);
  CdmResponseType status =
      crypto_session->UpdateUsageEntry(&usage_table_header_, usage_entry);

  if (status != NO_ERROR) return status;

  file_handle_->StoreUsageTableInfo(usage_table_header_, usage_entry_info_);
  return NO_ERROR;
}

CdmResponseType UsageTableHeader::DeleteEntry(uint32_t usage_entry_number,
                                              DeviceFiles* handle,
                                              metrics::CryptoMetrics* metrics) {
  LOGI("UsageTableHeader::DeleteEntry: Lock: usage entry #: %d",
       usage_entry_number);
  std::unique_lock<std::mutex> auto_lock(usage_table_header_lock_);
  if (usage_entry_number >= usage_entry_info_.size()) {
    LOGE("UsageTableHeader::DeleteEntry: usage entry number %d larger than "
         "usage entry size %d", usage_entry_number, usage_entry_info_.size());
    return USAGE_INVALID_PARAMETERS_1;
  }

  // Find the last valid entry number, in order to swap
  size_t swap_entry_number = usage_entry_info_.size() - 1;
  CdmUsageEntry swap_usage_entry;
  bool swap_usage_entry_valid = false;

  while (!swap_usage_entry_valid && swap_entry_number > usage_entry_number) {
    switch (usage_entry_info_[swap_entry_number].storage_type) {
      case kStorageLicense:
      case kStorageUsageInfo: {
        CdmResponseType status =
            GetEntry(swap_entry_number, handle, &swap_usage_entry);
        if (status == NO_ERROR) swap_usage_entry_valid = true;
        break;
      }
      case kStorageTypeUnknown:
      default:
        break;
    }
    if (!swap_usage_entry_valid) --swap_entry_number;
  }

  uint32_t number_of_entries_to_be_deleted =
      usage_entry_info_.size() - usage_entry_number;

  if (swap_usage_entry_valid) {
    CdmResponseType status = MoveEntry(swap_entry_number, swap_usage_entry,
                                       usage_entry_number, handle, metrics);
    // If unable to move entry, unset storage type of entry to be deleted and
    // resize |usage_entry_info_| so that swap usage entry is the last entry.
    if (status != NO_ERROR) {
      usage_entry_info_[usage_entry_number].storage_type = kStorageTypeUnknown;
      usage_entry_info_[usage_entry_number].key_set_id.clear();
      if (usage_entry_info_.size() - 1 == swap_entry_number) {
        file_handle_->StoreUsageTableInfo(usage_table_header_,
                                          usage_entry_info_);
      } else {
        Shrink(metrics, usage_entry_info_.size() - swap_entry_number - 1);
      }
      return NO_ERROR;
    }
    number_of_entries_to_be_deleted =
        usage_entry_info_.size() - swap_entry_number;
  }
  return Shrink(metrics, number_of_entries_to_be_deleted);
}

CdmResponseType UsageTableHeader::MoveEntry(
    uint32_t from_usage_entry_number, const CdmUsageEntry& from_usage_entry,
    uint32_t to_usage_entry_number, DeviceFiles* handle,
    metrics::CryptoMetrics* metrics) {
  LOGI("UsageTableHeader::MoveEntry: usage entry #: %d -> %d",
       from_usage_entry_number, to_usage_entry_number);

  // crypto_session points to an object whose scope is this method or a test
  // object whose scope is the lifetime of this class
  std::unique_ptr<CryptoSession> scoped_crypto_session;
  CryptoSession* crypto_session = test_crypto_session_.get();
  if (crypto_session == NULL) {
    scoped_crypto_session.reset((CryptoSession::MakeCryptoSession(metrics)));
    crypto_session = scoped_crypto_session.get();
  }

  crypto_session->Open(requested_security_level_);

  CdmResponseType status =
      crypto_session->LoadUsageEntry(from_usage_entry_number, from_usage_entry);

  if (status != NO_ERROR) {
    LOGE("UsageTableHeader::MoveEntry: Failed to load usage entry: %d",
         from_usage_entry_number);
    return status;
  }

  status = crypto_session->MoveUsageEntry(to_usage_entry_number);

  if (status != NO_ERROR) {
    LOGE("UsageTableHeader::MoveEntry: Failed to move usage entry: %d->%d",
         from_usage_entry_number, to_usage_entry_number);
    return status;
  }

  usage_entry_info_[to_usage_entry_number] =
      usage_entry_info_[from_usage_entry_number];

  CdmUsageEntry usage_entry;
  status = crypto_session->UpdateUsageEntry(&usage_table_header_, &usage_entry);

  if (status != NO_ERROR) {
    LOGE("UsageTableHeader::MoveEntry: Failed to update usage entry: %d",
         to_usage_entry_number);
    return status;
  }

  file_handle_->StoreUsageTableInfo(usage_table_header_, usage_entry_info_);

  StoreEntry(to_usage_entry_number, handle, usage_entry);

  return NO_ERROR;
}

CdmResponseType UsageTableHeader::GetEntry(uint32_t usage_entry_number,
                                           DeviceFiles* handle,
                                           CdmUsageEntry* usage_entry) {
  LOGI("UsageTableHeader::GetEntry: usage entry #: %d, storage type: %d",
       usage_entry_number, usage_entry_info_[usage_entry_number].storage_type);
  uint32_t entry_number;
  switch (usage_entry_info_[usage_entry_number].storage_type) {
    case kStorageLicense: {
      DeviceFiles::LicenseState license_state;
      std::string init_data, key_request, key_response, key_renewal_request;
      std::string key_renewal_response, release_server_url;
      int64_t playback_start_time, last_playback_time, grace_period_end_time;
      CdmAppParameterMap app_parameters;
      DeviceFiles::ResponseType sub_error_code = DeviceFiles::kNoError;
      if (!handle->RetrieveLicense(
              usage_entry_info_[usage_entry_number].key_set_id, &license_state,
              &init_data, &key_request, &key_response, &key_renewal_request,
              &key_renewal_response, &release_server_url, &playback_start_time,
              &last_playback_time, &grace_period_end_time, &app_parameters,
              usage_entry, &entry_number, &sub_error_code)) {
        LOGE("UsageTableHeader::GetEntry: Failed to retrieve license, %d",
             sub_error_code);
        return USAGE_GET_ENTRY_RETRIEVE_LICENSE_FAILED;
      }
      break;
    }
    case kStorageUsageInfo: {
      std::string provider_session_token;
      CdmKeyMessage license_request;
      CdmKeyResponse license_response;

      if (!handle->RetrieveUsageInfoByKeySetId(
              usage_entry_info_[usage_entry_number].usage_info_file_name,
              usage_entry_info_[usage_entry_number].key_set_id,
              &provider_session_token, &license_request, &license_response,
              usage_entry, &entry_number)) {
        LOGE(
            "UsageTableHeader::GetEntry: Failed to retrieve usage information");
        return USAGE_GET_ENTRY_RETRIEVE_USAGE_INFO_FAILED;
      }
      break;
    }
    case kStorageTypeUnknown:
    default:
      LOGE(
          "UsageTableHeader::GetEntry: Attempting to retrieve usage "
          "information from unknown storage type: %d",
          usage_entry_info_[usage_entry_number].storage_type);
      return USAGE_GET_ENTRY_RETRIEVE_INVALID_STORAGE_TYPE;
  }

  if (usage_entry_number != entry_number) {
    LOGE("UsageTableHeader::GetEntry: entry number mismatch: (%d, %d)",
         usage_entry_number, entry_number);
    return USAGE_ENTRY_NUMBER_MISMATCH;
  }

  return NO_ERROR;
}

CdmResponseType UsageTableHeader::StoreEntry(uint32_t usage_entry_number,
                                             DeviceFiles* handle,
                                             const CdmUsageEntry& usage_entry) {
  LOGI("UsageTableHeader::StoreEntry: usage entry #: %d, storage type: %d",
       usage_entry_number, usage_entry_info_[usage_entry_number].storage_type);
  uint32_t entry_number;
  switch (usage_entry_info_[usage_entry_number].storage_type) {
    case kStorageLicense: {
      DeviceFiles::LicenseState license_state;
      std::string init_data, key_request, key_response, key_renewal_request;
      std::string key_renewal_response, release_server_url;
      int64_t playback_start_time, last_playback_time, grace_period_end_time;
      CdmAppParameterMap app_parameters;
      CdmUsageEntry entry;
      DeviceFiles::ResponseType sub_error_code = DeviceFiles::kNoError;
      if (!handle->RetrieveLicense(
              usage_entry_info_[usage_entry_number].key_set_id, &license_state,
              &init_data, &key_request, &key_response, &key_renewal_request,
              &key_renewal_response, &release_server_url, &playback_start_time,
              &last_playback_time, &grace_period_end_time, &app_parameters,
              &entry, &entry_number, &sub_error_code)) {
        LOGE("UsageTableHeader::StoreEntry: Failed to retrieve license, %d",
             sub_error_code);
        return USAGE_STORE_ENTRY_RETRIEVE_LICENSE_FAILED;
      }
      if (!handle->StoreLicense(
              usage_entry_info_[usage_entry_number].key_set_id, license_state,
              init_data, key_request, key_response, key_renewal_request,
              key_renewal_response, release_server_url, playback_start_time,
              last_playback_time, grace_period_end_time, app_parameters,
              usage_entry, usage_entry_number, &sub_error_code)) {
        LOGE("UsageTableHeader::StoreEntry: Failed to store license, %d",
             sub_error_code);
        return USAGE_STORE_LICENSE_FAILED;
      }
      break;
    }
    case kStorageUsageInfo: {
      CdmUsageEntry entry;
      std::string provider_session_token, init_data, key_request, key_response,
          key_renewal_request;
      if (!handle->RetrieveUsageInfoByKeySetId(
              usage_entry_info_[usage_entry_number].usage_info_file_name,
              usage_entry_info_[usage_entry_number].key_set_id,
              &provider_session_token, &key_request, &key_response, &entry,
              &entry_number)) {
        LOGE(
            "UsageTableHeader::StoreEntry: Failed to retrieve usage "
            "information");
        return USAGE_STORE_ENTRY_RETRIEVE_USAGE_INFO_FAILED;
      }
      handle->DeleteUsageInfo(
          usage_entry_info_[usage_entry_number].usage_info_file_name,
          provider_session_token);
      if (!handle->StoreUsageInfo(
              provider_session_token, key_request, key_response,
              usage_entry_info_[usage_entry_number].usage_info_file_name,
              usage_entry_info_[usage_entry_number].key_set_id, usage_entry,
              usage_entry_number)) {
        LOGE("UsageTableHeader::StoreEntry: Failed to store usage information");
        return USAGE_STORE_USAGE_INFO_FAILED;
      }
      break;
    }
    case kStorageTypeUnknown:
    default:
      LOGE(
          "UsageTableHeader::GetUsageEntry: Attempting to retrieve usage "
          "information from unknown storage type: %d",
          usage_entry_info_[usage_entry_number].storage_type);
      return USAGE_STORE_ENTRY_RETRIEVE_INVALID_STORAGE_TYPE;
  }
  return NO_ERROR;
}

CdmResponseType UsageTableHeader::Shrink(
    metrics::CryptoMetrics* metrics,
    uint32_t number_of_usage_entries_to_delete) {
  LOGI("UsageTableHeader::Shrink: %d (of %d)",
       number_of_usage_entries_to_delete, usage_entry_info_.size());
  if (usage_entry_info_.empty()) {
    LOGE("UsageTableHeader::Shrink: usage entry info table unexpectedly empty");
    return NO_USAGE_ENTRIES;
  }

  if (usage_entry_info_.size() < number_of_usage_entries_to_delete) {
    LOGW(
        "UsageTableHeader::Shrink: cannot delete %d entries when usage entry "
        "table size is %d", number_of_usage_entries_to_delete,
        usage_entry_info_.size());
    return NO_ERROR;
  }

  if (number_of_usage_entries_to_delete == 0) return NO_ERROR;

  usage_entry_info_.resize(usage_entry_info_.size() -
                           number_of_usage_entries_to_delete);

  // crypto_session points to an object whose scope is this method or a test
  // object whose scope is the lifetime of this class
  std::unique_ptr<CryptoSession> scoped_crypto_session;
  CryptoSession* crypto_session = test_crypto_session_.get();
  if (crypto_session == NULL) {
    scoped_crypto_session.reset((CryptoSession::MakeCryptoSession(metrics)));
    crypto_session = scoped_crypto_session.get();
  }

  CdmResponseType status = crypto_session->Open(requested_security_level_);
  if (status != NO_ERROR) return status;

  status = crypto_session->ShrinkUsageTableHeader(usage_entry_info_.size(),
                                                  &usage_table_header_);
  if (status != NO_ERROR) return status;

  file_handle_->StoreUsageTableInfo(usage_table_header_, usage_entry_info_);
  return NO_ERROR;
}

CdmResponseType UsageTableHeader::UpgradeFromUsageTable(
    DeviceFiles* handle, metrics::CryptoMetrics* metrics) {
  UpgradeLicensesFromUsageTable(handle, metrics);
  UpgradeUsageInfoFromUsageTable(handle, metrics);
  return NO_ERROR;
}

bool UsageTableHeader::UpgradeLicensesFromUsageTable(
    DeviceFiles* handle, metrics::CryptoMetrics* metrics) {
  // Fetch the key set IDs for each offline license. For each license
  // * retrieve the provider session token,
  // * load the old usage table by creating a new dummy entry
  // * copy over the entry from the old usage table and
  // * update the usage header table and entry numbers
  // * save the usage table header and store the usage entry number and
  //   usage entry along with the license to persistent memory
  std::vector<std::string> key_set_ids;
  if (!handle->ListLicenses(&key_set_ids)) {
    LOGW(
        "UpgradeUsageTableHeader::UpgradeLicensesFromUsageTable: unable to "
        "retrieve list of licenses");
    return false;
  }

  for (size_t i = 0; i < key_set_ids.size(); ++i) {
    DeviceFiles::LicenseState license_state;
    std::string init_data, key_request, key_response, key_renewal_request;
    std::string key_renewal_response, release_server_url;
    int64_t playback_start_time, last_playback_time, grace_period_end_time;
    CdmAppParameterMap app_parameters;
    CdmUsageEntry usage_entry;
    uint32_t usage_entry_number;
    DeviceFiles::ResponseType sub_error_code = DeviceFiles::kNoError;
    if (!handle->RetrieveLicense(
            key_set_ids[i], &license_state, &init_data, &key_request,
            &key_response, &key_renewal_request, &key_renewal_response,
            &release_server_url, &playback_start_time, &last_playback_time,
            &grace_period_end_time, &app_parameters, &usage_entry,
            &usage_entry_number, &sub_error_code)) {
      LOGW(
          "UsageTableHeader::UpgradeLicensesFromUsageTable: Failed to "
          "retrieve license, %d", sub_error_code);
      continue;
    }

    std::string provider_session_token;
    if (!CdmLicense::ExtractProviderSessionToken(key_response,
                                                 &provider_session_token)) {
      LOGW(
          "UsageTableHeader::UpgradeLicensesFromUsageTable: Failed to "
          "retrieve provider session token");
      continue;
    }

    if (provider_session_token.empty()) continue;

    std::unique_ptr<CryptoSession> crypto_session(
        CryptoSession::MakeCryptoSession(metrics));
    CdmResponseType status = crypto_session->Open(requested_security_level_);

    if (status != NO_ERROR) continue;

    // We create this entry since OEMCrypto_CopyOldUsageEntry needs the old
    // usage table to be loaded in order to copy an entry.
    if (!CreateDummyOldUsageEntry(crypto_session.get())) continue;

    status = AddEntry(crypto_session.get(), true /* persistent license */,
                      key_set_ids[i], kEmptyString, &usage_entry_number);

    if (status != NO_ERROR) continue;

    status = crypto_session->CopyOldUsageEntry(provider_session_token);

    if (status != NO_ERROR) {
      crypto_session->Close();
      Shrink(metrics, 1);
      continue;
    }

    status = UpdateEntry(crypto_session.get(), &usage_entry);

    if (status != NO_ERROR) {
      crypto_session->Close();
      Shrink(metrics, 1);
      continue;
    }

    if (!handle->StoreLicense(
            key_set_ids[i], license_state, init_data, key_request, key_response,
            key_renewal_request, key_renewal_response, release_server_url,
            playback_start_time, last_playback_time, grace_period_end_time,
            app_parameters, usage_entry, usage_entry_number, &sub_error_code)) {
      LOGE(
          "UsageTableHeader::UpgradeLicensesFromUsageTable: Failed to store "
          "license, %d", sub_error_code);
      continue;
    }
  }

  return NO_ERROR;
}

bool UsageTableHeader::UpgradeUsageInfoFromUsageTable(
    DeviceFiles* handle, metrics::CryptoMetrics* metrics) {
  // Fetch all usage files. For each file retrieve all the usage info records
  // within the file. For each piece of usage information
  // * load the old usage table by creating a new dummy entry
  // * copy over the entry from the old usage table and
  // * update the usage header table and entry numbers
  // * save the usage table header
  // * once done processing all the usage records from a file, save the usage
  //   information to persistent memory along with usage entry number and usage
  //   entry.
  std::vector<std::string> usage_info_file_names;
  if (!handle->ListUsageInfoFiles(&usage_info_file_names)) {
    LOGW(
        "UpgradeUsageTableHeader::UpgradeUsageInfoFromUsageTable: Unable to "
        "retrieve list of usage info file names");
    return false;
  }

  for (size_t i = 0; i < usage_info_file_names.size(); ++i) {
    std::vector<DeviceFiles::CdmUsageData> usage_data;
    if (!handle->RetrieveUsageInfo(usage_info_file_names[i], &usage_data)) {
      LOGW(
          "UsageTableHeader::UpgradeUsageInfoFromUsageTable: Failed to "
          "retrieve usage records from %s",
          usage_info_file_names[i].c_str());
      continue;
    }

    for (size_t j = 0; j < usage_data.size(); ++j) {
      if (usage_data[j].provider_session_token.empty()) {
        LOGW(
            "UsageTableHeader::UpgradeUsageInfoFromUsageTable: Provider "
            "session id empty");
        continue;
      }

      std::unique_ptr<CryptoSession> crypto_session(
          CryptoSession::MakeCryptoSession(metrics));
      CdmResponseType status = crypto_session->Open(requested_security_level_);

      if (status != NO_ERROR) continue;

      // We create this entry since OEMCrypto_CopyOldUsageEntry needs the old
      // usage table to be loaded in order to copy an entry.
      if (!CreateDummyOldUsageEntry(crypto_session.get())) continue;

      // TODO(rfrias): We need to fill in the app id, but it is hashed
      // and we have no way to extract. Use the hased filename instead?
      status = AddEntry(crypto_session.get(), false /* usage info */,
                        usage_data[j].key_set_id, usage_info_file_names[i],
                        &(usage_data[j].usage_entry_number));

      if (status != NO_ERROR) continue;

      status = crypto_session->CopyOldUsageEntry(
          usage_data[j].provider_session_token);

      if (status != NO_ERROR) {
        crypto_session->Close();
        Shrink(metrics, 1);
        continue;
      }

      status = UpdateEntry(crypto_session.get(), &(usage_data[j].usage_entry));

      if (status != NO_ERROR) {
        crypto_session->Close();
        Shrink(metrics, 1);
        continue;
      }
    }

    if (!handle->StoreUsageInfo(usage_info_file_names[i], usage_data)) {
      LOGE(
          "UsageTableHeader::StoreUsageInfo: Failed to store usage records to "
          "%s",
          usage_info_file_names[i].c_str());
      continue;
    }
  }

  return NO_ERROR;
}

int64_t UsageTableHeader::GetRandomInRange(size_t upper_bound_exclusive) {
  if (upper_bound_exclusive == 0) {
    LOGE("upper_bound_exclusive must be > 0.");
    return -1;
  }
  return (int)((double)rand() / ((double)RAND_MAX + 1) * upper_bound_exclusive);
}

int64_t UsageTableHeader::GetRandomInRangeWithExclusion(
    size_t upper_bound_exclusive, size_t exclude) {
  if ((upper_bound_exclusive <= 1) ||
      (exclude >= upper_bound_exclusive)) {
    LOGE(
        "upper_bound_exclusive must be > 1 and exclude must be < "
        "upper_bound_exclusive.");
    return -1;
  }
  uint32_t random = GetRandomInRange(upper_bound_exclusive - 1);
  if (random >= exclude) random++;
  return random;
}

// TODO(fredgc): remove when b/65730828 is addressed
bool UsageTableHeader::CreateDummyOldUsageEntry(CryptoSession* crypto_session) {
  return crypto_session->CreateOldUsageEntry(
             kOldUsageEntryTimeSinceLicenseReceived,
             kOldUsageEntryTimeSinceFirstDecrypt,
             kOldUsageEntryTimeSinceLastDecrypt,
             CryptoSession::kUsageDurationsInvalid,
             kOldUsageEntryServerMacKey,
             kOldUsageEntryClientMacKey,
             kOldUsageEntryPoviderSessionToken);
}

// Test only method.
void UsageTableHeader::DeleteEntryForTest(uint32_t usage_entry_number) {
  LOGV("UsageTableHeader::DeleteEntryForTest: usage_entry_number: %d",
       usage_entry_number);
  if (usage_entry_number >= usage_entry_info_.size()) {
    LOGE("UsageTableHeader::DeleteEntryForTest: usage entry number %d larger "
         "than usage entry size %d", usage_entry_number,
         usage_entry_info_.size());
    return;
  }
  // Move last entry into deleted location and shrink usage entries
  usage_entry_info_[usage_entry_number] =
      usage_entry_info_[usage_entry_info_.size() - 1];
  usage_entry_info_.resize(usage_entry_info_.size() - 1);
}

}  // namespace wvcdm
