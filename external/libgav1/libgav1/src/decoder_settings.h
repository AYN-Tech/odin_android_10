#ifndef LIBGAV1_SRC_DECODER_SETTINGS_H_
#define LIBGAV1_SRC_DECODER_SETTINGS_H_

#include <cstdint>

#include "src/frame_buffer.h"

// All the declarations in this file are part of the public ABI.

namespace libgav1 {

// Applications must populate this structure before creating a decoder instance.
struct DecoderSettings {
  // Number of threads to use when decoding. Must be greater than 0. The
  // library will create at most |threads|-1 new threads, the calling thread is
  // considered part of the library's thread count. Defaults to 1 (no new
  // threads will be created).
  int threads = 1;
  // Do frame parallel decoding.
  bool frame_parallel = false;
  // Get frame buffer callback.
  GetFrameBufferCallback get = nullptr;
  // Release frame buffer callback.
  ReleaseFrameBufferCallback release = nullptr;
  // Passed as the private_data argument to the callbacks.
  void* callback_private_data = nullptr;
  // Mask indicating the post processing filters that need to be applied to the
  // reconstructed frame. From LSB:
  //   Bit 0: Loop filter (deblocking filter).
  //   Bit 1: Cdef.
  //   Bit 2: Superres.
  //   Bit 3: Loop restoration.
  //   Bit 4: Film grain synthesis.
  //   All the bits other than the last 5 are ignored.
  uint8_t post_filter_mask = 0x1f;
};

}  // namespace libgav1
#endif  // LIBGAV1_SRC_DECODER_SETTINGS_H_
