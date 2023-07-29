LOCAL_PATH:= $(call my-dir)

ifeq ($(filter mips mips64, $(TARGET_ARCH)),)
# Tests need to be compatible with devices that do not support gnu hash-style
LOCAL_LDFLAGS+=-Wl,--hash-style=both
endif

LOCAL_SRC_FILES:= \
  oec_device_features.cpp \
  oec_session_util.cpp \
  oemcrypto_session_tests_helper.cpp \
  oemcrypto_test.cpp \
  oemcrypto_test_android.cpp \
  oemcrypto_test_main.cpp \
  wvcrc.cpp \

LOCAL_C_INCLUDES += \
  $(LOCAL_PATH)/../include \
  $(LOCAL_PATH)/../ref/src \
  vendor/widevine/libwvdrmengine/cdm/core/include \
  vendor/widevine/libwvdrmengine/cdm/util/include \

LOCAL_STATIC_LIBRARIES := \
  libcdm \
  libcdm_utils \
  libcrypto_static \
  libgtest \
  libgtest_main \
  libwvlevel3 \
  libcdm_utils \

LOCAL_SHARED_LIBRARIES := \
  libbase \
  libdl \
  liblog \
  libmedia_omx \
  libstagefright_foundation \
  libutils \
  libz \

