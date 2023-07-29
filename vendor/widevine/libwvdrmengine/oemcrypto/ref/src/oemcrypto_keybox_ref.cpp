// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
//  Reference implementation of OEMCrypto APIs
//
#include "oemcrypto_keybox_ref.h"

#include <string.h>

#include <cstdint>
#include <string>

#include "log.h"
#include "oemcrypto_types.h"
#include "platform.h"
#include "wvcrc32.h"

namespace wvoec_ref {

WvKeybox::WvKeybox() : loaded_(false) {}

KeyboxError WvKeybox::Validate() {
  if (!loaded_) {
    LOGE("[KEYBOX NOT LOADED]");
    return OTHER_ERROR;
  }
  if (strncmp(reinterpret_cast<char*>(magic_), "kbox", 4) != 0) {
    LOGE("[KEYBOX HAS BAD MAGIC]");
    return BAD_MAGIC;
  }
  uint32_t crc_computed;
  uint32_t crc_stored;
  uint8_t* crc_stored_bytes = (uint8_t*)&crc_stored;
  memcpy(crc_stored_bytes, crc_, sizeof(crc_));
  wvoec::WidevineKeybox keybox;
  memset(&keybox, 0, sizeof(keybox));
  memcpy(keybox.device_id_, &device_id_[0], device_id_.size());
  memcpy(keybox.device_key_, &device_key_[0], sizeof(keybox.device_key_));
  memcpy(keybox.data_, key_data_, sizeof(keybox.data_));
  memcpy(keybox.magic_, magic_, sizeof(keybox.magic_));

  crc_computed = ntohl(wvcrc32(reinterpret_cast<uint8_t*>(&keybox),
                               sizeof(keybox) - 4));  // Drop last 4 bytes.
  if (crc_computed != crc_stored) {
    LOGE("[KEYBOX CRC problem: computed = %08x,  stored = %08x]\n",
         crc_computed, crc_stored);
    return BAD_CRC;
  }
  return NO_ERROR;
}

bool WvKeybox::InstallKeybox(const uint8_t* buffer, size_t keyBoxLength) {
  if (keyBoxLength != 128) {
    return false;
  }
  const wvoec::WidevineKeybox* keybox =
      reinterpret_cast<const wvoec::WidevineKeybox*>(buffer);
  size_t device_id_length =
      strnlen(reinterpret_cast<const char*>(keybox->device_id_), 32);
  device_id_.assign(keybox->device_id_, keybox->device_id_ + device_id_length);
  device_key_.assign(keybox->device_key_,
                     keybox->device_key_ + sizeof(keybox->device_key_));
  memcpy(key_data_, keybox->data_, sizeof(keybox->data_));
  memcpy(magic_, keybox->magic_, sizeof(keybox->magic_));
  memcpy(crc_, keybox->crc_, sizeof(keybox->crc_));
  loaded_ = true;
  return true;
}

}  // namespace wvoec_ref
