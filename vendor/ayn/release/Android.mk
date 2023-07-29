LOCAL_PATH:= $(call my-dir)

$(shell mkdir -p $(PRODUCT_OUT)/vendor/etc/thermal_config)
$(shell cp -rf $(call my-dir)/thermal-engine-normal.conf $(PRODUCT_OUT)/vendor/etc/thermal_config/thermal-engine-normal.conf)
$(shell cp -rf $(call my-dir)/thermal-engine-high.conf $(PRODUCT_OUT)/vendor/etc/thermal_config/thermal-engine-high.conf)
$(shell mkdir -p $(PRODUCT_OUT)/system/bin)
$(shell mkdir -p $(PRODUCT_OUT)/system/lib64)
$(shell cp -rf $(call my-dir)/exfat/mkfs.exfat $(PRODUCT_OUT)/system/bin/mkfs.exfat)
$(shell cp -rf $(call my-dir)/exfat/mount.exfat $(PRODUCT_OUT)/system/bin/mount.exfat)
$(shell cp -rf $(call my-dir)/exfat/fsck.exfat $(PRODUCT_OUT)/system/bin/fsck.exfat)
$(shell cp -rf $(call my-dir)/exfat/libexfat.so $(PRODUCT_OUT)/system/lib64/libexfat.so)


