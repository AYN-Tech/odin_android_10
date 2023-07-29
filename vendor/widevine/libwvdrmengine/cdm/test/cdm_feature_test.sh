#!/bin/bash
#
# This test runs the cdm_feature_test
# Precondition:
# * cdm_feature_test has been pushed to the device and exits at
#   /data/bin/cdm_feature_test
# * Follow instructions to load modifiable OEMCrypto

KEYBOX="keybox"
OEMCERT="oemcert"
SRM_UPDATE_SUPPORTED="srm_update_supported"
SRM_UPDATE_NOT_SUPPORTED="srm_update_not_supported"
SRM_INITIAL_VERSION_CURRENT="srm_initial_version_current"
SRM_INITIAL_VERSION_OUTDATED="srm_initial_version_outdated"
SRM_INITIAL_VERSION_NOT_SUPPORTED="srm_initial_version_not_supported"
SRM_REVOKED="srm_attached_device_revoked"
SRM_NOT_REVOKED="srm_attached_device_not_revoked"
OPTIONS_TXT_FILENAME="options.txt"
HOST_OPTIONS_PATH="/tmp/$OPTIONS_TXT_FILENAME"

# write_options_txt_file
# ["keybox"|"oemcert"|"srm_update_supported"|"srm_update_not_supported"|
#  "srm_initial_version_current"|"srm_initial_version_outdated"|
#  "srm_initial_version_not_supported"|"srm_attached_device_revoked"
#  "srm_attached_device_not_revoked"]
write_options_txt_file()
{
  YES="yes"
  NO="no"

  # default values
  use_keybox=$YES
  srm_initial_version_current=$NO
  srm_initial_version_supported=$YES
  srm_updatable=$NO
  srm_revoked=$NO

  for var in "$@"
  do
    case $var in
      $KEYBOX ) use_keybox=$YES ;;
      $OEMCERT ) use_keybox=$NO ;;
      $SRM_UPDATE_SUPPORTED ) srm_updatable=$YES ;;
      $SRM_UPDATE_NOT_SUPPORTED ) srm_updatable=$NO ;;
      $SRM_INITIAL_VERSION_CURRENT ) srm_initial_version_current=$YES ;;
      $SRM_INITIAL_VERSION_OUTDATED ) srm_initial_version_current=$NO ;;
      $SRM_INITIAL_VERSION_NOT_SUPPORTED ) srm_initial_version_supported=$NO ;;
      $SRM_REVOKED) srm_revoked=$YES ;;
      $SRM_NOT_REVOKED) srm_revoked=$NO ;;
    esac
  done

  echo "log_level 3" > $HOST_OPTIONS_PATH
  echo "security_level 1" >> $HOST_OPTIONS_PATH
  echo "current_hdcp 255" >> $HOST_OPTIONS_PATH
  echo "max_hdcp 255" >> $HOST_OPTIONS_PATH

  if [ "$srm_updatable" == "$YES" ]; then
    echo "srm_update_supported 1" >> $HOST_OPTIONS_PATH
  else
    echo "srm_update_supported 0" >> $HOST_OPTIONS_PATH
  fi

  echo "srm_load_fail 0" >> $HOST_OPTIONS_PATH

  if [ "$srm_initial_version_supported" == "$YES" ]; then
    if [ "$srm_initial_version_current" == "$YES" ]; then
      echo "srm_initial_version 3" >> $HOST_OPTIONS_PATH
    else
      echo "srm_initial_version 1" >> $HOST_OPTIONS_PATH
    fi
  else
    echo "srm_initial_version 0" >> $HOST_OPTIONS_PATH
  fi

  echo "srm_load_version -1" >> $HOST_OPTIONS_PATH

  if [ "$srm_revoked" == "$YES" ]; then
    echo "srm_blacklisted_device_attached 1" >> $HOST_OPTIONS_PATH
    echo "srm_attached_device_id 20362845044" >> $HOST_OPTIONS_PATH
    # echo "srm_attached_device_id 1079148782731" >> $HOST_OPTIONS_PATH
  else
    echo "srm_blacklisted_device_attached 0" >> $HOST_OPTIONS_PATH
    echo "srm_attached_device_id 12345678" >> $HOST_OPTIONS_PATH
  fi

  echo "security_patch_level 1" >> $HOST_OPTIONS_PATH
  echo "max_buffer_size 0" >> $HOST_OPTIONS_PATH

  if [ "$use_keybox" == "$NO" ]; then
    echo "use_keybox 0" >> $HOST_OPTIONS_PATH
  else
    echo "use_keybox 1" >> $HOST_OPTIONS_PATH
  fi

  echo "use_fallback 1" >> $HOST_OPTIONS_PATH
  echo "" >> $HOST_OPTIONS_PATH
  echo "kLoggingTraceOEMCryptoCalls 1" >> $HOST_OPTIONS_PATH
  echo "kLoggingDumpContentKeys 1" >> $HOST_OPTIONS_PATH
  echo "kLoggingDumpKeyControlBlocks 1" >> $HOST_OPTIONS_PATH
  echo "kLoggingDumpDerivedKeys 0" >> $HOST_OPTIONS_PATH
  echo "kLoggingTraceNonce 0" >> $HOST_OPTIONS_PATH
  echo "kLoggingTraceDecryption 0" >> $HOST_OPTIONS_PATH
  echo "kLoggingTraceUsageTable 0" >> $HOST_OPTIONS_PATH
  echo "kLoggingTraceDecryptCalls 0" >> $HOST_OPTIONS_PATH
  echo "kLoggingDumpTraceAll 0" >> $HOST_OPTIONS_PATH
}

