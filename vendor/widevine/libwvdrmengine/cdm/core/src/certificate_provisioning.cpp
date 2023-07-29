// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#include "certificate_provisioning.h"
#include "client_identification.h"
#include "device_files.h"
#include "file_store.h"
#include "license_protocol.pb.h"
#include "log.h"
#include "properties.h"
#include "service_certificate.h"
#include "string_conversions.h"
#include "wv_cdm_constants.h"

namespace {

const std::string kEmptyString;

// URL for Google Provisioning Server.
// The provisioning server supplies the certificate that is needed
// to communicate with the License Server.
const std::string kProvisioningServerUrl =
    "https://www.googleapis.com/"
    "certificateprovisioning/v1/devicecertificates/create"
    "?key=AIzaSyB-5OLKTx2iU5mko18DfdwK5611JIjbUhE";

// NOTE: Provider ID = widevine.com
const std::string kCpProductionServiceCertificate = wvcdm::a2bs_hex(
    "0ab9020803121051434fe2a44c763bcc2c826a2d6ef9a718f7d793d005228e02"
    "3082010a02820101009e27088659dbd9126bc6ed594caf652b0eaab82abb9862"
    "ada1ee6d2cb5247e94b28973fef5a3e11b57d0b0872c930f351b5694354a8c77"
    "ed4ee69834d2630372b5331c5710f38bdbb1ec3024cfadb2a8ac94d977d391b7"
    "d87c20c5c046e9801a9bffaf49a36a9ee6c5163eff5cdb63bfc750cf4a218618"
    "984e485e23a10f08587ec5d990e9ab0de71460dfc334925f3fb9b55761c61e28"
    "8398c387a0925b6e4dcaa1b36228d9feff7e789ba6e5ef6cf3d97e6ae05525db"
    "38f826e829e9b8764c9e2c44530efe6943df4e048c3c5900ca2042c5235dc80d"
    "443789e734bf8e59a55804030061ed48e7d139b521fbf35524b3000b3e2f6de0"
    "001f5eeb99e9ec635f02030100013a0c7769646576696e652e636f6d12800332"
    "2c2f3fedc47f8b7ba88a135a355466e378ed56a6fc29ce21f0cafc7fb253b073"
    "c55bed253d8650735417aad02afaefbe8d5687902b56a164490d83d590947515"
    "68860e7200994d322b5de07f82ef98204348a6c2c9619092340eb87df26f63bf"
    "56c191dc069b80119eb3060d771afaaeb2d30b9da399ef8a41d16f45fd121e09"
    "a0c5144da8f8eb46652c727225537ad65e2a6a55799909bbfb5f45b5775a1d1e"
    "ac4e06116c57adfa9ce0672f19b70b876f88e8b9fbc4f96ccc500c676cfb173c"
    "b6f52601573e2e45af1d9d2a17ef1487348c05cfc6d638ec2cae3fadb655e943"
    "1330a75d2ceeaa54803e371425111e20248b334a3a50c8eca683c448b8ac402c"
    "76e6f76e2751fbefb669f05703cec8c64cf7a62908d5fb870375eb0cc96c508e"
    "26e0c050f3fd3ebe68cef9903ef6405b25fc6e31f93559fcff05657662b3653a"
    "8598ed5751b38694419242a875d9e00d5a5832933024b934859ec8be78adccbb"
    "1ec7127ae9afeef9c5cd2e15bd3048e8ce652f7d8c5d595a0323238c598a28");

/*
 * Provisioning response is a base64-encoded protobuf, optionally within a
 * JSON wrapper. If the JSON wrapper is present, extract the embedded response
 * message. Then perform the base64 decode and return the result.
 *
 * If an error occurs during the parse or the decode, return an empty string.
 */
void ExtractAndDecodeSignedMessage(const std::string& provisioning_response,
                                   std::string* result) {
  const std::string json_start_substr("\"signedResponse\": \"");
  const std::string json_end_substr("\"");
  std::string message_string;

  size_t start = provisioning_response.find(json_start_substr);

  if (start == provisioning_response.npos) {
    // Message is not properly wrapped - reject it.
    LOGE("ExtractAndDecodeSignedMessage: cannot locate start substring");
    result->clear();
    return;
  } else {
    // Appears to be JSON-wrapped protobuf - find end of protobuf portion.
    size_t end = provisioning_response.find(json_end_substr,
                                            start + json_start_substr.length());
    if (end == provisioning_response.npos) {
      LOGE("ExtractAndDecodeSignedMessage: cannot locate end substring");
      result->clear();
      return;
    }
    size_t b64_string_size = end - start - json_start_substr.length();
    message_string.assign(provisioning_response,
                          start + json_start_substr.length(), b64_string_size);
  }

  if (message_string.empty()) {
    LOGE("ExtractAndDecodeSignedMessage: CdmProvisioningResponse is empty");
    result->clear();
    return;
  }

  // Decode the base64-encoded message.
  const std::vector<uint8_t> decoded_message =
      wvcdm::Base64SafeDecode(message_string);
  result->assign(decoded_message.begin(), decoded_message.end());
}

}  // namespace

