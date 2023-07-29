// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#include "string_conversions.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <vector>

#include "log.h"
#include "platform.h"

namespace wvcdm {

static const char kBase64Codes[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

// Gets the low |n| bits of |in|.
#define GET_LOW_BITS(in, n) ((in) & ((1 << (n)) - 1))
// Gets the given (zero-indexed) bits [a, b) of |in|.
#define GET_BITS(in, a, b) GET_LOW_BITS((in) >> (a), (b) - (a))
// Calculates a/b using round-up division (only works for positive numbers).
#define CEIL_DIVIDE(a, b) ((((a) - 1) / (b)) + 1)

int DecodeBase64Char(char c) {
  const char* it = strchr(kBase64Codes, c);
  if (it == NULL)
    return -1;
  return it - kBase64Codes;
}

bool DecodeHexChar(char ch, unsigned char* digit) {
  if (ch >= '0' && ch <= '9') {
    *digit = ch - '0';
  } else {
    ch = tolower(ch);
    if ((ch >= 'a') && (ch <= 'f')) {
      *digit = ch - 'a' + 10;
    } else {
      return false;
    }
  }
  return true;
}

// converts an ascii hex string(2 bytes per digit) into a decimal byte string
std::vector<uint8_t> a2b_hex(const std::string& byte) {
  std::vector<uint8_t> array;
  unsigned int count = byte.size();
  if (count == 0 || (count % 2) != 0) {
    LOGE("Invalid input size %u for string %s", count, byte.c_str());
    return array;
  }

  for (unsigned int i = 0; i < count / 2; ++i) {
    unsigned char msb = 0;  // most significant 4 bits
    unsigned char lsb = 0;  // least significant 4 bits
    if (!DecodeHexChar(byte[i * 2], &msb) ||
        !DecodeHexChar(byte[i * 2 + 1], &lsb)) {
      LOGE("Invalid hex value %c%c at index %d", byte[i * 2], byte[i * 2 + 1],
           i);
      return array;
    }
    array.push_back((msb << 4) | lsb);
  }
  return array;
}

// converts an ascii hex string(2 bytes per digit) into a decimal byte string
// dump the string with the label.
std::vector<uint8_t> a2b_hex(const std::string& label,
                             const std::string& byte) {
  std::cout << std::endl
            << "[[DUMP: " << label << " ]= \"" << byte << "\"]" << std::endl
            << std::endl;

  return a2b_hex(byte);
}

std::string a2bs_hex(const std::string& byte) {
  std::vector<uint8_t> array = a2b_hex(byte);
  return std::string(array.begin(), array.end());
}

std::string b2a_hex(const std::vector<uint8_t>& byte) {
  return HexEncode(&byte[0], byte.size());
}

std::string b2a_hex(const std::string& byte) {
  return HexEncode(reinterpret_cast<const uint8_t*>(byte.data()),
                   byte.length());
}

// Encode for standard base64 encoding (RFC4648).
// https://en.wikipedia.org/wiki/Base64
// Text    |       M        |       a       |       n        |
// ASCI    |   77 (0x4d)    |   97 (0x61)   |   110 (0x6e)   |
// Bits    | 0 1 0 0 1 1 0 1 0 1 1 0 0 0 0 1 0 1 1 0 1 1 1 0 |
// Index   |     19     |     22    |      5    |     46     |
// Base64  |      T     |      W    |      F    |      u     |
//         | <-----------------  24-bits  -----------------> |
std::string Base64Encode(const std::vector<uint8_t>& bin_input) {
  if (bin_input.empty()) {
    return std::string();
  }

  // |temp| stores a 24-bit block that is treated as an array where insertions
  // occur from high to low.
  uint32_t temp = 0;
  size_t out_index = 0;
  const size_t out_size = CEIL_DIVIDE(bin_input.size(), 3) * 4;
  std::string result(out_size, '\0');
  for (size_t i = 0; i < bin_input.size(); i++) {
    // "insert" 8-bits of data
    temp |= (bin_input[i] << ((2 - (i % 3)) * 8));

    if (i % 3 == 2) {
      result[out_index++] = kBase64Codes[GET_BITS(temp, 18, 24)];
      result[out_index++] = kBase64Codes[GET_BITS(temp, 12, 18)];
      result[out_index++] = kBase64Codes[GET_BITS(temp,  6, 12)];
      result[out_index++] = kBase64Codes[GET_BITS(temp,  0,  6)];
      temp = 0;
    }
  }

  if (bin_input.size() % 3 == 1) {
    result[out_index++] = kBase64Codes[GET_BITS(temp, 18, 24)];
    result[out_index++] = kBase64Codes[GET_BITS(temp, 12, 18)];
    result[out_index++] = '=';
    result[out_index++] = '=';
  } else if (bin_input.size() % 3 == 2) {
    result[out_index++] = kBase64Codes[GET_BITS(temp, 18, 24)];
    result[out_index++] = kBase64Codes[GET_BITS(temp, 12, 18)];
    result[out_index++] = kBase64Codes[GET_BITS(temp,  6, 12)];
    result[out_index++] = '=';
  }

  return result;
}

// Filename-friendly base64 encoding (RFC4648), commonly referred to
// as Base64WebSafeEncode.
//
// This is the encoding required to interface with the provisioning server, as
// well as for certain license server transactions.  It is also used for logging
// certain strings. The difference between web safe encoding vs regular encoding
// is that the web safe version replaces '+' with '-' and '/' with '_'.
std::string Base64SafeEncode(const std::vector<uint8_t>& bin_input) {
  if (bin_input.empty()) {
    return std::string();
  }

  std::string ret = Base64Encode(bin_input);
  for (size_t i = 0; i < ret.size(); i++) {
    if (ret[i] == '+')
      ret[i] = '-';
    else if (ret[i] == '/')
      ret[i] = '_';
  }
  return ret;
}

std::string Base64SafeEncodeNoPad(const std::vector<uint8_t>& bin_input) {
  std::string b64_output = Base64SafeEncode(bin_input);
  // Output size: ceiling [ bin_input.size() * 4 / 3 ].
  b64_output.resize((bin_input.size() * 4 + 2) / 3);
  return b64_output;
}

// Decode for standard base64 encoding (RFC4648).
std::vector<uint8_t> Base64Decode(const std::string& b64_input) {
  if (b64_input.empty()) {
    return std::vector<uint8_t>();
  }

  const size_t out_size_max = CEIL_DIVIDE(b64_input.size() * 3, 4);
  std::vector<uint8_t> result(out_size_max, '\0');

  // |temp| stores 24-bits of data that is treated as an array where insertions
  // occur from high to low.
  uint32_t temp = 0;
  size_t out_index = 0;
  size_t i;
  for (i = 0; i < b64_input.size(); i++) {
    if (b64_input[i] == '=') {
      // Verify an '=' only appears at the end.  We want i to remain at the
      // first '=', so we need an inner loop.
      for (size_t j = i; j < b64_input.size(); j++) {
        if (b64_input[j] != '=') {
          LOGE("base64Decode failed");
          return std::vector<uint8_t>();
        }
      }
      break;
    }

    const int decoded = DecodeBase64Char(b64_input[i]);
    if (decoded < 0) {
      LOGE("base64Decode failed");
      return std::vector<uint8_t>();
    }
    // "insert" 6-bits of data
    temp |= (decoded << ((3 - (i % 4)) * 6));

    if (i % 4 == 3) {
      result[out_index++] = GET_BITS(temp, 16, 24);
      result[out_index++] = GET_BITS(temp,  8, 16);
      result[out_index++] = GET_BITS(temp,  0,  8);
      temp = 0;
    }
  }

  switch (i % 4) {
    case 1:
      LOGE("base64Decode failed");
      return std::vector<uint8_t>();
    case 2:
      result[out_index++] = GET_BITS(temp, 16, 24);
      break;
    case 3:
      result[out_index++] = GET_BITS(temp, 16, 24);
      result[out_index++] = GET_BITS(temp,  8, 16);
      break;
  }
  result.resize(out_index);
  return result;
}

// Decode for Filename-friendly base64 encoding (RFC4648), commonly referred
// as Base64WebSafeDecode. Add padding if needed.
std::vector<uint8_t> Base64SafeDecode(const std::string& b64_input) {
  if (b64_input.empty()) {
    return std::vector<uint8_t>();
  }

  // Make a copy so we can modify it to replace the web-safe special characters
  // with the normal ones.
  std::string input_copy = b64_input;
  for (size_t i = 0; i < input_copy.size(); i++) {
    if (input_copy[i] == '-')
      input_copy[i] = '+';
    else if (input_copy[i] == '_')
      input_copy[i] = '/';
  }
  return Base64Decode(input_copy);
}

std::string HexEncode(const uint8_t* in_buffer, unsigned int size) {
  static const char kHexChars[] = "0123456789ABCDEF";

  // Each input byte creates two output hex characters.
  std::string out_buffer(size * 2, '\0');

  for (unsigned int i = 0; i < size; ++i) {
    char byte = in_buffer[i];
    out_buffer[(i << 1)] = kHexChars[(byte >> 4) & 0xf];
    out_buffer[(i << 1) + 1] = kHexChars[byte & 0xf];
  }
  return out_buffer;
}

std::string IntToString(int value) {
  // log10(2) ~= 0.3 bytes needed per bit or per byte log10(2**8) ~= 2.4.
  // So round up to allocate 3 output characters per byte, plus 1 for '-'.
  const int kOutputBufSize = 3 * sizeof(int) + 1;
  char buffer[kOutputBufSize];
  memset(buffer, 0, kOutputBufSize);
  snprintf(buffer, kOutputBufSize, "%d", value);

  std::string out_string(buffer);
  return out_string;
}

int64_t htonll64(int64_t x) {  // Convert to big endian (network-byte-order)
  union {
    uint32_t array[2];
    int64_t number;
  } mixed;
  mixed.number = 1;
  if (mixed.array[0] == 1) {  // Little Endian.
    mixed.number = x;
    uint32_t temp = mixed.array[0];
    mixed.array[0] = htonl(mixed.array[1]);
    mixed.array[1] = htonl(temp);
    return mixed.number;
  } else {  // Big Endian.
    return x;
  }
}

std::string BytesToString(const uint8_t* bytes, unsigned size) {
  if (!bytes || !size) return "";
  const char* char_bytes = reinterpret_cast<const char*>(bytes);
  return std::string(char_bytes, char_bytes + size);
}

}  // namespace wvcdm
