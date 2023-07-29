//
// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//

#include <stdio.h>
#include <string.h>
#include <ostream>
#include <string>

#include "cdm_client_property_set.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "media/stagefright/foundation/ABase.h"
#include "media/stagefright/foundation/AString.h"
#include "media/stagefright/MediaErrors.h"
#include "string_conversions.h"
#include "wv_cdm_constants.h"
#include "wv_cdm_types.h"
#include "wv_content_decryption_module.h"
#include "WVDrmPlugin.h"
#include "WVErrors.h"

using namespace android;
using namespace std;
using namespace testing;
using namespace wvcdm;
using namespace wvdrm;

namespace {
const String8 kEmptyString;
const String8 kOrigin("widevine.com");
const String8 kAppId("com.unittest.mock.app.id");
const uint8_t* const kUnprovisionResponse =
    reinterpret_cast<const uint8_t*>("unprovision");
const size_t kUnprovisionResponseSize = 11;

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

  MOCK_METHOD3(QueryStatus, CdmResponseType(SecurityLevel, const std::string&,
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

class MockDrmPluginListener : public DrmPluginListener {
 public:
  MOCK_METHOD4(sendEvent, void(DrmPlugin::EventType, int,
                               const Vector<uint8_t>*, const Vector<uint8_t>*));
  MOCK_METHOD2(sendExpirationUpdate, void(const Vector<uint8_t>*, int64_t));
  MOCK_METHOD3(sendKeysChange, void(const Vector<uint8_t>*,
                                    const Vector<DrmPlugin::KeyStatus>*, bool));
};

template <uint8_t DIGIT>
CdmResponseType setSessionIdOnMap(Unused, CdmQueryMap* map) {
  static const char oecId[] = {DIGIT + '0', '\0'};
  (*map)[QUERY_KEY_OEMCRYPTO_SESSION_ID] = oecId;
  return wvcdm::NO_ERROR;
}

MATCHER_P(HasOrigin, origin, "") {
  return arg.origin == origin;
}

class WVDrmPluginTest : public Test {
 protected:
  static const uint32_t kSessionIdSize = 16;
  uint8_t sessionIdRaw[kSessionIdSize];
  Vector<uint8_t> sessionId;
  CdmSessionId cdmSessionId;

  virtual void SetUp() {
    // Fill the session ID
    FILE* fp = fopen("/dev/urandom", "r");
    fread(sessionIdRaw, sizeof(uint8_t), kSessionIdSize, fp);
    fclose(fp);

    memcpy(sessionIdRaw, SESSION_ID_PREFIX, sizeof(SESSION_ID_PREFIX) - 1);
    sessionId.appendArray(sessionIdRaw, kSessionIdSize);
    cdmSessionId.assign(sessionId.begin(), sessionId.end());

    // Set default return values for gMock
    DefaultValue<CdmResponseType>::Set(wvcdm::NO_ERROR);
    DefaultValue<OEMCryptoResult>::Set(OEMCrypto_SUCCESS);
    DefaultValue<bool>::Set(true);
  }
};

struct OriginTestVariant {
  // For a test that does not expect any follow-up queries
  OriginTestVariant(const std::string& nameValue, const String8& originValue,
                    const std::string& expectedOriginValue)
    : name(nameValue), origin(originValue),
      expectedOrigin(expectedOriginValue) {}

  const std::string name;
  const String8 origin;
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
  WVDrmPlugin plugin(cdm.get(), &crypto);

  EXPECT_CALL(*cdm,
      OpenSession(StrEq("com.widevine"), _, HasOrigin(EMPTY_ORIGIN), _, _))
          .WillOnce(DoAll(SetArgPointee<4>(cdmSessionId),
                          Return(wvcdm::NO_ERROR)));

  // Provide expected behavior when plugin requests session control info
  EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Invoke(setSessionIdOnMap<4>));

  EXPECT_CALL(*cdm, CloseSession(_))
      .Times(AtLeast(0));

  status_t res = plugin.openSession(sessionId);

  ASSERT_EQ(OK, res);
  EXPECT_THAT(sessionId, ElementsAreArray(sessionIdRaw, kSessionIdSize));
}

TEST_F(WVDrmPluginTest, ClosesSessions) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  WVDrmPlugin plugin(cdm.get(), &crypto);

  EXPECT_CALL(*cdm, CloseSession(cdmSessionId))
      .Times(1);

  status_t res = plugin.closeSession(sessionId);

  ASSERT_EQ(OK, res);
}

TEST_F(WVDrmPluginTest, ClosesSessionWithError) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  WVDrmPlugin plugin(cdm.get(), &crypto);

  EXPECT_CALL(*cdm, CloseSession(cdmSessionId))
      .WillOnce(Return(SESSION_NOT_FOUND_1));

  status_t res = plugin.closeSession(sessionId);

  ASSERT_EQ(ERROR_DRM_SESSION_NOT_OPENED, res);
}

TEST_F(WVDrmPluginTest, GeneratesKeyRequests) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  WVDrmPlugin plugin(cdm.get(), &crypto);

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
  Vector<uint8_t> keySetId;
  keySetId.appendArray(keySetIdRaw, kKeySetIdSize);

  CdmInitData cdmInitData(reinterpret_cast<char*>(initDataRaw), kInitDataSize);
  Vector<uint8_t> initData;
  initData.appendArray(initDataRaw, kInitDataSize);

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
  Vector<uint8_t> psshBox;
  psshBox.appendArray(psshBoxRaw, kPsshBoxSize);

  CdmKeyMessage cdmRequest(requestRaw, requestRaw + kRequestSize);

  KeyedVector<String8, String8> parameters;
  CdmAppParameterMap cdmParameters;
  parameters.add(String8("paddingScheme"), String8("BUBBLE WRAP"));
  cdmParameters["paddingScheme"] = "BUBBLE WRAP";
  parameters.add(String8("favorite-particle"), String8("tetraquark"));
  cdmParameters["favorite-particle"] = "tetraquark";
  parameters.add(String8("answer"), String8("6 * 9"));
  cdmParameters["answer"] = "6 * 9";

  static const char* kDefaultUrl = "http://google.com/";
  static const char* kIsoBmffMimeType = "cenc";
  static const char* kWebmMimeType = "webm";

  struct TestSet {
    const char* mimeType;
    const Vector<uint8_t>& initDataIn;
    const CdmInitData& initDataOut;
  };

  // We run the same set of operations on several sets of data, simulating
  // different valid calling patterns.
  TestSet testSets[] = {
    {kIsoBmffMimeType, psshBox, cdmPsshBox},  // ISO-BMFF, EME passing style
    {kIsoBmffMimeType, initData, cdmPsshBox}, // ISO-BMFF, old passing style
    {kWebmMimeType, initData, cdmInitData}    // WebM
  };
  size_t testSetCount = sizeof(testSets) / sizeof(TestSet);

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
                          Return(wvcdm::KEY_MESSAGE)));

      EXPECT_CALL(*cdm, GenerateKeyRequest(cdmSessionId, "", mimeType, initData,
                                          kLicenseTypeStreaming, cdmParameters,
                                          NotNull(), HasOrigin(EMPTY_ORIGIN),
                                          _))
          .WillOnce(DoAll(SetArgPointee<8>(renewalRequest),
                          Return(wvcdm::KEY_MESSAGE)));

      EXPECT_CALL(*cdm, GenerateKeyRequest("", cdmKeySetId, mimeType, initData,
                                          kLicenseTypeRelease, cdmParameters,
                                          NotNull(), HasOrigin(EMPTY_ORIGIN),
                                          _))

          .WillOnce(DoAll(SetArgPointee<8>(releaseRequest),
                          Return(wvcdm::KEY_MESSAGE)));
    }
  }

  // Performs the actual tests
  for (size_t i = 0; i < testSetCount; ++i)
  {
    const String8 mimeType(testSets[i].mimeType);
    const Vector<uint8_t>& initData = testSets[i].initDataIn;

    Vector<uint8_t> request;
    String8 defaultUrl;
    DrmPlugin::KeyRequestType keyRequestType;

    status_t res = plugin.getKeyRequest(sessionId, initData, mimeType,
                                        DrmPlugin::kKeyType_Offline,
                                        parameters, request, defaultUrl,
                                        &keyRequestType);
    ASSERT_EQ(OK, res);
    EXPECT_THAT(request, ElementsAreArray(requestRaw, kRequestSize));
    EXPECT_EQ(DrmPlugin::kKeyRequestType_Initial, keyRequestType);
    EXPECT_STREQ(kDefaultUrl, defaultUrl.string());

    res = plugin.getKeyRequest(sessionId, initData, mimeType,
                               DrmPlugin::kKeyType_Streaming, parameters,
                               request, defaultUrl, &keyRequestType);
    ASSERT_EQ(OK, res);
    EXPECT_THAT(request, ElementsAreArray(requestRaw, kRequestSize));
    EXPECT_EQ(DrmPlugin::kKeyRequestType_Renewal, keyRequestType);
    EXPECT_STREQ(kDefaultUrl, defaultUrl.string());

    res = plugin.getKeyRequest(keySetId, initData, mimeType,
                               DrmPlugin::kKeyType_Release, parameters,
                               request, defaultUrl, &keyRequestType);
    ASSERT_EQ(OK, res);
    EXPECT_THAT(request, ElementsAreArray(requestRaw, kRequestSize));
    EXPECT_EQ(DrmPlugin::kKeyRequestType_Release, keyRequestType);
    EXPECT_STREQ(kDefaultUrl, defaultUrl.string());
  }
}

