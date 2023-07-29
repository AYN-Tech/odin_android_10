// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#ifndef WVCDM_CORE_POLICY_ENGINE_H_
#define WVCDM_CORE_POLICY_ENGINE_H_

#include <map>
#include <memory>
#include <string>

#include "disallow_copy_and_assign.h"
#include "license_key_status.h"
#include "license_protocol.pb.h"
#include "wv_cdm_types.h"

namespace wvcdm {

using video_widevine::LicenseIdentification;
using video_widevine::WidevinePsshData_EntitledKey;

class Clock;
class CryptoSession;
class WvCdmEventListener;

// This acts as an oracle that basically says "Yes(true) you may still decrypt
// or no(false) you may not decrypt this data anymore."
class PolicyEngine {
 public:
  PolicyEngine(CdmSessionId session_id, WvCdmEventListener* event_listener,
               CryptoSession* crypto_session);
  virtual ~PolicyEngine();

  // The value returned should be taken as a hint rather than an absolute
  // status. It is computed during the last call to either SetLicense/
  // UpdateLicense/OnTimerEvent/BeginDecryption and may be out of sync
  // depending on the amount of time elapsed. The current decryption
  // status is not calculated to avoid overhead in the decryption path.
  virtual bool CanDecryptContent(const KeyId& key_id);

  // Returns the current CdmKeyStatus for the given key, or
  // kKeyStatusKeyUnknown if the key is not found. This is useful for finding
  // out why a key is not usable.
  virtual CdmKeyStatus GetKeyStatus(const KeyId& key_id);

  // Verifies whether the policy allows use of the specified key of
  // a given security level for content decryption.
  virtual bool CanUseKeyForSecurityLevel(const KeyId& key_id);

  // OnTimerEvent is called when a timer fires. It notifies the Policy Engine
  // that the timer has fired and dispatches the relevant events through
  // |event_listener_|.
  virtual void OnTimerEvent();

  // SetLicense is used in handling the initial license response. It stores
  // an exact copy of the policy information stored in the license.
  // The license state transitions to kLicenseStateCanPlay if the license
  // permits playback.
  virtual void SetLicense(const video_widevine::License& license);

  // Used to update the currently loaded entitled content keys.
  virtual void SetEntitledLicenseKeys(
      const std::vector<WidevinePsshData_EntitledKey>& entitled_keys);

  // SetLicenseForRelease is used when releasing a license. The keys in this
  // license will be ignored, and any old keys will be expired.
  virtual void SetLicenseForRelease(const video_widevine::License& license);

  // Call this on first decrypt to set the start of playback.
  virtual bool BeginDecryption(void);
  virtual void DecryptionEvent(void);

  // UpdateLicense is used in handling a license response for a renewal request.
  // The response may only contain any policy fields that have changed. In this
  // case an exact copy is not what we want to happen. We also will receive an
  // updated license_start_time from the server. The license will transition to
  // kLicenseStateCanPlay if the license permits playback.
  virtual void UpdateLicense(const video_widevine::License& license);

  // Used for notifying the Policy Engine of resolution changes
  virtual void NotifyResolution(uint32_t width, uint32_t height);

  virtual void NotifySessionExpiration();

  virtual CdmResponseType Query(CdmQueryMap* query_response);

  virtual CdmResponseType QueryKeyAllowedUsage(const KeyId& key_id,
                                               CdmKeyAllowedUsage* key_usage);

  virtual const LicenseIdentification& license_id() { return license_id_; }

  bool GetSecondsSinceStarted(int64_t* seconds_since_started);
  bool GetSecondsSinceLastPlayed(int64_t* seconds_since_started);

  // for offline save and restore
  int64_t GetPlaybackStartTime() { return playback_start_time_; }
  int64_t GetLastPlaybackTime() { return last_playback_time_; }
  int64_t GetGracePeriodEndTime() { return grace_period_end_time_; }
  void RestorePlaybackTimes(int64_t playback_start_time,
                            int64_t last_playback_time,
                            int64_t grace_period_end_time);

  bool IsLicenseForFuture() { return license_state_ == kLicenseStatePending; }
  bool HasPlaybackStarted(int64_t current_time) {
    if (playback_start_time_ == 0) return false;

    const int64_t playback_time = current_time - playback_start_time_;
    return playback_time >= policy_.play_start_grace_period_seconds();
  }

