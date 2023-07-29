#include "src/yuv_buffer.h"

#include <cassert>
#include <cstddef>

#include "src/utils/common.h"
#include "src/utils/logging.h"
#include "src/utils/memory.h"

namespace libgav1 {
namespace {

// |align| must be a power of 2.
uint8_t* AlignAddr(uint8_t* const addr, const size_t align) {
  const auto value = reinterpret_cast<size_t>(addr);
  return reinterpret_cast<uint8_t*>(Align(value, align));
}

}  // namespace

YuvBuffer::~YuvBuffer() { AlignedFree(buffer_alloc_); }

// Size conventions:
// * Widths, heights, and border sizes are in pixels.
// * Strides and plane sizes are in bytes.
bool YuvBuffer::Realloc(int bitdepth, bool is_monochrome, int width, int height,
                        int8_t subsampling_x, int8_t subsampling_y, int border,
                        int byte_alignment,
                        GetFrameBufferCallback get_frame_buffer,
                        void* private_data, FrameBuffer* frame_buffer) {
  // Only support allocating buffers that have a border that's a multiple of
  // 32. The border restriction is required to get 16-byte alignment of the
  // start of the chroma rows.
  if ((border & 31) != 0) return false;

  assert(byte_alignment == 0 || byte_alignment >= 16);
  const int byte_align = (byte_alignment == 0) ? 1 : byte_alignment;
  // byte_align must be a power of 2.
  assert((byte_align & (byte_align - 1)) == 0);

  // aligned_width and aligned_height are width and height padded to a
  // multiple of 8 pixels.
  const int aligned_width = Align(width, 8);
  const int aligned_height = Align(height, 8);

  // Calculate y_stride (in bytes). It is padded to a multiple of 16 bytes.
  int y_stride = aligned_width + 2 * border;
#if LIBGAV1_MAX_BITDEPTH >= 10
  if (bitdepth > 8) y_stride *= sizeof(uint16_t);
#endif
  y_stride = Align(y_stride, 16);
  // Size of the Y plane in bytes.
  const uint64_t y_plane_size =
      (aligned_height + 2 * border) * static_cast<uint64_t>(y_stride) +
      byte_alignment;
  assert((y_plane_size & 15) == 0);

  const int uv_width = aligned_width >> subsampling_x;
  const int uv_height = aligned_height >> subsampling_y;
  const int uv_border_width = border >> subsampling_x;
  const int uv_border_height = border >> subsampling_y;

  // Calculate uv_stride (in bytes). It is padded to a multiple of 16 bytes.
  int uv_stride = uv_width + 2 * uv_border_width;
#if LIBGAV1_MAX_BITDEPTH >= 10
  if (bitdepth > 8) uv_stride *= sizeof(uint16_t);
#endif
  uv_stride = Align(uv_stride, 16);
  // Size of the U or V plane in bytes.
  const uint64_t uv_plane_size =
      (uv_height + 2 * uv_border_height) * static_cast<uint64_t>(uv_stride) +
      byte_alignment;
  assert((uv_plane_size & 15) == 0);

  const uint64_t frame_size = y_plane_size + 2 * uv_plane_size;

  // Allocate y_buffer, u_buffer, and v_buffer with 16-byte alignment.
  uint8_t* y_buffer = nullptr;
  uint8_t* u_buffer = nullptr;
  uint8_t* v_buffer = nullptr;

  if (get_frame_buffer != nullptr) {
    // |get_frame_buffer| allocates unaligned memory. Ask it to allocate 15
    // extra bytes so we can align the buffers to 16-byte boundaries.
    const int align_addr_extra_size = 15;
    const uint64_t external_y_plane_size = y_plane_size + align_addr_extra_size;
    const uint64_t external_uv_plane_size =
        uv_plane_size + align_addr_extra_size;

    assert(frame_buffer != nullptr);

    if (external_y_plane_size != static_cast<size_t>(external_y_plane_size) ||
        external_uv_plane_size != static_cast<size_t>(external_uv_plane_size)) {
      return false;
    }

    // Allocation to hold larger frame, or first allocation.
    if (get_frame_buffer(
            private_data, static_cast<size_t>(external_y_plane_size),
            static_cast<size_t>(external_uv_plane_size), frame_buffer) < 0) {
      return false;
    }

    if (frame_buffer->data[0] == nullptr ||
        frame_buffer->size[0] < external_y_plane_size ||
        frame_buffer->data[1] == nullptr ||
        frame_buffer->size[1] < external_uv_plane_size ||
        frame_buffer->data[2] == nullptr ||
        frame_buffer->size[2] < external_uv_plane_size) {
      assert(0 && "The get_frame_buffer callback malfunctioned.");
      LIBGAV1_DLOG(ERROR, "The get_frame_buffer callback malfunctioned.");
      return false;
    }

    y_buffer = AlignAddr(frame_buffer->data[0], 16);
    u_buffer = AlignAddr(frame_buffer->data[1], 16);
    v_buffer = AlignAddr(frame_buffer->data[2], 16);
  } else {
    assert(private_data == nullptr);
    assert(frame_buffer == nullptr);

    if (frame_size > buffer_alloc_size_) {
      // Allocation to hold larger frame, or first allocation.
      AlignedFree(buffer_alloc_);
      buffer_alloc_ = nullptr;

      if (frame_size != static_cast<size_t>(frame_size)) return false;

      buffer_alloc_ = static_cast<uint8_t*>(
          AlignedAlloc(16, static_cast<size_t>(frame_size)));
      if (buffer_alloc_ == nullptr) return false;

      buffer_alloc_size_ = static_cast<size_t>(frame_size);
    }

    y_buffer = buffer_alloc_;
    u_buffer = buffer_alloc_ + y_plane_size;
    v_buffer = buffer_alloc_ + y_plane_size + uv_plane_size;
  }

  y_crop_width_ = width;
  y_crop_height_ = height;
  y_width_ = aligned_width;
  y_height_ = aligned_height;
  stride_[kPlaneY] = y_stride;
  left_border_[kPlaneY] = right_border_[kPlaneY] = top_border_[kPlaneY] =
      bottom_border_[kPlaneY] = border;

  uv_crop_width_ = (width + subsampling_x) >> subsampling_x;
  uv_crop_height_ = (height + subsampling_y) >> subsampling_y;
  uv_width_ = uv_width;
  uv_height_ = uv_height;
  stride_[kPlaneU] = stride_[kPlaneV] = uv_stride;
  left_border_[kPlaneU] = right_border_[kPlaneU] = uv_border_width;
  top_border_[kPlaneU] = bottom_border_[kPlaneU] = uv_border_height;
  left_border_[kPlaneV] = right_border_[kPlaneV] = uv_border_width;
  top_border_[kPlaneV] = bottom_border_[kPlaneV] = uv_border_height;

  subsampling_x_ = subsampling_x;
  subsampling_y_ = subsampling_y;

  bitdepth_ = bitdepth;
  is_monochrome_ = is_monochrome;
  int border_bytes = border;
  int uv_border_width_bytes = uv_border_width;
#if LIBGAV1_MAX_BITDEPTH >= 10
  if (bitdepth > 8) {
    border_bytes *= sizeof(uint16_t);
    uv_border_width_bytes *= sizeof(uint16_t);
  }
#endif
  buffer_[kPlaneY] =
      AlignAddr(y_buffer + (border * y_stride) + border_bytes, byte_align);
  buffer_[kPlaneU] = AlignAddr(
      u_buffer + (uv_border_height * uv_stride) + uv_border_width_bytes,
      byte_align);
  buffer_[kPlaneV] = AlignAddr(
      v_buffer + (uv_border_height * uv_stride) + uv_border_width_bytes,
      byte_align);

  return true;
}

}  // namespace libgav1
