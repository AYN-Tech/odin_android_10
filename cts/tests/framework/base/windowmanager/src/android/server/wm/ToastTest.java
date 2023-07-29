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

package android.server.wm;

import static android.server.wm.app.Components.ToastReceiver.ACTION_TOAST_DISPLAYED;
import static android.server.wm.app.Components.ToastReceiver.ACTION_TOAST_TAP_DETECTED;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.ConditionVariable;
import android.os.Handler;
import android.os.Looper;
import android.platform.test.annotations.Presubmit;
import android.provider.Settings;
import android.server.wm.WindowManagerState.WindowState;
import android.server.wm.app.Components;
import android.view.WindowManager.LayoutParams;

import com.android.compatibility.common.util.SystemUtil;

import org.junit.Test;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

import javax.annotation.Nullable;

@Presubmit
public class ToastTest extends ActivityManagerTestBase {
    private static final String SETTING_HIDDEN_API_POLICY = "hidden_api_policy";
    private static final long TOAST_DISPLAY_TIMEOUT_MS = 8000;
    private static final long TOAST_TAP_TIMEOUT_MS = 3500;

    /**
     * Tests can be executed as soon as the device has booted. When that happens the broadcast queue
     * is long and it takes some time to process the broadcast we just sent.
     */
    private static final long BROADCAST_DELIVERY_TIMEOUT_MS = 60000;

    @Nullable
    private String mPreviousHiddenApiPolicy;
    private Map<String, ConditionVariable> mBroadcastsReceived;

    private BroadcastReceiver mAppCommunicator = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
             getBroadcastReceivedVariable(intent.getAction()).open();
        }
    };

    @Override
    public void setUp() throws Exception {
        super.setUp();
        SystemUtil.runWithShellPermissionIdentity(() -> {
            ContentResolver resolver = mContext.getContentResolver();
            mPreviousHiddenApiPolicy = Settings.Global.getString(resolver,
                    SETTING_HIDDEN_API_POLICY);
            Settings.Global.putString(resolver, SETTING_HIDDEN_API_POLICY, "1");
        });
        // Stopping just in case, to make sure reflection is allowed
        stopTestPackage(Components.getPackageName());

        // These are parallel broadcasts, not affected by a busy queue
        mBroadcastsReceived = Collections.synchronizedMap(new HashMap<>());
        IntentFilter filter = new IntentFilter();
        filter.addAction(ACTION_TOAST_DISPLAYED);
        filter.addAction(ACTION_TOAST_TAP_DETECTED);
        mContext.registerReceiver(mAppCommunicator, filter);
    }

    @Override
    public void tearDown() throws Exception {
        mContext.unregisterReceiver(mAppCommunicator);
        SystemUtil.runWithShellPermissionIdentity(() -> {
            Settings.Global.putString(mContext.getContentResolver(), SETTING_HIDDEN_API_POLICY,
                    mPreviousHiddenApiPolicy);
        });
        super.tearDown();
    }

    @Test
    public void testToastIsNotClickable() {
        Intent intent = new Intent();
        intent.setComponent(Components.TOAST_RECEIVER);
        sendAndWaitForBroadcast(intent);
        boolean toastDisplayed = getBroadcastReceivedVariable(ACTION_TOAST_DISPLAYED).block(
                TOAST_DISPLAY_TIMEOUT_MS);
        assertTrue("Toast not displayed on time", toastDisplayed);
        WindowManagerState wmState = getAmWmState().getWmState();
        wmState.computeState();
        WindowState toastWindow = wmState.findFirstWindowWithType(LayoutParams.TYPE_TOAST);
        assertNotNull("Couldn't retrieve toast window", toastWindow);

        tapOnCenter(toastWindow.getContainingFrame(), toastWindow.getDisplayId());

        boolean toastClicked = getBroadcastReceivedVariable(ACTION_TOAST_TAP_DETECTED).block(
                TOAST_TAP_TIMEOUT_MS);
        assertFalse("Toast tap detected", toastClicked);
    }

    private void sendAndWaitForBroadcast(Intent intent) {
        assertNotEquals("Can't wait on main thread", Thread.currentThread(),
                Looper.getMainLooper().getThread());

        ConditionVariable broadcastDelivered = new ConditionVariable(false);
        mContext.sendOrderedBroadcast(
                intent,
                null,
                new BroadcastReceiver() {
                    @Override
                    public void onReceive(Context context, Intent intent) {
                        broadcastDelivered.open();
                    }
                },
                new Handler(Looper.getMainLooper()),
                Activity.RESULT_OK,
                null,
                null);
        broadcastDelivered.block(BROADCAST_DELIVERY_TIMEOUT_MS);
    }

    private ConditionVariable getBroadcastReceivedVariable(String action) {
        return mBroadcastsReceived.computeIfAbsent(action, key -> new ConditionVariable());
    }
}
