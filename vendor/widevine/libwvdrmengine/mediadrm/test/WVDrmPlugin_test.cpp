//
// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
//#define LOG_NDEBUG 0
#define LOG_TAG "WVDrmPluginTest"
#include <utils/Log.h>

#include <stdio.h>
#include <ostream>
#include <string>
#include <list>
#include <vector>

#include "cdm_client_property_set.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "media/stagefright/foundation/ABase.h"
#include "media/stagefright/MediaErrors.h"
#include "string_conversions.h"
#include "wv_cdm_constants.h"
#include "wv_cdm_types.h"
#include "wv_content_decryption_module.h"
#include "HidlTypes.h"
#include "TypeConvert.h"
#include "WVDrmPlugin.h"
#include "WVErrors.h"

namespace wvdrm {
namespace hardware {
namespace drm {
namespace V1_2 {
namespace widevine {

using android::hardware::drm::V1_2::widevine::toHidlVec;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Args;
using ::testing::AtLeast;
using ::testing::DefaultValue;
using ::testing::ElementsAreArray;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::NotNull;
using ::testing::Pointee;
using ::testing::SaveArg;
using ::testing::SetArgPointee;
using ::testing::SetArrayArgument;
using ::testing::StrictMock;
using ::testing::StrEq;
using ::testing::Test;
using ::testing::Values;
using ::testing::WithParamInterface;
using ::testing::internal::ElementsAreArrayMatcher;

using wvcdm::kCertificateWidevine;
using wvcdm::kKeyRequestTypeInitial;
using wvcdm::kKeyRequestTypeRelease;
using wvcdm::kKeyRequestTypeRenewal;
using wvcdm::kLicenseTypeOffline;
using wvcdm::kLicenseTypeRelease;
using wvcdm::kLicenseTypeStreaming;
using wvcdm::kSecurityLevelL1;
using wvcdm::kSecurityLevelL3;
using wvcdm::Base64Encode;
using wvcdm::CdmAppParameterMap;
using wvcdm::CdmCertificateType;
using wvcdm::CdmClientPropertySet;
using wvcdm::CdmIdentifier;
using wvcdm::CdmInitData;
using wvcdm::CdmKeyMessage;
using wvcdm::CdmKeyResponse;
using wvcdm::CdmKeyRequest;
using wvcdm::CdmKeySetId;
using wvcdm::CdmKeySystem;
using wvcdm::CdmLicenseType;
using wvcdm::CdmOfflineLicenseState;
using wvcdm::CdmProvisioningRequest;
using wvcdm::CdmProvisioningResponse;
using wvcdm::CdmQueryMap;
using wvcdm::CdmSecureStopId;
using wvcdm::CdmSecurityLevel;
using wvcdm::CdmUsageInfo;
using wvcdm::CdmUsageInfoReleaseMessage;
using wvcdm::EMPTY_ORIGIN;
using wvcdm::KEY_ID_SIZE;
using wvcdm::KEY_IV_SIZE;
using wvcdm::KEY_SET_ID_PREFIX;
using wvcdm::NEVER_EXPIRES;
using wvcdm::QUERY_KEY_CURRENT_SRM_VERSION;
using wvcdm::QUERY_KEY_DEVICE_ID;
using wvcdm::QUERY_KEY_DECRYPT_HASH_SUPPORT;
using wvcdm::QUERY_KEY_MAX_NUMBER_OF_SESSIONS;
using wvcdm::QUERY_KEY_NUMBER_OF_OPEN_SESSIONS;
using wvcdm::QUERY_KEY_OEMCRYPTO_API_VERSION;
using wvcdm::QUERY_KEY_OEMCRYPTO_SESSION_ID;
using wvcdm::QUERY_KEY_PROVISIONING_ID;
using wvcdm::QUERY_KEY_SECURITY_LEVEL;
using wvcdm::QUERY_KEY_SRM_UPDATE_SUPPORT;
using wvcdm::QUERY_KEY_SYSTEM_ID;
using wvcdm::QUERY_KEY_WVCDM_VERSION;
using wvcdm::QUERY_KEY_RESOURCE_RATING_TIER;
using wvcdm::QUERY_KEY_OEMCRYPTO_BUILD_INFORMATION;
using wvcdm::QUERY_VALUE_SECURITY_LEVEL_L1;
using wvcdm::QUERY_VALUE_SECURITY_LEVEL_L3;
using wvcdm::SESSION_ID_PREFIX;
using wvcdm::WvCdmEventListener;

namespace {
const std::string kEmptyString;
const std::string kOrigin("widevine.com");
const std::string kAppId("com.unittest.mock.app.id");
const uint8_t* const kUnprovisionResponse =
    reinterpret_cast<const uint8_t*>("unprovision");
const size_t kUnprovisionResponseSize = 11;
const std::string kDeviceId("0123456789\0ABCDEF", 17);

// This is a serialized WvCdmMetrics message containing a small amount of
// sample data. This ensures we're able to extract it via a property.
const char kSerializedMetricsHex[] =
    "0a580a001a0210072202100d2a02100832182216636f6d2e676f6f676c652e616e64726f69"
    "642e676d734208220631342e302e304a06080112020800520610d5f3fad5056a0b1d00fd4c"
    "47280132020804a2010608011202080012cb010a0622047369643412b5010a021001520919"
    "1d5a643bdfff50405a0b1d00d01945280132021001620d1d00f8e84528013204080020006a"
    "0310b739820102100d8a01060801120248009a010310ff01da0106080112024800e2010e1d"
    "005243472801320528800248009a020d1d00b016452801320428404800a202060801120248"
    "19aa0206080212024800b2020b1d8098f047280132024800ba02021001ca020b1d00101945"
    "280132024800e202021004fa02021002a203021000b2030210021a09196891ed7c3f355040";

#define N_ELEM(a) (sizeof(a)/sizeof(a[0]))
}  // anonymous namespace

class MockCDM : public WvContentDecryptionModule {
 public:
  MOCK_METHOD5(OpenSession, CdmResponseType(const CdmKeySystem&,
                                            CdmClientPropertySet*,
                                            const CdmIdentifier&,
                                            WvCdmEventListener*,
                                            CdmSessionId*));

  MOCK_METHOD1(CloseSession, CdmResponseType(const CdmSessionId&));

  MOCK_METHOD9(GenerateKeyRequest,
                CdmResponseType(const CdmSessionId&, const CdmKeySetId&,
                                const std::string&, const CdmInitData&,
                                const CdmLicenseType, CdmAppParameterMap&,
                                CdmClientPropertySet*, const CdmIdentifier&,
                                CdmKeyRequest*));

  MOCK_METHOD3(AddKey, CdmResponseType(const CdmSessionId&,
                                       const CdmKeyResponse&,
                                       CdmKeySetId*));

  MOCK_METHOD1(RemoveKeys, CdmResponseType(const CdmSessionId&));

  MOCK_METHOD2(RestoreKey, CdmResponseType(const CdmSessionId&,
                                           const CdmKeySetId&));

  MOCK_METHOD3(QueryStatus, CdmResponseType(wvcdm::SecurityLevel,
                                            const std::string&,
                                            std::string*));

  MOCK_METHOD2(QueryKeyStatus, CdmResponseType(const CdmSessionId&,
                                               CdmQueryMap*));

  MOCK_METHOD2(QueryOemCryptoSessionId, CdmResponseType(const CdmSessionId&,
                                                        CdmQueryMap*));

  MOCK_METHOD6(GetProvisioningRequest, CdmResponseType(CdmCertificateType,
                                                       const std::string&,
                                                       const CdmIdentifier&,
                                                       const std::string&,
                                                       CdmProvisioningRequest*,
                                                       std::string*));

  MOCK_METHOD4(HandleProvisioningResponse,
      CdmResponseType(const CdmIdentifier&, CdmProvisioningResponse&,
                      std::string*, std::string*));

  MOCK_METHOD2(Unprovision, CdmResponseType(CdmSecurityLevel,
                                            const CdmIdentifier&));

  MOCK_METHOD3(GetUsageInfo, CdmResponseType(const std::string&,
                                             const CdmIdentifier&,
                                             CdmUsageInfo*));

  MOCK_METHOD4(GetUsageInfo, CdmResponseType(const std::string&,
                                             const CdmSecureStopId&,
                                             const CdmIdentifier&,
                                             CdmUsageInfo*));

  MOCK_METHOD2(RemoveAllUsageInfo, CdmResponseType(const std::string&,
                                                   const CdmIdentifier&));

  MOCK_METHOD2(ReleaseUsageInfo,
      CdmResponseType(const CdmUsageInfoReleaseMessage&, const CdmIdentifier&));

  MOCK_METHOD1(IsValidServiceCertificate, bool(const std::string&));

  MOCK_METHOD2(GetMetrics, CdmResponseType(const CdmIdentifier&,
                                           drm_metrics::WvCdmMetrics*));

  MOCK_METHOD2(GetDecryptHashError,
               CdmResponseType(const CdmSessionId&, std::string*));

  MOCK_METHOD3(ListStoredLicenses,
      CdmResponseType(CdmSecurityLevel,
                      const CdmIdentifier&,
                      std::vector<std::string>*));

  MOCK_METHOD4(GetOfflineLicenseState,
      CdmResponseType(const std::string&,
                      CdmSecurityLevel,
                      const CdmIdentifier&,
                      CdmOfflineLicenseState*));

  MOCK_METHOD3(RemoveOfflineLicense,
      CdmResponseType(const std::string&,
                      CdmSecurityLevel,
                      const CdmIdentifier&));
};

class MockCrypto : public WVGenericCryptoInterface {
 public:
  MOCK_METHOD3(selectKey, OEMCryptoResult(const OEMCrypto_SESSION,
                                          const uint8_t*, size_t));

  MOCK_METHOD6(encrypt, OEMCryptoResult(OEMCrypto_SESSION, const uint8_t*,
                                        size_t, const uint8_t*,
                                        OEMCrypto_Algorithm, uint8_t*));

  MOCK_METHOD6(decrypt, OEMCryptoResult(OEMCrypto_SESSION, const uint8_t*,
                                        size_t, const uint8_t*,
                                        OEMCrypto_Algorithm, uint8_t*));

  MOCK_METHOD6(sign, OEMCryptoResult(OEMCrypto_SESSION, const uint8_t*, size_t,
                                     OEMCrypto_Algorithm, uint8_t*, size_t*));

  MOCK_METHOD6(verify, OEMCryptoResult(OEMCrypto_SESSION, const uint8_t*,
                                       size_t, OEMCrypto_Algorithm,
                                       const uint8_t*, size_t));

  MOCK_METHOD3(loadDeviceRSAKey, OEMCryptoResult(OEMCrypto_SESSION,
                                                 const uint8_t*, size_t));

  MOCK_METHOD6(generateRSASignature, OEMCryptoResult(OEMCrypto_SESSION,
                                                     const uint8_t*, size_t,
                                                     uint8_t*, size_t*,
                                                     RSA_Padding_Scheme));
};

class MockDrmPluginListener : public IDrmPluginListener_V1_2 {
 public:
  MOCK_METHOD3(sendEvent, Return<void>(EventType, const hidl_vec<uint8_t>&,
                                       const hidl_vec<uint8_t>&));

  MOCK_METHOD2(sendExpirationUpdate,
               Return<void>(const hidl_vec<uint8_t>&, int64_t));

  MOCK_METHOD3(sendKeysChange, Return<void>(const hidl_vec<uint8_t>&,
                                            const hidl_vec<KeyStatus>&, bool));

  MOCK_METHOD1(sendSessionLostState,
               Return<void>(const hidl_vec<uint8_t>&));

  MOCK_METHOD3(sendKeysChange_1_2, Return<void>(const hidl_vec<uint8_t>&,
                                                const hidl_vec<KeyStatus_V1_2>&, bool));

};

template <uint8_t DIGIT>
CdmResponseType setSessionIdOnMap(testing::Unused, CdmQueryMap* map) {
  static const char oecId[] = {DIGIT + '0', '\0'};
  (*map)[QUERY_KEY_OEMCRYPTO_SESSION_ID] = oecId;
  return wvcdm::NO_ERROR;
}

MATCHER_P(HasOrigin, origin, "") {
  return arg.origin == origin;
}

class WVDrmPluginTest : public Test {
 protected:
  static const uint32_t kKeySetIdSize = 32;
  static const uint32_t kSessionIdSize = 16;
  uint8_t keySetIdRaw[kKeySetIdSize];
  uint8_t sessionIdRaw[kSessionIdSize];
  std::vector<uint8_t> keySetId;
  std::vector<uint8_t> sessionId;
  CdmSessionId cdmSessionId;

  virtual void SetUp() {
    // Fill the session ID
    FILE* fp = fopen("/dev/urandom", "r");
    fread(sessionIdRaw, sizeof(uint8_t), kSessionIdSize, fp);

    memcpy(sessionIdRaw, SESSION_ID_PREFIX, sizeof(SESSION_ID_PREFIX) - 1);
    sessionId.assign(sessionIdRaw, sessionIdRaw + kSessionIdSize);
    cdmSessionId.assign(sessionId.begin(), sessionId.end());

    fread(keySetIdRaw, sizeof(uint8_t), kKeySetIdSize, fp);
    fclose(fp);

    memcpy(keySetIdRaw, KEY_SET_ID_PREFIX, sizeof(KEY_SET_ID_PREFIX));
    CdmKeySetId cdmKeySetId(reinterpret_cast<char*>(keySetIdRaw),
                            kKeySetIdSize);
    keySetId.assign(keySetIdRaw, keySetIdRaw + kKeySetIdSize);

    // Set default return values for gMock
    DefaultValue<CdmResponseType>::Set(wvcdm::NO_ERROR);
    DefaultValue<OEMCryptoResult>::Set(OEMCrypto_SUCCESS);
    DefaultValue<bool>::Set(true);
  }
};

struct OriginTestVariant {
  // For a test that does not expect any follow-up queries
  OriginTestVariant(const std::string& nameValue,
                    const std::string& originValue,
                    const std::string& expectedOriginValue)
    : name(nameValue), origin(originValue),
      expectedOrigin(expectedOriginValue) {}