include $(CLEAR_VARS)
LOCAL_MODULE        := GameAssistant
LOCAL_MODULE_TAGS   := optional
LOCAL_MODULE_CLASS  := APPS
LOCAL_CERTIFICATE   := platform
LOCAL_MODULE_SUFFIX := .apk
LOCAL_SRC_FILES     := GameAssistant.apk
LOCAL_MODULE_PATH   := $(TARGET_OUT_VENDOR)/app
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE        := OdinLauncher
LOCAL_MODULE_TAGS   := optional
LOCAL_MODULE_CLASS  := APPS
LOCAL_CERTIFICATE   := platform
LOCAL_MODULE_SUFFIX := .apk
LOCAL_SRC_FILES     := OdinLauncher.apk
LOCAL_MODULE_PATH   := $(TARGET_OUT_VENDOR)/app
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE        := OdinRomLauncher
LOCAL_MODULE_TAGS   := optional
LOCAL_MODULE_CLASS  := APPS
LOCAL_CERTIFICATE   := platform
LOCAL_MODULE_SUFFIX := .apk
LOCAL_SRC_FILES     := OdinRomLauncher.apk
LOCAL_MODULE_PATH   := $(TARGET_OUT_VENDOR)/app
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE        := OdinSettings
LOCAL_MODULE_TAGS   := optional
LOCAL_MODULE_CLASS  := APPS
LOCAL_CERTIFICATE   := platform
LOCAL_MODULE_SUFFIX := .apk
LOCAL_SRC_FILES     := OdinSettings.apk
LOCAL_MODULE_PATH   := $(TARGET_OUT_VENDOR)/app
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE        := TouchMapping
LOCAL_MODULE_TAGS   := optional
LOCAL_MODULE_CLASS  := APPS
LOCAL_CERTIFICATE   := platform
LOCAL_MODULE_SUFFIX := .apk
LOCAL_SRC_FILES     := TouchMapping.apk
LOCAL_MODULE_PATH   := $(TARGET_OUT_VENDOR)/app
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE        := FactoryTest
LOCAL_MODULE_TAGS   := optional
LOCAL_MODULE_CLASS  := APPS
LOCAL_CERTIFICATE   := platform
LOCAL_MODULE_SUFFIX := .apk
LOCAL_SRC_FILES     := FactoryTest.apk
LOCAL_MODULE_PATH   := $(TARGET_OUT_VENDOR)/app
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE        := OdinDumpLog
LOCAL_MODULE_TAGS   := optional
LOCAL_MODULE_CLASS  := APPS
LOCAL_CERTIFICATE   := platform
LOCAL_MODULE_SUFFIX := .apk
LOCAL_SRC_FILES     := OdinDumpLog.apk
LOCAL_MODULE_PATH   := $(TARGET_OUT)/app
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE        := GamepadTest
LOCAL_MODULE_TAGS   := optional
LOCAL_MODULE_CLASS  := APPS
LOCAL_CERTIFICATE   := PRESIGNED
LOCAL_MODULE_SUFFIX := .apk
LOCAL_SRC_FILES     := GamepadTest.apk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE        := FPS_2D
LOCAL_MODULE_TAGS   := optional
LOCAL_MODULE_CLASS  := APPS
LOCAL_CERTIFICATE   := PRESIGNED
LOCAL_MODULE_SUFFIX := .apk
LOCAL_SRC_FILES     := FPS_2D.apk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE        := Gboard
LOCAL_MODULE_TAGS   := optional
LOCAL_MODULE_CLASS  := APPS
LOCAL_CERTIFICATE   := PRESIGNED
LOCAL_MODULE_SUFFIX := .apk
LOCAL_SRC_FILES     := Gboard.apk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE        := Chrome
LOCAL_MODULE_TAGS   := optional
LOCAL_MODULE_CLASS  := APPS
LOCAL_CERTIFICATE   := PRESIGNED
LOCAL_MODULE_SUFFIX := .apk
LOCAL_SRC_FILES     := Chrome.apk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE 		:= Upgrade
LOCAL_MODULE_TAGS 	:= optional
LOCAL_MODULE_CLASS 	:= APPS
LOCAL_CERTIFICATE 	:= platform
LOCAL_MODULE_SUFFIX     := .apk
LOCAL_SRC_FILES 	:= Upgrade.apk
LOCAL_PRIVILEGED_MODULE := true
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE 		:= WallpaperPicker3
LOCAL_MODULE_TAGS 	:= optional
LOCAL_MODULE_CLASS 	:= APPS
LOCAL_CERTIFICATE 	:= platform
LOCAL_MODULE_SUFFIX     := .apk
LOCAL_SRC_FILES 	:= WallpaperPicker3.apk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE 		:= ScreenCapture
LOCAL_MODULE_TAGS 	:= optional
LOCAL_MODULE_CLASS 	:= APPS
LOCAL_CERTIFICATE 	:= platform
LOCAL_MODULE_SUFFIX     := .apk
LOCAL_SRC_FILES 	:= ScreenCapture.apk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := SetupWizard
LOCAL_MODULE_STEM := SetupWizard.apk
LOCAL_MODULE_CLASS := APPS
LOCAL_CERTIFICATE := platform
LOCAL_MODULE_PATH := $(TARGET_OUT)/priv-app
LOCAL_SRC_FILES := SetupWizard.apk
LOCAL_OVERRIDES_PACKAGES := Provision
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := Q4Ota
LOCAL_MODULE_STEM := Q4Ota.apk
LOCAL_MODULE_CLASS := APPS
LOCAL_CERTIFICATE := platform
LOCAL_MODULE_PATH := $(TARGET_OUT)/priv-app
LOCAL_SRC_FILES := Q4Ota.apk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE        := OdinMusic
LOCAL_MODULE_TAGS   := optional
LOCAL_MODULE_CLASS  := APPS
LOCAL_CERTIFICATE   := platform
LOCAL_MODULE_SUFFIX := .apk
LOCAL_SRC_FILES     := OdinMusic.apk
LOCAL_OVERRIDES_PACKAGES := SnapdragonMusic
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE        := OdinCalendar
LOCAL_MODULE_TAGS   := optional
LOCAL_MODULE_CLASS  := APPS
LOCAL_CERTIFICATE   := platform
LOCAL_MODULE_SUFFIX := .apk
LOCAL_SRC_FILES     := OdinCalendar.apk
LOCAL_OVERRIDES_PACKAGES := CalendarGooglePrebuilt
include $(BUILD_PREBUILT)
