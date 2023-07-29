//
// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//

//#define LOG_NDEBUG 0
#define LOG_TAG "WVCryptoPluginTest"
#include <utils/Log.h>

#include <map>
#include <stdio.h>
#include <string>
#include <vector>

#include <binder/MemoryDealer.h>
#include <hidl/Status.h>
#include <hidlmemory/mapping.h>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "wv_cdm_constants.h"
#include "wv_cdm_types.h"
#include "wv_content_decryption_module.h"
#include "HidlTypes.h"
#include "OEMCryptoCENC.h"
#include "TypeConvert.h"
#include "WVCryptoPlugin.h"

namespace wvdrm {
namespace hardware {
namespace drm {
namespace V1_2 {
namespace widevine {

using ::android::MemoryDealer;

using ::testing::_;
using ::testing::DefaultValue;
using ::testing::ElementsAreArray;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::Matcher;
using ::testing::SetArgPointee;
using ::testing::StrictMock;
using ::testing::Test;
using ::testing::Value;
using ::testing::internal::ElementsAreArrayMatcher;

using wvcdm::kCipherModeCtr;
using wvcdm::CdmCencPatternEncryptionDescriptor;
using wvcdm::CdmCipherMode;
using wvcdm::CdmDecryptionParameters;
using wvcdm::CdmQueryMap;
using wvcdm::CdmResponseType;
using wvcdm::CdmSessionId;
using wvcdm::KEY_ID_SIZE;
using wvcdm::KEY_IV_SIZE;
using wvcdm::QUERY_KEY_SECURITY_LEVEL;
using wvcdm::QUERY_VALUE_SECURITY_LEVEL_L1;
using wvcdm::QUERY_VALUE_SECURITY_LEVEL_L3;

class MockCDM : public wvcdm::WvContentDecryptionModule {
 public:
  MOCK_METHOD1(IsOpenSession, bool(const CdmSessionId&));

  MOCK_METHOD3(Decrypt, CdmResponseType(const CdmSessionId&, bool,
                                        const CdmDecryptionParameters&));

  MOCK_METHOD2(QuerySessionStatus, CdmResponseType(const CdmSessionId&,
                                                   CdmQueryMap*));
};

class WVCryptoPluginTest : public Test {
 protected:
  static const uint32_t kSessionIdSize = 16;
  uint8_t* pDest = nullptr;
  uint8_t* pSrc = nullptr;
  uint8_t sessionId[kSessionIdSize];
  uint32_t nextBufferId = 0;
  std::map<void *, uint32_t> heapBases;

  virtual void SetUp() {
    FILE* fp = fopen("/dev/urandom", "r");
    fread(sessionId, sizeof(uint8_t), kSessionIdSize, fp);
    fclose(fp);

    // Set default CdmResponseType value for gMock
    DefaultValue<CdmResponseType>::Set(wvcdm::NO_ERROR);
    heapBases.clear();
  }

  void setHeapBase(WVCryptoPlugin& plugin,
                   const sp<android::IMemoryHeap>& heap) {
    ASSERT_NE(heap, nullptr);

    void* heapBase = heap->getBase();
    ASSERT_NE(heapBase, nullptr);

    native_handle_t* nativeHandle = native_handle_create(1, 0);
    ASSERT_NE(nativeHandle, nullptr);

    nativeHandle->data[0] = heap->getHeapID();

    auto hidlHandle = hidl_handle(nativeHandle);
    auto hidlMemory = hidl_memory("ashmem", hidlHandle, heap->getSize());
    heapBases.insert(
        std::pair<void*, uint32_t>(heapBase, nextBufferId));
    Return<void> hResult =
        plugin.setSharedBufferBase(hidlMemory, nextBufferId++);

    ALOGE_IF(!hResult.isOk(), "setHeapBase failed setSharedBufferBase");
  }

