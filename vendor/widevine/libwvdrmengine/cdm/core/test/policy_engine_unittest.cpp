// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#include <limits.h>

#include <algorithm>
#include <memory>
#include <sstream>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "license.h"
#include "mock_clock.h"
#include "policy_engine.h"
#include "test_base.h"
#include "test_printers.h"
#include "wv_cdm_event_listener.h"
#include "wv_cdm_constants.h"

namespace wvcdm {

using namespace testing;

namespace {
const int64_t kDurationUnlimited = 0;
const int64_t kLicenseStartTime = 1413517500;   // ~ 01/01/2013
const int64_t kPlaybackStartTime = kLicenseStartTime + 5;
const int64_t kRentalDuration = 604800;         // 7 days
const int64_t kPlaybackDuration = 172800;       // 48 hours
const int64_t kLicenseDuration = kRentalDuration + kPlaybackDuration;
const int64_t kLicenseRenewalPeriod = 120;           // 2 minutes
const int64_t kLicenseRenewalRetryInterval = 30;     // 30 seconds
const int64_t kLicenseRenewalRecoveryDuration = 30;  // 30 seconds
const int64_t kLowDuration = 300;                    // 5 minutes
const int64_t kHighDuration =
    std::max(std::max(kRentalDuration, kPlaybackDuration), kLicenseDuration);
const char* kRenewalServerUrl =
    "https://test.google.com/license/GetCencLicense";
const KeyId kKeyId = "357adc89f1673433c36c621f1b5c41ee";
const KeyId kEntitlementKeyId = "entitlementkeyid";
const KeyId kAnotherKeyId = "another_key_id";
const KeyId kSomeRandomKeyId = "some_random_key_id";
const KeyId kUnknownKeyId = "some_random_unknown_key_id";
const CdmSessionId kSessionId = "mock_session_id";

int64_t GetLicenseRenewalDelay(int64_t license_duration) {
  return license_duration > kLicenseRenewalPeriod
             ? license_duration - kLicenseRenewalPeriod
             : 0;
}

int64_t ParseInt(const std::string& str) {
  std::stringstream ss(str);
  int64_t ret;
  ss >> ret;
  return ret;
}

class HdcpOnlyMockCryptoSession : public TestCryptoSession {
 public:
  HdcpOnlyMockCryptoSession(metrics::CryptoMetrics* metrics) :
      TestCryptoSession(metrics) {}

  MOCK_METHOD2(GetHdcpCapabilities,
               CdmResponseType(HdcpCapability*, HdcpCapability*));
  CdmResponseType DoRealGetHdcpCapabilities(HdcpCapability* current,
                                 HdcpCapability* max) {
    return CryptoSession::GetHdcpCapabilities(current, max);
  }
};

class MockCdmEventListener : public WvCdmEventListener {
 public:
  MOCK_METHOD1(OnSessionRenewalNeeded, void(const CdmSessionId& session_id));
  MOCK_METHOD3(OnSessionKeysChange, void(const CdmSessionId& session_id,
                                         const CdmKeyStatusMap& keys_status,
                                         bool has_new_usable_key));
  MOCK_METHOD2(OnExpirationUpdate, void(const CdmSessionId& session_id,
                                        int64_t new_expiry_time_seconds));
};

}  // namespace

// protobuf generated classes.
using video_widevine::License;
using video_widevine::License_Policy;
using video_widevine::LicenseIdentification;
using video_widevine::STREAMING;
using video_widevine::OFFLINE;

// gmock methods
using ::testing::_;
using ::testing::AtLeast;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::MockFunction;
using ::testing::Pair;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::UnorderedElementsAre;

class PolicyEngineTest : public WvCdmTestBase {
 public:
  PolicyEngineTest() : crypto_session_(&dummy_metrics_) {
  }
 protected:
  void SetUp() override {
    WvCdmTestBase::SetUp();
    policy_engine_.reset(
        new PolicyEngine(kSessionId, &mock_event_listener_,
                         &crypto_session_));
    InjectMockClock();

    license_.set_license_start_time(kLicenseStartTime);

    LicenseIdentification* id = license_.mutable_id();
    id->set_version(1);
    id->set_type(STREAMING);

    License::KeyContainer* key = license_.add_key();
    key->set_type(License::KeyContainer::CONTENT);
    key->set_id(kKeyId);

    License_Policy* policy = license_.mutable_policy();
    policy = license_.mutable_policy();
    policy->set_can_play(true);
    policy->set_can_persist(false);
    policy->set_can_renew(false);
    // This is similar to an OFFLINE policy.
    policy->set_rental_duration_seconds(kRentalDuration);
    policy->set_playback_duration_seconds(kPlaybackDuration);
    policy->set_license_duration_seconds(kLicenseDuration);
    policy->set_renewal_recovery_duration_seconds(
        kLicenseRenewalRecoveryDuration);

    policy->set_renewal_server_url(kRenewalServerUrl);
    policy->set_renewal_delay_seconds(0);
    policy->set_renewal_retry_interval_seconds(kLicenseRenewalRetryInterval);
    policy->set_renew_with_usage(false);

    ON_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillByDefault(
          Invoke(
              &crypto_session_,
              &HdcpOnlyMockCryptoSession::DoRealGetHdcpCapabilities));
  }

  void InjectMockClock() {
    mock_clock_ = new MockClock();
    policy_engine_->set_clock(mock_clock_);
  }

  void ExpectSessionKeysChange(CdmKeyStatus expected_key_status,
                               bool expected_has_new_usable_key) {
    EXPECT_CALL(mock_event_listener_,
                OnSessionKeysChange(
                    kSessionId, UnorderedElementsAre(
                                    Pair(kKeyId, expected_key_status)),
                    expected_has_new_usable_key));
  }

  void ExpectSessionKeysChange(CdmKeyStatus expected_key_status,
                               bool expected_has_new_usable_key,
                               KeyId expected_keyid) {
    EXPECT_CALL(mock_event_listener_,
                OnSessionKeysChange(
                    kSessionId, UnorderedElementsAre(
                                    Pair(expected_keyid, expected_key_status)),
                    expected_has_new_usable_key));
  }

  void ExpectSessionKeysChange(CdmKeyStatus expected_key1_status,
                               CdmKeyStatus expected_key2_status,
                               bool expected_has_new_usable_key) {
    EXPECT_CALL(mock_event_listener_,
                OnSessionKeysChange(
                    kSessionId, UnorderedElementsAre(
                                    Pair(kKeyId, expected_key1_status),
                                    Pair(kAnotherKeyId, expected_key2_status)),
                    expected_has_new_usable_key));
  }

  void SetCdmSecurityLevel(CdmSecurityLevel security_level) {
    policy_engine_->SetSecurityLevelForTest(security_level);
  }

  metrics::CryptoMetrics dummy_metrics_;
  NiceMock<HdcpOnlyMockCryptoSession> crypto_session_;
  StrictMock<MockCdmEventListener> mock_event_listener_;
  MockClock* mock_clock_;
  std::unique_ptr<PolicyEngine> policy_engine_;
  License license_;
  MockFunction<void(int i)> check_;
};

TEST_F(PolicyEngineTest, NoLicense) {
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));
}

TEST_F(PolicyEngineTest, PlaybackSuccess_OfflineLicense) {
  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kPlaybackStartTime))
      .WillOnce(Return(kLicenseStartTime + 10));

  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kRentalDuration));
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kPlaybackStartTime + kPlaybackDuration));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(NO_ERROR)));

  policy_engine_->SetLicense(license_);
  policy_engine_->BeginDecryption();
  policy_engine_->OnTimerEvent();

  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kSomeRandomKeyId));
}

TEST_F(PolicyEngineTest, PlaybackSuccess_EntitlementLicense) {
  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kPlaybackStartTime))
      .WillOnce(Return(kLicenseStartTime + 10));

  ExpectSessionKeysChange(kKeyStatusUsable, true, kEntitlementKeyId);
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kRentalDuration));
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kPlaybackStartTime + kPlaybackDuration));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(NO_ERROR)));

  License::KeyContainer* key = license_.mutable_key(0);
  key->set_type(License::KeyContainer::ENTITLEMENT);
  key->set_id(kEntitlementKeyId);

  policy_engine_->SetLicense(license_);
  policy_engine_->BeginDecryption();
  policy_engine_->OnTimerEvent();
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));

  std::vector<WidevinePsshData_EntitledKey> entitled_keys(1);
  entitled_keys[0].set_entitlement_key_id(kEntitlementKeyId);
  entitled_keys[0].set_key_id(kKeyId);
  policy_engine_->SetEntitledLicenseKeys(entitled_keys);

  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kSomeRandomKeyId));
}

TEST_F(PolicyEngineTest, PlaybackSuccess_EntitlementLicenseExpiration) {
  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kPlaybackStartTime))
      .WillOnce(Return(kPlaybackStartTime + kPlaybackDuration + 10))
      .WillOnce(Return(kPlaybackStartTime + kPlaybackDuration + 11));

  ExpectSessionKeysChange(kKeyStatusUsable, true, kEntitlementKeyId);
  ExpectSessionKeysChange(kKeyStatusExpired, false, kEntitlementKeyId);
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kRentalDuration));
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kPlaybackStartTime + kPlaybackDuration));

  License::KeyContainer* key = license_.mutable_key(0);
  key->set_type(License::KeyContainer::ENTITLEMENT);
  key->set_id(kEntitlementKeyId);

  policy_engine_->SetLicense(license_);
  policy_engine_->BeginDecryption();
  policy_engine_->OnTimerEvent();
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));

  std::vector<WidevinePsshData_EntitledKey> entitled_keys(1);
  entitled_keys[0].set_entitlement_key_id(kEntitlementKeyId);
  entitled_keys[0].set_key_id(kKeyId);
  policy_engine_->SetEntitledLicenseKeys(entitled_keys);
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));
  policy_engine_->OnTimerEvent();
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));
}

TEST_F(PolicyEngineTest, PlaybackSuccess_StreamingLicense) {
  License_Policy* policy = license_.mutable_policy();
  policy->set_license_duration_seconds(kLowDuration);

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kPlaybackStartTime))
      .WillOnce(Return(kLicenseStartTime + 10));

  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kLowDuration));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  policy_engine_->SetLicense(license_);
  policy_engine_->BeginDecryption();
  policy_engine_->OnTimerEvent();

  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kSomeRandomKeyId));
}

TEST_F(PolicyEngineTest, PlaybackFailed_CanPlayFalse) {
  License_Policy* policy = license_.mutable_policy();
  policy->set_can_play(false);

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kLicenseStartTime + 5));

  ExpectSessionKeysChange(kKeyStatusExpired, false);

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  policy_engine_->SetLicense(license_);
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));

  policy_engine_->OnTimerEvent();

  policy_engine_->BeginDecryption();
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));
}

TEST_F(PolicyEngineTest, LicenseExpired_RentalDurationExpiredWithoutPlayback) {
  License_Policy* policy = license_.mutable_policy();
  policy->set_rental_duration_seconds(kLowDuration);
  policy->set_license_duration_seconds(kHighDuration);

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kPlaybackStartTime))
      .WillOnce(Return(kLicenseStartTime + kLowDuration - 1))
      .WillOnce(Return(kLicenseStartTime + kLowDuration));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  InSequence s;
  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kLowDuration));
  EXPECT_CALL(check_, Call(1));
  EXPECT_CALL(check_, Call(2));
  ExpectSessionKeysChange(kKeyStatusExpired, false);
  EXPECT_CALL(check_, Call(3));

  policy_engine_->SetLicense(license_);

  for (int i = 1; i <= 3; ++i) {
    EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
    policy_engine_->OnTimerEvent();
    check_.Call(i);
  }

  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));
}

TEST_F(PolicyEngineTest, PlaybackOk_RentalDurationPassedWithPlayback) {
  License_Policy* policy = license_.mutable_policy();
  policy->set_rental_duration_seconds(kLowDuration);

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kPlaybackStartTime))
      .WillOnce(Return(kLicenseStartTime + kLowDuration - 1))
      .WillOnce(Return(kLicenseStartTime + kLowDuration + 1));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  InSequence s;
  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kLowDuration));
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kPlaybackStartTime + kPlaybackDuration));
  EXPECT_CALL(check_, Call(1));
  EXPECT_CALL(check_, Call(2));

  policy_engine_->SetLicense(license_);
  policy_engine_->BeginDecryption();

  for (int i = 1; i <= 2; ++i) {
    EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
    policy_engine_->OnTimerEvent();
    check_.Call(i);
  }

  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
}

TEST_F(PolicyEngineTest, PlaybackFails_PlaybackDurationExpired) {
  int64_t playback_start_time = kLicenseStartTime + 10000;

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(playback_start_time))
      .WillOnce(Return(playback_start_time + kPlaybackDuration - 2))
      .WillOnce(Return(playback_start_time + kPlaybackDuration + 2));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  InSequence s;
  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kRentalDuration));
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, playback_start_time + kPlaybackDuration));
  EXPECT_CALL(check_, Call(1));
  ExpectSessionKeysChange(kKeyStatusExpired, false);
  EXPECT_CALL(check_, Call(2));

  policy_engine_->SetLicense(license_);
  policy_engine_->BeginDecryption();

  for (int i = 1; i <= 2; ++i) {
    EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
    policy_engine_->OnTimerEvent();
    check_.Call(i);
  }

  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));
}

