// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#ifndef CDM_BASE_PROPERTIES_CONFIGURATION_H_
#define CDM_BASE_PROPERTIES_CONFIGURATION_H_

#include "wv_cdm_constants.h"
#include "properties.h"

namespace wvcdm {

// Set only one of the three below to true. If secure buffer
// is selected, fallback to userspace buffers may occur
// if L1/L2 OEMCrypto APIs fail
const bool kPropertyOemCryptoUseSecureBuffers = true;
const bool kPropertyOemCryptoUseFifo = false;
const bool kPropertyOemCryptoUseUserSpaceBuffers = false;

// If true, provisioning messages will be binary strings (serialized protobuf
// messages). Otherwise they are base64 (web-safe) encoded, and the response
// string accepted by the CDM includes the JSON wrapper.
const bool kPropertyProvisioningMessagesAreBinary = false;

// Controls behavior when privacy mode is enabled. If true
// and a service certificate is not provided, a service certificate
// request will be generated when requested to generate a license request.
// A service certificate response will also be processed. If false,
// an error will be generated.
const bool kAllowServiceCertificateRequests = true;

} // namespace wvcdm

#endif  // CDM_BASE_WV_PROPERTIES_CONFIGURATION_H_
