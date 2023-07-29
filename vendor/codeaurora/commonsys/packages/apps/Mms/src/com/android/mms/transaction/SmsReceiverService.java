/*
 * Copyright (C) 2010-2014, 2017, 2019 The Linux Foundation. All rights reserved.
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

import static android.content.Intent.ACTION_BOOT_COMPLETED;
import static android.provider.Telephony.Sms.Intents.SMS_DELIVER_ACTION;

import java.util.Calendar;
import java.util.GregorianCalendar;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import android.app.Activity;
import android.app.Service;
import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.database.Cursor;
import android.database.sqlite.SqliteWrapper;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.Process;
import android.preference.PreferenceManager;
import android.provider.Telephony.Sms;
import android.provider.Telephony.Sms.Inbox;
import android.provider.Telephony.Sms.Intents;
import android.provider.Telephony.Sms.Outbox;
import android.telephony.CellBroadcastMessage;
import android.telephony.ServiceState;
import android.telephony.SmsCbMessage;
import android.telephony.SmsManager;
import android.telephony.SmsMessage;
import android.telephony.SubscriptionInfo;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;
import android.text.TextUtils;
import android.util.Log;
import android.widget.Toast;

import com.android.mms.LogTag;
import com.android.mms.MmsConfig;
import com.android.mms.R;
import com.android.mms.data.Contact;
import com.android.mms.data.Conversation;
import com.android.mms.ui.ClassZeroActivity;
import com.android.mms.ui.MessageUtils;
import com.android.mms.util.Recycler;
import com.android.mms.util.SendingProgressTokenManager;
import com.android.mms.widget.MmsWidgetProvider;
import com.android.mmswrapper.ConstantsWrapper;
import com.android.mmswrapper.ServiceStateWrapper;
import com.android.mmswrapper.SmsManagerWrapper;
import com.android.mmswrapper.SmsMessageWrapper;
import com.android.mmswrapper.SubscriptionManagerWrapper;
import com.android.mmswrapper.TelephonyManagerWrapper;
import com.android.mmswrapper.TelephonyWrapper;
import com.google.android.mms.MmsException;
import java.util.Iterator;
/**
 * This service essentially plays the role of a "worker thread", allowing us to store
 * incoming messages to the database, update notifications, etc. without blocking the
 * main thread that SmsReceiver runs on.
 */
public class SmsReceiverService extends Service {
    private static final String TAG = LogTag.TAG;
    private final static String SMS_PRIORITY = "priority";

    private ServiceHandler mServiceHandler;
    private Looper mServiceLooper;
    private static Map<Integer, Boolean> mSending = new HashMap<Integer, Boolean>();

    public static final String MESSAGE_SENT_ACTION =
        "com.android.mms.transaction.MESSAGE_SENT";

    // Indicates next message can be picked up and sent out.
    public static final String EXTRA_MESSAGE_SENT_SEND_NEXT ="SendNextMsg";

    public static final String ACTION_SEND_MESSAGE =
            "com.android.mms.transaction.SEND_MESSAGE";
    public static final String ACTION_SEND_INACTIVE_MESSAGE =
            "com.android.mms.transaction.SEND_INACTIVE_MESSAGE";

    // This must match the column IDs below.
    private static final String[] SEND_PROJECTION = new String[] {
        Sms._ID,        //0
        Sms.THREAD_ID,  //1
        Sms.ADDRESS,    //2
        Sms.BODY,       //3
        Sms.STATUS,     //4
        Sms.SUBSCRIPTION_ID,   //5
        SMS_PRIORITY   //6
    };

    private static final String[] SEND_PROJECTION_TYPE = new String[] {
        Sms._ID, // 0
        Sms.THREAD_ID, // 1
        Sms.ADDRESS, // 2
        Sms.BODY, // 3
        Sms.STATUS, // 4
        Sms.SUBSCRIPTION_ID, // 5
        Sms.TYPE // 6
    };

    static final String CB_AREA_INFO_RECEIVED_ACTION =
            "android.cellbroadcastreceiver.CB_AREA_INFO_RECEIVED";

    /* Cell Broadcast for channel 50 */
    static final int CB_CHANNEL_50 = 50;

    /* Cell Broadcast for channel 60 */
    static final int CB_CHANNEL_60 = 60;

    public Handler mToastHandler = new Handler();

    // This must match SEND_PROJECTION.
    private static final int SEND_COLUMN_ID         = 0;
    private static final int SEND_COLUMN_THREAD_ID  = 1;
    private static final int SEND_COLUMN_ADDRESS    = 2;
    private static final int SEND_COLUMN_BODY       = 3;
    private static final int SEND_COLUMN_STATUS     = 4;
    private static final int SEND_COLUMN_SUB_ID   = 5;
    private static final int SEND_COLUMN_PRIORITY   = 6;
    private static final int SEND_COLUMN_TYPE   = 6;

    private static boolean sIsSavingMessage = false;


