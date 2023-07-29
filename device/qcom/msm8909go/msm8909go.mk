ALLOW_MISSING_DEPENDENCIES := true
#Go variant flag
TARGET_HAS_LOW_RAM := true
WLAN_TARGET_HAS_LOW_RAM := true

TARGET_USES_AOSP_FOR_AUDIO := false
TARGET_USES_AOSP := true
TARGET_USES_QCOM_BSP := false
TARGET_USES_HWC2 := true
TARGET_USES_GRALLOC1 := true
ifeq ($(TARGET_USES_QCOM_BSP), true)
# Add QC Video Enhancements flag
endif #TARGET_USES_QCOM_BSP

#For targets which donot support vulkan
TARGET_NOT_SUPPORT_VULKAN :=true

ifeq ($(TARGET_USES_AOSP),true)
TARGET_USES_QTIC := false
else
TARGET_USES_NQ_NFC := true
TARGET_USES_QTIC := true
-include $(QCPATH)/common/config/qtic-config.mk
endif

DEVICE_PACKAGE_OVERLAYS := device/qcom/msm8909go/overlay-go

# Default vendor configuration.
ifeq ($(ENABLE_VENDOR_IMAGE),)
ENABLE_VENDOR_IMAGE := true
endif

# Disable QTIC until it's brought up in split system/vendor
# configuration to avoid compilation breakage.
ifeq ($(ENABLE_VENDOR_IMAGE), true)
TARGET_USES_QTIC := false
endif

TARGET_USES_QTIC_EXTENSION := true
#for android_filesystem_config.h
PRODUCT_PACKAGES += \
    fs_config_files

# Default A/B configuration.
ENABLE_AB ?= false

# Enable features in video HAL that can compile only on this platform
TARGET_USES_MEDIA_EXTENSIONS := true

TARGET_ENABLE_QC_AV_ENHANCEMENTS := true

# Camera configuration file. Shared by passthrough/binderized camera HAL
PRODUCT_PACKAGES += camera.device@1.0-impl
PRODUCT_PACKAGES += android.hardware.camera.provider@2.4-impl
# Enable binderized camera HAL
PRODUCT_PACKAGES += android.hardware.camera.provider@2.4-service

# media_profiles and media_codecs xmls for msm8909go
ifeq ($(TARGET_ENABLE_QC_AV_ENHANCEMENTS), true)
PRODUCT_COPY_FILES += device/qcom/msm8909go/media/media_profiles_8909.xml:system/etc/media_profiles.xml \
                      device/qcom/msm8909go/media/media_profiles_8909.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_profiles_vendor.xml \
                      device/qcom/msm8909go/media/media_codecs_8909.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_codecs.xml \
                      device/qcom/msm8909go/media/media_codecs_vendor.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_codecs_vendor.xml \
                      device/qcom/msm8909go/media/media_codecs_performance_8909.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_codecs_performance.xml \
                      device/qcom/msm8909go/media/media_codecs_vendor_audio.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_codecs_vendor_audio.xml
endif


# video seccomp policy files
# copy to system/vendor as well (since some devices may symlink to system/vendor and not create an actual partition for vendor)
PRODUCT_COPY_FILES += \
    device/qcom/msm8909go/seccomp/mediacodec-seccomp.policy:$(TARGET_COPY_OUT_VENDOR)/etc/seccomp_policy/mediacodec.policy \
    device/qcom/msm8909go/seccomp/mediaextractor-seccomp.policy:$(TARGET_COPY_OUT_VENDOR)/etc/seccomp_policy/mediaextractor.policy

PRODUCT_PROPERTY_OVERRIDES += \
    vendor.vidc.disable.split.mode=1 \
    vendor.mediacodec.binder.size=1

PRODUCT_PROPERTY_OVERRIDES += \
       persist.radio.multisim.config=ssss \
       persist.vendor.cne.override.memlimit=1

PRODUCT_PROPERTY_OVERRIDES += \
       dalvik.vm.heapminfree=6m \
       dalvik.vm.heapstartsize=8m \
       dalvik.vm.heaptargetutilization=0.75 \
       dalvik.vm.heapmaxfree=8m \

PRODUCT_PROPERTY_OVERRIDES += \
    debug.sdm.support_writeback=0

$(call inherit-product, device/qcom/common/common.mk)

