// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
//  Reference implementation of OEMCrypto APIs
//
#include "oemcrypto_engine_ref.h"

#include <assert.h>
#include <chrono>
#include <string.h>
#include <algorithm>
#include <iostream>
#include <utility>
#include <vector>

#include <openssl/aes.h>
#include <openssl/err.h>

#include "keys.h"
#include "log.h"
#include "oemcrypto_key_ref.h"
#include "oemcrypto_rsa_key_shared.h"
#include "string_conversions.h"

namespace wvoec_ref {

// Note: The class CryptoEngine is configured at compile time by compiling in
// different device property files.  The methods in this file are generic to
// all configurations.  See the files oemcrypto_engine_device_properties*.cpp
// for methods that are configured for specific configurations.

CryptoEngine::CryptoEngine(std::unique_ptr<wvcdm::FileSystem>&& file_system)
    : root_of_trust_(config_provisioning_method()),
      file_system_(std::move(file_system)),
      usage_table_() {
  ERR_load_crypto_strings();
}

CryptoEngine::~CryptoEngine() {
  ERR_free_strings();
}

bool CryptoEngine::Initialize() {
  usage_table_.reset(MakeUsageTable());
  return true;
}

void CryptoEngine::Terminate() {
  std::unique_lock<std::mutex> lock(session_table_lock_);
  ActiveSessions::iterator it;
  for (it = sessions_.begin(); it != sessions_.end(); ++it) {
    delete it->second;
  }
  sessions_.clear();
  root_of_trust_.Clear();
}

SessionId CryptoEngine::OpenSession() {
  std::unique_lock<std::mutex> lock(session_table_lock_);
  static OEMCrypto_SESSION unique_id = 1;
  SessionId id = ++unique_id;
  sessions_[id] = MakeSession(id);
  return id;
}

SessionContext* CryptoEngine::MakeSession(SessionId sid) {
  return new SessionContext(this, sid, root_of_trust_.SharedRsaKey());
}

UsageTable* CryptoEngine::MakeUsageTable() { return new UsageTable(this); }

bool CryptoEngine::DestroySession(SessionId sid) {
  SessionContext* sctx = FindSession(sid);
  std::unique_lock<std::mutex> lock(session_table_lock_);
  if (sctx) {
    sessions_.erase(sid);
    delete sctx;
    return true;
  } else {
    return false;
  }
}

SessionContext* CryptoEngine::FindSession(SessionId sid) {
  std::unique_lock<std::mutex> lock(session_table_lock_);
  ActiveSessions::iterator it = sessions_.find(sid);
  if (it != sessions_.end()) {
    return it->second;
  }
  return NULL;
}

time_t CryptoEngine::OnlineTime() {
  // Use the monotonic clock for times that don't have to be stable across
  // device boots.
  std::chrono::steady_clock clock;
  return clock.now().time_since_epoch() / std::chrono::seconds(1);
}

time_t CryptoEngine::RollbackCorrectedOfflineTime() {
  struct TimeInfo {
    // The max time recorded through this function call.
    time_t previous_time;
    // If the wall time is rollbacked to before the previous_time, this member
    // is updated to reflect the offset.
    time_t rollback_offset;
    // Pad the struct so that TimeInfo is a multiple of 16.
    uint8_t padding[16 - (2 * sizeof(time_t)) % 16];
  };

  std::vector<uint8_t> encrypted_buffer(sizeof(TimeInfo));
  std::vector<uint8_t> clear_buffer(sizeof(TimeInfo));
  TimeInfo time_info;
  memset(&time_info, 0, sizeof(time_info));
  // Use the device key for encrypt/decrypt.
  const std::vector<uint8_t>& key = DeviceRootKey();

  std::unique_ptr<wvcdm::File> file;
  std::string path;
  // Note: this path is OK for a real implementation, but using security level 1
  // would be better.
  // TODO(fredgc, jfore): Address how this property is presented to the ref.
  // For now, the path is empty.
  /*if (!wvcdm::Properties::GetDeviceFilesBasePath(wvcdm::kSecurityLevelL3,
                                                 &path)) {
    LOGE("RollbackCorrectedOfflineTime: Unable to get base path");
  }*/
  std::string filename = path + "StoredUsageTime.dat";

  wvcdm::FileSystem* file_system = file_system_.get();
  if (file_system->Exists(filename)) {
    // Load time info from previous call to this function.
    file = file_system->Open(filename, wvcdm::FileSystem::kReadOnly);
    if (!file) {
      LOGE("RollbackCorrectedOfflineTime: File open failed: %s",
           filename.c_str());
      return time(NULL);
    }
    file->Read(reinterpret_cast<char*>(&encrypted_buffer[0]), sizeof(TimeInfo));
    // Decrypt the encrypted TimeInfo buffer.
    AES_KEY aes_key;
    AES_set_decrypt_key(&key[0], 128, &aes_key);
    std::vector<uint8_t> iv(wvoec::KEY_IV_SIZE, 0);
    AES_cbc_encrypt(&encrypted_buffer[0], &clear_buffer[0], sizeof(TimeInfo),
                    &aes_key, iv.data(), AES_DECRYPT);
    memcpy(&time_info, &clear_buffer[0], sizeof(TimeInfo));
  }

  time_t current_time;
  // Add any time offsets in the past to the current time.
  current_time = time(NULL) + time_info.rollback_offset;
  if (time_info.previous_time > current_time) {
    // Time has been rolled back.
    // Update the rollback offset.
    time_info.rollback_offset += time_info.previous_time - current_time;
    // Keep current time at previous recorded time.
    current_time = time_info.previous_time;
  }
  // The new previous_time will either stay the same or move forward.
  time_info.previous_time = current_time;

  // Copy updated data and encrypt the buffer.
  memcpy(&clear_buffer[0], &time_info, sizeof(TimeInfo));
  AES_KEY aes_key;
  AES_set_encrypt_key(&key[0], 128, &aes_key);
  std::vector<uint8_t> iv(wvoec::KEY_IV_SIZE, 0);
  AES_cbc_encrypt(&clear_buffer[0], &encrypted_buffer[0], sizeof(TimeInfo),
                  &aes_key, iv.data(), AES_ENCRYPT);

  // Write the encrypted buffer to disk.
  file = file_system->Open(
      filename, wvcdm::FileSystem::kCreate | wvcdm::FileSystem::kTruncate);
  if (!file) {
    LOGE("RollbackCorrectedOfflineTime: File open failed: %s",
         filename.c_str());
    return time(NULL);
  }
  file->Write(reinterpret_cast<char*>(&encrypted_buffer[0]), sizeof(TimeInfo));

  // Return time with offset.
  return current_time;
}

bool CryptoEngine::NonceCollision(uint32_t nonce) {
  for (const auto & session_pair :  sessions_) {
    const SessionContext* session = session_pair.second;
    if (session->NonceCollision(nonce)) return true;
  }
  return false;
}

OEMCrypto_HDCP_Capability CryptoEngine::config_current_hdcp_capability() {
  return config_local_display_only() ? HDCP_NO_DIGITAL_OUTPUT : HDCP_V1;
}

OEMCrypto_HDCP_Capability CryptoEngine::config_maximum_hdcp_capability() {
  return HDCP_NO_DIGITAL_OUTPUT;
}

OEMCryptoResult CryptoEngine::SetDestination(
    OEMCrypto_DestBufferDesc* out_description, size_t data_length,
    uint8_t subsample_flags) {
  size_t max_length = 0;
  switch (out_description->type) {
    case OEMCrypto_BufferType_Clear:
      destination_ = out_description->buffer.clear.address;
      max_length = out_description->buffer.clear.max_length;
      break;
    case OEMCrypto_BufferType_Secure:
      destination_ =
          reinterpret_cast<uint8_t*>(out_description->buffer.secure.handle) +
          out_description->buffer.secure.offset;
      max_length = out_description->buffer.secure.max_length -
                   out_description->buffer.secure.offset;
      break;
    case OEMCrypto_BufferType_Direct:
      // Direct buffer type is only used on some specialized devices where
      // oemcrypto has a direct connection to the screen buffer.  It is not,
      // for example, supported on Android.
      destination_ = NULL;
      break;
    default:
      return OEMCrypto_ERROR_INVALID_CONTEXT;
  }
  size_t max_allowed = max_output_size();
  if (max_allowed > 0 &&
      (max_allowed < max_length || max_allowed < data_length)) {
    LOGE("Output too large (or buffer too small).");
    return OEMCrypto_ERROR_OUTPUT_TOO_LARGE;
  }

  if (out_description->type != OEMCrypto_BufferType_Direct &&
      max_length < data_length) {
    LOGE("[SetDestination(): OEMCrypto_ERROR_SHORT_BUFFER]");
    return OEMCrypto_ERROR_SHORT_BUFFER;
  }
  adjust_destination(out_description, data_length, subsample_flags);
  if ((out_description->type != OEMCrypto_BufferType_Direct) &&
      (destination_ == NULL)) {
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }
  return OEMCrypto_SUCCESS;
}

}  // namespace wvoec_ref