TEST_F(PolicyEngineTest, LicenseExpired_LicenseDurationExpiredWithoutPlayback) {
  License_Policy* policy = license_.mutable_policy();
  policy->set_license_duration_seconds(kLowDuration);

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kPlaybackStartTime))
      .WillOnce(Return(kLicenseStartTime + kLowDuration - 1))
      .WillOnce(Return(kLicenseStartTime + kLowDuration));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  InSequence s;
  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kLowDuration));
  EXPECT_CALL(check_, Call(1));
  EXPECT_CALL(check_, Call(2));
  ExpectSessionKeysChange(kKeyStatusExpired, false);
  EXPECT_CALL(check_, Call(3));

  policy_engine_->SetLicense(license_);

  for (int i = 1; i <= 3; ++i) {
    EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
    policy_engine_->OnTimerEvent();
    check_.Call(i);
  }

  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));
}

TEST_F(PolicyEngineTest, PlaybackFails_ExpiryBeforeRenewalDelay_Offline) {
  License_Policy* policy = license_.mutable_policy();
  policy->set_renewal_delay_seconds(kLicenseDuration + 10);
  policy->set_can_renew(true);

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kPlaybackStartTime))
      .WillOnce(Return(kPlaybackStartTime + kPlaybackDuration - 1))
      .WillOnce(Return(kPlaybackStartTime + kPlaybackDuration + 1));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  InSequence s;
  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kRentalDuration));
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kPlaybackStartTime + kPlaybackDuration));
  EXPECT_CALL(check_, Call(1));
  ExpectSessionKeysChange(kKeyStatusExpired, false);
  EXPECT_CALL(check_, Call(2));

  policy_engine_->SetLicense(license_);
  policy_engine_->BeginDecryption();

  for (int i = 1; i <= 2; ++i) {
    EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
    policy_engine_->OnTimerEvent();
    check_.Call(i);
  }

  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));
}

TEST_F(PolicyEngineTest, PlaybackFails_ExpiryBeforeRenewalDelay_Streaming) {
  License_Policy* policy = license_.mutable_policy();
  policy->clear_rental_duration_seconds();
  policy->clear_playback_duration_seconds();
  policy->set_renewal_delay_seconds(kLicenseDuration + 10);
  policy->set_can_renew(true);

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kPlaybackStartTime))
      .WillOnce(Return(kLicenseStartTime + kLicenseDuration - 1))
      .WillOnce(Return(kLicenseStartTime + kLicenseDuration + 1));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  InSequence s;
  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kLicenseDuration));
  EXPECT_CALL(check_, Call(1));
  ExpectSessionKeysChange(kKeyStatusExpired, false);
  EXPECT_CALL(check_, Call(2));

  policy_engine_->SetLicense(license_);
  policy_engine_->BeginDecryption();

  for (int i = 1; i <= 2; ++i) {
    EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
    policy_engine_->OnTimerEvent();
    check_.Call(i);
  }

  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));
}

TEST_F(PolicyEngineTest, PlaybackOk_RentalDuration0) {
  License_Policy* policy = license_.mutable_policy();
  policy->set_rental_duration_seconds(kDurationUnlimited);

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kLicenseStartTime + kRentalDuration - 1))
      .WillOnce(Return(kLicenseStartTime + kRentalDuration + 10))
      .WillOnce(Return(kLicenseStartTime + kLicenseDuration + 1));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  InSequence s;
  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kLicenseDuration));
  EXPECT_CALL(check_, Call(1));
  EXPECT_CALL(check_, Call(2));
  ExpectSessionKeysChange(kKeyStatusExpired, false);
  EXPECT_CALL(check_, Call(3));

  policy_engine_->SetLicense(license_);

  for (int i = 1; i <= 3; ++i) {
    EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
    policy_engine_->OnTimerEvent();
    check_.Call(i);
  }

  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));
}

TEST_F(PolicyEngineTest, PlaybackOk_PlaybackDuration0) {
  License_Policy* policy = license_.mutable_policy();
  policy->set_playback_duration_seconds(kDurationUnlimited);

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime))
      .WillOnce(Return(kPlaybackStartTime))
      .WillOnce(Return(kPlaybackStartTime + kPlaybackDuration - 2))
      .WillOnce(Return(kPlaybackStartTime + kPlaybackDuration + 2))
      .WillOnce(Return(kLicenseStartTime + kLicenseDuration - 2))
      .WillOnce(Return(kLicenseStartTime + kLicenseDuration + 2));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  InSequence s;
  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kRentalDuration));
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kLicenseDuration));
  EXPECT_CALL(check_, Call(1));
  EXPECT_CALL(check_, Call(2));
  EXPECT_CALL(check_, Call(3));
  ExpectSessionKeysChange(kKeyStatusExpired, false);
  EXPECT_CALL(check_, Call(4));

  policy_engine_->SetLicense(license_);
  policy_engine_->BeginDecryption();

  for (int i = 1; i <= 4; ++i) {
    EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
    policy_engine_->OnTimerEvent();
    check_.Call(i);
  }

  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));
}

TEST_F(PolicyEngineTest, PlaybackOk_LicenseDuration0) {
  License_Policy* policy = license_.mutable_policy();
  policy->set_license_duration_seconds(kDurationUnlimited);

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kPlaybackStartTime))
      .WillOnce(Return(kPlaybackStartTime + kPlaybackDuration - 1))
      .WillOnce(Return(kPlaybackStartTime + kPlaybackDuration + 1));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  InSequence s;
  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kRentalDuration));
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kPlaybackStartTime + kPlaybackDuration));
  EXPECT_CALL(check_, Call(1));
  ExpectSessionKeysChange(kKeyStatusExpired, false);
  EXPECT_CALL(check_, Call(2));

  policy_engine_->SetLicense(license_);
  policy_engine_->BeginDecryption();

  for (int i = 1; i <= 2; ++i) {
    EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
    policy_engine_->OnTimerEvent();
    check_.Call(i);
  }

  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));
}

TEST_F(PolicyEngineTest, PlaybackOk_PlaybackAndRental0) {
  License_Policy* policy = license_.mutable_policy();
  policy->clear_rental_duration_seconds();
  policy->clear_playback_duration_seconds();
  // Only |license_duration_seconds| set.

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kPlaybackStartTime))
      .WillOnce(Return(kLicenseStartTime + 10));

  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kLicenseDuration));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  policy_engine_->SetLicense(license_);
  policy_engine_->BeginDecryption();
  policy_engine_->OnTimerEvent();

  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kSomeRandomKeyId));
}

TEST_F(PolicyEngineTest, PlaybackOk_PlaybackAndLicense0_WithoutPlayback) {
  License_Policy* policy = license_.mutable_policy();
  policy->clear_license_duration_seconds();
  policy->clear_playback_duration_seconds();
  // Only |rental_duration_seconds| set.

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kLicenseStartTime + 10))
      .WillOnce(Return(kLicenseStartTime + kRentalDuration - 10))
      .WillOnce(Return(kLicenseStartTime + kRentalDuration + 10));

  ExpectSessionKeysChange(kKeyStatusExpired, false);
  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kRentalDuration));

  policy_engine_->SetLicense(license_);
  policy_engine_->OnTimerEvent();

  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kSomeRandomKeyId));

  policy_engine_->OnTimerEvent();

  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kSomeRandomKeyId));

  policy_engine_->OnTimerEvent();

  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kSomeRandomKeyId));
}

TEST_F(PolicyEngineTest, PlaybackOk_PlaybackAndLicense0_WithPlayback) {
  License_Policy* policy = license_.mutable_policy();
  policy->clear_license_duration_seconds();
  policy->clear_playback_duration_seconds();
  // Only |rental_duration_seconds| set.

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kPlaybackStartTime))
      .WillOnce(Return(kLicenseStartTime + 10))
      .WillOnce(Return(kLicenseStartTime + kRentalDuration + 10));

  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, 0));
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kRentalDuration));

  policy_engine_->SetLicense(license_);
  policy_engine_->BeginDecryption();
  policy_engine_->OnTimerEvent();

  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kSomeRandomKeyId));

  policy_engine_->OnTimerEvent();

  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kSomeRandomKeyId));
}

TEST_F(PolicyEngineTest, PlaybackOk_RentalAndLicense0_WithoutPlayback) {
  License_Policy* policy = license_.mutable_policy();
  policy->clear_license_duration_seconds();
  policy->clear_rental_duration_seconds();
  // Only |playback_duration_seconds| set.

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kLicenseStartTime + 10))
      .WillOnce(Return(kLicenseStartTime + kPlaybackDuration + 10));

  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_, OnExpirationUpdate(_, 0));

  policy_engine_->SetLicense(license_);
  policy_engine_->OnTimerEvent();

  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kSomeRandomKeyId));

  policy_engine_->OnTimerEvent();

  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kSomeRandomKeyId));
}

TEST_F(PolicyEngineTest, PlaybackOk_RentalAndLicense0_WithPlayback) {
  License_Policy* policy = license_.mutable_policy();
  policy->clear_license_duration_seconds();
  policy->clear_rental_duration_seconds();
  // Only |playback_duration_seconds| set.

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kPlaybackStartTime))
      .WillOnce(Return(kPlaybackStartTime + kPlaybackDuration - 10))
      .WillOnce(Return(kPlaybackStartTime + kPlaybackDuration + 10));

  ExpectSessionKeysChange(kKeyStatusExpired, false);
  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_, OnExpirationUpdate(_, 0));
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kPlaybackStartTime + kPlaybackDuration));

  policy_engine_->SetLicense(license_);
  policy_engine_->BeginDecryption();
  policy_engine_->OnTimerEvent();

  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kSomeRandomKeyId));

  policy_engine_->OnTimerEvent();

  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kSomeRandomKeyId));
}

TEST_F(PolicyEngineTest,
       PlaybackOk_RentalAndLicense0_WithPlaybackBeforeLicense) {
  License_Policy* policy = license_.mutable_policy();
  policy->clear_license_duration_seconds();
  policy->clear_rental_duration_seconds();
  // Only |playback_duration_seconds| set.

  policy_engine_->BeginDecryption();

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kPlaybackStartTime))
      .WillOnce(Return(kPlaybackStartTime + kPlaybackDuration - 10))
      .WillOnce(Return(kPlaybackStartTime + kPlaybackDuration + 10));

  ExpectSessionKeysChange(kKeyStatusExpired, false);
  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_, OnExpirationUpdate(_, 0));
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kPlaybackStartTime + kPlaybackDuration));

  policy_engine_->SetLicense(license_);
  policy_engine_->BeginDecryption();
  policy_engine_->OnTimerEvent();

  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kSomeRandomKeyId));

  policy_engine_->OnTimerEvent();

  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kSomeRandomKeyId));
}

TEST_F(PolicyEngineTest, PlaybackOk_Durations0) {
  License_Policy* policy = license_.mutable_policy();
  policy->set_rental_duration_seconds(kDurationUnlimited);
  policy->set_playback_duration_seconds(kDurationUnlimited);
  policy->set_license_duration_seconds(kDurationUnlimited);

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kPlaybackStartTime))
      .WillOnce(Return(kLicenseStartTime + kHighDuration - 1))
      .WillOnce(Return(kLicenseStartTime + kHighDuration));

  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_, OnExpirationUpdate(_, NEVER_EXPIRES));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  policy_engine_->SetLicense(license_);

  policy_engine_->BeginDecryption();
  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));

  policy_engine_->OnTimerEvent();
  policy_engine_->OnTimerEvent();

  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
}

TEST_F(PolicyEngineTest, PlaybackOk_LicenseWithFutureStartTime) {
  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime - 100))
      .WillOnce(Return(kLicenseStartTime - 50))
      .WillOnce(Return(kLicenseStartTime))
      .WillOnce(Return(kPlaybackStartTime));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  InSequence s;
  ExpectSessionKeysChange(kKeyStatusUsableInFuture, false);
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kRentalDuration));
  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kPlaybackStartTime + kPlaybackDuration));

  policy_engine_->SetLicense(license_);

  policy_engine_->OnTimerEvent();
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));

  policy_engine_->OnTimerEvent();
  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));

  policy_engine_->BeginDecryption();
  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
}

TEST_F(PolicyEngineTest, PlaybackFailed_CanRenewFalse) {
  License_Policy* policy = license_.mutable_policy();
  const int64_t license_renewal_delay =
      GetLicenseRenewalDelay(kPlaybackDuration);
  policy->set_renewal_delay_seconds(license_renewal_delay);
  policy->set_can_renew(false);

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kPlaybackStartTime))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay - 10))
      .WillOnce(Return(kPlaybackStartTime + kPlaybackDuration - 10))
      .WillOnce(Return(kPlaybackStartTime + kPlaybackDuration + 1));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  InSequence s;
  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kRentalDuration));
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kPlaybackStartTime + kPlaybackDuration));
  EXPECT_CALL(check_, Call(1));
  EXPECT_CALL(check_, Call(2));
  ExpectSessionKeysChange(kKeyStatusExpired, false);
  EXPECT_CALL(check_, Call(3));

  policy_engine_->SetLicense(license_);
  policy_engine_->BeginDecryption();

  for (int i = 1; i <= 3; ++i) {
    EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
    policy_engine_->OnTimerEvent();
    check_.Call(i);
  }

  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));
}

