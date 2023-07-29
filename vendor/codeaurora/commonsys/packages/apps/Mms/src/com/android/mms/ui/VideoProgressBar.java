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

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.util.AttributeSet;
import android.view.View;

import com.android.mms.R;

public class VideoProgressBar extends View {
    private Paint mPosPaint;
    private Paint mBgPaint;
    private float position;
    private int radiusDp = 6;
    private int radiusPx = dip2px(radiusDp);
    private int lineHDp = 2;
    private int lineHPx = dip2px(2);

    public VideoProgressBar(Context context, AttributeSet attrs) {
        super(context, attrs);
        mPosPaint = new Paint();
        mBgPaint = new Paint();
        mPosPaint.setColor(getResources().getColor(R.color.video_progress_bar));
        mBgPaint.setColor(getResources().getColor(R.color.video_timer_font));
        mPosPaint.setAntiAlias(true);
        mBgPaint.setAntiAlias(true);
    }

    public void updateLocation(float pos) {
        if (pos > 1.0 || pos < 0 ) return;
        position = pos;
        this.postInvalidate();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        int width = getWidth();
        int height = getHeight();
        int cx = (int)((width - 2 * radiusPx) * position + radiusPx);
        int cy = getHeight() / 2;
        canvas.drawRoundRect(new RectF(radiusPx, height / 2 - lineHPx / 2,
                width, height / 2 + lineHPx / 2), 10, 10, mBgPaint);
        canvas.drawRoundRect(new RectF(radiusPx, height / 2 - lineHPx / 2,
                width * position, height / 2 + lineHPx / 2), 10, 10, mPosPaint);
        canvas.drawCircle(cx, cy, radiusPx, mPosPaint);
    }

    private int dip2px(float dpValue) {
        final float scale = getContext().getResources().getDisplayMetrics().density;
        return (int) (dpValue * scale + 0.5f);
    }
}

