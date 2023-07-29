/*
 * Copyright (c) 2012-2014, 2017 The Linux Foundation. All rights reserved.
 * Not a Contribution.
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

import java.io.IOException;
import java.lang.Long;
import java.util.ArrayList;
import java.util.Iterator;

import android.app.NotificationManager;
import android.app.Notification;
import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.content.ContentValues;
import android.content.Intent;
import android.content.IntentFilter;
import android.database.Cursor;
import android.database.sqlite.SqliteWrapper;
import android.database.DatabaseUtils;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.PowerManager;
import android.os.SystemProperties;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkRequest;
import android.net.LinkProperties;
import android.provider.Telephony.Mms;
import android.provider.Telephony.MmsSms;
import android.provider.Telephony.Mms.Sent;
import android.provider.Telephony.MmsSms.PendingMessages;
import android.provider.Settings;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;
import android.text.TextUtils;
import android.util.Log;
import android.widget.Toast;

import com.android.mms.LogTag;
import com.android.mms.MmsApp;
import com.android.mms.MmsConfig;
import com.android.mms.R;
import com.android.mms.ui.MessageUtils;
import com.android.mms.util.DownloadManager;
import com.android.mms.util.RateController;
import com.android.mmswrapper.ConnectivityManagerWrapper;
import com.android.mmswrapper.ConstantsWrapper;
import com.android.mmswrapper.SubscriptionManagerWrapper;
import com.android.mmswrapper.TelephonyManagerWrapper;
import com.google.android.mms.pdu.GenericPdu;
import com.google.android.mms.pdu.NotificationInd;
import com.google.android.mms.pdu.PduHeaders;
import com.google.android.mms.pdu.PduParser;
import com.google.android.mms.pdu.PduPersister;
import android.content.SharedPreferences;
import com.android.mms.ui.MmsPreferenceActivity;
import android.preference.PreferenceManager;

/**
 * The TransactionService of the MMS Client is responsible for handling requests
 * to initiate client-transactions sent from:
 * <ul>
 * <li>The Proxy-Relay (Through Push messages)</li>
 * <li>The composer/viewer activities of the MMS Client (Through intents)</li>
 * </ul>
 * The TransactionService runs locally in the same process as the application.
 * It contains a HandlerThread to which messages are posted from the
 * intent-receivers of this application.
 * <p/>
 * <b>IMPORTANT</b>: This is currently the only instance in the system in
 * which simultaneous connectivity to both the mobile data network and
 * a Wi-Fi network is allowed. This makes the code for handling network
 * connectivity somewhat different than it is in other applications. In
 * particular, we want to be able to send or receive MMS messages when
 * a Wi-Fi connection is active (which implies that there is no connection
 * to the mobile data network). This has two main consequences:
 * <ul>
 * <li>Testing for current network connectivity ({@link android.net.NetworkInfo#isConnected()} is
 * not sufficient. Instead, the correct test is for network availability
 * ({@link android.net.NetworkInfo#isAvailable()}).</li>
 * <li>If the mobile data network is not in the connected state, but it is available,
 * we must initiate setup of the mobile data connection, and defer handling
 * the MMS transaction until the connection is established.</li>
 * </ul>
 */
public class TransactionService extends Service implements Observer {
    private static final String TAG = LogTag.TAG;

    /**
     * Used to identify notification intents broadcasted by the
     * TransactionService when a Transaction is completed.
     */
    public static final String TRANSACTION_COMPLETED_ACTION =
            "android.intent.action.TRANSACTION_COMPLETED_ACTION";

    /**
     * Action for the Intent which is sent by Alarm service to launch
     * TransactionService.
     */
    public static final String ACTION_ONALARM = "android.intent.action.ACTION_ONALARM";

    /**
     * Action for the Intent which is sent when the user turns on the auto-retrieve setting.
     * This service gets started to auto-retrieve any undownloaded messages.
     */
    public static final String ACTION_ENABLE_AUTO_RETRIEVE
            = "android.intent.action.ACTION_ENABLE_AUTO_RETRIEVE";

    /**
     * Used as extra key in notification intents broadcasted by the TransactionService
     * when a Transaction is completed (TRANSACTION_COMPLETED_ACTION intents).
     * Allowed values for this key are: TransactionState.INITIALIZED,
     * TransactionState.SUCCESS, TransactionState.FAILED.
     */
    public static final String STATE = "state";

    /**
     * Used as extra key in notification intents broadcasted by the TransactionService
     * when a Transaction is completed (TRANSACTION_COMPLETED_ACTION intents).
     * Allowed values for this key are any valid content uri.
     */
    public static final String STATE_URI = "uri";

    public static final String CANCEL_URI = "canceluri";

    private static final int EVENT_TRANSACTION_REQUEST = 1;
    private static final int EVENT_CONTINUE_MMS_CONNECTIVITY = 3;
    private static final int EVENT_HANDLE_NEXT_PENDING_TRANSACTION = 4;
    private static final int EVENT_NEW_INTENT = 5;
    private static final int EVENT_MMS_CONNECTIVITY_TIMEOUT = 6;
    private static final int EVENT_MMS_PDP_ACTIVATION_TIMEOUT = 7;
    private static final int EVENT_QUIT = 100;

    // delay 75 seconds to check whether we have enabled mms connectivity
    private static final int MMS_CONNECTIVITY_DELAY = 75 * 1000;
    private static final int MMS_CONNECTIVITY_RETRY_TIMES = 3;

    private static final int TOAST_MSG_QUEUED = 1;
    private static final int TOAST_DOWNLOAD_LATER = 2;
    private static final int TOAST_NO_APN = 3;
    private static final int TOAST_SEND_FAILED_RETRY = 4;
    private static final int TOAST_DOWNLOAD_FAILED_RETRY = 5;
    private static final int TOAST_SETUP_DATA_CALL_FAILED_FOR_SEND = 6;
    private static final int TOAST_SETUP_DATA_CALL_FAILED_FOR_DOWNLOAD = 7;
    private static final int TOAST_NONE = -1;

    // How often to extend the use of the MMS APN while a transaction
    // is still being processed.
    private static final int APN_EXTENSION_WAIT = 30 * 1000;
    private static final int PDP_ACTIVATION_TIMEOUT = 60 * 1000;

    private ServiceHandler mServiceHandler;
    private Looper mServiceLooper;
    private final ArrayList<Transaction> mProcessing  = new ArrayList<Transaction>();
    private final ArrayList<Transaction> mPending  = new ArrayList<Transaction>();
    private ConnectivityManager mConnMgr;

    private PowerManager.WakeLock mWakeLock;
    private int mMmsConnecvivityRetryCount;

    private ConnectivityManager.NetworkCallback[] mMmsNetworkCallback = null;
    private NetworkRequest[] mMmsNetworkRequest = null;

    private boolean[] mIsAvailable = null;
    private TelephonyManager mTelephonyManager = null;
    private int mPhoneCount;

    private static TransactionService sInstance = null;
    public static TransactionService getInstance() {
        return sInstance;
    }