  bool HasLicenseOrPlaybackDurationExpired(int64_t current_time);
  int64_t GetLicenseOrPlaybackDurationRemaining();

  bool CanRenew() { return policy_.can_renew(); }

  bool IsSufficientOutputProtection(const KeyId& key_id) {
    return license_keys_->MeetsConstraints(key_id);
  }

 private:
  friend class PolicyEngineTest;
  friend class PolicyEngineConstraintsTest;

  void InitDevice(CryptoSession* crypto_session);
  void SetDeviceResolution(uint32_t width, uint32_t height);
  void CheckDeviceHdcpStatusOnTimer(int64_t current_time);
  void CheckDeviceHdcpStatus();

  typedef enum {
    kLicenseStateInitial,
    kLicenseStatePending,  // if license is issued for sometime in the future
    kLicenseStateCanPlay,
    kLicenseStateNeedRenewal,
    kLicenseStateWaitingLicenseUpdate,
    kLicenseStateExpired
  } LicenseState;

  // Gets the clock time that the license expires.  This is the hard limit that
  // all license types must obey at all times.
  int64_t GetHardLicenseExpiryTime();
  // Gets the clock time that the rental duration will expire, using the license
  // duration if one is not present.
  int64_t GetRentalExpiryTime();
  // Gets the clock time that the license expires based on whether we have
  // started playing.  This takes into account GetHardLicenseExpiryTime.
  int64_t GetExpiryTime(int64_t current_time,
                        bool ignore_soft_enforce_playback_duration);

  int64_t GetLicenseOrRentalDurationRemaining(int64_t current_time);
  int64_t GetPlaybackDurationRemaining(int64_t current_time);

  bool HasRenewalDelayExpired(int64_t current_time);
  bool HasRenewalRecoveryDurationExpired(int64_t current_time);
  bool HasRenewalRetryIntervalExpired(int64_t current_time);

  void UpdateRenewalRequest(int64_t current_time);

  // Notifies updates in keys information and fire OnKeysChange event if
  // key changes.
  void NotifyKeysChange(CdmKeyStatus new_status);

  // Notifies updates in expiry time and fire OnExpirationUpdate event if
  // expiry time changes.
  void NotifyExpirationUpdate(int64_t current_time);

  // Guard against clock rollbacks
  int64_t GetCurrentTime();

  // Test only methods
  // set_clock alters ownership of the passed-in pointer.
  void set_clock(Clock* clock);

  void SetSecurityLevelForTest(CdmSecurityLevel security_level);

  LicenseState license_state_;

  // This is the current policy information for this license. This gets updated
  // as license renewals occur.
  video_widevine::License::Policy policy_;

  // This is the license id field from server response. This data gets passed
  // back to the server in each renewal request. When we get a renewal response
  // from the license server we will get an updated id field.
  video_widevine::LicenseIdentification license_id_;

  // The server returns the license start time in the license/license renewal
  // response based off the request time sent by the client in the
  // license request/renewal
  int64_t license_start_time_;
  int64_t playback_start_time_;
  int64_t last_playback_time_;
  int64_t last_expiry_time_;
  int64_t grace_period_end_time_;
  bool last_expiry_time_set_;
  bool was_expired_on_load_;

  // This is used as a reference point for policy management. This value
  // represents an offset from license_start_time_. This is used to
  // calculate the time where renewal retries should occur.
  int64_t next_renewal_time_;

  // to assist in clock rollback checks
  int64_t last_recorded_current_time_;

  // Used to dispatch CDM events.
  CdmSessionId session_id_;
  WvCdmEventListener* event_listener_;

  // Keys associated with license - holds allowed usage, usage constraints,
  // and current status (CdmKeyStatus)
  std::unique_ptr<LicenseKeys> license_keys_;

  // Device checks
  int64_t next_device_check_;
  uint32_t current_resolution_;
  CryptoSession* crypto_session_;

  std::unique_ptr<Clock> clock_;

  CORE_DISALLOW_COPY_AND_ASSIGN(PolicyEngine);
};

}  // namespace wvcdm

#endif  // WVCDM_CORE_POLICY_ENGINE_H_
