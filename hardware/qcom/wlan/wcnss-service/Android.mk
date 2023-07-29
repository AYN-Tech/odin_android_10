ifneq (,$(filter arm aarch64 arm64, $(TARGET_ARCH)))
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
ifeq ($(PRODUCT_VENDOR_MOVE_ENABLED),true)
LOCAL_VENDOR_MODULE := true
endif
LOCAL_MODULE := wcnss_service
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/common/inc/
LOCAL_SRC_FILES := wcnss_service.c
ifeq ($(strip $(TARGET_USES_QCOM_WCNSS_QMI)),true)
LOCAL_CFLAGS += -DWCNSS_QMI
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/qmi-framework/inc
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/qmi/services
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/qmi/platform
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/qmi/inc
LOCAL_SRC_FILES += wcnss_qmi_client.c
endif #TARGET_USES_QCOM_WCNSS_QMI
LOCAL_SHARED_LIBRARIES := libc libcutils libutils liblog
ifeq ($(strip $(TARGET_USES_QCOM_WCNSS_QMI)),true)
LOCAL_SHARED_LIBRARIES += libqmiservices libqmi_cci
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/libmdmdetect/inc
LOCAL_SHARED_LIBRARIES += libmdmdetect
LOCAL_HEADER_LIBRARIES += libril-qc-qmi-services-headers
endif #TARGET_USES_QCOM_WCNSS_QMI
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS += -Wall -Werror
include $(BUILD_EXECUTABLE)
endif
