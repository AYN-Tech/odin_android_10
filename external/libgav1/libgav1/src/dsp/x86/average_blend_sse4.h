#ifndef LIBGAV1_SRC_DSP_X86_AVERAGE_BLEND_SSE4_H_
#define LIBGAV1_SRC_DSP_X86_AVERAGE_BLEND_SSE4_H_

#include "src/dsp/cpu.h"
#include "src/dsp/dsp.h"

namespace libgav1 {
namespace dsp {

// Initializes Dsp::average_blend. This function is not thread-safe.
void AverageBlendInit_SSE4_1();

}  // namespace dsp
}  // namespace libgav1

// If sse4 is enabled and the baseline isn't set due to a higher level of
// optimization being enabled, signal the sse4 implementation should be used.
#if LIBGAV1_ENABLE_SSE4_1
#ifndef LIBGAV1_Dsp8bpp_AverageBlend
#define LIBGAV1_Dsp8bpp_AverageBlend LIBGAV1_DSP_SSE4_1
#endif

#endif  // LIBGAV1_ENABLE_SSE4_1

#endif  // LIBGAV1_SRC_DSP_X86_AVERAGE_BLEND_SSE4_H_
