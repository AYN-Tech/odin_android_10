// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#ifndef WVCDM_CORE_CLIENT_IDENTIFICATION_H_
#define WVCDM_CORE_CLIENT_IDENTIFICATION_H_

// ClientIdentification fills in the ClientIdentification portion
// of the License or Provisioning request messages.

#include "disallow_copy_and_assign.h"
#include "license_protocol.pb.h"
#include "wv_cdm_types.h"

namespace wvcdm {

class CryptoSession;

class ClientIdentification {
 public:
  ClientIdentification() : is_license_request_(true) {}
  virtual ~ClientIdentification() {}

  // Call this method when used with provisioning requests
  CdmResponseType Init(CryptoSession* crypto_session);

  // Use in conjunction with license requests
  // |client_token| must be provided
  // |device_id| optional
  // |crypto_session| input parameter, mandatory
  CdmResponseType Init(const std::string& client_token,
                       const std::string& device_id,
                       CryptoSession* crypto_session);

  // Fill the ClientIdentification portion of the license or provisioning
  // request
  // |app_parameters| parameters provided by client/app to be included in
  //                  provisioning/license request. optional, only used
  //                  if |is_license_request| is true
  // |provider_client_token| optional parameter specified by the content
  //                         and included in the license. Only used if
  //                         specified and if |is_license_request| is true
  // |client_id| Portion of license/provisioning request that needs to be
  //             populated.
  virtual CdmResponseType Prepare(
      const CdmAppParameterMap& app_parameters,
      const std::string& provider_client_token,
      video_widevine::ClientIdentification* client_id);

 private:
  bool GetProvisioningTokenType(
      video_widevine::ClientIdentification::TokenType* token_type);

  bool is_license_request_;
  std::string client_token_;
  std::string device_id_;
  CryptoSession* crypto_session_;

  CORE_DISALLOW_COPY_AND_ASSIGN(ClientIdentification);
};

}  // namespace wvcdm

#endif  // WVCDM_CORE_CLIENT_IDENTIFICATION_H_
