// Copyright 2017 Google Inc. All Rights Reserved.
//
// Unit tests for the metrics collections,
// EngineMetrics, SessionMetrics and CrytpoMetrics.

#include "metrics_collections.h"

#include <sstream>

#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "log.h"
#include "metrics.pb.h"
#include "wv_cdm_types.h"

using drm_metrics::MetricsGroup;
using google::protobuf::TextFormat;

namespace wvcdm {
namespace metrics {

// TODO(blueeyes): Improve this implementation by supporting full message
// API In CDM. That allows us to use MessageDifferencer.
class EngineMetricsTest : public ::testing::Test {
};

TEST_F(EngineMetricsTest, AllEngineMetrics) {
  EngineMetrics engine_metrics;

  // Set some values in all of the engine metrics.
  engine_metrics.cdm_engine_add_key_.Record(1.0, KEY_ADDED);
  engine_metrics.cdm_engine_close_session_.Record(1.0, NO_ERROR);
  engine_metrics.cdm_engine_decrypt_.Record(1.0, NO_ERROR);
  engine_metrics.cdm_engine_find_session_for_key_.Record(1.0, false);
  engine_metrics.cdm_engine_generate_key_request_.Record(1.0, NO_ERROR);
  engine_metrics.cdm_engine_get_provisioning_request_.Record(1.0, NO_ERROR);
  engine_metrics.cdm_engine_get_usage_info_.Record(1.0, NO_ERROR);
  engine_metrics.cdm_engine_handle_provisioning_response_.Record(1.0, NO_ERROR);
  engine_metrics.cdm_engine_life_span_.Record(1.0);
  engine_metrics.cdm_engine_open_key_set_session_.Record(1.0, NO_ERROR);
  engine_metrics.cdm_engine_open_session_.Record(1.0, NO_ERROR);
  engine_metrics.cdm_engine_query_key_status_.Record(1.0, NO_ERROR);
  engine_metrics.cdm_engine_release_all_usage_info_.Record(1.0, NO_ERROR);
  engine_metrics.cdm_engine_release_usage_info_.Record(1.0, NO_ERROR);
  engine_metrics.cdm_engine_remove_keys_.Record(1.0, NO_ERROR);
  engine_metrics.cdm_engine_restore_key_.Record(1.0, NO_ERROR);
  engine_metrics.cdm_engine_unprovision_.Record(1.0, NO_ERROR, kSecurityLevelL1);

  drm_metrics::MetricsGroup actual_metrics;
  engine_metrics.Serialize(&actual_metrics, true, false);

  // For each EventMetric, 2 metrics get serialized since only one sample was
  // taken. So, the total number of serialized metrics are 2*17.
  ASSERT_EQ(2 * 17, actual_metrics.metric_size());
  EXPECT_EQ(0, actual_metrics.metric_sub_group_size());

  // Spot check some metrics.
  EXPECT_EQ("/drm/widevine/cdm_engine/add_key/time/count{error:2}",
            actual_metrics.metric(0).name());
  EXPECT_EQ("/drm/widevine/cdm_engine/close_session/time/mean{error:0}",
            actual_metrics.metric(3).name());
  EXPECT_EQ("/drm/widevine/cdm_engine/decrypt/time/mean{error:0}",
            actual_metrics.metric(5).name());
  EXPECT_EQ(1.0, actual_metrics.metric(5).value().double_value());
}

TEST_F(EngineMetricsTest, EngineAndCryptoMetrics) {
  EngineMetrics engine_metrics;

  // Set some values in some of the engine metrics and some crypto metrics.
  engine_metrics.cdm_engine_add_key_.Record(1.0, KEY_ADDED);
  engine_metrics.cdm_engine_close_session_.Record(1.0, NO_ERROR);
  CryptoMetrics* crypto_metrics = engine_metrics.GetCryptoMetrics();

  crypto_metrics->crypto_session_generic_decrypt_
      .Record(2.0, NO_ERROR, Pow2Bucket(1025), kEncryptionAlgorithmAesCbc128);
  crypto_metrics->crypto_session_get_device_unique_id_
      .Record(4.0, false);

  drm_metrics::MetricsGroup actual_metrics;
  engine_metrics.Serialize(&actual_metrics, true, false);

  // For each EventMetric, 2 metrics get serialized since only one sample was
  // taken. So, the total number of serialized metrics are 2*4 since we
  // touched 4 metrics.
  ASSERT_EQ(2 * 4, actual_metrics.metric_size());
  EXPECT_EQ(0, actual_metrics.metric_sub_group_size());

  // Spot check some metrics.
  EXPECT_EQ("/drm/widevine/cdm_engine/add_key/time/count{error:2}",
            actual_metrics.metric(0).name());
  EXPECT_EQ(
      "/drm/widevine/crypto_session/generic_decrypt/time/count"
      "{error:0&length:1024&encryption_algorithm:1}",
      actual_metrics.metric(4).name());
  EXPECT_EQ(
      "/drm/widevine/crypto_session/get_device_unique_id/time/mean{success:0}",
      actual_metrics.metric(7).name());
  EXPECT_EQ(4.0, actual_metrics.metric(7).value().double_value());
}

TEST_F(EngineMetricsTest, EmptyEngineMetrics) {
  EngineMetrics engine_metrics;

  drm_metrics::MetricsGroup actual_metrics;
  engine_metrics.Serialize(&actual_metrics, true, false);

  EXPECT_EQ(0, actual_metrics.metric_size());
  EXPECT_EQ(0, actual_metrics.metric_sub_group_size());
}

TEST_F(EngineMetricsTest, EngineMetricsWithCompletedSessions) {
  EngineMetrics engine_metrics;

  // Set a values in an engine metric and in a crypto metric.
  engine_metrics.cdm_engine_add_key_.Record(1.0, KEY_ADDED);
  engine_metrics.GetCryptoMetrics()
      ->crypto_session_load_certificate_private_key_.Record(2.0, true);

  // Create two sessions and record some metrics.
  SessionMetrics* session_metrics_1 = engine_metrics.AddSession();
  session_metrics_1->SetSessionId("session_id_1");
  SessionMetrics* session_metrics_2 = engine_metrics.AddSession();
  session_metrics_2->SetSessionId("session_id_2");
  // Record a CryptoMetrics metric in the session.
  session_metrics_2->GetCryptoMetrics()->crypto_session_generic_decrypt_
      .Record(2.0, NO_ERROR, Pow2Bucket(1025), kEncryptionAlgorithmAesCbc128);
  session_metrics_2->SetSessionId("session_id_2");
  // Mark only session 2 as completed.
  session_metrics_2->SetCompleted();

  drm_metrics::MetricsGroup actual_metrics;
  engine_metrics.Serialize(&actual_metrics, true, false);

  // Validate metric counts.
  // For each EventMetric, 2 metrics get serialized since only one sample was
  // taken. So, the total number of serialized metrics are 2*2 since we
  // touched 2 metrics.
  ASSERT_EQ(2 * 2, actual_metrics.metric_size());
  ASSERT_EQ(1, actual_metrics.metric_sub_group_size());
  ASSERT_EQ(3, actual_metrics.metric_sub_group(0).metric_size());

  // Spot check some metrics.
  EXPECT_EQ("/drm/widevine/cdm_engine/add_key/time/count{error:2}",
            actual_metrics.metric(0).name());
  EXPECT_EQ("/drm/widevine/crypto_session/load_certificate_private_key"
            "/time/count{success:1}",
            actual_metrics.metric(2).name());
  EXPECT_EQ("/drm/widevine/cdm_session/session_id",
            actual_metrics.metric_sub_group(0).metric(0).name());
  EXPECT_EQ(
      "session_id_2",
      actual_metrics.metric_sub_group(0).metric(0).value().string_value());
  EXPECT_EQ(
      "/drm/widevine/crypto_session/generic_decrypt/time/count"
      "{error:0&length:1024&encryption_algorithm:1}",
      actual_metrics.metric_sub_group(0).metric(1).name());
}

TEST_F(EngineMetricsTest, EngineMetricsSerializeAllSessions) {
  EngineMetrics engine_metrics;

  // Create two sessions and record some metrics.
  SessionMetrics* session_metrics_1 = engine_metrics.AddSession();
  session_metrics_1->SetSessionId("session_id_1");
  SessionMetrics* session_metrics_2 = engine_metrics.AddSession();
  session_metrics_2->SetSessionId("session_id_2");
  // Mark only session 2 as completed.
  session_metrics_2->SetCompleted();

  drm_metrics::MetricsGroup actual_metrics;
  engine_metrics.Serialize(&actual_metrics, false, false);

  // Validate metric counts.
  // No Engine-level metrics were recorded.
  ASSERT_EQ(0, actual_metrics.metric_size());
  // Two sub groups, 1 per session.
  ASSERT_EQ(2, actual_metrics.metric_sub_group_size());
  ASSERT_EQ(1, actual_metrics.metric_sub_group(0).metric_size());

  // Spot check some metrics.
  EXPECT_EQ("/drm/widevine/cdm_session/session_id",
            actual_metrics.metric_sub_group(0).metric(0).name());
  EXPECT_EQ(
      "session_id_1",
      actual_metrics.metric_sub_group(0).metric(0).value().string_value());
  EXPECT_EQ("/drm/widevine/cdm_session/session_id",
            actual_metrics.metric_sub_group(1).metric(0).name());
  EXPECT_EQ(
      "session_id_2",
      actual_metrics.metric_sub_group(1).metric(0).value().string_value());
}

TEST_F(EngineMetricsTest, EngineMetricsRemoveSessions) {
  EngineMetrics engine_metrics;

  // Create two sessions and record some metrics.
  SessionMetrics* session_metrics_1 = engine_metrics.AddSession();
  session_metrics_1->SetSessionId("session_id_1");
  SessionMetrics* session_metrics_2 = engine_metrics.AddSession();
  session_metrics_2->SetSessionId("session_id_2");
  // Mark only session 2 as completed.
  session_metrics_2->SetCompleted();

  // Serialize all metrics, don't remove any.
  drm_metrics::MetricsGroup actual_metrics;
  engine_metrics.Serialize(&actual_metrics, false, false);

  // Validate metric counts.
  // Two sub groups, 1 per session.
  ASSERT_EQ(2, actual_metrics.metric_sub_group_size());

  // Serialize completed metrics, remove them.
  actual_metrics.Clear();
  engine_metrics.Serialize(&actual_metrics, true, true);
  // Validate metric counts.
  // Only one, completed session should exist.
  ASSERT_EQ(1, actual_metrics.metric_sub_group_size());
  ASSERT_EQ(1, actual_metrics.metric_sub_group(0).metric_size());
  EXPECT_EQ(
      "session_id_2",
      actual_metrics.metric_sub_group(0).metric(0).value().string_value());

  // Serialize all metrics, remove them.
  actual_metrics.Clear();
  engine_metrics.Serialize(&actual_metrics, false, true);
  // Validate metric counts.
  // Only one, non-complete session should exist.
  ASSERT_EQ(1, actual_metrics.metric_sub_group_size());
  ASSERT_EQ(1, actual_metrics.metric_sub_group(0).metric_size());
  EXPECT_EQ(
      "session_id_1",
      actual_metrics.metric_sub_group(0).metric(0).value().string_value());

  // Serialize all metrics, don't remove any.
  actual_metrics.Clear();
  engine_metrics.Serialize(&actual_metrics, false, false);
  // Validate metric counts.
  // There should be no more metric subgroups left.
  ASSERT_EQ(0, actual_metrics.metric_sub_group_size());
}

class SessionMetricsTest : public ::testing::Test {
};

TEST_F(SessionMetricsTest, AllSessionMetrics) {
  SessionMetrics session_metrics;
  session_metrics.SetSessionId("session_id 1");

  session_metrics.cdm_session_life_span_.Record(1.0);
  session_metrics.cdm_session_renew_key_.Record(1.0, NO_ERROR);
  session_metrics.cdm_session_restore_offline_session_.Record(1.0, NO_ERROR);
  session_metrics.cdm_session_restore_usage_session_.Record(1.0, NO_ERROR);

  // Record a CryptoMetrics metric in the session.
  session_metrics.GetCryptoMetrics()->crypto_session_generic_decrypt_
      .Record(2.0, NO_ERROR, Pow2Bucket(1025), kEncryptionAlgorithmAesCbc128);

  MetricsGroup actual_metrics;
  session_metrics.Serialize(&actual_metrics);

  ASSERT_EQ(11, actual_metrics.metric_size());
  EXPECT_EQ(0, actual_metrics.metric_sub_group_size());

  // Spot check some metrics.
  EXPECT_EQ("/drm/widevine/cdm_session/session_id",
            actual_metrics.metric(0).name());
  EXPECT_EQ("/drm/widevine/cdm_session/life_span/time/count",
            actual_metrics.metric(1).name());
  EXPECT_EQ("/drm/widevine/cdm_session/renew_key/time/mean{error:0}",
            actual_metrics.metric(4).name());
  EXPECT_EQ(1.0, actual_metrics.metric(4).value().double_value());
  EXPECT_EQ("/drm/widevine/crypto_session/generic_decrypt/time/count"
            "{error:0&length:1024&encryption_algorithm:1}",
            actual_metrics.metric(9).name());
}

TEST_F(SessionMetricsTest, EmptySessionMetrics) {
  SessionMetrics session_metrics;

  MetricsGroup actual_metrics;
  session_metrics.Serialize(&actual_metrics);

  // Session metric always has a session id.
  ASSERT_EQ(1, actual_metrics.metric_size());
  EXPECT_EQ("/drm/widevine/cdm_session/session_id",
            actual_metrics.metric(0).name());
  EXPECT_EQ("", actual_metrics.metric(0).value().string_value());
  EXPECT_EQ(0, actual_metrics.metric_sub_group_size());
}

class CryptoMetricsTest : public ::testing::Test {
};

TEST_F(CryptoMetricsTest, AllCryptoMetrics) {
  CryptoMetrics crypto_metrics;

  // Crypto session metrics.
  crypto_metrics.crypto_session_delete_all_usage_reports_
      .Record(1.0, NO_ERROR);
  crypto_metrics.crypto_session_delete_multiple_usage_information_
      .Record(1.0, NO_ERROR);
  crypto_metrics.crypto_session_generic_decrypt_
      .Record(2.0, NO_ERROR, Pow2Bucket(1025), kEncryptionAlgorithmAesCbc128);
  crypto_metrics.crypto_session_generic_encrypt_
      .Record(2.0, NO_ERROR, Pow2Bucket(1025), kEncryptionAlgorithmAesCbc128);
  crypto_metrics.crypto_session_generic_sign_
      .Record(2.0, NO_ERROR, Pow2Bucket(1025), kSigningAlgorithmHmacSha256);
  crypto_metrics.crypto_session_generic_verify_
      .Record(2.0, NO_ERROR, Pow2Bucket(1025), kSigningAlgorithmHmacSha256);
  crypto_metrics.crypto_session_get_device_unique_id_.Record(1.0, true);
  crypto_metrics.crypto_session_get_security_level_
      .Record(1.0, kSecurityLevelL1);
  crypto_metrics.crypto_session_get_system_id_.Record(1.0, true, 1234);
  crypto_metrics.crypto_session_get_token_.Record(1.0, true);
  crypto_metrics.crypto_session_life_span_.Record(1.0);
  crypto_metrics.crypto_session_load_certificate_private_key_
      .Record(1.0, true);
  crypto_metrics.crypto_session_open_.Record(1.0, NO_ERROR, kLevelDefault);
  crypto_metrics.crypto_session_update_usage_information_
      .Record(1.0, NO_ERROR);
  crypto_metrics.crypto_session_usage_information_support_.Record(1.0, true);

  // Oem crypto metrics.
  crypto_metrics.oemcrypto_api_version_.Record(1.0, 123, kLevelDefault);
  crypto_metrics.oemcrypto_close_session_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_copy_buffer_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED,
              kLevelDefault, Pow2Bucket(1025));
  crypto_metrics.oemcrypto_deactivate_usage_entry_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_decrypt_cenc_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED, Pow2Bucket(1025));
  crypto_metrics.oemcrypto_delete_usage_entry_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_delete_usage_table_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_derive_keys_from_session_key_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_force_delete_usage_entry_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_generate_derived_keys_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_generate_nonce_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED);
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
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED, kLevelDefault);
  crypto_metrics.oemcrypto_get_hdcp_capability_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED, kLevelDefault);
  crypto_metrics.oemcrypto_get_key_data_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED,
              Pow2Bucket(1025), kLevelDefault);
  crypto_metrics.oemcrypto_get_max_number_of_sessions_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED, kLevelDefault);
  crypto_metrics.oemcrypto_get_number_of_open_sessions_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED, kLevelDefault);
  crypto_metrics.oemcrypto_get_oem_public_certificate_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED);

  crypto_metrics.oemcrypto_get_provisioning_method_
      .Record(1.0, OEMCrypto_Keybox, kLevelDefault);

  crypto_metrics.oemcrypto_get_random_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED, Pow2Bucket(1025));
  crypto_metrics.oemcrypto_initialize_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_install_keybox_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED, kLevelDefault);
  crypto_metrics.oemcrypto_is_anti_rollback_hw_present_
      .Record(1.0, true, kLevelDefault);

  crypto_metrics.oemcrypto_is_keybox_valid_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED, kLevelDefault);
  crypto_metrics.oemcrypto_load_device_rsa_key_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_load_keys_.Record(1.0, OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_load_test_keybox_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_load_test_rsa_key_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_open_session_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED, kLevelDefault);
  crypto_metrics.oemcrypto_refresh_keys_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_report_usage_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_rewrap_device_rsa_key_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_rewrap_device_rsa_key_30_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_security_level_
      .Record(1.0, kSecurityLevelL2, kLevelDefault);
  crypto_metrics.oemcrypto_security_patch_level_
      .Record(1.0, 123, kLevelDefault);
  crypto_metrics.oemcrypto_select_key_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_supports_usage_table_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED, kLevelDefault);
  crypto_metrics.oemcrypto_update_usage_table_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED);
  crypto_metrics.oemcrypto_wrap_keybox_
      .Record(1.0, OEMCrypto_ERROR_INIT_FAILED);

  // Internal OEMCrypto Metrics
  crypto_metrics.oemcrypto_initialization_mode_
      .Record(1.0, OEMCrypto_INITIALIZED_FORCING_L3);
  crypto_metrics.oemcrypto_l1_api_version_.Record(1.0, 12, 123);

  MetricsGroup actual_metrics;
  crypto_metrics.Serialize(&actual_metrics);

  // 61 EventMetric instances, 2 values each.
  ASSERT_EQ(122, actual_metrics.metric_size());

  // Spot check some metrics.
  EXPECT_EQ(
      "/drm/widevine/crypto_session/delete_all_usage_reports/time/count"
      "{error:0}",
      actual_metrics.metric(0).name());
  EXPECT_EQ(1, actual_metrics.metric(0).value().int_value());
  EXPECT_EQ(
      "/drm/widevine/oemcrypto/l1_api_version/mean{version:12&min_version:123}",
      actual_metrics.metric(121).name());
  EXPECT_EQ(1.0, actual_metrics.metric(121).value().double_value());

  // No subgroups should exist.
  EXPECT_EQ(0, actual_metrics.metric_sub_group_size());
}

}  // namespace metrics
}  // namespace wvcdm
