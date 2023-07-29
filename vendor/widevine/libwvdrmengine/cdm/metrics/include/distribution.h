// Copyright 2017 Google Inc. All Rights Reserved.
//
// This file contains the definition of a Distribution class which computes
// the distribution values of a series of samples.

#ifndef WVCDM_METRICS_DISTRIBUTION_H_
#define WVCDM_METRICS_DISTRIBUTION_H_

#include <stdint.h>

namespace wvcdm {
namespace metrics {

// The Distribution class holds statistics about a series of values that the
// client provides via the Record method. A caller will call Record once for
// each of the values in a series. The Distribution instance will calculate the
// mean, count, min, max and variance of the distribution.
//
// Example usage:
//   Distribution dist;
//   dist.Record(1);
//   dist.Record(2);
//   dist.Mean();  // Returns 1.5.
//   dist.Count();  // Returns 2.
class Distribution {
 public:
  Distribution();

  // Uses the provided sample value to update the computed statistics.
  void Record(float value);

  // Return the value for each of the stats computed about the series of
  // values (min, max, count, etc.).
  float Min() const { return min_; }
  float Max() const { return max_; }
  float Mean() const { return mean_; }
  uint64_t Count() const { return count_; }
  double Variance() const {
    return count_ == 0 ? 0.0 : sum_squared_deviation_ / count_;
  }

 private:
  uint64_t count_;
  float min_;
  float max_;
  float mean_;
  double sum_squared_deviation_;
};

}  // namespace metrics
}  // namespace wvcdm

#endif  // WVCDM_METRICS_DISTRIBUTION_H_