  void toSharedBuffer(WVCryptoPlugin& plugin,
                      const sp<android::IMemory>& memory,
                      SharedBuffer* buffer) {
    ssize_t offset;
    size_t size;

    ASSERT_NE(memory, nullptr);
    ASSERT_NE(buffer, nullptr);

    sp<android::IMemoryHeap> heap = memory->getMemory(&offset, &size);
    ASSERT_NE(heap, nullptr);

    setHeapBase(plugin, heap);
    buffer->bufferId = heapBases[heap->getBase()];
    buffer->offset = offset >= 0 ? offset : 0;
    buffer->size = size;
  }
};

TEST_F(WVCryptoPluginTest, CorrectlyReportsSecureBuffers) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();

  CdmQueryMap l1Map;
  l1Map[QUERY_KEY_SECURITY_LEVEL] = QUERY_VALUE_SECURITY_LEVEL_L1;
  CdmQueryMap l3Map;
  l3Map[QUERY_KEY_SECURITY_LEVEL] = QUERY_VALUE_SECURITY_LEVEL_L3;

  // Provide the expected behavior for IsOpenSession
  EXPECT_CALL(*cdm, IsOpenSession(_))
      .WillRepeatedly(testing::Return(true));

  // Specify the expected calls to QuerySessionStatus
  EXPECT_CALL(*cdm, QuerySessionStatus(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(l1Map),
                      testing::Return(wvcdm::NO_ERROR)))
      .WillOnce(DoAll(SetArgPointee<1>(l3Map),
                      testing::Return(wvcdm::NO_ERROR)));

  WVCryptoPlugin plugin(sessionId, kSessionIdSize, cdm.get());

  EXPECT_TRUE(plugin.requiresSecureDecoderComponent("video/mp4")) <<
      "WVCryptoPlugin incorrectly allows an insecure video decoder on L1";
  EXPECT_FALSE(plugin.requiresSecureDecoderComponent("video/mp4")) <<
      "WVCryptoPlugin incorrectly expects a secure video decoder on L3";
  EXPECT_FALSE(plugin.requiresSecureDecoderComponent("audio/aac")) <<
      "WVCryptoPlugin incorrectly expects a secure audio decoder";
}

// Factory for matchers that perform deep matching of values against a
// CdmDecryptionParameters struct. For use in the test AttemptsToDecrypt.
class CDPMatcherFactory {
  public:
    // Some values do not change over the course of the test.  To avoid having
    // to re-specify them at every call site, we pass them into the factory
    // constructor.
    CDPMatcherFactory(bool isSecure, CdmCipherMode cipherMode, uint8_t* keyId,
                      void* out, size_t outLen, bool isVideo)
        : mIsSecure(isSecure), mCipherMode(cipherMode), mKeyId(keyId),
          mOut(out), mOutLen(outLen), mIsVideo(isVideo) {}

    testing::Matcher<const CdmDecryptionParameters&> operator()(
        bool isEncrypted,
        uint8_t* in,
        size_t inLen,
        uint8_t* iv,
        size_t blockOffset,
        size_t outOffset,
        uint8_t flags,
        CdmCencPatternEncryptionDescriptor& cdmPatternDesc) const {
      // TODO b/28295739
      // Add New MediaCrypto Unit Tests for CBC & Pattern Mode
      // in cdmPatternDesc.
      return testing::Truly(CDPMatcher(mIsSecure, mCipherMode, mKeyId, mOut,
                                       mOutLen, isEncrypted, in, inLen, iv,
                                       blockOffset, outOffset, flags, mIsVideo,
                                       cdmPatternDesc));
    }

  private:
    // Predicate that validates that the fields of a passed-in
    // CdmDecryptionParameters match the values it was given at construction
    // time.
    class CDPMatcher {
      public:
        CDPMatcher(bool isSecure, CdmCipherMode cipherMode, uint8_t* keyId,
                   void* out, size_t outLen, bool isEncrypted, uint8_t* in,
                   size_t inLen, uint8_t* iv, size_t blockOffset,
                   size_t outOffset, uint8_t flags, bool isVideo,
                   CdmCencPatternEncryptionDescriptor& cdmPatternDesc)
            : mIsSecure(isSecure), mCipherMode(cipherMode), mKeyId(keyId),
              mOut(out), mOutLen(outLen), mIsEncrypted(isEncrypted), mIn(in),
              mInLen(inLen), mIv(iv), mBlockOffset(blockOffset),
              mOutOffset(outOffset), mFlags(flags), mIsVideo(isVideo),
              mCdmPatternDesc(cdmPatternDesc) {}