TEST_F(WVDrmPluginTest, AddsKeys) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  WVDrmPlugin plugin(cdm.get(), &crypto);

  static const uint32_t kResponseSize = 256;
  uint8_t responseRaw[kResponseSize];
  static const uint32_t kKeySetIdSize = 32;
  uint8_t keySetIdRaw[kKeySetIdSize];
  FILE* fp = fopen("/dev/urandom", "r");
  fread(responseRaw, sizeof(uint8_t), kResponseSize, fp);
  fread(keySetIdRaw, sizeof(uint8_t), kKeySetIdSize, fp);
  fclose(fp);

  Vector<uint8_t> response;
  response.appendArray(responseRaw, kResponseSize);

  memcpy(keySetIdRaw, KEY_SET_ID_PREFIX, sizeof(KEY_SET_ID_PREFIX) - 1);
  CdmKeySetId cdmKeySetId(reinterpret_cast<char *>(keySetIdRaw), kKeySetIdSize);
  Vector<uint8_t> keySetId;

  Vector<uint8_t> emptyKeySetId;

  EXPECT_CALL(*cdm, AddKey(cdmSessionId,
                          ElementsAreArray(responseRaw, kResponseSize), _))
      .WillOnce(DoAll(SetArgPointee<2>(cdmKeySetId),
                      Return(wvcdm::KEY_ADDED)));

  EXPECT_CALL(*cdm, AddKey("", ElementsAreArray(responseRaw, kResponseSize),
                          Pointee(cdmKeySetId)))
      .Times(1);

  status_t res = plugin.provideKeyResponse(sessionId, response, keySetId);
  ASSERT_EQ(OK, res);
  ASSERT_THAT(keySetId, ElementsAreArray(keySetIdRaw, kKeySetIdSize));

  res = plugin.provideKeyResponse(keySetId, response, emptyKeySetId);
  ASSERT_EQ(OK, res);
  EXPECT_EQ(0u, emptyKeySetId.size());
}

TEST_F(WVDrmPluginTest, HandlesPrivacyCertCaseOfAddKey) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  WVDrmPlugin plugin(cdm.get(), &crypto);

  sp<StrictMock<MockDrmPluginListener> > listener =
      new StrictMock<MockDrmPluginListener>();

  const CdmClientPropertySet* propertySet = NULL;

  // Provide expected behavior in response to OpenSession and store the
  // property set
  EXPECT_CALL(*cdm, OpenSession(_, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<4>(cdmSessionId),
                            SaveArg<1>(&propertySet),
                            Return(wvcdm::NO_ERROR)));

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

  Vector<uint8_t> response;
  response.appendArray(responseRaw, kResponseSize);
  Vector<uint8_t> keySetId;

  EXPECT_CALL(*listener, sendEvent(DrmPlugin::kDrmPluginEventKeyNeeded, 0,
                                   Pointee(ElementsAreArray(sessionIdRaw,
                                                            kSessionIdSize)),
                                   NULL))
      .Times(1);

  EXPECT_CALL(*cdm, AddKey(_, _, _))
      .WillRepeatedly(Return(wvcdm::NEED_KEY));

  plugin.openSession(sessionId);
  ASSERT_THAT(propertySet, NotNull());

  status_t res = plugin.setListener(listener);
  ASSERT_EQ(OK, res);

  res = plugin.setPropertyString(String8("privacyMode"), String8("enable"));
  ASSERT_EQ(OK, res);
  EXPECT_TRUE(propertySet->use_privacy_mode());

  res = plugin.provideKeyResponse(sessionId, response, keySetId);
  ASSERT_EQ(OK, res);
}

TEST_F(WVDrmPluginTest, RemovesKeys) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  WVDrmPlugin plugin(cdm.get(), &crypto);

  EXPECT_CALL(*cdm, RemoveKeys(cdmSessionId))
      .Times(1);

  status_t res = plugin.removeKeys(sessionId);
  ASSERT_EQ(OK, res);
}

TEST_F(WVDrmPluginTest, RestoresKeys) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  WVDrmPlugin plugin(cdm.get(), &crypto);

  static const size_t kKeySetIdSize = 32;
  uint8_t keySetIdRaw[kKeySetIdSize];
  FILE* fp = fopen("/dev/urandom", "r");
  fread(keySetIdRaw, sizeof(uint8_t), kKeySetIdSize, fp);
  fclose(fp);

  Vector<uint8_t> keySetId;
  keySetId.appendArray(keySetIdRaw, kKeySetIdSize);

  EXPECT_CALL(*cdm, RestoreKey(cdmSessionId,
                              ElementsAreArray(keySetIdRaw, kKeySetIdSize)))
      .Times(1);

  status_t res = plugin.restoreKeys(sessionId, keySetId);
  ASSERT_EQ(OK, res);
}

TEST_F(WVDrmPluginTest, QueriesKeyStatus) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  WVDrmPlugin plugin(cdm.get(), &crypto);

  KeyedVector<String8, String8> expectedLicenseStatus;
  CdmQueryMap cdmLicenseStatus;

  expectedLicenseStatus.add(String8("areTheKeysAllRight"), String8("yes"));
  cdmLicenseStatus["areTheKeysAllRight"] = "yes";
  expectedLicenseStatus.add(String8("isGMockAwesome"), String8("ohhhhhhYeah"));
  cdmLicenseStatus["isGMockAwesome"] = "ohhhhhhYeah";
  expectedLicenseStatus.add(String8("answer"), String8("42"));
  cdmLicenseStatus["answer"] = "42";

  EXPECT_CALL(*cdm, QueryKeyStatus(cdmSessionId, _))
      .WillOnce(DoAll(SetArgPointee<1>(cdmLicenseStatus),
                      Return(wvcdm::NO_ERROR)));

  KeyedVector<String8, String8> licenseStatus;

  status_t res = plugin.queryKeyStatus(sessionId, licenseStatus);

  ASSERT_EQ(OK, res);

  ASSERT_EQ(expectedLicenseStatus.size(), licenseStatus.size());
  for (size_t i = 0; i < expectedLicenseStatus.size(); ++i) {
    const String8& key = expectedLicenseStatus.keyAt(i);
    EXPECT_NE(android::NAME_NOT_FOUND, licenseStatus.indexOfKey(key));
    EXPECT_EQ(expectedLicenseStatus.valueFor(key), licenseStatus.valueFor(key));
  }
}

