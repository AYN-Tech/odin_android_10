// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vector>
#include "license.h"
#include "license_key_status.h"

namespace wvcdm {

namespace {

static const uint32_t dev_lo_res = 200;
static const uint32_t dev_hi_res = 400;
static const uint32_t dev_top_res = 800;

static const uint32_t key_lo_res_min = 151;
static const uint32_t key_lo_res_max = 300;
static const uint32_t key_hi_res_min = 301;
static const uint32_t key_hi_res_max = 450;
static const uint32_t key_top_res_min = 451;
static const uint32_t key_top_res_max = 650;

// Content Keys
static const KeyId ck_sw_crypto = "c_key_SW_SECURE_CRYPTO";
static const KeyId ck_sw_decode = "c_key_SW_SECURE_DECODE";
static const KeyId ck_hw_crypto = "c_key_HW_SECURE_CRYPTO";
static const KeyId ck_hw_decode = "c_key_HW_SECURE_DECODE";
static const KeyId ck_hw_secure = "c_key_HW_SECURE_ALL";

// Operator Session Keys
static const KeyId osk_decrypt = "os_key_generic_decrypt";
static const KeyId osk_encrypt = "os_key_generic_encrypt";
static const KeyId osk_sign = "os_key_generic_sign";
static const KeyId osk_verify = "os_key_generic_verify";
static const KeyId osk_encrypt_decrypt = "os_key_generic_encrypt_decrypt";
static const KeyId osk_sign_verify = "os_key_generic_sign_verify";
static const KeyId osk_all = "os_key_generic_all";

// HDCP test keys
static const KeyId ck_sw_crypto_NO_HDCP = "ck_sw_crypto_NO_HDCP";
static const KeyId ck_hw_secure_NO_HDCP = "ck_hw_secure_NO_HDCP";
static const KeyId ck_sw_crypto_HDCP_V2_1 = "ck_sw_crypto_HDCP_V2_1";
static const KeyId ck_hw_secure_HDCP_V2_1 = "ck_hw_secure_HDCP_V2_1";
static const KeyId ck_sw_crypto_HDCP_V2_2 = "ck_sw_crypto_HDCP_V2_2";
static const KeyId ck_hw_secure_HDCP_V2_2 = "ck_hw_secure_HDCP_V2_2";
static const KeyId ck_sw_crypto_HDCP_V2_3 = "ck_sw_crypto_HDCP_V2_3";
static const KeyId ck_hw_secure_HDCP_V2_3 = "ck_hw_secure_HDCP_V2_3";
static const KeyId ck_sw_crypto_HDCP_NO_OUTPUT = "ck_sw_crypto_HDCP_NO_OUT";
static const KeyId ck_hw_secure_HDCP_NO_OUTPUT = "ck_hw_secure_HDCP_NO_OUT";

// Constraint test keys
static const KeyId ck_NO_HDCP_lo_res = "ck_NO_HDCP_lo_res";
static const KeyId ck_HDCP_NO_OUTPUT_hi_res = "ck_HDCP_NO_OUTPUT_hi_res";
static const KeyId ck_HDCP_V2_1_hi_res = "ck_HDCP_V2_1_hi_res";
static const KeyId ck_HDCP_V2_2_max_res = "ck_HDCP_V2_2_max_res";
static const KeyId ck_HDCP_V2_3_max_res = "ck_HDCP_V2_3_max_res";
static const KeyId ck_NO_HDCP_dual_res = "ck_NO_HDCP_dual_res";

}  // namespace

// protobuf generated classes.
using video_widevine::License;
using video_widevine::LicenseIdentification;
using video_widevine::STREAMING;
using video_widevine::OFFLINE;

typedef ::video_widevine::License::KeyContainer KeyContainer;
typedef KeyContainer::VideoResolutionConstraint VideoResolutionConstraint;

class LicenseKeysTest : public ::testing::Test {
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

  void SetUp() override {
    license_keys_.reset(new LicenseKeys(kSecurityLevelL1));
    LicenseIdentification* id = license_.mutable_id();
    id->set_version(1);
    id->set_type(STREAMING);
  }

  virtual void AddContentKey(
      const KeyId& key_id, bool set_level = false,
      KeyContainer::SecurityLevel level = KeyContainer::SW_SECURE_CRYPTO,
      bool set_hdcp = false, KeyContainer::OutputProtection::HDCP hdcp_value =
          KeyContainer::OutputProtection::HDCP_NONE,
      bool set_constraints = false,
      std::vector<VideoResolutionConstraint>* constraints = NULL) {
    KeyContainer* key = license_.add_key();
    key->set_type(KeyContainer::CONTENT);
    if (set_level) {
      key->set_level(level);
    }
    if (set_hdcp) {
      KeyContainer::OutputProtection* pro = key->mutable_required_protection();
      pro->set_hdcp(hdcp_value);
    }
    if (set_constraints) {
      for (std::vector<VideoResolutionConstraint>::iterator
               it = constraints->begin(); it != constraints->end(); ++it) {
        VideoResolutionConstraint* constraint =
            key->add_video_resolution_constraints();
        constraint->set_min_resolution_pixels(it->min_resolution_pixels());
        constraint->set_max_resolution_pixels(it->max_resolution_pixels());
        constraint->mutable_required_protection()->
            set_hdcp(it->required_protection().hdcp());
      }
    }
    key->set_id(key_id);
  }

  virtual void AddEntitlementKey(
      const KeyId& key_id, bool set_level = false,
      KeyContainer::SecurityLevel level = KeyContainer::SW_SECURE_CRYPTO,
      bool set_hdcp = false,
      KeyContainer::OutputProtection::HDCP hdcp_value =
          KeyContainer::OutputProtection::HDCP_NONE,
      bool set_constraints = false,
      std::vector<VideoResolutionConstraint>* constraints = NULL) {
    AddContentKey(key_id, set_level, level, set_hdcp, hdcp_value,
                  set_constraints, constraints);
    license_.mutable_key(license_.key_size() - 1)
        ->set_type(KeyContainer::ENTITLEMENT);
  }

