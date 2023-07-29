// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
//  Reference implementation of OEMCrypto APIs
//
#include "oemcrypto_key_ref.h"
#include "oemcrypto_types.h"

#include <string.h>
#include <vector>

#include "log.h"

namespace wvoec_ref {

bool KeyControlBlock::Validate() {
  if (memcmp(verification_, "kctl", 4) &&  // original verification
      memcmp(verification_, "kc09", 4) &&  // add in version 9 api
      memcmp(verification_, "kc10", 4) &&  // add in version 10 api
      memcmp(verification_, "kc11", 4) &&  // add in version 11 api
      memcmp(verification_, "kc12", 4) &&  // add in version 12 api
      memcmp(verification_, "kc13", 4) &&  // add in version 13 api
      memcmp(verification_, "kc14", 4) &&  // add in version 14 api
      memcmp(verification_, "kc15", 4)) {  // add in version 15 api
    LOGE("KCB: BAD verification string: %4.4s", verification_);
    valid_ = false;
  } else {
    valid_ = true;
  }
  return valid_;
}

// This extracts 4 bytes in network byte order to a 32 bit integer in
// host byte order.
uint32_t KeyControlBlock::ExtractField(const std::vector<uint8_t>& str,
                                       int idx) {
  int bidx = idx * 4;
  uint32_t t = static_cast<unsigned char>(str[bidx]) << 24;
  t |= static_cast<unsigned char>(str[bidx + 1]) << 16;
  t |= static_cast<unsigned char>(str[bidx + 2]) << 8;
  t |= static_cast<unsigned char>(str[bidx + 3]);
  return t;
}

KeyControlBlock::KeyControlBlock(
    const std::vector<uint8_t>& key_control_string) {
  if (key_control_string.size() < wvoec::KEY_CONTROL_SIZE) {
    LOGE("KCB: BAD Size: %d (not %d)", key_control_string.size(),
         wvoec::KEY_CONTROL_SIZE);
    return;
  }

  memcpy(verification_, &key_control_string[0], 4);
  duration_ = ExtractField(key_control_string, 1);
  nonce_ = ExtractField(key_control_string, 2);
  control_bits_ = ExtractField(key_control_string, 3);
  Validate();
}

void Key::UpdateDuration(const KeyControlBlock& control) {
  control_.set_duration(control.duration());
}

void KeyControlBlock::RequireLocalDisplay() {
  // Set all bits to require HDCP Local Display Only.
  control_bits_ |= wvoec::kControlHDCPVersionMask;
}

}  // namespace wvoec_ref