    @Override
    public void onCreate() {
        // Temporarily removed for this duplicate message track down.
//        if (Log.isLoggable(LogTag.TRANSACTION, Log.VERBOSE) || LogTag.DEBUG_SEND) {
//            Log.v(TAG, "onCreate");
//        }

        // Start up the thread running the service.  Note that we create a
        // separate thread because the service normally runs in the process's
        // main thread, which we don't want to block.
        HandlerThread thread = new HandlerThread(TAG, Process.THREAD_PRIORITY_BACKGROUND);
        thread.start();

        mServiceLooper = thread.getLooper();
        mServiceHandler = new ServiceHandler(mServiceLooper);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (!MmsConfig.isSmsEnabled(this)) {
            LogTag.debugD("SmsReceiverService: is not the default sms app");
            // NOTE: We MUST not call stopSelf() directly, since we need to
            // make sure the wake lock acquired by AlertReceiver is released.
            SmsReceiver.finishStartingService(SmsReceiverService.this, startId);
            return Service.START_NOT_STICKY;
        }
        // Temporarily removed for this duplicate message track down.

        int resultCode = intent != null ? intent.getIntExtra("result", 0) : 0;

        if (resultCode != 0) {
            LogTag.debugD("onStart: #" + startId + " resultCode: " + resultCode +
                    " = " + translateResultCode(resultCode));
        }

        Message msg = mServiceHandler.obtainMessage();
        msg.arg1 = startId;
        msg.obj = intent;
        mServiceHandler.sendMessage(msg);
        return Service.START_NOT_STICKY;
    }

    private static String translateResultCode(int resultCode) {
        switch (resultCode) {
            case Activity.RESULT_OK:
                return "Activity.RESULT_OK";
            case SmsManager.RESULT_ERROR_GENERIC_FAILURE:
                return "SmsManager.RESULT_ERROR_GENERIC_FAILURE";
            case SmsManager.RESULT_ERROR_RADIO_OFF:
                return "SmsManager.RESULT_ERROR_RADIO_OFF";
            case SmsManager.RESULT_ERROR_NULL_PDU:
                return "SmsManager.RESULT_ERROR_NULL_PDU";
            case SmsManager.RESULT_ERROR_NO_SERVICE:
                return "SmsManager.RESULT_ERROR_NO_SERVICE";
            case SmsManagerWrapper.RESULT_ERROR_LIMIT_EXCEEDED:
                return "SmsManager.RESULT_ERROR_LIMIT_EXCEEDED";
            case SmsManagerWrapper.RESULT_ERROR_FDN_CHECK_FAILURE:
                return "SmsManager.RESULT_ERROR_FDN_CHECK_FAILURE";
            default:
                return "Unknown error code";
        }
    }

