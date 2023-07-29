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

import org.codeaurora.wrapper.soundrecorder.util.LongArrayWrapper;
import android.content.Context;
import android.net.Uri;

import java.io.File;
import java.util.ArrayList;
import android.content.ContentValues;
import android.util.Log;
import android.provider.MediaStore;
import android.support.v4.content.FileProvider;
import android.os.Build;
import android.content.ContentUris;
import android.database.Cursor;
import android.content.ContentResolver;


public class FileUtils {
    public static final int NOT_FOUND = -1;
    public static final int SAVE_FILE_START_INDEX = 1;
    public static String getLastFileName(File file, boolean withExtension) {
        if (file == null) {
            return null;
        }
        return getLastFileName(file.getName(), withExtension);
    }

    public static String getLastFileName(String fileName, boolean withExtension) {
        if (fileName == null) {
            return null;
        }
        if (!withExtension) {
            int dotIndex = fileName.lastIndexOf(".");
            int pathSegmentIndex = fileName.lastIndexOf(File.separator) + 1;
            if (dotIndex == NOT_FOUND) {
                dotIndex = fileName.length();
            }
            if (pathSegmentIndex == NOT_FOUND) {
                pathSegmentIndex = 0;
            }
            fileName = fileName.substring(pathSegmentIndex, dotIndex);
        }
        return fileName;
    }

    public static String getFileExtension(File file, boolean withDot) {
        if (file == null) {
            return null;
        }
        String fileName = file.getName();
        int dotIndex = fileName.lastIndexOf(".");
        if (dotIndex == NOT_FOUND) {
            return null;
        }
        if (withDot) {
            return fileName.substring(dotIndex, fileName.length());
        }
        return fileName.substring(dotIndex + 1, fileName.length());
    }

    public static File renameFile(File file, String newName) {
        if (file == null) {
            return null;
        }
        String filePath = file.getAbsolutePath();
        String folderPath = file.getParent();
        String extension = filePath.substring(filePath.lastIndexOf("."), filePath.length());
        File newFile = new File(folderPath, newName + extension);
        if (file.renameTo(newFile)) {
            return newFile;
        }
        return file;
    }

    public static boolean exists(File file) {
        return file != null && file.exists();
    }

    public static long getSuitableIndexOfRecording(String prefix) {
        long returnIndex = SAVE_FILE_START_INDEX;
        File file = new File(StorageUtils.getPhoneStoragePath());
        File list[] = file.listFiles();
        LongArrayWrapper array = new LongArrayWrapper();
        if (list != null && list.length != 0) {
            for (File item : list) {
                String name = getLastFileName(item, false);
                if (name.startsWith(prefix)) {
                    int index = prefix.length();
                    String numString = name.substring(index, name.length());
                    try {
                        array.add(Long.parseLong(numString));
                    } catch (NumberFormatException e) {
                        e.printStackTrace();
                    }
                }
            }
        }

        int size = array.size();
        for (int i = 0; i < size; i++) {
            if (array.indexOf(returnIndex) >= 0) {
                returnIndex++;
            }
        }

        return returnIndex;
    }

    public static boolean isFolderEmpty(String filePath) {
        File file = new File(filePath);
        File[] files = file.listFiles();
        return files == null || files.length == 0;
    }

    public static boolean deleteFile(File file, Context context) {
        boolean ret = false;
        if (file.isDirectory()) {
            File[] files = file.listFiles();
            for (File f : files) {
                // remove all files in folder.
                deleteFile(f, context);
            }
        }
        if (file.delete()) {
            // update database.
            DatabaseUtils.delete(context, file);
            ret = true;
        }
        return ret;
    }


    public static Uri uriFromFile(File file, Context context) {
        if (file == null || context == null) {
            return null;
        }

        ContentResolver cr = context.getContentResolver();
        Cursor c = null;
        long id = 0;
        c = cr.query(MediaStore.Audio.Media.EXTERNAL_CONTENT_URI, null,
                MediaStore.Audio.Media.DATA + "=?", new String[] { file.getAbsolutePath() }, null);

        if(c != null){
            c.moveToFirst();
            id = c.getLong(c.getColumnIndexOrThrow(MediaStore.Audio.Media._ID));
        }

        Uri uri = ContentUris.withAppendedId(
                MediaStore.Audio.Media.EXTERNAL_CONTENT_URI, id);

        Log.d("FileUtils","uriFromFile uri="+uri);
        return uri;
    }

    public static ArrayList<Uri> urisFromFolder(File folder, Context context) {
        if (folder == null || !folder.isDirectory()) {
            return null;
        }

        ArrayList<Uri> uris = new ArrayList<Uri>();
        File[] list = folder.listFiles();
        for (File file : list) {
            if (Build.VERSION.SDK_INT >= 24) {
                uris.add(uriFromFile(file,context));
            } else {
                uris.add(Uri.fromFile(file));
            }
        }
        return uris;
    }
}
