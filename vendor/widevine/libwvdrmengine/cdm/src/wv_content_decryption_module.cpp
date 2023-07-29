// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#include "wv_content_decryption_module.h"

#include "cdm_client_property_set.h"
#include "cdm_engine.h"
#include "cdm_engine_factory.h"
#include "initialization_data.h"
#include "license.h"
#include "log.h"
#include "metrics.pb.h"
#include "properties.h"
#include "service_certificate.h"
#include "string_conversions.h"
#include "wv_cdm_constants.h"
#include "wv_cdm_event_listener.h"

namespace {
const int kCdmPolicyTimerDurationSeconds = 1;
}

namespace wvcdm {

std::mutex WvContentDecryptionModule::session_sharing_id_generation_lock_;

WvContentDecryptionModule::WvContentDecryptionModule() {}

WvContentDecryptionModule::~WvContentDecryptionModule() {
  CloseAllCdms();
  DisablePolicyTimer();
}

bool WvContentDecryptionModule::IsSupported(const std::string& init_data_type) {
  return InitializationData(init_data_type).is_supported();
}

bool WvContentDecryptionModule::IsCenc(const std::string& init_data_type) {
  return InitializationData(init_data_type).is_cenc();
}

bool WvContentDecryptionModule::IsWebm(const std::string& init_data_type) {
  return InitializationData(init_data_type).is_webm();
}

bool WvContentDecryptionModule::IsHls(const std::string& init_data_type) {
  return InitializationData(init_data_type).is_hls();
}

bool WvContentDecryptionModule::IsAudio(const std::string& init_data_type) {
  return InitializationData(init_data_type).is_audio();
}

CdmResponseType WvContentDecryptionModule::OpenSession(
    const CdmKeySystem& key_system, CdmClientPropertySet* property_set,
    const CdmIdentifier& identifier, WvCdmEventListener* event_listener,
    CdmSessionId* session_id) {
  if (property_set && property_set->is_session_sharing_enabled()) {
    std::unique_lock<std::mutex> auto_lock(session_sharing_id_generation_lock_);
    if (property_set->session_sharing_id() == 0)
      property_set->set_session_sharing_id(GenerateSessionSharingId());
  }

  CdmEngine* cdm_engine = EnsureCdmForIdentifier(identifier);
  CdmResponseType sts = cdm_engine->OpenSession(key_system, property_set,
                                                event_listener, session_id);
  if (sts == NO_ERROR) {
    cdm_by_session_id_[*session_id] = cdm_engine;
  }
  return sts;
}

CdmResponseType WvContentDecryptionModule::CloseSession(
    const CdmSessionId& session_id) {
  LOGV("WvContentDecryptionModule::CloseSession. id: %s", session_id.c_str());
  CdmEngine* cdm_engine = GetCdmForSessionId(session_id);
  // TODO(rfrias): Avoid reusing the error codes from CdmEngine.
  if (!cdm_engine) return SESSION_NOT_FOUND_1;
  std::unique_lock<std::mutex> auto_lock(cdms_lock_);
  CdmResponseType sts = cdm_engine->CloseSession(session_id);
  if (sts == NO_ERROR) {
    cdm_by_session_id_.erase(session_id);
  }

  return sts;
}

bool WvContentDecryptionModule::IsOpenSession(const CdmSessionId& session_id) {
  CdmEngine* cdm_engine = GetCdmForSessionId(session_id);
  return cdm_engine && cdm_engine->IsOpenSession(session_id);
}

CdmResponseType WvContentDecryptionModule::GenerateKeyRequest(
    const CdmSessionId& session_id, const CdmKeySetId& key_set_id,
    const std::string& init_data_type, const CdmInitData& init_data,
    const CdmLicenseType license_type, CdmAppParameterMap& app_parameters,
    CdmClientPropertySet* property_set, const CdmIdentifier& identifier,
    CdmKeyRequest* key_request) {
  CdmEngine* cdm_engine = EnsureCdmForIdentifier(identifier);
  CdmResponseType sts;
  if (license_type == kLicenseTypeRelease) {
    sts = cdm_engine->OpenKeySetSession(key_set_id, property_set, NULL);
    if (sts != NO_ERROR) return sts;
    cdm_by_session_id_[key_set_id] = cdm_engine;
  }

  const SecurityLevel requested_security_level =
      property_set &&
      property_set->security_level().compare(
          wvcdm::QUERY_VALUE_SECURITY_LEVEL_L3) == 0
          ? wvcdm::kLevel3
          : wvcdm::kLevelDefault;

  std::string oec_version;
  sts = cdm_engine->QueryStatus(requested_security_level,
                                QUERY_KEY_OEMCRYPTO_API_VERSION,
                                &oec_version);
  if (sts != NO_ERROR) {
    return sts;
  }
  InitializationData initialization_data(init_data_type, init_data,
                                         oec_version);

  sts = cdm_engine->GenerateKeyRequest(session_id, key_set_id,
                                       initialization_data, license_type,
                                       app_parameters, key_request);
  switch (license_type) {
    case kLicenseTypeRelease:
      if (sts != KEY_MESSAGE) {
        cdm_engine->CloseKeySetSession(key_set_id);
        cdm_by_session_id_.erase(key_set_id);
      }
      break;
    default:
      if (sts == KEY_MESSAGE) EnablePolicyTimer();
      break;
  }
  return sts;
}

CdmResponseType WvContentDecryptionModule::AddKey(
    const CdmSessionId& session_id, const CdmKeyResponse& key_data,
    CdmKeySetId* key_set_id) {
  CdmEngine* cdm_engine = session_id.empty() ? GetCdmForSessionId(*key_set_id)
                                             : GetCdmForSessionId(session_id);
  if (!cdm_engine) return SESSION_NOT_FOUND_3;
  // Save key_set_id, as CDM will return an empty key_set_id on release
  CdmKeySetId release_key_set_id;
  if (session_id.empty() && key_set_id != NULL) {
    release_key_set_id = *key_set_id;
  }
  CdmResponseType sts;
  CdmLicenseType license_type;
  sts = cdm_engine->AddKey(session_id, key_data, &license_type, key_set_id);
  // Empty session id indicates license type release.
  if (sts == KEY_ADDED && session_id.empty()) {
    cdm_engine->CloseKeySetSession(release_key_set_id);
    cdm_by_session_id_.erase(release_key_set_id);
  }
  return sts;
}

CdmResponseType WvContentDecryptionModule::RestoreKey(
    const CdmSessionId& session_id, const CdmKeySetId& key_set_id) {
  CdmEngine* cdm_engine = GetCdmForSessionId(session_id);
  if (!cdm_engine) return SESSION_NOT_FOUND_4;
  CdmResponseType sts;
  sts = cdm_engine->RestoreKey(session_id, key_set_id);
  if (sts == KEY_ADDED) EnablePolicyTimer();
  return sts;
}

CdmResponseType WvContentDecryptionModule::RemoveKeys(
    const CdmSessionId& session_id) {
  CdmEngine* cdm_engine = GetCdmForSessionId(session_id);
  if (!cdm_engine) return SESSION_NOT_FOUND_5;
  CdmResponseType sts = cdm_engine->RemoveKeys(session_id);
  return sts;
}

CdmResponseType WvContentDecryptionModule::QueryStatus(
    SecurityLevel security_level, const std::string& key, std::string* value) {
  CdmEngine* cdm_engine = EnsureCdmForIdentifier(kDefaultCdmIdentifier);
  return cdm_engine->QueryStatus(security_level, key, value);
}

CdmResponseType WvContentDecryptionModule::QuerySessionStatus(
    const CdmSessionId& session_id, CdmQueryMap* key_info) {
  CdmEngine* cdm_engine = GetCdmForSessionId(session_id);
  if (!cdm_engine) return SESSION_NOT_FOUND_8;
  return cdm_engine->QuerySessionStatus(session_id, key_info);
}

CdmResponseType WvContentDecryptionModule::QueryKeyStatus(
    const CdmSessionId& session_id, CdmQueryMap* key_info) {
  CdmEngine* cdm_engine = GetCdmForSessionId(session_id);
  if (!cdm_engine) return SESSION_NOT_FOUND_9;
  CdmResponseType sts;
  sts = cdm_engine->QueryKeyStatus(session_id, key_info);
  return sts;
}

CdmResponseType WvContentDecryptionModule::QueryOemCryptoSessionId(
    const CdmSessionId& session_id, CdmQueryMap* response) {
  CdmEngine* cdm_engine = GetCdmForSessionId(session_id);
  if (!cdm_engine) return SESSION_NOT_FOUND_10;
  return cdm_engine->QueryOemCryptoSessionId(session_id, response);
}

bool WvContentDecryptionModule::IsSecurityLevelSupported(
    CdmSecurityLevel level) {
  return CdmEngine::IsSecurityLevelSupported(level);
}

CdmResponseType WvContentDecryptionModule::GetProvisioningRequest(
    CdmCertificateType cert_type, const std::string& cert_authority,
    const CdmIdentifier& identifier, const std::string& service_certificate,
    CdmProvisioningRequest* request, std::string* default_url) {
  CdmEngine* cdm_engine = EnsureCdmForIdentifier(identifier);
  return cdm_engine->GetProvisioningRequest(
      cert_type, cert_authority, service_certificate, request, default_url);
}

CdmResponseType WvContentDecryptionModule::HandleProvisioningResponse(
    const CdmIdentifier& identifier, CdmProvisioningResponse& response,
    std::string* cert, std::string* wrapped_key) {
  CdmEngine* cdm_engine = EnsureCdmForIdentifier(identifier);
  return cdm_engine->HandleProvisioningResponse(response, cert, wrapped_key);
}

CdmResponseType WvContentDecryptionModule::Unprovision(
    CdmSecurityLevel level, const CdmIdentifier& identifier) {
  CdmEngine* cdm_engine = EnsureCdmForIdentifier(identifier);
  return cdm_engine->Unprovision(level);
}

CdmResponseType WvContentDecryptionModule::GetUsageInfo(
    const std::string& app_id, const CdmIdentifier& identifier,
    CdmUsageInfo* usage_info) {
  CdmEngine* cdm_engine = EnsureCdmForIdentifier(identifier);
  int error_detail = NO_ERROR;
  return cdm_engine->GetUsageInfo(app_id, &error_detail, usage_info);
}

CdmResponseType WvContentDecryptionModule::GetUsageInfo(
    const std::string& app_id, const CdmSecureStopId& ssid,
    const CdmIdentifier& identifier, CdmUsageInfo* usage_info) {
  CdmEngine* cdm_engine = EnsureCdmForIdentifier(identifier);
  int error_detail = NO_ERROR;
  return cdm_engine->GetUsageInfo(app_id, ssid, &error_detail, usage_info);
}

CdmResponseType WvContentDecryptionModule::RemoveAllUsageInfo(
    const std::string& app_id, const CdmIdentifier& identifier) {
  CdmEngine* cdm_engine = EnsureCdmForIdentifier(identifier);
  return cdm_engine->RemoveAllUsageInfo(app_id);
}

CdmResponseType WvContentDecryptionModule::RemoveUsageInfo(
    const std::string& app_id,
    const CdmIdentifier& identifier,
    const CdmSecureStopId& secure_stop_id) {
  CdmEngine* cdm_engine = EnsureCdmForIdentifier(identifier);
  return cdm_engine->RemoveUsageInfo(app_id, secure_stop_id);
}

CdmResponseType WvContentDecryptionModule::ReleaseUsageInfo(
    const CdmUsageInfoReleaseMessage& message,
    const CdmIdentifier& identifier) {
  CdmEngine* cdm_engine = EnsureCdmForIdentifier(identifier);
  return cdm_engine->ReleaseUsageInfo(message);
}

CdmResponseType WvContentDecryptionModule::GetSecureStopIds(
    const std::string& app_id,
    const CdmIdentifier& identifier,
    std::vector<CdmSecureStopId>* ssids) {
  if (ssids == NULL) {
    LOGE("WvContentDecryptionModule::GetSecureStopIds: ssid destination not "
         "provided");
    return PARAMETER_NULL;
  }

  CdmEngine* cdm_engine = EnsureCdmForIdentifier(identifier);
  CdmResponseType sts = cdm_engine->ListUsageIds(app_id, kSecurityLevelL1,
                                                 NULL, ssids);
  std::vector<CdmSecureStopId> secure_stop_ids;
  CdmResponseType sts_l3 = cdm_engine->ListUsageIds(app_id, kSecurityLevelL3,
                                                    NULL, &secure_stop_ids);
  ssids->insert(ssids->end(), secure_stop_ids.begin(), secure_stop_ids.end());
  return sts_l3 != NO_ERROR ? sts_l3 : sts;
}

CdmResponseType WvContentDecryptionModule::Decrypt(
    const CdmSessionId& session_id, bool validate_key_id,
    const CdmDecryptionParameters& parameters) {
  // First find the CdmEngine that has the given session_id.  If we are using
  // key sharing, the shared session will still be in the same CdmEngine.
  CdmEngine* cdm_engine = GetCdmForSessionId(session_id);
  if (!cdm_engine) {
    LOGE("WvContentDecryptionModule::Decrypt: session not found: %s",
         session_id.c_str());
    return SESSION_NOT_FOUND_18;
  }

  CdmSessionId local_session_id = session_id;
  if (validate_key_id && Properties::GetSessionSharingId(session_id) != 0) {
    bool status =
        cdm_engine->FindSessionForKey(*parameters.key_id, &local_session_id);
    if (!status) {
      // key does not need to be loaded if clear lead/frame has a
      // single subsample. It does in all other cases.
      if (parameters.is_encrypted ||
          !(parameters.subsample_flags & OEMCrypto_FirstSubsample) ||
          !(parameters.subsample_flags & OEMCrypto_LastSubsample)) {
        return KEY_NOT_FOUND_IN_SESSION;
      }
    }
  }
  return cdm_engine->Decrypt(local_session_id, parameters);
}

void WvContentDecryptionModule::NotifyResolution(const CdmSessionId& session_id,
                                                 uint32_t width,
                                                 uint32_t height) {
  CdmEngine* cdm_engine = GetCdmForSessionId(session_id);
  if (!cdm_engine) return;
  cdm_engine->NotifyResolution(session_id, width, height);
}

bool WvContentDecryptionModule::IsValidServiceCertificate(
    const std::string& certificate) {
  ServiceCertificate cert;
  CdmResponseType status = cert.Init(certificate);
  if (status != NO_ERROR) return false;
  return cert.has_certificate();
}


CdmResponseType WvContentDecryptionModule::GetMetrics(
    const CdmIdentifier& identifier, drm_metrics::WvCdmMetrics* metrics) {
  if (!metrics) {
    return PARAMETER_NULL;
  }
  std::unique_lock<std::mutex> auto_lock(cdms_lock_);
  auto it = cdms_.find(identifier);
  if (it == cdms_.end()) {
    LOGE("WVContentDecryptionModule::GetMetrics. cdm_identifier not found");
    // TODO(blueeyes): Add a better error.
    return UNKNOWN_ERROR;
  }
  return it->second.cdm_engine->GetMetricsSnapshot(metrics) ?
      NO_ERROR : UNKNOWN_ERROR;
}

WvContentDecryptionModule::CdmInfo::CdmInfo()
    : cdm_engine(CdmEngineFactory::CreateCdmEngine(&file_system)) {}

CdmEngine* WvContentDecryptionModule::EnsureCdmForIdentifier(
    const CdmIdentifier& identifier) {
  std::unique_lock<std::mutex> auto_lock(cdms_lock_);
  if (cdms_.find(identifier) == cdms_.end()) {
    // Accessing the map entry will create a new instance using the default
    // constructor. We then need to provide it with two pieces of info: The
    // origin provided by the app and an identifier that uniquely identifies
    // this CDM. We concatenate all pieces of the CdmIdentifier in order to
    // create an ID that is unique to that identifier.
    cdms_[identifier].file_system.set_origin(identifier.origin);
    cdms_[identifier].file_system.set_identifier(identifier.spoid +
                                                 identifier.origin);
    cdms_[identifier].cdm_engine->SetAppPackageName(
        identifier.app_package_name);
  }
  CdmEngine* cdm_engine = cdms_[identifier].cdm_engine.get();

  return cdm_engine;
}

CdmEngine* WvContentDecryptionModule::GetCdmForSessionId(
    const std::string& session_id) {
  // Use find to avoid creating empty entries when not found.
  auto it = cdm_by_session_id_.find(session_id);
  if (it == cdm_by_session_id_.end()) return NULL;
  return it->second;
}

void WvContentDecryptionModule::CloseAllCdms() {
  std::unique_lock<std::mutex> auto_lock(cdms_lock_);

  for (auto it = cdms_.begin(); it != cdms_.end();) {
    it = cdms_.erase(it);
  }
}

CdmResponseType WvContentDecryptionModule::CloseCdm(
    const CdmIdentifier& cdm_identifier) {
  // The policy timer ultimately calls OnTimerEvent (which wants to
  // acquire cdms_lock_). Therefore, we cannot acquire cdms_lock_ and then the
  // policy_timer_lock_ (via DisablePolicyTimer) at the same time.
  // Acquire the cdms_lock_ first, in its own scope.
  bool cdms_empty = false;
  {
    std::unique_lock<std::mutex> auto_lock(cdms_lock_);
    auto it = cdms_.find(cdm_identifier);
    if (it == cdms_.end()) {
      LOGE("WVContentDecryptionModule::Close. cdm_identifier not found.");
      // TODO(blueeyes): Create a better error.
      return UNKNOWN_ERROR;
    }
    // Remove any sessions that point to this engine.
    for (auto session_it = cdm_by_session_id_.begin();
         session_it != cdm_by_session_id_.end();) {
      if (session_it->second == it->second.cdm_engine.get()) {
        session_it = cdm_by_session_id_.erase(session_it);
      } else {
        ++session_it;
      }
    }
    cdms_.erase(it);
    cdms_empty = cdms_.empty();
  }

  if (cdms_empty) {
    DisablePolicyTimer();
  }
  return NO_ERROR;
}

CdmResponseType WvContentDecryptionModule::SetDecryptHash(
    const std::string& hash_data, CdmSessionId* id) {
  if (id == nullptr) {
    LOGE("WVContentDecryptionModule::SetDecryptHash: |id| was not provided");
    return PARAMETER_NULL;
  }

  uint32_t frame_number;
  std::string hash;
  CdmResponseType res =
      CdmEngine::ParseDecryptHashString(hash_data, id, &frame_number, &hash);

  if (res != NO_ERROR) return res;

  CdmEngine* cdm_engine = GetCdmForSessionId(*id);

  if (!cdm_engine) {
    LOGE("WVContentDecryptionModule::SetDecryptHash: Unable to find CdmEngine");
    return SESSION_NOT_FOUND_20;
  }

  res = cdm_engine->SetDecryptHash(*id, frame_number, hash);
  return res;
}

CdmResponseType WvContentDecryptionModule::GetDecryptHashError(
    const CdmSessionId& session_id,
    std::string* hash_error_string) {
  CdmEngine* cdm_engine = GetCdmForSessionId(session_id);

  if (!cdm_engine) {
    LOGE("WVContentDecryptionModule::GetDecryptHashError: Unable to find "
         "CdmEngine");
    return SESSION_NOT_FOUND_20;
  }
  return cdm_engine->GetDecryptHashError(session_id, hash_error_string);
}

void WvContentDecryptionModule::EnablePolicyTimer() {
  std::unique_lock<std::mutex> auto_lock(policy_timer_lock_);
  if (!policy_timer_.IsRunning())
    policy_timer_.Start(this, kCdmPolicyTimerDurationSeconds);
}

void WvContentDecryptionModule::DisablePolicyTimer() {
  std::unique_lock<std::mutex> auto_lock(policy_timer_lock_);
  if (policy_timer_.IsRunning()) {
    policy_timer_.Stop();
  }
}

void WvContentDecryptionModule::OnTimerEvent() {
  std::unique_lock<std::mutex> auto_lock(cdms_lock_);
  for (auto it = cdms_.begin(); it != cdms_.end(); ++it) {
    it->second.cdm_engine->OnTimerEvent();
  }
}

uint32_t WvContentDecryptionModule::GenerateSessionSharingId() {
  static int next_session_sharing_id = 0;
  return ++next_session_sharing_id;
}

CdmResponseType WvContentDecryptionModule::ListStoredLicenses(
    CdmSecurityLevel security_level,
    const CdmIdentifier& identifier,
    std::vector<CdmKeySetId>* key_set_ids) {
  CdmEngine* cdm_engine = EnsureCdmForIdentifier(identifier);
  return cdm_engine->ListStoredLicenses(
      security_level, key_set_ids);
}

CdmResponseType WvContentDecryptionModule::GetOfflineLicenseState(
    const CdmKeySetId& key_set_id,
    CdmSecurityLevel security_level,
    const CdmIdentifier& identifier,
    CdmOfflineLicenseState* license_state) {
  CdmEngine* cdm_engine = EnsureCdmForIdentifier(identifier);
  return cdm_engine->GetOfflineLicenseState(
      key_set_id, security_level, license_state);
}

CdmResponseType WvContentDecryptionModule::RemoveOfflineLicense(
    const CdmKeySetId& key_set_id,
    CdmSecurityLevel security_level,
    const CdmIdentifier& identifier) {
  CdmEngine* cdm_engine = EnsureCdmForIdentifier(identifier);
  return cdm_engine->RemoveOfflineLicense(
      key_set_id, security_level);
}

}  // namespace wvcdm
