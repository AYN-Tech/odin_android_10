// Copyright 2017 Google Inc. All Rights Reserved.
//
// Unit tests for EventMetric

#include "event_metric.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "string_conversions.h"

using drm_metrics::TestMetrics;
using testing::IsNull;
using testing::NotNull;

namespace wvcdm {
namespace metrics {

class EventMetricTest : public ::testing::Test {
 public:
  void SetUp() {}

 protected:
};

TEST_F(EventMetricTest, NoFieldsEmpty) {
  wvcdm::metrics::EventMetric<> metric;

  TestMetrics metric_proto;
  metric.ToProto(metric_proto.mutable_test_distributions());

  ASSERT_EQ(0, metric_proto.test_distributions().size());
}

TEST_F(EventMetricTest, NoFieldsSuccess) {
  wvcdm::metrics::EventMetric<> metric;
  metric.Record(10LL);
  metric.Record(10LL);

  TestMetrics metric_proto;
  metric.ToProto(metric_proto.mutable_test_distributions());

  std::string serialized_metrics;
  ASSERT_TRUE(metric_proto.SerializeToString(&serialized_metrics));

  ASSERT_EQ(1, metric_proto.test_distributions().size());
  EXPECT_EQ(2u, metric_proto.test_distributions(0).operation_count());
  EXPECT_FALSE(metric_proto.test_distributions(0).has_attributes())
      << std::string("Unexpected attributes value.  Serialized metrics: ")
      << wvcdm::b2a_hex(serialized_metrics);
}

TEST_F(EventMetricTest, OneFieldSuccess) {
  EventMetric<drm_metrics::Attributes::kErrorCodeFieldNumber, int> metric;
  metric.Record(10LL, 7);
  metric.Record(11LL, 13);
  metric.Record(12LL, 13);

  TestMetrics metric_proto;
  metric.ToProto(metric_proto.mutable_test_distributions());
  ASSERT_EQ(2, metric_proto.test_distributions().size());

  EXPECT_EQ(1u, metric_proto.test_distributions(0).operation_count());
  EXPECT_EQ(10LL, metric_proto.test_distributions(0).mean());

  EXPECT_EQ(2u, metric_proto.test_distributions(1).operation_count());
  EXPECT_NEAR(11.5, metric_proto.test_distributions(1).mean(), 1.0);
  EXPECT_NEAR(0.5, metric_proto.test_distributions(1).variance(), 0.5);
  EXPECT_EQ(11, metric_proto.test_distributions(1).min());
  EXPECT_EQ(12, metric_proto.test_distributions(1).max());
}

TEST_F(EventMetricTest, TwoFieldsSuccess) {
  EventMetric<drm_metrics::Attributes::kErrorCodeFieldNumber, int,
              drm_metrics::Attributes::kLengthFieldNumber, Pow2Bucket> metric;

  metric.Record(1, 7, Pow2Bucket(23));
  metric.Record(2, 7, Pow2Bucket(33));
  metric.Record(3, 11, Pow2Bucket(23));
  metric.Record(4, 11, Pow2Bucket(33));
  metric.Record(5, 7, Pow2Bucket(23));

  TestMetrics metric_proto;
  metric.ToProto(metric_proto.mutable_test_distributions());
  ASSERT_EQ(4, metric_proto.test_distributions().size());

  // error 7, bucket 16
  EXPECT_EQ(2u, metric_proto.test_distributions(0).operation_count());
  EXPECT_EQ(3LL, metric_proto.test_distributions(0).mean());
  EXPECT_NEAR(3.5, metric_proto.test_distributions(0).variance(), 0.5);
  EXPECT_EQ(1, metric_proto.test_distributions(0).min());
  EXPECT_EQ(5, metric_proto.test_distributions(0).max());
  EXPECT_EQ(7, metric_proto.test_distributions(0).attributes().error_code());
  EXPECT_EQ(16U, metric_proto.test_distributions(0).attributes().length());

  // error 7, bucket 32
  EXPECT_EQ(1u, metric_proto.test_distributions(1).operation_count());
  EXPECT_EQ(2, metric_proto.test_distributions(1).mean());
  EXPECT_EQ(7, metric_proto.test_distributions(1).attributes().error_code());
  EXPECT_EQ(32U, metric_proto.test_distributions(1).attributes().length());

  // error 11, bucket 16
  EXPECT_EQ(1u, metric_proto.test_distributions(2).operation_count());
  EXPECT_EQ(3, metric_proto.test_distributions(2).mean());
  EXPECT_EQ(11, metric_proto.test_distributions(2).attributes().error_code());
  EXPECT_EQ(16U, metric_proto.test_distributions(2).attributes().length());

  // error 11, bucket 32
  EXPECT_EQ(1u, metric_proto.test_distributions(3).operation_count());
  EXPECT_EQ(4, metric_proto.test_distributions(3).mean());
  EXPECT_EQ(11, metric_proto.test_distributions(3).attributes().error_code());
  EXPECT_EQ(32U, metric_proto.test_distributions(3).attributes().length());
}

TEST_F(EventMetricTest, ThreeFieldsSuccess) {
  EventMetric<drm_metrics::Attributes::kErrorCodeFieldNumber, int,
              drm_metrics::Attributes::kLengthFieldNumber, Pow2Bucket,
              drm_metrics::Attributes::kErrorCodeBoolFieldNumber, bool> metric;
  metric.Record(10LL, 7, Pow2Bucket(13), false);
  metric.Record(11LL, 8, Pow2Bucket(17), true);

  TestMetrics metric_proto;
  metric.ToProto(metric_proto.mutable_test_distributions());
  ASSERT_EQ(2, metric_proto.test_distributions().size());

  EXPECT_EQ(1u, metric_proto.test_distributions(0).operation_count());
  EXPECT_EQ(10LL, metric_proto.test_distributions(0).mean());
  EXPECT_FALSE(metric_proto.test_distributions(0).has_variance());
  EXPECT_EQ(7, metric_proto.test_distributions(0).attributes().error_code());
  EXPECT_EQ(8u, metric_proto.test_distributions(0).attributes().length());
  EXPECT_FALSE(metric_proto.test_distributions(0).attributes().error_code_bool());

  EXPECT_EQ(1u, metric_proto.test_distributions(1).operation_count());
  EXPECT_EQ(11LL, metric_proto.test_distributions(1).mean());
  EXPECT_FALSE(metric_proto.test_distributions(1).has_variance());
  EXPECT_EQ(8, metric_proto.test_distributions(1).attributes().error_code());
  EXPECT_EQ(16u, metric_proto.test_distributions(1).attributes().length());
  EXPECT_TRUE(metric_proto.test_distributions(1).attributes().error_code_bool());
}

TEST_F(EventMetricTest, FourFieldsSuccess) {
  EventMetric<drm_metrics::Attributes::kErrorCodeFieldNumber, int,
              drm_metrics::Attributes::kLengthFieldNumber, Pow2Bucket,
              drm_metrics::Attributes::kErrorCodeBoolFieldNumber, bool,
              drm_metrics::Attributes::kCdmSecurityLevelFieldNumber,
              CdmSecurityLevel> metric;

  metric.Record(10LL, 7, Pow2Bucket(13), true, kSecurityLevelL3);

  TestMetrics metric_proto;
  metric.ToProto(metric_proto.mutable_test_distributions());
  ASSERT_EQ(1, metric_proto.test_distributions().size());

  EXPECT_EQ(1u, metric_proto.test_distributions(0).operation_count());
  EXPECT_EQ(10LL, metric_proto.test_distributions(0).mean());
  EXPECT_FALSE(metric_proto.test_distributions(0).has_variance());
  EXPECT_EQ(7, metric_proto.test_distributions(0).attributes().error_code());
  EXPECT_EQ(8u, metric_proto.test_distributions(0).attributes().length());
  EXPECT_TRUE(metric_proto.test_distributions(0).attributes().error_code_bool());
  EXPECT_EQ(
      3u, metric_proto.test_distributions(0).attributes().cdm_security_level());
}

TEST_F(EventMetricTest, Pow2BucketTest) {
  std::stringstream value;
  value << Pow2Bucket(1);
  EXPECT_EQ("1", value.str());

  value.str("");
  value << Pow2Bucket(0);
  EXPECT_EQ("0", value.str());

  value.str("");
  value << Pow2Bucket(2);
  EXPECT_EQ("2", value.str());

  value.str("");
  value << Pow2Bucket(0xFFFFFFFFu);
  EXPECT_EQ("2147483648", value.str());

  value.str("");
  value << Pow2Bucket(0x80000000u);
  EXPECT_EQ("2147483648", value.str());

  value.str("");
  value << Pow2Bucket(0x7FFFFFFF);
  EXPECT_EQ("1073741824", value.str());
}

}  // namespace metrics
}  // namespace wvcdm