  const std::string name;
  const std::string origin;
  const std::string expectedOrigin;
};

void PrintTo(const OriginTestVariant& param, ::std::ostream* os) {
  *os << param.name << " Variant";

}

class WVDrmPluginOriginTest : public WVDrmPluginTest,
                              public WithParamInterface<OriginTestVariant> {};

TEST_F(WVDrmPluginTest, OpensSessions) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  EXPECT_CALL(*cdm,
      OpenSession(StrEq("com.widevine"), _, HasOrigin(EMPTY_ORIGIN), _, _))
          .WillOnce(DoAll(SetArgPointee<4>(cdmSessionId),
                          testing::Return(wvcdm::NO_ERROR)));

  // Provide expected mock behavior
  EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Invoke(setSessionIdOnMap<4>));

  EXPECT_CALL(*cdm, CloseSession(_))
      .Times(AtLeast(0));

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);

  plugin.openSession([&](Status status, hidl_vec<uint8_t> hSessionId) {
    ASSERT_EQ(Status::OK, status);
    sessionId.clear();
    sessionId.assign(hSessionId.data(), hSessionId.data() + hSessionId.size());
  });

  EXPECT_THAT(sessionId, ElementsAreArray(sessionIdRaw, kSessionIdSize));
  Status status = plugin.closeSession(toHidlVec(sessionId));
  ASSERT_EQ(Status::OK, status);
}

TEST_F(WVDrmPluginTest, OpensSessions_1_1) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  const CdmClientPropertySet* propertySet = nullptr;

  // Provide expected mock behavior
  EXPECT_CALL(*cdm, QueryStatus(_, QUERY_KEY_SECURITY_LEVEL, _))
      .WillOnce(DoAll(SetArgPointee<2>(QUERY_VALUE_SECURITY_LEVEL_L1),
                      testing::Return(wvcdm::NO_ERROR)))
      .WillOnce(DoAll(SetArgPointee<2>(QUERY_VALUE_SECURITY_LEVEL_L3),
                      testing::Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, OpenSession(_, _, _, _, _))
        .WillRepeatedly(DoAll(SetArgPointee<4>(cdmSessionId),
                        SaveArg<1>(&propertySet),
                        testing::Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId, _))
      .WillRepeatedly(Invoke(setSessionIdOnMap<4>));

  EXPECT_CALL(*cdm, CloseSession(_))
      .Times(AtLeast(0));

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);

  Status status = plugin.setPropertyString(hidl_string("securityLevel"), hidl_string("L1"));
  ASSERT_EQ(Status::OK, status);

  plugin.openSession_1_1(android::hardware::drm::V1_1::SecurityLevel::SW_SECURE_CRYPTO,
      [&](Status status, hidl_vec<uint8_t> hSessionId) {
    ASSERT_EQ(Status::OK, status);
    sessionId.clear();
    sessionId.assign(hSessionId.data(), hSessionId.data() + hSessionId.size());
  });

  ASSERT_THAT(propertySet, NotNull());
  EXPECT_THAT(sessionId, ElementsAreArray(sessionIdRaw, kSessionIdSize));
  status = plugin.closeSession(toHidlVec(sessionId));
  ASSERT_EQ(Status::OK, status);
}

TEST_F(WVDrmPluginTest, ClosesSessions) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  EXPECT_CALL(*cdm, CloseSession(cdmSessionId))
      .Times(1);

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);
  Status status = plugin.closeSession(toHidlVec(sessionId));
  ASSERT_EQ(Status::OK, status);
}

TEST_F(WVDrmPluginTest, ClosesSessionWithError) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  EXPECT_CALL(*cdm, CloseSession(cdmSessionId))
      .WillOnce(testing::Return(wvcdm::SESSION_NOT_FOUND_1));

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);
  Status status = plugin.closeSession(toHidlVec(sessionId));
  ASSERT_EQ(Status::ERROR_DRM_SESSION_NOT_OPENED, status);
}

// TODO b/35325611 Fix this disabled test
TEST_F(WVDrmPluginTest, DISABLED_GeneratesKeyRequests) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  static const size_t kInitDataSize = 128;
  uint8_t initDataRaw[kInitDataSize];
  static const size_t kRequestSize = 256;
  uint8_t requestRaw[kRequestSize];
  static const uint32_t kKeySetIdSize = 32;
  uint8_t keySetIdRaw[kKeySetIdSize];
  FILE* fp = fopen("/dev/urandom", "r");
  fread(initDataRaw, sizeof(uint8_t), kInitDataSize, fp);
  fread(requestRaw, sizeof(uint8_t), kRequestSize, fp);
  fread(keySetIdRaw, sizeof(uint8_t), kKeySetIdSize, fp);
  fclose(fp);

  memcpy(keySetIdRaw, KEY_SET_ID_PREFIX, sizeof(KEY_SET_ID_PREFIX) - 1);
  CdmKeySetId cdmKeySetId(reinterpret_cast<char*>(keySetIdRaw), kKeySetIdSize);
  std::vector<uint8_t> keySetId;
  keySetId.assign(keySetIdRaw, keySetIdRaw + kKeySetIdSize);

  CdmInitData cdmInitData(reinterpret_cast<char*>(initDataRaw), kInitDataSize);
  std::vector<uint8_t> initData;
  initData.assign(initDataRaw, initDataRaw + kInitDataSize);

  static const uint8_t psshPrefix[] = {
    0, 0, 0, 32 + kInitDataSize,                    // Total size
    'p', 's', 's', 'h',                             // "PSSH"
    0, 0, 0, 0,                                     // Flags - must be zero
    0xED, 0xEF, 0x8B, 0xA9, 0x79, 0xD6, 0x4A, 0xCE, // Widevine UUID
    0xA3, 0xC8, 0x27, 0xDC, 0xD5, 0x1D, 0x21, 0xED,
    0, 0, 0, kInitDataSize                          // Size of initData
  };
  static const size_t kPsshPrefixSize = sizeof(psshPrefix);
  static const size_t kPsshBoxSize = kPsshPrefixSize + kInitDataSize;
  uint8_t psshBoxRaw[kPsshBoxSize];
  memcpy(psshBoxRaw, psshPrefix, kPsshPrefixSize);
  memcpy(psshBoxRaw + kPsshPrefixSize, initDataRaw, kInitDataSize);
  CdmInitData cdmPsshBox(reinterpret_cast<char*>(psshBoxRaw), kPsshBoxSize);
  std::vector<uint8_t> psshBox;
  psshBox.assign(psshBoxRaw, psshBoxRaw + kPsshBoxSize);

  CdmKeyMessage cdmRequest(requestRaw, requestRaw + kRequestSize);

  std::map<std::string, std::string> parameters;
  CdmAppParameterMap cdmParameters;
  parameters.insert(
      std::pair<std::string, std::string>("paddingScheme", "BUBBLE WRAP"));
  cdmParameters["paddingScheme"] = "BUBBLE WRAP";
  parameters.insert(
      std::pair<std::string, std::string>("favorite-particle", "tetraquark"));
  cdmParameters["favorite-particle"] = "tetraquark";
  parameters.insert(
      std::pair<std::string, std::string>("answer", "6 * 9"));
  cdmParameters["answer"] = "6 * 9";

  std::vector<KeyValue> optionalParameters;
  KeyValue keyValue;
  for (std::map<std::string, std::string>::iterator itr = parameters.begin();
      itr != parameters.end(); ++itr) {
    keyValue.key = itr->first;
    keyValue.value = itr->second;
    optionalParameters.push_back(keyValue);
  }

  static const char* kDefaultUrl = "http://google.com/";
  static const char* kIsoBmffMimeType = "cenc";
  static const char* kWebmMimeType = "webm";

  struct TestSet {
    const char* mimeType;
    const std::vector<uint8_t>& initDataIn;
    const CdmInitData& initDataOut;
  };

  // We run the same set of operations on several sets of data, simulating
  // different valid calling patterns.
  TestSet testSets[] = {
    {kIsoBmffMimeType, psshBox, cdmPsshBox},  // ISO-BMFF, EME passing style
    {kIsoBmffMimeType, initData, cdmPsshBox}, // ISO-BMFF, old passing style
    {kWebmMimeType, initData, cdmInitData}    // WebM
  };
  size_t testSetCount = N_ELEM(testSets);

  // Set up the expected calls. Per gMock rules, this must be done for all test
  // sets prior to testing any of them.
  {
    InSequence calls;

    for (size_t i = 0; i < testSetCount; ++i)
    {
      const char* mimeType = testSets[i].mimeType;
      const CdmInitData& initData = testSets[i].initDataOut;

      CdmKeyRequest initialRequest = {
        cdmRequest,
        kKeyRequestTypeInitial,
        kDefaultUrl
      };

      CdmKeyRequest renewalRequest = {
        cdmRequest,
        kKeyRequestTypeRenewal,
        kDefaultUrl
      };

      CdmKeyRequest releaseRequest = {
        cdmRequest,
        kKeyRequestTypeRelease,
        kDefaultUrl
      };

      EXPECT_CALL(*cdm, GenerateKeyRequest(cdmSessionId, "", mimeType, initData,
                                          kLicenseTypeOffline, cdmParameters,
                                          NotNull(), HasOrigin(EMPTY_ORIGIN),
                                          _))
          .WillOnce(DoAll(SetArgPointee<8>(initialRequest),
                          testing::Return(wvcdm::KEY_MESSAGE)));

      EXPECT_CALL(*cdm, GenerateKeyRequest(cdmSessionId, "", mimeType, initData,
                                          kLicenseTypeStreaming, cdmParameters,
                                          NotNull(), HasOrigin(EMPTY_ORIGIN),
                                          _))
          .WillOnce(DoAll(SetArgPointee<8>(renewalRequest),
                          testing::Return(wvcdm::KEY_MESSAGE)));

      EXPECT_CALL(*cdm, GenerateKeyRequest("", cdmKeySetId, mimeType, initData,
                                          kLicenseTypeRelease, cdmParameters,
                                          NotNull(), HasOrigin(EMPTY_ORIGIN),
                                          _))

          .WillOnce(DoAll(SetArgPointee<8>(releaseRequest),
                          testing::Return(wvcdm::KEY_MESSAGE)));
    }
  }

  // Performs the actual tests
  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);
  for (size_t i = 0; i < testSetCount; ++i)
  {
    const std::string mimeType(testSets[i].mimeType);
    const std::vector<uint8_t>& initData = testSets[i].initDataIn;

    plugin.getKeyRequest(
        toHidlVec(sessionId), toHidlVec(initData),
        hidl_string(mimeType), KeyType::OFFLINE, toHidlVec(optionalParameters),
        [&](Status status, hidl_vec<uint8_t> hRequest,
            KeyRequestType keyRequestType, hidl_string defaultUrl) {
      ASSERT_EQ(Status::OK, status);

      std::vector<uint8_t> request(hRequest);
      EXPECT_THAT(request, ElementsAreArray(requestRaw, kRequestSize));
      EXPECT_EQ(KeyRequestType::INITIAL, keyRequestType);
      EXPECT_STREQ(kDefaultUrl, defaultUrl.c_str());
    });

    plugin.getKeyRequest(
        toHidlVec(sessionId), toHidlVec(initData),
        hidl_string(mimeType), KeyType::STREAMING, toHidlVec(optionalParameters),
        [&](Status status, hidl_vec<uint8_t> hRequest,
            KeyRequestType keyRequestType, hidl_string defaultUrl) {
      ASSERT_EQ(Status::OK, status);

      std::vector<uint8_t> request(hRequest);
      EXPECT_THAT(request, ElementsAreArray(requestRaw, kRequestSize));
      EXPECT_EQ(KeyRequestType::RENEWAL, keyRequestType);
      EXPECT_STREQ(kDefaultUrl, defaultUrl.c_str());
    });

    plugin.getKeyRequest(
        toHidlVec(sessionId), toHidlVec(initData),
        hidl_string(mimeType), KeyType::RELEASE, toHidlVec(optionalParameters),
        [&](Status status, hidl_vec<uint8_t> hRequest,
            KeyRequestType keyRequestType, hidl_string defaultUrl) {
      ASSERT_EQ(Status::OK, status);

      std::vector<uint8_t> request(hRequest);
      EXPECT_THAT(request, ElementsAreArray(requestRaw, kRequestSize));
      EXPECT_EQ(KeyRequestType::RELEASE, keyRequestType);
      EXPECT_STREQ(kDefaultUrl, defaultUrl.c_str());
    });
  }
}

TEST_F(WVDrmPluginTest, AddsKeys) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  static const uint32_t kResponseSize = 256;
  uint8_t responseRaw[kResponseSize];
  static const uint32_t kKeySetIdSize = 32;
  uint8_t keySetIdRaw[kKeySetIdSize];
  FILE* fp = fopen("/dev/urandom", "r");
  fread(responseRaw, sizeof(uint8_t), kResponseSize, fp);
  fread(keySetIdRaw, sizeof(uint8_t), kKeySetIdSize, fp);
  fclose(fp);

  std::vector<uint8_t> response;
  response.assign(responseRaw, responseRaw + kResponseSize);

  memcpy(keySetIdRaw, KEY_SET_ID_PREFIX, sizeof(KEY_SET_ID_PREFIX) - 1);
  CdmKeySetId cdmKeySetId(reinterpret_cast<char *>(keySetIdRaw), kKeySetIdSize);

  std::vector<uint8_t> keySetId;
  std::vector<uint8_t> emptyKeySetId;

  EXPECT_CALL(*cdm, AddKey(cdmSessionId,
                          ElementsAreArray(responseRaw, kResponseSize), _))
      .WillOnce(DoAll(SetArgPointee<2>(cdmKeySetId),
                      testing::Return(wvcdm::KEY_ADDED)));

  EXPECT_CALL(*cdm, AddKey("", ElementsAreArray(responseRaw, kResponseSize),
                          Pointee(cdmKeySetId)))
      .Times(1);

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);

  plugin.provideKeyResponse(
      toHidlVec(sessionId), toHidlVec(response),
      [&](Status status, hidl_vec<uint8_t> hKeySetId) {
    ASSERT_EQ(Status::OK, status);

    std::vector<uint8_t> id(hKeySetId);
    keySetId.assign(id.data(), id.data() + id.size());
    ASSERT_THAT(keySetId, ElementsAreArray(keySetIdRaw, kKeySetIdSize));
  });

  plugin.provideKeyResponse(
      toHidlVec(keySetId), toHidlVec(response),
      [&](Status status, hidl_vec<uint8_t> hKeySetId) {
    ASSERT_EQ(Status::OK, status);

    EXPECT_EQ(0u, hKeySetId.size());
  });
}

