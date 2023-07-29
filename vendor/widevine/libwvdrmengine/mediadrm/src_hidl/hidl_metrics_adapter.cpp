//
// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//

#include "hidl_metrics_adapter.h"

#include <android/hardware/drm/1.1/types.h>
#include "metrics.pb.h"

using android::hardware::hidl_vec;
using android::hardware::drm::V1_1::DrmMetricGroup;
using drm_metrics::CounterMetric;
using drm_metrics::DistributionMetric;
using google::protobuf::RepeatedPtrField;

namespace wvcdm {

namespace {

const char kAttributeErrorCode[] = "error_code";
const char kAttributeErrorCodeBool[] = "error_code_bool";
const char kAttributeCdmSecurityLevel[] = "cdm_security_level";
const char kAttributeSecurityLevel[] = "security_level";
const char kAttributeLength[] = "length";
const char kAttributeEncryptionAlgorithm[] = "encryption_algorithm";
const char kAttributeSigningAlgorithm[] = "signing_algorithm";
const char kAttributeOemCryptoResult[] = "oem_crypto_result";
const char kAttributeKeyStatusType[] = "key_status_type";
const char kAttributeEventType[] = "event_type";
const char kAttributeKeyRequestType[] = "key_request_type";
const char kAttributeLicenseType[] = "license_type";

template<typename T>
void SetValue(const T& value, DrmMetricGroup::Attribute* attribute);

template<>
void SetValue(const int& value, DrmMetricGroup::Attribute* attribute) {
  attribute->int64Value = value;
}
template<>
void SetValue(const bool& value, DrmMetricGroup::Attribute* attribute) {
  attribute->int64Value = value;
}
template<>
void SetValue(const unsigned int& value,
              DrmMetricGroup::Attribute* attribute) {
  attribute->int64Value = value;
}
template<>
__attribute__((unused))
void SetValue(const unsigned long& value,
              DrmMetricGroup::Attribute* attribute) {
  attribute->int64Value = value;
}
template<>
__attribute__((unused))
void SetValue(const unsigned long long& value,
              DrmMetricGroup::Attribute* attribute) {
  attribute->int64Value = value;
}

template<typename T>
void AddAttribute(
    const std::string& name, DrmMetricGroup::ValueType vt, T value,
    std::vector<DrmMetricGroup::Attribute>* attribute_vector) {
  // Set the default values.
  DrmMetricGroup::Attribute attribute = { name, vt, 0, 0, "" };
  SetValue(value, &attribute);
  attribute_vector->push_back(attribute);
}

}  // anonymous namespace

void HidlMetricsGroupBuilder::AddDistributions(
    const std::string& name,
    const RepeatedPtrField<drm_metrics::DistributionMetric>& distributions) {
  for (const auto& metric : distributions) {
    AddDistribution(name, metric);
  }
}

void HidlMetricsGroupBuilder::AddCounters(
    const std::string& name,
    const RepeatedPtrField<drm_metrics::CounterMetric>& counters) {
  for (const auto& counter : counters) {
    AddCounter(name, counter);
  }
}

void HidlMetricsGroupBuilder::AddDistribution(
    const std::string& name, const drm_metrics::DistributionMetric& distribution) {
  DrmMetricGroup::Metric metric;
  metric.name = name;
  AddAttributes(distribution.attributes(), &(metric.attributes));

  DrmMetricGroup::Value mean = {
      "mean", DrmMetricGroup::ValueType::DOUBLE_TYPE,
      0, distribution.mean(), "" };
  DrmMetricGroup::Value count = {
      "count", DrmMetricGroup::ValueType::INT64_TYPE,
      (int64_t) distribution.operation_count(), 0, "" };

  if (distribution.operation_count() == 1) {
    metric.values.resize(2);
    metric.values[0] = mean;
    metric.values[1] = count;
  } else {
    DrmMetricGroup::Value min = {
        "min", DrmMetricGroup::ValueType::DOUBLE_TYPE,
        0, distribution.min(), "" };
    DrmMetricGroup::Value max = {
        "max", DrmMetricGroup::ValueType::DOUBLE_TYPE,
        0, distribution.max(), "" };
    DrmMetricGroup::Value variance {
        "variance", DrmMetricGroup::ValueType::DOUBLE_TYPE,
        0, distribution.variance(), "" };
    metric.values.resize(5);
    metric.values[0] = mean;
    metric.values[1] = count;
    metric.values[2] = min;
    metric.values[3] = max;
    metric.values[4] = variance;
  }
  metrics_.push_back(metric);
}

void HidlMetricsGroupBuilder::AddCounter(
    const std::string& name, const drm_metrics::CounterMetric& counter) {
  DrmMetricGroup::Metric metric;
  metric.name = name;
  AddAttributes(counter.attributes(), &(metric.attributes));

  DrmMetricGroup::Value value = {
      "count", DrmMetricGroup::ValueType::INT64_TYPE, counter.count(), 0, "" };
  metric.values.resize(1);
  metric.values[0] = value;
  metrics_.push_back(metric);
}

void HidlMetricsGroupBuilder::AddValue(
    const std::string& name, const drm_metrics::ValueMetric& value_or_error) {
  DrmMetricGroup::Metric metric;
  DrmMetricGroup::Value value;

  metric.name = name;
  if (value_or_error.has_error_code()) {
    value = { "error_code", DrmMetricGroup::ValueType::INT64_TYPE,
              value_or_error.error_code(), 0, "" };
  } else if (value_or_error.has_int_value()) {
    value = { "value", DrmMetricGroup::ValueType::INT64_TYPE,
              value_or_error.int_value(), 0, "" };
  } else if (value_or_error.has_double_value()) {
    value = { "value", DrmMetricGroup::ValueType::DOUBLE_TYPE,
              0, value_or_error.double_value(), "" };
  } else if (value_or_error.has_string_value()) {
    value = { "value", DrmMetricGroup::ValueType::STRING_TYPE,
              0, 0, value_or_error.string_value() };
  } else {
    value = { "error", DrmMetricGroup::ValueType::STRING_TYPE,
              0, 0, "Unexpected value type." };
  }
  metric.values.resize(1);
  metric.values[0] = value;
  metrics_.push_back(metric);
}

void HidlMetricsGroupBuilder::AddAttributes(
    const drm_metrics::Attributes& attributes_proto,
    hidl_vec<DrmMetricGroup::Attribute>*
        attributes) {
  std::vector<DrmMetricGroup::Attribute>
      attribute_vector;
  if (attributes_proto.has_error_code()) {
    AddAttribute(
        kAttributeErrorCode,
        DrmMetricGroup::ValueType::INT64_TYPE,
        attributes_proto.error_code(), &attribute_vector);
  }
  if (attributes_proto.has_error_code_bool()) {
    AddAttribute(
        kAttributeErrorCodeBool,
        DrmMetricGroup::ValueType::INT64_TYPE,
        attributes_proto.error_code_bool(), &attribute_vector);
  }
  if (attributes_proto.has_cdm_security_level()) {
    AddAttribute(
        kAttributeCdmSecurityLevel,
        DrmMetricGroup::ValueType::INT64_TYPE,
        attributes_proto.cdm_security_level(), &attribute_vector);
  }
  if (attributes_proto.has_security_level()) {
    AddAttribute(
        kAttributeSecurityLevel,
        DrmMetricGroup::ValueType::INT64_TYPE,
        attributes_proto.security_level(), &attribute_vector);
  }
  if (attributes_proto.has_length()) {
    AddAttribute(
        kAttributeLength,
        DrmMetricGroup::ValueType::INT64_TYPE,
        attributes_proto.length(), &attribute_vector);
  }
  if (attributes_proto.has_encryption_algorithm()) {
    AddAttribute(
        kAttributeEncryptionAlgorithm,
        DrmMetricGroup::ValueType::INT64_TYPE,
        attributes_proto.encryption_algorithm(), &attribute_vector);
  }
  if (attributes_proto.has_signing_algorithm()) {
    AddAttribute(
        kAttributeSigningAlgorithm,
        DrmMetricGroup::ValueType::INT64_TYPE,
        attributes_proto.signing_algorithm(), &attribute_vector);
  }
  if (attributes_proto.has_oem_crypto_result()) {
    AddAttribute(
        kAttributeOemCryptoResult,
        DrmMetricGroup::ValueType::INT64_TYPE,
        attributes_proto.oem_crypto_result(), &attribute_vector);
  }
  if (attributes_proto.has_key_status_type()) {
    AddAttribute(
        kAttributeKeyStatusType,
        DrmMetricGroup::ValueType::INT64_TYPE,
        attributes_proto.key_status_type(), &attribute_vector);
  }
  if (attributes_proto.has_event_type()) {
    AddAttribute(
        kAttributeEventType,
        DrmMetricGroup::ValueType::INT64_TYPE,
        attributes_proto.event_type(), &attribute_vector);
  }
  if (attributes_proto.has_key_request_type()) {
    AddAttribute(
        kAttributeKeyRequestType,
        DrmMetricGroup::ValueType::INT64_TYPE,
        attributes_proto.key_request_type(), &attribute_vector);
  }
  if (attributes_proto.has_license_type()) {
    AddAttribute(
        kAttributeLicenseType,
        DrmMetricGroup::ValueType::INT64_TYPE,
        attributes_proto.license_type(), &attribute_vector);
  }

  *attributes = attribute_vector;
}

DrmMetricGroup HidlMetricsGroupBuilder::Build() {
  DrmMetricGroup metric_group;
  metric_group.metrics = metrics_;
  return metric_group;
}

HidlMetricsGroupBuilder::HidlMetricsGroupBuilder() {}

HidlMetricsAdapter::HidlMetricsAdapter() {}
HidlMetricsAdapter::~HidlMetricsAdapter() {}

void HidlMetricsAdapter::AddEngineMetrics(
    const drm_metrics::WvCdmMetrics::EngineMetrics& proto_metrics) {
  HidlMetricsGroupBuilder group_builder;
  AddCryptoMetrics(proto_metrics.crypto_metrics(), &group_builder);

  if (proto_metrics.has_oemcrypto_initialization_mode()) {
    group_builder.AddValue(
        "oemcrypto_initialization_mode",
        proto_metrics.oemcrypto_initialization_mode());
  }
  if (proto_metrics.has_oemcrypto_l1_api_version()) {
    group_builder.AddValue(
        "oemcrypto_l1_api_version",
        proto_metrics.oemcrypto_l1_api_version());
  }
  if (proto_metrics.has_oemcrypto_l1_min_api_version()) {
    group_builder.AddValue(
        "oemcrypto_l1_min_api_version",
        proto_metrics.oemcrypto_l1_min_api_version());
  }
  if (proto_metrics.has_app_package_name()) {
    group_builder.AddValue(
        "app_package_name",
        proto_metrics.app_package_name());
  }
  group_builder.AddDistributions(
      "cdm_engine_add_key_time_us",
      proto_metrics.cdm_engine_add_key_time_us());
  if (proto_metrics.has_cdm_engine_cdm_version()) {
    group_builder.AddValue(
        "cdm_engine_cdm_version",
        proto_metrics.cdm_engine_cdm_version());
  }
  group_builder.AddCounters(
      "cdm_engine_close_session",
      proto_metrics.cdm_engine_close_session());
  if (proto_metrics.has_cdm_engine_creation_time_millis()) {
    group_builder.AddValue(
        "cdm_engine_creation_time_millis",
        proto_metrics.cdm_engine_creation_time_millis());
  }
  group_builder.AddDistributions(
      "cdm_engine_decrypt_time_us",
      proto_metrics.cdm_engine_decrypt_time_us());
  group_builder.AddCounters(
      "cdm_engine_find_session_for_key",
      proto_metrics.cdm_engine_find_session_for_key());
  group_builder.AddDistributions(
      "cdm_engine_generate_key_request_time_us",
      proto_metrics.cdm_engine_generate_key_request_time_us());
  group_builder.AddDistributions(
      "cdm_engine_get_provisioning_request_time_us",
      proto_metrics.cdm_engine_get_provisioning_request_time_us());
  group_builder.AddCounters(
      "cdm_engine_get_secure_stop_ids",
      proto_metrics.cdm_engine_get_secure_stop_ids());
  group_builder.AddDistributions(
      "cdm_engine_get_usage_info_time_us",
      proto_metrics.cdm_engine_get_usage_info_time_us());
  group_builder.AddDistributions(
      "cdm_engine_handle_provisioning_response_time_us",
      proto_metrics.cdm_engine_handle_provisioning_response_time_us());
  if (proto_metrics.has_cdm_engine_life_span_ms()) {
    group_builder.AddValue(
        "cdm_engine_life_span_ms",
        proto_metrics.cdm_engine_life_span_ms());
  }
  group_builder.AddCounters(
      "cdm_engine_open_key_set_session",
      proto_metrics.cdm_engine_open_key_set_session());
  group_builder.AddCounters(
      "cdm_engine_open_session",
      proto_metrics.cdm_engine_open_session());
  group_builder.AddDistributions(
      "cdm_engine_query_key_status_time_us",
      proto_metrics.cdm_engine_query_key_status_time_us());
  group_builder.AddCounters(
      "cdm_engine_release_all_usage_info",
      proto_metrics.cdm_engine_release_all_usage_info());
  group_builder.AddCounters(
      "cdm_engine_release_usage_info",
      proto_metrics.cdm_engine_release_usage_info());
  group_builder.AddCounters(
      "cdm_engine_remove_all_usage_info",
      proto_metrics.cdm_engine_remove_all_usage_info());
  group_builder.AddCounters(
      "cdm_engine_remove_keys",
      proto_metrics.cdm_engine_remove_keys());
  group_builder.AddCounters(
      "cdm_engine_remove_usage_info",
      proto_metrics.cdm_engine_remove_usage_info());
  group_builder.AddDistributions(
      "cdm_engine_restore_key_time_us",
      proto_metrics.cdm_engine_restore_key_time_us());
  group_builder.AddCounters(
      "cdm_engine_unprovision",
      proto_metrics.cdm_engine_unprovision());
  if (proto_metrics.has_level3_oemcrypto_initialization_error()) {
    group_builder.AddValue(
        "level3_oemcrypto_initialization_error",
        proto_metrics.level3_oemcrypto_initialization_error());
  }
  if (proto_metrics.has_previous_oemcrypto_initialization_failure()) {
    group_builder.AddValue(
        "previous_oemcrypto_initialization_failure",
        proto_metrics.previous_oemcrypto_initialization_failure());
  }

  group_vector_.emplace_back(group_builder.Build());
}

void HidlMetricsAdapter::AddSessionMetrics(
    const drm_metrics::WvCdmMetrics::SessionMetrics& proto_metrics) {
  HidlMetricsGroupBuilder group_builder;
  AddCryptoMetrics(proto_metrics.crypto_metrics(), &group_builder);

  if (proto_metrics.has_session_id()) {
    group_builder.AddValue(
        "session_id",
        proto_metrics.session_id());
  }
  if (proto_metrics.has_cdm_session_life_span_ms()) {
    group_builder.AddValue(
        "cdm_session_life_span_ms",
        proto_metrics.cdm_session_life_span_ms());
  }
  group_builder.AddDistributions(
      "cdm_session_renew_key_time_us",
      proto_metrics.cdm_session_renew_key_time_us());
  group_builder.AddCounters(
      "cdm_session_restore_offline_session",
      proto_metrics.cdm_session_restore_offline_session());
  group_builder.AddCounters(
      "cdm_session_restore_usage_session",
      proto_metrics.cdm_session_restore_usage_session());
  group_builder.AddDistributions(
      "cdm_session_license_request_latency_ms",
      proto_metrics.cdm_session_license_request_latency_ms());
  if (proto_metrics.has_oemcrypto_build_info()) {
    group_builder.AddValue(
        "oemcrypto_build_info",
        proto_metrics.oemcrypto_build_info());
  }
  if (proto_metrics.has_license_sdk_version()) {
    group_builder.AddValue(
        "license_sdk_version",
        proto_metrics.license_sdk_version());
  }
  if (proto_metrics.has_license_service_version()) {
    group_builder.AddValue(
        "license_service_version",
        proto_metrics.license_service_version());
  }
  group_vector_.emplace_back(group_builder.Build());
}

void HidlMetricsAdapter::AddCryptoMetrics(
    const drm_metrics::WvCdmMetrics::CryptoMetrics& proto_metrics,
    HidlMetricsGroupBuilder* group_builder) {
  if (proto_metrics.has_crypto_session_security_level()) {
    group_builder->AddValue(
        "crypto_session_security_level",
        proto_metrics.crypto_session_security_level());
  }
  group_builder->AddCounters(
      "crypto_session_delete_all_usage_reports",
      proto_metrics.crypto_session_delete_all_usage_reports());
  group_builder->AddCounters(
      "crypto_session_delete_multiple_usage_information",
      proto_metrics.crypto_session_delete_multiple_usage_information());
  group_builder->AddDistributions(
      "crypto_session_generic_decrypt_time_us",
      proto_metrics.crypto_session_generic_decrypt_time_us());
  group_builder->AddDistributions(
      "crypto_session_generic_encrypt_time_us",
      proto_metrics.crypto_session_generic_encrypt_time_us());
  group_builder->AddDistributions(
      "crypto_session_generic_sign_time_us",
      proto_metrics.crypto_session_generic_sign_time_us());
  group_builder->AddDistributions(
      "crypto_session_generic_verify_time_us",
      proto_metrics.crypto_session_generic_verify_time_us());
  group_builder->AddCounters(
      "crypto_session_get_device_unique_id",
      proto_metrics.crypto_session_get_device_unique_id());
  group_builder->AddCounters(
      "crypto_session_get_token",
      proto_metrics.crypto_session_get_token());
  if (proto_metrics.has_crypto_session_life_span()) {
    group_builder->AddValue("crypto_session_life_span",
                            proto_metrics.crypto_session_life_span());
  }
  group_builder->AddDistributions(
      "crypto_session_load_certificate_private_key_time_us",
      proto_metrics.crypto_session_load_certificate_private_key_time_us());
  group_builder->AddDistributions(
      "crypto_session_open_time_us",
      proto_metrics.crypto_session_open_time_us());
  if (proto_metrics.has_crypto_session_system_id()) {
    group_builder->AddValue("crypto_session_system_id",
                            proto_metrics.crypto_session_system_id());
  }
  group_builder->AddDistributions(
      "crypto_session_update_usage_information_time_us",
      proto_metrics.crypto_session_update_usage_information_time_us());
  if (proto_metrics.has_crypto_session_usage_information_support()) {
    group_builder->AddValue("crypto_session_usage_information_support",
                            proto_metrics.crypto_session_usage_information_support());
  }
  if (proto_metrics.has_usage_table_header_initial_size()) {
    group_builder->AddValue("usage_table_header_initial_size",
                            proto_metrics.usage_table_header_initial_size());
  }
  group_builder->AddCounters(
      "usage_table_header_add_entry",
      proto_metrics.usage_table_header_add_entry());
  group_builder->AddCounters(
      "usage_table_header_delete_entry",
      proto_metrics.usage_table_header_delete_entry());
  group_builder->AddDistributions(
      "usage_table_header_update_entry_time_us",
      proto_metrics.usage_table_header_update_entry_time_us());
  group_builder->AddCounters(
      "usage_table_header_load_entry",
      proto_metrics.usage_table_header_load_entry());
  if (proto_metrics.has_oemcrypto_api_version()) {
    group_builder->AddValue("oemcrypto_api_version",
                            proto_metrics.oemcrypto_api_version());
  }
  group_builder->AddCounters(
      "oemcrypto_close_session",
      proto_metrics.oemcrypto_close_session());
  group_builder->AddDistributions(
      "oemcrypto_copy_buffer_time_us",
      proto_metrics.oemcrypto_copy_buffer_time_us());
  if (proto_metrics.has_oemcrypto_current_hdcp_capability()) {
    group_builder->AddValue("oemcrypto_current_hdcp_capability",
                            proto_metrics.oemcrypto_current_hdcp_capability());
  }
  group_builder->AddCounters(
      "oemcrypto_deactivate_usage_entry",
      proto_metrics.oemcrypto_deactivate_usage_entry());
  group_builder->AddDistributions(
      "oemcrypto_decrypt_cenc_time_us",
      proto_metrics.oemcrypto_decrypt_cenc_time_us());
  group_builder->AddCounters(
      "oemcrypto_delete_usage_entry",
      proto_metrics.oemcrypto_delete_usage_entry());
  group_builder->AddCounters(
      "oemcrypto_delete_usage_table",
      proto_metrics.oemcrypto_delete_usage_table());
  group_builder->AddDistributions(
      "oemcrypto_derive_keys_from_session_key_time_us",
      proto_metrics.oemcrypto_derive_keys_from_session_key_time_us());
  group_builder->AddCounters(
      "oemcrypto_force_delete_usage_entry",
      proto_metrics.oemcrypto_force_delete_usage_entry());
  group_builder->AddDistributions(
      "oemcrypto_generate_derived_keys_time_us",
      proto_metrics.oemcrypto_generate_derived_keys_time_us());
  group_builder->AddCounters(
      "oemcrypto_generate_nonce",
      proto_metrics.oemcrypto_generate_nonce());
  group_builder->AddDistributions(
      "oemcrypto_generate_rsa_signature_time_us",
      proto_metrics.oemcrypto_generate_rsa_signature_time_us());
  group_builder->AddDistributions(
      "oemcrypto_generate_signature_time_us",
      proto_metrics.oemcrypto_generate_signature_time_us());
  group_builder->AddDistributions(
      "oemcrypto_generic_decrypt_time_us",
      proto_metrics.oemcrypto_generic_decrypt_time_us());
  group_builder->AddDistributions(
      "oemcrypto_generic_encrypt_time_us",
      proto_metrics.oemcrypto_generic_encrypt_time_us());
  group_builder->AddDistributions(
      "oemcrypto_generic_sign_time_us",
      proto_metrics.oemcrypto_generic_sign_time_us());
  group_builder->AddDistributions(
      "oemcrypto_generic_verify_time_us",
      proto_metrics.oemcrypto_generic_verify_time_us());
  group_builder->AddCounters(
      "oemcrypto_get_device_id",
      proto_metrics.oemcrypto_get_device_id());
  group_builder->AddDistributions(
      "oemcrypto_get_key_data_time_us",
      proto_metrics.oemcrypto_get_key_data_time_us());
  group_builder->AddCounters(
      "oemcrypto_get_oem_public_certificate",
      proto_metrics.oemcrypto_get_oem_public_certificate());
  group_builder->AddCounters(
      "oemcrypto_get_random",
      proto_metrics.oemcrypto_get_random());
  group_builder->AddDistributions(
      "oemcrypto_initialize_time_us",
      proto_metrics.oemcrypto_initialize_time_us());
  if (proto_metrics.has_oemcrypto_is_anti_rollback_hw_present()) {
    group_builder->AddValue(
        "oemcrypto_is_anti_rollback_hw_present",
        proto_metrics.oemcrypto_is_anti_rollback_hw_present());
  }
  if (proto_metrics.has_oemcrypto_is_keybox_valid()) {
    group_builder->AddValue("oemcrypto_is_keybox_valid",
                            proto_metrics.oemcrypto_is_keybox_valid());
  }
  group_builder->AddDistributions(
      "oemcrypto_load_device_rsa_key_time_us",
      proto_metrics.oemcrypto_load_device_rsa_key_time_us());
  group_builder->AddDistributions(
      "oemcrypto_load_entitled_keys_time_us",
      proto_metrics.oemcrypto_load_entitled_keys_time_us());
  group_builder->AddDistributions(
      "oemcrypto_load_keys_time_us",
      proto_metrics.oemcrypto_load_keys_time_us());
  if (proto_metrics.has_oemcrypto_max_hdcp_capability()) {
    group_builder->AddValue("oemcrypto_max_hdcp_capability",
                            proto_metrics.oemcrypto_max_hdcp_capability());
  }
  if (proto_metrics.has_oemcrypto_max_number_of_sessions()) {
    group_builder->AddValue("oemcrypto_max_number_of_sessions",
                            proto_metrics.oemcrypto_max_number_of_sessions());
  }
  if (proto_metrics.has_oemcrypto_number_of_open_sessions()) {
    group_builder->AddValue("oemcrypto_number_of_open_sessions",
                            proto_metrics.oemcrypto_number_of_open_sessions());
  }
  if (proto_metrics.has_oemcrypto_provisioning_method()) {
    group_builder->AddValue("oemcrypto_provisioning_method",
                            proto_metrics.oemcrypto_provisioning_method());
  }
  group_builder->AddDistributions(
      "oemcrypto_refresh_keys_time_us",
      proto_metrics.oemcrypto_refresh_keys_time_us());
  group_builder->AddCounters(
      "oemcrypto_report_usage",
      proto_metrics.oemcrypto_report_usage());
  group_builder->AddDistributions(
      "oemcrypto_rewrap_device_rsa_key_time_us",
      proto_metrics.oemcrypto_rewrap_device_rsa_key_time_us());
  group_builder->AddDistributions(
      "oemcrypto_rewrap_device_rsa_key_30_time_us",
      proto_metrics.oemcrypto_rewrap_device_rsa_key_30_time_us());
  if (proto_metrics.has_oemcrypto_security_patch_level()) {
    group_builder->AddValue("oemcrypto_security_patch_level",
                            proto_metrics.oemcrypto_security_patch_level());
  }
  group_builder->AddDistributions(
      "oemcrypto_select_key_time_us",
      proto_metrics.oemcrypto_select_key_time_us());
  if (proto_metrics.has_oemcrypto_usage_table_support()) {
    group_builder->AddValue("oemcrypto_usage_table_support",
                            proto_metrics.oemcrypto_usage_table_support());
  }
  group_builder->AddCounters(
      "oemcrypto_update_usage_table",
      proto_metrics.oemcrypto_update_usage_table());
  group_builder->AddCounters(
      "oemcrypto_update_usage_entry",
      proto_metrics.oemcrypto_update_usage_entry());
}

const android::hardware::hidl_vec<
    DrmMetricGroup>
HidlMetricsAdapter::GetHidlGroupVector() {
  return group_vector_;
}

void HidlMetricsAdapter::ToHidlMetrics(
    const drm_metrics::WvCdmMetrics& proto_metrics,
    hidl_vec<DrmMetricGroup>* hidl_metrics) {
  // Convert the engine level metrics
  HidlMetricsAdapter adapter;

  if (proto_metrics.has_engine_metrics()) {
    adapter.AddEngineMetrics(proto_metrics.engine_metrics());
  }
  for (const auto& session_metrics : proto_metrics.session_metrics()) {
    adapter.AddSessionMetrics(session_metrics);
  }

  *hidl_metrics = adapter.GetHidlGroupVector();
}

}  // namespace wvcdm
