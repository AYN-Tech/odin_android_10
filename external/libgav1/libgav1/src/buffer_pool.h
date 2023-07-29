#ifndef LIBGAV1_SRC_BUFFER_POOL_H_
#define LIBGAV1_SRC_BUFFER_POOL_H_

#include <array>
#include <cstdint>
#include <memory>

#include "src/decoder_buffer.h"
#include "src/decoder_settings.h"
#include "src/dsp/common.h"
#include "src/frame_buffer.h"
#include "src/internal_frame_buffer_list.h"
#include "src/obu_parser.h"
#include "src/symbol_decoder_context.h"
#include "src/utils/array_2d.h"
#include "src/utils/constants.h"
#include "src/utils/segmentation.h"
#include "src/utils/segmentation_map.h"
#include "src/utils/types.h"
#include "src/yuv_buffer.h"

namespace libgav1 {

class BufferPool;

// A reference-counted frame buffer. Clients should access it via
// RefCountedBufferPtr, which manages reference counting transparently.
class RefCountedBuffer {
 public:
  // Not copyable or movable.
  RefCountedBuffer(const RefCountedBuffer&) = delete;
  RefCountedBuffer& operator=(const RefCountedBuffer&) = delete;

  // Allocates the YUV buffer. Returns true on success. Returns false on
  // failure.
  //
  // * |width| and |height| are the image dimensions in pixels.
  // * |subsampling_x| and |subsampling_y| (either 0 or 1) specify the
  //   subsampling of the width and height of the chroma planes, respectively.
  // * |border| is the size of the borders (on all four sides) in pixels.
  // * |byte_alignment| specifies the additional alignment requirement of the
  //   data buffers of the Y, U, and V planes. If |byte_alignment| is 0, there
  //   is no additional alignment requirement. Otherwise, |byte_alignment|
  //   must be a power of 2 and greater than or equal to 16.
  //   NOTE: The strides are a multiple of 16. Therefore only the first row in
  //   each plane is aligned to |byte_alignment|. Subsequent rows are only
  //   16-byte aligned.
  bool Realloc(int bitdepth, bool is_monochrome, int width, int height,
               int subsampling_x, int subsampling_y, int border,
               int byte_alignment);

  YuvBuffer* buffer() { return &yuv_buffer_; }

  // Returns the buffer private data set by the get frame buffer callback when
  // it allocated the YUV buffer.
  void* buffer_private_data() const { return raw_frame_buffer_.private_data; }

  // NOTE: In the current frame, this is the frame_type syntax element in the
  // frame header. In a reference frame, this implements the RefFrameType array
  // in the spec.
  FrameType frame_type() const { return frame_type_; }
  void set_frame_type(enum FrameType frame_type) { frame_type_ = frame_type; }

  // The sample position for subsampled streams. This is the
  // chroma_sample_position syntax element in the sequence header.
  //
  // NOTE: The decoder does not use chroma_sample_position, but it needs to be
  // passed on to the client in DecoderBuffer.
  ChromaSamplePosition chroma_sample_position() const {
    return chroma_sample_position_;
  }
  void set_chroma_sample_position(
      enum ChromaSamplePosition chroma_sample_position) {
    chroma_sample_position_ = chroma_sample_position;
  }

  // Whether the frame can be used as show existing frame in the future.
  bool showable_frame() const { return showable_frame_; }
  void set_showable_frame(bool value) { showable_frame_ = value; }

  uint8_t order_hint(ReferenceFrameType reference_frame) const {
    return order_hint_[reference_frame];
  }
  void set_order_hint(ReferenceFrameType reference_frame, uint8_t order_hint) {
    order_hint_[reference_frame] = order_hint;
  }
  void ClearOrderHints() { order_hint_.fill(0); }

  // Sets upscaled_width_, frame_width_, frame_height_, render_width_,
  // render_height_, rows4x4_ and columns4x4_ from the corresponding fields
  // in frame_header. Allocates motion_field_reference_frame_,
  // motion_field_mv_, and segmentation_map_. Returns true on success, false
  // on failure.
  bool SetFrameDimensions(const ObuFrameHeader& frame_header);

