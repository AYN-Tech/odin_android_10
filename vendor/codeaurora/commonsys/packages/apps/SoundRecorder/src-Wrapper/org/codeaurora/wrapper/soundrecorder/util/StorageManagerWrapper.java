/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.

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

package org.codeaurora.wrapper.soundrecorder.util;

import android.content.Context;
import android.os.storage.StorageManager;
import android.os.storage.StorageVolume;
import android.util.Log;

import java.io.File;

public class StorageManagerWrapper{
    private static final String TAG = "StorageManagerWrapper";
    private static final int VOLUME_SDCARD_INDEX = 1;

    private static StorageManager mStorageManager = null;

    public static StorageManager getStorageManager(Context context) {
        if (mStorageManager == null) {
            mStorageManager = (StorageManager) context.getSystemService(Context.STORAGE_SERVICE);
        }
        return mStorageManager;
    }

    public static StorageVolume[] getVolumeList(Context context) {
        StorageVolume[] volumes = null;

        try {
            volumes = getStorageManager(context).getVolumeList();
        } catch (Exception e) {
            Log.e(TAG, "couldn't talk to MountService", e);
        }
        return volumes;
    }

    public static StorageVolume getStorageVolume(Context context,File file) {
        return getStorageManager(context).getStorageVolume(file);
    }


    public static String getSdDirectory(Context context) {
        String sdDirectory = null;

        try {
            final StorageVolume[] volumes = StorageManagerWrapper.getVolumeList(context);
            if (volumes.length > VOLUME_SDCARD_INDEX) {
                StorageVolume volume = volumes[VOLUME_SDCARD_INDEX];
                if (volume.isRemovable()) {
                    sdDirectory = volume.getPath();
                }
            }
        } catch (Exception e) {
            Log.e(TAG, "couldn't talk to MountService", e);
        }

        return sdDirectory;
    }
}
