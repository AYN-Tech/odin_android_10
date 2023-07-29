/*
 * Copyright (C) 2011 The Android Open Source Project
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

package com.android.soundrecorder;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.DashPathEffect;
import android.graphics.Paint;
import android.graphics.PathEffect;
import android.util.AttributeSet;
import android.view.View;

public class VUMeter extends View {
    static final float DROPOFF_STEP = 0.18f;
    static final long ANIMATION_INTERVAL = 70;
    static final int MAX_AMPLITUDE = 32768;

    static final float MIN_ANGLE = (float) Math.PI * -27 / 100;
    static final float MAX_ANGLE = (float) Math.PI * 127 / 100;
    static final float MIN_DEGREES = radiansToDegrees(MIN_ANGLE);
    static final float MAX_DEGREES = radiansToDegrees(MAX_ANGLE);

    static final float START_DEGREES = MIN_DEGREES + 180;
    static final float MASK_DEGREES_OFFSET = 0.4f;

    float mCurrentAngle = MIN_ANGLE;

    private Paint mPaint;
    private Paint mOuterPaint;
    private Paint mProgressPaint;
    private Paint mDashedProgressPaint;
    private final int mMaskColor =
            getContext().getResources().getColor(R.color.vumeter_background_color);
    private final int mMainColor =
            getContext().getResources().getColor(R.color.vumeter_color);
    private final int mProgressColor =
            getContext().getResources().getColor(R.color.vumeter_progress_color);
    private final int mOuterWidth = getContext().getResources().
            getDimensionPixelSize(R.dimen.vumeter_outer_width);
    private final int mOuterInnerMargin = getContext().getResources().
            getDimensionPixelSize(R.dimen.vumeter_outer_inner_margin);
    private final int mProgressWidth = getContext().getResources().
            getDimensionPixelSize(R.dimen.vumeter_progress_width);
    private final int mProgressDashedWidth = getContext().getResources().
            getDimensionPixelSize(R.dimen.vumeter_progress_dashed_width);
    private final PathEffect mDashedEffects = new DashPathEffect(
            new float[]{
                    mProgressDashedWidth,
                    mProgressWidth
            }, 0);

    Recorder mRecorder;

    public VUMeter(Context context) {
        this(context, null);
    }

    public VUMeter(Context context, AttributeSet attrs) {
        super(context, attrs);
        init(context);
    }

    void init(Context context) {
        mPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mPaint.setStyle(Paint.Style.STROKE);
        mPaint.setStrokeWidth(mProgressWidth);
        mPaint.setColor(mMainColor);

        mOuterPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mOuterPaint.setStyle(Paint.Style.STROKE);
        mOuterPaint.setStrokeWidth(mOuterWidth);
        mOuterPaint.setColor(mMainColor);

        mProgressPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mProgressPaint.setStyle(Paint.Style.STROKE);
        mProgressPaint.setStrokeWidth(mProgressWidth);
        mProgressPaint.setColor(mProgressColor);

        mDashedProgressPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mDashedProgressPaint.setStyle(Paint.Style.STROKE);
        mDashedProgressPaint.setStrokeWidth(mProgressWidth + 1); // +1 to make sure cover bottom.
        mDashedProgressPaint.setPathEffect(mDashedEffects);
        mDashedProgressPaint.setColor(mMaskColor);

        mRecorder = null;

        mCurrentAngle = MIN_ANGLE;
    }

    public void resetAngle() {
        mCurrentAngle = MIN_ANGLE;
        invalidate();
    }

    public void setRecorder(Recorder recorder) {
        mRecorder = recorder;
        invalidate();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        float angle = MIN_ANGLE;
        if (mRecorder != null)
            angle += (MAX_ANGLE - MIN_ANGLE) * mRecorder.getMaxAmplitude() / MAX_AMPLITUDE;

        if (angle >= mCurrentAngle)
            mCurrentAngle = angle;
        else
            mCurrentAngle = Math.max(angle, mCurrentAngle - DROPOFF_STEP);

        mCurrentAngle = Math.min(MAX_ANGLE, mCurrentAngle);

        float w = getWidth();
        float h = getHeight();
        float size = Math.min(w, h);

        // outer arc
        float left = (w - size) / 2 + mOuterWidth / 2;
        float top = (h - size) / 2 + mOuterWidth / 2;
        float right = left + size - mOuterWidth;
        float bottom = top + size - mOuterWidth;
        canvas.drawArc(left, top, right, bottom,
                START_DEGREES, MAX_DEGREES - MIN_DEGREES, false, mOuterPaint);

        left = left + mOuterInnerMargin;
        top = top + mOuterInnerMargin;
        right = right - mOuterInnerMargin;
        bottom = bottom - mOuterInnerMargin;
        // progress background arc
        canvas.drawArc(left, top, right, bottom,
                START_DEGREES, MAX_DEGREES - MIN_DEGREES, false, mPaint);
        // progress arc
        float sweepDegrees = radiansToDegrees(mCurrentAngle - MIN_ANGLE);
        if (sweepDegrees > 0) {
            canvas.drawArc(left, top, right, bottom,
                    START_DEGREES, sweepDegrees, false, mProgressPaint);
        }

        // progress background mask arc
        canvas.drawArc(left, top, right, bottom,
                START_DEGREES - MASK_DEGREES_OFFSET,
                MAX_DEGREES - MIN_DEGREES - MASK_DEGREES_OFFSET * 2, false, mDashedProgressPaint);

        if (mRecorder != null && mRecorder.state() == Recorder.RECORDING_STATE)
            postInvalidateDelayed(ANIMATION_INTERVAL);
    }

    private static float radiansToDegrees(double radians) {
        return (float) (radians / Math.PI * 180);
    }

    private static float degreesToRadians(float degrees) {
        return (float) (degrees / 180 * Math.PI);
    }
}
