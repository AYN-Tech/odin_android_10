// Copyright 2017 Google Inc. All Rights Reserved.
//
// This file contains the helper classes and methods for using field tuples
// used by metrics classes to record variations of a single metric.
#ifndef WVCDM_METRICS_FIELD_TUPLES_H_
#define WVCDM_METRICS_FIELD_TUPLES_H_

#include <ostream>
#include <sstream>

namespace wvcdm {
namespace metrics {
namespace util {

// TODO(blueeyes): Change to use C++ 11 support for variadic template args.
// The C++ 03 pattern is no longer needed since we require C++11. b/68766426.

// This is a placeholder type for unused type parameters. It aids in supporting
// templated classes with "variable" type arguments.
struct Unused {
  // Required for compilation. Should never be used.
  inline friend std::ostream& operator<<(std::ostream& out, const Unused&) {
    return out;
  }
};

}  // namespace util
}  // namespace metrics
}  // namespace wvcdm

#endif  // WVCDM_METRICS_FIELD_TUPLES_H_
