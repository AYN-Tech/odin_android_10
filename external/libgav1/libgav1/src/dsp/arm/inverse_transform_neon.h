#ifndef LIBGAV1_SRC_DSP_ARM_INVERSE_TRANSFORM_NEON_H_
#define LIBGAV1_SRC_DSP_ARM_INVERSE_TRANSFORM_NEON_H_

#include "src/dsp/cpu.h"
#include "src/dsp/dsp.h"

namespace libgav1 {
namespace dsp {

// Initializes Dsp::inverse_transforms, see the defines below for specifics.
// This function is not thread-safe.
void InverseTransformInit_NEON();

}  // namespace dsp
}  // namespace libgav1

#if LIBGAV1_ENABLE_NEON
#define LIBGAV1_Dsp8bpp_1DTransformSize4_1DTransformDct LIBGAV1_DSP_NEON
#define LIBGAV1_Dsp8bpp_1DTransformSize8_1DTransformDct LIBGAV1_DSP_NEON
#define LIBGAV1_Dsp8bpp_1DTransformSize16_1DTransformDct LIBGAV1_DSP_NEON
#define LIBGAV1_Dsp8bpp_1DTransformSize32_1DTransformDct LIBGAV1_DSP_NEON
#define LIBGAV1_Dsp8bpp_1DTransformSize64_1DTransformDct LIBGAV1_DSP_NEON

#define LIBGAV1_Dsp8bpp_1DTransformSize4_1DTransformAdst LIBGAV1_DSP_NEON
#define LIBGAV1_Dsp8bpp_1DTransformSize8_1DTransformAdst LIBGAV1_DSP_NEON
#define LIBGAV1_Dsp8bpp_1DTransformSize16_1DTransformAdst LIBGAV1_DSP_NEON

#define LIBGAV1_Dsp8bpp_1DTransformSize4_1DTransformIdentity LIBGAV1_DSP_NEON
#define LIBGAV1_Dsp8bpp_1DTransformSize8_1DTransformIdentity LIBGAV1_DSP_NEON
#define LIBGAV1_Dsp8bpp_1DTransformSize16_1DTransformIdentity LIBGAV1_DSP_NEON
#define LIBGAV1_Dsp8bpp_1DTransformSize32_1DTransformIdentity LIBGAV1_DSP_NEON

#define LIBGAV1_Dsp8bpp_1DTransformSize4_1DTransformWht LIBGAV1_DSP_NEON
#endif  // LIBGAV1_ENABLE_NEON

#endif  // LIBGAV1_SRC_DSP_ARM_INVERSE_TRANSFORM_NEON_H_
