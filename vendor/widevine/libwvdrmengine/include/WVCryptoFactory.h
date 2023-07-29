//
// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//

#ifndef WV_CRYPTO_FACTORY_H_
#define WV_CRYPTO_FACTORY_H_

#include "media/hardware/CryptoAPI.h"
#include "media/stagefright/foundation/ABase.h"
#include "utils/Errors.h"

namespace wvdrm {

class WVCryptoFactory : public android::CryptoFactory {
 public:
  WVCryptoFactory() {}
  virtual ~WVCryptoFactory() {}

  virtual bool isCryptoSchemeSupported(const uint8_t uuid[16]) const;

  virtual android::status_t createPlugin(const uint8_t uuid[16],
                                         const void* data, size_t size,
                                         android::CryptoPlugin** plugin);

 private:
  DISALLOW_EVIL_CONSTRUCTORS(WVCryptoFactory);
};

} // namespace wvdrm

#endif // WV_CRYPTO_FACTORY_H_
