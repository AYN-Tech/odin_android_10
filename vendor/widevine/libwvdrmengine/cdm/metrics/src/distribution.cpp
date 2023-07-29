// Copyright 2017 Google Inc. All Rights Reserved.
//
// This file contains the definitions for the Distribution class members.

#include "distribution.h"

#include <float.h>

namespace wvcdm {
namespace metrics {

Distribution::Distribution()
    : count_(0ULL),
      min_(FLT_MAX),
      max_(-FLT_MAX),
      mean_(0.0),
      sum_squared_deviation_(0.0) {}

void Distribution::Record(float value) {
  // Using method of provisional means.
  float deviation = value - mean_;
  mean_ = mean_ + (deviation / ++count_);
  sum_squared_deviation_ =
      sum_squared_deviation_ + (deviation * (value - mean_));

  min_ = min_ < value ? min_ : value;
  max_ = max_ > value ? max_ : value;
}

}  // namespace metrics
}  // namespace wvcdm