    private static boolean MMS_DELAY_PREF_DEFAULT = false;
    private static int MMS_DELAY_PERIOD_DEFAULT = 100*1000;
    private static final int EVENT_MMS_RELEASE_NETWORK_DELAY = 8;

    private ConnectivityManager.NetworkCallback  getNetworkCallback(String subId) {
        final String mSubId = subId;
        final int mPhoneId = SubscriptionManagerWrapper.getPhoneId(Integer.parseInt(mSubId));

        return new ConnectivityManager.NetworkCallback() {
            @Override
            public void onAvailable(Network network) {
                if (isPhoneIdValid(mPhoneId)) {
                    mIsAvailable[mPhoneId] = true;
                    Log.d(TAG, "sub:" + mSubId
                            + "NetworkCallback.onAvailable: network=" + network
                            + " mIsAvailable=" + mIsAvailable[mPhoneId]);
                }
                mServiceHandler.removeMessages(EVENT_MMS_PDP_ACTIVATION_TIMEOUT,
                        Integer.parseInt(mSubId));
                onMmsPdpConnected(mSubId, network);

            }
            @Override
            public void onLosing(Network network, int timeToLive) {
                Log.d(TAG, "sub:" + mSubId + "NetworkCallback.onLosing: network="
                        + network + ", maxTimeToLive= " + timeToLive);
            }
            @Override
            public void onLost(Network network) {
                if (isPhoneIdValid(mPhoneId)) {
                    return;
                }
                mIsAvailable[mPhoneId] = false;
                Log.d(TAG, "sub:" + mSubId + "NetworkCallback.onLost: network=" + network +
                        " mIsAvailable=" + mIsAvailable[mPhoneId]);
            }
            @Override
            public void onCapabilitiesChanged(Network network, NetworkCapabilities nc) {
                Log.d(TAG, "sub:" + mSubId + "NetworkCallback.onCapabilitiesChanged: network="
                        + network + ", Cap = " + nc);
            }
            @Override
            public void onLinkPropertiesChanged(Network network, LinkProperties lp) {
                Log.d(TAG, "sub:" + mSubId + "NetworkCallback.onLinkPropertiesChanged: network="
                        + network + ", LP = " + lp);
            }

        };
    }

    private void onMmsPdpConnected(String subId, Network network) {
        LogTag.debugD("onMmsPdpConnected subId = " + subId);

        NetworkInfo mmsNetworkInfo = mConnMgr.getNetworkInfo(network);

        // Check availability of the mobile network.
        if (mmsNetworkInfo == null) {
            Log.e(TAG, "mms type is null or mobile data is turned off, bail");
        } else {
            // This is a very specific fix to handle the case where the phone receives an
            // incoming call during the time we're trying to setup the mms connection.
            // When the call ends, restart the process of mms connectivity.
            if (ConstantsWrapper.Phone.REASON_VOICE_CALL_ENDED.equals(mmsNetworkInfo.getReason())) {
                LogTag.debugD("   reason is " + ConstantsWrapper.Phone.REASON_VOICE_CALL_ENDED +
                        ", retrying mms connectivity");
                return;
            }

            if (mmsNetworkInfo.isConnected()) {
                TransactionSettings settings = new TransactionSettings(
                        TransactionService.this, mmsNetworkInfo.getExtraInfo(),
                        Integer.parseInt(subId));
                // If this APN doesn't have an MMSC, mark everything as failed and bail.
                if (TextUtils.isEmpty(settings.getMmscUrl())) {
                    LogTag.debugD("   empty MMSC url, bail");
                    mToastHandler.sendEmptyMessage(TOAST_NO_APN);
                    mServiceHandler.markAllPendingTransactionsAsFailed();
                    endMmsConnectivity(Integer.parseInt(subId));
                    return;
                }
                mConnMgr.bindProcessToNetwork(network);
                mServiceHandler.processPendingTransaction(null, settings);
            } else {
                LogTag.debugD("   TYPE_MOBILE_MMS not connected, bail");

                // Retry mms connectivity once it's possible to connect
                if (mmsNetworkInfo.isAvailable()) {
                    if (Log.isLoggable(LogTag.TRANSACTION, Log.VERBOSE)) {
                        LogTag.debugD("   retrying mms connectivity for it's available");
                    }
                }
            }

        }
    }

    private NetworkRequest buildNetworkRequest(String subId) {
        NetworkRequest.Builder builder = new NetworkRequest.Builder();
        builder.addTransportType(NetworkCapabilities.TRANSPORT_CELLULAR);
        builder.addCapability(NetworkCapabilities.NET_CAPABILITY_MMS);
        builder.setNetworkSpecifier(subId);
        NetworkRequest networkRequest = builder.build();
        return networkRequest;
    }

    public Handler mToastHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            String str = null;
            boolean showRetryToast = getResources().getBoolean(R.bool.config_mms_retry_toast);

            if (msg.what == TOAST_MSG_QUEUED) {
                str = getString(R.string.message_queued);
            } else if (msg.what == TOAST_DOWNLOAD_LATER) {
                str = getString(R.string.download_later);
            } else if (msg.what == TOAST_NO_APN) {
                str = getString(R.string.no_apn);
            } else if (msg.what == TOAST_SEND_FAILED_RETRY && showRetryToast) {
                str = getString(R.string.send_failed_retry);
            } else if (msg.what == TOAST_DOWNLOAD_FAILED_RETRY && showRetryToast) {
                str = getString(R.string.download_failed_retry);
            } else if (msg.what == TOAST_SETUP_DATA_CALL_FAILED_FOR_SEND && showRetryToast) {
                str = getString(R.string.no_network_send_failed_retry);
            } else if (msg.what == TOAST_SETUP_DATA_CALL_FAILED_FOR_DOWNLOAD && showRetryToast) {
                str = getString(R.string.no_network_download_failed_retry);
            }

