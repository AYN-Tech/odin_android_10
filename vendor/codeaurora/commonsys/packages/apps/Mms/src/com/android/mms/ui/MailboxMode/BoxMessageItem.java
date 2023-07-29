/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 * Not a Contribution.
 *
 * Copyright (C) 2012 The Android Open Source Project.
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

import android.content.Context;
import android.database.Cursor;

import com.android.mms.data.Contact;

import static com.android.mms.ui.MessageListAdapter.COLUMN_SMS_ADDRESS;
import static com.android.mms.ui.MessageListAdapter.COLUMN_SMS_BODY;
import static com.android.mms.ui.MessageListAdapter.COLUMN_SMS_DATE;
import static com.android.mms.ui.MessageListAdapter.COLUMN_SMS_READ;
import static com.android.mms.ui.MessageListAdapter.COLUMN_SUB_ID;
import static com.android.mms.ui.MessageListAdapter.COLUMN_SMS_TYPE;
import static com.android.mms.ui.MessageListAdapter.COLUMN_SMS_STATUS;
import static com.android.mms.ui.MessageListAdapter.COLUMN_SMS_DATE_SENT;
import static com.android.mms.ui.MessageListAdapter.COLUMN_THREAD_ID;
import static com.android.mms.ui.MessageListAdapter.COLUMN_SMS_LOCKED;

/**
 * Mostly immutable model for an SMS/MMS message.
 *
 * The only mutable field is the cached formatted message member, the formatting
 * of which is done outside this model in MessageListItem.
 */
public class BoxMessageItem {
    private static String TAG = "BoxMessageItem";

    final String mType;
    final long mMsgId;
    int mSubID;
    long mDate;
    long mDateSent;
    String mAddress;
    String mBody; // Body of SMS, first text of MMS.

    // Fields for MMS only.
    int mRead;
    int mStatus;
    int mSmsType;// if icc sms then StatusOnIcc
    boolean mLocked;
    String mDateStr;
    String mName;
    long mThreadId = 0;

    public BoxMessageItem(Context context, String type, long msgId, Cursor cursor) {
        mType = type;
        mMsgId = msgId;

        if ("sms".equals(type)) {
            mBody = cursor.getString(COLUMN_SMS_BODY);
            mAddress = cursor.getString(COLUMN_SMS_ADDRESS);
            mDate = cursor.getLong(COLUMN_SMS_DATE);
            mStatus = cursor.getInt(COLUMN_SMS_STATUS);
            mRead = cursor.getInt(COLUMN_SMS_READ);
            mSubID = cursor.getInt(COLUMN_SUB_ID);
            mDateSent = cursor.getLong(COLUMN_SMS_DATE_SENT);
            mSmsType = cursor.getInt(COLUMN_SMS_TYPE);
            mLocked = cursor.getInt(COLUMN_SMS_LOCKED) != 0;
            mThreadId = cursor.getInt(COLUMN_THREAD_ID);
            // For incoming messages, the ADDRESS field contains the sender.
            mName = Contact.get(mAddress, true).getName();
        }

        mDateStr = MessageUtils.formatTimeStampString(context, mDate, false);
    }

    public boolean isMms() {
        return "mms".equals(mType);
    }

    public boolean isSms() {
        return "sms".equals(mType);
    }

    public void setBody(String body) {
        mBody = body;
    }
}
