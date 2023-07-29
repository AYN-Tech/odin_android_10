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

import android.telephony.SubscriptionManager;

import java.util.Arrays;

public class SubscriptionManagerWrapper {

    private static final String TAG = "SubscriptionManagerWrapper";
    public static final int INVALID_PHONE_INDEX = SubscriptionManager.INVALID_PHONE_INDEX;

    public static int getPhoneId(int subId) {
        int ret = SubscriptionManager.getPhoneId(subId);
        LogUtils.logi(TAG, "getPhoneId:" + subId + "=" + ret);
        return ret;
    }

    public static int getSlotId(int subId) {
        int ret = SubscriptionManager.getSlotIndex(subId);
        LogUtils.logi(TAG, "getSlotId:" + subId + "=" + ret);
        return ret;
    }

    public static int[] getSubId(int slotId) {
        int[] ret = SubscriptionManager.getSubId(slotId);
        LogUtils.logi(TAG, "getSubId:" + slotId + "=" + Arrays.toString(ret));
        return ret;
    }

    public static boolean isActiveSubId(SubscriptionManager sm, int subId) {
        boolean ret = sm.isActiveSubId(subId);
        LogUtils.logi(TAG, "isActiveSubId:" + subId + "=" + ret);
        return ret;
    }
}
