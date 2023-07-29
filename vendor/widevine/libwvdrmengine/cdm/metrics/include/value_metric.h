// Copyright 2017 Google Inc. All Rights Reserved.
//
// This file contains the declarations for the Metric class and related
// types.
#ifndef WVCDM_METRICS_VALUE_METRIC_H_
#define WVCDM_METRICS_VALUE_METRIC_H_

#include <stdint.h>
#include <mutex>
#include <string>

#include "metrics.pb.h"

namespace wvcdm {
namespace metrics {

// Internal namespace for helper methods.
namespace impl {

// Helper function for setting a value in the proto.
template <typename T>
void SetValue(drm_metrics::ValueMetric *value_proto, const T &value);

}  // namespace impl

// The ValueMetric class supports storing a single, overwritable value or an
// error code. This class can be serialized to a drm_metrics::ValueMetric proto.
// If an error or value was not provided, this metric will serialize to nullptr.
//
// Example Usage:
//   Metric<string> cdm_version().Record("a.b.c.d");
//   std::unique_ptr<drm_metrics::ValueMetric> mymetric(
//       cdm_version.ToProto());
//
// Example Error Usage:
//
//   Metric<string> cdm_version().SetError(error_code);
//   std::unique_ptr<drm_metrics::ValueMetric> mymetric(
//       cdm_version.ToProto());
//
template <typename T>
class ValueMetric {
 public:
  // Constructs a metric with the given metric name.
  explicit ValueMetric()
      : error_code_(0), has_error_(false), has_value_(false) {}

  // Record the value of the metric.
  void Record(const T &value) {
    std::unique_lock<std::mutex> lock(internal_lock_);
    value_ = value;
    has_value_ = true;
    has_error_ = false;
  }

  // Set the error code if an error was encountered.
  void SetError(int error_code) {
    std::unique_lock<std::mutex> lock(internal_lock_);
    error_code_ = error_code;
    has_value_ = false;
    has_error_ = true;
  }

  bool HasValue() const {
    std::unique_lock<std::mutex> lock(internal_lock_);
    return has_value_;
  }
  const T &GetValue() const {
    std::unique_lock<std::mutex> lock(internal_lock_);
    return value_;
  }

  bool HasError() const {
    std::unique_lock<std::mutex> lock(internal_lock_);
    return has_error_;
  }
  int GetError() const {
    std::unique_lock<std::mutex> lock(internal_lock_);
    return error_code_;
  }

  // Clears the indicators that the metric or error was set.
  void Clear() {
    std::unique_lock<std::mutex> lock(internal_lock_);
    has_value_ = false;
    has_error_ = false;
  }

  // Returns a new ValueMetric proto containing the metric value or the
  // error code. If neither the error or value are set, it returns nullptr.
  drm_metrics::ValueMetric *ToProto() const {
    std::unique_lock<std::mutex> lock(internal_lock_);
    if (has_error_) {
      drm_metrics::ValueMetric *value_proto = new drm_metrics::ValueMetric;
      value_proto->set_error_code(error_code_);
      return value_proto;
    } else if (has_value_) {
      drm_metrics::ValueMetric *value_proto = new drm_metrics::ValueMetric;
      impl::SetValue(value_proto, value_);
      return value_proto;
    }

    return NULL;
  }

 private:
  T value_;
  int error_code_;
  bool has_error_;
  bool has_value_;

  /*
   * This locks the internal state of the value metric to ensure safety
   * across multiple threads preventing the caller from worrying about
   * locking.
   *
   * This field must be declared mutable because the lock must be takeable even
   * in const methods.
   */
  mutable std::mutex internal_lock_;
};

}  // namespace metrics
}  // namespace wvcdm

#endif  // WVCDM_METRICS_VALUE_METRIC_H_
