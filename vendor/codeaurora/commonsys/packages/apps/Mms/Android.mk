# Copyright 2007-2008 The Android Open Source Project

ifeq ($(TARGET_FWK_SUPPORTS_FULL_VALUEADDS),true)
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := $(call all-java-files-under, src)
LOCAL_SRC_FILES += src/org/codeaurora/presenceserv/IPresenceServiceCB.aidl \
                   src/org/codeaurora/presenceserv/IPresenceService.aidl

LOCAL_PACKAGE_NAME := Mms

# Builds against the public SDK
#LOCAL_SDK_VERSION := current
LOCAL_PRIVATE_PLATFORM_APIS:=true

LOCAL_JAVA_LIBRARIES += telephony-common org.apache.http.legacy telephony-ext
LOCAL_STATIC_JAVA_LIBRARIES += android-common jsr305
LOCAL_STATIC_JAVA_LIBRARIES += libchips
LOCAL_STATIC_JAVA_LIBRARIES += libphonenumber
LOCAL_STATIC_JAVA_LIBRARIES += android-support-v4
LOCAL_STATIC_JAVA_LIBRARIES += android-support-v7-recyclerview
LOCAL_STATIC_JAVA_LIBRARIES += MmsWrapper

LOCAL_RESOURCE_DIR = \
        $(LOCAL_PATH)/res \
        frameworks/opt/chips/res

LOCAL_AAPT_FLAGS := --auto-add-overlay
LOCAL_AAPT_FLAGS += --extra-packages com.android.ex.chips

LOCAL_PROGUARD_FLAG_FILES := proguard.flags

LOCAL_PRIVILEGED_MODULE := true

LOCAL_OVERRIDES_PACKAGES := messaging

#include $(BUILD_PACKAGE)

# This finds and builds the test apk as well, so a single make does both.
include $(call all-makefiles-under,$(LOCAL_PATH))
endif#TARGET_FWK_SUPPORTS_FULL_VALUEADDS
