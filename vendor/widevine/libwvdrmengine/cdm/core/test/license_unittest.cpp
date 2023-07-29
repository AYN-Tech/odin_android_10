// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

#include "clock.h"
#include "crypto_key.h"
#include "crypto_session.h"
#include "initialization_data.h"
#include "license.h"
#include "policy_engine.h"
#include "properties.h"
#include "service_certificate.h"
#include "string_conversions.h"
#include "test_base.h"
#include "test_printers.h"
#include "wv_cdm_constants.h"

namespace wvcdm {

namespace {

const std::string kEmptyString;
const std::string kAesKey = a2bs_hex("000102030405060708090a0b0c0d0e0f");
const std::string kAesIv = a2bs_hex("000102030405060708090a0b0c0d0e0f");
const std::string kCencInitDataHdr = a2bs_hex(
    "00000042"                          // blob size
    "70737368"                          // "pssh"
    "00000000"                          // flags
    "edef8ba979d64acea3c827dcd51d21ed"  // Widevine system id
    "00000022");                        // pssh data size
const std::string kCencPssh = a2bs_hex(
    "08011a0d7769646576696e655f74657374220f73747265616d696e675f636c697031");
const std::string kCdmSessionId = "sid2";
const std::string kCryptoSessionId = "id2";
const std::string kCryptoRequestId = a2bs_hex(
    "4341444542353737444337393044394330313030303030303030303030303030");
const uint32_t kNonce = 0x49e81305;
const int64_t kLicenseStartTime = 1413517500;  // ~ 01/01/2013
const std::string kEmptyServiceCertificate;
const std::string kInvalidServiceCertificate = "0b";
const std::string kDefaultServiceCertificate = a2bs_hex(
    "0ABF020803121028703454C008F63618ADE7443DB6C4C8188BE7F9900522"
    "8E023082010A0282010100B52112B8D05D023FCC5D95E2C251C1C649B417"
    "7CD8D2BEEF355BB06743DE661E3D2ABC3182B79946D55FDC08DFE9540781"
    "5E9A6274B322A2C7F5E067BB5F0AC07A89D45AEA94B2516F075B66EF811D"
    "0D26E1B9A6B894F2B9857962AA171C4F66630D3E4C602718897F5E1EF9B6"
    "AAF5AD4DBA2A7E14176DF134A1D3185B5A218AC05A4C41F081EFFF80A3A0"
    "40C50B09BBC740EEDCD8F14D675A91980F92CA7DDC646A06ADAD5101F74A"
    "0E498CC01F00532BAC217850BD905E90923656B7DFEFEF42486767F33EF6"
    "283D4F4254AB72589390BEE55808F1D668080D45D893C2BCA2F74D60A0C0"
    "D0A0993CEF01604703334C3638139486BC9DAF24FD67A07F9AD943020301"
    "00013A1273746167696E672E676F6F676C652E636F6D128003983E303526"
    "75F40BA715FC249BDAE5D4AC7249A2666521E43655739529721FF880E0AA"
    "EFC5E27BC980DAEADABF3FC386D084A02C82537848CC753FF497B011A7DA"
    "97788A00E2AA6B84CD7D71C07A48EBF61602CCA5A3F32030A7295C30DA91"
    "5B91DC18B9BC9593B8DE8BB50F0DEDC12938B8E9E039CDDE18FA82E81BB0"
    "32630FE955D85A566CE154300BF6D4C1BD126966356B287D657B18CE63D0"
    "EFD45FC5269E97EAB11CB563E55643B26FF49F109C2101AFCAF35B832F28"
    "8F0D9D45960E259E85FB5D24DBD2CF82764C5DD9BF727EFBE9C861F86932"
    "1F6ADE18905F4D92F9A6DA6536DB8475871D168E870BB2303CF70C6E9784"
    "C93D2DE845AD8262BE7E0D4E2E4A0759CEF82D109D2592C72429F8C01742"
    "BAE2B3DECADBC33C3E5F4BAF5E16ECB74EADBAFCB7C6705F7A9E3B6F3940"
    "383F9C5116D202A20C9229EE969C2519718303B50D0130C3352E06B014D8"
    "38540F8A0C227C0011E0F5B38E4E298ED2CB301EB4564965F55C5D79757A"
    "250A4EB9C84AB3E6539F6B6FDF56899EA29914");
const std::string kToken = a2bs_hex(
    "0AAE02080212107E0A892DEEB021E7AF696B938BB1D5B1188B85AD9D05228E023082010A02"
    "82010100DBEDF2BFB0EC98213766E65049B9AB176FA4B1FBFBB2A0C96C87D9F2B895E0ED77"
    "93BDA057E6BC3E0CA2348BC6831E03609445CA4D418CB98EAC98FFC87AB2364CE76BA26BEE"
    "CDB0C45BD2A6FE9FD38CC5A1C26303AEEB7E9C3CAFAB0D10E46C07E50BEDAB42BF21F40BD2"
    "E055DB0B455191D6B4CEEB11B3F1AFA42B5C0CE4C96B75A5283C0E3AE049AA7CF86D1C4EF6"
    "6A9088B53BCF320ABC9B98A22C219DC109014EFEA72DA5FF2ED5D655DE7AE06EAC6C6B4191"
    "523B2CD2DC1EBFF5F08B11CFE056F2826C1323F12704EC7EBBC1AF935129E5543804492AF9"
    "23B848F4AF47B4BFB131C39DDDC99DBAEEE0F30AD2ADBBC63E60793D0876E37391008BB4DB"
    "F7020301000128DD22128002A9E571776EA9D22A1BD14248DA88E12FD859989F613360B8D2"
    "DA40AF31CC071C7A138466F0EB745E3FD664C0E1A5E4F01098B8D56C34A0158DF9916D192F"
    "841ADCA17FD630E1C0CBE652CAC6A52B6A1581BE4029CE6FAE0E04D2D2C7861187AF8299D8"
    "3E008DB9A2789672CA1DED903773D7E82B234CE2C799EB73CF80600C08F17EEDDDF369D2B8"
    "4A08292F22D1F18FE89521905E713BA674F2217881DBD7711B8C48D5FDCE6FAB51F935E293"
    "CB29191AB012B115FD2F5F23164B063D0A929C3E254BF0F4FA60051EB6B3498ED99FF77C19"
    "68E8CD83A35CEB054D05433FD0EA6AAE43C87DDA377591D1DCC1831EE130BFFF6D139A5ADA"
    "738B0F257CCE2649E71AB4050AAE020801121017DCBC27D11341D497135442A188DAA6188F"
    "89809105228E023082010A0282010100D21ADD7549D2748B3494526A9C3FB86C79376BBE8C"
    "8856F601B8D10461F77ACC7331B10DEBF365120056CDB5662D25907B74F12382F0F4A0CA47"
    "5EEA9562815C6228F6F698ADA27879D8890F2A2D96A746DDEF5316301C003519C2A2250354"
    "674169FDDA41CE14D3C52BEA7A20384515012D5952B38AA19E15E8563CC7AAA81C2122880A"
    "A370A64FEA23C53FB83AC3DB5753214730A349E07F64BF32BE7EAD30D02612AF110BB44FB0"
    "8E1D308173B327EF64D40C41639542B2D1A73C98A6607EC6C683B513A58470514106EF87AE"
    "1E7B9C695B93A104DF7437BFC4167789748A43ED208F2C1FA710793C688885EAE732A8BFDF"
    "5B423B23D75B88FC0ADC8FBDB5020301000128DD2212800372D2FB88098BA3B85B6B4354E0"
    "3767DBE2D7724663FB0A62ABF7704EA910E01F221349EE16D0152C769384050CE78520668C"
    "06CCFD3D789AF3EB69FF163615CD609169FDBE2E15A029D34AD2605625BC81844C9D1E2CE0"
    "519039F3799ADAEF86641E20B033DC16DF2E5B9A1A2A417B8BB3B7A4D9AD1A99367448587D"
    "A13DDE05A3ED9D62FA42078973B4AA40263D7BFA23F1072E94CDF323FA45F78408823E55C4"
    "F4C5C723819CF44CE6D98E50C04EC24D93B1AAB8877B9108B9CA391308E1A3645EBB0E7CAC"
    "BB40B5451560ED799421873BFB5ABB917FA60DB9C77CB8606AF7E3142626F5EA40E5CB8AA0"
    "89D8E7D6A9361935C426A4450EA8BC2E57290D3BF0A0962991D2A91B752FC80C3E7E4E5503"
    "3D71C94B325307A68815F026448F56A2741CEBEFC18E8C142F5F62BFAA67A291517DDE982D"
    "8CD5A9DF6E3D3A99B806F6D60991358C5BE77117D4F3168F3348E9A048539F892F4D783152"
    "C7A8095224AA56B78C5CF7BD1AB1B179C0C0D11E3C3BAC84C141A00191321E3ACC17242E68"
    "3C");
const std::string kLicenseRequestSignature = a2bs_hex(
    "4A560ACFED04787BE0D29D7396234FA2E11D6DD0B22F87FD77AEAEDAA6C8FE54AD9859AE4E"
    "C9F12BCB947892D906DAEC1AD78CABD6F9D479CCF91AF5587DB6FC29CBEBF9C338BAF17790"
    "90980B1F3333BC901CDBF877490C7B85DB2BF9BC559C98450C6F1E8B2E192959F59CC53BD4"
    "85F2CC87D87C324750D5A8E28B821A3C55ABF27305AE4C58474D16E4FEE499D87A7D10AC84"
    "8D24103EB15C63C227A0D57A9D90F5A409D2D55147EE10A35AE291D2D725C7F161FF827221"
    "9AE18B91516E0CDD0B581590DDDEA2A2527E2C9ABA273629B586A9D22D451A827E332CFC3E"
    "9BEDB6CF3D8713F9E11675DF1F5DB9038DBBECAB9D1683F8722CAF6E18EC8C04AEE5");

const CryptoSession::SupportedCertificateTypes kDefaultSupportedCertTypes = {
    true,
    true,
    true
  };

const std::string kFakeEntitlementKeyId =
    a2bs_hex("2a538231c616c67143032a645f9c545d");
const std::string kFakeEntitledKeyId =
    a2bs_hex("f93c7a81e62d4e9a988ff20bca60f52d");
const std::string kFakeUnpaddedKey =
    a2bs_hex("b2047e7fab08b3a4dac76b7b82e8cd4d");
const std::string kFakePaddedKey =
    a2bs_hex("42f804e9ce0fa693692e1c4ffaeb0e14"
             "10101010101010101010101010101010");
const std::string kFakeKeyTooLong =
    a2bs_hex("d4bc8605d662878a46adb2adb6bf3c0b30a54a0c2f");
const std::string kFakeKeyTooShort = a2bs_hex("06e247e7f924208011");
const std::string kFakeIv = a2bs_hex("3d515a3ee0be1687080ac59da9e0d69a");

class MockCryptoSession : public TestCryptoSession {
 public:
  MockCryptoSession(metrics::CryptoMetrics* crypto_metrics)
      : TestCryptoSession(crypto_metrics) { }
  MOCK_METHOD0(IsOpen, bool());
  MOCK_METHOD0(request_id, const std::string&());
  MOCK_METHOD1(UsageInformationSupport, bool(bool*));
  MOCK_METHOD2(GetHdcpCapabilities,
               CdmResponseType(HdcpCapability*, HdcpCapability*));
  MOCK_METHOD1(GetSupportedCertificateTypes, bool(SupportedCertificateTypes*));
  MOCK_METHOD1(GetApiVersion, bool(uint32_t*));
  MOCK_METHOD1(GenerateNonce, CdmResponseType(uint32_t*));
  MOCK_METHOD3(PrepareRequest,
               CdmResponseType(const std::string&, bool, std::string*));
  MOCK_METHOD1(LoadEntitledContentKeys,
               CdmResponseType(const std::vector<CryptoKey>& key_array));
  MOCK_METHOD1(GetResourceRatingTier, bool(uint32_t*));
};

class MockPolicyEngine : public PolicyEngine {
 public:
  MockPolicyEngine(CryptoSession* crypto)
      : PolicyEngine("mock_session_id", NULL, crypto) {}
  MOCK_METHOD1(SetEntitledLicenseKeys,
      void(const std::vector<video_widevine::WidevinePsshData_EntitledKey>&));
};

class MockClock : public Clock {
 public:
  MOCK_METHOD0(GetCurrentTime, int64_t());
};

class MockInitializationData : public InitializationData {
 public:
  MockInitializationData(const std::string& type, const CdmInitData& data)
      : InitializationData(type, data) {}
  MOCK_METHOD0(is_supported, bool());
  MOCK_METHOD0(is_cenc, bool());
};

}  // namespace

// Protobuf generated classes
using video_widevine::LicenseRequest_ContentIdentification;
using video_widevine::ClientIdentification;
using video_widevine::License;
using video_widevine::License_KeyContainer;
using video_widevine::LicenseRequest;
using video_widevine::SignedMessage;
using video_widevine::WidevinePsshData_EntitledKey;

// gmock methods
using ::testing::_;
using ::testing::Eq;
using ::testing::NotNull;
using ::testing::PrintToStringParamName;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SetArgPointee;
using ::testing::UnorderedElementsAre;
using ::testing::Values;

class CdmLicenseTestPeer : public CdmLicense {
 public:
  CdmLicenseTestPeer(const CdmSessionId& session_id, Clock* clock)
      : CdmLicense(session_id, clock) {}

