# ----------------------------------------------------------------
# Builds libcdm.a
#
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CFLAGS := -DDYNAMIC_ADAPTER

LOCAL_C_INCLUDES := \
    vendor/widevine/libwvdrmengine/cdm/core/include \
    vendor/widevine/libwvdrmengine/cdm/metrics/include \
    vendor/widevine/libwvdrmengine/cdm/util/include \
    vendor/widevine/libwvdrmengine/cdm/include \
    vendor/widevine/libwvdrmengine/oemcrypto/include \

LOCAL_C_INCLUDES += \
    external/jsmn \
    external/protobuf/src \

LOCAL_HEADER_LIBRARIES := \
    libutils_headers

LOCAL_STATIC_LIBRARIES := libcdm_protos libcrypto

SRC_DIR := src
CORE_SRC_DIR := core/src
METRICS_SRC_DIR := metrics/src

LOCAL_SRC_FILES := \
    $(CORE_SRC_DIR)/buffer_reader.cpp \
    $(CORE_SRC_DIR)/cdm_engine.cpp \
    $(CORE_SRC_DIR)/cdm_engine_factory.cpp \
    $(CORE_SRC_DIR)/cdm_session.cpp \
    $(CORE_SRC_DIR)/cdm_session_map.cpp \
    $(CORE_SRC_DIR)/certificate_provisioning.cpp \
    $(CORE_SRC_DIR)/client_identification.cpp \
    $(CORE_SRC_DIR)/content_key_session.cpp \
    $(CORE_SRC_DIR)/crypto_session.cpp \
    $(CORE_SRC_DIR)/device_files.cpp \
    $(CORE_SRC_DIR)/entitlement_key_session.cpp \
    $(CORE_SRC_DIR)/initialization_data.cpp \
    $(CORE_SRC_DIR)/license.cpp \
    $(CORE_SRC_DIR)/license_key_status.cpp \
    $(CORE_SRC_DIR)/oemcrypto_adapter_dynamic.cpp \
    $(CORE_SRC_DIR)/policy_engine.cpp \
    $(CORE_SRC_DIR)/privacy_crypto_boringssl.cpp \
    $(CORE_SRC_DIR)/service_certificate.cpp \
    $(CORE_SRC_DIR)/usage_table_header.cpp \
    $(SRC_DIR)/wv_content_decryption_module.cpp \
    $(METRICS_SRC_DIR)/attribute_handler.cpp \
    $(METRICS_SRC_DIR)/counter_metric.cpp \
    $(METRICS_SRC_DIR)/distribution.cpp \
    $(METRICS_SRC_DIR)/event_metric.cpp \
    $(METRICS_SRC_DIR)/metrics_collections.cpp \
    $(METRICS_SRC_DIR)/timer_metric.cpp \
    $(METRICS_SRC_DIR)/value_metric.cpp


LOCAL_MODULE := libcdm
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_STATIC_LIBRARY)
