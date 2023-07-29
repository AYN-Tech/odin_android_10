#ifndef LIBGAV1_SRC_PREDICTION_MASK_H_
#define LIBGAV1_SRC_PREDICTION_MASK_H_

#include <cstddef>
#include <cstdint>

#include "src/utils/bit_mask_set.h"
#include "src/utils/types.h"

namespace libgav1 {

constexpr BitMaskSet kIsWedgeCompoundModeAllowed(kBlock8x8, kBlock8x16,
                                                 kBlock8x32, kBlock16x8,
                                                 kBlock16x16, kBlock16x32,
                                                 kBlock32x8, kBlock32x16,
                                                 kBlock32x32);

// This function generates wedge masks. It should be called only once for the
// decoder. If the video is key frame only, we don't have to call this
// function.
// 7.11.3.11.
void GenerateWedgeMask(uint8_t* wedge_master_mask_data,
                       uint8_t* wedge_masks_data);

// 7.11.3.12.
void GenerateWeightMask(const uint16_t* prediction_1, ptrdiff_t stride_1,
                        const uint16_t* prediction_2, ptrdiff_t stride_2,
                        bool mask_is_inverse, int width, int height,
                        int bitdepth, uint8_t* mask, ptrdiff_t mask_stride);

// 7.11.3.13.
void GenerateInterIntraMask(int mode, int width, int height, uint8_t* mask,
                            ptrdiff_t mask_stride);

}  // namespace libgav1
#endif  // LIBGAV1_SRC_PREDICTION_MASK_H_
