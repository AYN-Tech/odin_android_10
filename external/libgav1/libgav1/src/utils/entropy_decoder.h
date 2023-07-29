#ifndef LIBGAV1_SRC_UTILS_ENTROPY_DECODER_H_
#define LIBGAV1_SRC_UTILS_ENTROPY_DECODER_H_

#include <cstddef>
#include <cstdint>

#include "src/utils/bit_reader.h"

namespace libgav1 {

class DaalaBitReader : public BitReader {
 public:
  DaalaBitReader(const uint8_t* data, size_t size, bool allow_update_cdf);
  ~DaalaBitReader() override = default;

  // Move only.
  DaalaBitReader(DaalaBitReader&& rhs) noexcept;
  DaalaBitReader& operator=(DaalaBitReader&& rhs) noexcept;

  int ReadBit() override;
  int64_t ReadLiteral(int num_bits) override;
  // ReadSymbol() calls for which the |symbol_count| is only known at runtime
  // will use this variant.
  int ReadSymbol(uint16_t* cdf, int symbol_count);
  // ReadSymbol() calls for which the |symbol_count| is equal to 2 (boolean
  // symbols) will use this variant.
  bool ReadSymbol(uint16_t* cdf);
  bool ReadSymbolWithoutCdfUpdate(uint16_t* cdf);
  // Use either linear search or binary search for decoding the symbol depending
  // on |symbol_count|. ReadSymbol calls for which the |symbol_count| is known
  // at compile time will use this variant.
  template <int symbol_count>
  int ReadSymbol(uint16_t* cdf);

 private:
  using WindowSize = uint32_t;
  static constexpr uint32_t kWindowSize =
      static_cast<uint32_t>(sizeof(WindowSize)) * 8;

  // Reads a symbol using the |cdf| table which contains the probabilities of
  // each symbol. On a high level, this function does the following:
  //   1) Scale the |cdf| values.
  //   2) Find the index in the |cdf| array where the scaled CDF value crosses
  //   the modified |window_diff_| threshold.
  //   3) That index is the symbol that has been decoded.
  //   4) Update |window_diff_| and |values_in_range_| based on the symbol that
  //   has been decoded.
  int ReadSymbolImpl(const uint16_t* cdf, int symbol_count);
  // Similar to ReadSymbolImpl but it uses binary search to perform step 2 in
  // the comment above. As of now, this function is called when |symbol_count|
  // is greater than or equal to 8.
  int ReadSymbolImplBinarySearch(const uint16_t* cdf, int symbol_count);
  // Specialized implementation of ReadSymbolImpl based on the fact that
  // symbol_count == 2.
  int ReadSymbolImpl(const uint16_t* cdf);
  void PopulateBits();
  // Normalizes the range so that 32768 <= |values_in_range_| < 65536. Also
  // calls PopulateBits() if necessary.
  void NormalizeRange();

  const uint8_t* data_;
  const size_t size_;
  size_t data_index_;
  const bool allow_update_cdf_;
  // Number of bits of data in the current value.
  int bits_;
  // Number of values in the current range.
  uint16_t values_in_range_;
  // The difference between the high end of the current range and the coded
  // value minus 1. The 16 least significant bits of this variable is used to
  // decode the next symbol. It is filled in whenever |bits_| is less than 0.
  WindowSize window_diff_;
};

}  // namespace libgav1

#endif  // LIBGAV1_SRC_UTILS_ENTROPY_DECODER_H_
