// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

/*********************************************************************
 * pst_report.h
 *
 * Reference APIs needed to support Widevine's crypto algorithms.
 *********************************************************************/

#ifndef PST_REPORT_H_
#define PST_REPORT_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "OEMCryptoCENC.h"
#include "string_conversions.h"  // needed for htonll64.

namespace wvcdm {

class Unpacked_PST_Report {
 public:
  // This object does not own the buffer, and does not check that buffer
  // is not null.
  Unpacked_PST_Report(uint8_t *buffer) : buffer_(buffer) {}

  // Copy and move semantics of this class is like that of a pointer.
  Unpacked_PST_Report(const Unpacked_PST_Report& other) :
      buffer_(other.buffer_) {}

  Unpacked_PST_Report& operator=(const Unpacked_PST_Report& other) {
    buffer_ = other.buffer_;
    return *this;
  }

  size_t report_size() const {
    return pst_length() + kraw_pst_report_size;
  }

  static size_t report_size(size_t pst_length) {
    return pst_length + kraw_pst_report_size;
  }

  uint8_t status() const {
    return static_cast<uint8_t>(* (buffer_ +  kstatus_offset));
  }

  void set_status(uint8_t value)  {
    buffer_[kstatus_offset] = value;
  }

  uint8_t* signature()  {
    return buffer_ + ksignature_offset;
  }

  uint8_t clock_security_level() const {
    return static_cast<uint8_t>(* (buffer_ +  kclock_security_level_offset));
  }

  void set_clock_security_level(uint8_t value)  {
    buffer_[kclock_security_level_offset] = value;
  }

  uint8_t pst_length() const {
    return static_cast<uint8_t>(* (buffer_ +  kpst_length_offset));
  }

  void set_pst_length(uint8_t value)  {
    buffer_[kpst_length_offset] = value;
  }

  uint8_t padding() const {
    return static_cast<uint8_t>(* (buffer_ +  kpadding_offset));
  }

  void set_padding(uint8_t value)  {
    buffer_[kpadding_offset] = value;
  }

  // In host byte order.
  int64_t seconds_since_license_received() const {
    int64_t time;
    memcpy(&time, buffer_ +  kseconds_since_license_received_offset,
           sizeof(int64_t));
    return ntohll64(time);
  }

  // Parameter time is in host byte order.
  void set_seconds_since_license_received(int64_t time) const {
    time = ntohll64(time);
    memcpy(buffer_ +  kseconds_since_license_received_offset, &time,
           sizeof(int64_t));
  }

  // In host byte order.
  int64_t seconds_since_first_decrypt() const {
    int64_t time;
    memcpy(&time, buffer_ +  kseconds_since_first_decrypt_offset,
           sizeof(int64_t));
    return ntohll64(time);
  }

  // Parameter time is in host byte order.
  void set_seconds_since_first_decrypt(int64_t time) const {
    time = ntohll64(time);
    memcpy(buffer_ +  kseconds_since_first_decrypt_offset, &time,
           sizeof(int64_t));
  }

  // In host byte order.
  int64_t seconds_since_last_decrypt() const {
    int64_t time;
    memcpy(&time, buffer_ +  kseconds_since_last_decrypt_offset,
           sizeof(int64_t));
    return ntohll64(time);
  }

  // Parameter time is in host byte order.
  void set_seconds_since_last_decrypt(int64_t time) const {
    time = ntohll64(time);
    memcpy(buffer_ +  kseconds_since_last_decrypt_offset, &time,
           sizeof(int64_t));
  }

  uint8_t* pst() {
    return  (buffer_ +  kpst_offset);
  }

 private:
  uint8_t *buffer_;

  // Size of the PST_Report without the pst string.
  static const size_t kraw_pst_report_size = 48;
  static const size_t ksignature_offset  = 0;
  static const size_t kstatus_offset  = 20;
  static const size_t kclock_security_level_offset = 21;
  static const size_t kpst_length_offset = 22;
  static const size_t kpadding_offset = 23;
  static const size_t kseconds_since_license_received_offset = 24;
  static const size_t kseconds_since_first_decrypt_offset = 32;
  static const size_t kseconds_since_last_decrypt_offset = 40;
  static const size_t kpst_offset = 48;
};
}  // namespace wvcdm

#endif  // PST_REPORT_H_
