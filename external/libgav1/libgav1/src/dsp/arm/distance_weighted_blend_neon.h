#ifndef LIBGAV1_SRC_DSP_ARM_DISTANCE_WEIGHTED_BLEND_NEON_H_
#define LIBGAV1_SRC_DSP_ARM_DISTANCE_WEIGHTED_BLEND_NEON_H_

#include "src/dsp/cpu.h"
#include "src/dsp/dsp.h"

namespace libgav1 {
namespace dsp {

// Initializes Dsp::distance_weighted_blend. This function is not thread-safe.
void DistanceWeightedBlendInit_NEON();

}  // namespace dsp
}  // namespace libgav1

// If NEON is enabled signal the NEON implementation should be used instead of
// normal C.
#if LIBGAV1_ENABLE_NEON
#define LIBGAV1_Dsp8bpp_DistanceWeightedBlend LIBGAV1_DSP_NEON

#endif  // LIBGAV1_ENABLE_NEON

#endif  // LIBGAV1_SRC_DSP_ARM_DISTANCE_WEIGHTED_BLEND_NEON_H_