TEST_F(WVDrmPluginTest, HandlesPrivacyCertCaseOfAddKey) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  sp<StrictMock<MockDrmPluginListener> > listener =
      new StrictMock<MockDrmPluginListener>();

  const CdmClientPropertySet* propertySet = nullptr;

  // Provide expected behavior in response to OpenSession and store the
  // property set
  EXPECT_CALL(*cdm, OpenSession(_, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<4>(cdmSessionId),
                            SaveArg<1>(&propertySet),
                            testing::Return(wvcdm::NO_ERROR)));

  // Provide expected behavior when plugin requests session control info
  EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId, _))
      .WillRepeatedly(Invoke(setSessionIdOnMap<4>));

  EXPECT_CALL(*cdm, CloseSession(_))
      .Times(AtLeast(0));

  static const uint32_t kResponseSize = 256;
  uint8_t responseRaw[kResponseSize];
  FILE* fp = fopen("/dev/urandom", "r");
  fread(responseRaw, sizeof(uint8_t), kResponseSize, fp);
  fclose(fp);

  hidl_vec<uint8_t> hSessionId;
  hSessionId.setToExternal(sessionIdRaw, kSessionIdSize);
  hidl_vec<uint8_t> hEmptyData;

  EXPECT_CALL(*listener,
              sendEvent(EventType::KEY_NEEDED, hSessionId, hEmptyData))
      .Times(1);

  EXPECT_CALL(*cdm, AddKey(_, _, _))
      .WillRepeatedly(testing::Return(wvcdm::NEED_KEY));

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);

  plugin.openSession([&](Status status, hidl_vec<uint8_t> hSessionId) {
    ASSERT_EQ(Status::OK, status);
    sessionId.clear();
    sessionId.assign(hSessionId.data(), hSessionId.data() + hSessionId.size());
  });
  ASSERT_THAT(propertySet, NotNull());

  plugin.setListener(listener);

  Status status = plugin.setPropertyString(hidl_string("privacyMode"),
                                           hidl_string("enable"));
  ASSERT_EQ(Status::OK, status);
  EXPECT_TRUE(propertySet->use_privacy_mode());

  std::vector<uint8_t> response;
  response.assign(responseRaw, responseRaw + kResponseSize);
  std::vector<uint8_t> keySetId;

  plugin.provideKeyResponse(
      toHidlVec(sessionId), toHidlVec(response),
      [&](Status status, hidl_vec<uint8_t> /* keySetId */) {
    ASSERT_EQ(Status::OK, status);
  });
}

TEST_F(WVDrmPluginTest, RemovesKeys) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  EXPECT_CALL(*cdm, RemoveKeys(cdmSessionId))
      .Times(1);

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);
  Status status = plugin.removeKeys(toHidlVec(sessionId));
  ASSERT_EQ(Status::OK, status);
}

TEST_F(WVDrmPluginTest, RestoresKeys) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  EXPECT_CALL(*cdm, RestoreKey(cdmSessionId,
                               ElementsAreArray(keySetIdRaw, kKeySetIdSize)))
      .Times(1);

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);
  Status status = plugin.restoreKeys(toHidlVec(sessionId), toHidlVec(keySetId));
  ASSERT_EQ(Status::OK, status);
}

TEST_F(WVDrmPluginTest, QueriesKeyStatus) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  std::map<std::string, std::string> expectedLicenseStatus;
  CdmQueryMap cdmLicenseStatus;

  expectedLicenseStatus.insert(
      std::pair<std::string, std::string>("areTheKeysAllRight", "yes"));
  cdmLicenseStatus["areTheKeysAllRight"] = "yes";
  expectedLicenseStatus.insert(
      std::pair<std::string, std::string>("isGMockAwesome", "ohhhhhhYeah"));
  cdmLicenseStatus["isGMockAwesome"] = "ohhhhhhYeah";
  expectedLicenseStatus.insert(
      std::pair<std::string, std::string>("answer", "42"));
  cdmLicenseStatus["answer"] = "42";

  EXPECT_CALL(*cdm, QueryKeyStatus(cdmSessionId, _))
      .WillOnce(DoAll(SetArgPointee<1>(cdmLicenseStatus),
                      testing::Return(wvcdm::NO_ERROR)));

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);
  plugin.queryKeyStatus(toHidlVec(sessionId),
                        [&](Status status, hidl_vec<KeyValue>(hLicenseStatus)) {
    ASSERT_EQ(Status::OK, status);
    ASSERT_EQ(expectedLicenseStatus.size(), hLicenseStatus.size());

    KeyValue keyValuePair;
    size_t i = 0;
    for (std::map<std::string, std::string>::iterator itr =
                expectedLicenseStatus.begin();
        itr != expectedLicenseStatus.end(); ++itr) {
      keyValuePair.value = hLicenseStatus[i++].value;
      EXPECT_EQ(itr->second.c_str(), std::string(keyValuePair.value.c_str()));
    }
  });
}

TEST_F(WVDrmPluginTest, GetsProvisioningRequests) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  static const uint32_t kRequestSize = 256;
  uint8_t requestRaw[kRequestSize];
  FILE* fp = fopen("/dev/urandom", "r");
  fread(requestRaw, sizeof(uint8_t), kRequestSize, fp);
  fclose(fp);

  CdmProvisioningRequest cdmRequest(requestRaw, requestRaw + kRequestSize);

  static const char* kDefaultUrl = "http://google.com/";

  EXPECT_CALL(*cdm, GetProvisioningRequest(kCertificateWidevine, IsEmpty(),
                                           HasOrigin(EMPTY_ORIGIN), IsEmpty(),
                                           _, _))
      .WillOnce(DoAll(SetArgPointee<4>(cdmRequest),
                      SetArgPointee<5>(kDefaultUrl),
                      testing::Return(wvcdm::NO_ERROR)));

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);
  plugin.getProvisionRequest(
      hidl_string(""), hidl_string(""),
      [&](Status status, hidl_vec<uint8_t> hRequest, hidl_string defaultUrl) {
    ASSERT_EQ(Status::OK, status);

    std::vector<uint8_t> request(hRequest);
    EXPECT_THAT(request, ElementsAreArray(requestRaw, kRequestSize));
    EXPECT_STREQ(kDefaultUrl, defaultUrl.c_str());
  });
}

TEST_F(WVDrmPluginTest, HandlesProvisioningResponses) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  static const uint32_t kResponseSize = 512;
  uint8_t responseRaw[kResponseSize];
  FILE* fp = fopen("/dev/urandom", "r");
  fread(responseRaw, sizeof(uint8_t), kResponseSize, fp);
  fclose(fp);

  std::vector<uint8_t> response;
  response.assign(responseRaw, responseRaw + kResponseSize);

  EXPECT_CALL(*cdm, HandleProvisioningResponse(HasOrigin(EMPTY_ORIGIN),
                                               ElementsAreArray(responseRaw,
                                                                kResponseSize),
                                               _, _))
      .Times(1);

  std::vector<uint8_t> cert;
  std::vector<uint8_t> key;

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);
  plugin.provideProvisionResponse(
      toHidlVec(response),
      [&](Status status, hidl_vec<uint8_t> /* cert */,
          hidl_vec<uint8_t> /* key */) {
    ASSERT_EQ(Status::OK, status);
  });
}

TEST_F(WVDrmPluginTest, UnprovisionsDevice) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  EXPECT_CALL(*cdm, Unprovision(kSecurityLevelL1, HasOrigin(EMPTY_ORIGIN)))
      .Times(1);
  EXPECT_CALL(*cdm, Unprovision(kSecurityLevelL3, HasOrigin(EMPTY_ORIGIN)))
      .Times(1);

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);
  Status res = plugin.unprovisionDevice();
  ASSERT_EQ(Status::OK, res);
}

TEST_F(WVDrmPluginTest, MuxesUnprovisioningErrors) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  // Tests that both Unprovisions are called even if one fails. Also tests that
  // no matter which fails, the function always propagates the error.
  EXPECT_CALL(*cdm, Unprovision(kSecurityLevelL1, HasOrigin(EMPTY_ORIGIN)))
      .WillOnce(testing::Return(wvcdm::UNKNOWN_ERROR))
      .WillOnce(testing::Return(wvcdm::NO_ERROR))
      .WillOnce(testing::Return(wvcdm::UNKNOWN_ERROR));
  EXPECT_CALL(*cdm, Unprovision(kSecurityLevelL3, HasOrigin(EMPTY_ORIGIN)))
      .WillOnce(testing::Return(wvcdm::NO_ERROR))
      .WillOnce(testing::Return(wvcdm::UNKNOWN_ERROR))
      .WillOnce(testing::Return(wvcdm::UNKNOWN_ERROR));

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);
  Status res = plugin.unprovisionDevice();
  ASSERT_NE(Status::OK, res);
  res = plugin.unprovisionDevice();
  ASSERT_NE(Status::OK, res);
  res = plugin.unprovisionDevice();
  ASSERT_NE(Status::OK, res);
}

TEST_F(WVDrmPluginTest, UnprovisionsOrigin) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  std::vector<uint8_t> specialResponse;
  specialResponse.assign(
      kUnprovisionResponse, kUnprovisionResponse + kUnprovisionResponseSize);

  EXPECT_CALL(*cdm, Unprovision(kSecurityLevelL1, HasOrigin(kOrigin.c_str())))
      .Times(1);
  EXPECT_CALL(*cdm, Unprovision(kSecurityLevelL3, HasOrigin(kOrigin.c_str())))
      .Times(1);

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);

  Status status = plugin.setPropertyString(hidl_string("origin"),
                                           hidl_string(kOrigin));
  ASSERT_EQ(Status::OK, status);

  plugin.provideProvisionResponse(
      toHidlVec(specialResponse),
      [&](Status status, hidl_vec<uint8_t> /* cert */,
          hidl_vec<uint8_t> /* key */) {
    EXPECT_EQ(Status::OK, status);
  });
}

TEST_F(WVDrmPluginTest, UnprovisionsGloballyWithSpoid) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  std::vector<uint8_t> specialResponse;
  specialResponse.assign(
      kUnprovisionResponse, kUnprovisionResponse + kUnprovisionResponseSize);

  EXPECT_CALL(*cdm, QueryStatus(_, QUERY_KEY_DEVICE_ID, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(kDeviceId),
                            testing::Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, Unprovision(kSecurityLevelL1, HasOrigin(EMPTY_ORIGIN)))
      .Times(1);
  EXPECT_CALL(*cdm, Unprovision(kSecurityLevelL3, HasOrigin(EMPTY_ORIGIN)))
      .Times(1);

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, true);
  plugin.provideProvisionResponse(
      toHidlVec(specialResponse),
      [&](Status status, hidl_vec<uint8_t> /* cert */,
          hidl_vec<uint8_t> /* key */) {
    EXPECT_EQ(Status::OK, status);
  });
}

TEST_F(WVDrmPluginTest, WillNotUnprovisionWithoutOriginOrSpoid) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  std::vector<uint8_t> specialResponse;
  specialResponse.assign(
      kUnprovisionResponse, kUnprovisionResponse + kUnprovisionResponseSize);

  EXPECT_CALL(*cdm, Unprovision(_, _))
      .Times(0);

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);
  plugin.provideProvisionResponse(
      toHidlVec(specialResponse),
      [&](Status status, hidl_vec<uint8_t> /* cert */,
          hidl_vec<uint8_t> /* key */) {
    EXPECT_NE(Status::OK, status);
  });
}

TEST_F(WVDrmPluginTest, MuxesOriginUnprovisioningErrors) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  std::vector<uint8_t> specialResponse;
  specialResponse.assign(
      kUnprovisionResponse, kUnprovisionResponse + kUnprovisionResponseSize);

  // Tests that both Unprovisions are called even if one fails. Also tests that
  // no matter which fails, the function always propagates the error.
  EXPECT_CALL(*cdm, Unprovision(kSecurityLevelL1, HasOrigin(kOrigin.c_str())))
      .WillOnce(testing::Return(wvcdm::UNKNOWN_ERROR))
      .WillOnce(testing::Return(wvcdm::NO_ERROR))
      .WillOnce(testing::Return(wvcdm::UNKNOWN_ERROR));
  EXPECT_CALL(*cdm, Unprovision(kSecurityLevelL3, HasOrigin(kOrigin.c_str())))
      .WillOnce(testing::Return(wvcdm::NO_ERROR))
      .WillOnce(testing::Return(wvcdm::UNKNOWN_ERROR))
      .WillOnce(testing::Return(wvcdm::UNKNOWN_ERROR));

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);

  Status status = plugin.setPropertyString(hidl_string("origin"),
                                           hidl_string(kOrigin));
  ASSERT_EQ(Status::OK, status);

  plugin.provideProvisionResponse(
      toHidlVec(specialResponse),
      [&](Status status, hidl_vec<uint8_t> /* cert */,
          hidl_vec<uint8_t> /* key */) {
    EXPECT_NE(Status::OK, status);
  });

  plugin.provideProvisionResponse(
      toHidlVec(specialResponse),
      [&](Status status, hidl_vec<uint8_t> /* cert */,
          hidl_vec<uint8_t> /* key */) {
    EXPECT_NE(Status::OK, status);
  });

  plugin.provideProvisionResponse(
      toHidlVec(specialResponse),
      [&](Status status, hidl_vec<uint8_t> /* cert */,
          hidl_vec<uint8_t> /* key */) {
    EXPECT_NE(Status::OK, status);
  });
}

