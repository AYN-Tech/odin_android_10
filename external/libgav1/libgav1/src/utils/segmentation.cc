#include "src/utils/segmentation.h"

namespace libgav1 {

const int8_t kSegmentationFeatureBits[kSegmentFeatureMax] = {8, 6, 6, 6,
                                                             6, 3, 0, 0};
const int kSegmentationFeatureMaxValues[kSegmentFeatureMax] = {
    255,
    kMaxLoopFilterValue,
    kMaxLoopFilterValue,
    kMaxLoopFilterValue,
    kMaxLoopFilterValue,
    7,
    0,
    0};

}  // namespace libgav1
