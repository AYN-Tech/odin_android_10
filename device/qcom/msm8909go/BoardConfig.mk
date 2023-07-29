# Copyright (C) 2011 The Android Open-Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# config.mk
#
# Product-specific compile-time definitions.
#

ifeq ($(TARGET_ARCH),)
TARGET_ARCH := arm
endif

TARGET_BOARD_SUFFIX := go

TARGET_COMPILE_WITH_MSM_KERNEL := true
TARGET_KERNEL_APPEND_DTB := true
TARGET_KERNEL_CROSS_COMPILE_PREFIX := $(PWD)/prebuilts/gcc/linux-x86/arm/arm-eabi-4.8/bin/arm-eabi-

#USE_CLANG_PLATFORM_BUILD := true

BOARD_USES_GENERIC_AUDIO := true
USE_CAMERA_STUB := true

TARGET_DISABLE_DASH := true
-include $(QCPATH)/common/msm8909go/BoardConfigVendor.mk
TARGET_COMPILE_WITH_MSM_KERNEL := true

# set the cryptfs_hw directory path
TARGET_CRYPTFS_HW_PATH := device/qcom/common/cryptfs_hw

TARGET_USES_QCOM_DISPLAY_BSP := true

#TODO: Fix-me: Setting TARGET_HAVE_HDMI_OUT to false
# to get rid of compilation error.
TARGET_HAVE_HDMI_OUT := false
TARGET_USES_OVERLAY := true
#TARGET_USES_PCI_RCS := true
NUM_FRAMEBUFFER_SURFACE_BUFFERS := 3
TARGET_NO_BOOTLOADER := false
TARGET_NO_KERNEL := false
TARGET_NO_RADIOIMAGE := true
TARGET_NO_RPC := true
GET_FRAMEBUFFER_FORMAT_FROM_HWC := false

BOOTLOADER_GCC_VERSION := arm-eabi-4.8
BOOTLOADER_PLATFORM := msm8909# use msm8909 LK configuration

#TARGET_GLOBAL_CFLAGS += -mfpu=neon -mfloat-abi=softfp
#TARGET_GLOBAL_CPPFLAGS += -mfpu=neon -mfloat-abi=softfp
TARGET_CPU_ABI  := armeabi-v7a
TARGET_CPU_ABI2 := armeabi
TARGET_CPU_VARIANT := cortex-a7
TARGET_ARCH_VARIANT := armv7-a-neon
TARGET_CPU_SMP := true
ARCH_ARM_HAVE_TLS_REGISTER := true

TARGET_HARDWARE_3D := false
TARGET_BOARD_PLATFORM := msm8909
TARGET_BOOTLOADER_BOARD_NAME := msm8909

BOARD_SECCOMP_POLICY := device/qcom/$(TARGET_BOARD_PLATFORM)/seccomp

BOARD_KERNEL_BASE        := 0x80000000
BOARD_KERNEL_PAGESIZE    := 2048
BOARD_KERNEL_TAGS_OFFSET := 0x01E00000
BOARD_RAMDISK_OFFSET     := 0x02000000

TARGET_USES_UNCOMPRESSED_KERNEL := false

# Support to build images for 2K NAND page
BOARD_KERNEL_2KPAGESIZE := 2048
BOARD_KERNEL_2KSPARESIZE := 64

# Shader cache config options
# Maximum size of the  GLES Shaders that can be cached for reuse.
# Increase the size if shaders of size greater than 12KB are used.
MAX_EGL_CACHE_KEY_SIZE := 12*1024

# Maximum GLES shader cache size for each app to store the compiled shader
# binaries. Decrease the size if RAM or Flash Storage size is a limitation
# of the device.
MAX_EGL_CACHE_SIZE := 2048*1024

# Use signed boot and recovery image
#TARGET_BOOTIMG_SIGNED := true

TARGET_USERIMAGES_USE_EXT4 := true
TARGET_USERIMAGES_USE_F2FS := true
BOARD_USERDATAIMAGE_FILE_SYSTEM_TYPE := f2fs
BOARD_PERSISTIMAGE_FILE_SYSTEM_TYPE := ext4

BOARD_KERNEL_CMDLINE := console=ttyHSL0,115200,n8 androidboot.console=ttyHSL0 androidboot.hardware=qcom msm_rtb.filter=0x237 ehci-hcd.park=3 androidboot.bootdevice=7824900.sdhci lpm_levels.sleep_disabled=1 androidboot.memcg=true earlyprintk

BOARD_EGL_CFG := device/qcom/msm8909go/egl.cfg

ifeq ($(ENABLE_AB),true)
#A/B related defines
AB_OTA_UPDATER := true
# Full A/B partiton update set
# AB_OTA_PARTITIONS := xbl rpm tz hyp pmic modem abl boot keymaster cmnlib cmnlib64 system bluetooth