  int32_t upscaled_width() const { return upscaled_width_; }
  int32_t frame_width() const { return frame_width_; }
  int32_t frame_height() const { return frame_height_; }
  // RenderWidth() and RenderHeight() return the render size, which is a hint
  // to the application about the desired display size.
  int32_t render_width() const { return render_width_; }
  int32_t render_height() const { return render_height_; }
  int32_t rows4x4() const { return rows4x4_; }
  int32_t columns4x4() const { return columns4x4_; }

  // Entry at |row|, |column| corresponds to
  // MfRefFrames[row * 2 + 1][column * 2 + 1] in the spec.
  ReferenceFrameType* motion_field_reference_frame(int row, int column) {
    return &motion_field_reference_frame_[row][column];
  }

  // Entry at |row|, |column| corresponds to
  // MfMvs[row * 2 + 1][column * 2 + 1] in the spec.
  MotionVector* motion_field_mv(int row, int column) {
    return &motion_field_mv_[row][column];
  }

  SegmentationMap* segmentation_map() { return &segmentation_map_; }
  const SegmentationMap* segmentation_map() const { return &segmentation_map_; }

  // Only the |params| field of each GlobalMotion struct should be used.
  const std::array<GlobalMotion, kNumReferenceFrameTypes>& GlobalMotions()
      const {
    return global_motion_;
  }
  // Saves the GlobalMotion array. Only the |params| field of each GlobalMotion
  // struct is saved.
  void SetGlobalMotions(
      const std::array<GlobalMotion, kNumReferenceFrameTypes>& global_motions);

  // Returns the saved CDF tables.
  const SymbolDecoderContext& FrameContext() const { return frame_context_; }
  // Saves the CDF tables. The intra_frame_y_mode_cdf table is reset to the
  // default. The last entry in each table, representing the symbol count for
  // that context, is set to 0.
  void SetFrameContext(const SymbolDecoderContext& context);

  const std::array<int8_t, kNumReferenceFrameTypes>& loop_filter_ref_deltas()
      const {
    return loop_filter_ref_deltas_;
  }
  const std::array<int8_t, kLoopFilterMaxModeDeltas>& loop_filter_mode_deltas()
      const {
    return loop_filter_mode_deltas_;
  }
  // Saves the ref_deltas and mode_deltas arrays in loop_filter.
  void SetLoopFilterDeltas(const LoopFilter& loop_filter) {
    loop_filter_ref_deltas_ = loop_filter.ref_deltas;
    loop_filter_mode_deltas_ = loop_filter.mode_deltas;
  }

  // Copies the saved values of the following fields to the Segmentation
  // struct: feature_enabled, feature_data, segment_id_pre_skip, and
  // last_active_segment_id. The other fields are left unchanged.
  void GetSegmentationParameters(Segmentation* segmentation) const;
  // Saves the feature_enabled, feature_data, segment_id_pre_skip, and
  // last_active_segment_id fields of the Segmentation struct.
  void SetSegmentationParameters(const Segmentation& segmentation);

  const FilmGrainParams& film_grain_params() const {
    return film_grain_params_;
  }
  void set_film_grain_params(const FilmGrainParams& params) {
    film_grain_params_ = params;
  }

 private:
  friend class BufferPool;

  // Methods for BufferPool:
  RefCountedBuffer();
  ~RefCountedBuffer();
  void SetBufferPool(BufferPool* pool);
  static void ReturnToBufferPool(RefCountedBuffer* ptr);

  BufferPool* pool_ = nullptr;
  FrameBuffer raw_frame_buffer_;
  YuvBuffer yuv_buffer_;
  bool in_use_ = false;  // Only used by BufferPool.

  enum FrameType frame_type_ = kFrameKey;
  enum ChromaSamplePosition chroma_sample_position_ =
      kChromaSamplePositionUnknown;
  bool showable_frame_ = false;

