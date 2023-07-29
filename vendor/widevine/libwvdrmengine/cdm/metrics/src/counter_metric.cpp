// Copyright 2017 Google Inc. All Rights Reserved.
//
// This file contains implementations for the BaseCounterMetric, the base class
// for CounterMetric.

#include "counter_metric.h"

#include "metrics.pb.h"

namespace wvcdm {
namespace metrics {

void BaseCounterMetric::Increment(const std::string &counter_key,
                                  int64_t value) {
  std::unique_lock<std::mutex> lock(internal_lock_);

  if (value_map_.find(counter_key) == value_map_.end()) {
    value_map_[counter_key] = value;
  } else {
    value_map_[counter_key] = value_map_[counter_key] + value;
  }
}

}  // namespace metrics
}  // namespace wvcdm
