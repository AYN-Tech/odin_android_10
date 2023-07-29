LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_CFLAGS := \
    -DDYNAMIC_ADAPTER \
    -Wno-unused \
    -Wno-unused-parameter
LOCAL_C_INCLUDES := \
    system/core/base/include \
    system/core/include \
    vendor/widevine/libwvdrmengine/cdm/core/include \
    vendor/widevine/libwvdrmengine/cdm/util/include \
    vendor/widevine/libwvdrmengine/level3/include \
    vendor/widevine/libwvdrmengine/oemcrypto/include
LOCAL_MODULE := libwvlevel3
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_SRC_FILES := libl3oemcrypto.cpp \
    ../src/generate_entropy_android.cpp \
    ../src/get_unique_id_android.cpp \
    ../src/level3_file_system_android.cpp \
    ../src/level3_file_system_android_factory.cpp
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := widevine
LOCAL_MODULE_TARGET_ARCH := mips
include $(BUILD_STATIC_LIBRARY)