        bool operator()(const CdmDecryptionParameters& params) const {
          return params.is_secure == mIsSecure &&
                 params.cipher_mode == mCipherMode &&
                 Value(*params.key_id, ElementsAreArray(mKeyId, KEY_ID_SIZE)) &&
                 // TODO b/35259313
                 // Converts mOut from hidl address to physical address
                 // params.decrypt_buffer == mOut &&
                 params.decrypt_buffer_length == mOutLen &&
                 params.is_encrypted == mIsEncrypted &&
                 // TODO b/35259313
                 // Converts mIn from hidl address to physical address
                 // params.encrypt_buffer == mIn &&
                 params.encrypt_length == mInLen &&
                 Value(*params.iv, ElementsAreArray(mIv, KEY_IV_SIZE)) &&
                 params.block_offset == mBlockOffset &&
                 params.decrypt_buffer_offset == mOutOffset &&
                 params.subsample_flags == mFlags &&
                 params.is_video == mIsVideo;
        }

      private:
        bool mIsSecure;
        CdmCipherMode mCipherMode;
        uint8_t* mKeyId;
        void* mOut;
        size_t mOutLen;
        bool mIsEncrypted;
        uint8_t* mIn;
        size_t mInLen;
        uint8_t* mIv;
        size_t mBlockOffset;
        size_t mOutOffset;
        uint8_t mFlags;
        bool mIsVideo;
        CdmCencPatternEncryptionDescriptor mCdmPatternDesc;
    };

    bool mIsSecure;
    CdmCipherMode mCipherMode;
    uint8_t* mKeyId;
    void* mOut;
    size_t mOutLen;
    bool mIsVideo;
};

TEST_F(WVCryptoPluginTest, AttemptsToDecrypt) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();

  static const size_t kSubSampleCount = 6;

  SubSample subSamples[kSubSampleCount];
  memset(subSamples, 0, sizeof(subSamples));
  subSamples[0].numBytesOfEncryptedData = 16;
  subSamples[1].numBytesOfClearData = 16;
  subSamples[1].numBytesOfEncryptedData = 16;
  subSamples[2].numBytesOfEncryptedData = 8;
  subSamples[3].numBytesOfClearData = 29;
  subSamples[3].numBytesOfEncryptedData = 24;
  subSamples[4].numBytesOfEncryptedData = 60;
  subSamples[5].numBytesOfEncryptedData = 16;

  std::vector<SubSample> subSamplesVector(
      subSamples, subSamples + sizeof(subSamples) / sizeof(subSamples[0]));
  auto hSubSamples = hidl_vec<SubSample>(subSamplesVector);

  uint8_t baseIv[KEY_IV_SIZE];
  uint8_t keyId[KEY_ID_SIZE];

  static const size_t kDataSize = 185;
  uint8_t in[kDataSize];

  FILE* fp = fopen("/dev/urandom", "r");
  fread(keyId, sizeof(uint8_t), KEY_ID_SIZE, fp);
  fread(baseIv, sizeof(uint8_t), KEY_IV_SIZE, fp);
  fread(in, sizeof(uint8_t), kDataSize, fp);
  fclose(fp);

  sp<MemoryDealer> memDealer = new MemoryDealer(
      kDataSize * 2, "WVCryptoPlugin_test");
  sp<android::IMemory> source = memDealer->allocate(kDataSize);
  ASSERT_NE(source, nullptr);
  pSrc = static_cast<uint8_t*>(
      static_cast<void*>(source->pointer()));
  ASSERT_NE(pSrc, nullptr);
  memcpy(pSrc, in, source->size());

  sp<android::IMemory> destination = memDealer->allocate(kDataSize);
  ASSERT_NE(destination, nullptr);
  pDest = static_cast<uint8_t*>(
      static_cast<void*>(destination->pointer()));
  ASSERT_NE(pDest, nullptr);

