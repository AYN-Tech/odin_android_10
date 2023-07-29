#ifndef LIBGAV1_SRC_LOOP_RESTORATION_INFO_H_
#define LIBGAV1_SRC_LOOP_RESTORATION_INFO_H_

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "src/dsp/common.h"
#include "src/symbol_decoder_context.h"
#include "src/utils/constants.h"
#include "src/utils/entropy_decoder.h"

namespace libgav1 {

struct LoopRestorationUnitInfo {
  int row_start;
  int row_end;
  int column_start;
  int column_end;
};

class LoopRestorationInfo {
 public:
  LoopRestorationInfo(const LoopRestoration& loop_restoration, uint32_t width,
                      uint32_t height, int8_t subsampling_x,
                      int8_t subsampling_y, bool is_monochrome)
      : loop_restoration_(loop_restoration),
        width_(width),
        height_(height),
        subsampling_x_(subsampling_x),
        subsampling_y_(subsampling_y),
        is_monochrome_(is_monochrome) {}

  // Non copyable/movable.
  LoopRestorationInfo(const LoopRestorationInfo&) = delete;
  LoopRestorationInfo& operator=(const LoopRestorationInfo&) = delete;
  LoopRestorationInfo(LoopRestorationInfo&&) = delete;
  LoopRestorationInfo& operator=(LoopRestorationInfo&&) = delete;

  bool Allocate();
  // Populates the |unit_info| for the super block at |row4x4|, |column4x4|.
  // Returns true on success, false otherwise.
  bool PopulateUnitInfoForSuperBlock(Plane plane, BlockSize block_size,
                                     bool is_superres_scaled,
                                     uint8_t superres_scale_denominator,
                                     int row4x4, int column4x4,
                                     LoopRestorationUnitInfo* unit_info) const;
  void ReadUnitCoefficients(DaalaBitReader* reader,
                            SymbolDecoderContext* symbol_decoder_context,
                            Plane plane, int unit_id,
                            std::array<RestorationUnitInfo, kMaxPlanes>*
                                reference_unit_info);  // 5.11.58.
  void ReadWienerInfo(
      DaalaBitReader* reader, Plane plane, int unit_id,
      std::array<RestorationUnitInfo, kMaxPlanes>* reference_unit_info);
  void ReadSgrProjInfo(
      DaalaBitReader* reader, Plane plane, int unit_id,
      std::array<RestorationUnitInfo, kMaxPlanes>* reference_unit_info);

  // Getters.
  RestorationUnitInfo loop_restoration_info(Plane plane, int unit_id) const {
    return loop_restoration_info_[plane][unit_id];
  }

  int num_horizontal_units(Plane plane) const {
    return num_horizontal_units_[plane];
  }
  int num_vertical_units(Plane plane) const {
    return num_vertical_units_[plane];
  }
  int num_units(Plane plane) const { return num_units_[plane]; }

 private:
  // If plane_needs_filtering_[plane] is true, loop_restoration_info_[plane]
  // points to an array of num_units_[plane] elements.
  RestorationUnitInfo* loop_restoration_info_[kMaxPlanes];
  // Owns the memory that loop_restoration_info_[plane] points to.
  std::unique_ptr<RestorationUnitInfo[]> loop_restoration_info_buffer_;
  bool plane_needs_filtering_[kMaxPlanes];
  const LoopRestoration& loop_restoration_;
  uint32_t width_;
  uint32_t height_;
  int8_t subsampling_x_;
  int8_t subsampling_y_;
  bool is_monochrome_;
  int num_horizontal_units_[kMaxPlanes];
  int num_vertical_units_[kMaxPlanes];
  int num_units_[kMaxPlanes];
};

}  // namespace libgav1

#endif  // LIBGAV1_SRC_LOOP_RESTORATION_INFO_H_
