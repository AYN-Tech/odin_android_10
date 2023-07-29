/*
  Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
      * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
      * Redistributions in binary form must reproduce the above
        copyright notice, this list of conditions and the following
        disclaimer in the documentation and/or other materials provided
        with the distribution.
      * Neither the name of The Linux Foundation nor the names of its
        contributors may be used to endorse or promote products derived
        from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
  ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
  BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
  OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
  IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


package com.android.soundrecorder.util;

import android.app.AlertDialog;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.database.Cursor;
import android.net.Uri;
import android.provider.MediaStore;
import android.util.Log;

import com.android.soundrecorder.R;

import java.io.File;

public class DatabaseUtils {
    private static final String TAG = "DatabaseUtils";
    private static final String PLAY_LIST_NAME = "My recordings";
    public static final String VOLUME_NAME = "external";
    public static final int NOT_FOUND = -1;
    public static final Uri FILE_BASE_URI = Uri.parse("content://media/external/file");

    /*
     * A simple utility to do a query into the databases.
     */
    public static Cursor query(ContentResolver resolver, Uri uri, String[] projection,
            String selection, String[] selectionArgs, String sortOrder) {
        try {
            if (resolver == null) {
                return null;
            }
            return resolver.query(uri, projection, selection, selectionArgs, sortOrder);
        } catch (UnsupportedOperationException ex) {
            ex.printStackTrace();
            return null;
        }
    }

    /*
     * Add the audioId into the playlist
       and  maintain the play order in the playlist.
     */
    public static void InsertIntoPlaylist(ContentResolver resolver, int audioId, long playlistId) {
        Uri uri = MediaStore.Audio.Playlists.Members.getContentUri(VOLUME_NAME, playlistId);

        ContentValues value = new ContentValues();
        value.put(MediaStore.Audio.Playlists.Members.AUDIO_ID, audioId);
        value.put(MediaStore.Audio.Playlists.Members.PLAY_ORDER, audioId);
        try {
            resolver.insert(uri, value);
        } catch (Exception exception) {
            exception.printStackTrace();
        }
    }

    /*
     * get the id from the audio_playlists table
     *  for the default play list
     */
    public static int getPlaylistId(ContentResolver resolver) {
        final String where = MediaStore.Audio.Playlists.NAME + "=?";
        final String[] args = new String[] {
            PLAY_LIST_NAME
        };

        final String[] ids = new String[] {
            MediaStore.Audio.Playlists._ID
        };
        Uri uri = MediaStore.Audio.Playlists.EXTERNAL_CONTENT_URI;
        Cursor cur = query(resolver, uri, ids, where, args, null);
        if (cur == null) {
            Log.e(TAG, " the query is null");
        }
        int id = NOT_FOUND;
        if (cur != null) {
            cur.moveToFirst();
            if (!cur.isAfterLast()) {
                id = cur.getInt(0);
            }
            cur.close();
        }
        return id;
    }

    /*
     * Create a playlist
     */
    public static Uri createPlaylist(Context context, ContentResolver resolver) {
        ContentValues cv = new ContentValues();
        cv.put(MediaStore.Audio.Playlists.NAME, PLAY_LIST_NAME);

        Uri uri = resolver.insert(MediaStore.Audio.Playlists.EXTERNAL_CONTENT_URI, cv);
        if (uri == null) {
            new AlertDialog.Builder(context)
                    .setTitle(R.string.app_name)
                    .setMessage(R.string.error_mediadb_new_record)
                    .setPositiveButton(R.string.button_ok, null)
                    .setCancelable(false)
                    .show();
        }
        return uri;
    }

    public static boolean isDataExist(ContentResolver resolver, File file) {
        Uri uri = MediaStore.Audio.Media.EXTERNAL_CONTENT_URI;
        final String[] ids = new String[] {
                MediaStore.Audio.Playlists._ID
        };
        final String where = MediaStore.Audio.Playlists.DATA + "=?";
        final String[] args = new String[] {
                file.getAbsolutePath()
        };
        Cursor cursor = query(resolver, uri, ids, where, args, null);

        if (cursor != null && cursor.getCount() > 0) {
            cursor.close();
            return true;
        }
        return false;
    }

    /*
     * add media file to the database ,and return the content uri.
     */
    public static Uri addToMediaDB(Context context, File file, long duration, String mimeType) {
        ContentValues contentValues = new ContentValues();
        Resources res = context.getResources();

        String fileName = "";
        if (!"".equals(res.getString(R.string.def_save_name_prefix))) {
            fileName = file.getAbsolutePath().substring(
                    file.getAbsolutePath().lastIndexOf("/") + 1, file.getAbsolutePath().length());
        } else {
            fileName = FileUtils.getLastFileName(file, false);
        }

        long time = System.currentTimeMillis();
        long date = file.lastModified();

        contentValues.put(MediaStore.Audio.Media.ARTIST,
                res.getString(R.string.audio_db_artist_name));
        contentValues.put(MediaStore.Audio.Media.DATE_ADDED, (int) (time / 1000));
        contentValues.put(MediaStore.Audio.Media.DATA, file.getAbsolutePath());
        contentValues.put(MediaStore.Audio.Media.IS_MUSIC, "1");
        contentValues.put(MediaStore.Audio.Media.DATE_MODIFIED, (int) (date / 1000));
        contentValues.put(MediaStore.Audio.Media.DURATION, duration);
        contentValues.put(MediaStore.Audio.Media.TITLE, fileName);
        contentValues.put(MediaStore.Audio.Media.DISPLAY_NAME, fileName);
        contentValues.put(MediaStore.Audio.Media.ALBUM,
                res.getString(R.string.audio_db_album_name));
        contentValues.put(MediaStore.Audio.Media.MIME_TYPE, mimeType);

        Log.d(TAG, "Insert audio record: " + contentValues.toString());

        Uri baseUri = MediaStore.Audio.Media.EXTERNAL_CONTENT_URI;
        Uri uri;

        ContentResolver resolver = context.getContentResolver();
        try {
            uri = resolver.insert(baseUri, contentValues);
            Log.d(TAG, "MediaStore.Audio.Media.EXTERNAL_CONTENT_URI: " + baseUri+" uri="+uri);
        } catch (Exception exception) {
            uri = null;
        }
        if (uri == null) {
            new AlertDialog.Builder(context).setTitle(R.string.app_name)
                    .setPositiveButton(R.string.button_ok, null)
                    .setMessage(R.string.error_mediadb_new_record)
                    .setCancelable(false).show();
            return null;
        }
        if (getPlaylistId(resolver) == NOT_FOUND) {
            createPlaylist(context, resolver);
        }
        int audioId = Integer.valueOf(uri.getLastPathSegment());
        InsertIntoPlaylist(resolver, audioId, getPlaylistId(resolver));

        // Notify those applications such as Music listening to the
        // scanner events that a recorded audio file just created.
        context.sendBroadcast(new Intent(Intent.ACTION_MEDIA_SCANNER_SCAN_FILE, uri));
        return uri;
    }

    public static void rename(Context context, File file, File newFile) {
        ContentResolver resolver = context.getContentResolver();
        String title = FileUtils.getLastFileName(newFile, false);

        ContentValues cv = new ContentValues();
        cv.put(MediaStore.Audio.Media.TITLE, title);
        cv.put(MediaStore.Audio.Media.DISPLAY_NAME, title);
        cv.put(MediaStore.Audio.Media.DATA, newFile.getAbsolutePath());

        Uri base = MediaStore.Audio.Media.EXTERNAL_CONTENT_URI;
        String where = MediaStore.Audio.Media.DATA + " = \'" + file.getAbsolutePath() + "\'";
        resolver.update(base, cv, where, null);
    }

    public static void delete(Context context, File file) {
        ContentResolver resolver = context.getContentResolver();
        Uri base = MediaStore.Audio.Media.EXTERNAL_CONTENT_URI;
        String where = MediaStore.Audio.Media.DATA + " = \'" + file.getAbsolutePath() + "\'";
        resolver.delete(base, where, null);
    }

    public static long getFolderId(ContentResolver resolver, String folderPath) {
        String[] projection = {
                MediaStore.Files.FileColumns._ID, MediaStore.Files.FileColumns.DATA
        };
        String selection = MediaStore.Files.FileColumns.DATA + " = \'" + folderPath + "\'";
        Cursor cursor = DatabaseUtils.query(resolver, FILE_BASE_URI, projection, selection, null,
                MediaStore.Files.FileColumns._ID + " ASC LIMIT 1");
        if (cursor != null) {
            int idIndex = cursor.getColumnIndexOrThrow(MediaStore.Files.FileColumns._ID);
            if (cursor.getCount() > 0) {
                cursor.moveToNext();
                long id = cursor.getInt(idIndex);
                cursor.close();
                return id;
            }
        }
        return NOT_FOUND;
    }

    public static Cursor getFolderCursor(ContentResolver resolver, long folderId) {
        return getFolderCursor(resolver, new Long[] {folderId});
    }

    public static Cursor getFolderCursor(ContentResolver resolver, Long[] folderIds) {
        if (folderIds == null || folderIds.length == 0) {
            return null;
        }
        String[] projection = {
                MediaStore.Files.FileColumns._ID, MediaStore.Files.FileColumns.DATA,
                MediaStore.Audio.Media.DISPLAY_NAME, MediaStore.Audio.Media.DURATION,
                MediaStore.Audio.Media.DATE_MODIFIED, MediaStore.Files.FileColumns.PARENT
        };

        String selection = MediaStore.Audio.Media.IS_MUSIC + "=1" + " AND "
                + MediaStore.Files.FileColumns.PARENT + "=";
        StringBuilder allSelection = new StringBuilder();
        for (int i = 0; i < folderIds.length; i++) {
            if (i != 0) {
                allSelection.append(" OR ");
            }
            allSelection.append("(" + selection + "\'" + folderIds[i] + "\'" + ")");
        }
        return DatabaseUtils.query(resolver, FILE_BASE_URI, projection, allSelection.toString(),
                null, MediaStore.Audio.Media.DATE_MODIFIED + " DESC");
    }


    public static Cursor getFolderCursor(ContentResolver resolver) {
        String[] projection = {
                MediaStore.Files.FileColumns._ID, MediaStore.Files.FileColumns.DATA,
                MediaStore.Audio.Media.DISPLAY_NAME, MediaStore.Audio.Media.DURATION,
                MediaStore.Audio.Media.DATE_MODIFIED, MediaStore.Files.FileColumns.PARENT
        };

        String selection = MediaStore.Audio.Media.IS_MUSIC + "=1" + " AND "
                + MediaStore.Files.FileColumns.DATA + " LIKE "
                + "\'" + StorageUtils.getPhoneStoragePath()+ "%\'";

        return DatabaseUtils.query(resolver, FILE_BASE_URI, projection, selection.toString(),
                null, MediaStore.Audio.Media.DATE_MODIFIED + " DESC");
    }

}
