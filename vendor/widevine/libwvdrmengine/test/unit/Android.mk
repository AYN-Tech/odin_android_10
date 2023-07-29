LOCAL_PATH:= $(call my-dir)

# -----------------------------------------------------------------------------
# Builds libwvdrmengine_test
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
  legacy_src/WVCreatePluginFactories_test.cpp \
  legacy_src/WVCryptoFactory_test.cpp \
  legacy_src/WVDrmFactory_test.cpp \

LOCAL_C_INCLUDES := \
  frameworks/av/include \
  frameworks/native/include \
  vendor/widevine/libwvdrmengine/include \
  vendor/widevine/libwvdrmengine/mediadrm/include \
  vendor/widevine/libwvdrmengine/oemcrypto/include \

LOCAL_STATIC_LIBRARIES := \
  libcrypto \
  libgtest \
  libgtest_main \

LOCAL_SHARED_LIBRARIES := \
  libdl \
  liblog \
  libutils \
  libwvdrmengine \

LOCAL_MODULE := libwvdrmengine_test

LOCAL_MODULE_TAGS := tests

LOCAL_MODULE_OWNER := widevine
LOCAL_PROPRIETARY_MODULE := true

# When built, explicitly put it in the DATA/nativetest directory.
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/nativetest

ifneq ($(TARGET_ENABLE_MEDIADRM_64), true)
LOCAL_MODULE_TARGET_ARCH := arm x86 mips
endif

include $(BUILD_EXECUTABLE)

# -----------------------------------------------------------------------------
# Builds libwvdrmengine_hidl_test
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
  WVCreatePluginFactories_test.cpp \
  WVCryptoFactory_test.cpp \
  WVDrmFactory_test.cpp \

LOCAL_C_INCLUDES := \
  frameworks/av/include \
  frameworks/native/include \
  vendor/widevine/libwvdrmengine/include_hidl \
  vendor/widevine/libwvdrmengine/include \
  vendor/widevine/libwvdrmengine/mediadrm/include_hidl \
  vendor/widevine/libwvdrmengine/mediadrm/include \
  vendor/widevine/libwvdrmengine/oemcrypto/include \

LOCAL_STATIC_LIBRARIES := \
  libcrypto \
  libgtest \
  libgtest_main \

LOCAL_SHARED_LIBRARIES := \
  android.hardware.drm@1.0 \
  android.hardware.drm@1.1 \
  android.hardware.drm@1.2 \
  libbase \
  libdl \
  libhidlbase \
  libhidlmemory \
  liblog \
  libutils \
  libwvhidl \

LOCAL_MODULE := libwvdrmengine_hidl_test

LOCAL_MODULE_TAGS := tests

LOCAL_MODULE_OWNER := widevine
LOCAL_PROPRIETARY_MODULE := true

# When built, explicitly put it in the DATA/nativetest directory.
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/nativetest

ifneq ($(TARGET_ENABLE_MEDIADRM_64), true)
LOCAL_MODULE_TARGET_ARCH := arm x86 mips
endif

include $(BUILD_EXECUTABLE)
