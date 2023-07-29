// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
//  Reference implementation of OEMCrypto APIs
//
#ifndef OEMCRYPTO_RSA_KEY_SHARED_H_
#define OEMCRYPTO_RSA_KEY_SHARED_H_

#include <stdint.h>

#include <openssl/rsa.h>

namespace wvoec_ref {

// Shared pointer with specialized destructor.  This pointer is only shared
// from a CryptoEngine to a Session -- so we don't have to use full reference
// counting.
class RSA_shared_ptr {
 public:
  RSA_shared_ptr() : rsa_key_(NULL), key_owned_(false) {}
  ~RSA_shared_ptr() { reset(); };
  // Explicitly allow copy as share.
  explicit RSA_shared_ptr(const RSA_shared_ptr& other) :
      rsa_key_(other.rsa_key_), key_owned_(false) {}
  RSA* get() { return rsa_key_; }
  void reset();
  bool LoadPkcs8RsaKey(const uint8_t* buffer, size_t length);

 private:
  void operator=(const RSA_shared_ptr);  // disallow assign.

  RSA* rsa_key_;
  bool key_owned_;
};

// Log errors from BoringSSL.
void dump_boringssl_error();

}  // namespace wvoec_ref

#endif  // OEMCRYPTO_RSA_KEY_SHARED_H_
