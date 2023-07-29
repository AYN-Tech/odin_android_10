/*
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
import android.content.res.Configuration;
import android.os.Handler;
import android.os.Message;
import android.util.AttributeSet;
import android.util.Log;
import android.view.View;
import android.view.ViewStub;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.TextView;

import com.android.mms.LogTag;
import com.android.mms.R;
import com.android.mms.MmsConfig;
import com.android.mms.data.WorkingMessage;
import com.android.mms.model.SlideModel;
import com.android.mms.model.SlideshowModel;

/**
 * This is an embedded editor/view to add photos and sound/video clips
 * into a multimedia message.
 */
public class AttachmentEditor extends LinearLayout {
    private static final String TAG = LogTag.TAG;
    private static final boolean DEBUG = false;
    static final int MSG_EDIT_SLIDESHOW   = 1;
    static final int MSG_SEND_SLIDESHOW   = 2;
    static final int MSG_PLAY_SLIDESHOW   = 3;
    static final int MSG_REPLACE_IMAGE    = 4;
    static final int MSG_REPLACE_VIDEO    = 5;
    static final int MSG_REPLACE_AUDIO    = 6;
    static final int MSG_PLAY_VIDEO       = 7;
    static final int MSG_PLAY_AUDIO       = 8;
    static final int MSG_VIEW_IMAGE       = 9;
    static final int MSG_REMOVE_ATTACHMENT = 10;
    static final int MSG_VIEW_VCARD        = 11;
    static final int MSG_REPLACE_VCARD     = 12;

    private static int ATTACHMENT_FONT_SIZE = 12;

    private static final int KILOBYTE = 1024;

    private final Context mContext;
    private Handler mHandler;

    private SlideViewInterface mView;
    private SlideshowModel mSlideshow;
    private Presenter mPresenter;
    private boolean mCanSend;
    private Button mSendButton;
    private TextView mSizeIndicator;
    private int mMediaSize = 0;

    public AttachmentEditor(Context context, AttributeSet attr) {
        super(context, attr);
        mContext = context;
    }

    /**
     * Returns true if the attachment editor has an attachment to show.
     */
    public boolean update(WorkingMessage msg) {
        hideView();
        mView = null;

        // If there's no attachment, we have nothing to do.
        if (!msg.hasAttachment()) {
            return false;
        }

        // Get the slideshow from the message.
        mSlideshow = msg.getSlideshow();

        mView = createView();

        if ((mPresenter == null) || !mSlideshow.equals(mPresenter.getModel())) {
            mPresenter = PresenterFactory.getPresenter(
                    "MmsThumbnailPresenter", mContext, mView, mSlideshow);
        } else {
            mPresenter.setView(mView);
        }

        mPresenter.present(null);
        return true;
    }

    public void setHandler(Handler handler) {
        mHandler = handler;
    }

    public void setCanSend(boolean enable) {
        if (mCanSend != enable) {
            mCanSend = enable;
            updateSendButton();
        }
    }

    private void updateSendButton() {
        if (null != mSendButton) {
            mSendButton.setEnabled(mCanSend);
            mSendButton.setFocusable(mCanSend);
        }
    }

    public void hideView() {
        if (mView != null) {
            ((View)mView).setVisibility(View.GONE);
        }
    }

    private View getStubView(int stubId, int viewId) {
        View view = findViewById(viewId);
        if (view == null) {
            ViewStub stub = (ViewStub) findViewById(stubId);
            view = stub.inflate();
        }

        return view;
    }

    private class MessageOnClick implements OnClickListener {
        private int mWhat;

        public MessageOnClick(int what) {
            mWhat = what;
        }

        public void onClick(View v) {
            Message msg = Message.obtain(mHandler, mWhat);
            msg.sendToTarget();
        }
    }

