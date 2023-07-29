#include "src/dsp/cdef.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "src/dsp/dsp.h"
#include "src/utils/common.h"

namespace libgav1 {
namespace dsp {
namespace {

constexpr uint16_t kCdefLargeValue = 30000;

constexpr int16_t kDivisionTable[] = {0,   840, 420, 280, 210,
                                      168, 140, 120, 105};

constexpr uint8_t kPrimaryTaps[2][2] = {{4, 2}, {3, 3}};

constexpr uint8_t kSecondaryTaps[2][2] = {{2, 1}, {2, 1}};

constexpr int8_t kCdefDirections[8][2][2] = {
    {{-1, 1}, {-2, 2}}, {{0, 1}, {-1, 2}}, {{0, 1}, {0, 2}}, {{0, 1}, {1, 2}},
    {{1, 1}, {2, 2}},   {{1, 0}, {2, 1}},  {{1, 0}, {2, 0}}, {{1, 0}, {2, -1}}};

int Constrain(int diff, int threshold, int damping) {
  if (threshold == 0) return 0;
  damping = std::max(0, damping - FloorLog2(threshold));
  const int sign = (diff < 0) ? -1 : 1;
  return sign *
         Clip3(threshold - (std::abs(diff) >> damping), 0, std::abs(diff));
}

int32_t Square(int32_t x) { return x * x; }

template <int bitdepth, typename Pixel>
void CdefDirection_C(const void* const source, ptrdiff_t stride,
                     int* const direction, int* const variance) {
  assert(direction != nullptr);
  assert(variance != nullptr);
  const auto* src = static_cast<const Pixel*>(source);
  stride /= sizeof(Pixel);
  int32_t cost[8] = {};
  // |partial| does not have to be int32_t for 8bpp. int16_t will suffice. We
  // use int32_t to keep it simple since |cost| will have to be int32_t.
  int32_t partial[8][15] = {};
  for (int i = 0; i < 8; ++i) {
    for (int j = 0; j < 8; ++j) {
      const int x = (src[j] >> (bitdepth - 8)) - 128;
      partial[0][i + j] += x;
      partial[1][i + j / 2] += x;
      partial[2][i] += x;
      partial[3][3 + i - j / 2] += x;
      partial[4][7 + i - j] += x;
      partial[5][3 - i / 2 + j] += x;
      partial[6][j] += x;
      partial[7][i / 2 + j] += x;
    }
    src += stride;
  }
  for (int i = 0; i < 8; ++i) {
    cost[2] += Square(partial[2][i]);
    cost[6] += Square(partial[6][i]);
  }
  cost[2] *= kDivisionTable[8];
  cost[6] *= kDivisionTable[8];
  for (int i = 0; i < 7; ++i) {
    cost[0] += (Square(partial[0][i]) + Square(partial[0][14 - i])) *
               kDivisionTable[i + 1];
    cost[4] += (Square(partial[4][i]) + Square(partial[4][14 - i])) *
               kDivisionTable[i + 1];
  }
  cost[0] += Square(partial[0][7]) * kDivisionTable[8];
  cost[4] += Square(partial[4][7]) * kDivisionTable[8];
  for (int i = 1; i < 8; i += 2) {
    for (int j = 0; j < 5; ++j) {
      cost[i] += Square(partial[i][3 + j]);
    }
    cost[i] *= kDivisionTable[8];
    for (int j = 0; j < 3; ++j) {
      cost[i] += (Square(partial[i][j]) + Square(partial[i][10 - j])) *
                 kDivisionTable[2 * j + 2];
    }
  }
  int32_t best_cost = 0;
  *direction = 0;
  for (int i = 0; i < 8; ++i) {
    if (cost[i] > best_cost) {
      best_cost = cost[i];
      *direction = i;
    }
  }
  *variance = (best_cost - cost[(*direction + 4) & 7]) >> 10;
}

// Filters the source block. It doesn't check whether the candidate pixel is
// inside the frame. However it requires the source input to be padded with a
// constant large value if at the boundary. And the input should be uint16_t.
template <int bitdepth, typename Pixel>
void CdefFilter_C(const void* const source, const ptrdiff_t source_stride,
                  const int rows4x4, const int columns4x4, const int curr_x,
                  const int curr_y, const int subsampling_x,
                  const int subsampling_y, const int primary_strength,
                  const int secondary_strength, const int damping,
                  const int direction, void* const dest,
                  const ptrdiff_t dest_stride) {
  const int coeff_shift = bitdepth - 8;
  const int plane_width = MultiplyBy4(columns4x4) >> subsampling_x;
  const int plane_height = MultiplyBy4(rows4x4) >> subsampling_y;
  const int block_width = std::min(8 >> subsampling_x, plane_width - curr_x);
  const int block_height = std::min(8 >> subsampling_y, plane_height - curr_y);
  const auto* src = static_cast<const uint16_t*>(source);
  auto* dst = static_cast<Pixel*>(dest);
  const ptrdiff_t dst_stride = dest_stride / sizeof(Pixel);
  int y = 0;
  do {
    int x = 0;
    do {
      int16_t sum = 0;
      const uint16_t pixel_value = src[x];
      uint16_t max_value = pixel_value;
      uint16_t min_value = pixel_value;
      for (int k = 0; k < 2; ++k) {
        const int signs[] = {-1, 1};
        for (const int& sign : signs) {
          int dy = sign * kCdefDirections[direction][k][0];
          int dx = sign * kCdefDirections[direction][k][1];
          uint16_t value = src[dy * source_stride + dx + x];
          // Note: the summation can ignore the condition check in SIMD
          // implementation, because Constrain() will return 0 when
          // value == kCdefLargeValue.
          if (value != kCdefLargeValue) {
            sum += Constrain(value - pixel_value, primary_strength, damping) *
                   kPrimaryTaps[(primary_strength >> coeff_shift) & 1][k];
            max_value = std::max(value, max_value);
            min_value = std::min(value, min_value);
          }
          const int offsets[] = {-2, 2};
          for (const int& offset : offsets) {
            dy = sign * kCdefDirections[(direction + offset) & 7][k][0];
            dx = sign * kCdefDirections[(direction + offset) & 7][k][1];
            value = src[dy * source_stride + dx + x];
            // Note: the summation can ignore the condition check in SIMD
            // implementation.
            if (value != kCdefLargeValue) {
              sum +=
                  Constrain(value - pixel_value, secondary_strength, damping) *
                  kSecondaryTaps[(primary_strength >> coeff_shift) & 1][k];
              max_value = std::max(value, max_value);
              min_value = std::min(value, min_value);
            }
          }
        }
      }

      dst[x] = static_cast<Pixel>(Clip3(
          pixel_value + ((8 + sum - (sum < 0)) >> 4), min_value, max_value));
    } while (++x < block_width);

    src += source_stride;
    dst += dst_stride;
  } while (++y < block_height);
}

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(8);
  assert(dsp != nullptr);
#if LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  dsp->cdef_direction = CdefDirection_C<8, uint8_t>;
  dsp->cdef_filter = CdefFilter_C<8, uint8_t>;
#else  // !LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  static_cast<void>(dsp);
#ifndef LIBGAV1_Dsp8bpp_CdefDirection
  dsp->cdef_direction = CdefDirection_C<8, uint8_t>;
#endif
#ifndef LIBGAV1_Dsp8bpp_CdefFilter
  dsp->cdef_filter = CdefFilter_C<8, uint8_t>;
#endif
#endif  // LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
}

#if LIBGAV1_MAX_BITDEPTH >= 10
void Init10bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(10);
  assert(dsp != nullptr);
#if LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  dsp->cdef_direction = CdefDirection_C<10, uint16_t>;
  dsp->cdef_filter = CdefFilter_C<10, uint16_t>;
#else  // !LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  static_cast<void>(dsp);
#ifndef LIBGAV1_Dsp10bpp_CdefDirection
  dsp->cdef_direction = CdefDirection_C<10, uint16_t>;
#endif
#ifndef LIBGAV1_Dsp10bpp_CdefFilter
  dsp->cdef_filter = CdefFilter_C<10, uint16_t>;
#endif
#endif  // LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
}
#endif

}  // namespace

void CdefInit_C() {
  Init8bpp();
#if LIBGAV1_MAX_BITDEPTH >= 10
  Init10bpp();
#endif
}

}  // namespace dsp
}  // namespace libgav1
