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
import android.util.AttributeSet;
import android.view.View;
import android.widget.ListView;

import com.android.mms.ui.MessageListItem;

/**
 * ZoomMessageListView
 * <pre>
 *     Extension to handle a call for resizing its children
 * </pre>
 *
 * @see {@link android.widget.ListView}
 */
public class ZoomMessageListView extends ListView {

    public ZoomMessageListView(Context context) {
        super(context);
    }

    public ZoomMessageListView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public ZoomMessageListView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    public ZoomMessageListView(Context context, AttributeSet attrs, int defStyleAttr,
            int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);
    }

    /**
     * This function iterates the existing child views and delegates the zooming work onto the view
     * itself.
     *
     * @param scale {@link java.lang.Float}
     */
    public void handleZoomWithScale(float scale, boolean isEditorText) {
        int viewCount = getChildCount();
        for (int i = 0; i < viewCount; i++) {
            View view = getChildAt(i);
            if (view instanceof ZoomMessageListItem) {
                ((ZoomMessageListItem) view).setZoomScale(scale, isEditorText);
            }
        }
    }

}