TEST_F(WVDrmPluginTest, GetsSecureStops) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  static const uint32_t kStopSize = 53;
  static const uint32_t kStopCount = 7;
  uint8_t stopsRaw[kStopCount][kStopSize];
  FILE* fp = fopen("/dev/urandom", "r");
  for (uint32_t i = 0; i < kStopCount; ++i) {
    fread(stopsRaw[i], sizeof(uint8_t), kStopSize, fp);
  }
  fclose(fp);

  CdmUsageInfo cdmStops;
  for (uint32_t i = 0; i < kStopCount; ++i) {
    cdmStops.push_back(std::string(stopsRaw[i], stopsRaw[i] + kStopSize));
  }

  const char* app_id = "my_app_id";
  EXPECT_CALL(*cdm, GetUsageInfo(StrEq(app_id), _, _))
      .WillOnce(DoAll(SetArgPointee<2>(cdmStops),
                      testing::Return(wvcdm::NO_ERROR)));

  std::vector<std::vector<uint8_t> > stops;

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);
  Status status = plugin.setPropertyString(hidl_string("appId"),
                                           hidl_string(app_id));
  ASSERT_EQ(Status::OK, status);

  plugin.getSecureStops([&](Status status, hidl_vec<SecureStop> hSecureStops) {
    ASSERT_EQ(Status::OK, status);

    std::vector<SecureStop> secureStops(hSecureStops);
    std::vector<SecureStop>::iterator iter = secureStops.begin();
    std::vector<uint8_t> stop;
    while (iter != secureStops.end()) {
      stop = (*iter++).opaqueData;
      stops.push_back(stop);
    }
  });

  size_t index = 0;
  for (auto stop : stops) {
    EXPECT_THAT(stop, ElementsAreArray(stopsRaw[index++], kStopSize));
  }
  EXPECT_EQ(kStopCount, index);
}

TEST_F(WVDrmPluginTest, ReleasesAllSecureStops) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  EXPECT_CALL(*cdm, RemoveAllUsageInfo(StrEq(""), _))
      .Times(1);

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);

  Status status = plugin.setPropertyString(hidl_string("appId"),
                                           hidl_string(""));
  ASSERT_EQ(Status::OK, status);

  status = plugin.releaseAllSecureStops();
  ASSERT_EQ(Status::OK, status);
}

TEST_F(WVDrmPluginTest, ReleasesSecureStop) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  static const uint32_t kMessageSize = 128;
  uint8_t messageRaw[kMessageSize];
  FILE* fp = fopen("/dev/urandom", "r");
  fread(messageRaw, sizeof(uint8_t), kMessageSize, fp);
  fclose(fp);

  std::vector<uint8_t> message;
  message.assign(messageRaw, messageRaw + kMessageSize);

  EXPECT_CALL(*cdm, ReleaseUsageInfo(ElementsAreArray(messageRaw,
                                                      kMessageSize),
                                     _))
      .Times(1);

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);
  Status status = plugin.releaseSecureStop(toHidlVec(message));
  ASSERT_EQ(Status::OK, status);
}

