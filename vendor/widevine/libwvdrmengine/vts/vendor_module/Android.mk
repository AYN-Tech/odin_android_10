# ----------------------------------------------------------------
# Builds libvtswidevine.so
#
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES := \
    hardware/interfaces/drm/1.0/vts/functional \
    vendor/widevine/libwvdrmengine/cdm/include \
    vendor/widevine/libwvdrmengine/cdm/core/include \
    vendor/widevine/libwvdrmengine/cdm/core/test \
    vendor/widevine/libwvdrmengine/cdm/util/include \
    system/libhidl/base/include \
    system/core/base/include \
    system/libvintf/include \

LOCAL_SRC_FILES := \
    factory.cpp \
    vts_module.cpp \
    ../../cdm/core/test/url_request.cpp \
    ../../cdm/core/test/license_request.cpp \
    ../../cdm/core/test/http_socket.cpp \

LOCAL_STATIC_LIBRARIES := \
    libgtest \
    libcdm \
    libcdm_utils \
    libcrypto

LOCAL_SHARED_LIBRARIES := \
    libbase \
    liblog \
    libssl \
    libutils \

LOCAL_MODULE := libvtswidevine
LOCAL_MODULE_RELATIVE_PATH := drm-vts-test-libs
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true


include $(BUILD_SHARED_LIBRARY)
