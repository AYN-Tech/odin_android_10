// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#include "license_key_status.h"

#include <string>

#include "log.h"
#include "wv_cdm_constants.h"

namespace {
// License protocol aliases
typedef ::video_widevine::License::KeyContainer KeyContainer;
typedef KeyContainer::OutputProtection OutputProtection;
typedef KeyContainer::VideoResolutionConstraint VideoResolutionConstraint;
typedef ::google::protobuf::RepeatedPtrField<VideoResolutionConstraint>
    ConstraintList;

// Map the HDCP protection associated with a key in the license to
// an equivalent OEMCrypto HDCP protection level
wvcdm::CryptoSession::HdcpCapability ProtobufHdcpToOemCryptoHdcp(
    const OutputProtection::HDCP& input) {
  switch (input) {
    case OutputProtection::HDCP_NONE:
      return HDCP_NONE;
    case OutputProtection::HDCP_V1:
      return HDCP_V1;
    case OutputProtection::HDCP_V2:
      return HDCP_V2;
    case OutputProtection::HDCP_V2_1:
      return HDCP_V2_1;
    case OutputProtection::HDCP_V2_2:
      return HDCP_V2_2;
    case OutputProtection::HDCP_V2_3:
      return HDCP_V2_3;
    case OutputProtection::HDCP_NO_DIGITAL_OUTPUT:
      return HDCP_NO_DIGITAL_OUTPUT;
    default:
      LOGE(
          "ContentKeyStatus::ProtobufHdcpToOemCryptoHdcp: "
          "Unknown HDCP Level: input=%d, returning HDCP_NO_DIGITAL_OUTPUT",
          input);
      return HDCP_NO_DIGITAL_OUTPUT;
  }
}

// Returns the constraint from a set of constraints that matches the
// specified resolution, or null if none match
VideoResolutionConstraint* GetConstraintForRes(
    uint32_t res, ConstraintList& constraints_from_key) {
  typedef ConstraintList::pointer_iterator Iterator;
  for (Iterator i = constraints_from_key.pointer_begin();
       i != constraints_from_key.pointer_end(); ++i) {
    VideoResolutionConstraint* constraint = *i;
    if (constraint->has_min_resolution_pixels() &&
        constraint->has_max_resolution_pixels() &&
        res >= constraint->min_resolution_pixels() &&
        res <= constraint->max_resolution_pixels()) {
      return constraint;
    }
  }
  return NULL;
}

}  // namespace

