/*
 * Copyright (C) 2013 Roger Chen <cxr514033970@gmail.com>
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

package com.android.mms.ui;

import android.app.Notification;
import android.app.NotificationManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import com.android.mms.R;
import com.android.mms.transaction.MessagingNotification;
import com.android.mmswrapper.ConstantsWrapper;
import android.app.NotificationChannel;

public class SmsStorageMonitor extends BroadcastReceiver {
    private Context mContext;
    private final int NOTIFICATION_STORAGE_LIMITED_ID = -1;
    private static NotificationManager mNotificationManager;

    @Override
    public void onReceive(Context context, Intent intent) {
        mContext = context;
        if (mNotificationManager == null) {
            mNotificationManager =
                    (NotificationManager) mContext.getSystemService(Context.NOTIFICATION_SERVICE);
        }
        if (intent.getAction().equals(
                ConstantsWrapper.IntentConstants.ACTION_DEVICE_STORAGE_FULL)) {
            notifyReachStorageLimited(context);
        } else if (intent.getAction().equals(
                ConstantsWrapper.IntentConstants.ACTION_DEVICE_STORAGE_NOT_FULL)) {
            cancelStorageLimitedWarning();
        }
    }

    private void notifyReachStorageLimited(Context context) {
        Notification.Builder mBuilder =
                new Notification.Builder(mContext,MessagingNotification.DEVICE_FULL_CHANNEL_ID)
                        .setSmallIcon(R.mipmap.ic_launcher_smsmms)
                        .setContentTitle(mContext.getString(R.string.storage_warning_title))
                        .setContentText(mContext.getString(R.string.storage_warning_content))
                        .setOngoing(true);
        NotificationChannel channel = new NotificationChannel(
                MessagingNotification.DEVICE_FULL_CHANNEL_ID,
                context.getString(R.string.device_full_notification),
                NotificationManager.IMPORTANCE_HIGH);
        mNotificationManager.createNotificationChannel(channel);
        mNotificationManager.notify(NOTIFICATION_STORAGE_LIMITED_ID, mBuilder.build());
    }

    private void cancelStorageLimitedWarning() {
        mNotificationManager.cancel(NOTIFICATION_STORAGE_LIMITED_ID);
    }
}
