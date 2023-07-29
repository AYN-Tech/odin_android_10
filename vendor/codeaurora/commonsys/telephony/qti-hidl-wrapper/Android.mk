LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := qti-telephony-hidl-wrapper
LOCAL_PRODUCT_MODULE := true

LOCAL_STATIC_JAVA_LIBRARIES := android.hidl.base-V1.0-java android.hidl.manager-V1.0-java

include $(BUILD_JAVA_LIBRARY)

# ==========================================================================

include $(CLEAR_VARS)

LOCAL_MODULE := qti_telephony_hidl_wrapper.xml

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE_CLASS := ETC

# This will install the file in /product/etc/permissions
#
LOCAL_MODULE_PATH := $(TARGET_OUT_PRODUCT_ETC)/permissions

LOCAL_SRC_FILES := $(LOCAL_MODULE)

include $(BUILD_PREBUILT)
