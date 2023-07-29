// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#ifndef WVCDM_CORE_BUFFER_READER_H_
#define WVCDM_CORE_BUFFER_READER_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "disallow_copy_and_assign.h"

namespace wvcdm {

// Annotate a function indicating the caller must examine the return value.
// Use like:
//   int foo() WARN_UNUSED_RESULT;
// To explicitly ignore a result, see |ignore_result()| in <base/basictypes.h>.
#if defined(COMPILER_GCC)
#define WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#define WARN_UNUSED_RESULT
#endif

class BufferReader {
 public:
  BufferReader(const uint8_t* buf, size_t size)
      : buf_(buf), size_(buf != NULL ? size : 0), pos_(0) {}

  bool HasBytes(size_t count) const { return pos_ + count <= size_; }
  bool IsEOF() const { return pos_ >= size_; }

  // Read a value from the stream, performing endian correction,
  // and advance the stream pointer.
  bool Read1(uint8_t* v) WARN_UNUSED_RESULT;
  bool Read2(uint16_t* v) WARN_UNUSED_RESULT;
  bool Read2s(int16_t* v) WARN_UNUSED_RESULT;
  bool Read4(uint32_t* v) WARN_UNUSED_RESULT;
  bool Read4s(int32_t* v) WARN_UNUSED_RESULT;
  bool Read8(uint64_t* v) WARN_UNUSED_RESULT;
  bool Read8s(int64_t* v) WARN_UNUSED_RESULT;

  bool ReadString(std::string* str, size_t count) WARN_UNUSED_RESULT;
  bool ReadVec(std::vector<uint8_t>* t, size_t count) WARN_UNUSED_RESULT;

  // These variants read a 4-byte integer of the corresponding signedness and
  // store it in the 8-byte return type.
  bool Read4Into8(uint64_t* v) WARN_UNUSED_RESULT;
  bool Read4sInto8s(int64_t* v) WARN_UNUSED_RESULT;

  // Advance the stream by this many bytes.
  bool SkipBytes(size_t nbytes) WARN_UNUSED_RESULT;

  const uint8_t* data() const { return buf_; }
  size_t size() const { return size_; }
  size_t pos() const { return pos_; }

 protected:
  const uint8_t* buf_;
  size_t size_;
  size_t pos_;

  template <typename T>
  bool Read(T* t) WARN_UNUSED_RESULT;

  CORE_DISALLOW_COPY_AND_ASSIGN(BufferReader);
};

}  // namespace wvcdm

#endif  // WVCDM_CORE_BUFFER_READER_H_
