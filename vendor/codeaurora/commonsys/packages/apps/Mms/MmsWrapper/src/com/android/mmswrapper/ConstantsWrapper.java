/*
* Copyright (c) 2017, The Linux Foundation. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
* * Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
* * Redistributions in binary form must reproduce the above
* copyright notice, this list of conditions and the following
* disclaimer in the documentation and/or other materials provided
* with the distribution.
* * Neither the name of The Linux Foundation nor the names of its
* contributors may be used to endorse or promote products derived
* from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

package com.android.mmswrapper;

import android.content.Context;
import android.content.Intent;
import android.provider.ContactsContract;
import android.provider.Settings;
import android.view.View;
import android.widget.MediaController;

import com.android.internal.telephony.IccCardConstants;
import com.android.internal.telephony.PhoneConstants;
import com.android.internal.telephony.RILConstants;
import com.android.internal.telephony.TelephonyIntents;
import com.android.internal.telephony.TelephonyProperties;

public class ConstantsWrapper {
    private static final String TAG = "ConstantsWrapper";

    /**
     * Sync with com.android.internal.telephony.PhoneConstants
     */
    public static final class Phone {
        public static final int SUB1 = PhoneConstants.SUB1;
        public static final int SUB2 = PhoneConstants.SUB2;
        public static final int MAX_PHONE_COUNT_DUAL_SIM = PhoneConstants.MAX_PHONE_COUNT_DUAL_SIM;
        public static final String SUBSCRIPTION_KEY = PhoneConstants.SUBSCRIPTION_KEY;
        public static final String SLOT_KEY = PhoneConstants.SLOT_KEY;
        public static final String APN_TYPE_MMS = PhoneConstants.APN_TYPE_MMS;
        public static final String APN_TYPE_ALL = PhoneConstants.APN_TYPE_ALL;
        public static final String PHONE_KEY = PhoneConstants.PHONE_KEY;
        public static final String REASON_VOICE_CALL_ENDED = com.android.internal.
                telephony.Phone.REASON_VOICE_CALL_ENDED;
    }

    /**
     * Sync with com.android.internal.telephony.RILConstants
     */
    public static final class RIL {
        public static final int NETWORK_MODE_LTE_ONLY = RILConstants.NETWORK_MODE_LTE_ONLY;
    }

    /**
     * Sync with com.android.internal.telephony.TelephonyIntents
     */
    public static final class TelephonyIntent {
        public static final String ACTION_SHOW_NOTICE_ECM_BLOCK_OTHERS =
                TelephonyIntents.ACTION_SHOW_NOTICE_ECM_BLOCK_OTHERS;
        public static final String ACTION_SERVICE_STATE_CHANGED =
                TelephonyIntents.ACTION_SERVICE_STATE_CHANGED;
        public static final String ACTION_SIM_STATE_CHANGED =
                TelephonyIntents.ACTION_SIM_STATE_CHANGED;
    }

    /**
     * Sync with com.android.internal.telephony.TelephonyProperties;
     */
    public static final class TelephonyProperty {
        public static final String PROPERTY_INECM_MODE =
                TelephonyProperties.PROPERTY_INECM_MODE;
        public static final String PROPERTY_ICC_OPERATOR_NUMERIC =
                TelephonyProperties.PROPERTY_ICC_OPERATOR_NUMERIC;
        public static final String PROPERTY_OPERATOR_ISROAMING =
                TelephonyProperties.PROPERTY_OPERATOR_ISROAMING;
    }

    /**
     * Sync with com.android.internal.telephony.IccCardConstants
     */
    public static final class IccCard {
        public static final String INTENT_KEY_ICC_STATE = IccCardConstants.INTENT_KEY_ICC_STATE;
        public static final String INTENT_VALUE_ICC_LOADED =
                IccCardConstants.INTENT_VALUE_ICC_LOADED;
    }

    /**
     * Sync with android.provider.Settings.Global
     */
    public static final class SettingsGlobal {
        public static final String MOBILE_DATA = Settings.Global.MOBILE_DATA;
        public static final String CDMA_SUBSCRIPTION_MODE = Settings.Global.CDMA_SUBSCRIPTION_MODE;
        public static final String PREFERRED_NETWORK_MODE = Settings.Global.PREFERRED_NETWORK_MODE;
    }

    /**
     * Sync with android.provider.ContactsContract
     * Sync with android.content.Intent
     */
    public static final class IntentConstants {
        public static final String EXTRA_PHONE_URIS = ContactsContract.Intents.EXTRA_PHONE_URIS;
        public static final String ACTION_DEVICE_STORAGE_FULL = Intent.ACTION_DEVICE_STORAGE_FULL;
        public static final String ACTION_DEVICE_STORAGE_NOT_FULL =
                Intent.ACTION_DEVICE_STORAGE_NOT_FULL;
    }

    public static boolean getConfigVoiceCapable(Context context) {
        boolean ret = context.getResources().getBoolean(
                com.android.internal.R.bool.config_voice_capable);
        LogUtils.logi(TAG, "getConfigVoiceCapable=" + ret);
        return ret;
    }

    public static boolean getConfigCellBroadcastAppLinks(Context context) {
        boolean ret = context.getResources().getBoolean(
                com.android.internal.R.bool.config_cellBroadcastAppLinks);
        LogUtils.logi(TAG, "getConfigCellBroadcastAppLinks=" + ret);
        return ret;
    }

    public static View getMediacontrollerProgress(MediaController mc) {
        LogUtils.logi(TAG, "getMediacontrollerProgress");
        return mc.findViewById(com.android.internal.R.id.mediacontroller_progress);
    }

}
