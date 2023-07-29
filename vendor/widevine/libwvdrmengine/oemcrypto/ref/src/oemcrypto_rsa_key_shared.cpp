// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
//  Reference implementation of OEMCrypto APIs
//
#include "oemcrypto_rsa_key_shared.h"

#include <assert.h>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <openssl/x509.h>

#include "log.h"

namespace wvoec_ref {

void dump_boringssl_error() {
  int count = 0;
  while (unsigned long err = ERR_get_error()) {
    count++;
    char buffer[120];
    ERR_error_string_n(err, buffer, sizeof(buffer));
    LOGE("BoringSSL Error %d -- %lu -- %s", count, err, buffer);
  }
  LOGE("Reported %d BoringSSL Errors", count);
}

void RSA_shared_ptr::reset() {
  if (rsa_key_ && key_owned_) {
    RSA_free(rsa_key_);
  }
  key_owned_ = false;
  rsa_key_ = NULL;
}

bool RSA_shared_ptr::LoadPkcs8RsaKey(const uint8_t* buffer, size_t length) {
  assert(buffer != NULL);
  reset();
  uint8_t* pkcs8_rsa_key = const_cast<uint8_t*>(buffer);
  BIO* bio = BIO_new_mem_buf(pkcs8_rsa_key, length);
  if (bio == NULL) {
    LOGE("[LoadPkcs8RsaKey(): Could not allocate bio buffer]");
    return false;
  }
  bool success = true;
  PKCS8_PRIV_KEY_INFO* pkcs8_pki = d2i_PKCS8_PRIV_KEY_INFO_bio(bio, NULL);
  if (pkcs8_pki == NULL) {
    BIO_reset(bio);
    pkcs8_pki = d2i_PKCS8_PRIV_KEY_INFO_bio(bio, NULL);
    if (pkcs8_pki == NULL) {
      LOGE("[LoadPkcs8RsaKey(): d2i_PKCS8_PRIV_KEY_INFO_bio returned NULL]");
      dump_boringssl_error();
      success = false;
    }
  }
  EVP_PKEY* evp = NULL;
  if (success) {
    evp = EVP_PKCS82PKEY(pkcs8_pki);
    if (evp == NULL) {
      LOGE("[LoadPkcs8RsaKey(): EVP_PKCS82PKEY returned NULL]");
      dump_boringssl_error();
      success = false;
    }
  }
  if (success) {
    rsa_key_ = EVP_PKEY_get1_RSA(evp);
    if (rsa_key_ == NULL) {
      LOGE("[LoadPkcs8RsaKey(): PrivateKeyInfo did not contain an RSA key]");
      success = false;
    }
    key_owned_ = true;
  }
  if (evp != NULL) {
    EVP_PKEY_free(evp);
  }
  if (pkcs8_pki != NULL) {
    PKCS8_PRIV_KEY_INFO_free(pkcs8_pki);
  }
  BIO_free(bio);
  if (!success) {
    return false;
  }
  switch (RSA_check_key(rsa_key_)) {
    case 1:  // valid.
      return true;
    case 0:  // not valid.
      LOGE("[LoadPkcs8RsaKey(): rsa key not valid]");
      dump_boringssl_error();
      return false;
    default:  // -1 == check failed.
      LOGE("[LoadPkcs8RsaKey(): error checking rsa key]");
      dump_boringssl_error();
      return false;
  }
}

}  // namespace wvoec_ref
