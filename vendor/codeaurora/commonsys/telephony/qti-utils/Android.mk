LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(call all-java-files-under, src/org)
LOCAL_PRODUCT_MODULE := true
LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := qti-telephony-utils

include $(BUILD_JAVA_LIBRARY)

# ==========================================================================

include $(CLEAR_VARS)

LOCAL_MODULE := qti_telephony_utils.xml

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE_CLASS := ETC

# This will install the file in /product/etc/permissions
#
LOCAL_MODULE_PATH := $(TARGET_OUT_PRODUCT_ETC)/permissions

LOCAL_SRC_FILES := $(LOCAL_MODULE)

include $(BUILD_PREBUILT)

# Include subdirectory makefiles
# ============================================================
include $(call all-makefiles-under,$(LOCAL_PATH))
