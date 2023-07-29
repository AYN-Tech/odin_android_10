#include "src/buffer_pool.h"

#include <cassert>
#include <cstring>

#include "src/utils/common.h"
#include "src/utils/logging.h"

namespace libgav1 {

namespace {

// Copies the feature_enabled, feature_data, segment_id_pre_skip, and
// last_active_segment_id fields of Segmentation.
void CopySegmentationParameters(const Segmentation& from, Segmentation* to) {
  memcpy(to->feature_enabled, from.feature_enabled,
         sizeof(to->feature_enabled));
  memcpy(to->feature_data, from.feature_data, sizeof(to->feature_data));
  to->segment_id_pre_skip = from.segment_id_pre_skip;
  to->last_active_segment_id = from.last_active_segment_id;
}

}  // namespace

RefCountedBuffer::RefCountedBuffer() {
  memset(&raw_frame_buffer_, 0, sizeof(raw_frame_buffer_));
}

RefCountedBuffer::~RefCountedBuffer() = default;

bool RefCountedBuffer::Realloc(int bitdepth, bool is_monochrome, int width,
                               int height, int subsampling_x, int subsampling_y,
                               int border, int byte_alignment) {
  return yuv_buffer_.Realloc(bitdepth, is_monochrome, width, height,
                             subsampling_x, subsampling_y, border,
                             byte_alignment, pool_->get_frame_buffer_,
                             pool_->callback_private_data_, &raw_frame_buffer_);
}

bool RefCountedBuffer::SetFrameDimensions(const ObuFrameHeader& frame_header) {
  upscaled_width_ = frame_header.upscaled_width;
  frame_width_ = frame_header.width;
  frame_height_ = frame_header.height;
  render_width_ = frame_header.render_width;
  render_height_ = frame_header.render_height;
  rows4x4_ = frame_header.rows4x4;
  columns4x4_ = frame_header.columns4x4;
  const int rows4x4_half = DivideBy2(rows4x4_);
  const int columns4x4_half = DivideBy2(columns4x4_);
  if (!motion_field_reference_frame_.Reset(rows4x4_half, columns4x4_half,
                                           /*zero_initialize=*/false) ||
      !motion_field_mv_.Reset(rows4x4_half, columns4x4_half,
                              /*zero_initialize=*/false)) {
    return false;
  }
  return segmentation_map_.Allocate(rows4x4_, columns4x4_);
}

void RefCountedBuffer::SetGlobalMotions(
    const std::array<GlobalMotion, kNumReferenceFrameTypes>& global_motions) {
  for (int ref = kReferenceFrameLast; ref <= kReferenceFrameAlternate; ++ref) {
    static_assert(sizeof(global_motion_[ref].params) ==
                      sizeof(global_motions[ref].params),
                  "");
    memcpy(global_motion_[ref].params, global_motions[ref].params,
           sizeof(global_motion_[ref].params));
  }
}

void RefCountedBuffer::SetFrameContext(const SymbolDecoderContext& context) {
  frame_context_ = context;
  frame_context_.ResetIntraFrameYModeCdf();
  frame_context_.ResetCounters();
}

void RefCountedBuffer::GetSegmentationParameters(
    Segmentation* segmentation) const {
  CopySegmentationParameters(/*from=*/segmentation_, /*to=*/segmentation);
}

void RefCountedBuffer::SetSegmentationParameters(
    const Segmentation& segmentation) {
  CopySegmentationParameters(/*from=*/segmentation, /*to=*/&segmentation_);
}

void RefCountedBuffer::SetBufferPool(BufferPool* pool) { pool_ = pool; }

void RefCountedBuffer::ReturnToBufferPool(RefCountedBuffer* ptr) {
  ptr->pool_->ReturnUnusedBuffer(ptr);
}

// static
constexpr int BufferPool::kNumBuffers;

BufferPool::BufferPool(const DecoderSettings& settings) {
  if (settings.get != nullptr && settings.release != nullptr) {
    get_frame_buffer_ = settings.get;
    release_frame_buffer_ = settings.release;
    callback_private_data_ = settings.callback_private_data;
  } else {
    internal_frame_buffers_ = InternalFrameBufferList::Create(kNumBuffers);
    // GetInternalFrameBuffer checks whether its private_data argument is null,
    // so we don't need to check whether internal_frame_buffers_ is null here.
    get_frame_buffer_ = GetInternalFrameBuffer;
    release_frame_buffer_ = ReleaseInternalFrameBuffer;
    callback_private_data_ = internal_frame_buffers_.get();
  }
  for (RefCountedBuffer& buffer : buffers_) {
    buffer.SetBufferPool(this);
  }
}

BufferPool::~BufferPool() {
  for (const RefCountedBuffer& buffer : buffers_) {
    if (buffer.in_use_) {
      assert(0 && "RefCountedBuffer still in use at destruction time.");
      LIBGAV1_DLOG(ERROR, "RefCountedBuffer still in use at destruction time.");
    }
  }
}

RefCountedBufferPtr BufferPool::GetFreeBuffer() {
  for (RefCountedBuffer& buffer : buffers_) {
    if (!buffer.in_use_) {
      buffer.in_use_ = true;
      return RefCountedBufferPtr(&buffer, RefCountedBuffer::ReturnToBufferPool);
    }
  }

  // We should never run out of free buffers. If we reach here, there is a
  // reference leak.
  return RefCountedBufferPtr();
}

void BufferPool::ReturnUnusedBuffer(RefCountedBuffer* buffer) {
  assert(buffer->in_use_);
  buffer->in_use_ = false;
  if (buffer->raw_frame_buffer_.data[0] != nullptr) {
    release_frame_buffer_(callback_private_data_, &buffer->raw_frame_buffer_);
    memset(&buffer->raw_frame_buffer_, 0, sizeof(buffer->raw_frame_buffer_));
  }
}

}  // namespace libgav1
