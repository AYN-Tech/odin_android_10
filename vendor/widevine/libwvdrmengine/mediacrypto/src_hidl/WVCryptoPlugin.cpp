//
// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//

//#define LOG_NDEBUG 0
#define LOG_TAG "WVCdm"
#include <utils/Log.h>

#include "WVCryptoPlugin.h"

#include <hidlmemory/mapping.h>

#include "HidlTypes.h"
#include "mapErrors-inl.h"
#include "OEMCryptoCENC.h"
#include "openssl/sha.h"
#include "TypeConvert.h"
#include "wv_cdm_constants.h"
#include "WVErrors.h"

namespace {

static const size_t kAESBlockSize = 16;

inline Status toStatus_1_0(Status_V1_2 status) {
  switch (status) {
    case Status_V1_2::ERROR_DRM_INSUFFICIENT_SECURITY:
    case Status_V1_2::ERROR_DRM_FRAME_TOO_LARGE:
    case Status_V1_2::ERROR_DRM_SESSION_LOST_STATE:
      return Status::ERROR_DRM_UNKNOWN;
    default:
      return static_cast<Status>(status);
  }
}

}  // namespace

namespace wvdrm {
namespace hardware {
namespace drm {
namespace V1_2 {
namespace widevine {

using android::hardware::drm::V1_2::widevine::toVector;
using wvcdm::CdmDecryptionParameters;
using wvcdm::CdmQueryMap;
using wvcdm::CdmResponseType;
using wvcdm::CdmSessionId;
using wvcdm::KeyId;
using wvcdm::WvContentDecryptionModule;

WVCryptoPlugin::WVCryptoPlugin(const void* data, size_t size,
                               const sp<WvContentDecryptionModule>& cdm)
  : mCDM(cdm){
  if (data != NULL) {
    mSessionId.assign(static_cast<const char *>(data), size);
  }
  if (!mCDM->IsOpenSession(mSessionId)) {
    mSessionId.clear();
  }
}

Return<bool> WVCryptoPlugin::requiresSecureDecoderComponent(
    const hidl_string& mime) {
  if (!strncasecmp(mime.c_str(), "video/", 6)) {
    // Type is video, so query CDM to see if we require a secure decoder.
    CdmQueryMap status;

    CdmResponseType res = mCDM->QuerySessionStatus(mSessionId, &status);

    if (!isCdmResponseTypeSuccess(res)) {
      ALOGE("Error querying CDM status: %u", res);
      return false;
    }

    return status[wvcdm::QUERY_KEY_SECURITY_LEVEL] ==
        wvcdm::QUERY_VALUE_SECURITY_LEVEL_L1;
  } else {
    // Type is not video, so never require a secure decoder.
    return false;
  }
}

Return<void> WVCryptoPlugin::notifyResolution(
    uint32_t width, uint32_t height) {
  mCDM->NotifyResolution(mSessionId, width, height);
  return Void();
}

Return<Status> WVCryptoPlugin::setMediaDrmSession(
    const hidl_vec<uint8_t>& sessionId) {
  if (sessionId.size() == 0) {
    return Status::BAD_VALUE;
  }
  const std::vector<uint8_t> sId = toVector(sessionId);
  CdmSessionId cdmSessionId(sId.begin(), sId.end());
  if (!mCDM->IsOpenSession(cdmSessionId)) {
    return Status::ERROR_DRM_SESSION_NOT_OPENED;
  } else {
    mSessionId = cdmSessionId;
    return Status::OK;
  }
}

Return<void> WVCryptoPlugin::setSharedBufferBase(
    const hidl_memory& base, uint32_t bufferId) {
  sp<IMemory> hidlMemory = mapMemory(base);

  // allow mapMemory to return nullptr
  mSharedBufferMap[bufferId] = hidlMemory;
  return Void();
}

Return<void> WVCryptoPlugin::decrypt(
    bool secure,
    const hidl_array<uint8_t, 16>& keyId,
    const hidl_array<uint8_t, 16>& iv,
    Mode mode,
    const Pattern& pattern,
    const hidl_vec<SubSample>& subSamples,
    const SharedBuffer& source,
    uint64_t offset,
    const DestinationBuffer& destination,
    decrypt_cb _hidl_cb) {

  Status status = Status::ERROR_DRM_UNKNOWN;
  hidl_string detailedError;
  uint32_t bytesWritten = 0;

  Return<void> hResult = decrypt_1_2(
      secure, keyId, iv, mode, pattern, subSamples, source, offset, destination,
      [&](Status_V1_2 hStatus, uint32_t hBytesWritten, hidl_string hDetailedError) {
        status = toStatus_1_0(hStatus);
        if (status == Status::OK) {
          bytesWritten = hBytesWritten;
          detailedError = hDetailedError;
        }
      }
    );

  status = hResult.isOk() ? status : Status::ERROR_DRM_CANNOT_HANDLE;
  _hidl_cb(status, bytesWritten, detailedError);
  return Void();
}

Return<void> WVCryptoPlugin::decrypt_1_2(
    bool secure,
    const hidl_array<uint8_t, 16>& keyId,
    const hidl_array<uint8_t, 16>& iv,
    Mode mode,
    const Pattern& pattern,
    const hidl_vec<SubSample>& subSamples,
    const SharedBuffer& source,
    uint64_t offset,
    const DestinationBuffer& destination,
    decrypt_1_2_cb _hidl_cb) {

    if (mSharedBufferMap.find(source.bufferId) == mSharedBufferMap.end()) {
      _hidl_cb(Status_V1_2::ERROR_DRM_CANNOT_HANDLE, 0,
               "source decrypt buffer base not set");
      return Void();
    }

    if (destination.type == BufferType::SHARED_MEMORY) {
      const SharedBuffer& dest = destination.nonsecureMemory;
      if (mSharedBufferMap.find(dest.bufferId) == mSharedBufferMap.end()) {
        _hidl_cb(Status_V1_2::ERROR_DRM_CANNOT_HANDLE, 0,
                 "destination decrypt buffer base not set");
        return Void();
      }
    }

  if (mode != Mode::UNENCRYPTED &&
    mode != Mode::AES_CTR &&
    mode != Mode::AES_CBC) {
    _hidl_cb(Status_V1_2::BAD_VALUE,
             0, "Encryption mode is not supported by Widevine CDM.");
    return Void();
  }

  // Convert parameters to the form the CDM wishes to consume them in.
  const KeyId cryptoKey(
      reinterpret_cast<const char*>(keyId.data()), wvcdm::KEY_ID_SIZE);
  std::vector<uint8_t> ivVector(iv.data(), iv.data() + wvcdm::KEY_IV_SIZE);

  std::string errorDetailMsg;
  sp<IMemory> sourceBase = mSharedBufferMap[source.bufferId];
  if (sourceBase == nullptr) {
    _hidl_cb(Status_V1_2::ERROR_DRM_CANNOT_HANDLE, 0, "source is a nullptr");
    return Void();
  }

  if (source.offset + offset + source.size > sourceBase->getSize()) {
    _hidl_cb(Status_V1_2::ERROR_DRM_CANNOT_HANDLE, 0, "invalid buffer size");
    return Void();
  }

  uint8_t *base = static_cast<uint8_t *>
      (static_cast<void *>(sourceBase->getPointer()));
  uint8_t* srcPtr = static_cast<uint8_t *>(base + source.offset + offset);
  void* destPtr = NULL;
  if (destination.type == BufferType::SHARED_MEMORY) {
    const SharedBuffer& destBuffer = destination.nonsecureMemory;
    sp<IMemory> destBase = mSharedBufferMap[destBuffer.bufferId];
    if (destBase == nullptr) {
      _hidl_cb(Status_V1_2::ERROR_DRM_CANNOT_HANDLE, 0, "destination is a nullptr");
      return Void();
    }

    if (destBuffer.offset + destBuffer.size > destBase->getSize()) {
      _hidl_cb(Status_V1_2::ERROR_DRM_FRAME_TOO_LARGE, 0, "invalid buffer size");
      return Void();
    }
    destPtr = static_cast<void *>(base + destination.nonsecureMemory.offset);
  } else if (destination.type == BufferType::NATIVE_HANDLE) {
    native_handle_t *handle = const_cast<native_handle_t *>(
        destination.secureMemory.getNativeHandle());
    destPtr = static_cast<void *>(handle);
  }

  // Calculate the output buffer size and determine if any subsamples are
  // encrypted.
  size_t destSize = 0;
  bool haveEncryptedSubsamples = false;
  for (size_t i = 0; i < subSamples.size(); i++) {
    const SubSample &subSample = subSamples[i];
    destSize += subSample.numBytesOfClearData;
    destSize += subSample.numBytesOfEncryptedData;
    if (subSample.numBytesOfEncryptedData > 0) {
      haveEncryptedSubsamples = true;
    }
  }

  // Set up the decrypt params that do not vary.
  CdmDecryptionParameters params = CdmDecryptionParameters();
  params.is_secure = secure;
  params.key_id = &cryptoKey;
  params.iv = &ivVector;
  params.decrypt_buffer = destPtr;
  params.decrypt_buffer_length = destSize;
  params.pattern_descriptor.encrypt_blocks = pattern.encryptBlocks;
  params.pattern_descriptor.skip_blocks = pattern.skipBlocks;

  if (mode == Mode::AES_CTR) {
    params.cipher_mode = wvcdm::kCipherModeCtr;
  } else if (mode == Mode::AES_CBC) {
    params.cipher_mode = wvcdm::kCipherModeCbc;
  }

  // Iterate through subsamples, sending them to the CDM serially.
  size_t bufferOffset = 0;
  size_t blockOffset = 0;
  const size_t patternLengthInBytes =
      (pattern.encryptBlocks + pattern.skipBlocks) * kAESBlockSize;

  for (size_t i = 0; i < subSamples.size(); ++i) {
    const SubSample& subSample = subSamples[i];

    if (mode == Mode::UNENCRYPTED && subSample.numBytesOfEncryptedData != 0) {
      _hidl_cb(Status_V1_2::ERROR_DRM_CANNOT_HANDLE, 0,
               "Encrypted subsamples found in allegedly unencrypted data.");
      return Void();
    }

    // Calculate any flags that apply to this subsample's parts.
    uint8_t clearFlags = 0;
    uint8_t encryptedFlags = 0;

    // If this is the first subsample…
    if (i == 0) {
      // …add OEMCrypto_FirstSubsample to the first part that is present.
      if (subSample.numBytesOfClearData != 0) {
        clearFlags = clearFlags | OEMCrypto_FirstSubsample;
      } else {
        encryptedFlags = encryptedFlags | OEMCrypto_FirstSubsample;
      }
    }
    // If this is the last subsample…
    if (i == subSamples.size() - 1) {
      // …add OEMCrypto_LastSubsample to the last part that is present
      if (subSample.numBytesOfEncryptedData != 0) {
        encryptedFlags = encryptedFlags | OEMCrypto_LastSubsample;
      } else {
        clearFlags = clearFlags | OEMCrypto_LastSubsample;
      }
    }

    // "Decrypt" any unencrypted data.  Per the ISO-CENC standard, clear data
    // comes before encrypted data.
    if (subSample.numBytesOfClearData != 0) {
      params.is_encrypted = false;
      params.encrypt_buffer = srcPtr + bufferOffset;
      params.encrypt_length = subSample.numBytesOfClearData;
      params.block_offset = 0;
      params.decrypt_buffer_offset = bufferOffset;
      params.subsample_flags = clearFlags;

      Status_V1_2 res = attemptDecrypt(params, haveEncryptedSubsamples,
              &errorDetailMsg);
      if (res != Status_V1_2::OK) {
        _hidl_cb(res, 0, errorDetailMsg.c_str());
        return Void();
      }
      bufferOffset += subSample.numBytesOfClearData;
    }

    // Decrypt any encrypted data.  Per the ISO-CENC standard, encrypted data
    // comes after clear data.
    if (subSample.numBytesOfEncryptedData != 0) {
      params.is_encrypted = true;
      params.encrypt_buffer = srcPtr + bufferOffset;
      params.encrypt_length = subSample.numBytesOfEncryptedData;
      params.block_offset = blockOffset;
      params.decrypt_buffer_offset = bufferOffset;
      params.subsample_flags = encryptedFlags;

      Status_V1_2 res = attemptDecrypt(params, haveEncryptedSubsamples,
              &errorDetailMsg);
      if (res != Status_V1_2::OK) {
        _hidl_cb(res, 0, errorDetailMsg.c_str());
        return Void();
      }

      bufferOffset += subSample.numBytesOfEncryptedData;

      // Update the block offset, pattern offset, and IV as needed by the
      // various crypto modes. Possible combinations are cenc (AES-CTR), cens
      // (AES-CTR w/ Patterns), cbc1 (AES-CBC), and cbcs (AES-CBC w/ Patterns).
      if (mode == Mode::AES_CTR) {
        // Update the IV depending on how many encrypted blocks we passed.
        uint64_t increment = 0;
        if (patternLengthInBytes == 0) {
          // If there's no pattern, all the blocks are encrypted. We have to add
          // in blockOffset to account for any incomplete crypto blocks from the
          // preceding subsample.
          increment = (blockOffset + subSample.numBytesOfEncryptedData) /
              kAESBlockSize;
        } else {
          const uint64_t numBlocks =
              subSample.numBytesOfEncryptedData / kAESBlockSize;
          const uint64_t patternLengthInBlocks =
              pattern.encryptBlocks + pattern.skipBlocks;
          increment =
              (numBlocks / patternLengthInBlocks) * pattern.encryptBlocks;
          // A partial pattern is only encrypted if it is at least
          // mEncryptBlocks large.
          if (numBlocks % patternLengthInBlocks >= pattern.encryptBlocks)
            increment += pattern.encryptBlocks;
        }
        incrementIV(increment, &ivVector);

        // Update the block offset
        blockOffset = (blockOffset + subSample.numBytesOfEncryptedData) %
            kAESBlockSize;
      } else if (mode == Mode::AES_CBC && patternLengthInBytes == 0) {
        // If there is no pattern, assume cbc1 mode and update the IV.

        // Stash the last crypto block in the IV.
        const uint8_t* bufferEnd = srcPtr + bufferOffset +
                                   subSample.numBytesOfEncryptedData;
        ivVector.assign(bufferEnd - kAESBlockSize, bufferEnd);
      }
      // There is no branch for cbcs mode because the IV and pattern offest
      // reset at the start of each subsample, so they do not need to be
      // updated.
    }
  }

  _hidl_cb(Status_V1_2::OK, bufferOffset, errorDetailMsg.c_str());
  return Void();
}

Status_V1_2 WVCryptoPlugin::attemptDecrypt(const CdmDecryptionParameters& params,
        bool haveEncryptedSubsamples, std::string* errorDetailMsg) {
  CdmResponseType res = mCDM->Decrypt(mSessionId, haveEncryptedSubsamples,
                                      params);

  if (isCdmResponseTypeSuccess(res)) {
    return Status_V1_2::OK;
  } else {
    ALOGE("Decrypt error result in session %s during %s block: %d",
          mSessionId.c_str(),
          params.is_encrypted ? "encrypted" : "unencrypted",
          res);
    bool actionableError = true;
    switch (res) {
      case wvcdm::INSUFFICIENT_CRYPTO_RESOURCES:
        errorDetailMsg->assign(
            "Error decrypting data: insufficient crypto resources");
        break;
      case wvcdm::NEED_KEY:
      case wvcdm::KEY_NOT_FOUND_IN_SESSION:
        errorDetailMsg->assign(
            "Error decrypting data: requested key has not been loaded");
        break;
      case wvcdm::DECRYPT_NOT_READY:
        errorDetailMsg->assign(
            "Error decrypting data: license validity period is in the future");
        break;
      case wvcdm::SESSION_NOT_FOUND_FOR_DECRYPT:
        errorDetailMsg->assign(
            "Error decrypting data: session not found, possibly reclaimed");
        break;
      case wvcdm::DECRYPT_ERROR:
        errorDetailMsg->assign(
            "Error decrypting data: unspecified error");
        break;
      case wvcdm::INSUFFICIENT_OUTPUT_PROTECTION:
      case wvcdm::ANALOG_OUTPUT_ERROR:
        errorDetailMsg->assign(
            "Error decrypting data: insufficient output protection");
        break;
      case wvcdm::KEY_PROHIBITED_FOR_SECURITY_LEVEL:
        errorDetailMsg->assign(
            "Error decrypting data: key prohibited for security level");
        break;
      default:
        actionableError = false;
        break;
    }

    if (actionableError) {
      // This error is actionable by the app and should be passed up.
      return mapCdmResponseType_1_2(res);
    } else {
      // Swallow the specifics of other errors to obscure decrypt internals.
      return Status_V1_2::ERROR_DRM_UNKNOWN;
    }
  }
}

void WVCryptoPlugin::incrementIV(uint64_t increaseBy,
                                 std::vector<uint8_t>* ivPtr) {
  std::vector<uint8_t>& iv = *ivPtr;
  uint64_t* counterBuffer = reinterpret_cast<uint64_t*>(&iv[8]);
  (*counterBuffer) = htonq(ntohq(*counterBuffer) + increaseBy);
}

}  // namespace widevine
}  // namespace V1_2
}  // namespace drm
}  // namespace hardware
}  // namespace wvdrm
