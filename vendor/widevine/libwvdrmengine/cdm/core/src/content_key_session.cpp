// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#include "content_key_session.h"
#include "crypto_key.h"
#include "crypto_session.h"
#include "log.h"
#include "wv_cdm_constants.h"

namespace wvcdm {

// Generate Derived Keys for ContentKeySession
OEMCryptoResult ContentKeySession::GenerateDerivedKeys(
    const std::string& message) {
  std::string mac_deriv_message;
  std::string enc_deriv_message;
  GenerateMacContext(message, &mac_deriv_message);
  GenerateEncryptContext(message, &enc_deriv_message);

  LOGV("GenerateDerivedKeys: id=%ld", (uint32_t)oec_session_id_);
  OEMCryptoResult sts;
  M_TIME(sts = OEMCrypto_GenerateDerivedKeys(
             oec_session_id_,
             reinterpret_cast<const uint8_t*>(mac_deriv_message.data()),
             mac_deriv_message.size(),
             reinterpret_cast<const uint8_t*>(enc_deriv_message.data()),
             enc_deriv_message.size()),
         metrics_, oemcrypto_generate_derived_keys_, sts);
  if (OEMCrypto_SUCCESS != sts) {
    LOGE("GenerateDerivedKeys: OEMCrypto_GenerateDerivedKeys error=%d", sts);
  }

  return sts;
}

// Generate Derived Keys (from session key) for ContentKeySession
OEMCryptoResult ContentKeySession::GenerateDerivedKeys(
    const std::string& message,
    const std::string& session_key) {
  std::string mac_deriv_message;
  std::string enc_deriv_message;
  GenerateMacContext(message, &mac_deriv_message);
  GenerateEncryptContext(message, &enc_deriv_message);

  LOGV("GenerateDerivedKeys: id=%ld", (uint32_t)oec_session_id_);
  OEMCryptoResult sts;
  M_TIME(
      sts = OEMCrypto_DeriveKeysFromSessionKey(
          oec_session_id_, reinterpret_cast<const uint8_t*>(session_key.data()),
          session_key.size(),
          reinterpret_cast<const uint8_t*>(mac_deriv_message.data()),
          mac_deriv_message.size(),
          reinterpret_cast<const uint8_t*>(enc_deriv_message.data()),
          enc_deriv_message.size()),
      metrics_, oemcrypto_derive_keys_from_session_key_, sts);

  if (OEMCrypto_SUCCESS != sts) {
    LOGE("GenerateDerivedKeys: OEMCrypto_DeriveKeysFromSessionKey err=%d", sts);
  }

  return sts;
}

// Load Keys for ContentKeySession
OEMCryptoResult ContentKeySession::LoadKeys(
    const std::string& message, const std::string& signature,
    const std::string& mac_key_iv, const std::string& mac_key,
    const std::vector<CryptoKey>& keys,
    const std::string& provider_session_token, CdmCipherMode* cipher_mode,
    const std::string& srm_requirement) {
  return LoadKeysAsLicenseType(message, signature, mac_key_iv, mac_key, keys,
                               provider_session_token, cipher_mode,
                               srm_requirement, OEMCrypto_ContentLicense);
}

// Select Key for ContentKeySession
OEMCryptoResult ContentKeySession::SelectKey(const std::string& key_id,
                                             CdmCipherMode cipher_mode) {
  // Crypto session lock already locked.
  if (!cached_key_id_.empty() && cached_key_id_ == key_id &&
      cipher_mode_ == cipher_mode) {
    // Already using the desired key and cipher mode.
    return OEMCrypto_SUCCESS;
  }

  cached_key_id_ = key_id;
  cipher_mode_ = cipher_mode;

  const uint8_t* key_id_string =
      reinterpret_cast<const uint8_t*>(cached_key_id_.data());

  OEMCryptoResult sts;
  M_TIME(sts = OEMCrypto_SelectKey(oec_session_id_, key_id_string,
                                   cached_key_id_.size(),
                                   ToOEMCryptoCipherMode(cipher_mode)),
         metrics_, oemcrypto_select_key_, sts);

  if (OEMCrypto_SUCCESS != sts) {
    cached_key_id_.clear();
  }
  return sts;
}

// Decrypt for ContentKeySession
OEMCryptoResult ContentKeySession::Decrypt(
    const CdmDecryptionParameters& params,
    OEMCrypto_DestBufferDesc& buffer_descriptor,
    OEMCrypto_CENCEncryptPatternDesc& pattern_descriptor) {
  OEMCryptoResult sts;
  M_TIME(sts = OEMCrypto_DecryptCENC(
             oec_session_id_, params.encrypt_buffer, params.encrypt_length,
             params.is_encrypted, &(*params.iv).front(), params.block_offset,
             &buffer_descriptor, &pattern_descriptor, params.subsample_flags),
         metrics_, oemcrypto_decrypt_cenc_, sts,
         metrics::Pow2Bucket(params.encrypt_length));
  return sts;
}

OEMCryptoResult ContentKeySession::LoadKeysAsLicenseType(
    const std::string& message, const std::string& signature,
    const std::string& mac_key_iv, const std::string& mac_key,
    const std::vector<CryptoKey>& keys,
    const std::string& provider_session_token, CdmCipherMode* cipher_mode,
    const std::string& srm_requirement, OEMCrypto_LicenseType license_type) {
  const uint8_t* msg = reinterpret_cast<const uint8_t*>(message.data());
  cached_key_id_.clear();
  bool valid_mac_keys =
      mac_key.length() >= MAC_KEY_SIZE && mac_key_iv.length() >= KEY_IV_SIZE;
  OEMCrypto_Substring enc_mac_key =
      GetSubstring(message, mac_key, !valid_mac_keys);
  OEMCrypto_Substring enc_mac_key_iv =
      GetSubstring(message, mac_key_iv, !valid_mac_keys);
  if (!valid_mac_keys) LOGV("enc_mac_key not set");
  std::vector<OEMCrypto_KeyObject> load_keys(keys.size());
  std::vector<OEMCryptoCipherMode> cipher_modes(keys.size());
  for (size_t i = 0; i < keys.size(); ++i) {
    const CryptoKey* ki = &keys[i];
    OEMCrypto_KeyObject* ko = &load_keys[i];
    ko->key_id = GetSubstring(message, ki->key_id());
    ko->key_data_iv = GetSubstring(message, ki->key_data_iv());
    ko->key_data = GetSubstring(message, ki->key_data());
    bool has_key_control = ki->HasKeyControl();
    ko->key_control_iv =
        GetSubstring(message, ki->key_control_iv(), !has_key_control);
    ko->key_control =
        GetSubstring(message, ki->key_control(), !has_key_control);
    if (!has_key_control) {
      LOGE("For key %d: XXX key has no control block. size=%d", i,
           ki->key_control().length());
    }
    cipher_modes[i] = ToOEMCryptoCipherMode(ki->cipher_mode());

    // TODO(jfore): Is returning the cipher needed. If not drop this.
    *cipher_mode = ki->cipher_mode();
  }

  OEMCrypto_Substring pst = GetSubstring(message, provider_session_token);
  OEMCrypto_Substring srm_req = GetSubstring(message, srm_requirement);

  LOGV("id=%ld", (uint32_t)oec_session_id_);
  OEMCryptoResult sts;
  OEMCrypto_KeyObject* key_array_ptr = NULL;
  if (keys.size() > 0) key_array_ptr = &load_keys[0];
  OEMCryptoCipherMode* cipher_mode_ptr = NULL;
  if (keys.size() > 0) cipher_mode_ptr = &cipher_modes[0];
  M_TIME(sts = ::OEMCrypto_LoadKeys_Back_Compat(
             oec_session_id_, msg, message.length(),
             reinterpret_cast<const uint8_t*>(signature.data()),
             signature.length(), enc_mac_key_iv, enc_mac_key, keys.size(),
             key_array_ptr, pst, srm_req, license_type, cipher_mode_ptr),
         metrics_, oemcrypto_load_keys_, sts);
  return sts;
}

}  // namespace wvcdm
