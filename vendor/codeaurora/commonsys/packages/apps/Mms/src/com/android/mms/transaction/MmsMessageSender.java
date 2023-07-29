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

import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.net.Uri;
import android.preference.PreferenceManager;
import android.provider.Telephony;
import android.provider.Telephony.Mms;
import android.provider.Telephony.MmsSms;
import android.provider.Telephony.MmsSms.PendingMessages;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;
import android.util.Log;

import com.android.mms.data.Conversation;
import com.android.mms.LogTag;
import com.android.mms.ui.ComposeMessageActivity;
import com.android.mms.ui.MessagingPreferenceActivity;
import com.android.mms.ui.MessagingReportsPreferenceActivity;
import com.android.mms.ui.MessagingReportsPreferenceActivity;
import com.android.mms.ui.MmsPreferenceActivity;
import com.android.mms.ui.MessagingExpiryPreferenceActivity;
import com.android.mms.ui.MessageUtils;
import com.android.mms.util.SendingProgressTokenManager;
import com.android.mmswrapper.ConstantsWrapper;
import com.android.mmswrapper.SubscriptionManagerWrapper;
import com.google.android.mms.InvalidHeaderValueException;
import com.google.android.mms.MmsException;
import com.google.android.mms.pdu.EncodedStringValue;
import com.google.android.mms.pdu.GenericPdu;
import com.google.android.mms.pdu.PduHeaders;
import com.google.android.mms.pdu.PduPersister;
import com.google.android.mms.pdu.ReadRecInd;
import com.google.android.mms.pdu.SendReq;
import com.google.android.mms.util.SqliteWrapper;
import android.text.TextUtils;

public class MmsMessageSender implements MessageSender {
    private static final String TAG = LogTag.TAG;

    private final Context mContext;
    private final Uri mMessageUri;
    private final long mMessageSize;
    private int mSubId;

    // Default preference values
    private static final boolean DEFAULT_DELIVERY_REPORT_MODE  = false;
    private static final boolean DEFAULT_READ_REPORT_MODE      = false;
    private static final long    DEFAULT_EXPIRY_TIME     = 7 * 24 * 60 * 60;
    private static final int     DEFAULT_PRIORITY        = PduHeaders.PRIORITY_NORMAL;
    private static final String  DEFAULT_MESSAGE_CLASS   = PduHeaders.MESSAGE_CLASS_PERSONAL_STR;

    public MmsMessageSender(Context context, Uri location, long messageSize, int subId) {
        mContext = context;
        mMessageUri = location;
        mMessageSize = messageSize;
        mSubId = subId;
        if (mMessageUri == null) {
            throw new IllegalArgumentException("Null message URI.");
        }
    }

    public boolean sendMessage(long token) throws MmsException {
        // Load the MMS from the message uri
        if (Log.isLoggable(LogTag.APP, Log.VERBOSE)) {
            LogTag.debug("sendMessage uri: " + mMessageUri);
        }
        PduPersister p = PduPersister.getPduPersister(mContext);
        GenericPdu pdu = p.load(mMessageUri);

        if (pdu.getMessageType() != PduHeaders.MESSAGE_TYPE_SEND_REQ) {
            throw new MmsException("Invalid message: " + pdu.getMessageType());
        }

        SendReq sendReq = (SendReq) pdu;

        // Update headers.
        updatePreferencesHeaders(sendReq);

        // MessageClass.
        sendReq.setMessageClass(DEFAULT_MESSAGE_CLASS.getBytes());

        // Update the 'date' field of the message before sending it.
        sendReq.setDate(System.currentTimeMillis() / 1000L);

        sendReq.setMessageSize(mMessageSize);

        p.updateHeaders(mMessageUri, sendReq);

        long messageId = ContentUris.parseId(mMessageUri);

        // Move the message into MMS Outbox.
        if (!mMessageUri.toString().startsWith(Mms.Draft.CONTENT_URI.toString())) {
            // If the message is already in the outbox (most likely because we created a "primed"
            // message in the outbox when the user hit send), then we have to manually put an
            // entry in the pending_msgs table which is where TransacationService looks for
            // messages to send. Normally, the entry in pending_msgs is created by the trigger:
            // insert_mms_pending_on_update, when a message is moved from drafts to the outbox.
            ContentValues values = new ContentValues(7);

            values.put(PendingMessages.PROTO_TYPE, MmsSms.MMS_PROTO);
            values.put(PendingMessages.MSG_ID, messageId);
            values.put(PendingMessages.MSG_TYPE, pdu.getMessageType());
            values.put(PendingMessages.ERROR_TYPE, 0);
            values.put(PendingMessages.ERROR_CODE, 0);
            values.put(PendingMessages.RETRY_INDEX, 0);
            values.put(PendingMessages.DUE_TIME, 0);

            SqliteWrapper.insert(mContext, mContext.getContentResolver(),
                    PendingMessages.CONTENT_URI, values);
        } else {
            p.move(mMessageUri, Mms.Outbox.CONTENT_URI);
        }

        // Start MMS transaction service
        SendingProgressTokenManager.put(messageId, token);
        Intent intent = new Intent(mContext, TransactionService.class);
        intent.putExtra(Mms.SUBSCRIPTION_ID, mSubId);
        mContext.startService(intent);

        return true;
    }

