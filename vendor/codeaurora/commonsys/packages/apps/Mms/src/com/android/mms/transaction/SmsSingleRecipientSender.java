package com.android.mms.transaction;

import java.util.ArrayList;

import android.app.ActivityThread;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.RemoteException;
import android.preference.PreferenceManager;
import android.provider.Telephony.Sms;
import android.telephony.PhoneNumberUtils;
import android.telephony.SmsManager;
import android.telephony.SubscriptionManager;
import android.text.TextUtils;
import android.util.Log;

import com.android.mms.LogTag;
import com.android.mms.MmsConfig;
import com.android.mms.R;
import com.android.mms.data.Conversation;
import com.android.mms.ui.MessageUtils;
import com.android.mms.ui.MessagingPreferenceActivity;
import com.android.mms.util.ContactRecipientEntryUtils;
import com.android.mmswrapper.ConstantsWrapper;
import com.android.mmswrapper.SmsManagerWrapper;
import com.android.mmswrapper.SubscriptionManagerWrapper;
import com.android.mmswrapper.TelephonyWrapper;
import com.google.android.mms.MmsException;

public class SmsSingleRecipientSender extends SmsMessageSender {

    private final boolean mRequestDeliveryReport;
    private String mDest;
    private Uri mUri;
    private static final String TAG = LogTag.TAG;
    private int mPriority = -1;
    private boolean isExpectMore;

    public SmsSingleRecipientSender(Context context, String dest, String msgText, long threadId,
            boolean requestDeliveryReport, Uri uri, int subId, boolean expectMore) {
        super(context, null, msgText, threadId, subId);
        mRequestDeliveryReport = requestDeliveryReport;
        mDest = dest;
        mUri = uri;
        isExpectMore = expectMore;
    }

    public void setPriority(int priority) {
        this.mPriority = priority;
    }

    private boolean sendEmptyMessage() throws MmsException {
        boolean moved = TelephonyWrapper.Sms.moveMessageToFolder(mContext, mUri, Sms.MESSAGE_TYPE_OUTBOX, 0);
        if (!moved) {
            throw new MmsException("SmsMessageSender.sendEmptyMessage couldn't move message " +
                    "to outbox: " + mUri);
        }

        PendingIntent deliveryIntent;
        if (mRequestDeliveryReport) {
            deliveryIntent = PendingIntent.getBroadcast(
                    mContext, 0,
                    new Intent(
                            MessageStatusReceiver.MESSAGE_STATUS_RECEIVED_ACTION,
                            mUri,
                            mContext,
                            MessageStatusReceiver.class),
                            0);
        } else {
            deliveryIntent = null;
        }

        Intent intent  = new Intent(SmsReceiverService.MESSAGE_SENT_ACTION,
                mUri,
                mContext,
                SmsReceiver.class);
        intent.putExtra(ConstantsWrapper.Phone.SUBSCRIPTION_KEY, mSubId);
        int requestCode = 1;
        intent.putExtra(SmsReceiverService.EXTRA_MESSAGE_SENT_SEND_NEXT, true);

        LogTag.debugD("sendEmptyMessage sendIntent: " + intent);
        PendingIntent sentIntent = PendingIntent.getBroadcast(mContext, requestCode, intent, 0);

        int validityPeriod = getValidityPeriod(mSubId);
        // Remove all attributes for CDMA international roaming.
        if (MessageUtils.isCDMAInternationalRoaming(mSubId)) {
            Log.v(TAG, "sendEmptyMessage during CDMA international roaming.");
            mPriority = -1;
            deliveryIntent = null;
            validityPeriod = -1;
        }
        try {
            LogTag.debugD("sendTextForSubscriberWithSelfPermissions:"
                                    + "mDest=" + mDest
                                    + "mRequestDeliveryReport=" +mRequestDeliveryReport
                                    + "mServiceCenter=" + mServiceCenter
                                    + "mThreadId=" + mThreadId
                                    + "|mUri=" + mUri
                                    );
            TelephonyWrapper.sendTextForSubscriberWithSelfPermissions(mSubId,
                    ActivityThread.currentPackageName(), mDest,
                    mServiceCenter, "", sentIntent, deliveryIntent, true);
        } catch (RemoteException ex) {
            // ignore it
        } catch (Exception ex) {
            Log.e(TAG, "iccISms.sendTextForSubscriberWithSelfPermissions: caught", ex);
            throw new MmsException("SmsMessageSender.sendEmptyMessage: caught " + ex +
                    " from iccISms.sendTextForSubscriberWithSelfPermissions()");
        }
        return false;
    }

