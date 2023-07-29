/* Copyright (c) 2015, 2019 The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

package org.codeaurora.ims;

import android.telephony.ims.ImsReasonInfo;
/**
 * The class contains definitions for Qti specific constants related to any
 * value added features for video telephony.
 */
/**
 * @hide
 */
public class QtiCallConstants {

    /**
     * Call substate bitmask values
     */

    /* Default case */
    public static final int CALL_SUBSTATE_NONE = 0;

    /* Indicates that the call is connected but audio attribute is suspended */
    public static final int CALL_SUBSTATE_AUDIO_CONNECTED_SUSPENDED = 0x1;

    /* Indicates that the call is connected but video attribute is suspended */
    public static final int CALL_SUBSTATE_VIDEO_CONNECTED_SUSPENDED = 0x2;

    /* Indicates that the call is established but media retry is needed */
    public static final int CALL_SUBSTATE_AVP_RETRY = 0x4;

    /* Indicates that the call is multitasking */
    public static final int CALL_SUBSTATE_MEDIA_PAUSED = 0x8;

    /* Mask containing all the call substate bits set */
    public static final int CALL_SUBSTATE_ALL = CALL_SUBSTATE_AUDIO_CONNECTED_SUSPENDED |
        CALL_SUBSTATE_VIDEO_CONNECTED_SUSPENDED | CALL_SUBSTATE_AVP_RETRY |
        CALL_SUBSTATE_MEDIA_PAUSED;

    /* Call substate extra key name */
    public static final String CALL_SUBSTATE_EXTRA_KEY = "CallSubstate";

    /* Call encryption status extra key. The value will be a boolean. */
    public static final String CALL_ENCRYPTION_EXTRA_KEY = "CallEncryption";

    /* Call History Info extra key. The value will be a ArrayList of Strings. */
    public static final String EXTRAS_CALL_HISTORY_INFO = "CallHistoryInfo";

    /* Call fail code extra key name */
    public static final String EXTRAS_KEY_CALL_FAIL_EXTRA_CODE  = "CallFailExtraCode";

    /* Emergency service category extra key name */
    public static final String EXTRAS_KEY_EMERGENCY_SERVICE_CATEGORY  = "EmergencyServiceCategory";

    /*Key to check whether user can mark a call unwanted*/
    public static final String CAN_MARK_UNWANTED_CALL = "CanMarkUnwantedCall";

    /*Key for verstat verification status*/
    public static final String VERSTAT_VERIFICATION_STATUS = "VerstatVerificationStatus";

    public static final String VONR_INFO = "isCall5G";

    /* Call fail error code for handover not feasible */
    public static final int CALL_FAIL_EXTRA_CODE_LTE_3G_HA_FAILED = 149;

    /* Call fail error code for validate Video call number */
    public static final int CALL_FAIL_EXTRA_CODE_LOCAL_VALIDATE_NUMBER = 150;

    /* Call fail error code for Retry CS call*/
    public static final int CALL_FAIL_EXTRA_CODE_CALL_CS_RETRY_REQUIRED =
            ImsReasonInfo.CODE_LOCAL_CALL_CS_RETRY_REQUIRED;

    /* Calls are rejected due to low battery */
    public static final int CALL_FAIL_EXTRA_CODE_LOCAL_LOW_BATTERY =
            ImsReasonInfo.CODE_LOCAL_LOW_BATTERY;

    /* call fail error code to retry ims call without rtt */
    public static final int CODE_RETRY_ON_IMS_WITHOUT_RTT = 3001;
    /* Unknown disconnect cause */
    public static final int DISCONNECT_CAUSE_UNSPECIFIED = -1;

    /**
     * Whether the IMS to CS retry is enabled
     * <p>
     * Type: int (0 for false, 1 for true)
     * @hide
     */
    public static final String IMS_TO_CS_RETRY_ENABLED = "qti.settings.cs_retry";

    /* Default camera zoom max */
    public static final int CAMERA_MAX_ZOOM = 100;

    /**
     * Controls dial request route for CS calls.
     * 0 - Use the default routing strategy.
     * 1 - Place the call over CS path
     * 2 - Place the call over PS path
     */
    public static final String EXTRA_CALL_DOMAIN =
            "org.codeaurora.extra.CALL_DOMAIN";
    public static final int DOMAIN_AUTOMATIC = 0;
    public static final int DOMAIN_CS = 1;
    public static final int DOMAIN_PS = 2;

