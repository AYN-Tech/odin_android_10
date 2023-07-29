//
// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//

#ifndef WV_DRM_FACTORY_H_
#define WV_DRM_FACTORY_H_

#include "HidlTypes.h"
#include "WVGenericCryptoInterface.h"
#include "WVTypes.h"

namespace wvdrm {
namespace hardware {
namespace drm {
namespace V1_2 {
namespace widevine {

struct WVDrmFactory : public IDrmFactory {
  WVDrmFactory() {}
  virtual ~WVDrmFactory() {}

  Return<bool> isCryptoSchemeSupported(const hidl_array<uint8_t, 16>& uuid)
      override;

  Return<bool> isCryptoSchemeSupported_1_2(const hidl_array<uint8_t, 16>& uuid,
                                           const hidl_string& mimeType,
                                           SecurityLevel level)
      override;

  Return<bool> isContentTypeSupported(const hidl_string &mimeType)
      override;

  Return<void> createPlugin(
      const hidl_array<uint8_t, 16>& uuid,
      const hidl_string& appPackageName,
      createPlugin_cb _hidl_cb) override;

 private:
  WVDRM_DISALLOW_COPY_AND_ASSIGN(WVDrmFactory);

  static WVGenericCryptoInterface sOemCryptoInterface;

  static bool areSpoidsEnabled();
  static bool isBlankAppPackageNameAllowed();
  static int32_t firstApiLevel();

  friend class WVDrmFactoryTestPeer;
};

extern "C" IDrmFactory* HIDL_FETCH_IDrmFactory(const char* name);

}  // namespace widevine
}  // namespace V1_2
}  // namespace drm
}  // namespace hardware
}  // namespace wvdrm

#endif // WV_DRM_FACTORY_H_
