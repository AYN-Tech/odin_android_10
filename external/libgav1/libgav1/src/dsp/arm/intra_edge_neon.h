#ifndef LIBGAV1_SRC_DSP_ARM_INTRA_EDGE_NEON_H_
#define LIBGAV1_SRC_DSP_ARM_INTRA_EDGE_NEON_H_

#include "src/dsp/cpu.h"
#include "src/dsp/dsp.h"

namespace libgav1 {
namespace dsp {

// Initializes Dsp::intra_edge_filter and Dsp::intra_edge_upsampler. This
// function is not thread-safe.
void IntraEdgeInit_NEON();

}  // namespace dsp
}  // namespace libgav1

#if LIBGAV1_ENABLE_NEON
#define LIBGAV1_Dsp8bpp_IntraEdgeFilter LIBGAV1_DSP_NEON
#define LIBGAV1_Dsp8bpp_IntraEdgeUpsampler LIBGAV1_DSP_NEON

#endif  // LIBGAV1_ENABLE_NEON

#endif  // LIBGAV1_SRC_DSP_ARM_INTRA_EDGE_NEON_H_
