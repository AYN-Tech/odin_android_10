// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#ifndef WVCDM_CORE_ENTITLEMENT_KEY_SESSION_H_
#define WVCDM_CORE_ENTITLEMENT_KEY_SESSION_H_

#include <map>
#include <string>

#include "OEMCryptoCENC.h"
#include "content_key_session.h"
#include "crypto_key.h"
#include "metrics_collections.h"

namespace wvcdm {

class EntitlementKeySession : public ContentKeySession {
 public:
  EntitlementKeySession(CryptoSessionId oec_session_id,
                        metrics::CryptoMetrics* metrics);
  ~EntitlementKeySession() override {}

  KeySessionType Type() override { return kEntitlement; }

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
      const std::vector<CryptoKey>& keys) override;
  OEMCryptoResult SelectKey(const std::string& key_id,
                            CdmCipherMode cipher_mode) override;

 private:
  // The message is populated with the fields of the provided CryptoKey and the
  // returned object reflects the offsets and lengths into that message for each
  // field.
  OEMCrypto_EntitledContentKeyObject MakeOecEntitledKey(
      const CryptoKey& input_key, std::string& message);

  // Find the CryptoKey for the given entitled content key id.
  std::map<KeyId, CryptoKey> entitled_keys_;
  // Find the current entitled content key id for the given entitlement key id.
  std::map<KeyId, KeyId> current_loaded_content_keys_;
};

}  // namespace wvcdm

#endif  // WVCDM_CORE_ENTITLEMENT_KEY_SESSION_H_
