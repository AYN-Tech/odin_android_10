LOCAL_PATH := $(call my-dir)

#----------------------------------------------------------------------
# Host compiler configs
#----------------------------------------------------------------------
SOURCE_ROOT := $(shell pwd)
TARGET_HOST_COMPILER_PREFIX_OVERRIDE := prebuilts/gcc/linux-x86/host/x86_64-linux-glibc2.17-4.8/bin/x86_64-linux-
TARGET_HOST_CC_OVERRIDE := $(TARGET_HOST_COMPILER_PREFIX_OVERRIDE)gcc
TARGET_HOST_CXX_OVERRIDE := $(TARGET_HOST_COMPILER_PREFIX_OVERRIDE)g++
TARGET_HOST_AR_OVERRIDE := $(TARGET_HOST_COMPILER_PREFIX_OVERRIDE)ar
TARGET_HOST_LD_OVERRIDE := $(TARGET_HOST_COMPILER_PREFIX_OVERRIDE)ld

#----------------------------------------------------------------------
# Compile (L)ittle (K)ernel bootloader and the nandwrite utility
#----------------------------------------------------------------------
ifneq ($(strip $(TARGET_NO_BOOTLOADER)),true)

# Compile
include bootable/bootloader/edk2/AndroidBoot.mk

$(INSTALLED_BOOTLOADER_MODULE): $(TARGET_EMMC_BOOTLOADER) | $(ACP)
	$(transform-prebuilt-to-target)
$(BUILT_TARGET_FILES_PACKAGE): $(INSTALLED_BOOTLOADER_MODULE)

droidcore: $(INSTALLED_BOOTLOADER_MODULE)
endif

#----------------------------------------------------------------------
# Compile Linux Kernel
#----------------------------------------------------------------------
ifneq ($(wildcard device/qcom/kernelscripts/kernel_definitions.mk),)
include device/qcom/kernelscripts/kernel_definitions.mk
else
DTC := $(HOST_OUT_EXECUTABLES)/dtc$(HOST_EXECUTABLE_SUFFIX)
UFDT_APPLY_OVERLAY := $(HOST_OUT_EXECUTABLES)/ufdt_apply_overlay$(HOST_EXECUTABLE_SUFFIX)

TARGET_KERNEL_MAKE_ENV := DTC_EXT=$(SOURCE_ROOT)/$(DTC)
TARGET_KERNEL_MAKE_ENV += DTC_OVERLAY_TEST_EXT=$(SOURCE_ROOT)/$(UFDT_APPLY_OVERLAY)
TARGET_KERNEL_MAKE_ENV += CONFIG_BUILD_ARM64_DT_OVERLAY=y
TARGET_KERNEL_MAKE_ENV += HOSTCC=$(SOURCE_ROOT)/$(TARGET_HOST_CC_OVERRIDE)
TARGET_KERNEL_MAKE_ENV += HOSTAR=$(SOURCE_ROOT)/$(TARGET_HOST_AR_OVERRIDE)
TARGET_KERNEL_MAKE_ENV += HOSTLD=$(SOURCE_ROOT)/$(TARGET_HOST_LD_OVERRIDE)
ifeq ($(TARGET_USES_NEW_ION), false)
TARGET_KERNEL_MAKE_ENV += HOSTCFLAGS="-I/usr/include -I/usr/include/x86_64-linux-gnu -L/usr/lib -L/usr/lib/x86_64-linux-gnu"
else
TARGET_KERNEL_MAKE_ENV += HOSTCFLAGS="-I$(SOURCE_ROOT)/kernel/msm-$(TARGET_KERNEL_VERSION)/include/uapi -I/usr/include -I/usr/include/x86_64-linux-gnu -L/usr/lib -L/usr/lib/x86_64-linux-gnu"
endif
TARGET_KERNEL_MAKE_ENV += HOSTLDFLAGS="-L/usr/lib -L/usr/lib/x86_64-linux-gnu"

KERNEL_LLVM_BIN := $(lastword $(sort $(wildcard $(SOURCE_ROOT)/$(LLVM_PREBUILTS_BASE)/$(BUILD_OS)-x86/clang-4*)))/bin/clang

