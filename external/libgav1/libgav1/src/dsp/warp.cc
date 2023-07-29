#include "src/dsp/warp.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include "src/dsp/constants.h"
#include "src/dsp/dsp.h"
#include "src/utils/common.h"
#include "src/utils/constants.h"
#include "src/utils/memory.h"

namespace libgav1 {
namespace dsp {
namespace {

// Number of extra bits of precision in warped filtering.
constexpr int kWarpedDiffPrecisionBits = 10;

template <int bitdepth, typename Pixel>
void Warp_C(const void* const source, ptrdiff_t source_stride,
            const int source_width, const int source_height,
            const int* const warp_params, const int subsampling_x,
            const int subsampling_y, const int inter_round_bits_vertical,
            const int block_start_x, const int block_start_y,
            const int block_width, const int block_height, const int16_t alpha,
            const int16_t beta, const int16_t gamma, const int16_t delta,
            uint16_t* dest, const ptrdiff_t dest_stride) {
  constexpr int kRoundBitsHorizontal = (bitdepth == 12)
                                           ? kInterRoundBitsHorizontal12bpp
                                           : kInterRoundBitsHorizontal;
  // Intermediate_result is the output of the horizontal filtering and rounding.
  // The range is within 16 bits (unsigned).
  uint16_t intermediate_result[15][8];  // 15 rows, 8 columns.
  const int horizontal_offset = 1 << (bitdepth + kFilterBits - 1);
  const int vertical_offset =
      1 << (bitdepth + 2 * kFilterBits - kRoundBitsHorizontal);
  const auto* const src = static_cast<const Pixel*>(source);
  source_stride /= sizeof(Pixel);

  assert(block_width >= 8);
  assert(block_height >= 8);

  // Warp process applies for each 8x8 block (or smaller).
  for (int start_y = block_start_y; start_y < block_start_y + block_height;
       start_y += 8) {
    for (int start_x = block_start_x; start_x < block_start_x + block_width;
         start_x += 8) {
      const int src_x = (start_x + 4) << subsampling_x;
      const int src_y = (start_y + 4) << subsampling_y;
      const int dst_x =
          src_x * warp_params[2] + src_y * warp_params[3] + warp_params[0];
      const int dst_y =
          src_x * warp_params[4] + src_y * warp_params[5] + warp_params[1];
      const int x4 = dst_x >> subsampling_x;
      const int y4 = dst_y >> subsampling_y;
      const int ix4 = x4 >> kWarpedModelPrecisionBits;
      const int iy4 = y4 >> kWarpedModelPrecisionBits;

      // Horizontal filter.
      int sx4 = (x4 & ((1 << kWarpedModelPrecisionBits) - 1)) - beta * 7;
      for (int y = -7; y < 8; ++y) {
        // TODO(chengchen):
        // Because of warping, the index could be out of frame boundary. Thus
        // clip is needed. However, can we remove or reduce usage of clip?
        // Besides, special cases exist, for example,
        // if iy4 - 7 >= source_height or iy4 + 7 < 0, there's no need to do the
        // filtering.
        const int row = Clip3(iy4 + y, 0, source_height - 1);
        const Pixel* const src_row = src + row * source_stride;
        // Check for two simple special cases.
        if (ix4 - 7 >= source_width - 1) {
          // Every sample is equal to src_row[source_width - 1]. Since the sum
          // of the warped filter coefficients is 128 (= 2^7), the filtering is
          // equivalent to multiplying src_row[source_width - 1] by 128.
          const int s =
              (horizontal_offset >> kInterRoundBitsHorizontal) +
              (src_row[source_width - 1] << (7 - kInterRoundBitsHorizontal));
          Memset(intermediate_result[y + 7], s, 8);
          sx4 += beta;
          continue;
        }
        if (ix4 + 7 <= 0) {
          // Every sample is equal to src_row[0]. Since the sum of the warped
          // filter coefficients is 128 (= 2^7), the filtering is equivalent to
          // multiplying src_row[0] by 128.
          const int s = (horizontal_offset >> kInterRoundBitsHorizontal) +
                        (src_row[0] << (7 - kInterRoundBitsHorizontal));
          Memset(intermediate_result[y + 7], s, 8);
          sx4 += beta;
          continue;
        }
        // At this point, we know ix4 - 7 < source_width - 1 and ix4 + 7 > 0.
        // It follows that -6 <= ix4 <= source_width + 5. This inequality is
        // used below.
        int sx = sx4 - MultiplyBy4(alpha);
        for (int x = -4; x < 4; ++x) {
          const int offset =
              RightShiftWithRounding(sx, kWarpedDiffPrecisionBits) +
              kWarpedPixelPrecisionShifts;
          // Since alpha and beta have been validated by SetupShear(), one can
          // prove that 0 <= offset <= 3 * 2^6.
          assert(offset >= 0);
          assert(offset < 3 * kWarpedPixelPrecisionShifts + 1);
          // For SIMD optimization:
          // For 8 bit, the range of sum is within uint16_t, if we add an
          // horizontal offset:
          int sum = horizontal_offset;
          // Horizontal_offset guarantees sum is non negative.
          // If horizontal_offset is used, intermediate_result needs to be
          // uint16_t.
          // For 10/12 bit, the range of sum is within 32 bits.
          for (int k = 0; k < 8; ++k) {
            // We assume the source frame has left and right borders of at
            // least 13 pixels that extend the frame boundary pixels.
            //
            // Since -4 <= x <= 3 and 0 <= k <= 7, using the inequality on ix4
            // above, we have -13 <= ix4 + x + k - 3 <= source_width + 12, or
            // -13 <= column <= (source_width - 1) + 13. Therefore we may
            // over-read up to 13 pixels before the source row, or up to 13
            // pixels after the source row.
            const int column = ix4 + x + k - 3;
            sum += kWarpedFilters[offset][k] * src_row[column];
          }
          assert(sum >= 0 && sum < (horizontal_offset << 2));
          intermediate_result[y + 7][x + 4] = static_cast<uint16_t>(
              RightShiftWithRounding(sum, kRoundBitsHorizontal));
          sx += alpha;
        }
        sx4 += beta;
      }

      // Vertical filter.
      uint16_t* dst_row = dest + start_x - block_start_x;
      int sy4 =
          (y4 & ((1 << kWarpedModelPrecisionBits) - 1)) - MultiplyBy4(delta);
      // The spec says we should use the following loop condition:
      //   y < std::min(4, block_start_y + block_height - start_y - 4);
      // We can prove that block_start_y + block_height - start_y >= 8, which
      // implies std::min(4, block_start_y + block_height - start_y - 4) = 4.
      // So the loop condition is simply y < 4.
      //
      // Proof:
      //    start_y < block_start_y + block_height
      // => block_start_y + block_height - start_y > 0
      // => block_height - (start_y - block_start_y) > 0
      //
      // Since block_height >= 8 and is a power of 2, it follows that
      // block_height is a multiple of 8. start_y - block_start_y is also a
      // multiple of 8. Therefore their difference is a multiple of 8. Since
      // their difference is > 0, their difference must be >= 8.
      for (int y = -4; y < 4; ++y) {
        int sy = sy4 - MultiplyBy4(gamma);
        // The spec says we should use the following loop condition:
        //   x < std::min(4, block_start_x + block_width - start_x - 4);
        // Similar to the above, we can prove that the loop condition can be
        // simplified to x < 4.
        for (int x = -4; x < 4; ++x) {
          const int offset =
              RightShiftWithRounding(sy, kWarpedDiffPrecisionBits) +
              kWarpedPixelPrecisionShifts;
          // Since gamma and delta have been validated by SetupShear(), one can
          // prove that 0 <= offset <= 3 * 2^6.
          assert(offset >= 0);
          assert(offset < 3 * kWarpedPixelPrecisionShifts + 1);
          // Similar to horizontal_offset, vertical_offset guarantees sum
          // before shifting is non negative:
          int sum = vertical_offset;
          for (int k = 0; k < 8; ++k) {
            sum += kWarpedFilters[offset][k] *
                   intermediate_result[y + 4 + k][x + 4];
          }
          assert(sum >= 0 && sum < (vertical_offset << 2));
          sum = RightShiftWithRounding(sum, inter_round_bits_vertical);
          // Warp output is a predictor, whose type is uint16_t.
          // Do not clip it here. The clipping is applied at the stage of
          // final pixel value output.
          dst_row[x + 4] = static_cast<uint16_t>(sum);
          sy += gamma;
        }
        dst_row += dest_stride;
        sy4 += delta;
      }
    }
    dest += 8 * dest_stride;
  }
}

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(8);
  assert(dsp != nullptr);
#if LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  dsp->warp = Warp_C<8, uint8_t>;
#else  // !LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  static_cast<void>(dsp);
#ifndef LIBGAV1_Dsp8bpp_Warp
  dsp->warp = Warp_C<8, uint8_t>;
#endif
#endif  // LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
}

#if LIBGAV1_MAX_BITDEPTH >= 10
void Init10bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(10);
  assert(dsp != nullptr);
#if LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  dsp->warp = Warp_C<10, uint16_t>;
#else  // !LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  static_cast<void>(dsp);
#ifndef LIBGAV1_Dsp10bpp_Warp
  dsp->warp = Warp_C<10, uint16_t>;
#endif
#endif  // LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
}
#endif

}  // namespace

void WarpInit_C() {
  Init8bpp();
#if LIBGAV1_MAX_BITDEPTH >= 10
  Init10bpp();
#endif
}

}  // namespace dsp
}  // namespace libgav1