TEST_F(WVDrmPluginTest, ReturnsExpectedPropertyValues) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  CdmQueryMap l1Map;
  l1Map[QUERY_KEY_SECURITY_LEVEL] = QUERY_VALUE_SECURITY_LEVEL_L1;

  CdmQueryMap l3Map;
  l3Map[QUERY_KEY_SECURITY_LEVEL] = QUERY_VALUE_SECURITY_LEVEL_L3;

  static const std::string systemId = "The Universe";
  static const std::string provisioningId("Life\0&Everything", 16);
  static const std::string openSessions = "42";
  static const std::string maxSessions = "54";
  static const std::string oemCryptoApiVersion = "13";
  static const std::string currentSRMVersion = "1";
  static const std::string cdmVersion = "Infinity Minus 1";
  static const std::string resourceRatingTier = "1";
  static const std::string oemCryptoBuildInformation = "Mostly Harmless";
  static const std::string oemCryptoHashNotSupported = "0";
  static const std::string oemCryptoCrcClearBuffer = "1";
  static const std::string oemCryptoPartnerDefinedHash = "2";
  static const std::string decryptHashErrorBadHashAndFrameNumber = "53, 1";
  drm_metrics::WvCdmMetrics expected_metrics;
  std::string serialized_metrics = wvcdm::a2bs_hex(kSerializedMetricsHex);
  ASSERT_TRUE(expected_metrics.ParseFromString(serialized_metrics));

  EXPECT_CALL(*cdm, QueryStatus(_, QUERY_KEY_SECURITY_LEVEL, _))
      .WillOnce(DoAll(SetArgPointee<2>(QUERY_VALUE_SECURITY_LEVEL_L1),
                      testing::Return(wvcdm::NO_ERROR)))
      .WillOnce(DoAll(SetArgPointee<2>(QUERY_VALUE_SECURITY_LEVEL_L3),
                      testing::Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, QueryStatus(_, QUERY_KEY_DEVICE_ID, _))
      .WillOnce(DoAll(SetArgPointee<2>(kDeviceId),
                      testing::Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, QueryStatus(_, QUERY_KEY_SYSTEM_ID, _))
      .WillOnce(DoAll(SetArgPointee<2>(systemId),
                      testing::Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, QueryStatus(_, QUERY_KEY_PROVISIONING_ID, _))
      .WillOnce(DoAll(SetArgPointee<2>(provisioningId),
                      testing::Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, QueryStatus(_, QUERY_KEY_NUMBER_OF_OPEN_SESSIONS, _))
      .WillOnce(DoAll(SetArgPointee<2>(openSessions),
                      testing::Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, QueryStatus(_, QUERY_KEY_MAX_NUMBER_OF_SESSIONS, _))
      .WillOnce(DoAll(SetArgPointee<2>(maxSessions),
                      testing::Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, QueryStatus(_, QUERY_KEY_OEMCRYPTO_API_VERSION, _))
      .WillOnce(DoAll(SetArgPointee<2>(oemCryptoApiVersion),
                      testing::Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, QueryStatus(_, QUERY_KEY_SRM_UPDATE_SUPPORT, _))
      .WillOnce(DoAll(SetArgPointee<2>("True"),
                      testing::Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, QueryStatus(_, QUERY_KEY_CURRENT_SRM_VERSION, _))
      .WillOnce(DoAll(SetArgPointee<2>(currentSRMVersion),
                      testing::Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, QueryStatus(_, QUERY_KEY_WVCDM_VERSION, _))
      .WillOnce(DoAll(SetArgPointee<2>(cdmVersion),
                      testing::Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, QueryStatus(_, QUERY_KEY_RESOURCE_RATING_TIER, _))
      .WillOnce(DoAll(SetArgPointee<2>(resourceRatingTier),
                      testing::Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, QueryStatus(_, QUERY_KEY_DECRYPT_HASH_SUPPORT, _))
      .WillOnce(DoAll(SetArgPointee<2>("1"),
                      testing::Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, QueryStatus(_, QUERY_KEY_OEMCRYPTO_BUILD_INFORMATION, _))
      .WillOnce(DoAll(SetArgPointee<2>(oemCryptoBuildInformation),
                      testing::Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, GetDecryptHashError(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(decryptHashErrorBadHashAndFrameNumber),
                      testing::Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, GetMetrics(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(expected_metrics),
                      testing::Return(wvcdm::NO_ERROR)));

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);
  std::string stringResult;
  std::vector<uint8_t> vectorResult;

  plugin.getPropertyString(
      hidl_string("vendor"), [&](Status status, hidl_string stringResult) {
    ASSERT_EQ(Status::OK, status);
    EXPECT_STREQ("Google", stringResult.c_str());
  });

  plugin.getPropertyString(
      hidl_string("version"), [&](Status status, hidl_string stringResult) {
    ASSERT_EQ(Status::OK, status);
    EXPECT_STREQ(cdmVersion.c_str(), stringResult.c_str());
  });

  plugin.getPropertyString(
      hidl_string("description"), [&](Status status, hidl_string stringResult) {
    ASSERT_EQ(Status::OK, status);
    EXPECT_STREQ("Widevine CDM", stringResult.c_str());
  });

  plugin.getPropertyString(
      hidl_string("algorithms"), [&](Status status, hidl_string stringResult) {
    ASSERT_EQ(Status::OK, status);
    EXPECT_STREQ("AES/CBC/NoPadding,HmacSHA256", stringResult.c_str());
  });

  plugin.getPropertyString(
      hidl_string("securityLevel"),
      [&](Status status, hidl_string stringResult) {
    ASSERT_EQ(Status::OK, status);
    EXPECT_STREQ(QUERY_VALUE_SECURITY_LEVEL_L1.c_str(), stringResult.c_str());
  });

  plugin.getPropertyString(
      hidl_string("securityLevel"),
      [&](Status status, hidl_string stringResult) {
    ASSERT_EQ(Status::OK, status);
    EXPECT_STREQ(QUERY_VALUE_SECURITY_LEVEL_L3.c_str(), stringResult.c_str());
  });

  plugin.getPropertyByteArray(
      hidl_string("deviceUniqueId"),
      [&](Status status, hidl_vec<uint8_t> vectorResult) {
    ASSERT_EQ(Status::OK, status);
    std::vector<uint8_t> id(vectorResult);
    EXPECT_THAT(id, ElementsAreArray(kDeviceId.data(), kDeviceId.size()));
  });

  plugin.getPropertyString(
      hidl_string("systemId"),
      [&](Status status, hidl_string stringResult) {
    ASSERT_EQ(Status::OK, status);
    EXPECT_STREQ(systemId.c_str(), stringResult.c_str());
  });

  plugin.getPropertyByteArray(
      hidl_string("provisioningUniqueId"),
      [&](Status status, hidl_vec<uint8_t> vectorResult) {
    ASSERT_EQ(Status::OK, status);
    std::vector<uint8_t> id(vectorResult);
    EXPECT_THAT(id, ElementsAreArray(provisioningId.data(),
                provisioningId.size()));
  });

  plugin.getPropertyString(
      hidl_string("numberOfOpenSessions"),
      [&](Status status, hidl_string stringResult) {
    ASSERT_EQ(Status::OK, status);
    EXPECT_EQ(openSessions, stringResult.c_str());
  });

  plugin.getPropertyString(
      hidl_string("maxNumberOfSessions"),
      [&](Status status, hidl_string stringResult) {
    ASSERT_EQ(Status::OK, status);
    EXPECT_EQ(maxSessions, stringResult.c_str());
  });

  plugin.getPropertyString(
      hidl_string("oemCryptoApiVersion"),
      [&](Status status, hidl_string stringResult) {
    ASSERT_EQ(Status::OK, status);
    EXPECT_STREQ(oemCryptoApiVersion.c_str(), stringResult.c_str());
  });

  plugin.getPropertyString(
        hidl_string("SRMUpdateSupport"),
                    [&](Status status, hidl_string stringResult) {
    ASSERT_EQ(Status::OK, status);
    EXPECT_STREQ("True", stringResult.c_str());
  });

  plugin.getPropertyString(
        hidl_string("CurrentSRMVersion"),
                    [&](Status status, hidl_string stringResult) {
    ASSERT_EQ(Status::OK, status);
    EXPECT_STREQ(currentSRMVersion.c_str(), stringResult.c_str());
  });

  plugin.getPropertyString(
        hidl_string("resourceRatingTier"),
                    [&](Status status, hidl_string stringResult) {
    ASSERT_EQ(Status::OK, status);
    EXPECT_STREQ(resourceRatingTier.c_str(), stringResult.c_str());
  });

  plugin.getPropertyString(
        hidl_string("oemCryptoBuildInformation"),
                    [&](Status status, hidl_string stringResult) {
    ASSERT_EQ(Status::OK, status);
    EXPECT_STREQ(oemCryptoBuildInformation.c_str(), stringResult.c_str());
  });

  std::stringstream ss;
  ss << oemCryptoHashNotSupported << " " << oemCryptoCrcClearBuffer << " "
          << oemCryptoPartnerDefinedHash;
  std::string validResults = ss.str();
  plugin.getPropertyString(
        hidl_string("decryptHashSupport"),
                    [&](Status status, hidl_string stringResult) {
    ASSERT_EQ(Status::OK, status);
    EXPECT_EQ(oemCryptoCrcClearBuffer, stringResult);
  });

  plugin.getPropertyString(
        hidl_string("decryptHashError"),
                    [&](Status status, hidl_string stringResult) {
    ASSERT_EQ(Status::OK, status);
    EXPECT_EQ(decryptHashErrorBadHashAndFrameNumber, stringResult);
  });

  // This call occurs before any open session or other call. This means
  // that the cdm identifer is not yet sealed, and metrics return empty
  // metrics data.
  plugin.getPropertyByteArray(
      hidl_string("metrics"),
      [&](Status status, hidl_vec<uint8_t> vectorResult) {
    ASSERT_EQ(Status::OK, status);
    std::vector<uint8_t> id(vectorResult);
    const char empty[] = {};
    EXPECT_THAT(id, ElementsAreArray(empty, sizeof(empty)));
  });

  // Set expectations for the OpenSession call and a CloseSession call.
  EXPECT_CALL(*cdm,
      OpenSession(StrEq("com.widevine"), _, HasOrigin(EMPTY_ORIGIN), _, _))
          .WillOnce(DoAll(SetArgPointee<4>(cdmSessionId),
                          testing::Return(wvcdm::NO_ERROR)));
  // Provide expected behavior when plugin requests session control info
  EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Invoke(setSessionIdOnMap<4>));
  EXPECT_CALL(*cdm, CloseSession(_))
      .Times(AtLeast(0));

  // This call causes the cdm identifier to become sealed.
  std::vector<uint8_t> sessionId;
  plugin.openSession([&](Status status, hidl_vec<uint8_t> hSessionId) {
      ASSERT_EQ(Status::OK, status);
      sessionId.clear();
      sessionId.assign(hSessionId.data(),
                       hSessionId.data() + hSessionId.size());
  });

  // This call occurs after open session. The CDM identifer should be sealed.
  // And the call should populate the mock metrics data.
  plugin.getPropertyByteArray(
      hidl_string("metrics"),
      [&](Status status, hidl_vec<uint8_t> vectorResult) {
    ASSERT_EQ(Status::OK, status);
    std::vector<uint8_t> id(vectorResult);
    EXPECT_THAT(id, ElementsAreArray(serialized_metrics.data(),
                                     serialized_metrics.size()));
  });

  ASSERT_EQ(Status::OK, plugin.closeSession(toHidlVec(sessionId)));
}

TEST_F(WVDrmPluginTest, DoesNotGetUnknownProperties) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);
  std::string stringResult;
  std::vector<uint8_t> vectorResult;

  plugin.getPropertyString(
      hidl_string("unknownProperty"),
      [&](Status status, hidl_string stringResult) {
    ASSERT_NE(Status::OK, status);
    EXPECT_TRUE(stringResult.empty());
  });

  plugin.getPropertyByteArray(
      hidl_string("unknownProperty"),
      [&](Status status, hidl_vec<uint8_t> vectorResult) {
    ASSERT_NE(Status::OK, status);
    EXPECT_EQ(0u, vectorResult.size());
  });
}

TEST_F(WVDrmPluginTest, DoesNotSetUnknownProperties) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  static const uint32_t kValueSize = 32;
  uint8_t valueRaw[kValueSize];
  FILE* fp = fopen("/dev/urandom", "r");
  fread(valueRaw, sizeof(uint8_t), kValueSize, fp);
  fclose(fp);

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);
  std::vector<uint8_t> value;
  value.assign(valueRaw, valueRaw + kValueSize);

  Status status = plugin.setPropertyString(hidl_string("unknownProperty"),
                                           hidl_string("ignored"));
  ASSERT_NE(Status::OK, status);

  status = plugin.setPropertyByteArray(hidl_string("unknownProperty"),
                                       toHidlVec(value));
  ASSERT_NE(Status::OK, status);
}

TEST_F(WVDrmPluginTest, CompliesWithSpoidVariability) {
  StrictMock<MockCrypto> crypto;

  const std::string kDeviceIds[] = {
    kDeviceId,
    kDeviceId + " the Second",
  };
  const size_t kDeviceCount = N_ELEM(kDeviceIds);

  const std::string kAppNames[] = {
    std::string("com.google.widevine"),
    std::string("com.youtube"),
  };
  const size_t kAppCount = N_ELEM(kAppNames);

  const std::string kOrigins[] = {
    kOrigin,
    kOrigin + " but not that one, the other one.",
    std::string(/* Intentionally Empty */),
  };
  const size_t kOriginCount = N_ELEM(kOrigins);

  const size_t kPluginCount = 2;

  const size_t kPluginsPerCdm = kAppCount * kOriginCount * kPluginCount;

  // We will get kPluginCount SPOIDs for every app package name + device id +
  // origin combination.
  std::vector<uint8_t>
      spoids[kDeviceCount][kAppCount][kOriginCount][kPluginCount];

  for (size_t deviceIndex = 0; deviceIndex < kDeviceCount; ++deviceIndex) {
    const std::string& deviceId = kDeviceIds[deviceIndex];

    android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();

    EXPECT_CALL(*cdm, QueryStatus(_, QUERY_KEY_DEVICE_ID, _))
        .Times(AtLeast(kPluginsPerCdm))
        .WillRepeatedly(DoAll(SetArgPointee<2>(deviceId),
                              testing::Return(wvcdm::NO_ERROR)));

    for (size_t appIndex = 0; appIndex < kAppCount; ++appIndex) {
      const std::string& appPackageName = kAppNames[appIndex];

      for (size_t originIndex = 0; originIndex < kOriginCount; ++originIndex) {
        const std::string& origin = kOrigins[originIndex];

        for (size_t pluginIndex = 0;
             pluginIndex < kPluginCount;
             ++pluginIndex) {
          WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, true);

          if (!origin.empty()) {
            ASSERT_EQ(Status::OK,
                      plugin.setPropertyString(hidl_string("origin"),
                                               hidl_string(origin)));
          }

          plugin.getPropertyByteArray(
              hidl_string("deviceUniqueId"),
              [&](Status status, hidl_vec<uint8_t> vectorResult) {
            ASSERT_EQ(Status::OK, status);
            spoids[deviceIndex][appIndex][originIndex][pluginIndex] =
                vectorResult;
          });
        }
      }
    }
  }

  // This nest of loops makes sure all the SPOIDs we retrieved above are
  // identical if their parameters were identical and dissimilar otherwise.
  for (size_t deviceIndex = 0; deviceIndex < kDeviceCount; ++deviceIndex) {
    for (size_t appIndex = 0; appIndex < kAppCount; ++appIndex) {
      for (size_t originIndex = 0; originIndex < kOriginCount; ++originIndex) {
        for (size_t pluginIndex = 0;
             pluginIndex < kPluginCount;
             ++pluginIndex) {
          const std::vector<uint8_t>& firstSpoid =
              spoids[deviceIndex][appIndex][originIndex][pluginIndex];

          for (size_t deviceIndex2 = 0;
               deviceIndex2 < kDeviceCount;
               ++deviceIndex2) {
            for (size_t appIndex2 = 0; appIndex2 < kAppCount; ++appIndex2) {
              for (size_t originIndex2 = 0;
                   originIndex2 < kOriginCount;
                   ++originIndex2) {
                for (size_t pluginIndex2 = 0;
                     pluginIndex2 < kPluginCount;
                     ++pluginIndex2) {
                  const std::vector<uint8_t>& secondSpoid =
                      spoids[deviceIndex2][appIndex2][originIndex2][pluginIndex2];

                  if (deviceIndex == deviceIndex2 &&
                      appIndex == appIndex2 &&
                      originIndex == originIndex2) {
                    EXPECT_EQ(firstSpoid, secondSpoid);
                  } else {
                    EXPECT_NE(firstSpoid, secondSpoid);
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

TEST_F(WVDrmPluginTest, FailsGenericMethodsWithoutAnAlgorithmSet) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  std::vector<uint8_t> keyId;
  std::vector<uint8_t> input;
  std::vector<uint8_t> iv;
  std::vector<uint8_t> output;

  // Provide expected behavior to support session creation
  EXPECT_CALL(*cdm, OpenSession(StrEq("com.widevine"), _, _, _, _))
      .Times(AtLeast(1))
      .WillRepeatedly(DoAll(SetArgPointee<4>(cdmSessionId),
                            testing::Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Invoke(setSessionIdOnMap<4>));

  EXPECT_CALL(*cdm, CloseSession(_))
      .Times(AtLeast(0));

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);

  plugin.openSession([&](Status status, hidl_vec<uint8_t> hSessionId) {
    ASSERT_EQ(Status::OK, status);
    sessionId.clear();
    sessionId.assign(hSessionId.data(), hSessionId.data() + hSessionId.size());
  });

  // Note that we do not set the algorithms.  This should cause these methods
  // to fail.

  plugin.encrypt(toHidlVec(sessionId), toHidlVec(keyId), toHidlVec(input),
                 toHidlVec(iv), [&](Status status, hidl_vec<uint8_t> /* output */) {
    // NO_INIT is converted to Status::ERROR_DRM_UNKNOWN
    EXPECT_EQ(Status::ERROR_DRM_UNKNOWN, status);
  });

  plugin.decrypt(toHidlVec(sessionId), toHidlVec(keyId), toHidlVec(input),
                 toHidlVec(iv), [&](Status status, hidl_vec<uint8_t> /* output */) {
    EXPECT_EQ(Status::ERROR_DRM_UNKNOWN, status);
  });

  plugin.sign(toHidlVec(sessionId), toHidlVec(keyId), toHidlVec(input),
              [&](Status status, hidl_vec<uint8_t> /* signature */) {
    EXPECT_EQ(Status::ERROR_DRM_UNKNOWN, status);
  });

  plugin.verify(toHidlVec(sessionId), toHidlVec(keyId), toHidlVec(input),
                toHidlVec(output), [&](Status status, bool /* match */) {
    EXPECT_EQ(Status::ERROR_DRM_UNKNOWN, status);
  });
}

MATCHER_P(IsIV, iv, "") {
  for (size_t i = 0; i < KEY_IV_SIZE; ++i) {
    if (iv[i] != arg[i]) {
      return false;
    }
  }

  return true;
}

TEST_F(WVDrmPluginTest, CallsGenericEncrypt) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  static const size_t kDataSize = 256;
  uint8_t keyIdRaw[KEY_ID_SIZE];
  uint8_t inputRaw[kDataSize];
  uint8_t ivRaw[KEY_IV_SIZE];

  FILE* fp = fopen("/dev/urandom", "r");
  fread(keyIdRaw, sizeof(uint8_t), KEY_ID_SIZE, fp);
  fread(inputRaw, sizeof(uint8_t), kDataSize, fp);
  fread(ivRaw, sizeof(uint8_t), KEY_IV_SIZE, fp);
  fclose(fp);

  std::vector<uint8_t> keyId;
  keyId.assign(keyIdRaw, keyIdRaw + KEY_ID_SIZE);
  std::vector<uint8_t> input;
  input.assign(inputRaw, inputRaw + kDataSize);
  std::vector<uint8_t> iv;
  iv.assign(ivRaw, ivRaw + KEY_IV_SIZE);
  std::vector<uint8_t> output;

  {
    InSequence calls;

    EXPECT_CALL(crypto, selectKey(4, _, KEY_ID_SIZE))
        .With(Args<1, 2>(ElementsAreArray(keyIdRaw, KEY_ID_SIZE)))
        .Times(1);

    EXPECT_CALL(crypto, encrypt(4, _, kDataSize, IsIV(ivRaw),
                                OEMCrypto_AES_CBC_128_NO_PADDING, _))
        .With(Args<1, 2>(ElementsAreArray(inputRaw, kDataSize)))
        .Times(1);
  }

  // Provide expected behavior to support session creation
  EXPECT_CALL(*cdm, OpenSession(StrEq("com.widevine"), _, _, _, _))
      .Times(AtLeast(1))
      .WillRepeatedly(DoAll(SetArgPointee<4>(cdmSessionId),
                            testing::Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Invoke(setSessionIdOnMap<4>));

  EXPECT_CALL(*cdm, CloseSession(_))
      .Times(AtLeast(0));

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);

  plugin.openSession([&](Status status, hidl_vec<uint8_t> hSessionId) {
    ASSERT_EQ(Status::OK, status);
    sessionId.clear();
    sessionId.assign(hSessionId.data(), hSessionId.data() + hSessionId.size());
  });

  Status status = plugin.setCipherAlgorithm(toHidlVec(sessionId),
                                            hidl_string("AES/CBC/NoPadding"));
  ASSERT_EQ(Status::OK, status);

  plugin.encrypt(toHidlVec(sessionId), toHidlVec(keyId), toHidlVec(input),
                 toHidlVec(iv), [&](Status status, hidl_vec<uint8_t> /* output */) {
    ASSERT_EQ(Status::OK, status);
  });
}

TEST_F(WVDrmPluginTest, CallsGenericDecrypt) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  static const size_t kDataSize = 256;
  uint8_t keyIdRaw[KEY_ID_SIZE];
  uint8_t inputRaw[kDataSize];
  uint8_t ivRaw[KEY_IV_SIZE];

  FILE* fp = fopen("/dev/urandom", "r");
  fread(keyIdRaw, sizeof(uint8_t), KEY_ID_SIZE, fp);
  fread(inputRaw, sizeof(uint8_t), kDataSize, fp);
  fread(ivRaw, sizeof(uint8_t), KEY_IV_SIZE, fp);
  fclose(fp);

  std::vector<uint8_t> keyId;
  keyId.assign(keyIdRaw, keyIdRaw + KEY_ID_SIZE);
  std::vector<uint8_t> input;
  input.assign(inputRaw, inputRaw + kDataSize);
  std::vector<uint8_t> iv;
  iv.assign(ivRaw, ivRaw + KEY_IV_SIZE);
  std::vector<uint8_t> output;

  {
    InSequence calls;

    EXPECT_CALL(crypto, selectKey(4, _, KEY_ID_SIZE))
        .With(Args<1, 2>(ElementsAreArray(keyIdRaw, KEY_ID_SIZE)))
        .Times(1);

    EXPECT_CALL(crypto, decrypt(4, _, kDataSize, IsIV(ivRaw),
                                OEMCrypto_AES_CBC_128_NO_PADDING, _))
        .With(Args<1, 2>(ElementsAreArray(inputRaw, kDataSize)))
        .Times(1);
  }

  // Provide expected behavior to support session creation
  EXPECT_CALL(*cdm, OpenSession(StrEq("com.widevine"), _, _, _, _))
      .Times(AtLeast(1))
      .WillRepeatedly(DoAll(SetArgPointee<4>(cdmSessionId),
                            testing::Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Invoke(setSessionIdOnMap<4>));

  EXPECT_CALL(*cdm, CloseSession(_))
      .Times(AtLeast(0));

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);

  plugin.openSession([&](Status status, hidl_vec<uint8_t> hSessionId) {
    ASSERT_EQ(Status::OK, status);
    sessionId.clear();
    sessionId.assign(hSessionId.data(), hSessionId.data() + hSessionId.size());
  });

  Status status = plugin.setCipherAlgorithm(toHidlVec(sessionId),
                                            hidl_string("AES/CBC/NoPadding"));
  ASSERT_EQ(Status::OK, status);

  plugin.decrypt(toHidlVec(sessionId), toHidlVec(keyId), toHidlVec(input),
                 toHidlVec(iv), [&](Status status, hidl_vec<uint8_t> /* output */) {
    ASSERT_EQ(Status::OK, status);
  });
}

TEST_F(WVDrmPluginTest, CallsGenericSign) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  static const size_t kDataSize = 256;
  uint8_t keyIdRaw[KEY_ID_SIZE];
  uint8_t messageRaw[kDataSize];

  FILE* fp = fopen("/dev/urandom", "r");
  fread(keyIdRaw, sizeof(uint8_t), KEY_ID_SIZE, fp);
  fread(messageRaw, sizeof(uint8_t), kDataSize, fp);
  fclose(fp);

  std::vector<uint8_t> keyId;
  keyId.assign(keyIdRaw, keyIdRaw + KEY_ID_SIZE);
  std::vector<uint8_t> message;
  message.assign(messageRaw, messageRaw + kDataSize);
  std::vector<uint8_t> signature;

  {
    InSequence calls;

    EXPECT_CALL(crypto, selectKey(4, _, KEY_ID_SIZE))
        .With(Args<1, 2>(ElementsAreArray(keyIdRaw, KEY_ID_SIZE)))
        .Times(1);

    EXPECT_CALL(crypto, sign(4, _, kDataSize, OEMCrypto_HMAC_SHA256, _,
                             Pointee(0)))
        .With(Args<1, 2>(ElementsAreArray(messageRaw, kDataSize)))
        .WillOnce(DoAll(SetArgPointee<5>(64),
                        testing::Return(OEMCrypto_ERROR_SHORT_BUFFER)));

    EXPECT_CALL(crypto, sign(4, _, kDataSize, OEMCrypto_HMAC_SHA256, _,
                             Pointee(64)))
        .With(Args<1, 2>(ElementsAreArray(messageRaw, kDataSize)))
        .Times(1);
  }

  // Provide expected behavior to support session creation
  EXPECT_CALL(*cdm, OpenSession(StrEq("com.widevine"), _, _, _, _))
      .Times(AtLeast(1))
      .WillRepeatedly(DoAll(SetArgPointee<4>(cdmSessionId),
                            testing::Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Invoke(setSessionIdOnMap<4>));

  EXPECT_CALL(*cdm, CloseSession(_))
      .Times(AtLeast(0));

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);

  plugin.openSession([&](Status status, hidl_vec<uint8_t> hSessionId) {
    ASSERT_EQ(Status::OK, status);
    sessionId.clear();
    sessionId.assign(hSessionId.data(), hSessionId.data() + hSessionId.size());
  });

  Status status = plugin.setMacAlgorithm(toHidlVec(sessionId),
                                         hidl_string("HmacSHA256"));
  ASSERT_EQ(Status::OK, status);

  plugin.sign(toHidlVec(sessionId), toHidlVec(keyId), toHidlVec(message),
              [&](Status status, hidl_vec<uint8_t> signature) {
    ASSERT_EQ(Status::OK, status);
    ASSERT_NE(0u, signature.size());
  });
}

TEST_F(WVDrmPluginTest, CallsGenericVerify) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  static const size_t kDataSize = 256;
  static const size_t kSignatureSize = 16;
  uint8_t keyIdRaw[KEY_ID_SIZE];
  uint8_t messageRaw[kDataSize];
  uint8_t signatureRaw[kSignatureSize];

  FILE* fp = fopen("/dev/urandom", "r");
  fread(keyIdRaw, sizeof(uint8_t), KEY_ID_SIZE, fp);
  fread(messageRaw, sizeof(uint8_t), kDataSize, fp);
  fread(signatureRaw, sizeof(uint8_t), kSignatureSize, fp);
  fclose(fp);

  std::vector<uint8_t> keyId;
  keyId.assign(keyIdRaw, keyIdRaw + KEY_ID_SIZE);
  std::vector<uint8_t> message;
  message.assign(messageRaw, messageRaw + kDataSize);
  std::vector<uint8_t> signature;
  signature.assign(signatureRaw, signatureRaw + kSignatureSize);

  {
    InSequence calls;

    EXPECT_CALL(crypto, selectKey(4, _, KEY_ID_SIZE))
        .With(Args<1, 2>(ElementsAreArray(keyIdRaw, KEY_ID_SIZE)))
        .Times(1);

    EXPECT_CALL(crypto, verify(4, _, kDataSize, OEMCrypto_HMAC_SHA256, _,
                               kSignatureSize))
        .With(AllOf(Args<1, 2>(ElementsAreArray(messageRaw, kDataSize)),
                    Args<4, 5>(ElementsAreArray(signatureRaw, kSignatureSize))))
        .WillOnce(testing::Return(OEMCrypto_SUCCESS));

    EXPECT_CALL(crypto, selectKey(4, _, KEY_ID_SIZE))
        .With(Args<1, 2>(ElementsAreArray(keyIdRaw, KEY_ID_SIZE)))
        .Times(1);

    EXPECT_CALL(crypto, verify(4, _, kDataSize, OEMCrypto_HMAC_SHA256, _,
                               kSignatureSize))
        .With(AllOf(Args<1, 2>(ElementsAreArray(messageRaw, kDataSize)),
                    Args<4, 5>(ElementsAreArray(signatureRaw, kSignatureSize))))
        .WillOnce(testing::Return(OEMCrypto_ERROR_SIGNATURE_FAILURE));
  }

  // Provide expected behavior to support session creation
  EXPECT_CALL(*cdm, OpenSession(StrEq("com.widevine"), _, _, _, _))
      .Times(AtLeast(1))
      .WillRepeatedly(DoAll(SetArgPointee<4>(cdmSessionId),
                            testing::Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Invoke(setSessionIdOnMap<4>));

  EXPECT_CALL(*cdm, CloseSession(_))
      .Times(AtLeast(0));

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);

  plugin.openSession([&](Status status, hidl_vec<uint8_t> hSessionId) {
    ASSERT_EQ(Status::OK, status);
    sessionId.clear();
    sessionId.assign(hSessionId.data(), hSessionId.data() + hSessionId.size());
  });

  Status status = plugin.setMacAlgorithm(toHidlVec(sessionId),
                                         hidl_string("HmacSHA256"));
  ASSERT_EQ(Status::OK, status);

  plugin.verify(toHidlVec(sessionId), toHidlVec(keyId), toHidlVec(message),
                toHidlVec(signature), [&](Status status, bool match) {
    ASSERT_EQ(Status::OK, status);
    EXPECT_TRUE(match);
  });

  plugin.verify(toHidlVec(sessionId), toHidlVec(keyId), toHidlVec(message),
                toHidlVec(signature), [&](Status status, bool match) {
    ASSERT_EQ(Status::OK, status);
    EXPECT_FALSE(match);
  });
}

TEST_F(WVDrmPluginTest, RegistersForEvents) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  // Provide expected behavior to support session creation
  EXPECT_CALL(*cdm, OpenSession(StrEq("com.widevine"), _, _, _, _))
      .Times(AtLeast(1))
      .WillRepeatedly(DoAll(SetArgPointee<4>(cdmSessionId),
                            testing::Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Invoke(setSessionIdOnMap<4>));

  EXPECT_CALL(*cdm, CloseSession(_))
      .Times(AtLeast(0));

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);
  plugin.openSession([&](Status status, hidl_vec<uint8_t> /* hSessionId */) {
    ASSERT_EQ(Status::OK, status);
  });
}

TEST_F(WVDrmPluginTest, UnregistersForAllEventsOnDestruction) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  uint8_t sessionIdRaw1[kSessionIdSize];
  uint8_t sessionIdRaw2[kSessionIdSize];
  FILE* fp = fopen("/dev/urandom", "r");
  fread(sessionIdRaw1, sizeof(uint8_t), kSessionIdSize, fp);
  fread(sessionIdRaw2, sizeof(uint8_t), kSessionIdSize, fp);
  fclose(fp);

  CdmSessionId cdmSessionId1(sessionIdRaw1, sessionIdRaw1 + kSessionIdSize);
  CdmSessionId cdmSessionId2(sessionIdRaw2, sessionIdRaw2 + kSessionIdSize);

  EXPECT_CALL(*cdm, OpenSession(StrEq("com.widevine"), _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<4>(cdmSessionId1),
                      testing::Return(wvcdm::NO_ERROR)))
      .WillOnce(DoAll(SetArgPointee<4>(cdmSessionId2),
                      testing::Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId1, _))
      .WillOnce(Invoke(setSessionIdOnMap<4>));

  EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId2, _))
      .WillOnce(Invoke(setSessionIdOnMap<5>));

  EXPECT_CALL(*cdm, CloseSession(_))
      .Times(AtLeast(0));

  {
    WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);

    plugin.openSession([&](Status status, hidl_vec<uint8_t> /* hSessionId */) {
      ASSERT_EQ(Status::OK, status);
    });

    plugin.openSession([&](Status status, hidl_vec<uint8_t> /* hSessionId */) {
      ASSERT_EQ(Status::OK, status);
    });
  }
}

// TODO b/35325611 Fix this disabled test
TEST_F(WVDrmPluginTest, DISABLED_MarshalsEvents) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  sp<StrictMock<MockDrmPluginListener> > listener =
      new StrictMock<MockDrmPluginListener>();

  const int64_t kExpiryTimeInSeconds = 123456789012LL;
  std::string kKeyId1 = "Testing Key1 Id ";
  std::string kKeyId2 = "Testing Key2 Id ";
  std::string kKeyId3 = "Testing Key3 Id ";
  std::string kKeyId4 = "Testing Key4 Id ";
  std::string kKeyId5 = "Testing Key5 Id ";

  {
    InSequence calls;

    hidl_vec<uint8_t> hEmptyData;
    hidl_vec<uint8_t> hSessionId;
    hSessionId.setToExternal(sessionIdRaw, kSessionIdSize);

    std::vector<uint8_t> keyId;
    std::vector<KeyStatus_V1_2> keyStatusList;

    KeyStatus_V1_2 keyStatus;
    keyId.assign(kKeyId1.begin(), kKeyId1.end());
    keyStatus.keyId = toHidlVec(keyId);
    keyStatus.type = KeyStatusType_V1_2::EXPIRED;
    keyStatusList.push_back(keyStatus);

    hidl_vec<KeyStatus_V1_2> hKeyStatusList = toHidlVec(keyStatusList);
    EXPECT_CALL(*listener,
                sendKeysChange_1_2(hSessionId, hKeyStatusList, false));

    EXPECT_CALL(
        *listener,
        sendEvent(EventType::KEY_EXPIRED, hSessionId, hEmptyData));

    EXPECT_CALL(*listener,
                sendEvent(EventType::KEY_NEEDED, hSessionId, hEmptyData));

    // Expiry Time in Java API is in milliseconds.
    EXPECT_CALL(*listener, sendExpirationUpdate(hSessionId, NEVER_EXPIRES));
    EXPECT_CALL(*listener,
                sendExpirationUpdate(hSessionId, kExpiryTimeInSeconds * 1000));

    keyStatusList.clear();
    keyId.clear();
    keyId.assign(kKeyId1.begin(), kKeyId1.end());
    keyStatus.type = KeyStatusType_V1_2::USABLE;
    keyStatusList.push_back(keyStatus);
    keyId.assign(kKeyId2.begin(), kKeyId2.end());
    keyStatus.type = KeyStatusType_V1_2::OUTPUTNOTALLOWED;
    keyStatusList.push_back(keyStatus);
    keyId.assign(kKeyId3.begin(), kKeyId3.end());
    keyStatus.type = KeyStatusType_V1_2::INTERNALERROR;
    keyStatusList.push_back(keyStatus);
    keyId.assign(kKeyId4.begin(), kKeyId4.end());
    keyStatus.type = KeyStatusType_V1_2::STATUSPENDING;
    keyStatusList.push_back(keyStatus);
    keyId.assign(kKeyId5.begin(), kKeyId5.end());
    keyStatus.type = KeyStatusType_V1_2::USABLEINFUTURE;
    keyStatusList.push_back(keyStatus);

    hidl_vec<KeyStatus_V1_2> hKeyStatusList2 = toHidlVec(keyStatusList);
    EXPECT_CALL(*listener, sendKeysChange_1_2(hSessionId, hKeyStatusList2, false));
  }

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);
  plugin.setListener(listener);

  CdmKeyStatusMap cdmKeysStatus;
  cdmKeysStatus[kKeyId1] = wvcdm::kKeyStatusExpired;
  plugin.OnSessionKeysChange(cdmSessionId, cdmKeysStatus, false);

  plugin.OnSessionRenewalNeeded(cdmSessionId);
  plugin.OnExpirationUpdate(cdmSessionId, NEVER_EXPIRES);
  plugin.OnExpirationUpdate(cdmSessionId, kExpiryTimeInSeconds);

  cdmKeysStatus[kKeyId1] = wvcdm::kKeyStatusUsable;
  cdmKeysStatus[kKeyId2] = wvcdm::kKeyStatusOutputNotAllowed;
  cdmKeysStatus[kKeyId3] = wvcdm::kKeyStatusInternalError;
  cdmKeysStatus[kKeyId4] = wvcdm::kKeyStatusPending;
  cdmKeysStatus[kKeyId5] = wvcdm::kKeyStatusUsableInFuture;
  plugin.OnSessionKeysChange(cdmSessionId, cdmKeysStatus, true);
}

// TODO b/35325611 Fix this disabled test
TEST_F(WVDrmPluginTest, DISABLED_GeneratesProvisioningNeededEvent) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  sp<StrictMock<MockDrmPluginListener> > listener =
      new StrictMock<MockDrmPluginListener>();

  hidl_vec<uint8_t> hEmptyData;
  hidl_vec<uint8_t> hSessionId;
  hSessionId.setToExternal(sessionIdRaw, kSessionIdSize);

  EXPECT_CALL(*listener,
              sendEvent(EventType::PROVISION_REQUIRED, hSessionId, hEmptyData))
      .Times(1);

  EXPECT_CALL(*cdm, OpenSession(StrEq("com.widevine"), _, _, _, _))
      .Times(AtLeast(1))
      .WillRepeatedly(DoAll(SetArgPointee<4>(cdmSessionId),
                            testing::Return(wvcdm::NEED_PROVISIONING)));

  EXPECT_CALL(*cdm, CloseSession(_))
      .Times(AtLeast(0));

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);
  plugin.setListener(listener);

  plugin.openSession([&](Status status, hidl_vec<uint8_t> /* hSessionId */) {
    ASSERT_EQ(Status::ERROR_DRM_NOT_PROVISIONED, status);
  });
}

TEST_F(WVDrmPluginTest, ProvidesExpectedDefaultPropertiesToCdm) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  const CdmClientPropertySet* propertySet = nullptr;

  // Provide expected mock behavior
  {
    // Provide expected behavior in response to OpenSession and store the
    // property set
    EXPECT_CALL(*cdm, OpenSession(_, _, _, _, _))
        .WillRepeatedly(DoAll(SetArgPointee<4>(cdmSessionId),
                              SaveArg<1>(&propertySet),
                              testing::Return(wvcdm::NO_ERROR)));

    // Provide expected behavior when plugin requests session control info
    EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId, _))
        .WillRepeatedly(Invoke(setSessionIdOnMap<4>));

    EXPECT_CALL(*cdm, CloseSession(_))
        .Times(AtLeast(0));
  }

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);

  plugin.openSession([&](Status status, hidl_vec<uint8_t> /* hSessionId */) {
    ASSERT_EQ(Status::OK, status);
  });

  ASSERT_THAT(propertySet, NotNull());
  EXPECT_STREQ("", propertySet->security_level().c_str());
  EXPECT_FALSE(propertySet->use_privacy_mode());
  EXPECT_EQ(0u, propertySet->service_certificate().size());
  EXPECT_FALSE(propertySet->is_session_sharing_enabled());
  EXPECT_EQ(0u, propertySet->session_sharing_id());
  EXPECT_STREQ("", propertySet->app_id().c_str());
}

