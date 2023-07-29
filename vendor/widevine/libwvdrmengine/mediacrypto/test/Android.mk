LOCAL_PATH := $(call my-dir)

# -----------------------------------------------------------------------------
# Builds libwvdrmmediacrypto_test
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
  legacy_src/WVCryptoPlugin_test.cpp \

LOCAL_C_INCLUDES := \
  frameworks/av/include \
  frameworks/native/include \
  vendor/widevine/libwvdrmengine/cdm/core/include \
  vendor/widevine/libwvdrmengine/cdm/include \
  vendor/widevine/libwvdrmengine/cdm/metrics/include \
  vendor/widevine/libwvdrmengine/cdm/util/include \
  vendor/widevine/libwvdrmengine/mediacrypto/include \
  vendor/widevine/libwvdrmengine/oemcrypto/include \

LOCAL_STATIC_LIBRARIES := \
  libcdm \
  libcdm_protos \
  libcdm_utils \
  libcrypto \
  libjsmn \
  libgmock \
  libgmock_main \
  libgtest \
  libwvlevel3 \
  libwvdrmcryptoplugin \

LOCAL_SHARED_LIBRARIES := \
  libbase \
  libcutils \
  libdl \
  liblog \
  libprotobuf-cpp-lite \
  libstagefright_foundation \
  libutils \

LOCAL_C_INCLUDES += \
  external/protobuf/src \

LOCAL_MODULE := libwvdrmmediacrypto_test

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
# Builds libwvdrmmediacrypto_hidl_test
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
  WVCryptoPlugin_test.cpp \

LOCAL_C_INCLUDES := \
  frameworks/av/include \
  frameworks/native/include \
  vendor/widevine/libwvdrmengine/cdm/core/include \
  vendor/widevine/libwvdrmengine/cdm/include \
  vendor/widevine/libwvdrmengine/cdm/metrics/include \
  vendor/widevine/libwvdrmengine/cdm/util/include \
  vendor/widevine/libwvdrmengine/include_hidl \
  vendor/widevine/libwvdrmengine/mediacrypto/include_hidl \
  vendor/widevine/libwvdrmengine/mediacrypto/include \
  vendor/widevine/libwvdrmengine/oemcrypto/include \

LOCAL_STATIC_LIBRARIES := \
  libcdm \
  libcdm_protos \
  libcdm_utils \
  libcrypto \
  libjsmn \
  libgmock \
  libgmock_main \
  libgtest \
  libwvlevel3 \
  libwvdrmcryptoplugin_hidl \

LOCAL_SHARED_LIBRARIES := \
  android.hardware.drm@1.0 \
  android.hardware.drm@1.1 \
  android.hardware.drm@1.2 \
  android.hidl.memory@1.0 \
  libbase \
  libbinder \
  libcutils \
  libdl \
  libhidlbase \
  libhidlmemory \
  libhidltransport \
  liblog \
  libprotobuf-cpp-lite \
  libutils \

LOCAL_C_INCLUDES += \
  external/protobuf/src \

LOCAL_MODULE := libwvdrmmediacrypto_hidl_test

LOCAL_MODULE_TAGS := tests

LOCAL_MODULE_OWNER := widevine
LOCAL_PROPRIETARY_MODULE := true

# When built, explicitly put it in the DATA/nativetest directory.
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/nativetest

ifneq ($(TARGET_ENABLE_MEDIADRM_64), true)
LOCAL_MODULE_TARGET_ARCH := arm x86 mips
endif

include $(BUILD_EXECUTABLE)
