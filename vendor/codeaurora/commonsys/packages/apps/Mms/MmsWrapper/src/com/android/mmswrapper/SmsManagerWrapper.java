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

import android.app.PendingIntent;
import android.telephony.SmsManager;

import java.util.ArrayList;

public class SmsManagerWrapper {
    private static final String TAG = "SmsManagerWrapper";

    public static final int RESULT_ERROR_FDN_CHECK_FAILURE =
            SmsManager.RESULT_ERROR_FDN_CHECK_FAILURE;
    public static final int RESULT_ERROR_LIMIT_EXCEEDED = SmsManager.RESULT_ERROR_LIMIT_EXCEEDED;

    /**
     * Get SMS prompt property,  enabled or not
     *
     * @return true if enabled, false otherwise
     */
    public static boolean isSMSPromptEnabled(SmsManager sm) {
        boolean ret = sm.isSMSPromptEnabled();
        LogUtils.logi(TAG, "isSMSPromptEnabled=" + ret);
        return ret;
    }

    /**
     * Get the capacity count of sms on Icc card
     *
     * @return the capacity count of sms on Icc card
     */
    public static int getSmsCapacityOnIcc(SmsManager sm) {
        int ret = sm.getSmsCapacityOnIcc();
        LogUtils.logi(TAG, "getSmsCapacityOnIcc=" + ret);
        return ret;
    }

    public static void sendMultipartTextMessage(SmsManager sm, String destinationAddress,
                                                String scAddress, ArrayList<String> parts,
                                                ArrayList<PendingIntent> sentIntents,
                                                ArrayList<PendingIntent> deliveryIntents,
                                                int priority, boolean isExpectMore,
                                                int validityPeriod) {
        LogUtils.logi(TAG, "sendMultipartTextMessage");
        sm.sendMultipartTextMessage(destinationAddress, scAddress, parts, sentIntents,
                deliveryIntents, priority, isExpectMore, validityPeriod);
    }

}
