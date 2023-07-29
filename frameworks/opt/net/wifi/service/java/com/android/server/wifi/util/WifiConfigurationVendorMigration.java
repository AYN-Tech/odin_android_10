/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

package com.android.server.wifi.util;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.File;
import java.io.PrintWriter;
import java.util.stream.Collectors;


import android.os.Environment;
import android.os.Build;
import android.util.Log;
import java.util.BitSet;

import android.net.wifi.WifiConfiguration;


/**
 * This is a utility class to perform WifiConfiguration migration from older Android
 * version to current version after OTA upgrades. Migration is expected to be executed only once
 * at first device boot after OTA upgrade.
 *
 * On first execution, this creates a json file "WifiConfigurationVendorMigration.json" in
 * /data/misc/wifi/ directory and maps vendor specific old KeyMgmt values to the current KeyMgmt
 * values. Thus, retaining all supported vendor supported wifi configurations.
 * This class is not thread safe and should be executed only from NetworkListStoreData.
 *
 * This class supports migration of only shared WifiConfigStore.xml.
 *
 * NOTE: Currently it supports migration from Android P to Q.
 * TODO: Add support of migration from Q to R.
*/
public class WifiConfigurationVendorMigration {

    private static final String TAG = "WifiConfigurationVendorMigration";

    /* Default direcotory for all shared Wifi profiles. */
    private static final File BASE_DATA_MISC_DIR = Environment.getDataMiscDirectory();
    private static final String STORE_DIRECTORY_NAME = "wifi";

    /* File to store the migration status */
    private static final String WIFI_VENDOR_MIGRATION_FILE_NAME
                                = "WifiConfigurationVendorMigration.json";

    /* Migration started */
    public static final int MIGRATION_STATUS_STARTED = 1;
    /* Migration completed */
    public static final int MIGRATION_STATUS_COMPLETED = 2;

    /* JSON string for migration status */
    private static final String JSON_STRING_MIGRATION_STATUS = "status";
    /* JSON string for migration last migration version */
    private static final String JSON_STRING_MIGRATION_VERSION = "lastMigrationVersion";

    private int mStatus;
    private int mMigrationVersion;

    public WifiConfigurationVendorMigration() {
        // initialize defaults
        mStatus = MIGRATION_STATUS_STARTED;
        mMigrationVersion = Build.VERSION.SDK_INT;

        // read the values from json file.
        read();
    }

    /* Utility API for local consumption. It checks for presence of /data/misc/ dir */
    private File getFile() {
        File storeDir = new File(BASE_DATA_MISC_DIR, STORE_DIRECTORY_NAME);
        if (!storeDir.exists()) {
            Log.e(TAG, STORE_DIRECTORY_NAME + " doesn't exist in " + BASE_DATA_MISC_DIR.toString());
            return null;
        }
        return new File(storeDir, WIFI_VENDOR_MIGRATION_FILE_NAME);
    }

    /* Write the updated values to json file. This overwrites any previous data with latest data. */
    private boolean write() {
        try {
            JSONObject out = new JSONObject();
            out.put(JSON_STRING_MIGRATION_STATUS, mStatus);
            out.put(JSON_STRING_MIGRATION_VERSION, mMigrationVersion);

            File file = getFile();
            if (file != null) {
                PrintWriter pw = new PrintWriter(file.toString());
                pw.write(out.toString());
                pw.flush();
                pw.close();
            } else {
                Log.e(TAG, "write: Failed to get file");
                return false;
            }
        } catch (JSONException | FileNotFoundException e) {
            Log.e(TAG, "write exception: " + e.toString());
            return false;
        }
        return true;
    }

    /* Read the json file and parse to local members*/
    private boolean read() {
        File file = getFile();

        if (file == null || !file.exists()) {
            Log.e(TAG, "read: Failed to get file");
            return false;
        }

        BufferedReader reader = null;
        try {
            reader = new BufferedReader(new FileReader(file.toString()));
            String data = reader.lines().collect(Collectors.joining());
            JSONObject in = new JSONObject (data);

            mStatus = (int) in.get(JSON_STRING_MIGRATION_STATUS);
            mMigrationVersion = (int) in.get(JSON_STRING_MIGRATION_VERSION);
        } catch (JSONException | FileNotFoundException e) {
            Log.e(TAG, "read exception: " + e.toString());
            return false;
        } finally {
            try {
                if (reader != null) {
                    reader.close();
                }
            } catch (IOException e) {
                // Do nothing
            }
        }

        return true;
    }

    /* update the migration status to local members.
     * NOTE: This will not write to the json file. To write to json use update() */
    public void setStatus(int status) {
        if (status < MIGRATION_STATUS_STARTED || status > MIGRATION_STATUS_COMPLETED) {
            Log.e(TAG, "Wrong status code: " + status);
            return;
        }

        mStatus = status;
    }

    /* get current migration status */
    public int getStatus() {
        return mStatus;
    }

    /* Check if migration is required or not. Currently migration is expected only during OTA upgrades */
    public boolean isMigrationRequired() {
        if (mStatus == MIGRATION_STATUS_STARTED || mMigrationVersion < Build.VERSION.SDK_INT)
            return true;

        return false;
    }

    /* This writes data to json file */
    public void update() {
        mMigrationVersion = Build.VERSION.SDK_INT;
        write();
    }

    /* Checks for possible migration and updates the new mapping of vendor KeyMgmt.
     * Currently it handles migration from Android P to Q.
     */
    public void checkAndMigrateVendorConfiguration(String configKeyParsed, WifiConfiguration config) {
        // if store migration already took, no need to do it again.
        if (mStatus == MIGRATION_STATUS_COMPLETED)
            return;

        // config key matches, ie. non-vendor configuration.
        if (configKeyParsed.equals(config.configKey()))
            return;

        BitSet oldKeyMgmt = config.allowedKeyManagement;
        for (int bit = oldKeyMgmt.nextSetBit(0); bit != -1; bit = oldKeyMgmt.nextSetBit(bit + 1)) {
            switch (bit) {
                case 8: /* FILS_SHA256 */
                    config.setSecurityParams(WifiConfiguration.SECURITY_TYPE_FILS_SHA256);
                    break;
                case 9: /* FILS_SHA384 */
                    config.setSecurityParams(WifiConfiguration.SECURITY_TYPE_FILS_SHA384);
                    break;
                case 10: /* DPP */
                    config.allowedKeyManagement.clear();
                    config.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.DPP);
                    break;
                case 11: /* SAE */
                    config.setSecurityParams(WifiConfiguration.SECURITY_TYPE_SAE);
                    break;
                case 12: /* OWE */
                    config.setSecurityParams(WifiConfiguration.SECURITY_TYPE_OWE);
                    break;
                case 13: /* SUITE_B_192 */
                    config.setSecurityParams(WifiConfiguration.SECURITY_TYPE_EAP_SUITE_B);
                    break;
                default:
                    Log.e(TAG, "Unknown KeyMgmt: " + bit + " during vendor config migration");
                    break;
            }
        }
    }
}
