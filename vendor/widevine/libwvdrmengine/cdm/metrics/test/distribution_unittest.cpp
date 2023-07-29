// Copyright 2017 Google Inc. All Rights Reserved.
//
// Unit tests for Distribution.

#include <float.h>

#include "distribution.h"

#include "gtest/gtest.h"

namespace wvcdm {
namespace metrics {

TEST(DistributionTest, NoValuesRecorded) {
  Distribution distribution;
  EXPECT_EQ(FLT_MAX, distribution.Min());
  EXPECT_EQ(-FLT_MAX, distribution.Max());
  EXPECT_EQ(0, distribution.Mean());
  EXPECT_EQ(0u, distribution.Count());
  EXPECT_EQ(0, distribution.Variance());
}

TEST(DistributionTest, OneValueRecorded) {
  Distribution distribution;
  distribution.Record(5.0);
  EXPECT_EQ(5, distribution.Min());
  EXPECT_EQ(5, distribution.Max());
  EXPECT_EQ(5, distribution.Mean());
  EXPECT_EQ(1u, distribution.Count());
  EXPECT_EQ(0, distribution.Variance());
}

TEST(DistributionTest, MultipleValuesRecorded) {
  Distribution distribution;
  distribution.Record(5.0);
  distribution.Record(10.0);
  distribution.Record(15.0);
  EXPECT_EQ(5, distribution.Min());
  EXPECT_EQ(15, distribution.Max());
  EXPECT_EQ(10, distribution.Mean());
  EXPECT_EQ(3u, distribution.Count());
  EXPECT_NEAR(16.6667, distribution.Variance(), 0.0001);
}

}  // namespace metrics
}  // namespace wvcdm

