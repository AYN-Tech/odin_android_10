/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

package com.android.mms.ui;

import android.app.NotificationManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.database.sqlite.SqliteWrapper;
import android.net.Uri;
import android.provider.Telephony.Mms;
import android.util.Log;
import android.widget.Toast;

import com.android.mms.LogTag;
import com.android.mms.MmsConfig;
import com.android.mms.R;
import com.android.mms.data.Conversation;
import com.android.mms.transaction.MessagingNotification;
import com.android.mms.transaction.Transaction;
import com.android.mms.transaction.TransactionBundle;
import com.android.mms.transaction.TransactionService;
import com.android.mms.util.DownloadManager;

import com.google.android.mms.MmsException;
import com.google.android.mms.pdu.NotificationInd;
import com.google.android.mms.pdu.PduHeaders;
import com.google.android.mms.pdu.PduPersister;

public class NotificationActionHandleReceiver extends BroadcastReceiver {

    private static final String TAG = LogTag.TAG;

    public static final String ACTION_NOTIFICATION_MARK_READ = "com.android.mms.MARK_AS_READ";
    public static final String ACTION_NOTIFICATION_DOWNLOAD = "com.android.mms.DOWNLOAD";
    public static final String MSG_ID = "msg_id";
    public static final String MSG_SUB_ID = "msg_sub_id";
    public static final String MSG_THREAD_ID = "msg_thread_id";

    @Override
    public void onReceive(Context context, Intent intent) {
        if (!MessageUtils.hasBasicPermissions()) {
            Log.d("Mms", "NotificationActionHandleReceiver do not have basic permissions");
            return;
        }
        if (intent == null) {
            return;
        }
        NotificationManager notificationManager =
                (NotificationManager)context.getSystemService(Context.NOTIFICATION_SERVICE);
        if (ACTION_NOTIFICATION_MARK_READ.equals(intent.getAction())) {
            long threadId = intent.getLongExtra(MSG_THREAD_ID, Conversation.INVALID_THREAD_ID);
            boolean isGroup = intent.getBooleanExtra(
                    NotificationQuickReplyActivity.KEY_IS_GROUP, false);
            Conversation conv = Conversation.get(context, threadId, true);
            if (conv != null) {
                conv.markAsReadDelayNotification();
                MessagingNotification.cancelNotificationItem(context,
                        MessagingNotification.NOTIFICATION_ID, threadId, isGroup);
            }
        } else if (ACTION_NOTIFICATION_DOWNLOAD.equals(intent.getAction())) {
            long msgId = intent.getLongExtra(MSG_ID, -1);
            long subId = intent.getLongExtra(MSG_SUB_ID, -1);
            downloadAttachment(context, subId, msgId);
            notificationManager.cancel(MessagingNotification.NOTIFICATION_ID);
        }
    }

    private void downloadAttachment(Context context, long subId, long msgId) {
        Uri msgUri = Uri.withAppendedPath(Mms.CONTENT_URI, String.valueOf(msgId));
        try {
            NotificationInd nInd = (NotificationInd) PduPersister.getPduPersister(context)
                   .load(msgUri);
            if (MessageUtils.isMmsMemoryFull()) {
                Toast.makeText(context, R.string.sms_full_body, Toast.LENGTH_SHORT).show();
                return;
            }
            else if ((int) nInd.getMessageSize() > MmsConfig.getMaxMessageSize()) {
                Toast.makeText(context, R.string.mms_too_large, Toast.LENGTH_SHORT).show();
                return;
            }
            else if (!MessageUtils.isMobileDataEnabled(context, (int)subId)) {
                Toast.makeText(context, context.getString(R.string.inform_data_off),
                        Toast.LENGTH_LONG).show();
                return;
            }
        } catch (MmsException e) {
           Log.e(TAG, e.getMessage(), e);
           return;
        }

        Intent intent = new Intent(context, TransactionService.class);
        intent.putExtra(TransactionBundle.URI, msgUri.toString());
        intent.putExtra(TransactionBundle.TRANSACTION_TYPE,
                Transaction.RETRIEVE_TRANSACTION);
        intent.putExtra(Mms.SUBSCRIPTION_ID, subId);

        context.startService(intent);

        DownloadManager.getInstance().markState(
                msgUri, DownloadManager.STATE_PRE_DOWNLOADING);
    }
}
