// Constants and utility functions used for overlap blend implementations.
// This will be included inside an anonymous namespace on files where these are
// necessary.

// This is a flat array of masks for each block dimension from 2 to 32. The
// starting index for each length is length-2.
constexpr uint8_t kObmcMask[62] = {
    // Obmc Mask 2
    45, 64,
    // Obmc Mask 4
    39, 50, 59, 64,
    // Obmc Mask 8
    36, 42, 48, 53, 57, 61, 64, 64,
    // Obmc Mask 16
    34, 37, 40, 43, 46, 49, 52, 54, 56, 58, 60, 61, 64, 64, 64, 64,
    // Obmc Mask 32
    33, 35, 36, 38, 40, 41, 43, 44, 45, 47, 48, 50, 51, 52, 53, 55, 56, 57, 58,
    59, 60, 60, 61, 62, 64, 64, 64, 64, 64, 64, 64, 64};