    /**
     * Call supplementary services failures.
     * TODO: Rename the file to QtiCallConstants.java as generic IMS constants are added. This
     *  will be handled when we move the file to vendor/codeaurora/telephony project.
     */
    public static final int ERROR_CALL_CODE_UNSPECIFIED = -1;
    public static final int ERROR_CALL_SUPP_SVC_FAILED = 1;
    public static final int ERROR_CALL_SUPP_SVC_CANCELLED = 2;
    public static final int ERROR_CALL_SUPP_SVC_REINVITE_COLLISION = 3;

    /**
     * Private constructor. This class should not be instantiated.
     */
    private QtiCallConstants() {
    }

    /* UI Orientation Modes */
    public static final int ORIENTATION_MODE_UNSPECIFIED = -1;
    public static final int ORIENTATION_MODE_LANDSCAPE = 1;
    public static final int ORIENTATION_MODE_PORTRAIT = 2;
    public static final int ORIENTATION_MODE_DYNAMIC = 3;

    /* Orientation mode extra key name */
    public static final String ORIENTATION_MODE_EXTRA_KEY = "OrientationMode";

    /* Video call dataUsage Key that holds the data usage consumed by Video call
       on LTE/WLAN RATs */
    public static final String VIDEO_CALL_DATA_USAGE_KEY = "dataUsage";

    /* low battery extra key name that contains a boolean value,
       TRUE meaning battery is low else FALSE */
    public static final String LOW_BATTERY_EXTRA_KEY = "LowBattery";

   /* Upgrade/downgrade of a volte/vt call due to unknown reason. */
    public static final int CAUSE_CODE_UNSPECIFIED = 0;

    /* Upgrade of a volte call on request from local end */
    public static final int CAUSE_CODE_SESSION_MODIFY_UPGRADE_LOCAL_REQ = 1;

    /* Upgrade of a volte call on request from remote end */
    public static final int CAUSE_CODE_SESSION_MODIFY_UPGRADE_REMOTE_REQ = 2;

    /* Downgrade of a vt call on request from local end */
    public static final int CAUSE_CODE_SESSION_MODIFY_DOWNGRADE_LOCAL_REQ = 3;

    /* Downgrade of a vt call on request from remote end */
    public static final int CAUSE_CODE_SESSION_MODIFY_DOWNGRADE_REMOTE_REQ = 4;

    /* Downgrade of a vt call due to RTP/RTCP Timeout for Video stream */
    public static final int CAUSE_CODE_SESSION_MODIFY_DOWNGRADE_RTP_TIMEOUT = 5;

    /* Downgrade of a vt call due to QOS for Video stream */
    public static final int CAUSE_CODE_SESSION_MODIFY_DOWNGRADE_QOS = 6;

    /* Downgrade of a vt call due to PACKET LOSS for Video stream */
    public static final int CAUSE_CODE_SESSION_MODIFY_DOWNGRADE_PACKET_LOSS = 7;

    /* Downgrade of a vt call due to Low throughput for Video stream */
    public static final int CAUSE_CODE_SESSION_MODIFY_DOWNGRADE_LOW_THRPUT = 8;

    /* Downgrade of a vt call due to Thermal Mitigation */
    public static final int CAUSE_CODE_SESSION_MODIFY_DOWNGRADE_THERM_MITIGATION = 9;

    /* Downgrade of a vt call due to Lip-sync */
    public static final int CAUSE_CODE_SESSION_MODIFY_DOWNGRADE_LIPSYNC = 10;

    /* Downgrade of a vt call due to generic error */
    public static final int CAUSE_CODE_SESSION_MODIFY_DOWNGRADE_GENERIC_ERROR = 11;

    /* Session modification cause extra key name */
    public static final String SESSION_MODIFICATION_CAUSE_EXTRA_KEY = "SessionModificationCause";

    /** Modify call error due to low battery
     *  Value should not conflict with videoProvider.
     *      {@link VideoProvider#SESSION_MODIFY_REQUEST_REJECTED*}
     */
    public static final int SESSION_MODIFY_REQUEST_FAILED_LOW_BATTERY = 50;

    /**
     * Local device supports downgrade to voice
     */
    public static final int CAPABILITY_SUPPORTS_DOWNGRADE_TO_VOICE_LOCAL = 0x00800000;

