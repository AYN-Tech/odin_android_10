/**
 Copyright (c) 2015-2019 The Linux Foundation. All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are
 met:
     * Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above
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

package com.android.incallui;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.res.Resources;
import android.os.IBinder;
import android.os.PersistableBundle;
import android.os.RemoteException;
import android.telecom.Connection.VideoProvider;
import android.telephony.CarrierConfigManager;
import android.telephony.SubscriptionInfo;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;
import android.telephony.ims.ImsMmTelManager;
import android.widget.Toast;
import android.telecom.VideoProfile;
import com.android.dialer.util.PermissionsUtil;
import com.android.ims.ImsManager;
import com.android.incallui.call.CallList;
import com.android.incallui.call.DialerCall;
import com.android.incallui.call.state.DialerCallState;
import java.lang.reflect.*;
import org.codeaurora.ims.QtiCallConstants;
import org.codeaurora.ims.utils.QtiImsExtUtils;

/**
 * This class contains Qti specific utiltity functions.
 */
public class QtiCallUtils {

    private static String LOG_TAG = "QtiCallUtils";
    //Maximum number of IMS phones in device.
    private static final int MAX_IMS_PHONE_COUNT = 2;

    /**
     * Returns true if it is emergency number else false
     */
    public static boolean isEmergencyNumber(Context context, String number) {
        TelephonyManager telephonyManager =
                (TelephonyManager) context.getSystemService(Context.TELEPHONY_SERVICE);
        return telephonyManager.isEmergencyNumber(number);
    }

   /**
     * Displays the string corresponding to the resourceId as a Toast on the UI
     */
    public static void displayToast(Context context, int resourceId) {
      if (context == null) {
          Log.w(LOG_TAG, "displayToast context is null");
          return;
      }
      displayToast(context, context.getResources().getString(resourceId));
    }

    /**
     * Displays the message as a Toast on the UI
     */
    public static void displayToast(Context context, String msg) {
      if (context == null) {
          Log.w(LOG_TAG, "displayToast context is null");
          return;
      }
      Toast.makeText(context, msg, Toast.LENGTH_SHORT).show();
    }

   /**
     * Checks the boolean flag in config file to figure out if we are going to use Qti extension or
     * not
     */
    public static boolean useExt(Context context) {
        if (context == null) {
            Log.w(context, "Context is null...");
        }
        return context != null && context.getResources().getBoolean(R.bool.video_call_use_ext);
    }

    /**
     * Converts the call type to string
     */
    public static String callTypeToString(int callType) {
        switch (callType) {
            case VideoProfile.STATE_BIDIRECTIONAL:
                return "VT";
            case VideoProfile.STATE_TX_ENABLED:
                return "VT_TX";
            case VideoProfile.STATE_RX_ENABLED:
                return "VT_RX";
        }
        return "";
    }

    public static boolean isVideoBidirectional(DialerCall call) {
        return call != null && VideoProfile.isBidirectional(call.getVideoState());
    }

    public static boolean isVideoTxOnly(DialerCall call) {
        if (call == null) {
            return false;
        }
        return isVideoTxOnly(call.getVideoState());
    }

    public static boolean isVideoTxOnly(int videoState) {
        return VideoProfile.isTransmissionEnabled(videoState) &&
                !VideoProfile.isReceptionEnabled(videoState);
    }

    public static boolean isVideoRxOnly(DialerCall call) {
        if (call == null) {
            return false;
        }
        int videoState = call.getVideoState();
        return !VideoProfile.isTransmissionEnabled(videoState) &&
                VideoProfile.isReceptionEnabled(videoState);
    }

    /**
     * Returns true if the CAPABILITY_CANNOT_DOWNGRADE_VIDEO_TO_AUDIO is set to false.
     * Note that - CAPABILITY_SUPPORTS_DOWNGRADE_TO_VOICE_LOCAL and
     * CAPABILITY_SUPPORTS_DOWNGRADE_TO_VOICE_REMOTE maps to
     * CAPABILITY_CANNOT_DOWNGRADE_VIDEO_TO_AUDIO
     */
    public static boolean hasVoiceCapabilities(DialerCall call) {
        return call != null &&
                !call.can(android.telecom.Call.Details.CAPABILITY_CANNOT_DOWNGRADE_VIDEO_TO_AUDIO);
    }

