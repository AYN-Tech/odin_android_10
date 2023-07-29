LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    src/keys.cpp \
    src/oemcrypto_auth_ref.cpp \
    src/oemcrypto_engine_device_properties.cpp \
    src/oemcrypto_engine_ref.cpp \
    src/oemcrypto_key_ref.cpp \
    src/oemcrypto_keybox_ref.cpp \
    src/oemcrypto_keybox_testkey.cpp \
    src/oemcrypto_ref.cpp \
    src/oemcrypto_nonce_table.cpp \
    src/oemcrypto_old_usage_table_ref.cpp \
    src/oemcrypto_rsa_key_shared.cpp \
    src/oemcrypto_session.cpp \
    src/oemcrypto_session_key_table.cpp \
    src/oemcrypto_usage_table_ref.cpp \
    src/wvcrc.cpp \

LOCAL_MODULE_TAGS := tests

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/../include \
    $(LOCAL_PATH)/src \
    vendor/widevine/libwvdrmengine/cdm/util/include \

LOCAL_SHARED_LIBRARIES := \
    libbase \
    libdl \
    liblog \
    libmedia \
    libstagefright_foundation \
    libutils \
    libz \

LOCAL_STATIC_LIBRARIES := \
    libcdm_utils \
    libcrypto \

# Proprietary modules are put in vendor/lib instead of /system/lib.
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE := liboemcrypto

include $(BUILD_SHARED_LIBRARY)

