
// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
#ifndef WVCDM_CORE_CDM_ENGINE_METRICS_DECORATOR_H_
#define WVCDM_CORE_CDM_ENGINE_METRICS_DECORATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "cdm_engine.h"
#include "cdm_session_map.h"
#include "certificate_provisioning.h"
#include "clock.h"
#include "crypto_session.h"
#include "disallow_copy_and_assign.h"
#include "file_store.h"
#include "initialization_data.h"
#include "metrics_collections.h"
#include "oemcrypto_adapter.h"
#include "properties.h"
#include "service_certificate.h"
#include "wv_cdm_constants.h"
#include "wv_cdm_types.h"

namespace wvcdm {

class CdmClientPropertySet;
class CdmEngineFactory;
class CdmSession;
class CryptoEngine;
class UsagePropertySet;
class WvCdmEventListener;

// This is a templated decorator class implementation of the CdmEngine.
// It captures metric information related to the inner CdmEngine class.
// Usage:
//    FileSystem* file_system;  // Construction of FileSystem object not shown.
//    std::shared_ptr<metrics::EngineMetrics> metrics(new EngineMetrics);
//    CdmEngine* e = new CdmEngineMetricsImpl<CdmEngine>(file_system, metrics);
template<class T>
class CdmEngineMetricsImpl : public T {
 public:
  // This constructor initializes the instance and takes ownership of |metrics|.
  // |file_system| and |metrics| must not be null.
  // |metrics| is used within the base class constructor. So, it must be
  // passed in as a dependency and provided to the base constructor.
  CdmEngineMetricsImpl(FileSystem* file_system,
                       std::shared_ptr<metrics::EngineMetrics> metrics,
                       const std::string& spoid = EMPTY_SPOID)
      : T(file_system, metrics, spoid), metrics_(metrics) {
    metrics_->cdm_engine_creation_time_millis_.Record(clock_.GetCurrentTime());
    std::string cdm_version;
    if(Properties::GetWVCdmVersion(&cdm_version)) {
      metrics_->cdm_engine_cdm_version_.Record(cdm_version);
    } else {
      metrics_->cdm_engine_cdm_version_.SetError(false);
    }
  }

  ~CdmEngineMetricsImpl() override {};

  bool GetMetricsSnapshot(drm_metrics::WvCdmMetrics *metrics) override {
    if (metrics == nullptr) return false;
    metrics_->Serialize(metrics);
    return true;
  }

  CdmResponseType OpenSession(
      const CdmKeySystem& key_system, CdmClientPropertySet* property_set,
      const CdmSessionId& forced_session_id, WvCdmEventListener* event_listener)
          override {
    CdmResponseType sts = T::OpenSession(
        key_system, property_set, forced_session_id, event_listener);
    metrics_->cdm_engine_open_session_.Increment(sts);
    return sts;
  }

  CdmResponseType OpenSession(
      const CdmKeySystem& key_system, CdmClientPropertySet* property_set,
      WvCdmEventListener* event_listener, CdmSessionId* session_id)
          override {
    CdmResponseType sts = T::OpenSession(
        key_system, property_set, event_listener, session_id);
    metrics_->cdm_engine_open_session_.Increment(sts);
    return sts;
  }

  CdmResponseType CloseSession(const CdmSessionId& session_id) override {
    CdmResponseType sts = T::CloseSession(session_id);
    metrics_->cdm_engine_close_session_.Increment(sts);
    return sts;
  }

  CdmResponseType OpenKeySetSession(
      const CdmKeySetId& key_set_id, CdmClientPropertySet* property_set,
      WvCdmEventListener* event_listener) override {
    CdmResponseType sts = T::OpenKeySetSession(key_set_id, property_set,
                                               event_listener);
    metrics_->cdm_engine_open_key_set_session_.Increment(sts);
    return sts;
  }

  CdmResponseType GenerateKeyRequest(
      const CdmSessionId& session_id, const CdmKeySetId& key_set_id,
      const InitializationData& init_data, const CdmLicenseType license_type,
      CdmAppParameterMap& app_parameters, CdmKeyRequest* key_request) override {
    CdmResponseType sts;
    M_TIME(sts = T::GenerateKeyRequest(session_id, key_set_id, init_data,
                                       license_type, app_parameters,
                                       key_request),
           metrics_, cdm_engine_generate_key_request_, sts, license_type);
    return sts;
  }

  CdmResponseType AddKey(
      const CdmSessionId& session_id, const CdmKeyResponse& key_data,
      CdmLicenseType* license_type, CdmKeySetId* key_set_id) override {
    if (license_type == nullptr) {
      LOGE("CdmEngine::AddKey: license_type cannot be null.");
      return PARAMETER_NULL;
    }

    CdmResponseType sts;
    M_TIME(sts = T::AddKey(session_id, key_data, license_type, key_set_id),
           metrics_, cdm_engine_add_key_, sts, *license_type);
    return sts;
  }


  CdmResponseType RestoreKey(const CdmSessionId& session_id,
                             const CdmKeySetId& key_set_id) override {
    CdmResponseType sts;
    M_TIME(sts = T::RestoreKey(session_id, key_set_id),
           metrics_, cdm_engine_restore_key_, sts);
    return sts;
  }

