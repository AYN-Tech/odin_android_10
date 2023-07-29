/*
* Copyright (c) 2016, The Linux Foundation. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
* * Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
* * Redistributions in binary form must reproduce the above
* copyright notice, this list of conditions and the following
* disclaimer in the documentation and/or other materials provided
* with the distribution.
* * Neither the name of The Linux Foundation nor the names of its
* contributors may be used to endorse or promote products derived
* from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

package com.android.mms.ui;

import android.os.CountDownTimer;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;

import com.android.mms.LogTag;

public abstract class OnItemDoubleClickListener implements AdapterView.OnItemClickListener {
    private static final String TAG = "DClickListener";
    private static final int TIME_INTERVAL = 300;
    private static final int STATUS_IDLE = 0;
    private static final int STATUS_WAIT_NEXT_CLICK = 1;
    private static final int STATUS_CLICK_HANDLNG = 2;
    private View mClickedView = null;
    private CountDownTimer mTimer;
    private int mState = STATUS_IDLE;

    @Override
    public void onItemClick(AdapterView<?> adapterView, View view, int i, long l) {
        logV("onItemClick mState = " + mState);
        switch (mState) {
            case STATUS_IDLE:
                waitForNextClick(adapterView, view, i, l);
                break;
            case STATUS_WAIT_NEXT_CLICK:
                handleClick(adapterView, view, i, l);
                if (mClickedView == view) {
                    logD("onItemDoubleClick");
                    onItemDoubleClick(adapterView, view, i, l);
                } else {
                    logD("Different view onItemSingleClick!");
                    onItemSingleClick(adapterView, view, i, l);
                }
                break;
            case STATUS_CLICK_HANDLNG:
                //Do nothing to avoid triple or more clicks in a short time.
                break;
            default:
                break;
        }
    }

    private void waitForNextClick(final AdapterView<?> parent, final View view,
                                  final int position, final long id) {
        mState = STATUS_WAIT_NEXT_CLICK;
        mClickedView = view;
        mTimer = new CountDownTimer(TIME_INTERVAL, TIME_INTERVAL) {
            @Override
            public void onTick(long l) {

            }

            @Override
            public void onFinish() {
                logD("onItemSingleClick");
                handleClick(parent, view, position, id);
                onItemSingleClick(parent, view, position, id);
            }
        };
        mTimer.start();
    }

    private void handleClick(final AdapterView<?> parent, final View view,
                             final int position, final long id) {
        mState = STATUS_CLICK_HANDLNG;
        if (mTimer != null) {
            logV("handleClick reset timer");
            mTimer.cancel();
            mTimer = null;
        }
        mTimer = new CountDownTimer(TIME_INTERVAL, TIME_INTERVAL) {
            @Override
            public void onTick(long l) {

            }

            @Override
            public void onFinish() {
                logV("handleClick onFinish");
                mState = STATUS_IDLE;
                mClickedView = null;
            }
        };
        mTimer.start();
    }

    private void logD(String message) {
        if (LogTag.DEBUG) {
            Log.d(TAG, message);
        }
    }

    private void logV(String message) {
        if (LogTag.VERBOSE) {
            Log.v(TAG, message);
        }
    }

    public abstract void onItemSingleClick(AdapterView<?> parent, View view, int position, long id);

    public abstract void onItemDoubleClick(AdapterView<?> parent, View view, int position, long id);
}
