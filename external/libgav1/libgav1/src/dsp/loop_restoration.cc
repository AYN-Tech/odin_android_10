#include "src/dsp/loop_restoration.h"

#include <algorithm>  // std::max
#include <cassert>
#include <cstddef>
#include <cstdint>

#include "src/dsp/common.h"
#include "src/dsp/dsp.h"
#include "src/utils/common.h"
#include "src/utils/constants.h"

namespace libgav1 {
namespace dsp {
namespace {

// Precision of a division table (mtable)
constexpr int kSgrProjScaleBits = 20;
constexpr int kSgrProjReciprocalBits = 12;
// Core selfguided restoration precision bits.
constexpr int kSgrProjSgrBits = 8;
// Precision bits of generated values higher than source before projection.
constexpr int kSgrProjRestoreBits = 4;

// Section 7.17.3.
// a2: range [1, 256].
// if (z >= 255)
//   a2 = 256;
// else if (z == 0)
//   a2 = 1;
// else
//   a2 = ((z << kSgrProjSgrBits) + (z >> 1)) / (z + 1);
constexpr int kXByXPlus1[256] = {
    1,   128, 171, 192, 205, 213, 219, 224, 228, 230, 233, 235, 236, 238, 239,
    240, 241, 242, 243, 243, 244, 244, 245, 245, 246, 246, 247, 247, 247, 247,
    248, 248, 248, 248, 249, 249, 249, 249, 249, 250, 250, 250, 250, 250, 250,
    250, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 252, 252, 252, 252,
    252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 253, 253,
    253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253,
    253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 254, 254, 254,
    254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
    254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
    254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
    254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
    254, 254, 254, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    256};

constexpr int kOneByX[25] = {
    4096, 2048, 1365, 1024, 819, 683, 585, 512, 455, 410, 372, 341, 315,
    293,  273,  256,  241,  228, 216, 205, 195, 186, 178, 171, 164,
};

template <int bitdepth, typename Pixel>
struct LoopRestorationFuncs_C {
  LoopRestorationFuncs_C() = delete;