    @Override
    public void onDestroy() {
        // Temporarily removed for this duplicate message track down.
//        if (Log.isLoggable(LogTag.TRANSACTION, Log.VERBOSE) || LogTag.DEBUG_SEND) {
//            Log.v(TAG, "onDestroy");
//        }
        mServiceLooper.quit();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    private final class ServiceHandler extends Handler {
        public ServiceHandler(Looper looper) {
            super(looper);
        }

        /**
         * Handle incoming transaction requests.
         * The incoming requests are initiated by the MMSC Server or by the MMS Client itself.
         */
        @Override
        public void handleMessage(Message msg) {
            int serviceId = msg.arg1;
            Intent intent = (Intent)msg.obj;
            LogTag.debugD("handleMessage serviceId: " + serviceId + " intent: " + intent);
            if (intent != null && MmsConfig.isSmsEnabled(getApplicationContext())) {
                String action = intent.getAction();

                int error = intent.getIntExtra("errorCode", 0);

                LogTag.debugD("handleMessage action: " + action + " error: " + error);

                if (MESSAGE_SENT_ACTION.equals(intent.getAction())) {
                    handleSmsSent(intent, error);
                } else if (SMS_DELIVER_ACTION.equals(action)) {
                    handleSmsReceived(intent, error);
                } else if (CB_AREA_INFO_RECEIVED_ACTION.equals(action)) {
                    handleCbSmsReceived(intent, error);
                } else if (ACTION_BOOT_COMPLETED.equals(action)) {
                    handleBootCompleted();
                } else if (ConstantsWrapper.TelephonyIntent.ACTION_SERVICE_STATE_CHANGED.
                        equals(action)) {
                    handleServiceStateChanged(intent);
                } else if (ACTION_SEND_MESSAGE.endsWith(action)) {
                    handleSendMessage(intent);
                } else if (ACTION_SEND_INACTIVE_MESSAGE.equals(action)) {
                    handleSendInactiveMessage();
                }
            }
            // NOTE: We MUST not call stopSelf() directly, since we need to
            // make sure the wake lock acquired by AlertReceiver is released.
            SmsReceiver.finishStartingService(SmsReceiverService.this, serviceId);
        }
    }

    private void handleServiceStateChanged(Intent intent) {
        // If service just returned, start sending out the queued messages
        ServiceState serviceState = ServiceStateWrapper.newFromBundle(intent.getExtras());
        int subId = intent.getIntExtra(ConstantsWrapper.Phone.SUBSCRIPTION_KEY, -1);
        // if service state is IN_SERVICE & current subscription is same as
        // preferred SMS subscription.i.e.as set under SIM Settings, then
        // sendFirstQueuedMessage.
        if (serviceState.getState() == ServiceState.STATE_IN_SERVICE) {
            sendFirstQueuedMessage(subId);
        }
    }

    private void handleSendMessage(Intent intent) {
        int subId = intent.getIntExtra(ConstantsWrapper.Phone.SUBSCRIPTION_KEY,
                SubscriptionManager.getDefaultSmsSubscriptionId());
        printMap(mSending,"handleSendMessage : put queue SMS to sending");
        if (mSending.get(subId) == null) {
           mSending.put(subId, false);
        }
        printMap(mSending,"handleSendMessage : finish puting queue SMS to sending");
        if (!mSending.get(subId)) {
            sendFirstQueuedMessage(subId);
        } else if (!isSmsSending(subId)) {
            mSending.put(subId, false);
            sendFirstQueuedMessage(subId);
        } else {
            printSendingSMS(subId);
            LogTag.debugD("subId=" + subId + " is in mSending ");
        }
    }

    private boolean isSmsSending(int subId) {
        Log.d("Mms", "Check SMS message sending status subId:"+subId);
        final Uri uri = Uri.parse("content://sms/outbox");
        ContentResolver resolver = getContentResolver();
        String where = "sub_id=?";
        String[] whereArgs = new String[] { Integer.toString(subId) };
        Cursor c = SqliteWrapper.query(this, resolver, uri,
            SEND_PROJECTION_TYPE, where, whereArgs, "date ASC");
        try {
            if ((c != null) && (c.getCount() > 0)) {
                Log.d("Mms", " exist sending msg:");
                dumpSendingMsg(c);
                return true;
            }
        } finally {
            if (c != null) {
                c.close();
            }
        }
        Log.d("Mms", "Database does not exist sending message");
        return false;
    }

    private static void printMap(Map<Integer, Boolean> map, String functionName) {
        try {
            LogTag.debugD(functionName);
            Iterator iterator = map.entrySet().iterator();
            StringBuilder sb = new StringBuilder();
            while(iterator.hasNext()) {
                Map.Entry entry = (Map.Entry) iterator.next();
                boolean value =  (boolean) entry.getValue();
                int key = (int) entry.getKey();
                sb.append("value:"+value+";key:"+key+";");
            }
            LogTag.debugD("printSendingMap key and value" + sb.toString());
        } catch (Exception e) {
            LogTag.debugD("printSendingMap error" + e);
        }
    }

    private void handleSendInactiveMessage() {
        // Inactive messages includes all messages in outbox and queued box.
        moveOutboxMessagesToQueuedBox();
        // Process queued messages on all SUB's
        List<SubscriptionInfo> subInfoList =
                SubscriptionManager.from(getApplicationContext()).getActiveSubscriptionInfoList();
        if(subInfoList != null) {
            for (SubscriptionInfo info : subInfoList)
            {
                int subId = info.getSubscriptionId();
                if (mSending.get(subId) == null) {
                    printMap(mSending,"handleSendInactiveMessage :"
                            + "Remove fail sending SMS to sending");
                    mSending.put(subId, false);
                    printMap(mSending,"handleSendInactiveMessage :"
                            + "Finish removing fail sending SMS to sending");
                }
                if (!mSending.get(subId)) {
                    sendFirstQueuedMessage(subId);
                }
            }
        }
    }

    private void dumpSendingMsg(Cursor c) {
        if ((c != null) && (c.getCount() > 0)) {
            while (c.moveToNext()) {
                String msgText = c.getString(SEND_COLUMN_BODY);
                String address = c.getString(SEND_COLUMN_ADDRESS);
                int threadId = c.getInt(SEND_COLUMN_THREAD_ID);
                int status = c.getInt(SEND_COLUMN_STATUS);
                int msgId = c.getInt(SEND_COLUMN_ID);
                int subId = c.getInt(SEND_COLUMN_SUB_ID);
                int type = c.getInt(SEND_COLUMN_TYPE);
                Log.d("Mms", " sending message infor: Text: " + msgText
                    + " address: " + address
                    + " threadId: " + threadId
                    + " status: " + status
                    + " msgId: " + msgId
                    + " subId: " + subId
                    + " type: " + type);
           }
        }

    }

    private void printSendingSMS(int subId) {
        Log.d("Mms", "Check SMS message sending status subId:"+subId);
        final Uri uri = Uri.parse("content://sms/outbox");
        ContentResolver resolver = getContentResolver();
        String where = "sub_id=?";
        String[] whereArgs = new String[] { Integer.toString(subId) };
        Cursor c = SqliteWrapper.query(this, resolver, uri,
            SEND_PROJECTION_TYPE, where, whereArgs, "date ASC");
        try {
            if ((c != null) && (c.getCount() > 0)) {
                Log.d("Mms", " exist sending msg:");
                dumpSendingMsg(c);
            } else {
                Log.d("Mms", "Database does not exist sending message");
            }
        } catch (Exception e) {
            Log.d("Mms", " print sending SMS error:" + e);
        } finally {
            if (c != null) {
                c.close();
            }
        }
    }

    // Send first queued message of the given subscription
    public synchronized void sendFirstQueuedMessage(int subscription) {
        boolean success = true;
        boolean isExpectMore = false;
        // get all the queued messages from the database
        final Uri uri = Uri.parse("content://sms/queued");
        ContentResolver resolver = getContentResolver();
        String where = "sub_id=?";
        String[] whereArgs = new String[] {Integer.toString(subscription)};
        Cursor c = SqliteWrapper.query(this, resolver, uri,
                        SEND_PROJECTION, where, whereArgs, "date ASC"); // date ASC so we send out
                                                                       // in same order the user
                                                                       // tried to send messages.
        if (c != null) {
            try {
                if (c.moveToFirst()) {
                    String msgText = c.getString(SEND_COLUMN_BODY);
                    String address = c.getString(SEND_COLUMN_ADDRESS);
                    int threadId = c.getInt(SEND_COLUMN_THREAD_ID);
                    int status = c.getInt(SEND_COLUMN_STATUS);

                    int msgId = c.getInt(SEND_COLUMN_ID);
                    int subId = c.getInt(SEND_COLUMN_SUB_ID);
                    int priority = c.getInt(SEND_COLUMN_PRIORITY);
                    Uri msgUri = ContentUris.withAppendedId(Sms.CONTENT_URI, msgId);
                    // Get the information of is there any messages are pending to process.
                    // If yes, send this inforamtion to framework to control the link and send all
                    // messages on same link based on the support in framework
                    if (c.moveToNext()) {
                        isExpectMore = true;
                    }
                    SmsMessageSender sender = new SmsSingleRecipientSender(this,
                            address, msgText, threadId, status == Sms.STATUS_PENDING,
                            msgUri, subId, isExpectMore);
                    MessageUtils.markAsNotificationThreadIfNeed(this, threadId, address);

                    if(priority != -1){
                        ((SmsSingleRecipientSender)sender).setPriority(priority);
                    }

                    LogTag.debugD("sendFirstQueuedMessage " + msgUri +
                            ", address: " + address +
                            ", threadId: " + threadId);

                    try {
                        sender.sendMessage(SendingProgressTokenManager.NO_TOKEN);
                        printMap(mSending, "sendFirstQueuedMessage :"
                                + "Put first queue SMS to sending");
                        mSending.put(subscription, true);
                        printMap(mSending, "sendFirstQueuedMessage :"
                                + "Finish puting first queue SMS to sending");
                    } catch (MmsException e) {
                        Log.e(TAG, "sendFirstQueuedMessage: failed to send message " + msgUri
                                + ", caught ", e);
                        printMap(mSending, "sendFirstQueuedMessage:"
                                + "Sending SMS error, Finish sending");
                        mSending.put(subscription, false);
                        printMap(mSending, "sendFirstQueuedMessage:"
                                + "Sending SMS error, Finish sending");
                        messageFailedToSend(msgUri, SmsManager.RESULT_ERROR_GENERIC_FAILURE);
                        success = false;
                        // Sending current message fails. Try to send more pending messages
                        // if there is any.
                        Intent intent = new Intent(SmsReceiverService.ACTION_SEND_MESSAGE, null,
                                this,
                                SmsReceiver.class);
                        intent.putExtra(ConstantsWrapper.Phone.SUBSCRIPTION_KEY, subscription);
                        sendBroadcast(intent);
                    }
                }
            } finally {
                c.close();
            }
        }
        if (success) {
            // We successfully sent all the messages in the queue. We don't need to
            // be notified of any service changes any longer.
            // In case of MSIM don't unregister service state change if there are any messages
            // pending for process on other subscriptions. There may be a chance of other
            // subscription is register and waiting for sevice state changes to process the message.
            TelephonyManager tm = (TelephonyManager)this.
                    getSystemService(Context.TELEPHONY_SERVICE);
            if (!(tm.getPhoneCount() > 1) ||
                    isUnRegisterAllowed(subscription)) {
                unRegisterForServiceStateChanges();
            }
        }
    }

    private void handleSmsSent(Intent intent, int error) {
        Uri uri = intent.getData();
        int resultCode = intent.getIntExtra("result", 0);
        boolean sendNextMsg = intent.getBooleanExtra(EXTRA_MESSAGE_SENT_SEND_NEXT, false);
        int subId = intent.getIntExtra(ConstantsWrapper.Phone.SUBSCRIPTION_KEY,
                SubscriptionManager.getDefaultSmsSubscriptionId());
        printMap(mSending, "handleSmsSent:"
                + "Sending SMS has callback, change sending status");
        mSending.put(subId, false);
        printMap(mSending, "handleSmsSent: "
                + "Sending SMS has callback, finish changing sending status");
        LogTag.debugD("handleSmsSent uri: " + uri + " sendNextMsg: " + sendNextMsg +
                " resultCode: " + resultCode +
                " = " + translateResultCode(resultCode) + " error: " + error + ";subId:"+subId);

        if (resultCode == Activity.RESULT_OK) {
            if (sendNextMsg) {
                Log.v(TAG, "handleSmsSent: move message to sent folder uri: " + uri);
                if (!TelephonyWrapper.Sms.moveMessageToFolder(this, uri, Sms.MESSAGE_TYPE_SENT, error)) {
                    Log.e(TAG, "handleSmsSent: failed to move message " + uri + " to sent folder");
                }
                sendFirstQueuedMessage(subId);

            }

            // Update the notification for failed messages since they may be deleted.
            MessagingNotification.nonBlockingUpdateSendFailedNotification(this);
        } else if ((resultCode == SmsManager.RESULT_ERROR_RADIO_OFF) ||
                (resultCode == SmsManager.RESULT_ERROR_NO_SERVICE)) {
            LogTag.debugD("handleSmsSent: no service, queuing message w/ uri: " + uri);
            // We got an error with no service or no radio. Register for state changes so
            // when the status of the connection/radio changes, we can try to send the
            // queued up messages.
            registerForServiceStateChanges();
            // We couldn't send the message, put in the queue to retry later.
            TelephonyWrapper.Sms.moveMessageToFolder(this, uri, Sms.MESSAGE_TYPE_QUEUED, error);
            mToastHandler.post(new Runnable() {
                public void run() {
                    Toast.makeText(SmsReceiverService.this, getString(R.string.message_queued),
                            Toast.LENGTH_SHORT).show();
                }
            });
        } else if (resultCode == SmsManagerWrapper.RESULT_ERROR_FDN_CHECK_FAILURE) {
            messageFailedToSend(uri, resultCode);
            mToastHandler.post(new Runnable() {
                public void run() {
                    Toast.makeText(SmsReceiverService.this, getString(R.string.fdn_check_failure),
                            Toast.LENGTH_SHORT).show();
                }
            });
            if (sendNextMsg) {
                sendFirstQueuedMessage(subId);
            }
        } else {
            messageFailedToSend(uri, error);
            if (sendNextMsg) {
                sendFirstQueuedMessage(subId);
            }
        }
    }

    private void messageFailedToSend(Uri uri, int error) {
        LogTag.debugD("messageFailedToSend msg failed uri: " + uri + " error: " + error);
        TelephonyWrapper.Sms.moveMessageToFolder(this, uri, Sms.MESSAGE_TYPE_FAILED, error);
        MessagingNotification.notifySendFailed(getApplicationContext(), true);
    }

    private void handleSmsReceived(Intent intent, int error) {
        SmsMessage[] msgs = Intents.getMessagesFromIntent(intent);
        String format = intent.getStringExtra("format");

        // Because all sub id have been changed to phone id in Mms,
        // so also change it here.
        SmsMessage sms4log = msgs[0];
        LogTag.debugD("handleSmsReceived" + (sms4log.isReplace() ? "(replace)" : "") +
                    ", address: " + sms4log.getOriginatingAddress() +
                    ", body: " + sms4log.getMessageBody());
        int saveLoc = MessageUtils.getSmsPreferStoreLocation(this,
                SubscriptionManagerWrapper.getPhoneId(SmsMessageWrapper.getSubId(msgs[0])));
        if (getResources().getBoolean(R.bool.config_savelocation)
                && saveLoc == MessageUtils.PREFER_SMS_STORE_CARD) {
            LogTag.debugD("PREFER SMS STORE CARD");
            for (int i = 0; i < msgs.length; i++) {
                SmsMessage sms = msgs[i];
                boolean saveSuccess = saveMessageToIcc(sms);
                if (saveSuccess) {
                    int subId = MessageUtils.isMultiSimEnabledMms()
                            ? SmsMessageWrapper.getSubId(sms) : (int)MessageUtils.SUB_INVALID;
                    int phoneId = SubscriptionManagerWrapper.getPhoneId(subId);
                    String address = MessageUtils.convertIdp(this,
                            sms.getDisplayOriginatingAddress(), phoneId);
                    MessagingNotification.blockingUpdateNewIccMessageIndicator(
                            this, address, sms.getDisplayMessageBody(),
                            subId, sms.getTimestampMillis());
                    getContentResolver().notifyChange(MessageUtils.getIccUriBySubscription(
                            phoneId), null);
                } else {
                    LogTag.debugD("SaveMessageToIcc fail");
                    mToastHandler.post(new Runnable() {
                        public void run() {
                            Toast.makeText(getApplicationContext(),
                                    getString(R.string.pref_sim_card_full_save_to_phone),
                                    Toast.LENGTH_LONG).show();
                        }
                    });
                    // save message to phone if failed save to icc.
                    saveMessageToPhone(msgs, error, format);
                    break;
                }
            }
        } else {
            saveMessageToPhone(msgs, error, format);
        }
    }

    private void saveMessageToPhone(SmsMessage[] msgs, int error, String format){
        setSavingMessage(true);
        Uri messageUri = insertMessage(this, msgs, error, format);

        MessageUtils.checkIsPhoneMessageFull(this);

        if (messageUri != null) {
            long threadId = MessagingNotification.getSmsThreadId(this, messageUri);
            // Called off of the UI thread so ok to block.
            LogTag.debugD("saveMessageToPhone messageUri: " + messageUri + " threadId: " + threadId);
            MessagingNotification.blockingUpdateNewMessageIndicator(this, threadId, false);
            MessageUtils.updateThreadAttachTypeByThreadId(this, threadId);
        } else {
            LogTag.debugD("saveMessageToPhone messageUri is null !");
        }
        setSavingMessage(false);
    }

    private void handleCbSmsReceived(Intent intent, int error) {
        Bundle extras = intent.getExtras();
        if (extras == null) {
            return;
        }
        CellBroadcastMessage cbMessage = (CellBroadcastMessage) extras.get("message");
        if (cbMessage == null) {
            return;
        }
        boolean isMSim = MessageUtils.isMultiSimEnabledMms();
        TelephonyManager tm = (TelephonyManager)this.
                getSystemService(Context.TELEPHONY_SERVICE);
        String country = "";
        if (isMSim) {
            country = TelephonyManagerWrapper.getSimCountryIso(tm, cbMessage.getSubId());
        } else {
            country = tm.getSimCountryIso();
        }
        int serviceCategory = cbMessage.getServiceCategory();
        if ("in".equals(country) && (serviceCategory == CB_CHANNEL_50 ||
                serviceCategory == CB_CHANNEL_60)) {
            Uri cbMessageUri = storeCbMessage(this, cbMessage, error);
            if (cbMessageUri != null) {
                long threadId = MessagingNotification.getSmsThreadId(this, cbMessageUri);
                // Called off of the UI thread so ok to block.
                MessagingNotification.blockingUpdateNewMessageIndicator(this, threadId, false);
            }
        }

    }

    private void handleBootCompleted() {
        // Some messages may get stuck in the outbox. At this point, they're probably irrelevant
        // to the user, so mark them as failed and notify the user, who can then decide whether to
        // resend them manually.
        moveOutboxMessagesToFailedBox();
        int numUnreadFailed = getUnreadFailedMessageCount();
        if (numUnreadFailed > 0) {
            MessagingNotification.notifySendFailed(getApplicationContext(), true);
        }
        // Send any queued messages that were waiting from before the reboot.
        // // Process queued messages on all SUB's
        List<SubscriptionInfo> subInfoList =
                SubscriptionManager.from(getApplicationContext()).getActiveSubscriptionInfoList();
        if(subInfoList != null) {
            for (SubscriptionInfo info : subInfoList)
            {
                int subId = info.getSubscriptionId();
                if (mSending.get(subId) == null) {
                    printMap(mSending, "handleBootCompleted:"
                            + "After reboot, reset all subId status");
                    mSending.put(subId, false);
                    printMap(mSending, "handleBootCompleted:"
                            + "After reboot, finish reseting all subId status");
                }
                if (mSending.get(subId) != null) {
                    if (!mSending.get(subId)) {
                        sendFirstQueuedMessage(subId);
                    }
                }
            }
        }
        // Called off of the UI thread so ok to block.
        MessagingNotification.blockingUpdateNewMessageIndicator(
                this, MessagingNotification.THREAD_ALL, false);
    }

    private int getUnreadFailedMessageCount() {
        final Uri uri = Uri.parse("content://sms/failed");
        Cursor cursor = SqliteWrapper.query(
                getApplicationContext(), getContentResolver(), uri,
                new String[] { Sms._ID }, "read=0", null, null);
        int count = 0;
        if (cursor != null) {
            count = cursor.getCount();
            cursor.close();
        }

        return count;
    }

    /**
     * Move all messages that are in the outbox to the queued state
     * @return The number of messages that were actually moved
     */
    private int moveOutboxMessagesToQueuedBox() {
        ContentValues values = new ContentValues(1);

        values.put(Sms.TYPE, Sms.MESSAGE_TYPE_QUEUED);

        int messageCount = SqliteWrapper.update(
                getApplicationContext(), getContentResolver(), Outbox.CONTENT_URI,
                values, "type = " + Sms.MESSAGE_TYPE_OUTBOX, null);
        if (Log.isLoggable(LogTag.TRANSACTION, Log.VERBOSE) || LogTag.DEBUG_SEND) {
            Log.v(TAG, "moveOutboxMessagesToQueuedBox messageCount: " + messageCount);
        }
        return messageCount;
    }

    /**
     * Move all messages that are in the outbox to the failed state and set them to unread.
     * @return The number of messages that were actually moved
     */
    private int moveOutboxMessagesToFailedBox() {
        ContentValues values = new ContentValues(3);

        values.put(Sms.TYPE, Sms.MESSAGE_TYPE_FAILED);
        values.put(Sms.ERROR_CODE, SmsManager.RESULT_ERROR_GENERIC_FAILURE);
        values.put(Sms.READ, Integer.valueOf(0));

        int messageCount = SqliteWrapper.update(
                getApplicationContext(), getContentResolver(), Outbox.CONTENT_URI,
                values, "type = " + Sms.MESSAGE_TYPE_OUTBOX, null);
        if (Log.isLoggable(LogTag.TRANSACTION, Log.VERBOSE) || LogTag.DEBUG_SEND) {
            Log.v(TAG, "moveOutboxMessagesToFailedBox messageCount: " + messageCount);
        }
        return messageCount;
    }

    public static final String CLASS_ZERO_BODY_KEY = "CLASS_ZERO_BODY";

    // This must match the column IDs below.
    private final static String[] REPLACE_PROJECTION = new String[] {
        Sms._ID,
        Sms.ADDRESS,
        Sms.PROTOCOL
    };

    // This must match REPLACE_PROJECTION.
    private static final int REPLACE_COLUMN_ID = 0;

    /**
     * If the message is a class-zero message, display it immediately
     * and return null.  Otherwise, store it using the
     * <code>ContentResolver</code> and return the
     * <code>Uri</code> of the thread containing this message
     * so that we can use it for notification.
     */
    private Uri insertMessage(Context context, SmsMessage[] msgs, int error, String format) {
        // Build the helper classes to parse the messages.
        SmsMessage sms = msgs[0];

        if (sms.getMessageClass() == SmsMessage.MessageClass.CLASS_0) {
            displayClassZeroMessage(context, sms, format);
            return null;
        } else if (sms.isReplace()) {
            return replaceMessage(context, msgs, error);
        } else {
            return storeMessage(context, msgs, error);
        }
    }

    /**
     * This method is used if this is a "replace short message" SMS.
     * We find any existing message that matches the incoming
     * message's originating address and protocol identifier.  If
     * there is one, we replace its fields with those of the new
     * message.  Otherwise, we store the new message as usual.
     *
     * See TS 23.040 9.2.3.9.
     */
    private Uri replaceMessage(Context context, SmsMessage[] msgs, int error) {
        SmsMessage sms = msgs[0];
        ContentValues values = extractContentValues(sms);
        values.put(Sms.ERROR_CODE, error);
        int pduCount = msgs.length;

        if (pduCount == 1) {
            // There is only one part, so grab the body directly.
            values.put(Inbox.BODY, replaceFormFeeds(sms.getDisplayMessageBody()));
        } else {
            // Build up the body from the parts.
            StringBuilder body = new StringBuilder();
            for (int i = 0; i < pduCount; i++) {
                sms = msgs[i];
                try {
                    body.append(sms.getDisplayMessageBody());
                } catch (NullPointerException e) {
                    Log.e(TAG, "replaceMessage " + e, e);
                }
            }
            values.put(Inbox.BODY, replaceFormFeeds(body.toString()));
        }

        int subId = SmsMessageWrapper.getSubId(sms);
        ContentResolver resolver = context.getContentResolver();
        String originatingAddress = MessageUtils.convertIdp(this,
                sms.getOriginatingAddress(), subId);
        int protocolIdentifier = sms.getProtocolIdentifier();
        String selection;
        String[] selectionArgs;

        if (Log.isLoggable(LogTag.TRANSACTION, Log.VERBOSE)) {
            Log.v(TAG, " SmsReceiverService: replaceMessage:");
        }
        selection = Sms.ADDRESS + " = ? AND " +
                    Sms.PROTOCOL + " = ? AND " +
                    Sms.SUBSCRIPTION_ID +  " = ? ";
        selectionArgs = new String[] {
                originatingAddress, Integer.toString(protocolIdentifier),
                Integer.toString(subId)
            };

        Cursor cursor = SqliteWrapper.query(context, resolver, Inbox.CONTENT_URI,
                            REPLACE_PROJECTION, selection, selectionArgs, null);

        if (cursor != null) {
            try {
                if (cursor.moveToFirst()) {
                    long messageId = cursor.getLong(REPLACE_COLUMN_ID);
                    Uri messageUri = ContentUris.withAppendedId(
                            Sms.CONTENT_URI, messageId);

                    SqliteWrapper.update(context, resolver, messageUri,
                                        values, null, null);
                    return messageUri;
                }
            } finally {
                cursor.close();
            }
        }
        return storeMessage(context, msgs, error);
    }

    public static String replaceFormFeeds(String s) {
        // Some providers send formfeeds in their messages. Convert those formfeeds to newlines.
        return s == null ? "" : s.replace('\f', '\n');
    }

//    private static int count = 0;

    private Uri storeMessage(Context context, SmsMessage[] msgs, int error) {
        // Check to see whether short message count is up to 2000 for cmcc
        if (MessageUtils.checkIsPhoneMessageFull(this)) {
            return null;
        }

        SmsMessage sms = msgs[0];

        // Store the message in the content provider.
        ContentValues values = extractContentValues(sms);
        values.put(Sms.ERROR_CODE, error);
        values.put(Sms.SUBSCRIPTION_ID, SmsMessageWrapper.getSubId(sms));

        int pduCount = msgs.length;

        if (pduCount == 1) {
            // There is only one part, so grab the body directly.
            values.put(Inbox.BODY, replaceFormFeeds(sms.getDisplayMessageBody()));
        } else {
            // Build up the body from the parts.
            StringBuilder body = new StringBuilder();
            for (int i = 0; i < pduCount; i++) {
                sms = msgs[i];
                try {
                    body.append(sms.getDisplayMessageBody());
                } catch (NullPointerException e) {
                    Log.e(TAG, "storeMessage " + e, e);
                }
            }
            values.put(Inbox.BODY, replaceFormFeeds(body.toString()));
        }

        // Make sure we've got a thread id so after the insert we'll be able to delete
        // excess messages.
        Long threadId = values.getAsLong(Sms.THREAD_ID);
        String address = values.getAsString(Sms.ADDRESS);

        // Code for debugging and easy injection of short codes, non email addresses, etc.
        // See Contact.isAlphaNumber() for further comments and results.
//        switch (count++ % 8) {
//            case 0: address = "AB12"; break;
//            case 1: address = "12"; break;
//            case 2: address = "Jello123"; break;
//            case 3: address = "T-Mobile"; break;
//            case 4: address = "Mobile1"; break;
//            case 5: address = "Dogs77"; break;
//            case 6: address = "****1"; break;
//            case 7: address = "#4#5#6#"; break;
//        }

        if (!TextUtils.isEmpty(address)) {
            Contact cacheContact = Contact.get(address,true);
            if (cacheContact != null) {
                address = cacheContact.getNumber();
            }
        } else if (TextUtils.isEmpty(address)
                && getResources().getBoolean(R.bool.def_hide_unknown_sender)) {
            values.put(Sms.ADDRESS, "");
        } else {
            address = getString(R.string.unknown_sender);
            values.put(Sms.ADDRESS, address);
        }

        if (((threadId == null) || (threadId == 0)) && (address != null)) {
            threadId = Conversation.getOrCreateThreadId(context, address);
            values.put(Sms.THREAD_ID, threadId);
        }

        ContentResolver resolver = context.getContentResolver();

        Uri insertedUri = SqliteWrapper.insert(context, resolver, Inbox.CONTENT_URI, values);
        MessageUtils.markAsNotificationThreadIfNeed(context, threadId, address);

        // Now make sure we're not over the limit in stored messages
        Recycler.getSmsRecycler().deleteOldMessagesByThreadId(context, threadId);
        MmsWidgetProvider.notifyDatasetChanged(context);

        return insertedUri;
    }

    private Uri storeCbMessage(Context context, CellBroadcastMessage sms, int error) {
        // Store the broadcast message in the content provider.
        ContentValues values = new ContentValues();
        values.put(Sms.ERROR_CODE, error);
        values.put(Sms.SUBSCRIPTION_ID, sms.getSubId());

        // CB messages are concatenated by telephony framework into a single
        // message in intent, so grab the body directly.
        values.put(Inbox.BODY, sms.getMessageBody());

        // Make sure we've got a thread id so after the insert we'll be able to
        // delete excess messages.
        String address = getString(R.string.cell_broadcast_sender)
                + Integer.toString(sms.getServiceCategory());
        values.put(Sms.ADDRESS, address);
        Long threadId = Conversation.getOrCreateThreadId(context, address);
        values.put(Sms.THREAD_ID, threadId);
        values.put(Inbox.READ, 0);
        values.put(Inbox.SEEN, 0);

        ContentResolver resolver = context.getContentResolver();
        Uri insertedUri = SqliteWrapper.insert(context, resolver, Inbox.CONTENT_URI, values);

        // Now make sure we're not over the limit in stored messages
        Recycler.getSmsRecycler().deleteOldMessagesByThreadId(context, threadId);
        MmsWidgetProvider.notifyDatasetChanged(context);

        return insertedUri;
    }

    /**
     * Extract all the content values except the body from an SMS
     * message.
     */
    private ContentValues extractContentValues(SmsMessage sms) {
        // Store the message in the content provider.
        ContentValues values = new ContentValues();

        values.put(Inbox.ADDRESS, MessageUtils.convertIdp(this, sms.getDisplayOriginatingAddress(),
                SmsMessageWrapper.getSubId(sms)));

        // Use now for the timestamp to avoid confusion with clock
        // drift between the handset and the SMSC.
        // Check to make sure the system is giving us a non-bogus time.
        Calendar buildDate = new GregorianCalendar(2011, 8, 18);    // 18 Sep 2011
        Calendar nowDate = new GregorianCalendar();
        long now = System.currentTimeMillis();
        nowDate.setTimeInMillis(now);

        if (nowDate.before(buildDate)) {
            // It looks like our system clock isn't set yet because the current time right now
            // is before an arbitrary time we made this build. Instead of inserting a bogus
            // receive time in this case, use the timestamp of when the message was sent.
            now = sms.getTimestampMillis();
        }

        values.put(Inbox.DATE, new Long(now));
        values.put(Inbox.DATE_SENT, Long.valueOf(sms.getTimestampMillis()));
        values.put(Inbox.PROTOCOL, sms.getProtocolIdentifier());
        values.put(Inbox.READ, 0);
        values.put(Inbox.SEEN, 0);
        if (sms.getPseudoSubject().length() > 0) {
            values.put(Inbox.SUBJECT, sms.getPseudoSubject());
        }
        values.put(Inbox.REPLY_PATH_PRESENT, sms.isReplyPathPresent() ? 1 : 0);
        values.put(Inbox.SERVICE_CENTER, sms.getServiceCenterAddress());
        return values;
    }

    /**
     * Displays a class-zero message immediately in a pop-up window
     * with the number from where it received the Notification with
     * the body of the message
     *
     */
    private void displayClassZeroMessage(Context context, SmsMessage sms, String format) {
        int subId = SmsMessageWrapper.getSubId(sms);

        // Using NEW_TASK here is necessary because we're calling
        // startActivity from outside an activity.
        Intent smsDialogIntent = new Intent(context, ClassZeroActivity.class)
                .putExtra("pdu", sms.getPdu())
                .putExtra("format", format)
                .putExtra(ConstantsWrapper.Phone.SUBSCRIPTION_KEY, subId)
                .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK
                          | Intent.FLAG_ACTIVITY_MULTIPLE_TASK);

        context.startActivity(smsDialogIntent);
    }