# Subset A/B partitions for Android-only image update
AB_OTA_PARTITIONS ?= boot system
BOARD_BUILD_SYSTEM_ROOT_IMAGE := true
TARGET_NO_RECOVERY := true
BOARD_USES_RECOVERY_AS_BOOT := true
else
BOARD_RECOVERYIMAGE_PARTITION_SIZE := 0x02000000
BOARD_CACHEIMAGE_PARTITION_SIZE := 115343360
BOARD_CACHEIMAGE_FILE_SYSTEM_TYPE := ext4
TARGET_RECOVERY_UPDATER_LIBS += librecovery_updater_msm
# Enable System As Root even for non-A/B from P onwards
BOARD_BUILD_SYSTEM_ROOT_IMAGE := true
endif

ifeq ($(ENABLE_AB), true)
  ifeq ($(ENABLE_VENDOR_IMAGE), true)
    TARGET_RECOVERY_FSTAB := device/qcom/msm8909go/recovery_AB_split_variant.fstab
  else
    TARGET_RECOVERY_FSTAB := device/qcom/msm8909go/recovery_AB_non-split_variant.fstab
  endif
else
  ifeq ($(ENABLE_VENDOR_IMAGE),true)
    TARGET_RECOVERY_FSTAB := device/qcom/msm8909go/recovery_non-AB_split_variant.fstab
  else
    TARGET_RECOVERY_FSTAB := device/qcom/msm8909go/recovery_non-AB_non-split_variant.fstab
  endif
endif

BOARD_BOOTIMAGE_PARTITION_SIZE := 0x02000000
BOARD_SYSTEMIMAGE_PARTITION_SIZE := 943718400
BOARD_USERDATAIMAGE_PARTITION_SIZE := 5905580032
BOARD_PERSISTIMAGE_PARTITION_SIZE := 33554432
BOARD_OEMIMAGE_PARTITION_SIZE := 268435456
BOARD_FLASH_BLOCK_SIZE := 131072 # (BOARD_KERNEL_PAGESIZE * 64)

ifeq ($(ENABLE_VENDOR_IMAGE), true)
BOARD_VENDORIMAGE_PARTITION_SIZE := 262144000
BOARD_VENDORIMAGE_FILE_SYSTEM_TYPE := ext4
TARGET_COPY_OUT_VENDOR := vendor
BOARD_PROPERTY_OVERRIDES_SPLIT_ENABLED := true
endif


# Add NON-HLOS files for ota upgrade
ADD_RADIO_FILES ?= true

# Added to indicate that protobuf-c is supported in this build
PROTOBUF_SUPPORTED := false

TARGET_USES_ION := true
TARGET_USES_NEW_ION_API :=true

TARGET_INIT_VENDOR_LIB := libinit_msm
TARGET_PLATFORM_DEVICE_BASE := /devices/soc.0/

#Add support for firmare upgrade on msm8909go
HAVE_SYNAPTICS_I2C_RMI4_FW_UPGRADE := true

#Enable peripheral manager
TARGET_PER_MGR_ENABLED := true

#Use dlmalloc instead of jemalloc for mallocs
#MALLOC_IMPL := dlmalloc
MALLOC_SVELTE := true

ifeq ($(TARGET_USES_AOSP), true)
TARGET_HW_DISK_ENCRYPTION := false
else
# SDClang configuration
SDCLANG := true
#Enable HW based full disk encryption
TARGET_HW_DISK_ENCRYPTION := true
endif

#TARGET_VB_NOT_ENABLED := true

# Enable sensor multi HAL
USE_SENSOR_MULTI_HAL := true

#Disable SSC Feature
TARGET_USES_SSC := false

FEATURE_QCRIL_UIM_SAP_SERVER_MODE := true

# Control flag between KM versions
TARGET_HW_KEYMASTER_V03 := false

BOARD_HAL_STATIC_LIBRARIES := libhealthd.msm

ifneq ($(TARGET_BUILD_VARIANT),user)
  # Retain classes.dex in APK's for non-user builds
  DEX_PREOPT_DEFAULT := nostripping
endif

ifeq ($(HOST_OS),linux)
    ifeq ($(WITH_DEXPREOPT_BOOT_IMG_AND_SYSTEM_SERVER_ONLY),)
        WITH_DEXPREOPT_BOOT_IMG_AND_SYSTEM_SERVER_ONLY := true
    endif
endif


#################################################################################
# This is the End of BoardConfig.mk file.
# Now, Pickup other split Board.mk files:
#################################################################################
-include vendor/qcom/defs/board-defs/legacy/*.mk
#################################################################################
