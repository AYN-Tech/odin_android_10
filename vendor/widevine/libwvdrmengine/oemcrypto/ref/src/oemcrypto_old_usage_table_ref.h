// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
//  Reference implementation of OEMCrypto APIs
//
// This is from the v12 version of oemcrypto usage tables.  It is used for
// devices that upgrade from v12 to v13 in the field, and need to convert from
// the old type of usage table to the new.
#ifndef OEMCRYPTO_OLD_USAGE_TABLE_REF_H_
#define OEMCRYPTO_OLD_USAGE_TABLE_REF_H_

#include <stdint.h>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "OEMCryptoCENC.h"
#include "oemcrypto_types.h"
#include "openssl/sha.h"

namespace wvoec_ref {

class CryptoEngine;
class OldUsageTable;
class UsagetTableEntry;

struct OldStoredUsageEntry {
  // To save disk space, we only store a hash of the pst.
  uint8_t pst_hash[SHA256_DIGEST_LENGTH];
  int64_t time_of_license_received;
  int64_t time_of_first_decrypt;
  int64_t time_of_last_decrypt;
  enum OEMCrypto_Usage_Entry_Status status;
  uint8_t mac_key_server[wvoec::MAC_KEY_SIZE];
  uint8_t mac_key_client[wvoec::MAC_KEY_SIZE];
};

typedef union {
  struct OldStoredUsageEntry entry;
  uint8_t padding[128];  // multiple of block size and bigger than entry size.
} AlignedOldStoredUsageEntry;

struct OldStoredUsageTable {
  uint8_t signature[SHA256_DIGEST_LENGTH];
  uint8_t iv[wvoec::KEY_IV_SIZE];
  int64_t generation;
  uint64_t count;
  AlignedOldStoredUsageEntry entries[];
};

class OldUsageTableEntry {
 public:
  OldUsageTableEntry(OldUsageTable *old_usage_table,
                     const std::vector<uint8_t> &pst_hash);
  OldUsageTableEntry(OldUsageTable *old_usage_table,
                     const OldStoredUsageEntry *buffer);
  ~OldUsageTableEntry();
  const std::vector<uint8_t> &pst_hash() const { return pst_hash_; }

 private:
  std::vector<uint8_t> pst_hash_;
  const OldUsageTable *old_usage_table_;
  int64_t time_of_license_received_;
  int64_t time_of_first_decrypt_;
  int64_t time_of_last_decrypt_;
  enum OEMCrypto_Usage_Entry_Status status_;
  std::vector<uint8_t> mac_key_server_;
  std::vector<uint8_t> mac_key_client_;

  friend class UsageTableEntry;
  friend class UsageTable;
};

class OldUsageTable {
 public:
  OldUsageTable(CryptoEngine *ce);
  ~OldUsageTable() { Clear(); }
  OldUsageTableEntry *FindEntry(const std::vector<uint8_t> &pst);
  OldUsageTableEntry *CreateEntry(const std::vector<uint8_t> &pst);
  void Clear();
  static void DeleteFile(CryptoEngine *ce);

 private:
  OldUsageTableEntry *FindEntryLocked(const std::vector<uint8_t> &pst);
  bool ComputeHash(const std::vector<uint8_t> &pst,
                   std::vector<uint8_t> &pst_hash);

  typedef std::map<std::vector<uint8_t>, OldUsageTableEntry *> EntryMap;
  EntryMap table_;
  std::mutex lock_;
  int64_t generation_;
  CryptoEngine *ce_;

  friend class OldUsageTableEntry;
};

}  // namespace wvoec_ref

#endif  // OEMCRYPTO_OLD_USAGE_TABLE_REF_H_
