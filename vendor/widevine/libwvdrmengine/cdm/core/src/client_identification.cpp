// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#include "client_identification.h"

#include <sstream>

#include "crypto_session.h"
#include "license_protocol.pb.h"
#include "log.h"
#include "properties.h"
#include "string_conversions.h"
#include "wv_cdm_constants.h"

namespace {
const std::string kKeyCompanyName = "company_name";
const std::string kKeyModelName = "model_name";
const std::string kKeyArchitectureName = "architecture_name";
const std::string kKeyDeviceName = "device_name";
const std::string kKeyProductName = "product_name";
const std::string kKeyBuildInfo = "build_info";
const std::string kKeyDeviceId = "device_id";
const std::string kKeyWvCdmVersion = "widevine_cdm_version";
const std::string kKeyOemCryptoSecurityPatchLevel =
    "oem_crypto_security_patch_level";
const std::string kKeyOemCryptoBuildInformation =
    "oem_crypto_build_information";
}  // unnamed namespace

namespace wvcdm {
// Protobuf generated classes.
using video_widevine::ClientIdentification_ClientCapabilities;
using video_widevine::ClientIdentification_NameValue;
using video_widevine::EncryptedClientIdentification;
using video_widevine::ProvisioningOptions;
using video_widevine::ProvisioningRequest;
using video_widevine::ProvisioningResponse;
using video_widevine::SignedProvisioningMessage;

CdmResponseType ClientIdentification::Init(CryptoSession* crypto_session) {
  if (crypto_session == NULL) {
    LOGE("ClientIdentification::Init: crypto_session not provided");
    return PARAMETER_NULL;
  }

  is_license_request_ = false;
  crypto_session_ = crypto_session;
  return NO_ERROR;
}

CdmResponseType ClientIdentification::Init(const std::string& client_token,
                                           const std::string& device_id,
                                           CryptoSession* crypto_session) {
  if (crypto_session == NULL) {
    LOGE("ClientIdentification::Init: crypto_session not provided");
    return PARAMETER_NULL;
  }

  if (client_token.empty()) {
    LOGE("ClientIdentification::Init: crypto_session not provided");
    return PARAMETER_NULL;
  }

  is_license_request_ = true;
  device_id_ = device_id;
  client_token_ = client_token;
  crypto_session_ = crypto_session;
  return NO_ERROR;
}

/*
 * Return the ClientIdentification message token type for provisioning request.
 * NOTE: a DRM Cert should never be presented to the provisioning server.
 */
CdmResponseType ClientIdentification::Prepare(
    const CdmAppParameterMap& app_parameters,
    const std::string& provider_client_token,
    video_widevine::ClientIdentification* client_id) {

  if (is_license_request_) {
    client_id->set_type(
        video_widevine::ClientIdentification::DRM_DEVICE_CERTIFICATE);
    client_id->set_token(client_token_);
  } else {
    video_widevine::ClientIdentification::TokenType token_type;
    if (!GetProvisioningTokenType(&token_type)) {
      LOGE("ClientIdentification::Prepare: failure getting provisioning token "
           "type");
      return CLIENT_IDENTIFICATION_TOKEN_ERROR_1;
    }
    client_id->set_type(token_type);

    std::string token;
    CdmResponseType status = crypto_session_->GetProvisioningToken(&token);
    if (status != NO_ERROR) {
      LOGE("ClientIdentification::Prepare: failure getting provisioning token: "
           "%d", status);
      return status;
    }
    client_id->set_token(token);
  }

  ClientIdentification_NameValue* client_info;
  if (is_license_request_) {
    CdmAppParameterMap::const_iterator iter;
    for (iter = app_parameters.begin(); iter != app_parameters.end(); iter++) {
      client_info = client_id->add_client_info();
      client_info->set_name(iter->first);
      client_info->set_value(iter->second);
    }
  }
  std::string value;
  if (Properties::GetCompanyName(&value)) {
    client_info = client_id->add_client_info();
    client_info->set_name(kKeyCompanyName);
    client_info->set_value(value);
  }
  if (Properties::GetModelName(&value)) {
    client_info = client_id->add_client_info();
    client_info->set_name(kKeyModelName);
    client_info->set_value(value);
  }
  if (Properties::GetArchitectureName(&value)) {
    client_info = client_id->add_client_info();
    client_info->set_name(kKeyArchitectureName);
    client_info->set_value(value);
  }
  if (Properties::GetDeviceName(&value)) {
    client_info = client_id->add_client_info();
    client_info->set_name(kKeyDeviceName);
    client_info->set_value(value);
  }
  if (Properties::GetProductName(&value)) {
    client_info = client_id->add_client_info();
    client_info->set_name(kKeyProductName);
    client_info->set_value(value);
  }
  if (Properties::GetBuildInfo(&value)) {
    client_info = client_id->add_client_info();
    client_info->set_name(kKeyBuildInfo);
    client_info->set_value(value);
  }
  if (Properties::GetWVCdmVersion(&value)) {
    client_info = client_id->add_client_info();
    client_info->set_name(kKeyWvCdmVersion);
    client_info->set_value(value);
  }
  client_info = client_id->add_client_info();
  client_info->set_name(kKeyOemCryptoSecurityPatchLevel);
  std::stringstream ss;
  ss << (uint32_t)crypto_session_->GetSecurityPatchLevel();
  client_info->set_value(ss.str());

  if (!provider_client_token.empty()) {
    client_id->set_provider_client_token(provider_client_token);
  }

  ClientIdentification_ClientCapabilities* client_capabilities =
      client_id->mutable_client_capabilities();

  client_capabilities->set_client_token(true);

  if (is_license_request_) {
    bool supports_usage_information;
    if (crypto_session_->UsageInformationSupport(&supports_usage_information)) {
      client_capabilities->set_session_token(supports_usage_information);
    }

    client_capabilities->set_anti_rollback_usage_table(
        crypto_session_->IsAntiRollbackHwPresent());
  }

  uint32_t api_version = 0;
  if (crypto_session_->GetApiVersion(&api_version)) {
    client_capabilities->set_oem_crypto_api_version(api_version);
  }

  if (is_license_request_) {
    CryptoSession::HdcpCapability current_version, max_version;
    if (crypto_session_->GetHdcpCapabilities(&current_version, &max_version) ==
        NO_ERROR) {
      switch (max_version) {
        case HDCP_NONE:
          client_capabilities->set_max_hdcp_version(
              video_widevine::
                  ClientIdentification_ClientCapabilities_HdcpVersion_HDCP_NONE);
          break;
        case HDCP_V1:
          client_capabilities->set_max_hdcp_version(
              video_widevine::
                  ClientIdentification_ClientCapabilities_HdcpVersion_HDCP_V1);
          break;
        case HDCP_V2:
          client_capabilities->set_max_hdcp_version(
              video_widevine::
                  ClientIdentification_ClientCapabilities_HdcpVersion_HDCP_V2);
          break;
        case HDCP_V2_1:
          client_capabilities->set_max_hdcp_version(
              video_widevine::
                  ClientIdentification_ClientCapabilities_HdcpVersion_HDCP_V2_1);
          break;
        case HDCP_V2_2:
          client_capabilities->set_max_hdcp_version(
              video_widevine::
                  ClientIdentification_ClientCapabilities_HdcpVersion_HDCP_V2_2);
          break;
        case HDCP_V2_3:
          client_capabilities->set_max_hdcp_version(
              video_widevine::
                  ClientIdentification_ClientCapabilities_HdcpVersion_HDCP_V2_3);
          break;
        case HDCP_NO_DIGITAL_OUTPUT:
          client_capabilities->set_max_hdcp_version(
              video_widevine::
                  ClientIdentification_ClientCapabilities_HdcpVersion_HDCP_NO_DIGITAL_OUTPUT);
          break;
        default:
          LOGW(
              "ClientIdentification::PrepareClientId: unexpected HDCP max "
              "capability version %d", max_version);
      }
    }
  }

  CryptoSession::SupportedCertificateTypes supported_certs;
  if (crypto_session_->GetSupportedCertificateTypes(&supported_certs)) {
    if (supported_certs.rsa_2048_bit) {
      client_capabilities->add_supported_certificate_key_type(
          video_widevine::
              ClientIdentification_ClientCapabilities_CertificateKeyType_RSA_2048);
    }
    if (supported_certs.rsa_3072_bit) {
      client_capabilities->add_supported_certificate_key_type(
          video_widevine::
              ClientIdentification_ClientCapabilities_CertificateKeyType_RSA_3072);
    }
  }

  if (is_license_request_) {
    client_capabilities->set_can_update_srm(
        crypto_session_->IsSrmUpdateSupported());
    uint16_t srm_version;
    if (crypto_session_->GetSrmVersion(&srm_version) == NO_ERROR)
      client_capabilities->set_srm_version(srm_version);
  }
  bool can_support_output;
  bool can_disable_output;
  bool can_support_cgms_a;
  if (crypto_session_->GetAnalogOutputCapabilities(&can_support_output,
                                                   &can_disable_output,
                                                   &can_support_cgms_a)) {
    video_widevine::ClientIdentification_ClientCapabilities_AnalogOutputCapabilities
        capabilities = video_widevine::ClientIdentification_ClientCapabilities_AnalogOutputCapabilities_ANALOG_OUTPUT_NONE;

    if (can_support_output) {
      if (can_support_cgms_a) {
        capabilities = video_widevine::ClientIdentification_ClientCapabilities_AnalogOutputCapabilities_ANALOG_OUTPUT_SUPPORTS_CGMS_A;
      } else {
        capabilities = video_widevine::ClientIdentification_ClientCapabilities_AnalogOutputCapabilities_ANALOG_OUTPUT_SUPPORTED;
      }
    }
    client_capabilities->set_analog_output_capabilities(capabilities);
    client_capabilities->set_can_disable_analog_output(can_disable_output);
  } else {
    client_capabilities->set_analog_output_capabilities(video_widevine::ClientIdentification_ClientCapabilities_AnalogOutputCapabilities_ANALOG_OUTPUT_UNKNOWN);
  }

  uint32_t version, tier;
  if (crypto_session_->GetApiVersion(&version)) {
    if (version >= OEM_CRYPTO_API_VERSION_SUPPORTS_RESOURCE_RATING_TIER) {
      if (crypto_session_->GetResourceRatingTier(&tier)) {
        client_capabilities->set_resource_rating_tier(tier);
      }
    }
    std::string info;
    if (crypto_session_->GetBuildInformation(&info)) {
      client_info = client_id->add_client_info();
      client_info->set_name(kKeyOemCryptoBuildInformation);
      client_info->set_value(info);
    }
  }

  return NO_ERROR;
}

bool ClientIdentification::GetProvisioningTokenType(
    video_widevine::ClientIdentification::TokenType* token_type) {
  CdmClientTokenType token = crypto_session_->GetPreProvisionTokenType();
  switch (token) {
    case kClientTokenKeybox:
      *token_type = video_widevine::ClientIdentification::KEYBOX;
      return true;
    case kClientTokenOemCert:
      *token_type =
          video_widevine::ClientIdentification::OEM_DEVICE_CERTIFICATE;
      return true;
    case kClientTokenDrmCert:
    default:
      // shouldn't happen
      LOGE("CertificateProvisioning::GetProvisioningTokenType: unexpected "
          "provisioning type: %d", token);
      return false;
  }
}

}  // namespace wvcdm
