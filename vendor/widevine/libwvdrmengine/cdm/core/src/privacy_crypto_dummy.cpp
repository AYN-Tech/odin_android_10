// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
// Description:
//   Dummy version of privacy crypto classes for systems which
//   can't tolerate BoringSSL or OpenSSL as a dependency.
//

#include "privacy_crypto.h"

#include "log.h"

#ifdef __APPLE__
# include <CommonCrypto/CommonDigest.h>
# define SHA256 CC_SHA256
# define SHA256_DIGEST_LENGTH CC_SHA256_DIGEST_LENGTH
# define MD5 CC_MD5
# define MD5_DIGEST_LENGTH CC_MD5_DIGEST_LENGTH
#else
# error "No hash algorithm known for this platform."
#endif

namespace wvcdm {

AesCbcKey::AesCbcKey() {}

AesCbcKey::~AesCbcKey() {}

bool AesCbcKey::Init(const std::string& key) { return false; }

bool AesCbcKey::Encrypt(const std::string& in, std::string* out,
                        std::string* iv) {
  return false;
}

RsaPublicKey::RsaPublicKey() {}

RsaPublicKey::~RsaPublicKey() {}

bool RsaPublicKey::Init(const std::string& serialized_key) { return false; }

bool RsaPublicKey::Encrypt(const std::string& clear_message,
                           std::string* encrypted_message) {
  return false;
}

bool RsaPublicKey::VerifySignature(const std::string& message,
                                   const std::string& signature) {
  return false;
}


bool ExtractExtensionValueFromCertificate(const std::string& cert,
                                          const std::string& extension_oid,
                                          size_t cert_index, uint32_t* value) {
  LOGE("ExtractExtensionValueFromCertificate: Not supported in this build.");
  return false;
}

std::string Md5Hash(const std::string& data) {
  std::string hash(MD5_DIGEST_LENGTH, '\0');
  MD5(data.data(), data.size(), reinterpret_cast<uint8_t*>(&hash[0]));
  return hash;
}

std::string Sha256Hash(const std::string& data) {
  std::string hash(SHA256_DIGEST_LENGTH, '\0');
  SHA256(data.data(), data.size(), reinterpret_cast<uint8_t*>(&hash[0]));
  return hash;
}

}  // namespace wvcdm
