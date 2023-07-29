/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Not a Contribution.
 * Copyright (C) 2008 Esmertec AG.
 * Copyright (C) 2008 The Android Open Source Project
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

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.database.sqlite.SQLiteException;
import android.database.sqlite.SqliteWrapper;
import android.net.Uri;
import android.preference.PreferenceManager;
import android.provider.Telephony.Sms;
import android.provider.Telephony.Sms.Inbox;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;
import android.util.Log;

import com.android.mms.LogTag;
import com.android.mms.R;
import com.android.mms.ui.MessageUtils;
import com.android.mms.ui.MessagingPreferenceActivity;
import com.android.mms.ui.MessagingReportsPreferenceActivity;
import com.android.mms.ui.SmsPreferenceActivity;
import com.android.mmswrapper.ConstantsWrapper;
import com.android.mmswrapper.SubscriptionManagerWrapper;
import com.android.mmswrapper.TelephonyWrapper;
import com.google.android.mms.MmsException;
import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.ContentValues;


public class SmsMessageSender implements MessageSender {
    protected final Context mContext;
    protected final int mNumberOfDests;
    private final String[] mDests;
    protected final String mMessageText;
    protected final String mServiceCenter;
    protected final long mThreadId;
    protected long mTimestamp;
    private static final String TAG = LogTag.TAG;
    protected int mSubId;

    // Default preference values
    private static final boolean DEFAULT_DELIVERY_REPORT_MODE  = false;

    private static final String[] SERVICE_CENTER_PROJECTION = new String[] {
        Sms.Conversations.REPLY_PATH_PRESENT,
        Sms.Conversations.SERVICE_CENTER,
    };

    private static final int COLUMN_REPLY_PATH_PRESENT = 0;
    private static final int COLUMN_SERVICE_CENTER     = 1;

    public SmsMessageSender(Context context, String[] dests,
                 String msgText, long threadId, int subId) {
        mContext = context;
        mMessageText = msgText;
        if (dests != null) {
            mNumberOfDests = dests.length;
            mDests = new String[mNumberOfDests];
            System.arraycopy(dests, 0, mDests, 0, mNumberOfDests);
        } else {
            mNumberOfDests = 0;
            mDests = null;
        }
        mTimestamp = System.currentTimeMillis();
        mThreadId = threadId;
        mServiceCenter = getOutgoingServiceCenter(mThreadId);
        mSubId = subId;
    }

    public boolean sendMessage(long token) throws MmsException {
        // In order to send the message one by one, instead of sending now, the message will split,
        // and be put into the queue along with each destinations
        return queueMessage(token);
    }