            if (str != null) {
                Toast.makeText(TransactionService.this, str,
                        Toast.LENGTH_LONG).show();
            }
        }
    };

    @Override
    public void onCreate() {
        LogTag.debugD("Creating TransactionService");
        sInstance = this;

        mTelephonyManager = (TelephonyManager)getSystemService(TELEPHONY_SERVICE);
        if (mTelephonyManager != null) {
            mPhoneCount = mTelephonyManager.getPhoneCount();
        }
        mMmsNetworkRequest = new NetworkRequest[mPhoneCount];
        mMmsNetworkCallback = new ConnectivityManager.NetworkCallback[mPhoneCount];
        mIsAvailable = new boolean[mPhoneCount];

        // Start up the thread running the service.  Note that we create a
        // separate thread because the service normally runs in the process's
        // main thread, which we don't want to block.
        HandlerThread thread = new HandlerThread("TransactionService");
        thread.start();

        mServiceLooper = thread.getLooper();
        mServiceHandler = new ServiceHandler(mServiceLooper);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent != null) {
            Message msg = mServiceHandler.obtainMessage(EVENT_NEW_INTENT);
            msg.arg1 = startId;
            msg.obj = intent;
            mServiceHandler.sendMessage(msg);
        }
        return Service.START_NOT_STICKY;
    }

    private int getSubIdFromDb(Uri uri) {
        int subId = SubscriptionManager.getDefaultSmsSubscriptionId();
        Cursor c = getApplicationContext().getContentResolver().query(uri,
                null, null, null, null);
        Log.d(TAG, "Cursor= " + DatabaseUtils.dumpCursorToString(c));
        if (c != null) {
            try {
                if (c.moveToFirst()) {
                    subId = c.getInt(c.getColumnIndex(Mms.SUBSCRIPTION_ID));
                    Log.d(TAG, "subId in db = " + subId );
                    c.close();
                    c = null;
                }
            } finally {
                if (c != null) {
                    c.close();
                }
            }
        }
        Log.d(TAG, "Destination subId = " + subId);
        return subId;
    }

    private boolean isMmsAllowed() {
        boolean flag = true;

        if (isAirplaneModeActive()) {
            LogTag.debugD("Airplane mode is ON, MMS may not be allowed.");
            flag = false;
        }

        if(isCurrentRatIwlan()) {
            Log.d(TAG, "Current RAT is IWLAN, MMS allowed");
            flag = true;
        }
        return flag;

    }

    private boolean isAirplaneModeActive() {
        return Settings.Global.getInt(getContentResolver(),
                Settings.Global.AIRPLANE_MODE_ON, 0) != 0;
    }

    private boolean isCurrentRatIwlan() {
        boolean flag = false;
        TelephonyManager telephonyManager = (TelephonyManager)getSystemService(TELEPHONY_SERVICE);
        flag = (telephonyManager != null
                && (telephonyManager.getNetworkType() ==
                TelephonyManagerWrapper.NETWORK_TYPE_IWLAN));
        return flag;
    }

    public void onNewIntent(Intent intent, int serviceId) {
        if (!MmsConfig.isSmsEnabled(getApplicationContext())) {
            LogTag.debugD("TransactionService: is not the default sms app");
            stopSelf(serviceId);
            return;
        }
        mConnMgr = (ConnectivityManager) getSystemService(Context.CONNECTIVITY_SERVICE);
        if (mConnMgr == null || !MmsConfig.isSmsEnabled(getApplicationContext())) {
            endMmsConnectivity();
            stopSelfIfIdle(serviceId);
            return;
        }
        boolean noNetwork = false;

        LogTag.debugD("onNewIntent: serviceId: " + serviceId + ": " + intent.getExtras() +
                " intent=" + intent);

        Bundle extras = intent.getExtras();
        String action = intent.getAction();

        DownloadManager downloadManager = DownloadManager.getInstance();

        if ((ACTION_ONALARM.equals(action) || ACTION_ENABLE_AUTO_RETRIEVE.equals(action) ||
                (extras == null)) || ((extras != null) && !extras.containsKey("uri")
                && !extras.containsKey(CANCEL_URI))) {

            //We hit here when either the Retrymanager triggered us or there is
            //send operation in which case uri is not set. For rest of the
            //cases(MT MMS) we hit "else" case.

            // Scan database to find all pending operations.
            Cursor cursor = PduPersister.getPduPersister(this).getPendingMessages(
                    System.currentTimeMillis());
            LogTag.debugD("Cursor= " + DatabaseUtils.dumpCursorToString(cursor));
            if (cursor != null) {
                try {
                    int count = cursor.getCount();

                    LogTag.debugD("onNewIntent: cursor.count=" + count + " action=" + action);

                    if (count == 0) {
                        LogTag.debugD("onNewIntent: no pending messages. Stopping service.");
                        RetryScheduler.setRetryAlarm(this);
                        stopSelfIfIdle(serviceId);
                        return;
                    }

                    int columnIndexOfMsgId = cursor.getColumnIndexOrThrow(PendingMessages.MSG_ID);
                    int columnIndexOfMsgType = cursor.getColumnIndexOrThrow(
                            PendingMessages.MSG_TYPE);
                    int columnIndexOfRetryIndex = cursor.getColumnIndexOrThrow(
                            PendingMessages.RETRY_INDEX);

                    int inActiveSubCount = 0;
                    while (cursor.moveToNext()) {
                        int msgType = cursor.getInt(columnIndexOfMsgType);
                        int transactionType = getTransactionType(msgType);
                        LogTag.debugD("onNewIntent: msgType=" + msgType + " transactionType=" +
                                transactionType);
                        Uri uri = ContentUris.withAppendedId(
                                Mms.CONTENT_URI,
                                cursor.getLong(columnIndexOfMsgId));
                        boolean inRetry = cursor.getInt(columnIndexOfRetryIndex) > 0;
                        if (noNetwork) {
                            // Because there is a MMS queue list including
                            // unsent and undownload MMS in database while data
                            // network unenabled. Anyway we should give user the
                            // information about the last MMS in the queue list.
                            // While there is no data network, we will give
                            // a prompt to user about the last MMS failed in
                            // database, so we need to fetch the information of
                            // last pending MMS in the queue list, just make the
                            // cursor move to the end of the queue list, and
                            // give the corresponding prompt to user.
                            cursor.moveToLast();
                            transactionType = getTransactionType(cursor
                                    .getInt(columnIndexOfMsgType));
                            onNetworkUnavailable(serviceId, transactionType, uri, inRetry);
                            return;
                        }
                        switch (transactionType) {
                            case -1:
                                break;
                            case Transaction.RETRIEVE_TRANSACTION:
                                // If it's a transiently failed transaction,
                                // we should retry it in spite of current
                                // downloading mode. If the user just turned on the auto-retrieve
                                // option, we also retry those messages that don't have any errors.
                                int failureType = cursor.getInt(
                                        cursor.getColumnIndexOrThrow(
                                                PendingMessages.ERROR_TYPE));
                                boolean autoDownload = downloadManager.isAuto();
                                LogTag.debugD("onNewIntent: failureType=" + failureType +
                                        " action=" + action + " isTransientFailure:" +
                                        isTransientFailure(failureType) + " autoDownload=" +
                                        autoDownload);
                                if (!autoDownload && !inRetry) {
                                    // If autodownload is turned off and not in retry peroid,
                                    // don't process the transaction.
                                    LogTag.debugD("onNewIntent: skipping - autodownload off");
                                    break;
                                }
                                // If retry always is enabled, need mark state to downloading.
                                if (getResources().getBoolean(R.bool.config_retry_always)) {
                                    downloadManager.markState(
                                            uri, DownloadManager.STATE_DOWNLOADING);

                                }
                                // Logic is twisty. If there's no failure or the failure
                                // is a non-permanent failure, we want to process the transaction.
                                // Otherwise, break out and skip processing this transaction.
                                if (!(failureType == MmsSms.NO_ERROR ||
                                        isTransientFailure(failureType))) {
                                    LogTag.debugD("onNewIntent: skipping - permanent error");
                                    break;
                                }
                                LogTag.debugD( "onNewIntent: falling through and processing");
                               // fall-through
                            default:

                                int subId = getSubIdFromDb(uri);

                                Log.d(TAG, "destination Sub Id = " + subId);
                                if (subId < 0) {
                                    Log.d(TAG, "Subscriptions are not yet ready.");
                                    int defSmsSubId = getDefaultSmsSubscriptionId();
                                    // We dont have enough info about subId. We dont know if the
                                    // subId as persent in DB actually would match the subId of
                                    // defaultSmsSubId. We can not also ignore this transaction
                                    // since no retry would be scheduled if we return from here. The
                                    // only option is to do best effort try on current default sms
                                    // subId, if it failed then a retry would be scheduled and while
                                    // processing that retry attempt we would be able to get correct
                                    // subId from subscription manager.
                                    Log.d(TAG, "Override with default Sms subId = " + defSmsSubId);
                                    subId = defSmsSubId;
                                }

                                if ((!isMmsAllowed())
                                        && !getResources().getBoolean(R.bool.config_retry_always)) {
                                    Log.d(TAG, "mobileData off or no mms apn or APM, Abort");
                                    if (transactionType == Transaction.RETRIEVE_TRANSACTION) {
                                        downloadManager.markState(uri,
                                                DownloadManager.STATE_TRANSIENT_FAILURE);
                                    }
                                    onNetworkUnavailable(serviceId, transactionType, uri, inRetry);
                                    break;
                                }

                                if (!SubscriptionManager.from(getApplicationContext())
                                        .isActiveSubId(subId)) {
                                    LogTag.debugD("SubId is not active:" + subId);
                                    inActiveSubCount ++;
                                    if (inActiveSubCount == count) {
                                        LogTag.debugD("No active SubId found, stop self: " + count);
                                        stopSelfIfIdle(serviceId);
                                    }
                                    break;
                                }

                                TransactionBundle args = new TransactionBundle(transactionType,
                                        uri.toString(), subId);
                                // FIXME: We use the same startId for all MMs.
                                LogTag.debugD("onNewIntent: launchTransaction uri=" + uri);
                                // FIXME: We use the same serviceId for all MMs.
                                launchTransaction(serviceId, args, false);
                                break;
                        }
                    }
                } finally {
                    cursor.close();
                }
            } else {
                LogTag.debugD("onNewIntent: no pending messages. Stopping service.");
                RetryScheduler.setRetryAlarm(this);
                stopSelfIfIdle(serviceId);
            }
        } else if ((extras != null) && extras.containsKey(CANCEL_URI)) {
            String uriStr = intent.getStringExtra(CANCEL_URI);
            Uri mCancelUri = Uri.parse(uriStr);
            for (Transaction transaction : mProcessing) {
                transaction.cancelTransaction(mCancelUri);
            }
            for (Transaction transaction : mPending) {
                transaction.cancelTransaction(mCancelUri);
            }
        } else {
            LogTag.debugD("onNewIntent: launch transaction...");

            String uriStr = intent.getStringExtra("uri");
            Uri uri = Uri.parse(uriStr);

            int subId = getSubIdFromDb(uri);
            Log.d(TAG, "destination Sub Id = " + subId);
            if (subId < 0) {
               int defSmsSubId = getDefaultSmsSubscriptionId();
                Log.d(TAG, "Override with default Sms subId = " + defSmsSubId);
                subId = defSmsSubId;
            }

            if ((!isMmsAllowed()) && !getResources().getBoolean(R.bool.config_retry_always)) {
                Log.d(TAG, "Either mobile data is off or apn not present, Abort");

                downloadManager.markState(uri, DownloadManager.STATE_TRANSIENT_FAILURE);

                boolean isRetry = getRetryIndex(uri.getLastPathSegment()) > 0;
                int type = intent.getIntExtra(TransactionBundle.TRANSACTION_TYPE,
                        Transaction.NOTIFICATION_TRANSACTION);
                onNetworkUnavailable(serviceId, type, uri, isRetry);
                return;
            }

            if (!SubscriptionManager.from(getApplicationContext()).isActiveSubId(subId)) {
                LogTag.debugD("SubId is not active:" + subId);
                stopSelfIfIdle(serviceId);
                return;
            }

            Bundle bundle = intent.getExtras();
            bundle.putInt(ConstantsWrapper.Phone.SUBSCRIPTION_KEY, subId);
            // For launching NotificationTransaction and test purpose.
            TransactionBundle args = new TransactionBundle(bundle);
            launchTransaction(serviceId, args, noNetwork);
        }
    }

    private int getDefaultSmsSubscriptionId() {
        return SubscriptionManager.getDefaultSmsSubscriptionId();
    }

    private void stopSelfIfIdle(int startId) {
        synchronized (mProcessing) {
            if (mProcessing.isEmpty() && mPending.isEmpty()) {
                LogTag.debugD("stopSelfIfIdle: STOP!");
                stopSelf(startId);
            }
        }
    }

    private static boolean isTransientFailure(int type) {
        return type >= MmsSms.NO_ERROR && type < MmsSms.ERR_TYPE_GENERIC_PERMANENT;
    }

    private int getTransactionType(int msgType) {
        switch (msgType) {
            case PduHeaders.MESSAGE_TYPE_NOTIFICATION_IND:
                return Transaction.RETRIEVE_TRANSACTION;
            case PduHeaders.MESSAGE_TYPE_READ_REC_IND:
                return Transaction.READREC_TRANSACTION;
            case PduHeaders.MESSAGE_TYPE_SEND_REQ:
                return Transaction.SEND_TRANSACTION;
            default:
                Log.w(TAG, "Unrecognized MESSAGE_TYPE: " + msgType);
                return -1;
        }
    }

    private void launchTransaction(int serviceId, TransactionBundle txnBundle, boolean noNetwork) {
        if (noNetwork) {
            Log.w(TAG, "launchTransaction: no network error!");
            onNetworkUnavailable(serviceId, txnBundle.getTransactionType(),
                    Uri.parse(txnBundle.getUri()), false);
            return;
        }
        Message msg = mServiceHandler.obtainMessage(EVENT_TRANSACTION_REQUEST);
        msg.arg1 = serviceId;
        msg.obj = txnBundle;

        LogTag.debugD("launchTransaction: sending message " + msg);
        mServiceHandler.sendMessage(msg);
    }

    private void onNetworkUnavailable(int serviceId, int transactionType,
            Uri uri, boolean inRetry) {
        LogTag.debugD("onNetworkUnavailable: sid=" + serviceId + ", type=" + transactionType);

       if (transactionType == Transaction.NOTIFICATION_TRANSACTION
               && !DownloadManager.getInstance().isAuto()) {
           // Not trigger next retry for retrieval MMS if user not initiate it.
           return;
       }

        // Need the RetryScheduler first to update the retry index and result,
        // then set the toast type accordingly.
        if (getResources().getBoolean(R.bool.config_retry_always)) {
            RetryScheduler.scheduleRetry(getApplicationContext(), uri);
            RetryScheduler.setRetryAlarm(getApplicationContext(), uri);
        }

        int toastType = TOAST_NONE;
        if (transactionType == Transaction.RETRIEVE_TRANSACTION ||
                transactionType == Transaction.NOTIFICATION_TRANSACTION) {
            if (getResources().getBoolean(R.bool.config_retry_always) && inRetry) {
                toastType = isLastRetry(uri.getLastPathSegment()) ?
                        TOAST_NONE : TOAST_SETUP_DATA_CALL_FAILED_FOR_DOWNLOAD;
            } else {
                toastType = TOAST_SETUP_DATA_CALL_FAILED_FOR_DOWNLOAD;
            }
        } else if (transactionType == Transaction.SEND_TRANSACTION) {
            if (getResources().getBoolean(R.bool.config_retry_always) && inRetry) {
                toastType = isLastRetry(uri.getLastPathSegment()) ?
                        TOAST_NONE : TOAST_SETUP_DATA_CALL_FAILED_FOR_SEND;
            } else {
                if (getResources().getBoolean(R.bool.config_manual_resend)) {
                    updateMsgErrorType(uri, MmsSms.ERR_TYPE_MMS_PROTO_TRANSIENT);
                }
                toastType = TOAST_SETUP_DATA_CALL_FAILED_FOR_SEND;
            }
        }
        if (toastType != TOAST_NONE) {
            mToastHandler.sendEmptyMessage(toastType);
        }

        stopSelfIfIdle(serviceId);
    }

    private int getRetryIndex(String msgId) {
        Uri.Builder uriBuilder = PendingMessages.CONTENT_URI.buildUpon();
        uriBuilder.appendQueryParameter("protocol", "mms");
        uriBuilder.appendQueryParameter("message", msgId);

        Cursor cursor = null;
        try {
             cursor = SqliteWrapper.query(this, getContentResolver(),
                    uriBuilder.build(), null, null, null, null);
            if (cursor != null && (cursor.getCount() == 1) && cursor.moveToFirst()) {
                int index = cursor.getInt(cursor.getColumnIndexOrThrow(
                        PendingMessages.RETRY_INDEX));
                Log.v(TAG, "getRetryIndex:" + index) ;
                return index;
            }
            return 0;
        } finally {
            if (cursor != null) {
                cursor.close();
            }
        }
    }

    private void updateMsgErrorType(Uri mmsUri, int errorType) {
        // Update the pending_msgs table with an error type for that new item.
        ContentValues values = new ContentValues(1);
        values.put(PendingMessages.ERROR_TYPE, errorType);
        long msgId = ContentUris.parseId(mmsUri);
        SqliteWrapper.update(getApplicationContext(), getApplicationContext().getContentResolver(),
                PendingMessages.CONTENT_URI,
                values, PendingMessages.MSG_ID + "=" + msgId, null);
    }

    private boolean isLastRetry(String msgId) {
        Uri.Builder uriBuilder = PendingMessages.CONTENT_URI.buildUpon();
        uriBuilder.appendQueryParameter("protocol", "mms");
        uriBuilder.appendQueryParameter("message", msgId);

        Cursor cursor = null;
        try {
             cursor = SqliteWrapper.query(this, getContentResolver(),
                    uriBuilder.build(), null, null, null, null);
            if (cursor != null && (cursor.getCount() == 1) && cursor.moveToFirst()) {
                int retryIndex = cursor.getInt(cursor.getColumnIndexOrThrow(
                        PendingMessages.RETRY_INDEX));
                DefaultRetryScheme scheme = new DefaultRetryScheme(this, retryIndex);
                Log.i(TAG,"isLastRetry retryIndex="+retryIndex+" limit="+scheme.getRetryLimit());
                if (retryIndex >= scheme.getRetryLimit()) {
                    return true;
                }
            }
            return false;
        } finally {
            if (cursor != null) {
                cursor.close();
            }
        }
    }

    @Override
    public void onDestroy() {
        LogTag.debugD("Destroying TransactionService");
        if (!mPending.isEmpty()) {
            LogTag.debugD("TransactionService exiting with transaction still pending");
        }
        if (!isMmsDRNetwork()) {
            releaseWakeLock();
            sInstance = null;
        }
        mServiceHandler.sendEmptyMessageDelayed(EVENT_QUIT, 2*getMmsDelayTime());
        endMmsConnectivity();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    /**
     * Handle status change of Transaction (The Observable).
     */
    public void update(Observable observable) {
        Transaction transaction = (Transaction) observable;
        int serviceId = transaction.getServiceId();

        LogTag.debugD("update transaction " + serviceId);

        try {
            synchronized (mProcessing) {
                mProcessing.remove(transaction);
                if (mPending.size() > 0) {
                    LogTag.debugD("update: handle next pending transaction...");

                    Transaction nextTxn = mPending.get(0);

                    Log.d(TAG, "update: currentTxn = " + transaction
                            + ", nextTxn = " + nextTxn);

                    if (nextTxn.getSubId() != transaction.getSubId()) {
                        Log.d(TAG, "update: Next pending txn is on different sub. "
                                + "Finish for current sub.");
                        endMmsConnectivity(transaction.getSubId());
                    }

                    Message msg = mServiceHandler.obtainMessage(
                            EVENT_HANDLE_NEXT_PENDING_TRANSACTION,
                            transaction.getConnectionSettings());
                    mServiceHandler.sendMessage(msg);
                }
                else if (mProcessing.isEmpty()) {
                    LogTag.debugD("update: endMmsConnectivity");
                    endMmsConnectivity(transaction.getSubId());
                } else {
                    LogTag.debugD("update: mProcessing is not empty");
                }
            }

            Intent intent = new Intent(TRANSACTION_COMPLETED_ACTION);
            TransactionState state = transaction.getState();
            int result = state.getState();
            intent.putExtra(STATE, result);

            switch (result) {
                case TransactionState.SUCCESS:
                    LogTag.debugD("Transaction complete: " + serviceId);

                    intent.putExtra(STATE_URI, state.getContentUri());

                    // Notify user in the system-wide notification area.
                    switch (transaction.getType()) {
                        case Transaction.NOTIFICATION_TRANSACTION:
                        case Transaction.RETRIEVE_TRANSACTION:
                            // We're already in a non-UI thread called from
                            // NotificationTransacation.run(), so ok to block here.
                            long threadId = MessagingNotification.getThreadId(
                                    this, state.getContentUri());
                            MessagingNotification.blockingUpdateNewMessageIndicator(this,
                                    threadId,
                                    false);
                            MessagingNotification.updateDownloadFailedNotification(this);
                            MessageUtils.updateThreadAttachTypeByThreadId(this, threadId);
                            break;
                        case Transaction.SEND_TRANSACTION:
                            RateController.getInstance().update();
                            break;
                    }
                    break;
                case TransactionState.FAILED:
                    int type = transaction.getType();
                    Uri uri = state.getContentUri();
                    boolean failSetupDataCall = Transaction.FAIL_REASON_CAN_NOT_SETUP_DATA_CALL
                            == transaction.getFailReason();
                    if (uri != null) {
                        String msgId = uri.getLastPathSegment();
                        if (!isLastRetry(msgId)) {
                            if (type == Transaction.SEND_TRANSACTION) {
                                if (failSetupDataCall) {
                                    mToastHandler.sendEmptyMessage(
                                            TOAST_SETUP_DATA_CALL_FAILED_FOR_SEND);
                                } else {
                                    mToastHandler.sendEmptyMessage(TOAST_SEND_FAILED_RETRY);
                                }
                            } else if ((type == Transaction.RETRIEVE_TRANSACTION) ||
                                    (type == Transaction.NOTIFICATION_TRANSACTION)) {
                                if (DownloadManager.getInstance().isAuto() ||
                                        getResources().getBoolean(R.bool.config_retry_always)) {
                                    if (failSetupDataCall) {
                                        mToastHandler.sendEmptyMessage(
                                                TOAST_SETUP_DATA_CALL_FAILED_FOR_DOWNLOAD);
                                    } else {
                                        mToastHandler.sendEmptyMessage(
                                                TOAST_DOWNLOAD_FAILED_RETRY);
                                    }
                                }
                            }
                        }
                    }
                case TransactionState.CANCELED:
                    LogTag.debugD("Transaction failed: " + serviceId);
                    break;
                default:
                    LogTag.debugD( "Transaction state unknown: " +
                            serviceId + " " + result);
                    break;
            }

            LogTag.debugD("update: broadcast transaction result " + result);
            // Broadcast the result of the transaction.
            sendBroadcast(intent);
        } finally {
            transaction.detach(this);
            stopSelfIfIdle(serviceId);
        }
    }

    private synchronized void createWakeLock() {
        // Create a new wake lock if we haven't made one yet.
        if (mWakeLock == null) {
            PowerManager pm = (PowerManager)getSystemService(Context.POWER_SERVICE);
            mWakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "MMS Connectivity");
            mWakeLock.setReferenceCounted(false);
        }
    }

    private void acquireWakeLock() {
        // It's okay to double-acquire this because we are not using it
        // in reference-counted mode.
        Log.v(TAG, "mms acquireWakeLock");
        mWakeLock.acquire();
    }

    private void releaseWakeLock() {
        // Don't release the wake lock if it hasn't been created and acquired.
        if (mWakeLock != null && mWakeLock.isHeld()) {
            Log.v(TAG, "mms releaseWakeLock");
            mWakeLock.release();
        }
    }

    /* MMS app initiates one transaction at a time. Next transaction is started only on the
       completion(success/failure) of previous one.
    */
    protected void beginMmsConnectivity(int subId) throws IOException {
        // Take a wake lock so we don't fall asleep before the message is downloaded.
        createWakeLock();

        int phoneId = SubscriptionManagerWrapper.getPhoneId(subId);
        Log.v(TAG, "beginMmsConnectivity for subId = " + subId +" phoneId=" + phoneId);

        if (isPhoneIdValid(phoneId) && (mMmsNetworkRequest[phoneId] == null)) {
            mMmsNetworkRequest[phoneId] = buildNetworkRequest(Integer.toString(subId));
            mMmsNetworkCallback[phoneId] = getNetworkCallback(Integer.toString(subId));

            ConnectivityManagerWrapper.requestNetwork(mConnMgr,
                        mMmsNetworkRequest[phoneId], mMmsNetworkCallback[phoneId],
                        PDP_ACTIVATION_TIMEOUT);
            Log.d(TAG, "beginMmsConnectivity:call ConnectivityManager.requestNetwork ");
            Message message = mServiceHandler.obtainMessage(
                    EVENT_MMS_PDP_ACTIVATION_TIMEOUT);
            message.obj = subId;
            mServiceHandler.sendMessageDelayed(message, PDP_ACTIVATION_TIMEOUT);
        }
        acquireWakeLock();
    }

    private void releaseNetworkRequest(int subId) {
        int phoneId = SubscriptionManagerWrapper.getPhoneId(subId);
        if (isPhoneIdValid(phoneId) && (mMmsNetworkCallback[phoneId] != null)) {
            mIsAvailable[phoneId] = false;
            Log.d(TAG, "releaseNetworkRequest phoneId=" + phoneId);

            mConnMgr.unregisterNetworkCallback(mMmsNetworkCallback[phoneId]);
            mMmsNetworkRequest[phoneId] = null;
            mMmsNetworkCallback[phoneId] = null;
        }
    }

    private boolean isPhoneIdValid(int phoneId) {
        return phoneId >= 0 && phoneId < mPhoneCount;
    }

    protected void endMmsConnectivity() {
        for (int i = 0; i < mPhoneCount; i++) {
            int[] subId = SubscriptionManagerWrapper.getSubId(i);
            if (subId != null && (subId.length > 0)) {
                endMmsConnectivity(subId[0]);
            }
        }
    }

    protected void endMmsConnectivity(int subId) {
        if (isMmsDRNetwork()) {
            sendDRNetworkMsg(subId);
        } else {
            endMmsConnectivityNoDelay(subId);
        }
    }

    protected void endMmsConnectivityNoDelay(int subId) {
        try {
            LogTag.debugD("endMmsConnectivityNoDelay for subId = " + subId);
            if (mConnMgr != null) {
                releaseNetworkRequest(subId);
            }
        } finally {
            releaseWakeLock();
        }
   }

    protected void sendDRNetworkMsg(int subId) {
        LogTag.debugD("sendDRNetworkMsg subId="+subId);
        mServiceHandler.removeMessages(EVENT_MMS_RELEASE_NETWORK_DELAY);
        Message message = mServiceHandler.obtainMessage(
                EVENT_MMS_RELEASE_NETWORK_DELAY);
        message.obj = subId;
        mServiceHandler.sendMessageDelayed(message, getMmsDelayTime());
    }

    private boolean isMmsDRNetwork() {

        if (!getApplicationContext().getResources().
                    getBoolean(R.bool.support_MMS_RELEASE_NETWORK_DELAY))
            return false;
        SharedPreferences prefs = PreferenceManager.
                    getDefaultSharedPreferences(getApplicationContext());
        return prefs.getBoolean(
                    MmsPreferenceActivity.MMS_DELAY,
                    MMS_DELAY_PREF_DEFAULT);
    }

    private int getMmsDelayTime() {

        if (!isMmsDRNetwork()) {
            return 0;
        }
        SharedPreferences prefs = PreferenceManager.
                getDefaultSharedPreferences(getApplicationContext());
        String delayTime = prefs.getString(MmsPreferenceActivity.MMS_DELAY_VALUE,
                String.valueOf(MMS_DELAY_PERIOD_DEFAULT));
        LogTag.debugD("getMmsDelayTime delayTime: " + delayTime);
        return Integer.parseInt(delayTime);

    }

    private final class ServiceHandler extends Handler {
        public ServiceHandler(Looper looper) {
            super(looper);
        }

        private String decodeMessage(Message msg) {
            if (msg.what == EVENT_QUIT) {
                return "EVENT_QUIT";
            } else if (msg.what == EVENT_TRANSACTION_REQUEST) {
                return "EVENT_TRANSACTION_REQUEST";
            } else if (msg.what == EVENT_HANDLE_NEXT_PENDING_TRANSACTION) {
                return "EVENT_HANDLE_NEXT_PENDING_TRANSACTION";
            } else if (msg.what == EVENT_NEW_INTENT) {
                return "EVENT_NEW_INTENT";
            } else if (msg.what == EVENT_MMS_PDP_ACTIVATION_TIMEOUT) {
                return "EVENT_MMS_PDP_ACTIVATION_TIMEOUT";
            } else if (msg.what == EVENT_MMS_RELEASE_NETWORK_DELAY) {
                return "EVENT_MMS_RELEASE_NETWORK_DELAY";
            }
            return "unknown message.what";
        }

        private String decodeTransactionType(int transactionType) {
            if (transactionType == Transaction.NOTIFICATION_TRANSACTION) {
                return "NOTIFICATION_TRANSACTION";
            } else if (transactionType == Transaction.RETRIEVE_TRANSACTION) {
                return "RETRIEVE_TRANSACTION";
            } else if (transactionType == Transaction.SEND_TRANSACTION) {
                return "SEND_TRANSACTION";
            } else if (transactionType == Transaction.READREC_TRANSACTION) {
                return "READREC_TRANSACTION";
            }
            return "invalid transaction type";
        }

        /**
         * Handle incoming transaction requests.
         * The incoming requests are initiated by the MMSC Server or by the
         * MMS Client itself.
         */
        @Override
        public void handleMessage(Message msg) {
            LogTag.debugD("Handling incoming message: " + msg + " = " + decodeMessage(msg));

            Transaction transaction = null;

            switch (msg.what) {
                case EVENT_NEW_INTENT:
                    onNewIntent((Intent)msg.obj, msg.arg1);
                    break;

                case EVENT_QUIT:
                    getLooper().quit();
                    if (isMmsDRNetwork()) {
                        sInstance = null;
                    }
                    return;

                case EVENT_TRANSACTION_REQUEST:
                    int serviceId = msg.arg1;
                    try {
                        TransactionBundle args = (TransactionBundle) msg.obj;
                        TransactionSettings transactionSettings;

                        LogTag.debugD("EVENT_TRANSACTION_REQUEST MmscUrl=" +
                                args.getMmscUrl() + " proxy port: " + args.getProxyAddress());

                        // Set the connection settings for this transaction.
                        // If these have not been set in args, load the default settings.
                        String mmsc = args.getMmscUrl();
                        if (mmsc != null) {
                            transactionSettings = new TransactionSettings(
                                    mmsc, args.getProxyAddress(), args.getProxyPort());
                        } else {
                            transactionSettings = new TransactionSettings(
                                                    TransactionService.this, null,
                                                    args.getSubId());
                        }

                        int transactionType = args.getTransactionType();

                        LogTag.debugD("handle EVENT_TRANSACTION_REQUEST: transactionType=" +
                                transactionType + " " + decodeTransactionType(transactionType));

                        // Create appropriate transaction
                        switch (transactionType) {
                            case Transaction.NOTIFICATION_TRANSACTION:
                                String uri = args.getUri();
                                if (uri != null) {
                                    transaction = new NotificationTransaction(
                                            TransactionService.this, serviceId,
                                            transactionSettings, uri);
                                } else {
                                    // Now it's only used for test purpose.
                                    byte[] pushData = args.getPushData();
                                    PduParser parser = new PduParser(pushData,
                                            PduParserUtil.shouldParseContentDisposition());
                                    GenericPdu ind = parser.parse();

                                    int type = PduHeaders.MESSAGE_TYPE_NOTIFICATION_IND;
                                    if ((ind != null) && (ind.getMessageType() == type)) {
                                        transaction = new NotificationTransaction(
                                                TransactionService.this, serviceId,
                                                transactionSettings, (NotificationInd) ind);
                                    } else {
                                        Log.e(TAG, "Invalid PUSH data.");
                                        transaction = null;
                                        return;
                                    }
                                }
                                break;
                            case Transaction.RETRIEVE_TRANSACTION:
                                transaction = new RetrieveTransaction(
                                        TransactionService.this, serviceId,
                                        transactionSettings, args.getUri());
                                break;
                            case Transaction.SEND_TRANSACTION:
                                transaction = new SendTransaction(
                                        TransactionService.this, serviceId,
                                        transactionSettings, args.getUri());
                                break;
                            case Transaction.READREC_TRANSACTION:
                                transaction = new ReadRecTransaction(
                                        TransactionService.this, serviceId,
                                        transactionSettings, args.getUri());
                                break;
                            default:
                                Log.w(TAG, "Invalid transaction type: " + serviceId);
                                transaction = null;
                                return;
                        }
                        //copy the subId from TransactionBundle to transaction obj.
                        transaction.setSubId(args.getSubId());

                        if (!processTransaction(transaction)) {
                            transaction = null;
                            return;
                        }

                        LogTag.debugD("Started processing of incoming message: " + msg);
                    } catch (Exception ex) {
                        Log.w(TAG, "Exception occurred while handling message: " + msg, ex);

                        if (transaction != null) {
                            try {
                                transaction.detach(TransactionService.this);
                                if (mProcessing.contains(transaction)) {
                                    synchronized (mProcessing) {
                                        mProcessing.remove(transaction);
                                    }
                                }
                            } catch (Throwable t) {
                                Log.e(TAG, "Unexpected Throwable.", t);
                            } finally {
                                // Set transaction to null to allow stopping the
                                // transaction service.
                                transaction = null;
                            }
                        }
                    } finally {
                        if (transaction == null) {
                            LogTag.debugD("Transaction was null. Stopping self: " + serviceId);
                            endMmsConnectivity();
                            stopSelfIfIdle(serviceId);
                        }
                    }
                    return;
                case EVENT_HANDLE_NEXT_PENDING_TRANSACTION:
                    processPendingTransaction(transaction, (TransactionSettings) msg.obj);
                    return;
                case EVENT_MMS_CONNECTIVITY_TIMEOUT:
                    removeMessages(EVENT_MMS_CONNECTIVITY_TIMEOUT);
                    mMmsConnecvivityRetryCount++;
                    if (mMmsConnecvivityRetryCount > MMS_CONNECTIVITY_RETRY_TIMES) {
                        Log.d(TAG, "MMS_CONNECTIVITY_TIMEOUT");
                        mMmsConnecvivityRetryCount = 0;
                        return;
                    }

                    if (!mPending.isEmpty()) {
                        try {
                            beginMmsConnectivity(SubscriptionManager.getDefaultDataSubscriptionId());
                        } catch (IOException e) {
                            Log.w(TAG, "Attempt to use of MMS connectivity failed");
                            return;
                        }
                    }
                    return;
                case EVENT_MMS_PDP_ACTIVATION_TIMEOUT:
                    onPDPTimeout((int)msg.obj);
                    return;

                case EVENT_MMS_RELEASE_NETWORK_DELAY:
                    if (mPending.size() <= 0 && mProcessing.isEmpty()){
                        endMmsConnectivityNoDelay((int)msg.obj);
                    }
                    return;
                default:
                    Log.w(TAG, "what=" + msg.what);
                    return;
            }
        }

        private void onPDPTimeout(int subId) {
            LogTag.debugD("PDP activation timer expired, declare failure sub: "+ subId);
            synchronized (mProcessing) {
                ArrayList<Transaction> tranList = new ArrayList<Transaction>();
                if (!mProcessing.isEmpty()) {
                    // Get the process transaction
                    for (Transaction processTransaction : mProcessing) {
                        if (processTransaction.getSubId() == subId) {
                            tranList.add(processTransaction);
                        }
                    }
                }

                if (!mPending.isEmpty()) {
                    // Get the pending transaction and delete it.
                    Iterator<Transaction> it = mPending.iterator();
                    while (it.hasNext()) {
                        Transaction pendingTransaction = it.next();
                        if (pendingTransaction.getSubId() == subId) {
                            tranList.add(pendingTransaction);
                            it.remove();
                            LogTag.debugD("onPDPTimeout Remove req sub" + subId);
                        }
                    }
                }
                endMmsConnectivity(subId);
                for (Transaction transaction : tranList) {
                    transaction.attach(TransactionService.this);
                    transaction.abort();
                }
            }
        }

        public void markAllPendingTransactionsAsFailed() {
            LogTag.debugD("markAllPendingTransactionAsFailed");
            synchronized (mProcessing) {
                while (mPending.size() != 0) {
                    Transaction transaction = mPending.remove(0);
                    if (transaction instanceof SendTransaction) {
                        Uri uri = ((SendTransaction)transaction).mSendReqURI;
                        transaction.mTransactionState.setContentUri(uri);
                        int respStatus = PduHeaders.RESPONSE_STATUS_ERROR_NETWORK_PROBLEM;
                        ContentValues values = new ContentValues(1);
                        values.put(Mms.RESPONSE_STATUS, respStatus);

                        SqliteWrapper.update(TransactionService.this,
                                TransactionService.this.getContentResolver(),
                                uri, values, null, null);
                    }
                    transaction.notifyObservers();
               }
            }
        }

        public void processPendingTransaction(Transaction transaction,
                                               TransactionSettings settings) {

            LogTag.debugD("processPendingTxn: transaction=" + transaction);

            int numProcessTransaction = 0;
            synchronized (mProcessing) {
                if (mPending.size() != 0) {
                    boolean isPendingFound = false;
                    for (int index = 0; index < mPending.size(); index++) {
                        Transaction trans = mPending.get(index);
                        int phoneId = SubscriptionManager.getPhoneId(trans.getSubId());
                        if (isPhoneIdValid(phoneId) && mIsAvailable[phoneId]) {
                            transaction = mPending.remove(index);
                            LogTag.debugD("processPendingTxn: handle mPending[" + index + "]"
                                    + " sub" + transaction.getSubId());
                            isPendingFound = true;
                            break;
                        }
                    }
                    if (!isPendingFound) {
                        transaction = mPending.remove(0);
                        LogTag.debugD("processPendingTxn: no suitable item, try item0 sub"
                                + transaction.getSubId());
                    }
                }
                numProcessTransaction = mProcessing.size();
            }

            if (transaction != null) {
                if (settings != null) {
                    transaction.setConnectionSettings(settings);
                }

                /*
                 * Process deferred transaction
                 */
                try {
                    int serviceId = transaction.getServiceId();

                    LogTag.debugD("processPendingTxn: process " + serviceId);

                    if (processTransaction(transaction)) {
                        LogTag.debugD("Started deferred processing of transaction  "
                                + transaction);
                    } else {
                        transaction = null;
                        stopSelfIfIdle(serviceId);
                    }
                } catch (IOException e) {
                    Log.w(TAG, e.getMessage(), e);
                }
            } else {
                if (numProcessTransaction == 0) {
                    LogTag.debugD("processPendingTxn: no more transaction, endMmsConnectivity");
                    endMmsConnectivity();
                }
            }
        }

        /**
         * Internal method to begin processing a transaction.
         * @param transaction the transaction. Must not be {@code null}.
         * @return {@code true} if process has begun or will begin. {@code false}
         * if the transaction should be discarded.
         * @throws IOException if connectivity for MMS traffic could not be
         * established.
         */
        private boolean processTransaction(Transaction transaction) throws IOException {
            // Check if transaction already processing
            synchronized (mProcessing) {
                for (Transaction t : mPending) {
                    if (t.isEquivalent(transaction)) {
                        LogTag.debugD("Transaction already pending: " +
                                transaction.getServiceId());
                        return true;
                    }
                }
                for (Transaction t : mProcessing) {
                    if (t.isEquivalent(transaction)) {
                        LogTag.debugD("Duplicated transaction: " + transaction.getServiceId());
                        return true;
                    }
                }

                int subId = transaction.getSubId();
                int phoneId = SubscriptionManagerWrapper.getPhoneId(subId);
                boolean isRequestNetworkQueued = true;
                LogTag.debugD("processTransaction :subId="+subId
                    + "; phoneId = " + phoneId
                    + "; mPhoneCount = "+ mPhoneCount);
                for (int id =0; id < mPhoneCount; id++) {
                    if ((id != phoneId) && (mMmsNetworkRequest[id] != null)) {
                        isRequestNetworkQueued = false;
                        break;
                    }
                }
                LogTag.debugD("processTransaction :isRequestNetworkQueued="+isRequestNetworkQueued);
                // If there is already a transaction in processing list, because of the previous
                // beginMmsConnectivity call and there is another transaction just at a time,
                // when the pdp is connected, there will be a case of adding the new transaction
                // to the Processing list. But Processing list is never traversed to
                // resend, resulting in transaction not completed/sent.
                if ((mProcessing.size() > 0) || !isRequestNetworkQueued) {
                    LogTag.debugD("Adding transaction to 'mPending' list: " + transaction);
                    mPending.add(transaction);
                    return true;
                } else {
                    /*
                    * Make sure that the network connectivity necessary
                    * for MMS traffic is enabled. If it is not, we need
                    * to defer processing the transaction until
                    * connectivity is established.
                    */
                    beginMmsConnectivity(subId);

                if (isPhoneIdValid(phoneId) && !mIsAvailable[phoneId]) {
                    mPending.add(transaction);
                    LogTag.debugD("processTransaction: connResult=APN_REQUEST_STARTED, " +
                            "defer transaction pending MMS connectivity");
                    if (transaction instanceof SendTransaction) {
                        LogTag.debugD("remove cache while deferring");
                        MmsApp.getApplication().getPduLoaderManager().removePdu(
                                ((SendTransaction) transaction).mSendReqURI);
                    }
                        return true;
                    }

                    LogTag.debugD("Adding transaction to 'mProcessing' list: " + transaction);
                    mProcessing.add(transaction);
                }
            }

            LogTag.debugD("processTransaction: starting transaction " + transaction);

            // Attach to transaction and process it
            transaction.attach(TransactionService.this);
            transaction.process();
            return true;
        }
    }
}
