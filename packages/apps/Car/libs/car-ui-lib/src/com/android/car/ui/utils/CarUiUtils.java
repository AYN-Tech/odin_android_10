/*
 * Copyright (C) 2019 The Android Open Source Project
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
package com.android.car.ui.utils;

import android.app.Activity;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.util.TypedValue;

import androidx.annotation.DimenRes;
import androidx.annotation.Nullable;
import androidx.annotation.StyleRes;

/**
 * Collection of utility methods
 */
public final class CarUiUtils {
    /** This is a utility class */
    private CarUiUtils() {}

    /**
     * Reads a float value from a dimens resource. This is necessary as {@link Resources#getFloat}
     * is not currently public.
     *
     * @param res {@link Resources} to read values from
     * @param resId Id of the dimens resource to read
     */
    public static float getFloat(Resources res, @DimenRes int resId) {
        TypedValue outValue = new TypedValue();
        res.getValue(resId, outValue, true);
        return outValue.getFloat();
    }

    /** Returns the identifier of the resolved resource assigned to the given attribute. */
    public static int getAttrResourceId(Context context, int attr) {
        return getAttrResourceId(context, /*styleResId=*/ 0, attr);
    }

    /**
     * Returns the identifier of the resolved resource assigned to the given attribute defined in
     * the given style.
     */
    public static int getAttrResourceId(Context context, @StyleRes int styleResId, int attr) {
        TypedArray ta = context.obtainStyledAttributes(styleResId, new int[]{attr});
        int resId = ta.getResourceId(0, 0);
        ta.recycle();
        return resId;
    }

    /**
     * Gets the {@link Activity} for a certain {@link Context}.
     *
     * <p>It is possible the Context is not associated with an Activity, in which case
     * this method will return null.
     */
    @Nullable
    public static Activity getActivity(Context context) {
        while (context instanceof ContextWrapper) {
            if (context instanceof Activity) {
                return (Activity) context;
            }
            context = ((ContextWrapper) context).getBaseContext();
        }
        return null;
    }
}