  // |stride| for SelfGuidedFilter and WienerFilter is given in bytes.
  static void SelfGuidedFilter(const void* source, void* dest,
                               const RestorationUnitInfo& restoration_info,
                               ptrdiff_t source_stride, ptrdiff_t dest_stride,
                               int width, int height,
                               RestorationBuffer* buffer);
  static void WienerFilter(const void* source, void* dest,
                           const RestorationUnitInfo& restoration_info,
                           ptrdiff_t source_stride, ptrdiff_t dest_stride,
                           int width, int height, RestorationBuffer* buffer);
  // |stride| for box filter processing is in Pixels.
  static void BoxFilterPreProcess(const RestorationUnitInfo& restoration_info,
                                  const Pixel* src, ptrdiff_t stride, int width,
                                  int height, int pass,
                                  RestorationBuffer* buffer);
  static void BoxFilterProcess(const RestorationUnitInfo& restoration_info,
                               const Pixel* src, ptrdiff_t stride, int width,
                               int height, RestorationBuffer* buffer);
};

// Note: range of wiener filter coefficients.
// Wiener filter coefficients are symmetric, and their sum is 1 (128).
// The range of each coefficient:
// filter[0] = filter[6], 4 bits, min = -5, max = 10.
// filter[1] = filter[5], 5 bits, min = -23, max = 8.
// filter[2] = filter[4], 6 bits, min = -17, max = 46.
// filter[3] = 128 - (filter[0] + filter[1] + filter[2]) * 2.
// The difference from libaom is that in libaom:
// filter[3] = 0 - (filter[0] + filter[1] + filter[2]) * 2.
// Thus in libaom's computation, an offset of 128 is needed for filter[3].
inline void PopulateWienerCoefficients(
    const RestorationUnitInfo& restoration_info, int direction,
    int16_t* const filter) {
  filter[3] = 128;
  for (int i = 0; i < 3; ++i) {
    const int16_t coeff = restoration_info.wiener_info.filter[direction][i];
    filter[i] = coeff;
    filter[6 - i] = coeff;
    filter[3] -= MultiplyBy2(coeff);
  }
}

// Note: bit range for wiener filter.
// Wiener filter process first applies horizontal filtering to input pixels,
// followed by rounding with predefined bits (dependent on bitdepth).
// Then vertical filtering is applied, followed by rounding (dependent on
// bitdepth).
// The process is the same as convolution:
// <input> --> <horizontal filter> --> <rounding 0> --> <vertical filter>
// --> <rounding 1>
// By design:
// (a). horizontal/vertical filtering adds 7 bits to input.
// (b). The output of first rounding fits into 16 bits.
// (c). The output of second rounding fits into 16 bits.
// If input bitdepth > 8, the accumulator of the horizontal filter is larger
// than 16 bit and smaller than 32 bits.
// The accumulator of the vertical filter is larger than 16 bits and smaller
// than 32 bits.
template <int bitdepth, typename Pixel>
void LoopRestorationFuncs_C<bitdepth, Pixel>::WienerFilter(
    const void* const source, void* const dest,
    const RestorationUnitInfo& restoration_info, ptrdiff_t source_stride,
    ptrdiff_t dest_stride, int width, int height,
    RestorationBuffer* const buffer) {
  constexpr int kRoundBitsHorizontal = (bitdepth == 12)
                                           ? kInterRoundBitsHorizontal12bpp
                                           : kInterRoundBitsHorizontal;
  constexpr int kRoundBitsVertical =
      (bitdepth == 12) ? kInterRoundBitsVertical12bpp : kInterRoundBitsVertical;
  int16_t filter[kSubPixelTaps - 1];
  const int limit =
      (1 << (bitdepth + 1 + kWienerFilterBits - kRoundBitsHorizontal)) - 1;
  const auto* src = static_cast<const Pixel*>(source);
  auto* dst = static_cast<Pixel*>(dest);
  source_stride /= sizeof(Pixel);
  dest_stride /= sizeof(Pixel);
  const ptrdiff_t buffer_stride = buffer->wiener_buffer_stride;
  auto* wiener_buffer = buffer->wiener_buffer;
  // horizontal filtering.
  PopulateWienerCoefficients(restoration_info, WienerInfo::kHorizontal, filter);
  const int center_tap = 3;
  src -= center_tap * source_stride + center_tap;
  const int horizontal_rounding = 1 << (bitdepth + kWienerFilterBits - 1);
  for (int y = 0; y < height + kSubPixelTaps - 2; ++y) {
    for (int x = 0; x < width; ++x) {
      // sum fits into 16 bits only when bitdepth = 8.
      int sum = horizontal_rounding;
      for (int k = 0; k < kSubPixelTaps - 1; ++k) {
        sum += filter[k] * src[x + k];
      }
      const int rounded_sum = RightShiftWithRounding(sum, kRoundBitsHorizontal);
      wiener_buffer[x] = static_cast<uint16_t>(Clip3(rounded_sum, 0, limit));
    }
    src += source_stride;
    wiener_buffer += buffer_stride;
  }
  wiener_buffer = buffer->wiener_buffer;
  // vertical filtering.
  PopulateWienerCoefficients(restoration_info, WienerInfo::kVertical, filter);
  const int vertical_rounding = -(1 << (bitdepth + kRoundBitsVertical - 1));
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      // sum needs 32 bits.
      int sum = vertical_rounding;
      for (int k = 0; k < kSubPixelTaps - 1; ++k) {
        sum += filter[k] * wiener_buffer[k * buffer_stride + x];
      }
      const int rounded_sum = RightShiftWithRounding(sum, kRoundBitsVertical);
      dst[x] = static_cast<Pixel>(Clip3(rounded_sum, 0, (1 << bitdepth) - 1));
    }
    dst += dest_stride;
    wiener_buffer += buffer_stride;
  }
}

template <int bitdepth, typename Pixel>
void LoopRestorationFuncs_C<bitdepth, Pixel>::BoxFilterPreProcess(
    const RestorationUnitInfo& restoration_info, const Pixel* const src,
    ptrdiff_t stride, int width, int height, int pass,
    RestorationBuffer* const buffer) {
  const int sgr_proj_index = restoration_info.sgr_proj_info.index;
  const uint8_t radius = kSgrProjParams[sgr_proj_index][pass * 2];
  assert(radius != 0);
  const uint32_t n = (2 * radius + 1) * (2 * radius + 1);
  // const uint8_t scale = kSgrProjParams[sgr_proj_index][pass * 2 + 1];
  // n2_with_scale: max value < 2^16. min value is 4.
  // const uint32_t n2_with_scale = n * n * scale;
  // s: max value < 2^12.
  // const uint32_t s =
  // ((1 << kSgrProjScaleBits) + (n2_with_scale >> 1)) / n2_with_scale;
  const uint32_t s = kSgrScaleParameter[sgr_proj_index][pass];
  assert(s != 0);
  const ptrdiff_t array_stride = buffer->box_filter_process_intermediate_stride;
  // The size of the intermediate result buffer is the size of the filter area
  // plus horizontal (3) and vertical (3) padding. The processing start point
  // is the filter area start point -1 row and -1 column. Therefore we need to
  // set offset and use the intermediate_result as the start point for
  // processing.
  const ptrdiff_t intermediate_buffer_offset =
      kRestorationBorder * array_stride + kRestorationBorder;
  uint32_t* intermediate_result[2] = {
      buffer->box_filter_process_intermediate[0] + intermediate_buffer_offset -
          array_stride,
      buffer->box_filter_process_intermediate[1] + intermediate_buffer_offset -
          array_stride};
  // Calculate intermediate results, including one-pixel border, for example,
  // if unit size is 64x64, we calculate 66x66 pixels.
  for (int y = -1; y <= height; ++y) {
    if (pass == 0 && ((y & 1) == 0)) {
      intermediate_result[0] += array_stride;
      intermediate_result[1] += array_stride;
      continue;
    }
    for (int x = -1; x <= width; ++x) {
      uint32_t a = 0;
      uint32_t b = 0;
      for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
          const Pixel source = src[(y + dy) * stride + (x + dx)];
          // TODO(chengchen): Use boxsum for fast calculation.
          a += source * source;
          b += source;
        }
      }
      // a: before shift, max is 25 * (2^(bitdepth) - 1) * (2^(bitdepth) - 1).
      // since max bitdepth = 12, max < 2^31.
      // after shift, a < 2^16 * n < 2^22 regardless of bitdepth
      a = RightShiftWithRounding(a, (bitdepth - 8) << 1);
      // b: max is 25 * (2^(bitdepth) - 1). If bitdepth = 12, max < 2^19.
      // d < 2^8 * n < 2^14 regardless of bitdepth
      const uint32_t d = RightShiftWithRounding(b, bitdepth - 8);
      // p: Each term in calculating p = a * n - b * b is < 2^16 * n^2 < 2^28,
      // and p itself satisfies p < 2^14 * n^2 < 2^26.
      // This bound on p is due to:
      // https://en.wikipedia.org/wiki/Popoviciu's_inequality_on_variances
      // Note: Sometimes, in high bitdepth, we can end up with a*n < b*b.
      // This is an artifact of rounding, and can only happen if all pixels
      // are (almost) identical, so in this case we saturate to p=0.
      const uint32_t p = (a * n < d * d) ? 0 : a * n - d * d;
      // p * s < (2^14 * n^2) * round(2^20 / (n^2 * scale)) < 2^34 / scale <
      // 2^32 as long as scale >= 4. So p * s fits into a uint32_t, and z < 2^12
      // (this holds even after accounting for the rounding in s)
      const uint32_t z = RightShiftWithRounding(p * s, kSgrProjScaleBits);
      // a2: range [1, 256].
      uint32_t a2 = kXByXPlus1[std::min(z, 255u)];
      const uint32_t one_over_n = kOneByX[n - 1];
      // (kSgrProjSgrBits - a2) < 2^8, b < 2^(bitdepth) * n,
      // one_over_n = round(2^12 / n)
      // => the product here is < 2^(20 + bitdepth) <= 2^32,
      // and b is set to a value < 2^(8 + bitdepth).
      // This holds even with the rounding in one_over_n and in the overall
      // result, as long as (kSgrProjSgrBits - a2) is strictly less than 2^8.
      const uint32_t b2 = ((1 << kSgrProjSgrBits) - a2) * b * one_over_n;
      intermediate_result[0][x] = a2;
      intermediate_result[1][x] =
          RightShiftWithRounding(b2, kSgrProjReciprocalBits);
    }
    intermediate_result[0] += array_stride;
    intermediate_result[1] += array_stride;
  }
}

