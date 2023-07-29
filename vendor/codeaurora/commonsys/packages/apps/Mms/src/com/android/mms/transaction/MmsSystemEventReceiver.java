/*
 * Copyright (C) 2007-2008 Esmertec AG.
 * Copyright (C) 2007-2008 The Android Open Source Project
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

package com.android.mms.transaction;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.Uri;
import android.preference.PreferenceManager;
import android.provider.Telephony.Mms;
import android.telephony.SmsManager;
import android.telephony.SubscriptionManager;
import android.util.Log;

import com.android.mms.LogTag;
import com.android.mms.MmsApp;
import com.android.mms.R;
import com.android.mms.ui.MessageUtils;
import com.android.mms.ui.MessagingPreferenceActivity;
import com.android.mmswrapper.ConstantsWrapper;
import com.android.mmswrapper.ConnectivityManagerWrapper;

/**
 * MmsSystemEventReceiver receives the
 * {@link android.content.intent.ACTION_BOOT_COMPLETED},
 * {@link com.android.internal.telephony.TelephonyIntents.ACTION_ANY_DATA_CONNECTION_STATE_CHANGED}
 * and performs a series of operations which may include:
 * <ul>
 * <li>Show/hide the icon in notification area which is used to indicate
 * whether there is new incoming message.</li>
 * <li>Resend the MM's in the outbox.</li>
 * </ul>
 */
public class MmsSystemEventReceiver extends BroadcastReceiver {
    private static final String TAG = LogTag.TAG;
    private static ConnectivityManager mConnMgr = null;

    public static void wakeUpService(Context context) {
        if (Log.isLoggable(LogTag.TRANSACTION, Log.VERBOSE)) {
            Log.v(TAG, "wakeUpService: start transaction service ...");
        }

        context.startService(new Intent(context, TransactionService.class));
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        if (Log.isLoggable(LogTag.TRANSACTION, Log.VERBOSE)) {
            Log.v(TAG, "Intent received: " + intent);
        }
        if (!MessageUtils.hasBasicPermissions()) {
            Log.d(TAG, "MmsSystemEventReceiver do not have basic permissions");
            return;
        }

        String action = intent.getAction();
        if (action.equals(Mms.Intents.CONTENT_CHANGED_ACTION)) {
            Uri changed = (Uri) intent.getParcelableExtra(Mms.Intents.DELETED_CONTENTS);
            MmsApp.getApplication().getPduLoaderManager().removePdu(changed);
        } else if (action.equals(ConnectivityManager.CONNECTIVITY_ACTION)) {
            if (mConnMgr == null) {
                mConnMgr = (ConnectivityManager) context
                        .getSystemService(Context.CONNECTIVITY_SERVICE);
            }
            if (!ConnectivityManagerWrapper.getMobileDataEnabled(mConnMgr)) {
                if (Log.isLoggable(LogTag.TRANSACTION, Log.VERBOSE)) {
                    Log.v(TAG, "mobile data turned off, bailing");
                }
                return;
            }
            if (isMmsNetworkReadyForPendingMsg(context)) {
                wakeUpService(context);
            }

        } else if (action.equals(Intent.ACTION_AIRPLANE_MODE_CHANGED)) {
            boolean apm = intent.getBooleanExtra("state", false);
            if (!apm) {
                Log.d(TAG, "Airplane mode OFF");
                wakeUpService(context);
            }
        } else if (action.equals(ConstantsWrapper.TelephonyIntent.ACTION_SIM_STATE_CHANGED)) {
            String stateExtra = intent.getStringExtra(ConstantsWrapper.IccCard.INTENT_KEY_ICC_STATE);
            if (ConstantsWrapper.IccCard.INTENT_VALUE_ICC_LOADED.equals(stateExtra)) {
                MessagingNotification.nonBlockingUpdateNewMessageIndicator(
                        context, MessagingNotification.THREAD_NONE, false);
                if (isMmsNetworkReadyForPendingMsg(context)) {
                    wakeUpService(context);
                }
            }
        }
    }

    /**
     * isMmsNetworkReadyForPendingMsg
     *
     * @param context
     * @return mms network is available but not connected, means
     * it is ready to process pending list but not processing now.
     */
    private boolean isMmsNetworkReadyForPendingMsg(Context context) {
        if (null == mConnMgr) {
            mConnMgr = (ConnectivityManager) context
                    .getSystemService(Context.CONNECTIVITY_SERVICE);
        }
        NetworkInfo mmsNetworkInfo = mConnMgr
                .getNetworkInfo(ConnectivityManager.TYPE_MOBILE_MMS);
        if (null != mmsNetworkInfo) {
            boolean available = mmsNetworkInfo.isAvailable();
            boolean isConnected = mmsNetworkInfo.isConnected();
            LogTag.debugD("isMmsNetworkReadyForPendingMsg available = " + available +
                    ", isConnected = " + isConnected);
            return available && (!isConnected);
        } else {
            Log.e(TAG, "Empty mmsNetworkInfo!");
            return false;
        }
    }
}