    /**
     * Returns true if local has the VT Transmit and if remote capability has VT Receive set i.e.
     * Local can transmit and remote can receive
     */
    public static boolean hasTransmitVideoCapabilities(DialerCall call) {
        return call != null &&
                call.can(android.telecom.Call.Details.CAPABILITY_SUPPORTS_VT_LOCAL_TX)
                && call.can(android.telecom.Call.Details.CAPABILITY_SUPPORTS_VT_REMOTE_RX);
    }

    /**
     * Returns true if local has the VT Receive and if remote capability has VT Transmit set i.e.
     * Local can transmit and remote can receive
     */
    public static boolean hasReceiveVideoCapabilities(DialerCall call) {
        return call != null &&
                call.can(android.telecom.Call.Details.CAPABILITY_SUPPORTS_VT_LOCAL_RX)
                && call.can(android.telecom.Call.Details.CAPABILITY_SUPPORTS_VT_REMOTE_TX);
    }

     /**
      * Returns true if both voice and video capabilities (see above) are set
      */
     public static boolean hasVoiceOrVideoCapabilities(DialerCall call) {
         return hasVoiceCapabilities(call) || hasTransmitVideoCapabilities(call)
                 || hasReceiveVideoCapabilities(call);
     }

    public static CharSequence getLabelForIncomingWifiVideoCall(Context context) {
        final DialerCall call = getIncomingOrActiveCall();

        if (call == null) {
            return context.getString(R.string.contact_grid_incoming_wifi_video_call);
        }

        final int requestedVideoState = call.getVideoTech().getRequestedVideoState();

        if (QtiCallUtils.isVideoRxOnly(call)
            || requestedVideoState == VideoProfile.STATE_RX_ENABLED) {
            return context.getString(R.string.incoming_wifi_video_rx_call);
        } else if (QtiCallUtils.isVideoTxOnly(call)
            || requestedVideoState == VideoProfile.STATE_TX_ENABLED) {
            return context.getString(R.string.incoming_wifi_video_tx_call);
        } else {
            return context.getString(R.string.contact_grid_incoming_wifi_video_call);
        }
    }

    public static CharSequence getLabelForIncomingVideoCall(Context context) {
        final DialerCall call = getIncomingOrActiveCall();
        if (call == null) {
            return context.getString(R.string.contact_grid_incoming_video_call);
        }

        final int requestedVideoState = call.getVideoTech().getRequestedVideoState();

        if (QtiCallUtils.isVideoRxOnly(call)
            || requestedVideoState == VideoProfile.STATE_RX_ENABLED) {
            return context.getString(R.string.incoming_video_rx_call);
        } else if (QtiCallUtils.isVideoTxOnly(call)
            || requestedVideoState == VideoProfile.STATE_TX_ENABLED) {
            return context.getString(R.string.incoming_video_tx_call);
        } else {
            return context.getString(R.string.contact_grid_incoming_video_call);
        }
    }

    public static DialerCall getIncomingOrActiveCall() {
        CallList callList = InCallPresenter.getInstance().getCallList();
        if (callList == null) {
           return null;
        } else {
           return callList.getIncomingOrActive();
        }
    }

    public static DialerCall getIncomingCall() {
        CallList callList = InCallPresenter.getInstance().getCallList();
        return callList == null ? null : callList.getIncomingCall();
    }

    /** This method converts the QtiCallConstants' Orientation modes to the ActivityInfo
     * screen orientation.
     */
    public static int toScreenOrientation(int orientationMode) {
        switch(orientationMode) {
            case QtiCallConstants.ORIENTATION_MODE_LANDSCAPE:
                return ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE;
            case QtiCallConstants.ORIENTATION_MODE_PORTRAIT:
                return ActivityInfo.SCREEN_ORIENTATION_PORTRAIT;
            case QtiCallConstants.ORIENTATION_MODE_DYNAMIC:
                return ActivityInfo.SCREEN_ORIENTATION_FULL_SENSOR;
            default:
                return ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;
        }
    }

