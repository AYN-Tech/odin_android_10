#ifndef LIBGAV1_SRC_RECONSTRUCTION_H_
#define LIBGAV1_SRC_RECONSTRUCTION_H_

#include <cstdint>

#include "src/dsp/constants.h"
#include "src/dsp/dsp.h"
#include "src/utils/array_2d.h"
#include "src/utils/constants.h"
#include "src/utils/types.h"

namespace libgav1 {

// Steps 2 and 3 of section 7.12.3 (contains the implementation of section
// 7.13.3).
// Apply the inverse transforms and add the residual to the frame for the
// transform block size |tx_size| starting at position |start_x| and |start_y|.
template <typename Residual, typename Pixel>
void Reconstruct(const dsp::Dsp& dsp, TransformType tx_type,
                 TransformSize tx_size, bool lossless, Residual* buffer,
                 int start_x, int start_y, Array2DView<Pixel>* frame,
                 int16_t non_zero_coeff_count);

extern template void Reconstruct(const dsp::Dsp& dsp, TransformType tx_type,
                                 TransformSize tx_size, bool lossless,
                                 int16_t* buffer, int start_x, int start_y,
                                 Array2DView<uint8_t>* frame,
                                 int16_t non_zero_coeff_count);
#if LIBGAV1_MAX_BITDEPTH >= 10
extern template void Reconstruct(const dsp::Dsp& dsp, TransformType tx_type,
                                 TransformSize tx_size, bool lossless,
                                 int32_t* buffer, int start_x, int start_y,
                                 Array2DView<uint16_t>* frame,
                                 int16_t non_zero_coeff_count);
#endif

}  // namespace libgav1
#endif  // LIBGAV1_SRC_RECONSTRUCTION_H_
