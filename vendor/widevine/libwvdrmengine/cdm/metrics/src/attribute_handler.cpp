// Copyright 2018 Google Inc. All Rights Reserved.
//
// This file contains implementations for the AttributeHandler.

#include "attribute_handler.h"
#include "OEMCryptoCENC.h"
#include "pow2bucket.h"
#include "wv_cdm_types.h"

namespace wvcdm {
namespace metrics {

//
// Specializations for setting attribute fields.
//
template <>
void SetAttributeField<drm_metrics::Attributes::kErrorCodeFieldNumber,
                       CdmResponseType>(const CdmResponseType &cdm_error,
                                        drm_metrics::Attributes *attributes) {
  attributes->set_error_code(cdm_error);
}

template <>
void SetAttributeField<drm_metrics::Attributes::kCdmSecurityLevelFieldNumber,
                       CdmSecurityLevel>(
    const CdmSecurityLevel &cdm_security_level,
    drm_metrics::Attributes *attributes) {
  attributes->set_cdm_security_level(cdm_security_level);
}

template <>
void SetAttributeField<drm_metrics::Attributes::kSecurityLevelFieldNumber,
                       SecurityLevel>(const SecurityLevel &security_level,
                                      drm_metrics::Attributes *attributes) {
  attributes->set_security_level(security_level);
}

template <>
void SetAttributeField<drm_metrics::Attributes::kErrorCodeBoolFieldNumber,
                       bool>(const bool &cdm_error,
                             drm_metrics::Attributes *attributes) {
  attributes->set_error_code_bool(cdm_error);
}

template <>
void SetAttributeField<drm_metrics::Attributes::kOemCryptoResultFieldNumber,
                       OEMCryptoResult>(
    const OEMCryptoResult &oem_crypto_result,
    drm_metrics::Attributes *attributes) {
  attributes->set_oem_crypto_result(oem_crypto_result);
}

template <>
void SetAttributeField<drm_metrics::Attributes::kLengthFieldNumber, Pow2Bucket>(
    const Pow2Bucket &pow2, drm_metrics::Attributes *attributes) {
  attributes->set_length(pow2.value());
}

template <>
void SetAttributeField<drm_metrics::Attributes::kEncryptionAlgorithmFieldNumber,
                       CdmEncryptionAlgorithm>(
    const CdmEncryptionAlgorithm &encryption_algorithm,
    drm_metrics::Attributes *attributes) {
  attributes->set_encryption_algorithm(encryption_algorithm);
}

template <>
void SetAttributeField<drm_metrics::Attributes::kSigningAlgorithmFieldNumber,
                       CdmSigningAlgorithm>(
    const CdmSigningAlgorithm &signing_algorithm,
    drm_metrics::Attributes *attributes) {
  attributes->set_signing_algorithm(signing_algorithm);
}

template <>
void SetAttributeField<drm_metrics::Attributes::kKeyRequestTypeFieldNumber,
                       CdmKeyRequestType>(
    const CdmKeyRequestType &key_request_type,
    drm_metrics::Attributes *attributes) {
  attributes->set_key_request_type(key_request_type);
}

template <>
void SetAttributeField<drm_metrics::Attributes::kLicenseTypeFieldNumber,
                       CdmLicenseType>(
    const CdmLicenseType &license_type,
    drm_metrics::Attributes *attributes) {
  attributes->set_license_type(license_type);
}

template <>
void SetAttributeField<drm_metrics::Attributes::kErrorDetailFieldNumber,
                       int32_t>(
    const int32_t &error_detail,
    drm_metrics::Attributes *attributes) {
  attributes->set_error_detail(error_detail);
}

template <>
void SetAttributeField<0, util::Unused>(const util::Unused &,
                                        drm_metrics::Attributes *) {
  // Intentionally empty.
}

// Specializations only used by tests.
template <>
void SetAttributeField<drm_metrics::Attributes::kErrorCodeFieldNumber, int>(
    const int &cdm_error, drm_metrics::Attributes *attributes) {
  attributes->set_error_code(cdm_error);
}

}  // namespace metrics
}  // namespace wvcdm
