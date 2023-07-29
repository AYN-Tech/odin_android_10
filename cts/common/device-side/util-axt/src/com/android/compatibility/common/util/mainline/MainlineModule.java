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
package com.android.compatibility.common.util.mainline;

/**
 * Enum containing metadata for mainline modules.
 */
public enum MainlineModule {
    // Security
    MEDIA_SOFTWARE_CODEC("com.google.android.media.swcodec",
            true, ModuleType.APEX,
            "0C:2B:13:87:6D:E5:6A:E6:4E:D1:DE:93:42:2A:8A:3F:EA:6F:34:C0:FC:5D:7D:A1:BD:CF:EF"
                    + ":C1:A7:B7:C9:1D"),
    MEDIA("com.google.android.media",
            true, ModuleType.APEX,
            "16:C1:5C:FA:15:D0:FD:D0:7E:BE:CB:5A:76:6B:40:8B:05:DD:92:7E:1F:3A:DD:C5:AB:F6:8E"
                    + ":E8:B9:98:F9:FD"),
    DNS_RESOLVER("com.google.android.resolv",
            true, ModuleType.APEX,
            "EC:82:21:76:5E:4F:7E:2C:6D:8D:0F:0C:E9:BD:82:5B:98:BE:D2:0C:07:2C:C6:C8:08:DD:E4"
                    + ":68:5F:EB:A6:FF"),
    CONSCRYPT("com.google.android.conscrypt",
            true, ModuleType.APEX,
            "8C:5D:A9:10:E6:11:21:B9:D6:E0:3B:42:D3:20:6A:7D:AD:29:DD:C1:63:AE:CD:4B:8E:E9:3F"
                    + ":D3:83:79:CA:2A"),
    // Privacy
    PERMISSION_CONTROLLER("com.google.android.permissioncontroller",
            false, ModuleType.APK,
            "89:DF:B5:04:7E:E0:19:29:C2:18:4D:68:EF:49:64:F2:A9:0A:F1:24:C3:23:38:28:B8:F6:40"
                    + ":D9:E6:C0:0F:83"),
    ANDROID_SERVICES("com.google.android.ext.services",
            false, ModuleType.APK,
            "18:46:05:09:5B:E6:CA:22:D0:55:F3:4E:FA:F0:13:44:FD:3A:B3:B5:63:8C:30:62:76:10:EE"
                    + ":AE:8A:26:0B:29"),
    DOCUMENTS_UI("com.google.android.documentsui",
            true, ModuleType.APK,
            "9A:4B:85:34:44:86:EC:F5:1F:F8:05:EB:9D:23:17:97:79:BE:B7:EC:81:91:93:5A:CA:67:F0"
                    + ":F4:09:02:52:97"),
    // Consistency
    TZDATA("com.google.android.tzdata",
            true, ModuleType.APEX,
            "55:93:DD:78:CB:26:EC:9B:00:59:2A:6A:F5:94:E4:16:1F:FD:B5:E9:F3:71:A7:43:54:5F:93"
                    + ":F2:A0:F6:53:89"),
    NETWORK_STACK("com.google.android.networkstack",
            true, ModuleType.APK,
            "5F:A4:22:12:AD:40:3E:22:DD:6E:FE:75:F3:F3:11:84:05:1F:EF:74:4C:0B:05:BE:5C:73:ED"
                    + ":F6:0B:F6:2C:1E"),
    CAPTIVE_PORTAL_LOGIN("com.google.android.captiveportallogin",
            true, ModuleType.APK,
            "5F:A4:22:12:AD:40:3E:22:DD:6E:FE:75:F3:F3:11:84:05:1F:EF:74:4C:0B:05:BE:5C:73:ED"
                    + ":F6:0B:F6:2C:1E"),
    NETWORK_PERMISSION_CONFIGURATION("com.google.android.networkstack.permissionconfig",
            true, ModuleType.APK,
            "5F:A4:22:12:AD:40:3E:22:DD:6E:FE:75:F3:F3:11:84:05:1F:EF:74:4C:0B:05:BE:5C:73:ED"
                    + ":F6:0B:F6:2C:1E"),
    MODULE_METADATA("com.google.android.modulemetadata",
            true, ModuleType.APK,
            "BF:62:23:1E:28:F0:85:42:75:5C:F3:3C:9D:D8:3C:5D:1D:0F:A3:20:64:50:EF:BC:4C:3F:F3"
                    + ":D5:FD:A0:33:0F"),
    ;

    public final String packageName;
    public final boolean isPlayUpdated;
    public final ModuleType moduleType;
    public final String certSHA256;

    MainlineModule(String packageName, boolean isPlayUpdated, ModuleType moduleType,
            String certSHA256) {
        this.packageName = packageName;
        this.isPlayUpdated = isPlayUpdated;
        this.moduleType = moduleType;
        this.certSHA256 = certSHA256;
    }
}