if adb shell ls -l /data/vendor/mediadrm; then
  TARGET_OPTIONS_PATH="/data/vendor/mediadrm/oemcrypto/$OPTIONS_TXT_FILENAME"
else
  TARGET_OPTIONS_PATH="/data/mediadrm/oemcrypto/$OPTIONS_TXT_FILENAME"
fi

## Test: OEMCertificateProvisioning
write_options_txt_file $OEMCERT
adb push $HOST_OPTIONS_PATH $TARGET_OPTIONS_PATH
adb shell /data/bin/cdm_feature_test --gtest_filter=WvCdmFeatureTest.OEMCertificateProvisioning

## Test: KeyboxProvisioning
write_options_txt_file
adb push $HOST_OPTIONS_PATH $TARGET_OPTIONS_PATH
adb shell /data/bin/cdm_feature_test --gtest_filter=WvCdmFeatureTest.KeyboxProvisioning


## Test: Srm|VersionCurrent|OPRequested|Updatable|NotRevoked|CanPlayback
write_options_txt_file $SRM_INITIAL_VERSION_CURRENT $SRM_UPDATE_SUPPORTED $SRM_NOT_REVOKED
adb push $HOST_OPTIONS_PATH $TARGET_OPTIONS_PATH
adb shell /data/bin/cdm_feature_test --gtest_filter=Cdm/WvCdmSrmTest.Srm/0

## Test: Srm|VersionCurrent|OPRequested|NotUpdatable|NotRevoked|CanPlayback
write_options_txt_file $SRM_INITIAL_VERSION_CURRENT $SRM_UPDATE_NOT_SUPPORTED $SRM_NOT_REVOKED
adb push $HOST_OPTIONS_PATH $TARGET_OPTIONS_PATH
adb shell /data/bin/cdm_feature_test --gtest_filter=Cdm/WvCdmSrmTest.Srm/1

## Test: Srm|VersionCurrent|OPRequired|Updatable|NotRevoked|CanPlayback
write_options_txt_file $SRM_INITIAL_VERSION_CURRENT $SRM_UPDATE_SUPPORTED $SRM_NOT_REVOKED
adb push $HOST_OPTIONS_PATH $TARGET_OPTIONS_PATH
adb shell /data/bin/cdm_feature_test --gtest_filter=Cdm/WvCdmSrmTest.Srm/2