    private SlideViewInterface createView() {
        boolean inPortrait = inPortraitMode();
        if (mSlideshow.size() > 1) {
            return createSlideshowView(inPortrait);
        }

        SlideModel slide = mSlideshow.get(0);
        if (slide.hasImage()) {
            return createMediaView(
                    R.id.image_attachment_view_stub,
                    R.id.image_attachment_view,
                    R.id.view_image_button, R.id.replace_image_button, R.id.remove_image_button,
                    MSG_VIEW_IMAGE, MSG_REPLACE_IMAGE, MSG_REMOVE_ATTACHMENT);
        } else if (slide.hasVideo()) {
            return createMediaView(
                    R.id.video_attachment_view_stub,
                    R.id.video_attachment_view,
                    R.id.view_video_button, R.id.replace_video_button, R.id.remove_video_button,
                    MSG_PLAY_VIDEO, MSG_REPLACE_VIDEO, MSG_REMOVE_ATTACHMENT);
        } else if (slide.hasAudio()) {
            return createMediaView(
                    R.id.audio_attachment_view_stub,
                    R.id.audio_attachment_view,
                    R.id.play_audio_button, R.id.replace_audio_button, R.id.remove_audio_button,
                    MSG_PLAY_AUDIO, MSG_REPLACE_AUDIO, MSG_REMOVE_ATTACHMENT);
        } else if (slide.hasVcard()) {
            return createMediaView(R.id.vcard_attachment_view_stub,
                    R.id.vcard_attachment_view,
                    R.id.view_vcard_button,
                    R.id.replace_vcard_button,
                    R.id.remove_vcard_button,
                    MSG_VIEW_VCARD, MSG_REPLACE_VCARD, MSG_REMOVE_ATTACHMENT);
        } else {
            throw new IllegalArgumentException();
        }
    }

    /**
     * What is the current orientation?
     */
    private boolean inPortraitMode() {
        final Configuration configuration = mContext.getResources().getConfiguration();
        return configuration.orientation == Configuration.ORIENTATION_PORTRAIT;
    }

    private SlideViewInterface createMediaView(
            int stub_view_id, int real_view_id,
            int view_button_id, int replace_button_id, int remove_button_id,
            int view_message, int replace_message, int remove_message) {
        LinearLayout view = (LinearLayout)getStubView(stub_view_id, real_view_id);
        view.setVisibility(View.VISIBLE);
        mSizeIndicator = (TextView) view.findViewById(R.id.mms_size_indicator);
        if (mSizeIndicator != null) {
            if (view_message == MSG_VIEW_IMAGE) {
                mMediaSize = mSlideshow.getTotalMessageSize();
            } else {
                mMediaSize = mSlideshow.getCurrentMessageSize();
            }
            int currentSize = getSizeWithOverHead(mMediaSize + mSlideshow.getSubjectSize());
            if (DEBUG) {
                Log.v(TAG,"createMediaView currentSize = " + currentSize
                        + ", mMediaSize = " + mMediaSize
                        + ", subject size is "+mSlideshow.getSubjectSize());
            }
            mSizeIndicator.setText(mContext.getString(R.string.mms_size_indicator,
                            currentSize, MmsConfig.getMaxMessageSize() / KILOBYTE));
        }

        Button viewButton = (Button) view.findViewById(view_button_id);
        Button replaceButton = (Button) view.findViewById(replace_button_id);
        Button removeButton = (Button) view.findViewById(remove_button_id);

        if (viewButton != null) {
            viewButton.setTextSize(ATTACHMENT_FONT_SIZE);
        }
        if (replaceButton != null) {
            replaceButton.setTextSize(ATTACHMENT_FONT_SIZE);
        }
        if (removeButton != null) {
            removeButton.setTextSize(ATTACHMENT_FONT_SIZE);
        }
        viewButton.setOnClickListener(new MessageOnClick(view_message));
        replaceButton.setOnClickListener(new MessageOnClick(replace_message));
        removeButton.setOnClickListener(new MessageOnClick(remove_message));

        return (SlideViewInterface) view;
    }