  uint8_t iv[5][KEY_IV_SIZE];
  memcpy(iv[0], baseIv, sizeof(baseIv));
  iv[0][15] = 0;
  memcpy(iv[1], baseIv, sizeof(baseIv));
  iv[1][15] = 1;
  memcpy(iv[2], baseIv, sizeof(baseIv));
  iv[2][15] = 2;
  memcpy(iv[3], baseIv, sizeof(baseIv));
  iv[3][15] = 4;
  memcpy(iv[4], baseIv, sizeof(baseIv));
  iv[4][15] = 7;

  CdmCencPatternEncryptionDescriptor cdmPattern;
  CDPMatcherFactory ParamsAre =
      CDPMatcherFactory(false, kCipherModeCtr, keyId, pDest, kDataSize, true);

  // Provide the expected behavior for IsOpenSession
  EXPECT_CALL(*cdm, IsOpenSession(_))
      .WillRepeatedly(testing::Return(true));

  // Specify the expected calls to Decrypt
  {
    InSequence calls;

    // SubSample 0
    EXPECT_CALL(*cdm, Decrypt(ElementsAreArray(sessionId, kSessionIdSize),
                             true,
                             ParamsAre(true, pSrc, 16, iv[0], 0, 0,
                                       OEMCrypto_FirstSubsample, cdmPattern)))
        .Times(1);

    // SubSample 1
    EXPECT_CALL(*cdm, Decrypt(ElementsAreArray(sessionId, kSessionIdSize),
                             true,
                             ParamsAre(false, pSrc + 16, 16, iv[1], 0, 16, 0,
                                       cdmPattern)))
        .Times(1);

    EXPECT_CALL(*cdm, Decrypt(ElementsAreArray(sessionId, kSessionIdSize),
                             true,
                             ParamsAre(true, pSrc + 32, 16, iv[1], 0, 32, 0,
                                       cdmPattern)))
        .Times(1);

    // SubSample 2
    EXPECT_CALL(*cdm, Decrypt(ElementsAreArray(sessionId, kSessionIdSize),
                             true,
                             ParamsAre(true, pSrc + 48, 8, iv[2], 0, 48, 0,
                                       cdmPattern)))
        .Times(1);

    // SubSample 3
    EXPECT_CALL(*cdm, Decrypt(ElementsAreArray(sessionId, kSessionIdSize),
                             true,
                             ParamsAre(false, pSrc + 56, 29, iv[2], 0, 56, 0,
                                       cdmPattern)))
        .Times(1);

    EXPECT_CALL(*cdm, Decrypt(ElementsAreArray(sessionId, kSessionIdSize),
                             true,
                             ParamsAre(true, pSrc + 85, 24, iv[2], 8, 85, 0,
                                       cdmPattern)))
        .Times(1);

    // SubSample 4
    EXPECT_CALL(*cdm, Decrypt(ElementsAreArray(sessionId, kSessionIdSize),
                             true,
                             ParamsAre(true, pSrc + 109, 60, iv[3], 0, 109, 0,
                                       cdmPattern)))
        .Times(1);

    // SubSample 5
    EXPECT_CALL(*cdm, Decrypt(ElementsAreArray(sessionId, kSessionIdSize),
                             true,
                             ParamsAre(true, pSrc + 169, 16, iv[4], 12, 169,
                                       OEMCrypto_LastSubsample, cdmPattern)))
        .Times(1);
  }

  WVCryptoPlugin plugin(sessionId, kSessionIdSize, cdm.get());

  uint32_t bytesWritten = 0;
  std::string errorDetailMessage;
  DestinationBuffer hDestination;
  hDestination.type = BufferType::SHARED_MEMORY;
  toSharedBuffer(plugin, destination, &hDestination.nonsecureMemory);
  Pattern noPattern = { 0, 0 };

  SharedBuffer sourceBuffer;
  toSharedBuffer(plugin, source, &sourceBuffer);