PRODUCT_NAME := msm8909go
PRODUCT_DEVICE := msm8909go

# When can normal compile this module, need module owner enable below commands
# font rendering engine feature switch
-include $(QCPATH)/common/config/rendering-engine.mk
ifneq (,$(strip $(wildcard $(PRODUCT_RENDERING_ENGINE_REVLIB))))
    #MULTI_LANG_ENGINE := REVERIE
#   MULTI_LANG_ZAWGYI := REVERIE
endif

DEVICE_MANIFEST_FILE := device/qcom/msm8909go/manifest.xml
DEVICE_MATRIX_FILE   := device/qcom/common/compatibility_matrix.xml

PRODUCT_PACKAGES += android.hardware.media.omx@1.0-impl

#Android EGL implementation
PRODUCT_PACKAGES += libGLES_android

# Audio configuration file
-include $(TOPDIR)hardware/qcom/audio/configs/msm8909/msm8909.mk


PRODUCT_BOOT_JARS += tcmiface
#PRODUCT_BOOT_JARS += qcmediaplayer

# QTI extended functionality of android telephony.
# Required for MSIM manual provisioning and other related features.
PRODUCT_PACKAGES += telephony-ext
PRODUCT_BOOT_JARS += telephony-ext

ifneq ($(strip $(QCPATH)),)
PRODUCT_BOOT_JARS += oem-services
#PRODUCT_BOOT_JARS += com.qti.dpmframework
#PRODUCT_BOOT_JARS += dpmapi
#PRODUCT_BOOT_JARS += com.qti.location.sdk
endif

# Listen configuration file
PRODUCT_COPY_FILES += \
    device/qcom/msm8909go/listen_platform_info.xml:system/etc/listen_platform_info.xml

#VB xml
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.software.verified_boot.xml:system/etc/permissions/android.software.verified_boot.xml

# Feature definition files for msm8909go
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.sensor.accelerometer.xml:system/etc/permissions/android.hardware.sensor.accelerometer.xml \
    frameworks/native/data/etc/android.hardware.sensor.compass.xml:system/etc/permissions/android.hardware.sensor.compass.xml \
    frameworks/native/data/etc/android.hardware.sensor.gyroscope.xml:system/etc/permissions/android.hardware.sensor.gyroscope.xml \
    frameworks/native/data/etc/android.hardware.sensor.light.xml:system/etc/permissions/android.hardware.sensor.light.xml \
    frameworks/native/data/etc/android.hardware.sensor.proximity.xml:system/etc/permissions/android.hardware.sensor.proximity.xml

#fstab.qcom
#PRODUCT_PACKAGES += fstab.qcom

PRODUCT_PACKAGES += \
    libqcomvisualizer \
    libqcompostprocbundle \
    libqcomvoiceprocessing

#OEM Services library
PRODUCT_PACKAGES += oem-services
PRODUCT_PACKAGES += libsubsystem_control
PRODUCT_PACKAGES += libSubSystemShutdown

PRODUCT_PACKAGES += wcnss_service

#wlan driver
PRODUCT_COPY_FILES += \
    device/qcom/msm8909go/WCNSS_qcom_cfg.ini:vendor/etc/wifi/WCNSS_qcom_cfg.ini \
    device/qcom/msm8909go/WCNSS_wlan_dictionary.dat:persist/WCNSS_wlan_dictionary.dat \
    device/qcom/msm8909go/WCNSS_qcom_wlan_nv.bin:persist/WCNSS_qcom_wlan_nv.bin

PRODUCT_PACKAGES += \
    wpa_supplicant_overlay.conf \
    p2p_supplicant_overlay.conf
#for wlan
   PRODUCT_PACKAGES += \
       wificond \
       wifilogd

# MIDI feature
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.software.midi.xml:system/etc/permissions/android.software.midi.xml

#ANT+ stack
PRODUCT_PACKAGES += \
AntHalService \
libantradio \
antradio_app

# Display/Gralloc
PRODUCT_PACKAGES += \
    android.hardware.graphics.allocator@2.0-impl \
    android.hardware.graphics.allocator@2.0-service \
    android.hardware.graphics.mapper@2.0-impl \
    android.hardware.graphics.composer@2.1-impl \
    android.hardware.graphics.composer@2.1-service \
    android.hardware.memtrack@1.0-impl \
    android.hardware.memtrack@1.0-service \
    android.hardware.light@2.0-impl \
    android.hardware.light@2.0-service \
    android.hardware.configstore@1.0-service \
    android.hardware.broadcastradio@1.0-impl

