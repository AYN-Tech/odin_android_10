#ifndef LIBGAV1_SRC_DSP_ARM_LOOP_FILTER_NEON_H_
#define LIBGAV1_SRC_DSP_ARM_LOOP_FILTER_NEON_H_

#include "src/dsp/cpu.h"
#include "src/dsp/dsp.h"

namespace libgav1 {
namespace dsp {

// Initializes Dsp::loop_filters, see the defines below for specifics. This
// function is not thread-safe.
void LoopFilterInit_NEON();

}  // namespace dsp
}  // namespace libgav1

#if LIBGAV1_ENABLE_NEON

#define LIBGAV1_Dsp8bpp_LoopFilterSize4_LoopFilterTypeHorizontal \
  LIBGAV1_DSP_NEON
#define LIBGAV1_Dsp8bpp_LoopFilterSize4_LoopFilterTypeVertical LIBGAV1_DSP_NEON

#define LIBGAV1_Dsp8bpp_LoopFilterSize6_LoopFilterTypeHorizontal \
  LIBGAV1_DSP_NEON
#define LIBGAV1_Dsp8bpp_LoopFilterSize6_LoopFilterTypeVertical LIBGAV1_DSP_NEON

#define LIBGAV1_Dsp8bpp_LoopFilterSize8_LoopFilterTypeHorizontal \
  LIBGAV1_DSP_NEON
#define LIBGAV1_Dsp8bpp_LoopFilterSize8_LoopFilterTypeVertical LIBGAV1_DSP_NEON

#define LIBGAV1_Dsp8bpp_LoopFilterSize14_LoopFilterTypeHorizontal \
  LIBGAV1_DSP_NEON
#define LIBGAV1_Dsp8bpp_LoopFilterSize14_LoopFilterTypeVertical LIBGAV1_DSP_NEON

#endif  // LIBGAV1_ENABLE_NEON

#endif  // LIBGAV1_SRC_DSP_ARM_LOOP_FILTER_NEON_H_
