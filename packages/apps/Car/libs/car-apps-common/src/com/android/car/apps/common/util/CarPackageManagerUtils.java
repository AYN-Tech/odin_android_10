/*
 * Copyright 2019 The Android Open Source Project
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

package com.android.car.apps.common.util;

import android.car.Car;
import android.car.CarNotConnectedException;
import android.car.content.pm.CarPackageManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.util.Log;

import androidx.annotation.NonNull;

/**
 * Utility class to access CarPackageManager
 */
public class CarPackageManagerUtils {
    private static final String TAG = "CarPackageManagerUtils";

    private final Car mCarApi;
    private CarPackageManager mCarPackageManager;

    private static CarPackageManagerUtils sInstance = null;

    private CarPackageManagerUtils(Context context) {
        mCarApi = Car.createCar(context.getApplicationContext());
        try {
            mCarPackageManager = (CarPackageManager) mCarApi.getCarManager(Car.PACKAGE_SERVICE);
        } catch (CarNotConnectedException e) {
            Log.e(TAG, "Car not connected when retrieving car package manager", e);
        }
    }

    /**
     * Returns the singleton instance of this class
     */
    @NonNull
    public static CarPackageManagerUtils getInstance(Context context) {
        if (sInstance == null) {
            sInstance = new CarPackageManagerUtils(context);
        }
        return sInstance;
    }

    /**
     * Returns true if the provided Activity is distraction optimized
     */
    public boolean isDistractionOptimized(@NonNull ActivityInfo activityInfo) {
        if (mCarPackageManager != null) {
            try {
                return mCarPackageManager.isActivityDistractionOptimized(
                        activityInfo.packageName, activityInfo.name);
            } catch (CarNotConnectedException e) {
                Log.e(TAG, "Car not connected when getting driver optimization info", e);
                return false;
            }
        }
        return false;
    }

    /**
     * Attempts to resolve the provided intent into an activity, and returns true if the
     * resolved activity is distraction optimized
     */
    public boolean isDistractionOptimized(PackageManager packageManager, Intent intent) {
        ResolveInfo info = packageManager.resolveActivity(
                intent, PackageManager.MATCH_DEFAULT_ONLY);
        return (info != null) ? isDistractionOptimized(info.activityInfo) : false;
    }
}
