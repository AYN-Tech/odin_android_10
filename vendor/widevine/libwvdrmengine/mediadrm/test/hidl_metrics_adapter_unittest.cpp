//
// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//

// This file contains unit tests for the HidlMetricsAdapter.

#include "hidl_metrics_adapter.h"

#include <android/hardware/drm/1.1/types.h>
#include <sstream>
#include <utils/Log.h>

#include "gtest/gtest.h"
#include "metrics.pb.h"

using android::hardware::hidl_vec;
using android::hardware::drm::V1_1::DrmMetricGroup;
using drm_metrics::CounterMetric;
using drm_metrics::DistributionMetric;

namespace {

template<typename T>
std::string ToString(const T& attribute_or_value) {
  std::ostringstream os;
  switch (attribute_or_value.type) {
    case DrmMetricGroup::ValueType::DOUBLE_TYPE:
      os << "DOUBLE_TYPE. value: " << attribute_or_value.doubleValue;
      break;
    case DrmMetricGroup::ValueType::INT64_TYPE:
      os << "INT64_TYPE. value: " << attribute_or_value.int64Value;
      break;
    case DrmMetricGroup::ValueType::STRING_TYPE:
      os << "STRING_TYPE. value: " << attribute_or_value.stringValue;
      break;
    default:
      os << "UNKNOWN TYPE!!: " << (int64_t) attribute_or_value.type;
      break;
  }
  os << " (" << attribute_or_value.int64Value << ","
             << attribute_or_value.doubleValue << ",\""
             << attribute_or_value.stringValue << "\")";
  return os.str();
}

std::string ToString(const DrmMetricGroup::Metric& metric) {
  std::ostringstream os;
  os << "metric: " << metric.name << std::endl;
  os << "  attributes:" << std::endl;
  for (unsigned a = 0; a < metric.attributes.size(); a++) {
    os << "    " << metric.attributes[a].name << ". "
       << ToString(metric.attributes[a]) << std::endl;
  }
  os << "  values:" << std::endl;
  for (unsigned v = 0; v < metric.values.size(); v++) {
    os << "    " << metric.values[v].componentName << ". "
       << ToString(metric.values[v]) << std::endl;
  }
  return os.str();
}

std::string ToString(const hidl_vec<DrmMetricGroup>& metrics_vector) {
  std::ostringstream os;
  os << "hidl metrics..." << std::endl;
  os << "group count: " << metrics_vector.size() << std::endl;
  for (unsigned g = 0; g < metrics_vector.size(); g++) {
    os << "group " << g << ". metric count: "
       << metrics_vector[g].metrics.size() << std::endl;
    for (unsigned m = 0; m < metrics_vector[g].metrics.size(); m++) {
      const DrmMetricGroup::Metric metric = metrics_vector[g].metrics[m];
      os << ToString(metric);
    }
  }
  return os.str();
}

bool HasMetric(const DrmMetricGroup::Metric& expected,
               const DrmMetricGroup& actual_metrics) {
  DrmMetricGroup::Metric metric;
  auto it = std::find(actual_metrics.metrics.begin(),
                      actual_metrics.metrics.end(), expected);
  if (it == actual_metrics.metrics.end()) {
    ALOGE("COULDN'T FIND THE METRIC! %s", ToString(expected).c_str());
    for (auto it = actual_metrics.metrics.begin();
         it < actual_metrics.metrics.end(); it++) {
      if (expected.name == it->name) {
        ALOGE("Names match.");
      }
      if (expected.attributes == it->attributes) {
        ALOGE("attributes match.");
      }
      if (expected.values == it->values) {
        ALOGE("values match.");
      } else {
        ALOGE("values length match? %d, %d",
              expected.values.size(), it->values.size());
        if (expected.values.size() == it->values.size()) {
          for (unsigned int i = 0; i < expected.values.size(); i++) {
            ALOGE("value %d match? %d", i, expected.values[i] == it->values[i]);
            if (expected.values[i] != it->values[i]) {
              ALOGE("value component mismatch. %d. %s, %s",
                    i, expected.values[i].componentName.c_str(),
                    it->values[i].componentName.c_str());
            }
          }
        }
      }
    }
  } else {
    ALOGE("Found metric: %s", ToString(*it).c_str());
  }
  return it != actual_metrics.metrics.end();
}

}  // anonymous namespace

