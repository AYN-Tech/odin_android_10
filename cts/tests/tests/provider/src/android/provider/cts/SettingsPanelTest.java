/*
 * Copyright (C) 2018 The Android Open Source Project
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

package android.provider.cts;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assume.assumeTrue;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.provider.Settings;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject2;
import android.support.test.uiautomator.Until;

import com.android.compatibility.common.util.RequiredServiceRule;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Arrays;

/**
 * Tests related SettingsPanels:
 *
 * atest SettingsPanelTest
 */
@MediumTest
@RunWith(AndroidJUnit4.class)
public class SettingsPanelTest {

    private static final int TIMEOUT = 8000;

    private static final String RESOURCE_DONE = "done";
    private static final String RESOURCE_SEE_MORE = "see_more";
    private static final String RESOURCE_TITLE = "panel_title";

    private String mSettingsPackage = "com.android.settings";
    private String mLauncherPackage;

    private Context mContext;

    private UiDevice mDevice;

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getTargetContext();
        mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());

        PackageManager packageManager = mContext.getPackageManager();
        Intent launcherIntent = new Intent(Intent.ACTION_MAIN);
        launcherIntent.addCategory(Intent.CATEGORY_HOME);
        mLauncherPackage = packageManager.resolveActivity(launcherIntent,
                PackageManager.MATCH_DEFAULT_ONLY).activityInfo.packageName;

        if (packageManager.hasSystemFeature(PackageManager.FEATURE_AUTOMOTIVE)) {
            mSettingsPackage = "com.android.car.settings";
        }
    }

    @After
    public void cleanUp() {
        mDevice.pressHome();
        mDevice.wait(Until.hasObject(By.pkg(mLauncherPackage).depth(0)), TIMEOUT);
    }

    // Check correct package is opened

    @Test
    public void internetPanel_correctPackage() {
        launchInternetPanel();

        String currentPackage = mDevice.getCurrentPackageName();

        assertThat(currentPackage).isEqualTo(mSettingsPackage);
    }

    @Test
    public void volumePanel_correctPackage() {
        launchVolumePanel();

        String currentPackage = mDevice.getCurrentPackageName();

        assertThat(currentPackage).isEqualTo(mSettingsPackage);
    }

    @Test
    public void nfcPanel_correctPackage() {
        launchNfcPanel();

        String currentPackage = mDevice.getCurrentPackageName();

        assertThat(currentPackage).isEqualTo(mSettingsPackage);
    }

    @Test
    public void wifiPanel_correctPackage() {
        launchWifiPanel();

        String currentPackage = mDevice.getCurrentPackageName();

        assertThat(currentPackage).isEqualTo(mSettingsPackage);
    }

    @Test
    public void internetPanel_doneClosesPanel() {
        // Launch panel
        launchInternetPanel();
        String currentPackage = mDevice.getCurrentPackageName();
        assertThat(currentPackage).isEqualTo(mSettingsPackage);

        // Click the done button
        mDevice.findObject(By.res(mSettingsPackage, RESOURCE_DONE)).click();
        mDevice.wait(Until.hasObject(By.pkg(mLauncherPackage).depth(0)), TIMEOUT);

        // Assert that we have left the panel
        currentPackage = mDevice.getCurrentPackageName();
        assertThat(currentPackage).isNotEqualTo(mSettingsPackage);
    }

    @Test
    public void volumePanel_doneClosesPanel() {
        // Launch panel
        launchVolumePanel();
        String currentPackage = mDevice.getCurrentPackageName();
        assertThat(currentPackage).isEqualTo(mSettingsPackage);

        // Click the done button
        mDevice.findObject(By.res(mSettingsPackage, RESOURCE_DONE)).click();
        mDevice.wait(Until.hasObject(By.pkg(mLauncherPackage).depth(0)), TIMEOUT);

        // Assert that we have left the panel
        currentPackage = mDevice.getCurrentPackageName();
        assertThat(currentPackage).isNotEqualTo(mSettingsPackage);
    }

    @Test
    public void nfcPanel_doneClosesPanel() {
        // Launch panel
        launchNfcPanel();
        String currentPackage = mDevice.getCurrentPackageName();
        assertThat(currentPackage).isEqualTo(mSettingsPackage);

        // Click the done button
        mDevice.findObject(By.res(mSettingsPackage, RESOURCE_DONE)).click();
        mDevice.wait(Until.hasObject(By.pkg(mLauncherPackage).depth(0)), TIMEOUT);

        // Assert that we have left the panel
        currentPackage = mDevice.getCurrentPackageName();
        assertThat(currentPackage).isNotEqualTo(mSettingsPackage);
    }

    @Test
    public void wifiPanel_doneClosesPanel() {
        // Launch panel
        launchWifiPanel();
        String currentPackage = mDevice.getCurrentPackageName();
        assertThat(currentPackage).isEqualTo(mSettingsPackage);

        // Click the done button
        mDevice.findObject(By.res(mSettingsPackage, RESOURCE_DONE)).click();
        mDevice.wait(Until.hasObject(By.pkg(mLauncherPackage).depth(0)), TIMEOUT);

        // Assert that we have left the panel
        currentPackage = mDevice.getCurrentPackageName();
        assertThat(currentPackage).isNotEqualTo(mSettingsPackage);
    }

    @Test
    public void internetPanel_seeMoreButton_launchesIntoSettings() {
        // Launch panel
        launchInternetPanel();
        String currentPackage = mDevice.getCurrentPackageName();
        assertThat(currentPackage).isEqualTo(mSettingsPackage);

        // Click the see more button
        mDevice.findObject(By.res(mSettingsPackage, RESOURCE_SEE_MORE)).click();
        mDevice.wait(Until.hasObject(By.pkg(mSettingsPackage).depth(0)), TIMEOUT);

        // Assert that we're still in Settings, on a different page.
        currentPackage = mDevice.getCurrentPackageName();
        assertThat(currentPackage).isEqualTo(mSettingsPackage);
        UiObject2 titleView = mDevice.findObject(By.res(mSettingsPackage, RESOURCE_TITLE));
        assertThat(titleView).isNull();
    }

    @Test
    public void volumePanel_seeMoreButton_launchesIntoSettings() {
        // Launch panel
        launchVolumePanel();
        String currentPackage = mDevice.getCurrentPackageName();
        assertThat(currentPackage).isEqualTo(mSettingsPackage);

        // Click the see more button
        mDevice.findObject(By.res(mSettingsPackage, RESOURCE_SEE_MORE)).click();
        mDevice.wait(Until.hasObject(By.pkg(mSettingsPackage).depth(0)), TIMEOUT);

        // Assert that we're still in Settings, on a different page.
        currentPackage = mDevice.getCurrentPackageName();
        assertThat(currentPackage).isEqualTo(mSettingsPackage);
        UiObject2 titleView = mDevice.findObject(By.res(mSettingsPackage, RESOURCE_TITLE));
        assertThat(titleView).isNull();
    }

    @Test
    public void nfcPanel_seeMoreButton_launchesIntoSettings() {
        // Launch panel
        launchNfcPanel();
        String currentPackage = mDevice.getCurrentPackageName();
        assertThat(currentPackage).isEqualTo(mSettingsPackage);

        // Click the see more button
        mDevice.findObject(By.res(mSettingsPackage, RESOURCE_SEE_MORE)).click();
        mDevice.wait(Until.hasObject(By.pkg(mSettingsPackage).depth(0)), TIMEOUT);

        // Assert that we're still in Settings, on a different page.
        currentPackage = mDevice.getCurrentPackageName();
        assertThat(currentPackage).isEqualTo(mSettingsPackage);
        UiObject2 titleView = mDevice.findObject(By.res(mSettingsPackage, RESOURCE_TITLE));
        assertThat(titleView).isNull();
    }

    @Test
    public void wifiPanel_seeMoreButton_launchesIntoSettings() {
        // Launch panel
        launchWifiPanel();
        String currentPackage = mDevice.getCurrentPackageName();
        assertThat(currentPackage).isEqualTo(mSettingsPackage);

        // Click the see more button
        mDevice.findObject(By.res(mSettingsPackage, RESOURCE_SEE_MORE)).click();
        mDevice.wait(Until.hasObject(By.pkg(mSettingsPackage).depth(0)), TIMEOUT);

        // Assert that we're still in Settings, on a different page.
        currentPackage = mDevice.getCurrentPackageName();
        assertThat(currentPackage).isEqualTo(mSettingsPackage);
        UiObject2 titleView = mDevice.findObject(By.res(mSettingsPackage, RESOURCE_TITLE));
        assertThat(titleView).isNull();
    }

    private void launchVolumePanel() {
        launchPanel(Settings.Panel.ACTION_VOLUME);
    }

    private void launchInternetPanel() {
        launchPanel(Settings.Panel.ACTION_INTERNET_CONNECTIVITY);
    }

    private void launchNfcPanel() {
        assumeTrue("device does not support NFC", RequiredServiceRule.hasService("nfc"));
        launchPanel(Settings.Panel.ACTION_NFC);
    }

    private void launchWifiPanel() {
        launchPanel(Settings.Panel.ACTION_WIFI);
    }

    private void launchPanel(String action) {
        // Start from the home screen
        mDevice.pressHome();
        mDevice.wait(Until.hasObject(By.pkg(mLauncherPackage).depth(0)), TIMEOUT);

        Intent intent = new Intent(action);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK
                | Intent.FLAG_ACTIVITY_CLEAR_TASK);    // Clear out any previous instances
        mContext.startActivity(intent);

        // Wait for the app to appear
        mDevice.wait(Until.hasObject(By.pkg(mSettingsPackage).depth(0)), TIMEOUT);
    }
}
