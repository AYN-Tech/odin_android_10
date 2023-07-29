// Copyright 2019 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#include "rw_lock.h"

#include "log.h"

namespace wvcdm {

shared_mutex::~shared_mutex() {
  if (reader_count_ > 0) {
    LOGE("shared_mutex destroyed with active readers!");
  }
  if (has_writer_) {
    LOGE("shared_mutex destroyed with an active writer!");
  }
}

void shared_mutex::lock_shared() {
  std::unique_lock<std::mutex> lock(mutex_);

  while (has_writer_) {
    condition_variable_.wait(lock);
  }

  ++reader_count_;
}

void shared_mutex::unlock_shared() {
  std::unique_lock<std::mutex> lock(mutex_);

  --reader_count_;

  if (reader_count_ == 0) {
    condition_variable_.notify_all();
  }
}

bool shared_mutex::lock_implementation(bool abort_if_unavailable) {
  std::unique_lock<std::mutex> lock(mutex_);

  while (reader_count_ > 0 || has_writer_) {
    if (abort_if_unavailable) return false;
    condition_variable_.wait(lock);
  }

  has_writer_ = true;
  return true;
}

void shared_mutex::unlock() {
  std::unique_lock<std::mutex> lock(mutex_);

  has_writer_ = false;

  condition_variable_.notify_all();
}

}  // namespace wvcdm
