// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#include "service_certificate.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>
#include "properties.h"
#include "string_conversions.h"
#include "wv_cdm_constants.h"
#include "wv_cdm_types.h"

namespace wvcdm {

namespace {

const CdmSessionId kTestSessionId1 = "sid1";
const CdmSessionId kTestSessionId2 = "sid2";
const std::string kAppId = "com.example.test";

const std::string kTestSignedCertificate = a2bs_hex(
    "0AC102080312101705B917CC1204868B06333A2F772A8C1882B4829205228E023082010A02"
    "8201010099ED5B3B327DAB5E24EFC3B62A95B598520AD5BCCB37503E0645B814D876B8DF40"
    "510441AD8CE3ADB11BB88C4E725A5E4A9E0795291D58584023A7E1AF0E38A9127939300861"
    "0B6F158C878C7E21BFFBFEEA77E1019E1E5781E8A45F46263D14E60E8058A8607ADCE04FAC"
    "8457B137A8D67CCDEB33705D983A21FB4EECBD4A10CA47490CA47EAA5D438218DDBAF1CADE"
    "3392F13D6FFB6442FD31E1BF40B0C604D1C4BA4C9520A4BF97EEBD60929AFCEEF55BBAF564"
    "E2D0E76CD7C55C73A082B996120B8359EDCE24707082680D6F67C6D82C4AC5F3134490A74E"
    "EC37AF4B2F010C59E82843E2582F0B6B9F5DB0FC5E6EDF64FBD308B4711BCF1250019C9F5A"
    "0902030100013A146C6963656E73652E7769646576696E652E636F6D128003AE347314B5A8"
    "35297F271388FB7BB8CB5277D249823CDDD1DA30B93339511EB3CCBDEA04B944B927C12134"
    "6EFDBDEAC9D413917E6EC176A10438460A503BC1952B9BA4E4CE0FC4BFC20A9808AAAF4BFC"
    "D19C1DCFCDF574CCAC28D1B410416CF9DE8804301CBDB334CAFCD0D40978423A642E54613D"
    "F0AFCF96CA4A9249D855E42B3A703EF1767F6A9BD36D6BF82BE76BBF0CBA4FDE59D2ABCC76"
    "FEB64247B85C431FBCA52266B619FC36979543FCA9CBBDBBFAFA0E1A55E755A3C7BCE655F9"
    "646F582AB9CF70AA08B979F867F63A0B2B7FDB362C5BC4ECD555D85BCAA9C593C383C857D4"
    "9DAAB77E40B7851DDFD24998808E35B258E75D78EAC0CA16F7047304C20D93EDE4E8FF1C6F"
    "17E6243E3F3DA8FC1709870EC45FBA823A263F0CEFA1F7093B1909928326333705043A29BD"
    "A6F9B4342CC8DF543CB1A1182F7C5FFF33F10490FACA5B25360B76015E9C5A06AB8EE02F00"
    "D2E8D5986104AACC4DD475FD96EE9CE4E326F21B83C7058577B38732CDDABC6A6BED13FB0D"
    "49D38A45EB87A5F4");

class PropertiesTestPeer : public Properties {
 public:
  using Properties::ForceReinit;
};

}  // unnamed namespace

class ServiceCertificateTest : public ::testing::Test {
 protected:
  ServiceCertificate service_certificate_;
};

class StubCdmClientPropertySet : public CdmClientPropertySet {
 public:
  StubCdmClientPropertySet()
      : security_level_(QUERY_VALUE_SECURITY_LEVEL_L1),
        use_privacy_mode_(false),
        is_session_sharing_enabled_(false),
        session_sharing_id_(0),
        app_id_(kAppId) {}

  const std::string& security_level() const override { return security_level_; }

  bool use_privacy_mode() const override { return use_privacy_mode_; }

  const std::string& service_certificate() const override {
    return raw_service_certificate_;
  }

  void set_service_certificate(const std::string& cert) override {
    raw_service_certificate_ = cert;
  }

  bool is_session_sharing_enabled() const override {
    return is_session_sharing_enabled_;
  }

  uint32_t session_sharing_id() const override { return session_sharing_id_; }

  void set_session_sharing_id(uint32_t id) override {
    session_sharing_id_ = id;
  }

  const std::string& app_id() const override { return app_id_; }

  void enable_privacy_mode() { use_privacy_mode_ = true; }

 private:
  std::string security_level_;
  std::string raw_service_certificate_;
  bool use_privacy_mode_;
  bool is_session_sharing_enabled_;
  uint32_t session_sharing_id_;
  std::string app_id_;
};

TEST_F(ServiceCertificateTest, InitSuccess) {
  service_certificate_.Init(kTestSessionId1);
  EXPECT_FALSE(service_certificate_.has_certificate());
}

TEST_F(ServiceCertificateTest, InitPrivacyModeRequired) {
  StubCdmClientPropertySet property_set;

  property_set.enable_privacy_mode();

  PropertiesTestPeer::ForceReinit();
  ASSERT_TRUE(PropertiesTestPeer::AddSessionPropertySet(kTestSessionId1,
                                                        &property_set));

  service_certificate_.Init(kTestSessionId1);
  EXPECT_FALSE(service_certificate_.has_certificate());
}

TEST_F(ServiceCertificateTest, InitServiceCertificatePresent) {
  StubCdmClientPropertySet property_set;

  property_set.enable_privacy_mode();
  property_set.set_service_certificate(kTestSignedCertificate);

  PropertiesTestPeer::ForceReinit();
  ASSERT_TRUE(PropertiesTestPeer::AddSessionPropertySet(kTestSessionId1,
                                                        &property_set));

  std::string raw_service_certificate;
  EXPECT_TRUE(PropertiesTestPeer::GetServiceCertificate(
      kTestSessionId1, &raw_service_certificate));
  EXPECT_EQ(NO_ERROR, service_certificate_.Init(raw_service_certificate));
  EXPECT_TRUE(service_certificate_.has_certificate());
}

TEST_F(ServiceCertificateTest, SetServiceCertificate) {
  StubCdmClientPropertySet property_set;

  property_set.enable_privacy_mode();

  PropertiesTestPeer::ForceReinit();
  ASSERT_TRUE(PropertiesTestPeer::AddSessionPropertySet(kTestSessionId1,
                                                        &property_set));

  EXPECT_EQ(NO_ERROR, service_certificate_.Init(kTestSignedCertificate));
  EXPECT_TRUE(service_certificate_.has_certificate());
}

}  // namespace wvcdm
