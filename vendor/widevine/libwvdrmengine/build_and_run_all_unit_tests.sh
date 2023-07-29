#!/bin/sh

set -e

if [ -z "$ANDROID_BUILD_TOP" ]; then
  echo "Android build environment not set"
  exit -1
fi

# Read arguments in case the user wants to do a multicore build or
# copy files to a specific android device by providing a serial number
NUM_CORES=1
SERIAL_NUM=""
while getopts "j:s:" opt; do
  case $opt in
    j)
      NUM_CORES=$OPTARG
      ;;
    s)
      SERIAL_NUM="-s $OPTARG"
      ;;
  esac
done

# Define the relevant aliases
. $ANDROID_BUILD_TOP/build/envsetup.sh

# Build all the targets
# This list is slightly longer than the one in run_all_unit_tests.sh because
# it does not run very long tests or tests needing special setup.
WV_TEST_TARGETS="base64_test \
     buffer_reader_test \
     cdm_engine_test \
     cdm_engine_metrics_decorator_unittest \
     cdm_feature_test \
     cdm_extended_duration_test \
     cdm_session_unittest \
     counter_metric_unittest \
     crypto_session_unittest \
     device_files_unittest \
     distribution_unittest \
     event_metric_unittest \
     file_store_unittest \
     file_utils_unittest \
     generic_crypto_unittest \
     hidl_metrics_adapter_unittest \
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
     rw_lock_test \
     service_certificate_unittest \
     timer_unittest \
     usage_table_header_unittest \
     value_metric_unittest \
     wv_cdm_metrics_test"

cd $ANDROID_BUILD_TOP
pwd
m -j $NUM_CORES $WV_TEST_TARGETS


# Detect the device and check if Verity is going to stop the script from working
echo "waiting for device"
ADB_OUTPUT=`adb $SERIAL_NUM root && echo ". " && adb $SERIAL_NUM wait-for-device remount`
echo $ADB_OUTPUT
if echo $ADB_OUTPUT | grep -qi "verity"; then
  echo
  echo "ERROR: This device has Verity enabled. build_and_run_all_unit_tests.sh "
  echo "does not work if Verity is enabled. Please disable Verity with"
  echo "\"adb $SERIAL_NUM disable-verity\" and try again."
  exit -1
fi

# Push the files to the device

# Given a local path to a file, this will try to push it to /data/nativetest.
# If that fails, an error message will be printed.
try_adb_push() {
  # android-tests.zip requires /data/nativetest, we should use the same
  if [ -f $OUT/data/nativetest/$1 ]; then
    test_file=$OUT/data/nativetest/$1
  else
    echo "I cannot find $1"
    echo "I think it should be in $OUT/data/nativetest"
    exit 1
  fi
  adb $SERIAL_NUM shell mkdir -p /data/nativetest
  adb $SERIAL_NUM push $test_file /data/nativetest/$1
}

# Push the tests to the device
for f in $WV_TEST_TARGETS; do 
  try_adb_push $f
done

# Run the tests using run_all_unit_tests.sh
cd $ANDROID_BUILD_TOP/vendor/widevine/libwvdrmengine
./run_all_unit_tests.sh $SERIAL_NUM
