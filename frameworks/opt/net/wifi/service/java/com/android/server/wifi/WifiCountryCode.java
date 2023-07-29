/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.server.wifi;

import android.content.Context;
import android.content.Intent;
import android.net.wifi.WifiManager;
import android.os.UserHandle;
import android.text.TextUtils;
import android.util.Log;

import java.io.FileDescriptor;
import java.io.PrintWriter;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

/**
 * Provide functions for making changes to WiFi country code.
 * This Country Code is from MCC or phone default setting. This class sends Country Code
 * to driver through wpa_supplicant when ClientModeImpl marks current state as ready
 * using setReadyForChange(true).
 */
public class WifiCountryCode {
    private static final String TAG = "WifiCountryCode";
    private final WifiNative mWifiNative;
    private boolean DBG = false;
    private boolean mReady = false;
    private Context mContext;
    private static final SimpleDateFormat FORMATTER = new SimpleDateFormat("MM-dd HH:mm:ss.SSS");

    /** config option that indicate whether or not to reset country code to default when
     * cellular radio indicates country code loss
     */
    private boolean mRevertCountryCodeOnCellularLoss;
    private String mDefaultCountryCode = null;
    private String mTelephonyCountryCode = null;
    private String mDriverCountryCode = null;
    private String mTelephonyCountryTimestamp = null;
    private String mDriverCountryTimestamp = null;
    private String mReadyTimestamp = null;
    private boolean mForceCountryCode = false;

    public WifiCountryCode(
            Context context,
            WifiNative wifiNative,
            String oemDefaultCountryCode,
            boolean revertCountryCodeOnCellularLoss) {

        mContext = context;
        mWifiNative = wifiNative;
        mRevertCountryCodeOnCellularLoss = revertCountryCodeOnCellularLoss;

        if (!TextUtils.isEmpty(oemDefaultCountryCode)) {
            mDefaultCountryCode = oemDefaultCountryCode.toUpperCase(Locale.US);
        } else {
            if (mRevertCountryCodeOnCellularLoss) {
                Log.w(TAG, "config_wifi_revert_country_code_on_cellular_loss is set, "
                         + "but there is no default country code.");
                mRevertCountryCodeOnCellularLoss = false;
            }
        }

        Log.d(TAG, "mDefaultCountryCode " + mDefaultCountryCode
                + " mRevertCountryCodeOnCellularLoss " + mRevertCountryCodeOnCellularLoss);
    }

    public WifiCountryCode(
            WifiNative wifiNative,
            String oemDefaultCountryCode,
            boolean revertCountryCodeOnCellularLoss) {
        this(null, wifiNative, oemDefaultCountryCode, revertCountryCodeOnCellularLoss);
    }

    /**
     * Enable verbose logging for WifiCountryCode.
     */
    public void enableVerboseLogging(int verbose) {
        if (verbose > 0) {
            DBG = true;
        } else {
            DBG = false;
        }
    }

