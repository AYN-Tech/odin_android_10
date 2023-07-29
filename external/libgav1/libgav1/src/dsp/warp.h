#ifndef LIBGAV1_SRC_DSP_WARP_H_
#define LIBGAV1_SRC_DSP_WARP_H_

// Pull in LIBGAV1_DspXXX defines representing the implementation status
// of each function. The resulting value of each can be used by each module to
// determine whether an implementation is needed at compile time.
// IWYU pragma: begin_exports

// ARM:
#include "src/dsp/arm/warp_neon.h"

// IWYU pragma: end_exports

namespace libgav1 {
namespace dsp {

// Initializes Dsp::warp. This function is not thread-safe.
void WarpInit_C();

}  // namespace dsp
}  // namespace libgav1

#endif  // LIBGAV1_SRC_DSP_WARP_H_
