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
package android.car.cts;

import static android.car.Car.PERMISSION_CAR_TEST_SERVICE;

import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeTrue;

import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.platform.test.annotations.RequiresDevice;
import android.test.suitebuilder.annotation.SmallTest;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.FeatureUtil;

import java.util.List;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RequiresDevice
@RunWith(AndroidJUnit4.class)
public class PermissionTest {
    @Before
    public void setUp() {
        assumeTrue(FeatureUtil.isAutomotive());
    }

    @Test
    public void testNoCarTestServicePermission() {
        // This test is valid only in user build.
        assumeTrue(Build.TYPE.equals("user"));

        PackageManager pm = InstrumentationRegistry.getContext().getPackageManager();
        List<PackageInfo> holders = pm.getPackagesHoldingPermissions(new String[] {
                PERMISSION_CAR_TEST_SERVICE
        }, PackageManager.MATCH_UNINSTALLED_PACKAGES);

        assertTrue("No pre-installed packages can have PERMISSION_CAR_TEST_SERVICE: " + holders,
                holders.size() == 0);
    }
}