  using CdmLicense::HandleNewEntitledKeys;

  void set_entitlement_keys(License license) {
    entitlement_keys_.CopyFrom(license.key());
  }
};

class CdmLicenseTest : public WvCdmTestBase {
 protected:
  CdmLicenseTest(const std::string& pssh = (kCencInitDataHdr + kCencPssh))
      : pssh_(pssh) {}
  void SetUp() override {
    WvCdmTestBase::SetUp();
    clock_ = new MockClock();
    crypto_session_ = new MockCryptoSession(&crypto_metrics_);
    init_data_ = new MockInitializationData(CENC_INIT_DATA_FORMAT, pssh_);
    policy_engine_ = new MockPolicyEngine(crypto_session_);

    ON_CALL(*crypto_session_, GetSupportedCertificateTypes(NotNull()))
        .WillByDefault(
            DoAll(SetArgPointee<0>(kDefaultSupportedCertTypes), Return(true)));
  }

  void TearDown() override {
    if (cdm_license_) delete cdm_license_;
    if (policy_engine_) delete policy_engine_;
    if (init_data_) delete init_data_;
    if (crypto_session_) delete crypto_session_;
    if (clock_) delete clock_;
  }

  virtual void CreateCdmLicense() {
    cdm_license_ = new CdmLicenseTestPeer(kCdmSessionId, clock_);
    clock_ = NULL;
  }

