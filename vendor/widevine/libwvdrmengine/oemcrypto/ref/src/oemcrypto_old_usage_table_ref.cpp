// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
//  Reference implementation of OEMCrypto APIs
//
// This is from the v12 version of oemcrypto usage tables.  It is used for
// devices that upgrade from v12 to v13 in the field, and need to convert from
// the old type of usage table to the new.
#include "oemcrypto_old_usage_table_ref.h"

#include <string.h>
#include <time.h>

#include <string>
#include <vector>

#include <openssl/aes.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include "file_store.h"
#include "log.h"
#include "oemcrypto_engine_ref.h"
// TODO(fredgc): Setting the device files base bath is currently broken as
// wvcdm::Properties is no longer used by the reference code.
//#include "properties.h"
#include "pst_report.h"
#include "string_conversions.h"

namespace wvoec_ref {

OldUsageTableEntry::OldUsageTableEntry(OldUsageTable *old_usage_table,
                                       const std::vector<uint8_t> &pst_hash)
    : pst_hash_(pst_hash),
      old_usage_table_(old_usage_table),
      time_of_license_received_(
          old_usage_table_->ce_->RollbackCorrectedOfflineTime()),
      time_of_first_decrypt_(0),
      time_of_last_decrypt_(0),
      status_(kUnused) {}

OldUsageTableEntry::~OldUsageTableEntry() {}

OldUsageTableEntry::OldUsageTableEntry(OldUsageTable *old_usage_table,
                                       const OldStoredUsageEntry *buffer)
    : old_usage_table_(old_usage_table) {
  pst_hash_.assign(buffer->pst_hash, buffer->pst_hash + SHA256_DIGEST_LENGTH);
  time_of_license_received_ = buffer->time_of_license_received;
  time_of_first_decrypt_ = buffer->time_of_first_decrypt;
  time_of_last_decrypt_ = buffer->time_of_last_decrypt;
  status_ = buffer->status;
  mac_key_server_.assign(buffer->mac_key_server,
                         buffer->mac_key_server + wvoec::MAC_KEY_SIZE);
  mac_key_client_.assign(buffer->mac_key_client,
                         buffer->mac_key_client + wvoec::MAC_KEY_SIZE);
}

OldUsageTable::OldUsageTable(CryptoEngine *ce) {
  ce_ = ce;
  generation_ = 0;
  table_.clear();

  // Load saved table.
  wvcdm::FileSystem *file_system = ce->file_system();
  std::unique_ptr<wvcdm::File> file;
  std::string path;
  // Note: this path is OK for a real implementation, but using security level 1
  // would be better.
  // TODO(fredgc, jfore): Address how this property is presented to the ref.
  // For now, the path is empty.
  /*if (!Properties::GetDeviceFilesBasePath(kSecurityLevelL3, &path)) {
    LOGE("OldUsageTable: Unable to get base path");
    return;
  }*/
  std::string filename = path + "UsageTable.dat";
  if (!file_system->Exists(filename)) {
    return;
  }

  size_t file_size = file_system->FileSize(filename);
  std::vector<uint8_t> encrypted_buffer(file_size);
  std::vector<uint8_t> buffer(file_size);
  OldStoredUsageTable *stored_table =
      reinterpret_cast<OldStoredUsageTable *>(&buffer[0]);
  OldStoredUsageTable *encrypted_table =
      reinterpret_cast<OldStoredUsageTable *>(&encrypted_buffer[0]);

  file = file_system->Open(filename, wvcdm::FileSystem::kReadOnly);
  if (!file) {
    LOGE("OldUsageTable: File open failed: %s", path.c_str());
    return;
  }
  file->Read(reinterpret_cast<char *>(&encrypted_buffer[0]), file_size);

  // Verify the signature of the usage table file.

  // This should be encrypted and signed with a device specific key.
  // For the reference implementation, I'm just going to use the keybox key.
  const std::vector<uint8_t> &key = ce_->DeviceRootKey();
  if (key.empty()) {
    LOGE("OldUsageTable: DeviceRootKey is unexpectedly empty.");
    table_.clear();
    return;
  }

  uint8_t computed_signature[SHA256_DIGEST_LENGTH];
  unsigned int sig_length = sizeof(computed_signature);
  if (!HMAC(EVP_sha256(), &key[0], key.size(),
            &encrypted_buffer[SHA256_DIGEST_LENGTH],
            file_size - SHA256_DIGEST_LENGTH, computed_signature,
            &sig_length)) {
    LOGE("OldUsageTable: Could not recreate signature.");
    table_.clear();
    return;
  }
  if (memcmp(encrypted_table->signature, computed_signature, sig_length)) {
    LOGE("OldUsageTable: Invalid signature    given: %s",
         wvcdm::HexEncode(&encrypted_buffer[0], sig_length).c_str());
    LOGE("OldUsageTable: Invalid signature computed: %s",
         wvcdm::HexEncode(computed_signature, sig_length).c_str());
    table_.clear();
    return;
  }

  // Next, decrypt the table.
  uint8_t iv_buffer[wvoec::KEY_IV_SIZE];
  memcpy(iv_buffer, encrypted_table->iv, wvoec::KEY_IV_SIZE);
  AES_KEY aes_key;
  AES_set_decrypt_key(&key[0], 128, &aes_key);
  AES_cbc_encrypt(&encrypted_buffer[SHA256_DIGEST_LENGTH + wvoec::KEY_IV_SIZE],
                  &buffer[SHA256_DIGEST_LENGTH + wvoec::KEY_IV_SIZE],
                  file_size - SHA256_DIGEST_LENGTH - wvoec::KEY_IV_SIZE, &aes_key,
                  iv_buffer, AES_DECRYPT);

  // Next, read the generation number from a different location.
  // On a real implementation, you should NOT put the generation number in
  // a file in user space.  It should be stored in secure memory. For the
  // reference implementation, we'll just pretend this is secure.
  std::string filename2 = path + "GenerationNumber.dat";
  file = file_system->Open(filename2, wvcdm::FileSystem::kReadOnly);
  if (!file) {
    LOGE("OldUsageTable: File open failed: %s (clearing table)", path.c_str());
    generation_ = 0;
    table_.clear();
    return;
  }
  file->Read(reinterpret_cast<char *>(&generation_), sizeof(int64_t));
  if ((stored_table->generation > generation_ + 1) ||
      (stored_table->generation < generation_ - 1)) {
    LOGE("OldUsageTable: Rollback detected. Clearing Usage Table. %lx -> %lx",
         generation_, stored_table->generation);
    table_.clear();
    generation_ = 0;
    return;
  }

  // At this point, the stored table looks valid. We can load in all the
  // entries.
  for (uint64_t i = 0; i < stored_table->count; i++) {
    OldUsageTableEntry *entry =
        new OldUsageTableEntry(this, &stored_table->entries[i].entry);
    table_[entry->pst_hash()] = entry;
  }
}

OldUsageTableEntry *OldUsageTable::FindEntry(const std::vector<uint8_t> &pst) {
  std::unique_lock<std::mutex> lock(lock_);
  return FindEntryLocked(pst);
}

OldUsageTableEntry *OldUsageTable::FindEntryLocked(
    const std::vector<uint8_t> &pst) {
  std::vector<uint8_t> pst_hash;
  if (!ComputeHash(pst, pst_hash)) {
    LOGE("OldUsageTable: Could not compute hash of pst.");
    return NULL;
  }
  EntryMap::iterator it = table_.find(pst_hash);
  if (it == table_.end()) {
    return NULL;
  }
  return it->second;
}

OldUsageTableEntry *OldUsageTable::CreateEntry(
    const std::vector<uint8_t> &pst) {
  std::vector<uint8_t> pst_hash;
  if (!ComputeHash(pst, pst_hash)) {
    LOGE("OldUsageTable: Could not compute hash of pst.");
    return NULL;
  }
  OldUsageTableEntry *entry = new OldUsageTableEntry(this, pst_hash);
  std::unique_lock<std::mutex> lock(lock_);
  table_[pst_hash] = entry;
  return entry;
}

void OldUsageTable::Clear() {
  std::unique_lock<std::mutex> lock(lock_);
  for (EntryMap::iterator i = table_.begin(); i != table_.end(); ++i) {
    if (i->second) delete i->second;
  }
  table_.clear();
}

void OldUsageTable::DeleteFile(CryptoEngine *ce) {
  wvcdm::FileSystem *file_system = ce->file_system();
  std::string path;
  // Note: this path is OK for a real implementation, but using security level 1
  // would be better.
  // TODO(jfore): Address how this property is presented to the ref. For now,
  // the path is empty.
  /*if (!Properties::GetDeviceFilesBasePath(kSecurityLevelL3, &path)) {
    LOGE("OldUsageTable: Unable to get base path");
    return;
  }*/
  std::string filename = path + "UsageTable.dat";
  if (file_system->Exists(filename)) {
    if (!file_system->Remove(filename)) {
      LOGE("DeleteOldUsageTable: error removing file.");
    }
  }
}

bool OldUsageTable::ComputeHash(const std::vector<uint8_t> &pst,
                                std::vector<uint8_t> &pst_hash) {
  // The PST is not fixed size, and we have no promises that it is reasonbly
  // sized, so we compute a hash of it, and store that instead.
  pst_hash.resize(SHA256_DIGEST_LENGTH);
  SHA256_CTX context;
  if (!SHA256_Init(&context)) return false;
  if (!SHA256_Update(&context, &pst[0], pst.size())) return false;
  if (!SHA256_Final(&pst_hash[0], &context)) return false;
  return true;
}

}  // namespace wvoec_ref
