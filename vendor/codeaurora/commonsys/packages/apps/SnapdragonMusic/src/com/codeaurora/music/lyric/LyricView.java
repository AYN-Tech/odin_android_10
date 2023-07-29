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

package com.codeaurora.music.lyric;

import android.content.Context;
import android.graphics.Canvas;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AbsListView;
import android.widget.ListView;

import com.android.music.R;

public class LyricView extends ListView {
    public static final String TAG = "LyricView";
    private View mHeaderView;
    private View mFooterView;
    private int mLyricLineHeight;
    private int mLyricMidPos = 0;

    public LyricView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mLyricLineHeight =
                context.getResources().getDimensionPixelSize(R.dimen.lyric_line_height);
        mHeaderView = LayoutInflater.from(context).inflate(R.layout.v_transpant_footer, null);
        mFooterView = LayoutInflater.from(context).inflate(R.layout.v_transpant_footer, null);
        AbsListView.LayoutParams headerLayoutParams
                = new AbsListView.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0);
        AbsListView.LayoutParams footerLayoutParams
                = new AbsListView.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0);
        mHeaderView.setLayoutParams(headerLayoutParams);
        mFooterView.setLayoutParams(footerLayoutParams);
        addHeaderView(mHeaderView);
        addFooterView(mFooterView);
    }

    @Override
    protected void dispatchDraw(Canvas canvas) {
        super.dispatchDraw(canvas);
    }

    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        if (changed) {
            int height = getMeasuredHeight();
            int displayLines = height / mLyricLineHeight;

            int paddingValue = (height - displayLines * mLyricLineHeight) / 2;
            setPadding(getPaddingLeft(), paddingValue, getPaddingRight(), paddingValue);
            int offset = 0;
            //Calculate the rows of lyrics. Set mFooterView and mHeaderView's height of layout. Calculate the lyrics highlight position.
            if (displayLines % 2 == 0) {
                mLyricMidPos = (displayLines / 2 - 2) * mLyricLineHeight + 1;
                offset = (displayLines / 2 - 1) * mLyricLineHeight;
                mHeaderView.getLayoutParams().height = offset;
                mFooterView.getLayoutParams().height = offset + mLyricLineHeight;
            } else {
                mLyricMidPos = (displayLines / 2 - 1) * mLyricLineHeight + 1;
                offset = (displayLines / 2) * mLyricLineHeight;
                mHeaderView.getLayoutParams().height = offset;
                mFooterView.getLayoutParams().height = offset;
            }
        }
        super.onLayout(changed, l, t, r, b);
    }

    public int getLyricMidPos() {
        return mLyricMidPos;
    }
}