  CdmLicenseTestPeer* cdm_license_;
  MockClock* clock_;
  metrics::CryptoMetrics crypto_metrics_;
  MockCryptoSession* crypto_session_;
  MockInitializationData* init_data_;
  MockPolicyEngine* policy_engine_;
  std::string pssh_;
};

TEST_F(CdmLicenseTest, InitSuccess) {
  EXPECT_CALL(*crypto_session_, IsOpen()).WillOnce(Return(true));

  CreateCdmLicense();
  EXPECT_TRUE(cdm_license_->Init(kToken, kClientTokenDrmCert, kEmptyString,
                                 false, kEmptyServiceCertificate,
                                 crypto_session_, policy_engine_));
}

TEST_F(CdmLicenseTest, InitFail_EmptyToken) {
  CreateCdmLicense();
  EXPECT_FALSE(cdm_license_->Init("", kClientTokenDrmCert, "", false,
                                  kEmptyServiceCertificate, crypto_session_,
                                  policy_engine_));
}

TEST_F(CdmLicenseTest, InitFail_CryptoSessionNull) {
  CreateCdmLicense();
  EXPECT_FALSE(cdm_license_->Init(kToken, kClientTokenDrmCert, "", false,
                                  kEmptyServiceCertificate, NULL,
                                  policy_engine_));
}

TEST_F(CdmLicenseTest, InitFail_PolicyEngineNull) {
  EXPECT_CALL(*crypto_session_, IsOpen()).WillOnce(Return(true));

  CreateCdmLicense();
  EXPECT_FALSE(cdm_license_->Init(kToken, kClientTokenDrmCert, "", false,
                                  kEmptyServiceCertificate, crypto_session_,
                                  NULL));
}

TEST_F(CdmLicenseTest, InitWithEmptyServiceCert) {
  EXPECT_CALL(*crypto_session_, IsOpen()).WillOnce(Return(true));

  CreateCdmLicense();
  EXPECT_EQ(cdm_license_->Init(kToken, kClientTokenDrmCert, "", true,
                               kEmptyServiceCertificate, crypto_session_,
                               policy_engine_),
            Properties::allow_service_certificate_requests());
}

TEST_F(CdmLicenseTest, InitWithInvalidServiceCert) {
  EXPECT_CALL(*crypto_session_, IsOpen()).WillOnce(Return(true));

  CreateCdmLicense();
  EXPECT_FALSE(cdm_license_->Init(kToken, kClientTokenDrmCert, "", true,
                                  kInvalidServiceCertificate, crypto_session_,
                                  policy_engine_));
}

TEST_F(CdmLicenseTest, InitWithServiceCert) {
  EXPECT_CALL(*crypto_session_, IsOpen()).WillOnce(Return(true));

  CreateCdmLicense();
  EXPECT_TRUE(cdm_license_->Init(kToken, kClientTokenDrmCert, "", true,
                                 kDefaultServiceCertificate, crypto_session_,
                                 policy_engine_));
}

TEST_F(CdmLicenseTest, PrepareKeyRequestValidation) {
  bool usage_information_support = true;
  CryptoSession::HdcpCapability current_hdcp_version = HDCP_NO_DIGITAL_OUTPUT;
  CryptoSession::HdcpCapability max_hdcp_version = HDCP_V2_1;
  uint32_t crypto_session_api_version = 9;

  EXPECT_CALL(*crypto_session_, IsOpen())
      .WillOnce(Return(true));
  EXPECT_CALL(*crypto_session_, request_id())
      .WillOnce(ReturnRef(kCryptoRequestId));
  EXPECT_CALL(*crypto_session_, UsageInformationSupport(NotNull()))
      .WillOnce(
          DoAll(SetArgPointee<0>(usage_information_support), Return(true)));
  EXPECT_CALL(*crypto_session_, GetHdcpCapabilities(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgPointee<0>(current_hdcp_version),
                      SetArgPointee<1>(max_hdcp_version), Return(NO_ERROR)));
  EXPECT_CALL(*crypto_session_, GetSupportedCertificateTypes(NotNull()));
  EXPECT_CALL(*crypto_session_, GetApiVersion(NotNull()))
      .Times(2)
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(crypto_session_api_version), Return(true)));
  EXPECT_CALL(*clock_, GetCurrentTime()).WillOnce(Return(kLicenseStartTime));
  EXPECT_CALL(*crypto_session_, GenerateNonce(NotNull()))
      .WillOnce(DoAll(SetArgPointee<0>(kNonce), Return(NO_ERROR)));
  EXPECT_CALL(*crypto_session_, PrepareRequest(_, Eq(false), NotNull()))
      .WillOnce(
          DoAll(SetArgPointee<2>(kLicenseRequestSignature), Return(NO_ERROR)));

  CreateCdmLicense();
  EXPECT_TRUE(cdm_license_->Init(
      kToken, kClientTokenDrmCert, kEmptyString, true,
      kDefaultServiceCertificate, crypto_session_, policy_engine_));

  CdmAppParameterMap app_parameters;
  CdmKeyMessage signed_request;
  std::string server_url;
  EXPECT_EQ(cdm_license_->PrepareKeyRequest(
      *init_data_, kLicenseTypeStreaming, app_parameters,
      &signed_request, &server_url), KEY_MESSAGE);

  EXPECT_TRUE(!signed_request.empty());

  // Verify signed message
  SignedMessage signed_message;
  EXPECT_TRUE(signed_message.ParseFromString(signed_request));
  EXPECT_EQ(SignedMessage::LICENSE_REQUEST, signed_message.type());
  EXPECT_TRUE(signed_message.has_signature());
  EXPECT_TRUE(std::equal(signed_message.signature().begin(),
                         signed_message.signature().end(),
                         kLicenseRequestSignature.begin()));
  EXPECT_TRUE(!signed_message.msg().empty());

  // Verify license request
  LicenseRequest license_request;
  EXPECT_TRUE(license_request.ParseFromString(signed_message.msg()));

  // Verify Client Identification
  const ClientIdentification& client_id = license_request.client_id();
  EXPECT_EQ(video_widevine::
                ClientIdentification_TokenType_DRM_DEVICE_CERTIFICATE,
            client_id.type());
  EXPECT_TRUE(std::equal(client_id.token().begin(), client_id.token().end(),
                         kToken.begin()));

  EXPECT_LT(0, client_id.client_info_size());
  for (int i = 0; i < client_id.client_info_size(); ++i) {
    const ::video_widevine::ClientIdentification_NameValue&
        name_value = client_id.client_info(i);
    EXPECT_TRUE(!name_value.name().empty());
    EXPECT_TRUE(!name_value.value().empty());
  }

  EXPECT_FALSE(client_id.has_provider_client_token());
  EXPECT_FALSE(client_id.has_license_counter());

  const ::video_widevine::ClientIdentification_ClientCapabilities&
      client_capabilities = client_id.client_capabilities();
  EXPECT_TRUE(client_capabilities.has_client_token());
  EXPECT_TRUE(client_capabilities.has_session_token());
  EXPECT_FALSE(client_capabilities.video_resolution_constraints());
  EXPECT_EQ(video_widevine::
                ClientIdentification_ClientCapabilities_HdcpVersion_HDCP_V2_1,
            client_capabilities.max_hdcp_version());
  EXPECT_EQ(crypto_session_api_version,
            client_capabilities.oem_crypto_api_version());

  EXPECT_THAT(
      client_capabilities.supported_certificate_key_type(),
      UnorderedElementsAre(
          video_widevine::
              ClientIdentification_ClientCapabilities_CertificateKeyType_RSA_2048,
          video_widevine::
              ClientIdentification_ClientCapabilities_CertificateKeyType_RSA_3072));
  EXPECT_FALSE(client_capabilities.has_resource_rating_tier());

  // Verify Content Identification
  const LicenseRequest_ContentIdentification& content_id =
      license_request.content_id();
  ASSERT_TRUE(content_id.has_cenc_id_deprecated());
  EXPECT_FALSE(content_id.has_webm_id_deprecated());
  EXPECT_FALSE(content_id.has_existing_license());

  const ::video_widevine::LicenseRequest_ContentIdentification_CencDeprecated&
      cenc_id = content_id.cenc_id_deprecated();
  EXPECT_TRUE(std::equal(cenc_id.pssh(0).begin(), cenc_id.pssh(0).end(),
                         kCencPssh.begin()));
  EXPECT_EQ(video_widevine::STREAMING, cenc_id.license_type());
  EXPECT_TRUE(std::equal(cenc_id.request_id().begin(),
                         cenc_id.request_id().end(), kCryptoRequestId.begin()));

  // Verify other license request fields
  EXPECT_EQ(::video_widevine::LicenseRequest_RequestType_NEW,
            license_request.type());
  EXPECT_EQ(kLicenseStartTime, license_request.request_time());
  EXPECT_EQ(video_widevine::VERSION_2_1,
            license_request.protocol_version());
  EXPECT_EQ(kNonce, license_request.key_control_nonce());
}

