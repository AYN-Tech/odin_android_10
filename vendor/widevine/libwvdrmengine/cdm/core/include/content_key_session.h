// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#ifndef WVCDM_CORE_CONTENT_KEY_SESSION_H_
#define WVCDM_CORE_CONTENT_KEY_SESSION_H_

#include "key_session.h"
#include "metrics_collections.h"
#include "timer_metric.h"

namespace wvcdm {

class ContentKeySession : public KeySession {
 public:
  ContentKeySession(CryptoSessionId oec_session_id,
                    metrics::CryptoMetrics* metrics)
      : KeySession(metrics),
        oec_session_id_(oec_session_id),
        cipher_mode_(kCipherModeCtr) {}
  ~ContentKeySession() override {}

  KeySessionType Type() override { return kDefault; }

  // Generate Derived Keys for ContentKeySession
  OEMCryptoResult GenerateDerivedKeys(const std::string& message) override;

  // Generate Derived Keys (from session key) for ContentKeySession
  OEMCryptoResult GenerateDerivedKeys(const std::string& message,
                                      const std::string& session_key) override;

  // Load Keys for ContentKeySession
  OEMCryptoResult LoadKeys(const std::string& message,
                           const std::string& signature,
                           const std::string& mac_key_iv,
                           const std::string& mac_key,
                           const std::vector<CryptoKey>& keys,
                           const std::string& provider_session_token,
                           CdmCipherMode* cipher_mode,
                           const std::string& srm_requirement) override;

  OEMCryptoResult LoadEntitledContentKeys(
      const std::vector<CryptoKey>&) override {
    return OEMCrypto_ERROR_INVALID_CONTEXT;
  }

  // Select Key for ContentKeySession
  OEMCryptoResult SelectKey(const std::string& key_id,
                            CdmCipherMode cipher_mode) override;

  // Decrypt for ContentKeySession
  OEMCryptoResult Decrypt(
      const CdmDecryptionParameters& params,
      OEMCrypto_DestBufferDesc& buffer_descriptor,
      OEMCrypto_CENCEncryptPatternDesc& pattern_descriptor) override;

 protected:
  virtual OEMCryptoResult LoadKeysAsLicenseType(
      const std::string& message, const std::string& signature,
      const std::string& mac_key_iv, const std::string& mac_key,
      const std::vector<CryptoKey>& keys,
      const std::string& provider_session_token, CdmCipherMode* cipher_mode,
      const std::string& srm_requirement, OEMCrypto_LicenseType license_type);

  CryptoSessionId oec_session_id_;

 private:
  KeyId cached_key_id_;
  CdmCipherMode cipher_mode_;
};

}  // namespace wvcdm

#endif  // WVCDM_CORE_CONTENT_KEY_SESSION_H_
