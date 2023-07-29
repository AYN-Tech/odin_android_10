// Copyright 2017 Google Inc. All Rights Reserved.
//
// This file contains the declarations for the EventMetric class and related
// types.
#ifndef WVCDM_METRICS_EVENT_METRIC_H_
#define WVCDM_METRICS_EVENT_METRIC_H_

#include <cstdarg>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "OEMCryptoCENC.h"
#include "attribute_handler.h"
#include "distribution.h"
#include "log.h"
#include "metrics.pb.h"
#include "pow2bucket.h"

namespace wvcdm {
namespace metrics {

class EventMetricTest;

// This base class provides the common defintion used by all templated
// instances of EventMetric.
class BaseEventMetric {
 public:
 protected:
  // Instantiates a BaseEventMetric.
  BaseEventMetric() {}

  virtual ~BaseEventMetric();

  // Record will look for an existing instance of the Distribution identified
  // by the distribution_key string and update it. If the instance does
  // not exist, this will create it.
  void Record(const std::string &distribution_key, double value);

  // value_map_ contains a mapping from the string key (attribute name/values)
  // to the distribution instance which holds the metric information
  // (min, max, sum, etc.).
  std::map<std::string, Distribution *> value_map_;

 private:
  friend class EventMetricTest;

  /*
   * This locks the internal state of the event metric to ensure safety across
   * multiple threads preventing the caller from worrying about locking.
   */
  std::mutex internal_lock_;
};

// The EventMetric class is used to capture statistics about an event such as
// a method call. EventMetric keeps track of the count, mean, min, max and
// variance for the value being measured. For example, we may want to track the
// latency of a particular operation. Each time the operation is run, the time
// is calculated and provided to the EventMetric by using the
// EventMetric::Record method to capture and maintain statistics about the
// latency (max, min, count, mean, variance).
//
// The EventMetric class supports the ability to breakdown the statistics based
// on certain "field" values. For example, if a particular operation can run in
// one of two modes, it's useful to track the latency of the operation in each
// mode separately. You can use Fields to define how to breakdown the
// statistics. Each Field is a separate dimension. The statistics for each
// combination of field values are tracked independently.
//
// Example usage:
//
//
// EventMetric<
//     ::drm_metrics::Attributes::kErrorCodeFieldNumber,
//     CdmResponseType,
//     ::drm_metrics::Attributes::kCdmSecurityLevelFieldNumber,
//     CdmSecurityLevel> my_metric;
//
// // (latency value, error code, security level)
// my_metric.Increment(1, CdmResponseType::NO_ERROR, SecurityLevel::kLevel3);
//
// The EventMetric class serializes its values to a repeated field of
// drm_metrics::DistributionMetric instances. The field numbers used for
// recording the drm_metrics::Attributes are specified in the template
// parameters above (e.g. kErrorCodeFieldNumber).
template <int I1 = 0, typename F1 = util::Unused, int I2 = 0,
          typename F2 = util::Unused, int I3 = 0, typename F3 = util::Unused,
          int I4 = 0, typename F4 = util::Unused>
class EventMetric : public BaseEventMetric {
 public:
  // Create an EventMetric instance with no attribute breakdowns.
  explicit EventMetric() : BaseEventMetric(){}

  // Record will update the statistics of the EventMetric broken down by the
  // given field values.
  void Record(double value, F1 field1 = util::Unused(),
              F2 field2 = util::Unused(), F3 field3 = util::Unused(),
              F4 field4 = util::Unused()) {
    std::string key =
        attribute_handler_.GetSerializedAttributes(field1, field2,
                                                   field3, field4);
    BaseEventMetric::Record(key, value);
  }

  const std::map<std::string, Distribution *>* GetDistributions() const {
    return &value_map_;
  };

  void ToProto(
      ::google::protobuf::RepeatedPtrField<drm_metrics::DistributionMetric>
          *distributions_proto) const;

 private:
  friend class EventMetricTest;
  AttributeHandler<I1, F1, I2, F2, I3, F3, I4, F4> attribute_handler_;

  inline void SetDistributionValues(
      const Distribution &distribution,
      drm_metrics::DistributionMetric *metric_proto) const {
    metric_proto->set_mean(distribution.Mean());
    metric_proto->set_operation_count(distribution.Count());
    if (distribution.Count() > 1) {
      metric_proto->set_min(distribution.Min());
      metric_proto->set_max(distribution.Max());
      metric_proto->set_mean(distribution.Mean());
      metric_proto->set_variance(distribution.Variance());
      metric_proto->set_operation_count(distribution.Count());
    }
  }
};

// Partial Specializations of Event Metrics.

// Specialization for the event metric with no attributes.
// For this, we don't deserialize and populate the attributes message.
template <>
inline void EventMetric<0, util::Unused, 0, util::Unused, 0, util::Unused, 0,
                        util::Unused>::
    ToProto(
        ::google::protobuf::RepeatedPtrField<drm_metrics::DistributionMetric>
            *distributions_proto) const {
  const std::map<std::string, Distribution *>* distributions
      = GetDistributions();
  for (std::map<std::string, Distribution *>::const_iterator it =
       distributions->begin(); it != distributions->end(); it++) {
    drm_metrics::DistributionMetric *new_metric = distributions_proto->Add();
    SetDistributionValues(*it->second, new_metric);
  }
}

template <int I1, typename F1, int I2, typename F2, int I3, typename F3, int I4,
          typename F4>
inline void EventMetric<I1, F1, I2, F2, I3, F3, I4, F4>::ToProto(
    ::google::protobuf::RepeatedPtrField<drm_metrics::DistributionMetric>
        *distributions_proto) const {
  const std::map<std::string, Distribution *>* distributions
      = GetDistributions();
  for (std::map<std::string, Distribution *>::const_iterator it =
       distributions->begin(); it != distributions->end(); it++) {
    drm_metrics::DistributionMetric *new_metric = distributions_proto->Add();
    if (!new_metric->mutable_attributes()->ParseFromString(it->first)) {
      LOGE("Failed to parse the attributes from a string.");
    }
    SetDistributionValues(*it->second, new_metric);
  }
}

}  // namespace metrics
}  // namespace wvcdm

#endif  // WVCDM_METRICS_EVENT_METRIC_H_
