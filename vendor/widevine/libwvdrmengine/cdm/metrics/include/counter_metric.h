// Copyright 2017 Google Inc. All Rights Reserved.
//
// This file contains the declarations for the Metric class and related
// types.
#ifndef WVCDM_METRICS_COUNTER_METRIC_H_
#define WVCDM_METRICS_COUNTER_METRIC_H_

#include <cstdarg>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "attribute_handler.h"
#include "field_tuples.h"

namespace wvcdm {
namespace metrics {

class CounterMetricTest;

// This base class provides the common defintion used by all templated
// instances of CounterMetric.
class BaseCounterMetric {
 public:
  const std::map<std::string, int64_t>* GetValues() const {
    return &value_map_;
  };

 protected:
  // Instantiates a BaseCounterMetric.
  BaseCounterMetric() {}

  virtual ~BaseCounterMetric() {}

  // Increment will look for an existing instance of the field names and
  // add the new value to the existing value. If the instance does not exist,
  // this method will create it.
  void Increment(const std::string &counter_key, int64_t value);

 private:
  friend class CounterMetricTest;
  const std::string metric_name_;
  // value_map_ contains a mapping from the field name/value pairs to the
  // counter(int64_t) instance.
  std::map<std::string, int64_t> value_map_;

  /*
   * This locks the internal state of the counter metric to ensure safety
   * across multiple threads preventing the caller from worrying about
   * locking.
   */
  std::mutex internal_lock_;
};

// The CounterMetric class is used to track a counter such as the number of
// times a method is called. It can also track a delta that could be positive
// or negative.
//
// The CounterMetric class supports the ability to keep track of multiple
// variations of the count based on certain "field" values. E.g. keep track of
// the counts of success and failure counts for a method call. Or keep track of
// number of times the method was called with a particular parameter.
// Fields define what variations to track. Each Field is a separate dimension.
// The count for each combination of field values is tracked independently.
//
// Example usage:
//
// CounterMetric<
//     ::drm_metrics::Attributes::kErrorCodeFieldNumber,
//     CdmResponseType,
//     ::drm_metrics::Attributes::kCdmSecurityLevelFieldNumber,
//     CdmSecurityLevel> my_metric;
//
// // (counter value, error code, security level)
// my_metric.Increment(1, CdmResponseType::NO_ERROR, SecurityLevel::kLevel3);
//
// The CounterMetric class serializes its values to a repeated field of
// drm_metrics::CounterMetric instances. The field numbers used for
// recording the drm_metrics::Attributes are specified in the template
// parameters above (e.g. kErrorCodeFieldNumber).
template <int I1 = 0, typename F1 = util::Unused, int I2 = 0,
          typename F2 = util::Unused, int I3 = 0, typename F3 = util::Unused,
          int I4 = 0, typename F4 = util::Unused>
class CounterMetric : public BaseCounterMetric {
 public:
  explicit CounterMetric() : BaseCounterMetric() {}

  // Increment will update the counter value associated with the provided
  // field values.
  void Increment(F1 field1 = util::Unused(), F2 field2 = util::Unused(),
                 F3 field3 = util::Unused(), F4 field4 = util::Unused()) {
    Increment(1, field1, field2, field3, field4);
  }

  // Increment will add the value to the counter associated with the provided
  // field values.
  void Increment(int64_t value, F1 field1 = util::Unused(),
                 F2 field2 = util::Unused(), F3 field3 = util::Unused(),
                 F4 field4 = util::Unused()) {
    std::string key =
        attribute_handler_.GetSerializedAttributes(field1, field2,
                                                   field3, field4);
    BaseCounterMetric::Increment(key, value);
  }

  void ToProto(::google::protobuf::RepeatedPtrField<drm_metrics::CounterMetric>
                   *counters) const;

 private:
  friend class CounterMetricTest;
  AttributeHandler<I1, F1, I2, F2, I3, F3, I4, F4> attribute_handler_;
};

// Partial specializations for CounterMetric template.

// Specialization for the CounterMetric with no attributes.
// For this, we don't deserialize and populate the attributes message.
template <>
inline void CounterMetric<0, util::Unused, 0, util::Unused, 0, util::Unused, 0,
                          util::Unused>::
    ToProto(::google::protobuf::RepeatedPtrField<drm_metrics::CounterMetric>
                *counters) const {

  const std::map<std::string, int64_t>* values = GetValues();
  for (std::map<std::string, int64_t>::const_iterator it = values->begin();
       it != values->end(); it++) {
    drm_metrics::CounterMetric *new_counter = counters->Add();
    new_counter->set_count(it->second);
  }
}

template <int I1, typename F1, int I2, typename F2, int I3, typename F3, int I4,
          typename F4>
inline void CounterMetric<I1, F1, I2, F2, I3, F3, I4, F4>::ToProto(
    ::google::protobuf::RepeatedPtrField<drm_metrics::CounterMetric>
        *counters) const {
  const std::map<std::string, int64_t>* values = GetValues();
  for (std::map<std::string, int64_t>::const_iterator it = values->begin();
       it != values->end(); it++) {
    drm_metrics::CounterMetric *new_counter = counters->Add();
    if (!new_counter->mutable_attributes()->ParseFromString(it->first)) {
      LOGE("Failed to parse the attributes from a string.");
    }
    new_counter->set_count(it->second);
  }
}

}  // namespace metrics
}  // namespace wvcdm

#endif  // WVCDM_METRICS_COUNTER_METRIC_H_
