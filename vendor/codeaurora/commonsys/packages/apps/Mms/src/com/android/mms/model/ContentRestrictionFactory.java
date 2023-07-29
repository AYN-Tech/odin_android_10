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
package com.android.mms.model;

import android.content.Context;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;

import com.android.mms.MmsApp;
import com.android.mms.MmsConfig;
import com.android.mms.ui.MessagingPreferenceActivity;

public class ContentRestrictionFactory {
    private static ContentRestriction sContentRestriction;

    private static int sCreationMode;

    private ContentRestrictionFactory() {
    }

    public static void initContentRestriction(int creationMode) {
        sContentRestriction = new CarrierContentRestriction(creationMode);
        sCreationMode = creationMode;
    }

    public static ContentRestriction getContentRestriction() {
        Context context = MmsApp.getApplication().getApplicationContext();
        SharedPreferences preferences = PreferenceManager
                .getDefaultSharedPreferences(context);
        int creationMode = Integer.parseInt(preferences.getString(
                MessagingPreferenceActivity.MMS_CREATION_MODE,
                MessagingPreferenceActivity.CREATION_MODE_FREE + ""));
        if (null == sContentRestriction) {
            if (MmsConfig.isCreationModeEnabled()) {
                sContentRestriction = new CarrierContentRestriction(
                        creationMode);
            } else {
                sContentRestriction = new CarrierContentRestriction();
            }

        }
        sCreationMode = creationMode;
        return sContentRestriction;
    }


    public static void reset() {
        sContentRestriction = null;
    }

    public static int getUsedCreationMode() {
        return sCreationMode;
    }

}