TEST_F(PolicyEngineTest, PlaybackOk_RenewSuccess) {
  License_Policy* policy = license_.mutable_policy();
  policy->set_can_renew(true);
  policy->set_license_duration_seconds(kLowDuration);
  int64_t license_renewal_delay = GetLicenseRenewalDelay(kLowDuration);
  policy->set_renewal_delay_seconds(license_renewal_delay);
  int64_t new_license_start_time =
      kLicenseStartTime + license_renewal_delay + 15;

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kPlaybackStartTime))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay - 15))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay + 10))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay + 20))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay +
                       kLicenseRenewalRetryInterval + 10));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  InSequence s;
  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kLowDuration));
  EXPECT_CALL(check_, Call(1));
  EXPECT_CALL(mock_event_listener_, OnSessionRenewalNeeded(_));
  EXPECT_CALL(check_, Call(2));
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, new_license_start_time + kLowDuration));
  EXPECT_CALL(check_, Call(3));

  policy_engine_->SetLicense(license_);
  policy_engine_->BeginDecryption();

  for (int i = 1; i <= 2; ++i) {
    EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
    policy_engine_->OnTimerEvent();
    check_.Call(i);
  }

  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));

  license_.set_license_start_time(new_license_start_time);
  policy->set_playback_duration_seconds(2 * kPlaybackDuration);
  LicenseIdentification* id = license_.mutable_id();
  id->set_version(2);
  policy_engine_->UpdateLicense(license_);

  policy_engine_->OnTimerEvent();
  check_.Call(3);

  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
}

TEST_F(PolicyEngineTest, PlaybackOk_RenewSuccess_WithFutureStartTime) {
  License_Policy* policy = license_.mutable_policy();
  policy->set_can_renew(true);
  policy->set_license_duration_seconds(kLowDuration);
  int64_t license_renewal_delay = GetLicenseRenewalDelay(kLowDuration);
  policy->set_renewal_delay_seconds(license_renewal_delay);
  int64_t new_license_start_time =
      kLicenseStartTime + license_renewal_delay + 50;

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kPlaybackStartTime))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay - 15))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay + 10))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay + 20))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay + 30))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay + 60));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  InSequence s;
  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kLowDuration));
  EXPECT_CALL(check_, Call(1));
  EXPECT_CALL(mock_event_listener_, OnSessionRenewalNeeded(_));
  EXPECT_CALL(check_, Call(2));
  ExpectSessionKeysChange(kKeyStatusUsableInFuture, false);
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, new_license_start_time + kLowDuration));
  EXPECT_CALL(check_, Call(3));
  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(check_, Call(4));

  policy_engine_->SetLicense(license_);
  policy_engine_->BeginDecryption();

  for (int i = 1; i <= 2; ++i) {
    EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
    policy_engine_->OnTimerEvent();
    check_.Call(i);
  }
  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));

  license_.set_license_start_time(new_license_start_time);
  LicenseIdentification* id = license_.mutable_id();
  id->set_version(2);
  policy_engine_->UpdateLicense(license_);

  policy_engine_->OnTimerEvent();
  check_.Call(3);
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));

  policy_engine_->OnTimerEvent();
  check_.Call(4);
  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
}

TEST_F(PolicyEngineTest, LicenseExpired_RenewFailedVersionNotUpdated) {
  License_Policy* policy = license_.mutable_policy();
  policy->set_can_renew(true);
  policy->set_license_duration_seconds(kLowDuration);
  int64_t license_renewal_delay = GetLicenseRenewalDelay(kLowDuration);
  policy->set_renewal_delay_seconds(license_renewal_delay);

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay - 10))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay + 10))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay + 40))
      .WillOnce(Return(kLicenseStartTime + kLowDuration + 10));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  InSequence s;
  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kLowDuration));
  EXPECT_CALL(check_, Call(1));
  EXPECT_CALL(mock_event_listener_, OnSessionRenewalNeeded(_));
  EXPECT_CALL(check_, Call(2));
  EXPECT_CALL(mock_event_listener_, OnSessionRenewalNeeded(_));
  EXPECT_CALL(check_, Call(3));
  ExpectSessionKeysChange(kKeyStatusExpired, false);
  EXPECT_CALL(check_, Call(4));

  policy_engine_->SetLicense(license_);

  for (int i = 1; i <= 2; ++i) {
    EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
    policy_engine_->OnTimerEvent();
    check_.Call(i);
  }

  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));

  license_.set_license_start_time(kLicenseStartTime + license_renewal_delay +
                                  30);
  policy_engine_->UpdateLicense(license_);

  policy_engine_->OnTimerEvent();
  check_.Call(3);

  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));

  policy_engine_->OnTimerEvent();
  check_.Call(4);

  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));
}

TEST_F(PolicyEngineTest, PlaybackFailed_RepeatedRenewFailures) {
  License_Policy* policy = license_.mutable_policy();
  policy->set_can_renew(true);
  const int64_t license_renewal_delay =
      GetLicenseRenewalDelay(kLicenseDuration);
  policy->set_renewal_delay_seconds(license_renewal_delay);
  policy->clear_rental_duration_seconds();
  policy->clear_playback_duration_seconds();

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kPlaybackStartTime))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay - 10))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay + 10))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay + 20))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay + 40))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay + 50))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay + 70))
      .WillOnce(Return(kLicenseStartTime + kLicenseDuration - 15))
      .WillOnce(Return(kLicenseStartTime + kLicenseDuration));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  InSequence s;
  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kLicenseDuration));
  EXPECT_CALL(check_, Call(1));
  EXPECT_CALL(mock_event_listener_, OnSessionRenewalNeeded(_));
  EXPECT_CALL(check_, Call(2));
  EXPECT_CALL(check_, Call(3));
  EXPECT_CALL(mock_event_listener_, OnSessionRenewalNeeded(_));
  EXPECT_CALL(check_, Call(4));
  EXPECT_CALL(check_, Call(5));
  EXPECT_CALL(mock_event_listener_, OnSessionRenewalNeeded(_));
  EXPECT_CALL(check_, Call(6));
  EXPECT_CALL(mock_event_listener_, OnSessionRenewalNeeded(_));
  EXPECT_CALL(check_, Call(7));
  ExpectSessionKeysChange(kKeyStatusExpired, false);
  EXPECT_CALL(check_, Call(8));

  policy_engine_->SetLicense(license_);
  policy_engine_->BeginDecryption();

  for (int i = 1; i <= 8; ++i) {
    EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
    policy_engine_->OnTimerEvent();
    check_.Call(i);
  }
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));
}

TEST_F(PolicyEngineTest, PlaybackOk_RenewSuccessAfterExpiry) {
  License_Policy* policy = license_.mutable_policy();
  policy->set_can_renew(true);
  policy->set_license_duration_seconds(kLowDuration);
  int64_t license_renewal_delay = GetLicenseRenewalDelay(kLowDuration);
  policy->set_renewal_delay_seconds(license_renewal_delay);
  policy->clear_rental_duration_seconds();

  int64_t new_license_start_time = kPlaybackStartTime + kPlaybackDuration + 10;
  int64_t new_playback_duration = kPlaybackDuration + 100;
  int64_t new_license_duration = kLicenseDuration + 100;

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kPlaybackStartTime))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay - 10))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay + 10))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay + 20))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay + 40))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay + 50))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay + 70))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay + 80))
      .WillOnce(Return(kLicenseStartTime + kLowDuration - 10))
      .WillOnce(Return(kLicenseStartTime + kLowDuration + 1))
      .WillOnce(Return(new_license_start_time))
      .WillOnce(Return(kPlaybackStartTime + kPlaybackDuration + 20));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  InSequence s;
  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kLowDuration));
  EXPECT_CALL(check_, Call(1));
  EXPECT_CALL(mock_event_listener_, OnSessionRenewalNeeded(_));
  EXPECT_CALL(check_, Call(2));
  EXPECT_CALL(check_, Call(3));
  EXPECT_CALL(mock_event_listener_, OnSessionRenewalNeeded(_));
  EXPECT_CALL(check_, Call(4));
  EXPECT_CALL(check_, Call(5));
  EXPECT_CALL(mock_event_listener_, OnSessionRenewalNeeded(_));
  EXPECT_CALL(check_, Call(6));
  EXPECT_CALL(check_, Call(7));
  EXPECT_CALL(mock_event_listener_, OnSessionRenewalNeeded(_));
  EXPECT_CALL(check_, Call(8));
  ExpectSessionKeysChange(kKeyStatusExpired, false);
  EXPECT_CALL(check_, Call(9));
  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(
      mock_event_listener_,
      OnExpirationUpdate(_, kPlaybackStartTime + new_playback_duration));
  EXPECT_CALL(check_, Call(10));

  policy_engine_->SetLicense(license_);
  policy_engine_->BeginDecryption();

  for (int i = 1; i <= 9; ++i) {
    EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
    policy_engine_->OnTimerEvent();
    check_.Call(i);
  }
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));

  license_.set_license_start_time(new_license_start_time);
  LicenseIdentification* id = license_.mutable_id();
  id->set_version(2);
  policy->set_playback_duration_seconds(new_playback_duration);
  policy->set_license_duration_seconds(new_license_duration);

  policy_engine_->UpdateLicense(license_);

  policy_engine_->OnTimerEvent();
  check_.Call(10);

  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
}

TEST_F(PolicyEngineTest, PlaybackOk_RenewSuccessAfterFailures) {
  License_Policy* policy = license_.mutable_policy();
  policy->set_can_renew(true);
  policy->clear_rental_duration_seconds();
  policy->clear_playback_duration_seconds();
  int64_t license_renewal_delay = GetLicenseRenewalDelay(kLicenseDuration);
  policy->set_renewal_delay_seconds(license_renewal_delay);
  int64_t new_license_start_time =
      kLicenseStartTime + license_renewal_delay + 55;

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kLicenseStartTime + 5))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay - 10))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay + 10))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay + 20))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay + 40))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay + 50))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay + 55))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay + 67))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay + 200));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  InSequence s;
  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kLicenseDuration));
  EXPECT_CALL(check_, Call(1));
  EXPECT_CALL(mock_event_listener_, OnSessionRenewalNeeded(_));
  EXPECT_CALL(check_, Call(2));
  EXPECT_CALL(check_, Call(3));
  EXPECT_CALL(mock_event_listener_, OnSessionRenewalNeeded(_));
  EXPECT_CALL(check_, Call(4));
  EXPECT_CALL(check_, Call(5));
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, new_license_start_time + kLicenseDuration));
  EXPECT_CALL(check_, Call(6));
  EXPECT_CALL(check_, Call(7));

  policy_engine_->SetLicense(license_);
  policy_engine_->BeginDecryption();

  for (int i = 1; i <= 5; ++i) {
    EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
    policy_engine_->OnTimerEvent();
    check_.Call(i);
  }

  license_.set_license_start_time(new_license_start_time);
  LicenseIdentification* id = license_.mutable_id();
  id->set_version(2);
  policy_engine_->UpdateLicense(license_);

  for (int i = 6; i <= 7; ++i) {
    EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
    policy_engine_->OnTimerEvent();
    check_.Call(i);
  }
  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
}

TEST_F(PolicyEngineTest, PlaybackOk_RenewedWithUsage) {
  int64_t new_license_start_time = kLicenseStartTime + 10;
  int64_t new_playback_duration = kPlaybackDuration + 100;

  License_Policy* policy = license_.mutable_policy();
  policy->set_can_renew(true);
  policy->set_renew_with_usage(true);

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kLicenseStartTime + 3))
      .WillOnce(Return(kPlaybackStartTime))
      .WillOnce(Return(kLicenseStartTime + 10))
      .WillOnce(Return(kLicenseStartTime + 20))
      .WillOnce(Return(kLicenseStartTime + 40));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  InSequence s;
  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kRentalDuration));
  EXPECT_CALL(check_, Call(1));
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kPlaybackStartTime + kPlaybackDuration));
  EXPECT_CALL(mock_event_listener_, OnSessionRenewalNeeded(_));
  EXPECT_CALL(check_, Call(2));
  EXPECT_CALL(
      mock_event_listener_,
      OnExpirationUpdate(_, kPlaybackStartTime + new_playback_duration));
  EXPECT_CALL(check_, Call(3));

  policy_engine_->SetLicense(license_);

  policy_engine_->OnTimerEvent();
  check_.Call(1);

  policy_engine_->BeginDecryption();
  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));

  policy_engine_->OnTimerEvent();
  check_.Call(2);

  license_.set_license_start_time(new_license_start_time);
  license_.mutable_policy()->set_playback_duration_seconds(
      new_playback_duration);
  policy->set_renew_with_usage(false);
  LicenseIdentification* id = license_.mutable_id();
  id->set_version(2);
  policy_engine_->UpdateLicense(license_);

  policy_engine_->OnTimerEvent();
  check_.Call(3);

  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
}

TEST_F(PolicyEngineTest, MultipleKeysInLicense) {
  const char kSigningKeyId[] = "signing_key";

  license_.clear_key();
  License::KeyContainer* content_key = license_.add_key();
  content_key->set_type(License::KeyContainer::CONTENT);
  content_key->set_id(kKeyId);
  License::KeyContainer* non_content_key = license_.add_key();
  non_content_key->set_type(License::KeyContainer::SIGNING);
  non_content_key->set_id(kSigningKeyId);
  License::KeyContainer* content_key_without_id = license_.add_key();
  content_key_without_id->set_type(License::KeyContainer::CONTENT);
  License::KeyContainer* another_content_key = license_.add_key();
  another_content_key->set_type(License::KeyContainer::CONTENT);
  another_content_key->set_id(kAnotherKeyId);

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  ExpectSessionKeysChange(kKeyStatusUsable, kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_, OnExpirationUpdate(_, _));

  policy_engine_->SetLicense(license_);
  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
  EXPECT_TRUE(policy_engine_->CanDecryptContent(kAnotherKeyId));
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kSigningKeyId));
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kSomeRandomKeyId));
}

