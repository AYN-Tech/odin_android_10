//
// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//

#include "gtest/gtest.h"
#include "WVCreatePluginFactories.h"

#include <memory>

using namespace android;

TEST(CreatePluginFactoriesTest, CreatesDrmFactory) {
  std::unique_ptr<DrmFactory> factory(createDrmFactory());

  EXPECT_NE((DrmFactory*)NULL, factory.get()) <<
      "createDrmFactory() returned null";
}

TEST(CreatePluginFactoriesTest, CreatesCryptoFactory) {
  std::unique_ptr<CryptoFactory> factory(createCryptoFactory());

  EXPECT_NE((CryptoFactory*)NULL, factory.get()) <<
      "createCryptoFactory() returned null";
}
