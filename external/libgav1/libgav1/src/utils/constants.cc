#include "src/utils/constants.h"

namespace libgav1 {

const uint8_t k4x4WidthLog2[kMaxBlockSizes] = {0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
                                               2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5};

const uint8_t k4x4HeightLog2[kMaxBlockSizes] = {
    0, 1, 2, 0, 1, 2, 3, 0, 1, 2, 3, 4, 1, 2, 3, 4, 2, 3, 4, 5, 4, 5};

const uint8_t kNum4x4BlocksWide[kMaxBlockSizes] = {
    1, 1, 1, 2, 2, 2, 2, 4, 4, 4, 4, 4, 8, 8, 8, 8, 16, 16, 16, 16, 32, 32};

const uint8_t kNum4x4BlocksHigh[kMaxBlockSizes] = {
    1, 2, 4, 1, 2, 4, 8, 1, 2, 4, 8, 16, 2, 4, 8, 16, 4, 8, 16, 32, 16, 32};

const uint8_t kBlockWidthPixels[kMaxBlockSizes] = {
    4,  4,  4,  8,  8,  8,  8,  16, 16, 16,  16,
    16, 32, 32, 32, 32, 64, 64, 64, 64, 128, 128};

const uint8_t kBlockHeightPixels[kMaxBlockSizes] = {
    4,  8, 16, 4,  8,  16, 32, 4,  8,   16, 32,
    64, 8, 16, 32, 64, 16, 32, 64, 128, 64, 128};

// 9.3 -- Partition_Subsize[]
const BlockSize kSubSize[kMaxPartitionTypes][kMaxBlockSizes] = {
    // kPartitionNone
    {kBlock4x4,     kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock8x8,
     kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock16x16,
     kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock32x32,
     kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock64x64,   kBlockInvalid,
     kBlockInvalid, kBlock128x128},
    // kPartitionHorizontal
    {kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock8x4,
     kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock16x8,
     kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock32x16,
     kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock64x32,   kBlockInvalid,
     kBlockInvalid, kBlock128x64},
    // kPartitionVertical
    {kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock4x8,
     kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock8x16,
     kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock16x32,
     kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock32x64,   kBlockInvalid,
     kBlockInvalid, kBlock64x128},
    // kPartitionSplit
    {kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock4x4,
     kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock8x8,
     kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock16x16,
     kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock32x32,   kBlockInvalid,
     kBlockInvalid, kBlock64x64},
    // kPartitionHorizontalWithTopSplit
    {kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock8x4,
     kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock16x8,
     kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock32x16,
     kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock64x32,   kBlockInvalid,
     kBlockInvalid, kBlock128x64},
    // kPartitionHorizontalWithBottomSplit
    {kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock8x4,
     kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock16x8,
     kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock32x16,
     kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock64x32,   kBlockInvalid,
     kBlockInvalid, kBlock128x64},
    // kPartitionVerticalWithLeftSplit
    {kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock4x8,
     kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock8x16,
     kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock16x32,
     kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock32x64,   kBlockInvalid,
     kBlockInvalid, kBlock64x128},
    // kPartitionVerticalWithRightSplit
    {kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock4x8,
     kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock8x16,
     kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock16x32,
     kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock32x64,   kBlockInvalid,
     kBlockInvalid, kBlock64x128},
    // kPartitionHorizontal4
    {kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlockInvalid,
     kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock16x4,
     kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock32x8,
     kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock64x16,   kBlockInvalid,
     kBlockInvalid, kBlockInvalid},
    // kPartitionVertical4
    {kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlockInvalid,
     kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock4x16,
     kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock8x32,
     kBlockInvalid, kBlockInvalid, kBlockInvalid, kBlock16x64,   kBlockInvalid,
     kBlockInvalid, kBlockInvalid}};

// 5.11.38 (implemented as a simple look up. first dimension is block size,
// second and third are subsampling_x and subsampling_y).
const BlockSize kPlaneResidualSize[kMaxBlockSizes][2][2] = {
    {{kBlock4x4, kBlock4x4}, {kBlock4x4, kBlock4x4}},
    {{kBlock4x8, kBlock4x4}, {kBlockInvalid, kBlock4x4}},
    {{kBlock4x16, kBlock4x8}, {kBlockInvalid, kBlock4x8}},
    {{kBlock8x4, kBlockInvalid}, {kBlock4x4, kBlock4x4}},
    {{kBlock8x8, kBlock8x4}, {kBlock4x8, kBlock4x4}},
    {{kBlock8x16, kBlock8x8}, {kBlockInvalid, kBlock4x8}},
    {{kBlock8x32, kBlock8x16}, {kBlockInvalid, kBlock4x16}},
    {{kBlock16x4, kBlockInvalid}, {kBlock8x4, kBlock8x4}},
    {{kBlock16x8, kBlockInvalid}, {kBlock8x8, kBlock8x4}},
    {{kBlock16x16, kBlock16x8}, {kBlock8x16, kBlock8x8}},
    {{kBlock16x32, kBlock16x16}, {kBlockInvalid, kBlock8x16}},
    {{kBlock16x64, kBlock16x32}, {kBlockInvalid, kBlock8x32}},
    {{kBlock32x8, kBlockInvalid}, {kBlock16x8, kBlock16x4}},
    {{kBlock32x16, kBlockInvalid}, {kBlock16x16, kBlock16x8}},
    {{kBlock32x32, kBlock32x16}, {kBlock16x32, kBlock16x16}},
    {{kBlock32x64, kBlock32x32}, {kBlockInvalid, kBlock16x32}},
    {{kBlock64x16, kBlockInvalid}, {kBlock32x16, kBlock32x8}},
    {{kBlock64x32, kBlockInvalid}, {kBlock32x32, kBlock32x16}},
    {{kBlock64x64, kBlock64x32}, {kBlock32x64, kBlock32x32}},
    {{kBlock64x128, kBlock64x64}, {kBlockInvalid, kBlock32x64}},
    {{kBlock128x64, kBlockInvalid}, {kBlock64x64, kBlock64x32}},
    {{kBlock128x128, kBlock128x64}, {kBlock64x128, kBlock64x64}}};

const uint8_t kTransformWidth[kNumTransformSizes] = {
    4, 4, 4, 8, 8, 8, 8, 16, 16, 16, 16, 16, 32, 32, 32, 32, 64, 64, 64};

const uint8_t kTransformHeight[kNumTransformSizes] = {
    4, 8, 16, 4, 8, 16, 32, 4, 8, 16, 32, 64, 8, 16, 32, 64, 16, 32, 64};

const uint8_t kTransformWidth4x4[kNumTransformSizes] = {
    1, 1, 1, 2, 2, 2, 2, 4, 4, 4, 4, 4, 8, 8, 8, 8, 16, 16, 16};

const uint8_t kTransformHeight4x4[kNumTransformSizes] = {
    1, 2, 4, 1, 2, 4, 8, 1, 2, 4, 8, 16, 2, 4, 8, 16, 4, 8, 16};

const uint8_t kTransformWidthLog2[kNumTransformSizes] = {
    2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6};

const uint8_t kTransformHeightLog2[kNumTransformSizes] = {
    2, 3, 4, 2, 3, 4, 5, 2, 3, 4, 5, 6, 3, 4, 5, 6, 4, 5, 6};

// 9.3 -- Split_Tx_Size[]
const TransformSize kSplitTransformSize[kNumTransformSizes] = {
    kTransformSize4x4,   kTransformSize4x4,   kTransformSize4x8,
    kTransformSize4x4,   kTransformSize4x4,   kTransformSize8x8,
    kTransformSize8x16,  kTransformSize8x4,   kTransformSize8x8,
    kTransformSize8x8,   kTransformSize16x16, kTransformSize16x32,
    kTransformSize16x8,  kTransformSize16x16, kTransformSize16x16,
    kTransformSize32x32, kTransformSize32x16, kTransformSize32x32,
    kTransformSize32x32};

// Square transform of size min(w,h).
const TransformSize kTransformSizeSquareMin[kNumTransformSizes] = {
    kTransformSize4x4,   kTransformSize4x4,   kTransformSize4x4,
    kTransformSize4x4,   kTransformSize8x8,   kTransformSize8x8,
    kTransformSize8x8,   kTransformSize4x4,   kTransformSize8x8,
    kTransformSize16x16, kTransformSize16x16, kTransformSize16x16,
    kTransformSize8x8,   kTransformSize16x16, kTransformSize32x32,
    kTransformSize32x32, kTransformSize16x16, kTransformSize32x32,
    kTransformSize64x64};

// Square transform of size max(w,h).
const TransformSize kTransformSizeSquareMax[kNumTransformSizes] = {
    kTransformSize4x4,   kTransformSize8x8,   kTransformSize16x16,
    kTransformSize8x8,   kTransformSize8x8,   kTransformSize16x16,
    kTransformSize32x32, kTransformSize16x16, kTransformSize16x16,
    kTransformSize16x16, kTransformSize32x32, kTransformSize64x64,
    kTransformSize32x32, kTransformSize32x32, kTransformSize32x32,
    kTransformSize64x64, kTransformSize64x64, kTransformSize64x64,
    kTransformSize64x64};

const uint8_t kNumTransformTypesInSet[kNumTransformSets] = {1, 7, 5, 16, 12, 2};

const uint8_t kSgrProjParams[1 << kSgrProjParamsBits][4] = {
    {2, 12, 1, 4},  {2, 15, 1, 6},  {2, 18, 1, 8},  {2, 21, 1, 9},
    {2, 24, 1, 10}, {2, 29, 1, 11}, {2, 36, 1, 12}, {2, 45, 1, 13},
    {2, 56, 1, 14}, {2, 68, 1, 15}, {0, 0, 1, 5},   {0, 0, 1, 8},
    {0, 0, 1, 11},  {0, 0, 1, 14},  {2, 30, 0, 0},  {2, 75, 0, 0}};

const int8_t kSgrProjMultiplierMin[2] = {-96, -32};

const int8_t kSgrProjMultiplierMax[2] = {31, 95};

const int8_t kWienerTapsMin[3] = {-5, -23, -17};

const int8_t kWienerTapsMax[3] = {10, 8, 46};

const int16_t kUpscaleFilter[kSuperResFilterShifts][kSuperResFilterTaps] = {
    {0, 0, 0, 128, 0, 0, 0, 0},        {0, 0, -1, 128, 2, -1, 0, 0},
    {0, 1, -3, 127, 4, -2, 1, 0},      {0, 1, -4, 127, 6, -3, 1, 0},
    {0, 2, -6, 126, 8, -3, 1, 0},      {0, 2, -7, 125, 11, -4, 1, 0},
    {-1, 2, -8, 125, 13, -5, 2, 0},    {-1, 3, -9, 124, 15, -6, 2, 0},
    {-1, 3, -10, 123, 18, -6, 2, -1},  {-1, 3, -11, 122, 20, -7, 3, -1},
    {-1, 4, -12, 121, 22, -8, 3, -1},  {-1, 4, -13, 120, 25, -9, 3, -1},
    {-1, 4, -14, 118, 28, -9, 3, -1},  {-1, 4, -15, 117, 30, -10, 4, -1},
    {-1, 5, -16, 116, 32, -11, 4, -1}, {-1, 5, -16, 114, 35, -12, 4, -1},
    {-1, 5, -17, 112, 38, -12, 4, -1}, {-1, 5, -18, 111, 40, -13, 5, -1},
    {-1, 5, -18, 109, 43, -14, 5, -1}, {-1, 6, -19, 107, 45, -14, 5, -1},
    {-1, 6, -19, 105, 48, -15, 5, -1}, {-1, 6, -19, 103, 51, -16, 5, -1},
    {-1, 6, -20, 101, 53, -16, 6, -1}, {-1, 6, -20, 99, 56, -17, 6, -1},
    {-1, 6, -20, 97, 58, -17, 6, -1},  {-1, 6, -20, 95, 61, -18, 6, -1},
    {-2, 7, -20, 93, 64, -18, 6, -2},  {-2, 7, -20, 91, 66, -19, 6, -1},
    {-2, 7, -20, 88, 69, -19, 6, -1},  {-2, 7, -20, 86, 71, -19, 6, -1},
    {-2, 7, -20, 84, 74, -20, 7, -2},  {-2, 7, -20, 81, 76, -20, 7, -1},
    {-2, 7, -20, 79, 79, -20, 7, -2},  {-1, 7, -20, 76, 81, -20, 7, -2},
    {-2, 7, -20, 74, 84, -20, 7, -2},  {-1, 6, -19, 71, 86, -20, 7, -2},
    {-1, 6, -19, 69, 88, -20, 7, -2},  {-1, 6, -19, 66, 91, -20, 7, -2},
    {-2, 6, -18, 64, 93, -20, 7, -2},  {-1, 6, -18, 61, 95, -20, 6, -1},
    {-1, 6, -17, 58, 97, -20, 6, -1},  {-1, 6, -17, 56, 99, -20, 6, -1},
    {-1, 6, -16, 53, 101, -20, 6, -1}, {-1, 5, -16, 51, 103, -19, 6, -1},
    {-1, 5, -15, 48, 105, -19, 6, -1}, {-1, 5, -14, 45, 107, -19, 6, -1},
    {-1, 5, -14, 43, 109, -18, 5, -1}, {-1, 5, -13, 40, 111, -18, 5, -1},
    {-1, 4, -12, 38, 112, -17, 5, -1}, {-1, 4, -12, 35, 114, -16, 5, -1},
    {-1, 4, -11, 32, 116, -16, 5, -1}, {-1, 4, -10, 30, 117, -15, 4, -1},
    {-1, 3, -9, 28, 118, -14, 4, -1},  {-1, 3, -9, 25, 120, -13, 4, -1},
    {-1, 3, -8, 22, 121, -12, 4, -1},  {-1, 3, -7, 20, 122, -11, 3, -1},
    {-1, 2, -6, 18, 123, -10, 3, -1},  {0, 2, -6, 15, 124, -9, 3, -1},
    {0, 2, -5, 13, 125, -8, 2, -1},    {0, 1, -4, 11, 125, -7, 2, 0},
    {0, 1, -3, 8, 126, -6, 2, 0},      {0, 1, -3, 6, 127, -4, 1, 0},
    {0, 1, -2, 4, 127, -3, 1, 0},      {0, 0, -1, 2, 128, -1, 0, 0},
};

const int16_t kWarpedFilters[3 * kWarpedPixelPrecisionShifts + 1][8] = {
    // [-1, 0).
    {0, 0, 127, 1, 0, 0, 0, 0},
    {0, -1, 127, 2, 0, 0, 0, 0},
    {1, -3, 127, 4, -1, 0, 0, 0},
    {1, -4, 126, 6, -2, 1, 0, 0},
    {1, -5, 126, 8, -3, 1, 0, 0},
    {1, -6, 125, 11, -4, 1, 0, 0},
    {1, -7, 124, 13, -4, 1, 0, 0},
    {2, -8, 123, 15, -5, 1, 0, 0},
    {2, -9, 122, 18, -6, 1, 0, 0},
    {2, -10, 121, 20, -6, 1, 0, 0},
    {2, -11, 120, 22, -7, 2, 0, 0},
    {2, -12, 119, 25, -8, 2, 0, 0},
    {3, -13, 117, 27, -8, 2, 0, 0},
    {3, -13, 116, 29, -9, 2, 0, 0},
    {3, -14, 114, 32, -10, 3, 0, 0},
    {3, -15, 113, 35, -10, 2, 0, 0},
    {3, -15, 111, 37, -11, 3, 0, 0},
    {3, -16, 109, 40, -11, 3, 0, 0},
    {3, -16, 108, 42, -12, 3, 0, 0},
    {4, -17, 106, 45, -13, 3, 0, 0},
    {4, -17, 104, 47, -13, 3, 0, 0},
    {4, -17, 102, 50, -14, 3, 0, 0},
    {4, -17, 100, 52, -14, 3, 0, 0},
    {4, -18, 98, 55, -15, 4, 0, 0},
    {4, -18, 96, 58, -15, 3, 0, 0},
    {4, -18, 94, 60, -16, 4, 0, 0},
    {4, -18, 91, 63, -16, 4, 0, 0},
    {4, -18, 89, 65, -16, 4, 0, 0},
    {4, -18, 87, 68, -17, 4, 0, 0},
    {4, -18, 85, 70, -17, 4, 0, 0},
    {4, -18, 82, 73, -17, 4, 0, 0},
    {4, -18, 80, 75, -17, 4, 0, 0},
    {4, -18, 78, 78, -18, 4, 0, 0},
    {4, -17, 75, 80, -18, 4, 0, 0},
    {4, -17, 73, 82, -18, 4, 0, 0},
    {4, -17, 70, 85, -18, 4, 0, 0},
    {4, -17, 68, 87, -18, 4, 0, 0},
    {4, -16, 65, 89, -18, 4, 0, 0},
    {4, -16, 63, 91, -18, 4, 0, 0},
    {4, -16, 60, 94, -18, 4, 0, 0},
    {3, -15, 58, 96, -18, 4, 0, 0},
    {4, -15, 55, 98, -18, 4, 0, 0},
    {3, -14, 52, 100, -17, 4, 0, 0},
    {3, -14, 50, 102, -17, 4, 0, 0},
    {3, -13, 47, 104, -17, 4, 0, 0},
    {3, -13, 45, 106, -17, 4, 0, 0},
    {3, -12, 42, 108, -16, 3, 0, 0},
    {3, -11, 40, 109, -16, 3, 0, 0},
    {3, -11, 37, 111, -15, 3, 0, 0},
    {2, -10, 35, 113, -15, 3, 0, 0},
    {3, -10, 32, 114, -14, 3, 0, 0},
    {2, -9, 29, 116, -13, 3, 0, 0},
    {2, -8, 27, 117, -13, 3, 0, 0},
    {2, -8, 25, 119, -12, 2, 0, 0},
    {2, -7, 22, 120, -11, 2, 0, 0},
    {1, -6, 20, 121, -10, 2, 0, 0},
    {1, -6, 18, 122, -9, 2, 0, 0},
    {1, -5, 15, 123, -8, 2, 0, 0},
    {1, -4, 13, 124, -7, 1, 0, 0},
    {1, -4, 11, 125, -6, 1, 0, 0},
    {1, -3, 8, 126, -5, 1, 0, 0},
    {1, -2, 6, 126, -4, 1, 0, 0},
    {0, -1, 4, 127, -3, 1, 0, 0},
    {0, 0, 2, 127, -1, 0, 0, 0},
    // [0, 1).
    {0, 0, 0, 127, 1, 0, 0, 0},
    {0, 0, -1, 127, 2, 0, 0, 0},
    {0, 1, -3, 127, 4, -2, 1, 0},
    {0, 1, -5, 127, 6, -2, 1, 0},
    {0, 2, -6, 126, 8, -3, 1, 0},
    {-1, 2, -7, 126, 11, -4, 2, -1},
    {-1, 3, -8, 125, 13, -5, 2, -1},
    {-1, 3, -10, 124, 16, -6, 3, -1},
    {-1, 4, -11, 123, 18, -7, 3, -1},
    {-1, 4, -12, 122, 20, -7, 3, -1},
    {-1, 4, -13, 121, 23, -8, 3, -1},
    {-2, 5, -14, 120, 25, -9, 4, -1},
    {-1, 5, -15, 119, 27, -10, 4, -1},
    {-1, 5, -16, 118, 30, -11, 4, -1},
    {-2, 6, -17, 116, 33, -12, 5, -1},
    {-2, 6, -17, 114, 35, -12, 5, -1},
    {-2, 6, -18, 113, 38, -13, 5, -1},
    {-2, 7, -19, 111, 41, -14, 6, -2},
    {-2, 7, -19, 110, 43, -15, 6, -2},
    {-2, 7, -20, 108, 46, -15, 6, -2},
    {-2, 7, -20, 106, 49, -16, 6, -2},
    {-2, 7, -21, 104, 51, -16, 7, -2},
    {-2, 7, -21, 102, 54, -17, 7, -2},
    {-2, 8, -21, 100, 56, -18, 7, -2},
    {-2, 8, -22, 98, 59, -18, 7, -2},
    {-2, 8, -22, 96, 62, -19, 7, -2},
    {-2, 8, -22, 94, 64, -19, 7, -2},
    {-2, 8, -22, 91, 67, -20, 8, -2},
    {-2, 8, -22, 89, 69, -20, 8, -2},
    {-2, 8, -22, 87, 72, -21, 8, -2},
    {-2, 8, -21, 84, 74, -21, 8, -2},
    {-2, 8, -22, 82, 77, -21, 8, -2},
    {-2, 8, -21, 79, 79, -21, 8, -2},
    {-2, 8, -21, 77, 82, -22, 8, -2},
    {-2, 8, -21, 74, 84, -21, 8, -2},
    {-2, 8, -21, 72, 87, -22, 8, -2},
    {-2, 8, -20, 69, 89, -22, 8, -2},
    {-2, 8, -20, 67, 91, -22, 8, -2},
    {-2, 7, -19, 64, 94, -22, 8, -2},
    {-2, 7, -19, 62, 96, -22, 8, -2},
    {-2, 7, -18, 59, 98, -22, 8, -2},
    {-2, 7, -18, 56, 100, -21, 8, -2},
    {-2, 7, -17, 54, 102, -21, 7, -2},
    {-2, 7, -16, 51, 104, -21, 7, -2},
    {-2, 6, -16, 49, 106, -20, 7, -2},
    {-2, 6, -15, 46, 108, -20, 7, -2},
    {-2, 6, -15, 43, 110, -19, 7, -2},
    {-2, 6, -14, 41, 111, -19, 7, -2},
    {-1, 5, -13, 38, 113, -18, 6, -2},
    {-1, 5, -12, 35, 114, -17, 6, -2},
    {-1, 5, -12, 33, 116, -17, 6, -2},
    {-1, 4, -11, 30, 118, -16, 5, -1},
    {-1, 4, -10, 27, 119, -15, 5, -1},
    {-1, 4, -9, 25, 120, -14, 5, -2},
    {-1, 3, -8, 23, 121, -13, 4, -1},
    {-1, 3, -7, 20, 122, -12, 4, -1},
    {-1, 3, -7, 18, 123, -11, 4, -1},
    {-1, 3, -6, 16, 124, -10, 3, -1},
    {-1, 2, -5, 13, 125, -8, 3, -1},
    {-1, 2, -4, 11, 126, -7, 2, -1},
    {0, 1, -3, 8, 126, -6, 2, 0},
    {0, 1, -2, 6, 127, -5, 1, 0},
    {0, 1, -2, 4, 127, -3, 1, 0},
    {0, 0, 0, 2, 127, -1, 0, 0},
    // [1, 2).
    {0, 0, 0, 1, 127, 0, 0, 0},
    {0, 0, 0, -1, 127, 2, 0, 0},
    {0, 0, 1, -3, 127, 4, -1, 0},
    {0, 0, 1, -4, 126, 6, -2, 1},
    {0, 0, 1, -5, 126, 8, -3, 1},
    {0, 0, 1, -6, 125, 11, -4, 1},
    {0, 0, 1, -7, 124, 13, -4, 1},
    {0, 0, 2, -8, 123, 15, -5, 1},
    {0, 0, 2, -9, 122, 18, -6, 1},
    {0, 0, 2, -10, 121, 20, -6, 1},
    {0, 0, 2, -11, 120, 22, -7, 2},
    {0, 0, 2, -12, 119, 25, -8, 2},
    {0, 0, 3, -13, 117, 27, -8, 2},
    {0, 0, 3, -13, 116, 29, -9, 2},
    {0, 0, 3, -14, 114, 32, -10, 3},
    {0, 0, 3, -15, 113, 35, -10, 2},
    {0, 0, 3, -15, 111, 37, -11, 3},
    {0, 0, 3, -16, 109, 40, -11, 3},
    {0, 0, 3, -16, 108, 42, -12, 3},
    {0, 0, 4, -17, 106, 45, -13, 3},
    {0, 0, 4, -17, 104, 47, -13, 3},
    {0, 0, 4, -17, 102, 50, -14, 3},
    {0, 0, 4, -17, 100, 52, -14, 3},
    {0, 0, 4, -18, 98, 55, -15, 4},
    {0, 0, 4, -18, 96, 58, -15, 3},
    {0, 0, 4, -18, 94, 60, -16, 4},
    {0, 0, 4, -18, 91, 63, -16, 4},
    {0, 0, 4, -18, 89, 65, -16, 4},
    {0, 0, 4, -18, 87, 68, -17, 4},
    {0, 0, 4, -18, 85, 70, -17, 4},
    {0, 0, 4, -18, 82, 73, -17, 4},
    {0, 0, 4, -18, 80, 75, -17, 4},
    {0, 0, 4, -18, 78, 78, -18, 4},
    {0, 0, 4, -17, 75, 80, -18, 4},
    {0, 0, 4, -17, 73, 82, -18, 4},
    {0, 0, 4, -17, 70, 85, -18, 4},
    {0, 0, 4, -17, 68, 87, -18, 4},
    {0, 0, 4, -16, 65, 89, -18, 4},
    {0, 0, 4, -16, 63, 91, -18, 4},
    {0, 0, 4, -16, 60, 94, -18, 4},
    {0, 0, 3, -15, 58, 96, -18, 4},
    {0, 0, 4, -15, 55, 98, -18, 4},
    {0, 0, 3, -14, 52, 100, -17, 4},
    {0, 0, 3, -14, 50, 102, -17, 4},
    {0, 0, 3, -13, 47, 104, -17, 4},
    {0, 0, 3, -13, 45, 106, -17, 4},
    {0, 0, 3, -12, 42, 108, -16, 3},
    {0, 0, 3, -11, 40, 109, -16, 3},
    {0, 0, 3, -11, 37, 111, -15, 3},
    {0, 0, 2, -10, 35, 113, -15, 3},
    {0, 0, 3, -10, 32, 114, -14, 3},
    {0, 0, 2, -9, 29, 116, -13, 3},
    {0, 0, 2, -8, 27, 117, -13, 3},
    {0, 0, 2, -8, 25, 119, -12, 2},
    {0, 0, 2, -7, 22, 120, -11, 2},
    {0, 0, 1, -6, 20, 121, -10, 2},
    {0, 0, 1, -6, 18, 122, -9, 2},
    {0, 0, 1, -5, 15, 123, -8, 2},
    {0, 0, 1, -4, 13, 124, -7, 1},
    {0, 0, 1, -4, 11, 125, -6, 1},
    {0, 0, 1, -3, 8, 126, -5, 1},
    {0, 0, 1, -2, 6, 126, -4, 1},
    {0, 0, 0, -1, 4, 127, -3, 1},
    {0, 0, 0, 0, 2, 127, -1, 0},
    // dummy, replicate row index 191.
    {0, 0, 0, 0, 2, 127, -1, 0}};

const int16_t kSubPixelFilters[6][16][8] = {{{0, 0, 0, 128, 0, 0, 0, 0},
                                             {0, 2, -6, 126, 8, -2, 0, 0},
                                             {0, 2, -10, 122, 18, -4, 0, 0},
                                             {0, 2, -12, 116, 28, -8, 2, 0},
                                             {0, 2, -14, 110, 38, -10, 2, 0},
                                             {0, 2, -14, 102, 48, -12, 2, 0},
                                             {0, 2, -16, 94, 58, -12, 2, 0},
                                             {0, 2, -14, 84, 66, -12, 2, 0},
                                             {0, 2, -14, 76, 76, -14, 2, 0},
                                             {0, 2, -12, 66, 84, -14, 2, 0},
                                             {0, 2, -12, 58, 94, -16, 2, 0},
                                             {0, 2, -12, 48, 102, -14, 2, 0},
                                             {0, 2, -10, 38, 110, -14, 2, 0},
                                             {0, 2, -8, 28, 116, -12, 2, 0},
                                             {0, 0, -4, 18, 122, -10, 2, 0},
                                             {0, 0, -2, 8, 126, -6, 2, 0}},
                                            {{0, 0, 0, 128, 0, 0, 0, 0},
                                             {0, 2, 28, 62, 34, 2, 0, 0},
                                             {0, 0, 26, 62, 36, 4, 0, 0},
                                             {0, 0, 22, 62, 40, 4, 0, 0},
                                             {0, 0, 20, 60, 42, 6, 0, 0},
                                             {0, 0, 18, 58, 44, 8, 0, 0},
                                             {0, 0, 16, 56, 46, 10, 0, 0},
                                             {0, -2, 16, 54, 48, 12, 0, 0},
                                             {0, -2, 14, 52, 52, 14, -2, 0},
                                             {0, 0, 12, 48, 54, 16, -2, 0},
                                             {0, 0, 10, 46, 56, 16, 0, 0},
                                             {0, 0, 8, 44, 58, 18, 0, 0},
                                             {0, 0, 6, 42, 60, 20, 0, 0},
                                             {0, 0, 4, 40, 62, 22, 0, 0},
                                             {0, 0, 4, 36, 62, 26, 0, 0},
                                             {0, 0, 2, 34, 62, 28, 2, 0}},
                                            {{0, 0, 0, 128, 0, 0, 0, 0},
                                             {-2, 2, -6, 126, 8, -2, 2, 0},
                                             {-2, 6, -12, 124, 16, -6, 4, -2},
                                             {-2, 8, -18, 120, 26, -10, 6, -2},
                                             {-4, 10, -22, 116, 38, -14, 6, -2},
                                             {-4, 10, -22, 108, 48, -18, 8, -2},
                                             {-4, 10, -24, 100, 60, -20, 8, -2},
                                             {-4, 10, -24, 90, 70, -22, 10, -2},
                                             {-4, 12, -24, 80, 80, -24, 12, -4},
                                             {-2, 10, -22, 70, 90, -24, 10, -4},
                                             {-2, 8, -20, 60, 100, -24, 10, -4},
                                             {-2, 8, -18, 48, 108, -22, 10, -4},
                                             {-2, 6, -14, 38, 116, -22, 10, -4},
                                             {-2, 6, -10, 26, 120, -18, 8, -2},
                                             {-2, 4, -6, 16, 124, -12, 6, -2},
                                             {0, 2, -2, 8, 126, -6, 2, -2}},
                                            {{0, 0, 0, 128, 0, 0, 0, 0},
                                             {0, 0, 0, 120, 8, 0, 0, 0},
                                             {0, 0, 0, 112, 16, 0, 0, 0},
                                             {0, 0, 0, 104, 24, 0, 0, 0},
                                             {0, 0, 0, 96, 32, 0, 0, 0},
                                             {0, 0, 0, 88, 40, 0, 0, 0},
                                             {0, 0, 0, 80, 48, 0, 0, 0},
                                             {0, 0, 0, 72, 56, 0, 0, 0},
                                             {0, 0, 0, 64, 64, 0, 0, 0},
                                             {0, 0, 0, 56, 72, 0, 0, 0},
                                             {0, 0, 0, 48, 80, 0, 0, 0},
                                             {0, 0, 0, 40, 88, 0, 0, 0},
                                             {0, 0, 0, 32, 96, 0, 0, 0},
                                             {0, 0, 0, 24, 104, 0, 0, 0},
                                             {0, 0, 0, 16, 112, 0, 0, 0},
                                             {0, 0, 0, 8, 120, 0, 0, 0}},
                                            {{0, 0, 0, 128, 0, 0, 0, 0},
                                             {0, 0, -4, 126, 8, -2, 0, 0},
                                             {0, 0, -8, 122, 18, -4, 0, 0},
                                             {0, 0, -10, 116, 28, -6, 0, 0},
                                             {0, 0, -12, 110, 38, -8, 0, 0},
                                             {0, 0, -12, 102, 48, -10, 0, 0},
                                             {0, 0, -14, 94, 58, -10, 0, 0},
                                             {0, 0, -12, 84, 66, -10, 0, 0},
                                             {0, 0, -12, 76, 76, -12, 0, 0},
                                             {0, 0, -10, 66, 84, -12, 0, 0},
                                             {0, 0, -10, 58, 94, -14, 0, 0},
                                             {0, 0, -10, 48, 102, -12, 0, 0},
                                             {0, 0, -8, 38, 110, -12, 0, 0},
                                             {0, 0, -6, 28, 116, -10, 0, 0},
                                             {0, 0, -4, 18, 122, -8, 0, 0},
                                             {0, 0, -2, 8, 126, -4, 0, 0}},
                                            {{0, 0, 0, 128, 0, 0, 0, 0},
                                             {0, 0, 30, 62, 34, 2, 0, 0},
                                             {0, 0, 26, 62, 36, 4, 0, 0},
                                             {0, 0, 22, 62, 40, 4, 0, 0},
                                             {0, 0, 20, 60, 42, 6, 0, 0},
                                             {0, 0, 18, 58, 44, 8, 0, 0},
                                             {0, 0, 16, 56, 46, 10, 0, 0},
                                             {0, 0, 14, 54, 48, 12, 0, 0},
                                             {0, 0, 12, 52, 52, 12, 0, 0},
                                             {0, 0, 12, 48, 54, 14, 0, 0},
                                             {0, 0, 10, 46, 56, 16, 0, 0},
                                             {0, 0, 8, 44, 58, 18, 0, 0},
                                             {0, 0, 6, 42, 60, 20, 0, 0},
                                             {0, 0, 4, 40, 62, 22, 0, 0},
                                             {0, 0, 4, 36, 62, 26, 0, 0},
                                             {0, 0, 2, 34, 62, 30, 0, 0}}};

// 9.3 -- Dr_Intra_Derivative[]
// This is a more compact version of the table from the spec. angle / 2 - 1 is
// used as the lookup. Note angle / 3 - 1 would work too, but the calculation
// becomes more costly.
const int16_t kDirectionalIntraPredictorDerivative[44] = {
    //              Approx angle
    1023, 0,     // 3, ...
    547,         // 6, ...
    372,  0, 0,  // 9, ...
    273,         // 14, ...
    215,  0,     // 17, ...
    178,         // 20, ...
    151,  0,     // 23, ... (113 & 203 are base angles)
    132,         // 26, ...
    116,  0,     // 29, ...
    102,  0,     // 32, ...
    90,          // 36, ...
    80,   0,     // 39, ...
    71,          // 42, ...
    64,   0,     // 45, ... (45 & 135 are base angles)
    57,          // 48, ...
    51,   0,     // 51, ...
    45,   0,     // 54, ...
    40,          // 58, ...
    35,   0,     // 61, ...
    31,          // 64, ...
    27,   0,     // 67, ... (67 & 157 are base angles)
    23,          // 70, ...
    19,   0,     // 73, ...
    15,   0,     // 76, ...
    11,   0,     // 81, ...
    7,           // 84, ...
    3,           // 87, ...
};

const uint8_t kDeblockFilterLevelIndex[kMaxPlanes][kNumLoopFilterTypes] = {
    {0, 1}, {2, 2}, {3, 3}};

const int8_t kMaskIdLookup[4][kMaxBlockSizes] = {
    // transform size 4x4.
    {0,  1,  13, 2, 3,  4,  15, 14, 5,  6,  7,
     17, 16, 8,  9, 10, 18, 11, 12, -1, -1, -1},
    // transform size 8x8.
    {-1, -1, -1, -1, 19, 20, 29, -1, 21, 22, 23,
     31, 30, 24, 25, 26, 32, 27, 28, -1, -1, -1},
    // transform size 16x16.
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, 33, 34,
     40, -1, 35, 36, 37, 41, 38, 39, -1, -1, -1},
    // transform size 32x32.
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, 42, 43, -1, 44, 45, -1, -1, -1},
};

