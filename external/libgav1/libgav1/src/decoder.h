#ifndef LIBGAV1_SRC_DECODER_H_
#define LIBGAV1_SRC_DECODER_H_

#include <cstddef>
#include <cstdint>
#include <memory>

#include "src/decoder_buffer.h"
#include "src/decoder_settings.h"
#include "src/status_code.h"
#include "src/symbol_visibility.h"

namespace libgav1 {

// Forward declaration.
class DecoderImpl;

class LIBGAV1_PUBLIC Decoder {
 public:
  Decoder();
  ~Decoder();

  // Init must be called exactly once per instance. Subsequent calls will do
  // nothing. If |settings| is nullptr, the decoder will be initialized with
  // default settings. Returns kLibgav1StatusOk on success, an error status
  // otherwise.
  StatusCode Init(const DecoderSettings* settings);

  // Enqueues a compressed frame to be decoded. Applications can continue
  // enqueue'ing up to |GetMaxAllowedFrames()|. The decoder can be thought of as
  // a queue of size |GetMaxAllowedFrames()|. Returns kLibgav1StatusOk on
  // success and an error status otherwise. Returning an error status here isn't
  // a fatal error and the decoder can continue decoding further frames. To
  // signal EOF, call this function with |data| as nullptr and |size| as 0. That
  // will release all the frames held by the decoder.
  //
  // |user_private_data| may be used to asssociate application specific private
  // data with the compressed frame. It will be copied to the user_private_data
  // field of the DecoderBuffer returned by the corresponding |DequeueFrame()|
  // call.
  //
  // NOTE: |EnqueueFrame()| does not copy the data. Therefore, after a
  // successful |EnqueueFrame()| call, the caller must keep the |data| buffer
  // alive until the corresponding |DequeueFrame()| call returns.
  StatusCode EnqueueFrame(const uint8_t* data, size_t size,
                          int64_t user_private_data);

  // Dequeues a decompressed frame. If there are enqueued compressed frames,
  // decodes one and sets |*out_ptr| to the last displayable frame in the
  // compressed frame. If there are no displayable frames available, sets
  // |*out_ptr| to nullptr. Returns an error status if there is an error.
  StatusCode DequeueFrame(const DecoderBuffer** out_ptr);

  // Returns the maximum number of frames allowed to be enqueued at a time. The
  // decoder will reject frames beyond this count. If |settings_.frame_parallel|
  // is false, then this function will always return 1.
  int GetMaxAllowedFrames() const;

  // Returns the maximum bitdepth that is supported by this decoder.
  static int GetMaxBitdepth();

 private:
  bool initialized_ = false;
  DecoderSettings settings_;
  std::unique_ptr<DecoderImpl> impl_;
};

}  // namespace libgav1

#endif  // LIBGAV1_SRC_DECODER_H_