TEST_F(WVDrmPluginTest, GetsProvisioningRequests) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  WVDrmPlugin plugin(cdm.get(), &crypto);

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
                      Return(wvcdm::NO_ERROR)));

  Vector<uint8_t> request;
  String8 defaultUrl;

  status_t res = plugin.getProvisionRequest(String8(""), String8(""), request,
                                            defaultUrl);

  ASSERT_EQ(OK, res);
  EXPECT_THAT(request, ElementsAreArray(requestRaw, kRequestSize));
  EXPECT_STREQ(kDefaultUrl, defaultUrl.string());
}

TEST_F(WVDrmPluginTest, HandlesProvisioningResponses) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  WVDrmPlugin plugin(cdm.get(), &crypto);

  static const uint32_t kResponseSize = 512;
  uint8_t responseRaw[kResponseSize];
  FILE* fp = fopen("/dev/urandom", "r");
  fread(responseRaw, sizeof(uint8_t), kResponseSize, fp);
  fclose(fp);

  Vector<uint8_t> response;
  response.appendArray(responseRaw, kResponseSize);

  EXPECT_CALL(*cdm, HandleProvisioningResponse(HasOrigin(EMPTY_ORIGIN),
                                               ElementsAreArray(responseRaw,
                                                                kResponseSize),
                                               _, _))
      .Times(1);

  Vector<uint8_t> cert;
  Vector<uint8_t> key;

  status_t res = plugin.provideProvisionResponse(response, cert, key);

  ASSERT_EQ(OK, res);
}

TEST_F(WVDrmPluginTest, UnprovisionsDevice) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  WVDrmPlugin plugin(cdm.get(), &crypto);

  EXPECT_CALL(*cdm, Unprovision(kSecurityLevelL1, HasOrigin(EMPTY_ORIGIN)))
      .Times(1);
  EXPECT_CALL(*cdm, Unprovision(kSecurityLevelL3, HasOrigin(EMPTY_ORIGIN)))
      .Times(1);

  status_t res = plugin.unprovisionDevice();
  ASSERT_EQ(OK, res);
}

TEST_F(WVDrmPluginTest, MuxesUnprovisioningErrors) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  WVDrmPlugin plugin(cdm.get(), &crypto);

  // Tests that both Unprovisions are called even if one fails. Also tests that
  // no matter which fails, the function always propagates the error.
  EXPECT_CALL(*cdm, Unprovision(kSecurityLevelL1, HasOrigin(EMPTY_ORIGIN)))
      .WillOnce(Return(wvcdm::UNKNOWN_ERROR))
      .WillOnce(Return(wvcdm::NO_ERROR))
      .WillOnce(Return(wvcdm::UNKNOWN_ERROR));
  EXPECT_CALL(*cdm, Unprovision(kSecurityLevelL3, HasOrigin(EMPTY_ORIGIN)))
      .WillOnce(Return(wvcdm::NO_ERROR))
      .WillOnce(Return(wvcdm::UNKNOWN_ERROR))
      .WillOnce(Return(wvcdm::UNKNOWN_ERROR));

  status_t res = plugin.unprovisionDevice();
  ASSERT_NE(OK, res);
  res = plugin.unprovisionDevice();
  ASSERT_NE(OK, res);
  res = plugin.unprovisionDevice();
  ASSERT_NE(OK, res);
}

TEST_F(WVDrmPluginTest, UnprovisionsOrigin) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  WVDrmPlugin plugin(cdm.get(), &crypto);

  Vector<uint8_t> cert;
  Vector<uint8_t> key;
  Vector<uint8_t> specialResponse;
  specialResponse.appendArray(kUnprovisionResponse, kUnprovisionResponseSize);

  EXPECT_CALL(*cdm, Unprovision(kSecurityLevelL1, HasOrigin(kOrigin.string())))
      .Times(1);
  EXPECT_CALL(*cdm, Unprovision(kSecurityLevelL3, HasOrigin(kOrigin.string())))
      .Times(1);

  status_t res = plugin.setPropertyString(String8("origin"), kOrigin);
  ASSERT_EQ(OK, res);
  res = plugin.provideProvisionResponse(specialResponse, cert, key);
  EXPECT_EQ(OK, res);
}

TEST_F(WVDrmPluginTest, WillNotUnprovisionWithoutOrigin) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  WVDrmPlugin plugin(cdm.get(), &crypto);

  Vector<uint8_t> cert;
  Vector<uint8_t> key;
  Vector<uint8_t> specialResponse;
  specialResponse.appendArray(kUnprovisionResponse, kUnprovisionResponseSize);

  EXPECT_CALL(*cdm, Unprovision(_, _))
      .Times(0);

  status_t res = plugin.provideProvisionResponse(specialResponse, cert, key);
  EXPECT_NE(OK, res);
}

TEST_F(WVDrmPluginTest, MuxesOriginUnprovisioningErrors) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  WVDrmPlugin plugin(cdm.get(), &crypto);

  Vector<uint8_t> cert;
  Vector<uint8_t> key;
  Vector<uint8_t> specialResponse;
  specialResponse.appendArray(kUnprovisionResponse, kUnprovisionResponseSize);

  // Tests that both Unprovisions are called even if one fails. Also tests that
  // no matter which fails, the function always propagates the error.
  EXPECT_CALL(*cdm, Unprovision(kSecurityLevelL1, HasOrigin(kOrigin.string())))
      .WillOnce(Return(wvcdm::UNKNOWN_ERROR))
      .WillOnce(Return(wvcdm::NO_ERROR))
      .WillOnce(Return(wvcdm::UNKNOWN_ERROR));
  EXPECT_CALL(*cdm, Unprovision(kSecurityLevelL3, HasOrigin(kOrigin.string())))
      .WillOnce(Return(wvcdm::NO_ERROR))
      .WillOnce(Return(wvcdm::UNKNOWN_ERROR))
      .WillOnce(Return(wvcdm::UNKNOWN_ERROR));

  status_t res = plugin.setPropertyString(String8("origin"), kOrigin);
  ASSERT_EQ(OK, res);
  res = plugin.provideProvisionResponse(specialResponse, cert, key);
  EXPECT_NE(OK, res);
  res = plugin.provideProvisionResponse(specialResponse, cert, key);
  EXPECT_NE(OK, res);
  res = plugin.provideProvisionResponse(specialResponse, cert, key);
  EXPECT_NE(OK, res);
}

TEST_F(WVDrmPluginTest, GetsSecureStops) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  WVDrmPlugin plugin(cdm.get(), &crypto);
  const char* app_id = "my_app_id";
  plugin.setPropertyString(String8("appId"), String8(app_id));

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
    cdmStops.push_back(string(stopsRaw[i], stopsRaw[i] + kStopSize));
  }

  EXPECT_CALL(*cdm, GetUsageInfo(StrEq(app_id), _, _))
      .WillOnce(DoAll(SetArgPointee<2>(cdmStops),
                      Return(wvcdm::NO_ERROR)));

  List<Vector<uint8_t> > stops;

  status_t res = plugin.getSecureStops(stops);

  ASSERT_EQ(OK, res);

  List<Vector<uint8_t> >::iterator iter = stops.begin();
  uint32_t rawIter = 0;
  while (rawIter < kStopCount && iter != stops.end()) {
    EXPECT_THAT(*iter, ElementsAreArray(stopsRaw[rawIter], kStopSize));

    ++iter;
    ++rawIter;
  }
  // Assert that both lists are the same length
  EXPECT_EQ(kStopCount, rawIter);
  EXPECT_EQ(stops.end(), iter);
}

TEST_F(WVDrmPluginTest, ReleasesAllSecureStops) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  WVDrmPlugin plugin(cdm.get(), &crypto);

  status_t res = plugin.setPropertyString(String8("appId"), String8(""));
  ASSERT_EQ(OK, res);

  EXPECT_CALL(*cdm, RemoveAllUsageInfo(StrEq(""), _))
      .Times(1);

  res = plugin.releaseAllSecureStops();
  ASSERT_EQ(OK, res);
}