TEST_F(PolicyEngineTest, PlaybackOk_SoftEnforcePlaybackDuration) {
  License_Policy* policy = license_.mutable_policy();
  policy->set_soft_enforce_playback_duration(true);

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kPlaybackStartTime))
      .WillOnce(Return(kPlaybackStartTime + kPlaybackDuration - 5))
      .WillOnce(Return(kPlaybackStartTime + kPlaybackDuration + 5))
      .WillOnce(Return(kLicenseStartTime + kLicenseDuration - 5))
      .WillOnce(Return(kLicenseStartTime + kLicenseDuration + 5));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  InSequence seq;
  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kRentalDuration));
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kLicenseDuration));
  EXPECT_CALL(check_, Call(1));
  EXPECT_CALL(check_, Call(2));
  EXPECT_CALL(check_, Call(3));
  ExpectSessionKeysChange(kKeyStatusExpired, false);
  EXPECT_CALL(check_, Call(4));

  policy_engine_->SetLicense(license_);
  policy_engine_->BeginDecryption();

  for (int i = 1; i <= 4; ++i) {
    EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
    policy_engine_->OnTimerEvent();
    check_.Call(i);
  }

  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));
}

TEST_F(PolicyEngineTest, LicenseExpired_SoftEnforceLoadBeforeExpire) {
  License_Policy* policy = license_.mutable_policy();
  policy->set_soft_enforce_playback_duration(true);

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kPlaybackStartTime + kPlaybackDuration - 5))
      .WillOnce(Return(kPlaybackStartTime + kPlaybackDuration + 10));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  InSequence seq;
  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kRentalDuration));
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kLicenseDuration));

  policy_engine_->SetLicense(license_);
  policy_engine_->RestorePlaybackTimes(kPlaybackStartTime, kPlaybackStartTime,
                                       kPlaybackStartTime);
  policy_engine_->OnTimerEvent();

  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
}

TEST_F(PolicyEngineTest, LicenseExpired_SoftEnforceLoadAfterExpire) {
  License_Policy* policy = license_.mutable_policy();
  policy->set_soft_enforce_playback_duration(true);

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kPlaybackStartTime + kPlaybackDuration + 5))
      .WillOnce(Return(kPlaybackStartTime + kPlaybackDuration + 10));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  InSequence seq;
  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kRentalDuration));
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kPlaybackStartTime + kPlaybackDuration));
  ExpectSessionKeysChange(kKeyStatusExpired, false);

  policy_engine_->SetLicense(license_);
  policy_engine_->RestorePlaybackTimes(kPlaybackStartTime, kPlaybackStartTime,
                                       kPlaybackStartTime);
  policy_engine_->OnTimerEvent();

  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));
}

TEST_F(PolicyEngineTest, PlaybackOk_GracePeriod) {
  const int64_t kGracePeriod = 300;  // 5 minutes
  License_Policy* policy = license_.mutable_policy();
  policy->set_play_start_grace_period_seconds(kGracePeriod);

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kPlaybackStartTime))
      .WillOnce(Return(kPlaybackStartTime + kGracePeriod - 5))
      .WillOnce(Return(kPlaybackStartTime + kGracePeriod + 5))
      .WillOnce(Return(kPlaybackStartTime + kPlaybackDuration - 5))
      .WillOnce(Return(kPlaybackStartTime + kPlaybackDuration + 5));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  InSequence seq;
  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kRentalDuration));
  EXPECT_CALL(check_, Call(1));
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kPlaybackStartTime + kPlaybackDuration));
  EXPECT_CALL(check_, Call(2));
  EXPECT_CALL(check_, Call(3));
  ExpectSessionKeysChange(kKeyStatusExpired, false);
  EXPECT_CALL(check_, Call(4));

  policy_engine_->SetLicense(license_);
  policy_engine_->BeginDecryption();

  for (int i = 1; i <= 4; ++i) {
    EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
    policy_engine_->OnTimerEvent();
    check_.Call(i);
  }

  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));
}

TEST_F(PolicyEngineTest, PlaybackOk_GracePeriodWithLoad) {
  const int64_t kGracePeriod = 300;  // 5 minutes
  const int64_t kNewPlaybackStartTime = kPlaybackStartTime + kPlaybackDuration;
  License_Policy* policy = license_.mutable_policy();
  policy->set_play_start_grace_period_seconds(kGracePeriod);

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kNewPlaybackStartTime))
      .WillOnce(Return(kNewPlaybackStartTime))
      .WillOnce(Return(kNewPlaybackStartTime + kGracePeriod - 5))
      .WillOnce(Return(kNewPlaybackStartTime + kGracePeriod + 5))
      .WillOnce(Return(kNewPlaybackStartTime + kPlaybackDuration - 5))
      .WillOnce(Return(kNewPlaybackStartTime + kPlaybackDuration + 5));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  InSequence seq;
  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kRentalDuration));
  EXPECT_CALL(check_, Call(1));
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kNewPlaybackStartTime + kPlaybackDuration));
  EXPECT_CALL(check_, Call(2));
  EXPECT_CALL(check_, Call(3));
  ExpectSessionKeysChange(kKeyStatusExpired, false);
  EXPECT_CALL(check_, Call(4));

  policy_engine_->SetLicense(license_);
  policy_engine_->RestorePlaybackTimes(kPlaybackStartTime, kPlaybackStartTime,
                                       0);
  policy_engine_->BeginDecryption();

  for (int i = 1; i <= 4; ++i) {
    EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
    policy_engine_->OnTimerEvent();
    check_.Call(i);
  }

  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));
}

TEST_F(PolicyEngineTest, PlaybackOk_GracePeriodWithExpiredLoad) {
  const int64_t kGracePeriod = 300;  // 5 minutes
  const int64_t kNewPlaybackStartTime =
      kPlaybackStartTime + kPlaybackDuration + 5;
  License_Policy* policy = license_.mutable_policy();
  policy->set_play_start_grace_period_seconds(kGracePeriod);

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kNewPlaybackStartTime))
      .WillOnce(Return(kNewPlaybackStartTime));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  InSequence seq;
  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kRentalDuration));
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kPlaybackStartTime + kPlaybackDuration));
  ExpectSessionKeysChange(kKeyStatusExpired, false);

  policy_engine_->SetLicense(license_);
  policy_engine_->RestorePlaybackTimes(kPlaybackStartTime, kPlaybackStartTime,
                                       kPlaybackStartTime);

  policy_engine_->OnTimerEvent();
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));
}

TEST_F(PolicyEngineTest, PlaybackOk_CanStoreGracePeriod) {
  const int64_t kGracePeriod = 300;  // 5 minutes
  License_Policy* policy = license_.mutable_policy();
  policy->set_play_start_grace_period_seconds(kGracePeriod);

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kPlaybackStartTime))
      .WillOnce(Return(kPlaybackStartTime + 50))
      .WillOnce(Return(kPlaybackStartTime + kGracePeriod + 2));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  InSequence seq;
  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kRentalDuration));
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kPlaybackStartTime + kPlaybackDuration));

  policy_engine_->SetLicense(license_);
  policy_engine_->BeginDecryption();

  policy_engine_->OnTimerEvent();
  EXPECT_EQ(0, policy_engine_->GetGracePeriodEndTime());

  policy_engine_->OnTimerEvent();
  EXPECT_EQ(kPlaybackStartTime, policy_engine_->GetGracePeriodEndTime());
}

struct KeySecurityLevelParams {
  bool is_security_level_set;
  License::KeyContainer::SecurityLevel security_level;
  bool expect_can_L1_use_key;
  bool expect_can_L2_use_key;
  bool expect_can_L3_use_key;
  bool expect_can_level_unknown_use_key;
};

KeySecurityLevelParams key_security_test_vectors[] = {
  { false, License::KeyContainer::HW_SECURE_ALL, true, true, true, true },
  { true, License::KeyContainer::SW_SECURE_CRYPTO, true, true, true, false },
  { true, License::KeyContainer::SW_SECURE_DECODE, true, true, true, false },
  { true, License::KeyContainer::HW_SECURE_CRYPTO, true, true, false, false },
  { true, License::KeyContainer::HW_SECURE_DECODE, true, false, false, false },
  { true, License::KeyContainer::HW_SECURE_ALL, true, false, false, false },
};

class PolicyEngineKeySecurityLevelTest
  : public PolicyEngineTest,
    public ::testing::WithParamInterface<KeySecurityLevelParams*> {};

TEST_P(PolicyEngineKeySecurityLevelTest, CanUseKeyForSecurityLevel) {

  KeySecurityLevelParams* param = GetParam();

  license_.clear_key();

  License::KeyContainer* content_key = license_.add_key();
  content_key->set_type(License::KeyContainer::CONTENT);
  content_key->set_id(kKeyId);
  if (param->is_security_level_set)
    content_key->set_level(param->security_level);

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1));

  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_, OnExpirationUpdate(_, _));

  SetCdmSecurityLevel(kSecurityLevelL1);
  policy_engine_->SetLicense(license_);

  EXPECT_EQ(param->expect_can_L1_use_key,
            policy_engine_->CanUseKeyForSecurityLevel(kKeyId));

  SetCdmSecurityLevel(kSecurityLevelL2);
  policy_engine_->SetLicense(license_);

  EXPECT_EQ(param->expect_can_L2_use_key,
            policy_engine_->CanUseKeyForSecurityLevel(kKeyId));

  SetCdmSecurityLevel(kSecurityLevelL3);
  policy_engine_->SetLicense(license_);

  EXPECT_EQ(param->expect_can_L3_use_key,
            policy_engine_->CanUseKeyForSecurityLevel(kKeyId));

  SetCdmSecurityLevel(kSecurityLevelUnknown);
  policy_engine_->SetLicense(license_);

  EXPECT_EQ(param->expect_can_level_unknown_use_key,
            policy_engine_->CanUseKeyForSecurityLevel(kKeyId));
}

INSTANTIATE_TEST_CASE_P(
    PolicyEngine, PolicyEngineKeySecurityLevelTest,
    ::testing::Range(&key_security_test_vectors[0],
                     &key_security_test_vectors[5]));

class PolicyEngineKeyAllowedUsageTest : public PolicyEngineTest {
 protected:
  enum KeyFlag {
    kKeyFlagNull,
    kKeyFlagFalse,
    kKeyFlagTrue
  };

  static const KeyFlag kEncryptNull = kKeyFlagNull;
  static const KeyFlag kEncryptFalse = kKeyFlagFalse;
  static const KeyFlag kEncryptTrue = kKeyFlagTrue;
  static const KeyFlag kDecryptNull = kKeyFlagNull;
  static const KeyFlag kDecryptFalse = kKeyFlagFalse;
  static const KeyFlag kDecryptTrue = kKeyFlagTrue;
  static const KeyFlag kSignNull = kKeyFlagNull;
  static const KeyFlag kSignFalse = kKeyFlagFalse;
  static const KeyFlag kSignTrue = kKeyFlagTrue;
  static const KeyFlag kVerifyNull = kKeyFlagNull;
  static const KeyFlag kVerifyFalse = kKeyFlagFalse;
  static const KeyFlag kVerifyTrue = kKeyFlagTrue;

  static const KeyFlag kContentSecureFalse = kKeyFlagFalse;
  static const KeyFlag kContentSecureTrue = kKeyFlagTrue;
  static const KeyFlag kContentClearFalse = kKeyFlagFalse;
  static const KeyFlag kContentClearTrue = kKeyFlagTrue;


  void ExpectAllowedContentKeySettings(const CdmKeyAllowedUsage& key_usage,
                                       KeyFlag secure, KeyFlag clear,
                                       CdmKeySecurityLevel key_security_level) {
    EXPECT_EQ(key_usage.decrypt_to_secure_buffer, secure == kKeyFlagTrue);
    EXPECT_EQ(key_usage.decrypt_to_clear_buffer, clear == kKeyFlagTrue);
    EXPECT_EQ(key_usage.key_security_level_, key_security_level);
    EXPECT_FALSE(key_usage.generic_encrypt);
    EXPECT_FALSE(key_usage.generic_decrypt);
    EXPECT_FALSE(key_usage.generic_sign);
    EXPECT_FALSE(key_usage.generic_verify);
  }

  void ExpectAllowedOperatorKeySettings(const CdmKeyAllowedUsage& key_usage,
                                        KeyFlag encrypt, KeyFlag decrypt,
                                        KeyFlag sign, KeyFlag verify) {
    EXPECT_FALSE(key_usage.decrypt_to_secure_buffer);
    EXPECT_FALSE(key_usage.decrypt_to_clear_buffer);
    EXPECT_EQ(key_usage.generic_encrypt, encrypt == kKeyFlagTrue);
    EXPECT_EQ(key_usage.generic_decrypt, decrypt == kKeyFlagTrue);
    EXPECT_EQ(key_usage.generic_sign, sign == kKeyFlagTrue);
    EXPECT_EQ(key_usage.generic_verify, verify == kKeyFlagTrue);
  }

