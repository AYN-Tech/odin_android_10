#include "src/dsp/convolve.h"
#include "src/dsp/dsp.h"

#if LIBGAV1_ENABLE_SSE4_1
namespace libgav1 {
namespace dsp {

void ConvolveInit_SSE4_1() {}

}  // namespace dsp
}  // namespace libgav1

#else   // !LIBGAV1_ENABLE_SSE4_1
namespace libgav1 {
namespace dsp {

void ConvolveInit_SSE4_1() {}

}  // namespace dsp
}  // namespace libgav1
#endif  // LIBGAV1_ENABLE_SSE4_1
