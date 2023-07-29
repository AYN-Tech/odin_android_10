// Copyright 2016 Google Inc. All Rights Reserved.

#include "metrics_collections.h"

#include <algorithm>

#include "log.h"
#include "metrics.pb.h"

using ::drm_metrics::Attributes;
using ::drm_metrics::WvCdmMetrics;
using ::google::protobuf::RepeatedPtrField;
using ::wvcdm::metrics::EventMetric;

namespace {

// Helper struct for comparing session ids.
struct CompareSessionIds {
  const std::string &target_;

  CompareSessionIds(const wvcdm::CdmSessionId &target) : target_(target){};

  bool operator()(const std::shared_ptr<wvcdm::metrics::SessionMetrics> metrics)
      const {
    return metrics->GetSessionId() == target_;
  }
};

}  // anonymous namespace

namespace wvcdm {
namespace metrics {

void CryptoMetrics::Serialize(WvCdmMetrics::CryptoMetrics *crypto_metrics)
    const {
  /* CRYPTO SESSION */
  crypto_metrics->set_allocated_crypto_session_security_level(
      crypto_session_security_level_.ToProto());
  crypto_session_delete_all_usage_reports_.ToProto(
      crypto_metrics->mutable_crypto_session_delete_all_usage_reports());
  crypto_session_delete_multiple_usage_information_.ToProto(
      crypto_metrics
          ->mutable_crypto_session_delete_multiple_usage_information());
  crypto_session_generic_decrypt_.ToProto(
      crypto_metrics->mutable_crypto_session_generic_decrypt_time_us());
  crypto_session_generic_encrypt_.ToProto(
      crypto_metrics->mutable_crypto_session_generic_encrypt_time_us());
  crypto_session_generic_sign_.ToProto(
      crypto_metrics->mutable_crypto_session_generic_sign_time_us());
  crypto_session_generic_verify_.ToProto(
      crypto_metrics->mutable_crypto_session_generic_verify_time_us());
  crypto_session_get_device_unique_id_.ToProto(
      crypto_metrics->mutable_crypto_session_get_device_unique_id());
  crypto_session_get_token_.ToProto(
      crypto_metrics->mutable_crypto_session_get_token());
  crypto_metrics->set_allocated_crypto_session_life_span(
      crypto_session_life_span_.ToProto());
  crypto_session_load_certificate_private_key_.ToProto(
      crypto_metrics
          ->mutable_crypto_session_load_certificate_private_key_time_us());
  crypto_session_open_.ToProto(
      crypto_metrics->mutable_crypto_session_open_time_us());
  crypto_metrics->set_allocated_crypto_session_system_id(
      crypto_session_system_id_.ToProto());
  crypto_session_update_usage_information_.ToProto(
      crypto_metrics
          ->mutable_crypto_session_update_usage_information_time_us());
  crypto_metrics->set_allocated_crypto_session_usage_information_support(
      crypto_session_usage_information_support_.ToProto());

  /* USAGE TABLE HEADER */
  usage_table_header_add_entry_.ToProto(
      crypto_metrics->mutable_usage_table_header_add_entry());
  usage_table_header_delete_entry_.ToProto(
      crypto_metrics->mutable_usage_table_header_delete_entry());
  usage_table_header_update_entry_.ToProto(
      crypto_metrics->mutable_usage_table_header_update_entry_time_us());
  usage_table_header_load_entry_.ToProto(
      crypto_metrics->mutable_usage_table_header_load_entry());
  crypto_metrics->set_allocated_usage_table_header_initial_size(
      usage_table_header_initial_size_.ToProto());

  /* OEMCRYPTO */
  crypto_metrics->set_allocated_oemcrypto_api_version(
      oemcrypto_api_version_.ToProto());
  oemcrypto_close_session_.ToProto(
      crypto_metrics->mutable_oemcrypto_close_session());
  oemcrypto_copy_buffer_.ToProto(
      crypto_metrics->mutable_oemcrypto_copy_buffer_time_us());
  crypto_metrics->set_allocated_oemcrypto_current_hdcp_capability(
      oemcrypto_current_hdcp_capability_.ToProto());
  oemcrypto_deactivate_usage_entry_.ToProto(
      crypto_metrics->mutable_oemcrypto_deactivate_usage_entry());
  oemcrypto_decrypt_cenc_.ToProto(
      crypto_metrics->mutable_oemcrypto_decrypt_cenc_time_us());
  oemcrypto_delete_usage_entry_.ToProto(
      crypto_metrics->mutable_oemcrypto_delete_usage_entry());
  oemcrypto_delete_usage_table_.ToProto(
      crypto_metrics->mutable_oemcrypto_delete_usage_table());
  oemcrypto_derive_keys_from_session_key_.ToProto(
      crypto_metrics->mutable_oemcrypto_derive_keys_from_session_key_time_us());
  oemcrypto_force_delete_usage_entry_.ToProto(
      crypto_metrics->mutable_oemcrypto_force_delete_usage_entry());
  oemcrypto_generate_derived_keys_.ToProto(
      crypto_metrics->mutable_oemcrypto_generate_derived_keys_time_us());
  oemcrypto_generate_nonce_.ToProto(
      crypto_metrics->mutable_oemcrypto_generate_nonce());
  oemcrypto_generate_rsa_signature_.ToProto(
      crypto_metrics->mutable_oemcrypto_generate_rsa_signature_time_us());
  oemcrypto_generate_signature_.ToProto(
      crypto_metrics->mutable_oemcrypto_generate_signature_time_us());
  oemcrypto_generic_decrypt_.ToProto(
      crypto_metrics->mutable_oemcrypto_generic_decrypt_time_us());
  oemcrypto_generic_encrypt_.ToProto(
      crypto_metrics->mutable_oemcrypto_generic_encrypt_time_us());
  oemcrypto_generic_sign_.ToProto(
      crypto_metrics->mutable_oemcrypto_generic_sign_time_us());
  oemcrypto_generic_verify_.ToProto(
      crypto_metrics->mutable_oemcrypto_generic_verify_time_us());
  oemcrypto_get_device_id_.ToProto(
      crypto_metrics->mutable_oemcrypto_get_device_id());
  oemcrypto_get_key_data_.ToProto(
      crypto_metrics->mutable_oemcrypto_get_key_data_time_us());
  oemcrypto_get_oem_public_certificate_.ToProto(
      crypto_metrics->mutable_oemcrypto_get_oem_public_certificate());
  oemcrypto_get_random_.ToProto(crypto_metrics->mutable_oemcrypto_get_random());
  oemcrypto_initialize_.ToProto(
      crypto_metrics->mutable_oemcrypto_initialize_time_us());
  crypto_metrics->set_allocated_oemcrypto_is_anti_rollback_hw_present(
      oemcrypto_is_anti_rollback_hw_present_.ToProto());
  crypto_metrics->set_allocated_oemcrypto_is_keybox_valid(
      oemcrypto_is_keybox_valid_.ToProto());
  oemcrypto_load_device_rsa_key_.ToProto(
      crypto_metrics->mutable_oemcrypto_load_device_rsa_key_time_us());
  oemcrypto_load_entitled_keys_.ToProto(
      crypto_metrics->mutable_oemcrypto_load_entitled_keys_time_us());
  oemcrypto_load_keys_.ToProto(
      crypto_metrics->mutable_oemcrypto_load_keys_time_us());
  crypto_metrics->set_allocated_oemcrypto_max_hdcp_capability(
      oemcrypto_max_hdcp_capability_.ToProto());
  crypto_metrics->set_allocated_oemcrypto_max_number_of_sessions(
      oemcrypto_max_number_of_sessions_.ToProto());
  crypto_metrics->set_allocated_oemcrypto_number_of_open_sessions(
      oemcrypto_number_of_open_sessions_.ToProto());
  crypto_metrics->set_allocated_oemcrypto_provisioning_method(
      oemcrypto_provisioning_method_.ToProto());
  oemcrypto_refresh_keys_.ToProto(
      crypto_metrics->mutable_oemcrypto_refresh_keys_time_us());
  oemcrypto_report_usage_.ToProto(
      crypto_metrics->mutable_oemcrypto_report_usage());
  oemcrypto_rewrap_device_rsa_key_.ToProto(
      crypto_metrics->mutable_oemcrypto_rewrap_device_rsa_key_time_us());
  oemcrypto_rewrap_device_rsa_key_30_.ToProto(
      crypto_metrics->mutable_oemcrypto_rewrap_device_rsa_key_30_time_us());
  crypto_metrics->set_allocated_oemcrypto_security_patch_level(
      oemcrypto_security_patch_level_.ToProto());
  oemcrypto_select_key_.ToProto(
      crypto_metrics->mutable_oemcrypto_select_key_time_us());
  crypto_metrics->set_allocated_oemcrypto_usage_table_support(
      oemcrypto_usage_table_support_.ToProto());
  oemcrypto_update_usage_table_.ToProto(
      crypto_metrics->mutable_oemcrypto_update_usage_table());
  oemcrypto_update_usage_entry_.ToProto(
      crypto_metrics->mutable_oemcrypto_update_usage_entry());
  oemcrypto_create_usage_table_header_.ToProto(
      crypto_metrics->mutable_oemcrypto_create_usage_table_header());
  oemcrypto_load_usage_table_header_.ToProto(
      crypto_metrics->mutable_oemcrypto_load_usage_table_header());
  oemcrypto_shrink_usage_table_header_.ToProto(
      crypto_metrics->mutable_oemcrypto_shrink_usage_table_header());
  oemcrypto_create_new_usage_entry_.ToProto(
      crypto_metrics->mutable_oemcrypto_create_new_usage_entry());
  oemcrypto_load_usage_entry_.ToProto(
      crypto_metrics->mutable_oemcrypto_load_usage_entry());
  oemcrypto_move_entry_.ToProto(
      crypto_metrics->mutable_oemcrypto_move_entry());
  oemcrypto_create_old_usage_entry_.ToProto(
      crypto_metrics->mutable_oemcrypto_create_old_usage_entry());
  oemcrypto_copy_old_usage_entry_.ToProto(
      crypto_metrics->mutable_oemcrypto_copy_old_usage_entry());
  crypto_metrics->set_allocated_oemcrypto_set_sandbox(
      oemcrypto_set_sandbox_.ToProto());
  oemcrypto_set_decrypt_hash_.ToProto(
      crypto_metrics->mutable_oemcrypto_set_decrypt_hash());
  crypto_metrics->set_allocated_oemcrypto_resource_rating_tier(
      oemcrypto_resource_rating_tier_.ToProto());
}

SessionMetrics::SessionMetrics() : session_id_(""), completed_(false) {}

void SessionMetrics::Serialize(WvCdmMetrics::SessionMetrics *session_metrics)
    const {
  SerializeSessionMetrics(session_metrics);
  crypto_metrics_.Serialize(session_metrics->mutable_crypto_metrics());
}

void SessionMetrics::SerializeSessionMetrics(
    WvCdmMetrics::SessionMetrics *session_metrics) const {
  // If the session id was set, add it to the metrics. It's possible that
  // it's not set in some circumstances such as when provisioning is needed.
  if (!session_id_.empty()) {
    session_metrics->mutable_session_id()->set_string_value(session_id_);
  }
  session_metrics->set_allocated_cdm_session_life_span_ms(
      cdm_session_life_span_.ToProto());
  cdm_session_renew_key_.ToProto(
      session_metrics->mutable_cdm_session_renew_key_time_us());
  cdm_session_restore_offline_session_.ToProto(
      session_metrics->mutable_cdm_session_restore_offline_session());
  cdm_session_restore_usage_session_.ToProto(
      session_metrics->mutable_cdm_session_restore_usage_session());
  cdm_session_license_request_latency_ms_.ToProto(
      session_metrics->mutable_cdm_session_license_request_latency_ms());
  session_metrics->set_allocated_oemcrypto_build_info(
      oemcrypto_build_info_.ToProto());
  session_metrics->set_allocated_license_sdk_version(
      license_sdk_version_.ToProto());
  session_metrics->set_allocated_license_service_version(
      license_service_version_.ToProto());
}

OemCryptoDynamicAdapterMetrics::OemCryptoDynamicAdapterMetrics()
    : level3_oemcrypto_initialization_error_(),
      oemcrypto_initialization_mode_(),
      previous_oemcrypto_initialization_failure_(),
      oemcrypto_l1_api_version_(),
      oemcrypto_l1_min_api_version_() {}

void OemCryptoDynamicAdapterMetrics::SetInitializationMode(
    OEMCryptoInitializationMode mode) {
  std::unique_lock<std::mutex> lock(adapter_lock_);
  oemcrypto_initialization_mode_.Record(mode);
}

void OemCryptoDynamicAdapterMetrics::SetLevel3InitializationError(
    OEMCryptoInitializationMode mode) {
  std::unique_lock<std::mutex> lock(adapter_lock_);
  level3_oemcrypto_initialization_error_.Record(mode);
}

void OemCryptoDynamicAdapterMetrics::SetPreviousInitializationFailure(
    OEMCryptoInitializationMode mode) {
  std::unique_lock<std::mutex> lock(adapter_lock_);
  previous_oemcrypto_initialization_failure_.Record(mode);
}

void OemCryptoDynamicAdapterMetrics::SetL1ApiVersion(uint32_t version) {
  std::unique_lock<std::mutex> lock(adapter_lock_);
  oemcrypto_l1_api_version_.Record(version);
}

void OemCryptoDynamicAdapterMetrics::SetL1MinApiVersion(uint32_t version) {
  std::unique_lock<std::mutex> lock(adapter_lock_);
  oemcrypto_l1_min_api_version_.Record(version);
}

void OemCryptoDynamicAdapterMetrics::Serialize(
    WvCdmMetrics::EngineMetrics *engine_metrics) const {
  std::unique_lock<std::mutex> lock(adapter_lock_);

  engine_metrics->set_allocated_level3_oemcrypto_initialization_error(
      oemcrypto_initialization_mode_.ToProto());
  engine_metrics->set_allocated_oemcrypto_initialization_mode(
      oemcrypto_initialization_mode_.ToProto());
  engine_metrics->set_allocated_previous_oemcrypto_initialization_failure(
      oemcrypto_initialization_mode_.ToProto());
  engine_metrics->set_allocated_oemcrypto_l1_api_version(
      oemcrypto_l1_api_version_.ToProto());
  engine_metrics->set_allocated_oemcrypto_l1_min_api_version(
      oemcrypto_l1_min_api_version_.ToProto());
}

void OemCryptoDynamicAdapterMetrics::Clear() {
  std::unique_lock<std::mutex> lock(adapter_lock_);

  level3_oemcrypto_initialization_error_.Clear();
  oemcrypto_initialization_mode_.Clear();
  previous_oemcrypto_initialization_failure_.Clear();
  oemcrypto_l1_api_version_.Clear();
  oemcrypto_l1_min_api_version_.Clear();
}

// This method returns a reference. This means that the destructor is never
// executed for the returned object.
OemCryptoDynamicAdapterMetrics &GetDynamicAdapterMetricsInstance() {
  // This is safe in C++ 11 since the initialization is guaranteed to run
  // only once regardless of multi-threaded access.
  static OemCryptoDynamicAdapterMetrics *adapter_metrics =
      new OemCryptoDynamicAdapterMetrics();
  return *adapter_metrics;
}

EngineMetrics::EngineMetrics() {
  life_span_internal_.Start();
}

EngineMetrics::~EngineMetrics() {
  std::unique_lock<std::mutex> lock(session_metrics_lock_);
  if (!active_session_metrics_list_.empty()
      || !completed_session_metrics_list_.empty()) {
    LOGV("EngineMetrics::~EngineMetrics. Session counts: "
         "active %d. completed %d.", active_session_metrics_list_.size(),
         completed_session_metrics_list_.size());
  }
}

std::shared_ptr<SessionMetrics> EngineMetrics::AddSession() {
  std::unique_lock<std::mutex> lock(session_metrics_lock_);
  active_session_metrics_list_.push_back(std::make_shared<SessionMetrics>());
  return active_session_metrics_list_.back();
}

void EngineMetrics::RemoveSession(wvcdm::CdmSessionId session_id) {
  std::unique_lock<std::mutex> lock(session_metrics_lock_);
  active_session_metrics_list_.erase(
      std::remove_if(active_session_metrics_list_.begin(),
                     active_session_metrics_list_.end(),
                     CompareSessionIds(session_id)),
      active_session_metrics_list_.end());
  completed_session_metrics_list_.erase(
      std::remove_if(completed_session_metrics_list_.begin(),
                     completed_session_metrics_list_.end(),
                     CompareSessionIds(session_id)),
      completed_session_metrics_list_.end());
}

void EngineMetrics::ConsolidateSessions() {
  auto completed_filter =
      [] (const std::shared_ptr<SessionMetrics>& session_metrics) {
        return session_metrics->IsCompleted();
      };

  std::unique_lock<std::mutex> lock(session_metrics_lock_);
  std::copy_if(active_session_metrics_list_.begin(),
               active_session_metrics_list_.end(),
               std::back_inserter(completed_session_metrics_list_),
               completed_filter);
  active_session_metrics_list_.erase(
      std::remove_if(active_session_metrics_list_.begin(),
                     active_session_metrics_list_.end(),
                     completed_filter),
      active_session_metrics_list_.end());

  // TODO(b/118664842): Add support to merge older metrics into one
  // consolidated metric.
  int excess_completed = completed_session_metrics_list_.size()
                         - kMaxCompletedSessions;
  if (excess_completed > 0) {
    completed_session_metrics_list_.erase(
        completed_session_metrics_list_.begin(),
        completed_session_metrics_list_.begin() + excess_completed);
  }
}

void EngineMetrics::Serialize(WvCdmMetrics *wv_metrics) const {
  std::unique_lock<std::mutex> lock(session_metrics_lock_);
  WvCdmMetrics::EngineMetrics *engine_metrics =
      wv_metrics->mutable_engine_metrics();
  // Serialize the most recent metrics from the OemCyrpto dynamic adapter.
  OemCryptoDynamicAdapterMetrics &adapter_metrics =
      GetDynamicAdapterMetricsInstance();
  adapter_metrics.Serialize(engine_metrics);
  if (!app_package_name_.empty()) {
    engine_metrics->mutable_app_package_name()->set_string_value(
        app_package_name_);
  }
  SerializeEngineMetrics(engine_metrics);

  for (auto it = completed_session_metrics_list_.begin();
       it != completed_session_metrics_list_.end(); it++) {
    (*it)->Serialize(wv_metrics->add_session_metrics());
  }
  for (auto it = active_session_metrics_list_.begin();
       it != active_session_metrics_list_.end(); it++) {
    (*it)->Serialize(wv_metrics->add_session_metrics());
  }
}

void EngineMetrics::SetAppPackageName(const std::string &app_package_name) {
  app_package_name_ = app_package_name;
}

void EngineMetrics::SerializeEngineMetrics(
    WvCdmMetrics::EngineMetrics *engine_metrics) const {
  // Set the engine lifespan at the time of serialization.
  engine_metrics->mutable_cdm_engine_life_span_ms()->set_int_value(
      life_span_internal_.AsMs());
  cdm_engine_add_key_.ToProto(
      engine_metrics->mutable_cdm_engine_add_key_time_us());
  engine_metrics->set_allocated_cdm_engine_cdm_version(
      cdm_engine_cdm_version_.ToProto());
  cdm_engine_close_session_.ToProto(
      engine_metrics->mutable_cdm_engine_close_session());
  engine_metrics->set_allocated_cdm_engine_creation_time_millis(
      cdm_engine_creation_time_millis_.ToProto());
  cdm_engine_decrypt_.ToProto(
      engine_metrics->mutable_cdm_engine_decrypt_time_us());
  cdm_engine_find_session_for_key_.ToProto(
      engine_metrics->mutable_cdm_engine_find_session_for_key());
  cdm_engine_generate_key_request_.ToProto(
      engine_metrics->mutable_cdm_engine_generate_key_request_time_us());
  cdm_engine_get_provisioning_request_.ToProto(
      engine_metrics->mutable_cdm_engine_get_provisioning_request_time_us());
  cdm_engine_get_secure_stop_ids_.ToProto(
      engine_metrics->mutable_cdm_engine_get_secure_stop_ids());
  cdm_engine_get_usage_info_.ToProto(
      engine_metrics->mutable_cdm_engine_get_usage_info_time_us());
  cdm_engine_handle_provisioning_response_.ToProto(
      engine_metrics
          ->mutable_cdm_engine_handle_provisioning_response_time_us());
  cdm_engine_open_key_set_session_.ToProto(
      engine_metrics->mutable_cdm_engine_open_key_set_session());
  cdm_engine_open_session_.ToProto(
      engine_metrics->mutable_cdm_engine_open_session());
  cdm_engine_remove_all_usage_info_.ToProto(
      engine_metrics->mutable_cdm_engine_remove_all_usage_info());
  cdm_engine_remove_usage_info_.ToProto(
      engine_metrics->mutable_cdm_engine_remove_usage_info());
  cdm_engine_query_key_status_.ToProto(
      engine_metrics->mutable_cdm_engine_query_key_status_time_us());
  cdm_engine_release_all_usage_info_.ToProto(
      engine_metrics->mutable_cdm_engine_release_all_usage_info());
  cdm_engine_release_usage_info_.ToProto(
      engine_metrics->mutable_cdm_engine_release_usage_info());
  cdm_engine_remove_keys_.ToProto(
      engine_metrics->mutable_cdm_engine_remove_keys());
  cdm_engine_restore_key_.ToProto(
      engine_metrics->mutable_cdm_engine_restore_key_time_us());
  cdm_engine_unprovision_.ToProto(
      engine_metrics->mutable_cdm_engine_unprovision());

  crypto_metrics_.Serialize(engine_metrics->mutable_crypto_metrics());
}

}  // namespace metrics
}  // namespace wvcdm