TEST_F(WVDrmPluginTest, ReleasesSecureStops) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  WVDrmPlugin plugin(cdm.get(), &crypto);

  static const uint32_t kMessageSize = 128;
  uint8_t messageRaw[kMessageSize];
  FILE* fp = fopen("/dev/urandom", "r");
  fread(messageRaw, sizeof(uint8_t), kMessageSize, fp);
  fclose(fp);

  Vector<uint8_t> message;
  message.appendArray(messageRaw, kMessageSize);

  EXPECT_CALL(*cdm, ReleaseUsageInfo(ElementsAreArray(messageRaw,
                                                      kMessageSize),
                                     _))
      .Times(1);

  status_t res = plugin.releaseSecureStops(message);

  ASSERT_EQ(OK, res);
}

TEST_F(WVDrmPluginTest, ReturnsExpectedPropertyValues) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  WVDrmPlugin plugin(cdm.get(), &crypto);

  CdmQueryMap l1Map;
  l1Map[QUERY_KEY_SECURITY_LEVEL] = QUERY_VALUE_SECURITY_LEVEL_L1;

  CdmQueryMap l3Map;
  l3Map[QUERY_KEY_SECURITY_LEVEL] = QUERY_VALUE_SECURITY_LEVEL_L3;

  static const string uniqueId = "The Universe";
  static const string systemId = "42";
  static const string provisioningId("Life\0&Everything", 16);
  static const string openSessions = "15";
  static const string maxSessions = "18";
  static const string oemCryptoApiVersion = "10";
  static const string currentSRMVersion = "1";
  static const string cdmVersion = "Infinity Minus 1";

  drm_metrics::WvCdmMetrics expected_metrics;
  std::string serialized_metrics = wvcdm::a2bs_hex(kSerializedMetricsHex);
  ASSERT_TRUE(expected_metrics.ParseFromString(serialized_metrics));

  EXPECT_CALL(*cdm, QueryStatus(_, QUERY_KEY_SECURITY_LEVEL, _))
      .WillOnce(DoAll(SetArgPointee<2>(QUERY_VALUE_SECURITY_LEVEL_L1),
                      Return(wvcdm::NO_ERROR)))
      .WillOnce(DoAll(SetArgPointee<2>(QUERY_VALUE_SECURITY_LEVEL_L3),
                      Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, QueryStatus(_, QUERY_KEY_DEVICE_ID, _))
      .WillOnce(DoAll(SetArgPointee<2>(uniqueId),
                      Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, QueryStatus(_, QUERY_KEY_SYSTEM_ID, _))
      .WillOnce(DoAll(SetArgPointee<2>(systemId),
                      Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, QueryStatus(_, QUERY_KEY_PROVISIONING_ID, _))
      .WillOnce(DoAll(SetArgPointee<2>(provisioningId),
                      Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, QueryStatus(_, QUERY_KEY_NUMBER_OF_OPEN_SESSIONS, _))
      .WillOnce(DoAll(SetArgPointee<2>(openSessions),
                      Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, QueryStatus(_, QUERY_KEY_MAX_NUMBER_OF_SESSIONS, _))
      .WillOnce(DoAll(SetArgPointee<2>(maxSessions),
                      Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, QueryStatus(_, QUERY_KEY_OEMCRYPTO_API_VERSION, _))
      .WillOnce(DoAll(SetArgPointee<2>(oemCryptoApiVersion),
                      Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, QueryStatus(_, QUERY_KEY_SRM_UPDATE_SUPPORT, _))
      .WillOnce(DoAll(SetArgPointee<2>("True"),
                      testing::Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, QueryStatus(_, QUERY_KEY_CURRENT_SRM_VERSION, _))
      .WillOnce(DoAll(SetArgPointee<2>(currentSRMVersion),
                      testing::Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, QueryStatus(_, QUERY_KEY_WVCDM_VERSION, _))
      .WillOnce(DoAll(SetArgPointee<2>(cdmVersion),
                      Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, GetMetrics(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(expected_metrics),
                      testing::Return(wvcdm::NO_ERROR)));

  String8 stringResult;
  Vector<uint8_t> vectorResult;

  status_t res = plugin.getPropertyString(String8("vendor"), stringResult);
  ASSERT_EQ(OK, res);
  EXPECT_STREQ("Google", stringResult.string());

  res = plugin.getPropertyString(String8("version"), stringResult);
  ASSERT_EQ(OK, res);
  EXPECT_STREQ(cdmVersion.c_str(), stringResult.string());

  res = plugin.getPropertyString(String8("description"), stringResult);
  ASSERT_EQ(OK, res);
  EXPECT_STREQ("Widevine CDM", stringResult.string());

  res = plugin.getPropertyString(String8("algorithms"), stringResult);
  ASSERT_EQ(OK, res);
  EXPECT_STREQ("AES/CBC/NoPadding,HmacSHA256", stringResult.string());

  res = plugin.getPropertyString(String8("securityLevel"), stringResult);
  ASSERT_EQ(OK, res);
  EXPECT_STREQ(QUERY_VALUE_SECURITY_LEVEL_L1.c_str(), stringResult.string());

  res = plugin.getPropertyString(String8("securityLevel"), stringResult);
  ASSERT_EQ(OK, res);
  EXPECT_STREQ(QUERY_VALUE_SECURITY_LEVEL_L3.c_str(), stringResult.string());

  res = plugin.getPropertyByteArray(String8("deviceUniqueId"), vectorResult);
  ASSERT_EQ(OK, res);
  EXPECT_THAT(vectorResult, ElementsAreArray(uniqueId.data(), uniqueId.size()));

  res = plugin.getPropertyString(String8("systemId"), stringResult);
  ASSERT_EQ(OK, res);
  EXPECT_STREQ(systemId.c_str(), stringResult.string());

  res = plugin.getPropertyByteArray(String8("provisioningUniqueId"), vectorResult);
  ASSERT_EQ(OK, res);
  EXPECT_THAT(vectorResult, ElementsAreArray(provisioningId.data(),
                                             provisioningId.size()));

  res = plugin.getPropertyString(String8("numberOfOpenSessions"), stringResult);
  ASSERT_EQ(OK, res);
  EXPECT_EQ(openSessions, stringResult.string());

  res = plugin.getPropertyString(String8("maxNumberOfSessions"), stringResult);
  ASSERT_EQ(OK, res);
  EXPECT_EQ(maxSessions, stringResult.string());

  res = plugin.getPropertyString(String8("oemCryptoApiVersion"), stringResult);
  ASSERT_EQ(OK, res);

  EXPECT_STREQ(oemCryptoApiVersion.c_str(), stringResult.string());

  res = plugin.getPropertyString(String8("SRMUpdateSupport"), stringResult);
  ASSERT_EQ(OK, res);
  EXPECT_STREQ("True", stringResult.string());

  res = plugin.getPropertyString(String8("CurrentSRMVersion"), stringResult);
  ASSERT_EQ(OK, res);
  EXPECT_STREQ(currentSRMVersion.c_str(), stringResult.string());

  vectorResult.clear();
  res = plugin.getPropertyByteArray(String8("metrics"), vectorResult);
  ASSERT_EQ(OK, res);
  EXPECT_THAT(vectorResult, ElementsAreArray(serialized_metrics.data(),
                                             serialized_metrics.size()));
}

TEST_F(WVDrmPluginTest, DoesNotGetUnknownProperties) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  WVDrmPlugin plugin(cdm.get(), &crypto);

  String8 stringResult;
  Vector<uint8_t> vectorResult;

  status_t res = plugin.getPropertyString(String8("unknownProperty"),
                                          stringResult);
  ASSERT_NE(OK, res);
  EXPECT_TRUE(stringResult.isEmpty());

  res = plugin.getPropertyByteArray(String8("unknownProperty"),
                                             vectorResult);
  ASSERT_NE(OK, res);
  EXPECT_TRUE(vectorResult.isEmpty());
}

TEST_F(WVDrmPluginTest, DoesNotSetUnknownProperties) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  WVDrmPlugin plugin(cdm.get(), &crypto);

  static const uint32_t kValueSize = 32;
  uint8_t valueRaw[kValueSize];
  FILE* fp = fopen("/dev/urandom", "r");
  fread(valueRaw, sizeof(uint8_t), kValueSize, fp);
  fclose(fp);

  Vector<uint8_t> value;
  value.appendArray(valueRaw, kValueSize);

  status_t res = plugin.setPropertyString(String8("unknownProperty"),
                                          String8("ignored"));
  ASSERT_NE(OK, res);

  res = plugin.setPropertyByteArray(String8("unknownProperty"), value);
  ASSERT_NE(OK, res);
}

TEST_F(WVDrmPluginTest, FailsGenericMethodsWithoutAnAlgorithmSet) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  WVDrmPlugin plugin(cdm.get(), &crypto);

  Vector<uint8_t> keyId;
  Vector<uint8_t> input;
  Vector<uint8_t> iv;
  Vector<uint8_t> output;
  bool match;

  // Provide expected behavior to support session creation
  EXPECT_CALL(*cdm, OpenSession(StrEq("com.widevine"), _, _, _, _))
      .Times(AtLeast(1))
      .WillRepeatedly(DoAll(SetArgPointee<4>(cdmSessionId),
                            Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Invoke(setSessionIdOnMap<4>));

  EXPECT_CALL(*cdm, CloseSession(_))
      .Times(AtLeast(0));

  status_t res = plugin.openSession(sessionId);
  ASSERT_EQ(OK, res);

  // Note that we do not set the algorithms.  This should cause these methods
  // to fail.

  res = plugin.encrypt(sessionId, keyId, input, iv, output);
  EXPECT_EQ(NO_INIT, res);

  res = plugin.decrypt(sessionId, keyId, input, iv, output);
  EXPECT_EQ(NO_INIT, res);

  res = plugin.sign(sessionId, keyId, input, output);
  EXPECT_EQ(NO_INIT, res);

  res = plugin.verify(sessionId, keyId, input, output, match);
  EXPECT_EQ(NO_INIT, res);
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
  WVDrmPlugin plugin(cdm.get(), &crypto);

  static const size_t kDataSize = 256;
  uint8_t keyIdRaw[KEY_ID_SIZE];
  uint8_t inputRaw[kDataSize];
  uint8_t ivRaw[KEY_IV_SIZE];

  FILE* fp = fopen("/dev/urandom", "r");
  fread(keyIdRaw, sizeof(uint8_t), KEY_ID_SIZE, fp);
  fread(inputRaw, sizeof(uint8_t), kDataSize, fp);
  fread(ivRaw, sizeof(uint8_t), KEY_IV_SIZE, fp);
  fclose(fp);

  Vector<uint8_t> keyId;
  keyId.appendArray(keyIdRaw, KEY_ID_SIZE);
  Vector<uint8_t> input;
  input.appendArray(inputRaw, kDataSize);
  Vector<uint8_t> iv;
  iv.appendArray(ivRaw, KEY_IV_SIZE);
  Vector<uint8_t> output;

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
                            Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Invoke(setSessionIdOnMap<4>));

  EXPECT_CALL(*cdm, CloseSession(_))
      .Times(AtLeast(0));

  status_t res = plugin.openSession(sessionId);
  ASSERT_EQ(OK, res);

  res = plugin.setCipherAlgorithm(sessionId, String8("AES/CBC/NoPadding"));
  ASSERT_EQ(OK, res);

  res = plugin.encrypt(sessionId, keyId, input, iv, output);
  ASSERT_EQ(OK, res);
}

TEST_F(WVDrmPluginTest, CallsGenericDecrypt) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  WVDrmPlugin plugin(cdm.get(), &crypto);

  static const size_t kDataSize = 256;
  uint8_t keyIdRaw[KEY_ID_SIZE];
  uint8_t inputRaw[kDataSize];
  uint8_t ivRaw[KEY_IV_SIZE];

  FILE* fp = fopen("/dev/urandom", "r");
  fread(keyIdRaw, sizeof(uint8_t), KEY_ID_SIZE, fp);
  fread(inputRaw, sizeof(uint8_t), kDataSize, fp);
  fread(ivRaw, sizeof(uint8_t), KEY_IV_SIZE, fp);
  fclose(fp);

  Vector<uint8_t> keyId;
  keyId.appendArray(keyIdRaw, KEY_ID_SIZE);
  Vector<uint8_t> input;
  input.appendArray(inputRaw, kDataSize);
  Vector<uint8_t> iv;
  iv.appendArray(ivRaw, KEY_IV_SIZE);
  Vector<uint8_t> output;

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
                            Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Invoke(setSessionIdOnMap<4>));

  EXPECT_CALL(*cdm, CloseSession(_))
      .Times(AtLeast(0));

  status_t res = plugin.openSession(sessionId);
  ASSERT_EQ(OK, res);

  res = plugin.setCipherAlgorithm(sessionId, String8("AES/CBC/NoPadding"));
  ASSERT_EQ(OK, res);

  res = plugin.decrypt(sessionId, keyId, input, iv, output);
  ASSERT_EQ(OK, res);
}

TEST_F(WVDrmPluginTest, CallsGenericSign) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  WVDrmPlugin plugin(cdm.get(), &crypto);

  static const size_t kDataSize = 256;
  uint8_t keyIdRaw[KEY_ID_SIZE];
  uint8_t messageRaw[kDataSize];

  FILE* fp = fopen("/dev/urandom", "r");
  fread(keyIdRaw, sizeof(uint8_t), KEY_ID_SIZE, fp);
  fread(messageRaw, sizeof(uint8_t), kDataSize, fp);
  fclose(fp);

  Vector<uint8_t> keyId;
  keyId.appendArray(keyIdRaw, KEY_ID_SIZE);
  Vector<uint8_t> message;
  message.appendArray(messageRaw, kDataSize);
  Vector<uint8_t> signature;

  {
    InSequence calls;

    EXPECT_CALL(crypto, selectKey(4, _, KEY_ID_SIZE))
        .With(Args<1, 2>(ElementsAreArray(keyIdRaw, KEY_ID_SIZE)))
        .Times(1);

    EXPECT_CALL(crypto, sign(4, _, kDataSize, OEMCrypto_HMAC_SHA256, _,
                             Pointee(0)))
        .With(Args<1, 2>(ElementsAreArray(messageRaw, kDataSize)))
        .WillOnce(DoAll(SetArgPointee<5>(64),
                        Return(OEMCrypto_ERROR_SHORT_BUFFER)));

    EXPECT_CALL(crypto, sign(4, _, kDataSize, OEMCrypto_HMAC_SHA256, _,
                             Pointee(64)))
        .With(Args<1, 2>(ElementsAreArray(messageRaw, kDataSize)))
        .Times(1);
  }

  // Provide expected behavior to support session creation
  EXPECT_CALL(*cdm, OpenSession(StrEq("com.widevine"), _, _, _, _))
      .Times(AtLeast(1))
      .WillRepeatedly(DoAll(SetArgPointee<4>(cdmSessionId),
                            Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Invoke(setSessionIdOnMap<4>));

  EXPECT_CALL(*cdm, CloseSession(_))
      .Times(AtLeast(0));

  status_t res = plugin.openSession(sessionId);
  ASSERT_EQ(OK, res);

  res = plugin.setMacAlgorithm(sessionId, String8("HmacSHA256"));
  ASSERT_EQ(OK, res);

  res = plugin.sign(sessionId, keyId, message, signature);
  ASSERT_EQ(OK, res);
}

TEST_F(WVDrmPluginTest, CallsGenericVerify) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  WVDrmPlugin plugin(cdm.get(), &crypto);

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

  Vector<uint8_t> keyId;
  keyId.appendArray(keyIdRaw, KEY_ID_SIZE);
  Vector<uint8_t> message;
  message.appendArray(messageRaw, kDataSize);
  Vector<uint8_t> signature;
  signature.appendArray(signatureRaw, kSignatureSize);
  bool match;

  {
    InSequence calls;

    EXPECT_CALL(crypto, selectKey(4, _, KEY_ID_SIZE))
        .With(Args<1, 2>(ElementsAreArray(keyIdRaw, KEY_ID_SIZE)))
        .Times(1);

    EXPECT_CALL(crypto, verify(4, _, kDataSize, OEMCrypto_HMAC_SHA256, _,
                               kSignatureSize))
        .With(AllOf(Args<1, 2>(ElementsAreArray(messageRaw, kDataSize)),
                    Args<4, 5>(ElementsAreArray(signatureRaw, kSignatureSize))))
        .WillOnce(Return(OEMCrypto_SUCCESS));

    EXPECT_CALL(crypto, selectKey(4, _, KEY_ID_SIZE))
        .With(Args<1, 2>(ElementsAreArray(keyIdRaw, KEY_ID_SIZE)))
        .Times(1);

    EXPECT_CALL(crypto, verify(4, _, kDataSize, OEMCrypto_HMAC_SHA256, _,
                               kSignatureSize))
        .With(AllOf(Args<1, 2>(ElementsAreArray(messageRaw, kDataSize)),
                    Args<4, 5>(ElementsAreArray(signatureRaw, kSignatureSize))))
        .WillOnce(Return(OEMCrypto_ERROR_SIGNATURE_FAILURE));
  }

  // Provide expected behavior to support session creation
  EXPECT_CALL(*cdm, OpenSession(StrEq("com.widevine"), _, _, _, _))
      .Times(AtLeast(1))
      .WillRepeatedly(DoAll(SetArgPointee<4>(cdmSessionId),
                            Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Invoke(setSessionIdOnMap<4>));

  EXPECT_CALL(*cdm, CloseSession(_))
      .Times(AtLeast(0));

  status_t res = plugin.openSession(sessionId);
  ASSERT_EQ(OK, res);

  res = plugin.setMacAlgorithm(sessionId, String8("HmacSHA256"));
  ASSERT_EQ(OK, res);

  res = plugin.verify(sessionId, keyId, message, signature, match);
  ASSERT_EQ(OK, res);
  EXPECT_TRUE(match);

  res = plugin.verify(sessionId, keyId, message, signature, match);
  ASSERT_EQ(OK, res);
  EXPECT_FALSE(match);
}

TEST_F(WVDrmPluginTest, RegistersForEvents) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  WVDrmPlugin plugin(cdm.get(), &crypto);

  // Provide expected behavior to support session creation
  EXPECT_CALL(*cdm, OpenSession(StrEq("com.widevine"), _, _, &plugin, _))
      .Times(AtLeast(1))
      .WillRepeatedly(DoAll(SetArgPointee<4>(cdmSessionId),
                            Return(wvcdm::NO_ERROR)));

  EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Invoke(setSessionIdOnMap<4>));

  EXPECT_CALL(*cdm, CloseSession(_))
      .Times(AtLeast(0));

  status_t res = plugin.openSession(sessionId);
  ASSERT_EQ(OK, res);
}

TEST_F(WVDrmPluginTest, UnregistersForAllEventsOnDestruction) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;

  {
    WVDrmPlugin plugin(cdm.get(), &crypto);

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
                        Return(wvcdm::NO_ERROR)))
        .WillOnce(DoAll(SetArgPointee<4>(cdmSessionId2),
                        Return(wvcdm::NO_ERROR)));

    EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId1, _))
        .WillOnce(Invoke(setSessionIdOnMap<4>));

    EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId2, _))
        .WillOnce(Invoke(setSessionIdOnMap<5>));

    EXPECT_CALL(*cdm, CloseSession(_))
        .Times(AtLeast(0));

    status_t res = plugin.openSession(sessionId);
    ASSERT_EQ(OK, res);

    res = plugin.openSession(sessionId);
    ASSERT_EQ(OK, res);
  }
}