template <int bitdepth, typename Pixel>
void LoopRestorationFuncs_C<bitdepth, Pixel>::BoxFilterProcess(
    const RestorationUnitInfo& restoration_info, const Pixel* src,
    ptrdiff_t stride, int width, int height, RestorationBuffer* const buffer) {
  const int sgr_proj_index = restoration_info.sgr_proj_info.index;
  for (int pass = 0; pass < 2; ++pass) {
    const uint8_t radius = kSgrProjParams[sgr_proj_index][pass * 2];
    const Pixel* src_ptr = src;
    if (radius == 0) continue;
    LoopRestorationFuncs_C<bitdepth, Pixel>::BoxFilterPreProcess(
        restoration_info, src_ptr, stride, width, height, pass, buffer);

    int* filtered_output = buffer->box_filter_process_output[pass];
    const ptrdiff_t filtered_output_stride =
        buffer->box_filter_process_output_stride;
    const ptrdiff_t intermediate_stride =
        buffer->box_filter_process_intermediate_stride;
    // Set intermediate buffer start point to the actual start point of
    // filtering.
    const ptrdiff_t intermediate_buffer_offset =
        kRestorationBorder * intermediate_stride + kRestorationBorder;
    for (int y = 0; y < height; ++y) {
      const int shift = (pass == 0 && (y & 1) != 0) ? 4 : 5;
      uint32_t* const array_start[2] = {
          buffer->box_filter_process_intermediate[0] +
              intermediate_buffer_offset + y * intermediate_stride,
          buffer->box_filter_process_intermediate[1] +
              intermediate_buffer_offset + y * intermediate_stride};
      for (int x = 0; x < width; ++x) {
        uint32_t a = 0;
        uint32_t b = 0;
        uint32_t* intermediate_result[2] = {
            array_start[0] - intermediate_stride,
            array_start[1] - intermediate_stride};
        for (int dy = -1; dy <= 1; ++dy) {
          for (int dx = -1; dx <= 1; ++dx) {
            int weight;
            if (pass == 0) {
              if (((y + dy) & 1) != 0) {
                weight = (dx == 0) ? 6 : 5;
              } else {
                continue;
              }
            } else {
              weight = ((dx & dy) == 0) ? 4 : 3;
            }
            // intermediate_result[0]: range [1, 256].
            // intermediate_result[1] < 2^20.
            a += weight * intermediate_result[0][x + dx];
            b += weight * intermediate_result[1][x + dx];
          }
          intermediate_result[0] += intermediate_stride;
          intermediate_result[1] += intermediate_stride;
        }
        // v < 2^32. All intermediate calculations are positive.
        const uint32_t v = a * src_ptr[x] + b;
        filtered_output[x] = RightShiftWithRounding(
            v, kSgrProjSgrBits + shift - kSgrProjRestoreBits);
      }
      src_ptr += stride;
      filtered_output += filtered_output_stride;
    }
  }
}