    public boolean sendMessage(long token) throws MmsException {
        if (mMessageText == null) {
            // Don't try to send an empty message, and destination should be just
            // one.
            throw new MmsException("Null message body or have multiple destinations.");
        }
        SmsManager smsManager = SmsManager.getSmsManagerForSubscriptionId(mSubId);
        ArrayList<String> messages = null;
        if ((MmsConfig.getEmailGateway() != null) &&
                (ContactRecipientEntryUtils.isEmailAddress(mDest)
                        || MessageUtils.isAlias(mDest))) {
            String msgText;
            msgText = mDest + " " + mMessageText;
            mDest = MmsConfig.getEmailGateway();
            messages = smsManager.divideMessage(msgText);
        } else {
            messages = smsManager.divideMessage(mMessageText);
            // remove spaces and dashes from destination number
            // (e.g. "801 555 1212" -> "8015551212")
            // (e.g. "+8211-123-4567" -> "+82111234567")
            mDest = PhoneNumberUtils.stripSeparators(mDest);
            mDest = Conversation.verifySingleRecipient(mContext, mThreadId, mDest);
            mDest = MessageUtils.checkIdp(mContext, mDest, mSubId);
        }
        int messageCount = messages.size();

        if (messageCount == 0) {
            if (!mContext.getResources().getBoolean(R.bool.enable_send_blank_message)) {
                // Don't try to send an empty message.
                throw new MmsException("SmsMessageSender.sendMessage: divideMessage returned " +
                            "empty messages. Original message is \"" + mMessageText + "\"");
            } else {
                return sendEmptyMessage();
            }
        }

        boolean moved = TelephonyWrapper.Sms.moveMessageToFolder(mContext, mUri, Sms.MESSAGE_TYPE_OUTBOX, 0);
        if (!moved) {
            throw new MmsException("SmsMessageSender.sendMessage: couldn't move message " +
                    "to outbox: " + mUri);
        }

        ArrayList<PendingIntent> deliveryIntents =  new ArrayList<PendingIntent>(messageCount);
        ArrayList<PendingIntent> sentIntents = new ArrayList<PendingIntent>(messageCount);
        for (int i = 0; i < messageCount; i++) {
            if (mRequestDeliveryReport && (i == (messageCount - 1))) {
                // TODO: Fix: It should not be necessary to
                // specify the class in this intent.  Doing that
                // unnecessarily limits customizability.
                deliveryIntents.add(PendingIntent.getBroadcast(
                        mContext, 0,
                        new Intent(
                                MessageStatusReceiver.MESSAGE_STATUS_RECEIVED_ACTION,
                                mUri,
                                mContext,
                                MessageStatusReceiver.class),
                                0));
            } else {
                deliveryIntents.add(null);
            }
            Intent intent  = new Intent(SmsReceiverService.MESSAGE_SENT_ACTION,
                    mUri,
                    mContext,
                    SmsReceiver.class);
            intent.putExtra(ConstantsWrapper.Phone.SUBSCRIPTION_KEY, mSubId);
            int requestCode = 0;
            if (i == messageCount -1) {
                // Changing the requestCode so that a different pending intent
                // is created for the last fragment with
                // EXTRA_MESSAGE_SENT_SEND_NEXT set to true.
                requestCode = 1;
                intent.putExtra(SmsReceiverService.EXTRA_MESSAGE_SENT_SEND_NEXT, true);
            }
            LogTag.debugD("sendMessage sendIntent: " + intent);
            sentIntents.add(PendingIntent.getBroadcast(mContext, requestCode, intent, 0));
        }

        int validityPeriod = getValidityPeriod(mSubId);
        // Remove all attributes for CDMA international roaming.
        if (mContext.getResources().getBoolean(R.bool.config_ignore_sms_attributes) &&
                MessageUtils.isCDMAInternationalRoaming(mSubId)) {
            Log.v(TAG, "sendMessage during CDMA international roaming.");
            mPriority = -1;
            deliveryIntents = null;
            validityPeriod = -1;
        }
        try {
            LogTag.debugD("sendMultipartTextMessage:mDest=" + mDest
                    + "|mServiceCenter=" + mServiceCenter
                    + "|messages=" + ((messages != null) ? TextUtils.join(">", messages) : "empty")
                    + "|mPriority=" + mPriority
                    + "|isExpectMore=" + isExpectMore
                    + "|validityPeriod=" + validityPeriod
                    + "|threadId=" + mThreadId
                    + "|uri=" + mUri
                    + "|msgs.count=" + messageCount
                    + "|token=" + token
                    + "|mSubId=" + mSubId
                    + "|mRequestDeliveryReport=" + mRequestDeliveryReport
                    );
            SmsManagerWrapper.sendMultipartTextMessage(smsManager, mDest, mServiceCenter, messages,
                    sentIntents, deliveryIntents, mPriority, isExpectMore, validityPeriod);
        } catch (Exception ex) {
            Log.e(TAG, "SmsMessageSender.sendMessage: caught", ex);
            throw new MmsException("SmsMessageSender.sendMessage: caught " + ex +
                    " from SmsManager.sendMultipartTextMessage()");
        }
        return false;
    }

    private int getValidityPeriod(int subscription) {
        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(mContext);
        String valitidyPeriod = null;
        if (MessageUtils.isMultiSimEnabledMms()) {
            int phoneId = SubscriptionManagerWrapper.getPhoneId(subscription);
            switch (phoneId) {
                case MessageUtils.SUB1:
                    if (MessageUtils.isMsimIccCardActive()) {
                        valitidyPeriod = prefs.getString(
                                MessagingPreferenceActivity.SMS_VALIDITY_FOR_SIM1,
                                null);
                    }
                    else {
                        valitidyPeriod = prefs.getString(
                                MessagingPreferenceActivity.SMS_VALIDITY_SIM1, null);
                    }
                    break;
                case MessageUtils.SUB2:
                    if (MessageUtils.isMsimIccCardActive()) {
                        valitidyPeriod = prefs.getString(
                                MessagingPreferenceActivity.SMS_VALIDITY_FOR_SIM2,
                                null);
                    }
                    else {
                        valitidyPeriod = prefs.getString(
                                MessagingPreferenceActivity.SMS_VALIDITY_SIM2, null);
                        break;
                    }
                default:
                    break;
            }
        } else {
            valitidyPeriod = prefs.getString(MessagingPreferenceActivity.SMS_VALIDITY_NO_MULTI, null);
        }
        return (valitidyPeriod == null) ? -1 : Integer.parseInt(valitidyPeriod);
    }

    private void log(String msg) {
        Log.d(LogTag.TAG, "[SmsSingleRecipientSender] " + msg);
    }
}