  void ExpectSecureContentKey(const KeyId& key_id,
                              CdmKeySecurityLevel key_security_level) {
    CdmKeyAllowedUsage key_usage;
    EXPECT_EQ(NO_ERROR,
              policy_engine_->QueryKeyAllowedUsage(key_id, &key_usage));

    ExpectAllowedContentKeySettings(key_usage, kContentSecureTrue,
                                    kContentSecureFalse, key_security_level);
  }

  void ExpectLessSecureContentKey(const KeyId& key_id,
                                  CdmKeySecurityLevel key_security_level) {
    CdmKeyAllowedUsage key_usage;
    EXPECT_EQ(NO_ERROR,
              policy_engine_->QueryKeyAllowedUsage(key_id, &key_usage));

    ExpectAllowedContentKeySettings(key_usage, kContentSecureTrue,
                                    kContentSecureTrue, key_security_level);
  }

  void ExpectOperatorSessionKey(const KeyId& key_id, KeyFlag encrypt,
                                KeyFlag decrypt, KeyFlag sign, KeyFlag verify) {
    CdmKeyAllowedUsage key_usage;
    EXPECT_EQ(NO_ERROR,
              policy_engine_->QueryKeyAllowedUsage(key_id, &key_usage));

    ExpectAllowedOperatorKeySettings(key_usage, encrypt, decrypt, sign, verify);
  }

  void AddOperatorSessionKey(const KeyId& key_id, KeyFlag encrypt,
                             KeyFlag decrypt, KeyFlag sign, KeyFlag verify) {
    License::KeyContainer* non_content_key = license_.add_key();
    non_content_key->set_type(License::KeyContainer::OPERATOR_SESSION);
    non_content_key->set_id(key_id);
    License::KeyContainer::OperatorSessionKeyPermissions* permissions =
        non_content_key->mutable_operator_session_key_permissions();
    if (encrypt != kKeyFlagNull) {
      permissions->set_allow_encrypt(encrypt == kKeyFlagTrue);
    }
    if (decrypt != kKeyFlagNull) {
      permissions->set_allow_decrypt(decrypt == kKeyFlagTrue);
    }
    if (sign != kKeyFlagNull) {
      permissions->set_allow_sign(sign == kKeyFlagTrue);
    }
    if (verify != kKeyFlagNull) {
      permissions->set_allow_signature_verify(verify == kKeyFlagTrue);
    }
  }
};

TEST_F(PolicyEngineKeyAllowedUsageTest, AllowedUsageBasic) {

  const KeyId kGenericKeyId = "oper_session_key";

  license_.clear_key();

  // most secure
  License::KeyContainer* content_key = license_.add_key();
  content_key->set_type(License::KeyContainer::CONTENT);
  content_key->set_id(kKeyId);
  content_key->set_level(License::KeyContainer::HW_SECURE_ALL);

  SetCdmSecurityLevel(kSecurityLevelL1);

  // generic operator session key (sign)
  AddOperatorSessionKey(kGenericKeyId, kEncryptNull, kDecryptNull, kSignTrue,
                        kVerifyNull);

  License::KeyContainer* content_key_without_id = license_.add_key();
  content_key_without_id->set_type(License::KeyContainer::CONTENT);

  // default level - less secure
  License::KeyContainer* another_content_key = license_.add_key();
  another_content_key->set_type(License::KeyContainer::CONTENT);
  another_content_key->set_id(kAnotherKeyId);

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  ExpectSessionKeysChange(kKeyStatusUsable, kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_, OnExpirationUpdate(_, _));

  policy_engine_->SetLicense(license_);

  ExpectSecureContentKey(kKeyId, kHardwareSecureAll);
  ExpectLessSecureContentKey(kAnotherKeyId, kKeySecurityLevelUnset);
  ExpectOperatorSessionKey(kGenericKeyId, kEncryptNull, kDecryptNull,
                           kSignTrue, kVerifyNull);

  CdmKeyAllowedUsage key_usage;
  EXPECT_EQ(KEY_NOT_FOUND_1,
            policy_engine_->QueryKeyAllowedUsage(kUnknownKeyId, &key_usage));
}

TEST_F(PolicyEngineKeyAllowedUsageTest, AllowedUsageGeneric) {

  const KeyId kGenericEncryptKeyId = "oper_session_key_1";
  const KeyId kGenericDecryptKeyId = "oper_session_key_2";
  const KeyId kGenericSignKeyId = "oper_session_key_3";
  const KeyId kGenericVerifyKeyId = "oper_session_key_4";
  const KeyId kGenericFullKeyId = "oper_session_key_5";
  const KeyId kGenericExplicitKeyId = "oper_session_key_6";

  license_.clear_key();

  // more secure
  License::KeyContainer* content_key = license_.add_key();
  content_key->set_type(License::KeyContainer::CONTENT);
  content_key->set_id(kKeyId);
  content_key->set_level(License::KeyContainer::HW_SECURE_DECODE);

  // less secure
  License::KeyContainer* another_content_key = license_.add_key();
  another_content_key->set_type(License::KeyContainer::CONTENT);
  another_content_key->set_id(kAnotherKeyId);
  another_content_key->set_level(License::KeyContainer::HW_SECURE_CRYPTO);

  SetCdmSecurityLevel(kSecurityLevelL1);

  // generic operator session keys
  AddOperatorSessionKey(kGenericSignKeyId, kEncryptNull, kDecryptNull,
                        kSignTrue, kVerifyNull);
  AddOperatorSessionKey(kGenericEncryptKeyId, kEncryptTrue, kDecryptNull,
                        kSignNull, kVerifyNull);
  AddOperatorSessionKey(kGenericDecryptKeyId, kEncryptNull, kDecryptTrue,
                        kSignNull, kVerifyNull);
  AddOperatorSessionKey(kGenericVerifyKeyId, kEncryptNull, kDecryptNull,
                        kSignNull, kVerifyTrue);
  AddOperatorSessionKey(kGenericFullKeyId, kEncryptTrue, kDecryptTrue,
                        kSignTrue, kVerifyTrue);
  AddOperatorSessionKey(kGenericExplicitKeyId, kEncryptFalse, kDecryptTrue,
                        kSignFalse, kVerifyTrue);

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  ExpectSessionKeysChange(kKeyStatusUsable, kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_, OnExpirationUpdate(_, _));

  policy_engine_->SetLicense(license_);

  ExpectSecureContentKey(kKeyId, kHardwareSecureDecode);
  ExpectLessSecureContentKey(kAnotherKeyId, kHardwareSecureCrypto);
  ExpectOperatorSessionKey(kGenericEncryptKeyId, kEncryptTrue, kDecryptFalse,
                           kSignFalse, kVerifyFalse);
  ExpectOperatorSessionKey(kGenericDecryptKeyId, kEncryptFalse, kDecryptTrue,
                           kSignFalse, kVerifyFalse);
  ExpectOperatorSessionKey(kGenericSignKeyId, kEncryptFalse, kDecryptFalse,
                           kSignTrue, kVerifyFalse);
  ExpectOperatorSessionKey(kGenericVerifyKeyId, kEncryptFalse, kDecryptFalse,
                           kSignFalse, kVerifyTrue);
  ExpectOperatorSessionKey(kGenericFullKeyId, kEncryptTrue, kDecryptTrue,
                           kSignTrue, kVerifyTrue);
  ExpectOperatorSessionKey(kGenericExplicitKeyId, kEncryptFalse, kDecryptTrue,
                           kSignFalse, kVerifyTrue);
}

class PolicyEngineQueryTest : public PolicyEngineTest {
 protected:
  void SetUp() override {
    PolicyEngineTest::SetUp();
    policy_engine_.reset(new PolicyEngine(kSessionId, NULL, &crypto_session_));
    InjectMockClock();

    // Use a STREAMING license policy.
    License_Policy* policy = license_.mutable_policy();
    policy->set_license_duration_seconds(kLowDuration);
    policy->set_can_renew(true);
    policy->set_renewal_delay_seconds(GetLicenseRenewalDelay(kLowDuration));
  }
};

TEST_F(PolicyEngineQueryTest, QuerySuccess_LicenseNotReceived) {
  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime));

  CdmQueryMap query_info;
  EXPECT_EQ(NO_ERROR, policy_engine_->Query(&query_info));
  EXPECT_EQ(0u, query_info.size());
}

TEST_F(PolicyEngineQueryTest, QuerySuccess_LicenseStartTimeNotSet) {
  license_.clear_license_start_time();

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1));

  policy_engine_->SetLicense(license_);

  CdmQueryMap query_info;
  EXPECT_EQ(NO_ERROR, policy_engine_->Query(&query_info));
  EXPECT_EQ(0u, query_info.size());
}

TEST_F(PolicyEngineQueryTest, QuerySuccess) {
  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kLicenseStartTime + 100));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  policy_engine_->SetLicense(license_);

  CdmQueryMap query_info;
  EXPECT_EQ(NO_ERROR, policy_engine_->Query(&query_info));
  EXPECT_EQ(QUERY_VALUE_STREAMING, query_info[QUERY_KEY_LICENSE_TYPE]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_PLAY_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_FALSE, query_info[QUERY_KEY_PERSIST_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_RENEW_ALLOWED]);

  EXPECT_EQ(kLowDuration - 100,
            ParseInt(query_info[QUERY_KEY_LICENSE_DURATION_REMAINING]));
  EXPECT_EQ(kPlaybackDuration,
            ParseInt(query_info[QUERY_KEY_PLAYBACK_DURATION_REMAINING]));
  EXPECT_EQ(kRenewalServerUrl, query_info[QUERY_KEY_RENEWAL_SERVER_URL]);
}

TEST_F(PolicyEngineQueryTest, QuerySuccess_PlaybackNotBegun) {
  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kLicenseStartTime + 100))
      .WillOnce(Return(kLicenseStartTime + 200));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  policy_engine_->SetLicense(license_);

  CdmQueryMap query_info;
  EXPECT_EQ(NO_ERROR, policy_engine_->Query(&query_info));
  EXPECT_EQ(QUERY_VALUE_STREAMING, query_info[QUERY_KEY_LICENSE_TYPE]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_PLAY_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_FALSE, query_info[QUERY_KEY_PERSIST_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_RENEW_ALLOWED]);

  EXPECT_EQ(kLowDuration - 100,
            ParseInt(query_info[QUERY_KEY_LICENSE_DURATION_REMAINING]));
  EXPECT_EQ(kPlaybackDuration,
            ParseInt(query_info[QUERY_KEY_PLAYBACK_DURATION_REMAINING]));
  EXPECT_EQ(kRenewalServerUrl, query_info[QUERY_KEY_RENEWAL_SERVER_URL]);

  EXPECT_EQ(NO_ERROR, policy_engine_->Query(&query_info));
  EXPECT_EQ(QUERY_VALUE_STREAMING, query_info[QUERY_KEY_LICENSE_TYPE]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_PLAY_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_FALSE, query_info[QUERY_KEY_PERSIST_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_RENEW_ALLOWED]);

  EXPECT_EQ(kLowDuration - 200,
            ParseInt(query_info[QUERY_KEY_LICENSE_DURATION_REMAINING]));
  EXPECT_EQ(kPlaybackDuration,
            ParseInt(query_info[QUERY_KEY_PLAYBACK_DURATION_REMAINING]));
  EXPECT_EQ(kRenewalServerUrl, query_info[QUERY_KEY_RENEWAL_SERVER_URL]);
}

TEST_F(PolicyEngineQueryTest, QuerySuccess_PlaybackBegun) {
  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kLicenseStartTime + 50))
      .WillOnce(Return(kLicenseStartTime + 100))
      .WillOnce(Return(kLicenseStartTime + 150))
      .WillOnce(Return(kLicenseStartTime + 200));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  policy_engine_->SetLicense(license_);

  CdmQueryMap query_info;
  EXPECT_EQ(NO_ERROR, policy_engine_->Query(&query_info));
  EXPECT_EQ(QUERY_VALUE_STREAMING, query_info[QUERY_KEY_LICENSE_TYPE]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_PLAY_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_FALSE, query_info[QUERY_KEY_PERSIST_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_RENEW_ALLOWED]);

  EXPECT_EQ(kLowDuration - 50,
            ParseInt(query_info[QUERY_KEY_LICENSE_DURATION_REMAINING]));
  EXPECT_EQ(kPlaybackDuration,
            ParseInt(query_info[QUERY_KEY_PLAYBACK_DURATION_REMAINING]));
  EXPECT_EQ(kRenewalServerUrl, query_info[QUERY_KEY_RENEWAL_SERVER_URL]);

  policy_engine_->BeginDecryption();
  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));

  policy_engine_->OnTimerEvent();

  EXPECT_EQ(NO_ERROR, policy_engine_->Query(&query_info));
  EXPECT_EQ(QUERY_VALUE_STREAMING, query_info[QUERY_KEY_LICENSE_TYPE]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_PLAY_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_FALSE, query_info[QUERY_KEY_PERSIST_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_RENEW_ALLOWED]);

  EXPECT_EQ(kLowDuration - 200,
            ParseInt(query_info[QUERY_KEY_LICENSE_DURATION_REMAINING]));
  EXPECT_EQ(kPlaybackDuration - 100,
            ParseInt(query_info[QUERY_KEY_PLAYBACK_DURATION_REMAINING]));
  EXPECT_EQ(kRenewalServerUrl, query_info[QUERY_KEY_RENEWAL_SERVER_URL]);
}

