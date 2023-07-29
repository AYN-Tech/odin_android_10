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

package android.telecom4.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.util.Log;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.CddTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Build, install and run the tests by running the commands below:
 * make CtsTelecom4TestCases -j64
 * cts-tradefed run cts -m CtsTelecom4TestCases --test android.telecom4.cts.InCallServiceImplTest
 */
@RunWith(AndroidJUnit4.class)
public class InCallServiceImplTest {
    private static final String TAG = "InCallServiceTest";
    private static final String IN_CALL_SERVICE_ACTION = "android.telecom.InCallService";
    private static final String IN_CALL_SERVICE_PERMISSION =
            "android.permission.BIND_INCALL_SERVICE";

    private Context mContext;
    private PackageManager mPackageManager;

    @Before
    public void setup() {
        mContext = InstrumentationRegistry.getContext();
        mPackageManager = mContext.getPackageManager();
    }

    @CddTest(requirement = "7.4.1.2/C-1-3")
    @Test
    public void resolveInCallIntent() {
        if (!hasTelephonyFeature()) {
            Log.d(TAG, "Bypass the test since telephony is not available.");
            return;
        }

        Intent intent = new Intent();
        intent.setAction(IN_CALL_SERVICE_ACTION);
        ResolveInfo resolveInfo = mPackageManager.resolveService(intent,
                PackageManager.GET_SERVICES | PackageManager.GET_META_DATA);

        assertNotNull(resolveInfo);
        assertNotNull(resolveInfo.serviceInfo);
        assertNotNull(resolveInfo.serviceInfo.packageName);
        assertNotNull(resolveInfo.serviceInfo.name);
        assertEquals(IN_CALL_SERVICE_PERMISSION, resolveInfo.serviceInfo.permission);
    }

    private boolean hasTelephonyFeature() {
        return mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY);
    }
}
