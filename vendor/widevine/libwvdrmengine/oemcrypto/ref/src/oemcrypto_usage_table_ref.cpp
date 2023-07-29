// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
//  Reference implementation of OEMCrypto APIs
//
#include "oemcrypto_usage_table_ref.h"

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
#include "oemcrypto_old_usage_table_ref.h"
// TODO(fredgc): Setting the device files base bath is currently broken as
// wvcdm::Properties is no longer used by the reference code.
//#include "properties.h"
#include "pst_report.h"
#include "string_conversions.h"

namespace wvoec_ref {
namespace {
const size_t kMagicLength = 8;
const char* kEntryVerification = "USEENTRY";
const char* kHeaderVerification = "USEHEADR";
// Offset into a signed block where we start encrypting. We need to
// skip the signature and the iv.
const size_t kEncryptionOffset = SHA256_DIGEST_LENGTH + SHA256_DIGEST_LENGTH;

// A structure that holds an usage entry and its signature.
struct SignedEntryBlock {
  uint8_t signature[SHA256_DIGEST_LENGTH];
  uint8_t iv[SHA256_DIGEST_LENGTH];
  uint8_t verification[kMagicLength];
  StoredUsageEntry data;
};

// This has the data in the header of constant size.  There is also an array
// of generation numbers.
struct SignedHeaderBlock {
  uint8_t signature[SHA256_DIGEST_LENGTH];
  uint8_t iv[SHA256_DIGEST_LENGTH];
  uint8_t verification[kMagicLength];
  int64_t master_generation;
  uint64_t count;
};

}  // namespace

UsageTableEntry::UsageTableEntry(UsageTable* table, uint32_t index,
                                 int64_t generation)
    : usage_table_(table), recent_decrypt_(false), forbid_report_(true) {
  memset(&data_, 0, sizeof(data_));
  data_.generation_number = generation;
  data_.index = index;
}

UsageTableEntry::~UsageTableEntry() { usage_table_->ReleaseEntry(data_.index); }

OEMCryptoResult UsageTableEntry::SetPST(const uint8_t* pst, size_t pst_length) {
  if (pst_length > kMaxPSTLength) return OEMCrypto_ERROR_BUFFER_TOO_LARGE;
  data_.pst_length = pst_length;
  if (!pst || !pst_length) return OEMCrypto_ERROR_INVALID_CONTEXT;
  memcpy(data_.pst, pst, pst_length);
  data_.time_of_license_received =
      usage_table_->ce_->RollbackCorrectedOfflineTime();
  return OEMCrypto_SUCCESS;
}

bool UsageTableEntry::VerifyPST(const uint8_t* pst, size_t pst_length) {
  if (pst_length > kMaxPSTLength) return false;
  if (data_.pst_length != pst_length) return false;
  if (!pst || !pst_length) return false;
  return 0 == memcmp(pst, data_.pst, pst_length);
}

bool UsageTableEntry::VerifyMacKeys(const std::vector<uint8_t>& server,
                                    const std::vector<uint8_t>& client) {
  return (server.size() == wvoec::MAC_KEY_SIZE) &&
         (client.size() == wvoec::MAC_KEY_SIZE) &&
         (0 == memcmp(&server[0], data_.mac_key_server, wvoec::MAC_KEY_SIZE)) &&
         (0 == memcmp(&client[0], data_.mac_key_client, wvoec::MAC_KEY_SIZE));
}

bool UsageTableEntry::SetMacKeys(const std::vector<uint8_t>& server,
                                 const std::vector<uint8_t>& client) {
  if ((server.size() != wvoec::MAC_KEY_SIZE) ||
      (client.size() != wvoec::MAC_KEY_SIZE))
    return false;
  memcpy(data_.mac_key_server, &server[0], wvoec::MAC_KEY_SIZE);
  memcpy(data_.mac_key_client, &client[0], wvoec::MAC_KEY_SIZE);
  return true;
}

bool UsageTableEntry::CheckForUse() {
  if (Inactive()) return false;
  recent_decrypt_ = true;
  if (data_.status == kUnused) {
    data_.status = kActive;
    data_.time_of_first_decrypt =
        usage_table_->ce_->RollbackCorrectedOfflineTime();
    data_.generation_number++;
    usage_table_->IncrementGeneration();
  }
  return true;
}

void UsageTableEntry::Deactivate(const std::vector<uint8_t>& pst) {
  if (data_.status == kUnused) {
    data_.status = kInactiveUnused;
  } else if (data_.status == kActive) {
    data_.status = kInactiveUsed;
  }
  forbid_report_ = true;
  data_.generation_number++;
  usage_table_->IncrementGeneration();
}

OEMCryptoResult UsageTableEntry::ReportUsage(const std::vector<uint8_t>& pst,
                                             uint8_t* buffer,
                                             size_t* buffer_length) {
  if (forbid_report_) return OEMCrypto_ERROR_ENTRY_NEEDS_UPDATE;
  if (recent_decrypt_) return OEMCrypto_ERROR_ENTRY_NEEDS_UPDATE;
  if (pst.size() == 0 || pst.size() > kMaxPSTLength ||
      pst.size() != data_.pst_length) {
    LOGE("ReportUsage: bad pst length = %d, should be %d.", pst.size(),
         data_.pst_length);
    return OEMCrypto_ERROR_WRONG_PST;
  }
  if (memcmp(&pst[0], data_.pst, data_.pst_length)) {
    LOGE("ReportUsage: wrong pst %s, should be %s.", wvcdm::b2a_hex(pst).c_str(),
         wvcdm::HexEncode(data_.pst, data_.pst_length).c_str());
    return OEMCrypto_ERROR_WRONG_PST;
  }
  size_t length_needed = wvcdm::Unpacked_PST_Report::report_size(pst.size());
  if (*buffer_length < length_needed) {
    *buffer_length = length_needed;
    return OEMCrypto_ERROR_SHORT_BUFFER;
  }
  if (!buffer) {
    LOGE("ReportUsage: buffer was null pointer.");
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }
  wvcdm::Unpacked_PST_Report pst_report(buffer);
  int64_t now = usage_table_->ce_->RollbackCorrectedOfflineTime();
  pst_report.set_seconds_since_license_received(now -
                                                data_.time_of_license_received);
  pst_report.set_seconds_since_first_decrypt(now - data_.time_of_first_decrypt);
  pst_report.set_seconds_since_last_decrypt(now - data_.time_of_last_decrypt);
  pst_report.set_status(data_.status);
  pst_report.set_clock_security_level(kSecureTimer);
  pst_report.set_pst_length(data_.pst_length);
  memcpy(pst_report.pst(), data_.pst, data_.pst_length);
  unsigned int md_len = SHA_DIGEST_LENGTH;
  if (!HMAC(EVP_sha1(), data_.mac_key_client, wvoec::MAC_KEY_SIZE,
            buffer + SHA_DIGEST_LENGTH, length_needed - SHA_DIGEST_LENGTH,
            pst_report.signature(), &md_len)) {
    LOGE("ReportUsage: could not compute signature.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  return OEMCrypto_SUCCESS;
}

void UsageTableEntry::UpdateAndIncrement() {
  if (recent_decrypt_) {
    data_.time_of_last_decrypt =
        usage_table_->ce_->RollbackCorrectedOfflineTime();
    recent_decrypt_ = false;
  }
  data_.generation_number++;
  usage_table_->IncrementGeneration();
  forbid_report_ = false;
}

OEMCryptoResult UsageTableEntry::SaveData(CryptoEngine* ce,
                                          SessionContext* session,
                                          uint8_t* signed_buffer,
                                          size_t buffer_size) {
  // buffer_size was determined by calling function.
  if (buffer_size != SignedEntrySize()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  std::vector<uint8_t> clear_buffer(buffer_size);
  memset(&clear_buffer[0], 0, buffer_size);
  memset(signed_buffer, 0, buffer_size);
  SignedEntryBlock* clear =
      reinterpret_cast<SignedEntryBlock*>(&clear_buffer[0]);
  SignedEntryBlock* encrypted =
      reinterpret_cast<SignedEntryBlock*>(signed_buffer);
  clear->data = this->data_;  // Copy the current data.
  memcpy(clear->verification, kEntryVerification, kMagicLength);

  // This should be encrypted and signed with a device specific key.
  // For the reference implementation, I'm just going to use the keybox key.
  const std::vector<uint8_t>& key = ce->DeviceRootKey();
  if (key.empty()) {
    LOGE("SaveUsageEntry: DeviceRootKey is unexpectedly empty.");
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }

  // Encrypt the entry.
  RAND_bytes(encrypted->iv, wvoec::KEY_IV_SIZE);
  uint8_t iv_buffer[wvoec::KEY_IV_SIZE];  // working iv buffer.
  memcpy(iv_buffer, encrypted->iv, wvoec::KEY_IV_SIZE);
  AES_KEY aes_key;
  AES_set_encrypt_key(&key[0], 128, &aes_key);
  AES_cbc_encrypt(
      &clear_buffer[kEncryptionOffset], &signed_buffer[kEncryptionOffset],
      buffer_size - kEncryptionOffset, &aes_key, iv_buffer, AES_ENCRYPT);

  // Sign the entry.
  unsigned int sig_length = SHA256_DIGEST_LENGTH;
  if (!HMAC(EVP_sha256(), &key[0], key.size(),
            &signed_buffer[SHA256_DIGEST_LENGTH],
            buffer_size - SHA256_DIGEST_LENGTH, encrypted->signature,
            &sig_length)) {
    LOGE("SaveUsageEntry: Could not sign entry.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  return OEMCrypto_SUCCESS;
}

OEMCryptoResult UsageTableEntry::LoadData(CryptoEngine* ce, uint32_t index,
                                          const std::vector<uint8_t>& buffer) {
  if (buffer.size() < SignedEntrySize()) return OEMCrypto_ERROR_SHORT_BUFFER;
  if (buffer.size() > SignedEntrySize())
    LOGW("LoadUsageTableEntry: buffer is large.  %d > %d", buffer.size(),
         SignedEntrySize());
  std::vector<uint8_t> clear_buffer(buffer.size());
  SignedEntryBlock* clear =
      reinterpret_cast<SignedEntryBlock*>(&clear_buffer[0]);
  const SignedEntryBlock* encrypted =
      reinterpret_cast<const SignedEntryBlock*>(&buffer[0]);

  // This should be encrypted and signed with a device specific key.
  // For the reference implementation, I'm just going to use the keybox key.
  const std::vector<uint8_t>& key = ce->DeviceRootKey();
  if (key.empty()) {
    LOGE("LoadUsageEntry: DeviceRootKey is unexpectedly empty.");
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }

  // Verify the signature of the usage entry. Sign encrypted into clear buffer.
  unsigned int sig_length = SHA256_DIGEST_LENGTH;
  if (!HMAC(EVP_sha256(), &key[0], key.size(), &buffer[SHA256_DIGEST_LENGTH],
            buffer.size() - SHA256_DIGEST_LENGTH, clear->signature,
            &sig_length)) {
    LOGE("LoadUsageEntry: Could not sign entry.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (memcmp(clear->signature, encrypted->signature, SHA256_DIGEST_LENGTH)) {
    LOGE("LoadUsageEntry: Signature did not match.");
    LOGE("LoadUsageEntry: Invalid signature    given: %s",
         wvcdm::HexEncode(encrypted->signature, sig_length).c_str());
    LOGE("LoadUsageEntry: Invalid signature computed: %s",
         wvcdm::HexEncode(clear->signature, sig_length).c_str());
    return OEMCrypto_ERROR_SIGNATURE_FAILURE;
  }

  // Next, decrypt the entry.
  uint8_t iv_buffer[wvoec::KEY_IV_SIZE];
  memcpy(iv_buffer, encrypted->iv, wvoec::KEY_IV_SIZE);
  AES_KEY aes_key;
  AES_set_decrypt_key(&key[0], 128, &aes_key);
  AES_cbc_encrypt(&buffer[kEncryptionOffset], &clear_buffer[kEncryptionOffset],
                  buffer.size() - kEncryptionOffset, &aes_key, iv_buffer,
                  AES_DECRYPT);

  // Check the verification string is correct.
  if (memcmp(kEntryVerification, clear->verification, kMagicLength)) {
    LOGE("LoadUsageEntry: Invalid magic: %s=%8.8s expected: %s=%8.8s",
         wvcdm::HexEncode(clear->verification, kMagicLength).c_str(),
         clear->verification,
         wvcdm::HexEncode(reinterpret_cast<const uint8_t*>(kEntryVerification),
                   kMagicLength)
             .c_str(),
         reinterpret_cast<const uint8_t*>(kEntryVerification));
    return OEMCrypto_ERROR_BAD_MAGIC;
  }

  // Check that the index is correct.
  if (index != clear->data.index) {
    LOGE("LoadUsageEntry: entry says index is %d, not %d", clear->data.index,
         index);
    return OEMCrypto_ERROR_INVALID_SESSION;
  }
  if (clear->data.status > kInactiveUnused) {
    LOGE("LoadUsageEntry: entry has bad status %d", clear->data.status);
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  this->data_ = clear->data;
  return OEMCrypto_SUCCESS;
}

OEMCryptoResult UsageTableEntry::CopyOldUsageEntry(
    const std::vector<uint8_t>& pst) {
  OldUsageTableEntry* old_entry = usage_table_->FindOldUsageEntry(pst);
  if (!old_entry) return OEMCrypto_ERROR_WRONG_PST;
  data_.time_of_license_received = old_entry->time_of_license_received_;
  data_.time_of_first_decrypt = old_entry->time_of_first_decrypt_;
  data_.time_of_last_decrypt = old_entry->time_of_last_decrypt_;
  data_.status = old_entry->status_;
  if (old_entry->mac_key_server_.size() != wvoec::MAC_KEY_SIZE) {
    LOGE("CopyOldEntry: Old entry has bad server mac key.");
  } else {
    memcpy(data_.mac_key_server, &(old_entry->mac_key_server_[0]),
           wvoec::MAC_KEY_SIZE);
  }
  if (old_entry->mac_key_client_.size() != wvoec::MAC_KEY_SIZE) {
    LOGE("CopyOldEntry: Old entry has bad client mac key.");
  } else {
    memcpy(data_.mac_key_client, &(old_entry->mac_key_client_[0]),
           wvoec::MAC_KEY_SIZE);
  }
  if (pst.size() > kMaxPSTLength) {
    LOGE("CopyOldEntry: PST Length was too large.  Truncating.");
    data_.pst_length = kMaxPSTLength;
  } else {
    data_.pst_length = pst.size();
  }
  memcpy(data_.pst, pst.data(), data_.pst_length);
  data_.pst[data_.pst_length] = '\0';
  return OEMCrypto_SUCCESS;
}

size_t UsageTableEntry::SignedEntrySize() {
  size_t base = sizeof(SignedEntryBlock);
  // round up to make even number of blocks:
  size_t blocks = (base - 1) / wvoec::KEY_IV_SIZE + 1;
  return blocks * wvoec::KEY_IV_SIZE;
}

UsageTable::~UsageTable() {
  if (old_table_) {
    delete old_table_;
    old_table_ = NULL;
  }
}

size_t UsageTable::SignedHeaderSize(size_t count) {
  size_t base = sizeof(SignedHeaderBlock) + count * sizeof(int64_t);
  // round up to make even number of blocks:
  size_t blocks = (base - 1) / wvoec::KEY_IV_SIZE + 1;
  return blocks * wvoec::KEY_IV_SIZE;
}

OEMCryptoResult UsageTable::UpdateUsageEntry(SessionContext* session,
                                             UsageTableEntry* entry,
                                             uint8_t* header_buffer,
                                             size_t* header_buffer_length,
                                             uint8_t* entry_buffer,
                                             size_t* entry_buffer_length) {
  size_t signed_header_size = SignedHeaderSize(generation_numbers_.size());
  if (*entry_buffer_length < UsageTableEntry::SignedEntrySize() ||
      *header_buffer_length < signed_header_size) {
    *entry_buffer_length = UsageTableEntry::SignedEntrySize();
    *header_buffer_length = signed_header_size;
    return OEMCrypto_ERROR_SHORT_BUFFER;
  }
  *entry_buffer_length = UsageTableEntry::SignedEntrySize();
  *header_buffer_length = signed_header_size;
  if ((!header_buffer) || (!entry_buffer))
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  entry->UpdateAndIncrement();
  generation_numbers_[entry->index()] = entry->generation_number();
  OEMCryptoResult result =
      entry->SaveData(ce_, session, entry_buffer, *entry_buffer_length);
  if (result != OEMCrypto_SUCCESS) return result;
  result = SaveUsageTableHeader(header_buffer, *header_buffer_length);
  return result;
}

UsageTableEntry* UsageTable::MakeEntry(uint32_t index) {
  return new UsageTableEntry(this, index, master_generation_number_);
}

OEMCryptoResult UsageTable::CreateNewUsageEntry(SessionContext* session,
                                                UsageTableEntry** entry,
                                                uint32_t* usage_entry_number) {
  if (!header_loaded_) {
    LOGE("CreateNewUsageEntry: Header not loaded.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (!entry) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  if (!usage_entry_number) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  uint32_t index = generation_numbers_.size();
  size_t max = ce_->max_usage_table_size();
  if (max > 0 && index >= max) {
    LOGE("Too many usage entries: %d/%d", index, max);
    return OEMCrypto_ERROR_INSUFFICIENT_RESOURCES;
  }
  UsageTableEntry* new_entry = MakeEntry(index);
  generation_numbers_.push_back(master_generation_number_);
  sessions_.push_back(session);
  master_generation_number_++;
  *entry = new_entry;
  *usage_entry_number = index;
  return OEMCrypto_SUCCESS;
}

OEMCryptoResult UsageTable::LoadUsageEntry(SessionContext* session,
                                           UsageTableEntry** entry,
                                           uint32_t index,
                                           const std::vector<uint8_t>& buffer) {
  if (!header_loaded_) {
    LOGE("CreateNewUsageEntry: Header not loaded.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (!entry) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  if (index >= generation_numbers_.size())
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  if (sessions_[index]) {
    LOGE("LoadUsageEntry: index %d used by other session.", index);
    return OEMCrypto_ERROR_INVALID_SESSION;
  }
  size_t max = ce_->max_usage_table_size();
  if (max > 0 && index >= max) {
    LOGE("Too many usage entries: %d/%d", index, max);
    return OEMCrypto_ERROR_INSUFFICIENT_RESOURCES;
  }
  UsageTableEntry* new_entry = MakeEntry(index);

  OEMCryptoResult status = new_entry->LoadData(ce_, index, buffer);
  if (status != OEMCrypto_SUCCESS) {
    delete new_entry;
    return status;
  }
  if (new_entry->generation_number() != generation_numbers_[index]) {
    LOGE("Generation SKEW: %ld -> %ld", new_entry->generation_number(),
         generation_numbers_[index]);
    if ((new_entry->generation_number() + 1 < generation_numbers_[index]) ||
        (new_entry->generation_number() - 1 > generation_numbers_[index])) {
      delete new_entry;
      return OEMCrypto_ERROR_GENERATION_SKEW;
    }
    status = OEMCrypto_WARNING_GENERATION_SKEW;
  }
  sessions_[index] = session;
  *entry = new_entry;
  return status;
}

OEMCryptoResult UsageTable::ShrinkUsageTableHeader(
    uint32_t new_table_size, uint8_t* header_buffer,
    size_t* header_buffer_length) {
  if (new_table_size > generation_numbers_.size()) {
    LOGE("OEMCrypto_ShrinkUsageTableHeader: %d > %zd.", new_table_size,
         generation_numbers_.size());
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  size_t signed_header_size = SignedHeaderSize(new_table_size);
  if (*header_buffer_length < signed_header_size) {
    *header_buffer_length = signed_header_size;
    return OEMCrypto_ERROR_SHORT_BUFFER;
  }
  *header_buffer_length = signed_header_size;
  if (!header_buffer) {
    LOGE("OEMCrypto_ShrinkUsageTableHeader: buffer null.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  for (size_t i = new_table_size; i < sessions_.size(); ++i) {
    if (sessions_[i]) {
      LOGE("ShrinkUsageTableHeader: session open for %d", i);
      return OEMCrypto_ERROR_ENTRY_IN_USE;
    }
  }
  generation_numbers_.resize(new_table_size);
  sessions_.resize(new_table_size);
  master_generation_number_++;
  return SaveUsageTableHeader(header_buffer, *header_buffer_length);
}

OEMCryptoResult UsageTable::SaveUsageTableHeader(uint8_t* signed_buffer,
                                                 size_t buffer_size) {
  if (!SaveGenerationNumber()) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  size_t count = generation_numbers_.size();
  // buffer_size was determined by calling function.
  if (buffer_size != SignedHeaderSize(count))
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  std::vector<uint8_t> clear_buffer(buffer_size);
  memset(&clear_buffer[0], 0, buffer_size);
  memset(signed_buffer, 0, buffer_size);
  SignedHeaderBlock* clear =
      reinterpret_cast<SignedHeaderBlock*>(&clear_buffer[0]);
  SignedHeaderBlock* encrypted =
      reinterpret_cast<SignedHeaderBlock*>(signed_buffer);

  // Pack the clear data into the clear buffer.
  memcpy(clear->verification, kHeaderVerification, kMagicLength);
  clear->master_generation = master_generation_number_;
  clear->count = count;
  // This points to the variable size part of the buffer.
  int64_t* stored_generations =
      reinterpret_cast<int64_t*>(&clear_buffer[sizeof(SignedHeaderBlock)]);
  std::copy(generation_numbers_.begin(), generation_numbers_.begin() + count,
            stored_generations);

  // This should be encrypted and signed with a device specific key.
  // For the reference implementation, I'm just going to use the keybox key.
  const std::vector<uint8_t>& key = ce_->DeviceRootKey();
  if (key.empty()) {
    LOGE("SaveUsageTableHeader: DeviceRootKey is unexpectedly empty.");
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }

  // Encrypt the entry.
  RAND_bytes(encrypted->iv, wvoec::KEY_IV_SIZE);
  uint8_t iv_buffer[wvoec::KEY_IV_SIZE];  // working iv buffer.
  memcpy(iv_buffer, encrypted->iv, wvoec::KEY_IV_SIZE);
  AES_KEY aes_key;
  AES_set_encrypt_key(&key[0], 128, &aes_key);
  AES_cbc_encrypt(
      &clear_buffer[kEncryptionOffset], &signed_buffer[kEncryptionOffset],
      buffer_size - kEncryptionOffset, &aes_key, iv_buffer, AES_ENCRYPT);

  // Sign the entry.
  unsigned int sig_length = SHA256_DIGEST_LENGTH;
  if (!HMAC(EVP_sha256(), &key[0], key.size(),
            &signed_buffer[SHA256_DIGEST_LENGTH],
            buffer_size - SHA256_DIGEST_LENGTH, encrypted->signature,
            &sig_length)) {
    LOGE("SaveUsageHeader: Could not sign entry.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  return OEMCrypto_SUCCESS;
}

OEMCryptoResult UsageTable::LoadUsageTableHeader(
    const std::vector<uint8_t>& buffer) {
  if (!LoadGenerationNumber(false)) return OEMCrypto_ERROR_UNKNOWN_FAILURE;

  if (buffer.size() < SignedHeaderSize(0)) return OEMCrypto_ERROR_SHORT_BUFFER;
  size_t max = ce_->max_usage_table_size();
  if (max > 0 && buffer.size() > SignedHeaderSize(max)) {
    LOGE("Header too big: %zd bytes/%zd bytes",
         buffer.size(), SignedHeaderSize(max));
    return OEMCrypto_ERROR_INSUFFICIENT_RESOURCES;
  }
  std::vector<uint8_t> clear_buffer(buffer.size());
  SignedHeaderBlock* clear =
      reinterpret_cast<SignedHeaderBlock*>(&clear_buffer[0]);
  const SignedHeaderBlock* encrypted =
      reinterpret_cast<const SignedHeaderBlock*>(&buffer[0]);

  // This should be encrypted and signed with a device specific key.
  // For the reference implementation, I'm just going to use the keybox key.
  const std::vector<uint8_t>& key = ce_->DeviceRootKey();
  if (key.empty()) {
    LOGE("LoadUsageTableHeader: DeviceRootKey is unexpectedly empty.");
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }

  // Verify the signature of the usage entry. Sign encrypted into clear buffer.
  unsigned int sig_length = SHA256_DIGEST_LENGTH;
  if (!HMAC(EVP_sha256(), &key[0], key.size(), &buffer[SHA256_DIGEST_LENGTH],
            buffer.size() - SHA256_DIGEST_LENGTH, clear->signature,
            &sig_length)) {
    LOGE("LoadUsageTableHeader: Could not sign entry.");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  if (memcmp(clear->signature, encrypted->signature, SHA256_DIGEST_LENGTH)) {
    LOGE("LoadUsageTableHeader: Signature did not match.");
    LOGE("LoadUsageTableHeader: Invalid signature    given: %s",
         wvcdm::HexEncode(encrypted->signature, sig_length).c_str());
    LOGE("LoadUsageTableHeader: Invalid signature computed: %s",
         wvcdm::HexEncode(clear->signature, sig_length).c_str());
    return OEMCrypto_ERROR_SIGNATURE_FAILURE;
  }

  // Next, decrypt the entry.
  uint8_t iv_buffer[wvoec::KEY_IV_SIZE];
  memcpy(iv_buffer, encrypted->iv, wvoec::KEY_IV_SIZE);
  AES_KEY aes_key;
  AES_set_decrypt_key(&key[0], 128, &aes_key);
  AES_cbc_encrypt(&buffer[kEncryptionOffset], &clear_buffer[kEncryptionOffset],
                  buffer.size() - kEncryptionOffset, &aes_key, iv_buffer,
                  AES_DECRYPT);

  // Check the verification string is correct.
  if (memcmp(kHeaderVerification, clear->verification, kMagicLength)) {
    LOGE("LoadUsageTableHeader: Invalid magic: %s=%8.8s expected: %s=%8.8s",
         wvcdm::HexEncode(clear->verification, kMagicLength).c_str(),
         clear->verification,
         wvcdm::HexEncode(reinterpret_cast<const uint8_t*>(kHeaderVerification),
                   kMagicLength)
             .c_str(),
         reinterpret_cast<const uint8_t*>(kHeaderVerification));
    return OEMCrypto_ERROR_BAD_MAGIC;
  }

  // Check that size is correct, now that we know what it should be.
  if (buffer.size() < SignedHeaderSize(clear->count)) {
    return OEMCrypto_ERROR_SHORT_BUFFER;
  }
  if (buffer.size() > SignedHeaderSize(clear->count)) {
    LOGW("LoadUsageTableHeader: buffer is large.  %d > %d", buffer.size(),
         SignedHeaderSize(clear->count));
  }

  OEMCryptoResult status = OEMCrypto_SUCCESS;
  if (clear->master_generation != master_generation_number_) {
    LOGE("Generation SKEW: %ld -> %ld", clear->master_generation,
         master_generation_number_);
    if ((clear->master_generation + 1 < master_generation_number_) ||
        (clear->master_generation - 1 > master_generation_number_)) {
      return OEMCrypto_ERROR_GENERATION_SKEW;
    }
    status = OEMCrypto_WARNING_GENERATION_SKEW;
  }
  int64_t* stored_generations =
      reinterpret_cast<int64_t*>(&clear_buffer[0] + sizeof(SignedHeaderBlock));
  generation_numbers_.assign(stored_generations,
                             stored_generations + clear->count);
  sessions_.clear();
  sessions_.resize(clear->count);
  header_loaded_ = true;
  return status;
}

OEMCryptoResult UsageTable::MoveEntry(UsageTableEntry* entry,
                                      uint32_t new_index) {
  if (new_index >= generation_numbers_.size()) {
    LOGE("MoveEntry: index beyond end of usage table %d >= %d", new_index,
         generation_numbers_.size());
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }
  if (sessions_[new_index]) {
    LOGE("MoveEntry: session open for %d", new_index);
    return OEMCrypto_ERROR_ENTRY_IN_USE;
  }
  if (!entry) {
    LOGE("MoveEntry: null entry");
    return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  }
  sessions_[new_index] = sessions_[entry->index()];
  sessions_[entry->index()] = 0;

  entry->set_index(new_index);
  generation_numbers_[new_index] = master_generation_number_;
  entry->set_generation_number(master_generation_number_);
  master_generation_number_++;
  return OEMCrypto_SUCCESS;
}

void UsageTable::IncrementGeneration() {
  master_generation_number_++;
  SaveGenerationNumber();
}

bool UsageTable::SaveGenerationNumber() {
  wvcdm::FileSystem* file_system = ce_->file_system();
  std::string path;
  // Note: this path is OK for a real implementation, but using security level 1
  // would be better.
  // TODO(jfore, rfrias): Address how this property is presented to the ref.
  // For now, the path is empty.
  /*if (!Properties::GetDeviceFilesBasePath(kSecurityLevelL3,
                                                 &path)) {
    LOGE("UsageTable: Unable to get base path");
    return false;
  }*/
  // On a real implementation, you should NOT put the generation number in
  // a file in user space.  It should be stored in secure memory.
  std::string filename = path + "GenerationNumber.dat";
  auto file = file_system->Open(
      filename, wvcdm::FileSystem::kCreate | wvcdm::FileSystem::kTruncate);
  if (!file) {
    LOGE("UsageTable: File open failed: %s", path.c_str());
    return false;
  }
  file->Write(reinterpret_cast<char*>(&master_generation_number_),
              sizeof(int64_t));
  return true;
}

bool UsageTable::LoadGenerationNumber(bool or_make_new_one) {
  wvcdm::FileSystem* file_system = ce_->file_system();
  std::string path;
  // Note: this path is OK for a real implementation, but using security level 1
  // would be better.
  // TODO(jfore, rfrias): Address how this property is presented to the ref.
  // For now, the path is empty.
  /*if (!Properties::GetDeviceFilesBasePath(kSecurityLevelL3,
                                                 &path)) {
    LOGE("UsageTable: Unable to get base path");
    return false;
  }*/
  // On a real implementation, you should NOT put the generation number in
  // a file in user space.  It should be stored in secure memory.
  std::string filename = path + "GenerationNumber.dat";
  auto file = file_system->Open(filename, wvcdm::FileSystem::kReadOnly);
  if (!file) {
    if (or_make_new_one) {
      RAND_bytes(reinterpret_cast<uint8_t*>(&master_generation_number_),
                 sizeof(int64_t));
      return true;
    }
    LOGE("UsageTable: File open failed: %s (clearing table)", path.c_str());
    master_generation_number_ = 0;
    return false;
  }
  file->Read(reinterpret_cast<char*>(&master_generation_number_),
             sizeof(int64_t));
  return true;
}

OEMCryptoResult UsageTable::CreateUsageTableHeader(
    uint8_t* header_buffer, size_t* header_buffer_length) {
  size_t signed_header_size = SignedHeaderSize(0);
  if (*header_buffer_length < signed_header_size) {
    *header_buffer_length = signed_header_size;
    return OEMCrypto_ERROR_SHORT_BUFFER;
  }
  *header_buffer_length = signed_header_size;
  if (!LoadGenerationNumber(true)) return OEMCrypto_ERROR_UNKNOWN_FAILURE;
  // Make sure there are no entries that are currently tied to an open session.
  for (size_t i = 0; i < sessions_.size(); ++i) {
    if (sessions_[i] != NULL) {
      LOGE("CreateUsageTableHeader: index %d used by session.", i);
      return OEMCrypto_ERROR_INVALID_SESSION;
    }
  }
  sessions_.clear();
  generation_numbers_.clear();
  header_loaded_ = true;
  return SaveUsageTableHeader(header_buffer, *header_buffer_length);
}

OldUsageTableEntry* UsageTable::FindOldUsageEntry(
    const std::vector<uint8_t>& pst) {
  if (!old_table_) old_table_ = new OldUsageTable(ce_);
  return old_table_->FindEntry(pst);
}

OEMCryptoResult UsageTable::DeleteOldUsageTable() {
  if (old_table_) {
    old_table_->Clear();
    delete old_table_;
    old_table_ = NULL;
  }
  OldUsageTable::DeleteFile(ce_);
  return OEMCrypto_SUCCESS;
}

OEMCryptoResult UsageTable::CreateOldUsageEntry(
    uint64_t time_since_license_received, uint64_t time_since_first_decrypt,
    uint64_t time_since_last_decrypt, OEMCrypto_Usage_Entry_Status status,
    uint8_t* server_mac_key, uint8_t* client_mac_key, const uint8_t* pst,
    size_t pst_length) {
  if (!old_table_) old_table_ = new OldUsageTable(ce_);
  std::vector<uint8_t> pstv(pst, pst + pst_length);
  OldUsageTableEntry* old_entry = old_table_->CreateEntry(pstv);

  int64_t now = ce_->RollbackCorrectedOfflineTime();
  old_entry->time_of_license_received_ = now - time_since_license_received;
  old_entry->time_of_first_decrypt_ = now - time_since_first_decrypt;
  old_entry->time_of_last_decrypt_ = now - time_since_last_decrypt;
  old_entry->status_ = status;
  old_entry->mac_key_server_.assign(server_mac_key,
                                    server_mac_key + wvoec::MAC_KEY_SIZE);
  old_entry->mac_key_client_.assign(client_mac_key,
                                    client_mac_key + wvoec::MAC_KEY_SIZE);
  return OEMCrypto_SUCCESS;
}

}  // namespace wvoec_ref