    private static boolean enforceReadPhoneState(Context context, String message) {
        try {
            context.enforceCallingOrSelfPermission(
                    android.Manifest.permission.READ_PRIVILEGED_PHONE_STATE, message);
            return true;
        } catch (SecurityException e) {
            context.enforceCallingOrSelfPermission(android.Manifest.permission.READ_PHONE_STATE,
                    message);
            return false;
        }
    }

    public static int getSubId(Context context, int phoneId) {
        SubscriptionManager subscriptionManager =
                (SubscriptionManager) context.getSystemService(
                Context.TELEPHONY_SUBSCRIPTION_SERVICE);
        if (subscriptionManager == null) {
            Log.e(LOG_TAG, "getSubId SubscriptionManager is null");
            return SubscriptionManager.INVALID_SUBSCRIPTION_ID;
        }

        SubscriptionInfo subInfo =
                subscriptionManager.getActiveSubscriptionInfoForSimSlotIndex(phoneId);
        if (subInfo == null) {
            Log.e(LOG_TAG, "getSubId subInfo is null");
            return SubscriptionManager.INVALID_SUBSCRIPTION_ID;
        }

        return subInfo.getSubscriptionId();
    }

   /**
    * Whether ims is registered
    * @param context of the activity
    * @param int phoneId which need to check
    * @return boolean whether ims is registered
    */
    private static boolean isImsRegistered(Context context, int phoneId,
            TelephonyManager telephonyManager) {
        int subId = getSubId(context, phoneId);
        if (!SubscriptionManager.isValidSubscriptionId(subId)) {
            Log.e(LOG_TAG, "isImsRegistered subId is invalid");
            return false;
        }
        return telephonyManager.isImsRegistered(subId);
    }

   /**
    * Show 4G Conference call menu option unless both SIMs are specific operators SIMs
    * and both are not VoLTE/VT enabled.
    * @param context of the activity.
    * @return boolean whether should show 4G conference dialer menu option.
    */
    public static boolean show4gConferenceDialerMenuOption(Context context) {
        if (!PermissionsUtil.hasPhonePermissions(context) || hasConferenceCall() ||
                !enforceReadPhoneState(context, "show4gConferenceDialerMenuOption")) {
            Log.i(LOG_TAG, "show4gConferenceDialerMenuOption no phone permissions");
            return false;
        }
        Log.i(LOG_TAG, "inside show4gConferenceDialerMenuOption");
        int unregisteredSpecificImsPhoneCount = 0;
        TelephonyManager telephonyManager =
                (TelephonyManager) context.getSystemService(Context.TELEPHONY_SERVICE);
        CarrierConfigManager configManager =
                (CarrierConfigManager) context.getSystemService(Context.CARRIER_CONFIG_SERVICE);
        final int phoneCount = telephonyManager.getPhoneCount();
        boolean isEnhanced4gLteModeSettingEnabled = false;
        boolean isVolteEnabledByPlatform = false;
        for (int i = 0; i < phoneCount; i++) {
            final boolean isImsRegistered = isImsRegistered(context, i, telephonyManager);
            Log.i(LOG_TAG, "phoneId = " + i + " isImsRegistered = " + isImsRegistered);
            final boolean isCarrierConfigEnabled = QtiImsExtUtils.isCarrierConfigEnabled(i,
                    context, "config_enable_conference_dialer");

            if (isImsRegistered) {
                return true;
            } else if (!isImsRegistered && isCarrierConfigEnabled) {
                unregisteredSpecificImsPhoneCount++;
            } else if (!isCarrierConfigEnabled) {
                int subId = getSubId(context, i);
                if (SubscriptionManager.isValidSubscriptionId(subId)) {
                    ImsMmTelManager imsMmTelMgr = ImsMmTelManager.
                            createForSubscriptionId(subId);
                    isEnhanced4gLteModeSettingEnabled |=
                            imsMmTelMgr.isAdvancedCallingSettingEnabled();
                    if (configManager != null) {
                        PersistableBundle b = configManager.getConfigForSubId(subId);
                        if (b != null) {
                            isVolteEnabledByPlatform |= b.getBoolean(
                                    CarrierConfigManager.KEY_CARRIER_VOLTE_AVAILABLE_BOOL);
                        }
                    }
                }
            }
        }
        Log.i(LOG_TAG, "unregisteredSpecificImsPhoneCount = " + unregisteredSpecificImsPhoneCount);
        return unregisteredSpecificImsPhoneCount < MAX_IMS_PHONE_COUNT &&
                (isEnhanced4gLteModeSettingEnabled && isVolteEnabledByPlatform);
    }