TEST_F(WVDrmPluginTest, MarshalsEvents) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  WVDrmPlugin plugin(cdm.get(), &crypto);

  sp<StrictMock<MockDrmPluginListener> > listener =
      new StrictMock<MockDrmPluginListener>();

  const int64_t kExpiryTimeInSeconds = 123456789012LL;
  const char kKeyId1[] = "Testing Key1 Id ";
  const char kKeyId2[] = "Testing Key2 Id ";
  const char kKeyId3[] = "Testing Key3 Id ";
  const char kKeyId4[] = "Testing Key4 Id ";
  const char kKeyId5[] = "Testing Key5 Id ";

  {
    InSequence calls;

    EXPECT_CALL(*listener,
                sendKeysChange(
                    Pointee(ElementsAreArray(sessionIdRaw, kSessionIdSize)),
                    Pointee(UnorderedElementsAre(AllOf(
                        Field(&DrmPlugin::KeyStatus::mKeyId,
                              ElementsAreArray(kKeyId1, sizeof(kKeyId1) - 1)),
                        Field(&DrmPlugin::KeyStatus::mType,
                              DrmPlugin::kKeyStatusType_Expired)))),
                    false));
    EXPECT_CALL(
        *listener,
        sendEvent(DrmPlugin::kDrmPluginEventKeyExpired, 0,
                  Pointee(ElementsAreArray(sessionIdRaw, kSessionIdSize)),
                  NULL));
    EXPECT_CALL(
        *listener,
        sendEvent(DrmPlugin::kDrmPluginEventKeyNeeded, 0,
                  Pointee(ElementsAreArray(sessionIdRaw, kSessionIdSize)),
                  NULL));
    // Expiry Time in Java API is in milliseconds.
    EXPECT_CALL(*listener,
                sendExpirationUpdate(
                    Pointee(ElementsAreArray(sessionIdRaw, kSessionIdSize)),
                    NEVER_EXPIRES));
    EXPECT_CALL(*listener,
                sendExpirationUpdate(
                    Pointee(ElementsAreArray(sessionIdRaw, kSessionIdSize)),
                    kExpiryTimeInSeconds * 1000));
    EXPECT_CALL(
        *listener,
        sendKeysChange(
            Pointee(ElementsAreArray(sessionIdRaw, kSessionIdSize)),
            Pointee(UnorderedElementsAre(
                AllOf(Field(&DrmPlugin::KeyStatus::mKeyId,
                            ElementsAreArray(kKeyId1, sizeof(kKeyId1) - 1)),
                      Field(&DrmPlugin::KeyStatus::mType,
                            DrmPlugin::kKeyStatusType_Usable)),
                AllOf(Field(&DrmPlugin::KeyStatus::mKeyId,
                            ElementsAreArray(kKeyId2, sizeof(kKeyId2) - 1)),
                      Field(&DrmPlugin::KeyStatus::mType,
                            DrmPlugin::kKeyStatusType_OutputNotAllowed)),
                AllOf(Field(&DrmPlugin::KeyStatus::mKeyId,
                            ElementsAreArray(kKeyId3, sizeof(kKeyId3) - 1)),
                      Field(&DrmPlugin::KeyStatus::mType,
                            DrmPlugin::kKeyStatusType_InternalError)),
                AllOf(Field(&DrmPlugin::KeyStatus::mKeyId,
                            ElementsAreArray(kKeyId4, sizeof(kKeyId4) - 1)),
                      Field(&DrmPlugin::KeyStatus::mType,
                            DrmPlugin::kKeyStatusType_StatusPending)),
                AllOf(Field(&DrmPlugin::KeyStatus::mKeyId,
                            ElementsAreArray(kKeyId5, sizeof(kKeyId5) - 1)),
                      Field(&DrmPlugin::KeyStatus::mType,
                            DrmPlugin::kKeyStatusType_UsableInFuture)))),
            true));
  }

  status_t res = plugin.setListener(listener);
  ASSERT_EQ(OK, res);

  CdmKeyStatusMap cdmKeysStatus;
  cdmKeysStatus[kKeyId1] = kKeyStatusExpired;
  plugin.OnSessionKeysChange(cdmSessionId, cdmKeysStatus, false);

  plugin.OnSessionRenewalNeeded(cdmSessionId);
  plugin.OnExpirationUpdate(cdmSessionId, NEVER_EXPIRES);
  plugin.OnExpirationUpdate(cdmSessionId, kExpiryTimeInSeconds);

  cdmKeysStatus[kKeyId1] = kKeyStatusUsable;
  cdmKeysStatus[kKeyId2] = kKeyStatusOutputNotAllowed;
  cdmKeysStatus[kKeyId3] = kKeyStatusInternalError;
  cdmKeysStatus[kKeyId4] = kKeyStatusPending;
  cdmKeysStatus[kKeyId5] = kKeyStatusUsableInFuture;
  plugin.OnSessionKeysChange(cdmSessionId, cdmKeysStatus, true);
}

