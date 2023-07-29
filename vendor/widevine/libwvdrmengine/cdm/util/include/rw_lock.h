// Copyright 2019 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#ifndef WVCDM_UTIL_RW_LOCK_H_
#define WVCDM_UTIL_RW_LOCK_H_

#include <stdint.h>

#include <condition_variable>
#include <mutex>

#include "disallow_copy_and_assign.h"
#include "util_common.h"

namespace wvcdm {

// A simple reader-writer mutex implementation that mimics the one from C++17
class CORE_UTIL_EXPORT shared_mutex {
 public:
  shared_mutex() : reader_count_(0), has_writer_(false) {}
  ~shared_mutex();

  // These methods take the mutex as a reader. They do not fulfill the
  // SharedMutex requirement from the C++14 STL, but they fulfill enough of it
  // to be used with |shared_lock| below.
  void lock_shared();
  void unlock_shared();

  // These methods take the mutex as a writer. They fulfill the Mutex
  // requirement from the C++11 STL so that this mutex can be used with
  // |std::unique_lock|.
  void lock() { lock_implementation(false); }
  bool try_lock() { return lock_implementation(true); }
  void unlock();

 private:
  bool lock_implementation(bool abort_if_unavailable);

  uint32_t reader_count_;
  bool has_writer_;

  std::mutex mutex_;
  std::condition_variable condition_variable_;

  CORE_DISALLOW_COPY_AND_ASSIGN(shared_mutex);
};

// A simple reader lock implementation that mimics the one from C++14
template <typename Mutex>
class shared_lock {
 public:
  explicit shared_lock(Mutex& lock) : lock_(&lock) { lock_->lock_shared(); }
  explicit shared_lock(Mutex* lock) : lock_(lock) { lock_->lock_shared(); }
  ~shared_lock() { lock_->unlock_shared(); }

 private:
  Mutex* lock_;

  CORE_DISALLOW_COPY_AND_ASSIGN(shared_lock);
};

}  // namespace wvcdm

#endif  // WVCDM_UTIL_RW_LOCK_H_
