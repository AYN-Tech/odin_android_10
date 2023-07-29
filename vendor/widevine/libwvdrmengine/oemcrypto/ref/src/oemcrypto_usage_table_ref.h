// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
//  Reference implementation of OEMCrypto APIs
//
#ifndef OEMCRYPTO_USAGE_TABLE_REF_H_
#define OEMCRYPTO_USAGE_TABLE_REF_H_

#include <stdint.h>
#include <map>
#include <string>
#include <vector>

#include "OEMCryptoCENC.h"
#include "openssl/sha.h"
#include "oemcrypto_types.h"

namespace wvoec_ref {

class SessionContext;
class CryptoEngine;
class UsageTable;
class OldUsageTable;
class OldUsageTableEntry;

const size_t kMaxPSTLength = 255;
// This is the data we store offline.
struct StoredUsageEntry {
  int64_t generation_number;
  int64_t time_of_license_received;
  int64_t time_of_first_decrypt;
  int64_t time_of_last_decrypt;
  enum OEMCrypto_Usage_Entry_Status status;
  uint8_t mac_key_server[wvoec::MAC_KEY_SIZE];
  uint8_t mac_key_client[wvoec::MAC_KEY_SIZE];
  uint32_t index;
  uint8_t pst[kMaxPSTLength+1];  // add 1 for padding.
  uint8_t pst_length;
};

class UsageTableEntry {
 public:
  UsageTableEntry(UsageTable* table, uint32_t index, int64_t generation);
  virtual ~UsageTableEntry();  // Free memory, remove reference in header.
  bool Inactive() { return data_.status >= kInactive; }
  OEMCryptoResult SetPST(const uint8_t* pst, size_t pst_length);
  bool VerifyPST(const uint8_t* pst, size_t pst_length);
  bool VerifyMacKeys(const std::vector<uint8_t>& server,
                     const std::vector<uint8_t>& client);
  bool SetMacKeys(const std::vector<uint8_t>& server,
                  const std::vector<uint8_t>& client);
  // Returns false if the entry is inactive.  Otherwise, returns true.
  // If the status was unused, it is updated, and decrypt times are flaged
  // for update.
  bool CheckForUse();
  void Deactivate(const std::vector<uint8_t>& pst);
  virtual OEMCryptoResult ReportUsage(const std::vector<uint8_t>& pst,
                                      uint8_t* buffer, size_t* buffer_length);
  virtual void UpdateAndIncrement();
  OEMCryptoResult SaveData(CryptoEngine* ce, SessionContext* session,
                           uint8_t* signed_buffer, size_t buffer_size);
  OEMCryptoResult LoadData(CryptoEngine* ce, uint32_t index,
                           const std::vector<uint8_t>& buffer);
  virtual OEMCryptoResult CopyOldUsageEntry(const std::vector<uint8_t>& pst);
  int64_t generation_number() { return data_.generation_number; }
  void set_generation_number(int64_t value) { data_.generation_number = value; }
  void set_index(int32_t index) { data_.index = index; }
  uint32_t index() { return data_.index; }
  static size_t SignedEntrySize();
  const uint8_t* mac_key_server() { return data_.mac_key_server; }
  const uint8_t* mac_key_client() { return data_.mac_key_client; }

 protected:
  UsageTable* usage_table_;  // Owner of this object.
  bool recent_decrypt_;
  bool forbid_report_;
  StoredUsageEntry data_;
};

class UsageTable {
 public:
  explicit UsageTable(CryptoEngine* ce)
      : ce_(ce), header_loaded_(false), old_table_(NULL) {};
  virtual ~UsageTable();

  OEMCryptoResult CreateNewUsageEntry(SessionContext* session,
                                      UsageTableEntry** entry,
                                      uint32_t* usage_entry_number);
  OEMCryptoResult LoadUsageEntry(SessionContext* session,
                                 UsageTableEntry** entry, uint32_t index,
                                 const std::vector<uint8_t>& buffer);
  OEMCryptoResult UpdateUsageEntry(SessionContext* session,
                                   UsageTableEntry* entry,
                                   uint8_t* header_buffer,
                                   size_t* header_buffer_length,
                                   uint8_t* entry_buffer,
                                   size_t* entry_buffer_length);
  OEMCryptoResult MoveEntry(UsageTableEntry* entry, uint32_t new_index);
  OEMCryptoResult CreateUsageTableHeader(uint8_t* header_buffer,
                                         size_t* header_buffer_length);
  OEMCryptoResult LoadUsageTableHeader(const std::vector<uint8_t>& buffer);
  OEMCryptoResult ShrinkUsageTableHeader(uint32_t new_table_size,
                                         uint8_t* header_buffer,
                                         size_t* header_buffer_length);
  void ReleaseEntry(uint32_t index) { sessions_[index] = 0; }
  void IncrementGeneration();
  static size_t SignedHeaderSize(size_t count);
  OldUsageTableEntry* FindOldUsageEntry(const std::vector<uint8_t>& pst);
  OEMCryptoResult DeleteOldUsageTable();
  OEMCryptoResult CreateOldUsageEntry(uint64_t time_since_license_received,
                                      uint64_t time_since_first_decrypt,
                                      uint64_t time_since_last_decrypt,
                                      OEMCrypto_Usage_Entry_Status status,
                                      uint8_t* server_mac_key,
                                      uint8_t* client_mac_key,
                                      const uint8_t* pst, size_t pst_length);

 protected:
  virtual UsageTableEntry* MakeEntry(uint32_t index);
  virtual OEMCryptoResult SaveUsageTableHeader(uint8_t* signed_buffer,
                                               size_t buffer_size);
  virtual bool SaveGenerationNumber();
  virtual bool LoadGenerationNumber(bool or_make_new_one);

  CryptoEngine* ce_;
  bool header_loaded_;
  int64_t master_generation_number_;
  std::vector<int64_t> generation_numbers_;
  std::vector<SessionContext*> sessions_;
  OldUsageTable* old_table_;

  friend class UsageTableEntry;
};

}  // namespace wvoec_ref

#endif  // OEMCRYPTO_USAGE_TABLE_REF_H_
