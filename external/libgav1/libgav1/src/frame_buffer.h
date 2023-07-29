#ifndef LIBGAV1_SRC_FRAME_BUFFER_H_
#define LIBGAV1_SRC_FRAME_BUFFER_H_

// All the declarations in this file are part of the public ABI. This file may
// be included by both C and C++ files.

#include <stddef.h>
#include <stdint.h>

// The callback functions use the C linkage conventions.
#if defined(__cplusplus)
extern "C" {
#endif

// This structure represents an allocated frame buffer.
struct Libgav1FrameBuffer {
  // In the |data| and |size| arrays, the elements at indexes 0, 1, and 2 are
  // for the Y, U, and V planes, respectively.
  uint8_t* data[3];    // Pointers to the data buffers.
  size_t size[3];      // Sizes of the data buffers in bytes.
  void* private_data;  // Frame buffer's private data. Available for use by the
                       // release frame buffer callback. Also copied to the
                       // |buffer_private_data| field of DecoderBuffer for use
                       // by the consumer of a DecoderBuffer.
};

// This callback is invoked by the decoder to allocate a frame buffer, which
// consists of three data buffers, for the Y, U, and V planes, respectively.
// |y_plane_min_size| specifies the minimum size in bytes of the Y plane data
// buffer, and |uv_plane_min_size| specifies the minimum size in bytes of the
// U and V plane data buffers.
//
// The callback must set |frame_buffer->data[i]| to point to the data buffers,
// and set |frame_buffer->size[i]| to the actual sizes of the data buffers. The
// callback may set |frame_buffer->private_data| to a value that will be useful
// to the release frame buffer callback and the consumer of a DecoderBuffer.
//
// Returns 0 on success, -1 on failure.
typedef int (*Libgav1GetFrameBufferCallback)(void* private_data,
                                             size_t y_plane_min_size,
                                             size_t uv_plane_min_size,
                                             Libgav1FrameBuffer* frame_buffer);

// This callback is invoked by the decoder to release a frame buffer.
//
// Returns 0 on success, -1 on failure.
typedef int (*Libgav1ReleaseFrameBufferCallback)(
    void* private_data, Libgav1FrameBuffer* frame_buffer);

#if defined(__cplusplus)
}  // extern "C"

// Declare type aliases for C++.
namespace libgav1 {

using FrameBuffer = Libgav1FrameBuffer;
using GetFrameBufferCallback = Libgav1GetFrameBufferCallback;
using ReleaseFrameBufferCallback = Libgav1ReleaseFrameBufferCallback;

}  // namespace libgav1
#endif  // defined(__cplusplus)

#endif  // LIBGAV1_SRC_FRAME_BUFFER_H_
