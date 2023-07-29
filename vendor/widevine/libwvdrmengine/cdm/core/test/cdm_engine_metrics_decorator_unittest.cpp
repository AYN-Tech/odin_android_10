// Copyright 2018 Google LLC. All Rights Reserved.  This fil and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

// These tests are for the cdm engine metrics implementation. They ensure that
// The cdm engine metrics impl is correctly forwarding calls to the internal
// implementation and capturing metrics as appropriate.

#include "cdm_engine_metrics_decorator.h"

#include <memory>
#include <string>

#include "cdm_client_property_set.h"
#include "cdm_engine.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "metrics.pb.h"
#include "wv_cdm_event_listener.h"

using ::testing::_;
using ::testing::ByRef;
using ::testing::Eq;
using ::testing::Matcher;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::StrictMock;
using ::wvcdm::metrics::EngineMetrics;

namespace wvcdm {


class MockCdmClientPropertySet : public CdmClientPropertySet {
 public:

  MOCK_CONST_METHOD0(security_level, const std::string&());
  MOCK_CONST_METHOD0(use_privacy_mode, bool());
  MOCK_CONST_METHOD0(service_certificate, const std::string&());
  MOCK_METHOD1(set_service_certificate, void(const std::string&));
  MOCK_CONST_METHOD0(is_session_sharing_enabled, bool());
  MOCK_CONST_METHOD0(session_sharing_id, uint32_t());
  MOCK_METHOD1(set_session_sharing_id, void(uint32_t));
  MOCK_CONST_METHOD0(app_id, const std::string&());
};

class MockWvCdmEventListener : public WvCdmEventListener {
  MOCK_METHOD1(OnSessionRenewalNeeded, void(const CdmSessionId&));
  MOCK_METHOD3(OnSessionKeysChange,
               void(const CdmSessionId&, const CdmKeyStatusMap&,
                    bool has_new_usable_key));
  MOCK_METHOD2(OnExpirationUpdate, void(const CdmSessionId&, int64_t));
};

class MockCdmEngineImpl : public CdmEngine {
 public:
  MockCdmEngineImpl(FileSystem* file_system,
                    std::shared_ptr<EngineMetrics> metrics,
                    const std::string& spoid)
      : CdmEngine(file_system, metrics, spoid) {}
  MOCK_METHOD4(OpenSession, CdmResponseType(
      const CdmKeySystem&, CdmClientPropertySet*,
      const CdmSessionId&, WvCdmEventListener*));
  MOCK_METHOD4(OpenSession, CdmResponseType(
      const CdmKeySystem&, CdmClientPropertySet*,
      WvCdmEventListener*, CdmSessionId*));
  MOCK_METHOD1(CloseSession, CdmResponseType(const CdmSessionId&));
  MOCK_METHOD3(OpenKeySetSession,
               CdmResponseType(const CdmKeySetId&, CdmClientPropertySet*,
                               WvCdmEventListener*));
  MOCK_METHOD6(GenerateKeyRequest,
               CdmResponseType(const CdmSessionId&, const CdmKeySetId&,
                               const InitializationData&, const CdmLicenseType,
                               CdmAppParameterMap&, CdmKeyRequest*));
  MOCK_METHOD4(AddKey, CdmResponseType(
      const CdmSessionId&, const CdmKeyResponse&,
      CdmLicenseType*, CdmKeySetId*));
  MOCK_METHOD2(RestoreKey, CdmResponseType(const CdmSessionId&,
                                           const CdmKeySetId&));
  MOCK_METHOD1(RemoveKeys, CdmResponseType(const CdmSessionId&));
  MOCK_METHOD2(QueryKeyStatus, CdmResponseType(const CdmSessionId&,
                                               CdmQueryMap*));
  MOCK_METHOD5(GetProvisioningRequest, CdmResponseType(
        CdmCertificateType, const std::string&, const std::string&,
        CdmProvisioningRequest*, std::string*));
  MOCK_METHOD3(HandleProvisioningResponse, CdmResponseType(
        const CdmProvisioningResponse&, std::string*, std::string*));
  MOCK_METHOD1(Unprovision, CdmResponseType(CdmSecurityLevel));
  MOCK_METHOD4(ListUsageIds, CdmResponseType(
        const std::string&, CdmSecurityLevel,
        std::vector<std::string>*, std::vector<std::string>*));
  MOCK_METHOD1(RemoveAllUsageInfo, CdmResponseType(const std::string&));
  MOCK_METHOD2(RemoveAllUsageInfo, CdmResponseType(const std::string&,
                                                   CdmSecurityLevel));
  MOCK_METHOD2(RemoveUsageInfo, CdmResponseType(const std::string&,
                                                const CdmSecureStopId&));
  MOCK_METHOD1(ReleaseUsageInfo,
               CdmResponseType(const CdmUsageInfoReleaseMessage&));
  MOCK_METHOD2(Decrypt, CdmResponseType(const CdmSessionId&,
                                        const CdmDecryptionParameters&));
  MOCK_METHOD2(FindSessionForKey, bool(const KeyId&, CdmSessionId*));
};

class WvCdmEngineMetricsImplTest : public ::testing::Test {
 public:
  void SetUp() override {
    file_system_.reset(new FileSystem);
    std::shared_ptr<EngineMetrics> engine_metrics(new EngineMetrics);
    test_cdm_metrics_engine_.reset(
        new CdmEngineMetricsImpl<StrictMock<MockCdmEngineImpl>>(
            file_system_.get(), engine_metrics));

  }

