LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := XtsOEMCryptoTestCases
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)

LOCAL_XTS_TEST_PACKAGE := google.oemcrypto

include $(LOCAL_PATH)/common.mk

include $(BUILD_XTS_EXECUTABLE)
