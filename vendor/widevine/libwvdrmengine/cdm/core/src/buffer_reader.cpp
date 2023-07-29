// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#include "buffer_reader.h"

#include "log.h"
#include "platform.h"

namespace wvcdm {

bool BufferReader::Read1(uint8_t* v) {
  if (v == NULL) {
    LOGE("BufferReader::Read1 : Failure during parse: Null output parameter "
         "when expecting non-null");
    return false;
  }

  if (!HasBytes(1)) {
    LOGV("BufferReader::Read1 : Failure while parsing: "
         "Not enough bytes (1)");
    return false;
  }

  *v = buf_[pos_++];
  return true;
}

// Internal implementation of multi-byte reads
template <typename T>
bool BufferReader::Read(T* v) {
  if (v == NULL) {
    LOGE("BufferReader::Read<T> : Failure during parse: Null output parameter "
         "when expecting non-null (%s)", __PRETTY_FUNCTION__);
    return false;
  }

  if (!HasBytes(sizeof(T))) {
    LOGV("BufferReader::Read<T> : Failure during parse: "
         "Not enough bytes (%u)", sizeof(T));
    return false;
  }

  T tmp = 0;
  for (size_t i = 0; i < sizeof(T); i++) {
    tmp <<= 8;
    tmp += buf_[pos_++];
  }
  *v = tmp;
  return true;
}

bool BufferReader::Read2(uint16_t* v) { return Read(v); }
bool BufferReader::Read2s(int16_t* v) { return Read(v); }
bool BufferReader::Read4(uint32_t* v) { return Read(v); }
bool BufferReader::Read4s(int32_t* v) { return Read(v); }
bool BufferReader::Read8(uint64_t* v) { return Read(v); }
bool BufferReader::Read8s(int64_t* v) { return Read(v); }

bool BufferReader::ReadString(std::string* str, size_t count) {
  if (str == NULL) {
    LOGE("BufferReader::ReadString : Failure during parse: Null output "
         "parameter when expecting non-null");
    return false;
  }

  if (!HasBytes(count)) {
    LOGV("BufferReader::ReadString : Parse Failure: "
         "Not enough bytes (%d)", count);
    return false;
  }

  str->assign(buf_ + pos_, buf_ + pos_ + count);
  pos_ += count;
  return true;
}

bool BufferReader::ReadVec(std::vector<uint8_t>* vec, size_t count) {
  if (vec == NULL) {
    LOGE("BufferReader::ReadVec : Failure during parse: Null output parameter "
         "when expecting non-null");
    return false;
  }

  if (!HasBytes(count)) {
    LOGV("BufferReader::ReadVec : Parse Failure: "
         "Not enough bytes (%d)", count);
    return false;
  }

  vec->clear();
  vec->insert(vec->end(), buf_ + pos_, buf_ + pos_ + count);
  pos_ += count;
  return true;
}

bool BufferReader::SkipBytes(size_t bytes) {
  if (!HasBytes(bytes)) {
    LOGV("BufferReader::SkipBytes : Parse Failure: "
         "Not enough bytes (%d)", bytes);
    return false;
  }

  pos_ += bytes;
  return true;
}

bool BufferReader::Read4Into8(uint64_t* v) {
  if (v == NULL) {
    LOGE("BufferReader::Read4Into8 : Failure during parse: Null output "
         "parameter when expecting non-null");
    return false;
  }

  uint32_t tmp;
  if (!Read4(&tmp)) {
    return false;
  }
  *v = tmp;
  return true;
}

bool BufferReader::Read4sInto8s(int64_t* v) {
  if (v == NULL) {
    LOGE("BufferReader::Read4sInto8s : Failure during parse: Null output "
         "parameter when expecting non-null");
    return false;
  }

  // Beware of the need for sign extension.
  int32_t tmp;
  if (!Read4s(&tmp)) {
    return false;
  }
  *v = tmp;
  return true;
}
}  // namespace wvcdm
