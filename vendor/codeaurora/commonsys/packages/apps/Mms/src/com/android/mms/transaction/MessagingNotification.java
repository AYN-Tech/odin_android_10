/*
 * Copyright (C) 2010-2014, 2016-2017, The Linux Foundation. All rights reserved.
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

import static com.google.android.mms.pdu.PduHeaders.MESSAGE_TYPE_NOTIFICATION_IND;
import static com.google.android.mms.pdu.PduHeaders.MESSAGE_TYPE_RETRIEVE_CONF;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.SortedSet;
import java.util.TreeSet;
import java.util.List;

import android.app.ActivityManager;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.TaskStackBuilder;
import android.app.RemoteInput;
import android.content.BroadcastReceiver;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.database.Cursor;
import android.database.sqlite.SqliteWrapper;
import android.graphics.Bitmap;
import android.graphics.Typeface;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.media.AudioAttributes;
import android.media.AudioManager;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Handler;
import android.preference.PreferenceManager;
import android.provider.BaseColumns;
import android.provider.Telephony.Mms;
import android.provider.Telephony.MmsSms;
import android.provider.Telephony.Sms;
import android.support.v4.graphics.drawable.RoundedBitmapDrawable;
import android.telephony.PhoneStateListener;
import android.telephony.SubscriptionManager;
import android.telephony.SubscriptionInfo;
import android.telephony.TelephonyManager;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.SpannableStringBuilder;
import android.text.TextUtils;
import android.text.style.StyleSpan;
import android.text.style.TextAppearanceSpan;
import android.util.Log;
import android.widget.Toast;

import com.android.mms.LogTag;
import com.android.mms.MmsConfig;
import com.android.mms.R;
import com.android.mms.data.Contact;
import com.android.mms.data.Conversation;
import com.android.mms.data.WorkingMessage;
import com.android.mms.model.SlideModel;
import com.android.mms.model.SlideshowModel;
import com.android.mms.ui.ComposeMessageActivity;
import com.android.mms.ui.ConversationList;
import com.android.mms.ui.LetterTileDrawable;
import com.android.mms.ui.MailBoxMessageContent;
import com.android.mms.ui.MailBoxMessageList;
import com.android.mms.ui.ManageSimMessages;
import com.android.mms.ui.MessageUtils;
import com.android.mms.ui.MessagingPreferenceActivity;
import com.android.mms.ui.MobilePaperShowActivity;
import com.android.mms.ui.NotificationActionHandleReceiver;
import com.android.mms.ui.NotificationQuickReplyActivity;
import com.android.mms.util.AddressUtils;
import com.android.mms.util.DownloadManager;
import com.android.mms.util.MaterialColorMapUtils;
import com.android.mms.widget.MmsWidgetProvider;
import com.android.mmswrapper.ConstantsWrapper;
import com.android.mmswrapper.SubscriptionManagerWrapper;

import com.google.android.mms.MmsException;
import com.google.android.mms.pdu.EncodedStringValue;
import com.google.android.mms.pdu.GenericPdu;
import com.google.android.mms.pdu.MultimediaMessagePdu;
import com.google.android.mms.pdu.PduHeaders;
import com.google.android.mms.pdu.PduPersister;
import android.app.NotificationChannel;


/**
 * This class is used to update the notification indicator. It will check whether
 * there are unread messages. If yes, it would show the notification indicator,
 * otherwise, hide the indicator.
 */
public class MessagingNotification {

    private static final String TAG = LogTag.APP;
    private static final boolean DEBUG = false;

    public static final int NOTIFICATION_ID = 123;
    public static final int FULL_NOTIFICATION_ID   = 125;
    private static final int ICC_NOTIFICATION_ID_SLOT1 = 126;
    private static final int ICC_NOTIFICATION_ID_SLOT2 = 127;
    private static final int ICC_NOTIFICATION_ID = 128;
    public static final int MESSAGE_FAILED_NOTIFICATION_ID = 789;
    public static final int DOWNLOAD_FAILED_NOTIFICATION_ID = 531;

    public static final int SERVICE_NOTIFICATION_ID = 1;
    /**
     * This is the volume at which to play the in-conversation notification sound,
     * expressed as a fraction of the system notification volume.
     */
    private static final float IN_CONVERSATION_NOTIFICATION_VOLUME = 0.25f;

    // This must be consistent with the column constants below.
    private static final String[] MMS_STATUS_PROJECTION = new String[] {
        Mms.THREAD_ID, Mms.DATE, Mms._ID, Mms.SUBJECT, Mms.SUBJECT_CHARSET, Mms.SUBSCRIPTION_ID };

    // This must be consistent with the column constants below.
    private static final String[] SMS_STATUS_PROJECTION = new String[] {
        Sms.THREAD_ID, Sms.DATE, Sms.ADDRESS, Sms.BODY, Sms.SUBSCRIPTION_ID, Sms._ID };

    private static final int[] NEW_ICC_NOTIFICATION_ID = new int[] {
        ICC_NOTIFICATION_ID_SLOT1, ICC_NOTIFICATION_ID_SLOT2
    };

    private static final String[] MAILBOX_PROJECTION = new String[] {
            MmsSms.TYPE_DISCRIMINATOR_COLUMN, BaseColumns._ID,
            Sms.TYPE, Mms.MESSAGE_BOX
    };

    private static final String[] SMS_THREAD_ID_PROJECTION = new String[] { Sms.THREAD_ID };
    private static final String[] MMS_THREAD_ID_PROJECTION = new String[] { Mms.THREAD_ID };

    private static final int COLUMN_MMS_THREAD_ID   = 0;
    private static final int COLUMN_MMS_DATE        = 1;
    private static final int COLUMN_MMS_ID          = 2;
    private static final int COLUMN_MMS_SUBJECT     = 3;
    private static final int COLUMN_MMS_SUBJECT_CS  = 4;
    private static final int COLUMN_MMS_SUB_ID      = 5;

    private static final int COLUMN_SMS_THREAD_ID   = 0;
    private static final int COLUMN_SMS_DATE        = 1;
    private static final int COLUMN_SMS_ADDRESS     = 2;
    private static final int COLUMN_SMS_BODY        = 3;
    private static final int COLUMN_SMS_SUB_ID      = 4;
    private static final int COLUMN_SMS_ID          = 5;

    private static final int MAILBOX_MSG_TYPE       = 0;
    private static final int MAILBOX_ID             = 1;
    private static final int MAILBOX_SMS_TYPE       = 2;
    private static final int MAILBOX_MMS_BOX        = 3;

    private static final String NEW_INCOMING_SM_CONSTRAINT =
            "(" + Sms.TYPE + " = " + Sms.MESSAGE_TYPE_INBOX
            + " AND " + Sms.SEEN + " = 0)";

    private static final String NEW_DELIVERY_SM_CONSTRAINT =
        "(" + Sms.TYPE + " = " + Sms.MESSAGE_TYPE_SENT
        + " AND " + Sms.STATUS + " = "+ Sms.STATUS_COMPLETE +")";

    private static final String NEW_INCOMING_MM_CONSTRAINT =
            "(" + Mms.MESSAGE_BOX + "=" + Mms.MESSAGE_BOX_INBOX
            + " AND " + Mms.SEEN + "=0"
            + " AND (" + Mms.MESSAGE_TYPE + "=" + MESSAGE_TYPE_NOTIFICATION_IND
            + " OR " + Mms.MESSAGE_TYPE + "=" + MESSAGE_TYPE_RETRIEVE_CONF + "))";

    private static final NotificationInfoComparator INFO_COMPARATOR =
            new NotificationInfoComparator();

    private static final Uri UNDELIVERED_URI = Uri.parse("content://mms-sms/undelivered");
    private final static String UNDELIVERED_FLAG = "undelivered_flag";
    private final static String FAILED_DOWNLOAD_FLAG = "failed_download_flag";

    private final static String NOTIFICATION_DELETED_ACTION =
            "com.android.mms.NOTIFICATION_DELETED_ACTION";
    private static final String NOTIFICATION_GROUP_KEY = "group_key_mms";
    private static final String NOTIFICATION_MSG_TAG = ":sms:";

    public  static final String NEW_MESSAGE_CHANNEL_ID = "533";
    public  static final String SIM_FULL_CHANNEL_ID = "534";
    public  static final String DEVICE_FULL_CHANNEL_ID = "535";
    public  static final String REJECTED_MESSAGE_CHANNEL_ID = "536";

    public static class OnDeletedReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (Log.isLoggable(LogTag.APP, Log.VERBOSE)) {
                Log.d(TAG, "[MessagingNotification] clear notification: mark all msgs seen");
            }

