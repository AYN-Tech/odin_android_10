// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
//  Reference implementation of OEMCrypto APIs
//
#include "oemcrypto_nonce_table.h"

namespace wvoec_ref {

void NonceTable::AddNonce(uint32_t nonce) {
  int new_slot = -1;
  int oldest_slot = -1;

  // Flush any nonces that have been checked but not flushed.
  // After flush, nonces will be either valid or invalid.
  Flush();

  for (int i = 0; i < kTableSize; ++i) {
    // Increase age of all valid nonces.
    if (kNTStateValid == state_[i]) {
      ++age_[i];
      if (-1 == oldest_slot) {
        oldest_slot = i;
      } else {
        if (age_[i] > age_[oldest_slot]) {
          oldest_slot = i;
        }
      }
    } else {
      if (-1 == new_slot) {
        age_[i] = 0;
        nonces_[i] = nonce;
        state_[i] = kNTStateValid;
        new_slot = i;
      }
    }
  }
  if (-1 == new_slot) {
    // reuse oldest
    // assert (oldest_slot != -1)
    int i = oldest_slot;
    age_[i] = 0;
    nonces_[i] = nonce;
    state_[i] = kNTStateValid;
  }
}

bool NonceTable::CheckNonce(uint32_t nonce) {
  for (int i = 0; i < kTableSize; ++i) {
    if (kNTStateInvalid != state_[i]) {
      if (nonce == nonces_[i]) {
        state_[i] = kNTStateFlushPending;
        return true;
      }
    }
  }
  return false;
}

bool NonceTable::NonceCollision(uint32_t nonce) const {
  for (int i = 0; i < kTableSize; ++i) {
    if (nonce == nonces_[i]) return true;
  }
  return false;
}

void NonceTable::Flush() {
  for (int i = 0; i < kTableSize; ++i) {
    if (kNTStateFlushPending == state_[i]) {
      state_[i] = kNTStateInvalid;
    }
  }
}

}  // namespace wvoec_ref
