//
// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//

//#define LOG_NDEBUG 0
#define LOG_TAG "WVCdm"
#include <utils/Log.h>

#include "WVDrmFactory.h"

#include "android-base/properties.h"
#include "wv_cdm_constants.h"
#include "wv_content_decryption_module.h"
#include "HidlTypes.h"
#include "WVCDMSingleton.h"
#include "WVDrmPlugin.h"
#include "WVUUID.h"

namespace wvdrm {
namespace hardware {
namespace drm {
namespace V1_2 {
namespace widevine {

WVGenericCryptoInterface WVDrmFactory::sOemCryptoInterface;

Return<bool> WVDrmFactory::isCryptoSchemeSupported(
    const hidl_array<uint8_t, 16>& uuid) {
  return isWidevineUUID(uuid.data());
}

Return<bool> WVDrmFactory::isCryptoSchemeSupported_1_2(
        const hidl_array<uint8_t, 16>& uuid,
        const hidl_string& initDataType,
        SecurityLevel level) {

    if (!isWidevineUUID(uuid.data()) || !isContentTypeSupported(initDataType)) {
        return false;
    }

    if (wvcdm::WvContentDecryptionModule::IsSecurityLevelSupported(
                    wvcdm::kSecurityLevelL1)) {
        if (wvcdm::WvContentDecryptionModule::IsAudio(initDataType)) {
            if (level < SecurityLevel::HW_SECURE_ALL) {
                return true;
            }
        } else {
            return true;
        }
    }
    return level <= SecurityLevel::SW_SECURE_DECODE;
}

Return<bool> WVDrmFactory::isContentTypeSupported(
    const hidl_string& initDataType) {
  return wvcdm::WvContentDecryptionModule::IsSupported(initDataType.c_str());
}

Return<void> WVDrmFactory::createPlugin(
    const hidl_array<uint8_t, 16>& uuid,
    const hidl_string& appPackageName,
    createPlugin_cb _hidl_cb) {

  WVDrmPlugin *plugin = NULL;
  if (!isCryptoSchemeSupported(uuid.data())) {
    ALOGE("Widevine Drm HAL: failed to create drm plugin, " \
        "invalid crypto scheme");
    _hidl_cb(Status::ERROR_DRM_CANNOT_HANDLE, plugin);
    return Void();
  }

  if (!isBlankAppPackageNameAllowed() && appPackageName.empty()) {
    ALOGE("Widevine Drm HAL: Failed to create DRM Plugin, blank App Package "
          "Name disallowed.");
    _hidl_cb(Status::ERROR_DRM_CANNOT_HANDLE, plugin);
    return Void();
  }

  plugin = new WVDrmPlugin(getCDM(), appPackageName.c_str(),
                           &sOemCryptoInterface, areSpoidsEnabled());
  _hidl_cb(Status::OK, plugin);
  return Void();
}

bool WVDrmFactory::areSpoidsEnabled() {
  return firstApiLevel() >= 26;  // Android O
}

bool WVDrmFactory::isBlankAppPackageNameAllowed() {
  return firstApiLevel() < 29;  // Android Q
}

int32_t WVDrmFactory::firstApiLevel() {
  // Check what this device's first API level was.
  int32_t firstApiLevel =
      android::base::GetIntProperty<int32_t>("ro.product.first_api_level", 0);
  if (firstApiLevel == 0) {
    // First API Level is 0 on factory ROMs, but we can assume the current SDK
    // version is the first if it's a factory ROM.
    firstApiLevel =
        android::base::GetIntProperty<int32_t>("ro.build.version.sdk", 0);
  }
  return firstApiLevel;
}


}  // namespace widevine
}  // namespace V1_2
}  // namespace drm
}  // namespace hardware
}  // namespace wvdrm
