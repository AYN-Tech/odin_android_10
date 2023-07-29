//
// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//

#ifndef WV_DRM_FACTORY_H_
#define WV_DRM_FACTORY_H_

#include "media/drm/DrmAPI.h"
#include "media/stagefright/foundation/ABase.h"
#include "utils/Errors.h"
#include "WVGenericCryptoInterface.h"

namespace wvdrm {

class WVDrmFactory : public android::DrmFactory {
 public:
  WVDrmFactory() {}
  virtual ~WVDrmFactory() {}

  virtual bool isCryptoSchemeSupported(const uint8_t uuid[16]);

  virtual bool isContentTypeSupported(const android::String8 &initDataType);

  virtual android::status_t createDrmPlugin(const uint8_t uuid[16],
                                            android::DrmPlugin** plugin);

 private:
  DISALLOW_EVIL_CONSTRUCTORS(WVDrmFactory);

  static WVGenericCryptoInterface sOemCryptoInterface;
};

} // namespace wvdrm

#endif // WV_DRM_FACTORY_H_
