#/bin/bash

# Helper script to use atest to run the MediaDrm CTS and GTS tests

# This script:
#  1. Unlocks a device that has no pin set
#  2. Asserts that it has internet connectivity
#  3. Clears logcat
#  4. Uses atest to run the MediaDrm CTS and GTS tests
#  5. Saves output of atest and logcat to log files in the $OUT dir

if [[ -z "$ANDROID_SERIAL" ]]; then
  echo '$ANDROID_SERIAL not set. Set ANDROID_SERIAL to the serial of the' \
    'device you want to test.'
  exit 1
fi

if [[ -z "$OUT" ]]; then
  echo '$OUT not set. You must source build/envsetup.sh and run lunch first.'
  exit 1
fi

adb root

if [[ "$?" -ne 0 ]]; then
  exit 1
fi

# Make screen never sleep while charging
adb shell settings put global stay_on_while_plugged_in 7

# Unlock a device that does not have a pin or password set (e.g. swipe up to unlock):
# press the wakeup key, then if the device is still locked, press the menu key
adb shell input keyevent 224
sleep 1
if [[ $(adb shell dumpsys nfc | grep '^mScreenState=ON_LOCKED$') ]] ; then
  adb shell input keyevent 82
fi

if [[ ! $(adb shell ping -c 1 google.com) ]]; then
  echo "No wifi. Exiting"
  exit 1
fi

adb shell mkdir -p /sdcard/test/images/

TIMESTAMP="$(date +"%b-%d-%H%M")"
STDOUT_FILE="$OUT/atest.mediadrm.$ANDROID_SERIAL.$TIMESTAMP.stdout.log"
STDERR_FILE="$OUT/atest.mediadrm.$ANDROID_SERIAL.$TIMESTAMP.stderr.log "
LOGCAT_FILE="$OUT/atest.mediadrm.$ANDROID_SERIAL.$TIMESTAMP.logcat.log"

adb logcat -c

atest -v -s $ANDROID_SERIAL \
  GtsMediaTestCases:com.google.android.media.gts.DecoderMetricsTests \
  GtsMediaTestCases:com.google.android.media.gts.DrmSessionManagerTest \
  GtsMediaTestCases:com.google.android.media.gts.MediaCodecCencTest \
  GtsMediaTestCases:com.google.android.media.gts.MediaCodecStressTest \
  GtsMediaTestCases:com.google.android.media.gts.MediaCodecTest \
  GtsMediaTestCases:com.google.android.media.gts.MediaDrmTest \
  GtsMediaTestCases:com.google.android.media.gts.MediaPlayerTest \
  GtsMediaTestCases:com.google.android.media.gts.Vp8CodecTest \
  GtsMediaTestCases:com.google.android.media.gts.WidevineCodecStressTests \
  GtsMediaTestCases:com.google.android.media.gts.WidevineDashPolicyTests \
  GtsMediaTestCases:com.google.android.media.gts.WidevineFailureTests \
  GtsMediaTestCases:com.google.android.media.gts.WidevineGenericOpsTests \
  GtsMediaTestCases:com.google.android.media.gts.WidevineH264PlaybackTests \
  GtsMediaTestCases:com.google.android.media.gts.WidevineHEVCPlaybackTests \
  GtsMediaTestCases:com.google.android.media.gts.WidevineHLSPlaybackTests \
  GtsMediaTestCases:com.google.android.media.gts.WidevineIdentifierTests \
  GtsMediaTestCases:com.google.android.media.gts.WidevineVP9WebMPlaybackTests \
  GtsMediaTestCases:com.google.android.media.gts.WidevineYouTubePerformanceTests \
  GtsExoPlayerTestCases:com.google.android.exoplayer.gts.DashTest \
  CtsMediaTestCases:android.media.cts.MediaDrmClearkeyTest \
  CtsMediaTestCases:android.media.cts.MediaCodecTest \
  CtsMediaTestCases:android.media.cts.MediaDrmMetricsTest \
  CtsMediaTestCases:android.media.cts.MediaPlayerDrmTest \
  CtsMediaTestCases:android.media.cts.MediaPlayer2DrmTest \
  CtsMediaTestCases:android.media.cts.NativeMediaDrmClearkeyTest \
  > >(tee -a $STDOUT_FILE) \
  2> >(tee -a $STDERR_FILE >&2)

adb logcat -d > $LOGCAT_FILE

cat <<EOF
Saved logs to:
  $STDOUT_FILE
  $STDERR_FILE
  $LOGCAT_FILE
EOF
