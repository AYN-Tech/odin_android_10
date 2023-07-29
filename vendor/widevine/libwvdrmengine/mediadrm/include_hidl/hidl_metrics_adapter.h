//
// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//

#ifndef CDM_HIDL_METRICS_ADAPTER_H_
#define CDM_HIDL_METRICS_ADAPTER_H_

#include <vector>

#include <android/hardware/drm/1.1/types.h>

#include "metrics.pb.h"

namespace wvcdm {

// Convenience alias. Used only in the declaration of HidlMetricsGroup.
namespace internal {
using DrmMetricGroup = android::hardware::drm::V1_1::DrmMetricGroup;
}  // namespace internal

class HidlMetricsAdapter;

// This class is used to convert from the metrics.proto format for metrics
// to the format specified in the android.hardware.drm@1.1 DrmMetricGroup
// type. This class converts the common metric types into a single group.
//
// Example:
//   drm_metrics::DistributionMetric distribution;
//   distribution.set_operation_count(1);
//   HidlMetricsGroupBuilder builder;
//   builder.AddDistributions("test distribution", { distribution });
//
//   DrmMetricGroup group = builder.Build();
class HidlMetricsGroupBuilder {
 public:
  // Adds a group of distributions with the given base name.
  void AddDistributions(
      const std::string& name,
      const google::protobuf::RepeatedPtrField<
          drm_metrics::DistributionMetric>& distributions);

  // Adds a group of counter metrics with the given base name.
  void AddCounters(
      const std::string& name,
      const google::protobuf::RepeatedPtrField<
          drm_metrics::CounterMetric>& counters);

  // Adds a value metric.
  void AddValue(const std::string& name,
                const drm_metrics::ValueMetric& value_or_error);

  // Builds the metric group containing all of the previously added metrics.
  internal::DrmMetricGroup Build();

 private:
  friend class HidlMetricsAdapter;
  std::vector<internal::DrmMetricGroup::Metric> metrics_;

  HidlMetricsGroupBuilder();
  // Adds a distribution with the given name and distribution values.
  void AddDistribution(const std::string& name,
                       const drm_metrics::DistributionMetric& distribution);
  // Adds a counter metric
  void AddCounter(const std::string& name,
                  const drm_metrics::CounterMetric& counter);
  void AddAttributes(
      const drm_metrics::Attributes& attributes_proto,
      ::android::hardware::hidl_vec<internal::DrmMetricGroup::Attribute>*
          attributes);
};

// This class handles adding the engine and session metric collections. This
// will generate one DrmMetricGroup for each EngineMetric added and one for
// each SessionMetric added. This class also supports a static utility method to
// convert a WvCdmMetrics proto to a vector of DrmMetricGroup instances.
class HidlMetricsAdapter {
 public:
  // Utility method to quickly convert a WvCdmMetrics proto to a DrmMetricGroup
  // vector.
  static void ToHidlMetrics(
      const drm_metrics::WvCdmMetrics& proto_metrics,
      android::hardware::hidl_vec<internal::DrmMetricGroup>* hidl_metrics);

  HidlMetricsAdapter();
  ~HidlMetricsAdapter();

  // Adds the EngineMetrics instance to the Adapter. It will be converted and
  // stored. The converted metrics can be fetched via GetHidlGroupVector.
  void AddEngineMetrics(
    const drm_metrics::WvCdmMetrics::EngineMetrics& proto_metrics);

  // Adds the SessionMetrics instance to the Adapter. It will be converted and
  // stored. The converted metrics can be fetched via GetHidlGroupVector.
  void AddSessionMetrics(
    const drm_metrics::WvCdmMetrics::SessionMetrics& proto_metrics);

  // Returns the converted DrmMetricGroup vector containing all of the
  // previously added engine and session metrics collections.
  const android::hardware::hidl_vec<internal::DrmMetricGroup>
      GetHidlGroupVector();

 private:
  void AddCryptoMetrics(
      const drm_metrics::WvCdmMetrics::CryptoMetrics& proto_metrics,
      HidlMetricsGroupBuilder* group_builder);
  std::vector<internal::DrmMetricGroup> group_vector_;
};

}  // namespace wvcdm

#endif  // CDM_HIDL_METRICS_ADAPTER_H_
