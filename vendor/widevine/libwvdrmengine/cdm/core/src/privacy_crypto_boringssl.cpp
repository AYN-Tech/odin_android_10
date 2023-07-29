// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
// Description:
//   Definition of classes representing RSA public keys used
//   for signature verification and encryption and decryption.
//

#include "privacy_crypto.h"

#include <openssl/aes.h>
#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/md5.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <openssl/x509v3.h>

#include "log.h"

namespace {
const int kPssSaltLength = 20;
const int kRsaPkcs1OaepPaddingLength = 41;

RSA* GetKey(const std::string& serialized_key) {
  BIO* bio = BIO_new_mem_buf(const_cast<char*>(serialized_key.data()),
                             serialized_key.size());
  if (bio == NULL) {
    LOGE("GetKey: BIO_new_mem_buf returned NULL");
    return NULL;
  }
  RSA* key = d2i_RSAPublicKey_bio(bio, NULL);

  if (key == NULL) {
    LOGE("GetKey: RSA key deserialization failure: %s",
         ERR_error_string(ERR_get_error(), NULL));
    BIO_free(bio);
    return NULL;
  }

  BIO_free(bio);
  return key;
}

void FreeKey(RSA* key) {
  if (key != NULL) {
    RSA_free(key);
  }
}

template <typename T, void (*func)(T*)>
class boringssl_ptr {
 public:
  explicit boringssl_ptr(T* p = NULL) : ptr_(p) {}
  ~boringssl_ptr() {
    if (ptr_) func(ptr_);
  }
  T& operator*() const { return *ptr_; }
  T* operator->() const { return ptr_; }
  T* get() const { return ptr_; }

 private:
  T* ptr_;
  CORE_DISALLOW_COPY_AND_ASSIGN(boringssl_ptr);
};

void DeleteX509Stack(STACK_OF(X509)* stack) {
  sk_X509_pop_free(stack, X509_free);
}

std::string GetExtensionOid(X509_EXTENSION* extension) {
  ASN1_OBJECT* const extension_object = X509_EXTENSION_get_object(extension);
  if (!extension_object) {
    return "";
  }

  const int size = OBJ_obj2txt(nullptr, 0, extension_object, 1);
  std::string ret(size, '\0');
  OBJ_obj2txt(&ret[0], ret.size() + 1, extension_object, 1);
  return ret;
}

}  // namespace

