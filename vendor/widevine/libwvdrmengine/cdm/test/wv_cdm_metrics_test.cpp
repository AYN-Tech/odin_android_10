// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
// This file contains unit tests for the WvContentDecryptionModule class
// that pertain to collecting and reporting metrics.

#include "cdm_identifier.h"
#include "gmock/gmock.h"
#include "log.h"
#include "metrics.pb.h"
#include "string_conversions.h"
#include "test_base.h"
#include "test_printers.h"
#include "wv_cdm_types.h"
#include "wv_content_decryption_module.h"

using ::testing::Eq;
using ::testing::StrEq;
using ::testing::Ge;
using ::testing::Gt;
using ::testing::Lt;
using ::testing::Test;
using wvcdm::CdmResponseType;

namespace {

const std::string kEmptyServiceCertificate;

}  // unnamed namespace

namespace wvcdm {

// This class is used to test the metrics-related feaures of the
// WvContentDecryptionModule class.
class WvContentDecryptionModuleMetricsTest : public WvCdmTestBase {
 protected:
  void Unprovision(CdmIdentifier identifier) {
    EXPECT_EQ(NO_ERROR, decryptor_.Unprovision(kSecurityLevelL1, identifier));
    EXPECT_EQ(NO_ERROR, decryptor_.Unprovision(kSecurityLevelL3, identifier));
  }
  wvcdm::WvContentDecryptionModule decryptor_;
};

TEST_F(WvContentDecryptionModuleMetricsTest, IdentifierNotFound) {
  drm_metrics::WvCdmMetrics metrics;
  ASSERT_EQ(wvcdm::UNKNOWN_ERROR,
            decryptor_.GetMetrics(kDefaultCdmIdentifier, &metrics));
}

TEST_F(WvContentDecryptionModuleMetricsTest, EngineOnlyMetrics) {
  std::string request;
  std::string provisioning_server_url;
  CdmCertificateType cert_type = kCertificateWidevine;
  std::string cert_authority, cert, wrapped_key;

  // This call will create a CdmEngine instance with an EngineMetrics instance.
  EXPECT_EQ(wvcdm::NO_ERROR,
            decryptor_.GetProvisioningRequest(
                cert_type, cert_authority, kDefaultCdmIdentifier,
                kEmptyServiceCertificate, &request,
                &provisioning_server_url));

  drm_metrics::WvCdmMetrics metrics;
  ASSERT_EQ(wvcdm::NO_ERROR,
            decryptor_.GetMetrics(kDefaultCdmIdentifier, &metrics));

  // 100 is an arbitrary high value that shouldn't ever occur.
  EXPECT_THAT(
      metrics.engine_metrics().level3_oemcrypto_initialization_error()
      .int_value(), Lt(100));
  EXPECT_THAT(
      metrics.engine_metrics().level3_oemcrypto_initialization_error()
      .int_value(), Ge(0));
  EXPECT_THAT(
      metrics.engine_metrics().oemcrypto_initialization_mode().int_value(),
      Lt(100));
  EXPECT_THAT(
      metrics.engine_metrics().oemcrypto_initialization_mode().int_value(),
      Ge(0));
  EXPECT_THAT(
      metrics.engine_metrics().previous_oemcrypto_initialization_failure()
      .int_value(), Lt(100));
  EXPECT_THAT(
      metrics.engine_metrics().previous_oemcrypto_initialization_failure()
      .int_value(), Ge(0));
  ASSERT_THAT(metrics.engine_metrics()
                  .cdm_engine_get_provisioning_request_time_us().size(), Eq(1));
  EXPECT_THAT(metrics.engine_metrics()
                  .cdm_engine_get_provisioning_request_time_us(0)
                  .operation_count(),
                      Eq(1u));
}


TEST_F(WvContentDecryptionModuleMetricsTest, EngineAndSessionMetrics) {
  CdmSessionId session_id;
  wvcdm::CdmKeySystem key_system("com.widevine");
  Unprovision(kDefaultCdmIdentifier);

  // Openning the session will fail with NEEDS_PROVISIONING error. But it will
  // still create some session-level stats.
  EXPECT_EQ(CdmResponseType::NEED_PROVISIONING,
            decryptor_.OpenSession(key_system, NULL,
                                   kDefaultCdmIdentifier, NULL, &session_id));

  drm_metrics::WvCdmMetrics metrics;
  ASSERT_EQ(wvcdm::NO_ERROR,
            decryptor_.GetMetrics(kDefaultCdmIdentifier, &metrics));
  std::string serialized_metrics;
  ASSERT_TRUE(metrics.SerializeToString(&serialized_metrics));

  // Spot check some metric values.
  // Validate engine-level metrics.
  EXPECT_TRUE(metrics.engine_metrics()
      .has_level3_oemcrypto_initialization_error());
  EXPECT_TRUE(metrics.engine_metrics().has_oemcrypto_initialization_mode());
  EXPECT_TRUE(metrics.engine_metrics()
      .has_previous_oemcrypto_initialization_failure());
  ASSERT_THAT(metrics.engine_metrics().cdm_engine_open_session().size(), Eq(1));
  EXPECT_THAT(metrics.engine_metrics().cdm_engine_open_session(0).count(),
              Eq(1));
  EXPECT_THAT(metrics.engine_metrics()
                  .cdm_engine_open_session(0).attributes().error_code(),
                      Eq(CdmResponseType::NEED_PROVISIONING));

  // Validate a session-level metric.
  ASSERT_THAT(metrics.session_metrics().size(), Eq(1));
  EXPECT_THAT(
      metrics.session_metrics(0).cdm_session_life_span_ms().double_value(),
      Gt(0.0))
          << "Unexpected failure with session_metrics: "
          << wvcdm::b2a_hex(serialized_metrics);
}

TEST_F(WvContentDecryptionModuleMetricsTest,
       DifferentCdmIdentifiersHaveDifferentMetrics) {
  CdmSessionId session_id;
  wvcdm::CdmKeySystem key_system("com.widevine");
  CdmIdentifier identifiers[] = { kDefaultCdmIdentifier,
                                { "foo", "bar", "baz", 7 },
                                // Note that this has all the same parameters
                                // as the one above except for the unique_id.
                                { "foo", "bar", "baz", 8 }};
  const int cdm_engine_count = 3;

  // Force Unprovision.
  for (int i = 0; i < cdm_engine_count; i++) {
    Unprovision(identifiers[i]);
  }

  for (int i = 0; i < cdm_engine_count; i++) {
    // To make sure we can detect different engine metrics,
    // make the open session call a different number of times for
    // each identifier.
    for (int j = 0; j <= i; j ++) {
      EXPECT_EQ(CdmResponseType::NEED_PROVISIONING,
                decryptor_.OpenSession(key_system, NULL,
                                       identifiers[i], NULL, &session_id));
    }
  }

  for (int i = 0; i < cdm_engine_count; i++) {
    drm_metrics::WvCdmMetrics metrics;
    metrics.Clear();
    ASSERT_EQ(wvcdm::NO_ERROR,
              decryptor_.GetMetrics(identifiers[i], &metrics));
    std::string serialized_metrics;
    ASSERT_TRUE(metrics.SerializeToString(&serialized_metrics));

    ASSERT_THAT(metrics.engine_metrics().cdm_engine_open_session().size(),
                Eq(1));
    // The number of times open session was called should match the index
    // of the identifier
    EXPECT_THAT(metrics.engine_metrics().cdm_engine_open_session(0).count(),
                Eq(i + 1));
    EXPECT_THAT(
        metrics.engine_metrics()
            .cdm_engine_open_session(0).attributes().error_code(),
        Eq(CdmResponseType::NEED_PROVISIONING));

    // Spot check a session-level metric.
    ASSERT_THAT(metrics.session_metrics().size(), Eq(i + 1))
        << "Unexpected failure with session_metrics: "
        << wvcdm::b2a_hex(serialized_metrics);
    EXPECT_THAT(metrics.session_metrics(0)
                .cdm_session_life_span_ms().double_value(), Gt(0.0))
        << "Unexpected failure with session_metrics: "
        << wvcdm::b2a_hex(serialized_metrics);
  }
}

}  // wvcdm namespace