 protected:
  std::unique_ptr<FileSystem> file_system_;
  std::unique_ptr<CdmEngineMetricsImpl<StrictMock<MockCdmEngineImpl>>>
      test_cdm_metrics_engine_;
};

TEST_F(WvCdmEngineMetricsImplTest, OpenSession_Overload1) {
  MockCdmClientPropertySet property_set;
  MockWvCdmEventListener event_listener;

  EXPECT_CALL(*test_cdm_metrics_engine_,
      OpenSession(
          StrEq("foo"), Eq(&property_set),
          Matcher<const CdmSessionId&>(Eq("bar")), Eq(&event_listener)))
      .WillOnce(Return(wvcdm::UNKNOWN_ERROR));

  ASSERT_EQ(wvcdm::UNKNOWN_ERROR,
            test_cdm_metrics_engine_->OpenSession("foo", &property_set, "bar",
                                                  &event_listener));

  drm_metrics::WvCdmMetrics metrics_proto;
  test_cdm_metrics_engine_->GetMetricsSnapshot(&metrics_proto);
  ASSERT_EQ(1, metrics_proto.engine_metrics().cdm_engine_open_session_size());
  EXPECT_EQ(wvcdm::UNKNOWN_ERROR,
            metrics_proto.engine_metrics()
                .cdm_engine_open_session(0).attributes().error_code());
}

TEST_F(WvCdmEngineMetricsImplTest, OpenSession_Overload2) {
  MockCdmClientPropertySet property_set;
  MockWvCdmEventListener event_listener;
  CdmSessionId session_id;

  EXPECT_CALL(*test_cdm_metrics_engine_,
      OpenSession(
          StrEq("foo"), Eq(&property_set),
          Eq(&event_listener), Matcher<CdmSessionId*>(Eq(&session_id))))
      .WillOnce(Return(wvcdm::UNKNOWN_ERROR));

  ASSERT_EQ(wvcdm::UNKNOWN_ERROR,
            test_cdm_metrics_engine_->OpenSession(
                "foo", &property_set, &event_listener, &session_id));

  drm_metrics::WvCdmMetrics metrics_proto;
  test_cdm_metrics_engine_->GetMetricsSnapshot(&metrics_proto);
  ASSERT_EQ(1, metrics_proto.engine_metrics().cdm_engine_open_session_size());
  EXPECT_EQ(wvcdm::UNKNOWN_ERROR,
            metrics_proto.engine_metrics()
                .cdm_engine_open_session(0).attributes().error_code());
}

TEST_F(WvCdmEngineMetricsImplTest, CloseSession) {
  MockCdmClientPropertySet property_set;
  MockWvCdmEventListener event_listener;

  EXPECT_CALL(*test_cdm_metrics_engine_, CloseSession(Eq("bar")))
      .WillOnce(Return(wvcdm::UNKNOWN_ERROR));

  ASSERT_EQ(wvcdm::UNKNOWN_ERROR,
            test_cdm_metrics_engine_->CloseSession("bar"));

  drm_metrics::WvCdmMetrics metrics_proto;
  test_cdm_metrics_engine_->GetMetricsSnapshot(&metrics_proto);
  ASSERT_EQ(1, metrics_proto.engine_metrics().cdm_engine_close_session_size());
  EXPECT_EQ(wvcdm::UNKNOWN_ERROR,
            metrics_proto.engine_metrics()
                .cdm_engine_close_session(0).attributes().error_code());
}

TEST_F(WvCdmEngineMetricsImplTest, OpenKeySetSession) {
  MockCdmClientPropertySet property_set;
  MockWvCdmEventListener event_listener;

  EXPECT_CALL(*test_cdm_metrics_engine_,
      OpenKeySetSession(Eq("bar"), Eq(&property_set), Eq(&event_listener)))
      .WillOnce(Return(wvcdm::UNKNOWN_ERROR));

  ASSERT_EQ(wvcdm::UNKNOWN_ERROR,
            test_cdm_metrics_engine_->OpenKeySetSession("bar", &property_set,
                                                        &event_listener));

  drm_metrics::WvCdmMetrics metrics_proto;
  test_cdm_metrics_engine_->GetMetricsSnapshot(&metrics_proto);
  ASSERT_EQ(
      1, metrics_proto.engine_metrics().cdm_engine_open_key_set_session_size());
  EXPECT_EQ(wvcdm::UNKNOWN_ERROR,
            metrics_proto.engine_metrics()
                .cdm_engine_open_key_set_session(0).attributes().error_code());
}

TEST_F(WvCdmEngineMetricsImplTest, GenerateKeyRequest) {
  InitializationData initialization_data;
  CdmAppParameterMap app_parameters;
  CdmKeyRequest key_request;

  EXPECT_CALL(*test_cdm_metrics_engine_,
      GenerateKeyRequest(Eq("foo"), Eq("bar"), _, Eq(kLicenseTypeStreaming),
                         Eq(app_parameters), Eq(&key_request)))
      .WillOnce(Return(wvcdm::UNKNOWN_ERROR));

  ASSERT_EQ(wvcdm::UNKNOWN_ERROR,
            test_cdm_metrics_engine_->GenerateKeyRequest(
                "foo", "bar", initialization_data,
                kLicenseTypeStreaming, app_parameters, &key_request));

  drm_metrics::WvCdmMetrics metrics_proto;
  test_cdm_metrics_engine_->GetMetricsSnapshot(&metrics_proto);
  ASSERT_EQ(
      1, metrics_proto.engine_metrics()
          .cdm_engine_generate_key_request_time_us_size());
  EXPECT_EQ(wvcdm::UNKNOWN_ERROR,
            metrics_proto.engine_metrics()
                .cdm_engine_generate_key_request_time_us(0)
                .attributes().error_code());
}

TEST_F(WvCdmEngineMetricsImplTest, AddKey) {
  CdmLicenseType license_type;
  CdmKeySetId key_set_id;

  EXPECT_CALL(*test_cdm_metrics_engine_,
      AddKey(Eq("fake session id"), Eq("fake response"), Eq(&license_type),
             Eq(&key_set_id)))
      .WillOnce(Return(wvcdm::UNKNOWN_ERROR));

  ASSERT_EQ(wvcdm::UNKNOWN_ERROR,
            test_cdm_metrics_engine_->AddKey(
                "fake session id", "fake response",
                &license_type, &key_set_id));

  drm_metrics::WvCdmMetrics metrics_proto;
  test_cdm_metrics_engine_->GetMetricsSnapshot(&metrics_proto);
  ASSERT_EQ(
      1, metrics_proto.engine_metrics()
          .cdm_engine_add_key_time_us_size());
  EXPECT_EQ(wvcdm::UNKNOWN_ERROR,
            metrics_proto.engine_metrics()
                .cdm_engine_add_key_time_us(0)
                .attributes().error_code());
}

TEST_F(WvCdmEngineMetricsImplTest, RestoreKey) {
  EXPECT_CALL(*test_cdm_metrics_engine_,
      RestoreKey(Eq("fake session id"), Eq("fake key set id")))
      .WillOnce(Return(wvcdm::UNKNOWN_ERROR));

  ASSERT_EQ(wvcdm::UNKNOWN_ERROR,
            test_cdm_metrics_engine_->RestoreKey(
                "fake session id", "fake key set id"));

  drm_metrics::WvCdmMetrics metrics_proto;
  test_cdm_metrics_engine_->GetMetricsSnapshot(&metrics_proto);
  ASSERT_EQ(
      1, metrics_proto.engine_metrics()
          .cdm_engine_restore_key_time_us_size());
  EXPECT_EQ(wvcdm::UNKNOWN_ERROR,
            metrics_proto.engine_metrics()
                .cdm_engine_restore_key_time_us(0)
                .attributes().error_code());
}

TEST_F(WvCdmEngineMetricsImplTest, RemoveKeys) {
  EXPECT_CALL(*test_cdm_metrics_engine_,
      RemoveKeys(Eq("fake session id")))
      .WillOnce(Return(wvcdm::UNKNOWN_ERROR));

  ASSERT_EQ(wvcdm::UNKNOWN_ERROR,
            test_cdm_metrics_engine_->RemoveKeys("fake session id"));

  drm_metrics::WvCdmMetrics metrics_proto;
  test_cdm_metrics_engine_->GetMetricsSnapshot(&metrics_proto);
  ASSERT_EQ(
      1, metrics_proto.engine_metrics()
          .cdm_engine_remove_keys_size());
  EXPECT_EQ(wvcdm::UNKNOWN_ERROR,
            metrics_proto.engine_metrics()
                .cdm_engine_remove_keys(0)
                .attributes().error_code());
}

TEST_F(WvCdmEngineMetricsImplTest, QueryKeyStatus) {
  CdmQueryMap query_response;
  EXPECT_CALL(*test_cdm_metrics_engine_,
      QueryKeyStatus(Eq("fake session id"), Eq(&query_response)))
      .WillOnce(Return(wvcdm::UNKNOWN_ERROR));

  ASSERT_EQ(wvcdm::UNKNOWN_ERROR,
            test_cdm_metrics_engine_->QueryKeyStatus("fake session id",
                                                     &query_response));

  drm_metrics::WvCdmMetrics metrics_proto;
  test_cdm_metrics_engine_->GetMetricsSnapshot(&metrics_proto);
  ASSERT_EQ(
      1, metrics_proto.engine_metrics()
          .cdm_engine_query_key_status_time_us_size());
  EXPECT_EQ(wvcdm::UNKNOWN_ERROR,
            metrics_proto.engine_metrics()
                .cdm_engine_query_key_status_time_us(0)
                .attributes().error_code());
}

TEST_F(WvCdmEngineMetricsImplTest, GetProvisioningRequest) {
  CdmProvisioningRequest request;
  std::string default_url;

  EXPECT_CALL(*test_cdm_metrics_engine_,
      GetProvisioningRequest(Eq(kCertificateX509),
                             Eq("fake certificate authority"),
                             Eq("fake service certificate"),
                             Eq(&request), Eq(&default_url)))
      .WillOnce(Return(wvcdm::UNKNOWN_ERROR));

  ASSERT_EQ(wvcdm::UNKNOWN_ERROR,
            test_cdm_metrics_engine_->GetProvisioningRequest(
                kCertificateX509, "fake certificate authority",
                "fake service certificate", &request, &default_url));

  drm_metrics::WvCdmMetrics metrics_proto;
  test_cdm_metrics_engine_->GetMetricsSnapshot(&metrics_proto);
  ASSERT_EQ(
      1, metrics_proto.engine_metrics()
          .cdm_engine_get_provisioning_request_time_us_size());
  EXPECT_EQ(wvcdm::UNKNOWN_ERROR,
            metrics_proto.engine_metrics()
                .cdm_engine_get_provisioning_request_time_us(0)
                .attributes().error_code());
}

TEST_F(WvCdmEngineMetricsImplTest, HandleProvisioningResponse) {
  CdmProvisioningResponse response;
  std::string cert;
  std::string wrapped_key;

  EXPECT_CALL(*test_cdm_metrics_engine_,
      HandleProvisioningResponse(Eq("fake provisioning response"),
                                 Eq(&cert), Eq(&wrapped_key)))
      .WillOnce(Return(wvcdm::UNKNOWN_ERROR));

  ASSERT_EQ(wvcdm::UNKNOWN_ERROR,
            test_cdm_metrics_engine_->HandleProvisioningResponse(
                "fake provisioning response", &cert, &wrapped_key));

  drm_metrics::WvCdmMetrics metrics_proto;
  test_cdm_metrics_engine_->GetMetricsSnapshot(&metrics_proto);
  ASSERT_EQ(
      1, metrics_proto.engine_metrics()
          .cdm_engine_handle_provisioning_response_time_us_size());
  EXPECT_EQ(wvcdm::UNKNOWN_ERROR,
            metrics_proto.engine_metrics()
                .cdm_engine_handle_provisioning_response_time_us(0)
                .attributes().error_code());
}

TEST_F(WvCdmEngineMetricsImplTest, Unprovision) {
  EXPECT_CALL(*test_cdm_metrics_engine_,
      Unprovision(Eq(kSecurityLevelL2)))
      .WillOnce(Return(wvcdm::UNKNOWN_ERROR));

  ASSERT_EQ(wvcdm::UNKNOWN_ERROR,
            test_cdm_metrics_engine_->Unprovision(kSecurityLevelL2));

  drm_metrics::WvCdmMetrics metrics_proto;
  test_cdm_metrics_engine_->GetMetricsSnapshot(&metrics_proto);
  ASSERT_EQ(
      1, metrics_proto.engine_metrics()
          .cdm_engine_unprovision_size());
  EXPECT_EQ(wvcdm::UNKNOWN_ERROR,
            metrics_proto.engine_metrics()
                .cdm_engine_unprovision(0)
                .attributes().error_code());
}

TEST_F(WvCdmEngineMetricsImplTest, RemoveAllUsageInfo_Overload1) {
  EXPECT_CALL(*test_cdm_metrics_engine_,
      RemoveAllUsageInfo(Eq("fake app id")))
      .WillOnce(Return(wvcdm::UNKNOWN_ERROR));

  ASSERT_EQ(wvcdm::UNKNOWN_ERROR,
            test_cdm_metrics_engine_->RemoveAllUsageInfo("fake app id"));

  drm_metrics::WvCdmMetrics metrics_proto;
  test_cdm_metrics_engine_->GetMetricsSnapshot(&metrics_proto);
  ASSERT_EQ(
      1, metrics_proto.engine_metrics()
          .cdm_engine_remove_all_usage_info_size());
  EXPECT_EQ(wvcdm::UNKNOWN_ERROR,
            metrics_proto.engine_metrics()
                .cdm_engine_remove_all_usage_info(0)
                .attributes().error_code());
}

TEST_F(WvCdmEngineMetricsImplTest, RemoveAllUsageInfo_Overload2) {
  EXPECT_CALL(*test_cdm_metrics_engine_,
      RemoveAllUsageInfo(Eq("fake app id"), Eq(kSecurityLevelL2)))
      .WillOnce(Return(wvcdm::UNKNOWN_ERROR));

  ASSERT_EQ(wvcdm::UNKNOWN_ERROR,
            test_cdm_metrics_engine_->RemoveAllUsageInfo("fake app id",
                                                         kSecurityLevelL2));

  drm_metrics::WvCdmMetrics metrics_proto;
  test_cdm_metrics_engine_->GetMetricsSnapshot(&metrics_proto);
  ASSERT_EQ(
      1, metrics_proto.engine_metrics()
          .cdm_engine_remove_all_usage_info_size());
  EXPECT_EQ(wvcdm::UNKNOWN_ERROR,
            metrics_proto.engine_metrics()
                .cdm_engine_remove_all_usage_info(0)
                .attributes().error_code());
}


TEST_F(WvCdmEngineMetricsImplTest, RemoveUsageInfo) {
  EXPECT_CALL(*test_cdm_metrics_engine_,
      RemoveUsageInfo(Eq("fake app id"), Eq("fake secure stop id")))
      .WillOnce(Return(wvcdm::UNKNOWN_ERROR));

  ASSERT_EQ(wvcdm::UNKNOWN_ERROR,
            test_cdm_metrics_engine_->RemoveUsageInfo("fake app id",
                                                      "fake secure stop id"));

  drm_metrics::WvCdmMetrics metrics_proto;
  test_cdm_metrics_engine_->GetMetricsSnapshot(&metrics_proto);
  ASSERT_EQ(
      1, metrics_proto.engine_metrics()
          .cdm_engine_remove_usage_info_size());
  EXPECT_EQ(wvcdm::UNKNOWN_ERROR,
            metrics_proto.engine_metrics()
                .cdm_engine_remove_usage_info(0)
                .attributes().error_code());
}

TEST_F(WvCdmEngineMetricsImplTest, ReleaseUsageInfo) {
  EXPECT_CALL(*test_cdm_metrics_engine_,
      ReleaseUsageInfo(Eq("fake release message")))
      .WillOnce(Return(wvcdm::UNKNOWN_ERROR));

  ASSERT_EQ(wvcdm::UNKNOWN_ERROR,
            test_cdm_metrics_engine_->ReleaseUsageInfo("fake release message"));

  drm_metrics::WvCdmMetrics metrics_proto;
  test_cdm_metrics_engine_->GetMetricsSnapshot(&metrics_proto);
  ASSERT_EQ(
      1, metrics_proto.engine_metrics()
          .cdm_engine_release_usage_info_size());
  EXPECT_EQ(wvcdm::UNKNOWN_ERROR,
            metrics_proto.engine_metrics()
                .cdm_engine_release_usage_info(0)
                .attributes().error_code());
}

TEST_F(WvCdmEngineMetricsImplTest, ListUsageIds) {
  std::vector<std::string> ksids;
  std::vector<std::string> provider_session_tokens;

  EXPECT_CALL(*test_cdm_metrics_engine_,
      ListUsageIds(Eq("fake app id"), Eq(kSecurityLevelL2),
                   Eq(&ksids), Eq(&provider_session_tokens)))
      .WillOnce(Return(wvcdm::UNKNOWN_ERROR));

  ASSERT_EQ(wvcdm::UNKNOWN_ERROR,
            test_cdm_metrics_engine_->ListUsageIds(
                "fake app id", kSecurityLevelL2, &ksids,
                &provider_session_tokens));

  drm_metrics::WvCdmMetrics metrics_proto;
  test_cdm_metrics_engine_->GetMetricsSnapshot(&metrics_proto);
  ASSERT_EQ(
      1, metrics_proto.engine_metrics()
          .cdm_engine_get_secure_stop_ids_size());
  EXPECT_EQ(wvcdm::UNKNOWN_ERROR,
            metrics_proto.engine_metrics()
                .cdm_engine_get_secure_stop_ids(0)
                .attributes().error_code());
}

TEST_F(WvCdmEngineMetricsImplTest, FindSessionForKey) {
  CdmSessionId session_id;

  EXPECT_CALL(*test_cdm_metrics_engine_,
      FindSessionForKey(Eq("fake key id"), Eq(&session_id)))
      .WillOnce(Return(true));

  ASSERT_TRUE(test_cdm_metrics_engine_->FindSessionForKey("fake key id",
                                                          &session_id));

  drm_metrics::WvCdmMetrics metrics_proto;
  test_cdm_metrics_engine_->GetMetricsSnapshot(&metrics_proto);
  ASSERT_EQ(
      1, metrics_proto.engine_metrics()
          .cdm_engine_find_session_for_key_size());
  EXPECT_TRUE(metrics_proto.engine_metrics()
                 .cdm_engine_find_session_for_key(0)
                 .attributes().error_code_bool());
}

TEST_F(WvCdmEngineMetricsImplTest, Decrypt) {
  CdmDecryptionParameters parameters;
  EXPECT_CALL(*test_cdm_metrics_engine_, Decrypt(Eq("fake session id"), _))
      .WillOnce(Return(wvcdm::UNKNOWN_ERROR));

  ASSERT_EQ(wvcdm::UNKNOWN_ERROR,
            test_cdm_metrics_engine_->Decrypt("fake session id", parameters));

  drm_metrics::WvCdmMetrics metrics_proto;
  test_cdm_metrics_engine_->GetMetricsSnapshot(&metrics_proto);
  ASSERT_EQ(
      1, metrics_proto.engine_metrics()
          .cdm_engine_decrypt_time_us_size());
  EXPECT_EQ(wvcdm::UNKNOWN_ERROR,
            metrics_proto.engine_metrics()
                .cdm_engine_decrypt_time_us(0)
                .attributes().error_code());
}

}  // namespace wvcdm
