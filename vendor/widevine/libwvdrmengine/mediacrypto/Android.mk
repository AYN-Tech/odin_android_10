LOCAL_PATH := $(call my-dir)

# -----------------------------------------------------------------------------
# Builds libwvdrmcryptoplugin
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
  src/WVCryptoPlugin.cpp \

LOCAL_C_INCLUDES := \
  frameworks/av/include \
  frameworks/native/include \
  vendor/widevine/libwvdrmengine/cdm/core/include \
  vendor/widevine/libwvdrmengine/cdm/include \
  vendor/widevine/libwvdrmengine/cdm/metrics/include \
  vendor/widevine/libwvdrmengine/cdm/util/include \
  vendor/widevine/libwvdrmengine/include \
  vendor/widevine/libwvdrmengine/mediacrypto/include \
  vendor/widevine/libwvdrmengine/oemcrypto/include \

LOCAL_HEADER_LIBRARIES := \
  libutils_headers

LOCAL_STATIC_LIBRARIES := \
  libcdm_protos \
  libcrypto \

LOCAL_SHARED_LIBRARIES := \
  liblog

LOCAL_MODULE := libwvdrmcryptoplugin
LOCAL_PROPRIETARY_MODULE := true

LOCAL_MODULE_TAGS := optional

include $(BUILD_STATIC_LIBRARY)

# -----------------------------------------------------------------------------
# Builds libwvdrmcryptoplugin_hidl
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
  src_hidl/WVCryptoPlugin.cpp \

LOCAL_C_INCLUDES := \
  frameworks/av/include \
  frameworks/native/include \
  vendor/widevine/libwvdrmengine/cdm/core/include \
  vendor/widevine/libwvdrmengine/cdm/include \
  vendor/widevine/libwvdrmengine/cdm/metrics/include \
  vendor/widevine/libwvdrmengine/cdm/util/include \
  vendor/widevine/libwvdrmengine/include_hidl \
  vendor/widevine/libwvdrmengine/include \
  vendor/widevine/libwvdrmengine/mediacrypto/include_hidl \
  vendor/widevine/libwvdrmengine/mediacrypto/include \
  vendor/widevine/libwvdrmengine/oemcrypto/include \

LOCAL_HEADER_LIBRARIES := \
  libutils_headers

LOCAL_STATIC_LIBRARIES := \
  libcdm_protos \
  libcrypto \

LOCAL_SHARED_LIBRARIES := \
  android.hardware.drm@1.0 \
  android.hardware.drm@1.1 \
  android.hardware.drm@1.2 \
  android.hidl.memory@1.0 \
  libhidlmemory \
  liblog

LOCAL_MODULE := libwvdrmcryptoplugin_hidl
LOCAL_PROPRIETARY_MODULE := true

LOCAL_MODULE_TAGS := optional

include $(BUILD_STATIC_LIBRARY)
