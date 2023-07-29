#ifndef LIBGAV1_SRC_DECODER_SCRATCH_BUFFER_H_
#define LIBGAV1_SRC_DECODER_SCRATCH_BUFFER_H_

#include <cstdint>
#include <mutex>  // NOLINT (unapproved c++11 header)

#include "src/dsp/constants.h"
#include "src/utils/compiler_attributes.h"
#include "src/utils/constants.h"
#include "src/utils/memory.h"
#include "src/utils/stack.h"

namespace libgav1 {

// Buffer to facilitate decoding a superblock.
struct DecoderScratchBuffer : public Allocable {
  static constexpr int kBlockDecodedStride = 34;

 private:
#if LIBGAV1_MAX_BITDEPTH >= 10
  static constexpr int kPixelSize = 2;
#else
  static constexpr int kPixelSize = 1;
#endif

 public:
  // The following prediction modes need a prediction mask:
  // kCompoundPredictionTypeDiffWeighted, kCompoundPredictionTypeWedge,
  // kCompoundPredictionTypeIntra. They are mutually exclusive. This buffer is
  // used to store the prediction mask during the inter prediction process. The
  // mask only needs to be created for the Y plane and is used for the U & V
  // planes.
  alignas(kMaxAlignment) uint8_t
      prediction_mask[kMaxSuperBlockSizeSquareInPixels];

  // For each instance of the DecoderScratchBuffer, only one of the following
  // buffers will be used at any given time, so it is ok to share them in a
  // union.
  union {
    // Union usage note: This is used only by functions in the "inter"
    // prediction path.
    //
    // Buffers used for inter prediction process.
    alignas(kMaxAlignment) uint16_t
        prediction_buffer[2][kMaxSuperBlockSizeSquareInPixels];

    struct {
      // Union usage note: This is used only by functions in the "intra"
      // prediction path.
      //
      // Buffer used for storing subsampled luma samples needed for CFL
      // prediction. This buffer is used to avoid repetition of the subsampling
      // for the V plane when it is already done for the U plane.
      int16_t cfl_luma_buffer[kCflLumaBufferStride][kCflLumaBufferStride];

      // Union usage note: This is used only by the
      // Tile::ReadTransformCoefficients() function (and the helper functions
      // that it calls). This cannot be shared with |cfl_luma_buffer| since
      // |cfl_luma_buffer| has to live across the 3 plane loop in
      // Tile::TransformBlock.
      //
      // Buffer used by Tile::ReadTransformCoefficients() to store the quantized
      // coefficients until the dequantization process is performed.
      int32_t quantized_buffer[kQuantizedCoefficientBufferSize];
    };
  };

  // Buffer used for convolve. The maximum size required for this buffer is:
  //  maximum block height (with scaling) = 2 * 128 = 256.
  //  maximum block stride (with scaling and border aligned to 16) =
  //     (2 * 128 + 7 + 9) * pixel_size = 272 * pixel_size.
  alignas(kMaxAlignment) uint8_t
      convolve_block_buffer[256 * 272 * DecoderScratchBuffer::kPixelSize];

  // Flag indicating whether the data in |cfl_luma_buffer| is valid.
  bool cfl_luma_buffer_valid;

  // Equivalent to BlockDecoded array in the spec. This stores the decoded
  // state of every 4x4 block in a superblock. It has 1 row/column border on
  // all 4 sides (hence the 34x34 dimension instead of 32x32). Note that the
  // spec uses "-1" as an index to access the left and top borders. In the
  // code, we treat the index (1, 1) as equivalent to the spec's (0, 0). So
  // all accesses into this array will be offset by +1 when compared with the
  // spec.
  bool block_decoded[kMaxPlanes][kBlockDecodedStride][kBlockDecodedStride];
};

class DecoderScratchBufferPool {
 public:
  std::unique_ptr<DecoderScratchBuffer> Get() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (buffers_.Empty()) {
      std::unique_ptr<DecoderScratchBuffer> scratch_buffer(
          new (std::nothrow) DecoderScratchBuffer);
      return scratch_buffer;
    }
    return buffers_.Pop();
  }

  void Release(std::unique_ptr<DecoderScratchBuffer> scratch_buffer) {
    std::lock_guard<std::mutex> lock(mutex_);
    buffers_.Push(std::move(scratch_buffer));
  }

 private:
  std::mutex mutex_;
  // We will never need more than kMaxThreads scratch buffers since that is the
  // maximum amount of work that will be done at any given time.
  Stack<std::unique_ptr<DecoderScratchBuffer>, kMaxThreads> buffers_
      LIBGAV1_GUARDED_BY(mutex_);
};

}  // namespace libgav1

#endif  // LIBGAV1_SRC_DECODER_SCRATCH_BUFFER_H_