  virtual void AddOperatorSessionKey(
      const KeyId& key_id, bool set_perms = false,
      KeyFlag encrypt = kKeyFlagNull, KeyFlag decrypt = kKeyFlagNull,
      KeyFlag sign = kKeyFlagNull, KeyFlag verify = kKeyFlagNull) {
    KeyContainer* non_content_key = license_.add_key();
    non_content_key->set_type(KeyContainer::OPERATOR_SESSION);
    non_content_key->set_id(key_id);
    if (set_perms) {
      KeyContainer::OperatorSessionKeyPermissions* permissions =
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
  }

  virtual void AddSigningKey(const KeyId& key_id) {
    KeyContainer* key = license_.add_key();
    key->set_type(KeyContainer::SIGNING);
    key->set_id(key_id);
  }

  virtual void ExpectAllowedUsageContent(
      const CdmKeyAllowedUsage& key_usage, KeyFlag secure, KeyFlag clear,
      CdmKeySecurityLevel key_security_level) {
    EXPECT_EQ(key_usage.decrypt_to_secure_buffer, secure == kKeyFlagTrue);
    EXPECT_EQ(key_usage.decrypt_to_clear_buffer, clear == kKeyFlagTrue);
    EXPECT_EQ(key_usage.key_security_level_, key_security_level);
    EXPECT_FALSE(key_usage.generic_encrypt);
    EXPECT_FALSE(key_usage.generic_decrypt);
    EXPECT_FALSE(key_usage.generic_sign);
    EXPECT_FALSE(key_usage.generic_verify);
  }

  virtual void ExpectAllowedUsageOperator(
      const CdmKeyAllowedUsage& key_usage, KeyFlag encrypt, KeyFlag decrypt,
      KeyFlag sign, KeyFlag verify) {
    EXPECT_FALSE(key_usage.decrypt_to_secure_buffer);
    EXPECT_FALSE(key_usage.decrypt_to_clear_buffer);
    EXPECT_EQ(key_usage.generic_encrypt, encrypt == kKeyFlagTrue);
    EXPECT_EQ(key_usage.generic_decrypt, decrypt == kKeyFlagTrue);
    EXPECT_EQ(key_usage.generic_sign, sign == kKeyFlagTrue);
    EXPECT_EQ(key_usage.generic_verify, verify == kKeyFlagTrue);
  }

  virtual int NumContentKeys() {
    return content_key_count_;
  }

  virtual void StageContentKeys() {
    content_key_count_ = 0;
    AddContentKey(ck_sw_crypto, true, KeyContainer::SW_SECURE_CRYPTO);
    content_key_count_++;
    AddContentKey(ck_sw_decode, true, KeyContainer::SW_SECURE_DECODE);
    content_key_count_++;
    AddContentKey(ck_hw_crypto, true, KeyContainer::HW_SECURE_CRYPTO);
    content_key_count_++;
    AddContentKey(ck_hw_decode, true, KeyContainer::HW_SECURE_DECODE);
    content_key_count_++;
    AddContentKey(ck_hw_secure, true, KeyContainer::HW_SECURE_ALL);
    content_key_count_++;
    license_keys_->SetFromLicense(license_);
  }

  virtual void StageOperatorSessionKeys() {
    AddOperatorSessionKey(osk_decrypt, true,
                          kEncryptNull, kDecryptTrue, kSignNull, kVerifyNull);
    AddOperatorSessionKey(osk_encrypt, true,
                          kEncryptTrue, kDecryptNull, kSignNull, kVerifyNull);
    AddOperatorSessionKey(osk_sign, true,
                          kEncryptNull, kDecryptNull, kSignTrue, kVerifyNull);
    AddOperatorSessionKey(osk_verify, true,
                          kEncryptNull, kDecryptNull, kSignNull, kVerifyTrue);
    AddOperatorSessionKey(osk_encrypt_decrypt, true,
                          kEncryptTrue, kDecryptTrue, kSignNull, kVerifyNull);
    AddOperatorSessionKey(osk_sign_verify, true,
                          kEncryptNull, kDecryptNull, kSignTrue, kVerifyTrue);
    AddOperatorSessionKey(osk_all, true,
                          kEncryptTrue, kDecryptTrue, kSignTrue, kVerifyTrue);
    license_keys_->SetFromLicense(license_);
  }

  virtual void StageHdcpKeys() {
    content_key_count_ = 0;
    AddContentKey(ck_sw_crypto_NO_HDCP, true, KeyContainer::SW_SECURE_CRYPTO,
                  true, KeyContainer::OutputProtection::HDCP_NONE);
    content_key_count_++;
    AddContentKey(ck_hw_secure_NO_HDCP, true, KeyContainer::HW_SECURE_ALL, true,
                  KeyContainer::OutputProtection::HDCP_NONE);
    content_key_count_++;

    AddContentKey(ck_sw_crypto_HDCP_V2_1, true, KeyContainer::SW_SECURE_CRYPTO,
                  true, KeyContainer::OutputProtection::HDCP_V2_1);
    content_key_count_++;
    AddContentKey(ck_hw_secure_HDCP_V2_1, true, KeyContainer::HW_SECURE_ALL,
                  true, KeyContainer::OutputProtection::HDCP_V2_1);
    content_key_count_++;

    AddContentKey(ck_sw_crypto_HDCP_V2_2, true, KeyContainer::SW_SECURE_CRYPTO,
                  true, KeyContainer::OutputProtection::HDCP_V2_2);
    content_key_count_++;
    AddContentKey(ck_hw_secure_HDCP_V2_2, true, KeyContainer::HW_SECURE_ALL,
                  true, KeyContainer::OutputProtection::HDCP_V2_2);
    content_key_count_++;

    AddContentKey(ck_sw_crypto_HDCP_V2_3, true, KeyContainer::SW_SECURE_CRYPTO,
                  true, KeyContainer::OutputProtection::HDCP_V2_3);
    content_key_count_++;
    AddContentKey(ck_hw_secure_HDCP_V2_3, true, KeyContainer::HW_SECURE_ALL,
                  true, KeyContainer::OutputProtection::HDCP_V2_3);
    content_key_count_++;

    AddContentKey(ck_sw_crypto_HDCP_NO_OUTPUT, true,
                  KeyContainer::SW_SECURE_CRYPTO, true,
                  KeyContainer::OutputProtection::HDCP_NO_DIGITAL_OUTPUT);
    content_key_count_++;
    AddContentKey(ck_hw_secure_HDCP_NO_OUTPUT, true,
                  KeyContainer::HW_SECURE_ALL, true,
                  KeyContainer::OutputProtection::HDCP_NO_DIGITAL_OUTPUT);
    content_key_count_++;
    license_keys_->SetFromLicense(license_);
  }

  virtual void AddConstraint(
      std::vector<VideoResolutionConstraint>& constraints, uint32_t min_res,
      uint32_t max_res, bool set_hdcp = false,
      KeyContainer::OutputProtection::HDCP hdcp =
          KeyContainer::OutputProtection::HDCP_NONE) {
    VideoResolutionConstraint constraint;
    constraint.set_min_resolution_pixels(min_res);
    constraint.set_max_resolution_pixels(max_res);
    if (set_hdcp) {
      constraint.mutable_required_protection()->set_hdcp(hdcp);
    }
    constraints.push_back(constraint);
  }

  virtual void StageConstraintKeys() {
    content_key_count_ = 0;

    std::vector<VideoResolutionConstraint> constraints;

    AddConstraint(constraints, key_lo_res_min, key_lo_res_max);

    AddContentKey(ck_NO_HDCP_lo_res, true, KeyContainer::SW_SECURE_CRYPTO, true,
                  KeyContainer::OutputProtection::HDCP_NONE, true,
                  &constraints);
    content_key_count_++;

    constraints.clear();
    AddConstraint(constraints, key_hi_res_min, key_hi_res_max);

    AddContentKey(
        ck_HDCP_NO_OUTPUT_hi_res, true, KeyContainer::SW_SECURE_CRYPTO, true,
        KeyContainer::OutputProtection::HDCP_NO_DIGITAL_OUTPUT, true,
        &constraints);
    content_key_count_++;

    constraints.clear();
    AddConstraint(constraints, key_hi_res_min, key_hi_res_max);

    AddContentKey(ck_HDCP_V2_1_hi_res, true, KeyContainer::SW_SECURE_CRYPTO,
                  true, KeyContainer::OutputProtection::HDCP_V2_1, true,
                  &constraints);
    content_key_count_++;

    constraints.clear();
    AddConstraint(constraints, key_top_res_min, key_top_res_max);

    AddContentKey(ck_HDCP_V2_2_max_res, true, KeyContainer::SW_SECURE_CRYPTO,
                  true, KeyContainer::OutputProtection::HDCP_V2_2, true,
                  &constraints);
    content_key_count_++;

    constraints.clear();
    AddConstraint(constraints, key_top_res_min, key_top_res_max);

    AddContentKey(ck_HDCP_V2_3_max_res, true, KeyContainer::SW_SECURE_CRYPTO,
                  true, KeyContainer::OutputProtection::HDCP_V2_3, true,
                  &constraints);
    content_key_count_++;

    constraints.clear();
    AddConstraint(constraints, key_lo_res_min, key_lo_res_max);
    AddConstraint(constraints, key_hi_res_min, key_hi_res_max, true,
                  KeyContainer::OutputProtection::HDCP_NO_DIGITAL_OUTPUT);

    AddContentKey(ck_NO_HDCP_dual_res, true, KeyContainer::HW_SECURE_ALL, true,
                  KeyContainer::OutputProtection::HDCP_NONE, true,
                  &constraints);
    content_key_count_++;

    license_keys_->SetFromLicense(license_);
  }

  virtual void ExpectKeyStatusesEqual(CdmKeyStatusMap& key_status_map,
                                      CdmKeyStatus expected_status) {
    for (CdmKeyStatusMap::iterator it = key_status_map.begin();
         it != key_status_map.end(); ++it) {
      EXPECT_TRUE(it->second == expected_status);
    }
  }

  virtual void ExpectKeyStatusEqual(CdmKeyStatusMap& key_status_map,
                                    const KeyId& key_id,
                                    CdmKeyStatus expected_status) {
    for (CdmKeyStatusMap::iterator it = key_status_map.begin();
         it != key_status_map.end(); ++it) {
      if (key_id == it->first) {
        EXPECT_TRUE(it->second == expected_status);
      }
    }
  }


  size_t content_key_count_;
  std::unique_ptr<LicenseKeys> license_keys_;
  License license_;
};

TEST_F(LicenseKeysTest, Empty) {
  EXPECT_TRUE(license_keys_->Empty());
}

TEST_F(LicenseKeysTest, NotEmpty) {
  const KeyId c_key = "content_key";
  AddContentKey(c_key);
  license_keys_->SetFromLicense(license_);
  EXPECT_FALSE(license_keys_->Empty());
}

TEST_F(LicenseKeysTest, BadKeyId) {
  const KeyId c_key = "content_key";
  const KeyId os_key = "op_sess_key";
  const KeyId unk_key = "unknown_key";
  CdmKeyAllowedUsage allowed_usage;
  AddContentKey(c_key);
  AddOperatorSessionKey(os_key);
  license_keys_->SetFromLicense(license_);
  EXPECT_FALSE(license_keys_->IsContentKey(unk_key));
  EXPECT_FALSE(license_keys_->CanDecryptContent(unk_key));
  EXPECT_TRUE(license_keys_->MeetsConstraints(unk_key));
  EXPECT_TRUE(license_keys_->MeetsSecurityLevelConstraints(unk_key));
  EXPECT_FALSE(license_keys_->GetAllowedUsage(unk_key, &allowed_usage));
}

TEST_F(LicenseKeysTest, SigningKey) {
  const KeyId c_key = "content_key";
  const KeyId os_key = "op_sess_key";
  const KeyId sign_key = "signing_key";
  CdmKeyAllowedUsage allowed_usage;
  AddSigningKey(sign_key);
  AddContentKey(c_key);
  AddOperatorSessionKey(os_key);
  license_keys_->SetFromLicense(license_);
  EXPECT_FALSE(license_keys_->IsContentKey(sign_key));
  EXPECT_FALSE(license_keys_->CanDecryptContent(sign_key));
  EXPECT_TRUE(license_keys_->MeetsConstraints(sign_key));
  EXPECT_TRUE(license_keys_->MeetsSecurityLevelConstraints(sign_key));
  EXPECT_FALSE(license_keys_->GetAllowedUsage(sign_key, &allowed_usage));
}

TEST_F(LicenseKeysTest, ContentKey) {
  const KeyId c_key = "content_key";
  AddContentKey(c_key);
  EXPECT_FALSE(license_keys_->IsContentKey(c_key));

  license_keys_->SetFromLicense(license_);
  EXPECT_TRUE(license_keys_->IsContentKey(c_key));
}

TEST_F(LicenseKeysTest, EntitlementKey) {
  const KeyId e_key = "entitlement_key";
  const KeyId c_key = "content_key";
  AddEntitlementKey(e_key);
  EXPECT_FALSE(license_keys_->IsContentKey(e_key));

  license_keys_->SetFromLicense(license_);
  // TODO(juce, rfrias): For simplicity entitlement keys are indicated as
  // content keys. It doesn't break anything, but CanDecryptContent returns true
  // for and entitlement key id.
  EXPECT_TRUE(license_keys_->IsContentKey(e_key));

  std::vector<WidevinePsshData_EntitledKey> entitled_keys(1);
  entitled_keys[0].set_entitlement_key_id(e_key);
  entitled_keys[0].set_key_id(c_key);
  EXPECT_FALSE(license_keys_->IsContentKey(c_key));
  license_keys_->SetEntitledKeys(entitled_keys);
  EXPECT_TRUE(license_keys_->IsContentKey(c_key));
}

TEST_F(LicenseKeysTest, OperatorSessionKey) {
  const KeyId os_key = "op_sess_key";
  EXPECT_FALSE(license_keys_->IsContentKey(os_key));
  AddOperatorSessionKey(os_key);

  license_keys_->SetFromLicense(license_);
  EXPECT_FALSE(license_keys_->IsContentKey(os_key));
}

TEST_F(LicenseKeysTest, CanDecrypt) {
  const KeyId os_key = "op_sess_key";
  const KeyId c_key = "content_key";
  const KeyId e_key = "entitlement_key";
  EXPECT_FALSE(license_keys_->CanDecryptContent(c_key));
  EXPECT_FALSE(license_keys_->CanDecryptContent(os_key));
  EXPECT_FALSE(license_keys_->CanDecryptContent(e_key));
  AddOperatorSessionKey(os_key);
  AddContentKey(c_key);
  AddEntitlementKey(e_key);
  license_keys_->SetFromLicense(license_);
  EXPECT_FALSE(license_keys_->CanDecryptContent(c_key));
  EXPECT_FALSE(license_keys_->CanDecryptContent(os_key));
  EXPECT_FALSE(license_keys_->CanDecryptContent(e_key));
  bool new_usable_keys = false;
  bool any_change = false;
  any_change = license_keys_->ApplyStatusChange(kKeyStatusUsable,
                                               &new_usable_keys);
  EXPECT_TRUE(any_change);
  EXPECT_TRUE(new_usable_keys);
  EXPECT_TRUE(license_keys_->CanDecryptContent(c_key));
  EXPECT_FALSE(license_keys_->CanDecryptContent(os_key));

  any_change = license_keys_->ApplyStatusChange(kKeyStatusExpired,
                                               &new_usable_keys);
  EXPECT_TRUE(any_change);
  EXPECT_FALSE(new_usable_keys);
  EXPECT_FALSE(license_keys_->CanDecryptContent(c_key));
  EXPECT_FALSE(license_keys_->CanDecryptContent(os_key));
  EXPECT_FALSE(license_keys_->CanDecryptContent(e_key));
}

TEST_F(LicenseKeysTest, AllowedUsageNull) {
  const KeyId os_key = "op_sess_key";
  const KeyId c_key = "content_key";
  const KeyId sign_key = "signing_key";
  const KeyId e_key = "entitlement_key";
  AddOperatorSessionKey(os_key);
  AddContentKey(c_key);
  AddSigningKey(sign_key);
  AddEntitlementKey(e_key);
  license_keys_->SetFromLicense(license_);
  CdmKeyAllowedUsage usage_1;
  EXPECT_FALSE(license_keys_->GetAllowedUsage(sign_key, &usage_1));

  CdmKeyAllowedUsage usage_2;
  EXPECT_TRUE(license_keys_->GetAllowedUsage(c_key, &usage_2));
  ExpectAllowedUsageContent(usage_2, kContentClearTrue, kContentSecureTrue,
                            kKeySecurityLevelUnset);

  CdmKeyAllowedUsage usage_3;
  EXPECT_TRUE(license_keys_->GetAllowedUsage(os_key, &usage_3));
  ExpectAllowedUsageContent(usage_3, kContentClearFalse, kContentSecureFalse,
                            kKeySecurityLevelUnset);

  CdmKeyAllowedUsage usage_4;
  EXPECT_TRUE(license_keys_->GetAllowedUsage(os_key, &usage_4));
  ExpectAllowedUsageContent(usage_4, kContentClearFalse, kContentSecureFalse,
                            kKeySecurityLevelUnset);
}

TEST_F(LicenseKeysTest, AllowedUsageContent) {
  StageContentKeys();
  CdmKeyAllowedUsage u_sw_crypto;
  EXPECT_TRUE(license_keys_->GetAllowedUsage(ck_sw_crypto, &u_sw_crypto));
  ExpectAllowedUsageContent(u_sw_crypto, kContentSecureTrue, kContentClearTrue,
                            kSoftwareSecureCrypto);

  CdmKeyAllowedUsage u_sw_decode;
  EXPECT_TRUE(license_keys_->GetAllowedUsage(ck_sw_decode, &u_sw_decode));
  ExpectAllowedUsageContent(u_sw_decode, kContentSecureTrue, kContentClearTrue,
                            kSoftwareSecureDecode);

  CdmKeyAllowedUsage u_hw_crypto;
  EXPECT_TRUE(license_keys_->GetAllowedUsage(ck_hw_crypto, &u_hw_crypto));
  ExpectAllowedUsageContent(u_hw_crypto, kContentSecureTrue, kContentClearTrue,
                            kHardwareSecureCrypto);

  CdmKeyAllowedUsage u_hw_decode;
  EXPECT_TRUE(license_keys_->GetAllowedUsage(ck_hw_decode, &u_hw_decode));
  ExpectAllowedUsageContent(u_hw_decode, kContentSecureTrue,
                            kContentClearFalse, kHardwareSecureDecode);

  CdmKeyAllowedUsage u_hw_secure;
  EXPECT_TRUE(license_keys_->GetAllowedUsage(ck_hw_secure, &u_hw_secure));
  ExpectAllowedUsageContent(u_hw_secure, kContentSecureTrue,
                            kContentClearFalse, kHardwareSecureAll);
}

TEST_F(LicenseKeysTest, AllowedUsageOperatorSession) {
  StageOperatorSessionKeys();
  CdmKeyAllowedUsage u_encrypt;
  EXPECT_TRUE(license_keys_->GetAllowedUsage(osk_encrypt, &u_encrypt));
  ExpectAllowedUsageOperator(u_encrypt, kEncryptTrue, kDecryptFalse, kSignFalse,
                             kVerifyFalse);

  CdmKeyAllowedUsage u_decrypt;
  EXPECT_TRUE(license_keys_->GetAllowedUsage(osk_decrypt, &u_decrypt));
  ExpectAllowedUsageOperator(u_decrypt, kEncryptFalse, kDecryptTrue, kSignFalse,
                             kVerifyFalse);

  CdmKeyAllowedUsage u_sign;
  EXPECT_TRUE(license_keys_->GetAllowedUsage(osk_sign, &u_sign));
  ExpectAllowedUsageOperator(u_sign, kEncryptFalse, kDecryptFalse, kSignTrue,
                             kVerifyFalse);

  CdmKeyAllowedUsage u_verify;
  EXPECT_TRUE(license_keys_->GetAllowedUsage(osk_verify, &u_verify));
  ExpectAllowedUsageOperator(u_verify, kEncryptFalse, kDecryptFalse, kSignFalse,
                             kVerifyTrue);

  CdmKeyAllowedUsage u_encrypt_decrypt;
  EXPECT_TRUE(license_keys_->GetAllowedUsage(osk_encrypt_decrypt,
                                            &u_encrypt_decrypt));
  ExpectAllowedUsageOperator(u_encrypt_decrypt, kEncryptTrue, kDecryptTrue,
                             kSignFalse, kVerifyFalse);

  CdmKeyAllowedUsage u_sign_verify;
  EXPECT_TRUE(license_keys_->GetAllowedUsage(osk_sign_verify, &u_sign_verify));
  ExpectAllowedUsageOperator(u_sign_verify, kEncryptFalse, kDecryptFalse,
                             kSignTrue, kVerifyTrue);

  CdmKeyAllowedUsage u_all;
  EXPECT_TRUE(license_keys_->GetAllowedUsage(osk_all, &u_all));
  ExpectAllowedUsageOperator(u_all, kEncryptTrue, kDecryptTrue, kSignTrue,
                             kVerifyTrue);
}

TEST_F(LicenseKeysTest, ExtractKeyStatuses) {
  CdmKeyStatusMap key_status_map;
  StageOperatorSessionKeys();
  license_keys_->ExtractKeyStatuses(&key_status_map);
  EXPECT_EQ(0u, key_status_map.size());
  StageContentKeys();
  license_keys_->ExtractKeyStatuses(&key_status_map);
  EXPECT_EQ(content_key_count_, key_status_map.size());
  ExpectKeyStatusesEqual(key_status_map, kKeyStatusInternalError);
}

TEST_F(LicenseKeysTest, KeyStatusChanges) {
  bool new_usable_keys = false;
  bool any_change = false;
  CdmKeyStatusMap key_status_map;
  StageOperatorSessionKeys();
  StageContentKeys();

  license_keys_->ExtractKeyStatuses(&key_status_map);
  EXPECT_EQ(content_key_count_, key_status_map.size());
  ExpectKeyStatusesEqual(key_status_map, kKeyStatusInternalError);

  // change to pending
  any_change = license_keys_->ApplyStatusChange(kKeyStatusPending,
                                               &new_usable_keys);
  EXPECT_TRUE(any_change);
  EXPECT_FALSE(new_usable_keys);
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_sw_crypto));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_hw_secure));

  license_keys_->ExtractKeyStatuses(&key_status_map);
  EXPECT_EQ(content_key_count_, key_status_map.size());
  ExpectKeyStatusesEqual(key_status_map, kKeyStatusPending);

  // change to usable in future
  any_change = license_keys_->ApplyStatusChange(kKeyStatusUsableInFuture,
                                               &new_usable_keys);
  EXPECT_TRUE(any_change);
  EXPECT_FALSE(new_usable_keys);
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_sw_crypto));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_hw_secure));

  license_keys_->ExtractKeyStatuses(&key_status_map);
  EXPECT_EQ(content_key_count_, key_status_map.size());
  ExpectKeyStatusesEqual(key_status_map, kKeyStatusUsableInFuture);

  // change to usable
  any_change = license_keys_->ApplyStatusChange(kKeyStatusUsable,
                                               &new_usable_keys);
  EXPECT_TRUE(any_change);
  EXPECT_TRUE(new_usable_keys);
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_sw_crypto));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_hw_secure));

  license_keys_->ExtractKeyStatuses(&key_status_map);
  EXPECT_EQ(content_key_count_, key_status_map.size());
  ExpectKeyStatusesEqual(key_status_map, kKeyStatusUsable);

  // change to usable (again)
  any_change = license_keys_->ApplyStatusChange(kKeyStatusUsable,
                                               &new_usable_keys);
  EXPECT_FALSE(any_change);
  EXPECT_FALSE(new_usable_keys);
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_sw_crypto));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_hw_secure));

  license_keys_->ExtractKeyStatuses(&key_status_map);
  EXPECT_EQ(content_key_count_, key_status_map.size());
  ExpectKeyStatusesEqual(key_status_map, kKeyStatusUsable);

  // change to expired
  any_change = license_keys_->ApplyStatusChange(kKeyStatusExpired,
                                               &new_usable_keys);
  EXPECT_TRUE(any_change);
  EXPECT_FALSE(new_usable_keys);
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_sw_crypto));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_hw_secure));

  license_keys_->ExtractKeyStatuses(&key_status_map);
  EXPECT_EQ(content_key_count_, key_status_map.size());
  ExpectKeyStatusesEqual(key_status_map, kKeyStatusExpired);
}

