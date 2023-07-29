// Copyright 2018 Google Inc. All Rights Reserved.
//
// This file contains the declaration of the Pow2Bucket class which
// is a convenient way to bucketize sampled values into powers of 2.
#ifndef WVCDM_METRICS_POW2BUCKET_H_
#define WVCDM_METRICS_POW2BUCKET_H_

namespace wvcdm {
namespace metrics {

// This class converts the size_t value into the highest power of two
// below the value.  E.g. for 7, the value is 4.  For 11, the value is 8.
// This class is intended to simplify the use of EventMetric Fields that may
// have many possible values, but we want to bucket them into a small set of
// numbers (32 or 64).
class Pow2Bucket {
 public:
  explicit Pow2Bucket(size_t value) : value_(GetLowerBucket(value)) {}

  Pow2Bucket(const Pow2Bucket &value) : value_(value.value_) {}

  size_t value() const { return value_; }

  // Support for converting to string.
  friend std::ostream &operator<<(std::ostream &os, const Pow2Bucket &log) {
    return os << log.value_;
  }

 private:
  inline size_t GetLowerBucket(size_t value) {
    if (!value) {
      return 0;
    }

    size_t log = 0;
    while (value) {
      log++;
      value >>= 1;
    }

    return 1u << (log - 1);
  }

  size_t value_;
};

}  // namespace metrics
}  // namespace wvcdm

#endif  // WVCDM_METRICS_POW2BUCKET_H_
