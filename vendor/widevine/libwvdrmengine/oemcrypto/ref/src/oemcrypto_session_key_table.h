// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
//  Reference implementation of OEMCrypto APIs
//
#ifndef REF_OEMCRYPTO_SESSION_KEY_TABLE_H_
#define REF_OEMCRYPTO_SESSION_KEY_TABLE_H_

#include <stdint.h>
#include <map>
#include <vector>

#include "disallow_copy_and_assign.h"
#include "oemcrypto_key_ref.h"
#include "oemcrypto_types.h"

namespace wvoec_ref {

class SessionContext;
class CryptoEngine;
class UsageTable;
class UsageTableEntry;

typedef std::vector<uint8_t> KeyId;
typedef std::map<KeyId, Key*> KeyMap;
typedef std::map<KeyId, EntitlementKey*> EntitlementKeyMap;

// SessionKeyTable holds the keys for the current session
class SessionKeyTable {
 public:
  SessionKeyTable() {}
  ~SessionKeyTable();

  bool Insert(const KeyId key_id, const Key& key_data);
  Key* Find(const KeyId key_id);
  void Remove(const KeyId key_id);
  void UpdateDuration(const KeyControlBlock& control);
  size_t size() const { return keys_.size(); }

 private:
  KeyMap keys_;

  CORE_DISALLOW_COPY_AND_ASSIGN(SessionKeyTable);
};

class EntitlementKeyTable {
  typedef std::map<KeyId, KeyId> ContentIdToEntitlementIdMap;

 public:
  EntitlementKeyTable() {}
  ~EntitlementKeyTable() {}
  bool Insert(const KeyId key_id, const Key& key_data);
  Key* Find(const KeyId key_id);
  void Remove(const KeyId key_id);
  void UpdateDuration(const KeyControlBlock& control);
  size_t size() const { return contentid_to_entitlementid_.size(); }
  bool SetContentKey(const KeyId& entitlement_id, const KeyId& content_key_id,
                     const std::vector<uint8_t> content_key);
  EntitlementKey* GetEntitlementKey(const KeyId& entitlement_id);

 private:
  EntitlementKeyMap keys_;
  ContentIdToEntitlementIdMap contentid_to_entitlementid_;
  CORE_DISALLOW_COPY_AND_ASSIGN(EntitlementKeyTable);
};

}  // namespace wvoec_ref

#endif  // REF_OEMCRYPTO_SESSION_KEY_TABLE_H_
