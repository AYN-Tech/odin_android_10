#ifndef LIBGAV1_SRC_DSP_ARM_AVERAGE_BLEND_NEON_H_
#define LIBGAV1_SRC_DSP_ARM_AVERAGE_BLEND_NEON_H_

#include "src/dsp/cpu.h"
#include "src/dsp/dsp.h"

namespace libgav1 {
namespace dsp {

// Initializes Dsp::average_blend. This function is not thread-safe.
void AverageBlendInit_NEON();

}  // namespace dsp
}  // namespace libgav1

#if LIBGAV1_ENABLE_NEON
#define LIBGAV1_Dsp8bpp_AverageBlend LIBGAV1_DSP_NEON
#endif  // LIBGAV1_ENABLE_NEON

#endif  // LIBGAV1_SRC_DSP_ARM_AVERAGE_BLEND_NEON_H_
