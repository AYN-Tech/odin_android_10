/*
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
import android.gesture.GestureOverlayView;
import android.preference.PreferenceManager;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.FloatMath;
import android.util.Log;
import android.view.MotionEvent;

import java.util.HashMap;
import java.util.Map;

import com.android.mms.ui.MessagingPreferenceActivity;

/**
 * ZoomGestureOverlayView
 * <pre>
 *     The purpose of this view is to hijack the touches before the {@link
 * android.gesture.GestureOverlayView} gets a chance to handle it because it doesn't seem to
 * listen to you when you
 * call {@link android.gesture.GestureOverlayView#setEventsInterceptionEnabled(boolean)}
 * and set it to false.  I would assume this would hand me the events I wanted, but it doesn't.
 *
 * However if it is set to true you get all of the events, it just sets the action to
 *
 * @see {@link android.view.MotionEvent#ACTION_CANCEL} </pre>
 * @see {@link android.gesture.GestureOverlayView}
 */
public class ZoomGestureOverlayView extends GestureOverlayView {

    // Constants
    private static final String TAG = ZoomGestureOverlayView.class.getSimpleName();
    private static final float THRESHOLD = 30.0f;
    private static final float SCALE_FACTOR_DOWN = 0.95f;
    private static final float SCALE_FACTOR_UP = 1.1f;
    private static final boolean DEFAULT_GESTURE_MODE  = false;

    // Members
    private float mOldDistance = 0.0f; // Stash spot for the 'original distance' or 'last distance'
    private final Map<IZoomListener, IZoomListener> mZoomListenerMap =
            new HashMap<IZoomListener, IZoomListener>();

    // Flags
    private boolean mCanZoom = false;
    private Context mContext;

    /**
     * Constructor
     *
     * @param context {@link android.content.Context}
     */
    public ZoomGestureOverlayView(Context context) {
        this(context, null);
        mContext = context;
    }

    /**
     * Constructor
     *
     * @param context {@link android.content.Context}
     * @param attrs   {@link android.util.AttributeSet}
     */
    public ZoomGestureOverlayView(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
        mContext = context;
    }

    /**
     * Constructor
     *
     * @param context  {@link android.content.Context}
     * @param attrs    {@link android.util.AttributeSet}
     * @param defStyle {@link java.lang.Integer}
     */
    public ZoomGestureOverlayView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        mContext = context;
    }

    /**
     * Handle debug logging when set <code> adb shell setprop log.tag.ZoomGestureOverlayView DEBUG
     * </code>
     *
     * @param message {@link java.lang.String} if empty or null, gracefully do nothing
     */
    private void logDebug(String message) {
        if (!TextUtils.isEmpty(message)) {
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, message);
            }
        }
    }

    private Boolean enableGesture() {
        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(mContext);
        boolean enableGesture = false;
        enableGesture = prefs.getBoolean(
                MessagingPreferenceActivity.ENABLE_GESTURE, DEFAULT_GESTURE_MODE);
        return enableGesture;
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
        Log.v(TAG, "dispatchTouchEvent(" + event + ")");
        logDebug("mCanZoom = " + mCanZoom);
        logDebug("mOldDistance = " + mOldDistance);
        final int action = event.getAction() & MotionEvent.ACTION_MASK;
        switch (action) {
            case MotionEvent.ACTION_POINTER_DOWN:
                // Check pointer count, if we don't have 2 we don't need to worry
                int pointerCount = event.getPointerCount();
                if (pointerCount == 2) {
                    // Flag as able to zoom
                    mCanZoom = true;
                    // Calculate the original distance between fingers when touched
                    mOldDistance = calculateSpacing(event);
                    logDebug("mOldDistance(updated) = " + mOldDistance);
                } else {
                    // Reset can zoom on any other finger count
                    mCanZoom = false;
                }
                break;
            case MotionEvent.ACTION_MOVE:
                if (!mCanZoom || !enableGesture()) {
                    break;
                }
                // Fetch new distance
                float newDistance = calculateSpacing(event);
                // Calculate the difference, if negative we should probably zoom out
                float distanceDifference = newDistance - mOldDistance;
                // If the absolute value is greater, we want to handle it regardless of whether
                // it is positive or negative
                if (newDistance > 0 && Math.abs(distanceDifference) > THRESHOLD) {
                    logDebug("Absolute threshold satisfied!");
                    // Update old distance
                    mOldDistance = newDistance;
                    // Replace old distance with new so we can keep on zoom zoom zoomin'
                    float scale = (distanceDifference > 0) ? SCALE_FACTOR_UP : SCALE_FACTOR_DOWN;
                    fireListenersWithScale(scale);
                    return true;
                }
                break;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                mCanZoom = false;
                break;
        }
        return super.dispatchTouchEvent(event);
    }

    /**
     * Calculates the distance between fingers
     *
     * @param event {@link android.view.MotionEvent}
     *
     * @return {@link java.lang.Float}
     */
    private float calculateSpacing(MotionEvent event) {
        try {
            float x = event.getX(0) - event.getX(1);
            float y = event.getY(0) - event.getY(1);
            return (float) Math.sqrt(x * x + y * y);
        } catch (Exception e) {
            return 0.0f;
        }
    }

    /**
     * IZoomListener
     * <pre>
     *     Listen for a zoom trigger event
     * </pre>
     */
    public interface IZoomListener {
        public void onZoomWithScale(float scale);
    }

    /**
     * Add a zoom listener
     *
     * @param listener {@link com.android.mms.ui.zoom.ZoomGestureOverlayView.IZoomListener}
     *
     * @throws IllegalArgumentException {@link java.lang.IllegalArgumentException}
     */
    public void addZoomListener(IZoomListener listener) throws IllegalArgumentException {
        if (listener == null) {
            throw new IllegalArgumentException("'listener' cannot be null!");
        }
        mZoomListenerMap.put(listener, null);
    }

    /**
     * Remove a zoom listener
     *
     * @param listener {@link com.android.mms.ui.zoom.ZoomGestureOverlayView.IZoomListener}
     *
     * @throws IllegalArgumentException {@link java.lang.IllegalArgumentException}
     */
    public void removeZoomListener(IZoomListener listener) throws IllegalArgumentException {
        if (listener == null) {
            throw new IllegalArgumentException("'listener' cannot be null!");
        }
        if (mZoomListenerMap.containsKey(listener)) {
            mZoomListenerMap.remove(listener);
        }
    }

    /**
     * Fire the photon torpedos!....er I mean listeners :)
     *
     * @param scale {@link java.lang.Float}
     */
    private void fireListenersWithScale(final float scale) {
        getHandler().post(new Runnable() {
            @Override
            public void run() {
                for (IZoomListener listener : mZoomListenerMap.keySet()) {
                    listener.onZoomWithScale(scale);
                }
            }
        });
    }

}
