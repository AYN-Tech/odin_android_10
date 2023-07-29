// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#ifndef WVCDM_CORE_USAGE_TABLE_HEADER_H_
#define WVCDM_CORE_USAGE_TABLE_HEADER_H_

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "crypto_session.h"
#include "device_files.h"
#include "disallow_copy_and_assign.h"
#include "file_store.h"
#include "metrics_collections.h"
#include "wv_cdm_types.h"

namespace wvcdm {

// Offline licenses/secure stops may be securely tracked using usage
// tables (OEMCrypto v9-12) or usage table headers+usage entries
// (OEMCrypto v13+). This class assists with the latter, synchronizing
// access to usage table header and associated data-structures and controlling
// when they are read in or written out to non-secure persistent storage.
//
// Each OEMCrypto (for each security level) will maintain its own usage table
// header. Each license will have an associated usage entry that is also
// stored in persistent memory and is noted in the usage table header.
// Usage entry information will be verified when licenses are loaded.
//
// OEMCrypto for each security level have their own usage table
// headers. They are loaded on initialization and written out periodically.
// The lifecycle of this class is tied to when OEMCrypto is
// initialized/terminated.
//
// Sessions and licenses are however handled by CdmSession and so most
// calls to maniplate the usage table header related to usage entries
// are by CdmSession.
//
// Upgrades from a fixed size usage table (supported by previous
// versions of the OEMCrypto API v9-12) are handled by this class.
// |usage_entry| and |usage_entry_number|s need to be saved in the license
// and usage info records by the caller.
class UsageTableHeader {
 public:
  UsageTableHeader();
  virtual ~UsageTableHeader() {}

  // |crypto_session| is used to create or load a usage master table and
  // not cached beyound this call.
  bool Init(CdmSecurityLevel security_level, CryptoSession* crypto_session);

  // |persistent_license| false indicates usage info record
  virtual CdmResponseType AddEntry(CryptoSession* crypto_session,
                                   bool persistent_license,
                                   const CdmKeySetId& key_set_id,
                                   const std::string& usage_info_filename,
                                   uint32_t* usage_entry_number);
  virtual CdmResponseType LoadEntry(CryptoSession* crypto_session,
                                   const CdmUsageEntry& usage_entry,
                                   uint32_t usage_entry_number);
  virtual CdmResponseType UpdateEntry(CryptoSession* crypto_session,
                                      CdmUsageEntry* usage_entry);

  // The licenses or usage info records specified by |usage_entry_number|
  // should not be in use by any open CryptoSession objects when calls
  // to DeleteEntry and MoveEntry are made.
  virtual CdmResponseType DeleteEntry(uint32_t usage_entry_number,
                                      DeviceFiles* handle,
                                      metrics::CryptoMetrics* metrics);

  // Test only method. This method emulates the behavior of DeleteEntry
  // without actually invoking OEMCrypto (through CryptoSession)
  // or storage (through DeviceFiles). It modifies internal data structures
  // when DeleteEntry is mocked. This allows one to test methods that are
  // dependent on DeleteEntry without having to set expectations
  // for the objects that DeleteEntry depends on.
  void DeleteEntryForTest(uint32_t usage_entry_number);

  // TODO(rfrias): Move to utility class
  static int64_t GetRandomInRange(size_t upper_bound_exclusive);
  static int64_t GetRandomInRangeWithExclusion(size_t upper_bound_exclusive,
                                               size_t exclude);
  size_t size() { return usage_entry_info_.size(); }

 private:
  CdmResponseType MoveEntry(uint32_t from /* usage entry number */,
                            const CdmUsageEntry& from_usage_entry,
                            uint32_t to /* usage entry number */,
                            DeviceFiles* handle,
                            metrics::CryptoMetrics* metrics);

  CdmResponseType GetEntry(uint32_t usage_entry_number, DeviceFiles* handle,
                           CdmUsageEntry* usage_entry);
  CdmResponseType StoreEntry(uint32_t usage_entry_number, DeviceFiles* handle,
                             const CdmUsageEntry& usage_entry);

  CdmResponseType Shrink(metrics::CryptoMetrics* metrics,
                         uint32_t number_of_usage_entries_to_delete);

  CdmResponseType UpgradeFromUsageTable(DeviceFiles* handle,
                                        metrics::CryptoMetrics* metrics);
  bool UpgradeLicensesFromUsageTable(DeviceFiles* handle,
                                     metrics::CryptoMetrics* metrics);
  bool UpgradeUsageInfoFromUsageTable(DeviceFiles* handle,
                                      metrics::CryptoMetrics* metrics);

  virtual bool is_inited() { return is_inited_; }

  virtual bool CreateDummyOldUsageEntry(CryptoSession* crypto_session);

  // This handle and file system is only to be used when accessing
  // usage_table_header. Usage entries should use the file system provided
  // by CdmSession.
  std::unique_ptr<DeviceFiles> file_handle_;
  std::unique_ptr<FileSystem> file_system_;
  CdmSecurityLevel security_level_;
  SecurityLevel requested_security_level_;

  CdmUsageTableHeader usage_table_header_;
  std::vector<CdmUsageEntryInfo> usage_entry_info_;

  // Lock to ensure that a single object is created for each security level
  // and data member to represent whether an object has been correctly
  // initialized.
  bool is_inited_;

  // Synchonizes access to the Usage Table Header and bookkeeping
  // data-structures
  std::mutex usage_table_header_lock_;

  metrics::CryptoMetrics alternate_crypto_metrics_;

  // Test related declarations
  friend class UsageTableHeaderTest;

  // These setters are for testing only. Takes ownership of the pointers.
  void SetDeviceFiles(DeviceFiles* device_files) {
    file_handle_.reset(device_files);
  }
  void SetCryptoSession(CryptoSession* crypto_session) {
    test_crypto_session_.reset(crypto_session);
  }

  // Test related data members
  std::unique_ptr<CryptoSession> test_crypto_session_;

  CORE_DISALLOW_COPY_AND_ASSIGN(UsageTableHeader);
};

}  // namespace wvcdm

#endif  // WVCDM_CORE_USAGE_TABLE_HEADER_H_
