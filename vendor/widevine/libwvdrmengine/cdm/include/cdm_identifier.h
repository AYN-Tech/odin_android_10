// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
// CdmIdentifier - Struct that holds all the information necessary to
//                 uniquely identify a CdmEngine instance in the
//                 WvContentDecryptionModule multiplexing layer.

#ifndef CDM_BASE_CDM_IDENTIFIER_H_
#define CDM_BASE_CDM_IDENTIFIER_H_

#include <string>

#include "wv_cdm_constants.h"

namespace wvcdm {

// CdmIdentifier contains all the information necessary to uniquely identify a
// distinct CdmEngine instance on Android. There should be a unique CdmEngine
// (and thus distinct storage space) for every combination of SPOID and origin.
struct CdmIdentifier {
  // The Stable Per-Origin Identifier, or SPOID. May be blank on old, SPOID-less
  // systems, in which case multiple apps with the same origin will share a
  // CdmEngine and storage.
  std::string spoid;

  // The origin. May be blank if the app does not set an origin, which is
  // the likely behavior of most non-web-browser apps.
  std::string origin;

  // The application package name provided by the application. This is used to
  // provide a friendly name of the application package for the purposes of
  // logging and metrics.
  std::string app_package_name;

  // The unique identifier guarantees that no two identifiers share the same
  // CdmEngine instance. We're moving to a model where a plugin maps 1 to 1
  // with a CdmEngine instance. This is a simple way to implement that.
  uint32_t unique_id;

  // This method is needed to check to see if the identifier is equivalent
  // to the default cdm. E.g. no spoid, origin or app package name. Use this
  // comparison in lieu of the == operator when checking to see if the
  // identifier would cause the default provisioned certificate to be used.
  bool IsEquivalentToDefault() {
    return spoid == EMPTY_SPOID
        && origin == EMPTY_ORIGIN
        && app_package_name == EMPTY_APP_PACKAGE_NAME;
  }
};

// Provide comparison operators
inline bool operator==(const CdmIdentifier& lhs, const CdmIdentifier& rhs) {
  return lhs.spoid == rhs.spoid && lhs.origin == rhs.origin
      && lhs.app_package_name == rhs.app_package_name
      && lhs.unique_id == rhs.unique_id;
}

inline bool operator!=(const CdmIdentifier& lhs, const CdmIdentifier& rhs) {
  return !(lhs == rhs);
}

inline bool operator<(const CdmIdentifier& lhs, const CdmIdentifier& rhs) {
  return (lhs.spoid < rhs.spoid)
      || ((lhs.spoid == rhs.spoid)
          && (lhs.origin < rhs.origin
              || (lhs.origin == rhs.origin
                  && (lhs.app_package_name < rhs.app_package_name
                      || (lhs.app_package_name == rhs.app_package_name
                          && lhs.unique_id < rhs.unique_id)))));
}

inline bool operator>(const CdmIdentifier& lhs, const CdmIdentifier& rhs) {
  return rhs < lhs;
}

inline bool operator<=(const CdmIdentifier& lhs, const CdmIdentifier& rhs) {
  return !(lhs > rhs);
}

inline bool operator>=(const CdmIdentifier& lhs, const CdmIdentifier& rhs) {
  return !(lhs < rhs);
}

// Provide default
static const CdmIdentifier kDefaultCdmIdentifier = {
  EMPTY_SPOID,
  EMPTY_ORIGIN,
  EMPTY_APP_PACKAGE_NAME,
  0
};

}  // namespace wvcdm

#endif  // CDM_BASE_CDM_IDENTIFIER_H_