    /**
      * Remote device supports downgrade to voice
      */
    public static final int CAPABILITY_SUPPORTS_DOWNGRADE_TO_VOICE_REMOTE = 0x01000000;

    /**
     * Add participant in an active or conference call option
     */
    public static final int CAPABILITY_ADD_PARTICIPANT = 0x02000000;

    /* Invalid phone Id */
    public static final int INVALID_PHONE_ID = -1;

    /**
     * Extra indicating the Wifi Quality
     * <p>
     * Type: int (one of the VOWIFI_QUALITY_* values)
     */
    public static final String VOWIFI_CALL_QUALITY_EXTRA_KEY = "VoWiFiCallQuality";

    public static final int VOWIFI_QUALITY_NONE = 0;
    public static final int VOWIFI_QUALITY_EXCELLENT = 1;
    public static final int VOWIFI_QUALITY_FAIR = 2;
    public static final int VOWIFI_QUALITY_POOR = 4;

    /**
     * Extra indicating the conference support from lower layers
     * <p>
     * Type: boolean (true if conference is supported else false)
     */
    public static final String CONF_SUPPORT_IND_EXTRA_KEY = "ConfSupportInd";

    public static final String EXTRA_PHONE_ID = "phoneId";

    //holds the call fail cause because of which redial is attempted
    public static final String EXTRA_RETRY_CALL_FAIL_REASON = "RetryCallFailReason";
    //holds the radiotech on which lower layers may try attempting redial
    public static final String EXTRA_RETRY_CALL_FAIL_RADIOTECH = "RetryCallFailRadioTech";


   /**
     * Whether RTT visibility is on or off
     * The value 1 - enable, 0 - disable
     * This is set through ImsSettings UI
     */
    public static final String QTI_IMS_RTT_VISIBILITY = "rtt_mode_visibility";

   /**
     * Property for RTT Operating mode
     * For TMO - 0 : Upon Request Mode (Disabled)
     *           1 : Automatic Mode (Full)
     * For Vzw - 1 : Automatic Mode (Full)
     *
     */
    public static final String PROPERTY_RTT_OPERATING_MODE = "persist.vendor.radio.rtt.operval";

    // RTT default phone id
    public static final int RTT_DEFAULT_PHONE_ID = 0;

    // RTT Off
    public static final int RTT_MODE_DISABLED = 0;

    // RTT On
    public static final int RTT_MODE_FULL = 1;

   // RTT Visibility Off
    public static final int RTT_VISIBILITY_DISABLED = 0;

    // RTT Visibility On
    public static final int RTT_VISIBILITY_ENABLED = 1;

   /**
     * Broadcast Action: Send RTT Text Message
     */
    public static final String ACTION_SEND_RTT_TEXT =
            "org.codeaurora.intent.action.send.rtt.text";

   /**
     * RTT Text Value
     */
    public static final String RTT_TEXT_VALUE =
            "org.codeaurora.intent.action.rtt.textvalue";

   /**
     * Broadcast Action: RTT Operation
     */
    public static final String ACTION_RTT_OPERATION =
            "org.codeaurora.intent.action.send.rtt.operation";

   /**
     * RTT Operation Type
     */
    public static final String RTT_OPERATION_TYPE =
            "org.codeaurora.intent.action.rtt.operation.type";

    // RTT Operation Type can be one of the following
    // To request upgrade of regular call to RTT call
    public static final int RTT_UPGRADE_INITIATE = 1;
    // To accept incoming RTT upgrade request
    public static final int RTT_UPGRADE_CONFIRM = 2;
    // To reject incoming RTT upgrade request
    public static final int RTT_UPGRADE_REJECT = 3;
    // To request downgrade of RTT call to regular call
    public static final int RTT_DOWNGRADE_INITIATE = 4;

    // Recorder Auto-Scaling Factor
    public static final int RECORDER_SCALING_FACTOR = 8;

    /**
     * Whether auto reject is enabled for IMS calls on a sub when high priority data
     * is on the other sub
     * Type: int (0 for disabled, 1 for enabled)
     */
    public static final String IMS_AUTO_REJECT = "qti.settings.auto_reject";
    // Auto reject call modes
    public static final int AUTO_REJECT_CALL_DISABLED = 0;
    public static final int AUTO_REJECT_CALL_ENABLED = 1;
    // Auto reject call UI item, avoid conflicting values from ImsCallUtils.ConfigItem
    public static final int AUTO_REJECT_CALL_MODE = 1000;
}

