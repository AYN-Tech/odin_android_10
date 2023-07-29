//
// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//

//#define LOG_NDEBUG 0
#define LOG_TAG "WVCdm"
#include <utils/Log.h>

#include "WVCryptoPlugin.h"

#include <endian.h>
#include <string.h>
#include <string>
#include <vector>

#include "mapErrors-inl.h"
#include "media/stagefright/MediaErrors.h"
#include "OEMCryptoCENC.h"
#include "openssl/sha.h"
#include "utils/Errors.h"
#include "utils/String8.h"
#include "wv_cdm_constants.h"
#include "WVErrors.h"

namespace {

static const size_t kAESBlockSize = 16;

}  // namespace

namespace wvdrm {

using namespace android;
using namespace std;
using namespace wvcdm;

WVCryptoPlugin::WVCryptoPlugin(const void* data, size_t size,
                               const sp<WvContentDecryptionModule>& cdm)
  : mCDM(cdm),
    mTestMode(false),
    mSessionId(configureTestMode(data, size)) {}

CdmSessionId WVCryptoPlugin::configureTestMode(const void* data, size_t size) {
  CdmSessionId sessionId;
  if (data != NULL) {
    sessionId.assign(static_cast<const char *>(data), size);
    size_t index = sessionId.find("test_mode");
    if (index != string::npos) {
      sessionId = sessionId.substr(0, index);
      mTestMode = true;
    }
  }
  if (!mCDM->IsOpenSession(sessionId)) {
    sessionId.clear();
  }
  return sessionId;
}

bool WVCryptoPlugin::requiresSecureDecoderComponent(const char* mime) const {
  if (!strncasecmp(mime, "video/", 6)) {
    // Type is video, so query CDM to see if we require a secure decoder.
    CdmQueryMap status;

    CdmResponseType res = mCDM->QuerySessionStatus(mSessionId, &status);

    if (!isCdmResponseTypeSuccess(res)) {
      ALOGE("Error querying CDM status: %u", res);
      return false;
    }

    return status[QUERY_KEY_SECURITY_LEVEL] == QUERY_VALUE_SECURITY_LEVEL_L1;
  } else {
    // Type is not video, so never require a secure decoder.
    return false;
  }
}

void WVCryptoPlugin::notifyResolution(uint32_t width, uint32_t height) {
  mCDM->NotifyResolution(mSessionId, width, height);
}

status_t WVCryptoPlugin::setMediaDrmSession(const Vector<uint8_t>& sessionId) {
  CdmSessionId cdmSessionId(reinterpret_cast<const char *>(sessionId.array()),
                            sessionId.size());
  if (sessionId.size() == 0) {
      return android::BAD_VALUE;
  }
  if (!mCDM->IsOpenSession(cdmSessionId)) {
    return android::ERROR_DRM_SESSION_NOT_OPENED;
  } else {
    mSessionId = cdmSessionId;
    return android::NO_ERROR;
  }
}

// Returns negative values for error code and positive values for the size of
// decrypted data.  In theory, the output size can be larger than the input
// size, but in practice this should never happen for AES-CTR.
ssize_t WVCryptoPlugin::decrypt(bool secure, const uint8_t key[KEY_ID_SIZE],
                                const uint8_t iv[KEY_IV_SIZE], Mode mode,
                                const Pattern& pattern, const void* srcPtr,
                                const SubSample* subSamples,
                                size_t numSubSamples, void* dstPtr,
                                AString* errorDetailMsg) {
  if (mode != kMode_Unencrypted &&
      mode != kMode_AES_CTR &&
      mode != kMode_AES_CBC) {
    errorDetailMsg->setTo("Encryption mode is not supported by Widevine CDM.");
    return kErrorUnsupportedCrypto;
  }

  // Convert parameters to the form the CDM wishes to consume them in.
  const KeyId keyId(reinterpret_cast<const char*>(key), KEY_ID_SIZE);
  vector<uint8_t> ivVector(iv, iv + KEY_IV_SIZE);
  const uint8_t* const source = static_cast<const uint8_t*>(srcPtr);
  uint8_t* const dest = static_cast<uint8_t*>(dstPtr);

  // Calculate the output buffer size and determine if any subsamples are
  // encrypted.
  size_t destSize = 0;
  bool haveEncryptedSubsamples = false;
  for (size_t i = 0; i < numSubSamples; i++) {
    const SubSample &subSample = subSamples[i];
    destSize += subSample.mNumBytesOfClearData;
    destSize += subSample.mNumBytesOfEncryptedData;
    if (subSample.mNumBytesOfEncryptedData > 0) {
      haveEncryptedSubsamples = true;
    }
  }

  // Set up the decrypt params that do not vary.
  CdmDecryptionParameters params = CdmDecryptionParameters();
  params.is_secure = secure;
  params.key_id = &keyId;
  params.iv = &ivVector;
  params.decrypt_buffer = dest;
  params.decrypt_buffer_length = destSize;
  params.pattern_descriptor.encrypt_blocks = pattern.mEncryptBlocks;
  params.pattern_descriptor.skip_blocks = pattern.mSkipBlocks;

  if (mode == kMode_AES_CTR) {
    params.cipher_mode = kCipherModeCtr;
  } else if (mode == kMode_AES_CBC) {
    params.cipher_mode = kCipherModeCbc;
  }

  // Iterate through subsamples, sending them to the CDM serially.
  size_t offset = 0;
  size_t blockOffset = 0;
  const size_t patternLengthInBytes =
      (pattern.mEncryptBlocks + pattern.mSkipBlocks) * kAESBlockSize;

  for (size_t i = 0; i < numSubSamples; ++i) {
    const SubSample& subSample = subSamples[i];

    if (mode == kMode_Unencrypted && subSample.mNumBytesOfEncryptedData != 0) {
      errorDetailMsg->setTo("Encrypted subsamples found in allegedly "
                            "unencrypted data.");
      return kErrorExpectedUnencrypted;
    }

    // Calculate any flags that apply to this subsample's parts.
    uint8_t clearFlags = 0;
    uint8_t encryptedFlags = 0;

    // If this is the first subsample…
    if (i == 0) {
      // …add OEMCrypto_FirstSubsample to the first part that is present.
      if (subSample.mNumBytesOfClearData != 0) {
        clearFlags = clearFlags | OEMCrypto_FirstSubsample;
      } else {
        encryptedFlags = encryptedFlags | OEMCrypto_FirstSubsample;
      }
    }
    // If this is the last subsample…
    if (i == numSubSamples - 1) {
      // …add OEMCrypto_LastSubsample to the last part that is present
      if (subSample.mNumBytesOfEncryptedData != 0) {
        encryptedFlags = encryptedFlags | OEMCrypto_LastSubsample;
      } else {
        clearFlags = clearFlags | OEMCrypto_LastSubsample;
      }
    }

    // "Decrypt" any unencrypted data.  Per the ISO-CENC standard, clear data
    // comes before encrypted data.
    if (subSample.mNumBytesOfClearData != 0) {
      params.is_encrypted = false;
      params.encrypt_buffer = source + offset;
      params.encrypt_length = subSample.mNumBytesOfClearData;
      params.block_offset = 0;
      params.decrypt_buffer_offset = offset;
      params.subsample_flags = clearFlags;

      status_t res = attemptDecrypt(params, haveEncryptedSubsamples,
                                    errorDetailMsg);
      if (res != android::OK) {
        return res;
      }

      offset += subSample.mNumBytesOfClearData;
    }

    // Decrypt any encrypted data.  Per the ISO-CENC standard, encrypted data
    // comes after clear data.
    if (subSample.mNumBytesOfEncryptedData != 0) {
      params.is_encrypted = true;
      params.encrypt_buffer = source + offset;
      params.encrypt_length = subSample.mNumBytesOfEncryptedData;
      params.block_offset = blockOffset;
      params.decrypt_buffer_offset = offset;
      params.subsample_flags = encryptedFlags;

      status_t res = attemptDecrypt(params, haveEncryptedSubsamples,
                                    errorDetailMsg);
      if (res != android::OK) {
        return res;
      }

      offset += subSample.mNumBytesOfEncryptedData;

      // Update the block offset, pattern offset, and IV as needed by the
      // various crypto modes. Possible combinations are cenc (AES-CTR), cens
      // (AES-CTR w/ Patterns), cbc1 (AES-CBC), and cbcs (AES-CBC w/ Patterns).
      if (mode == kMode_AES_CTR) {
        // Update the IV depending on how many encrypted blocks we passed.
        uint64_t increment = 0;
        if (patternLengthInBytes == 0) {
          // If there's no pattern, all the blocks are encrypted. We have to add
          // in blockOffset to account for any incomplete crypto blocks from the
          // preceding subsample.
          increment = (blockOffset + subSample.mNumBytesOfEncryptedData) /
              kAESBlockSize;
        } else {
          const uint64_t numBlocks =
              subSample.mNumBytesOfEncryptedData / kAESBlockSize;
          const uint64_t patternLengthInBlocks =
              pattern.mEncryptBlocks + pattern.mSkipBlocks;
          increment =
              (numBlocks / patternLengthInBlocks) * pattern.mEncryptBlocks;
          // A partial pattern is only encrypted if it is at least
          // mEncryptBlocks large.
          if (numBlocks % patternLengthInBlocks >= pattern.mEncryptBlocks)
            increment += pattern.mEncryptBlocks;
        }
        incrementIV(increment, &ivVector);

        // Update the block offset
        blockOffset = (blockOffset + subSample.mNumBytesOfEncryptedData) %
            kAESBlockSize;
      } else if (mode == kMode_AES_CBC && patternLengthInBytes == 0) {
        // If there is no pattern, assume cbc1 mode and update the IV.

        // Stash the last crypto block in the IV.
        const uint8_t* bufferEnd = source + offset +
                                   subSample.mNumBytesOfEncryptedData;
        ivVector.assign(bufferEnd - kAESBlockSize, bufferEnd);
      }
      // There is no branch for cbcs mode because the IV and pattern offest
      // reset at the start of each subsample, so they do not need to be
      // updated.
    }
  }

  // In test mode, we return an error that causes a detailed error
  // message string containing a SHA256 hash of the decrypted data
  // to get passed to the java app via CryptoException.  The test app
  // can then use the hash to verify that decryption was successful.

  if (mTestMode) {
      if (secure) {
          // can't access data in secure mode
          errorDetailMsg->setTo("secure");
      } else {
          SHA256_CTX ctx;
          uint8_t digest[SHA256_DIGEST_LENGTH];
          SHA256_Init(&ctx);
          SHA256_Update(&ctx, dstPtr, offset);
          SHA256_Final(digest, &ctx);
          String8 buf;
          for (size_t i = 0; i < sizeof(digest); i++) {
              buf.appendFormat("%02x", digest[i]);
          }
          errorDetailMsg->setTo(buf.string());
      }

      return kErrorTestMode;
  }

  return static_cast<ssize_t>(offset);
}

status_t WVCryptoPlugin::attemptDecrypt(const CdmDecryptionParameters& params,
                                        bool haveEncryptedSubsamples,
                                        AString* errorDetailMsg) {
  CdmResponseType res = mCDM->Decrypt(mSessionId, haveEncryptedSubsamples,
                                      params);

  if (isCdmResponseTypeSuccess(res)) {
    return android::OK;
  } else {
    ALOGE("Decrypt error result in session %s during %s block: %d",
          mSessionId.c_str(),
          params.is_encrypted ? "encrypted" : "unencrypted",
          res);
    bool actionableError = true;
    switch (res) {
      case wvcdm::INSUFFICIENT_CRYPTO_RESOURCES:
        errorDetailMsg->setTo(
            "Error decrypting data: insufficient crypto resources");
        break;
      case wvcdm::NEED_KEY:
      case wvcdm::KEY_NOT_FOUND_IN_SESSION:
        errorDetailMsg->setTo(
            "Error decrypting data: requested key has not been loaded");
        break;
      case wvcdm::DECRYPT_NOT_READY:
        errorDetailMsg->setTo(
            "Error decrypting data: license validity period is in the future");
        break;
      case wvcdm::SESSION_NOT_FOUND_FOR_DECRYPT:
        errorDetailMsg->setTo(
            "Error decrypting data: session not found, possibly reclaimed");
        break;
      case wvcdm::DECRYPT_ERROR:
        errorDetailMsg->setTo(
            "Error decrypting data: unspecified error");
        break;
      case wvcdm::INSUFFICIENT_OUTPUT_PROTECTION:
      case wvcdm::ANALOG_OUTPUT_ERROR:
        errorDetailMsg->setTo(
            "Error decrypting data: insufficient output protection");
        break;
      case wvcdm::KEY_PROHIBITED_FOR_SECURITY_LEVEL:
        errorDetailMsg->setTo(
            "Error decrypting data: key prohibited for security level");
        break;
      default:
        actionableError = false;
        break;
    }

    if (actionableError) {
      // This error is actionable by the app and should be passed up.
      return mapCdmResponseType(res);
    } else {
      // Swallow the specifics of other errors to obscure decrypt internals.
      return kErrorCDMGeneric;
    }
  }
}

void WVCryptoPlugin::incrementIV(uint64_t increaseBy, vector<uint8_t>* ivPtr) {
  vector<uint8_t>& iv = *ivPtr;
  uint64_t* counterBuffer = reinterpret_cast<uint64_t*>(&iv[8]);
  (*counterBuffer) = htonq(ntohq(*counterBuffer) + increaseBy);
}

}  // namespace wvdrm