  plugin.decrypt(
      false, hidl_array<uint8_t, 16>(keyId), hidl_array<uint8_t, 16>(iv[0]),
      Mode::AES_CTR, noPattern, hSubSamples, sourceBuffer, 0, hDestination,
      [&](Status status, uint32_t hBytesWritten, hidl_string hDetailedError) {
        EXPECT_EQ(status, Status::OK);

        bytesWritten = hBytesWritten;
        errorDetailMessage.assign(hDetailedError.c_str());
      });

  EXPECT_EQ(kDataSize, bytesWritten) <<
            "WVCryptoPlugin decrypted the wrong number of bytes";
  EXPECT_EQ(0u, errorDetailMessage.size()) <<
            "WVCryptoPlugin reported a detailed error message.";
}

TEST_F(WVCryptoPluginTest, CommunicatesSecureBufferRequest) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();

  uint8_t keyId[KEY_ID_SIZE];
  uint8_t iv[KEY_IV_SIZE];

  static const size_t kDataSize = 32;
  uint8_t in[kDataSize];

  FILE* fp = fopen("/dev/urandom", "r");
  fread(keyId, sizeof(uint8_t), KEY_ID_SIZE, fp);
  fread(iv, sizeof(uint8_t), KEY_IV_SIZE, fp);
  fread(in, sizeof(uint8_t), kDataSize, fp);
  fclose(fp);

  SubSample subSample;
  subSample.numBytesOfClearData = 16;
  subSample.numBytesOfEncryptedData = 16;
  std::vector<SubSample> subSampleVector;
  subSampleVector.push_back(subSample);
  auto hSubSamples = hidl_vec<SubSample>(subSampleVector);

  // Provide the expected behavior for IsOpenSession
  EXPECT_CALL(*cdm, IsOpenSession(_))
      .WillRepeatedly(testing::Return(true));

  // Specify the expected calls to Decrypt
  {
    InSequence calls;

    typedef CdmDecryptionParameters CDP;

    EXPECT_CALL(*cdm, Decrypt(_, _, Field(&CDP::is_secure, false)))
        .Times(2);

    EXPECT_CALL(*cdm, Decrypt(_, _, Field(&CDP::is_secure, true)))
        .Times(2);
  }

  sp<MemoryDealer> memDealer = new MemoryDealer(
      kDataSize * 2, "WVCryptoPlugin_test");
  sp<android::IMemory> source = memDealer->allocate(kDataSize);
  ASSERT_NE(source, nullptr);
  pSrc = static_cast<uint8_t*>(
      static_cast<void*>(source->pointer()));
  ASSERT_NE(pSrc, nullptr);
  memcpy(pSrc, in, source->size());

  sp<android::IMemory> destination = memDealer->allocate(kDataSize);
  ASSERT_NE(destination, nullptr);
  pDest = static_cast<uint8_t*>(
      static_cast<void*>(destination->pointer()));
  ASSERT_NE(pDest, nullptr);

  WVCryptoPlugin plugin(sessionId, kSessionIdSize, cdm.get());

  uint32_t bytesWritten = 0;
  std::string errorDetailMessage;
  DestinationBuffer hDestination;
  hDestination.type = BufferType::SHARED_MEMORY;
  toSharedBuffer(plugin, destination, &hDestination.nonsecureMemory);
  Pattern noPattern = { 0, 0 };

  SharedBuffer sourceBuffer;
  toSharedBuffer(plugin, source, &sourceBuffer);

  plugin.decrypt(
      false, hidl_array<uint8_t, 16>(keyId), hidl_array<uint8_t, 16>(iv),
      Mode::AES_CTR, noPattern, hSubSamples, sourceBuffer, 0, hDestination,
      [&](Status status, uint32_t hBytesWritten, hidl_string hDetailedError) {
        EXPECT_EQ(status, Status::OK);

        bytesWritten = hBytesWritten;
        errorDetailMessage.assign(hDetailedError.c_str());
      });

  EXPECT_EQ(0u, errorDetailMessage.size()) <<
            "WVCryptoPlugin reported a detailed error message.";