TEST_F(WVDrmPluginTest, GeneratesProvisioningNeededEvent) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  WVDrmPlugin plugin(cdm.get(), &crypto);

  sp<StrictMock<MockDrmPluginListener> > listener =
      new StrictMock<MockDrmPluginListener>();

  EXPECT_CALL(*listener, sendEvent(DrmPlugin::kDrmPluginEventProvisionRequired, 0,
                                   Pointee(ElementsAreArray(sessionIdRaw,
                                                            kSessionIdSize)),
                                   NULL))
      .Times(1);

  EXPECT_CALL(*cdm, OpenSession(StrEq("com.widevine"), _, _, _, _))
      .Times(AtLeast(1))
      .WillRepeatedly(DoAll(SetArgPointee<4>(cdmSessionId),
                            Return(wvcdm::NEED_PROVISIONING)));

  EXPECT_CALL(*cdm, CloseSession(_))
      .Times(AtLeast(0));

  status_t res = plugin.setListener(listener);
  ASSERT_EQ(OK, res);

  res = plugin.openSession(sessionId);
  ASSERT_EQ(ERROR_DRM_NOT_PROVISIONED, res);
}

TEST_F(WVDrmPluginTest, ProvidesExpectedDefaultPropertiesToCdm) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  WVDrmPlugin plugin(cdm.get(), &crypto);

  const CdmClientPropertySet* propertySet = NULL;

  // Provide expected mock behavior
  {
    // Provide expected behavior in response to OpenSession and store the
    // property set
    EXPECT_CALL(*cdm, OpenSession(_, _, _, _, _))
        .WillRepeatedly(DoAll(SetArgPointee<4>(cdmSessionId),
                              SaveArg<1>(&propertySet),
                              Return(wvcdm::NO_ERROR)));

    // Provide expected behavior when plugin requests session control info
    EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId, _))
        .WillRepeatedly(Invoke(setSessionIdOnMap<4>));

    EXPECT_CALL(*cdm, CloseSession(_))
        .Times(AtLeast(0));
  }

  plugin.openSession(sessionId);

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
  WVDrmPlugin plugin(cdm.get(), &crypto);

  const CdmClientPropertySet* propertySet = NULL;

  // Provide expected mock behavior
  {
    // Provide expected behavior in response to OpenSession and store the
    // property set
    EXPECT_CALL(*cdm, OpenSession(_, _, _, _, _))
        .WillRepeatedly(DoAll(SetArgPointee<4>(cdmSessionId),
                              SaveArg<1>(&propertySet),
                              Return(wvcdm::NO_ERROR)));

    // Provide expected behavior when plugin queries for the security level
    EXPECT_CALL(*cdm, QueryStatus(_, QUERY_KEY_SECURITY_LEVEL, _))
        .WillRepeatedly(DoAll(SetArgPointee<2>(QUERY_VALUE_SECURITY_LEVEL_L3),
                              Return(wvcdm::NO_ERROR)));

    // Provide expected behavior when plugin requests session control info
    EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId, _))
        .WillRepeatedly(Invoke(setSessionIdOnMap<4>));

    EXPECT_CALL(*cdm, CloseSession(_))
        .Times(AtLeast(0));
  }

  status_t res;

  // Test setting an empty string
  res = plugin.setPropertyString(String8("appId"), String8(""));
  ASSERT_EQ(OK, res);

  // Test setting an application id before a session is opened.
  res = plugin.setPropertyString(String8("appId"), kAppId);
  ASSERT_EQ(OK, res);

  plugin.openSession(sessionId);
  ASSERT_THAT(propertySet, NotNull());

  // Verify application id is set correctly.
  EXPECT_STREQ(kAppId, propertySet->app_id().c_str());

  // Test setting application id while session is opened, this should fail.
  res = plugin.setPropertyString(String8("appId"), kAppId);
  ASSERT_EQ(kErrorSessionIsOpen, res);
}

