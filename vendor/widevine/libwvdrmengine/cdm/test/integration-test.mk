# -------------------------------------------------------------------
# Makes a unit or end to end test.
# test_name must be passed in as the base filename(without the .cpp).
#
$(call assert-not-null,test_name)

include $(CLEAR_VARS)

LOCAL_MODULE := $(test_name)
LOCAL_MODULE_TAGS := tests

LOCAL_SRC_FILES := \
    $(test_main) \
    $(test_src_dir)/$(test_name).cpp \
    ../../oemcrypto/test//oec_device_features.cpp \
    ../core/test/config_test_env.cpp \
    ../core/test/http_socket.cpp \
    ../core/test/license_request.cpp \
    ../core/test/test_base.cpp \
    ../core/test/test_printers.cpp \
    ../core/test/url_request.cpp

LOCAL_C_INCLUDES := \
    vendor/widevine/libwvdrmengine/android/cdm/test \
    vendor/widevine/libwvdrmengine/cdm/core/include \
    vendor/widevine/libwvdrmengine/cdm/core/test \
    vendor/widevine/libwvdrmengine/cdm/include \
    vendor/widevine/libwvdrmengine/cdm/metrics/include \
    vendor/widevine/libwvdrmengine/cdm/util/include \
    vendor/widevine/libwvdrmengine/oemcrypto/include \
    vendor/widevine/libwvdrmengine/oemcrypto/test \

LOCAL_C_INCLUDES += external/protobuf/src

LOCAL_STATIC_LIBRARIES := \
    libcdm \
    libcdm_protos \
    libcdm_utils \
    libcrypto_static \
    libjsmn \
    libgmock \
    libgtest \
    libwvlevel3 \

LOCAL_SHARED_LIBRARIES := \
    libbase \
    libdl \
    liblog \
    libmedia_omx \
    libprotobuf-cpp-lite \
    libssl \
    libstagefright_foundation \
    libutils \

LOCAL_CFLAGS += -DUNIT_TEST

LOCAL_MODULE_OWNER := widevine
LOCAL_PROPRIETARY_MODULE := true

# When built, explicitly put it in the DATA/nativetest directory.
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/nativetest

ifneq ($(TARGET_ENABLE_MEDIADRM_64), true)
LOCAL_MODULE_TARGET_ARCH := arm x86 mips
endif

include $(BUILD_EXECUTABLE)
