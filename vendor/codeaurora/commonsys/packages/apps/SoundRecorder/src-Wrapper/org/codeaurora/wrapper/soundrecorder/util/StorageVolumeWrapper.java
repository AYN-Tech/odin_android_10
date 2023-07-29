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
import android.os.storage.StorageVolume;
import android.util.Log;

import java.io.File;

public class StorageVolumeWrapper{
    private static final String TAG = "StorageVolumeWrapper";

    private static StorageVolume mStorageVolume = null;

    /*must set Instance before use this class*/
    public static StorageVolume getBaseInstance(Context context) {
        return mStorageVolume;
    }

    /*must set Instance before use it*/
    public static void setBaseInstance(StorageVolume base) {
        mStorageVolume = base;
    }

    /*must call it to set Instance before use this class*/
    public static void setBaseInstance(Context context,File file) {
        StorageVolume vol = StorageManagerWrapper.getStorageVolume(context,file);

        mStorageVolume = vol;
    }

    public static String getPath() {
        String path = null;

        if (mStorageVolume != null) {
            path = mStorageVolume.getPath();
        }else{
            Log.e(TAG, "getPath mStorageVolume is null");
        }

        return path;
    }

    public static String getState() {
        String state = null;

        if (mStorageVolume != null) {
            state = mStorageVolume.getState();
        }else{
            Log.e(TAG, "getState mStorageVolume is null");
        }

        return state;
    }

}
