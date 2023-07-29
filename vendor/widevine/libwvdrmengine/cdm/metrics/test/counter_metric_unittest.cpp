// Copyright 2017 Google Inc. All Rights Reserved.
//
// Unit tests for CounterMetric

#include "counter_metric.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "string_conversions.h"

using drm_metrics::TestMetrics;
using testing::IsNull;
using testing::NotNull;

namespace wvcdm {
namespace metrics {

TEST(CounterMetricTest, NoFieldsEmpty) {
  wvcdm::metrics::CounterMetric<> metric;

  TestMetrics metric_proto;
  metric.ToProto(metric_proto.mutable_test_counters());
  ASSERT_EQ(0, metric_proto.test_counters().size());
}

TEST(CounterMetricTest, NoFieldsSuccess) {
  wvcdm::metrics::CounterMetric<> metric;
  metric.Increment();
  metric.Increment(10);

  TestMetrics metric_proto;
  std::string serialized_metrics;
  metric.ToProto(metric_proto.mutable_test_counters());
  ASSERT_EQ(1, metric_proto.test_counters().size());

  ASSERT_TRUE(metric_proto.SerializeToString(&serialized_metrics));

  EXPECT_EQ(11, metric_proto.test_counters(0).count());
  EXPECT_FALSE(metric_proto.test_counters(0).has_attributes())
      << std::string("Unexpected attributes value.  Serialized metrics: ")
      << wvcdm::b2a_hex(serialized_metrics);
}

TEST(CounterMetricTest, OneFieldSuccess) {
  wvcdm::metrics::CounterMetric<drm_metrics::Attributes::kErrorCodeFieldNumber,
                                int> metric;
  metric.Increment(7);
  metric.Increment(10, 7);
  metric.Increment(13);
  metric.Increment(20, 13);

  TestMetrics metric_proto;
  metric.ToProto(metric_proto.mutable_test_counters());
  ASSERT_EQ(2, metric_proto.test_counters().size());

  EXPECT_EQ(11u, metric_proto.test_counters(0).count());
  EXPECT_EQ(7, metric_proto.test_counters(0).attributes().error_code());
  EXPECT_EQ(21, metric_proto.test_counters(1).count());
  EXPECT_EQ(13, metric_proto.test_counters(1).attributes().error_code());
}

TEST(CounterMetricTest, TwoFieldsSuccess) {
  CounterMetric<drm_metrics::Attributes::kErrorCodeFieldNumber, int,
                drm_metrics::Attributes::kLengthFieldNumber, Pow2Bucket> metric;

  metric.Increment(7, Pow2Bucket(23));  // Increment by one.
  metric.Increment(2, 7, Pow2Bucket(33));
  metric.Increment(3, 11, Pow2Bucket(23));
  metric.Increment(4, 11, Pow2Bucket(33));
  metric.Increment(5, 7, Pow2Bucket(23));
  metric.Increment(-5, 7, Pow2Bucket(33));

  // Verify all instances.
  TestMetrics metric_proto;
  metric.ToProto(metric_proto.mutable_test_counters());
  ASSERT_EQ(4, metric_proto.test_counters().size());

  EXPECT_EQ(6, metric_proto.test_counters(0).count());
  EXPECT_EQ(7, metric_proto.test_counters(0).attributes().error_code());
  EXPECT_EQ(16u, metric_proto.test_counters(0).attributes().length());
  EXPECT_EQ(-3, metric_proto.test_counters(1).count());
  EXPECT_EQ(7, metric_proto.test_counters(1).attributes().error_code());
  EXPECT_EQ(32u, metric_proto.test_counters(1).attributes().length());
  EXPECT_EQ(3, metric_proto.test_counters(2).count());
  EXPECT_EQ(11, metric_proto.test_counters(2).attributes().error_code());
  EXPECT_EQ(16u, metric_proto.test_counters(2).attributes().length());
  EXPECT_EQ(4, metric_proto.test_counters(3).count());
  EXPECT_EQ(11, metric_proto.test_counters(3).attributes().error_code());
  EXPECT_EQ(32u, metric_proto.test_counters(3).attributes().length());
}

TEST(CounterMetricTest, ThreeFieldsSuccess) {
  CounterMetric<drm_metrics::Attributes::kErrorCodeFieldNumber, int,
                drm_metrics::Attributes::kLengthFieldNumber, Pow2Bucket,
                drm_metrics::Attributes::kErrorCodeBoolFieldNumber, bool>
                    metric;
  metric.Increment(7, Pow2Bucket(13), true);

  TestMetrics metric_proto;
  metric.ToProto(metric_proto.mutable_test_counters());
  ASSERT_EQ(1, metric_proto.test_counters().size());
  EXPECT_EQ(1, metric_proto.test_counters(0).count());
  EXPECT_EQ(7, metric_proto.test_counters(0).attributes().error_code());
  EXPECT_EQ(8u, metric_proto.test_counters(0).attributes().length());
  EXPECT_TRUE(metric_proto.test_counters(0).attributes().error_code_bool());
}

TEST(CounterMetricTest, FourFieldsSuccess) {
  CounterMetric<drm_metrics::Attributes::kErrorCodeFieldNumber, int,
                drm_metrics::Attributes::kLengthFieldNumber, Pow2Bucket,
                drm_metrics::Attributes::kErrorCodeBoolFieldNumber, bool,
                drm_metrics::Attributes::kSecurityLevelFieldNumber,
                SecurityLevel> metric;
  metric.Increment(10LL, 7, Pow2Bucket(13), true, kLevel3);

  TestMetrics metric_proto;
  metric.ToProto(metric_proto.mutable_test_counters());
  ASSERT_EQ(1, metric_proto.test_counters().size());
  EXPECT_EQ(10, metric_proto.test_counters(0).count());
  EXPECT_EQ(7, metric_proto.test_counters(0).attributes().error_code());
  EXPECT_EQ(8u, metric_proto.test_counters(0).attributes().length());
  EXPECT_TRUE(metric_proto.test_counters(0).attributes().error_code_bool());
  EXPECT_EQ(kLevel3,
            metric_proto.test_counters(0).attributes().security_level());
}

}  // namespace metrics
}  // namespace wvcdm