namespace wvcdm {

AesCbcKey::AesCbcKey() {}

AesCbcKey::~AesCbcKey() {}

bool AesCbcKey::Init(const std::string& key) {
  if (key.size() != AES_BLOCK_SIZE) {
    LOGE("AesCbcKey::Init: unexpected key size: %d", key.size());
    return false;
  }

  key_ = key;
  return true;
}

bool AesCbcKey::Encrypt(const std::string& in, std::string* out,
                        std::string* iv) {
  if (in.empty()) {
    LOGE("AesCbcKey::Encrypt: no cleartext provided");
    return false;
  }
  if (iv == NULL) {
    LOGE("AesCbcKey::Encrypt: initialization vector destination not provided");
    return false;
  }
  if (iv->size() != AES_BLOCK_SIZE) {
    LOGE("AesCbcKey::Encrypt: invalid iv size: %d", iv->size());
    return false;
  }
  if (out == NULL) {
    LOGE("AesCbcKey::Encrypt: crypttext destination not provided");
    return false;
  }
  if (key_.empty()) {
    LOGE("AesCbcKey::Encrypt: AES key not initialized");
    return false;
  }

  EVP_CIPHER_CTX* evp_cipher_ctx = EVP_CIPHER_CTX_new();
  if (EVP_EncryptInit(evp_cipher_ctx, EVP_aes_128_cbc(),
                      reinterpret_cast<uint8_t*>(&key_[0]),
                      reinterpret_cast<uint8_t*>(&(*iv)[0])) == 0) {
    LOGE("AesCbcKey::Encrypt: AES CBC setup failure: %s",
         ERR_error_string(ERR_get_error(), NULL));
    EVP_CIPHER_CTX_free(evp_cipher_ctx);
    return false;
  }

  out->resize(in.size() + AES_BLOCK_SIZE);
  int out_length = out->size();
  if (EVP_EncryptUpdate(
          evp_cipher_ctx, reinterpret_cast<uint8_t*>(&(*out)[0]), &out_length,
          reinterpret_cast<uint8_t*>(const_cast<char*>(in.data())),
          in.size()) == 0) {
    LOGE("AesCbcKey::Encrypt: encryption failure: %s",
         ERR_error_string(ERR_get_error(), NULL));
    EVP_CIPHER_CTX_free(evp_cipher_ctx);
    return false;
  }

  int padding = 0;
  if (EVP_EncryptFinal_ex(evp_cipher_ctx,
                          reinterpret_cast<uint8_t*>(&(*out)[out_length]),
                          &padding) == 0) {
    LOGE("AesCbcKey::Encrypt: PKCS7 padding failure: %s",
         ERR_error_string(ERR_get_error(), NULL));
    EVP_CIPHER_CTX_free(evp_cipher_ctx);
    return false;
  }

  EVP_CIPHER_CTX_free(evp_cipher_ctx);
  out->resize(out_length + padding);
  return true;
}

RsaPublicKey::RsaPublicKey() {}

RsaPublicKey::~RsaPublicKey() {}

bool RsaPublicKey::Init(const std::string& serialized_key) {
  if (serialized_key.empty()) {
    LOGE("RsaPublicKey::Init: no serialized key provided");
    return false;
  }

  serialized_key_ = serialized_key;
  return true;
}

bool RsaPublicKey::Encrypt(const std::string& clear_message,
                           std::string* encrypted_message) {
  if (clear_message.empty()) {
    LOGE("RsaPublicKey::Encrypt: message to be encrypted is empty");
    return false;
  }
  if (encrypted_message == NULL) {
    LOGE("RsaPublicKey::Encrypt: no encrypt message buffer provided");
    return false;
  }
  if (serialized_key_.empty()) {
    LOGE("RsaPublicKey::Encrypt: RSA key not initialized");
    return false;
  }

  RSA* key = GetKey(serialized_key_);
  if (key == NULL) {
    // Error already logged by GetKey.
    return false;
  }

  int rsa_size = RSA_size(key);
  if (static_cast<int>(clear_message.size()) >
      rsa_size - kRsaPkcs1OaepPaddingLength) {
    LOGE("RsaPublicKey::Encrypt: message too large to be encrypted (actual %d",
         " max allowed %d)", clear_message.size(),
         rsa_size - kRsaPkcs1OaepPaddingLength);
    FreeKey(key);
    return false;
  }

  encrypted_message->assign(rsa_size, 0);
  if (RSA_public_encrypt(
          clear_message.size(),
          const_cast<unsigned char*>(
              reinterpret_cast<const unsigned char*>(clear_message.data())),
          reinterpret_cast<unsigned char*>(&(*encrypted_message)[0]), key,
          RSA_PKCS1_OAEP_PADDING) != rsa_size) {
    LOGE("RsaPublicKey::Encrypt: encrypt failure: %s",
         ERR_error_string(ERR_get_error(), NULL));
    FreeKey(key);
    return false;
  }

  FreeKey(key);
  return true;
}

// LogBoringSSLError is a callback from BoringSSL which is called with each error
// in the thread's error queue.
static int LogBoringSSLError(const char* msg, size_t /* len */, void* /* ctx */) {
  LOGE("  %s", msg);
  return 1;
}

static bool VerifyPSSSignature(EVP_PKEY *pkey, const std::string &message,
                               const std::string &signature) {
  EVP_MD_CTX* evp_md_ctx = EVP_MD_CTX_new();
  EVP_PKEY_CTX *pctx = NULL;

  if (EVP_DigestVerifyInit(evp_md_ctx, &pctx, EVP_sha1(), NULL /* no ENGINE */,
                           pkey) != 1) {
    LOGE("EVP_DigestVerifyInit failed in VerifyPSSSignature");
    goto err;
  }

  if (EVP_PKEY_CTX_set_signature_md(pctx,
                                    const_cast<EVP_MD *>(EVP_sha1())) != 1) {
    LOGE("EVP_PKEY_CTX_set_signature_md failed in VerifyPSSSignature");
    goto err;
  }

  if (EVP_PKEY_CTX_set_rsa_padding(pctx, RSA_PKCS1_PSS_PADDING) != 1) {
    LOGE("EVP_PKEY_CTX_set_rsa_padding failed in VerifyPSSSignature");
    goto err;
  }

  if (EVP_PKEY_CTX_set_rsa_pss_saltlen(pctx, kPssSaltLength) != 1) {
    LOGE("EVP_PKEY_CTX_set_rsa_pss_saltlen failed in VerifyPSSSignature");
    goto err;
  }

  if (EVP_DigestVerifyUpdate(evp_md_ctx, message.data(), message.size()) != 1) {
    LOGE("EVP_DigestVerifyUpdate failed in VerifyPSSSignature");
    goto err;
  }

  if (EVP_DigestVerifyFinal(
          evp_md_ctx, const_cast<uint8_t *>(
                    reinterpret_cast<const uint8_t *>(signature.data())),
          signature.size()) != 1) {
    LOGE(
        "EVP_DigestVerifyFinal failed in VerifyPSSSignature. (Probably a bad "
        "signature.)");
    goto err;
  }

    EVP_MD_CTX_free(evp_md_ctx);
  return true;

err:
  ERR_print_errors_cb(LogBoringSSLError, NULL);
  EVP_MD_CTX_free(evp_md_ctx);
  return false;
}

bool RsaPublicKey::VerifySignature(const std::string& message,
                                   const std::string& signature) {
  if (serialized_key_.empty()) {
    LOGE("RsaPublicKey::VerifySignature: RSA key not initialized");
    return false;
  }
  if (message.empty()) {
    LOGE("RsaPublicKey::VerifySignature: signed message is empty");
    return false;
  }
  RSA* rsa_key = GetKey(serialized_key_);
  if (rsa_key == NULL) {
    // Error already logged by GetKey.
    return false;
  }
  EVP_PKEY *pkey = EVP_PKEY_new();
  if (pkey == NULL) {
    LOGE("RsaPublicKey::VerifySignature: EVP_PKEY allocation failed");
    FreeKey(rsa_key);
    return false;
  }
  if (EVP_PKEY_set1_RSA(pkey, rsa_key) != 1) {
    LOGE("RsaPublicKey::VerifySignature: failed to wrap key in an EVP_PKEY");
    FreeKey(rsa_key);
    EVP_PKEY_free(pkey);
    return false;
  }
  FreeKey(rsa_key);

  const bool ok = VerifyPSSSignature(pkey, message, signature);
  EVP_PKEY_free(pkey);

  if (!ok) {
    LOGE("RsaPublicKey::VerifySignature: RSA verify failure");
    return false;
  }

  return true;
}


bool ExtractExtensionValueFromCertificate(const std::string& cert,
                                          const std::string& extension_oid,
                                          size_t cert_index, uint32_t* value) {
  // Load the certificate chain into a BoringSSL X509 Stack
  const boringssl_ptr<STACK_OF(X509), DeleteX509Stack> x509_stack(
      sk_X509_new_null());
  if (x509_stack.get() == NULL) {
    LOGE("ExtractExtensionValueFromCertificate: "
         "Unable to allocate X509 Stack.");
    return false;
  }

  CBS pkcs7;
  CBS_init(&pkcs7, reinterpret_cast<const uint8_t*>(cert.data()), cert.size());
  if (!PKCS7_get_certificates(x509_stack.get(), &pkcs7)) {
    LOGE("ExtractExtensionValueFromCertificate: "
         "Error getting certificate chain.");
    return false;
  }

  STACK_OF(X509)* certs = x509_stack.get();

  // Find the desired certificate from the stack.
  if (sk_X509_num(certs) <= cert_index) {
    LOGE("ExtractExtensionValueFromCertificate: "
         "Expected at least %zu certificates in chain, got %d",
         cert_index + 1, sk_X509_num(certs));
    return false;
  }

  X509* const intermediate_cert = sk_X509_value(certs, cert_index);
  if (!intermediate_cert) {
    LOGE("ExtractExtensionValueFromCertificate: "
         "Unable to get intermediate cert.");
    return false;
  }

  // Find the Widevine System ID extension in the intermediate cert
  const int extension_count = X509_get_ext_count(intermediate_cert);
  for (int i = 0; i < extension_count; ++i) {
    X509_EXTENSION* const extension = X509_get_ext(intermediate_cert, i);
    if (!extension) {
      LOGE("ExtractExtensionValueFromCertificate: "
           "Unable to get cert extension %d", i);
      continue;
    }

    if (GetExtensionOid(extension) != extension_oid) {
      // This extension is not the Widevine System ID, so we should move on to
      // the next one.
      continue;
    }

    ASN1_OCTET_STRING* const octet_str = X509_EXTENSION_get_data(extension);
    if (!octet_str) {
      LOGE("ExtractExtensionValueFromCertificate: "
           "Unable to get data of extension.");
      return false;
    }

    const unsigned char* data = octet_str->data;
    if (!data) {
      LOGE("ExtractExtensionValueFromCertificate: "
           "Null data in extension.");
      return false;
    }

    ASN1_INTEGER* const asn1_integer =
        d2i_ASN1_INTEGER(NULL, &data, octet_str->length);
    if (!asn1_integer) {
      LOGE("ExtractExtensionValueFromCertificate: "
           "Unable to decode data in extension.");
      return false;
    }

    const long system_id_long = ASN1_INTEGER_get(asn1_integer);
    ASN1_INTEGER_free(asn1_integer);
    if (system_id_long == -1) {
      LOGE("ExtractExtensionValueFromCertificate: "
           "Unable to decode ASN integer in extension.");
      return false;
    }

    *value = static_cast<uint32_t>(system_id_long);
    return true;
  }

  LOGE("ExtractExtensionValueFromCertificate: Extension not found.");
  return false;
}

std::string Md5Hash(const std::string& data) {
  std::string hash(MD5_DIGEST_LENGTH, '\0');
  MD5(reinterpret_cast<const uint8_t*>(data.data()), data.size(),
      reinterpret_cast<uint8_t*>(&hash[0]));
  return hash;
}

std::string Sha256Hash(const std::string& data) {
  std::string hash(SHA256_DIGEST_LENGTH, '\0');
  SHA256(reinterpret_cast<const uint8_t*>(data.data()), data.size(),
         reinterpret_cast<uint8_t*>(&hash[0]));
  return hash;
}

}  // namespace wvcdm