    // Update the headers which are stored in SharedPreferences.
    private void updatePreferencesHeaders(SendReq sendReq) throws MmsException {
        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(mContext);

        // Expiry.
        long expiryTime = getExpiryTime(prefs);

        String expiryStr = prefs.getString(MessagingPreferenceActivity.EXPIRY_TIME, "0");
        Log.v(TAG, "updatePreferencesHeaders expiryTime = " + expiryTime + ", expiryStr = "
                + expiryStr);
        if (0 != expiryTime) {
            // Add for sub , if don't set , net will set to Maxinum to default
            sendReq.setExpiry(expiryTime);
        }

        // Priority.
        sendReq.setPriority(prefs.getInt(MessagingPreferenceActivity.PRIORITY, DEFAULT_PRIORITY));

        // Delivery report.
        int slotId = SubscriptionManagerWrapper.getSlotId(mSubId);
        boolean dr = false;
        if (MessageUtils.isMultiSimEnabledMms()) {
            dr = prefs.getBoolean((slotId == ConstantsWrapper.Phone.SUB1) ?
                    MmsPreferenceActivity.MMS_DELIVERY_REPORT_SUB1 :
                    MmsPreferenceActivity.MMS_DELIVERY_REPORT_SUB2,
                    DEFAULT_DELIVERY_REPORT_MODE);
        } else {
            dr = prefs.getBoolean(MmsPreferenceActivity.MMS_DELIVERY_REPORT_SS,
                    DEFAULT_DELIVERY_REPORT_MODE);
        }
        sendReq.setDeliveryReport(dr ? PduHeaders.VALUE_YES : PduHeaders.VALUE_NO);
        // Read report.
        boolean rr = false;
        if (MessageUtils.isMultiSimEnabledMms()) {
            rr = prefs.getBoolean((slotId == ConstantsWrapper.Phone.SUB1) ?
                    MmsPreferenceActivity.MMS_READ_REPORT_SUB1 :
                    MmsPreferenceActivity.MMS_READ_REPORT_SUB2,
                    DEFAULT_READ_REPORT_MODE);
        } else {
            rr = prefs.getBoolean(MmsPreferenceActivity.MMS_READ_REPORT_SS,
                    DEFAULT_READ_REPORT_MODE);
        }
        sendReq.setReadReport(rr ? PduHeaders.VALUE_YES : PduHeaders.VALUE_NO);
    }

    private long getExpiryTime(SharedPreferences prefs) {
        long expiryTime = 0;
        if (MessageUtils.isMsimIccCardActive()) {
            expiryTime = Long.parseLong(
                    prefs.getString((mSubId == ConstantsWrapper.Phone.SUB1) ?
                            MessagingExpiryPreferenceActivity.MMS_VALIDITY_FOR_SIM1 :
                            MessagingExpiryPreferenceActivity.MMS_VALIDITY_FOR_SIM2, "0"));
            return expiryTime;
        }
        if (MessageUtils.isMultiSimEnabledMms()) {
            expiryTime = Long.parseLong(
                    prefs.getString((mSubId == ConstantsWrapper.Phone.SUB1) ?
                            MmsPreferenceActivity.MMS_VALIDITY_SIM1:
                            MmsPreferenceActivity.MMS_VALIDITY_SIM2, "0"));
        } else {
            expiryTime = Long.parseLong(
                    prefs.getString(MmsPreferenceActivity.MMS_VALIDITY_PERIOD_NO_MULTI, "0"));
        }
        return expiryTime;
    }

    public static void sendReadRec(Context context, String to, String messageId, int subId,
            int status) {
        EncodedStringValue[] sender = new EncodedStringValue[1];
        sender[0] = new EncodedStringValue(to);
        if (TextUtils.isEmpty(to) || TextUtils.isEmpty(messageId)) {
            Log.e(TAG, "Invalide PDU: to: " + to + ";messageId: " + messageId);
            return;
        }
        try {
            final ReadRecInd readRec = new ReadRecInd(
                    new EncodedStringValue(PduHeaders.FROM_INSERT_ADDRESS_TOKEN_STR.getBytes()),
                    messageId.getBytes(),
                    PduHeaders.CURRENT_MMS_VERSION,
                    status,
                    sender);

            readRec.setDate(System.currentTimeMillis() / 1000);

            Uri uri = PduPersister.getPduPersister(context).persist(readRec, Mms.Outbox.CONTENT_URI,
                    true, MessagingPreferenceActivity.getIsGroupMmsEnabled(context), null);
            ContentValues values = new ContentValues(1);
            values.put(Mms.SUBSCRIPTION_ID, subId);
            SqliteWrapper.update(context, context.getContentResolver(), uri, values, null, null);
            context.startService(new Intent(context, TransactionService.class));
        } catch (InvalidHeaderValueException e) {
            Log.e(TAG, "Invalide header value", e);
        } catch (MmsException e) {
            Log.e(TAG, "Persist message failed", e);
        }
    }
}