TEST_F(PolicyEngineQueryTest, QuerySuccess_Offline) {
  LicenseIdentification* id = license_.mutable_id();
  id->set_type(OFFLINE);

  License_Policy* policy = license_.mutable_policy();
  policy->set_can_persist(true);
  policy->set_can_renew(false);
  policy->set_rental_duration_seconds(kRentalDuration);
  policy->set_license_duration_seconds(kLicenseDuration);

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kLicenseStartTime + 100))
      .WillOnce(Return(kLicenseStartTime + 200))
      .WillOnce(Return(kLicenseStartTime + 300));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  policy_engine_->SetLicense(license_);

  policy_engine_->OnTimerEvent();

  policy_engine_->BeginDecryption();
  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));

  CdmQueryMap query_info;
  EXPECT_EQ(NO_ERROR, policy_engine_->Query(&query_info));
  EXPECT_EQ(QUERY_VALUE_OFFLINE, query_info[QUERY_KEY_LICENSE_TYPE]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_PLAY_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_PERSIST_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_FALSE, query_info[QUERY_KEY_RENEW_ALLOWED]);

  EXPECT_EQ(kRentalDuration - 300,
            ParseInt(query_info[QUERY_KEY_LICENSE_DURATION_REMAINING]));
  EXPECT_EQ(kPlaybackDuration - 100,
            ParseInt(query_info[QUERY_KEY_PLAYBACK_DURATION_REMAINING]));
  EXPECT_EQ(kRenewalServerUrl, query_info[QUERY_KEY_RENEWAL_SERVER_URL]);
}

TEST_F(PolicyEngineQueryTest, QuerySuccess_InitialRentalDurationExpired) {
  License_Policy* policy = license_.mutable_policy();
  policy->set_rental_duration_seconds(kLowDuration);
  policy->set_license_duration_seconds(kHighDuration);

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + kLowDuration + 1))
      .WillOnce(Return(kLicenseStartTime + kLowDuration + 5));

  policy_engine_->SetLicense(license_);

  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));

  CdmQueryMap query_info;
  EXPECT_EQ(NO_ERROR, policy_engine_->Query(&query_info));
  EXPECT_EQ(QUERY_VALUE_STREAMING, query_info[QUERY_KEY_LICENSE_TYPE]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_PLAY_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_FALSE, query_info[QUERY_KEY_PERSIST_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_RENEW_ALLOWED]);

  EXPECT_EQ(0, ParseInt(query_info[QUERY_KEY_LICENSE_DURATION_REMAINING]));
  EXPECT_EQ(kPlaybackDuration,
            ParseInt(query_info[QUERY_KEY_PLAYBACK_DURATION_REMAINING]));
  EXPECT_EQ(kRenewalServerUrl, query_info[QUERY_KEY_RENEWAL_SERVER_URL]);
}

TEST_F(PolicyEngineQueryTest, QuerySuccess_InitialLicenseDurationExpired) {
  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + kLowDuration + 1))
      .WillOnce(Return(kLicenseStartTime + kLowDuration + 5));

  policy_engine_->SetLicense(license_);

  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));

  CdmQueryMap query_info;
  EXPECT_EQ(NO_ERROR, policy_engine_->Query(&query_info));
  EXPECT_EQ(QUERY_VALUE_STREAMING, query_info[QUERY_KEY_LICENSE_TYPE]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_PLAY_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_FALSE, query_info[QUERY_KEY_PERSIST_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_RENEW_ALLOWED]);

  EXPECT_EQ(0, ParseInt(query_info[QUERY_KEY_LICENSE_DURATION_REMAINING]));
  EXPECT_EQ(kPlaybackDuration,
            ParseInt(query_info[QUERY_KEY_PLAYBACK_DURATION_REMAINING]));
  EXPECT_EQ(kRenewalServerUrl, query_info[QUERY_KEY_RENEWAL_SERVER_URL]);
}

TEST_F(PolicyEngineQueryTest, QuerySuccess_CanPlayFalse) {
  LicenseIdentification* id = license_.mutable_id();
  id->set_type(OFFLINE);

  License_Policy* policy = license_.mutable_policy();
  policy->set_can_play(false);
  policy->set_can_persist(true);

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kLicenseStartTime + 5))
      .WillOnce(Return(kLicenseStartTime + 100));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  policy_engine_->SetLicense(license_);
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));

  policy_engine_->OnTimerEvent();

  policy_engine_->BeginDecryption();
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));

  CdmQueryMap query_info;
  EXPECT_EQ(NO_ERROR, policy_engine_->Query(&query_info));
  EXPECT_EQ(QUERY_VALUE_OFFLINE, query_info[QUERY_KEY_LICENSE_TYPE]);
  EXPECT_EQ(QUERY_VALUE_FALSE, query_info[QUERY_KEY_PLAY_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_PERSIST_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_RENEW_ALLOWED]);

  EXPECT_EQ(kLowDuration - 100,
            ParseInt(query_info[QUERY_KEY_LICENSE_DURATION_REMAINING]));
  EXPECT_EQ(kPlaybackDuration,
            ParseInt(query_info[QUERY_KEY_PLAYBACK_DURATION_REMAINING]));
  EXPECT_EQ(kRenewalServerUrl, query_info[QUERY_KEY_RENEWAL_SERVER_URL]);
}

TEST_F(PolicyEngineQueryTest, QuerySuccess_RentalDurationExpired) {
  License_Policy* policy = license_.mutable_policy();
  policy->set_rental_duration_seconds(kLowDuration);
  policy->set_license_duration_seconds(kHighDuration);
  policy->set_renewal_delay_seconds(GetLicenseRenewalDelay(kHighDuration));

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kPlaybackStartTime))
      .WillOnce(Return(kLicenseStartTime + kLowDuration - 1))
      .WillOnce(Return(kLicenseStartTime + kLowDuration))
      .WillOnce(Return(kLicenseStartTime + kLowDuration + 5));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  policy_engine_->SetLicense(license_);

  policy_engine_->BeginDecryption();
  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));

  for (int i = 1; i <= 2; ++i) {
    policy_engine_->OnTimerEvent();
  }

  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));

  CdmQueryMap query_info;
  EXPECT_EQ(NO_ERROR, policy_engine_->Query(&query_info));
  EXPECT_EQ(QUERY_VALUE_STREAMING, query_info[QUERY_KEY_LICENSE_TYPE]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_PLAY_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_FALSE, query_info[QUERY_KEY_PERSIST_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_RENEW_ALLOWED]);

  EXPECT_EQ(0, ParseInt(query_info[QUERY_KEY_LICENSE_DURATION_REMAINING]));
  EXPECT_EQ(kPlaybackDuration - kLowDuration,
            ParseInt(query_info[QUERY_KEY_PLAYBACK_DURATION_REMAINING]));
  EXPECT_EQ(kRenewalServerUrl, query_info[QUERY_KEY_RENEWAL_SERVER_URL]);
}

TEST_F(PolicyEngineQueryTest, QuerySuccess_PlaybackDurationExpired) {
  License_Policy* policy = license_.mutable_policy();
  policy->set_playback_duration_seconds(kLowDuration);
  policy->set_license_duration_seconds(kHighDuration);
  policy->set_renewal_delay_seconds(GetLicenseRenewalDelay(kHighDuration));

  int64_t playback_start_time = kLicenseStartTime + 10000;
  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(playback_start_time))
      .WillOnce(Return(playback_start_time - 2 + kLowDuration))
      .WillOnce(Return(playback_start_time + 2 + kLowDuration))
      .WillOnce(Return(playback_start_time + 5 + kLowDuration));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  policy_engine_->SetLicense(license_);

  policy_engine_->BeginDecryption();
  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));

  for (int i = 1; i <= 2; ++i) {
    policy_engine_->OnTimerEvent();
  }

  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));

  CdmQueryMap query_info;
  EXPECT_EQ(NO_ERROR, policy_engine_->Query(&query_info));
  EXPECT_EQ(QUERY_VALUE_STREAMING, query_info[QUERY_KEY_LICENSE_TYPE]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_PLAY_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_FALSE, query_info[QUERY_KEY_PERSIST_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_RENEW_ALLOWED]);

  EXPECT_EQ(0, ParseInt(query_info[QUERY_KEY_LICENSE_DURATION_REMAINING]));
  EXPECT_EQ(0, ParseInt(query_info[QUERY_KEY_PLAYBACK_DURATION_REMAINING]));
  EXPECT_EQ(kRenewalServerUrl, query_info[QUERY_KEY_RENEWAL_SERVER_URL]);
}

TEST_F(PolicyEngineQueryTest, QuerySuccess_LicenseDurationExpired) {
  License_Policy* policy = license_.mutable_policy();
  policy->set_can_renew(false);

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kPlaybackStartTime))
      .WillOnce(Return(kLicenseStartTime + kLowDuration - 1))
      .WillOnce(Return(kLicenseStartTime + kLowDuration))
      .WillOnce(Return(kLicenseStartTime + kLowDuration + 5));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  policy_engine_->SetLicense(license_);

  policy_engine_->BeginDecryption();
  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));

  for (int i = 1; i <= 2; ++i) {
    policy_engine_->OnTimerEvent();
  }

  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));

  CdmQueryMap query_info;
  EXPECT_EQ(NO_ERROR, policy_engine_->Query(&query_info));
  EXPECT_EQ(QUERY_VALUE_STREAMING, query_info[QUERY_KEY_LICENSE_TYPE]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_PLAY_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_FALSE, query_info[QUERY_KEY_PERSIST_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_FALSE, query_info[QUERY_KEY_RENEW_ALLOWED]);

  EXPECT_EQ(0, ParseInt(query_info[QUERY_KEY_LICENSE_DURATION_REMAINING]));
  EXPECT_EQ(kPlaybackDuration - kLowDuration,
            ParseInt(query_info[QUERY_KEY_PLAYBACK_DURATION_REMAINING]));
  EXPECT_EQ(kRenewalServerUrl, query_info[QUERY_KEY_RENEWAL_SERVER_URL]);
}

TEST_F(PolicyEngineQueryTest, QuerySuccess_RentalDuration0) {
  License_Policy* policy = license_.mutable_policy();
  policy->set_rental_duration_seconds(kDurationUnlimited);
  int64_t license_renewal_delay = GetLicenseRenewalDelay(kLowDuration);

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kLicenseStartTime + 5))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay - 1))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay))
      .WillOnce(Return(kLicenseStartTime + kLowDuration - 1))
      .WillOnce(Return(kLicenseStartTime + kLowDuration))
      .WillOnce(Return(kLicenseStartTime + kLowDuration + 5));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  policy_engine_->SetLicense(license_);

  policy_engine_->BeginDecryption();
  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));

  for (int i = 1; i <= 4; ++i) {
    policy_engine_->OnTimerEvent();
  }

  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));

  CdmQueryMap query_info;
  EXPECT_EQ(NO_ERROR, policy_engine_->Query(&query_info));
  EXPECT_EQ(QUERY_VALUE_STREAMING, query_info[QUERY_KEY_LICENSE_TYPE]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_PLAY_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_FALSE, query_info[QUERY_KEY_PERSIST_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_RENEW_ALLOWED]);

  EXPECT_EQ(0, ParseInt(query_info[QUERY_KEY_LICENSE_DURATION_REMAINING]));
  EXPECT_EQ(kPlaybackDuration - kLowDuration,
            ParseInt(query_info[QUERY_KEY_PLAYBACK_DURATION_REMAINING]));
  EXPECT_EQ(kRenewalServerUrl, query_info[QUERY_KEY_RENEWAL_SERVER_URL]);
}

TEST_F(PolicyEngineQueryTest, QuerySuccess_PlaybackDuration0) {
  License_Policy* policy = license_.mutable_policy();
  policy->set_playback_duration_seconds(kDurationUnlimited);
  policy->set_license_duration_seconds(kHighDuration);
  int64_t license_renewal_delay = GetLicenseRenewalDelay(kHighDuration);
  policy->set_renewal_delay_seconds(license_renewal_delay);

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime))
      .WillOnce(Return(kLicenseStartTime + 5))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay - 2))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay + 2))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay + 5))
      .WillOnce(Return(kLicenseStartTime + kHighDuration - 2))
      .WillOnce(Return(kLicenseStartTime + kHighDuration + 2))
      .WillOnce(Return(kLicenseStartTime + kHighDuration + 5));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  policy_engine_->SetLicense(license_);

  policy_engine_->BeginDecryption();
  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));

  for (int i = 1; i <= 2; ++i) {
    policy_engine_->OnTimerEvent();
  }

  CdmQueryMap query_info;
  EXPECT_EQ(NO_ERROR, policy_engine_->Query(&query_info));
  EXPECT_EQ(QUERY_VALUE_STREAMING, query_info[QUERY_KEY_LICENSE_TYPE]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_PLAY_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_FALSE, query_info[QUERY_KEY_PERSIST_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_RENEW_ALLOWED]);

  EXPECT_EQ(0,
            ParseInt(query_info[QUERY_KEY_LICENSE_DURATION_REMAINING]));
  EXPECT_EQ(LLONG_MAX,
            ParseInt(query_info[QUERY_KEY_PLAYBACK_DURATION_REMAINING]));
  EXPECT_EQ(kRenewalServerUrl, query_info[QUERY_KEY_RENEWAL_SERVER_URL]);

  for (int i = 3; i <= 4; ++i) {
    policy_engine_->OnTimerEvent();
  }

  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));

  EXPECT_EQ(NO_ERROR, policy_engine_->Query(&query_info));
  EXPECT_EQ(QUERY_VALUE_STREAMING, query_info[QUERY_KEY_LICENSE_TYPE]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_PLAY_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_FALSE, query_info[QUERY_KEY_PERSIST_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_RENEW_ALLOWED]);

  EXPECT_EQ(0,
            ParseInt(query_info[QUERY_KEY_LICENSE_DURATION_REMAINING]));
  EXPECT_EQ(LLONG_MAX,
            ParseInt(query_info[QUERY_KEY_PLAYBACK_DURATION_REMAINING]));
  EXPECT_EQ(kRenewalServerUrl, query_info[QUERY_KEY_RENEWAL_SERVER_URL]);
}