    private void registerForServiceStateChanges() {
        Context context = getApplicationContext();
        unRegisterForServiceStateChanges();

        IntentFilter intentFilter = new IntentFilter();
        intentFilter.addAction(ConstantsWrapper.TelephonyIntent.ACTION_SERVICE_STATE_CHANGED);
        if (Log.isLoggable(LogTag.TRANSACTION, Log.VERBOSE) || LogTag.DEBUG_SEND) {
            Log.v(TAG, "registerForServiceStateChanges");
        }

        context.registerReceiver(SmsReceiver.getInstance(), intentFilter);
    }

    private void unRegisterForServiceStateChanges() {
        if (Log.isLoggable(LogTag.TRANSACTION, Log.VERBOSE) || LogTag.DEBUG_SEND) {
            Log.v(TAG, "unRegisterForServiceStateChanges");
        }
        try {
            Context context = getApplicationContext();
            context.unregisterReceiver(SmsReceiver.getInstance());
        } catch (IllegalArgumentException e) {
            // Allow un-matched register-unregister calls
        }
    }

    /**
     * Return true if there are no queued messages on other subscriptions.
     */
    private boolean isUnRegisterAllowed(int subId) {
        boolean allowed = true;
        List<SubscriptionInfo> subInfoList =
                SubscriptionManager.from(getApplicationContext()).getActiveSubscriptionInfoList();
        if(subInfoList != null) {
            final Uri uri = Uri.parse("content://sms/queued");
            ContentResolver resolver = getContentResolver();
            String where = "sub_id=?";
            for (SubscriptionInfo info : subInfoList) {
                int subInfoSubId = info.getSubscriptionId();
                if (subInfoSubId != subId) {
                    String[] whereArgs = new String[] {Integer.toString(subInfoSubId)};
                    Cursor c = SqliteWrapper.query(this, resolver, uri,
                            SEND_PROJECTION, where, whereArgs, "date ASC");
                    if (c != null) {
                        try {
                            if (c.moveToFirst()) {
                                Log.d(TAG, "Find queued SMS on other sub, do not unregister.");
                                allowed = false;
                            }
                        } finally {
                            c.close();
                        }
                    }
                    if(!allowed) {
                        return allowed;
                    }
                }
            }
        }
        return allowed;
    }

    private boolean saveMessageToIcc(SmsMessage sms) {
        int subscription = SmsMessageWrapper.getSubId(sms);
        String address = MessageUtils.convertIdp(this, sms.getOriginatingAddress(),
            subscription);
        return  MessageUtils.insertMessageIntoIcc(subscription, address,
            sms.getMessageBody(), Sms.MESSAGE_TYPE_INBOX, sms.getTimestampMillis());
    }

    private static void setSavingMessage(boolean isSaving) {
        sIsSavingMessage = isSaving;
    }

    public static boolean getSavingMessage() {
        return sIsSavingMessage;
    }
}
