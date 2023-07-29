#ifndef LIBGAV1_SRC_DSP_ARM_CONVOLVE_NEON_H_
#define LIBGAV1_SRC_DSP_ARM_CONVOLVE_NEON_H_

#include "src/dsp/cpu.h"
#include "src/dsp/dsp.h"

namespace libgav1 {
namespace dsp {

// Initializes Dsp::convolve. This function is not thread-safe.
void ConvolveInit_NEON();

}  // namespace dsp
}  // namespace libgav1

#if LIBGAV1_ENABLE_NEON
#define LIBGAV1_Dsp8bpp_ConvolveHorizontal LIBGAV1_DSP_NEON
#define LIBGAV1_Dsp8bpp_ConvolveVertical LIBGAV1_DSP_NEON
#define LIBGAV1_Dsp8bpp_Convolve2D LIBGAV1_DSP_NEON

#define LIBGAV1_Dsp8bpp_ConvolveCompoundCopy LIBGAV1_DSP_NEON
#define LIBGAV1_Dsp8bpp_ConvolveCompoundHorizontal LIBGAV1_DSP_NEON
#define LIBGAV1_Dsp8bpp_ConvolveCompoundVertical LIBGAV1_DSP_NEON
#define LIBGAV1_Dsp8bpp_ConvolveCompound2D LIBGAV1_DSP_NEON

// TODO(petersonab,b/139707209): Fix source buffer overreads.
// #define LIBGAV1_Dsp8bpp_ConvolveCompoundScale2D LIBGAV1_DSP_NEON
#endif  // LIBGAV1_ENABLE_NEON

#endif  // LIBGAV1_SRC_DSP_ARM_CONVOLVE_NEON_H_
