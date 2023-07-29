# Build rules for tests to be included in the google_tests.zip package
# are discovered by having an Android.mk file located in vendor/<project>/tests/.

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

# Targets to be included in google_tests.zip
# Referenced by vendor/google/build/tasks/google_tests.mk
WIDEVINE_TEST_MAKE_TARGETS := \
    base64_test \
    buffer_reader_test \
    cdm_engine_test \
    cdm_extended_duration_test \
    cdm_feature_test \
    cdm_session_unittest \
    counter_metric_unittest \
    crypto_session_unittest \
    device_files_unittest \
    distribution_unittest \
    event_metric_unittest \
    file_store_unittest \
    file_utils_unittest \
    http_socket_test \
    initialization_data_unittest \
    libwvdrmdrmplugin_hidl_test \
    libwvdrmdrmplugin_test \
    libwvdrmengine_hidl_test \
    libwvdrmengine_test \
    libwvdrmmediacrypto_hidl_test \
    libwvdrmmediacrypto_test \
    license_keys_unittest \
    license_unittest \
    oemcrypto_test \
    policy_engine_constraints_unittest \
    policy_engine_unittest \
    request_license_test \
    service_certificate_unittest \
    timer_unittest \
    usage_table_header_unittest \
    value_metric_unittest \
    wv_cdm_metrics_test \
    CastSignAPITest \
    MediaDrmAPITest \

# Call the makefiles for all our tests used in TradeFederation
MODULAR_DRM_PATH := vendor/widevine/libwvdrmengine
include $(MODULAR_DRM_PATH)/cdm/test/Android.mk
include $(MODULAR_DRM_PATH)/mediacrypto/test/Android.mk
include $(MODULAR_DRM_PATH)/mediadrm/test/Android.mk
include $(MODULAR_DRM_PATH)/oemcrypto/test/Android.mk
include $(MODULAR_DRM_PATH)/test/unit/Android.mk
include $(MODULAR_DRM_PATH)/test/java/MediaDrmApiTest/Android.mk
include $(MODULAR_DRM_PATH)/test/castv2/Android.mk
