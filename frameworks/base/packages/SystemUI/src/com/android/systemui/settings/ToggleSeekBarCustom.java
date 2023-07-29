/*
 * Copyright (C) 2014 The Android Open Source Project
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

package com.android.systemui.settings;

import android.content.Context;
import android.content.Intent;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.SeekBar;

import com.android.settingslib.RestrictedLockUtils;
import com.android.systemui.Dependency;
import com.android.systemui.plugins.ActivityStarter;

import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;
import android.graphics.PorterDuff;
import com.android.systemui.R;

public class ToggleSeekBarCustom extends SeekBar {
    private String mAccessibilityLabel;
	
	private Paint textWarnningPaint;
	private Drawable mProgressDraw;
	private Drawable mProgressBgDraw;
	private int warnningColor= R.color.warnning;
	
    private RestrictedLockUtils.EnforcedAdmin mEnforcedAdmin = null;

    public ToggleSeekBarCustom(Context context) {
        super(context);
    }

    public ToggleSeekBarCustom(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public ToggleSeekBarCustom(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
		//mWarningPaint=new Paint();
        if (mEnforcedAdmin != null) {
            Intent intent = RestrictedLockUtils.getShowAdminSupportDetailsIntent(
                    mContext, mEnforcedAdmin);
            Dependency.get(ActivityStarter.class).postStartActivityDismissingKeyguard(intent, 0);
            return true;
        }
        if (!isEnabled()) {
            setEnabled(true);
        }

        return super.onTouchEvent(event);
    }

    public void setAccessibilityLabel(String label) {
        mAccessibilityLabel = label;
    }

    @Override
    public void onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo info) {
        super.onInitializeAccessibilityNodeInfo(info);
        if (mAccessibilityLabel != null) {
            info.setText(mAccessibilityLabel);
        }
    }

    public void setEnforcedAdmin(RestrictedLockUtils.EnforcedAdmin admin) {
        mEnforcedAdmin = admin;
    }
	
    @Override
    protected synchronized void onDraw(Canvas canvas) {
        super.onDraw(canvas); 
	    float progressRatio = (float)getProgress()/getMax();
	    System.out.println("branBright--ToggleSeekBarCustom  onDraw() progressRatio="+progressRatio);
	    if(progressRatio >= 0.69){
		    getDrawableBac();
		    getTextPaint();
		    canvas.drawText(mContext.getString(R.string.high_power_consumption),getWidth()/2,getHeight()*3/4-10,textWarnningPaint);
		    mProgressDraw.setColorFilter(getResources().getColor(R.color.warnning), PorterDuff.Mode.SRC);		
	    }else{
		    getProgressDrawable().clearColorFilter();
	    }
    }

    private void getDrawableBac() {
        LayerDrawable mLayerDrawable=(LayerDrawable) getProgressDrawable();
        mProgressDraw = mLayerDrawable.getDrawable(1);
		mProgressBgDraw = mLayerDrawable.getDrawable(0);
    }
	
    private void getTextPaint() {
        textWarnningPaint=new Paint();
        textWarnningPaint.setColor(Color.WHITE);
        textWarnningPaint.setTextSize(getHeight()/2);
        textWarnningPaint.setStyle(Paint.Style.FILL);
        textWarnningPaint.setFlags(Paint.ANTI_ALIAS_FLAG);
        textWarnningPaint.setAntiAlias(true);
        textWarnningPaint.setTextAlign(Paint.Align.CENTER);
    }
}