TEST_F(WVDrmPluginTest, CanSetAppId) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  const CdmClientPropertySet* propertySet = nullptr;

  // Provide expected mock behavior
  {
    // Provide expected behavior in response to OpenSession and store the
    // property set
    EXPECT_CALL(*cdm, OpenSession(_, _, _, _, _))
        .WillRepeatedly(DoAll(SetArgPointee<4>(cdmSessionId),
                              SaveArg<1>(&propertySet),
                              testing::Return(wvcdm::NO_ERROR)));

    // Provide expected behavior when plugin queries for the security level
    EXPECT_CALL(*cdm, QueryStatus(_, QUERY_KEY_SECURITY_LEVEL, _))
        .WillRepeatedly(DoAll(SetArgPointee<2>(QUERY_VALUE_SECURITY_LEVEL_L3),
                              testing::Return(wvcdm::NO_ERROR)));

    // Provide expected behavior when plugin requests session control info
    EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId, _))
        .WillRepeatedly(Invoke(setSessionIdOnMap<4>));

    EXPECT_CALL(*cdm, CloseSession(_))
        .Times(AtLeast(0));
  }

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);

  // Test setting an empty string
  Status status = plugin.setPropertyString(hidl_string("appId"),
                                           hidl_string(""));
  ASSERT_EQ(Status::OK, status);

  // Test setting an application id before a session is opened.
  status = plugin.setPropertyString(hidl_string("appId"),
                                    hidl_string(kAppId));
  ASSERT_EQ(Status::OK, status);

  plugin.openSession([&](Status status, hidl_vec<uint8_t> /* hSessionId */) {
    ASSERT_EQ(Status::OK, status);
  });
  ASSERT_THAT(propertySet, NotNull());

  // Verify application id is set correctly.
  EXPECT_STREQ(kAppId.c_str(), propertySet->app_id().c_str());

  // Test setting application id while session is opened, this should fail.
  status = plugin.setPropertyString(hidl_string("appId"),
                                    hidl_string(kAppId));
  ASSERT_EQ(Status::ERROR_DRM_UNKNOWN, status);
}

