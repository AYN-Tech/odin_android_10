// Copyright 2019 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "rw_lock.h"

using std::this_thread::sleep_for;

namespace wvcdm {

namespace {

const size_t kFleetSize = 20;

const auto kShortSnooze = std::chrono::milliseconds(10);
// By convention, we assume any other threads put into flight will have
// finished their minimal actions after 1 second.
const auto kDefaultTimeout = std::chrono::seconds(1);

struct ConditionVariablePair {
  std::mutex mutex;
  std::condition_variable condition_variable;
};

template <typename Duration>
bool WaitForTrue(std::atomic<bool>* value, Duration timeout) {
  std::chrono::steady_clock clock;
  auto start = clock.now();
  while (!*value) {
    if (clock.now() - start >= timeout) return false;
    sleep_for(kShortSnooze);
  }
  return true;
}

}  // namespace

TEST(SharedMutexUnitTests, ReadersDoNotBlockReaders) {
  shared_mutex mutex_under_test;

  // Used to signal the primary reader thread to stop holding the lock.
  ConditionVariablePair delay_primary;
  // Used to indicate if the primary thread is waiting. (and thus holding the
  // mutex)
  std::atomic<bool> primary_is_waiting(false);

  // Start a thread that takes the shared_mutex as a reader and holds it until
  // signalled to stop.
  std::future<void> primary_reader = std::async(std::launch::async, [&] {
    shared_lock<shared_mutex> auto_lock(mutex_under_test);

    ASSERT_FALSE(primary_is_waiting);
    primary_is_waiting = true;

    std::unique_lock<std::mutex> lock(delay_primary.mutex);
    delay_primary.condition_variable.wait(lock);

    primary_is_waiting = false;
  });

  // Wait for the primary thread to be waiting.
  ASSERT_TRUE(WaitForTrue(&primary_is_waiting, kDefaultTimeout));

  // Start a bunch of threads that take the shared_mutex as a reader and
  // immediately release it. These should not be blocked.
  std::vector<std::future<void>> reader_fleet;

  for (size_t i = 0; i < kFleetSize; ++i) {
    reader_fleet.push_back(std::async(std::launch::async, [&] {
      shared_lock<shared_mutex> auto_lock(mutex_under_test);
      EXPECT_TRUE(primary_is_waiting);
      sleep_for(kShortSnooze);
    }));
  }

  // The readers fleet should all complete successfully.
  for (std::future<void>& reader : reader_fleet) {
    EXPECT_EQ(std::future_status::ready, reader.wait_for(kDefaultTimeout));
  }

  // Verify the primary reader's state and signal it to finish.
  EXPECT_TRUE(primary_is_waiting);
  delay_primary.condition_variable.notify_one();
  EXPECT_EQ(std::future_status::ready,
            primary_reader.wait_for(kDefaultTimeout));
}

TEST(SharedMutexUnitTests, ReadersBlockWriters) {
  shared_mutex mutex_under_test;

  // Used to signal the reader thread to stop holding the lock.
  ConditionVariablePair delay_reader;
  // Used to indicate if the reader thread is waiting. (and thus holding the
  // mutex)
  std::atomic<bool> reader_is_waiting(false);

  // Start a thread that takes the shared_mutex as a reader and holds it until
  // signalled to stop.
  std::future<void> reader = std::async(std::launch::async, [&] {
    shared_lock<shared_mutex> auto_lock(mutex_under_test);

    ASSERT_FALSE(reader_is_waiting);
    reader_is_waiting = true;

    std::unique_lock<std::mutex> lock(delay_reader.mutex);
    delay_reader.condition_variable.wait(lock);

    reader_is_waiting = false;
  });

  // Wait for the reader thread to be waiting.
  ASSERT_TRUE(WaitForTrue(&reader_is_waiting, kDefaultTimeout));

  // Start a thread that takes the shared_mutex as a writer and immediately
  // releases it. This should be blocked by the reader above.
  std::future<void> writer = std::async(std::launch::async, [&] {
    std::unique_lock<shared_mutex> auto_lock(mutex_under_test);
    EXPECT_FALSE(reader_is_waiting);
    sleep_for(kShortSnooze);
  });

  // The writer should not be complete still, because it is waiting on the
  // shared_mutex.
  EXPECT_NE(std::future_status::ready, writer.wait_for(kDefaultTimeout));

  // Verify the reader is still waiting and signal it to finish.
  EXPECT_TRUE(reader_is_waiting);
  delay_reader.condition_variable.notify_one();

  // Make sure the reader actually finishes.
  EXPECT_EQ(std::future_status::ready, reader.wait_for(kDefaultTimeout));

  // The writer should now be able to complete as well.
  EXPECT_EQ(std::future_status::ready, writer.wait_for(kDefaultTimeout));
}

TEST(SharedMutexUnitTests, WritersBlockReaders) {
  shared_mutex mutex_under_test;

  // Used to signal the writer thread to stop holding the lock.
  ConditionVariablePair delay_writer;
  // Used to indicate if the writer thread is waiting. (and thus holding the
  // mutex)
  std::atomic<bool> writer_is_waiting(false);

  // Start a thread that takes the shared_mutex as a writer and holds it until
  // signalled to stop.
  std::future<void> writer = std::async(std::launch::async, [&] {
    std::unique_lock<shared_mutex> auto_lock(mutex_under_test);

    ASSERT_FALSE(writer_is_waiting);
    writer_is_waiting = true;

    std::unique_lock<std::mutex> lock(delay_writer.mutex);
    delay_writer.condition_variable.wait(lock);

    writer_is_waiting = false;
  });

  // Wait for the writer thread to be waiting.
  ASSERT_TRUE(WaitForTrue(&writer_is_waiting, kDefaultTimeout));

  // Start a thread that takes the shared_mutex as a reader and immediately
  // releases it. This should be blocked by the writer above.
  std::future<void> reader = std::async(std::launch::async, [&] {
    shared_lock<shared_mutex> auto_lock(mutex_under_test);
    EXPECT_FALSE(writer_is_waiting);
    sleep_for(kShortSnooze);
  });

  // The reader should not be complete still, because it is waiting on the
  // shared_mutex.
  EXPECT_NE(std::future_status::ready, reader.wait_for(kDefaultTimeout));

  // Verify the writer is still waiting and signal it to finish.
  EXPECT_TRUE(writer_is_waiting);
  delay_writer.condition_variable.notify_one();

  // Make sure the writer actually finishes.
  EXPECT_EQ(std::future_status::ready, writer.wait_for(kDefaultTimeout));

  // The reader should now be able to complete as well.
  EXPECT_EQ(std::future_status::ready, reader.wait_for(kDefaultTimeout));
}

TEST(SharedMutexUnitTests, WritersBlockWriters) {
  shared_mutex mutex_under_test;

  // Used to signal the primary writer thread to stop holding the lock.
  ConditionVariablePair delay_primary;
  // Used to indicate if the primary thread is waiting. (and thus holding the
  // mutex)
  std::atomic<bool> primary_is_waiting(false);

  // Start a thread that takes the shared_mutex as a writer and holds it until
  // signalled to stop.
  std::future<void> primary = std::async(std::launch::async, [&] {
    std::unique_lock<shared_mutex> auto_lock(mutex_under_test);

    ASSERT_FALSE(primary_is_waiting);
    primary_is_waiting = true;

    std::unique_lock<std::mutex> lock(delay_primary.mutex);
    delay_primary.condition_variable.wait(lock);

    primary_is_waiting = false;
  });

  // Wait for the primary thread to be waiting.
  ASSERT_TRUE(WaitForTrue(&primary_is_waiting, kDefaultTimeout));

  // Start a thread that takes the shared_mutex as a writer and immediately
  // releases it. This should be blocked by the primary writer above.
  std::future<void> secondary = std::async(std::launch::async, [&] {
    std::unique_lock<shared_mutex> auto_lock(mutex_under_test);
    EXPECT_FALSE(primary_is_waiting);
    sleep_for(kShortSnooze);
  });

  // The secondary writer should not be complete still, because it is waiting on
  // the shared_mutex.
  EXPECT_NE(std::future_status::ready, secondary.wait_for(kDefaultTimeout));

  // Verify the primary is still waiting and signal it to finish.
  EXPECT_TRUE(primary_is_waiting);
  delay_primary.condition_variable.notify_one();

  // Make sure the primary writer actually finished.
  EXPECT_EQ(std::future_status::ready, primary.wait_for(kDefaultTimeout));

  // The secondary writer should now be complete as well.
  EXPECT_EQ(std::future_status::ready, secondary.wait_for(kDefaultTimeout));
}

TEST(SharedMutexUnitTests, NonConcurrentLocksDoNotBlock) {
  shared_mutex mutex_under_test;

  for (size_t i = 0; i < kFleetSize / 2; ++i) {
    // Start a thread that takes the shared_mutex as a reader and immediately
    // releases it.
    std::future<void> reader = std::async(std::launch::async, [&] {
      shared_lock<shared_mutex> auto_lock(mutex_under_test);
      sleep_for(kShortSnooze);
    });

    ASSERT_EQ(std::future_status::ready, reader.wait_for(kDefaultTimeout));

    // Start a thread that takes the shared_mutex as a writer and immediately
    // releases it.
    std::future<void> writer = std::async(std::launch::async, [&] {
      std::unique_lock<shared_mutex> auto_lock(mutex_under_test);
      sleep_for(kShortSnooze);
    });

    ASSERT_EQ(std::future_status::ready, writer.wait_for(kDefaultTimeout));
  }
}

TEST(SharedMutexUnitTests, LargeVolumeOfThreadsSortsItselfOutEventually) {
  shared_mutex mutex_under_test;

  // Start a lot of threads that try to hold the lock at once.
  std::vector<std::future<void>> fleet;

  for (size_t i = 0; i < kFleetSize / 2; ++i) {
    fleet.push_back(std::async(std::launch::async, [&] {
      std::unique_lock<shared_mutex> auto_lock(mutex_under_test);
      sleep_for(kShortSnooze);
    }));

    fleet.push_back(std::async(std::launch::async, [&] {
      shared_lock<shared_mutex> auto_lock(mutex_under_test);
      sleep_for(kShortSnooze);
    }));
  }

  // The fleet should all complete eventually.
  for (std::future<void>& future : fleet) {
    EXPECT_EQ(std::future_status::ready, future.wait_for(kDefaultTimeout));
  }
}

}  // namespace wvcdm
