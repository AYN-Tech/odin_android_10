/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.

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

package com.android.soundrecorder.util;

import android.content.Context;
import android.os.Environment;
import android.os.StatFs;
import org.codeaurora.wrapper.soundrecorder.util.StorageManagerWrapper;
import org.codeaurora.wrapper.soundrecorder.util.StorageVolumeWrapper;
import android.util.Log;
import com.android.soundrecorder.R;
import java.io.IOException;

import java.io.File;

public class StorageUtils {
    private static final String TAG = "StorageUtils";
    private static final int VOLUME_SDCARD_INDEX = 1;

    public static final int STORAGE_PATH_PHONE_INDEX = 0;
    public static final int STORAGE_PATH_SD_INDEX = 1;
    private static final String FOLDER_NAME = "SoundRecorder";
    public static final String FM_RECORDING_FOLDER_NAME = "FMRecording";
    public static final String CALL_RECORDING_FOLDER_NAME = "CallRecord";
    private static final String STORAGE_PATH_EXTERNAL_ROOT = Environment
            .getExternalStorageDirectory().toString();
    private static final String STORAGE_PATH_LOCAL_PHONE =
            STORAGE_PATH_EXTERNAL_ROOT + File.separator + FOLDER_NAME;
    private static final String STORAGE_PATH_FM_RECORDING = STORAGE_PATH_EXTERNAL_ROOT
            + File.separator + FM_RECORDING_FOLDER_NAME;
    private static final String STORAGE_PATH_CALL_RECORDING = STORAGE_PATH_EXTERNAL_ROOT
            + File.separator + CALL_RECORDING_FOLDER_NAME;
    private static String sSdDirectory;
    private static String STORAGE_PATH_EXTERNAL_FILES_DIR = STORAGE_PATH_EXTERNAL_ROOT;

    private static final int SD_STORAGE_FREE_BLOCK = 1;
    private static final double PHONE_STORAGE_FREE_BLOCK_PERCENT = 5 / 100.0;

    public static void getExternalFilesDirPath(Context context) {
        try {
            if (context != null) {
                STORAGE_PATH_EXTERNAL_FILES_DIR = Environment.getExternalStoragePublicDirectory(
                        Environment.DIRECTORY_MUSIC).getCanonicalPath();
                Log.d(TAG, "getExternalFilesDirPath =" + STORAGE_PATH_EXTERNAL_FILES_DIR);
            }
        } catch (IOException e) {
            Log.e(TAG, "getExternalFilesDirPath error");
            return;
        }
    }

    private static String getSdDirectory(Context context) {
        if (sSdDirectory == null) {
           sSdDirectory = StorageManagerWrapper.getSdDirectory(context);
        }
        return sSdDirectory;
    }

    public static String getSdState(Context context) {
        String sdPath = getSdDirectory(context);

        if (sdPath != null) {
            StorageVolumeWrapper.setBaseInstance(context,new File(sdPath));
            String state = StorageVolumeWrapper.getState();
            if (state != null) {
                return state;
            } else {
                return Environment.MEDIA_UNKNOWN;
            }
        }
        return Environment.MEDIA_UNMOUNTED;
    }

    public static boolean isPhoneStorageMounted() {
        return Environment.getExternalStorageState().equals(Environment.MEDIA_MOUNTED);
    }

    public static boolean isSdMounted(Context context) {
        return getSdState(context).equals(Environment.MEDIA_MOUNTED);
    }

    public static String applyCustomStoragePath(Context context) {
        return Environment.getExternalStorageDirectory().toString() + File.separator
                + context.getResources().getString(R.string.folder_name);
    }

    public static String getPhoneStoragePath() {
        return STORAGE_PATH_EXTERNAL_FILES_DIR;
    }

    public static String getSdStoragePath(Context context) {
        return STORAGE_PATH_EXTERNAL_FILES_DIR;
    }

    public static String getFmRecordingStoragePath() {
        return STORAGE_PATH_FM_RECORDING;
    }

    public static String getCallRecordingStoragePath() {
        return STORAGE_PATH_CALL_RECORDING;
    }

    /**
     * Is there any point of trying to start recording?
     */
    public static boolean diskSpaceAvailable(Context context, int path) {
        return getAvailableBlocks(context, path) > 0;
    }

    private static long getAvailableBlocks(Context context, int path) {
        long blocks;
        if (path == StorageUtils.STORAGE_PATH_SD_INDEX) {
            StatFs fs = new StatFs(getSdDirectory(context));
            // keep one free block
            blocks = fs.getAvailableBlocksLong() - SD_STORAGE_FREE_BLOCK;
        } else {
            StatFs fs = new StatFs(Environment.getExternalStorageDirectory().getAbsolutePath());
            // keep 5% free block
            blocks = fs.getAvailableBlocksLong()
                    - (long)(fs.getBlockCountLong() * PHONE_STORAGE_FREE_BLOCK_PERCENT);
        }
        return blocks;
    }

    public static long getAvailableBlockSize(Context context, int path) {
        StatFs fs;
        if (path == StorageUtils.STORAGE_PATH_SD_INDEX) {
            fs = new StatFs(getSdDirectory(context));
        } else {
            fs = new StatFs(Environment.getExternalStorageDirectory().getAbsolutePath());
        }
        return getAvailableBlocks(context, path) * fs.getBlockSizeLong();
    }
}
