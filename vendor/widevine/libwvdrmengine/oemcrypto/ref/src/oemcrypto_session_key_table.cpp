// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
//  Reference implementation of OEMCrypto APIs
//
#include "oemcrypto_session_key_table.h"

#include "keys.h"
#include "log.h"

namespace wvoec_ref {

SessionKeyTable::~SessionKeyTable() {
  for (KeyMap::iterator i = keys_.begin(); i != keys_.end(); ++i) {
    if (NULL != i->second) {
      delete i->second;
    }
  }
}

bool SessionKeyTable::Insert(const KeyId key_id, const Key& key_data) {
  if (keys_.find(key_id) != keys_.end()) return false;
  keys_[key_id] = new Key(key_data);
  return true;
}

Key* SessionKeyTable::Find(const KeyId key_id) {
  if (keys_.find(key_id) == keys_.end()) {
    return NULL;
  }
  return keys_[key_id];
}

void SessionKeyTable::Remove(const KeyId key_id) {
  if (keys_.find(key_id) != keys_.end()) {
    delete keys_[key_id];
    keys_.erase(key_id);
  }
}

void SessionKeyTable::UpdateDuration(const KeyControlBlock& control) {
  for (KeyMap::iterator it = keys_.begin(); it != keys_.end(); ++it) {
    it->second->UpdateDuration(control);
  }
}

bool EntitlementKeyTable::Insert(const KeyId key_id, const Key& key_data) {
  // |key_id| and |key_data| are for an entitlement key. Insert a new
  // entitlement key entry.
  if (keys_.find(key_id) != keys_.end()) return false;
  keys_[key_id] = new EntitlementKey(key_data);
  // If this is a new insertion, we don't have a content key assigned yet.
  return true;
}

Key* EntitlementKeyTable::Find(const KeyId key_id) {
  // |key_id| refers to a content key.
  ContentIdToEntitlementIdMap::iterator it =
      contentid_to_entitlementid_.find(key_id);
  if (it == contentid_to_entitlementid_.end()) {
    return NULL;
  }

  if (keys_.find(it->second) == keys_.end()) {
    return NULL;
  }
  return keys_[it->second];
}

void EntitlementKeyTable::Remove(const KeyId key_id) {
  // |key_id| refers to a content key. No one currently calls Remove so this
  // method is free to change if needed.
  ContentIdToEntitlementIdMap::iterator it =
      contentid_to_entitlementid_.find(key_id);
  if (it == contentid_to_entitlementid_.end()) {
    return;
  }
  keys_.erase(it->second);
  contentid_to_entitlementid_.erase(key_id);
}

void EntitlementKeyTable::UpdateDuration(const KeyControlBlock& control) {
  for (EntitlementKeyMap::iterator it = keys_.begin(); it != keys_.end();
       ++it) {
    it->second->UpdateDuration(control);
  }
}

bool EntitlementKeyTable::SetContentKey(
    const KeyId& entitlement_id, const KeyId& content_key_id,
    const std::vector<uint8_t> content_key) {
  EntitlementKeyMap::iterator it = keys_.find(entitlement_id);
  if (it == keys_.end()) {
    return false;
  }
  contentid_to_entitlementid_.erase(it->second->content_key_id());
  if (!it->second->SetContentKey(content_key_id, content_key)) {
    return false;
  }
  contentid_to_entitlementid_[content_key_id] = entitlement_id;
  return true;
}

EntitlementKey* EntitlementKeyTable::GetEntitlementKey(
    const KeyId& entitlement_id) {
  EntitlementKeyMap::iterator it = keys_.find(entitlement_id);
  if (it == keys_.end()) {
    return nullptr;
  }
  return it->second;
}

}  // namespace wvoec_ref
