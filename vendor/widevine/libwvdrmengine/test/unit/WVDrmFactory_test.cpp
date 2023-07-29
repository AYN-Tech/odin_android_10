/*
 * Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
 * source code may only be used and distributed under the Widevine Master
 * License Agreement.
 */

#include "android-base/properties.h"
#include "cutils/properties.h"
#include "gtest/gtest.h"
#include "HidlTypes.h"
#include "WVDrmFactory.h"

namespace wvdrm {
namespace hardware {
namespace drm {
namespace V1_2 {
namespace widevine {

using namespace android;

const uint8_t kWidevineUUID[16] = {
  0xED,0xEF,0x8B,0xA9,0x79,0xD6,0x4A,0xCE,
  0xA3,0xC8,0x27,0xDC,0xD5,0x1D,0x21,0xED
};

const uint8_t kOldNetflixWidevineUUID[16] = {
  0x29,0x70,0x1F,0xE4,0x3C,0xC7,0x4A,0x34,
  0x8C,0x5B,0xAE,0x90,0xC7,0x43,0x9A,0x47
};

const uint8_t kUnknownUUID[16] = {
  0x6A,0x7F,0xAA,0xB0,0x83,0xC7,0x9E,0x20,
  0x08,0xBC,0xEF,0x32,0x34,0x1A,0x9A,0x26
};

class WVDrmFactoryTestPeer : public WVDrmFactory {
 public:
  using WVDrmFactory::areSpoidsEnabled;
  using WVDrmFactory::isBlankAppPackageNameAllowed;
};

TEST(WVDrmFactoryTest, SupportsSupportedCryptoSchemes) {
  WVDrmFactory factory;

  EXPECT_TRUE(factory.isCryptoSchemeSupported(kWidevineUUID)) <<
      "WVPluginFactory does not support Widevine's UUID";

  EXPECT_TRUE(factory.isCryptoSchemeSupported(kOldNetflixWidevineUUID)) <<
      "WVPluginFactory does not support the old Netflix Widevine UUID";
}

TEST(WVDrmFactoryTest, DoesNotSupportUnsupportedCryptoSchemes) {
  WVDrmFactory factory;

  EXPECT_FALSE(factory.isCryptoSchemeSupported(kUnknownUUID)) <<
      "WVPluginFactory incorrectly claims to support an unknown UUID";
}

TEST(WVDrmFactoryTest, SupportsSupportedContainerFormats) {
  WVDrmFactory factory;

  EXPECT_TRUE(factory.isContentTypeSupported(hidl_string("video/mp4"))) <<
      "WVPluginFactory does not support ISO-BMFF video";

  EXPECT_TRUE(factory.isContentTypeSupported(hidl_string("audio/mp4"))) <<
      "WVPluginFactory does not support ISO-BMFF audio";

  EXPECT_TRUE(factory.isContentTypeSupported(hidl_string("video/webm"))) <<
      "WVPluginFactory does not support WebM video";

  EXPECT_TRUE(factory.isContentTypeSupported(hidl_string("audio/webm"))) <<
      "WVPluginFactory does not support WebM audio";
}

TEST(WVDrmFactoryTest, DoesNotSupportUnsupportedContainerFormats) {
  WVDrmFactory factory;

  // Taken from Encoding.com's list of the most common internet video MIME-types
  EXPECT_FALSE(factory.isContentTypeSupported(hidl_string("video/x-matroska"))) <<
      "WVPluginFactory incorrectly claims to support Matroska";

  EXPECT_FALSE(factory.isContentTypeSupported(hidl_string("video/x-flv"))) <<
      "WVPluginFactory incorrectly claims to support Flash Video";

  EXPECT_FALSE(factory.isContentTypeSupported(hidl_string("application/x-mpegURL"))) <<
      "WVPluginFactory incorrectly claims to support m3u8 Indexes";

  EXPECT_FALSE(factory.isContentTypeSupported(hidl_string("video/MP2T"))) <<
      "WVPluginFactory incorrectly claims to support MPEG-2 TS";

  EXPECT_FALSE(factory.isContentTypeSupported(hidl_string("video/3gpp"))) <<
      "WVPluginFactory incorrectly claims to support 3GP Mobile";

  EXPECT_FALSE(factory.isContentTypeSupported(hidl_string("video/quicktime"))) <<
      "WVPluginFactory incorrectly claims to support Quicktime";

  EXPECT_FALSE(factory.isContentTypeSupported(hidl_string("video/x-msvideo"))) <<
      "WVPluginFactory incorrectly claims to support AVI";

  EXPECT_FALSE(factory.isContentTypeSupported(hidl_string("video/x-ms-wmv"))) <<
      "WVPluginFactory incorrectly claims to support WMV";
}

TEST(WVDrmFactoryTest, SupportsSupportedCryptoSchemeWithLevel) {
  WVDrmFactory factory;

  EXPECT_TRUE(factory.isCryptoSchemeSupported_1_2(kWidevineUUID, hidl_string("video/mp4"),
                  android::hardware::drm::V1_1::SecurityLevel::SW_SECURE_CRYPTO)) <<
      "WVPluginFactory does not support Widevine's UUID for video/mp4 and SW_SECURE_CRYPTO";
}

TEST(WVDrmFactoryTest, CalculatesSpoidUseCorrectly) {
  WVDrmFactoryTestPeer factory;

  int32_t firstApiLevel =
      android::base::GetIntProperty<int32_t>("ro.product.first_api_level", 0);
  if (firstApiLevel == 0) {
    // First API Level is 0 on factory ROMs, but we can assume the current SDK
    // version is the first if it's a factory ROM.
    firstApiLevel =
        android::base::GetIntProperty<int32_t>("ro.build.version.sdk", 0);
  }
  bool shouldUseSpoids = (firstApiLevel >= 26);  // Android O

  EXPECT_EQ(shouldUseSpoids, factory.areSpoidsEnabled()) <<
      "WVDrmFactory calculated a different SPOID state than expected.";
}

TEST(WVDrmFactoryTest, CalculatesBlankAppPackageNamePermissibilityCorrectly) {
  WVDrmFactoryTestPeer factory;

  int32_t firstApiLevel =
      android::base::GetIntProperty<int32_t>("ro.product.first_api_level", 0);
  if (firstApiLevel == 0) {
    // First API Level is 0 on factory ROMs, but we can assume the current SDK
    // version is the first if it's a factory ROM.
    firstApiLevel =
        android::base::GetIntProperty<int32_t>("ro.build.version.sdk", 0);
  }
  bool shouldAllow = (firstApiLevel < 29);  // Android Q

  EXPECT_EQ(shouldAllow, factory.isBlankAppPackageNameAllowed()) <<
      "WVDrmFactory calculated a different Blank App Package Name state than expected.";
}

}  // namespace widevine
}  // namespace V1_2
}  // namespace drm
}  // namespace hardware
}  // namespace wvdrm