  CdmResponseType RemoveKeys(const CdmSessionId& session_id) override {
    CdmResponseType sts = T::RemoveKeys(session_id);
    metrics_->cdm_engine_remove_keys_.Increment(sts);
    return sts;
  }

  CdmResponseType QueryKeyStatus(const CdmSessionId& session_id,
                                 CdmQueryMap* query_response) override {
    CdmResponseType sts;
    M_TIME(sts = T::QueryKeyStatus(session_id, query_response),
           metrics_, cdm_engine_query_key_status_, sts);
    return sts;
  }

  CdmResponseType GetProvisioningRequest(
      CdmCertificateType cert_type, const std::string& cert_authority,
      const std::string& service_certificate,
      CdmProvisioningRequest* request, std::string* default_url) override {
    CdmResponseType sts;
    M_TIME(sts = T::GetProvisioningRequest(cert_type, cert_authority,
                                           service_certificate,
                                           request, default_url),
           metrics_, cdm_engine_get_provisioning_request_, sts);
    return sts;
  }

  CdmResponseType HandleProvisioningResponse(
      const CdmProvisioningResponse& response, std::string* cert,
      std::string* wrapped_key) override {
    CdmResponseType sts;
    M_TIME(
        sts = T::HandleProvisioningResponse(response, cert, wrapped_key),
        metrics_, cdm_engine_handle_provisioning_response_, sts);
    return sts;
  }


  CdmResponseType Unprovision(CdmSecurityLevel security_level) override {
    CdmResponseType sts = T::Unprovision(security_level);
    metrics_->cdm_engine_unprovision_.Increment(sts, security_level);
    return sts;
  }

  CdmResponseType GetUsageInfo(const std::string& app_id,
                               int* error_detail,
                               CdmUsageInfo* usage_info) override {
    CdmResponseType sts;
    int error_detail_alt;
    M_TIME(sts = T::GetUsageInfo(app_id, &error_detail_alt, usage_info),
           metrics_, cdm_engine_get_usage_info_, sts, error_detail_alt);
    if (error_detail != nullptr) {
      *error_detail = error_detail_alt;
    }
    return sts;
  }


  CdmResponseType GetUsageInfo(const std::string& app_id,
                               const CdmSecureStopId& ssid,
                               int* error_detail,
                               CdmUsageInfo* usage_info) override {
    CdmResponseType sts;
    int error_detail_alt;
    M_TIME(sts = T::GetUsageInfo(app_id, ssid, &error_detail_alt, usage_info),
           metrics_, cdm_engine_get_usage_info_, sts, error_detail_alt);
    if (error_detail != nullptr) {
      *error_detail = error_detail_alt;
    }
    return sts;
  }

  CdmResponseType RemoveAllUsageInfo(const std::string& app_id) override {
    CdmResponseType sts = T::RemoveAllUsageInfo(app_id);
    metrics_->cdm_engine_remove_all_usage_info_.Increment(sts);
    return sts;
  }

  CdmResponseType RemoveAllUsageInfo(const std::string& app_id,
                                     CdmSecurityLevel security_level) override {
    CdmResponseType sts = T::RemoveAllUsageInfo(app_id, security_level);
    metrics_->cdm_engine_remove_all_usage_info_.Increment(sts);
    return sts;
  }

  CdmResponseType RemoveUsageInfo(
      const std::string& app_id, const CdmSecureStopId& secure_stop_id)
          override {
    CdmResponseType sts = T::RemoveUsageInfo(app_id, secure_stop_id);
    metrics_->cdm_engine_remove_usage_info_.Increment(sts);
    return sts;
  }

  CdmResponseType ReleaseUsageInfo(const CdmUsageInfoReleaseMessage& message)
      override {
    CdmResponseType sts = T::ReleaseUsageInfo(message);
    metrics_->cdm_engine_release_usage_info_.Increment(sts);
    return sts;
  }

  CdmResponseType ListUsageIds(
      const std::string& app_id, CdmSecurityLevel security_level,
      std::vector<std::string>* ksids,
      std::vector<std::string>* provider_session_tokens) override {
    CdmResponseType sts = T::ListUsageIds(app_id, security_level,
                                          ksids, provider_session_tokens);
    metrics_->cdm_engine_get_secure_stop_ids_.Increment(sts);
    return sts;
  }


  bool FindSessionForKey(const KeyId& key_id, CdmSessionId* session_id)
      override {
    bool status =
        T::FindSessionForKey(key_id, session_id);
    metrics_->cdm_engine_find_session_for_key_.Increment(status);
    return status;
  }

  CdmResponseType Decrypt(const CdmSessionId& session_id,
                          const CdmDecryptionParameters& parameters) override {
    CdmResponseType sts;
    M_TIME(sts = T::Decrypt(session_id, parameters),
           metrics_, cdm_engine_decrypt_, sts,
           metrics::Pow2Bucket(parameters.encrypt_length));
    return sts;
  }

 private:
  std::shared_ptr<metrics::EngineMetrics> metrics_;
  Clock clock_;
};

}  // wvcdm namespace
#endif  // WVCDM_CORE_CDM_ENGINE_METRICS_DECORATOR_H_
