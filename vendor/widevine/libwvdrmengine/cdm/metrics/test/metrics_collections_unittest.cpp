// Copyright 2017 Google Inc. All Rights Reserved.
//
// Unit tests for the metrics collections,
// EngineMetrics, SessionMetrics and CryptoMetrics.

#include "metrics_collections.h"

#include <sstream>

#include "device_files.h"
#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "log.h"
#include "metrics.pb.h"
#include "string_conversions.h"
#include "wv_cdm_types.h"

using drm_metrics::WvCdmMetrics;

namespace {
const char kSessionId1[] = "session_id_1";
const char kSessionId2[] = "session_id_2";
}  // anonymous namespace

namespace wvcdm {
namespace metrics {

// TODO(blueeyes): Improve this implementation by supporting full message
// API In CDM. That allows us to use MessageDifferencer.
class EngineMetricsTest : public ::testing::Test {
};

TEST_F(EngineMetricsTest, AllEngineMetrics) {
  EngineMetrics engine_metrics;

  // Set some values in all of the engine metrics.
  engine_metrics.cdm_engine_add_key_.Record(1.0, KEY_ADDED,
                                            kLicenseTypeRelease);
  engine_metrics.cdm_engine_close_session_.Increment(NO_ERROR);
  engine_metrics.cdm_engine_decrypt_.Record(1.0, NO_ERROR,
                                            metrics::Pow2Bucket(8));
  engine_metrics.cdm_engine_find_session_for_key_.Increment(false);
  engine_metrics.cdm_engine_generate_key_request_.Record(1.0, NO_ERROR,
                                                         kLicenseTypeRelease);
  engine_metrics.cdm_engine_get_provisioning_request_.Record(1.0, NO_ERROR);
  engine_metrics.cdm_engine_get_usage_info_.Record(1.0, NO_ERROR,
                                                   UNKNOWN_ERROR);
  engine_metrics.cdm_engine_handle_provisioning_response_.Record(1.0, NO_ERROR);
  engine_metrics.cdm_engine_open_key_set_session_.Increment(NO_ERROR);
  engine_metrics.cdm_engine_open_session_.Increment(NO_ERROR);
  engine_metrics.cdm_engine_query_key_status_.Record(1.0, NO_ERROR);
  engine_metrics.cdm_engine_release_all_usage_info_.Increment(NO_ERROR);
  engine_metrics.cdm_engine_release_usage_info_.Increment(NO_ERROR);
  engine_metrics.cdm_engine_remove_keys_.Increment(NO_ERROR);
  engine_metrics.cdm_engine_restore_key_.Record(1.0, NO_ERROR);
  engine_metrics.cdm_engine_unprovision_.Increment(NO_ERROR, kSecurityLevelL1);
  engine_metrics.SetAppPackageName("test package name");
  engine_metrics.cdm_engine_cdm_version_.Record("test cdm version");
  engine_metrics.cdm_engine_creation_time_millis_.Record(100);
  engine_metrics.cdm_engine_get_secure_stop_ids_.Increment(UNKNOWN_ERROR);
  engine_metrics.cdm_engine_remove_all_usage_info_.Increment(UNKNOWN_ERROR);
  engine_metrics.cdm_engine_remove_usage_info_.Increment(UNKNOWN_ERROR);

  // Also set and serialize the oemcrypto dynamic adapter metrics.
  OemCryptoDynamicAdapterMetrics adapter_metrics;
  adapter_metrics.SetInitializationMode(OEMCrypto_INITIALIZED_FORCING_L3);
  adapter_metrics.SetL1ApiVersion(2);
  adapter_metrics.SetL1MinApiVersion(1);

  WvCdmMetrics actual;
  engine_metrics.Serialize(&actual);
  adapter_metrics.Serialize(actual.mutable_engine_metrics());

  EXPECT_EQ(0, actual.session_metrics_size());
  ASSERT_TRUE(actual.has_engine_metrics());
  EXPECT_GT(actual.engine_metrics().cdm_engine_add_key_time_us_size(), 0);
  EXPECT_GT(actual.engine_metrics().cdm_engine_decrypt_time_us_size(), 0);
  EXPECT_GT(actual.engine_metrics().cdm_engine_find_session_for_key_size(), 0);
  EXPECT_GT(
      actual.engine_metrics()
          .cdm_engine_generate_key_request_time_us_size(), 0);
  EXPECT_GT(
      actual.engine_metrics()
          .cdm_engine_get_provisioning_request_time_us_size(), 0);
  EXPECT_GT(actual.engine_metrics().cdm_engine_get_usage_info_time_us_size(),
            0);
  EXPECT_GT(
      actual.engine_metrics()
          .cdm_engine_handle_provisioning_response_time_us_size(), 0);
  EXPECT_GT(actual.engine_metrics().cdm_engine_open_key_set_session_size(), 0);
  EXPECT_GT(actual.engine_metrics().cdm_engine_open_session_size(), 0);
  EXPECT_GT(actual.engine_metrics().cdm_engine_query_key_status_time_us_size(),
            0);
  EXPECT_GT(actual.engine_metrics().cdm_engine_release_all_usage_info_size(),
            0);
  EXPECT_GT(actual.engine_metrics().cdm_engine_release_usage_info_size(), 0);
  EXPECT_GT(actual.engine_metrics().cdm_engine_remove_keys_size(), 0);
  EXPECT_GT(actual.engine_metrics().cdm_engine_restore_key_time_us_size(), 0);
  EXPECT_GT(actual.engine_metrics().cdm_engine_unprovision_size(), 0);
  EXPECT_GT(actual.engine_metrics().cdm_engine_get_secure_stop_ids_size(), 0);
  EXPECT_GT(actual.engine_metrics().cdm_engine_remove_all_usage_info_size(), 0);
  EXPECT_GT(actual.engine_metrics().cdm_engine_remove_usage_info_size(), 0);
  EXPECT_EQ("test package name",
            actual.engine_metrics().app_package_name().string_value());
  EXPECT_EQ("test cdm version",
            actual.engine_metrics().cdm_engine_cdm_version().string_value());
  EXPECT_EQ(100,
            actual.engine_metrics()
                .cdm_engine_creation_time_millis().int_value());
}

TEST_F(EngineMetricsTest, EngineAndCryptoMetrics) {
  EngineMetrics engine_metrics;

  // Set some values in some of the engine metrics and some crypto metrics.
  engine_metrics.cdm_engine_add_key_.Record(1.0, KEY_ADDED,
                                            kLicenseTypeRelease);
  engine_metrics.cdm_engine_close_session_.Increment(UNKNOWN_ERROR);
  CryptoMetrics* crypto_metrics = engine_metrics.GetCryptoMetrics();

  crypto_metrics->crypto_session_get_device_unique_id_.Increment(NO_ERROR);
  crypto_metrics->crypto_session_generic_decrypt_
      .Record(2.0, NO_ERROR, Pow2Bucket(1025), kEncryptionAlgorithmAesCbc128);

  WvCdmMetrics actual_metrics;
  engine_metrics.Serialize(&actual_metrics);

  ASSERT_TRUE(actual_metrics.has_engine_metrics());
  EXPECT_EQ(0, actual_metrics.session_metrics_size());

  // Spot check some metrics.
  ASSERT_EQ(1,
            actual_metrics.engine_metrics().cdm_engine_add_key_time_us_size());
  EXPECT_EQ(2, actual_metrics.engine_metrics()
                             .cdm_engine_add_key_time_us(0)
                             .attributes().error_code());
  ASSERT_EQ(1,
            actual_metrics.engine_metrics().cdm_engine_close_session_size());
  EXPECT_EQ(UNKNOWN_ERROR, actual_metrics.engine_metrics()
                                         .cdm_engine_close_session(0)
                                         .attributes().error_code());
  ASSERT_EQ(1, actual_metrics.engine_metrics().crypto_metrics()
                             .crypto_session_get_device_unique_id_size());
  EXPECT_EQ(1, actual_metrics.engine_metrics().crypto_metrics()
                             .crypto_session_get_device_unique_id(0)
                             .count());
  EXPECT_EQ(NO_ERROR, actual_metrics.engine_metrics().crypto_metrics()
                                    .crypto_session_get_device_unique_id(0)
                                    .attributes().error_code());
  ASSERT_EQ(1, actual_metrics.engine_metrics().crypto_metrics()
                             .crypto_session_generic_decrypt_time_us_size());
  EXPECT_EQ(2.0, actual_metrics.engine_metrics().crypto_metrics()
                             .crypto_session_generic_decrypt_time_us(0)
                             .mean());
  EXPECT_EQ(NO_ERROR, actual_metrics.engine_metrics().crypto_metrics()
                                    .crypto_session_generic_decrypt_time_us(0)
                                    .attributes().error_code());
}

TEST_F(EngineMetricsTest, EmptyEngineMetrics) {
  EngineMetrics engine_metrics;

  WvCdmMetrics actual_metrics;
  engine_metrics.Serialize(&actual_metrics);

  // Empty engine metics will still produce an engine metrics record.
  EXPECT_TRUE(actual_metrics.has_engine_metrics());
  EXPECT_EQ(0, actual_metrics.session_metrics_size());
}

TEST_F(EngineMetricsTest, EngineMetricsWithSessions) {
  EngineMetrics engine_metrics;

  // Set a values in an engine metric and in a crypto metric.
  engine_metrics.cdm_engine_add_key_.Record(1.0, KEY_ADDED,
                                            kLicenseTypeRelease);
  engine_metrics.GetCryptoMetrics()
      ->crypto_session_load_certificate_private_key_.Record(2.0, NO_ERROR);

  // Create two sessions and record some metrics.
  std::shared_ptr<SessionMetrics> session_metrics_1 =
      engine_metrics.AddSession();
  session_metrics_1->SetSessionId(kSessionId1);
  std::shared_ptr<SessionMetrics> session_metrics_2 =
      engine_metrics.AddSession();
  session_metrics_2->SetSessionId(kSessionId2);
  // Record a CryptoMetrics metric in the session.
  session_metrics_2->GetCryptoMetrics()->crypto_session_generic_decrypt_
      .Record(2.0, NO_ERROR, Pow2Bucket(1025), kEncryptionAlgorithmAesCbc128);

  WvCdmMetrics actual_metrics;
  engine_metrics.Serialize(&actual_metrics);

  // Spot check some metrics.
  ASSERT_TRUE(actual_metrics.has_engine_metrics());
  EXPECT_EQ(1,
            actual_metrics.engine_metrics().cdm_engine_add_key_time_us_size());
  EXPECT_EQ(2, actual_metrics.session_metrics_size());
  EXPECT_EQ(kSessionId1,
            actual_metrics.session_metrics(0).session_id().string_value());
  EXPECT_EQ(kSessionId2,
            actual_metrics.session_metrics(1).session_id().string_value());
  EXPECT_EQ(1, actual_metrics.session_metrics(1).crypto_metrics()
                   .crypto_session_generic_decrypt_time_us_size());
}

TEST_F(EngineMetricsTest, EngineMetricsConsolidateSessions) {
  EngineMetrics engine_metrics;

  // Create one more session than the max allowed closed sessions.
  std::vector<std::shared_ptr<SessionMetrics>> session_metrics;
  for (int i = 0; i < kMaxCompletedSessions + 1; i++) {
    session_metrics.push_back(engine_metrics.AddSession());
    session_metrics.back()->SetSessionId(std::to_string(i));
  }

  // Mark sessions as completed.
  for (auto it = session_metrics.begin(); it != session_metrics.end(); it++) {
    (*it)->SetCompleted();
  }

  engine_metrics.ConsolidateSessions();

  // Consolidate the metrics now. These should now be consolidated down to
  // kMaxCompletedSessions.
  WvCdmMetrics actual_metrics;
  engine_metrics.Serialize(&actual_metrics);
  ASSERT_EQ(kMaxCompletedSessions, actual_metrics.session_metrics_size());
}

TEST_F(EngineMetricsTest, EngineMetricsConsolidateSessionsNoClosedSessions) {
  EngineMetrics engine_metrics;

  // Create one more session than the max allowed closed sessions.
  std::vector<std::shared_ptr<SessionMetrics>> session_metrics;
  for (int i = 0; i < kMaxCompletedSessions + 1; i++) {
    session_metrics.push_back(engine_metrics.AddSession());
    session_metrics.back()->SetSessionId(std::to_string(i));
  }

  // Consolidate sessions will do nothing since no sessions are closed.
  engine_metrics.ConsolidateSessions();

  // Serializing now will include all sessions since none are closed.
  WvCdmMetrics actual_metrics;
  engine_metrics.Serialize(&actual_metrics);
  ASSERT_EQ(kMaxCompletedSessions + 1, actual_metrics.session_metrics_size());
}

TEST_F(EngineMetricsTest, EngineMetricsConsolidateSessionsNoSessions) {
  EngineMetrics engine_metrics;

  // Consolidate sessions will do nothing since there are no sessions
  engine_metrics.ConsolidateSessions();

  WvCdmMetrics actual_metrics;
  engine_metrics.Serialize(&actual_metrics);
  ASSERT_EQ(0, actual_metrics.session_metrics_size());
}

class SessionMetricsTest : public ::testing::Test {
};

TEST_F(SessionMetricsTest, AllSessionMetrics) {
  SessionMetrics session_metrics;
  session_metrics.SetSessionId(kSessionId1);
  session_metrics.cdm_session_life_span_.Record(1.0);
  session_metrics.cdm_session_renew_key_.Record(1.0, NO_ERROR);
  session_metrics.cdm_session_restore_offline_session_.Increment(
      NO_ERROR, DeviceFiles::ResponseType::kObjectNotInitialized);
  session_metrics.cdm_session_restore_usage_session_.Increment(
      NO_ERROR, DeviceFiles::ResponseType::kObjectNotInitialized);
  session_metrics.cdm_session_license_request_latency_ms_.Record(
      2.0, kKeyRequestTypeInitial);
  session_metrics.oemcrypto_build_info_.Record("test build info");
  session_metrics.license_sdk_version_.Record("test license sdk version");
  session_metrics.license_service_version_.Record("test license service version");

  // Record a CryptoMetrics metric in the session.
  session_metrics.GetCryptoMetrics()->crypto_session_generic_decrypt_
      .Record(2.0, NO_ERROR, Pow2Bucket(1025), kEncryptionAlgorithmAesCbc128);

  WvCdmMetrics::SessionMetrics actual;
  session_metrics.Serialize(&actual);
  EXPECT_STREQ(kSessionId1, actual.session_id().string_value().c_str());
  EXPECT_EQ(1.0, actual.cdm_session_life_span_ms().double_value());
  EXPECT_GT(actual.cdm_session_renew_key_time_us_size(), 0);
  EXPECT_GT(actual.cdm_session_restore_offline_session_size(), 0);
  EXPECT_GT(actual.cdm_session_restore_usage_session_size(), 0);
  EXPECT_GT(actual.cdm_session_license_request_latency_ms_size(), 0);
  EXPECT_STREQ("test build info",
               actual.oemcrypto_build_info().string_value().c_str());
  EXPECT_STREQ("test license sdk version",
               actual.license_sdk_version().string_value().c_str());
  EXPECT_STREQ("test license service version",
               actual.license_service_version().string_value().c_str());
  EXPECT_GT(
      actual.crypto_metrics().crypto_session_generic_decrypt_time_us_size(), 0);
}

TEST_F(SessionMetricsTest, EmptySessionMetrics) {
  SessionMetrics session_metrics;

  WvCdmMetrics::SessionMetrics actual_metrics;
  session_metrics.Serialize(&actual_metrics);

  EXPECT_TRUE(actual_metrics.has_crypto_metrics());
  EXPECT_FALSE(actual_metrics.has_session_id());
  EXPECT_FALSE(actual_metrics.has_cdm_session_life_span_ms());
  EXPECT_EQ(0, actual_metrics.cdm_session_renew_key_time_us_size());
  EXPECT_EQ(0, actual_metrics.cdm_session_restore_offline_session_size());
  EXPECT_EQ(0, actual_metrics.cdm_session_restore_usage_session_size());
}

class CryptoMetricsTest : public ::testing::Test {
};

TEST_F(CryptoMetricsTest, AllCryptoMetrics) {
  CryptoMetrics crypto_metrics;

  // Crypto session metrics.
  crypto_metrics.crypto_session_delete_all_usage_reports_.Increment(NO_ERROR);
  crypto_metrics.crypto_session_delete_multiple_usage_information_
      .Increment(NO_ERROR);
  crypto_metrics.crypto_session_generic_decrypt_
      .Record(2.0, NO_ERROR, Pow2Bucket(1025), kEncryptionAlgorithmAesCbc128);
  crypto_metrics.crypto_session_generic_encrypt_
      .Record(2.0, NO_ERROR, Pow2Bucket(1025), kEncryptionAlgorithmAesCbc128);
  crypto_metrics.crypto_session_generic_sign_
      .Record(2.0, NO_ERROR, Pow2Bucket(1025), kSigningAlgorithmHmacSha256);
  crypto_metrics.crypto_session_generic_verify_
      .Record(2.0, NO_ERROR, Pow2Bucket(1025), kSigningAlgorithmHmacSha256);
  crypto_metrics.crypto_session_get_device_unique_id_.Increment(NO_ERROR);
  crypto_metrics.crypto_session_get_token_.Increment(NO_ERROR);
  crypto_metrics.crypto_session_life_span_.Record(1.0);
  crypto_metrics.crypto_session_load_certificate_private_key_
      .Record(1.0, NO_ERROR);
  crypto_metrics.crypto_session_open_.Record(1.0, NO_ERROR, kLevelDefault);
  crypto_metrics.crypto_session_update_usage_information_
      .Record(1.0, NO_ERROR);
  crypto_metrics.crypto_session_usage_information_support_.Record(true);
  crypto_metrics.crypto_session_security_level_.Record(kSecurityLevelL2);

  // Usage table header metrics.
  crypto_metrics.usage_table_header_initial_size_.Record(200);
  crypto_metrics.usage_table_header_add_entry_.Increment(UNKNOWN_ERROR);
  crypto_metrics.usage_table_header_delete_entry_.Increment(UNKNOWN_ERROR);
  crypto_metrics.usage_table_header_update_entry_.Record(2.0, UNKNOWN_ERROR);
  crypto_metrics.usage_table_header_load_entry_.Increment(UNKNOWN_ERROR);

  // Oem crypto metrics.
  crypto_metrics.oemcrypto_api_version_.Record(123);
  crypto_metrics.oemcrypto_close_session_
      .Increment(OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_copy_buffer_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED, Pow2Bucket(1025));
  crypto_metrics.oemcrypto_deactivate_usage_entry_
      .Increment(OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_decrypt_cenc_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED, Pow2Bucket(1025));
  crypto_metrics.oemcrypto_delete_usage_entry_
      .Increment(OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_delete_usage_table_
      .Increment(OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_derive_keys_from_session_key_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_force_delete_usage_entry_
      .Increment(OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_generate_derived_keys_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_generate_nonce_
      .Increment(OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_generate_rsa_signature_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED, Pow2Bucket(1025));
  crypto_metrics.oemcrypto_generate_signature_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED, Pow2Bucket(1025));
  crypto_metrics.oemcrypto_generic_decrypt_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED, Pow2Bucket(1025));
  crypto_metrics.oemcrypto_generic_encrypt_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED, Pow2Bucket(1025));
  crypto_metrics.oemcrypto_generic_sign_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED, Pow2Bucket(1025));
  crypto_metrics.oemcrypto_generic_verify_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED, Pow2Bucket(1025));
  crypto_metrics.oemcrypto_get_device_id_
      .Increment(OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_get_key_data_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED, Pow2Bucket(1025));
  crypto_metrics.oemcrypto_max_number_of_sessions_.Record(7);
  crypto_metrics.oemcrypto_number_of_open_sessions_.Record(5);
  crypto_metrics.oemcrypto_get_oem_public_certificate_
      .Increment(OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_provisioning_method_.Record(OEMCrypto_Keybox);
  crypto_metrics.oemcrypto_get_random_
      .Increment(OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_initialize_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_is_anti_rollback_hw_present_.Record(true);
  crypto_metrics.oemcrypto_is_keybox_valid_.Record(true);
  crypto_metrics.oemcrypto_load_device_rsa_key_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_load_keys_.Record(1.0, OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_refresh_keys_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_report_usage_.Increment(OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_rewrap_device_rsa_key_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_rewrap_device_rsa_key_30_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_security_patch_level_.Record(123);
  crypto_metrics.oemcrypto_select_key_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_update_usage_table_
      .Increment(OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_create_usage_table_header_
      .Increment(OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_load_usage_table_header_
      .Increment(OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_shrink_usage_table_header_
      .Increment(OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_create_new_usage_entry_
      .Increment(OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_load_usage_entry_
      .Increment(OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_move_entry_
      .Increment(OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_create_old_usage_entry_
      .Increment(OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_copy_old_usage_entry_
      .Increment(OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_set_sandbox_.Record("sandbox");
  crypto_metrics.oemcrypto_set_decrypt_hash_
      .Increment(OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_resource_rating_tier_.Record(123);

  WvCdmMetrics::CryptoMetrics actual;
  crypto_metrics.Serialize(&actual);

  // Crypto session metrics.
  EXPECT_GT(actual.crypto_session_delete_all_usage_reports_size(), 0);
  EXPECT_GT(actual.crypto_session_delete_multiple_usage_information_size(), 0);
  EXPECT_GT(actual.crypto_session_generic_decrypt_time_us_size(), 0);
  EXPECT_GT(actual.crypto_session_generic_encrypt_time_us_size(), 0);
  EXPECT_GT(actual.crypto_session_generic_sign_time_us_size(), 0);
  EXPECT_GT(actual.crypto_session_generic_verify_time_us_size(), 0);
  EXPECT_GT(actual.crypto_session_get_device_unique_id_size(), 0);
  EXPECT_GT(actual.crypto_session_get_token_size(), 0);
  EXPECT_EQ(1.0, actual.crypto_session_life_span().double_value());
  EXPECT_GT(actual.crypto_session_load_certificate_private_key_time_us_size(),
            0);
  EXPECT_GT(actual.crypto_session_open_time_us_size(), 0);
  EXPECT_GT(actual.crypto_session_update_usage_information_time_us_size(), 0);
  EXPECT_EQ(true,
            actual.crypto_session_usage_information_support().int_value());
  EXPECT_EQ(kSecurityLevelL2,
            actual.crypto_session_security_level().int_value());

  // Usage table header metrics.
  EXPECT_EQ(200, actual.usage_table_header_initial_size().int_value());
  EXPECT_GT(actual.usage_table_header_add_entry_size(), 0);
  EXPECT_GT(actual.usage_table_header_delete_entry_size(), 0);
  EXPECT_GT(actual.usage_table_header_update_entry_time_us_size(), 0);
  EXPECT_GT(actual.usage_table_header_load_entry_size(), 0);

  // Oem crypto metrics.
  EXPECT_EQ(123, actual.oemcrypto_api_version().int_value());
  EXPECT_GT(actual.oemcrypto_close_session_size(), 0);
  EXPECT_GT(actual.oemcrypto_copy_buffer_time_us_size(), 0);
  EXPECT_GT(actual.oemcrypto_deactivate_usage_entry_size(), 0);
  EXPECT_GT(actual.oemcrypto_decrypt_cenc_time_us_size(), 0);
  EXPECT_GT(actual.oemcrypto_delete_usage_entry_size(), 0);
  EXPECT_GT(actual.oemcrypto_delete_usage_table_size(), 0);
  EXPECT_GT(actual.oemcrypto_derive_keys_from_session_key_time_us_size(), 0);
  EXPECT_GT(actual.oemcrypto_force_delete_usage_entry_size(), 0);
  EXPECT_GT(actual.oemcrypto_generate_derived_keys_time_us_size(), 0);
  EXPECT_GT(actual.oemcrypto_generate_nonce_size(), 0);
  EXPECT_GT(actual.oemcrypto_generate_rsa_signature_time_us_size(), 0);
  EXPECT_GT(actual.oemcrypto_generate_signature_time_us_size(), 0);
  EXPECT_GT(actual.oemcrypto_generic_decrypt_time_us_size(), 0);
  EXPECT_GT(actual.oemcrypto_generic_encrypt_time_us_size(), 0);
  EXPECT_GT(actual.oemcrypto_generic_sign_time_us_size(), 0);
  EXPECT_GT(actual.oemcrypto_generic_verify_time_us_size(), 0);
  EXPECT_GT(actual.oemcrypto_get_device_id_size(), 0);
  EXPECT_GT(actual.oemcrypto_get_key_data_time_us_size(), 0);
  EXPECT_EQ(7, actual.oemcrypto_max_number_of_sessions().int_value());
  EXPECT_EQ(5, actual.oemcrypto_number_of_open_sessions().int_value());
  EXPECT_GT(actual.oemcrypto_get_oem_public_certificate_size(), 0);
  EXPECT_EQ(OEMCrypto_Keybox,
            actual.oemcrypto_provisioning_method().int_value());
  EXPECT_GT(actual.oemcrypto_get_random_size(), 0);
  EXPECT_GT(actual.oemcrypto_initialize_time_us_size(), 0);
  EXPECT_EQ(true, actual.oemcrypto_is_anti_rollback_hw_present().int_value());
  EXPECT_EQ(true, actual.oemcrypto_is_keybox_valid().int_value());
  EXPECT_GT(actual.oemcrypto_load_device_rsa_key_time_us_size(), 0);
  EXPECT_GT(actual.oemcrypto_load_keys_time_us_size(), 0);
  EXPECT_GT(actual.oemcrypto_refresh_keys_time_us_size(), 0);
  EXPECT_GT(actual.oemcrypto_report_usage_size(), 0);
  EXPECT_GT(actual.oemcrypto_rewrap_device_rsa_key_time_us_size(), 0);
  EXPECT_GT(actual.oemcrypto_rewrap_device_rsa_key_30_time_us_size(), 0);
  EXPECT_EQ(123, actual.oemcrypto_security_patch_level().int_value());
  EXPECT_GT(actual.oemcrypto_select_key_time_us_size(), 0);
  EXPECT_GT(actual.oemcrypto_update_usage_table_size(), 0);
  EXPECT_GT(actual.oemcrypto_create_usage_table_header_size(), 0);
  EXPECT_GT(actual.oemcrypto_load_usage_table_header_size(), 0);
  EXPECT_GT(actual.oemcrypto_shrink_usage_table_header_size(), 0);
  EXPECT_GT(actual.oemcrypto_create_new_usage_entry_size(), 0);
  EXPECT_GT(actual.oemcrypto_load_usage_entry_size(), 0);
  EXPECT_GT(actual.oemcrypto_move_entry_size(), 0);
  EXPECT_GT(actual.oemcrypto_create_old_usage_entry_size(), 0);
  EXPECT_GT(actual.oemcrypto_copy_old_usage_entry_size(), 0);
  EXPECT_EQ("sandbox", actual.oemcrypto_set_sandbox().string_value());
  EXPECT_GT(actual.oemcrypto_set_decrypt_hash_size(), 0);
  EXPECT_EQ(123, actual.oemcrypto_resource_rating_tier().int_value());
}

}  // namespace metrics
}  // namespace wvcdm