ifeq ($(TARGET_HAS_LOW_RAM),true)
TARGET_EXCLUDES_DISPLAY_PP := true
endif #TARGET_HAS_LOW_RAM

PRODUCT_PACKAGES += \
    libandroid_net \
    libandroid_net_32

PRODUCT_FULL_TREBLE_OVERRIDE := true

PRODUCT_VENDOR_MOVE_ENABLED := true

# Vibrator
PRODUCT_PACKAGES += \
    android.hardware.vibrator@1.0-impl \
    android.hardware.vibrator@1.0-service \

# Defined the locales
PRODUCT_LOCALES := en_US

# When can normal compile this module,  need module owner enable below commands
# Add the overlay path
#PRODUCT_PACKAGE_OVERLAYS := $(QCPATH)/qrdplus/Extension/res \
       $(QCPATH)/qrdplus/globalization/multi-language/res-overlay \
       $(PRODUCT_PACKAGE_OVERLAYS)

# Sensor HAL conf file
PRODUCT_COPY_FILES += \
    device/qcom/msm8909go/sensors/hals.conf:vendor/etc/sensors/hals.conf


#Boot control HAL test app
PRODUCT_PACKAGES_DEBUG += bootctl

#Healthd packages
PRODUCT_PACKAGES += android.hardware.health@2.0-impl \
                    android.hardware.health@2.0-service \
                    libhealthd.msm
#Supports verity
PRODUCT_SUPPORTS_VERITY := true

# Power
PRODUCT_PACKAGES += \
    android.hardware.power@1.0-service \
    android.hardware.power@1.0-impl

#Enable QTI KEYMASTER and GATEKEEPER HIDLs
ifeq ($(ENABLE_VENDOR_IMAGE), true)
KMGK_USE_QTI_SERVICE := true
endif

#Enable AOSP KEYMASTER and GATEKEEPER HIDLs
ifneq ($(KMGK_USE_QTI_SERVICE), true)
PRODUCT_PACKAGES += android.hardware.gatekeeper@1.0-impl \
                    android.hardware.gatekeeper@1.0-service \
                    android.hardware.keymaster@3.0-impl \
                    android.hardware.keymaster@3.0-service
endif

$(call inherit-product-if-exists, frameworks/base/data/sounds/AudioPackageGo.mk)

# Inherit Go default properties, sets is-low-ram-device flag etc.
$(call inherit-product, build/target/product/go_defaults.mk)

PRODUCT_PROPERTY_OVERRIDES += dalvik.vm.foreground-heap-growth-multiplier=2.0
PRODUCT_MINIMIZE_JAVA_DEBUG_INFO := true

ifeq ($(ENABLE_AB),true)
#A/B related packages
PRODUCT_PACKAGES += update_engine \
                    update_engine_client \
                    update_verifier \
                    bootctrl.msm8909 \
                    brillo_update_payload \
                    android.hardware.boot@1.0-impl \
                    android.hardware.boot@1.0-service
#Boot control HAL test app
PRODUCT_PACKAGES_DEBUG += bootctl

PRODUCT_STATIC_BOOT_CONTROL_HAL := \
  bootctrl.msm8909 \
  librecovery_updater_msm \
  libz \
  libcutils

PRODUCT_PACKAGES += \
  update_engine_sideload
endif

# Add soft home, back and multitask keys
PRODUCT_PROPERTY_OVERRIDES += qemu.hw.mainkeys=1
PRODUCT_PROPERTY_OVERRIDES += rild.libpath=/vendor/lib/libril-qc-qmi-1.so

SDM660_DISABLE_MODULE := true

# Enable extra vendor libs
ENABLE_EXTRA_VENDOR_LIBS := true
PRODUCT_PACKAGES += vendor-extra-libs


###################################################################################
# This is the End of target.mk file.
# Now, Pickup other split product.mk files:
###################################################################################
$(call inherit-product-if-exists, vendor/qcom/defs/product-defs/legacy/*.mk)
###################################################################################
