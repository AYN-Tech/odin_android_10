//
// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//

#ifndef WV_CRYPTO_PLUGIN_H_
#define WV_CRYPTO_PLUGIN_H_

#include <stdint.h>

#include "utils/StrongPointer.h"
#include "utils/Vector.h"

#include "media/hardware/CryptoAPI.h"
#include "media/stagefright/foundation/ABase.h"
#include "media/stagefright/foundation/AString.h"
#include "wv_content_decryption_module.h"

namespace wvdrm {

class WVCryptoPlugin : public android::CryptoPlugin {
 public:
  WVCryptoPlugin(const void* data, size_t size,
                 const android::sp<wvcdm::WvContentDecryptionModule>& cdm);
  virtual ~WVCryptoPlugin() {}

  virtual bool requiresSecureDecoderComponent(const char* mime) const;

  virtual void notifyResolution(uint32_t width, uint32_t height);

  virtual android::status_t setMediaDrmSession(
      const android::Vector<uint8_t>& sessionId);

  virtual ssize_t decrypt(bool secure, const uint8_t key[16],
                          const uint8_t iv[16], Mode mode, const Pattern &pattern,
                          const void* srcPtr,
                          const SubSample* subSamples, size_t numSubSamples,
                          void* dstPtr, android::AString* errorDetailMsg);

 private:
  DISALLOW_EVIL_CONSTRUCTORS(WVCryptoPlugin);

  android::sp<wvcdm::WvContentDecryptionModule> const mCDM;

  bool mTestMode;
  wvcdm::CdmSessionId mSessionId;

  wvcdm::CdmSessionId configureTestMode(const void* data, size_t size);
  android::status_t attemptDecrypt(
      const wvcdm::CdmDecryptionParameters& params,
      bool haveEncryptedSubsamples, android::AString* errorDetailMsg);
  static void incrementIV(uint64_t increaseBy, std::vector<uint8_t>* ivPtr);
};

} // namespace wvdrm

#endif // WV_CRYPTO_PLUGIN_H_