    private boolean queueMessage(long token) throws MmsException {
        if ((mMessageText == null) || (mNumberOfDests == 0)) {
            // Don't try to send an empty message.
            if (!(mMessageText == null &&
                    mContext.getResources().getBoolean(R.bool.enable_send_blank_message))) {
                throw new MmsException("Null message body or dest.");
            }
        }

        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(mContext);
        boolean requestDeliveryReport = false;
        if (MessageUtils.isMultiSimEnabledMms()) {
            int slotId = SubscriptionManagerWrapper.getSlotId(mSubId);
            if (MessageUtils.isMsimIccCardActive()) {
                requestDeliveryReport = prefs.getBoolean((slotId == ConstantsWrapper.Phone.SUB1) ?
                        MessagingReportsPreferenceActivity.SMS_DELIVERY_REPORT_SUB1 :
                        MessagingReportsPreferenceActivity.SMS_DELIVERY_REPORT_SUB2,
                        DEFAULT_DELIVERY_REPORT_MODE);
            } else {
                requestDeliveryReport = prefs.getBoolean((slotId == ConstantsWrapper.Phone.SUB1) ?
                        SmsPreferenceActivity.SMS_DELIVERY_REPORT_SIM1 :
                        SmsPreferenceActivity.SMS_DELIVERY_REPORT_SIM2,
                        DEFAULT_DELIVERY_REPORT_MODE);
            }
        } else {
            requestDeliveryReport = prefs.getBoolean(
                    SmsPreferenceActivity.SMS_DELIVERY_REPORT_NO_MULTI,
                    DEFAULT_DELIVERY_REPORT_MODE);
        }

        int priority = -1;
        try {
            String priorityStr = PreferenceManager.getDefaultSharedPreferences(mContext).getString(
                    "pref_key_sms_cdma_priority", "");
            priority = Integer.parseInt(priorityStr);
        } catch (Exception e) {
            Log.w(TAG, "get priority error:" + e);
        }

        for (int i = 0; i < mNumberOfDests; i++) {
            try {
                LogTag.debugD("queueMessage mDests[i]: " + mDests[i] + " mThreadId: " + mThreadId);
                // Check to see whether short message count is up to 2000 for cmcc
                if (MessageUtils.checkIsPhoneMessageFull(mContext)) {
                    break;
                }
                log("updating Database with subId = " + mSubId);
                addMessageToUri(mSubId, mContext.getContentResolver(),
                        Uri.parse("content://sms/queued"), mDests[i],
                        mMessageText, null, mTimestamp,
                        true /* read */,
                        requestDeliveryReport,
                        mThreadId, priority);
                MessageUtils.updateThreadAttachTypeByThreadId(mContext, mThreadId);
            } catch (SQLiteException e) {
                if (LogTag.DEBUG_SEND) {
                    Log.e(TAG, "queueMessage SQLiteException", e);
                }
                SqliteWrapper.checkSQLiteException(mContext, e);
            }
        }
        Intent intent = new Intent(SmsReceiverService.ACTION_SEND_MESSAGE, null, mContext,
                SmsReceiver.class);
        intent.putExtra(ConstantsWrapper.Phone.SUBSCRIPTION_KEY, mSubId);
        // Notify the SmsReceiverService to send the message out
        mContext.sendBroadcast(intent);
        return false;
    }

    private Uri addMessageToUri(int subId, ContentResolver resolver,
        Uri uri, String address, String body, String subject,
        Long date, boolean read, boolean deliveryReport,
        long threadId, int priority) {
        ContentValues values = new ContentValues(8);
        values.put(Sms.SUBSCRIPTION_ID, subId);
        values.put(Sms.ADDRESS, address);
        if (date != null) {
            values.put(Sms.DATE, date);
        }
        values.put(Sms.READ, read ? Integer.valueOf(1) : Integer.valueOf(0));
        values.put(Sms.SUBJECT, subject);
        values.put(Sms.BODY, body);
        values.put(Sms.PRIORITY, priority);
        if (deliveryReport) {
            values.put(Sms.STATUS, Sms.STATUS_PENDING);
        }
        if (threadId != -1L) {
            values.put(Sms.THREAD_ID, threadId);
        }
        return resolver.insert(uri, values);
    }

    /**
     * Get the service center to use for a reply.
     *
     * The rule from TS 23.040 D.6 is that we send reply messages to
     * the service center of the message to which we're replying, but
     * only if we haven't already replied to that message and only if
     * <code>TP-Reply-Path</code> was set in that message.
     *
     * Therefore, return the service center from the most recent
     * message in the conversation, but only if it is a message from
     * the other party, and only if <code>TP-Reply-Path</code> is set.
     * Otherwise, return null.
     */
    private String getOutgoingServiceCenter(long threadId) {
        Cursor cursor = null;

        try {
            cursor = SqliteWrapper.query(mContext, mContext.getContentResolver(),
                            Inbox.CONTENT_URI, SERVICE_CENTER_PROJECTION,
                            "thread_id = " + threadId, null, "date DESC");

            if ((cursor == null) || !cursor.moveToFirst()) {
                return null;
            }

            boolean replyPathPresent = (1 == cursor.getInt(COLUMN_REPLY_PATH_PRESENT));
            return replyPathPresent ? cursor.getString(COLUMN_SERVICE_CENTER) : null;
        } finally {
            if (cursor != null) {
                cursor.close();
            }
        }
    }

    private void log(String msg) {
        Log.d(LogTag.TAG, "[SmsMsgSender] " + msg);
    }
}
