/*
 * Copyright (C) 2017 The Android Open Source Project
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
package com.android.cts.deviceandprofileowner;

import android.app.ActivityManager;
import android.app.ActivityOptions;
import android.app.admin.DevicePolicyManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.Build;
import android.os.Bundle;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.Until;
import android.telecom.TelecomManager;
import android.util.Log;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Test class that is meant to be driven from the host and can't be run alone, which is required
 * for tests that include rebooting or other connection-breaking steps. For this reason, this class
 * does not override tearDown and setUp just initializes the test state, changing nothing in the
 * device. Therefore, the host is responsible for making sure the tests leave the device in a clean
 * state after running.
 */
@RunWith(AndroidJUnit4.class)
public class LockTaskHostDrivenTest extends BaseDeviceAdminTest {

    private static final String TAG = LockTaskHostDrivenTest.class.getName();
    private static final int ACTIVITY_RESUMED_TIMEOUT_MILLIS = 20000;  // 20 seconds
    private static final String ACTION_EMERGENCY_DIAL = "com.android.phone.EmergencyDialer.DIAL";
    private static final String LOCK_TASK_ACTIVITY
            = LockTaskUtilityActivityIfWhitelisted.class.getName();

    private UiDevice mUiDevice;
    private Context mContext;
    private PackageManager mPackageManager;
    private ActivityManager mActivityManager;
    private TelecomManager mTelcomManager;
    private DevicePolicyManager mDevicePolicyManager;

    @Before
    public void setUp() {
        mUiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        mContext = InstrumentationRegistry.getContext();
        mPackageManager = mContext.getPackageManager();
        mActivityManager = mContext.getSystemService(ActivityManager.class);
        mTelcomManager = mContext.getSystemService(TelecomManager.class);
        mDevicePolicyManager = mContext.getSystemService(DevicePolicyManager.class);
    }

    @Test
    public void startLockTask() throws Exception {
        Log.d(TAG, "startLockTask on host-driven test (no cleanup)");
        setLockTaskPackages(mContext.getPackageName());
        setDefaultHomeIntentReceiver();
        launchLockTaskActivity();
        mUiDevice.waitForIdle();
    }

    @Test
    public void cleanupLockTask() {
        Log.d(TAG, "cleanupLockTask on host-driven test");
        mDevicePolicyManager.clearPackagePersistentPreferredActivities(
                ADMIN_RECEIVER_COMPONENT,
                mContext.getPackageName());
        setLockTaskPackages();
        mDevicePolicyManager.setLockTaskFeatures(ADMIN_RECEIVER_COMPONENT, 0);
        // In case some activity is still in foreground
        mUiDevice.pressHome();
    }

    /**
     * On low-RAM devices, this test can take too long to finish, so the test runner can incorrectly
     * assume it's finished. Therefore, only use it once in a given test.
     */
    @Test
    public void testLockTaskIsActiveAndCantBeInterrupted() throws Exception {
        Log.d(TAG, "testLockTaskIsActiveAndCantBeInterrupted on host-driven test");
        waitAndCheckLockedActivityIsResumed();
        checkLockedActivityIsRunning();

        mUiDevice.pressBack();
        mUiDevice.waitForIdle();
        checkLockedActivityIsRunning();

        mUiDevice.pressHome();
        mUiDevice.waitForIdle();
        checkLockedActivityIsRunning();

        mUiDevice.pressRecentApps();
        mUiDevice.waitForIdle();
        checkLockedActivityIsRunning();

        mUiDevice.waitForIdle();
    }

    @Test
    public void testLockTaskIsExitedIfNotWhitelisted() throws Exception {
        Log.d(TAG, "testLockTaskIsExitedIfNotWhitelisted on host-driven test");

        // Whitelist this package
        setLockTaskPackages(mContext.getPackageName());

        // Launch lock task root activity
        setDefaultHomeIntentReceiver();
        launchLockTaskActivity();
        waitAndCheckLockedActivityIsResumed();
        assertEquals(
                ActivityManager.LOCK_TASK_MODE_LOCKED, mActivityManager.getLockTaskModeState());

        // Remove it from whitelist
        setLockTaskPackages();
        mUiDevice.waitForIdle();

        // The activity should be finished and exit lock task mode
        waitAndCheckLockedActivityIsPaused();
        assertEquals(ActivityManager.LOCK_TASK_MODE_NONE, mActivityManager.getLockTaskModeState());
    }

    @Test
    public void testLockTaskCanLaunchDefaultDialer() throws Exception {
        if (!hasTelephonyFeature()) {
            Log.d(TAG, "testLockTaskCanLaunchDefaultDialer skipped");
            return;
        }

        Log.d(TAG, "testLockTaskCanLaunchDefaultDialer on host-driven test");

        // Whitelist dialer package
        String dialerPackage = mTelcomManager.getSystemDialerPackage();
        assertNotNull(dialerPackage);
        setLockTaskPackages(mContext.getPackageName(), dialerPackage);

        // Launch lock task root activity
        setDefaultHomeIntentReceiver();
        launchLockTaskActivity();
        waitAndCheckLockedActivityIsResumed();
        assertEquals(
                ActivityManager.LOCK_TASK_MODE_LOCKED, mActivityManager.getLockTaskModeState());

        // Launch dialer
        launchDialerIntoLockTaskMode(dialerPackage);

        // Wait until dialer package starts
        mUiDevice.wait(
                Until.hasObject(By.pkg(dialerPackage).depth(0)),
                ACTIVITY_RESUMED_TIMEOUT_MILLIS);
        mUiDevice.waitForIdle();
        waitAndCheckLockedActivityIsPaused();

        // But still in LockTask mode
        assertEquals(
                ActivityManager.LOCK_TASK_MODE_LOCKED,
                mActivityManager.getLockTaskModeState());
    }

