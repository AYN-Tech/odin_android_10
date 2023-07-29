//
// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//

//#define LOG_NDEBUG 0
#define LOG_TAG "WVCdm"
#include <log/log.h>

#include "WVGenericCryptoInterface.h"

#include "wv_cdm_constants.h"

namespace wvdrm {

using namespace std;
using namespace wvcdm;

OEMCryptoResult WVGenericCryptoInterface::signRSA(const uint8_t* wrapped_rsa_key,
                                                  size_t wrapped_rsa_key_length,
                                                  const uint8_t* message,
                                                  size_t message_length,
                                                  std::vector<uint8_t>& signature,
                                                  RSA_Padding_Scheme padding_scheme) {
  OEMCrypto_SESSION session;
  OEMCryptoResult sts = OEMCrypto_OpenSession(&session);
  if (sts != OEMCrypto_SUCCESS) return sts;
  sts = OEMCrypto_LoadDeviceRSAKey(session, wrapped_rsa_key,
                                   wrapped_rsa_key_length);
  if (sts == OEMCrypto_SUCCESS) {
    size_t signatureSize = 0;
    sts = OEMCrypto_GenerateRSASignature(session, message, message_length,
                                         NULL, &signatureSize,
                                         padding_scheme);
    if (sts == OEMCrypto_SUCCESS) {
      // Should be short buffer.
      sts = OEMCrypto_ERROR_UNKNOWN_FAILURE;
    } else if (sts == OEMCrypto_ERROR_SHORT_BUFFER) {
      signature.resize(signatureSize);
      sts = OEMCrypto_GenerateRSASignature(session, message, message_length,
                                           signature.data(), &signatureSize,
                                           padding_scheme);
    }
  }
  OEMCrypto_CloseSession(session);
  return sts;
}

}  // namespace wvdrm