TEST_P(WVDrmPluginOriginTest, CanSetOrigin) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  WVDrmPlugin plugin(cdm.get(), &crypto);
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
                      Return(wvcdm::NO_ERROR)));

  // Set the properties & run the test
  if (!params.origin.isEmpty()) {
    ASSERT_EQ(OK, plugin.setPropertyString(String8("origin"), params.origin));
  }
  EXPECT_EQ(OK, plugin.openSession(sessionId));
  // Test setting an origin while sessions are opened. This should fail.
  EXPECT_NE(OK, plugin.setPropertyString(String8("origin"), kOrigin));
  EXPECT_EQ(OK, plugin.closeSession(sessionId));
}

INSTANTIATE_TEST_CASE_P(OriginTests, WVDrmPluginOriginTest, Values(
    OriginTestVariant("No Origin", kEmptyString, EMPTY_ORIGIN),
    OriginTestVariant("With an Origin", kOrigin, kOrigin.string())));

TEST_F(WVDrmPluginTest, CanSetSecurityLevel) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  WVDrmPlugin plugin(cdm.get(), &crypto);

  const CdmClientPropertySet* propertySet = NULL;

  EXPECT_CALL(*cdm, QueryStatus(_, QUERY_KEY_SECURITY_LEVEL, _))
      .WillOnce(DoAll(SetArgPointee<2>(QUERY_VALUE_SECURITY_LEVEL_L3),
                      Return(wvcdm::NO_ERROR)))
      .WillOnce(DoAll(SetArgPointee<2>(QUERY_VALUE_SECURITY_LEVEL_L1),
                      Return(wvcdm::NO_ERROR)));

  // Provide expected mock behavior
  {
    // Provide expected behavior in response to OpenSession and store the
    // property set
    EXPECT_CALL(*cdm, OpenSession(_, _, _, _, _))
        .WillRepeatedly(DoAll(SetArgPointee<4>(cdmSessionId),
                              SaveArg<1>(&propertySet),
                              Return(wvcdm::NO_ERROR)));

    // Provide expected behavior when plugin requests session control info
    EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId, _))
        .WillRepeatedly(Invoke(setSessionIdOnMap<4>));

    EXPECT_CALL(*cdm, CloseSession(_))
        .Times(AtLeast(0));
  }

  status_t res;

  // Test forcing L3
  res = plugin.setPropertyString(String8("securityLevel"), String8("L3"));
  ASSERT_EQ(OK, res);

  plugin.openSession(sessionId);
  ASSERT_THAT(propertySet, NotNull());
  EXPECT_STREQ("L3", propertySet->security_level().c_str());
  plugin.closeSession(sessionId);

  // Test returning to L1 on an L3 device (Should Fail)
  res = plugin.setPropertyString(String8("securityLevel"), String8("L1"));
  ASSERT_NE(OK, res);

  plugin.openSession(sessionId);
  ASSERT_THAT(propertySet, NotNull());
  EXPECT_STREQ("L3", propertySet->security_level().c_str());
  plugin.closeSession(sessionId);

  // Test returning to L1 on an L1 device
  res = plugin.setPropertyString(String8("securityLevel"), String8("L1"));
  ASSERT_EQ(OK, res);

  plugin.openSession(sessionId);
  ASSERT_THAT(propertySet, NotNull());
  EXPECT_STREQ("", propertySet->security_level().c_str());
  plugin.closeSession(sessionId);

  // Test un-forcing a level (first forcing to L3 so we have something to reset)
  res = plugin.setPropertyString(String8("securityLevel"), String8("L3"));
  ASSERT_EQ(OK, res);

  plugin.openSession(sessionId);
  ASSERT_THAT(propertySet, NotNull());
  EXPECT_STREQ("L3", propertySet->security_level().c_str());
  plugin.closeSession(sessionId);

  res = plugin.setPropertyString(String8("securityLevel"), String8(""));
  ASSERT_EQ(OK, res);

  plugin.openSession(sessionId);
  ASSERT_THAT(propertySet, NotNull());
  EXPECT_STREQ("", propertySet->security_level().c_str());
  plugin.closeSession(sessionId);

  // Test nonsense (Should Fail)
  res = plugin.setPropertyString(String8("securityLevel"), String8("nonsense"));
  ASSERT_NE(OK, res);

  plugin.openSession(sessionId);
  ASSERT_THAT(propertySet, NotNull());
  EXPECT_STREQ("", propertySet->security_level().c_str());
  plugin.closeSession(sessionId);

  // Test attempting to force a level with a session open (Should Fail)
  plugin.openSession(sessionId);
  res = plugin.setPropertyString(String8("securityLevel"), String8("L3"));
  ASSERT_NE(OK, res);
}

