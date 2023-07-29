#ifndef LIBGAV1_SRC_DSP_ARM_OBMC_NEON_H_
#define LIBGAV1_SRC_DSP_ARM_OBMC_NEON_H_

#include "src/dsp/cpu.h"
#include "src/dsp/dsp.h"

namespace libgav1 {
namespace dsp {

// Initializes Dsp::obmc_blend. This function is not thread-safe.
void ObmcInit_NEON();

}  // namespace dsp
}  // namespace libgav1

// If NEON is enabled, signal the NEON implementation should be used.
#if LIBGAV1_ENABLE_NEON
#define LIBGAV1_Dsp8bpp_ObmcVertical LIBGAV1_DSP_NEON
#define LIBGAV1_Dsp8bpp_ObmcHorizontal LIBGAV1_DSP_NEON
#endif  // LIBGAV1_ENABLE_NEON

#endif  // LIBGAV1_SRC_DSP_ARM_OBMC_NEON_H_
