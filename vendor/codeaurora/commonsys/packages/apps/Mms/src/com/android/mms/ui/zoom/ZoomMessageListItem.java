/*
* Copyright (c) 2016, The Linux Foundation. All rights reserved.
* Not a Contribution.
* Copyright (C) 2014 The CyanogenMod Project
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

package com.android.mms.ui.zoom;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Handler;
import android.preference.PreferenceManager;
import android.util.AttributeSet;
import android.util.Log;
import android.util.TypedValue;
import android.widget.LinearLayout;
import android.widget.TextView;
import com.android.mms.ui.MessageListItem;
import com.android.mms.ui.MessagingPreferenceActivity;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * ZoomMessageListItem
 * <pre>
 *     Zoom logic bits
 * </pre>
 *
 * @see {@link MessageListItem}
 */
public class ZoomMessageListItem extends LinearLayout {

    // Log tag
    private static final String TAG = ZoomMessageListItem.class.getSimpleName();
    private Context sContext;
    private static float mCurrentTextSize;

    // "Zooming" constants
    private static final int MIN_FONT_SIZE = 14;  //sp
    private static final int MAX_FONT_SIZE = 24;  //sp
    private static final int INPUT_MIN_FONT_SIZE = 16;
    private static final int INPUT_MAX_FONT_SIZE = 26;
    public static final int DIFF_FONT_SIZE = 2;

    // Members
    private final List<TextView> mZoomableTextViewList = new ArrayList<TextView>();
    private final Map<TextView, Float> mOriginalTextSizes = new HashMap<>();

    /**
     * Constructor
     *
     * @param context {@link android.content.Context}
     */
    public ZoomMessageListItem(Context context) {
        super(context);
        sContext = context;
    }

    /**
     * Constructor
     *
     * @param context {@link android.content.Context}
     * @param attrs   {@link android.util.AttributeSet}
     */
    public ZoomMessageListItem(Context context, AttributeSet attrs) {
        super(context, attrs);
        sContext = context;
    }

    /**
     * Add a text view to be zoomed
     *
     * @param textView {@link android.widget.TextView}
     */
    public void addZoomableTextView(TextView textView) {
        if (textView == null) {
            return;
        }
        if (!mZoomableTextViewList.contains(textView)) {
            mZoomableTextViewList.add(textView);
            mOriginalTextSizes.put(textView, textView.getTextSize());
        }
    }

    /**
     * Accept the scale to use and handle "zooming" the text to the given scale
     *
     * @param scale {@link java.lang.Float}
     */
    public void handleZoomWithScale(final float scale) {
        getHandler().post(new Runnable() {
            @Override
            public void run() {
                for (TextView textView : mZoomableTextViewList) {
                    zoomViewByScale(sContext, textView, scale, true);
                }
            }
        });
    }

    /**
     * This will "zoom" the text by changing the font size based ont he given scale for the given
     * view
     *
     * @param context {@link android.content.Context}
     * @param view    {@link android.widget.TextView}
     * @param scale   {@link java.lang.Float}
     */
    public static void zoomViewByScale(Context context, TextView view,
                                       float scale, boolean isEditorText) {
        if (view == null) {
            Log.w(TAG, "'view' is null!");
            return;
        }
        int maxSize;
        int minSize;
        if (isEditorText) {
            maxSize = INPUT_MAX_FONT_SIZE;
            minSize = INPUT_MIN_FONT_SIZE;
        } else {
            maxSize = MAX_FONT_SIZE;
            minSize = MIN_FONT_SIZE;
        }
        // getTextSize() returns absolute pixels
        // convert to scaled for proper math flow
        float currentTextSize = pixelsToSp(context, view.getTextSize());
        // Calculate based on the scale (1.1 and 0.95 in this case)
        float calculatedSize = currentTextSize * scale;
        // Limit max and min
        if (calculatedSize > maxSize) {
            currentTextSize = maxSize;
        } else if (calculatedSize < minSize) {
            currentTextSize = minSize;
        } else {
            // Specify the calculated if we are within the reasonable bounds
            currentTextSize = calculatedSize;
        }
        // Cast to int in order to normalize it
        // setTextSize takes a Scaled Pixel value
        view.setTextSize((int) currentTextSize);
        mCurrentTextSize = currentTextSize;

    }

    /**
     * Convert absolute pixels to scaled pixels based on density
     *
     * @param px {@link java.lang.Float}
     *
     * @return {@link java.lang.Float}
     */
    private static float pixelsToSp(Context context, float px) {
        float scaledDensity = context.getResources().getDisplayMetrics().scaledDensity;
        return px / scaledDensity;
    }

    /**
     * Accept the font size to use and handle "zooming" the text to the given scale
     *
     * @param fontSize {@link java.lang.Integer}
     */
    public void setZoomScale(final float scale, final boolean isEditorText) {
        Handler handler = getHandler();
        if (handler != null) {
            handler.post(new Runnable() {
                @Override
                public void run() {
                    for (TextView textView : mZoomableTextViewList) {
                        zoomViewByScale(getContext(), textView, scale, isEditorText);
                        setZoomScalePreference();
                    }
                }
            });
        }
    }

    private void setZoomScalePreference() {
        SharedPreferences.Editor editor = PreferenceManager
                .getDefaultSharedPreferences(sContext).edit();
        editor.putFloat(MessagingPreferenceActivity.ZOOM_MESSAGE, mCurrentTextSize);
        editor.apply();
    }

}
