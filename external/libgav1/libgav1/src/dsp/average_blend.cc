#include "src/dsp/average_blend.h"

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "src/dsp/dsp.h"
#include "src/utils/common.h"

namespace libgav1 {
namespace dsp {
namespace {

template <int bitdepth, typename Pixel>
void AverageBlend_C(const uint16_t* prediction_0,
                    const ptrdiff_t prediction_stride_0,
                    const uint16_t* prediction_1,
                    const ptrdiff_t prediction_stride_1, const int width,
                    const int height, void* const dest,
                    const ptrdiff_t dest_stride) {
  // An offset to cancel offsets used in compound predictor generation that
  // make intermediate computations non negative.
  constexpr int compound_round_offset =
      (2 << (bitdepth + 4)) + (2 << (bitdepth + 3));
  // 7.11.3.2 Rounding variables derivation process
  //   2 * FILTER_BITS(7) - (InterRound0(3|5) + InterRound1(7))
  constexpr int inter_post_round_bits = (bitdepth == 12) ? 2 : 4;
  auto* dst = static_cast<Pixel*>(dest);
  const ptrdiff_t dst_stride = dest_stride / sizeof(Pixel);

  int y = 0;
  do {
    int x = 0;
    do {
      // prediction range: 8bpp: [0, 15471] 10bpp: [0, 61983] 12bpp: [0, 62007].
      int res = prediction_0[x] + prediction_1[x];
      res -= compound_round_offset;
      dst[x] = static_cast<Pixel>(
          Clip3(RightShiftWithRounding(res, inter_post_round_bits + 1), 0,
                (1 << bitdepth) - 1));
    } while (++x < width);

    dst += dst_stride;
    prediction_0 += prediction_stride_0;
    prediction_1 += prediction_stride_1;
  } while (++y < height);
}

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(8);
  assert(dsp != nullptr);
#if LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  dsp->average_blend = AverageBlend_C<8, uint8_t>;
#else  // !LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  static_cast<void>(dsp);
#ifndef LIBGAV1_Dsp8bpp_AverageBlend
  dsp->average_blend = AverageBlend_C<8, uint8_t>;
#endif
#endif  // LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
}

#if LIBGAV1_MAX_BITDEPTH >= 10
void Init10bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(10);
  assert(dsp != nullptr);
#if LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
#ifndef LIBGAV1_Dsp10bpp_AverageBlend
  dsp->average_blend = AverageBlend_C<10, uint16_t>;
#endif
#else  // !LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  static_cast<void>(dsp);
#ifndef LIBGAV1_Dsp10bpp_AverageBlend
  dsp->average_blend = AverageBlend_C<10, uint16_t>;
#endif
#endif  // LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
}
#endif

}  // namespace

void AverageBlendInit_C() {
  Init8bpp();
#if LIBGAV1_MAX_BITDEPTH >= 10
  Init10bpp();
#endif
}

}  // namespace dsp
}  // namespace libgav1
