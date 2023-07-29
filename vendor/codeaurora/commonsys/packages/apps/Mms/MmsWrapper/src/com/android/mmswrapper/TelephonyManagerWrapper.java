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
import android.provider.Settings.SettingNotFoundException;
import android.telecom.PhoneAccount;
import android.telephony.ServiceState;
import android.telephony.TelephonyManager;

public class TelephonyManagerWrapper {
    private static final String TAG = "TelephonyManagerWrapper";
    public static final int NETWORK_TYPE_IWLAN = TelephonyManager.NETWORK_TYPE_IWLAN;

    public static int getIntAtIndex(android.content.ContentResolver cr,
                                    String name, int index) throws SettingNotFoundException {
        int ret = TelephonyManager.getIntAtIndex(cr, name, index);
        LogUtils.logi(TAG, "getIntAtIndex name=" + name + "|index=" + index + "|ret=" + ret);
        return ret;
    }

    public static String getLine1Number(TelephonyManager tm, int subId) {
        String ret = tm.getLine1Number(subId);
        LogUtils.logi(TAG, "getLine1Number subId=" + subId + "|ret=" + ret);
        return ret;
    }

    public static boolean isNetworkRoaming(TelephonyManager tm, int subId) {
        boolean ret = tm.isNetworkRoaming(subId);
        LogUtils.logi(TAG, "isNetworkRoaming subId=" + subId + "|ret=" + ret);
        return ret;
    }

    public static boolean hasIccCard(TelephonyManager tm, int slotId) {
        boolean ret = tm.hasIccCard(slotId);
        LogUtils.logi(TAG, "hasIccCard slotId=" + slotId + "|ret=" + ret);
        return ret;
    }

    public static int getSimState(TelephonyManager tm, int slotId) {
        int ret = tm.getSimState(slotId);
        LogUtils.logi(TAG, "getSimState slotId=" + slotId + "|ret=" + ret);
        return ret;
    }

    public static String getSimCountryIso(TelephonyManager tm, int subId) {
        String ret = tm.getSimCountryIso(subId);
        LogUtils.logi(TAG, "getSimCountryIso subId=" + subId + "|ret=" + ret);
        return ret;
    }

    public static int getSubIdForPhoneAccount(TelephonyManager tm, PhoneAccount phoneAccount) {
        int ret = tm.getSubIdForPhoneAccount(phoneAccount);
        LogUtils.logi(TAG, "getSubIdForPhoneAccount=" + ret);
        return ret;
    }

    public static boolean isMultiSimEnabled(TelephonyManager tm) {
        boolean ret = tm.isMultiSimEnabled();
        LogUtils.logi(TAG, "isMultiSimEnabled=" + ret);
        return ret;
    }

    public static boolean isImsRegistered(TelephonyManager tm) {
        boolean ret = tm.isImsRegistered();
        LogUtils.logi(TAG, "isImsRegistered=" + ret);
        return ret;
    }

    public static int getCurrentPhoneType(TelephonyManager tm) {
        int ret = tm.getCurrentPhoneType();
        LogUtils.logi(TAG, "getCurrentPhoneType=" + ret);
        return ret;
    }

    public static int getCurrentPhoneType(TelephonyManager tm, int subId) {
        int ret = tm.getCurrentPhoneType(subId);
        LogUtils.logi(TAG, "getCurrentPhoneType subId=" + subId + "|ret=" + ret);
        return ret;
    }

    public static ServiceState getServiceStateForSubscriber(TelephonyManager tm, int subId) {
        return tm.getServiceStateForSubscriber(subId);
    }

    public static boolean isMobileDataEnabled(TelephonyManager tm, int subId) {
        boolean ret = false;
        if (tm.isMultiSimEnabled()) {
            LogUtils.logd(TAG, "isMobileDataEnabled subId:" + subId);
            if (subId > -1) {
                ret = tm.getDataEnabled(subId);
            }
        } else {
            LogUtils.logd(TAG, "isMobileDataEnabled SS");
            ret = tm.getDataEnabled();
        }
        LogUtils.logd(TAG, "isMobileDataEnabled: " + ret);
        return ret;
    }

    public static boolean is1xNetwork(Context context, int sub) {
        boolean ret = false;
        TelephonyManager tm = (TelephonyManager)context.getSystemService(Context.TELEPHONY_SERVICE);
        int dataType = tm.getNetworkType(sub);
        if (dataType == TelephonyManager.NETWORK_TYPE_1xRTT
                || dataType == TelephonyManager.NETWORK_TYPE_CDMA) {
            ret = true;
        }
        LogUtils.logd(TAG, "sub:" + sub + " is1xNetwork:" + ret);
        return ret;
    }

    public static String getNai(Context context, int phoneId) {
        boolean ret = false;
        TelephonyManager tm = (TelephonyManager)context.getSystemService(Context.TELEPHONY_SERVICE);
        return tm.getNai(phoneId);
    }

    public static boolean isCurrentRatIwlan(Context context, int subId) {
        boolean flag = false;
        TelephonyManager telephonyManager =
                (TelephonyManager) context.getSystemService(context.TELEPHONY_SERVICE);
        flag = (telephonyManager != null
                && (telephonyManager.getDataNetworkType(subId)
                        == TelephonyManagerWrapper.NETWORK_TYPE_IWLAN));
        LogUtils.logi(TAG, "isCurrentRatIwlan subId=" + subId + "flag=" + flag);
        return flag;
    }

    public static boolean isNetworkRoaming(Context context, int subId) {
        boolean flag = false;
        TelephonyManager telephonyManager =
                (TelephonyManager) context.getSystemService(context.TELEPHONY_SERVICE);
        flag = (telephonyManager != null && telephonyManager.isNetworkRoaming(subId));
        LogUtils.logi(TAG, "isNetworkRoaming subId=" + subId + "flag=" + flag);
        return flag;
    }
}
