//
// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//

#include "gtest/gtest.h"
#include "WVCreatePluginFactories.h"

namespace wvdrm {
namespace hardware {
namespace drm {
namespace V1_2 {
namespace widevine {

using ::android::sp;

using namespace android;

TEST(CreatePluginFactoriesTest, CreatesDrmFactory) {
  sp<IDrmFactory> factory(createDrmFactory());

  EXPECT_NE((IDrmFactory*)NULL, factory.get()) <<
      "createDrmFactory() returned null";
}

TEST(CreatePluginFactoriesTest, CreatesCryptoFactory) {
  sp<ICryptoFactory> factory(createCryptoFactory());

  EXPECT_NE((ICryptoFactory*)NULL, factory.get()) <<
      "createCryptoFactory() returned null";
}

}  // namespace widevine
}  // namespace V1_2
}  // namespace drm
}  // namespace hardware
}  // namespace wvdrm
