ifeq ($(TARGET_FWK_SUPPORTS_FULL_VALUEADDS),true)
ifneq ($(TARGET_HAS_LOW_RAM),true)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional
LOCAL_SDK_VERSION := system_current
LOCAL_USE_AAPT2 := true

LOCAL_SRC_FILES := $(call all-java-files-under, src) \
    src/com/android/music/IMediaPlaybackService.aidl

LOCAL_STATIC_ANDROID_LIBRARIES := \
        $(ANDROID_SUPPORT_DESIGN_TARGETS) \
        android-support-compat \
        android-support-v4 \
        android-support-media-compat \
        android-support-v7-appcompat

LOCAL_RESOURCE_DIR = \
        $(LOCAL_PATH)/res \
        frameworks/support/v7/appcompat/res \
        frameworks/support/coordinatorlayout/src/main/res

LOCAL_PACKAGE_NAME := SnapdragonMusic
LOCAL_OVERRIDES_PACKAGES := Music
LOCAL_CERTIFICATE := platform

LOCAL_PROGUARD_FLAG_FILES := proguard.flags

LOCAL_AAPT_FLAGS := \
        --auto-add-overlay \
        --extra-packages android.support.v7.appcompat \
        --extra-packages android.support.design

include $(BUILD_PACKAGE)

# Use the folloing include to make our test apk.
#include $(call all-makefiles-under,$(LOCAL_PATH))

endif
endif