TEST_F(LicenseKeysTest, HdcpChanges) {
  bool new_usable_keys = false;
  bool any_change = false;
  CdmKeyStatusMap key_status_map;
  StageHdcpKeys();

  any_change = license_keys_->ApplyStatusChange(kKeyStatusUsable,
                                               &new_usable_keys);
  EXPECT_TRUE(any_change);
  EXPECT_TRUE(new_usable_keys);
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_sw_crypto_NO_HDCP));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_hw_secure_NO_HDCP));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_sw_crypto_HDCP_V2_1));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_hw_secure_HDCP_V2_1));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_sw_crypto_HDCP_V2_2));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_hw_secure_HDCP_V2_2));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_sw_crypto_HDCP_V2_3));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_hw_secure_HDCP_V2_3));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_sw_crypto_HDCP_NO_OUTPUT));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_hw_secure_HDCP_NO_OUTPUT));

  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_sw_crypto_NO_HDCP));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_hw_secure_NO_HDCP));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_sw_crypto_HDCP_V2_1));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_hw_secure_HDCP_V2_1));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_sw_crypto_HDCP_V2_2));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_hw_secure_HDCP_V2_2));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_sw_crypto_HDCP_V2_3));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_hw_secure_HDCP_V2_3));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_sw_crypto_HDCP_NO_OUTPUT));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_hw_secure_HDCP_NO_OUTPUT));

  license_keys_->ApplyConstraints(100, HDCP_NONE);
  any_change = license_keys_->ApplyStatusChange(kKeyStatusUsable,
                                               &new_usable_keys);

  EXPECT_TRUE(any_change);
  EXPECT_FALSE(new_usable_keys);
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_sw_crypto_NO_HDCP));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_hw_secure_NO_HDCP));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_sw_crypto_HDCP_V2_1));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_hw_secure_HDCP_V2_1));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_sw_crypto_HDCP_V2_2));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_hw_secure_HDCP_V2_2));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_sw_crypto_HDCP_V2_3));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_hw_secure_HDCP_V2_3));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_sw_crypto_HDCP_NO_OUTPUT));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_hw_secure_HDCP_NO_OUTPUT));

  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_sw_crypto_NO_HDCP));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_hw_secure_NO_HDCP));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_sw_crypto_HDCP_V2_1));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_hw_secure_HDCP_V2_1));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_sw_crypto_HDCP_V2_2));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_hw_secure_HDCP_V2_2));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_sw_crypto_HDCP_V2_3));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_hw_secure_HDCP_V2_3));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_sw_crypto_HDCP_NO_OUTPUT));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_hw_secure_HDCP_NO_OUTPUT));

  license_keys_->ExtractKeyStatuses(&key_status_map);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_NO_HDCP, kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_NO_HDCP, kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_1,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_1,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_2,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_2,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_3,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_3,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_NO_OUTPUT,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_NO_OUTPUT,
                       kKeyStatusOutputNotAllowed);

  license_keys_->ApplyConstraints(100, HDCP_V1);
  any_change = license_keys_->ApplyStatusChange(kKeyStatusUsable,
                                               &new_usable_keys);

  EXPECT_FALSE(any_change);
  EXPECT_FALSE(new_usable_keys);
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_sw_crypto_NO_HDCP));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_hw_secure_NO_HDCP));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_sw_crypto_HDCP_V2_1));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_hw_secure_HDCP_V2_1));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_sw_crypto_HDCP_V2_2));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_hw_secure_HDCP_V2_2));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_sw_crypto_HDCP_V2_3));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_hw_secure_HDCP_V2_3));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_sw_crypto_HDCP_NO_OUTPUT));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_hw_secure_HDCP_NO_OUTPUT));

  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_sw_crypto_NO_HDCP));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_hw_secure_NO_HDCP));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_sw_crypto_HDCP_V2_1));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_hw_secure_HDCP_V2_1));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_sw_crypto_HDCP_V2_2));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_hw_secure_HDCP_V2_2));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_sw_crypto_HDCP_V2_3));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_hw_secure_HDCP_V2_3));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_sw_crypto_HDCP_NO_OUTPUT));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_hw_secure_HDCP_NO_OUTPUT));

  license_keys_->ExtractKeyStatuses(&key_status_map);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_NO_HDCP, kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_NO_HDCP, kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_1,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_1,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_2,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_2,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_3,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_3,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_NO_OUTPUT,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_NO_OUTPUT,
                       kKeyStatusOutputNotAllowed);

  license_keys_->ApplyConstraints(100, HDCP_V2_1);
  any_change = license_keys_->ApplyStatusChange(kKeyStatusUsable,
                                               &new_usable_keys);

  EXPECT_TRUE(any_change);
  EXPECT_TRUE(new_usable_keys);
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_sw_crypto_NO_HDCP));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_hw_secure_NO_HDCP));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_sw_crypto_HDCP_V2_1));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_hw_secure_HDCP_V2_1));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_sw_crypto_HDCP_V2_2));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_hw_secure_HDCP_V2_2));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_sw_crypto_HDCP_V2_3));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_hw_secure_HDCP_V2_3));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_sw_crypto_HDCP_NO_OUTPUT));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_hw_secure_HDCP_NO_OUTPUT));

  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_sw_crypto_NO_HDCP));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_hw_secure_NO_HDCP));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_sw_crypto_HDCP_V2_1));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_hw_secure_HDCP_V2_1));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_sw_crypto_HDCP_V2_2));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_hw_secure_HDCP_V2_2));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_sw_crypto_HDCP_V2_3));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_hw_secure_HDCP_V2_3));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_sw_crypto_HDCP_NO_OUTPUT));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_hw_secure_HDCP_NO_OUTPUT));

  license_keys_->ExtractKeyStatuses(&key_status_map);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_NO_HDCP, kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_NO_HDCP, kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_1,
                       kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_1,
                       kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_2,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_2,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_3,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_3,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_NO_OUTPUT,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_NO_OUTPUT,
                       kKeyStatusOutputNotAllowed);

  license_keys_->ApplyConstraints(100, HDCP_V2_2);
  any_change = license_keys_->ApplyStatusChange(kKeyStatusUsable,
                                               &new_usable_keys);

  EXPECT_TRUE(any_change);
  EXPECT_TRUE(new_usable_keys);
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_sw_crypto_NO_HDCP));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_hw_secure_NO_HDCP));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_sw_crypto_HDCP_V2_1));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_hw_secure_HDCP_V2_1));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_sw_crypto_HDCP_V2_2));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_hw_secure_HDCP_V2_2));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_sw_crypto_HDCP_V2_3));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_hw_secure_HDCP_V2_3));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_sw_crypto_HDCP_NO_OUTPUT));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_hw_secure_HDCP_NO_OUTPUT));

  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_sw_crypto_NO_HDCP));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_hw_secure_NO_HDCP));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_sw_crypto_HDCP_V2_1));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_hw_secure_HDCP_V2_1));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_sw_crypto_HDCP_V2_2));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_hw_secure_HDCP_V2_2));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_sw_crypto_HDCP_V2_3));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_hw_secure_HDCP_V2_3));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_sw_crypto_HDCP_NO_OUTPUT));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_hw_secure_HDCP_NO_OUTPUT));

  license_keys_->ExtractKeyStatuses(&key_status_map);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_NO_HDCP, kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_NO_HDCP, kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_1,
                       kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_1,
                       kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_2,
                       kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_2,
                       kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_3,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_3,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_NO_OUTPUT,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_NO_OUTPUT,
                       kKeyStatusOutputNotAllowed);

  license_keys_->ApplyConstraints(100, HDCP_V2_3);
  any_change = license_keys_->ApplyStatusChange(kKeyStatusUsable,
                                               &new_usable_keys);

  EXPECT_TRUE(any_change);
  EXPECT_TRUE(new_usable_keys);
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_sw_crypto_NO_HDCP));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_hw_secure_NO_HDCP));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_sw_crypto_HDCP_V2_1));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_hw_secure_HDCP_V2_1));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_sw_crypto_HDCP_V2_2));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_hw_secure_HDCP_V2_2));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_sw_crypto_HDCP_V2_3));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_hw_secure_HDCP_V2_3));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_sw_crypto_HDCP_NO_OUTPUT));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_hw_secure_HDCP_NO_OUTPUT));

  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_sw_crypto_NO_HDCP));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_hw_secure_NO_HDCP));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_sw_crypto_HDCP_V2_1));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_hw_secure_HDCP_V2_1));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_sw_crypto_HDCP_V2_2));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_hw_secure_HDCP_V2_2));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_sw_crypto_HDCP_V2_3));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_hw_secure_HDCP_V2_3));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_sw_crypto_HDCP_NO_OUTPUT));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_hw_secure_HDCP_NO_OUTPUT));

  license_keys_->ExtractKeyStatuses(&key_status_map);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_NO_HDCP, kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_NO_HDCP, kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_1,
                       kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_1,
                       kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_2,
                       kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_2,
                       kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_3,
                       kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_3,
                       kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_NO_OUTPUT,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_NO_OUTPUT,
                       kKeyStatusOutputNotAllowed);

  license_keys_->ApplyConstraints(100, HDCP_NO_DIGITAL_OUTPUT);
  any_change = license_keys_->ApplyStatusChange(kKeyStatusUsable,
                                               &new_usable_keys);

  EXPECT_TRUE(any_change);
  EXPECT_TRUE(new_usable_keys);
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_sw_crypto_NO_HDCP));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_hw_secure_NO_HDCP));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_sw_crypto_HDCP_V2_1));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_hw_secure_HDCP_V2_1));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_sw_crypto_HDCP_NO_OUTPUT));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_hw_secure_HDCP_NO_OUTPUT));

  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_sw_crypto_NO_HDCP));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_hw_secure_NO_HDCP));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_sw_crypto_HDCP_V2_1));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_hw_secure_HDCP_V2_1));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_sw_crypto_HDCP_NO_OUTPUT));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_hw_secure_HDCP_NO_OUTPUT));

  license_keys_->ExtractKeyStatuses(&key_status_map);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_NO_HDCP, kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_NO_HDCP, kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_1,
                       kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_1,
                       kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_2,
                       kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_2,
                       kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_3,
                       kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_3,
                       kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_NO_OUTPUT,
                       kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_NO_OUTPUT,
                       kKeyStatusUsable);

  license_keys_->ApplyConstraints(100, HDCP_NONE);
  any_change = license_keys_->ApplyStatusChange(kKeyStatusUsable,
                                               &new_usable_keys);

  EXPECT_TRUE(any_change);
  EXPECT_FALSE(new_usable_keys);
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_sw_crypto_NO_HDCP));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_hw_secure_NO_HDCP));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_sw_crypto_HDCP_V2_1));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_hw_secure_HDCP_V2_1));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_sw_crypto_HDCP_V2_2));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_hw_secure_HDCP_V2_2));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_sw_crypto_HDCP_V2_3));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_hw_secure_HDCP_V2_3));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_sw_crypto_HDCP_NO_OUTPUT));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_hw_secure_HDCP_NO_OUTPUT));

  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_sw_crypto_NO_HDCP));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_hw_secure_NO_HDCP));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_sw_crypto_HDCP_V2_1));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_hw_secure_HDCP_V2_1));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_sw_crypto_HDCP_V2_2));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_hw_secure_HDCP_V2_2));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_sw_crypto_HDCP_V2_3));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_hw_secure_HDCP_V2_3));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_sw_crypto_HDCP_NO_OUTPUT));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_hw_secure_HDCP_NO_OUTPUT));

  license_keys_->ExtractKeyStatuses(&key_status_map);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_NO_HDCP, kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_NO_HDCP, kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_1,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_1,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_2,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_2,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_3,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_3,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_NO_OUTPUT,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_NO_OUTPUT,
                       kKeyStatusOutputNotAllowed);
}

