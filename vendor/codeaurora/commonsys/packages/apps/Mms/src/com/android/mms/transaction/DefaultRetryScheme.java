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

package com.android.mms.transaction;

import com.android.mms.LogTag;
import com.android.mms.R;

import android.content.Context;
import android.util.Config;
import android.util.Log;

import com.android.mms.R;

/**
 * Default retry scheme, based on specs.
 */
public class DefaultRetryScheme extends AbstractRetryScheme {
    private static final String TAG = LogTag.TAG;
    private static final boolean DEBUG = false;
    private static final boolean LOCAL_LOGV = DEBUG ? Config.LOGD : Config.LOGV;

    private int mCustomeRetryTimes = -1;

    private static int[] sDefaultRetryScheme;

    public DefaultRetryScheme(Context context, int retriedTimes) {
        super(retriedTimes);

        sDefaultRetryScheme = context.getResources().getIntArray(R.array.retry_scheme);
        mRetriedTimes = mRetriedTimes < 0 ? 0 : mRetriedTimes;
        mRetriedTimes = mRetriedTimes >= sDefaultRetryScheme.length
                ? sDefaultRetryScheme.length - 1 : mRetriedTimes;

        mCustomeRetryTimes = context.getResources().getInteger(R.integer.def_retry_times);
    }

    @Override
    public int getRetryLimit() {
        if (mCustomeRetryTimes == -1) {
            return sDefaultRetryScheme.length;
        } else {
            return mCustomeRetryTimes;
        }
    }

    @Override
    public long getWaitingInterval() {
        if (LOCAL_LOGV) {
            Log.v(TAG, "Next int: " + sDefaultRetryScheme[mRetriedTimes]);
        }
        return sDefaultRetryScheme[mRetriedTimes];
    }
}