$(warning Kernel source tree path is: $(TARGET_KERNEL_SOURCE))
$(warning Kernel version  is: $(TARGET_KERNEL_VERSION))
$(warning Kernel version  is: $(KERNEL_DEFCONFIG))
include $(TARGET_KERNEL_SOURCE)/AndroidKernel.mk
$(TARGET_PREBUILT_KERNEL): $(DTC) $(UFDT_APPLY_OVERLAY)

$(INSTALLED_KERNEL_TARGET): $(TARGET_PREBUILT_KERNEL) | $(ACP)
	$(transform-prebuilt-to-target)
endif

#----------------------------------------------------------------------
# Copy additional target-specific files
#----------------------------------------------------------------------
include $(CLEAR_VARS)
LOCAL_MODULE       := vold.fstab
LOCAL_MODULE_TAGS  := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES    := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)


include $(CLEAR_VARS)
LOCAL_MODULE       := gpio-keys.kl
LOCAL_MODULE_TAGS  := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES    := $(LOCAL_MODULE)
LOCAL_MODULE_PATH  := $(TARGET_OUT_KEYLAYOUT)
include $(BUILD_PREBUILT)

# QSSI merge config files
ifeq ($(ENABLE_AB), true)
    # Handle Case for QSSI-Dynamic Partition
    ifeq ($(BOARD_DYNAMIC_PARTITION_ENABLE), true)
        $(call dist-for-goals,droidcore,$(LOCAL_PATH)/ota_merge_configs/dynamic_partition/ab/merge_config_system_misc_info_keys)
        $(call dist-for-goals,droidcore,$(LOCAL_PATH)/ota_merge_configs/dynamic_partition/ab/merge_config_other_item_list)
        $(call dist-for-goals,droidcore,$(LOCAL_PATH)/ota_merge_configs/dynamic_partition/ab/merge_config_system_item_list)
    else
        $(call dist-for-goals,droidcore,$(LOCAL_PATH)/ota_merge_configs/without_dynamic_partition/ab/merge_config_system_misc_info_keys)
        $(call dist-for-goals,droidcore,$(LOCAL_PATH)/ota_merge_configs/without_dynamic_partition/ab/merge_config_other_item_list)
        $(call dist-for-goals,droidcore,$(LOCAL_PATH)/ota_merge_configs/without_dynamic_partition/ab/merge_config_system_item_list)
    endif
else
    # Handle Case for QSSI-Dynamic Partition
    ifeq ($(BOARD_DYNAMIC_PARTITION_ENABLE), true)
        $(call dist-for-goals,droidcore,$(LOCAL_PATH)/ota_merge_configs/dynamic_partition/non_ab/merge_config_system_item_list)
        $(call dist-for-goals,droidcore,$(LOCAL_PATH)/ota_merge_configs/dynamic_partition/non_ab/merge_config_other_item_list)
        $(call dist-for-goals,droidcore,$(LOCAL_PATH)/ota_merge_configs/dynamic_partition/non_ab/merge_config_system_misc_info_keys)
    else
        $(call dist-for-goals,droidcore,$(LOCAL_PATH)/ota_merge_configs/without_dynamic_partition/non_ab/merge_config_system_item_list)
        $(call dist-for-goals,droidcore,$(LOCAL_PATH)/ota_merge_configs/without_dynamic_partition/non_ab/merge_config_other_item_list)
        $(call dist-for-goals,droidcore,$(LOCAL_PATH)/ota_merge_configs/without_dynamic_partition/non_ab/merge_config_system_misc_info_keys)
    endif
endif

#----------------------------------------------------------------------
# extra images
#----------------------------------------------------------------------
ifeq ($(TARGET_PRODUCT),qssi)
include device/qcom/common/generate_extra_images.mk
endif

#create firmware directory for qssi
$(shell  mkdir -p $(TARGET_OUT_VENDOR)/firmware)

# override default make with prebuilt make path (if any)
ifneq (, $(wildcard $(shell pwd)/prebuilts/build-tools/linux-x86/bin/make))
   MAKE := $(shell pwd)/prebuilts/build-tools/linux-x86/bin/$(MAKE)
endif