TEST_F(LicenseKeysTest, ConstraintChanges) {
  bool new_usable_keys = false;
  bool any_change = false;
  CdmKeyStatusMap key_status_map;
  StageConstraintKeys();

  // No constraints set by device
  any_change = license_keys_->ApplyStatusChange(kKeyStatusUsable,
                                               &new_usable_keys);
  EXPECT_TRUE(any_change);
  EXPECT_TRUE(new_usable_keys);
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_NO_HDCP_lo_res));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_HDCP_NO_OUTPUT_hi_res));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_HDCP_V2_1_hi_res));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_HDCP_V2_2_max_res));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_HDCP_V2_3_max_res));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_NO_HDCP_dual_res));

  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_NO_HDCP_lo_res));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_HDCP_NO_OUTPUT_hi_res));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_HDCP_V2_1_hi_res));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_HDCP_V2_2_max_res));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_HDCP_V2_3_max_res));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_NO_HDCP_dual_res));

  // Low-res device, no HDCP support
  license_keys_->ApplyConstraints(dev_lo_res, HDCP_NONE);
  any_change = license_keys_->ApplyStatusChange(kKeyStatusUsable,
                                               &new_usable_keys);

  EXPECT_TRUE(any_change);
  EXPECT_FALSE(new_usable_keys);
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_NO_HDCP_lo_res));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_HDCP_NO_OUTPUT_hi_res));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_HDCP_V2_1_hi_res));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_HDCP_V2_2_max_res));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_HDCP_V2_3_max_res));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_NO_HDCP_dual_res));

  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_NO_HDCP_lo_res));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_HDCP_NO_OUTPUT_hi_res));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_HDCP_V2_1_hi_res));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_HDCP_V2_2_max_res));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_HDCP_V2_3_max_res));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_NO_HDCP_dual_res));

  license_keys_->ExtractKeyStatuses(&key_status_map);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_NO_HDCP, kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_NO_HDCP, kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_1,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_1,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_2,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_2,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_3,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_3,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_NO_OUTPUT,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_NO_OUTPUT,
                       kKeyStatusOutputNotAllowed);

  // Hi-res device, HDCP_V1 support
  license_keys_->ApplyConstraints(dev_hi_res, HDCP_V1);
  any_change = license_keys_->ApplyStatusChange(kKeyStatusUsable,
                                               &new_usable_keys);

  EXPECT_TRUE(any_change);
  EXPECT_TRUE(new_usable_keys);
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_NO_HDCP_lo_res));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_HDCP_NO_OUTPUT_hi_res));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_HDCP_V2_1_hi_res));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_HDCP_V2_2_max_res));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_HDCP_V2_3_max_res));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_NO_HDCP_dual_res));

  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_NO_HDCP_lo_res));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_HDCP_NO_OUTPUT_hi_res));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_HDCP_V2_1_hi_res));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_HDCP_V2_2_max_res));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_HDCP_V2_3_max_res));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_NO_HDCP_dual_res));

  license_keys_->ExtractKeyStatuses(&key_status_map);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_NO_HDCP, kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_NO_HDCP, kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_1,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_1,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_2,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_2,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_3,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_3,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_NO_OUTPUT,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_NO_OUTPUT,
                       kKeyStatusOutputNotAllowed);

  // Lo-res device, HDCP V2.2 support
  license_keys_->ApplyConstraints(dev_lo_res, HDCP_V2_2);
  any_change = license_keys_->ApplyStatusChange(kKeyStatusUsable,
                                               &new_usable_keys);

  EXPECT_TRUE(any_change);
  EXPECT_TRUE(new_usable_keys);
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_NO_HDCP_lo_res));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_HDCP_NO_OUTPUT_hi_res));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_HDCP_V2_1_hi_res));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_HDCP_V2_2_max_res));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_HDCP_V2_3_max_res));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_NO_HDCP_dual_res));

  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_NO_HDCP_lo_res));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_HDCP_NO_OUTPUT_hi_res));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_HDCP_V2_1_hi_res));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_HDCP_V2_2_max_res));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_HDCP_V2_3_max_res));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_NO_HDCP_dual_res));

  license_keys_->ExtractKeyStatuses(&key_status_map);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_1,
                       kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_1,
                       kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_2,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_2,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_3,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_3,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_NO_OUTPUT,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_NO_OUTPUT,
                       kKeyStatusOutputNotAllowed);

  // Hi-res device, Maximal HDCP support
  license_keys_->ApplyConstraints(dev_hi_res, HDCP_NO_DIGITAL_OUTPUT);
  any_change = license_keys_->ApplyStatusChange(kKeyStatusUsable,
                                               &new_usable_keys);

  EXPECT_TRUE(any_change);
  EXPECT_TRUE(new_usable_keys);
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_NO_HDCP_lo_res));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_HDCP_NO_OUTPUT_hi_res));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_HDCP_V2_1_hi_res));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_HDCP_V2_2_max_res));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_HDCP_V2_3_max_res));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_NO_HDCP_dual_res));

  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_NO_HDCP_lo_res));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_HDCP_NO_OUTPUT_hi_res));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_HDCP_V2_1_hi_res));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_HDCP_V2_2_max_res));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_HDCP_V2_3_max_res));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_NO_HDCP_dual_res));

  license_keys_->ExtractKeyStatuses(&key_status_map);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_1,
                       kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_1,
                       kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_2,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_2,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_3,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_3,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_NO_OUTPUT,
                       kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_NO_OUTPUT,
                       kKeyStatusUsable);

  // Lo-res device, no HDCP support
  license_keys_->ApplyConstraints(dev_lo_res, HDCP_NONE);
  any_change = license_keys_->ApplyStatusChange(kKeyStatusUsable,
                                               &new_usable_keys);

  EXPECT_TRUE(any_change);
  EXPECT_TRUE(new_usable_keys);
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_NO_HDCP_lo_res));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_HDCP_NO_OUTPUT_hi_res));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_HDCP_V2_1_hi_res));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_HDCP_V2_2_max_res));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_HDCP_V2_3_max_res));
  EXPECT_TRUE(license_keys_->CanDecryptContent(ck_NO_HDCP_dual_res));

  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_NO_HDCP_lo_res));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_HDCP_NO_OUTPUT_hi_res));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_HDCP_V2_1_hi_res));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_HDCP_V2_2_max_res));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_HDCP_V2_3_max_res));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_NO_HDCP_dual_res));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_NO_HDCP_dual_res));
  EXPECT_TRUE(license_keys_->MeetsConstraints(ck_NO_HDCP_dual_res));

  license_keys_->ExtractKeyStatuses(&key_status_map);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_NO_HDCP,
                       kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_NO_HDCP,
                       kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_1,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_1,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_2,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_2,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_3,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_3,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_NO_OUTPUT,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_NO_OUTPUT,
                       kKeyStatusOutputNotAllowed);

  // Too-high-res -- all keys rejected
  license_keys_->ApplyConstraints(dev_top_res, HDCP_NONE);
  any_change = license_keys_->ApplyStatusChange(kKeyStatusUsable,
                                               &new_usable_keys);

  EXPECT_TRUE(any_change);
  EXPECT_FALSE(new_usable_keys);
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_NO_HDCP_lo_res));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_HDCP_NO_OUTPUT_hi_res));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_HDCP_V2_1_hi_res));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_HDCP_V2_2_max_res));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_HDCP_V2_3_max_res));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_NO_HDCP_dual_res));

  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_NO_HDCP_lo_res));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_HDCP_NO_OUTPUT_hi_res));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_HDCP_V2_1_hi_res));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_HDCP_V2_2_max_res));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_HDCP_V2_3_max_res));
  EXPECT_FALSE(license_keys_->MeetsConstraints(ck_NO_HDCP_dual_res));

  license_keys_->ExtractKeyStatuses(&key_status_map);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_NO_HDCP,
                       kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_NO_HDCP,
                       kKeyStatusUsable);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_1,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_1,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_2,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_2,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_V2_3,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_V2_3,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_sw_crypto_HDCP_NO_OUTPUT,
                       kKeyStatusOutputNotAllowed);
  ExpectKeyStatusEqual(key_status_map, ck_hw_secure_HDCP_NO_OUTPUT,
                       kKeyStatusOutputNotAllowed);
}

