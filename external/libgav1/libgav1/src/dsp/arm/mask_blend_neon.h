#ifndef LIBGAV1_SRC_DSP_ARM_MASK_BLEND_NEON_H_
#define LIBGAV1_SRC_DSP_ARM_MASK_BLEND_NEON_H_

#include "src/dsp/cpu.h"
#include "src/dsp/dsp.h"

namespace libgav1 {
namespace dsp {

// Initializes Dsp::mask_blend. This function is not thread-safe.
void MaskBlendInit_NEON();

}  // namespace dsp
}  // namespace libgav1

#if LIBGAV1_ENABLE_NEON
#define LIBGAV1_Dsp8bpp_MaskBlend444 LIBGAV1_DSP_NEON
#define LIBGAV1_Dsp8bpp_MaskBlend422 LIBGAV1_DSP_NEON
#define LIBGAV1_Dsp8bpp_MaskBlend420 LIBGAV1_DSP_NEON
#define LIBGAV1_Dsp8bpp_MaskBlendInterIntra444 LIBGAV1_DSP_NEON
#define LIBGAV1_Dsp8bpp_MaskBlendInterIntra422 LIBGAV1_DSP_NEON
#define LIBGAV1_Dsp8bpp_MaskBlendInterIntra420 LIBGAV1_DSP_NEON
#endif  // LIBGAV1_ENABLE_NEON

#endif  // LIBGAV1_SRC_DSP_ARM_MASK_BLEND_NEON_H_