namespace wvcdm {

TEST(HidlMetricsAdapterTest, EmptyMetrics) {
  drm_metrics::WvCdmMetrics metrics_proto;
  hidl_vec<DrmMetricGroup> hidl_metrics;

  HidlMetricsAdapter::ToHidlMetrics(metrics_proto, &hidl_metrics);
  ASSERT_EQ(0U, hidl_metrics.size()) << ToString(hidl_metrics);
}

// Adds a metric from each type - Value, counter and distribution.
TEST(HidlMetricsAdapterTest, AllMetricTypes) {
  drm_metrics::WvCdmMetrics metrics_proto;

  // Value metric - error.
  metrics_proto
      .mutable_engine_metrics()
      ->mutable_crypto_metrics()
      ->mutable_crypto_session_security_level()
      ->set_error_code(7);
  DrmMetricGroup::Metric expected_value_metric_1 = {
        "crypto_session_security_level",
        {},
        { { "error_code", DrmMetricGroup::ValueType::INT64_TYPE, 7, 0, "" } }
      };

  // Value metric - integer.
  metrics_proto
      .mutable_engine_metrics()
      ->mutable_oemcrypto_initialization_mode()
      ->set_int_value(11);
  DrmMetricGroup::Metric expected_value_metric_2 = {
        "oemcrypto_initialization_mode",
        {},
        { { "value", DrmMetricGroup::ValueType::INT64_TYPE, 11, 0, "" } }
      };

  // Value metric - double.
  metrics_proto
      .mutable_engine_metrics()
      ->mutable_cdm_engine_life_span_ms()
      ->set_double_value(3.14159);
  DrmMetricGroup::Metric expected_value_metric_3 = {
        "cdm_engine_life_span_ms",
        {},
        { { "value", DrmMetricGroup::ValueType::DOUBLE_TYPE, 0, 3.14159, "" } }
      };

  // Value metric - string.
  metrics_proto
      .mutable_engine_metrics()
      ->mutable_cdm_engine_cdm_version()
      ->set_string_value("test");
  DrmMetricGroup::Metric expected_value_metric_4 = {
        "cdm_engine_cdm_version",
        {},
        { { "value", DrmMetricGroup::ValueType::STRING_TYPE, 0, 0, "test" } }
      };

  // Counter metric
  CounterMetric* counter = metrics_proto.mutable_engine_metrics()
      ->mutable_crypto_metrics()->add_crypto_session_get_token();
  counter->set_count(13);
  counter->mutable_attributes()->set_error_code(17);
  DrmMetricGroup::Metric expected_counter_metric_1 = {
        "crypto_session_get_token",
        { { "error_code", DrmMetricGroup::ValueType::INT64_TYPE, 17, 0, "" } },
        { { "count", DrmMetricGroup::ValueType::INT64_TYPE, 13, 0, "" } }
      };
  // Add a second counter.
  counter = metrics_proto.mutable_engine_metrics()
      ->mutable_crypto_metrics()->add_crypto_session_get_token();
  counter->set_count(19);
  counter->mutable_attributes()->set_error_code(23);
  DrmMetricGroup::Metric expected_counter_metric_2 = {
        "crypto_session_get_token",
        { { "error_code", DrmMetricGroup::ValueType::INT64_TYPE, 23, 0, "" } },
        { { "count", DrmMetricGroup::ValueType::INT64_TYPE, 19, 0, "" } }
      };

  // Distribution metric
  DistributionMetric* distribution = metrics_proto.mutable_engine_metrics()
      ->mutable_crypto_metrics()->add_crypto_session_open_time_us();
  distribution->set_min(1.0);
  distribution->set_max(1.2);
  distribution->set_mean(1.1);
  distribution->set_variance(.01);
  distribution->set_operation_count(2);
  distribution->mutable_attributes()->set_error_code(0);
  DrmMetricGroup::Metric expected_distribution_metric_1 = {
        "crypto_session_open_time_us",
        { { "error_code", DrmMetricGroup::ValueType::INT64_TYPE, 0, 0, "" } },
        { { "mean", DrmMetricGroup::ValueType::DOUBLE_TYPE, 0, 1.1f, "" },
          { "count", DrmMetricGroup::ValueType::INT64_TYPE, 2, 0, "" },
          { "min", DrmMetricGroup::ValueType::DOUBLE_TYPE, 0, 1.0, "" },
          { "max", DrmMetricGroup::ValueType::DOUBLE_TYPE, 0, 1.2f, "" },
          { "variance", DrmMetricGroup::ValueType::DOUBLE_TYPE, 0, 0.01, "" } }
      };
  // Add a second distribution
  distribution = metrics_proto.mutable_engine_metrics()
      ->mutable_crypto_metrics()->add_crypto_session_open_time_us();
  distribution->set_mean(0.7);
  distribution->set_operation_count(1);
  distribution->mutable_attributes()->set_error_code(27);
  DrmMetricGroup::Metric expected_distribution_metric_2 = {
        "crypto_session_open_time_us",
        { { "error_code", DrmMetricGroup::ValueType::INT64_TYPE, 27, 0, "" } },
        { { "mean", DrmMetricGroup::ValueType::DOUBLE_TYPE, 0, 0.7f, "" },
          { "count", DrmMetricGroup::ValueType::INT64_TYPE, 1, 0, "" } }
      };

  hidl_vec<DrmMetricGroup> hidl_metrics;

  HidlMetricsAdapter::ToHidlMetrics(metrics_proto, &hidl_metrics);
  ASSERT_EQ(1U, hidl_metrics.size());
  ASSERT_EQ(8U, hidl_metrics[0].metrics.size()) << ToString(hidl_metrics);

  EXPECT_TRUE(HasMetric(expected_value_metric_1, hidl_metrics[0]))
      << "Missing value_metric_1. " << ToString(expected_value_metric_1)
      << std::endl << "In metrics: " << ToString(hidl_metrics);
  EXPECT_TRUE(HasMetric(expected_value_metric_2, hidl_metrics[0]))
      << "Missing value_metric_2. " << ToString(expected_value_metric_2)
      << std::endl << "In metrics: " << ToString(hidl_metrics);
  EXPECT_TRUE(HasMetric(expected_value_metric_3, hidl_metrics[0]))
      << "Missing value_metric_3." << ToString(expected_value_metric_3)
      << std::endl << "In metrics: " << ToString(hidl_metrics);
  EXPECT_TRUE(HasMetric(expected_value_metric_4, hidl_metrics[0]))
      << "Missing value_metric_4. " << ToString(expected_value_metric_4)
      << std::endl << "In metrics: " << ToString(hidl_metrics);
  EXPECT_TRUE(HasMetric(expected_counter_metric_1, hidl_metrics[0]))
      << "Missing counter_metric_1. " << ToString(expected_counter_metric_1)
      << std::endl << "In metrics: " << ToString(hidl_metrics);
  EXPECT_TRUE(HasMetric(expected_counter_metric_2, hidl_metrics[0]))
      << "Missing counter_metric_2. " << ToString(expected_counter_metric_2)
      << std::endl << "In metrics: " << ToString(hidl_metrics);
  EXPECT_TRUE(HasMetric(expected_distribution_metric_1, hidl_metrics[0]))
      << "Missing distribution_metric_1. "
      << ToString(expected_distribution_metric_1) << std::endl
      << "In metrics: " << ToString(hidl_metrics);
  EXPECT_TRUE(HasMetric(expected_distribution_metric_2, hidl_metrics[0]))
      << "Missing distribution_metric_2. "
      << ToString(expected_distribution_metric_2) << std::endl
      << "In metrics: " << ToString(hidl_metrics);
}

// Add a single metric with all attributes to confirm that all attributes
// can be converted.
TEST(HidlMetricsAdapterTest, AddAllAttrbitues) {
  // Create a test attribute proto with all attributes set.
  drm_metrics::WvCdmMetrics metrics_proto;
  CounterMetric* counter = metrics_proto.mutable_engine_metrics()
      ->mutable_crypto_metrics()->add_crypto_session_get_token();
  counter->set_count(13);
  drm_metrics::Attributes* attributes = counter->mutable_attributes();
  attributes->set_error_code(17);
  attributes->set_error_code_bool(true);
  attributes->set_cdm_security_level(19);
  attributes->set_security_level(23);
  attributes->set_length(29);
  attributes->set_encryption_algorithm(31);
  attributes->set_signing_algorithm(37);
  attributes->set_oem_crypto_result(41);
  attributes->set_key_status_type(43);
  attributes->set_event_type(47);
  attributes->set_key_request_type(53);
  attributes->set_license_type(59);

  DrmMetricGroup::Metric expected_counter_metric = {
        "crypto_session_get_token",
        { { "error_code", DrmMetricGroup::ValueType::INT64_TYPE, 17, 0, "" },
          { "error_code_bool",
            DrmMetricGroup::ValueType::INT64_TYPE, true, 0, "" },
          { "cdm_security_level",
            DrmMetricGroup::ValueType::INT64_TYPE, 19, 0, "" },
          { "security_level",
            DrmMetricGroup::ValueType::INT64_TYPE, 23, 0, "" },
          { "length", DrmMetricGroup::ValueType::INT64_TYPE, 29, 0, "" },
          { "encryption_algorithm",
            DrmMetricGroup::ValueType::INT64_TYPE, 31, 0, "" },
          { "signing_algorithm",
            DrmMetricGroup::ValueType::INT64_TYPE, 37, 0, "" },
          { "oem_crypto_result",
            DrmMetricGroup::ValueType::INT64_TYPE, 41, 0, "" },
          { "key_status_type",
            DrmMetricGroup::ValueType::INT64_TYPE, 43, 0, "" },
          { "event_type", DrmMetricGroup::ValueType::INT64_TYPE, 47, 0, "" },
          { "key_request_type",
            DrmMetricGroup::ValueType::INT64_TYPE, 53, 0, "" },
          { "license_type",
            DrmMetricGroup::ValueType::INT64_TYPE, 59, 0, "" } },
        { { "count", DrmMetricGroup::ValueType::INT64_TYPE, 13, 0, "" } } };

  // Confirm that all of the attributes exist in the hidl data.
  hidl_vec<DrmMetricGroup> hidl_metrics;
  HidlMetricsAdapter::ToHidlMetrics(metrics_proto, &hidl_metrics);
  EXPECT_TRUE(HasMetric(expected_counter_metric, hidl_metrics[0]))
      << "Missing expected_counter_metrc. "
      << ToString(expected_counter_metric) << std::endl
      << "In metrics: " << ToString(hidl_metrics);
}

// Add all session and engine metrics to cofirm that all are converted.
// Only check the counts since other tests confirm that metrics are converted
// properly.
TEST(HidlMetricsAdapterTest, EngineAndSessionAllMetrics) {
  // Set all CryptoSession metrics.
  drm_metrics::WvCdmMetrics::CryptoMetrics crypto_metrics;
  crypto_metrics.mutable_crypto_session_security_level()->set_int_value(1);
  crypto_metrics.add_crypto_session_delete_all_usage_reports()->set_count(13);
  crypto_metrics.add_crypto_session_delete_multiple_usage_information
      ()->set_count(13);
  crypto_metrics.add_crypto_session_generic_decrypt_time_us()->set_min(1.0f);
  crypto_metrics.add_crypto_session_generic_encrypt_time_us()->set_min(1.0f);
  crypto_metrics.add_crypto_session_generic_sign_time_us()->set_min(1.0f);
  crypto_metrics.add_crypto_session_generic_verify_time_us()->set_min(1.0f);
  crypto_metrics.add_crypto_session_get_device_unique_id()->set_count(13);
  crypto_metrics.add_crypto_session_get_token()->set_count(13);
  crypto_metrics.mutable_crypto_session_life_span()->set_int_value(1);
  crypto_metrics.add_crypto_session_load_certificate_private_key_time_us
      ()->set_min(1.0f);
  crypto_metrics.add_crypto_session_open_time_us()->set_min(1.0f);
  crypto_metrics.mutable_crypto_session_system_id()->set_int_value(1);
  crypto_metrics.add_crypto_session_update_usage_information_time_us
      ()->set_min(1.0f);
  crypto_metrics.mutable_crypto_session_usage_information_support
      ()->set_int_value(1);

  // Usage Table Metrics
  crypto_metrics.mutable_usage_table_header_initial_size()->set_int_value(1);
  crypto_metrics.add_usage_table_header_add_entry()->set_count(13);
  crypto_metrics.add_usage_table_header_delete_entry()->set_count(13);
  crypto_metrics.add_usage_table_header_update_entry_time_us()->set_min(1.0f);
  crypto_metrics.add_usage_table_header_load_entry()->set_count(13);

    // OemCrypto metrics.
  crypto_metrics.mutable_oemcrypto_api_version()->set_int_value(1);
  crypto_metrics.add_oemcrypto_close_session()->set_count(13);
  crypto_metrics.add_oemcrypto_copy_buffer_time_us()->set_min(1.0f);
  crypto_metrics.mutable_oemcrypto_current_hdcp_capability()->set_int_value(1);
  crypto_metrics.add_oemcrypto_deactivate_usage_entry()->set_count(13);
  crypto_metrics.add_oemcrypto_decrypt_cenc_time_us()->set_min(1.0f);
  crypto_metrics.add_oemcrypto_delete_usage_entry()->set_count(13);
  crypto_metrics.add_oemcrypto_delete_usage_table()->set_count(13);
  crypto_metrics.add_oemcrypto_derive_keys_from_session_key_time_us
      ()->set_min(1.0f);
  crypto_metrics.add_oemcrypto_force_delete_usage_entry()->set_count(13);
  crypto_metrics.add_oemcrypto_generate_derived_keys_time_us()->set_min(1.0f);
  crypto_metrics.add_oemcrypto_generate_nonce()->set_count(13);
  crypto_metrics.add_oemcrypto_generate_rsa_signature_time_us()->set_min(1.0f);
  crypto_metrics.add_oemcrypto_generate_signature_time_us()->set_min(1.0f);
  crypto_metrics.add_oemcrypto_generic_decrypt_time_us()->set_min(1.0f);
  crypto_metrics.add_oemcrypto_generic_encrypt_time_us()->set_min(1.0f);
  crypto_metrics.add_oemcrypto_generic_sign_time_us()->set_min(1.0f);
  crypto_metrics.add_oemcrypto_generic_verify_time_us()->set_min(1.0f);
  crypto_metrics.add_oemcrypto_get_device_id()->set_count(13);
  crypto_metrics.add_oemcrypto_get_key_data_time_us()->set_min(1.0f);
  crypto_metrics.add_oemcrypto_get_oem_public_certificate()->set_count(13);
  crypto_metrics.add_oemcrypto_get_random()->set_count(13);
  crypto_metrics.add_oemcrypto_initialize_time_us()->set_min(1.0f);
  crypto_metrics.mutable_oemcrypto_is_anti_rollback_hw_present
      ()->set_int_value(1);
  crypto_metrics.mutable_oemcrypto_is_keybox_valid()->set_int_value(1);
  crypto_metrics.add_oemcrypto_load_device_rsa_key_time_us()->set_min(1.0f);
  crypto_metrics.add_oemcrypto_load_entitled_keys_time_us()->set_min(1.0f);
  crypto_metrics.add_oemcrypto_load_keys_time_us()->set_min(1.0f);
  crypto_metrics.mutable_oemcrypto_max_hdcp_capability()->set_int_value(1);
  crypto_metrics.mutable_oemcrypto_max_number_of_sessions()->set_int_value(1);
  crypto_metrics.mutable_oemcrypto_number_of_open_sessions()->set_int_value(1);
  crypto_metrics.mutable_oemcrypto_provisioning_method()->set_int_value(1);
  crypto_metrics.add_oemcrypto_refresh_keys_time_us()->set_min(1.0f);
  crypto_metrics.add_oemcrypto_report_usage()->set_count(13);
  crypto_metrics.add_oemcrypto_rewrap_device_rsa_key_time_us()->set_min(1.0f);
  crypto_metrics.add_oemcrypto_rewrap_device_rsa_key_30_time_us()
      ->set_min(1.0f);
  crypto_metrics.mutable_oemcrypto_security_patch_level()->set_int_value(1);
  crypto_metrics.add_oemcrypto_select_key_time_us()->set_min(1.0f);
  crypto_metrics.mutable_oemcrypto_usage_table_support()->set_int_value(1);
  crypto_metrics.add_oemcrypto_update_usage_table()->set_count(13);
  crypto_metrics.add_oemcrypto_update_usage_entry()->set_count(13);

  drm_metrics::WvCdmMetrics::SessionMetrics session_metrics;
  session_metrics.mutable_session_id()->set_string_value("test");
  *(session_metrics.mutable_crypto_metrics()) = crypto_metrics;
  session_metrics.mutable_cdm_session_life_span_ms()->set_double_value(1.0f);
  session_metrics.add_cdm_session_renew_key_time_us()->set_min(1.0f);
  session_metrics.add_cdm_session_restore_offline_session()->set_count(13);
  session_metrics.add_cdm_session_restore_usage_session()->set_count(13);
  session_metrics.add_cdm_session_license_request_latency_ms()->set_min(1.0);
  session_metrics.mutable_oemcrypto_build_info()->set_string_value("test");
  session_metrics.mutable_license_sdk_version()->set_string_value("test sdk");
  session_metrics.mutable_license_service_version()->set_string_value("test service");

  drm_metrics::WvCdmMetrics::EngineMetrics engine_metrics;
  *(engine_metrics.mutable_crypto_metrics()) = crypto_metrics;
  // OEMCrypto Initialize Metrics.
  engine_metrics.mutable_level3_oemcrypto_initialization_error()
      ->set_int_value(1);
  engine_metrics.mutable_oemcrypto_initialization_mode()->set_int_value(1);
  engine_metrics.mutable_previous_oemcrypto_initialization_failure()
      ->set_int_value(1);
  engine_metrics.mutable_oemcrypto_l1_api_version()->set_int_value(1);
  engine_metrics.mutable_oemcrypto_l1_min_api_version()->set_int_value(1);
  // CdmEngine Metrics.
  engine_metrics.mutable_app_package_name()->set_int_value(1);
  engine_metrics.add_cdm_engine_add_key_time_us()->set_min(1.0f);
  engine_metrics.mutable_cdm_engine_cdm_version()->set_int_value(1);
  engine_metrics.add_cdm_engine_close_session()->set_count(13);
  engine_metrics.mutable_cdm_engine_creation_time_millis()->set_int_value(1);
  engine_metrics.add_cdm_engine_decrypt_time_us()->set_min(1.0f);
  engine_metrics.add_cdm_engine_find_session_for_key()->set_count(13);
  engine_metrics.add_cdm_engine_generate_key_request_time_us()->set_min(1.0f);
  engine_metrics.add_cdm_engine_get_provisioning_request_time_us()
      ->set_min(1.0f);
  engine_metrics.add_cdm_engine_get_secure_stop_ids()->set_count(13);
  engine_metrics.add_cdm_engine_get_usage_info_time_us()->set_min(1.0f);
  engine_metrics.add_cdm_engine_handle_provisioning_response_time_us()
      ->set_min(1.0f);
  engine_metrics.mutable_cdm_engine_life_span_ms()->set_int_value(1);
  engine_metrics.add_cdm_engine_open_key_set_session()->set_count(13);
  engine_metrics.add_cdm_engine_open_session()->set_count(13);
  engine_metrics.add_cdm_engine_query_key_status_time_us()->set_min(1.0f);
  engine_metrics.add_cdm_engine_release_all_usage_info()->set_count(13);
  engine_metrics.add_cdm_engine_release_usage_info()->set_count(13);
  engine_metrics.add_cdm_engine_remove_all_usage_info()->set_count(13);
  engine_metrics.add_cdm_engine_remove_keys()->set_count(13);
  engine_metrics.add_cdm_engine_remove_usage_info()->set_count(13);
  engine_metrics.add_cdm_engine_restore_key_time_us()->set_min(1.0f);
  engine_metrics.add_cdm_engine_unprovision()->set_count(13);

  drm_metrics::WvCdmMetrics metrics_proto;
  *(metrics_proto.add_session_metrics()) = session_metrics;
  *(metrics_proto.mutable_engine_metrics()) = engine_metrics;

  hidl_vec<DrmMetricGroup> hidl_metrics;
  HidlMetricsAdapter::ToHidlMetrics(metrics_proto, &hidl_metrics);
  ASSERT_EQ(2U, hidl_metrics.size());
  EXPECT_EQ(89U, hidl_metrics[0].metrics.size()) << ToString(hidl_metrics);
  EXPECT_EQ(70U, hidl_metrics[1].metrics.size()) << ToString(hidl_metrics);
}

}  // namespace wvcdm