struct KeySecurityLevelParams {
  CdmSecurityLevel security_level;
  bool expect_usable_keys;
  bool expect_can_use_sw_crypto_key;
  bool expect_can_use_sw_decode_key;
  bool expect_can_use_hw_crypto_key;
  bool expect_can_use_hw_decode_key;
  bool expect_can_use_hw_secure_key;
};

KeySecurityLevelParams key_security_level_test_vectors[] = {
  { kSecurityLevelUninitialized, false, false, false, false, false, false},
  { kSecurityLevelL1, true, true, true, true, true, true},
  { kSecurityLevelL2, true, true, true, true, false, false},
  { kSecurityLevelL3, true, true, true, false, false, false},
  { kSecurityLevelUnknown, false, false, false, false, false, false},
};

class LicenseKeysSecurityLevelConstraintsTest
    : public LicenseKeysTest,
      public ::testing::WithParamInterface<KeySecurityLevelParams*> {};

TEST_P(LicenseKeysSecurityLevelConstraintsTest, KeyStatusChange) {
  bool new_usable_keys = false;
  bool any_change = false;
  CdmKeyStatusMap key_status_map;

  KeySecurityLevelParams* config = GetParam();
  license_keys_->SetSecurityLevelForTest(config->security_level);
  StageContentKeys();

  license_keys_->ExtractKeyStatuses(&key_status_map);
  EXPECT_EQ(content_key_count_, key_status_map.size());
  ExpectKeyStatusesEqual(key_status_map, kKeyStatusInternalError);

  // change to pending
  any_change = license_keys_->ApplyStatusChange(kKeyStatusPending,
                                                &new_usable_keys);
  EXPECT_TRUE(any_change);
  EXPECT_FALSE(new_usable_keys);
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_sw_crypto));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_sw_decode));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_hw_crypto));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_hw_decode));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_hw_secure));

  license_keys_->ExtractKeyStatuses(&key_status_map);
  EXPECT_EQ(content_key_count_, key_status_map.size());
  ExpectKeyStatusesEqual(key_status_map, kKeyStatusPending);

  // change to usable in future
  any_change = license_keys_->ApplyStatusChange(kKeyStatusUsableInFuture,
                                                &new_usable_keys);
  EXPECT_TRUE(any_change);
  EXPECT_FALSE(new_usable_keys);
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_sw_crypto));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_sw_decode));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_hw_crypto));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_hw_decode));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_hw_secure));

  license_keys_->ExtractKeyStatuses(&key_status_map);
  EXPECT_EQ(content_key_count_, key_status_map.size());
  ExpectKeyStatusesEqual(key_status_map, kKeyStatusUsableInFuture);

  // change to usable
  any_change = license_keys_->ApplyStatusChange(kKeyStatusUsable,
                                                &new_usable_keys);
  EXPECT_TRUE(any_change);
  EXPECT_EQ(config->expect_usable_keys, new_usable_keys);
  EXPECT_EQ(config->expect_can_use_sw_crypto_key,
            license_keys_->CanDecryptContent(ck_sw_crypto));
  EXPECT_EQ(config->expect_can_use_sw_decode_key,
            license_keys_->CanDecryptContent(ck_sw_decode));
  EXPECT_EQ(config->expect_can_use_hw_crypto_key,
            license_keys_->CanDecryptContent(ck_hw_crypto));
  EXPECT_EQ(config->expect_can_use_hw_decode_key,
            license_keys_->CanDecryptContent(ck_hw_decode));
  EXPECT_EQ(config->expect_can_use_hw_secure_key,
            license_keys_->CanDecryptContent(ck_hw_secure));

  license_keys_->ExtractKeyStatuses(&key_status_map);
  EXPECT_EQ(content_key_count_, key_status_map.size());

  EXPECT_EQ(config->expect_can_use_sw_crypto_key
                ? kKeyStatusUsable : kKeyStatusOutputNotAllowed,
            key_status_map[ck_sw_crypto]);
  EXPECT_EQ(config->expect_can_use_sw_decode_key
                ? kKeyStatusUsable : kKeyStatusOutputNotAllowed,
            key_status_map[ck_sw_decode]);
  EXPECT_EQ(config->expect_can_use_hw_crypto_key
                ? kKeyStatusUsable : kKeyStatusOutputNotAllowed,
            key_status_map[ck_hw_crypto]);
  EXPECT_EQ(config->expect_can_use_hw_decode_key
                ? kKeyStatusUsable : kKeyStatusOutputNotAllowed,
            key_status_map[ck_hw_decode]);
  EXPECT_EQ(config->expect_can_use_hw_secure_key
                ? kKeyStatusUsable : kKeyStatusOutputNotAllowed,
            key_status_map[ck_hw_secure]);

  // change to usable (again)
  any_change = license_keys_->ApplyStatusChange(kKeyStatusUsable,
                                                &new_usable_keys);
  EXPECT_FALSE(any_change);
  EXPECT_FALSE(new_usable_keys);
  EXPECT_EQ(config->expect_can_use_sw_crypto_key,
            license_keys_->CanDecryptContent(ck_sw_crypto));
  EXPECT_EQ(config->expect_can_use_sw_decode_key,
            license_keys_->CanDecryptContent(ck_sw_decode));
  EXPECT_EQ(config->expect_can_use_hw_crypto_key,
            license_keys_->CanDecryptContent(ck_hw_crypto));
  EXPECT_EQ(config->expect_can_use_hw_decode_key,
            license_keys_->CanDecryptContent(ck_hw_decode));
  EXPECT_EQ(config->expect_can_use_hw_secure_key,
            license_keys_->CanDecryptContent(ck_hw_secure));

  license_keys_->ExtractKeyStatuses(&key_status_map);
  EXPECT_EQ(content_key_count_, key_status_map.size());
  EXPECT_EQ(config->expect_can_use_sw_crypto_key
                ? kKeyStatusUsable : kKeyStatusOutputNotAllowed,
            key_status_map[ck_sw_crypto]);
  EXPECT_EQ(config->expect_can_use_sw_decode_key
                ? kKeyStatusUsable : kKeyStatusOutputNotAllowed,
            key_status_map[ck_sw_decode]);
  EXPECT_EQ(config->expect_can_use_hw_crypto_key
                ? kKeyStatusUsable : kKeyStatusOutputNotAllowed,
            key_status_map[ck_hw_crypto]);
  EXPECT_EQ(config->expect_can_use_hw_decode_key
                ? kKeyStatusUsable : kKeyStatusOutputNotAllowed,
            key_status_map[ck_hw_decode]);
  EXPECT_EQ(config->expect_can_use_hw_secure_key
                ? kKeyStatusUsable : kKeyStatusOutputNotAllowed,
            key_status_map[ck_hw_secure]);

  // change to expired
  any_change = license_keys_->ApplyStatusChange(kKeyStatusExpired,
                                                &new_usable_keys);
  EXPECT_TRUE(any_change);
  EXPECT_FALSE(new_usable_keys);
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_sw_crypto));
  EXPECT_FALSE(license_keys_->CanDecryptContent(ck_hw_secure));

  license_keys_->ExtractKeyStatuses(&key_status_map);
  EXPECT_EQ(content_key_count_, key_status_map.size());
  ExpectKeyStatusesEqual(key_status_map, kKeyStatusExpired);
}

INSTANTIATE_TEST_CASE_P(KeyStatusChange,
                        LicenseKeysSecurityLevelConstraintsTest,
                        ::testing::Range(&key_security_level_test_vectors[0],
                                         &key_security_level_test_vectors[5]));

}  // namespace wvcdm
