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

package com.android.mms.ui;

import android.content.Context;
import android.database.Cursor;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.graphics.Typeface;
import android.os.Handler;
import android.provider.Telephony;
import android.telephony.TelephonyManager;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.style.ForegroundColorSpan;
import android.text.style.StyleSpan;
import android.text.style.TextAppearanceSpan;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Checkable;
import android.widget.ImageView;
import android.widget.QuickContactBadge;
import android.widget.RelativeLayout;
import android.widget.TextView;

import com.android.mms.LogTag;
import com.android.mms.MmsApp;
import com.android.mms.R;
import com.android.mms.data.Contact;
import com.android.mms.data.ContactList;
import com.android.mms.data.Conversation;
import com.android.mms.util.ContactRecipientEntryUtils;
import com.android.mms.util.DownloadManager;
import com.android.mms.util.MaterialColorMapUtils;

import java.util.Locale;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * This class manages the view for given conversation.
 */
public class ConversationListItem extends RelativeLayout implements Contact.UpdateListener,
            Checkable {
    private static final String TAG = LogTag.TAG;
    private static final boolean DEBUG = false;

    private TextView mSubjectView;
    private TextView mFromView;
    private TextView mDateView;
    private TextView mAttachmentInfoView;
    private TextView mAttachmentStatusView;
    private TextView mAttachmentStatusSubView;
    private TextView mSendFailView;
    private View mAttachmentView;
    private View mErrorIndicator;
    private QuickContactBadge mAvatarView;

    static private Drawable sDefaultContactImage;
    private static Drawable sDefaultCheckedImageDrawable;
    private static Drawable sDefaultGroupContactImage;

    private static final int MAX_GROUP_AVATAR_NUM = 4;

    // For posting UI update Runnables from other threads:
    private Handler mHandler = new Handler();

    private Conversation mConversation;
    private boolean isLastMessageMms;

    public static final StyleSpan STYLE_BOLD = new StyleSpan(Typeface.BOLD);
    public static final StyleSpan ITALIC = new StyleSpan(Typeface.ITALIC);

    public ConversationListItem(Context context) {
        super(context);
    }

    public ConversationListItem(Context context, AttributeSet attrs) {
        super(context, attrs);

        if (sDefaultContactImage == null) {
            sDefaultContactImage = context.getResources().getDrawable(R.drawable.stranger);
        }
        if (sDefaultCheckedImageDrawable == null) {
            sDefaultCheckedImageDrawable = context.getResources().getDrawable(R.drawable.selected);
        }
        if (sDefaultGroupContactImage == null) {
            sDefaultGroupContactImage = context.getResources()
                    .getDrawable(R.drawable.stranger_group);
        }
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mFromView = (TextView) findViewById(R.id.from);
        mSubjectView = (TextView) findViewById(R.id.subject);

        mDateView = (TextView) findViewById(R.id.date);
        mAttachmentInfoView = (TextView) findViewById(R.id.attachment_info);
        mAttachmentStatusView = (TextView) findViewById(R.id.attachment_status);
        mAttachmentStatusSubView = (TextView) findViewById(R.id.attachment_sub_status);
        mSendFailView = (TextView)findViewById(R.id.error_info);
        mAttachmentView = findViewById(R.id.attachment);
        mErrorIndicator = findViewById(R.id.error);
        mAvatarView = (QuickContactBadge) findViewById(R.id.avatar);
        mAvatarView.setOverlay(null);
    }

    public Conversation getConversation() {
        return mConversation;
    }

    /**
     * Only used for header binding.
     */
    public void bind(String title, String explain) {
        mFromView.setText(title);
        mSubjectView.setText(explain);
    }

    private CharSequence formatMessage() {
        String from = mConversation.getRecipients().formatNames(", ");
        if (MessageUtils.isWapPushNumber(from)) {
            String[] mAddresses = from.split(":");
            from = mAddresses[mContext.getResources().getInteger(
                    R.integer.wap_push_address_index)];
        }

        /**
         * Add boolean to know that the "from" haven't the Arabic and '+'.
         * Make sure the "from" display normally for RTL.
         */
        Boolean isEnName = false;
        Boolean isLayoutRtl = (TextUtils.getLayoutDirectionFromLocale(Locale.getDefault())
                == View.LAYOUT_DIRECTION_RTL);
        if (isLayoutRtl && from != null) {
            if (from.length() >= 1) {
                Pattern pattern = Pattern.compile("[^أ-ي]+");
                Matcher matcher = pattern.matcher(from);
                isEnName = matcher.matches();
                if (from.charAt(0) != '\u202D') {
                    if (isEnName) {
                        from = '\u202D' + from + '\u202C';
                    }
                }
            }
        }

        SpannableStringBuilder buf = new SpannableStringBuilder(from);

        if (mConversation.hasDraft()) {
            if (isLayoutRtl && isEnName) {
                int before = buf.length();
                buf.insert(1,'\u202E'
                        + mContext.getResources().getString(R.string.draft_separator)
                        + '\u202C');
                buf.setSpan(new ForegroundColorSpan(
                        mContext.getResources().getColor(R.drawable.text_color_black)),
                        1, buf.length() - before + 1, Spannable.SPAN_INCLUSIVE_EXCLUSIVE);
                before = buf.length();
                int size;
                buf.insert(1, mContext.getResources().getString(R.string.has_draft));
                size = android.R.style.TextAppearance_Small;
                buf.setSpan(new TextAppearanceSpan(mContext, size), 1,
                        buf.length() - before + 1, Spannable.SPAN_INCLUSIVE_EXCLUSIVE);
                buf.setSpan(new ForegroundColorSpan(
                        mContext.getResources().getColor(R.drawable.text_color_red)),
                        1, buf.length() - before + 1, Spannable.SPAN_INCLUSIVE_EXCLUSIVE);
            } else {
                SpannableStringBuilder bufDraft = new SpannableStringBuilder(mContext
                        .getResources().getString(R.string.has_draft));
                bufDraft.setSpan(ITALIC, 0, bufDraft.length(),
                        Spannable.SPAN_INCLUSIVE_EXCLUSIVE);
                mDateView.setText(bufDraft);
            }
        }

        // Unread messages are shown in bold
        if (mConversation.hasUnreadMessages()) {
            buf.setSpan(STYLE_BOLD, 0, buf.length(),
                    Spannable.SPAN_INCLUSIVE_EXCLUSIVE);
            getLayoutParams().height = mContext.getResources().getDimensionPixelSize(
                    R.dimen.conversation_list_itme_height_unread);
        } else {
            getLayoutParams().height = mContext.getResources().getDimensionPixelSize(
                    R.dimen.conversation_list_itme_height);
        }
        return buf;
    }

    private void updateAvatarView() {
        ViewGroup parent = (ViewGroup) findViewById(R.id.avatar_layout);
        parent.removeAllViews();
        if (mConversation.isChecked()) {
            setContactDrawable(mAvatarView, null, false);
            parent.addView(mAvatarView);
        } else {
            if (mConversation.getRecipients().size() == 1) {
                Contact contact = mConversation.getRecipients().get(0);
                setContactDrawable(mAvatarView, contact, false);
                parent.addView(mAvatarView);
            } else {
                setGroupAvatar(parent);
            }
        }
    }

    private void setGroupAvatar(ViewGroup parent) {
        int size = mConversation.getRecipients().size();
        if (size > MAX_GROUP_AVATAR_NUM) {
            size = MAX_GROUP_AVATAR_NUM;
        }
        int group_layout = 0;
        switch (size) {
            case 2:
                group_layout = R.layout.group_chat_2_layout;
                break;
            case 3:
                group_layout = R.layout.group_chat_3_layout;
                break;
            case 4:
                group_layout = R.layout.group_chat_4_layout;
                break;
            default:
                Log.e(TAG, "No valid group layout found");
                return;
        }
        ViewGroup groupAvatar = (ViewGroup) LayoutInflater.from(mContext)
                .inflate(group_layout, parent, false);
        for (int i = 0; i < groupAvatar.getChildCount(); i++) {
            setContactDrawable((QuickContactBadge) groupAvatar.getChildAt(i),
                    mConversation.getRecipients().get(i), true);
        }
        parent.addView(groupAvatar);
    }

    private void setContactDrawable(QuickContactBadge view, Contact contact, boolean isGroup) {
        view.setScaleType(ImageView.ScaleType.CENTER_INSIDE);
        view.setOverlay(null);
        Drawable defaultContactImage = isGroup ? sDefaultGroupContactImage : sDefaultContactImage;
        Drawable backgroundDrawable = null;
        Drawable avatarDrawable;
        if (contact == null) {
            avatarDrawable = sDefaultCheckedImageDrawable;
            view.setBackgroundResource(R.drawable.selected_icon_background);
            view.setImageDrawable(avatarDrawable);
            view.setVisibility(View.VISIBLE);
            return;
        }
        avatarDrawable = contact.getAvatar(mContext, defaultContactImage);
        if (contact.existsInDatabase()) {
            if (avatarDrawable.equals(defaultContactImage)) {
                if (LetterTileDrawable.isEnglishLetterString(contact.getNameForAvatar())) {
                    avatarDrawable = MaterialColorMapUtils
                            .getLetterTitleDraw(mContext, contact);
                } else {
                    backgroundDrawable = MaterialColorMapUtils.getLetterTitleDraw(mContext,
                            contact);
                    view.setBackgroundDrawable(backgroundDrawable);
                }
            } else {
                view.setScaleType(ImageView.ScaleType.CENTER_CROP);
                view.setBackgroundDrawable(null);
            }

            view.assignContactUri(contact.getUri());
        } else {
            // identify it is phone number or email address,handle it respectively
            if (ContactRecipientEntryUtils.isEmailAddress(contact.getNumber())) {
                view.assignContactFromEmail(contact.getNumber(), true);
            } else if (MessageUtils.isWapPushNumber(contact.getNumber())) {
                view.assignContactFromPhone(
                        MessageUtils.getWapPushNumber(contact.getNumber()), true);
            } else {
                view.assignContactFromPhone(contact.getNumber(), true);
            }
            contact.setContactColor(mContext.getResources().getColor(R.color.avatar_default_color));
            backgroundDrawable = MaterialColorMapUtils.getLetterTitleDraw(mContext, contact);
            view.setBackgroundDrawable(backgroundDrawable);
        }
        view.setImageDrawable(avatarDrawable);
        view.setVisibility(View.VISIBLE);
    }

    private void updateFromView() {
        mFromView.setText(formatMessage());
        updateAvatarView();
    }

    public void onUpdate(Contact updated) {
        if (Log.isLoggable(LogTag.CONTACT, Log.DEBUG)) {
            Log.v(TAG, "onUpdate: " + this + " contact: " + updated);
        }
        mHandler.post(new Runnable() {
            public void run() {
                updateFromView();
            }
        });
    }

    public final void bind(Context context, final Conversation conversation) {
        //if (DEBUG) Log.v(TAG, "bind()");

        mConversation = conversation;
        String attachmentInfo = mConversation.getAttachmentInfo();
        isLastMessageMms = !"SMS".equals(attachmentInfo);
        if (mConversation.hasUnreadMessages() && !isLastMessageMms) {
            mSubjectView.setSingleLine(false);
            mSubjectView.setMaxLines(mContext.getResources().getInteger(
                    R.integer.max_unread_message_lines));
        } else {
            mSubjectView.setSingleLine(true);
        }

        updateBackground();

        boolean hasError = conversation.hasError();

        boolean hasAttachment = conversation.hasAttachment();
        mAttachmentView.setVisibility(hasAttachment ? VISIBLE : GONE);

        // Date
        mDateView.setText(formateUnreadToBold(MessageUtils.formatTimeStampString(context,
                conversation.getDate())));

        // From.
        mFromView.setText(formatMessage());

        // Register for updates in changes of any of the contacts in this conversation.
        ContactList contacts = conversation.getRecipients();

        if (Log.isLoggable(LogTag.CONTACT, Log.DEBUG)) {
            Log.v(TAG, "bind: contacts.addListeners " + this);
        }
        Contact.addListener(this);
        String snippet = conversation.getSnippet();
        if (!TextUtils.isEmpty(snippet)) {
            if (isLastMessageMms && !mConversation.hasDraft()
                    && (mConversation.getSnippetCs() != 0)) {
                snippet = mContext.getResources().getString(R.string.subject_label) + snippet;
            }
            mSubjectView.setText(formateUnreadToBold(snippet));
            mSubjectView.setVisibility(View.VISIBLE);
        } else {
            mSubjectView.setVisibility(View.GONE);
        }

        // Transmission error indicator.
        mErrorIndicator.setVisibility(hasError ? VISIBLE : GONE);
        mSendFailView.setVisibility(hasError ? VISIBLE : GONE);
        mDateView.setVisibility(hasError ? GONE : VISIBLE);

        updateAvatarView();
        updateAttachmentView(attachmentInfo);
    }

    private void updateAttachmentView(String attachmentInfo) {
        if (isLastMessageMms) {
            if (TextUtils.isEmpty(attachmentInfo) || mConversation.hasDraft()) {
                mAttachmentInfoView.setVisibility(View.GONE);
            } else {
                mAttachmentInfoView.setVisibility(View.VISIBLE);
                mAttachmentInfoView.setText(formateUnreadToBold(attachmentInfo));
                if (mConversation.hasUnreadMessages()) {
                    mAttachmentInfoView.setSingleLine(false);
                    mAttachmentInfoView.setMaxLines(mContext.getResources().getInteger(
                            R.integer.max_unread_message_lines));
                } else {
                    mAttachmentInfoView.setSingleLine(true);
                }
            }
            int mmsStatus = mConversation.getLastMessageStatus();
            mAttachmentStatusView.setVisibility(View.VISIBLE);
            mAttachmentStatusSubView.setVisibility(View.VISIBLE);
            switch (mmsStatus) {
                case DownloadManager.STATE_PRE_DOWNLOADING:
                case DownloadManager.STATE_DOWNLOADING:
                    mAttachmentStatusView.setText(R.string.new_mms_message);
                    mAttachmentStatusSubView.setText(R.string.downloading);
                    mAttachmentStatusSubView.setTextColor(Color.BLACK);
                    mAttachmentInfoView.setVisibility(View.GONE);
                    mDateView.setVisibility(View.GONE);
                    break;
                case DownloadManager.STATE_UNSTARTED:
                    DownloadManager downloadManager = DownloadManager.getInstance();
                    boolean autoDownload = downloadManager.isAuto();
                    boolean dataSuspended = (MmsApp.getApplication().getTelephonyManager()
                            .getDataState() == TelephonyManager.DATA_SUSPENDED);
                    if (!dataSuspended) {
                        mAttachmentInfoView.setVisibility(View.GONE);
                        if (autoDownload) {
                            mAttachmentStatusView.setText(R.string.new_mms_message);
                            mAttachmentStatusSubView.setText(R.string.downloading);
                            mAttachmentStatusSubView.setTextColor(Color.BLACK);
                            mAttachmentInfoView.setVisibility(View.GONE);
                            mDateView.setVisibility(View.GONE);
                        } else {
                            mAttachmentStatusSubView.setVisibility(View.GONE);
                            mAttachmentStatusView.setText(R.string.new_mms_download);
                        }
                        break;
                    }
                case DownloadManager.STATE_UNKNOWN:
                    mAttachmentStatusView.setVisibility(View.GONE);
                    mAttachmentStatusSubView.setVisibility(View.GONE);
                    break;
                case DownloadManager.STATE_TRANSIENT_FAILURE:
                case DownloadManager.STATE_PERMANENT_FAILURE:
                default:
                    mAttachmentInfoView.setVisibility(View.GONE);
                    mAttachmentStatusView.setText(R.string.could_not_download);
                    mAttachmentStatusSubView.setText(R.string.touch_to_download);
                    mAttachmentStatusSubView.setTextColor(Color.RED);
                    mErrorIndicator.setVisibility(View.VISIBLE);
                    mDateView.setVisibility(View.GONE);
                    break;
            }
        } else {
            mAttachmentInfoView.setVisibility(View.GONE);
            mAttachmentStatusView.setVisibility(View.GONE);
            mAttachmentStatusSubView.setVisibility(View.GONE);
        }
    }

    private CharSequence formateUnreadToBold(String content) {
        SpannableStringBuilder buf = null;
        if (content != null) {
            buf = new SpannableStringBuilder(content);
            if (mConversation.hasUnreadMessages()) {
                buf.setSpan(STYLE_BOLD, 0, buf.length(),
                        Spannable.SPAN_INCLUSIVE_EXCLUSIVE);
            }
        }
        return buf;
    }

    private void updateBackground() {
        int backgroundId;
        if (mConversation != null && mConversation.isChecked()) {
            backgroundId = R.color.conversation_item_selected;
        } else if (mConversation != null && mConversation.hasUnreadMessages()) {
            backgroundId = R.drawable.conversation_item_background_unread;
        } else {
            backgroundId = R.drawable.conversation_item_background_read;
        }
        Drawable background = mContext.getResources().getDrawable(backgroundId);
        setBackground(background);
    }

    public final void unbind() {
        if (Log.isLoggable(LogTag.CONTACT, Log.DEBUG)) {
            Log.v(TAG, "unbind: contacts.removeListeners " + this);
        }
        // Unregister contact update callbacks.
        Contact.removeListener(this);
    }

    public void setChecked(boolean checked) {
        try {
            if(mConversation != null){
                mConversation.setIsChecked(checked);
                updateBackground();
            }
        } catch (Exception e) {
            // TODO: handle exception
        }

    }

    public boolean isChecked() {
        return mConversation != null && mConversation.isChecked();
    }

    public void toggle() {
        mConversation.setIsChecked(!mConversation.isChecked());
    }
}