// Assume box_filter_process_output[2] are allocated before calling
// this function. Their sizes are width * height, stride equals width.
template <int bitdepth, typename Pixel>
void LoopRestorationFuncs_C<bitdepth, Pixel>::SelfGuidedFilter(
    const void* const source, void* const dest,
    const RestorationUnitInfo& restoration_info, ptrdiff_t source_stride,
    ptrdiff_t dest_stride, int width, int height,
    RestorationBuffer* const buffer) {
  const int w0 = restoration_info.sgr_proj_info.multiplier[0];
  const int w1 = restoration_info.sgr_proj_info.multiplier[1];
  const int w2 = (1 << kSgrProjPrecisionBits) - w0 - w1;
  const int index = restoration_info.sgr_proj_info.index;
  const uint8_t r0 = kSgrProjParams[index][0];
  const uint8_t r1 = kSgrProjParams[index][2];
  const ptrdiff_t array_stride = buffer->box_filter_process_output_stride;
  int* box_filter_process_output[2] = {buffer->box_filter_process_output[0],
                                       buffer->box_filter_process_output[1]};
  const auto* src = static_cast<const Pixel*>(source);
  auto* dst = static_cast<Pixel*>(dest);
  source_stride /= sizeof(Pixel);
  dest_stride /= sizeof(Pixel);
  LoopRestorationFuncs_C<bitdepth, Pixel>::BoxFilterProcess(
      restoration_info, src, source_stride, width, height, buffer);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const int u = src[x] << kSgrProjRestoreBits;
      int v = w1 * u;
      if (r0 != 0) {
        v += w0 * box_filter_process_output[0][x];
      } else {
        v += w0 * u;
      }
      if (r1 != 0) {
        v += w2 * box_filter_process_output[1][x];
      } else {
        v += w2 * u;
      }
      // if r0 == 0 and r1 == 0, the range of v is:
      // bits(u) + bits(w0/w1/w2) + 2 = bitdepth + 13.
      // Then, range of s is bitdepth + 2. This is a rough estimation, taking
      // the maximum value of each element.
      const int s = RightShiftWithRounding(
          v, kSgrProjRestoreBits + kSgrProjPrecisionBits);
      dst[x] = static_cast<Pixel>(Clip3(s, 0, (1 << bitdepth) - 1));
    }
    src += source_stride;
    dst += dest_stride;
    box_filter_process_output[0] += array_stride;
    box_filter_process_output[1] += array_stride;
  }
}

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(8);
  assert(dsp != nullptr);
