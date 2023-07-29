# -----------------------------------------------------------------------------
# CDM top level makefile
#
LOCAL_PATH := $(call my-dir)

# -----------------------------------------------------------------------------
# Copies move script to /system/bin.
#
# The move script is only needed for existing devices that
# are running Widevine DRM and are upgrading to Pi or later
# Android releases. New devices release with Pi and future
# Android releases do not need to run this script.
#
# To run this script, vendor must add the dependency to the
# corresponding device.mk file and build from ANDROID_ROOT.
#
# For example:
#   PRODUCT_PACKAGES += \
#       android.hardware.drm@1.0-impl \
#       android.hardware.drm@1.0-service \
#       android.hardware.drm@1.0-service.widevine \
#       move_widevine_data.sh
#
# In addition, vendor needs to update device SELinux policy.
#
# The mv command preserves SELinux labels(i.e. media_data_file).
# We need to run restorecon to put the correct context after the move.
# However, restorecon is not implemented for /vendor/bin, so we put
# the script in /system/bin.
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES := move_widevine_data.sh
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE := $(LOCAL_SRC_FILES)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := widevine


include $(BUILD_PREBUILT)

# -----------------------------------------------------------------------------
# Builds android.hardware.drm@1.2-service.widevine
#
include $(CLEAR_VARS)

include $(LOCAL_PATH)/common_widevine_service.mk
LOCAL_SRC_FILES := src_hidl/service.cpp
LOCAL_MODULE := android.hardware.drm@1.2-service.widevine
LOCAL_INIT_RC := src_hidl/android.hardware.drm@1.2-service.widevine.rc

include $(BUILD_EXECUTABLE)

# -----------------------------------------------------------------------------
# Builds android.hardware.drm@1.2-service-lazy.widevine
#
include $(CLEAR_VARS)

include $(LOCAL_PATH)/common_widevine_service.mk
LOCAL_SRC_FILES := src_hidl/serviceLazy.cpp
LOCAL_MODULE := android.hardware.drm@1.2-service-lazy.widevine
LOCAL_OVERRIDES_MODULES := android.hardware.drm@1.2-service.widevine
LOCAL_INIT_RC := src_hidl/android.hardware.drm@1.2-service-lazy.widevine.rc

include $(BUILD_EXECUTABLE)

# -----------------------------------------------------------------------------
# Builds libcdm_utils.a
#
include $(CLEAR_VARS)

LOCAL_MODULE := libcdm_utils
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_PROPRIETARY_MODULE := true

LOCAL_STATIC_LIBRARIES := libcrypto

LOCAL_C_INCLUDES := \
    vendor/widevine/libwvdrmengine/cdm/core/include \
    vendor/widevine/libwvdrmengine/cdm/include \
    vendor/widevine/libwvdrmengine/cdm/util/include \
    vendor/widevine/libwvdrmengine/oemcrypto/include \

LOCAL_HEADER_LIBRARIES := \
    libbase_headers \
    libutils_headers \

LOCAL_SHARED_LIBRARIES := \
    liblog \

LOCAL_CFLAGS := -DCORE_UTIL_IMPLEMENTATION

SRC_DIR := cdm/src
UTIL_SRC_DIR := cdm/util/src
CORE_SRC_DIR := cdm/core/src
LOCAL_SRC_FILES := \
    $(CORE_SRC_DIR)/properties.cpp \
    $(UTIL_SRC_DIR)/platform.cpp \
    $(UTIL_SRC_DIR)/rw_lock.cpp \
    $(UTIL_SRC_DIR)/string_conversions.cpp \
    $(UTIL_SRC_DIR)/clock.cpp \
    $(UTIL_SRC_DIR)/file_store.cpp \
    $(UTIL_SRC_DIR)/file_utils.cpp \
    $(UTIL_SRC_DIR)/log.cpp \
    $(SRC_DIR)/properties_android.cpp \
    $(SRC_DIR)/timer.cpp \

include $(BUILD_STATIC_LIBRARY)

# -----------------------------------------------------------------------------
# Builds libcdm_protos.a
# Generates *.a, *.pb.h and *.pb.cc for *.proto files.
#
include $(CLEAR_VARS)

LOCAL_MODULE := libcdm_protos
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_PROPRIETARY_MODULE := true

CORE_PROTO_SRC_FILES := $(call all-proto-files-under, cdm/core/src)
METRICS_PROTO_SRC_FILES := $(call all-proto-files-under, cdm/metrics/src)
LOCAL_SRC_FILES := $(CORE_PROTO_SRC_FILES) $(METRICS_PROTO_SRC_FILES)

generated_sources_dir := $(call local-generated-sources-dir)

