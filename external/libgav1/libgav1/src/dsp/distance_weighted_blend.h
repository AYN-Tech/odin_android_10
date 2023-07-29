#ifndef LIBGAV1_SRC_DSP_DISTANCE_WEIGHTED_BLEND_H_
#define LIBGAV1_SRC_DSP_DISTANCE_WEIGHTED_BLEND_H_

// Pull in LIBGAV1_DspXXX defines representing the implementation status
// of each function. The resulting value of each can be used by each module to
// determine whether an implementation is needed at compile time.
// IWYU pragma: begin_exports

// ARM:
#include "src/dsp/arm/distance_weighted_blend_neon.h"

// x86:
// Note includes should be sorted in logical order avx2/avx/sse4, etc.
// The order of includes is important as each tests for a superior version
// before setting the base.
// clang-format off
#include "src/dsp/x86/distance_weighted_blend_sse4.h"
// clang-format on

// IWYU pragma: end_exports

namespace libgav1 {
namespace dsp {

// Initializes Dsp::distance_weighted_blend. This function is not thread-safe.
void DistanceWeightedBlendInit_C();

}  // namespace dsp
}  // namespace libgav1

#endif  // LIBGAV1_SRC_DSP_DISTANCE_WEIGHTED_BLEND_H_