TEST_F(CdmLicenseTest, PrepareKeyRequestValidationV15) {
  bool usage_information_support = true;
  CryptoSession::HdcpCapability current_hdcp_version = HDCP_NO_DIGITAL_OUTPUT;
  CryptoSession::HdcpCapability max_hdcp_version = HDCP_V2_1;
  uint32_t crypto_session_api_version = 15;
  uint32_t resource_rating_tier = RESOURCE_RATING_TIER_LOW;

  EXPECT_CALL(*crypto_session_, IsOpen())
      .WillOnce(Return(true));
  EXPECT_CALL(*crypto_session_, request_id())
      .WillOnce(ReturnRef(kCryptoRequestId));
  EXPECT_CALL(*crypto_session_, UsageInformationSupport(NotNull()))
      .WillOnce(
          DoAll(SetArgPointee<0>(usage_information_support), Return(true)));
  EXPECT_CALL(*crypto_session_, GetHdcpCapabilities(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgPointee<0>(current_hdcp_version),
                      SetArgPointee<1>(max_hdcp_version), Return(NO_ERROR)));
  EXPECT_CALL(*crypto_session_, GetSupportedCertificateTypes(NotNull()));
  EXPECT_CALL(*crypto_session_, GetApiVersion(NotNull()))
      .Times(2)
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(crypto_session_api_version), Return(true)));
  EXPECT_CALL(*crypto_session_, GetResourceRatingTier(NotNull()))
      .WillOnce(
          DoAll(SetArgPointee<0>(resource_rating_tier), Return(true)));
  EXPECT_CALL(*clock_, GetCurrentTime()).WillOnce(Return(kLicenseStartTime));
  EXPECT_CALL(*crypto_session_, GenerateNonce(NotNull()))
      .WillOnce(DoAll(SetArgPointee<0>(kNonce), Return(NO_ERROR)));
  EXPECT_CALL(*crypto_session_, PrepareRequest(_, Eq(false), NotNull()))
      .WillOnce(
          DoAll(SetArgPointee<2>(kLicenseRequestSignature), Return(NO_ERROR)));

  CreateCdmLicense();
  EXPECT_TRUE(cdm_license_->Init(
      kToken, kClientTokenDrmCert, kEmptyString, true,
      kDefaultServiceCertificate, crypto_session_, policy_engine_));

  CdmAppParameterMap app_parameters;
  CdmKeyMessage signed_request;
  std::string server_url;
  EXPECT_EQ(cdm_license_->PrepareKeyRequest(
      *init_data_, kLicenseTypeStreaming, app_parameters,
      &signed_request, &server_url), KEY_MESSAGE);

  EXPECT_TRUE(!signed_request.empty());

  // Verify signed message
  SignedMessage signed_message;
  EXPECT_TRUE(signed_message.ParseFromString(signed_request));
  EXPECT_EQ(SignedMessage::LICENSE_REQUEST, signed_message.type());
  EXPECT_TRUE(signed_message.has_signature());
  EXPECT_TRUE(std::equal(signed_message.signature().begin(),
                         signed_message.signature().end(),
                         kLicenseRequestSignature.begin()));
  EXPECT_TRUE(!signed_message.msg().empty());

  // Verify license request
  LicenseRequest license_request;
  EXPECT_TRUE(license_request.ParseFromString(signed_message.msg()));

  // Verify Client Identification
  const ClientIdentification& client_id = license_request.client_id();
  EXPECT_EQ(video_widevine::
                ClientIdentification_TokenType_DRM_DEVICE_CERTIFICATE,
            client_id.type());
  EXPECT_TRUE(std::equal(client_id.token().begin(), client_id.token().end(),
                         kToken.begin()));

  EXPECT_LT(0, client_id.client_info_size());
  for (int i = 0; i < client_id.client_info_size(); ++i) {
    const ::video_widevine::ClientIdentification_NameValue&
        name_value = client_id.client_info(i);
    EXPECT_TRUE(!name_value.name().empty());
    EXPECT_TRUE(!name_value.value().empty());
  }

  EXPECT_FALSE(client_id.has_provider_client_token());
  EXPECT_FALSE(client_id.has_license_counter());

  const ::video_widevine::ClientIdentification_ClientCapabilities&
      client_capabilities = client_id.client_capabilities();
  EXPECT_TRUE(client_capabilities.has_client_token());
  EXPECT_TRUE(client_capabilities.has_session_token());
  EXPECT_FALSE(client_capabilities.video_resolution_constraints());
  EXPECT_EQ(video_widevine::
                ClientIdentification_ClientCapabilities_HdcpVersion_HDCP_V2_1,
            client_capabilities.max_hdcp_version());
  EXPECT_EQ(crypto_session_api_version,
            client_capabilities.oem_crypto_api_version());

  EXPECT_THAT(
      client_capabilities.supported_certificate_key_type(),
      UnorderedElementsAre(
          video_widevine::
              ClientIdentification_ClientCapabilities_CertificateKeyType_RSA_2048,
          video_widevine::
              ClientIdentification_ClientCapabilities_CertificateKeyType_RSA_3072));
  EXPECT_EQ(resource_rating_tier,
            client_capabilities.resource_rating_tier());

  // Verify Content Identification
  const LicenseRequest_ContentIdentification& content_id =
      license_request.content_id();
  ASSERT_TRUE(content_id.has_cenc_id_deprecated());
  EXPECT_FALSE(content_id.has_webm_id_deprecated());
  EXPECT_FALSE(content_id.has_existing_license());

  const ::video_widevine::LicenseRequest_ContentIdentification_CencDeprecated&
      cenc_id = content_id.cenc_id_deprecated();
  EXPECT_TRUE(std::equal(cenc_id.pssh(0).begin(), cenc_id.pssh(0).end(),
                         kCencPssh.begin()));
  EXPECT_EQ(video_widevine::STREAMING, cenc_id.license_type());
  EXPECT_TRUE(std::equal(cenc_id.request_id().begin(),
                         cenc_id.request_id().end(), kCryptoRequestId.begin()));

  // Verify other license request fields
  EXPECT_EQ(::video_widevine::LicenseRequest_RequestType_NEW,
            license_request.type());
  EXPECT_EQ(kLicenseStartTime, license_request.request_time());
  EXPECT_EQ(video_widevine::VERSION_2_1,
            license_request.protocol_version());
  EXPECT_EQ(kNonce, license_request.key_control_nonce());
}