# $(generated_sources_dir)/proto/$(LOCAL_PATH)/cdm/core/src is used
# to locate *.pb.h by cdm source
# $(generated_sources_dir)/proto is used to locate *.pb.h included
# by *.pb.cc
# The module that depends on this library will have LOCAL_C_INCLUDES prepended
# with this path.
LOCAL_EXPORT_C_INCLUDE_DIRS := \
    $(generated_sources_dir)/proto \
    $(generated_sources_dir)/proto/$(LOCAL_PATH)/cdm/core/src \
    $(generated_sources_dir)/proto/$(LOCAL_PATH)/cdm/metrics/src

include $(BUILD_STATIC_LIBRARY)

# -----------------------------------------------------------------------------
# Builds libwvdrmengine.so
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
  src/WVCDMSingleton.cpp \
  src/WVCreatePluginFactories.cpp \
  src/WVCryptoFactory.cpp \
  src/WVDrmFactory.cpp \
  src/WVUUID.cpp \

LOCAL_C_INCLUDES := \
  frameworks/av/include \
  frameworks/native/include \
  vendor/widevine/libwvdrmengine/cdm/core/include \
  vendor/widevine/libwvdrmengine/cdm/metrics/include \
  vendor/widevine/libwvdrmengine/cdm/util/include \
  vendor/widevine/libwvdrmengine/cdm/include \
  vendor/widevine/libwvdrmengine/include \
  vendor/widevine/libwvdrmengine/mediacrypto/include \
  vendor/widevine/libwvdrmengine/mediadrm/include \
  vendor/widevine/libwvdrmengine/oemcrypto/include \

LOCAL_STATIC_LIBRARIES := \
  libcdm \
  libcdm_protos \
  libcdm_utils \
  libcrypto \
  libjsmn \
  libwvdrmcryptoplugin \
  libwvdrmdrmplugin \
  libwvlevel3 \

LOCAL_SHARED_LIBRARIES := \
  libbase \
  libdl \
  liblog \
  libprotobuf-cpp-lite \
  libstagefright_foundation \
  libutils \

LOCAL_HEADER_LIBRARIES := \
  libutils_headers \
  libstagefright_headers

LOCAL_MODULE := libwvdrmengine

LOCAL_MODULE_RELATIVE_PATH := mediadrm

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE_OWNER := widevine

LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)

# -----------------------------------------------------------------------------
# Builds libwvhidl.so
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
  src/WVCDMSingleton.cpp \
  src/WVUUID.cpp \
  src_hidl/WVCreatePluginFactories.cpp \
  src_hidl/WVCryptoFactory.cpp \
  src_hidl/WVDrmFactory.cpp \

LOCAL_C_INCLUDES := \
  frameworks/av/include \
  frameworks/native/include \
  vendor/widevine/libwvdrmengine/cdm/core/include \
  vendor/widevine/libwvdrmengine/cdm/metrics/include \
  vendor/widevine/libwvdrmengine/cdm/util/include \
  vendor/widevine/libwvdrmengine/cdm/include \
  vendor/widevine/libwvdrmengine/include_hidl \
  vendor/widevine/libwvdrmengine/include \
  vendor/widevine/libwvdrmengine/mediacrypto/include_hidl \
  vendor/widevine/libwvdrmengine/mediacrypto/include \
  vendor/widevine/libwvdrmengine/mediadrm/include_hidl \
  vendor/widevine/libwvdrmengine/mediadrm/include \
  vendor/widevine/libwvdrmengine/oemcrypto/include \

LOCAL_STATIC_LIBRARIES := \
  libcdm \
  libcdm_protos \
  libcdm_utils \
  libcrypto \
  libjsmn \
  libwvdrmcryptoplugin_hidl \
  libwvdrmdrmplugin_hidl \
  libwvlevel3 \

LOCAL_SHARED_LIBRARIES := \
  android.hardware.drm@1.0 \
  android.hardware.drm@1.1 \
  android.hardware.drm@1.2 \
  android.hidl.memory@1.0 \
  libbase \
  libdl \
  libhidlbase \
  libhidlmemory \
  libhidltransport \
  libhwbinder \
  liblog \
  libprotobuf-cpp-lite \
  libutils \

LOCAL_MODULE := libwvhidl

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE_OWNER := widevine

LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)

include vendor/widevine/libwvdrmengine/cdm/Android.mk
include vendor/widevine/libwvdrmengine/level3/Android.mk
include vendor/widevine/libwvdrmengine/mediacrypto/Android.mk
include vendor/widevine/libwvdrmengine/mediadrm/Android.mk
include vendor/widevine/libwvdrmengine/vts/vendor_module/Android.mk