    private SlideViewInterface createSlideshowView(boolean inPortrait) {
        LinearLayout view =(LinearLayout) getStubView(
                R.id.slideshow_attachment_view_stub,
                R.id.slideshow_attachment_view);
        view.setVisibility(View.VISIBLE);
        mSizeIndicator = (TextView) view.findViewById(R.id.mms_size_indicator);
        mMediaSize = mSlideshow.getTotalMessageSize();
        if (mSizeIndicator != null) {
            int currentSize = getSizeWithOverHead(mMediaSize + mSlideshow.getSubjectSize());
            if (DEBUG) {
                Log.v(TAG,"createSlideshowView currentSize = " + currentSize
                        + ", mMediaSize = " + mMediaSize
                        + ", subject size is "+mSlideshow.getSubjectSize());
            }
            mSizeIndicator.setText(mContext.getString(R.string.mms_size_indicator,
                            currentSize, MmsConfig.getMaxMessageSize() / KILOBYTE));
        }

        Button editBtn = (Button) view.findViewById(R.id.edit_slideshow_button);
        mSendButton = (Button) view.findViewById(R.id.send_slideshow_button);
        updateSendButton();
        final ImageButton playBtn = (ImageButton) view.findViewById(
                R.id.play_slideshow_button);

        editBtn.setOnClickListener(new MessageOnClick(MSG_EDIT_SLIDESHOW));
        mSendButton.setOnClickListener(new MessageOnClick(MSG_SEND_SLIDESHOW));
        playBtn.setOnClickListener(new MessageOnClick(MSG_PLAY_SLIDESHOW));

        Button removeButton = (Button) view.findViewById(R.id.remove_slideshow_button);
        removeButton.setOnClickListener(new MessageOnClick(MSG_REMOVE_ATTACHMENT));

        return (SlideViewInterface) view;
    }

    public void hideSlideshowSendButton() {
        if (mSendButton != null) {
            mSendButton.setVisibility(View.GONE);
        }
    }

    @Override
    protected void onDetachedFromWindow() {
        if (mPresenter != null) {
            mPresenter.cancelBackgroundLoading();
        }
        super.onDetachedFromWindow();
    }

    public boolean canAddTextForMms(CharSequence s) {
        int totalSize = 0;
        int textSize = s.toString().getBytes().length;
        int remainSize = MmsConfig.getMaxMessageSize();
        if (mMediaSize != 0) {
            // The AttachmentEditor only can edit text if there only one silde.
            // Here mSlideshow already includes text size, need to recalculate the totalsize.
            int totalTextSize = mSlideshow.getTotalTextMessageSize();
            remainSize = mSlideshow.getRemainMessageSize();
            if (DEBUG) {
                Log.v(TAG,"onTextChangeForMms totalTextSize = "+totalTextSize);
            }
            if (textSize != 0 && mSlideshow.size() == 1) {
                totalSize = mMediaSize - totalTextSize + textSize;
                remainSize = remainSize + totalTextSize - textSize;
            } else {
                totalSize = mMediaSize + textSize;
                remainSize = remainSize - textSize;
            }
            remainSize = remainSize -  mSlideshow.getSubjectSize();
        }

        if (DEBUG) {
            Log.v(TAG,"onTextChangeForMms textSize = " + textSize
                    + ", mMediaSize = " + mMediaSize
                    + ", totalSize = " + totalSize
                    + ", remainSize = "  + remainSize
                    );
        }

        if (mSizeIndicator != null) {
            int currentSize = getSizeWithOverHead(totalSize + mSlideshow.getSubjectSize());
            if (remainSize < 0) {
                currentSize = MmsConfig.getMaxMessageSize() / KILOBYTE;
            }

            if (DEBUG) {
                Log.v(TAG,"onTextChangeForMms currentSize = " + currentSize
                        + ", totalSize = " + totalSize
                        + ", subject size is "+mSlideshow.getSubjectSize());
            }
            mSizeIndicator.setText(mContext.getString(R.string.mms_size_indicator,
                            currentSize, MmsConfig.getMaxMessageSize() / KILOBYTE));
        }
        return remainSize >= 0;
    }

    private int getSizeWithOverHead(int size) {
        return (size + KILOBYTE -1) / KILOBYTE + 1;
    }
}