## Test: Srm|VersionCurrent|OPRequired|NotUpdatable|NotRevoked|CanPlayback
write_options_txt_file $SRM_INITIAL_VERSION_CURRENT $SRM_UPDATE_NOT_SUPPORTED $SRM_NOT_REVOKED
adb push $HOST_OPTIONS_PATH $TARGET_OPTIONS_PATH
adb shell /data/bin/cdm_feature_test --gtest_filter=Cdm/WvCdmSrmTest.Srm/3

## Test: Srm|VersionOutdated|OPRequested|Updatable|NotRevoked|CanPlayback
write_options_txt_file $SRM_INITIAL_VERSION_OUTDATED $SRM_UPDATE_SUPPORTED $SRM_NOT_REVOKED
adb push $HOST_OPTIONS_PATH $TARGET_OPTIONS_PATH
adb shell /data/bin/cdm_feature_test --gtest_filter=Cdm/WvCdmSrmTest.Srm/4

## Test: Srm|VersionOutdated|OPRequested|NotUpdatable|NotRevoked|CanPlayback
write_options_txt_file $SRM_INITIAL_VERSION_OUTDATED $SRM_UPDATE_NOT_SUPPORTED $SRM_NOT_REVOKED
adb push $HOST_OPTIONS_PATH $TARGET_OPTIONS_PATH
adb shell /data/bin/cdm_feature_test --gtest_filter=Cdm/WvCdmSrmTest.Srm/5

## Test: Srm|VersionOutdated|OPRequired|Updatable|NotRevoked|CanPlayback
write_options_txt_file $SRM_INITIAL_VERSION_OUTDATED $SRM_UPDATE_SUPPORTED $SRM_NOT_REVOKED
adb push $HOST_OPTIONS_PATH $TARGET_OPTIONS_PATH
adb shell /data/bin/cdm_feature_test --gtest_filter=Cdm/WvCdmSrmTest.Srm/6

# TODO(rfrias): Uncomment out test after b/64946456 is addressed
## Test: Srm|VersionOutdated|OPRequired|NotUpdatable|NotRevoked|CannotPlayback
#write_options_txt_file $SRM_INITIAL_VERSION_OUTDATED $SRM_UPDATE_NOT_SUPPORTED $SRM_NOT_REVOKED
#adb push $HOST_OPTIONS_PATH $TARGET_OPTIONS_PATH
#adb shell /data/bin/cdm_feature_test --gtest_filter=Cdm/WvCdmSrmTest.Srm/7

## Test: Srm|VersionCurrent|OPRequested|Updatable|Revoked|CanPlayback
write_options_txt_file $SRM_INITIAL_VERSION_CURRENT $SRM_UPDATE_SUPPORTED $SRM_REVOKED
adb push $HOST_OPTIONS_PATH $TARGET_OPTIONS_PATH
adb shell /data/bin/cdm_feature_test --gtest_filter=Cdm/WvCdmSrmTest.Srm/8

## Test: Srm|VersionCurrent|OPRequested|NotUpdatable|Revoked|CanPlayback
write_options_txt_file $SRM_INITIAL_VERSION_CURRENT $SRM_UPDATE_NOT_SUPPORTED $SRM_REVOKED
adb push $HOST_OPTIONS_PATH $TARGET_OPTIONS_PATH
adb shell /data/bin/cdm_feature_test --gtest_filter=Cdm/WvCdmSrmTest.Srm/9

# TODO(rfrias): Uncomment out test after b/64946456 is addressed
## Test: Srm|VersionCurrent|OPRequired|Updatable|Revoked|CannotPlayback
#write_options_txt_file $SRM_INITIAL_VERSION_CURRENT $SRM_UPDATE_SUPPORTED $SRM_REVOKED
#adb push $HOST_OPTIONS_PATH $TARGET_OPTIONS_PATH
#adb shell /data/bin/cdm_feature_test --gtest_filter=Cdm/WvCdmSrmTest.Srm/10