            Conversation.markAllConversationsAsSeen(context);
        }
    }

    public static final long THREAD_ALL  = -1;
    public static final long THREAD_NONE = -2;
    public static final long THREAD_SIM1 = -3;
    public static final long THREAD_SIM2 = -4;
    public static final long THREAD_SIM  = -5;

    /**
     * Keeps track of the thread ID of the conversation that's currently displayed to the user
     */
    private static long sCurrentlyDisplayedThreadId;
    private static final Object sCurrentlyDisplayedThreadLock = new Object();

    private static long sCurrentlyDisplayedMsgType;
    private static final Object sCurrentlyDisplayedMsgTypeLock = new Object();

    private static OnDeletedReceiver sNotificationDeletedReceiver = new OnDeletedReceiver();
    private static Intent sNotificationOnDeleteIntent;
    private static Handler sHandler = new Handler();
    private static PduPersister sPduPersister;
    private static final int MAX_BITMAP_DIMEN_DP = 360;
    private static float sScreenDensity;

    private static boolean includeEmergencySMS;
    private static String emergentcyAddress = "911";

    private static final int MAX_MESSAGES_TO_SHOW = 8;  // the maximum number of new messages to
                                                        // show in a single notification.
    public static final AudioAttributes AUDIO_ATTRIBUTES_ALARM = new AudioAttributes.Builder()
            .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
            .setUsage(AudioAttributes.USAGE_ALARM)
            .build();

    private MessagingNotification() {
    }

    public static void init(Context context) {
        // set up the intent filter for notification deleted action
        IntentFilter intentFilter = new IntentFilter();
        intentFilter.addAction(NOTIFICATION_DELETED_ACTION);

        // TODO: should we unregister when the app gets killed?
        context.registerReceiver(sNotificationDeletedReceiver, intentFilter);
        sPduPersister = PduPersister.getPduPersister(context);

        // initialize the notification deleted action
        sNotificationOnDeleteIntent = new Intent(NOTIFICATION_DELETED_ACTION);

        sScreenDensity = context.getResources().getDisplayMetrics().density;
    }

    /**
     * Specifies which message thread is currently being viewed by the user. New messages in that
     * thread will not generate a notification icon and will play the notification sound at a lower
     * volume. Make sure you set this to THREAD_NONE when the UI component that shows the thread is
     * no longer visible to the user (e.g. Activity.onPause(), etc.)
     * @param threadId The ID of the thread that the user is currently viewing. Pass THREAD_NONE
     *  if the user is not viewing a thread, or THREAD_ALL if the user is viewing the conversation
     *  list (note: that latter one has no effect as of this implementation)
     */
    public static void setCurrentlyDisplayedThreadId(long threadId) {
        synchronized (sCurrentlyDisplayedThreadLock) {
            sCurrentlyDisplayedThreadId = threadId;
            if (DEBUG) {
                Log.d(TAG, "setCurrentlyDisplayedThreadId: " + sCurrentlyDisplayedThreadId);
            }
        }
    }

    public static void setCurrentlyDisplayedMsgType(long msgType) {
        synchronized (sCurrentlyDisplayedMsgTypeLock) {
            sCurrentlyDisplayedMsgType = msgType;
            if (DEBUG) {
                Log.d(TAG, "sCurrentlyDisplayedMsgType: " + sCurrentlyDisplayedMsgType);
            }
        }
    }

    /**
     * Checks to see if there are any "unseen" messages or delivery
     * reports.  Shows the most recent notification if there is one.
     * Does its work and query in a worker thread.
     *
     * @param context the context to use
     */
    public static void nonBlockingUpdateNewMessageIndicator(final Context context,
            final long newMsgThreadId,
            final boolean isStatusMessage) {
        if (DEBUG) {
            Log.d(TAG, "nonBlockingUpdateNewMessageIndicator: newMsgThreadId: " +
                    newMsgThreadId +
                    " sCurrentlyDisplayedThreadId: " + sCurrentlyDisplayedThreadId +
                    " sCurrentlyDisplayedMsgType: " + sCurrentlyDisplayedMsgType);
        }
        new Thread(new Runnable() {
            @Override
            public void run() {
                blockingUpdateNewMessageIndicator(context, newMsgThreadId, isStatusMessage);
            }
        }, "MessagingNotification.nonBlockingUpdateNewMessageIndicator").start();
    }

    /**
     * Checks to see if there are any "unseen" messages or delivery
     * reports and builds a sorted (by delivery date) list of unread notifications.
     *
     * @param context the context to use
     * @param newMsgThreadId The thread ID of a new message that we're to notify about; if there's
     *  no new message, use THREAD_NONE. If we should notify about multiple or unknown thread IDs,
     *  use THREAD_ALL.
     * @param isStatusMessage
     */
    public static void blockingUpdateNewMessageIndicator(Context context, long newMsgThreadId,
            boolean isStatusMessage) {
        blockingUpdateNewMessageIndicator(context,newMsgThreadId,isStatusMessage,null);
    }
    public static void blockingUpdateNewMessageIndicator(Context context, long newMsgThreadId,
            boolean isStatusMessage, Uri msgUri) {
        if (DEBUG) {
            Contact.logWithTrace(TAG, "blockingUpdateNewMessageIndicator: newMsgThreadId: " +
                    newMsgThreadId);
        }
        final boolean isDefaultSmsApp = MmsConfig.isSmsEnabled(context);
        if (!isDefaultSmsApp) {
            cancelNotification(context, NOTIFICATION_ID);
            if (DEBUG || Log.isLoggable(LogTag.APP, Log.VERBOSE)) {
                Log.d(TAG, "blockingUpdateNewMessageIndicator: not the default sms app - skipping "
                        + "notification");
            }
            return;
        }

        // notificationSet is kept sorted by the incoming message delivery time, with the
        // most recent message first.
        SortedSet<NotificationInfo> notificationSet =
                new TreeSet<NotificationInfo>(INFO_COMPARATOR);

        Set<Long> threads = new HashSet<Long>(4);

        addMmsNotificationInfos(context, threads, notificationSet);
        addSmsNotificationInfos(context, threads, notificationSet);

        if (notificationSet.isEmpty()) {
            if (DEBUG) {
                Log.d(TAG, "blockingUpdateNewMessageIndicator: notificationSet is empty, " +
                        "canceling existing notifications");
            }
            cancelNotification(context, NOTIFICATION_ID);
        } else {
            if (DEBUG || Log.isLoggable(LogTag.APP, Log.VERBOSE)) {
                Log.d(TAG, "blockingUpdateNewMessageIndicator: count=" + notificationSet.size() +
                        ", newMsgThreadId=" + newMsgThreadId);
            }

            if (isInCurrentConversation(newMsgThreadId, threads)) {
                if (DEBUG) {
                    Log.d(TAG, "blockingUpdateNewMessageIndicator: newMsgThreadId == " +
                            "sCurrentlyDisplayedThreadId so NOT showing notification," +
                            " but playing soft sound. threadId: " + newMsgThreadId);
                }
                playInConversationNotificationSound(context);
                return;
            }
            updateNotification(context, newMsgThreadId != THREAD_NONE, threads.size(),
                    notificationSet);
        }

        // And deals with delivery reports (which use Toasts). It's safe to call in a worker
        // thread because the toast will eventually get posted to a handler.
        if (isStatusMessage && msgUri != null) {
            MmsSmsDeliveryInfo delivery = getSmsNewDeliveryInfo(context,msgUri);
            if (delivery != null) {
                delivery.deliver(context, isStatusMessage);
            }
        }

        notificationSet.clear();
        threads.clear();
    }

    private static boolean isInCurrentConversation(long newMsgThreadId, Set<Long> threads) {
        if (MessageUtils.isMailboxMode()) {
            // For mail box mode, only message with the same tpye will not show the notification.
            synchronized (sCurrentlyDisplayedMsgTypeLock) {
                return (sCurrentlyDisplayedMsgType == MailBoxMessageList.TYPE_INBOX) &&
                        (newMsgThreadId != THREAD_NONE);
            }
        } else {
            // For conversation mode, only incomming unread message with the same valid thread id
            // will not show the notification.
            synchronized (sCurrentlyDisplayedThreadLock) {
                return (newMsgThreadId > 0 && newMsgThreadId == sCurrentlyDisplayedThreadId &&
                        threads.contains(newMsgThreadId));
            }
        }
    }

    public static void blockingUpdateNewIccMessageIndicator(Context context, String address,
            String message, int subId, long timeMillis) {
        int phoneId = SubscriptionManagerWrapper.getPhoneId(subId);
        if (MessageUtils.getSimThreadByPhoneId(phoneId) == sCurrentlyDisplayedThreadId) {
            // We are already diplaying the messages list view, no need to send notification.
            // Just play notification sound.
            Log.d(TAG, "blockingUpdateNewIccMessageIndicator displaying sim messages now");
            playInConversationNotificationSound(context);
            return;
        }
        final Notification.Builder noti = new Notification.Builder(context,
                NEW_MESSAGE_CHANNEL_ID).setWhen(timeMillis);
        Contact contact = Contact.get(address, false);
        NotificationInfo info = getNewIccMessageNotificationInfo(context, true /* isSms */,
                address, message, null /* subject */, subId, timeMillis,
                null /* attachmentBitmap */, contact, WorkingMessage.TEXT);
        noti.setSmallIcon(R.drawable.ic_notification);
        NotificationManager nm = (NotificationManager)
                context.getSystemService(Context.NOTIFICATION_SERVICE);

        NotificationChannel channel = new NotificationChannel(NEW_MESSAGE_CHANNEL_ID,
                context.getString(R.string.message_notification),
                NotificationManager.IMPORTANCE_HIGH);
        nm.createNotificationChannel(channel);

//        TaskStackBuilder taskStackBuilder = TaskStackBuilder.create(context);
        // Update the notification.
        PendingIntent pendingIntent;
        if (phoneId == MessageUtils.SUB_INVALID) {
            pendingIntent = PendingIntent.getActivity(context, 0, info.mClickIntent,
                    PendingIntent.FLAG_UPDATE_CURRENT);
        } else {
            // Use requestCode to avoid updating all intents of previous notifications
            pendingIntent = PendingIntent.getActivity(context,
                    NEW_ICC_NOTIFICATION_ID[phoneId], info.mClickIntent,
                    PendingIntent.FLAG_UPDATE_CURRENT);
        }
        String title = info.mTitle;
        noti.setContentTitle(title)
            .setContentIntent(pendingIntent)
                    //taskStackBuilder.getPendingIntent(0, PendingIntent.FLAG_UPDATE_CURRENT))
            .setCategory(Notification.CATEGORY_MESSAGE)
            .setPriority(Notification.PRIORITY_DEFAULT);

        int defaults = 0;
        SharedPreferences sp = PreferenceManager.getDefaultSharedPreferences(context);
        boolean vibrate = false;
        if (sp.contains(MessagingPreferenceActivity.NOTIFICATION_VIBRATE)) {
            // The most recent change to the vibrate preference is to store a boolean
            // value in NOTIFICATION_VIBRATE. If prefs contain that preference, use that
            // first.
            vibrate = sp.getBoolean(MessagingPreferenceActivity.NOTIFICATION_VIBRATE,
                    false);
        } else if (sp.contains(MessagingPreferenceActivity.NOTIFICATION_VIBRATE_WHEN)) {
            // This is to support the pre-JellyBean MR1.1 version of vibrate preferences
            // when vibrate was a tri-state setting. As soon as the user opens the Messaging
            // app's settings, it will migrate this setting from NOTIFICATION_VIBRATE_WHEN
            // to the boolean value stored in NOTIFICATION_VIBRATE.
            String vibrateWhen =
                    sp.getString(MessagingPreferenceActivity.NOTIFICATION_VIBRATE_WHEN, null);
            vibrate = "always".equals(vibrateWhen);
        }
        if (vibrate) {
            defaults |= Notification.DEFAULT_VIBRATE;
        }
        String ringtoneStr = sp.getString(MessagingPreferenceActivity.NOTIFICATION_RINGTONE,
                null);
        if (!includeEmergencySMS) {
            TelephonyManager tm = (TelephonyManager)context.getSystemService(
                    Context.TELEPHONY_SERVICE);
            int state = tm.getCallState();
            if (state != TelephonyManager.CALL_STATE_IDLE) {
                noti.setSound(TextUtils.isEmpty(ringtoneStr) ? null : Uri.parse(ringtoneStr),
                        AUDIO_ATTRIBUTES_ALARM);
            } else {
                noti.setSound(TextUtils.isEmpty(ringtoneStr) ? null : Uri.parse(ringtoneStr));
            }
        }
        Log.d(TAG, "blockingUpdateNewIccMessageIndicator: adding sound to the notification");
        defaults |= Notification.DEFAULT_LIGHTS;
        noti.setDefaults(defaults);

        // set up delete intent
        noti.setDeleteIntent(PendingIntent.getBroadcast(context, 0,
                sNotificationOnDeleteIntent, 0));

        final Notification notification;
        // This sets the text for the collapsed form:
        noti.setContentText(info.formatBigMessage(context));

        if (info.mAttachmentBitmap != null) {
            // The message has a picture, show that

            notification = new Notification.BigPictureStyle(noti)
                .bigPicture(info.mAttachmentBitmap)
                // This sets the text for the expanded picture form:
                .setSummaryText(info.formatPictureMessage(context))
                .build();
        } else {
            // Show a single notification -- big style with the text of the whole message
            notification = new Notification.BigTextStyle(noti)
                .bigText(info.formatBigMessage(context))
                .build();
        }

        notifyUserIfFullScreen(context, title);

        if (phoneId == MessageUtils.SUB_INVALID) {
            nm.notify(ICC_NOTIFICATION_ID, notification);
        } else {
            nm.notify(NEW_ICC_NOTIFICATION_ID[phoneId], notification);
        }

    }

    /**
     * Play the in-conversation notification sound (it's the regular notification sound, but
     * played at half-volume
     */
    private static void playInConversationNotificationSound(Context context) {
        if (includeEmergencySMS && context.getResources().getBoolean(
                R.bool.config_regional_sms_notification_from_911_disable)) {
            return;
        }
        SharedPreferences sp = PreferenceManager.getDefaultSharedPreferences(context);
        String ringtoneStr = sp.getString(MessagingPreferenceActivity.NOTIFICATION_RINGTONE,
                null);
        if (TextUtils.isEmpty(ringtoneStr)) {
            // Nothing to play
            return;
        }
        Uri ringtoneUri = Uri.parse(ringtoneStr);
        final NotificationPlayer player = new NotificationPlayer(LogTag.APP);
        player.play(context, ringtoneUri, false, AudioManager.STREAM_NOTIFICATION,
                IN_CONVERSATION_NOTIFICATION_VOLUME);

        // Stop the sound after five seconds to handle continuous ringtones
        sHandler.postDelayed(new Runnable() {
            @Override
            public void run() {
                player.stop();
            }
        }, 5000);
    }

    /**
     * Updates all pending notifications, clearing or updating them as
     * necessary.
     */
    public static void blockingUpdateAllNotifications(final Context context, long threadId) {
        if (DEBUG) {
            Contact.logWithTrace(TAG, "blockingUpdateAllNotifications: newMsgThreadId: " +
                    threadId);
        }
        nonBlockingUpdateNewMessageIndicator(context, threadId, false);
        nonBlockingUpdateSendFailedNotification(context);
        updateDownloadFailedNotification(context);
        MmsWidgetProvider.notifyDatasetChanged(context);
    }

    private static final class MmsSmsDeliveryInfo {
        public CharSequence mTicker;
        public long mTimeMillis;

        public MmsSmsDeliveryInfo(CharSequence ticker, long timeMillis) {
            mTicker = ticker;
            mTimeMillis = timeMillis;
        }

        public void deliver(Context context, boolean isStatusMessage) {
            updateDeliveryNotification(
                    context, isStatusMessage, mTicker, mTimeMillis);
        }
    }

    private static final class NotificationInfo {
        public final Intent mClickIntent;
        public final String mMessage;
        public final CharSequence mTicker;
        public final long mTimeMillis;
        public final String mTitle;
        public final Bitmap mAttachmentBitmap;
        public final Contact mSender;
        public final boolean mIsSms;
        public final int mAttachmentType;
        public final String mSubject;
        public final long mThreadId;
        public final int mSubId;

        /**
         * @param isSms            true if sms, false if mms
         * @param clickIntent      where to go when the user taps the notification
         * @param message          for a single message, this is the message text
         * @param subject          text of mms subject
         * @param ticker           text displayed ticker-style across the notification,
         *                         typically formatted as sender: message
         * @param timeMillis       date the message was received
         * @param title            for a single message, this is the sender
         * @param attachmentBitmap a bitmap of an attachment, such as a picture or video
         * @param sender           contact of the sender
         * @param attachmentType   of the mms attachment
         * @param threadId         thread this message belongs to
         * @param subId            sub which this message belongs to
         */
        public NotificationInfo(boolean isSms,
                Intent clickIntent, String message, String subject,
                CharSequence ticker, long timeMillis, String title,
                Bitmap attachmentBitmap, Contact sender,
                int attachmentType, long threadId, int subId) {
            mIsSms = isSms;
            mClickIntent = clickIntent;
            mMessage = message;
            mSubject = subject;
            mTicker = ticker;
            mTimeMillis = timeMillis;
            mTitle = title;
            mAttachmentBitmap = attachmentBitmap;
            mSender = sender;
            mAttachmentType = attachmentType;
            mThreadId = threadId;
            mSubId = subId;
        }

        public long getTime() {
            return mTimeMillis;
        }

        // This is the message string used in bigText and bigPicture notifications.
        public CharSequence formatBigMessage(Context context) {
            final TextAppearanceSpan notificationSubjectSpan = new TextAppearanceSpan(
                    context, R.style.NotificationPrimaryText);

            // Change multiple newlines (with potential white space between), into a single new line
            final String message =
                    !TextUtils.isEmpty(mMessage) ? mMessage.replaceAll("\\n\\s+", "\n") : "";

            SpannableStringBuilder spannableStringBuilder = new SpannableStringBuilder();
            if (!TextUtils.isEmpty(mSubject)) {
                spannableStringBuilder.append(mSubject);
                spannableStringBuilder.setSpan(notificationSubjectSpan, 0, mSubject.length(), 0);
            }
            if (mAttachmentType > WorkingMessage.TEXT) {
                if (spannableStringBuilder.length() > 0) {
                    spannableStringBuilder.append('\n');
                }
                spannableStringBuilder.append(getAttachmentTypeString(context, mAttachmentType));
            }
            if (mMessage != null) {
                if (spannableStringBuilder.length() > 0) {
                    spannableStringBuilder.append('\n');
                }
                spannableStringBuilder.append(mMessage);
            }
            return spannableStringBuilder;
        }

        // This is the message string used in each line of an inboxStyle notification.
        public CharSequence formatInboxMessage(Context context) {
          final TextAppearanceSpan notificationSenderSpan = new TextAppearanceSpan(
                  context, R.style.NotificationPrimaryText);

          final TextAppearanceSpan notificationSubjectSpan = new TextAppearanceSpan(
                  context, R.style.NotificationSubjectText);

          // Change multiple newlines (with potential white space between), into a single new line
          final String message =
                  !TextUtils.isEmpty(mMessage) ? mMessage.replaceAll("\\n\\s+", "\n") : "";

          SpannableStringBuilder spannableStringBuilder = new SpannableStringBuilder();
          final String sender = mSender.getName();
          if (!TextUtils.isEmpty(sender)) {
              spannableStringBuilder.append(sender);
              spannableStringBuilder.setSpan(notificationSenderSpan, 0, sender.length(), 0);
          }
          String separator = context.getString(R.string.notification_separator);
          if (!mIsSms) {
              if (!TextUtils.isEmpty(mSubject)) {
                  if (spannableStringBuilder.length() > 0) {
                      spannableStringBuilder.append(separator);
                  }
                  int start = spannableStringBuilder.length();
                  spannableStringBuilder.append(mSubject);
                  spannableStringBuilder.setSpan(notificationSubjectSpan, start,
                          start + mSubject.length(), 0);
              }
              if (mAttachmentType > WorkingMessage.TEXT) {
                  if (spannableStringBuilder.length() > 0) {
                      spannableStringBuilder.append(separator);
                  }
                  spannableStringBuilder.append(getAttachmentTypeString(context, mAttachmentType));
              }
          }
          if (message.length() > 0) {
              if (spannableStringBuilder.length() > 0) {
                  spannableStringBuilder.append(separator);
              }
              int start = spannableStringBuilder.length();
              spannableStringBuilder.append(message);
              spannableStringBuilder.setSpan(notificationSubjectSpan, start,
                      start + message.length(), 0);
          }
          return spannableStringBuilder;
        }

        // This is the summary string used in bigPicture notifications.
        public CharSequence formatPictureMessage(Context context) {
            final TextAppearanceSpan notificationSubjectSpan = new TextAppearanceSpan(
                    context, R.style.NotificationPrimaryText);

            // Change multiple newlines (with potential white space between), into a single new line
            final String message =
                    !TextUtils.isEmpty(mMessage) ? mMessage.replaceAll("\\n\\s+", "\n") : "";

            // Show the subject or the message (if no subject)
            SpannableStringBuilder spannableStringBuilder = new SpannableStringBuilder();
            if (!TextUtils.isEmpty(mSubject)) {
                spannableStringBuilder.append(mSubject);
                spannableStringBuilder.setSpan(notificationSubjectSpan, 0, mSubject.length(), 0);
            }
            if (message.length() > 0 && spannableStringBuilder.length() == 0) {
                spannableStringBuilder.append(message);
                spannableStringBuilder.setSpan(notificationSubjectSpan, 0, message.length(), 0);
            }
            return spannableStringBuilder;
        }
    }

    // Return a formatted string with all the sender names separated by commas.
    private static CharSequence formatSenders(Context context,
            ArrayList<NotificationInfo> senders) {
        String separator = context.getString(R.string.enumeration_comma);   // ", "
        SpannableStringBuilder spannableStringBuilder = new SpannableStringBuilder();
        int len = senders.size();
        for (int i = 0; i < len; i++) {
            if (i > 0) {
                spannableStringBuilder.append(separator);
            }
            spannableStringBuilder.append(senders.get(i).mSender.getName());
        }

        return spannableStringBuilder;
    }

    // Return a formatted string with the attachmentType spelled out as a string. For
    // no attachment (or just text), return null.
    private static CharSequence getAttachmentTypeString(Context context, int attachmentType) {
        final TextAppearanceSpan notificationAttachmentSpan = new TextAppearanceSpan(
                context, R.style.NotificationSecondaryText);
        int id = 0;
        switch (attachmentType) {
            case WorkingMessage.AUDIO: id = R.string.attachment_audio; break;
            case WorkingMessage.VIDEO: id = R.string.attachment_video; break;
            case WorkingMessage.SLIDESHOW: id = R.string.attachment_slideshow; break;
            case WorkingMessage.IMAGE: id = R.string.attachment_picture; break;
        }
        if (id > 0) {
            final SpannableString spannableString = new SpannableString(context.getString(id));
            spannableString.setSpan(notificationAttachmentSpan,
                    0, spannableString.length(), 0);
            return spannableString;
        }
        return null;
     }

    /**
     *
     * Sorts by the time a notification was received in descending order -- newer first.
     *
     */
    private static final class NotificationInfoComparator
            implements Comparator<NotificationInfo> {
        @Override
        public int compare(
                NotificationInfo info1, NotificationInfo info2) {
            return Long.signum(info2.getTime() - info1.getTime());
        }
    }

    private static final void addMmsNotificationInfos(
            Context context, Set<Long> threads, SortedSet<NotificationInfo> notificationSet) {
        ContentResolver resolver = context.getContentResolver();

        // This query looks like this when logged:
        // I/Database(  147): elapsedTime4Sql|/data/data/com.android.providers.telephony/databases/
        // mmssms.db|0.362 ms|SELECT thread_id, date, _id, sub, sub_cs FROM pdu WHERE ((msg_box=1
        // AND seen=0 AND (m_type=130 OR m_type=132))) ORDER BY date desc

        Cursor cursor = SqliteWrapper.query(context, resolver, Mms.CONTENT_URI,
                            MMS_STATUS_PROJECTION, NEW_INCOMING_MM_CONSTRAINT,
                            null, Mms.DATE + " desc");

        if (cursor == null) {
            return;
        }

        try {
            while (cursor.moveToNext()) {

                long msgId = cursor.getLong(COLUMN_MMS_ID);
                Uri msgUri = Mms.CONTENT_URI.buildUpon().appendPath(
                        Long.toString(msgId)).build();
                String address = AddressUtils.getFrom(context, msgUri);

                Contact contact = Contact.get(address, false);
                if (contact.getSendToVoicemail()) {
                    // don't notify, skip this one
                    continue;
                }

                String subjectLabel = context.getResources().getString(R.string.subject_label);
                String subject = getMmsSubject(
                        cursor.getString(COLUMN_MMS_SUBJECT),
                        cursor.getInt(COLUMN_MMS_SUBJECT_CS));
                if (!TextUtils.isEmpty(subject)) {
                    subject = subjectLabel + subject;
                }
                subject = MessageUtils.cleanseMmsSubject(context, subject);

                long threadId = cursor.getLong(COLUMN_MMS_THREAD_ID);
                long timeMillis = cursor.getLong(COLUMN_MMS_DATE) * 1000;
                int subId = cursor.getInt(COLUMN_MMS_SUB_ID);

                if (Log.isLoggable(LogTag.APP, Log.VERBOSE)) {
                    Log.d(TAG, "addMmsNotificationInfos: count=" + cursor.getCount() +
                            ", addr = " + address + ", thread_id=" + threadId);
                }

                // Extract the message and/or an attached picture from the first slide
                Bitmap attachedPicture = null;
                String messageBody = null;
                int attachmentType = WorkingMessage.TEXT;
                try {
                    GenericPdu pdu = sPduPersister.load(msgUri);
                    if (pdu != null && pdu instanceof MultimediaMessagePdu) {
                        SlideshowModel slideshow = SlideshowModel.createFromPduBody(context,
                                ((MultimediaMessagePdu)pdu).getBody());
                        attachmentType = getAttachmentType(slideshow);
                        SlideModel firstSlide = slideshow.get(0);
                        if (firstSlide != null) {
                            if (firstSlide.hasImage()) {
                                int maxDim = dp2Pixels(MAX_BITMAP_DIMEN_DP);
                                attachedPicture = firstSlide.getImage().getBitmap(maxDim, maxDim);
                            }
                            if (firstSlide.hasText()) {
                                messageBody = firstSlide.getText().getText();
                            }
                        }
                    }
                } catch (final MmsException e) {
                    Log.e(TAG, "MmsException loading uri: " + msgUri, e);
                    continue;   // skip this bad boy -- don't generate an empty notification
                }

                NotificationInfo info = getNewMessageNotificationInfo(context,
                        false /* isSms */,
                        address,
                        messageBody, subject,
                        threadId, subId,
                        timeMillis,
                        attachedPicture,
                        contact,
                        attachmentType);
                if (MessageUtils.isMailboxMode()) {
                    info.mClickIntent.setData(msgUri);
                }
                notificationSet.add(info);

                threads.add(threadId);
            }
        } finally {
            cursor.close();
        }
    }

    // Look at the passed in slideshow and determine what type of attachment it is.
    private static int getAttachmentType(SlideshowModel slideshow) {
        int slideCount = slideshow.size();

        if (slideCount == 0) {
            return WorkingMessage.TEXT;
        } else if (slideCount > 1) {
            return WorkingMessage.SLIDESHOW;
        } else {
            SlideModel slide = slideshow.get(0);
            if (slide.hasImage()) {
                return WorkingMessage.IMAGE;
            } else if (slide.hasVideo()) {
                return WorkingMessage.VIDEO;
            } else if (slide.hasAudio()) {
                return WorkingMessage.AUDIO;
            }
        }
        return WorkingMessage.TEXT;
    }

    private static final int dp2Pixels(int dip) {
        return (int) (dip * sScreenDensity + 0.5f);
    }

    private static final MmsSmsDeliveryInfo getSmsNewDeliveryInfo(Context context, Uri msgUri) {
        LogTag.debugD("getSmsNewDeliveryInfo:" + msgUri);
        ContentResolver resolver = context.getContentResolver();
        Cursor cursor = SqliteWrapper.query(context, resolver, msgUri,
                    SMS_STATUS_PROJECTION, NEW_DELIVERY_SM_CONSTRAINT,
                    null, Sms.DATE);

        if (cursor == null) {
            LogTag.debugD("getSmsNewDeliveryInfo null");
            return null;
        }

        try {
            if (!cursor.moveToLast()) {
                LogTag.debugD("getSmsNewDeliveryInfo moveToLast null");
                return null;
            }

            String address = cursor.getString(COLUMN_SMS_ADDRESS);
            LogTag.debugD("getSmsNewDeliveryInfo moveToLast address:" + address);
            long timeMillis = 3000;

            Contact contact = Contact.get(address, false);
            String name = contact.getNameAndNumber();

            return new MmsSmsDeliveryInfo(context.getString(R.string.delivery_toast_body, name),
                timeMillis);

        } finally {
            cursor.close();
        }
    }

    private static final void addSmsNotificationInfos(
            Context context, Set<Long> threads, SortedSet<NotificationInfo> notificationSet) {
        ContentResolver resolver = context.getContentResolver();
        Cursor cursor = SqliteWrapper.query(context, resolver, Sms.CONTENT_URI,
                SMS_STATUS_PROJECTION, NEW_INCOMING_SM_CONSTRAINT,
                null, Sms.DATE + " desc");

        if (cursor == null) {
            return;
        }

        includeEmergencySMS = false;
        try {
            while (cursor.moveToNext()) {
                String address = cursor.getString(COLUMN_SMS_ADDRESS);
                if (MessageUtils.isWapPushNumber(address)) {
                    String[] mAddresses = address.split(":");
                    address = mAddresses[context.getResources().getInteger(
                            R.integer.wap_push_address_index)];
                }
                if (context.getResources().getBoolean(
                            R.bool.config_regional_sms_notification_from_911_disable)) {
                    if (TextUtils.equals(address,emergentcyAddress)) {
                        includeEmergencySMS = true;
                    }
                }
                Contact contact = Contact.get(address, false);
                if (contact.getSendToVoicemail()) {
                    // don't notify, skip this one
                    continue;
                }

                String message = cursor.getString(COLUMN_SMS_BODY);
                long threadId = cursor.getLong(COLUMN_SMS_THREAD_ID);
                long timeMillis = cursor.getLong(COLUMN_SMS_DATE);
                int subId = cursor.getInt(COLUMN_SMS_SUB_ID);
                String msgId = cursor.getString(COLUMN_SMS_ID);

                if (Log.isLoggable(LogTag.APP, Log.VERBOSE))
                {
                    Log.d(TAG, "addSmsNotificationInfos: count=" + cursor.getCount() +
                            ", addr=" + address + ", thread_id=" + threadId);
                }


                NotificationInfo info = getNewMessageNotificationInfo(context, true /* isSms */,
                        address, message, null /* subject */,
                        threadId, subId, timeMillis, null /* attachmentBitmap */,
                        contact, WorkingMessage.TEXT);
                if (MessageUtils.isMailboxMode()) {
                    info.mClickIntent.setData(
                            Uri.withAppendedPath(Sms.CONTENT_URI, msgId));
                }
                notificationSet.add(info);

                threads.add(threadId);
                threads.add(cursor.getLong(COLUMN_SMS_THREAD_ID));
            }
        } finally {
            cursor.close();
        }
    }

    private static final NotificationInfo getNewMessageNotificationInfo(
            Context context,
            boolean isSms,
            String address,
            String message,
            String subject,
            long threadId,
            int subId,
            long timeMillis,
            Bitmap attachmentBitmap,
            Contact contact,
            int attachmentType) {
        Intent clickIntent = getClickIntent(context, isSms, threadId);

        String senderInfo = buildTickerMessage(
                context, address, null, null, subId).toString();
        String senderInfoName = senderInfo.substring(
                0, senderInfo.length());
        CharSequence ticker = buildTickerMessage(
                context, address, subject, message, subId);

        return new NotificationInfo(isSms,
                clickIntent, message, subject, ticker, timeMillis,
                senderInfoName, attachmentBitmap, contact, attachmentType, threadId, subId);
    }

    private static final Intent getClickIntent(Context context, boolean isSms, long threadId) {
        Intent intent;
        if (!MessageUtils.isMailboxMode()) {
            intent = ComposeMessageActivity.createIntent(context, threadId);
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK
                    | Intent.FLAG_ACTIVITY_SINGLE_TOP
                    | Intent.FLAG_ACTIVITY_CLEAR_TOP);
        } else if (isSms) {
            intent = new Intent(context, MailBoxMessageContent.class);
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        } else if (DownloadManager.getInstance().isAuto()) {
            intent = new Intent(context, MobilePaperShowActivity.class);
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        } else {
            // Else case: for MMS not downloaded.
            intent = new Intent(context, MailBoxMessageList.class);
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK
                    | Intent.FLAG_ACTIVITY_SINGLE_TOP
                    | Intent.FLAG_ACTIVITY_CLEAR_TOP);
            intent.putExtra(MessageUtils.MAIL_BOX_ID,
                    MailBoxMessageList.TYPE_INBOX);
        }
        return intent;
    }

    private static final NotificationInfo getNewIccMessageNotificationInfo(
            Context context,
            boolean isSms,
            String address,
            String message,
            String subject,
            int subId,
            long timeMillis,
            Bitmap attachmentBitmap,
            Contact contact,
            int attachmentType) {
        Intent clickIntent = new Intent(context, ManageSimMessages.class);
        clickIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK
                | Intent.FLAG_ACTIVITY_SINGLE_TOP
                | Intent.FLAG_ACTIVITY_CLEAR_TOP);

        clickIntent.putExtra(ConstantsWrapper.Phone.SLOT_KEY, SubscriptionManagerWrapper.getPhoneId(subId));
        String senderInfo = buildTickerMessage(
                context, address, null, null, subId).toString();
        String senderInfoName = senderInfo.substring(
                0, senderInfo.length());
        CharSequence ticker = buildTickerMessage(
                context, address, subject, message, subId);

        return new NotificationInfo(isSms,
                clickIntent, message, subject, ticker, timeMillis,
                senderInfoName, attachmentBitmap, contact, attachmentType, 0, subId);
    }

    public static void cancelNotification(Context context, int notificationId) {
        NotificationManager nm = (NotificationManager) context.getSystemService(
                Context.NOTIFICATION_SERVICE);
        if (DEBUG) {
            Log.d(TAG, "cancelNotification ID:" + notificationId);
        }
        if (NOTIFICATION_ID == notificationId) {
            final String notificationTag = buildNotificationTag(context,
                    NOTIFICATION_MSG_TAG, null);
            nm.cancel(notificationTag, notificationId);
            return;
        }
        nm.cancel(notificationId);
    }

    public static void cancelNotificationItem(Context context, int notificationId,
            final long conversationId, boolean isGroup) {
        if(NOTIFICATION_ID != notificationId){
            return;
        }
        if (DEBUG) {
            Log.d(TAG, "cancelNotificationItem ID:" + conversationId + "|isGroup="+isGroup);
        }
        if(isGroup){
            NotificationManager nm = (NotificationManager) context.getSystemService(
                    Context.NOTIFICATION_SERVICE);
            final String notificationTag = buildNotificationTag(context,
                    NOTIFICATION_MSG_TAG, Long.toString(conversationId));
            nm.cancel(notificationTag, notificationId);
        }else{
            cancelNotification(context,notificationId);
        }
    }

    private static void updateDeliveryNotification(final Context context,
                                                   boolean isStatusMessage,
                                                   final CharSequence message,
                                                   final long timeMillis) {
        if (!isStatusMessage) {
            return;
        }


        if (!MessagingPreferenceActivity.getNotificationEnabled(context)) {
            return;
        }

        sHandler.post(new Runnable() {
            @Override
            public void run() {
                Toast.makeText(context, message, (int)timeMillis).show();
            }
        });
    }

    /**
     * updateNotification is *the* main function for building the actual notification handed to
     * the NotificationManager
     * @param context
     * @param isNew if we've got a new message, show the ticker
     * @param uniqueThreadCount
     * @param notificationSet the set of notifications to display
     */
    private static void updateNotification(
            Context context,
            boolean isNew,
            int uniqueThreadCount,
            SortedSet<NotificationInfo> notificationSet) {
        // If the user has turned off notifications in settings, don't do any notifying.
        if (!MessagingPreferenceActivity.getNotificationEnabled(context)) {
            if (DEBUG) {
                Log.d(TAG, "updateNotification: notifications turned off in prefs, bailing");
            }
            return;
        }

        // Figure out what we've got -- whether all sms's, mms's, or a mixture of both.
        final int messageCount = notificationSet.size();
        NotificationInfo mostRecentNotification = notificationSet.first();

        final Notification.Builder noti = new Notification.Builder(context,NEW_MESSAGE_CHANNEL_ID)
                .setWhen(mostRecentNotification.mTimeMillis)
                .setColor(context.getResources().getColor(R.color.notification_accent_color));

        if (isNew) {
            noti.setTicker(mostRecentNotification.mTicker);
        }

        // If we have more than one unique thread, change the title (which would
        // normally be the contact who sent the message) to a generic one that
        // makes sense for multiple senders, and change the Intent to take the
        // user to the conversation list instead of the specific thread.

        // Cases:
        //   1) single message from single thread - intent goes to ComposeMessageActivity
        //   2) multiple messages from single thread - intent goes to ComposeMessageActivity
        //   3) messages from multiple threads - intent goes to ConversationList

        final Resources res = context.getResources();
        String title = null;
        Bitmap avatar = null;
        PendingIntent pendingIntent = null;
        final int idealIconHeight =
                res.getDimensionPixelSize(android.R.dimen.notification_large_icon_height);
        final int idealIconWidth =
                res.getDimensionPixelSize(android.R.dimen.notification_large_icon_width);
        boolean isMultiNewMessages = MessageUtils.isMailboxMode() ?
                messageCount > 1 : uniqueThreadCount > 1;
        if (isMultiNewMessages) {    // messages from multiple threads
            Intent mainActivityIntent = getMultiThreadsViewIntent(context);
            pendingIntent = PendingIntent.getActivity(context, 0,
                    mainActivityIntent, PendingIntent.FLAG_UPDATE_CURRENT);
            title = context.getString(R.string.message_count_notification, messageCount);
            Drawable backgroundDrawable = context
                    .getResources().getDrawable(R.drawable.notification_multi_icon_background);
            Drawable foregroundDrawable = context
                    .getResources().getDrawable(R.drawable.notification_down_multi);
            Bitmap background = MaterialColorMapUtils
                    .vectobitmap(backgroundDrawable, idealIconWidth);
            Bitmap foreground = MaterialColorMapUtils
                    .vectobitmap(foregroundDrawable, idealIconWidth / 2);
            Bitmap roundBitmap = MaterialColorMapUtils.toConformBitmap(background, foreground);
            noti.setLargeIcon(roundBitmap);
        } else {    // same thread, single or multiple messages
            title = mostRecentNotification.mTitle;
            Contact contact = mostRecentNotification.mSender;
            Drawable contactDrawable = contact.getAvatar(context, null);

            if (contactDrawable != null) {
                // Show the sender's avatar as the big icon. Contact bitmaps are 96x96 so we
                // have to scale 'em up to 128x128 to fill the whole notification large icon.
                if (contactDrawable instanceof RoundedBitmapDrawable) {
                    avatar = ((RoundedBitmapDrawable) contactDrawable).getBitmap();
                }
                if (avatar != null) {
                    if (avatar.getHeight() < idealIconHeight) {
                        // Scale this image to fit the intended size
                        avatar = Bitmap.createScaledBitmap(
                                avatar, idealIconWidth, idealIconHeight, true);
                        avatar = MaterialColorMapUtils.getCircularBitmap(context, avatar);
                    }
                    if (avatar != null) {
                        noti.setLargeIcon(avatar);
                    }
                }
            } else if (LetterTileDrawable.isEnglishLetterString(contact.getNameForAvatar())) {
                Bitmap newAvatar = MaterialColorMapUtils.getLetterTitleDraw(context,
                        contact, idealIconWidth);
                Bitmap roundBitmap = MaterialColorMapUtils.getCircularBitmap(context, newAvatar);
                noti.setLargeIcon(roundBitmap);
                avatar = roundBitmap;
            } else {
                Bitmap newAvatar = MaterialColorMapUtils.getLetterTitleDraw(context,
                        contact, idealIconWidth);
                Bitmap background = MaterialColorMapUtils.getCircularBitmap(context, newAvatar);
                Drawable foregroundDrawable =
                        context.getResources().getDrawable(R.drawable.myavatar);
                Bitmap foreground = MaterialColorMapUtils
                        .vectobitmap(foregroundDrawable, idealIconWidth / 2);
                Bitmap roundBitmap = MaterialColorMapUtils.toConformBitmap(background, foreground);
                noti.setLargeIcon(roundBitmap);
                avatar = roundBitmap;
            }

            pendingIntent = PendingIntent.getActivity(context, 0,
                    mostRecentNotification.mClickIntent, PendingIntent.FLAG_UPDATE_CURRENT);
        }

        Intent mActivityIntent = getMultiThreadsViewIntent(context);
        PendingIntent mPendingIntent = PendingIntent.getActivity(context, 0,
                mActivityIntent, PendingIntent.FLAG_UPDATE_CURRENT);
        // Always have to set the small icon or the notification is ignored
        noti.setSmallIcon(R.drawable.ic_notification);

        NotificationManager nm = (NotificationManager)
                context.getSystemService(Context.NOTIFICATION_SERVICE);

        NotificationChannel channel = new NotificationChannel(NEW_MESSAGE_CHANNEL_ID,
                context.getString(R.string.message_notification),
                NotificationManager.IMPORTANCE_HIGH);
        nm.createNotificationChannel(channel);

        // Update the notification.
        noti.setContentTitle(title)
            .setContentIntent(pendingIntent)
            .setCategory(Notification.CATEGORY_MESSAGE)
            .setPriority(Notification.PRIORITY_HIGH);     // TODO: set based on contact coming
                                                             // from a favorite.

        // Tag notification with all senders.
        for (NotificationInfo info : notificationSet) {
            Uri peopleReferenceUri = info.mSender.getPeopleReferenceUri();
            if (peopleReferenceUri != null) {
                noti.addPerson(peopleReferenceUri.toString());
            }
        }

        int defaults = 0;

        if (isNew) {
            SharedPreferences sp = PreferenceManager.getDefaultSharedPreferences(context);

            boolean vibrate = false;
            if (sp.contains(MessagingPreferenceActivity.NOTIFICATION_VIBRATE)) {
                // The most recent change to the vibrate preference is to store a boolean
                // value in NOTIFICATION_VIBRATE. If prefs contain that preference, use that
                // first.
                vibrate = sp.getBoolean(MessagingPreferenceActivity.NOTIFICATION_VIBRATE,
                        false);
            } else if (sp.contains(MessagingPreferenceActivity.NOTIFICATION_VIBRATE_WHEN)) {
                // This is to support the pre-JellyBean MR1.1 version of vibrate preferences
                // when vibrate was a tri-state setting. As soon as the user opens the Messaging
                // app's settings, it will migrate this setting from NOTIFICATION_VIBRATE_WHEN
                // to the boolean value stored in NOTIFICATION_VIBRATE.
                String vibrateWhen =
                        sp.getString(MessagingPreferenceActivity.NOTIFICATION_VIBRATE_WHEN, null);
                vibrate = "always".equals(vibrateWhen);
            }
            if (vibrate) {
                defaults |= Notification.DEFAULT_VIBRATE;
            }

            String ringtoneStr = sp.getString(MessagingPreferenceActivity.NOTIFICATION_RINGTONE,
                    null);
            if (!includeEmergencySMS) {
                TelephonyManager tm = (TelephonyManager)context.getSystemService(
                        Context.TELEPHONY_SERVICE);
                int state = tm.getCallState();
                if (state != TelephonyManager.CALL_STATE_IDLE) {
                     noti.setSound(TextUtils.isEmpty(ringtoneStr) ? null : Uri.parse(ringtoneStr),
                             AUDIO_ATTRIBUTES_ALARM);
                } else {
                    noti.setSound(TextUtils.isEmpty(ringtoneStr) ? null : Uri.parse(ringtoneStr));
                }
            }
            Log.d(TAG, "updateNotification: new message, adding sound to the notification");
        }

        defaults |= Notification.DEFAULT_LIGHTS;

        noti.setDefaults(defaults);

        // set up delete intent
        noti.setDeleteIntent(PendingIntent.getBroadcast(context, 0,
                sNotificationOnDeleteIntent, 0));

        final Notification notification;

        if (messageCount == 1) {
            // We've got a single message
            String bigMessage = mostRecentNotification.formatBigMessage(context).toString();
            String pictureMessage = mostRecentNotification.formatPictureMessage(context).toString();
            if (!mostRecentNotification.mIsSms && !DownloadManager.getInstance().isAuto()) {
                Cursor cursor = null;
                try {
                    cursor = SqliteWrapper.query(context, context.getContentResolver(),
                            Mms.Inbox.CONTENT_URI, new String[] {
                            Mms._ID, Mms.THREAD_ID, Mms.SUBSCRIPTION_ID, Mms.MESSAGE_TYPE},
                            Mms.THREAD_ID + "=" + mostRecentNotification.mThreadId,
                            null, null);
                    if (cursor != null && cursor.getCount() != 0) {
                        cursor.moveToFirst();
                        long subId = cursor.getLong(
                                cursor.getColumnIndexOrThrow(Mms.SUBSCRIPTION_ID));
                        long msgId = cursor.getLong(
                                cursor.getColumnIndexOrThrow(Mms._ID));
                        long msgType = cursor.getLong(
                                cursor.getColumnIndexOrThrow(Mms.MESSAGE_TYPE));
                        addNotificationReplyAction(noti, context, mostRecentNotification,
                                false, null);

                        if (msgType == PduHeaders.MESSAGE_TYPE_NOTIFICATION_IND) {
                            // Create "Download" option in notification
                            CharSequence downloadText = context.getText(
                                    R.string.notification_download);
                            Intent downloadIntent = new Intent(context,
                                    NotificationActionHandleReceiver.class);
                            downloadIntent.setAction(
                                    NotificationActionHandleReceiver.ACTION_NOTIFICATION_DOWNLOAD);
                            downloadIntent.putExtra(
                                    NotificationActionHandleReceiver.MSG_SUB_ID, subId);
                            downloadIntent.putExtra(
                                    NotificationActionHandleReceiver.MSG_ID, msgId);
                            PendingIntent downloadPendingIntent = PendingIntent.getBroadcast(
                                    context, 0, downloadIntent, PendingIntent.FLAG_UPDATE_CURRENT);

                            noti.addAction(R.drawable.notification_download,
                                    downloadText, downloadPendingIntent);

                            bigMessage = context.getResources().getString(
                                    R.string.new_mms_download);
                            pictureMessage = context.getResources().getString(
                                    R.string.new_mms_download);
                        }
                    }
                } finally {
                    if (cursor != null) {
                        cursor.close();
                    }
                }
            } else {
                addNotificationReplyAction(noti, context, mostRecentNotification, false, null);
            }

            // This sets the text for the collapsed form:
            noti.setContentText(bigMessage);

            if (mostRecentNotification.mAttachmentBitmap != null) {
                // The message has a picture, show that

                noti.setLargeIcon(mostRecentNotification.mAttachmentBitmap);
                notification = new Notification.BigPictureStyle(noti)
                    .bigPicture(mostRecentNotification.mAttachmentBitmap)
                    .bigLargeIcon(avatar)
                    // This sets the text for the expanded picture form:
                    .setSummaryText(pictureMessage)
                    .build();
            } else {
                // Show a single notification -- big style with the text of the whole message
                notification = new Notification.BigTextStyle(noti)
                    .bigText(bigMessage)
                    .build();
            }
            if (DEBUG) {
                Log.d(TAG, "updateNotification: single message notification");
            }
        } else {
            // We've got multiple messages
            if (!isMultiNewMessages) {
                // We've got multiple messages for the same thread.
                // Starting with the oldest new message, display the full text of each message.
                // Begin a line for each subsequent message.
                SpannableStringBuilder buf = new SpannableStringBuilder();
                NotificationInfo infos[] =
                        notificationSet.toArray(new NotificationInfo[messageCount]);
                int len = infos.length;
                for (int i = len - 1; i >= 0; i--) {
                    NotificationInfo info = infos[i];

                    buf.append(info.formatBigMessage(context));

                    if (i != 0) {
                        buf.append('\n');
                    }
                }

                noti.setContentText(context.getString(R.string.message_count_notification,
                        messageCount));
                addNotificationReplyAction(noti, context, mostRecentNotification, false, null);

                // Show a single notification -- big style with the text of all the messages
                notification = new Notification.BigTextStyle(noti)
                    .bigText(buf)
                    // Forcibly show the last line, with the app's smallIcon in it, if we
                    // kicked the smallIcon out with an avatar bitmap
                    .setSummaryText((avatar == null) ? null : " ")
                    .build();
                if (DEBUG) {
                    Log.d(TAG, "updateNotification: multi messages for single thread");
                }
            } else {
                // Build a set of the most recent notification per threadId.
                HashSet<Long> uniqueThreads = new HashSet<Long>(messageCount);
                ArrayList<NotificationInfo> mostRecentNotifPerThread =
                        new ArrayList<NotificationInfo>();
                Iterator<NotificationInfo> notifications = notificationSet.iterator();
                while (notifications.hasNext()) {
                    NotificationInfo notificationInfo = notifications.next();
                    if (!uniqueThreads.contains(notificationInfo.mThreadId)) {
                        uniqueThreads.add(notificationInfo.mThreadId);
                        mostRecentNotifPerThread.add(notificationInfo);
                    }
                }
                uniqueThreads.clear();

                // When collapsed, show all the senders like this:
                //     Fred Flinstone, Barry Manilow, Pete...
                noti.setContentText(formatSenders(context, mostRecentNotifPerThread));
                noti.setGroup(NOTIFICATION_GROUP_KEY);
                noti.setGroupSummary(true);
                Notification.InboxStyle inboxStyle = new Notification.InboxStyle(noti);

                // We have to set the summary text to non-empty so the content text doesn't show
                // up when expanded.
                inboxStyle.setSummaryText(" ");

                // At this point we've got multiple messages in multiple threads. We only
                // want to show the most recent message per thread, which are in
                // mostRecentNotifPerThread.
                int uniqueThreadMessageCount = mostRecentNotifPerThread.size();
                int maxMessages = Math.min(MAX_MESSAGES_TO_SHOW, uniqueThreadMessageCount);

                for (int i = 0; i < maxMessages; i++) {
                    NotificationInfo info = mostRecentNotifPerThread.get(i);
                    inboxStyle.addLine(info.formatInboxMessage(context));
                }
                notification = inboxStyle.build();

                //1. send notification for every conversation.
                for (int j = 0; j < uniqueThreadMessageCount; j++) {
                    int index = uniqueThreadMessageCount - j - 1;
                    if (index < 0) {
                        break;
                    }
                    final String sortKey = String.format(Locale.US, "%02d", index);
                    Long theadIdforNoti = mostRecentNotifPerThread.get(index).mThreadId;
                    SortedSet<NotificationInfo> notiSet =
                            new TreeSet<NotificationInfo>(notificationSet);
                    Iterator<NotificationInfo> temoNoti = notiSet.iterator();
                    while (temoNoti.hasNext()) {
                        NotificationInfo tempInfo = temoNoti.next();
                        if (tempInfo.mThreadId != theadIdforNoti) {
                            temoNoti.remove();
                        }
                    }
                    createMultiConvNotification(context, theadIdforNoti, sortKey, isNew, notiSet);
                    notiSet.clear();
                }
                mostRecentNotifPerThread.clear();

                //2. send group summary notification
                notifyUserIfFullScreen(context, title);
                doNotify(notification, context);

                if (DEBUG) {
                    Log.d(TAG, "updateNotification: multi messages," +
                            " showing inboxStyle notification");
                }
                //3. Done
                return;
            }
        }

        notifyUserIfFullScreen(context, title);
        doNotify(notification, context);
    }


    /**
     * createMultiConvNotification is used for create bundled notification
     * for multi conversation.
     *
     * @param context
     * @param threadId        conversation thread id
     * @param sortkey         key to sort notifications
     * @param isNew           if we've got a new message
     * @param notificationSet the set of notifications to display
     */
    private static void createMultiConvNotification(
            Context context,
            final long threadId,
            final String sortkey,
            boolean isNew,
            SortedSet<NotificationInfo> notificationSet) {

        // Figure out what we've got -- whether all sms's, mms's, or a mixture of both.
        final int messageCount = notificationSet.size();
        NotificationInfo mostRecentNotification = notificationSet.first();
        PendingIntent pendingIntent = PendingIntent.getActivity(context, 0,
                mostRecentNotification.mClickIntent, PendingIntent.FLAG_UPDATE_CURRENT);
        final Notification.Builder noti = new Notification.Builder(context, NEW_MESSAGE_CHANNEL_ID)
                .setWhen(mostRecentNotification.mTimeMillis)
                .setColor(context.getResources().getColor(R.color.notification_accent_color))
                .setContentTitle(mostRecentNotification.mTitle)
                .setContentIntent(pendingIntent)
                .setDeleteIntent(PendingIntent.getBroadcast(context, 0,
                        sNotificationOnDeleteIntent, 0))
                .setSmallIcon(R.drawable.ic_notification)
                .setCategory(Notification.CATEGORY_MESSAGE)
                .setPriority(Notification.PRIORITY_HIGH)
                .setGroup(NOTIFICATION_GROUP_KEY)
                .setSortKey(sortkey);
        if (DEBUG) {
            Log.d(TAG, "createMultiConvNotification: messageCount =" + messageCount
                    + "|threadId=" + mostRecentNotification.mThreadId +
                    "|sender name=" + mostRecentNotification.mSender.getName() +
                    "|Sortkey =" + sortkey);
        }

        final Resources res = context.getResources();
        Bitmap avatar = buildNotificationAvatar(noti, context, mostRecentNotification.mSender);

        // Tag notification with  senders
        Uri peopleReferenceUri = mostRecentNotification.mSender.getPeopleReferenceUri();
        if (peopleReferenceUri != null) {
            noti.addPerson(peopleReferenceUri.toString());
        }

        final Notification notification;

        if (messageCount == 1) {
            String bigMessage = mostRecentNotification.formatBigMessage(context).toString();
            String pictureMessage = mostRecentNotification.formatPictureMessage(context).toString();
            if (!mostRecentNotification.mIsSms && !DownloadManager.getInstance().isAuto()) {
                Cursor cursor = null;
                try {
                    cursor = SqliteWrapper.query(context, context.getContentResolver(),
                            Mms.Inbox.CONTENT_URI, new String[]{
                                    Mms._ID, Mms.THREAD_ID, Mms.SUBSCRIPTION_ID, Mms.MESSAGE_TYPE},
                            Mms.THREAD_ID + "=" + mostRecentNotification.mThreadId,
                            null, null);
                    if (cursor != null && cursor.getCount() != 0) {
                        cursor.moveToFirst();
                        long subId = cursor.getLong(
                                cursor.getColumnIndexOrThrow(Mms.SUBSCRIPTION_ID));
                        long msgId = cursor.getLong(
                                cursor.getColumnIndexOrThrow(Mms._ID));
                        long msgType = cursor.getLong(
                                cursor.getColumnIndexOrThrow(Mms.MESSAGE_TYPE));
                        addNotificationReplyAction(noti, context, mostRecentNotification,
                                true, sortkey);

                        if (msgType == PduHeaders.MESSAGE_TYPE_NOTIFICATION_IND) {
                            // Create "Download" option in notification
                            CharSequence downloadText = context.getText(
                                    R.string.notification_download);
                            Intent downloadIntent = new Intent(context,
                                    NotificationActionHandleReceiver.class);
                            Uri msgUri = Mms.CONTENT_URI.buildUpon().appendPath(
                                    Long.toString(mostRecentNotification.mThreadId)).build();
                            downloadIntent.setAction(
                                    NotificationActionHandleReceiver.ACTION_NOTIFICATION_DOWNLOAD);
                            downloadIntent.putExtra(
                                    NotificationActionHandleReceiver.MSG_SUB_ID, subId);
                            downloadIntent.putExtra(
                                    NotificationActionHandleReceiver.MSG_ID, msgId);
                            downloadIntent.setData(msgUri);
                            PendingIntent downloadPendingIntent = PendingIntent.getBroadcast(
                                    context, 0, downloadIntent, PendingIntent.FLAG_UPDATE_CURRENT);

                            noti.addAction(R.drawable.notification_download,
                                    downloadText, downloadPendingIntent);

                            bigMessage = context.getResources().getString(
                                    R.string.new_mms_download);
                            pictureMessage = context.getResources().getString(
                                    R.string.new_mms_download);
                        }
                    }
                } finally {
                    if (cursor != null) {
                        cursor.close();
                    }
                }
            } else {
                addNotificationReplyAction(noti, context, mostRecentNotification, true, sortkey);
            }

            // This sets the text for the collapsed form:
            noti.setContentText(bigMessage);

            if (mostRecentNotification.mAttachmentBitmap != null) {
                // The message has a picture, show that
                noti.setLargeIcon(mostRecentNotification.mAttachmentBitmap);
                notification = new Notification.BigPictureStyle(noti)
                        .bigPicture(mostRecentNotification.mAttachmentBitmap)
                        .bigLargeIcon(avatar)
                        // This sets the text for the expanded picture form:
                        .setSummaryText(pictureMessage)
                        .build();
            } else {
                // Show a single notification -- big style with the text of the whole message
                notification = new Notification.BigTextStyle(noti)
                        .bigText(bigMessage)
                        .build();
            }
        } else {
            // We've got multiple messages for the same thread.
            // Starting with the oldest new message, display the full text of each message.
            // Begin a line for each subsequent message.
            SpannableStringBuilder buf = new SpannableStringBuilder();
            NotificationInfo infos[] =
                    notificationSet.toArray(new NotificationInfo[messageCount]);
            int len = infos.length;
            for (int i = len - 1; i >= 0; i--) {
                NotificationInfo info = infos[i];

                buf.append(info.formatBigMessage(context));

                if (i != 0) {
                    buf.append('\n');
                }
            }

            noti.setContentText(context.getString(R.string.message_count_notification,
                    messageCount));
            addNotificationReplyAction(noti, context, mostRecentNotification, true, sortkey);

            // Show a single notification -- big style with the text of all the messages
            notification = new Notification.BigTextStyle(noti)
                    .bigText(buf)
                    // Forcibly show the last line, with the app's smallIcon in it, if we
                    // kicked the smallIcon out with an avatar bitmap
                    .setSummaryText((avatar == null) ? null : " ")
                    .build();
        }

        notification.flags |= Notification.FLAG_AUTO_CANCEL;
        doNotify(notification, context, threadId);
    }

    /**
     * Update notification item after direct replied.
     * @param context
     * @param threadId conversation thread id
     * @param name     sender's name
     * @param address  sender's number
     * @param replyMsg direct replied message
     * @param isGroup  whether it is group item
     * @param sortkey  sortkey for group item
     */
    public static void updateNotificationItem(
            Context context,
            final long threadId,
            String name,
            String address,
            String replyMsg,
            boolean isGroup,
            final String sortkey) {
        Intent clickIntent = getClickIntent(context, true, threadId);
        PendingIntent pendingIntent = PendingIntent.getActivity(context, 0,
                clickIntent, PendingIntent.FLAG_UPDATE_CURRENT);

        final Notification.Builder noti = new Notification.Builder(context,
                NEW_MESSAGE_CHANNEL_ID)
                .setWhen(System.currentTimeMillis())
                .setColor(context.getResources().getColor(R.color.notification_accent_color))
                .setContentTitle(name)
                .setContentIntent(pendingIntent)
                .setSmallIcon(R.drawable.ic_notification)
                .setCategory(Notification.CATEGORY_MESSAGE)
                .setPriority(Notification.PRIORITY_HIGH);

        if (DEBUG) {
            Log.d(TAG, "updateNotificationItem: name =" + name
                    + "|threadId=" + threadId + "|address=" + address
                    + "|isGroup=" + isGroup + "|sortkey=" + sortkey);
        }
        if (isGroup) {
            noti.setGroup(NOTIFICATION_GROUP_KEY);
            if(null != sortkey) {
                noti.setSortKey(sortkey);
            }
        }

        final Resources res = context.getResources();
        //contact.get may cost sometime
        Contact contact = Contact.get(address, false);
        buildNotificationAvatar(noti, context, contact);

        Uri peopleReferenceUri = contact.getPeopleReferenceUri();
        if (peopleReferenceUri != null) {
            noti.addPerson(peopleReferenceUri.toString());
        }

        final Notification notification;
        StringBuilder replyString = new StringBuilder(res.getString(R.string.you));
        replyString.append(" ").append(replyMsg);
        noti.setContentText(replyString.toString());
        notification = new Notification.BigTextStyle(noti)
                .bigText(replyString)
                .build();

        notification.flags |= Notification.FLAG_AUTO_CANCEL;
        if (isGroup) {
            doNotify(notification, context, threadId);
        } else {
            doNotify(notification, context);
        }

    }

    private static void doNotify(final Notification notification,
            Context context, final long threadId) {
        String threadIdStr = Long.toString(threadId);
        NotificationManager nm = (NotificationManager) context
                .getSystemService(Context.NOTIFICATION_SERVICE);
        final String notificationTag = buildNotificationTag(context,
                NOTIFICATION_MSG_TAG, threadIdStr);
        NotificationChannel channel = new NotificationChannel(
                NEW_MESSAGE_CHANNEL_ID,
                context.getString(R.string.message_notification),
                NotificationManager.IMPORTANCE_HIGH);
        nm.createNotificationChannel(channel);
        nm.notify(notificationTag, NOTIFICATION_ID, notification);
    }

    private static void doNotify(final Notification notification,
            Context context) {
        NotificationManager nm = (NotificationManager) context
                .getSystemService(Context.NOTIFICATION_SERVICE);
        final String notificationTag = buildNotificationTag(context,
                NOTIFICATION_MSG_TAG, null);
        NotificationChannel channel = new NotificationChannel(
                NEW_MESSAGE_CHANNEL_ID,
                context.getString(R.string.message_notification),
                NotificationManager.IMPORTANCE_HIGH);
        nm.createNotificationChannel(channel);
        nm.notify(notificationTag, NOTIFICATION_ID, notification);
    }

    private static void addNotificationReplyAction(final Notification.Builder notiBuilder,
            Context context, NotificationInfo notificationInfo,
            boolean isGroup, final String sortkey) {
        if (null == notiBuilder || null == notificationInfo) {
            return;
        }
        CharSequence replyText = context.getText(R.string.notification_reply);
        CharSequence typeMessageText = context.getText(R.string.notification_type_message);
        Intent replyIntent = new Intent(context, NotificationQuickReplyActivity.class);
        replyIntent.putExtra(NotificationQuickReplyActivity.MSG_THREAD_ID,
                notificationInfo.mThreadId);
        Uri msgUri = Mms.CONTENT_URI.buildUpon().appendPath(
                Long.toString(notificationInfo.mThreadId)).build();
        replyIntent.setData(msgUri);
        replyIntent.putExtra(NotificationQuickReplyActivity.MSG_SENDER,
                notificationInfo.mSender.getName());
        replyIntent.putExtra(NotificationQuickReplyActivity.MSG_SUB_ID,
                notificationInfo.mSubId);
        replyIntent.putExtra(NotificationQuickReplyActivity.MSG_ADDRESS,
                notificationInfo.mSender.getNumber());
        replyIntent.putExtra(NotificationQuickReplyActivity.KEY_IS_GROUP,
                isGroup);
        replyIntent.putExtra(NotificationQuickReplyActivity.KEY_SORT,
                sortkey);

        PendingIntent replyPendingIntent = PendingIntent.getActivity(context, 0,
                replyIntent, PendingIntent.FLAG_UPDATE_CURRENT);
        RemoteInput remoteInput = new RemoteInput.Builder(
                NotificationQuickReplyActivity.KEY_TEXT_REPLY)
                .setLabel(typeMessageText)
                .build();
        Notification.Action actionReply = new Notification.Action.Builder(
                R.drawable.notification_reply,
                replyText,
                replyPendingIntent)
                .addRemoteInput(remoteInput)
                .build();
        notiBuilder.addAction(actionReply);
    }

    private static Bitmap buildNotificationAvatar(final Notification.Builder notiBuilder,
            Context context, Contact contact) {
        if (null == contact || null == notiBuilder) {
            return null;
        }
        final Resources res = context.getResources();
        Bitmap avatar = null;
        final int idealIconHeight =
                res.getDimensionPixelSize(android.R.dimen.notification_large_icon_height);
        final int idealIconWidth =
                res.getDimensionPixelSize(android.R.dimen.notification_large_icon_width);

        Drawable contactDrawable = contact.getAvatar(context, null);

        if (contactDrawable != null) {
            // Show the sender's avatar as the big icon. Contact bitmaps are 96x96 so we
            // have to scale 'em up to 128x128 to fill the whole notification large icon.
            if (contactDrawable instanceof RoundedBitmapDrawable) {
                avatar = ((RoundedBitmapDrawable) contactDrawable).getBitmap();
            }
            if (avatar != null) {
                if (avatar.getHeight() < idealIconHeight) {
                    // Scale this image to fit the intended size
                    avatar = Bitmap.createScaledBitmap(
                            avatar, idealIconWidth, idealIconHeight, true);
                    avatar = MaterialColorMapUtils.getCircularBitmap(context, avatar);
                }
                if (avatar != null) {
                    notiBuilder.setLargeIcon(avatar);
                }
            }
        } else if (LetterTileDrawable.isEnglishLetterString(contact.getNameForAvatar())) {
            Bitmap newAvatar = MaterialColorMapUtils.getLetterTitleDraw(context,
                    contact, idealIconWidth);
            Bitmap roundBitmap = MaterialColorMapUtils.getCircularBitmap(context, newAvatar);
            notiBuilder.setLargeIcon(roundBitmap);
            avatar = roundBitmap;
        } else {
            Bitmap newAvatar = MaterialColorMapUtils.getLetterTitleDraw(context,
                    contact, idealIconWidth);
            Bitmap background = MaterialColorMapUtils.getCircularBitmap(context, newAvatar);
            Drawable foregroundDrawable =
                    context.getResources().getDrawable(R.drawable.myavatar);
            Bitmap foreground = MaterialColorMapUtils
                    .vectobitmap(foregroundDrawable, idealIconWidth / 2);
            Bitmap roundBitmap = MaterialColorMapUtils.toConformBitmap(background, foreground);
            notiBuilder.setLargeIcon(roundBitmap);
            avatar = roundBitmap;
        }
        return avatar;
    }


    /**
     * Returns a unique tag to identify a notification.
     *
     * @param name           The tag name (in practice, the type)
     * @param conversationId The conversation id (optional)
     */
    private static String buildNotificationTag(
            final Context context, final String name, final String conversationId) {
        if (conversationId != null) {
            return context.getPackageName() + name + ":" + conversationId;
        } else {
            return context.getPackageName() + name;
        }
    }

    protected static CharSequence buildTickerMessage(
            Context context, String address, String subject, String body, int subId) {
        String displayAddress = Contact.get(address, true).getName();

        StringBuilder buf = new StringBuilder(
                displayAddress == null
                ? ""
                : displayAddress.replace('\n', ' ').replace('\r', ' '));
        buf.append(' ');

        if ((MessageUtils.getPhoneCount()) > 1 &&
                MessageUtils.isMsimIccCardActive()) {
            SubscriptionInfo sir = SubscriptionManager.from(context)
                    .getActiveSubscriptionInfo(subId);
            int slotNumber = (sir != null) ? (sir.getSimSlotIndex() + 1) : 0;

            if (DEBUG) {
                Log.d(TAG, "buildTickerMessage subId: " + subId + " slotNumber " + slotNumber);
            }
            if (slotNumber > 0) {
                String dividerStr = context.getResources().getString(
                        R.string.notification_sim_divider);
                String strSim = context.getResources().getString(R.string.notification_sim_info);
                buf.append(dividerStr).append(strSim).append(slotNumber + "");
            }
            //SMS/MMS is operating based of PhoneId which is 0, 1..
            if (!TextUtils.isEmpty(subject) || !TextUtils.isEmpty(body)) {
                buf.append("-");
            }
        }

        int offset = buf.length();
        if (!TextUtils.isEmpty(subject)) {
            subject = subject.replace('\n', ' ').replace('\r', ' ');
            buf.append(subject);
            buf.append(' ');
        }

        if (!TextUtils.isEmpty(body)) {
            body = body.replace('\n', ' ').replace('\r', ' ');
            buf.append(body);
        }

        SpannableString spanText = new SpannableString(buf.toString());
        spanText.setSpan(new StyleSpan(Typeface.BOLD), 0, offset,
                Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);

        return spanText;
    }

    private static String getMmsSubject(String sub, int charset) {
        return TextUtils.isEmpty(sub) ? ""
                : new EncodedStringValue(charset, PduPersister.getBytes(sub)).getString();
    }

    public static void notifyDownloadFailed(Context context, long threadId) {
        notifyFailed(context, true, threadId, false);
    }

    public static void notifySendFailed(Context context) {
        notifyFailed(context, false, 0, false);
    }

    public static void notifySendFailed(Context context, boolean noisy) {
        notifyFailed(context, false, 0, noisy);
    }

    private static void notifyFailed(Context context, boolean isDownload, long threadId,
                                     boolean noisy) {
        // TODO factor out common code for creating notifications
        boolean enabled = MessagingPreferenceActivity.getNotificationEnabled(context);
        if (!enabled) {
            return;
        }

        // Strategy:
        // a. If there is a single failure notification, tapping on the notification goes
        //    to the compose view.
        // b. If there are two failure it stays in the thread view. Selecting one undelivered
        //    thread will dismiss one undelivered notification but will still display the
        //    notification.If you select the 2nd undelivered one it will dismiss the notification.
        int totalFailedCount = getUndeliveredMessageCount(context);

        Intent failedIntent;
        final Notification.Builder noti = new Notification.Builder(context,
                NEW_MESSAGE_CHANNEL_ID);
        Notification notification = new Notification.BigTextStyle(noti)
                .build();

        String title;
        String description;
        if (totalFailedCount > 1) {
            description = context.getString(R.string.notification_failed_multiple,
                    Integer.toString(totalFailedCount));
            title = context.getString(R.string.notification_failed_multiple_title);
        } else {
            title = isDownload ?
                        context.getString(R.string.message_download_failed_title) :
                        context.getString(R.string.message_send_failed_title);

            description = context.getString(R.string.message_failed_body);
        }

        TaskStackBuilder taskStackBuilder = TaskStackBuilder.create(context);
        // Get failed intent by folder mode or conversation mode.
        if (MessageUtils.isMailboxMode()) {
            failedIntent = getFailedIntentFromFolderMode(context, totalFailedCount, isDownload);
            if (failedIntent == null) {
                return;
            } else if (isDownload) {
                // When isDownload is true, the valid threadId is passed into this function.
                failedIntent.putExtra(FAILED_DOWNLOAD_FLAG, true);
            } else {
                failedIntent.putExtra(UNDELIVERED_FLAG, true);
            }
        } else {
            failedIntent = getFailedIntentFromConversationMode(context,
                    isDownload, threadId);
        }

        taskStackBuilder.addNextIntent(failedIntent);

        notification.icon = R.drawable.notification_icon_failed;

        notification.tickerText = title;

        notification.setLatestEventInfo(context, title, description,
                taskStackBuilder.getPendingIntent(0,  PendingIntent.FLAG_UPDATE_CURRENT));

        if (noisy) {
            SharedPreferences sp = PreferenceManager.getDefaultSharedPreferences(context);
            boolean vibrate = sp.getBoolean(MessagingPreferenceActivity.NOTIFICATION_VIBRATE,
                    false /* don't vibrate by default */);
            if (vibrate) {
                notification.defaults |= Notification.DEFAULT_VIBRATE;
            }

            String ringtoneStr = sp.getString(MessagingPreferenceActivity.NOTIFICATION_RINGTONE,
                    null);
            notification.sound = TextUtils.isEmpty(ringtoneStr) ? null : Uri.parse(ringtoneStr);
        }

        NotificationManager notificationMgr = (NotificationManager)
                context.getSystemService(Context.NOTIFICATION_SERVICE);
        NotificationChannel channel = new NotificationChannel(
                NEW_MESSAGE_CHANNEL_ID,
                context.getString(R.string.message_notification),
                NotificationManager.IMPORTANCE_HIGH);
        notificationMgr.createNotificationChannel(channel);

        if (isDownload) {
            notificationMgr.notify(DOWNLOAD_FAILED_NOTIFICATION_ID, notification);
        } else {
            notificationMgr.notify(MESSAGE_FAILED_NOTIFICATION_ID, notification);
        }
    }

    /**
     * Return the pending intent for failed messages in conversation mode.
     * @param context The context
     * @param isDownload Whether the message is failed to download
     * @param threadId The thread if of the  message failed to download
     */
    private static Intent getFailedIntentFromConversationMode(
        Context context, boolean isDownload, long threadId) {
        Cursor cursor = SqliteWrapper.query(context, context.getContentResolver(),
                UNDELIVERED_URI, MMS_THREAD_ID_PROJECTION, "read=0", null, null);
        if (cursor == null) {
            return null;
        }
        try {
            Intent failedIntent;
            if (isFailedMessagesInSameThread(cursor)) {
                failedIntent = new Intent(context, ComposeMessageActivity.class);
                if (isDownload) {
                    // When isDownload is true, the valid threadId is passed into this function.
                    failedIntent.putExtra(FAILED_DOWNLOAD_FLAG, true);
                    failedIntent.putExtra(ComposeMessageActivity.THREAD_ID, threadId);
                } else {
                    // For send failed case, get the thread id from the cursor.
                    failedIntent.putExtra(UNDELIVERED_FLAG, true);
                    failedIntent.putExtra(ComposeMessageActivity.THREAD_ID,
                            getUndeliveredMessageThreadId(cursor));
                }
            } else {
                failedIntent = new Intent(context, ConversationList.class);
            }
            return failedIntent;
        } finally {
            cursor.close();
        }
    }

    /**
     * Return the pending intent for failed messages in folder mode.
     * @param context The context
     * @param failedCount The failed messages' count
     * @param isDownload Whether the messages is for received
     */
    private static Intent getFailedIntentFromFolderMode(Context context,
            int failedCount, boolean isDownload) {
        // Query the DB and return the cursor of the  undelivered messages
        Cursor cursor = SqliteWrapper.query(context, context.getContentResolver(),
                UNDELIVERED_URI, MAILBOX_PROJECTION, "read=0", null, null);
        if (cursor == null) {
            return null;
        }

        try {
            int mailboxId = MailBoxMessageList.TYPE_INVALID;
            Intent failedIntent;

            if (failedCount > 1) {
                if (isFailedMessagesInSameBox(cursor)) {
                    mailboxId = getUndeliveredMessageBoxId(cursor);
                } else {
                    mailboxId = MailBoxMessageList.TYPE_INBOX;
                }

                failedIntent = new Intent(context, MailBoxMessageList.class);
                failedIntent.putExtra(MessageUtils.MAIL_BOX_ID,
                        mailboxId);
                failedIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                return failedIntent;
            }

            // The rest cases: the "failedCount" is 1.
            Uri msgUri;
            String type = getUndeliveredMessageType(cursor);
            Long msgId = getUndeliveredMessageId(cursor);
            if (TextUtils.isEmpty(type)) {
                return null;
            }
            if (type.equals("sms")) {
                failedIntent = new Intent(context, MailBoxMessageContent.class);
                msgUri = Uri.withAppendedPath(Sms.CONTENT_URI, String.valueOf(msgId));
                failedIntent.setData(msgUri);
            } else {
                // MMS type.
                if (isDownload) {
                    //  Download fail will jump to MailBoxMessageList INBOX.
                    failedIntent = new Intent(context, MailBoxMessageList.class);
                    mailboxId = MailBoxMessageList.TYPE_INBOX;
                    failedIntent.putExtra(MessageUtils.MAIL_BOX_ID, mailboxId);
                } else {
                    failedIntent = new Intent(context, MobilePaperShowActivity.class);
                    msgUri = Uri.withAppendedPath(Mms.CONTENT_URI, String.valueOf(msgId));
                    failedIntent.setData(msgUri);
                }
            }
            failedIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

            return failedIntent;
        } finally {
            cursor.close();
        }
    }

    // Query the DB and return the number of undelivered messages (total for both SMS and MMS)
    private static int getUndeliveredMessageCount(Context context) {
        Cursor undeliveredCursor = SqliteWrapper.query(context, context.getContentResolver(),
                UNDELIVERED_URI, MMS_THREAD_ID_PROJECTION, "read=0", null, null);
        if (undeliveredCursor == null) {
            return 0;
        }
        int count = undeliveredCursor.getCount();
        undeliveredCursor.close();
        return count;
    }

    // Get the box id of the first undelivered message
    private static long getUndeliveredMessageThreadId(Cursor cursor) {
        if (cursor.moveToFirst()) {
            return cursor.getLong(0);
        } else {
            return 0;
        }
    }

    // Whether all the undelivered messages belong to the same thread.
    private static boolean isFailedMessagesInSameThread(Cursor cursor) {
        long firstThreadId = getUndeliveredMessageThreadId(cursor);
        boolean isSame = true;
        while (cursor.moveToNext()) {
            if (cursor.getLong(0) != firstThreadId) {
                isSame = false;
                break;
            }
        }
        return isSame;
    }

    // Get the type of the first undelivered message (SMS or MMS)
    private static String getUndeliveredMessageType(Cursor cursor) {
        if (cursor.moveToFirst()) {
            return cursor.getString(MAILBOX_MSG_TYPE);
        } else {
            return null;
        }
    }

    // Get the box id of the first undelivered message
    private static long getUndeliveredMessageId(Cursor cursor) {
        if (cursor.moveToFirst()) {
            return cursor.getLong(MAILBOX_ID);
        } else {
            return 0;
        }
    }

    // Get the box id of the first undelivered message
    private static int getUndeliveredMessageBoxId(Cursor cursor) {
        if (cursor.moveToFirst()) {
            if (cursor.getString(MAILBOX_MSG_TYPE).equals("sms")) {
                return cursor.getInt(MAILBOX_SMS_TYPE);
            } else {
                return cursor.getInt(MAILBOX_MMS_BOX);
            }
        }
        return MailBoxMessageList.TYPE_INBOX;
    }

    // Whether all the undelivered messages belong to the same box.
    private static boolean isFailedMessagesInSameBox(Cursor cursor) {
        int firstBoxId = getUndeliveredMessageBoxId(cursor);
        boolean isSame = true;
        while (cursor.moveToNext()) {
            if (cursor.getString(MAILBOX_MSG_TYPE).equals("sms")) {
                if (cursor.getInt(MAILBOX_SMS_TYPE) != firstBoxId) {
                    isSame = false;
                    break;
                }
            } else {
                if (cursor.getInt(MAILBOX_MMS_BOX) != firstBoxId) {
                    isSame = false;
                    break;
                }
            }
        }
        return isSame;
    }

    public static void nonBlockingUpdateSendFailedNotification(final Context context) {
        new AsyncTask<Void, Void, Integer>() {
            protected Integer doInBackground(Void... none) {
                return getUndeliveredMessageCount(context);
            }

            protected void onPostExecute(Integer result) {
                if (result < 1) {
                    cancelNotification(context, MESSAGE_FAILED_NOTIFICATION_ID);
                } else {
                    // rebuild and adjust the message count if necessary.
                    notifySendFailed(context);
                }
            }
        }.execute();
    }

    /**
     *  If all the undelivered messages belong to "threadId", cancel the notification.
     */
    public static void updateSendFailedNotificationForThread(Context context, long threadId) {
        Cursor cursor = SqliteWrapper.query(context, context.getContentResolver(),
                UNDELIVERED_URI, MMS_THREAD_ID_PROJECTION, "read=0", null, null);
        if (cursor == null) {
            return;
        }
        try {
            if (cursor.getCount() > 0
                    && getUndeliveredMessageThreadId(cursor) == threadId
                    && isFailedMessagesInSameThread(cursor)) {
                cancelNotification(context, MESSAGE_FAILED_NOTIFICATION_ID);
            }
        } finally {
            cursor.close();
        }
    }

    private static int getDownloadFailedMessageCount(Context context) {
        // Look for any messages in the MMS Inbox that are of the type
        // NOTIFICATION_IND (i.e. not already downloaded) and in the
        // permanent failure state.  If there are none, cancel any
        // failed download notification.
        Cursor c = SqliteWrapper.query(context, context.getContentResolver(),
                Mms.Inbox.CONTENT_URI, null,
                Mms.MESSAGE_TYPE + "=" +
                    String.valueOf(PduHeaders.MESSAGE_TYPE_NOTIFICATION_IND) +
                " AND " + Mms.STATUS + "=" +
                    String.valueOf(DownloadManager.STATE_PERMANENT_FAILURE),
                null, null);
        if (c == null) {
            return 0;
        }
        int count = c.getCount();
        c.close();
        return count;
    }

    public static void updateDownloadFailedNotification(Context context) {
        if (getDownloadFailedMessageCount(context) < 1) {
            cancelNotification(context, DOWNLOAD_FAILED_NOTIFICATION_ID);
        }
    }

    public static boolean isFailedToDeliver(Intent intent) {
        return (intent != null) && intent.getBooleanExtra(UNDELIVERED_FLAG, false);
    }

    public static boolean isFailedToDownload(Intent intent) {
        return (intent != null) && intent.getBooleanExtra(FAILED_DOWNLOAD_FLAG, false);
    }

    /**
     * Get the thread ID of the SMS message with the given URI
     * @param context The context
     * @param uri The URI of the SMS message
     * @return The thread ID, or THREAD_NONE if the URI contains no entries
     */
    public static long getSmsThreadId(Context context, Uri uri) {
        Cursor cursor = SqliteWrapper.query(
            context,
            context.getContentResolver(),
            uri,
            SMS_THREAD_ID_PROJECTION,
            null,
            null,
            null);

        if (cursor == null) {
            if (DEBUG) {
                Log.d(TAG, "getSmsThreadId uri: " + uri + " NULL cursor! returning THREAD_NONE");
            }
            return THREAD_NONE;
        }

        try {
            if (cursor.moveToFirst()) {
                int columnIndex = cursor.getColumnIndex(Sms.THREAD_ID);
                if (columnIndex < 0) {
                    if (DEBUG) {
                        Log.d(TAG, "getSmsThreadId uri: " + uri +
                                " Couldn't read row 0, col -1! returning THREAD_NONE");
                    }
                    return THREAD_NONE;
                }
                long threadId = cursor.getLong(columnIndex);
                if (DEBUG) {
                    Log.d(TAG, "getSmsThreadId uri: " + uri +
                            " returning threadId: " + threadId);
                }
                return threadId;
            } else {
                if (DEBUG) {
                    Log.d(TAG, "getSmsThreadId uri: " + uri +
                            " NULL cursor! returning THREAD_NONE");
                }
                return THREAD_NONE;
            }
        } finally {
            cursor.close();
        }
    }

    /**
     * Get the thread ID of the MMS message with the given URI
     * @param context The context
     * @param uri The URI of the SMS message
     * @return The thread ID, or THREAD_NONE if the URI contains no entries
     */
    public static long getThreadId(Context context, Uri uri) {
        Cursor cursor = SqliteWrapper.query(
                context,
                context.getContentResolver(),
                uri,
                MMS_THREAD_ID_PROJECTION,
                null,
                null,
                null);

        if (cursor == null) {
            if (DEBUG) {
                Log.d(TAG, "getThreadId uri: " + uri + " NULL cursor! returning THREAD_NONE");
            }
            return THREAD_NONE;
        }

        try {
            if (cursor.moveToFirst()) {
                int columnIndex = cursor.getColumnIndex(Mms.THREAD_ID);
                if (columnIndex < 0) {
                    if (DEBUG) {
                        Log.d(TAG, "getThreadId uri: " + uri +
                                " Couldn't read row 0, col -1! returning THREAD_NONE");
                    }
                    return THREAD_NONE;
                }
                long threadId = cursor.getLong(columnIndex);
                if (DEBUG) {
                    Log.d(TAG, "getThreadId uri: " + uri +
                            " returning threadId: " + threadId);
                }
                return threadId;
            } else {
                if (DEBUG) {
                    Log.d(TAG, "getThreadId uri: " + uri +
                            " NULL cursor! returning THREAD_NONE");
                }
                return THREAD_NONE;
            }
        } finally {
            cursor.close();
        }
    }

    private static void notifyUserIfFullScreen(Context context, String from) {
        ActivityManager am = (ActivityManager)context.getSystemService(Context.ACTIVITY_SERVICE);
        List<ActivityManager.RunningTaskInfo> runningTasks = am.getRunningTasks(1);

        if (runningTasks.size() > 0) {
            String topActivity = runningTasks.get(0).topActivity.getClassName();
            Log.d(TAG, "checkIsFullScreenMode: the top activity is: " + topActivity);
            if ((topActivity != null)
                    && (topActivity.equals("com.android.browser.BrowserActivity"))) {
                Intent intent = new Intent("com.android.mms.transaction.MESSAGE_RECEIVED");
                intent.putExtra("from", from);
                context.sendBroadcast(intent);
            }
        }
    }

    public static void blockingRemoveIccNotifications(Context context, int slotId) {
        if (slotId == MessageUtils.SUB_INVALID) {
            cancelNotification(context, ICC_NOTIFICATION_ID);
        } else {
            cancelNotification(context, NEW_ICC_NOTIFICATION_ID[slotId]);
        }
    }

    /**
     * Checks to see if the message memory is full.
     *
     * @param context the context to use
     * @param isFull if notify a full icon, it should be true, otherwise, false.
     */
    public static void updateMessageFullIndicator(Context context, boolean isSms, boolean isFull) {
        if (isFull) {
            sendFullNotification(context, isSms);
        } else {
            cancelNotification(context, FULL_NOTIFICATION_ID);
        }
    }

    /**
     * This method sends a notification to NotificationManager to display
     * an dialog indicating the message memory is full.
     */
    private static void sendFullNotification(Context context, boolean isSms) {
        NotificationManager nm = (NotificationManager)context.getSystemService(
                Context.NOTIFICATION_SERVICE);
        final Notification.Builder noti =
                new Notification.Builder(context, SIM_FULL_CHANNEL_ID);
        String title = context.getString(isSms ? R.string.sms_full_title
                : R.string.memory_low_title);
        String description = context.getString(isSms ? R.string.sms_full_body
                : R.string.memory_low_body);
        PendingIntent intent = PendingIntent.getActivity(context, 0,  new Intent(), 0);
        Notification notification = new Notification.BigTextStyle(noti).build();
        notification.icon = R.drawable.notification_icon_failed;
        notification.tickerText = title;
        notification.setLatestEventInfo(context, title, description, intent);
        NotificationChannel channel = new NotificationChannel(SIM_FULL_CHANNEL_ID,
                context.getString(R.string.sim_full_notification),
                NotificationManager.IMPORTANCE_HIGH);
        nm.createNotificationChannel(channel);
        nm.notify(FULL_NOTIFICATION_ID, notification);
    }

    /**
     * Return the intent of multi-unread messges notification.
     */
    public static Intent getMultiThreadsViewIntent(Context context) {
        Intent intent;
        if (MessageUtils.isMailboxMode()) {
                intent = new Intent(context, MailBoxMessageList.class);
                intent.putExtra(MessageUtils.MAIL_BOX_ID,
                    MailBoxMessageList.TYPE_INBOX);
        } else {
            intent = new Intent(context, ConversationList.class);
        }
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK
                | Intent.FLAG_ACTIVITY_SINGLE_TOP
                | Intent.FLAG_ACTIVITY_CLEAR_TOP);

        return intent;
    }
}
