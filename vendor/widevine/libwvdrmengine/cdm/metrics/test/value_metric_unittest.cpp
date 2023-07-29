// Copyright 2017 Google Inc. All Rights Reserved.
//
// Unit tests for ValueMetric.

#include <memory>
#include <string>

#include "value_metric.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "metrics.pb.h"

namespace wvcdm {
namespace metrics {

TEST(ValueMetricTest, StringValue) {
  ValueMetric<std::string> metric;
  metric.Record("foo");

  std::unique_ptr<drm_metrics::ValueMetric> metric_proto(metric.ToProto());
  ASSERT_EQ("foo", metric_proto->string_value());
  ASSERT_FALSE(metric_proto->has_error_code());
}

TEST(ValueMetricTest, DoubleValue) {
  ValueMetric<double> metric;
  metric.Record(42.0);

  std::unique_ptr<drm_metrics::ValueMetric> metric_proto(metric.ToProto());
  ASSERT_EQ(42.0, metric_proto->double_value());
  ASSERT_FALSE(metric_proto->has_error_code());
}

TEST(ValueMetricTest, Int32Value) {
  ValueMetric<int32_t> metric;
  metric.Record(42);

  std::unique_ptr<drm_metrics::ValueMetric> metric_proto(metric.ToProto());
  ASSERT_EQ(42, metric_proto->int_value());
  ASSERT_FALSE(metric_proto->has_error_code());
}

TEST(ValueMetricTest, Int64Value) {
  ValueMetric<int64_t> metric;
  metric.Record(42);

  std::unique_ptr<drm_metrics::ValueMetric> metric_proto(metric.ToProto());
  ASSERT_EQ(42, metric_proto->int_value());
  ASSERT_FALSE(metric_proto->has_error_code());
}

TEST(ValueMetricTest, SetError) {
  ValueMetric<int64_t> metric;
  metric.Record(42);
  metric.SetError(7);

  std::unique_ptr<drm_metrics::ValueMetric> metric_proto(metric.ToProto());
  ASSERT_EQ(7, metric_proto->error_code());
  ASSERT_FALSE(metric_proto->has_int_value());
}

}  // namespace metrics
}  // namespace wvcdm
