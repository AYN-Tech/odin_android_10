// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#include "policy_engine.h"

#include <limits.h>
#include <sstream>

#include "clock.h"
#include "log.h"
#include "properties.h"
#include "string_conversions.h"
#include "wv_cdm_constants.h"
#include "wv_cdm_event_listener.h"

using video_widevine::License;

namespace {

const int kCdmPolicyTimerDurationSeconds = 1;
const int kClockSkewDelta = 5;  // seconds

}  // namespace

namespace wvcdm {

PolicyEngine::PolicyEngine(CdmSessionId session_id,
                           WvCdmEventListener* event_listener,
                           CryptoSession* crypto_session)
    : license_state_(kLicenseStateInitial),
      license_start_time_(0),
      playback_start_time_(0),
      last_playback_time_(0),
      last_expiry_time_(0),
      grace_period_end_time_(0),
      last_expiry_time_set_(false),
      was_expired_on_load_(false),
      next_renewal_time_(0),
      last_recorded_current_time_(0),
      session_id_(session_id),
      event_listener_(event_listener),
      license_keys_(new LicenseKeys(crypto_session->GetSecurityLevel())),
      clock_(new Clock) {
  InitDevice(crypto_session);
}

PolicyEngine::~PolicyEngine() {}

bool PolicyEngine::CanDecryptContent(const KeyId& key_id) {
  if (license_keys_->IsContentKey(key_id)) {
    return license_keys_->CanDecryptContent(key_id);
  } else {
    LOGE("PolicyEngine::CanDecryptContent Key '%s' not in license.",
         b2a_hex(key_id).c_str());
    return false;
  }
}

CdmKeyStatus PolicyEngine::GetKeyStatus(const KeyId& key_id) {
  return license_keys_->GetKeyStatus(key_id);
}

void PolicyEngine::InitDevice(CryptoSession* crypto_session) {
  current_resolution_ = HDCP_UNSPECIFIED_VIDEO_RESOLUTION;
  next_device_check_ = 0;
  crypto_session_ = crypto_session;
}

void PolicyEngine::SetDeviceResolution(uint32_t width, uint32_t height) {
  current_resolution_ = width * height;
  CheckDeviceHdcpStatus();
}

void PolicyEngine::CheckDeviceHdcpStatusOnTimer(int64_t current_time) {
  if (current_time >= next_device_check_) {
    CheckDeviceHdcpStatus();
    next_device_check_ = current_time + HDCP_DEVICE_CHECK_INTERVAL;
  }
}

void PolicyEngine::CheckDeviceHdcpStatus() {
  if (!license_keys_->Empty()) {
    CryptoSession::HdcpCapability current_hdcp_level;
    CryptoSession::HdcpCapability ignored;
    CdmResponseType status =
        crypto_session_->GetHdcpCapabilities(&current_hdcp_level, &ignored);
    if (status != NO_ERROR) {
      current_hdcp_level = HDCP_NONE;
    }
    license_keys_->ApplyConstraints(current_resolution_, current_hdcp_level);
  }
}

void PolicyEngine::OnTimerEvent() {
  last_recorded_current_time_ += kCdmPolicyTimerDurationSeconds;
  int64_t current_time = GetCurrentTime();

  // If we have passed the grace period, the expiration will update.
  if (grace_period_end_time_ == 0 && HasPlaybackStarted(current_time)) {
    grace_period_end_time_ = playback_start_time_;
    NotifyExpirationUpdate(current_time);
  }

  // License expiration trumps all.
  if (HasLicenseOrPlaybackDurationExpired(current_time) &&
      license_state_ != kLicenseStateExpired) {
    license_state_ = kLicenseStateExpired;
    NotifyKeysChange(kKeyStatusExpired);
    return;
  }

  // Check device conditions that affect playability (HDCP, resolution)
  CheckDeviceHdcpStatusOnTimer(current_time);

  bool renewal_needed = false;

  // Test to determine if renewal should be attempted.
  switch (license_state_) {
    case kLicenseStateCanPlay: {
      if (HasRenewalDelayExpired(current_time)) {
        renewal_needed = true;
      }
      // HDCP may change, so force a check.
      NotifyKeysChange(kKeyStatusUsable);
      break;
    }

    case kLicenseStateNeedRenewal: {
      renewal_needed = true;
      break;
    }

    case kLicenseStateWaitingLicenseUpdate: {
      if (HasRenewalRetryIntervalExpired(current_time)) {
        renewal_needed = true;
      }
      break;
    }

    case kLicenseStatePending: {
      if (current_time >= license_start_time_) {
        license_state_ = kLicenseStateCanPlay;
        NotifyKeysChange(kKeyStatusUsable);
      }
      break;
    }

    case kLicenseStateInitial:
    case kLicenseStateExpired: {
      break;
    }

    default: {
      license_state_ = kLicenseStateExpired;
      NotifyKeysChange(kKeyStatusInternalError);
      break;
    }
  }

  if (renewal_needed) {
    UpdateRenewalRequest(current_time);
    if (event_listener_) event_listener_->OnSessionRenewalNeeded(session_id_);
  }
}

void PolicyEngine::SetLicense(const License& license) {
  license_id_.Clear();
  license_id_.CopyFrom(license.id());
  policy_.Clear();
  license_keys_->SetFromLicense(license);
  UpdateLicense(license);
}

void PolicyEngine::SetEntitledLicenseKeys(
    const std::vector<WidevinePsshData_EntitledKey>& entitled_keys) {
  license_keys_->SetEntitledKeys(entitled_keys);
}

void PolicyEngine::SetLicenseForRelease(const License& license) {
  license_id_.Clear();
  license_id_.CopyFrom(license.id());
  policy_.Clear();

  // Expire any old keys.
  NotifyKeysChange(kKeyStatusExpired);
  UpdateLicense(license);
}

void PolicyEngine::UpdateLicense(const License& license) {
  if (!license.has_policy()) return;

  if (kLicenseStateExpired == license_state_) {
    LOGD("PolicyEngine::UpdateLicense: updating an expired license");
  }

  policy_.MergeFrom(license.policy());

  // some basic license validation
  // license start time needs to be specified in the initial response
  if (!license.has_license_start_time()) return;

  // if renewal, discard license if version has not been updated
  if (license_state_ != kLicenseStateInitial) {
    if (license.id().version() > license_id_.version())
      license_id_.CopyFrom(license.id());
    else
      return;
  }

  // Update time information
  license_start_time_ = license.license_start_time();
  next_renewal_time_ = license_start_time_ + policy_.renewal_delay_seconds();

  int64_t current_time = GetCurrentTime();
  if (!policy_.can_play() ||
      HasLicenseOrPlaybackDurationExpired(current_time)) {
    license_state_ = kLicenseStateExpired;
    NotifyKeysChange(kKeyStatusExpired);
    return;
  }

  // Update state
  if (current_time >= license_start_time_) {
    license_state_ = kLicenseStateCanPlay;
    NotifyKeysChange(kKeyStatusUsable);
  } else {
    license_state_ = kLicenseStatePending;
    NotifyKeysChange(kKeyStatusUsableInFuture);
  }
  NotifyExpirationUpdate(current_time);
}

bool PolicyEngine::BeginDecryption() {
  if (playback_start_time_ == 0) {
    switch (license_state_) {
      case kLicenseStateCanPlay:
      case kLicenseStateNeedRenewal:
      case kLicenseStateWaitingLicenseUpdate:
        playback_start_time_ = GetCurrentTime();
        last_playback_time_ = playback_start_time_;
        if (policy_.play_start_grace_period_seconds() == 0)
          grace_period_end_time_ = playback_start_time_;

        if (policy_.renew_with_usage()) {
          license_state_ = kLicenseStateNeedRenewal;
        }
        NotifyExpirationUpdate(playback_start_time_);
        return true;
      case kLicenseStateInitial:
      case kLicenseStatePending:
      case kLicenseStateExpired:
      default:
        return false;
    }
  }
  else {
    return true;
  }
}

void PolicyEngine::DecryptionEvent() { last_playback_time_ = GetCurrentTime(); }

void PolicyEngine::NotifyResolution(uint32_t width, uint32_t height) {
  SetDeviceResolution(width, height);
}

void PolicyEngine::NotifySessionExpiration() {
  license_state_ = kLicenseStateExpired;
  NotifyKeysChange(kKeyStatusExpired);
}

CdmResponseType PolicyEngine::Query(CdmQueryMap* query_response) {
  std::stringstream ss;
  int64_t current_time = GetCurrentTime();

  if (license_state_ == kLicenseStateInitial) {
    query_response->clear();
    return NO_ERROR;
  }

  (*query_response)[QUERY_KEY_LICENSE_TYPE] =
      license_id_.type() == video_widevine::STREAMING ? QUERY_VALUE_STREAMING
                                                      : QUERY_VALUE_OFFLINE;
  (*query_response)[QUERY_KEY_PLAY_ALLOWED] =
      policy_.can_play() ? QUERY_VALUE_TRUE : QUERY_VALUE_FALSE;
  (*query_response)[QUERY_KEY_PERSIST_ALLOWED] =
      policy_.can_persist() ? QUERY_VALUE_TRUE : QUERY_VALUE_FALSE;
  (*query_response)[QUERY_KEY_RENEW_ALLOWED] =
      policy_.can_renew() ? QUERY_VALUE_TRUE : QUERY_VALUE_FALSE;
  ss << GetLicenseOrRentalDurationRemaining(current_time);
  (*query_response)[QUERY_KEY_LICENSE_DURATION_REMAINING] = ss.str();
  ss.str("");
  ss << GetPlaybackDurationRemaining(current_time);
  (*query_response)[QUERY_KEY_PLAYBACK_DURATION_REMAINING] = ss.str();
  (*query_response)[QUERY_KEY_RENEWAL_SERVER_URL] =
      policy_.renewal_server_url();

  return NO_ERROR;
}

CdmResponseType PolicyEngine::QueryKeyAllowedUsage(
    const KeyId& key_id, CdmKeyAllowedUsage* key_usage) {
  if (NULL == key_usage) {
    LOGE("PolicyEngine::QueryKeyAllowedUsage: no key_usage provided");
    return PARAMETER_NULL;
  }
  if (license_keys_->GetAllowedUsage(key_id, key_usage)) {
    return NO_ERROR;
  }
  return KEY_NOT_FOUND_1;
}

bool PolicyEngine::CanUseKeyForSecurityLevel(const KeyId& key_id) {
  return license_keys_->MeetsSecurityLevelConstraints(key_id);
}

bool PolicyEngine::GetSecondsSinceStarted(int64_t* seconds_since_started) {
  if (playback_start_time_ == 0) return false;

  *seconds_since_started = GetCurrentTime() - playback_start_time_;
  return (*seconds_since_started >= 0) ? true : false;
}

bool PolicyEngine::GetSecondsSinceLastPlayed(
    int64_t* seconds_since_last_played) {
  if (last_playback_time_ == 0) return false;

  *seconds_since_last_played = GetCurrentTime() - last_playback_time_;
  return (*seconds_since_last_played >= 0) ? true : false;
}

int64_t PolicyEngine::GetLicenseOrPlaybackDurationRemaining() {
  const int64_t current_time = GetCurrentTime();
  const int64_t expiry_time =
      GetExpiryTime(current_time,
                    /* ignore_soft_enforce_playback_duration */ false);
  if (expiry_time == NEVER_EXPIRES) return LLONG_MAX;
  if (expiry_time < current_time) return 0;
  return expiry_time - current_time;
}

void PolicyEngine::RestorePlaybackTimes(int64_t playback_start_time,
                                        int64_t last_playback_time,
                                        int64_t grace_period_end_time) {
  playback_start_time_ = (playback_start_time > 0) ? playback_start_time : 0;
  last_playback_time_ = (last_playback_time > 0) ? last_playback_time : 0;
  grace_period_end_time_ = grace_period_end_time;

  if (policy_.play_start_grace_period_seconds() != 0) {
    // If we are using grace period, we may need to override some of the values
    // given to us by OEMCrypto.  |grace_period_end_time| will be 0 if the grace
    // period has not expired (effectively playback has not begun).  Otherwise,
    // |grace_period_end_time| contains the playback start time we should use.
    playback_start_time_ = grace_period_end_time;
  }

  const int64_t current_time = GetCurrentTime();
  const int64_t expiry_time =
      GetExpiryTime(current_time,
                    /* ignore_soft_enforce_playback_duration */ true);
  was_expired_on_load_ =
      expiry_time != NEVER_EXPIRES && expiry_time < current_time;

  NotifyExpirationUpdate(current_time);
}

void PolicyEngine::UpdateRenewalRequest(int64_t current_time) {
  license_state_ = kLicenseStateWaitingLicenseUpdate;
  next_renewal_time_ = current_time + policy_.renewal_retry_interval_seconds();
}

bool PolicyEngine::HasLicenseOrPlaybackDurationExpired(int64_t current_time) {
  const int64_t expiry_time =
      GetExpiryTime(current_time,
                    /* ignore_soft_enforce_playback_duration */ false);
  return expiry_time != NEVER_EXPIRES && expiry_time <= current_time;
}

// For the policy time fields checked in the following methods, a value of 0
// indicates that there is no limit to the duration. If the fields are zero
// (including the hard limit) then these methods will return NEVER_EXPIRES.

int64_t PolicyEngine::GetHardLicenseExpiryTime() {
  return policy_.license_duration_seconds() > 0
             ? license_start_time_ + policy_.license_duration_seconds()
             : NEVER_EXPIRES;
}

int64_t PolicyEngine::GetRentalExpiryTime() {
  const int64_t hard_limit = GetHardLicenseExpiryTime();
  if (policy_.rental_duration_seconds() == 0) return hard_limit;
  const int64_t expiry_time =
      license_start_time_ + policy_.rental_duration_seconds();
  if (hard_limit == NEVER_EXPIRES) return expiry_time;
  return std::min(hard_limit, expiry_time);
}

int64_t PolicyEngine::GetExpiryTime(
    int64_t current_time, bool ignore_soft_enforce_playback_duration) {
  if (!HasPlaybackStarted(current_time)) return GetRentalExpiryTime();

  const int64_t hard_limit = GetHardLicenseExpiryTime();
  if (policy_.playback_duration_seconds() == 0) return hard_limit;
  if (!ignore_soft_enforce_playback_duration && !was_expired_on_load_ &&
      policy_.soft_enforce_playback_duration()) {
    return hard_limit;
  }
  const int64_t expiry_time =
      playback_start_time_ + policy_.playback_duration_seconds();

  if (hard_limit == NEVER_EXPIRES) return expiry_time;
  return std::min(hard_limit, expiry_time);
}

int64_t PolicyEngine::GetLicenseOrRentalDurationRemaining(
    int64_t current_time) {
  // This is only used in Query.  This should return the time remaining on
  // license_duration_seconds for streaming licenses and rental_duration_seconds
  // for offline licenses.
  if (HasLicenseOrPlaybackDurationExpired(current_time)) return 0;
  const int64_t license_expiry_time = GetRentalExpiryTime();
  if (license_expiry_time == NEVER_EXPIRES) return LLONG_MAX;
  if (license_expiry_time < current_time) return 0;
  const int64_t policy_license_duration = policy_.license_duration_seconds();
  if (policy_license_duration == NEVER_EXPIRES)
    return license_expiry_time - current_time;
  return std::min(license_expiry_time - current_time, policy_license_duration);
}

int64_t PolicyEngine::GetPlaybackDurationRemaining(int64_t current_time) {
  // This is only used in Query.  This should return playback_duration_seconds,
  // or the time remaining on it if playing.
  const int64_t playback_duration = policy_.playback_duration_seconds();
  if (playback_duration == 0) return LLONG_MAX;
  if (playback_start_time_ == 0) return playback_duration;

  const int64_t playback_expiry_time = playback_duration + playback_start_time_;
  if (playback_expiry_time < current_time) return 0;
  const int64_t policy_playback_duration = policy_.playback_duration_seconds();
  return std::min(playback_expiry_time - current_time,
                  policy_playback_duration);
}

bool PolicyEngine::HasRenewalDelayExpired(int64_t current_time) {
  return policy_.can_renew() && (policy_.renewal_delay_seconds() > 0) &&
         license_start_time_ + policy_.renewal_delay_seconds() <= current_time;
}

bool PolicyEngine::HasRenewalRecoveryDurationExpired(int64_t current_time) {
  // NOTE: Renewal Recovery Duration is currently not used.
  return (policy_.renewal_recovery_duration_seconds() > 0) &&
         license_start_time_ + policy_.renewal_recovery_duration_seconds() <=
             current_time;
}

bool PolicyEngine::HasRenewalRetryIntervalExpired(int64_t current_time) {
  return policy_.can_renew() &&
         (policy_.renewal_retry_interval_seconds() > 0) &&
         next_renewal_time_ <= current_time;
}

// Apply a key status to the current keys.
// If this represents a new key status, perform a notification callback.
// NOTE: if the new status is kKeyStatusUsable, the HDCP check may result in an
// override to kKeyStatusOutputNotAllowed.
void PolicyEngine::NotifyKeysChange(CdmKeyStatus new_status) {
  bool keys_changed;
  bool has_new_usable_key = false;
  if (new_status == kKeyStatusUsable) {
    CheckDeviceHdcpStatus();
  }
  keys_changed =
      license_keys_->ApplyStatusChange(new_status, &has_new_usable_key);
  if (event_listener_ && keys_changed) {
    CdmKeyStatusMap content_keys;
    license_keys_->ExtractKeyStatuses(&content_keys);
    event_listener_->OnSessionKeysChange(session_id_, content_keys,
                                         has_new_usable_key);
  }
}

void PolicyEngine::NotifyExpirationUpdate(int64_t current_time) {
  const int64_t expiry_time =
      GetExpiryTime(current_time,
                    /* ignore_soft_enforce_playback_duration */ false);
  if (!last_expiry_time_set_ || expiry_time != last_expiry_time_) {
    last_expiry_time_ = expiry_time;
    if (event_listener_)
      event_listener_->OnExpirationUpdate(session_id_, expiry_time);
  }
  last_expiry_time_set_ = true;
}

int64_t PolicyEngine::GetCurrentTime() {
  int64_t current_time = clock_->GetCurrentTime();
  if (current_time + kClockSkewDelta < last_recorded_current_time_)
    current_time = last_recorded_current_time_;
  else
    last_recorded_current_time_ = current_time;
  return current_time;
}

void PolicyEngine::set_clock(Clock* clock) { clock_.reset(clock); }

void PolicyEngine::SetSecurityLevelForTest(CdmSecurityLevel security_level) {
  license_keys_->SetSecurityLevelForTest(security_level);
}

}  // namespace wvcdm