  plugin.decrypt(
      true, hidl_array<uint8_t, 16>(keyId), hidl_array<uint8_t, 16>(iv),
      Mode::AES_CTR, noPattern, hSubSamples, sourceBuffer, 0, hDestination,
      [&](Status status, uint32_t hBytesWritten, hidl_string hDetailedError) {
        EXPECT_EQ(status, Status::OK);

        bytesWritten = hBytesWritten;
        errorDetailMessage.assign(hDetailedError.c_str());
      });

  EXPECT_EQ(0u, errorDetailMessage.size()) <<
            "WVCryptoPlugin reported a detailed error message.";
}

TEST_F(WVCryptoPluginTest, SetsFlagsForMinimumSubsampleRuns) {
  android::sp<MockCDM> cdm = new MockCDM();

  uint8_t keyId[KEY_ID_SIZE];
  uint8_t iv[KEY_IV_SIZE];

  static const size_t kDataSize = 16;
  uint8_t in[kDataSize];

  FILE* fp = fopen("/dev/urandom", "r");
  fread(keyId, sizeof(uint8_t), KEY_ID_SIZE, fp);
  fread(iv, sizeof(uint8_t), KEY_IV_SIZE, fp);
  fread(in, sizeof(uint8_t), kDataSize, fp);
  fclose(fp);

  SubSample clearSubSample;
  clearSubSample.numBytesOfClearData = 16;
  clearSubSample.numBytesOfEncryptedData = 0;
  std::vector<SubSample> clearSubSampleVector;
  clearSubSampleVector.push_back(clearSubSample);
  auto hClearSubSamples = hidl_vec<SubSample>(clearSubSampleVector);

  SubSample encryptedSubSample;
  encryptedSubSample.numBytesOfClearData = 0;
  encryptedSubSample.numBytesOfEncryptedData = 16;
  std::vector<SubSample> encryptedSubSampleVector;
  encryptedSubSampleVector.push_back(encryptedSubSample);
  auto hEncryptedSubSamples = hidl_vec<SubSample>(encryptedSubSampleVector);

  SubSample mixedSubSample;
  mixedSubSample.numBytesOfClearData = 8;
  mixedSubSample.numBytesOfEncryptedData = 8;
  std::vector<SubSample> mixedSubSampleVector;
  mixedSubSampleVector.push_back(mixedSubSample);
  auto hMixedSubSamples = hidl_vec<SubSample>(mixedSubSampleVector);

  // Provide the expected behavior for IsOpenSession
  EXPECT_CALL(*cdm, IsOpenSession(_))
      .WillRepeatedly(testing::Return(true));

  // Specify the expected calls to Decrypt
  {
    InSequence calls;

    typedef CdmDecryptionParameters CDP;

    EXPECT_CALL(*cdm, Decrypt(_, _, Field(&CDP::subsample_flags,
                                          OEMCrypto_FirstSubsample |
                                          OEMCrypto_LastSubsample)))
        .Times(2);

    EXPECT_CALL(*cdm, Decrypt(_, _, Field(&CDP::subsample_flags,
                                          OEMCrypto_FirstSubsample)))
        .Times(1);

    EXPECT_CALL(*cdm, Decrypt(_, _, Field(&CDP::subsample_flags,
                                          OEMCrypto_LastSubsample)))
        .Times(1);
  }

  sp<MemoryDealer> memDealer = new MemoryDealer(
      kDataSize * 2, "WVCryptoPlugin_test");
  sp<android::IMemory> source = memDealer->allocate(kDataSize);
  ASSERT_NE(source, nullptr);
  pSrc = static_cast<uint8_t*>(
      static_cast<void*>(source->pointer()));
  ASSERT_NE(pSrc, nullptr);
  memcpy(pSrc, in, source->size());

  sp<android::IMemory> destination = memDealer->allocate(kDataSize);
  ASSERT_NE(destination, nullptr);
  pDest = static_cast<uint8_t*>(
      static_cast<void*>(destination->pointer()));
  ASSERT_NE(pDest, nullptr);

  WVCryptoPlugin plugin(sessionId, kSessionIdSize, cdm.get());

  uint32_t bytesWritten = 0;
  std::string errorDetailMessage;
  DestinationBuffer hDestination;
  hDestination.type = BufferType::SHARED_MEMORY;
  toSharedBuffer(plugin, destination, &hDestination.nonsecureMemory);
  Pattern noPattern = { 0, 0 };

  SharedBuffer sourceBuffer;
  toSharedBuffer(plugin, source, &sourceBuffer);

  plugin.decrypt(
      false, hidl_array<uint8_t, 16>(keyId), hidl_array<uint8_t, 16>(iv),
      Mode::AES_CTR, noPattern, hClearSubSamples, sourceBuffer, 0, hDestination,
      [&](Status status, uint32_t hBytesWritten, hidl_string hDetailedError) {
        EXPECT_EQ(status, Status::OK);

        bytesWritten = hBytesWritten;
        errorDetailMessage.assign(hDetailedError.c_str());
      });

  EXPECT_EQ(0u, errorDetailMessage.size()) <<
            "WVCryptoPlugin reported a detailed error message.";

  plugin.decrypt(
      false, hidl_array<uint8_t, 16>(keyId), hidl_array<uint8_t, 16>(iv),
      Mode::AES_CTR, noPattern, hEncryptedSubSamples, sourceBuffer, 0,
      hDestination,
      [&](Status status, uint32_t hBytesWritten, hidl_string hDetailedError) {
        EXPECT_EQ(status, Status::OK);

        bytesWritten = hBytesWritten;
        errorDetailMessage.assign(hDetailedError.c_str());
      });

  EXPECT_EQ(0u, errorDetailMessage.size()) <<
            "WVCryptoPlugin reported a detailed error message.";

  plugin.decrypt(
      false, hidl_array<uint8_t, 16>(keyId), hidl_array<uint8_t, 16>(iv),
      Mode::AES_CTR, noPattern, hMixedSubSamples, sourceBuffer, 0, hDestination,
      [&](Status status, uint32_t hBytesWritten, hidl_string hDetailedError) {
        EXPECT_EQ(status, Status::OK);

        bytesWritten = hBytesWritten;
        errorDetailMessage.assign(hDetailedError.c_str());
      });

  EXPECT_EQ(0u, errorDetailMessage.size()) <<
            "WVCryptoPlugin reported a detailed error message.";
}

TEST_F(WVCryptoPluginTest, AllowsSessionIdChanges) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();