TEST_P(WVDrmPluginOriginTest, CanSetOrigin) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  OriginTestVariant params = GetParam();

  // Provide expected mock behavior
  {
    // Provide expected behavior when plugin requests session control info
    EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId, _))
        .WillRepeatedly(Invoke(setSessionIdOnMap<4>));

    // Provide expected behavior when plugin closes a session
    EXPECT_CALL(*cdm, CloseSession(_))
        .Times(AtLeast(0));
  }

  // Note which mock calls we expect
  EXPECT_CALL(*cdm, OpenSession(_, _, HasOrigin(params.expectedOrigin), _, _))
      .WillOnce(DoAll(SetArgPointee<4>(cdmSessionId),
                      testing::Return(wvcdm::NO_ERROR)));

  // Set the properties & run the test
  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);

  if (!params.origin.empty()) {
    ASSERT_EQ(Status::OK,
              plugin.setPropertyString(hidl_string("origin"),
                                       hidl_string(params.origin)));
  }

  plugin.openSession([&](Status status, hidl_vec<uint8_t> /* hSessionId */) {
    EXPECT_EQ(Status::OK, status);
  });
  // Test setting an origin while sessions are opened. This should fail.
  EXPECT_NE(Status::OK,
            plugin.setPropertyString(hidl_string("origin"),
                                     hidl_string(kOrigin)));
  EXPECT_EQ(Status::OK, plugin.closeSession(toHidlVec(sessionId)));
}

INSTANTIATE_TEST_CASE_P(OriginTests, WVDrmPluginOriginTest, Values(
    OriginTestVariant("No Origin", kEmptyString, EMPTY_ORIGIN),
    OriginTestVariant("With an Origin", kOrigin, kOrigin.c_str())));

TEST_F(WVDrmPluginTest, CanSetSecurityLevel) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  const CdmClientPropertySet* propertySet = nullptr;

  EXPECT_CALL(*cdm, QueryStatus(_, QUERY_KEY_SECURITY_LEVEL, _))
      .WillOnce(DoAll(SetArgPointee<2>(QUERY_VALUE_SECURITY_LEVEL_L3),
                      testing::Return(wvcdm::NO_ERROR)))
      .WillOnce(DoAll(SetArgPointee<2>(QUERY_VALUE_SECURITY_LEVEL_L1),
                      testing::Return(wvcdm::NO_ERROR)));

  // Provide expected mock behavior
  {
    // Provide expected behavior in response to OpenSession and store the
    // property set
    EXPECT_CALL(*cdm, OpenSession(_, _, _, _, _))
        .WillRepeatedly(DoAll(SetArgPointee<4>(cdmSessionId),
                              SaveArg<1>(&propertySet),
                              testing::Return(wvcdm::NO_ERROR)));

    // Provide expected behavior when plugin requests session control info
    EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId, _))
        .WillRepeatedly(Invoke(setSessionIdOnMap<4>));

    EXPECT_CALL(*cdm, CloseSession(_))
        .Times(AtLeast(0));
  }

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);

  // Test forcing L3
  Status status = plugin.setPropertyString(hidl_string("securityLevel"),
                                           hidl_string("L3"));
  ASSERT_EQ(Status::OK, status);

  plugin.openSession([&](Status status, hidl_vec<uint8_t> hSessionId) {
    ASSERT_EQ(Status::OK, status);
    sessionId.clear();
    sessionId.assign(hSessionId.data(), hSessionId.data() + hSessionId.size());
  });
  ASSERT_THAT(propertySet, NotNull());
  EXPECT_STREQ("L3", propertySet->security_level().c_str());
  status = plugin.closeSession(toHidlVec(sessionId));
  ASSERT_EQ(Status::OK, status);

  // Test returning to L1 on an L3 device (Should Fail)
  status = plugin.setPropertyString(hidl_string("securityLevel"),
                                           hidl_string("L1"));
  ASSERT_NE(Status::OK, status);

  plugin.openSession([&](Status status, hidl_vec<uint8_t> hSessionId) {
    ASSERT_EQ(Status::OK, status);
    sessionId.clear();
    sessionId.assign(hSessionId.data(), hSessionId.data() + hSessionId.size());
  });
  ASSERT_THAT(propertySet, NotNull());
  EXPECT_STREQ("L3", propertySet->security_level().c_str());
  status = plugin.closeSession(toHidlVec(sessionId));
  ASSERT_EQ(Status::OK, status);

  // Test returning to L1 on an L1 device
  status = plugin.setPropertyString(hidl_string("securityLevel"),
                                           hidl_string("L1"));
  ASSERT_EQ(Status::OK, status);

  plugin.openSession([&](Status status, hidl_vec<uint8_t> hSessionId) {
    ASSERT_EQ(Status::OK, status);
    sessionId.clear();
    sessionId.assign(hSessionId.data(), hSessionId.data() + hSessionId.size());
  });
  ASSERT_THAT(propertySet, NotNull());
  EXPECT_STREQ("", propertySet->security_level().c_str());
  status = plugin.closeSession(toHidlVec(sessionId));
  ASSERT_EQ(Status::OK, status);

  // Test un-forcing a level (first forcing to L3 so we have something to reset)
  status = plugin.setPropertyString(hidl_string("securityLevel"),
                                           hidl_string("L3"));
  ASSERT_EQ(Status::OK, status);

  plugin.openSession([&](Status status, hidl_vec<uint8_t> hSessionId) {
    ASSERT_EQ(Status::OK, status);
    sessionId.clear();
    sessionId.assign(hSessionId.data(), hSessionId.data() + hSessionId.size());
  });
  ASSERT_THAT(propertySet, NotNull());
  EXPECT_STREQ("L3", propertySet->security_level().c_str());
  status = plugin.closeSession(toHidlVec(sessionId));
  ASSERT_EQ(Status::OK, status);

  status = plugin.setPropertyString(hidl_string("securityLevel"),
                                           hidl_string(""));
  ASSERT_EQ(Status::OK, status);

  plugin.openSession([&](Status status, hidl_vec<uint8_t> hSessionId) {
    ASSERT_EQ(Status::OK, status);
    sessionId.clear();
    sessionId.assign(hSessionId.data(), hSessionId.data() + hSessionId.size());
  });
  ASSERT_THAT(propertySet, NotNull());
  EXPECT_STREQ("", propertySet->security_level().c_str());
  status = plugin.closeSession(toHidlVec(sessionId));
  ASSERT_EQ(Status::OK, status);

  // Test nonsense (Should Fail)
  status = plugin.setPropertyString(hidl_string("securityLevel"),
                                           hidl_string("nonsense"));
  ASSERT_NE(Status::OK, status);

  plugin.openSession([&](Status status, hidl_vec<uint8_t> hSessionId) {
    ASSERT_EQ(Status::OK, status);
    sessionId.clear();
    sessionId.assign(hSessionId.data(), hSessionId.data() + hSessionId.size());
  });
  ASSERT_THAT(propertySet, NotNull());
  EXPECT_STREQ("", propertySet->security_level().c_str());
  status = plugin.closeSession(toHidlVec(sessionId));
  ASSERT_EQ(Status::OK, status);

  // Test attempting to force a level with a session open (Should Fail)
  plugin.openSession([&](Status status, hidl_vec<uint8_t> /* hSessionId */) {
    ASSERT_EQ(Status::OK, status);
  });
  status = plugin.setPropertyString(hidl_string("securityLevel"),
                                           hidl_string("L3"));
  ASSERT_NE(Status::OK, status);
}

TEST_F(WVDrmPluginTest, CanSetPrivacyMode) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  const CdmClientPropertySet* propertySet = nullptr;

  // Provide expected mock behavior
  {
    // Provide expected behavior in response to OpenSession and store the
    // property set
    EXPECT_CALL(*cdm, OpenSession(_, _, _, _, _))
        .WillRepeatedly(DoAll(SetArgPointee<4>(cdmSessionId),
                              SaveArg<1>(&propertySet),
                              testing::Return(wvcdm::NO_ERROR)));

    // Provide expected behavior when plugin requests session control info
    EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId, _))
        .WillRepeatedly(Invoke(setSessionIdOnMap<4>));

    EXPECT_CALL(*cdm, CloseSession(_))
        .Times(AtLeast(0));
  }

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);

  plugin.openSession([&](Status status, hidl_vec<uint8_t> hSessionId) {
    ASSERT_EQ(Status::OK, status);
    sessionId.clear();
    sessionId.assign(hSessionId.data(), hSessionId.data() + hSessionId.size());
  });
  ASSERT_THAT(propertySet, NotNull());

  // Test turning on privacy mode
  Status status = plugin.setPropertyString(hidl_string("privacyMode"),
                                           hidl_string("enable"));
  ASSERT_EQ(Status::OK, status);
  EXPECT_TRUE(propertySet->use_privacy_mode());

  // Test turning off privacy mode
  status = plugin.setPropertyString(hidl_string("privacyMode"),
                                    hidl_string("disable"));
  ASSERT_EQ(Status::OK, status);
  EXPECT_FALSE(propertySet->use_privacy_mode());

  // Test nonsense (Should Fail)
  status = plugin.setPropertyString(hidl_string("privacyMode"),
                                    hidl_string("nonsense"));
  ASSERT_NE(Status::OK, status);
}

TEST_F(WVDrmPluginTest, CanSetServiceCertificate) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  const CdmClientPropertySet* propertySet = nullptr;

  static const size_t kPrivacyCertSize = 256;
  uint8_t privacyCertRaw[kPrivacyCertSize];

  FILE* fp = fopen("/dev/urandom", "r");
  fread(privacyCertRaw, sizeof(uint8_t), kPrivacyCertSize, fp);
  fclose(fp);

  std::vector<uint8_t> privacyCert;
  privacyCert.assign(privacyCertRaw, privacyCertRaw + kPrivacyCertSize);
  std::string strPrivacyCert(reinterpret_cast<char*>(privacyCertRaw),
                             kPrivacyCertSize);
  std::vector<uint8_t> emptyVector;

  // Provide expected mock behavior
  {
    // Provide expected behavior in response to OpenSession and store the
    // property set
    EXPECT_CALL(*cdm, OpenSession(_, _, _, _, _))
        .WillRepeatedly(DoAll(SetArgPointee<4>(cdmSessionId),
                              SaveArg<1>(&propertySet),
                              testing::Return(wvcdm::NO_ERROR)));

    // Provide expected behavior when plugin requests session control info
    EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId, _))
        .WillRepeatedly(Invoke(setSessionIdOnMap<4>));

    EXPECT_CALL(*cdm, CloseSession(_))
        .Times(AtLeast(0));
  }

  // Validate that the certificate is validated. Accept it once and reject it
  // once. Note that there is no expected call for when the certificate is
  // cleared.
  EXPECT_CALL(*cdm, IsValidServiceCertificate(strPrivacyCert))
      .WillOnce(testing::Return(true))
      .WillOnce(testing::Return(false));

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);

  plugin.openSession([&](Status status, hidl_vec<uint8_t> hSessionId) {
    ASSERT_EQ(Status::OK, status);
    sessionId.clear();
    sessionId.assign(hSessionId.data(), hSessionId.data() + hSessionId.size());
  });
  ASSERT_THAT(propertySet, NotNull());

  // Test setting a certificate
  Status status = plugin.setPropertyByteArray(hidl_string("serviceCertificate"),
                                              toHidlVec(privacyCert));
  ASSERT_EQ(Status::OK, status);
  EXPECT_THAT(propertySet->service_certificate(),
              ElementsAreArray(privacyCertRaw, kPrivacyCertSize));

  // Test clearing a certificate
  status = plugin.setPropertyByteArray(hidl_string("serviceCertificate"),
                                       toHidlVec(emptyVector));
  ASSERT_EQ(Status::OK, status);
  EXPECT_EQ(0u, propertySet->service_certificate().size());

  // Test setting a certificate and having it fail
  status = plugin.setPropertyByteArray(hidl_string("serviceCertificate"),
                                       toHidlVec(privacyCert));
  ASSERT_NE(Status::OK, status);
  EXPECT_EQ(0u, propertySet->service_certificate().size());
}