TEST_F(PolicyEngineQueryTest, QuerySuccess_LicenseDuration0) {
  License_Policy* policy = license_.mutable_policy();
  policy->set_license_duration_seconds(kDurationUnlimited);

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kPlaybackStartTime))
      .WillOnce(Return(kPlaybackStartTime + kPlaybackDuration - 1))
      .WillOnce(Return(kPlaybackStartTime + kPlaybackDuration))
      .WillOnce(Return(kPlaybackStartTime + kPlaybackDuration + 5));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  policy_engine_->SetLicense(license_);

  policy_engine_->BeginDecryption();
  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));

  for (int i = 1; i <= 2; ++i) {
    policy_engine_->OnTimerEvent();
  }

  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));

  CdmQueryMap query_info;
  EXPECT_EQ(NO_ERROR, policy_engine_->Query(&query_info));
  EXPECT_EQ(QUERY_VALUE_STREAMING, query_info[QUERY_KEY_LICENSE_TYPE]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_PLAY_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_FALSE, query_info[QUERY_KEY_PERSIST_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_RENEW_ALLOWED]);

  EXPECT_EQ(0, ParseInt(query_info[QUERY_KEY_LICENSE_DURATION_REMAINING]));
  EXPECT_EQ(0, ParseInt(query_info[QUERY_KEY_PLAYBACK_DURATION_REMAINING]));
  EXPECT_EQ(kRenewalServerUrl, query_info[QUERY_KEY_RENEWAL_SERVER_URL]);
}

TEST_F(PolicyEngineQueryTest, QuerySuccess_PlaybackAndRental0) {
  License_Policy* policy = license_.mutable_policy();
  policy->set_rental_duration_seconds(kDurationUnlimited);
  policy->set_playback_duration_seconds(kDurationUnlimited);
  policy->set_license_duration_seconds(kLowDuration);
  // Only |license_duration_seconds| set.

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kPlaybackStartTime))
      .WillOnce(Return(kLicenseStartTime + 10))
      .WillOnce(Return(kLicenseStartTime + kLowDuration - 10))
      .WillOnce(Return(kLicenseStartTime + kLowDuration + 10))
      .WillOnce(Return(kLicenseStartTime + kLowDuration + 10));

  policy_engine_->SetLicense(license_);
  policy_engine_->BeginDecryption();
  policy_engine_->OnTimerEvent();

  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kSomeRandomKeyId));

  CdmQueryMap query_info;
  EXPECT_EQ(NO_ERROR, policy_engine_->Query(&query_info));
  EXPECT_EQ(QUERY_VALUE_STREAMING, query_info[QUERY_KEY_LICENSE_TYPE]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_PLAY_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_FALSE, query_info[QUERY_KEY_PERSIST_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_RENEW_ALLOWED]);

  EXPECT_EQ(10, ParseInt(query_info[QUERY_KEY_LICENSE_DURATION_REMAINING]));
  EXPECT_EQ(LLONG_MAX,
            ParseInt(query_info[QUERY_KEY_PLAYBACK_DURATION_REMAINING]));
  EXPECT_EQ(kRenewalServerUrl, query_info[QUERY_KEY_RENEWAL_SERVER_URL]);

  policy_engine_->OnTimerEvent();

  EXPECT_EQ(NO_ERROR, policy_engine_->Query(&query_info));
  EXPECT_EQ(QUERY_VALUE_STREAMING, query_info[QUERY_KEY_LICENSE_TYPE]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_PLAY_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_FALSE, query_info[QUERY_KEY_PERSIST_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_RENEW_ALLOWED]);

  EXPECT_EQ(0, ParseInt(query_info[QUERY_KEY_LICENSE_DURATION_REMAINING]));
  EXPECT_EQ(LLONG_MAX,
            ParseInt(query_info[QUERY_KEY_PLAYBACK_DURATION_REMAINING]));
  EXPECT_EQ(kRenewalServerUrl, query_info[QUERY_KEY_RENEWAL_SERVER_URL]);
}

TEST_F(PolicyEngineQueryTest, QuerySuccess_PlaybackAndLicense0_WithoutPlayback) {
  License_Policy* policy = license_.mutable_policy();
  policy->set_rental_duration_seconds(kRentalDuration);
  policy->set_playback_duration_seconds(kDurationUnlimited);
  policy->set_license_duration_seconds(kDurationUnlimited);
  // Only |rental_duration_seconds| set.

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kLicenseStartTime + 10))
      .WillOnce(Return(kLicenseStartTime + kRentalDuration - 10))
      .WillOnce(Return(kLicenseStartTime + kRentalDuration - 10))
      .WillOnce(Return(kLicenseStartTime + kRentalDuration + 10))
      .WillOnce(Return(kLicenseStartTime + kRentalDuration + 10));

  policy_engine_->SetLicense(license_);
  policy_engine_->OnTimerEvent();

  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kSomeRandomKeyId));

  policy_engine_->OnTimerEvent();

  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kSomeRandomKeyId));

  CdmQueryMap query_info;
  EXPECT_EQ(NO_ERROR, policy_engine_->Query(&query_info));
  EXPECT_EQ(QUERY_VALUE_STREAMING, query_info[QUERY_KEY_LICENSE_TYPE]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_PLAY_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_FALSE, query_info[QUERY_KEY_PERSIST_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_RENEW_ALLOWED]);

  EXPECT_EQ(10, ParseInt(query_info[QUERY_KEY_LICENSE_DURATION_REMAINING]));
  EXPECT_EQ(LLONG_MAX,
            ParseInt(query_info[QUERY_KEY_PLAYBACK_DURATION_REMAINING]));
  EXPECT_EQ(kRenewalServerUrl, query_info[QUERY_KEY_RENEWAL_SERVER_URL]);

  policy_engine_->OnTimerEvent();

  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kSomeRandomKeyId));

  EXPECT_EQ(NO_ERROR, policy_engine_->Query(&query_info));
  EXPECT_EQ(QUERY_VALUE_STREAMING, query_info[QUERY_KEY_LICENSE_TYPE]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_PLAY_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_FALSE, query_info[QUERY_KEY_PERSIST_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_RENEW_ALLOWED]);

  EXPECT_EQ(0, ParseInt(query_info[QUERY_KEY_LICENSE_DURATION_REMAINING]));
  EXPECT_EQ(LLONG_MAX,
            ParseInt(query_info[QUERY_KEY_PLAYBACK_DURATION_REMAINING]));
  EXPECT_EQ(kRenewalServerUrl, query_info[QUERY_KEY_RENEWAL_SERVER_URL]);
}

TEST_F(PolicyEngineQueryTest, QuerySuccess_PlaybackAndLicense0_WithPlayback) {
  License_Policy* policy = license_.mutable_policy();
  policy->set_rental_duration_seconds(kRentalDuration);
  policy->set_playback_duration_seconds(kDurationUnlimited);
  policy->set_license_duration_seconds(kDurationUnlimited);
  // Only |rental_duration_seconds| set.

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kPlaybackStartTime))
      .WillOnce(Return(kLicenseStartTime + kPlaybackDuration - 10))
      .WillOnce(Return(kLicenseStartTime + kPlaybackDuration - 10))
      .WillOnce(Return(kLicenseStartTime + kPlaybackDuration + 10))
      .WillOnce(Return(kLicenseStartTime + kPlaybackDuration + 10))
      .WillOnce(Return(kLicenseStartTime + kRentalDuration + 10))
      .WillOnce(Return(kLicenseStartTime + kRentalDuration + 10));

  policy_engine_->SetLicense(license_);
  policy_engine_->BeginDecryption();
  policy_engine_->OnTimerEvent();

  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kSomeRandomKeyId));

  CdmQueryMap query_info;
  EXPECT_EQ(NO_ERROR, policy_engine_->Query(&query_info));
  EXPECT_EQ(QUERY_VALUE_STREAMING, query_info[QUERY_KEY_LICENSE_TYPE]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_PLAY_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_FALSE, query_info[QUERY_KEY_PERSIST_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_RENEW_ALLOWED]);

  EXPECT_EQ(kRentalDuration - kPlaybackDuration + 10,
            ParseInt(query_info[QUERY_KEY_LICENSE_DURATION_REMAINING]));
  EXPECT_EQ(LLONG_MAX,
            ParseInt(query_info[QUERY_KEY_PLAYBACK_DURATION_REMAINING]));
  EXPECT_EQ(kRenewalServerUrl, query_info[QUERY_KEY_RENEWAL_SERVER_URL]);

  policy_engine_->OnTimerEvent();

  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kSomeRandomKeyId));

  EXPECT_EQ(NO_ERROR, policy_engine_->Query(&query_info));
  EXPECT_EQ(QUERY_VALUE_STREAMING, query_info[QUERY_KEY_LICENSE_TYPE]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_PLAY_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_FALSE, query_info[QUERY_KEY_PERSIST_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_RENEW_ALLOWED]);

  EXPECT_EQ(kRentalDuration - kPlaybackDuration - 10,
            ParseInt(query_info[QUERY_KEY_LICENSE_DURATION_REMAINING]));
  EXPECT_EQ(LLONG_MAX,
            ParseInt(query_info[QUERY_KEY_PLAYBACK_DURATION_REMAINING]));
  EXPECT_EQ(kRenewalServerUrl, query_info[QUERY_KEY_RENEWAL_SERVER_URL]);

  policy_engine_->OnTimerEvent();

  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kSomeRandomKeyId));

  EXPECT_EQ(NO_ERROR, policy_engine_->Query(&query_info));
  EXPECT_EQ(QUERY_VALUE_STREAMING, query_info[QUERY_KEY_LICENSE_TYPE]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_PLAY_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_FALSE, query_info[QUERY_KEY_PERSIST_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_RENEW_ALLOWED]);

  EXPECT_EQ(0, ParseInt(query_info[QUERY_KEY_LICENSE_DURATION_REMAINING]));
  EXPECT_EQ(LLONG_MAX,
            ParseInt(query_info[QUERY_KEY_PLAYBACK_DURATION_REMAINING]));
  EXPECT_EQ(kRenewalServerUrl, query_info[QUERY_KEY_RENEWAL_SERVER_URL]);
}

TEST_F(PolicyEngineQueryTest, QuerySuccess_RentalAndLicense0_WithoutPlayback) {
  License_Policy* policy = license_.mutable_policy();
  policy->set_rental_duration_seconds(kDurationUnlimited);
  policy->set_playback_duration_seconds(kPlaybackDuration);
  policy->set_license_duration_seconds(kDurationUnlimited);
  // Only |playback_duration_seconds| set.

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kLicenseStartTime + 10))
      .WillOnce(Return(kLicenseStartTime + 10))
      .WillOnce(Return(kLicenseStartTime + kPlaybackDuration + 10))
      .WillOnce(Return(kLicenseStartTime + kPlaybackDuration + 10));

  policy_engine_->SetLicense(license_);
  policy_engine_->OnTimerEvent();

  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kSomeRandomKeyId));

  CdmQueryMap query_info;
  EXPECT_EQ(NO_ERROR, policy_engine_->Query(&query_info));
  EXPECT_EQ(QUERY_VALUE_STREAMING, query_info[QUERY_KEY_LICENSE_TYPE]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_PLAY_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_FALSE, query_info[QUERY_KEY_PERSIST_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_RENEW_ALLOWED]);

  EXPECT_EQ(LLONG_MAX,
            ParseInt(query_info[QUERY_KEY_LICENSE_DURATION_REMAINING]));
  EXPECT_EQ(kPlaybackDuration,
            ParseInt(query_info[QUERY_KEY_PLAYBACK_DURATION_REMAINING]));
  EXPECT_EQ(kRenewalServerUrl, query_info[QUERY_KEY_RENEWAL_SERVER_URL]);

  policy_engine_->OnTimerEvent();

  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kSomeRandomKeyId));

  EXPECT_EQ(NO_ERROR, policy_engine_->Query(&query_info));
  EXPECT_EQ(QUERY_VALUE_STREAMING, query_info[QUERY_KEY_LICENSE_TYPE]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_PLAY_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_FALSE, query_info[QUERY_KEY_PERSIST_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_RENEW_ALLOWED]);

  EXPECT_EQ(LLONG_MAX,
            ParseInt(query_info[QUERY_KEY_LICENSE_DURATION_REMAINING]));
  EXPECT_EQ(kPlaybackDuration,
            ParseInt(query_info[QUERY_KEY_PLAYBACK_DURATION_REMAINING]));
  EXPECT_EQ(kRenewalServerUrl, query_info[QUERY_KEY_RENEWAL_SERVER_URL]);
}

