#ifndef LIBGAV1_SRC_STATUS_CODE_H_
#define LIBGAV1_SRC_STATUS_CODE_H_

// All the declarations in this file are part of the public ABI. This file may
// be included by both C and C++ files.

// The Libgav1StatusCode enum type: A libgav1 function may return
// Libgav1StatusCode to indicate success or the reason for failure.
typedef enum {
  // Success.
  kLibgav1StatusOk = 0,

  // An unknown error. Used as the default error status if error detail is not
  // available.
  kLibgav1StatusUnknownError = -1,

  // An invalid function argument.
  kLibgav1StatusInvalidArgument = -2,

  // Memory allocation failure.
  kLibgav1StatusOutOfMemory = -3,

  // Ran out of a resource (other than memory).
  kLibgav1StatusResourceExhausted = -4,

  // The object is not initialized.
  kLibgav1StatusNotInitialized = -5,

  // An operation that can only be performed once has already been performed.
  kLibgav1StatusAlready = -6,

  // Not implemented, or not supported.
  kLibgav1StatusUnimplemented = -7,

  // An internal error in libgav1. Usually this indicates a programming error.
  kLibgav1StatusInternalError = -8,

  // The bitstream is not encoded correctly or violates a bitstream conformance
  // requirement.
  kLibgav1StatusBitstreamError = -9,

  // An extra enumerator to prevent people from writing code that fails to
  // compile when a new status code is added.
  //
  // Do not reference this enumerator. In particular, if you write code that
  // switches on Libgav1StatusCode, add a default: case instead of a case that
  // mentions this enumerator.
  //
  // Do not depend on the value (currently -1000) listed here. It may change in
  // the future.
  kLibgav1StatusReservedForFutureExpansionUseDefaultInSwitchInstead_ = -1000
} Libgav1StatusCode;

#if defined(__cplusplus)
// Declare type aliases for C++.
namespace libgav1 {

using StatusCode = Libgav1StatusCode;

}  // namespace libgav1
#endif  // defined(__cplusplus)

#endif  // LIBGAV1_SRC_STATUS_CODE_H_