  uint8_t keyId[KEY_ID_SIZE];
  uint8_t iv[KEY_IV_SIZE];
  uint8_t sessionId2[kSessionIdSize];

  static const size_t kDataSize = 32;
  uint8_t in[kDataSize];

  FILE* fp = fopen("/dev/urandom", "r");
  fread(keyId, sizeof(uint8_t), KEY_ID_SIZE, fp);
  fread(iv, sizeof(uint8_t), KEY_IV_SIZE, fp);
  fread(sessionId2, sizeof(uint8_t), kSessionIdSize, fp);
  fread(in, sizeof(uint8_t), kDataSize, fp);
  fclose(fp);

  SubSample subSample;
  subSample.numBytesOfClearData = 16;
  subSample.numBytesOfEncryptedData = 16;
  std::vector<SubSample> subSampleVector;
  subSampleVector.push_back(subSample);
  auto hSubSamples = hidl_vec<SubSample>(subSampleVector);

  std::vector<uint8_t> sessionIdVector(sessionId, sessionId + kSessionIdSize);
  std::vector<uint8_t> sessionId2Vector(sessionId2,
                                        sessionId2 + kSessionIdSize);

  // Provide the expected behavior for IsOpenSession
  EXPECT_CALL(*cdm, IsOpenSession(_))
      .WillRepeatedly(testing::Return(true));

  // Specify the expected calls to Decrypt
  {
    InSequence calls;

    EXPECT_CALL(*cdm,
                Decrypt(ElementsAreArray(sessionId, kSessionIdSize), _, _))
        .Times(2);

    EXPECT_CALL(*cdm,
                Decrypt(ElementsAreArray(sessionId2, kSessionIdSize), _, _))
        .Times(2);
  }

