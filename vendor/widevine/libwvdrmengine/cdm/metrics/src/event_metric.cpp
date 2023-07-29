// Copyright 2017 Google Inc. All Rights Reserved.
//
// This file contains implementations for the BaseEventMetric.

#include "event_metric.h"

using ::google::protobuf::RepeatedPtrField;

namespace wvcdm {
namespace metrics {

BaseEventMetric::~BaseEventMetric() {
  std::unique_lock<std::mutex> lock(internal_lock_);

  for (std::map<std::string, Distribution *>::iterator it = value_map_.begin();
       it != value_map_.end(); it++) {
    delete it->second;
  }
}

void BaseEventMetric::Record(const std::string &key, double value) {
  std::unique_lock<std::mutex> lock(internal_lock_);

  Distribution *distribution;

  if (value_map_.find(key) == value_map_.end()) {
    distribution = new Distribution();
    value_map_[key] = distribution;
  } else {
    distribution = value_map_[key];
  }

  distribution->Record(value);
}

}  // namespace metrics
}  // namespace wvcdm
