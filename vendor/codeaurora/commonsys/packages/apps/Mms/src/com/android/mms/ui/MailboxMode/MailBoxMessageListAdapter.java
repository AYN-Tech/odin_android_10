/*
 * Copyright (c) 2014, 2016-2017 The Linux Foundation. All rights reserved.
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

import java.util.LinkedHashMap;
import java.util.Map;

import android.app.ListActivity;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.database.Cursor;
import android.graphics.drawable.Drawable;
import android.graphics.Typeface;
import android.net.Uri;
import android.os.Handler;
import android.provider.Telephony.Sms;
import android.provider.Telephony.Mms;
import android.provider.Telephony.MmsSms;
import android.telephony.SubscriptionManager;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.style.StyleSpan;
import android.text.TextUtils;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.CursorAdapter;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.QuickContactBadge;
import android.widget.TextView;

import com.android.mms.data.Contact;
import com.android.mms.data.Conversation;
import com.android.mms.LogTag;
import com.android.mms.R;
import com.android.mms.ui.MessageUtils;
import com.android.mms.ui.MailboxMode.MailBoxMessageListItem;
import com.android.mmswrapper.SubscriptionManagerWrapper;
import com.google.android.mms.pdu.EncodedStringValue;
import com.google.android.mms.pdu.PduPersister;

import static com.android.mms.ui.MessageListAdapter.COLUMN_MSG_TYPE;
import static com.android.mms.ui.MessageListAdapter.COLUMN_ID;
import static com.android.mms.ui.MessageListAdapter.COLUMN_THREAD_ID;
import static com.android.mms.ui.MessageListAdapter.COLUMN_MMS_LOCKED;
import static com.android.mms.ui.MessageListAdapter.COLUMN_MMS_SUBJECT;
import static com.android.mms.ui.MessageListAdapter.COLUMN_MMS_DATE;
import static com.android.mms.ui.MessageListAdapter.COLUMN_MMS_READ;
import static com.android.mms.ui.MessageListAdapter.COLUMN_MMS_MESSAGE_TYPE;
import static com.android.mms.ui.MessageListAdapter.COLUMN_MMS_MESSAGE_BOX;
import static com.android.mms.ui.MessageListAdapter.COLUMN_MMS_ERROR_TYPE;
import static com.android.mms.ui.MessageListAdapter.COLUMN_MMS_SUBJECT_CHARSET;
import static com.android.mms.ui.MessageListAdapter.COLUMN_MMS_SUB_ID;
import static com.android.mms.ui.MessageListAdapter.COLUMN_RECIPIENT_IDS;
import static com.android.mms.ui.MessageListAdapter.COLUMN_SMS_READ;

public class MailBoxMessageListAdapter extends CursorAdapter implements Contact.UpdateListener {
    private LayoutInflater mInflater;
    private static final String TAG = "MailBoxMessageListAdapter";

    private OnListContentChangedListener mListChangedListener;
    private final LinkedHashMap<String, BoxMessageItem> mMessageItemCache;
    private static final int CACHE_SIZE = 50;
    private static final StyleSpan STYLE_BOLD = new StyleSpan(Typeface.BOLD);

    // For posting UI update Runnables from other threads:
    private Handler mHandler = new Handler();
    private ListView mListView;
    QuickContactBadge mAvatarView;
    TextView mNameView;
    TextView mBodyView;
    TextView mDateView;
    ImageView mErrorIndicator;
    ImageView mImageViewLock;

    private int mSubscription = MessageUtils.SUB_INVALID;
    private String mMsgType; // "sms" or "mms"
    private String mAddress;
    private String mName;
    private int mMsgBox;
    private int mWapPushAddressIndex;
    private Context mContext;

    public MailBoxMessageListAdapter(Context context, OnListContentChangedListener changedListener,
            Cursor cursor) {
        super(context, cursor);
        mContext = context;
        mListView = ((ListActivity) context).getListView();
        // Cache the LayoutInflate to avoid asking for a new one each time.
        mInflater = LayoutInflater.from(context);
        mListChangedListener = changedListener;
        mMessageItemCache = new LinkedHashMap<String, BoxMessageItem>(10, 1.0f, true) {
            @Override
            protected boolean removeEldestEntry(Map.Entry eldest) {
                return size() > CACHE_SIZE;
            }
        };
        mWapPushAddressIndex = context.getResources().getInteger(R.integer.wap_push_address_index);
    }

    public BoxMessageItem getCachedMessageItem(String type, long msgId, Cursor c) {
        BoxMessageItem item = mMessageItemCache.get(getKey(type, msgId));
        if (item == null) {
            item = new BoxMessageItem(mContext, type, msgId, c);
            mMessageItemCache.put(getKey(item.mType, item.mMsgId), item);
        }
        return item;
    }

    private static String getKey(String type, long id) {
        if ("mms".equals(type)) {
            return "";
        } else {
            return type + String.valueOf(id);
        }
    }

    private void updateAvatarView() {
        Drawable avatarDrawable;
        Drawable sDefaultContactImage = mContext.getResources().getDrawable(
                R.drawable.ic_contact_picture);
        Drawable sDefaultContactImageMms = mContext.getResources().getDrawable(
                R.drawable.ic_contact_picture_mms);

        boolean isDraft = false;
        if (mMsgType.equals("mms") && mMsgBox == Mms.MESSAGE_BOX_DRAFTS ||
                mMsgType.equals("sms") && mMsgBox == Sms.MESSAGE_TYPE_DRAFT) {
            isDraft = true;
        }

        if (!isDraft && MessageUtils.isMsimIccCardActive()) {
            int phoneId = SubscriptionManagerWrapper.getPhoneId(mSubscription);
            sDefaultContactImage = (phoneId == MessageUtils.SUB1) ? mContext.getResources()
                    .getDrawable(R.drawable.ic_contact_picture_card1) : mContext.getResources()
                    .getDrawable(R.drawable.ic_contact_picture_card2);
            sDefaultContactImageMms = (phoneId == MessageUtils.SUB1) ? mContext
                    .getResources().getDrawable(R.drawable.ic_contact_picture_mms_card1) : mContext
                    .getResources().getDrawable(R.drawable.ic_contact_picture_mms_card2);
        }

        Contact contact = Contact.get(mAddress, true);
        if (mMsgType.equals("mms")) {
            avatarDrawable = sDefaultContactImageMms;
        } else {
            avatarDrawable = sDefaultContactImage;
        }
        avatarDrawable = contact.getAvatar(mContext, avatarDrawable);

        if(!contact.isCreatedByMultipleCellPhone()) {
            if (contact.existsInDatabase()) {
                mAvatarView.assignContactUri(contact.getUri());
            } else if (MessageUtils.isWapPushNumber(contact.getNumber())) {
                mAvatarView.assignContactFromPhone(
                        MessageUtils.getWapPushNumber(contact.getNumber()), true);
            } else {
                mAvatarView.assignContactFromPhone(contact.getNumber(), true);
            }
        }

        mAvatarView.setImageDrawable(avatarDrawable);
        mAvatarView.setVisibility(View.VISIBLE);
    }

    public void onUpdate(Contact updated) {
        if (Log.isLoggable(LogTag.CONTACT, Log.DEBUG)) {
            Log.v(TAG, "onUpdate: " + this + " contact: " + updated);
        }

        mHandler.post(new Runnable() {
            public void run() {
                notifyDataSetChanged();
            }
        });
    }

    public View newView(Context context, Cursor cursor, ViewGroup parent) {
        /* FIXME: this is called 3+x times too many by the ListView */
        View ret = mInflater.inflate(R.layout.mailbox_msg_list, parent, false);
        return ret;
    }

    @Override
    public long getItemId(int position) {
        return position;
    }

    public void updateItemBackgroud(int position) {
        Cursor cursor = (Cursor)getItem(position);
        int firstPosition = mListView.getFirstVisiblePosition();
        View view = mListView.getChildAt(position - firstPosition);
        if (cursor == null || view == null) {
            return;
        } else {
            ((MailBoxMessageListItem) view).updateBackground();
        }
    }

    public void bindView(View view, Context context, Cursor cursor) {
        if (Log.isLoggable(LogTag.CONTACT, Log.DEBUG)) {
            Log.v(TAG, "bind: contacts.addListeners " + this);
        }
        Contact.addListener(this);
        cleanItemCache();

        final String type = cursor.getString(COLUMN_MSG_TYPE);
        mMsgType = type;
        long msgId = cursor.getLong(COLUMN_ID);
        long threadId = cursor.getLong(COLUMN_THREAD_ID);
        String addr = "";
        String bodyStr = "";
        String nameContact = "";
        String dateStr = "";
        String recipientIds = "";
        // Set time stamp
        long date = 0;
        Drawable sendTypeIcon = null;
        boolean isError = false;
        boolean isLocked = false;
        boolean isUnread = false;
        mMsgBox = Sms.MESSAGE_TYPE_INBOX;

        if (type.equals("sms")) {
            BoxMessageItem item = getCachedMessageItem(type, msgId, cursor);
            int status = item.mStatus;
            mMsgBox = item.mSmsType;
            int smsRead = item.mRead;
            isUnread = (smsRead == 0 ? true : false);
            mSubscription = item.mSubID;
            addr = item.mAddress;
            isError = item.mSmsType == Sms.MESSAGE_TYPE_FAILED;
            isLocked = item.mLocked;
            bodyStr = item.mBody;
            dateStr = item.mDateStr;
            nameContact = item.mName;
        } else if (type.equals("mms")) {
            final int mmsRead = cursor.getInt(COLUMN_MMS_READ);
            mSubscription = cursor.getInt(COLUMN_MMS_SUB_ID);
            int messageType = cursor.getInt(COLUMN_MMS_MESSAGE_TYPE);
            mMsgBox = cursor.getInt(COLUMN_MMS_MESSAGE_BOX);
            isError = cursor.getInt(COLUMN_MMS_ERROR_TYPE)
                    >= MmsSms.ERR_TYPE_GENERIC_PERMANENT;
            isLocked = cursor.getInt(COLUMN_MMS_LOCKED) != 0;
            recipientIds = cursor.getString(COLUMN_RECIPIENT_IDS);

            if (0 == mmsRead && mMsgBox == Mms.MESSAGE_BOX_INBOX) {
                isUnread = true;
            }

            bodyStr = MessageUtils.extractEncStrFromCursor(cursor, COLUMN_MMS_SUBJECT,
                    COLUMN_MMS_SUBJECT_CHARSET);
            if (bodyStr.equals("")) {
                bodyStr = mContext.getString(R.string.no_subject_view);
            }

            date = cursor.getLong(COLUMN_MMS_DATE) * 1000;
            dateStr = MessageUtils.formatTimeStampString(context, date, false);

            // get address and name of MMS from recipientIds
            addr = recipientIds;
            if (!TextUtils.isEmpty(recipientIds)) {
                addr = MessageUtils.getRecipientsByIds(context, recipientIds, true);
                nameContact = Contact.get(addr, true).getName();
            } else if (threadId > 0) {
                addr = MessageUtils.getAddressByThreadId(context, threadId);
                nameContact = Contact.get(addr, true).getName();
            } else {
                addr = "";
                nameContact = "";
            }
        }

        Conversation conv = Conversation.from(context, cursor);
        ((MailBoxMessageListItem) view).bind(isUnread,
                mListView.isItemChecked(cursor.getPosition()),
                conv);

        mBodyView = (TextView) view.findViewById(R.id.msgBody);
        mDateView = (TextView) view.findViewById(R.id.textViewDate);
        mErrorIndicator = (ImageView)view.findViewById(R.id.error);
        mImageViewLock = (ImageView) view.findViewById(R.id.imageViewLock);
        mNameView = (TextView) view.findViewById(R.id.textName);
        mAvatarView = (QuickContactBadge) view.findViewById(R.id.avatar);
        mAddress = addr;
        mName = nameContact;

        if (MessageUtils.isWapPushNumber(addr) && MessageUtils.isWapPushNumber(nameContact)) {
            String[] mMailBoxAddresses = addr.split(":");
            String[] mMailBoxName = nameContact.split(":");
            formatNameView(
                    mMailBoxAddresses[mWapPushAddressIndex],
                    mMailBoxName[mWapPushAddressIndex],
                    isUnread);
        } else if (MessageUtils.isWapPushNumber(addr)) {
            String[] mailBoxAddresses = addr.split(":");
            addr = mailBoxAddresses[mWapPushAddressIndex];
            formatNameView(addr, mName, isUnread);
        } else if (MessageUtils.isWapPushNumber(nameContact)) {
            String[] mailBoxName = nameContact.split(":");
            nameContact = mailBoxName[mWapPushAddressIndex];
            formatNameView(mAddress, nameContact, isUnread);
        } else {
            formatNameView(mAddress, mName, isUnread);
        }

        updateAvatarView();

        mImageViewLock.setVisibility(isLocked ? View.VISIBLE : View.GONE);

        // Transmission error indicator.
        mErrorIndicator.setVisibility(isError ? View.VISIBLE : View.GONE);

        mDateView.setText(dateStr);
        if (isUnread) {
            SpannableStringBuilder buf = new SpannableStringBuilder(bodyStr);
            buf.setSpan(STYLE_BOLD, 0, buf.length(), Spannable.SPAN_INCLUSIVE_EXCLUSIVE);
            mBodyView.setText(buf);
        } else {
            mBodyView.setText(bodyStr);
        }
    }

    public void formatNameView(String address, String name, boolean isUnread) {
        SpannableStringBuilder buf = null;
        if (TextUtils.isEmpty(name)) {
            if (!TextUtils.isEmpty(address)) {
                buf = new SpannableStringBuilder(address);
            }
        } else {
            buf = new SpannableStringBuilder(name);
        }
        if (isUnread && buf != null) {
            buf.setSpan(STYLE_BOLD, 0, buf.length(),
                    Spannable.SPAN_INCLUSIVE_EXCLUSIVE);
        }
        mNameView.setText(buf);
    }

    public void cleanItemCache() {
        mMessageItemCache.clear();
    }

    @Override
    public void notifyDataSetChanged() {
        super.notifyDataSetChanged();
        mMessageItemCache.clear();
    }

    /**
     * Callback on the UI thread when the content observer on the backing cursor
     * fires. Instead of calling requery we need to do an async query so that
     * the requery doesn't block the UI thread for a long time.
     */
    @Override
    protected void onContentChanged() {
        mListChangedListener.onListContentChanged();
    }

    public interface OnListContentChangedListener {
        void onListContentChanged();
    }
}