struct EntitledKeyVariant {
  EntitledKeyVariant(const char* name, const std::string& key,
                     bool should_succeed)
      : name(name),
        key(key),
        should_succeed(should_succeed) {}
  const std::string name;
  const std::string key;
  const bool should_succeed;

  friend void PrintTo(const EntitledKeyVariant& self, std::ostream* os) {
    *os << self.name;
  }
};

class CdmLicenseEntitledKeyTest
    : public CdmLicenseTest,
      public ::testing::WithParamInterface<EntitledKeyVariant> {};

TEST_P(CdmLicenseEntitledKeyTest, LoadsEntitledKeys) {
  EntitledKeyVariant variant = GetParam();

  // Set up a known, fake entitlement key
  License entitlement_license;
  License_KeyContainer* entitlement_key = entitlement_license.add_key();
  entitlement_key->set_type(
      video_widevine::License_KeyContainer_KeyType_ENTITLEMENT);
  entitlement_key->set_id(kFakeEntitlementKeyId);

  // Set up a fake entitled key that matches the entitlement key
  std::vector<WidevinePsshData_EntitledKey> entitled_keys(1);
  WidevinePsshData_EntitledKey& padded_key = entitled_keys[0];
  padded_key.set_entitlement_key_id(kFakeEntitlementKeyId);
  padded_key.set_key_id(kFakeEntitledKeyId);
  padded_key.set_key(variant.key);
  padded_key.set_iv(kFakeIv);

  // Set the expected downstream calls
  EXPECT_CALL(*crypto_session_, IsOpen())
      .WillOnce(Return(true));

  if (variant.should_succeed) {
    EXPECT_CALL(*crypto_session_, LoadEntitledContentKeys(_))
      .WillOnce(Return(KEY_ADDED));
    EXPECT_CALL(*policy_engine_, SetEntitledLicenseKeys(_))
      .Times(1);
  } else {
    EXPECT_CALL(*crypto_session_, LoadEntitledContentKeys(_))
      .Times(0);
    EXPECT_CALL(*policy_engine_, SetEntitledLicenseKeys(_))
      .Times(0);
  }

  // Set up the CdmLicense with the mocks and fake entitlement key
  CreateCdmLicense();
  EXPECT_TRUE(cdm_license_->Init(
      kToken, kClientTokenDrmCert, kEmptyString, true,
      kDefaultServiceCertificate, crypto_session_, policy_engine_));
  cdm_license_->set_entitlement_keys(entitlement_license);

  // Call the function under test and check its return value
  CdmResponseType ret = cdm_license_->HandleNewEntitledKeys(entitled_keys);

  if (variant.should_succeed) {
    EXPECT_EQ(KEY_ADDED, ret);
  } else {
    EXPECT_NE(KEY_ADDED, ret);
  }
}

INSTANTIATE_TEST_CASE_P(
    EntitledKeyTests, CdmLicenseEntitledKeyTest,
    Values(
        EntitledKeyVariant("UnpaddedKey", kFakeUnpaddedKey, true),
        EntitledKeyVariant("PaddedKey", kFakePaddedKey, true),
        EntitledKeyVariant("KeyTooLong", kFakeKeyTooLong, true),
        EntitledKeyVariant("KeyTooShort", kFakeKeyTooShort, false)),
    PrintToStringParamName());

}  // namespace wvcdm