    private void sendCountryCodeChangedBroadcast() {
        if (mContext == null) {
            return;
        }

        Log.d(TAG, "sending WIFI_COUNTRY_CODE_CHANGED_ACTION");
        Intent intent = new Intent(WifiManager.WIFI_COUNTRY_CODE_CHANGED_ACTION);
        intent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY_BEFORE_BOOT);
        intent.putExtra(WifiManager.EXTRA_COUNTRY_CODE, getCountryCode());
        mContext.sendStickyBroadcastAsUser(intent, UserHandle.ALL);
    }

    /**
     * Change the state to indicates if wpa_supplicant is ready to handle country code changing
     * request or not.
     * We call native code to request country code changes only when wpa_supplicant is
     * started but not yet L2 connected.
     */
    public synchronized void setReadyForChange(boolean ready) {
        mReady = ready;
        mReadyTimestamp = FORMATTER.format(new Date(System.currentTimeMillis()));
        // We are ready to set country code now.
        // We need to post pending country code request.
        if (mReady) {
            updateCountryCode();
        }
    }

    /**
     * Enable force-country-code mode
     * @param countryCode The forced two-letter country code
     */
    synchronized void enableForceCountryCode(String countryCode) {
        if (TextUtils.isEmpty(countryCode)) {
            Log.d(TAG, "Fail to force country code because the received country code is empty");
            return;
        }
        mForceCountryCode = true;
        mTelephonyCountryCode = countryCode.toUpperCase(Locale.US);
        // If wpa_supplicant is ready we set the country code now, otherwise it will be
        // set once wpa_supplicant is ready.
        if (mReady) {
            updateCountryCode();
        } else {
            Log.d(TAG, "skip update supplicant not ready yet");
        }
    }

    /**
     * Disable force-country-code mode
     */
    synchronized void disableForceCountryCode() {
        mForceCountryCode = false;
        // Set mTelephonyCountryCode to null so that default country code is used until
        // next call of setCountryCode().
        mTelephonyCountryCode = null;
    }

    /**
     * Handle country code change request.
     * @param countryCode The country code intended to set.
     * This is supposed to be from Telephony service.
     * otherwise we think it is from other applications.
     * @return Returns true if the country code passed in is acceptable.
     */
    public synchronized boolean setCountryCode(String countryCode) {
        if (mForceCountryCode) {
            Log.d(TAG, "Country code can't be set because it is the force-country-code mode");
            return false;
        }
        Log.d(TAG, "Receive set country code request: " + countryCode);
        mTelephonyCountryTimestamp = FORMATTER.format(new Date(System.currentTimeMillis()));

        // Empty country code.
        if (TextUtils.isEmpty(countryCode)) {
            if (mRevertCountryCodeOnCellularLoss) {
                Log.d(TAG, "Received empty country code, reset to default country code");
                mTelephonyCountryCode = null;
            }
        } else {
            mTelephonyCountryCode = countryCode.toUpperCase(Locale.US);
        }
        // If wpa_supplicant is ready we set the country code now, otherwise it will be
        // set once wpa_supplicant is ready.
        if (mReady) {
            updateCountryCode();
        } else {
            Log.d(TAG, "skip update supplicant not ready yet");
        }
        sendCountryCodeChangedBroadcast();
        return true;
    }

    /**
     * Method to get the Country Code that was sent to wpa_supplicant.
     *
     * @return Returns the local copy of the Country Code that was sent to the driver upon
     * setReadyForChange(true).
     * If wpa_supplicant was never started, this may be null even if a SIM reported a valid
     * country code.
     * Returns null if no Country Code was sent to driver.
     */
    public synchronized String getCountryCodeSentToDriver() {
        return mDriverCountryCode;
    }

    /**
     * Method to return the currently reported Country Code from the SIM or phone default setting.
     *
     * @return The currently reported Country Code from the SIM. If there is no Country Code
     * reported from SIM, a phone default Country Code will be returned.
     * Returns null when there is no Country Code available.
     */
    public synchronized String getCountryCode() {
        return pickCountryCode();
    }

    /**
     * Method to dump the current state of this WifiCounrtyCode object.
     */
    public synchronized void dump(FileDescriptor fd, PrintWriter pw, String[] args) {

        pw.println("mRevertCountryCodeOnCellularLoss: " + mRevertCountryCodeOnCellularLoss);
        pw.println("mDefaultCountryCode: " + mDefaultCountryCode);
        pw.println("mDriverCountryCode: " + mDriverCountryCode);
        pw.println("mTelephonyCountryCode: " + mTelephonyCountryCode);
        pw.println("mTelephonyCountryTimestamp: " + mTelephonyCountryTimestamp);
        pw.println("mDriverCountryTimestamp: " + mDriverCountryTimestamp);
        pw.println("mReadyTimestamp: " + mReadyTimestamp);
        pw.println("mReady: " + mReady);
    }

    private void updateCountryCode() {
        String country = pickCountryCode();
        Log.d(TAG, "updateCountryCode to " + country);

        // We do not check if the country code equals the current one.
        // There are two reasons:
        // 1. Wpa supplicant may silently modify the country code.
        // 2. If Wifi restarted therefoere wpa_supplicant also restarted,
        // the country code counld be reset to '00' by wpa_supplicant.
        if (country != null) {
            setCountryCodeNative(country);
        }
        // We do not set country code if there is no candidate. This is reasonable
        // because wpa_supplicant usually starts with an international safe country
        // code setting: '00'.
    }

    private String pickCountryCode() {
        if (mTelephonyCountryCode != null) {
            return mTelephonyCountryCode;
        }
        if (mDefaultCountryCode != null) {
            return mDefaultCountryCode;
        }
        // If there is no candidate country code we will return null.
        return null;
    }

    private boolean setCountryCodeNative(String country) {
        mDriverCountryTimestamp = FORMATTER.format(new Date(System.currentTimeMillis()));
        if (mWifiNative.setCountryCode(mWifiNative.getClientInterfaceName(), country)) {
            Log.d(TAG, "Succeeded to set country code to: " + country);
            mDriverCountryCode = country;
            return true;
        }
        Log.d(TAG, "Failed to set country code to: " + country);
        return false;
    }
}