namespace wvcdm {

bool LicenseKeys::IsContentKey(const std::string& key_id) {
  if (key_statuses_.count(key_id) > 0) {
    return key_statuses_[key_id]->IsContentKey();
  } else if (content_keyid_to_entitlement_key_id_.count(key_id) > 0) {
    return true;
  } else {
    return false;
  }
}

bool LicenseKeys::CanDecryptContent(const std::string& key_id) {
  if (key_statuses_.count(key_id) > 0) {
    return key_statuses_[key_id]->CanDecryptContent();
  } else if (content_keyid_to_entitlement_key_id_.count(key_id) > 0) {
    if (key_statuses_.count(content_keyid_to_entitlement_key_id_[key_id]) > 0) {
      return key_statuses_[content_keyid_to_entitlement_key_id_[key_id]]
          ->CanDecryptContent();
    }
    return false;
  } else {
    return false;
  }
}

bool LicenseKeys::GetAllowedUsage(const KeyId& key_id,
                                  CdmKeyAllowedUsage* allowed_usage) {
  if (key_statuses_.count(key_id) > 0) {
    return key_statuses_[key_id]->GetAllowedUsage(allowed_usage);
  } else if (content_keyid_to_entitlement_key_id_.count(key_id) > 0) {
    if (key_statuses_.count(content_keyid_to_entitlement_key_id_[key_id]) > 0) {
      return key_statuses_[content_keyid_to_entitlement_key_id_[key_id]]
          ->CanDecryptContent();
    }
    return false;
  } else {
    return false;
  }
}

bool LicenseKeys::ApplyStatusChange(CdmKeyStatus new_status,
                                    bool* new_usable_keys) {
  bool keys_changed = false;
  bool newly_usable = false;
  *new_usable_keys = false;
  for (LicenseKeyStatusIterator it = key_statuses_.begin();
       it != key_statuses_.end(); ++it) {
    bool usable;
    if (it->second->ApplyStatusChange(new_status, &usable)) {
      newly_usable |= usable;
      keys_changed = true;
    }
  }
  *new_usable_keys = newly_usable;
  return keys_changed;
}

CdmKeyStatus LicenseKeys::GetKeyStatus(const KeyId& key_id) {
  if (key_statuses_.count(key_id) == 0) {
    return kKeyStatusKeyUnknown;
  }
  return key_statuses_[key_id]->GetKeyStatus();
}

void LicenseKeys::ExtractKeyStatuses(CdmKeyStatusMap* content_keys) {
  content_keys->clear();
  for (LicenseKeyStatusIterator it = key_statuses_.begin();
       it != key_statuses_.end(); ++it) {
    if (it->second->IsContentKey()) {
      const KeyId key_id = it->first;
      CdmKeyStatus key_status = it->second->GetKeyStatus();
      (*content_keys)[key_id] = key_status;
    }
  }
}

bool LicenseKeys::MeetsConstraints(const KeyId& key_id) {
  if (key_statuses_.count(key_id) > 0) {
    return key_statuses_[key_id]->MeetsConstraints();
  } else {
    // If a Key ID is unknown to us, we don't know of any constraints for it,
    // so never block decryption.
    return true;
  }
}

bool LicenseKeys::MeetsSecurityLevelConstraints(const KeyId& key_id) {
  if (key_statuses_.count(key_id) > 0) {
    return key_statuses_[key_id]->MeetsSecurityLevelConstraints();
  } else {
    // If a Key ID is unknown to us, we don't know of any constraints for it,
    // so never block decryption.
    return true;
  }
}

void LicenseKeys::ApplyConstraints(
    uint32_t new_resolution, CryptoSession::HdcpCapability new_hdcp_level) {
  for (LicenseKeyStatusIterator i = key_statuses_.begin();
       i != key_statuses_.end(); ++i) {
    i->second->ApplyConstraints(new_resolution, new_hdcp_level);
  }
}

void LicenseKeys::SetFromLicense(const video_widevine::License& license) {
  this->Clear();
  for (int32_t key_index = 0; key_index < license.key_size(); ++key_index) {
    const KeyContainer& key = license.key(key_index);
    if (key.has_id() && (key.type() == KeyContainer::CONTENT ||
                         key.type() == KeyContainer::OPERATOR_SESSION ||
                         key.type() == KeyContainer::ENTITLEMENT)) {
      const KeyId& key_id = key.id();
      key_statuses_[key_id] = new LicenseKeyStatus(key, security_level_);
    }
  }
}

void LicenseKeys::SetEntitledKeys(
    const std::vector<WidevinePsshData_EntitledKey>& keys) {
  for (std::vector<WidevinePsshData_EntitledKey>::const_iterator key =
           keys.begin();
       key != keys.end(); key++) {
    // Check to see if we have an entitlement key for this content key.
    std::map<KeyId, LicenseKeyStatus*>::iterator entitlement =
        key_statuses_.find(key->entitlement_key_id());
    if (entitlement == key_statuses_.end()) {
      continue;
    }
    // And set the new content key id.
    content_keyid_to_entitlement_key_id_[key->key_id()] =
        key->entitlement_key_id();
  }
}

LicenseKeyStatus::LicenseKeyStatus(const KeyContainer& key,
                                   const CdmSecurityLevel security_level)
    : is_content_key_(false),
      key_status_(kKeyStatusInternalError),
      meets_constraints_(true),
      meets_security_level_constraints_(true),
      default_hdcp_level_(HDCP_NONE) {
  allowed_usage_.Clear();
  constraints_.Clear();

  if (key.type() == KeyContainer::CONTENT ||
      key.type() == KeyContainer::ENTITLEMENT) {
    ParseContentKey(key, security_level);
  } else if (key.type() == KeyContainer::OPERATOR_SESSION) {
    ParseOperatorSessionKey(key);
  }
}

void LicenseKeyStatus::ParseContentKey(const KeyContainer& key,
                                       CdmSecurityLevel security_level) {
  is_content_key_ = true;
  if (key.has_level() && ((key.level() == KeyContainer::HW_SECURE_DECODE) ||
                          (key.level() == KeyContainer::HW_SECURE_ALL))) {
    allowed_usage_.decrypt_to_clear_buffer = false;
    allowed_usage_.decrypt_to_secure_buffer = true;
  } else {
    allowed_usage_.decrypt_to_clear_buffer = true;
    allowed_usage_.decrypt_to_secure_buffer = true;
  }

  if (key.has_level()) {
    switch (key.level()) {
      case KeyContainer::SW_SECURE_CRYPTO:
        allowed_usage_.key_security_level_ = kSoftwareSecureCrypto;
        break;
      case KeyContainer::SW_SECURE_DECODE:
        allowed_usage_.key_security_level_ = kSoftwareSecureDecode;
        break;
      case KeyContainer::HW_SECURE_CRYPTO:
        allowed_usage_.key_security_level_ = kHardwareSecureCrypto;
        break;
      case KeyContainer::HW_SECURE_DECODE:
        allowed_usage_.key_security_level_ = kHardwareSecureDecode;
        break;
      case KeyContainer::HW_SECURE_ALL:
        allowed_usage_.key_security_level_ = kHardwareSecureAll;
        break;
      default:
        allowed_usage_.key_security_level_ = kKeySecurityLevelUnknown;
        break;
    }

    switch (security_level) {
      case kSecurityLevelL1:
        meets_security_level_constraints_ = true;
        break;
      case kSecurityLevelL2:
      case kSecurityLevelL3:
        switch (key.level()) {
          case KeyContainer::SW_SECURE_CRYPTO:
          case KeyContainer::SW_SECURE_DECODE:
            meets_security_level_constraints_ = true;
            break;
          case KeyContainer::HW_SECURE_CRYPTO:
            meets_security_level_constraints_ =
                security_level == kSecurityLevelL2;
            break;
          default:
            meets_security_level_constraints_ = false;
            break;
        }
        break;
      default:
        meets_security_level_constraints_ = false;
        break;
    }
  } else {
    allowed_usage_.key_security_level_ = kKeySecurityLevelUnset;
    meets_security_level_constraints_ = true;
  }
  allowed_usage_.SetValid();

  if (key.video_resolution_constraints_size() > 0) {
    SetConstraints(key.video_resolution_constraints());
  }

  if (key.has_required_protection()) {
    default_hdcp_level_ =
        ProtobufHdcpToOemCryptoHdcp(key.required_protection().hdcp());
  }
}

void LicenseKeyStatus::ParseOperatorSessionKey(const KeyContainer& key) {
  is_content_key_ = false;
  if (key.has_operator_session_key_permissions()) {
    OperatorSessionKeyPermissions permissions =
        key.operator_session_key_permissions();
    if (permissions.has_allow_encrypt())
      allowed_usage_.generic_encrypt = permissions.allow_encrypt();
    if (permissions.has_allow_decrypt())
      allowed_usage_.generic_decrypt = permissions.allow_decrypt();
    if (permissions.has_allow_sign())
      allowed_usage_.generic_sign = permissions.allow_sign();
    if (permissions.has_allow_signature_verify())
      allowed_usage_.generic_verify = permissions.allow_signature_verify();
  } else {
    allowed_usage_.generic_encrypt = false;
    allowed_usage_.generic_decrypt = false;
    allowed_usage_.generic_sign = false;
    allowed_usage_.generic_verify = false;
  }
  allowed_usage_.SetValid();
}

void LicenseKeys::Clear() {
  for (LicenseKeyStatusIterator i = key_statuses_.begin();
       i != key_statuses_.end(); ++i) {
    delete i->second;
  }
  key_statuses_.clear();
}

bool LicenseKeyStatus::CanDecryptContent() {
  return is_content_key_ && key_status_ == kKeyStatusUsable;
}

bool LicenseKeyStatus::GetAllowedUsage(CdmKeyAllowedUsage* allowed_usage) {
  if (NULL == allowed_usage) return false;
  *allowed_usage = allowed_usage_;
  return true;
}

bool LicenseKeyStatus::ApplyStatusChange(CdmKeyStatus new_status,
                                         bool* new_usable_key) {
  *new_usable_key = false;
  if (!is_content_key_) {
    return false;
  }
  CdmKeyStatus updated_status = new_status;
  if (updated_status == kKeyStatusUsable) {
    if (!MeetsConstraints() || !MeetsSecurityLevelConstraints()) {
      updated_status = kKeyStatusOutputNotAllowed;
    }
  }
  if (key_status_ != updated_status) {
    key_status_ = updated_status;
    if (updated_status == kKeyStatusUsable) {
      *new_usable_key = true;
    }
    return true;
  }
  return false;
}

// If the key has constraints, find the constraint that applies.
// If none found, then the constraint test fails.
// If a constraint is found, verify that the device's current HDCP
// level is sufficient.  If the constraint has an HDCP setting, use it,
// If the key has no constraints, or if the constraint has no HDCP
// requirement, use the key's default HDCP setting to check against the
// device's current HDCP level.
void LicenseKeyStatus::ApplyConstraints(
    uint32_t video_pixels, CryptoSession::HdcpCapability new_hdcp_level) {
  VideoResolutionConstraint* current_constraint = NULL;
  if (HasConstraints() && video_pixels != HDCP_UNSPECIFIED_VIDEO_RESOLUTION) {
    current_constraint = GetConstraintForRes(video_pixels, constraints_);
    if (NULL == current_constraint) {
      meets_constraints_ = false;
      return;
    }
  }

  CryptoSession::HdcpCapability desired_hdcp_level;
  if (current_constraint && current_constraint->has_required_protection()) {
    desired_hdcp_level = ProtobufHdcpToOemCryptoHdcp(
        current_constraint->required_protection().hdcp());
  } else {
    desired_hdcp_level = default_hdcp_level_;
  }

  meets_constraints_ = (new_hdcp_level >= desired_hdcp_level);
}

void LicenseKeyStatus::SetConstraints(const ConstraintList& constraints) {
  if (!is_content_key_) {
    return;
  }
  constraints_.Clear();
  constraints_.MergeFrom(constraints);
  meets_constraints_ = true;
}

}  // namespace wvcdm
