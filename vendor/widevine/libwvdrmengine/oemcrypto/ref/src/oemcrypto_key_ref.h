// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
//  Reference implementation of OEMCrypto APIs
//
#ifndef OEMCRYPTO_KEY_REF_H_
#define OEMCRYPTO_KEY_REF_H_

#include <stdint.h>
#include <string>
#include <vector>

namespace wvoec_ref {

class KeyControlBlock {
 public:
  KeyControlBlock(const std::vector<uint8_t>& key_control_string);
  ~KeyControlBlock() {}

  bool Validate();
  void Invalidate() { valid_ = false; }

  bool valid() const { return valid_; }
  uint32_t duration() const { return duration_; }
  void set_duration(uint32_t duration) { duration_ = duration; }
  uint32_t nonce() const { return nonce_; }
  const char* verification() const { return verification_; }
  uint32_t control_bits() const { return control_bits_; }
  void RequireLocalDisplay();

 private:
  uint32_t ExtractField(const std::vector<uint8_t>& str, int idx);

  bool valid_;
  char verification_[4];
  uint32_t duration_;
  uint32_t nonce_;
  uint32_t control_bits_;
};

// AES-128 crypto key, or HMAC signing key.
class Key {
 public:
  Key(const Key& key)
      : value_(key.value_), control_(key.control_), ctr_mode_(key.ctr_mode_) {}
  Key(const std::vector<uint8_t>& key_string, const KeyControlBlock& control)
      : value_(key_string), control_(control), ctr_mode_(true){};

  virtual ~Key() {};
  void UpdateDuration(const KeyControlBlock& control);
  virtual const std::vector<uint8_t>& value() const { return value_; }
  const KeyControlBlock& control() const { return control_; }
  bool ctr_mode() const { return ctr_mode_; }
  void set_ctr_mode(bool ctr_mode) { ctr_mode_ = ctr_mode; }

 private:
  std::vector<uint8_t> value_;
  KeyControlBlock control_;
  bool ctr_mode_;
};

// AES-256 entitlement key. |Key| holds the entitlement key. |EntitlementKey|
// holds the content key.
class EntitlementKey : public Key {
 public:
  EntitlementKey(const Key& key) : Key(key) {}
  ~EntitlementKey() override {}
  const std::vector<uint8_t>& value() const override { return content_key_; }
  const std::vector<uint8_t>& content_key() { return content_key_; }
  const std::vector<uint8_t>& content_key_id() { return content_key_id_; }
  const std::vector<uint8_t>& entitlement_key() { return Key::value(); }
  bool SetContentKey(const std::vector<uint8_t>& content_key_id,
                     const std::vector<uint8_t>& content_key) {
    content_key_.assign(content_key.begin(), content_key.end());
    content_key_id_.assign(content_key_id.begin(), content_key_id.end());
    return true;
  }

 private:
  std::vector<uint8_t> content_key_;
  std::vector<uint8_t> content_key_id_;
};

}  // namespace wvoec_ref

#endif  // OEMCRYPTO_KEY_REF_H_
