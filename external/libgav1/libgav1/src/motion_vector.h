#ifndef LIBGAV1_SRC_MOTION_VECTOR_H_
#define LIBGAV1_SRC_MOTION_VECTOR_H_

#include <algorithm>
#include <array>
#include <cstdint>

#include "src/buffer_pool.h"
#include "src/obu_parser.h"
#include "src/tile.h"
#include "src/utils/array_2d.h"
#include "src/utils/constants.h"
#include "src/utils/types.h"

namespace libgav1 {

inline bool IsGlobalMvBlock(PredictionMode mode,
                            GlobalMotionTransformationType type,
                            BlockSize size) {
  return ((mode == kPredictionModeGlobalMv ||
           mode == kPredictionModeGlobalGlobalMv) &&
          type > kGlobalMotionTransformationTypeTranslation &&
          std::min(kBlockWidthPixels[size], kBlockHeightPixels[size]) >= 8);
}

// The |contexts| output parameter may be null. If the caller does not need
// the |contexts| output, pass nullptr as the argument.
void FindMvStack(
    const Tile::Block& block, bool is_compound,
    const std::array<bool, kNumReferenceFrameTypes>& reference_frame_sign_bias,
    const Array2D<TemporalMotionVector>& motion_field_mv,
    CandidateMotionVector ref_mv_stack[kMaxRefMvStackSize], int* num_mv_found,
    MvContexts* contexts,
    MotionVector global_mv[2]);  // 7.10.2

void FindWarpSamples(const Tile::Block& block, int* num_warp_samples,
                     int* num_samples_scanned,
                     int candidates[kMaxLeastSquaresSamples][4]);  // 7.10.4.

// Section 7.9.1 in the spec. But this is done per tile instead of for the whole
// frame.
void SetupMotionField(
    const ObuSequenceHeader& sequence_header,
    const ObuFrameHeader& frame_header, const RefCountedBuffer& current_frame,
    const std::array<RefCountedBufferPtr, kNumReferenceFrameTypes>&
        reference_frames,
    Array2D<TemporalMotionVector>* motion_field_mv, int row4x4_start,
    int row4x4_end, int column4x4_start, int column4x4_end);

}  // namespace libgav1

#endif  // LIBGAV1_SRC_MOTION_VECTOR_H_