TEST_F(PolicyEngineQueryTest, QuerySuccess_RentalAndLicense0_WithPlayback) {
  License_Policy* policy = license_.mutable_policy();
  policy->set_rental_duration_seconds(kDurationUnlimited);
  policy->set_playback_duration_seconds(kPlaybackDuration);
  policy->set_license_duration_seconds(kDurationUnlimited);
  // Only |playback_duration_seconds| set.

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kPlaybackStartTime))
      .WillOnce(Return(kPlaybackStartTime + 10))
      .WillOnce(Return(kPlaybackStartTime + 10))
      .WillOnce(Return(kPlaybackStartTime + kPlaybackDuration - 10))
      .WillOnce(Return(kPlaybackStartTime + kPlaybackDuration - 10))
      .WillOnce(Return(kPlaybackStartTime + kPlaybackDuration + 10))
      .WillOnce(Return(kPlaybackStartTime + kPlaybackDuration + 10));

  policy_engine_->SetLicense(license_);
  policy_engine_->BeginDecryption();
  policy_engine_->OnTimerEvent();

  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kSomeRandomKeyId));

  CdmQueryMap query_info;
  EXPECT_EQ(NO_ERROR, policy_engine_->Query(&query_info));
  EXPECT_EQ(QUERY_VALUE_STREAMING, query_info[QUERY_KEY_LICENSE_TYPE]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_PLAY_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_FALSE, query_info[QUERY_KEY_PERSIST_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_RENEW_ALLOWED]);

  EXPECT_EQ(LLONG_MAX,
            ParseInt(query_info[QUERY_KEY_LICENSE_DURATION_REMAINING]));
  EXPECT_EQ(kPlaybackDuration - 10,
            ParseInt(query_info[QUERY_KEY_PLAYBACK_DURATION_REMAINING]));
  EXPECT_EQ(kRenewalServerUrl, query_info[QUERY_KEY_RENEWAL_SERVER_URL]);

  policy_engine_->OnTimerEvent();

  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kSomeRandomKeyId));

  EXPECT_EQ(NO_ERROR, policy_engine_->Query(&query_info));
  EXPECT_EQ(QUERY_VALUE_STREAMING, query_info[QUERY_KEY_LICENSE_TYPE]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_PLAY_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_FALSE, query_info[QUERY_KEY_PERSIST_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_RENEW_ALLOWED]);

  EXPECT_EQ(LLONG_MAX,
            ParseInt(query_info[QUERY_KEY_LICENSE_DURATION_REMAINING]));
  EXPECT_EQ(10,
            ParseInt(query_info[QUERY_KEY_PLAYBACK_DURATION_REMAINING]));
  EXPECT_EQ(kRenewalServerUrl, query_info[QUERY_KEY_RENEWAL_SERVER_URL]);

  policy_engine_->OnTimerEvent();

  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kSomeRandomKeyId));

  EXPECT_EQ(NO_ERROR, policy_engine_->Query(&query_info));
  EXPECT_EQ(QUERY_VALUE_STREAMING, query_info[QUERY_KEY_LICENSE_TYPE]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_PLAY_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_FALSE, query_info[QUERY_KEY_PERSIST_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_RENEW_ALLOWED]);

  EXPECT_EQ(0,
            ParseInt(query_info[QUERY_KEY_LICENSE_DURATION_REMAINING]));
  EXPECT_EQ(0,
            ParseInt(query_info[QUERY_KEY_PLAYBACK_DURATION_REMAINING]));
  EXPECT_EQ(kRenewalServerUrl, query_info[QUERY_KEY_RENEWAL_SERVER_URL]);
}

TEST_F(PolicyEngineQueryTest, QuerySuccess_Durations0) {
  License_Policy* policy = license_.mutable_policy();
  policy->set_rental_duration_seconds(kDurationUnlimited);
  policy->set_playback_duration_seconds(kDurationUnlimited);
  policy->set_license_duration_seconds(kDurationUnlimited);
  policy->set_renewal_delay_seconds(kHighDuration + 10);

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kLicenseStartTime + 5))
      .WillOnce(Return(kLicenseStartTime + kHighDuration))
      .WillOnce(Return(kLicenseStartTime + kHighDuration + 9))
      .WillOnce(Return(kLicenseStartTime + kHighDuration + 15));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  policy_engine_->SetLicense(license_);

  policy_engine_->BeginDecryption();
  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));

  policy_engine_->OnTimerEvent();
  policy_engine_->OnTimerEvent();

  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));

  CdmQueryMap query_info;
  EXPECT_EQ(NO_ERROR, policy_engine_->Query(&query_info));
  EXPECT_EQ(QUERY_VALUE_STREAMING, query_info[QUERY_KEY_LICENSE_TYPE]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_PLAY_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_FALSE, query_info[QUERY_KEY_PERSIST_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_RENEW_ALLOWED]);

  EXPECT_EQ(LLONG_MAX,
            ParseInt(query_info[QUERY_KEY_LICENSE_DURATION_REMAINING]));
  EXPECT_EQ(LLONG_MAX,
            ParseInt(query_info[QUERY_KEY_PLAYBACK_DURATION_REMAINING]));
  EXPECT_EQ(kRenewalServerUrl, query_info[QUERY_KEY_RENEWAL_SERVER_URL]);
}

TEST_F(PolicyEngineQueryTest, QuerySuccess_LicenseWithFutureStartTime) {
  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime - 100))
      .WillOnce(Return(kLicenseStartTime - 50))
      .WillOnce(Return(kLicenseStartTime - 10))
      .WillOnce(Return(kLicenseStartTime))
      .WillOnce(Return(kLicenseStartTime + 10))
      .WillOnce(Return(kLicenseStartTime + 25));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  policy_engine_->SetLicense(license_);

  policy_engine_->OnTimerEvent();
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));

  CdmQueryMap query_info;
  EXPECT_EQ(NO_ERROR, policy_engine_->Query(&query_info));
  EXPECT_EQ(QUERY_VALUE_STREAMING, query_info[QUERY_KEY_LICENSE_TYPE]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_PLAY_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_FALSE, query_info[QUERY_KEY_PERSIST_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_RENEW_ALLOWED]);

  EXPECT_EQ(kLowDuration,
            ParseInt(query_info[QUERY_KEY_LICENSE_DURATION_REMAINING]));
  EXPECT_EQ(kPlaybackDuration,
            ParseInt(query_info[QUERY_KEY_PLAYBACK_DURATION_REMAINING]));
  EXPECT_EQ(kRenewalServerUrl, query_info[QUERY_KEY_RENEWAL_SERVER_URL]);

  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));

  policy_engine_->OnTimerEvent();
  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
  policy_engine_->BeginDecryption();

  EXPECT_EQ(NO_ERROR, policy_engine_->Query(&query_info));
  EXPECT_EQ(QUERY_VALUE_STREAMING, query_info[QUERY_KEY_LICENSE_TYPE]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_PLAY_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_FALSE, query_info[QUERY_KEY_PERSIST_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_RENEW_ALLOWED]);

  EXPECT_EQ(kLowDuration - 25,
            ParseInt(query_info[QUERY_KEY_LICENSE_DURATION_REMAINING]));
  EXPECT_EQ(kPlaybackDuration - 15,
            ParseInt(query_info[QUERY_KEY_PLAYBACK_DURATION_REMAINING]));
  EXPECT_EQ(kRenewalServerUrl, query_info[QUERY_KEY_RENEWAL_SERVER_URL]);
}

TEST_F(PolicyEngineQueryTest, QuerySuccess_Renew) {
  int64_t license_renewal_delay = license_.policy().renewal_delay_seconds();

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kLicenseStartTime + 5))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay - 25))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay + 10))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay + 20))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay +
                       kLicenseRenewalRetryInterval + 10))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay +
                       kLicenseRenewalRetryInterval + 15));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  policy_engine_->SetLicense(license_);

  policy_engine_->BeginDecryption();
  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));

  for (int i = 1; i <= 2; ++i) {
    policy_engine_->OnTimerEvent();
  }

  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));

  license_.set_license_start_time(kLicenseStartTime + license_renewal_delay +
                                  15);
  LicenseIdentification* id = license_.mutable_id();
  id->set_version(2);
  policy_engine_->UpdateLicense(license_);

  policy_engine_->OnTimerEvent();
  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));

  CdmQueryMap query_info;
  EXPECT_EQ(NO_ERROR, policy_engine_->Query(&query_info));
  EXPECT_EQ(QUERY_VALUE_STREAMING, query_info[QUERY_KEY_LICENSE_TYPE]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_PLAY_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_FALSE, query_info[QUERY_KEY_PERSIST_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_RENEW_ALLOWED]);

  EXPECT_EQ(kLowDuration - kLicenseRenewalRetryInterval,
            ParseInt(query_info[QUERY_KEY_LICENSE_DURATION_REMAINING]));
  EXPECT_EQ(kPlaybackDuration + 5 - license_renewal_delay -
                kLicenseRenewalRetryInterval - 15,
            ParseInt(query_info[QUERY_KEY_PLAYBACK_DURATION_REMAINING]));
  EXPECT_EQ(kRenewalServerUrl, query_info[QUERY_KEY_RENEWAL_SERVER_URL]);
}

TEST_F(PolicyEngineQueryTest, QuerySuccess_RenewWithFutureStartTime) {
  int64_t license_renewal_delay = license_.policy().renewal_delay_seconds();

  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kLicenseStartTime + 5))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay - 25))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay + 10))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay + 20))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay +
                       kLicenseRenewalRetryInterval + 10))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay +
                       kLicenseRenewalRetryInterval + 20))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay +
                       kLicenseRenewalRetryInterval + 30))
      .WillOnce(Return(kLicenseStartTime + license_renewal_delay +
                       kLicenseRenewalRetryInterval + 40));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  policy_engine_->SetLicense(license_);

  policy_engine_->BeginDecryption();
  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));

  for (int i = 1; i <= 2; ++i) {
    policy_engine_->OnTimerEvent();
  }

  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));

  license_.set_license_start_time(kLicenseStartTime + license_renewal_delay +
                                  kLicenseRenewalRetryInterval + 20);
  LicenseIdentification* id = license_.mutable_id();
  id->set_version(2);
  policy_engine_->UpdateLicense(license_);

  policy_engine_->OnTimerEvent();
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));

  CdmQueryMap query_info;
  EXPECT_EQ(NO_ERROR, policy_engine_->Query(&query_info));
  EXPECT_EQ(QUERY_VALUE_STREAMING, query_info[QUERY_KEY_LICENSE_TYPE]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_PLAY_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_FALSE, query_info[QUERY_KEY_PERSIST_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_RENEW_ALLOWED]);

  EXPECT_EQ(kLowDuration,
            ParseInt(query_info[QUERY_KEY_LICENSE_DURATION_REMAINING]));
  EXPECT_EQ(kPlaybackDuration + 5 - license_renewal_delay -
                kLicenseRenewalRetryInterval - 20,
            ParseInt(query_info[QUERY_KEY_PLAYBACK_DURATION_REMAINING]));
  EXPECT_EQ(kRenewalServerUrl, query_info[QUERY_KEY_RENEWAL_SERVER_URL]);

  policy_engine_->OnTimerEvent();
  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));

  EXPECT_EQ(NO_ERROR, policy_engine_->Query(&query_info));
  EXPECT_EQ(QUERY_VALUE_STREAMING, query_info[QUERY_KEY_LICENSE_TYPE]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_PLAY_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_FALSE, query_info[QUERY_KEY_PERSIST_ALLOWED]);
  EXPECT_EQ(QUERY_VALUE_TRUE, query_info[QUERY_KEY_RENEW_ALLOWED]);

  EXPECT_EQ(kLowDuration - 20,
            ParseInt(query_info[QUERY_KEY_LICENSE_DURATION_REMAINING]));
  EXPECT_EQ(kPlaybackDuration + 5 - license_renewal_delay -
                kLicenseRenewalRetryInterval - 40,
            ParseInt(query_info[QUERY_KEY_PLAYBACK_DURATION_REMAINING]));
  EXPECT_EQ(kRenewalServerUrl, query_info[QUERY_KEY_RENEWAL_SERVER_URL]);
}

TEST_F(PolicyEngineTest, SetLicenseForRelease) {
  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1));

  // No key change event will fire.
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kRentalDuration));
  policy_engine_->SetLicenseForRelease(license_);
  // No keys were loaded.
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));
}

TEST_F(PolicyEngineTest, SetLicenseForReleaseAfterSetLicense) {
  EXPECT_CALL(*mock_clock_, GetCurrentTime())
      .WillOnce(Return(kLicenseStartTime + 1))
      .WillOnce(Return(kPlaybackStartTime))
      .WillOnce(Return(kLicenseStartTime + 10));

  ExpectSessionKeysChange(kKeyStatusUsable, true);
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kLicenseStartTime + kRentalDuration));
  EXPECT_CALL(mock_event_listener_,
              OnExpirationUpdate(_, kPlaybackStartTime + kPlaybackDuration));

  EXPECT_CALL(crypto_session_, GetHdcpCapabilities(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<0>(HDCP_NO_DIGITAL_OUTPUT),
                Return(GET_HDCP_CAPABILITY_FAILED)));

  policy_engine_->SetLicense(license_);
  policy_engine_->BeginDecryption();
  policy_engine_->OnTimerEvent();
  EXPECT_TRUE(policy_engine_->CanDecryptContent(kKeyId));
  ::testing::Mock::VerifyAndClear(&mock_event_listener_);

  // Set the license again with use_keys set to false.
  // This would happen when asking the session to generate a release message
  // on an existing session.
  ExpectSessionKeysChange(kKeyStatusExpired, false);
  policy_engine_->SetLicenseForRelease(license_);
  EXPECT_FALSE(policy_engine_->CanDecryptContent(kKeyId));
}

}  // namespace wvcdm