  // Note: order_hint_[0] (for kReferenceFrameIntra) is not used.
  std::array<uint8_t, kNumReferenceFrameTypes> order_hint_ = {};

  int32_t upscaled_width_ = 0;
  int32_t frame_width_ = 0;
  int32_t frame_height_ = 0;
  int32_t render_width_ = 0;
  int32_t render_height_ = 0;
  int32_t columns4x4_ = 0;
  int32_t rows4x4_ = 0;

  // Array of size (rows4x4 / 2) x (columns4x4 / 2). Entry at i, j corresponds
  // to MfRefFrames[i * 2 + 1][j * 2 + 1] in the spec.
  Array2D<ReferenceFrameType> motion_field_reference_frame_;
  // Array of size (rows4x4 / 2) x (columns4x4 / 2). Entry at i, j corresponds
  // to MfMvs[i * 2 + 1][j * 2 + 1] in the spec.
  Array2D<MotionVector> motion_field_mv_;
  // segmentation_map_ contains a rows4x4_ by columns4x4_ 2D array.
  SegmentationMap segmentation_map_;

  // Only the |params| field of each GlobalMotion struct is used.
  // global_motion_[0] (for kReferenceFrameIntra) is not used.
  std::array<GlobalMotion, kNumReferenceFrameTypes> global_motion_ = {};
  SymbolDecoderContext frame_context_;
  std::array<int8_t, kNumReferenceFrameTypes> loop_filter_ref_deltas_;
  std::array<int8_t, kLoopFilterMaxModeDeltas> loop_filter_mode_deltas_;
  // Only the feature_enabled, feature_data, segment_id_pre_skip, and
  // last_active_segment_id fields of the Segmentation struct are used.
  //
  // Note: The spec only requires that we save feature_enabled and
  // feature_data. Since segment_id_pre_skip and last_active_segment_id depend
  // on feature_enabled only, we also save their values as an optimization.
  Segmentation segmentation_ = {};
  FilmGrainParams film_grain_params_ = {};
};

// RefCountedBufferPtr contains a reference to a RefCountedBuffer.
//
// Note: For simplicity, RefCountedBufferPtr is implemented as a
// std::shared_ptr<RefCountedBuffer>. This requires a heap allocation of the
// control block for std::shared_ptr. To avoid that heap allocation, we can
// add a |ref_count_| field to RefCountedBuffer and implement a custom
// RefCountedBufferPtr class.
using RefCountedBufferPtr = std::shared_ptr<RefCountedBuffer>;

// BufferPool maintains a pool of RefCountedBuffers.
class BufferPool {
 public:
  explicit BufferPool(const DecoderSettings& settings);

  // Not copyable or movable.
  BufferPool(const BufferPool&) = delete;
  BufferPool& operator=(const BufferPool&) = delete;

  ~BufferPool();

  // Finds a free buffer in the buffer pool and returns a reference to the
  // free buffer. If there is no free buffer, returns a null pointer.
  RefCountedBufferPtr GetFreeBuffer();

 private:
  friend class RefCountedBuffer;

  // Reference frames + 1 scratch frame (for either the current frame or the
  // film grain frame).
  static constexpr int kNumBuffers = kNumReferenceFrameTypes + 1;

  // Returns an unused buffer to the buffer pool. Called by RefCountedBuffer
  // only.
  void ReturnUnusedBuffer(RefCountedBuffer* buffer);

  RefCountedBuffer buffers_[kNumBuffers];

  std::unique_ptr<InternalFrameBufferList> internal_frame_buffers_;

  // Frame buffer callbacks.
  GetFrameBufferCallback get_frame_buffer_;
  ReleaseFrameBufferCallback release_frame_buffer_;
  // Private data associated with the frame buffer callbacks.
  void* callback_private_data_;
};

}  // namespace libgav1

#endif  // LIBGAV1_SRC_BUFFER_POOL_H_
