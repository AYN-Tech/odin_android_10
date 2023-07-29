#ifndef LIBGAV1_SRC_DSP_INTRA_EDGE_H_
#define LIBGAV1_SRC_DSP_INTRA_EDGE_H_

// Pull in LIBGAV1_DspXXX defines representing the implementation status
// of each function. The resulting value of each can be used by each module to
// determine whether an implementation is needed at compile time.
// IWYU pragma: begin_exports

// ARM:
#include "src/dsp/arm/intra_edge_neon.h"

// x86:
// Note includes should be sorted in logical order avx2/avx/sse4, etc.
// The order of includes is important as each tests for a superior version
// before setting the base.
// clang-format off
#include "src/dsp/x86/intra_edge_sse4.h"
// clang-format on

// IWYU pragma: end_exports

namespace libgav1 {
namespace dsp {

// Initializes Dsp::intra_edge_filter and Dsp::intra_edge_upsampler. This
// function is not thread-safe.
void IntraEdgeInit_C();

}  // namespace dsp
}  // namespace libgav1

#endif  // LIBGAV1_SRC_DSP_INTRA_EDGE_H_
