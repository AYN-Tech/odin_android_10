// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
#ifndef WVCDM_CORE_SERVICE_CERTIFICATE_H_
#define WVCDM_CORE_SERVICE_CERTIFICATE_H_

// Service Certificates are used to encrypt the ClientIdentification message
// that is part of Device Provisioning, License, Renewal, and Release requests.
// It also supplies a provider_id setting used in device provisioning.
// Service Certificates are typically supplied by the application. If one
// is not supplied and privacy mode is enabled, the CDM will send a Service
// Certificate Request to the target server to get one. Once the Service
// Certificate is established for the session, it should not change.

#include <memory>

#include "disallow_copy_and_assign.h"
#include "license_protocol.pb.h"
#include "privacy_crypto.h"
#include "wv_cdm_types.h"

namespace wvcdm {

class CryptoSession;

class ServiceCertificate {
 public:
  ServiceCertificate() : has_certificate_(false) {}
  virtual ~ServiceCertificate() {}

  // Set up a new service certificate.
  // Accept a serialized video_widevine::SignedDrmDeviceCertificate message.
  virtual CdmResponseType Init(const std::string& signed_certificate);

  bool has_certificate() const { return has_certificate_; }
  const std::string certificate() const { return certificate_; }
  const std::string& provider_id() const { return provider_id_; }

  // Verify the signature for a message.
  virtual CdmResponseType VerifySignedMessage(const std::string& message,
                                              const std::string& signature);

  // Encrypt the ClientIdentification message for a provisioning or
  // licensing request.  Encryption is performed using the current
  // service certificate.  Return a failure if the service certificate is
  // not present, not valid, or if some other error occurs.
  // The routine should not be called if privacy mode is off or if the
  // certificate is empty.
  virtual CdmResponseType EncryptClientId(
      CryptoSession* crypto_session,
      const video_widevine::ClientIdentification* clear_client_id,
      video_widevine::EncryptedClientIdentification* encrypted_client_id);

  // Helper methods
  static bool GetRequest(CdmKeyMessage* request);
  static CdmResponseType ParseResponse(const std::string& response,
                                       std::string* signed_certificate);
 private:

  // Encrypt data using RSA with OAEP padding.
  // |plaintext| is the data to be encrypted. |ciphertext| is a pointer to a
  // string to contain the decrypted data on return, and may not be null.
  // returns NO_ERROR if successful or an appropriate error code otherwise.
  virtual CdmResponseType EncryptRsaOaep(const std::string& plaintext,
                                         std::string* ciphertext);

  // Track whether object holds valid certificate
  bool has_certificate_;

  // Certificate, verified and extracted from signed message.
  std::string certificate_;

  // Certificate serial number.
  std::string serial_number_;

  // Provider ID, extracted from certificate message.
  std::string provider_id_;

  // Public key.
  std::unique_ptr<RsaPublicKey> public_key_;

  CORE_DISALLOW_COPY_AND_ASSIGN(ServiceCertificate);
};

}  // namespace wvcdm

#endif  // WVCDM_CORE_SERVICE_CERTIFICATE_H_
