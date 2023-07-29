# -----------------------------------------------------------------------------
# Common rules for android.hardware.drm@1.2-service.widevine and
# android.hardware.drm@1.1-service-lazy.widevine
#

LOCAL_C_INCLUDES := \
  vendor/widevine/libwvdrmengine/include_hidl \
  vendor/widevine/libwvdrmengine/mediadrm/include \
  vendor/widevine/libwvdrmengine/oemcrypto/include \

LOCAL_SHARED_LIBRARIES := \
  android.hardware.drm@1.0 \
  android.hardware.drm@1.1 \
  android.hardware.drm@1.2 \
  libbase \
  libhidlbase \
  libhidltransport \
  libhwbinder \
  liblog \
  libutils \
  libwvhidl \
  libbinder \

LOCAL_HEADER_LIBRARIES := \
  libstagefright_foundation_headers

LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR)/bin/hw
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := widevine

ifneq ($(TARGET_ENABLE_MEDIADRM_64), true)
LOCAL_MODULE_TARGET_ARCH := arm x86 mips
endif
