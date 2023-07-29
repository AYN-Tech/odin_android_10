#include "src/utils/raw_bit_reader.h"

#include <cassert>
#include <limits>

#include "src/utils/common.h"
#include "src/utils/logging.h"

// Note <cinttypes> is only needed when logging is enabled (for the PRI*
// macros). It depends on the definition of LIBGAV1_ENABLE_LOGGING from
// logging.h, thus the non-standard header ordering.
#if LIBGAV1_ENABLE_LOGGING
#include <cinttypes>
#endif

namespace libgav1 {
namespace {

constexpr int kMaximumLeb128Size = 8;
constexpr uint8_t kLeb128ValueByteMask = 0x7f;
constexpr uint8_t kLeb128TerminationByteMask = 0x80;

uint8_t Mod8(size_t n) {
  // Last 3 bits are the value of mod 8.
  return n & 0x07;
}

size_t DivideBy8(size_t n, bool ceil) { return (n + (ceil ? 7 : 0)) >> 3; }

}  // namespace

RawBitReader::RawBitReader(const uint8_t* data, size_t size)
    : data_(data), bit_offset_(0), size_(size) {
  assert(data_ != nullptr || size_ == 0);
}

int RawBitReader::ReadBitImpl() {
  const size_t byte_offset = DivideBy8(bit_offset_, false);
  const uint8_t byte = data_[byte_offset];
  const uint8_t shift = 7 - Mod8(bit_offset_);
  ++bit_offset_;
  return static_cast<int>((byte >> shift) & 0x01);
}

int RawBitReader::ReadBit() {
  if (Finished()) return -1;
  return ReadBitImpl();
}

int64_t RawBitReader::ReadLiteral(int num_bits) {
  assert(num_bits <= 32);
  if (!CanReadLiteral(num_bits)) return -1;
  uint32_t value = 0;
  // We can now call ReadBitImpl() since we've made sure that there are enough
  // bits to be read.
  for (int i = num_bits - 1; i >= 0; --i) {
    value |= static_cast<uint32_t>(ReadBitImpl()) << i;
  }
  return value;
}

bool RawBitReader::ReadInverseSignedLiteral(int num_bits, int* const value) {
  assert(num_bits + 1 < 32);
  *value = static_cast<int>(ReadLiteral(num_bits + 1));
  if (*value == -1) return false;
  const int sign_bit = 1 << num_bits;
  if ((*value & sign_bit) != 0) {
    *value -= 2 * sign_bit;
  }
  return true;
}

bool RawBitReader::ReadLittleEndian(int num_bytes, size_t* const value) {
  // We must be at a byte boundary.
  assert(Mod8(bit_offset_) == 0);
  assert(num_bytes <= 4);
  static_assert(sizeof(size_t) >= 4, "");
  if (value == nullptr) return false;
  size_t byte_offset = DivideBy8(bit_offset_, false);
  if (Finished() || byte_offset + num_bytes > size_) {
    LIBGAV1_DLOG(ERROR, "Not enough bits to read Little Endian value.");
    return false;
  }
  *value = 0;
  for (int i = 0; i < num_bytes; ++i) {
    const size_t byte = data_[byte_offset];
    *value |= (byte << (i * 8));
    ++byte_offset;
  }
  bit_offset_ = byte_offset * 8;
  return true;
}

bool RawBitReader::ReadUnsignedLeb128(size_t* const value) {
  // We must be at a byte boundary.
  assert(Mod8(bit_offset_) == 0);
  if (value == nullptr) return false;
  uint64_t value64 = 0;
  for (int i = 0; i < kMaximumLeb128Size; ++i) {
    if (Finished()) {
      LIBGAV1_DLOG(ERROR, "Not enough bits to read LEB128 value.");
      return false;
    }
    const size_t byte_offset = DivideBy8(bit_offset_, false);
    const uint8_t byte = data_[byte_offset];
    bit_offset_ += 8;
    value64 |= static_cast<uint64_t>(byte & kLeb128ValueByteMask) << (i * 7);
    if ((byte & kLeb128TerminationByteMask) == 0) {
      if (value64 != static_cast<size_t>(value64) ||
          value64 > std::numeric_limits<uint32_t>::max()) {
        LIBGAV1_DLOG(
            ERROR, "LEB128 value (%" PRIu64 ") exceeded uint32_t maximum (%u).",
            value64, std::numeric_limits<uint32_t>::max());
        return false;
      }
      *value = static_cast<size_t>(value64);
      return true;
    }
  }
  LIBGAV1_DLOG(
      ERROR,
      "Exceeded kMaximumLeb128Size (%d) when trying to read LEB128 value",
      kMaximumLeb128Size);
  return false;
}

bool RawBitReader::ReadUvlc(uint32_t* const value) {
  if (value == nullptr) return false;
  int leading_zeros = 0;
  while (true) {
    const int bit = ReadBit();
    if (bit == -1) {
      LIBGAV1_DLOG(ERROR, "Not enough bits to read uvlc value.");
      return false;
    }
    if (bit == 1) break;
    ++leading_zeros;
    if (leading_zeros == 32) {
      LIBGAV1_DLOG(ERROR,
                   "Exceeded maximum size (32) when trying to read uvlc value");
      return false;
    }
  }
  const int literal = static_cast<int>(ReadLiteral(leading_zeros));
  if (literal == -1) {
    LIBGAV1_DLOG(ERROR, "Not enough bits to read uvlc value.");
    return false;
  }
  *value = literal + (1U << leading_zeros) - 1;
  return true;
}

bool RawBitReader::AlignToNextByte() {
  while ((bit_offset_ & 7) != 0) {
    if (ReadBit() != 0) {
      return false;
    }
  }
  return true;
}

bool RawBitReader::VerifyAndSkipTrailingBits(size_t num_bits) {
  if (ReadBit() != 1) return false;
  for (size_t i = 0; i < num_bits - 1; ++i) {
    if (ReadBit() != 0) return false;
  }
  return true;
}

bool RawBitReader::SkipBytes(size_t num_bytes) {
  // If we are not at a byte boundary, return false.
  return ((bit_offset_ & 7) != 0) ? false : SkipBits(num_bytes * 8);
}

bool RawBitReader::SkipBits(size_t num_bits) {
  // If the reader is already finished, return false.
  if (Finished()) return false;
  // If skipping |num_bits| runs out of buffer, return false.
  const size_t bit_offset = bit_offset_ + num_bits - 1;
  if (DivideBy8(bit_offset, false) >= size_) return false;
  bit_offset_ += num_bits;
  return true;
}

bool RawBitReader::CanReadLiteral(size_t num_bits) const {
  if (Finished()) return false;
  const size_t bit_offset = bit_offset_ + num_bits - 1;
  return DivideBy8(bit_offset, false) < size_;
}

bool RawBitReader::Finished() const {
  return DivideBy8(bit_offset_, false) >= size_;
}

}  // namespace libgav1