# TODO(rfrias): Uncomment out test after b/64946456 is addressed
## Test: Srm|VersionCurrent|OPRequired|NotUpdatable|Revoked|CannotPlayback
#write_options_txt_file $SRM_INITIAL_VERSION_CURRENT $SRM_UPDATE_NOT_SUPPORTED $SRM_REVOKED
#adb push $HOST_OPTIONS_PATH $TARGET_OPTIONS_PATH
#adb shell /data/bin/cdm_feature_test --gtest_filter=Cdm/WvCdmSrmTest.Srm/11

## Test: Srm|VersionOutdated|OPRequested|Updatable|Revoked|CanPlayback
write_options_txt_file $SRM_INITIAL_VERSION_OUTDATED $SRM_UPDATE_SUPPORTED $SRM_REVOKED
adb push $HOST_OPTIONS_PATH $TARGET_OPTIONS_PATH
adb shell /data/bin/cdm_feature_test --gtest_filter=Cdm/WvCdmSrmTest.Srm/12

## Test: Srm|VersionOutdated|OPRequested|NotUpdatable|Revoked|CanPlayback
write_options_txt_file $SRM_INITIAL_VERSION_OUTDATED $SRM_UPDATE_NOT_SUPPORTED $SRM_REVOKED
adb push $HOST_OPTIONS_PATH $TARGET_OPTIONS_PATH
adb shell /data/bin/cdm_feature_test --gtest_filter=Cdm/WvCdmSrmTest.Srm/13

# TODO(rfrias): Uncomment out test after b/64946456 is addressed
## Test: Srm|VersionOutdated|OPRequired|Updatable|Revoked|CannotPlayback
#write_options_txt_file $SRM_INITIAL_VERSION_OUTDATED $SRM_UPDATE_SUPPORTED $SRM_REVOKED
#adb push $HOST_OPTIONS_PATH $TARGET_OPTIONS_PATH
#adb shell /data/bin/cdm_feature_test --gtest_filter=Cdm/WvCdmSrmTest.Srm/14

# TODO(rfrias): Uncomment out test after b/64946456 is addressed
## Test: Srm|VersionOutdated|OPRequired|NotUpdatable|Revoked|CannotPlayback
#write_options_txt_file $SRM_INITIAL_VERSION_OUTDATED $SRM_UPDATE_NOT_SUPPORTED $SRM_REVOKED
#adb push $HOST_OPTIONS_PATH $TARGET_OPTIONS_PATH
#adb shell /data/bin/cdm_feature_test --gtest_filter=Cdm/WvCdmSrmTest.Srm/15

## Test: Srm|VersionNotSupported|OPRequested|NotUpdatable|NotRevoked|CanPlayback
write_options_txt_file $SRM_INITIAL_VERSION_NOT_SUPPORTED $SRM_UPDATE_NOT_SUPPORTED $SRM_NOT_REVOKED
adb push $HOST_OPTIONS_PATH $TARGET_OPTIONS_PATH
adb shell /data/bin/cdm_feature_test --gtest_filter=Cdm/WvCdmSrmNotSupportedTest.Srm/0

# TODO(rfrias): Uncomment out test after b/64946456 is addressed
## Test: Srm|VersionNotSupported|OPRequired|NotUpdatable|NotRevoked|CannotPlayback
#write_options_txt_file $SRM_INITIAL_VERSION_NOT_SUPPORTED $SRM_UPDATE_NOT_SUPPORTED $SRM_NOT_REVOKED
#adb push $HOST_OPTIONS_PATH $TARGET_OPTIONS_PATH
#adb shell /data/bin/cdm_feature_test --gtest_filter=Cdm/WvCdmSrmNotSupportedTest.Srm/1