#if LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  dsp->loop_restorations[0] = LoopRestorationFuncs_C<8, uint8_t>::WienerFilter;
  dsp->loop_restorations[1] =
      LoopRestorationFuncs_C<8, uint8_t>::SelfGuidedFilter;
#else  // !LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  static_cast<void>(dsp);
#ifndef LIBGAV1_Dsp8bpp_WienerFilter
  dsp->loop_restorations[0] = LoopRestorationFuncs_C<8, uint8_t>::WienerFilter;
#endif
#ifndef LIBGAV1_Dsp8bpp_SelfGuidedFilter
  dsp->loop_restorations[1] =
      LoopRestorationFuncs_C<8, uint8_t>::SelfGuidedFilter;
#endif
#endif  // LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
}

#if LIBGAV1_MAX_BITDEPTH >= 10

void Init10bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(10);
  assert(dsp != nullptr);
#if LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  dsp->loop_restorations[0] =
      LoopRestorationFuncs_C<10, uint16_t>::WienerFilter;
  dsp->loop_restorations[1] =
      LoopRestorationFuncs_C<10, uint16_t>::SelfGuidedFilter;
#else  // !LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  static_cast<void>(dsp);
#ifndef LIBGAV1_Dsp10bpp_WienerFilter
  dsp->loop_restorations[0] =
      LoopRestorationFuncs_C<10, uint16_t>::WienerFilter;
#endif
#ifndef LIBGAV1_Dsp10bpp_SelfGuidedFilter
  dsp->loop_restorations[1] =
      LoopRestorationFuncs_C<10, uint16_t>::SelfGuidedFilter;
#endif
#endif  // LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
}

#endif  // LIBGAV1_MAX_BITDEPTH >= 10
}  // namespace

void LoopRestorationInit_C() {
  Init8bpp();
#if LIBGAV1_MAX_BITDEPTH >= 10
  Init10bpp();
#endif
  // Local functions that may be unused depending on the optimizations
  // available.
  static_cast<void>(PopulateWienerCoefficients);
}

}  // namespace dsp
}  // namespace libgav1
