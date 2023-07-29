// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#include "entitlement_key_session.h"

#include "crypto_key.h"
#include "crypto_session.h"
#include "log.h"


namespace wvcdm {
EntitlementKeySession::EntitlementKeySession(CryptoSessionId oec_session_id,
                                             metrics::CryptoMetrics* metrics)
    : ContentKeySession(oec_session_id, metrics), entitled_keys_() {}

OEMCryptoResult EntitlementKeySession::LoadKeys(
    const std::string& message, const std::string& signature,
    const std::string& mac_key_iv, const std::string& mac_key,
    const std::vector<CryptoKey>& keys,
    const std::string& provider_session_token, CdmCipherMode* cipher_mode,
    const std::string& srm_requirement) {
  // Call our superclass's LoadKeysAsLicenseType(), but set the license type to
  // OEMCrypto_EntitlementLicense.
  return ContentKeySession::LoadKeysAsLicenseType(
      message, signature, mac_key_iv, mac_key, keys, provider_session_token,
      cipher_mode, srm_requirement, OEMCrypto_EntitlementLicense);
}

OEMCryptoResult EntitlementKeySession::LoadEntitledContentKeys(
    const std::vector<CryptoKey>& keys) {
  // The array |keys| contains new content keys, plus entitlement key ids for
  // those content keys.
  for (size_t i = 0; i < keys.size(); ++i) {
    // Since OEMCrypto only supports loading one entitled key per entitlement
    // key at a time, (b/110266851) we defer loading until SelectKey() tells us
    // which entitled key we actually need. For fast lookup later, we index the
    // entitled keys by their ID.
    const CryptoKey& input_key = keys[i];
    entitled_keys_[input_key.key_id()] = input_key;
  }

  return OEMCrypto_SUCCESS;
}

OEMCryptoResult EntitlementKeySession::SelectKey(const std::string& key_id,
                                                 CdmCipherMode cipher_mode) {
  if (entitled_keys_.find(key_id) == entitled_keys_.end()) {
    LOGE("Unknown entitled key ID selected.");
    return OEMCrypto_ERROR_NO_CONTENT_KEY;
  }

  CryptoKey entitled_content_key = entitled_keys_[key_id];
  if (current_loaded_content_keys_[entitled_content_key
                                        .entitlement_key_id()] != key_id) {
    // Before the key can be selected, it must be loaded under its associated
    // entitlement key. This could, in theory, be done ahead of time during
    // LoadEntitledContentKeys(), but OEMCrypto v14 only supports one content
    // key per entitlement key at a time, (b/110266851) so we must swap out for
    // the correct key every time the current content key id has changed for an
    // entitlement key.
    std::string message;
    OEMCrypto_EntitledContentKeyObject entitled_key =
        MakeOecEntitledKey(entitled_content_key, message);
    OEMCryptoResult result = OEMCrypto_SUCCESS;
    M_TIME(
        result = OEMCrypto_LoadEntitledContentKeys(
            oec_session_id_, reinterpret_cast<const uint8_t*>(message.data()),
            message.size(), 1, &entitled_key),
        metrics_, oemcrypto_load_entitled_keys_, result);
    if (result != OEMCrypto_SUCCESS) {
      LOGE("SelectKey: OEMCrypto_LoadEntitledContentKeys error=%d", result);
      return result;
    }

    current_loaded_content_keys_[entitled_content_key.entitlement_key_id()] =
        key_id;
  }
  return ContentKeySession::SelectKey(key_id, cipher_mode);
}

OEMCrypto_EntitledContentKeyObject EntitlementKeySession::MakeOecEntitledKey(
    const CryptoKey& input_key, std::string& message) {
  OEMCrypto_EntitledContentKeyObject output_key;
  message.clear();

  const std::string& entitlement_key_id = input_key.entitlement_key_id();
  const std::string& key_id = input_key.key_id();
  const std::string& key_data_iv = input_key.key_data_iv();
  const std::string& key_data = input_key.key_data();

  output_key.entitlement_key_id.offset = message.size();
  message += entitlement_key_id;
  output_key.entitlement_key_id.length = entitlement_key_id.size();

  output_key.content_key_id.offset = message.size();
  message += key_id;
  output_key.content_key_id.length = key_id.size();

  output_key.content_key_data_iv.offset = message.size();
  message += key_data_iv;
  output_key.content_key_data_iv.length = key_data_iv.size();

  output_key.content_key_data.offset = message.size();
  message += key_data;
  output_key.content_key_data.length = key_data.size();

  return output_key;
}

}  // namespace wvcdm
