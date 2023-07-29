/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.

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

package com.android.soundrecorder.filelist.player;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import com.android.soundrecorder.R;
import com.android.soundrecorder.util.Utils;

public class PlayerPanel extends LinearLayout {
    private static final int MAX_PROGRESS = 1000;
    private ProgressBar mProgressBar;
    private TextView mProgressTimeView;
    private TextView mProgressTotalView;
    private TextView mTitleView;
    private ImageButton mPlayerButton;

    private LayoutChangedListener mLayoutChangedListener;

    public interface LayoutChangedListener {
        void onLayoutChanged(View view);
    }

    public void setLayoutChangedListener(LayoutChangedListener listener) {
        mLayoutChangedListener = listener;
    }

    public PlayerPanel(Context context) {
        this(context, null);
    }

    public PlayerPanel(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public PlayerPanel(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        initResources();
    }

    private void initResources() {
        mProgressBar = (ProgressBar) findViewById(R.id.player_progress);
        mProgressTimeView = (TextView) findViewById(R.id.player_progress_time);
        mProgressTotalView = (TextView) findViewById(R.id.player_total_time);
        mTitleView = (TextView) findViewById(R.id.player_title);
        mPlayerButton = (ImageButton) findViewById(R.id.player_controller);

        mProgressBar.setMax(MAX_PROGRESS);
        updateProgress(0, 0);
    }

    public void setPlayerButtonClickListener(OnClickListener listener) {
        mPlayerButton.setOnClickListener(listener);
    }

    public void updateTitle(String title) {
        mTitleView.setText(title);
    }

    public void updateProgress(long current, long total) {
        mProgressBar.setProgress(total == 0 ? 0 : (int) (current * MAX_PROGRESS / total));
        // millisecond to second
        long cur = Math.round(current * 1.0f / 1000);
        mProgressTimeView.setText(Utils.timeToString(getContext(), cur));
        mProgressTotalView.setText(Utils.timeToString(getContext(), total / 1000));
    }

    public void updatePlayerStatus(boolean playing) {
        if (playing) {
            mPlayerButton.setImageResource(R.drawable.player_panel_pause);
        } else {
            mPlayerButton.setImageResource(R.drawable.player_panel_play);
        }
    }

    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        super.onLayout(changed, l, t, r, b);
        if (changed && mLayoutChangedListener != null) {
            mLayoutChangedListener.onLayoutChanged(this);
        }
    }

    @Override
    public void setVisibility(int visibility) {
        int oldVisibility = getVisibility();
        super.setVisibility(visibility);
        if (visibility != oldVisibility && mLayoutChangedListener != null) {
            mLayoutChangedListener.onLayoutChanged(this);
        }
    }
}
