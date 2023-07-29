#ifndef LIBGAV1_SRC_UTILS_TYPES_H_
#define LIBGAV1_SRC_UTILS_TYPES_H_

#include <cstdint>
#include <memory>

#include "src/utils/array_2d.h"
#include "src/utils/constants.h"
#include "src/utils/memory.h"

namespace libgav1 {

struct MotionVector : public Allocable {
  static const int kRow = 0;
  static const int kColumn = 1;

  bool operator==(const MotionVector& rhs) {
    return mv[0] == rhs.mv[0] && mv[1] == rhs.mv[1];
  }

  int mv[2];
};

struct CandidateMotionVector {
  MotionVector mv[2];
  int weight;
};

// Stores the motion vector used for motion field estimation.
struct TemporalMotionVector : public Allocable {
  MotionVector mv;
  int reference_offset;
};

// MvContexts contains the contexts used to decode portions of an inter block
// mode info to set the y_mode field in BlockParameters.
//
// The contexts in the struct correspond to the ZeroMvContext, RefMvContext,
// and NewMvContext variables in the spec.
struct MvContexts {
  int zero_mv;
  int reference_mv;
  int new_mv;
};

struct PaletteModeInfo {
  uint8_t size[kNumPlaneTypes];
  uint16_t color[kMaxPlanes][kMaxPaletteSize];
};

// Stores the parameters used by the prediction process. The members of the
// struct are filled in when parsing the bitstream and used when the prediction
// is computed. The information in this struct is associated with a single
// block.
// While both BlockParameters and PredictionParameters store information
// pertaining to a Block, the only difference is that BlockParameters outlives
// the block itself (for example, some of the variables in BlockParameters are
// used to compute the context for reading elements in the subsequent blocks).
struct PredictionParameters : public Allocable {
  bool use_filter_intra;
  FilterIntraPredictor filter_intra_mode;
  int angle_delta[kNumPlaneTypes];
  int8_t cfl_alpha_u;
  int8_t cfl_alpha_v;
  int max_luma_width;
  int max_luma_height;
  Array2D<uint8_t> color_index_map[kNumPlaneTypes];
  bool use_intra_block_copy;
  InterIntraMode inter_intra_mode;
  bool is_wedge_inter_intra;
  int wedge_index;
  int wedge_sign;
  bool mask_is_inverse;
  MotionMode motion_mode;
  CompoundPredictionType compound_prediction_type;
  CandidateMotionVector ref_mv_stack[kMaxRefMvStackSize];
  int ref_mv_count;
  int ref_mv_index;
  MotionVector global_mv[2];
  int num_warp_samples;
  int warp_estimate_candidates[kMaxLeastSquaresSamples][4];
};

// A lot of BlockParameters objects are created, so the smallest type is used
// for each field. The ranges of some fields are documented to justify why
// their types are large enough.
struct BlockParameters : public Allocable {
  BlockSize size;
  // segment_id is in the range [0, 7].
  int8_t segment_id;
  bool use_predicted_segment_id;  // only valid with temporal update enabled.
  bool skip;
  // True means that this block will use some default settings (that
  // correspond to compound prediction) and so most of the mode info is
  // skipped. False means that the mode info is not skipped.
  bool skip_mode;
  bool is_inter;
  PredictionMode y_mode;
  PredictionMode uv_mode;
  TransformSize transform_size;
  TransformSize uv_transform_size;
  PaletteModeInfo palette_mode_info;
  ReferenceFrameType reference_frame[2];
  MotionVector mv[2];
  bool is_explicit_compound_type;  // comp_group_idx in the spec.
  bool is_compound_type_average;   // compound_idx in the spec.
  InterpolationFilter interpolation_filter[2];
  // The index of this array is as follows:
  //  0 - Y plane vertical filtering.
  //  1 - Y plane horizontal filtering.
  //  2 - U plane (both directions).
  //  3 - V plane (both directions).
  uint8_t deblock_filter_level[kFrameLfCount];
  // When |Tile::split_parse_and_decode_| is true, each block gets its own
  // instance of |prediction_parameters|. When it is false, all the blocks point
  // to |Tile::prediction_parameters_|. This field is valid only as long as the
  // block is *being* decoded. The lifetime and usage of this field can be
  // better understood by following its flow in tile.cc.
  std::unique_ptr<PredictionParameters> prediction_parameters;
};

}  // namespace libgav1
#endif  // LIBGAV1_SRC_UTILS_TYPES_H_