  sp<MemoryDealer> memDealer = new MemoryDealer(
      kDataSize * 2, "WVCryptoPlugin_test");
  sp<android::IMemory> source = memDealer->allocate(kDataSize);
  ASSERT_NE(source, nullptr);
  pSrc = static_cast<uint8_t*>(
      static_cast<void*>(source->pointer()));
  ASSERT_NE(pSrc, nullptr);
  memcpy(pSrc, in, source->size());

  sp<android::IMemory> destination = memDealer->allocate(kDataSize);
  ASSERT_NE(destination, nullptr);
  pDest = static_cast<uint8_t*>(
      static_cast<void*>(destination->pointer()));
  ASSERT_NE(pDest, nullptr);

  uint8_t blank[1];  // Some compilers will not accept 0.
  WVCryptoPlugin plugin(blank, 0, cdm.get());

  uint32_t bytesWritten = 0;
  std::string errorDetailMessage;
  DestinationBuffer hDestination;
  hDestination.type = BufferType::SHARED_MEMORY;
  toSharedBuffer(plugin, destination, &hDestination.nonsecureMemory);
  Pattern noPattern = { 0, 0 };

  SharedBuffer sourceBuffer;
  toSharedBuffer(plugin, source, &sourceBuffer);

  Status status = plugin.setMediaDrmSession(sessionIdVector);
  EXPECT_EQ(status, Status::OK);

  plugin.decrypt(
      false, hidl_array<uint8_t, 16>(keyId), hidl_array<uint8_t, 16>(iv),
      Mode::AES_CTR, noPattern, hSubSamples, sourceBuffer, 0, hDestination,
      [&](Status status, uint32_t hBytesWritten, hidl_string hDetailedError) {
        EXPECT_EQ(status, Status::OK);

        bytesWritten = hBytesWritten;
        errorDetailMessage.assign(hDetailedError.c_str());
      });

  EXPECT_EQ(0u, errorDetailMessage.size()) <<
            "WVCryptoPlugin reported a detailed error message.";

  status = plugin.setMediaDrmSession(sessionId2Vector);
  EXPECT_EQ(status, Status::OK);

  plugin.decrypt(
      false, hidl_array<uint8_t, 16>(keyId), hidl_array<uint8_t, 16>(iv),
      Mode::AES_CTR, noPattern, hSubSamples, sourceBuffer, 0, hDestination,
      [&](Status status, uint32_t hBytesWritten, hidl_string hDetailedError) {
        EXPECT_EQ(status, Status::OK);

        bytesWritten = hBytesWritten;
        errorDetailMessage.assign(hDetailedError.c_str());
      });

  EXPECT_EQ(0u, errorDetailMessage.size()) <<
            "WVCryptoPlugin reported a detailed error message.";
}

TEST_F(WVCryptoPluginTest, DisallowsUnopenedSessionIdChanges) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();

  uint8_t blank[1];  // Some compilers will not accept 0.
  std::vector<uint8_t> sessionIdVector(sessionId, sessionId + kSessionIdSize);

  // Specify the expected calls to IsOpenSession
  {
    InSequence calls;

    EXPECT_CALL(*cdm, IsOpenSession(ElementsAreArray(blank, 0)))
        .WillOnce(testing::Return(false));

    EXPECT_CALL(*cdm, IsOpenSession(ElementsAreArray(sessionId, kSessionIdSize)))
        .WillOnce(testing::Return(false))
        .WillOnce(testing::Return(true));
  }

  WVCryptoPlugin plugin(blank, 0, cdm.get());

  Status status = plugin.setMediaDrmSession(sessionIdVector);
  EXPECT_EQ(status, Status::ERROR_DRM_SESSION_NOT_OPENED);

  status = plugin.setMediaDrmSession(sessionIdVector);
  EXPECT_EQ(status, Status::OK);
}

}  // namespace widevine
}  // namespace V1_2
}  // namespace drm
}  // namespace hardware
}  // namespace wvdrm