TEST_F(WVDrmPluginTest, CanSetSessionSharing) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  const CdmClientPropertySet* propertySet = nullptr;

  // Provide expected mock behavior
  {
    // Provide expected behavior in response to OpenSession and store the
    // property set
    EXPECT_CALL(*cdm, OpenSession(_, _, _, _, _))
        .WillRepeatedly(DoAll(SetArgPointee<4>(cdmSessionId),
                              SaveArg<1>(&propertySet),
                              testing::Return(wvcdm::NO_ERROR)));

    // Provide expected behavior when plugin requests session control info
    EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId, _))
        .WillRepeatedly(Invoke(setSessionIdOnMap<4>));

    EXPECT_CALL(*cdm, CloseSession(_))
        .Times(AtLeast(0));
  }

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);

  // Test turning on session sharing
  Status status = plugin.setPropertyString(hidl_string("sessionSharing"),
                                           hidl_string("enable"));
  ASSERT_EQ(Status::OK, status);

  plugin.openSession([&](Status status, hidl_vec<uint8_t> hSessionId) {
    ASSERT_EQ(Status::OK, status);
    sessionId.clear();
    sessionId.assign(hSessionId.data(), hSessionId.data() + hSessionId.size());
  });
  ASSERT_THAT(propertySet, NotNull());
  EXPECT_TRUE(propertySet->is_session_sharing_enabled());
  status = plugin.closeSession(toHidlVec(sessionId));
  ASSERT_EQ(Status::OK, status);

  // Test turning off session sharing
  status = plugin.setPropertyString(hidl_string("sessionSharing"),
                                                hidl_string("disable"));
  ASSERT_EQ(Status::OK, status);

  plugin.openSession([&](Status status, hidl_vec<uint8_t> hSessionId) {
    ASSERT_EQ(Status::OK, status);
    sessionId.clear();
    sessionId.assign(hSessionId.data(), hSessionId.data() + hSessionId.size());
  });
  ASSERT_THAT(propertySet, NotNull());
  EXPECT_FALSE(propertySet->is_session_sharing_enabled());
  status = plugin.closeSession(toHidlVec(sessionId));
  ASSERT_EQ(Status::OK, status);

  // Test nonsense (Should Fail)
  status = plugin.setPropertyString(hidl_string("sessionSharing"),
                                                hidl_string("nonsense"));
  ASSERT_NE(Status::OK, status);

  // Test changing sharing with a session open (Should Fail)
  plugin.openSession([&](Status status, hidl_vec<uint8_t> hSessionId) {
    ASSERT_EQ(Status::OK, status);
    sessionId.clear();
    sessionId.assign(hSessionId.data(), hSessionId.data() + hSessionId.size());
  });
  status = plugin.setPropertyString(hidl_string("sessionSharing"),
                                                hidl_string("enable"));
  ASSERT_NE(Status::OK, status);

  status = plugin.closeSession(toHidlVec(sessionId));
  ASSERT_EQ(Status::OK, status);
}

TEST_F(WVDrmPluginTest, AllowsStoringOfSessionSharingId) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  CdmClientPropertySet* propertySet = nullptr;

  uint32_t sharingId;
  FILE* fp = fopen("/dev/urandom", "r");
  fread(&sharingId, sizeof(uint32_t), 1, fp);
  fclose(fp);

  // Provide expected mock behavior
  {
    // Provide expected behavior in response to OpenSession and store the
    // property set
    EXPECT_CALL(*cdm, OpenSession(_, _, _, _, _))
        .WillRepeatedly(DoAll(SetArgPointee<4>(cdmSessionId),
                              SaveArg<1>(&propertySet),
                              testing::Return(wvcdm::NO_ERROR)));

    // Provide expected behavior when plugin requests session control info
    EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId, _))
        .WillRepeatedly(Invoke(setSessionIdOnMap<4>));

    EXPECT_CALL(*cdm, CloseSession(_))
        .Times(AtLeast(0));
  }

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);
  plugin.openSession([&](Status status, hidl_vec<uint8_t> hSessionId) {
    ASSERT_EQ(Status::OK, status);
    sessionId.clear();
    sessionId.assign(hSessionId.data(), hSessionId.data() + hSessionId.size());
  });

  ASSERT_THAT(propertySet, NotNull());
  propertySet->set_session_sharing_id(sharingId);
  EXPECT_EQ(sharingId, propertySet->session_sharing_id());
}

TEST_F(WVDrmPluginTest, CanSetDecryptHashProperties) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  // Provide expected mock behavior
  {
    // Provide expected behavior in response to OpenSession and store the
    // property set
    EXPECT_CALL(*cdm, OpenSession(_, _, _, _, _))
        .WillRepeatedly(DoAll(SetArgPointee<4>(cdmSessionId),
                              testing::Return(wvcdm::NO_ERROR)));

    // Provide expected behavior when plugin requests session control info
    EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId, _))
        .WillRepeatedly(Invoke(setSessionIdOnMap<4>));

    EXPECT_CALL(*cdm, CloseSession(_))
        .Times(AtLeast(0));
  }

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);

  hidl_vec<uint8_t> hSessionId;
  hSessionId.setToExternal(sessionIdRaw, kSessionIdSize);
  plugin.openSession([&](Status status, hidl_vec<uint8_t> hSessionId) {
    ASSERT_EQ(Status::OK, status);
    sessionId.clear();
    sessionId.assign(hSessionId.data(), hSessionId.data() + hSessionId.size());
  });

  // CDM expects the string property value to be in the following format:
  // "<sessionId>,<frameNumber>,<base64encodedHash>"
  static const std::string frameNumber = ",1";
  uint32_t hash = 0xbeef;  // crc32 hash
  std::vector<uint8_t> hashVector(
      reinterpret_cast<uint8_t*>(&hash),
      reinterpret_cast<uint8_t*>(&hash) + sizeof(uint32_t));
  std::string base64EncodedHash = Base64Encode(hashVector);
  std::string computedHash(sessionId.begin(), sessionId.end());
  computedHash.append(frameNumber.c_str());
  computedHash.append(base64EncodedHash.c_str());
  Status status = plugin.setPropertyString(hidl_string("decryptHash"),
                                           hidl_string(computedHash));
  ASSERT_NE(Status::OK, status);

  status = plugin.closeSession(toHidlVec(sessionId));
  ASSERT_EQ(Status::OK, status);
}

TEST_F(WVDrmPluginTest, DoesNotSetDecryptHashProperties) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  // Provide expected mock behavior
  {
    // Provide expected behavior in response to OpenSession and store the
    // property set
    EXPECT_CALL(*cdm, OpenSession(_, _, _, _, _))
        .WillRepeatedly(DoAll(SetArgPointee<4>(cdmSessionId),
                              testing::Return(wvcdm::NO_ERROR)));

    // Provide expected behavior when plugin requests session control info
    EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId, _))
        .WillRepeatedly(Invoke(setSessionIdOnMap<4>));

    EXPECT_CALL(*cdm, CloseSession(_))
        .Times(AtLeast(0));
  }

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);

  hidl_vec<uint8_t> hSessionId;
  hSessionId.setToExternal(sessionIdRaw, kSessionIdSize);
  plugin.openSession([&](Status status, hidl_vec<uint8_t> hSessionId) {
    ASSERT_EQ(Status::OK, status);
    sessionId.clear();
    sessionId.assign(hSessionId.data(), hSessionId.data() + hSessionId.size());
  });

  // CDM expects the string property value to be in the following format:
  // "<sessionId>,<frameNumber>,<base64encodedHash>"
  static const std::string frameNumber = ",1";
  static const std::string hash = ",AZaz0+,/";
  std::string value(sessionId.begin(), sessionId.end());
  value.append(frameNumber.c_str());

  // Tests for missing token handling
  Status status = plugin.setPropertyString(hidl_string("decryptHash"),
                                           hidl_string(value));
  EXPECT_NE(Status::OK, status);

  // Tests for empty token
  value.append(",");
  status = plugin.setPropertyString(hidl_string("decryptHash"),
                                    hidl_string(value));
  EXPECT_NE(Status::OK, status);

  // Tests for invalid sessionId
  value.clear();
  value.append("bad session id");
  value.append(",1");
  value.append(hash.c_str());
  status = plugin.setPropertyString(hidl_string("decryptHash"),
                                    hidl_string(value));
  EXPECT_NE(Status::OK, status);

  // Tests for malformed Base64encode hash, with a ","
  std::string computedHash(sessionId.begin(), sessionId.end());
  computedHash.append(frameNumber.c_str());
  computedHash.append(hash.c_str());
  status = plugin.setPropertyString(hidl_string("decryptHash"),
                                    hidl_string(computedHash));
  EXPECT_NE(Status::OK, status);

  status = plugin.closeSession(toHidlVec(sessionId));
  ASSERT_EQ(Status::OK, status);
}

TEST_F(WVDrmPluginTest, GetOfflineLicenseIds) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  const uint32_t kLicenseCount = 5;

  uint8_t mockIdsRaw[kLicenseCount * 2][kKeySetIdSize];
  FILE* fp = fopen("/dev/urandom", "r");
  for (uint32_t i = 0; i < kLicenseCount * 2; ++i) {
    fread(mockIdsRaw[i], sizeof(uint8_t), kKeySetIdSize, fp);
  }
  fclose(fp);

  std::vector<std::string> mockIdsL1;
  for (uint32_t i = 0; i < kLicenseCount; ++i) {
    mockIdsL1.push_back(std::string(mockIdsRaw[i],
                        mockIdsRaw[i] + kKeySetIdSize));
  }

  std::vector<std::string> mockIdsL3;
  for (uint32_t i = 0; i < kLicenseCount; ++i) {
    mockIdsL3.push_back(std::string(mockIdsRaw[i+5],
                        mockIdsRaw[i+5] + kKeySetIdSize));
  }

  EXPECT_CALL(*cdm,
              ListStoredLicenses(kSecurityLevelL1, HasOrigin(EMPTY_ORIGIN), _))
      .WillOnce(DoAll(SetArgPointee<2>(mockIdsL1),
                      testing::Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm,
              ListStoredLicenses(kSecurityLevelL3, HasOrigin(EMPTY_ORIGIN), _))
      .WillOnce(DoAll(SetArgPointee<2>(mockIdsL3),
                      testing::Return(wvcdm::NO_ERROR)));

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);

  std::vector<std::vector<uint8_t> > offlineIds;
  offlineIds.clear();
  plugin.getOfflineLicenseKeySetIds(
      [&](Status status, hidl_vec<KeySetId> hKeySetIds) {
    ASSERT_EQ(Status::OK, status);

    std::vector<KeySetId> ids(hKeySetIds);

    for (auto id : ids) {
      offlineIds.push_back(id);
    }
  });


  size_t index = 0;
  for (auto id : offlineIds) {
    EXPECT_THAT(id, ElementsAreArray(mockIdsRaw[index++], kKeySetIdSize));
  }
  EXPECT_EQ(kLicenseCount * 2, index);
}

TEST_F(WVDrmPluginTest, GetOfflineLicenseState) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  EXPECT_CALL(*cdm, QueryStatus(_, QUERY_KEY_SECURITY_LEVEL, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(QUERY_VALUE_SECURITY_LEVEL_L1),
                      testing::Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm,
              GetOfflineLicenseState(_, kSecurityLevelL1,
                                     HasOrigin(EMPTY_ORIGIN), _))
      .WillOnce(DoAll(SetArgPointee<3>(wvcdm::kLicenseStateActive),
                      testing::Return(wvcdm::NO_ERROR)))
      .WillOnce(DoAll(SetArgPointee<3>(wvcdm::kLicenseStateReleasing),
                      testing::Return(wvcdm::NO_ERROR)))
      .WillOnce(DoAll(SetArgPointee<3>(wvcdm::kLicenseStateUnknown),
                      testing::Return(wvcdm::NO_ERROR)));

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);
  Status status = plugin.setPropertyString(
      hidl_string("securityLevel"), hidl_string("L1"));
  ASSERT_EQ(Status::OK, status);

  plugin.getOfflineLicenseState(toHidlVec(keySetId),
      [&](Status status, OfflineLicenseState hLicenseState) {
    ASSERT_EQ(Status::OK, status);
    ASSERT_EQ(OfflineLicenseState::USABLE, hLicenseState);
  });

  plugin.getOfflineLicenseState(toHidlVec(keySetId),
      [&](Status status, OfflineLicenseState hLicenseState) {
    ASSERT_EQ(Status::OK, status);
    ASSERT_EQ(OfflineLicenseState::INACTIVE, hLicenseState);
  });

  plugin.getOfflineLicenseState(toHidlVec(keySetId),
      [&](Status status, OfflineLicenseState hLicenseState) {
    ASSERT_EQ(Status::OK, status);
    ASSERT_EQ(OfflineLicenseState::UNKNOWN, hLicenseState);
  });
}

TEST_F(WVDrmPluginTest, RemoveOfflineLicense) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  std::string appPackageName;

  EXPECT_CALL(*cdm,
              RemoveOfflineLicense(_, kSecurityLevelL1,
                                   HasOrigin(EMPTY_ORIGIN)))
      .Times(1);

  WVDrmPlugin plugin(cdm.get(), appPackageName, &crypto, false);

  Status status = plugin.removeOfflineLicense(toHidlVec(keySetId));
  ASSERT_EQ(Status::OK, status);
}

}  // namespace widevine
}  // namespace V1_2
}  // namespace drm
}  // namespace hardware
}  // namespace wvdrm
