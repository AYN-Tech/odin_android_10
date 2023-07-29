// This header is used to access the OEM certificate if one is in use.
#ifndef OEM_CERT_H_
#define OEM_CERT_H_

#include <stddef.h>
#include <stdint.h>

namespace wvoec_ref {

// Refer to the following in main modules
extern const uint32_t kOEMSystemId;

extern const uint8_t* kOEMPrivateKey;
extern const uint8_t* kOEMPublicCert;

extern const size_t kOEMPrivateKeySize;
extern const size_t kOEMPublicCertSize;

}  // namespace wvoec_ref

#endif  // OEM_CERT_H_