    @Test
    public void testLockTaskCanLaunchEmergencyDialer() throws Exception {
        if (!hasTelephonyFeature()) {
            Log.d(TAG, "testLockTaskCanLaunchEmergencyDialer skipped");
            return;
        }

        // Find dialer package
        String dialerPackage = getEmergencyDialerPackageName();
        if (dialerPackage == null || dialerPackage.isEmpty()) {
            Log.d(TAG, "testLockTaskCanLaunchEmergencyDialer skipped since no emergency dialer");
            return;
        }

        Log.d(TAG, "testLockTaskCanLaunchEmergencyDialer on host-driven test");

        // Emergency dialer should be usable as long as keyguard feature is enabled
        // regardless of the package whitelist
        mDevicePolicyManager.setLockTaskFeatures(
                ADMIN_RECEIVER_COMPONENT, DevicePolicyManager.LOCK_TASK_FEATURE_KEYGUARD);
        setLockTaskPackages(mContext.getPackageName());

        // Launch lock task root activity
        setDefaultHomeIntentReceiver();
        launchLockTaskActivity();
        waitAndCheckLockedActivityIsResumed();
        assertEquals(
                ActivityManager.LOCK_TASK_MODE_LOCKED, mActivityManager.getLockTaskModeState());

        // Launch dialer
        launchEmergencyDialer();

        // Wait until dialer package starts
        mUiDevice.wait(
                Until.hasObject(By.pkg(dialerPackage).depth(0)),
                ACTIVITY_RESUMED_TIMEOUT_MILLIS);
        mUiDevice.waitForIdle();
        waitAndCheckLockedActivityIsPaused();

        // But still in LockTask mode
        assertEquals(
                ActivityManager.LOCK_TASK_MODE_LOCKED,
                mActivityManager.getLockTaskModeState());
    }

    private boolean hasTelephonyFeature() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP &&
                mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY);
    }

    private void checkLockedActivityIsRunning() {
        String activityName =
                mActivityManager.getAppTasks().get(0).getTaskInfo().topActivity.getClassName();
        assertEquals(LOCK_TASK_ACTIVITY, activityName);
        assertEquals(
                ActivityManager.LOCK_TASK_MODE_LOCKED, mActivityManager.getLockTaskModeState());
    }

    private void waitAndCheckLockedActivityIsResumed() throws Exception {
        mUiDevice.waitForIdle();
        assertTrue(
                LockTaskUtilityActivity.waitUntilActivityResumed(ACTIVITY_RESUMED_TIMEOUT_MILLIS));
    }

    private void waitAndCheckLockedActivityIsPaused() throws Exception {
        mUiDevice.waitForIdle();
        assertTrue(
                LockTaskUtilityActivity.waitUntilActivityPaused(ACTIVITY_RESUMED_TIMEOUT_MILLIS));
    }

    private void launchDialerIntoLockTaskMode(String dialerPackage) {
        Intent intent = new Intent(Intent.ACTION_DIAL)
                .setPackage(dialerPackage)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        Bundle options = ActivityOptions.makeBasic().setLockTaskEnabled(true).toBundle();
        mContext.startActivity(intent, options);
    }

    private void launchEmergencyDialer() {
        Intent intent = new Intent(ACTION_EMERGENCY_DIAL).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mContext.startActivity(intent);
    }

    private String getEmergencyDialerPackageName() {
        Intent intent = new Intent(ACTION_EMERGENCY_DIAL).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        ResolveInfo dialerInfo =
                mPackageManager.resolveActivity(intent, PackageManager.MATCH_DEFAULT_ONLY);
        return (dialerInfo != null) ? dialerInfo.activityInfo.packageName : null;
    }

    private void launchLockTaskActivity() {
        Intent intent = new Intent(mContext, LockTaskUtilityActivityIfWhitelisted.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(LockTaskUtilityActivity.START_LOCK_TASK, true);
        mContext.startActivity(intent);
    }

    private void setLockTaskPackages(String... packages) {
        mDevicePolicyManager.setLockTaskPackages(ADMIN_RECEIVER_COMPONENT, packages);
    }

    private void setDefaultHomeIntentReceiver() {
        IntentFilter intentFilter = new IntentFilter(Intent.ACTION_MAIN);
        intentFilter.addCategory(Intent.CATEGORY_HOME);
        intentFilter.addCategory(Intent.CATEGORY_DEFAULT);
        mDevicePolicyManager.addPersistentPreferredActivity(
                ADMIN_RECEIVER_COMPONENT, intentFilter,
                new ComponentName(mContext.getPackageName(), LOCK_TASK_ACTIVITY));
    }
}