namespace wvcdm {
// Protobuf generated classes.
using video_widevine::ClientIdentification_ClientCapabilities;
using video_widevine::ClientIdentification_NameValue;
using video_widevine::EncryptedClientIdentification;
using video_widevine::ProvisioningOptions;
using video_widevine::ProvisioningRequest;
using video_widevine::ProvisioningResponse;
using video_widevine::SignedProvisioningMessage;

CdmResponseType CertificateProvisioning::Init(
    const std::string& service_certificate) {

  std::string certificate = service_certificate.empty() ?
      kCpProductionServiceCertificate : service_certificate;
  return service_certificate_->Init(certificate);
}

/*
 * Fill in the appropriate SPOID (Stable Per-Origin IDentifier) option.
 * One of spoid, provider_id or stable_id will be passed to the provisioning
 * server for determining a unique per origin ID for the device.
 * It is also valid (though deprecated) to leave the settings unset.
 */
CdmResponseType CertificateProvisioning::SetSpoidParameter(
    const std::string& origin, const std::string& spoid,
    ProvisioningRequest* request) {
  if (!request) {
    LOGE("CertificateProvisioning::SetSpoidParameter: No request buffer "
         "passed to method.");
    return PARAMETER_NULL;
  }
  if (!spoid.empty()) {
    // Use the SPOID that has been pre-provided
    request->set_spoid(spoid);
  } else if (Properties::UseProviderIdInProvisioningRequest()) {
    if (!service_certificate_->provider_id().empty()) {
      request->set_provider_id(service_certificate_->provider_id());
    } else {
      LOGE("CertificateProvisioning::SetSpoidParameter: Failure getting "
           "provider ID");
      return SERVICE_CERTIFICATE_PROVIDER_ID_EMPTY;
    }
  } else if (origin != EMPTY_ORIGIN) {
    // Legacy behavior - Concatenate Unique ID with Origin
    std::string device_unique_id;
    CdmResponseType status =
        crypto_session_->GetInternalDeviceUniqueId(&device_unique_id);

    if (status != NO_ERROR) {
      LOGE("CertificateProvisioning::SetSpoidParameter: Failure getting "
           "device unique ID");
      return status;
    }
    request->set_stable_id(device_unique_id + origin);
  }  // No else clause, by design. It is valid to do nothing.
  return NO_ERROR;
}

/*
 * Return the provisioning protocol version - dictated by OEMCrypto
 * support for OEM certificates.
 */
SignedProvisioningMessage::ProtocolVersion
    CertificateProvisioning::GetProtocolVersion() {
  if (crypto_session_->GetPreProvisionTokenType() == kClientTokenOemCert)
    return SignedProvisioningMessage::VERSION_3;
  else
    return SignedProvisioningMessage::VERSION_2;
}

/*
 * Compose a device provisioning request and output *request in a
 * JSON-compatible format (web-safe base64).
 * Also return *default_url of the provisioning server.
 *
 * Returns NO_ERROR for success and CERT_PROVISIONING_REQUEST_ERROR_? if fails.
 */
CdmResponseType CertificateProvisioning::GetProvisioningRequest(
    SecurityLevel requested_security_level, CdmCertificateType cert_type,
    const std::string& cert_authority, const std::string& origin,
    const std::string& spoid, CdmProvisioningRequest* request,
    std::string* default_url) {
  if (!default_url) {
    LOGE("GetProvisioningRequest: pointer for returning URL is NULL");
    return CERT_PROVISIONING_REQUEST_ERROR_1;
  }

  default_url->assign(kProvisioningServerUrl);

  CdmResponseType status = crypto_session_->Open(requested_security_level);
  if (NO_ERROR != status) {
    LOGE("GetProvisioningRequest: fails to create a crypto session");
    return status;
  }

  // Prepare device provisioning request.
  ProvisioningRequest provisioning_request;

  wvcdm::ClientIdentification id;
  status = id.Init(crypto_session_.get());
  if (status != NO_ERROR) return status;

  video_widevine::ClientIdentification* client_id =
      provisioning_request.mutable_client_id();

  CdmAppParameterMap app_parameter;
  status = id.Prepare(app_parameter, kEmptyString, client_id);
  if (status != NO_ERROR) return status;

  if (!service_certificate_->has_certificate()) {
    LOGE("CertificateProvisioning::GetProvisioningRequest: Service "
         "Certificate not staged");
    return CERT_PROVISIONING_EMPTY_SERVICE_CERTIFICATE;
  }

  // Encrypt client identification
  EncryptedClientIdentification* encrypted_client_id =
      provisioning_request.mutable_encrypted_client_id();
  status = service_certificate_->EncryptClientId(
      crypto_session_.get(), client_id, encrypted_client_id);
  provisioning_request.clear_client_id();

  uint32_t nonce;
  status = crypto_session_->GenerateNonce(&nonce);

  if (status != NO_ERROR) {
    LOGE("GetProvisioningRequest: fails to generate a nonce: %d", status);
    return status == NONCE_GENERATION_ERROR ?
        CERT_PROVISIONING_NONCE_GENERATION_ERROR : status;
  }

  // The provisioning server does not convert the nonce to uint32_t, it just
  // passes the binary data to the response message.
  std::string the_nonce(reinterpret_cast<char*>(&nonce), sizeof(nonce));
  provisioning_request.set_nonce(the_nonce);

  ProvisioningOptions* options = provisioning_request.mutable_options();
  switch (cert_type) {
    case kCertificateWidevine:
      options->set_certificate_type(
          video_widevine::ProvisioningOptions_CertificateType_WIDEVINE_DRM);
      break;
    case kCertificateX509:
      options->set_certificate_type(
          video_widevine::ProvisioningOptions_CertificateType_X509);
      break;
    default:
      LOGE("GetProvisioningRequest: unknown certificate type %ld", cert_type);
      return CERT_PROVISIONING_INVALID_CERT_TYPE;
  }

  cert_type_ = cert_type;
  options->set_certificate_authority(cert_authority);

  status = SetSpoidParameter(origin, spoid, &provisioning_request);
  if (status != NO_ERROR) return status;

  std::string serialized_message;
  provisioning_request.SerializeToString(&serialized_message);

  // Derives signing and encryption keys and constructs signature.
  std::string request_signature;
  status = crypto_session_->PrepareRequest(serialized_message, true,
                                           &request_signature);

  if (status != NO_ERROR) {
    LOGE("GetProvisioningRequest: fails to prepare request");
    return status;
  }

  if (request_signature.empty()) {
    LOGE("GetProvisioningRequest: request signature is empty");
    return CERT_PROVISIONING_REQUEST_ERROR_4;
  }

  SignedProvisioningMessage signed_provisioning_msg;
  signed_provisioning_msg.set_message(serialized_message);
  signed_provisioning_msg.set_signature(request_signature);
  signed_provisioning_msg.set_protocol_version(GetProtocolVersion());

  std::string serialized_request;
  signed_provisioning_msg.SerializeToString(&serialized_request);

  if (!wvcdm::Properties::provisioning_messages_are_binary()) {
    // Return request as web-safe base64 string
    std::vector<uint8_t> request_vector(serialized_request.begin(),
                                        serialized_request.end());
    request->assign(Base64SafeEncodeNoPad(request_vector));
  } else {
    request->swap(serialized_request);
  }
  return NO_ERROR;
}

/*
 * The response message consists of a device certificate and the device RSA key.
 * The device RSA key is stored in the T.E.E. The device certificate is stored
 * in the device.
 *
 * Returns NO_ERROR for success and CERT_PROVISIONING_RESPONSE_ERROR_? if fails.
 */
CdmResponseType CertificateProvisioning::HandleProvisioningResponse(
    FileSystem* file_system, const CdmProvisioningResponse& response_message,
    std::string* cert, std::string* wrapped_key) {

  if (response_message.empty()) {
    LOGE("HandleProvisioningResponse: response message is empty.");
    return CERT_PROVISIONING_RESPONSE_ERROR_1;
  }

  std::string response;
  if (wvcdm::Properties::provisioning_messages_are_binary()) {
    response.assign(response_message);
  } else {
    // The response is base64 encoded in a JSON wrapper.
    // Extract it and decode it. On error return an empty string.
    ExtractAndDecodeSignedMessage(response_message, &response);
    if (response.empty()) {
      LOGE("HandleProvisioningResponse: response message is "
           "an invalid JSON/base64 string.");
      return CERT_PROVISIONING_RESPONSE_ERROR_1;
    }
  }

  // Authenticates provisioning response using D1s (server key derived from
  // the provisioing request's input). Validate provisioning response and
  // stores private device RSA key and certificate.
  SignedProvisioningMessage signed_response;
  if (!signed_response.ParseFromString(response)) {
    LOGE("HandleProvisioningResponse: fails to parse signed response");
    return CERT_PROVISIONING_RESPONSE_ERROR_2;
  }

  bool error = false;
  if (!signed_response.has_signature()) {
    LOGE("HandleProvisioningResponse: signature not found");
    error = true;
  }

  if (!signed_response.has_message()) {
    LOGE("HandleProvisioningResponse: message not found");
    error = true;
  }

  if (error) return CERT_PROVISIONING_RESPONSE_ERROR_3;

  const std::string& signed_message = signed_response.message();
  const std::string& signature = signed_response.signature();
  ProvisioningResponse provisioning_response;

  if (!provisioning_response.ParseFromString(signed_message)) {
    LOGE("HandleProvisioningResponse: Fails to parse signed message");
    return CERT_PROVISIONING_RESPONSE_ERROR_4;
  }

  if (!provisioning_response.has_device_rsa_key()) {
    LOGE("HandleProvisioningResponse: key not found");
    return CERT_PROVISIONING_RESPONSE_ERROR_5;
  }

  // If Provisioning 3.0 (OEM Cert provisioned), verify that the
  // message is properly signed.
  if (crypto_session_->GetPreProvisionTokenType() == kClientTokenOemCert) {
    if (service_certificate_->VerifySignedMessage(signed_message, signature)
        != NO_ERROR) {
      // TODO(b/69562876): if the cert is bad, request a new one.
      LOGE("HandleProvisioningResponse: message not properly signed");
      return CERT_PROVISIONING_RESPONSE_ERROR_6;
    }
  }

  const std::string& new_private_key = provisioning_response.device_rsa_key();
  const std::string& nonce = provisioning_response.nonce();
  const std::string& iv = provisioning_response.device_rsa_key_iv();

  const std::string& wrapping_key = (provisioning_response.has_wrapping_key()) ?
      provisioning_response.wrapping_key() : std::string();

  std::string wrapped_private_key;

  CdmResponseType status =
      crypto_session_->RewrapCertificate(signed_message, signature, nonce,
                                         new_private_key, iv, wrapping_key,
                                         &wrapped_private_key);

  if (status != NO_ERROR) {
    LOGE("HandleProvisioningResponse: RewrapCertificate fails");
    return status;
  }

  CdmSecurityLevel security_level = crypto_session_->GetSecurityLevel();
  crypto_session_->Close();

  if (cert_type_ == kCertificateX509) {
    *cert = provisioning_response.device_certificate();
    *wrapped_key = wrapped_private_key;
    return NO_ERROR;
  }

  // This is the entire certificate (SignedDrmDeviceCertificate).
  // This will be stored to the device as the final step in the device
  // provisioning process.
  const std::string& device_certificate =
      provisioning_response.device_certificate();

  DeviceFiles handle(file_system);
  if (!handle.Init(security_level)) {
    LOGE("HandleProvisioningResponse: failed to init DeviceFiles");
    return CERT_PROVISIONING_RESPONSE_ERROR_7;
  }
  if (!handle.StoreCertificate(device_certificate, wrapped_private_key)) {
    LOGE("HandleProvisioningResponse: failed to save provisioning certificate");
    return CERT_PROVISIONING_RESPONSE_ERROR_8;
  }

  return NO_ERROR;
}

}  // namespace wvcdm