   /**
    * Show Add to 4G Conference call option in Dialpad menu if at least one SIM is
    * specific operators SIM and has VoLTE/VT enabled.
    * @param context of the activity.
    * @return boolean whether should show add to 4G conference call menu option.
    */
    public static boolean showAddTo4gConferenceCallOption(Context context) {
        if (!PermissionsUtil.hasPhonePermissions(context) || hasConferenceCall()) {
            return false;
        }
        TelephonyManager telephonyManager =
                (TelephonyManager) context.getSystemService(Context.TELEPHONY_SERVICE);
        final int phoneCount = telephonyManager.getPhoneCount();
        for (int i = 0; i < phoneCount; i++) {
            final boolean isImsRegistered = isImsRegistered(context, i, telephonyManager);
            Log.i(LOG_TAG, "phoneId = " + i + " isImsRegistered = " + isImsRegistered);
            if (isImsRegistered && QtiImsExtUtils.isCarrierConfigEnabled(i, context,
                    "config_enable_conference_dialer")) {
                return true;
            }
        }
        return false;
    }

    /**
     * Open conference uri dialer or 4G conference dialer.
     * @param context of the activity.
     * @return void.
     */
    public static void openConferenceUriDialerOr4gConferenceDialer(Context context) {
        if (!PermissionsUtil.hasPhonePermissions(context)) {
            return;
        }
        boolean shallOpenOperator4gDialer = false;
        int registeredImsPhoneCount = 0;
        TelephonyManager telephonyManager =
                (TelephonyManager) context.getSystemService(Context.TELEPHONY_SERVICE);
        final int phoneCount = telephonyManager.getPhoneCount();
        for (int i = 0; i < phoneCount; i++) {
            final boolean isImsRegistered = isImsRegistered(context, i, telephonyManager);
            Log.i(LOG_TAG, "phoneId = " + i + " isImsRegistered = " + isImsRegistered);
            if (isImsRegistered) {
                registeredImsPhoneCount++;
                if (QtiImsExtUtils.isCarrierConfigEnabled(i, context,
                        "config_enable_conference_dialer")) {
                    if (!shallOpenOperator4gDialer) {
                        shallOpenOperator4gDialer = true;
                    } else {
                        //Both two subs have specific operators SIM.
                        //Need to open the specific operators 4g Dialer.
                        registeredImsPhoneCount--;
                    }
                }
            }
        }
        Log.i(LOG_TAG, "registeredImsPhoneCount = " + registeredImsPhoneCount);
        if((registeredImsPhoneCount < MAX_IMS_PHONE_COUNT) && shallOpenOperator4gDialer) {
            //Launch 4G conference dialer: Specific Operator reg in IMS and only one sub reg in ims.
            context.startActivity(getConferenceDialerIntent(null));
        } else if (shallOpenOperator4gDialer && (registeredImsPhoneCount > 1)) {
            //Launch user chosen 4G dialer: Specific Operator reg in IMS and another sub
            //also reg in ims.
            openUserSelected4GDialer(context);
        } else {
            //Launch conference URI dialer: Specific Operator not reg in IMS but other
            //operator reg in ims.
            context.startActivity(getConferenceDialerIntent());
        }
    }

