// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#ifndef WVCDM_CORE_CERTIFICATE_PROVISIONING_H_
#define WVCDM_CORE_CERTIFICATE_PROVISIONING_H_

#include <memory>
#include <string>

#include "crypto_session.h"
#include "disallow_copy_and_assign.h"
#include "license_protocol.pb.h"
#include "metrics_collections.h"
#include "oemcrypto_adapter.h"
#include "service_certificate.h"
#include "wv_cdm_types.h"

namespace wvcdm {

class CdmClientPropertySet;
class CdmSession;
class FileSystem;
class ServiceCertificate;

class CertificateProvisioning {
 public:
  CertificateProvisioning(metrics::CryptoMetrics* metrics) :
      crypto_session_(CryptoSession::MakeCryptoSession(metrics)),
      cert_type_(kCertificateWidevine),
      service_certificate_(new ServiceCertificate()) {}
  ~CertificateProvisioning() {}

  CdmResponseType Init(const std::string& service_certificate);

  // Construct a valid provisioning request.
  // The request will be sent to the provisioning server.
  CdmResponseType GetProvisioningRequest(
      SecurityLevel requested_security_level, CdmCertificateType cert_type,
      const std::string& cert_authority, const std::string& origin,
      const std::string& spoid, CdmProvisioningRequest* request,
      std::string* default_url);

  // Process the provisioning response.
  CdmResponseType HandleProvisioningResponse(
      FileSystem* file_system, const CdmProvisioningResponse& response,
      std::string* cert, std::string* wrapped_key);

 private:
  CdmResponseType SetSpoidParameter(
      const std::string& origin,
      const std::string& spoid,
      video_widevine::ProvisioningRequest* request);

  video_widevine::SignedProvisioningMessage::ProtocolVersion
      GetProtocolVersion();

  std::unique_ptr<CryptoSession> crypto_session_;
  CdmCertificateType cert_type_;
  std::unique_ptr<ServiceCertificate> service_certificate_;

  CORE_DISALLOW_COPY_AND_ASSIGN(CertificateProvisioning);
};

}  // namespace wvcdm

#endif  // WVCDM_CORE_CERTIFICATE_PROVISIONING_H_
