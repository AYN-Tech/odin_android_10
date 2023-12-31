LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := aacEncode.cpp
LOCAL_SRC_FILES += aacDecode.cpp

LOCAL_C_INCLUDES := $(LOCAL_PATH)
LOCAL_C_INCLUDES += $(TOP)/external/aac/libAACenc/include
LOCAL_C_INCLUDES += $(TOP)/external/aac/libAACdec/include
LOCAL_C_INCLUDES += $(TOP)/external/aac/libSYS/include

LOCAL_SHARED_LIBRARIES := liblog
LOCAL_STATIC_LIBRARIES := libFraunhoferAAC

LOCAL_32_BIT_ONLY := true
LOCAL_MODULE := libwfdaac
LOCAL_CFLAGS += -O0

LOCAL_SANITIZE := integer_overflow
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := aacEncode.cpp
LOCAL_SRC_FILES += aacDecode.cpp

LOCAL_C_INCLUDES := $(LOCAL_PATH)
LOCAL_C_INCLUDES += $(TOP)/external/aac/libAACenc/include
LOCAL_C_INCLUDES += $(TOP)/external/aac/libAACdec/include
LOCAL_C_INCLUDES += $(TOP)/external/aac/libSYS/include

LOCAL_HEADER_LIBRARIES += libutils_headers

LOCAL_SHARED_LIBRARIES := liblog
LOCAL_STATIC_LIBRARIES := libFraunhoferAAC

LOCAL_VENDOR_MODULE := true

LOCAL_32_BIT_ONLY := true
LOCAL_MODULE := libwfdaac_vendor
LOCAL_CFLAGS += -O0

LOCAL_SANITIZE := integer_overflow
include $(BUILD_SHARED_LIBRARY)