const int8_t kVerticalBorderMaskIdLookup[kMaxBlockSizes] = {
    0,  47, 61, 49, 19, 51, 63, 62, 53, 33, 55,
    65, 64, 57, 42, 59, 66, 60, 46, -1, -1, -1};

const uint64_t kTopMaskLookup[67][4] = {
    // transform size 4X4
    {0x0000000000000001ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 4X4, transform size 4X4
    {0x0000000000010001ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 4X8, transform size 4X4
    {0x0000000000000003ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 8X4, transform size 4X4
    {0x0000000000030003ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 8X8, transform size 4X4
    {0x0003000300030003ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 8X16, transform size 4X4
    {0x00000000000f000fULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 16X8, transform size 4X4
    {0x000f000f000f000fULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 16X16, transform size 4X4
    {0x000f000f000f000fULL, 0x000f000f000f000fULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 16X32, transform size 4X4
    {0x00ff00ff00ff00ffULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 32X16, transform size 4X4
    {0x00ff00ff00ff00ffULL, 0x00ff00ff00ff00ffULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 32X32, transform size 4X4
    {0x00ff00ff00ff00ffULL, 0x00ff00ff00ff00ffULL, 0x00ff00ff00ff00ffULL,
     0x00ff00ff00ff00ffULL},  // block size 32X64, transform size 4X4
    {0xffffffffffffffffULL, 0xffffffffffffffffULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 64X32, transform size 4X4
    {0xffffffffffffffffULL, 0xffffffffffffffffULL, 0xffffffffffffffffULL,
     0xffffffffffffffffULL},  // block size 64X64, transform size 4x4
    {0x0001000100010001ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 4X16, transform size 4X4
    {0x000000000000000fULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 16X4, transform size 4X4
    {0x0003000300030003ULL, 0x0003000300030003ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 8X32, transform size 4X4
    {0x0000000000ff00ffULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 32X8, transform size 4X4
    {0x000f000f000f000fULL, 0x000f000f000f000fULL, 0x000f000f000f000fULL,
     0x000f000f000f000fULL},  // block size 16X64, transform size 4X4
    {0xffffffffffffffffULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 64X16, transform size 4X4
    // transform size 8X8
    {0x0000000000000003ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 8X8, transform size 8X8
    {0x0000000300000003ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 8X16, transform size 8X8
    {0x000000000000000fULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 16X8, transform size 8X8
    {0x0000000f0000000fULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 16X16, transform size 8X8
    {0x0000000f0000000fULL, 0x0000000f0000000fULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 16X32, transform size 8X8
    {0x000000ff000000ffULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 32X16, transform size 8X8
    {0x000000ff000000ffULL, 0x000000ff000000ffULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 32X32, transform size 8X8
    {0x000000ff000000ffULL, 0x000000ff000000ffULL, 0x000000ff000000ffULL,
     0x000000ff000000ffULL},  // block size 32X64, transform size 8X8
    {0x0000ffff0000ffffULL, 0x0000ffff0000ffffULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 64X32, transform size 8X8
    {0x0000ffff0000ffffULL, 0x0000ffff0000ffffULL, 0x0000ffff0000ffffULL,
     0x0000ffff0000ffffULL},  // block size 64X64, transform size 8X8
    {0x0000000300000003ULL, 0x0000000300000003ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 8X32, transform size 8X8
    {0x00000000000000ffULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 32X8, transform size 8X8
    {0x0000000f0000000fULL, 0x0000000f0000000fULL, 0x0000000f0000000fULL,
     0x0000000f0000000fULL},  // block size 16X64, transform size 8X8
    {0x0000ffff0000ffffULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 64X16, transform size 8X8
    // transform size 16X16
    {0x000000000000000fULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 16X16, transform size 16X16
    {0x000000000000000fULL, 0x000000000000000fULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 16X32, transform size 16X16
    {0x00000000000000ffULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 32X16, transform size 16X16
    {0x00000000000000ffULL, 0x00000000000000ffULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 32X32, transform size 16X16
    {0x00000000000000ffULL, 0x00000000000000ffULL, 0x00000000000000ffULL,
     0x00000000000000ffULL},  // block size 32X64, transform size 16X16
    {0x000000000000ffffULL, 0x000000000000ffffULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 64X32, transform size 16X16
    {0x000000000000ffffULL, 0x000000000000ffffULL, 0x000000000000ffffULL,
     0x000000000000ffffULL},  // block size 64X64, transform size 16X16
    {0x000000000000000fULL, 0x000000000000000fULL, 0x000000000000000fULL,
     0x000000000000000fULL},  // block size 16X64, transform size 16X16
    {0x000000000000ffffULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 64X16, transform size 16X16
    // transform size 32X32
    {0x00000000000000ffULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 32X32, transform size 32X32
    {0x00000000000000ffULL, 0x0000000000000000ULL, 0x00000000000000ffULL,
     0x0000000000000000ULL},  // block size 32X64, transform size 32X32
    {0x000000000000ffffULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 64X32, transform size 32X32
    {0x000000000000ffffULL, 0x0000000000000000ULL, 0x000000000000ffffULL,
     0x0000000000000000ULL},  // block size 64X64, transform size 32X32
    // transform size 64X64
    {0x000000000000ffffULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 64X64, transform size 64X64
    // 2:1, 1:2 transform sizes.
    {0x0000000000000001ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 4X8, transform size 4X8
    {0x0000000100000001ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 4X16, transform size 4X8
    {0x0000000000000003ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 8X4, transform size 8X4
    {0x000000000000000fULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 16X4, transform size 8X4
    {0x0000000000000003ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 8X16, transform size 8X16
    {0x0000000000000003ULL, 0x0000000000000003ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 8X32, transform size 8X16
    {0x000000000000000fULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 16X8, transform size 16X8
    {0x00000000000000ffULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 32X8, transform size 16X8
    {0x000000000000000fULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 16X32, transform size 16X32
    {0x000000000000000fULL, 0x0000000000000000ULL, 0x000000000000000fULL,
     0x0000000000000000ULL},  // block size 16X64, transform size 16X32
    {0x00000000000000ffULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 32X16, transform size 32X16
    {0x000000000000ffffULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 64X16, transform size 32X16
    {0x00000000000000ffULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 32X64, transform size 32X64
    {0x000000000000ffffULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 64X32, transform size 64X32
    // 4:1, 1:4 transform sizes.
    {0x0000000000000001ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 4X16, transform size 4X16
    {0x000000000000000fULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 16X4, transform size 16X4
    {0x0000000000000003ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 8X32, transform size 8X32
    {0x00000000000000ffULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 32X8, transform size 32X8
    {0x000000000000000fULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 16X64, transform size 16X64
    {0x000000000000ffffULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 64X16, transform size 64X16
};

const uint64_t kLeftMaskLookup[67][4] = {
    // transform size 4X4
    {0x0000000000000001ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 4X4, transform size 4X4
    {0x0000000000010001ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 4X8, transform size 4X4
    {0x0000000000000003ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 8X4, transform size 4X4
    {0x0000000000030003ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 8X8, transform size 4X4
    {0x0003000300030003ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 8X16, transform size 4X4
    {0x00000000000f000fULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 16X8, transform size 4X4
    {0x000f000f000f000fULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 16X16, transform size 4X4
    {0x000f000f000f000fULL, 0x000f000f000f000fULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 16X32, transform size 4X4
    {0x00ff00ff00ff00ffULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 32X16, transform size 4X4
    {0x00ff00ff00ff00ffULL, 0x00ff00ff00ff00ffULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 32X32, transform size 4X4
    {0x00ff00ff00ff00ffULL, 0x00ff00ff00ff00ffULL, 0x00ff00ff00ff00ffULL,
     0x00ff00ff00ff00ffULL},  // block size 32X64, transform size 4X4
    {0xffffffffffffffffULL, 0xffffffffffffffffULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 64X32, transform size 4X4
    {0xffffffffffffffffULL, 0xffffffffffffffffULL, 0xffffffffffffffffULL,
     0xffffffffffffffffULL},  // block size 64X64, transform size 4X4
    {0x0001000100010001ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 4X16, transform size 4X4
    {0x000000000000000fULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 16X4, transform size 4X4
    {0x0003000300030003ULL, 0x0003000300030003ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 8X32, transform size 4X4
    {0x0000000000ff00ffULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 32X8, transform size 4X4
    {0x000f000f000f000fULL, 0x000f000f000f000fULL, 0x000f000f000f000fULL,
     0x000f000f000f000fULL},  // block size 16X64, transform size 4X4
    {0xffffffffffffffffULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 64X16, transform size 4X4
    // transform size 8X8
    {0x0000000000010001ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 8X8, transform size 8X8
    {0x0001000100010001ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 8X16, transform size 8X8
    {0x0000000000050005ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 16X8, transform size 8X8
    {0x0005000500050005ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 16X16, transform size 8X8
    {0x0005000500050005ULL, 0x0005000500050005ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 16X32, transform size 8X8
    {0x0055005500550055ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 32X16, transform size 8X8
    {0x0055005500550055ULL, 0x0055005500550055ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 32X32, transform size 8X8
    {0x0055005500550055ULL, 0x0055005500550055ULL, 0x0055005500550055ULL,
     0x0055005500550055ULL},  // block size 32X64, transform size 8X8
    {0x5555555555555555ULL, 0x5555555555555555ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 64X32, transform size 8X8
    {0x5555555555555555ULL, 0x5555555555555555ULL, 0x5555555555555555ULL,
     0x5555555555555555ULL},  // block size 64X64, transform size 8X8
    {0x0001000100010001ULL, 0x0001000100010001ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 8X32, transform size 8X8
    {0x0000000000550055ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 32X8, transform size 8X8
    {0x0005000500050005ULL, 0x0005000500050005ULL, 0x0005000500050005ULL,
     0x0005000500050005ULL},  // block size 16X64, transform size 8X8
    {0x5555555555555555ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 64X16, transform size 8X8
    // transform size 16X16
    {0x0001000100010001ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 16X16, transform size 16X16
    {0x0001000100010001ULL, 0x0001000100010001ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 16X32, transform size 16X16
    {0x0011001100110011ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 32X16, transform size 16X16
    {0x0011001100110011ULL, 0x0011001100110011ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 32X32, transform size 16X16
    {0x0011001100110011ULL, 0x0011001100110011ULL, 0x0011001100110011ULL,
     0x0011001100110011ULL},  // block size 32X64, transform size 16X16
    {0x1111111111111111ULL, 0x1111111111111111ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 64X32, transform size 16X16
    {0x1111111111111111ULL, 0x1111111111111111ULL, 0x1111111111111111ULL,
     0x1111111111111111ULL},  // block size 64X64, transform size 16X16
    {0x0001000100010001ULL, 0x0001000100010001ULL, 0x0001000100010001ULL,
     0x0001000100010001ULL},  // block size 16X64, transform size 16X16
    {0x1111111111111111ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 64X16, transform size 16X16
    // transform size 32X32
    {0x0001000100010001ULL, 0x0001000100010001ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 32X32, transform size 32X32
    {0x0101010101010101ULL, 0x0101010101010101ULL, 0x0101010101010101ULL,
     0x0101010101010101ULL},  // block size 32X64, transform size 32X32
    {0x0101010101010101ULL, 0x0101010101010101ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 64X32, transform size 32X32
    {0x0101010101010101ULL, 0x0101010101010101ULL, 0x0101010101010101ULL,
     0x0101010101010101ULL},  // block size 64X64, transform size 32X32
    // transform size 64X64
    {0x0001000100010001ULL, 0x0001000100010001ULL, 0x0001000100010001ULL,
     0x0001000100010001ULL},  // block size 64X64, transform size 64X64
    // 2:1, 1:2 transform sizes.
    {0x0000000000010001ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 4X8, transform size 4X8
    {0x0001000100010001ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 4X16, transform size 4X8
    {0x0000000000000001ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 8X4, transform size 8X4
    {0x0000000000000005ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 16X4, transform size 8X4
    {0x0001000100010001ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 8X16, transform size 8X16
    {0x0001000100010001ULL, 0x0001000100010001ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 8X32, transform size 8X16
    {0x0000000000010001ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 16X8, transform size 16X8
    {0x0000000000110011ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 32X8, transform size 16X8
    {0x0001000100010001ULL, 0x0001000100010001ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 16X32, transform size 16X32
    {0x0001000100010001ULL, 0x0001000100010001ULL, 0x0001000100010001ULL,
     0x0001000100010001ULL},  // block size 16X64, transform size 16X32
    {0x0001000100010001ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 32X16, transform size 32X16
    {0x0101010101010101ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 64X16, transform size 32X16
    {0x0001000100010001ULL, 0x0001000100010001ULL, 0x0001000100010001ULL,
     0x0001000100010001ULL},  // block size 32X64, transform size 32X64
    {0x0001000100010001ULL, 0x0001000100010001ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 64X32, transform size 64X32
    // 4:1, 1:4 transform sizes.
    {0x0001000100010001ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 4X16, transform size 4X16
    {0x0000000000000001ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 16X4, transform size 16X4
    {0x0001000100010001ULL, 0x0001000100010001ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 8X32, transform size 8X32
    {0x0000000000010001ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 32X8, transform size 32X8
    {0x0001000100010001ULL, 0x0001000100010001ULL, 0x0001000100010001ULL,
     0x0001000100010001ULL},  // block size 16X64, transform size 16X64
    {0x0001000100010001ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
     0x0000000000000000ULL},  // block size 64X16, transform size 64X16
};

}  // namespace libgav1