    /**
    * Open user selected 4G dialer.
    * @param context of the activity.
    * @return void.
    */
    public static void openUserSelected4GDialer(Context context) {
        Resources resources = context.getResources();
        CharSequence options[] = new CharSequence[] {
            resources.getString(R.string.conference_uri_dialer_option),
            resources.getString(R.string.conference_4g_dialer_option)};
        AlertDialog.Builder builder = new AlertDialog.Builder(context);
        builder.setTitle(R.string.select_your_option);
        builder.setItems(options, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                //The user clicked on options[which]
                Log.d(LOG_TAG, "onClick : which option = " + which);
                if (which == 1) {
                    //Launch 4G conference dialer.
                    context.startActivity(getConferenceDialerIntent(null));
                } else {
                    //Launch conference URI dialer:
                    context.startActivity(getConferenceDialerIntent());
                }
            }
            });
        builder.setNegativeButton(R.string.select_your_4g_dialer_cancel_option,
                new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                //The user clicked on Cancel
            }
        });
        AlertDialog alertDialog = builder.create();
        alertDialog.show();
    }

   /**
    * get intent to start conference dialer
    * with this intent, we can originate an conference call
    */
    public static Intent getConferenceDialerIntent() {
        Intent intent = new Intent("org.codeaurora.confuridialer.ACTION_LAUNCH_CONF_URI_DIALER");
        return intent;
    }

   /**
    * get intent to start conference dialer
    * with this intent, we can originate an conference call
    */
    public static Intent getConferenceDialerIntent(String number) {
        Intent intent = new Intent("org.codeaurora.confdialer.ACTION_LAUNCH_CONF_DIALER");
        intent.putExtra("confernece_number_key", number);
        return intent;
    }

   /**
    * used to get intent to start conference dialer
    * with this intent, we can add participants to an existing conference call
    */
    public static Intent getAddParticipantsIntent() {
        Intent intent = new Intent("org.codeaurora.confuridialer.ACTION_LAUNCH_CONF_URI_DIALER");
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra("add_participant", true);
        return intent;
    }

   /**
    * used to get intent to start conference dialer
    * with this intent, we can add participants to an existing conference call
    */
    public static Intent getAddParticipantsIntent(String number) {
        Intent intent = new Intent("org.codeaurora.confdialer.ACTION_LAUNCH_CONF_DIALER");
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra("add_participant", true);
        intent.putExtra("current_participant_list", number);
        return intent;
    }

    //Checks if DialerCall has video CRBT - an outgoing receive-only video call
    public static boolean hasVideoCrbtVoLteCall(Context context, DialerCall call) {
        if (context == null || !QtiImsExtUtils.isCarrierConfigEnabled(
            BottomSheetHelper.getInstance().getPhoneId(),
            context, "config_enable_video_crbt")) {
        }
        return (call != null && call.getState() == DialerCallState.DIALING
            && isVideoRxOnly(call));
    }

    //Checks if CallList has CRBT VoLTE call - an outgoing receive-only video call
    public static boolean hasVideoCrbtVoLteCall(Context context) {
        if (context == null || !QtiImsExtUtils.isCarrierConfigEnabled(
            BottomSheetHelper.getInstance().getPhoneId(),
            context, "config_enable_video_crbt")) {
            return false;
        }
        DialerCall call = CallList.getInstance().getFirstCall();
        return (call != null && call.getState() == DialerCallState.DIALING
                && isVideoRxOnly(call));
    }

    //Checks if CallList has CRBT Video Call. An outgoing bidirectional video call
    //is treated as CRBT video call if CRBT feature is enabled
    public static boolean hasVideoCrbtVtCall(Context context) {
        DialerCall call = CallList.getInstance().getFirstCall();
        boolean videoCrbtConfig = QtiImsExtUtils.isCarrierConfigEnabled(
                 BottomSheetHelper.getInstance().getPhoneId(),
                 context, "config_enable_video_crbt");
        return (videoCrbtConfig && call != null && call.getState() == DialerCallState.DIALING
                && isVideoBidirectional(call));
    }

    //Checks if CallList has conference call
    public static boolean hasConferenceCall() {
        DialerCall activeCall = CallList.getInstance().getActiveCall();
        boolean hasConfCall = activeCall != null ? activeCall.isConferenceCall() : false;
        if (!hasConfCall) {
            DialerCall bgCall = CallList.getInstance().getBackgroundCall();
            hasConfCall = bgCall != null ? bgCall.isConferenceCall() : false;
        }
        return hasConfCall;
    }
}