TEST_F(WVDrmPluginTest, CanSetPrivacyMode) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  WVDrmPlugin plugin(cdm.get(), &crypto);

  const CdmClientPropertySet* propertySet = NULL;

  // Provide expected mock behavior
  {
    // Provide expected behavior in response to OpenSession and store the
    // property set
    EXPECT_CALL(*cdm, OpenSession(_, _, _, _, _))
        .WillRepeatedly(DoAll(SetArgPointee<4>(cdmSessionId),
                              SaveArg<1>(&propertySet),
                              Return(wvcdm::NO_ERROR)));

    // Provide expected behavior when plugin requests session control info
    EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId, _))
        .WillRepeatedly(Invoke(setSessionIdOnMap<4>));

    EXPECT_CALL(*cdm, CloseSession(_))
        .Times(AtLeast(0));
  }

  plugin.openSession(sessionId);
  ASSERT_THAT(propertySet, NotNull());

  status_t res;

  // Test turning on privacy mode
  res = plugin.setPropertyString(String8("privacyMode"), String8("enable"));
  ASSERT_EQ(OK, res);
  EXPECT_TRUE(propertySet->use_privacy_mode());

  // Test turning off privacy mode
  res = plugin.setPropertyString(String8("privacyMode"), String8("disable"));
  ASSERT_EQ(OK, res);
  EXPECT_FALSE(propertySet->use_privacy_mode());

  // Test nonsense (Should Fail)
  res = plugin.setPropertyString(String8("privacyMode"), String8("nonsense"));
  ASSERT_NE(OK, res);
}

TEST_F(WVDrmPluginTest, CanSetServiceCertificate) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  WVDrmPlugin plugin(cdm.get(), &crypto);

  const CdmClientPropertySet* propertySet = NULL;

  static const size_t kPrivacyCertSize = 256;
  uint8_t privacyCertRaw[kPrivacyCertSize];

  FILE* fp = fopen("/dev/urandom", "r");
  fread(privacyCertRaw, sizeof(uint8_t), kPrivacyCertSize, fp);
  fclose(fp);

  Vector<uint8_t> privacyCert;
  privacyCert.appendArray(privacyCertRaw, kPrivacyCertSize);
  std::string strPrivacyCert(reinterpret_cast<char*>(privacyCertRaw),
                             kPrivacyCertSize);
  Vector<uint8_t> emptyVector;

  // Provide expected mock behavior
  {
    // Provide expected behavior in response to OpenSession and store the
    // property set
    EXPECT_CALL(*cdm, OpenSession(_, _, _, _, _))
        .WillRepeatedly(DoAll(SetArgPointee<4>(cdmSessionId),
                              SaveArg<1>(&propertySet),
                              Return(wvcdm::NO_ERROR)));

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
      .WillOnce(Return(true))
      .WillOnce(Return(false));

  plugin.openSession(sessionId);
  ASSERT_THAT(propertySet, NotNull());

  status_t res;

  // Test setting a certificate
  res = plugin.setPropertyByteArray(String8("serviceCertificate"), privacyCert);
  ASSERT_EQ(OK, res);
  EXPECT_THAT(propertySet->service_certificate(),
              ElementsAreArray(privacyCertRaw, kPrivacyCertSize));

  // Test clearing a certificate
  res = plugin.setPropertyByteArray(String8("serviceCertificate"), emptyVector);
  ASSERT_EQ(OK, res);
  EXPECT_EQ(0u, propertySet->service_certificate().size());

  // Test setting a certificate and having it fail
  res = plugin.setPropertyByteArray(String8("serviceCertificate"), privacyCert);
  ASSERT_NE(OK, res);
  EXPECT_EQ(0u, propertySet->service_certificate().size());
}

TEST_F(WVDrmPluginTest, CanSetSessionSharing) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  WVDrmPlugin plugin(cdm.get(), &crypto);

  const CdmClientPropertySet* propertySet = NULL;

  // Provide expected mock behavior
  {
    // Provide expected behavior in response to OpenSession and store the
    // property set
    EXPECT_CALL(*cdm, OpenSession(_, _, _, _, _))
        .WillRepeatedly(DoAll(SetArgPointee<4>(cdmSessionId),
                              SaveArg<1>(&propertySet),
                              Return(wvcdm::NO_ERROR)));

    // Provide expected behavior when plugin requests session control info
    EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId, _))
        .WillRepeatedly(Invoke(setSessionIdOnMap<4>));

    EXPECT_CALL(*cdm, CloseSession(_))
        .Times(AtLeast(0));
  }

  status_t res;

  // Test turning on session sharing
  res = plugin.setPropertyString(String8("sessionSharing"), String8("enable"));
  ASSERT_EQ(OK, res);

  plugin.openSession(sessionId);
  ASSERT_THAT(propertySet, NotNull());
  EXPECT_TRUE(propertySet->is_session_sharing_enabled());
  plugin.closeSession(sessionId);

  // Test turning off session sharing
  res = plugin.setPropertyString(String8("sessionSharing"), String8("disable"));
  ASSERT_EQ(OK, res);

  plugin.openSession(sessionId);
  ASSERT_THAT(propertySet, NotNull());
  EXPECT_FALSE(propertySet->is_session_sharing_enabled());
  plugin.closeSession(sessionId);

  // Test nonsense (Should Fail)
  res = plugin.setPropertyString(String8("sessionSharing"), String8("nonsense"));
  ASSERT_NE(OK, res);

  // Test changing sharing with a session open (Should Fail)
  plugin.openSession(sessionId);
  res = plugin.setPropertyString(String8("sessionSharing"), String8("enable"));
  ASSERT_NE(OK, res);
}

TEST_F(WVDrmPluginTest, AllowsStoringOfSessionSharingId) {
  android::sp<StrictMock<MockCDM>> cdm = new StrictMock<MockCDM>();
  StrictMock<MockCrypto> crypto;
  WVDrmPlugin plugin(cdm.get(), &crypto);

  CdmClientPropertySet* propertySet = NULL;

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
                              Return(wvcdm::NO_ERROR)));

    // Provide expected behavior when plugin requests session control info
    EXPECT_CALL(*cdm, QueryOemCryptoSessionId(cdmSessionId, _))
        .WillRepeatedly(Invoke(setSessionIdOnMap<4>));

    EXPECT_CALL(*cdm, CloseSession(_))
        .Times(AtLeast(0));
  }

  plugin.openSession(sessionId);

  ASSERT_THAT(propertySet, NotNull());
  propertySet->set_session_sharing_id(sharingId);
  EXPECT_EQ(sharingId, propertySet->session_sharing_id());
}
