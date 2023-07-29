#ifndef LIBGAV1_SRC_DSP_CDEF_H_
#define LIBGAV1_SRC_DSP_CDEF_H_

namespace libgav1 {
namespace dsp {

// Initializes Dsp::cdef_direction and cdef::filter. This function is not
// thread-safe.
void CdefInit_C();

}  // namespace dsp
}  // namespace libgav1

#endif  // LIBGAV1_SRC_DSP_CDEF_H_
