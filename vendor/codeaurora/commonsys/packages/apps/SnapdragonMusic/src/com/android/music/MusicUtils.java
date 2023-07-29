/*
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

package com.android.music;

import android.app.Activity;
import android.app.Fragment;
import android.app.FragmentManager;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.content.res.Resources;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.ColorMatrix;
import android.graphics.ColorMatrixColorFilter;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.media.audiofx.AudioEffect;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import android.provider.MediaStore;
import android.provider.Settings;
import android.support.v4.widget.DrawerLayout;
import android.support.v7.app.ActionBarDrawerToggle;
import android.support.v7.widget.Toolbar;
import android.telecom.PhoneAccountHandle;
import android.telecom.TelecomManager;
import android.telephony.SubscriptionInfo;
import android.telephony.TelephonyManager;
import android.telephony.SubscriptionManager;
import android.text.TextUtils;
import android.text.format.Time;
import android.util.Log;
import android.util.LruCache;
import android.view.Gravity;
import android.view.Menu;
import android.view.MenuItem;
import android.view.SubMenu;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewTreeObserver.OnPreDrawListener;
import android.view.Window;
import android.webkit.WebView.FindListener;
import android.widget.AdapterView;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.PopupMenu;
import android.widget.RelativeLayout;
import android.widget.TabWidget;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;
import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.PrintWriter;
import java.util.Arrays;
import java.util.Formatter;
import java.util.HashMap;
import java.util.Locale;


//import android.drm.DrmHelper;
import android.annotation.SuppressLint;
import android.drm.DrmManagerClient;
import android.drm.DrmRights;
import android.drm.DrmStore;
import android.drm.DrmStore.Action;
import android.drm.DrmStore.RightsStatus;

import com.codeaurora.music.custom.MusicPanelLayout.BoardState;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class MusicUtils {

    private static final String TAG = "MusicUtils";

    private final static long MAX_DRM_RING_TONE_SIZE = 300 * 1024; // 300KB

    public static boolean mPlayAllFromMenu = false;
    private static boolean mGroupByFolder = false;
    private static boolean mDisableAnimation;
    public static boolean mRepeatPlay = false;
    public static boolean isFragmentRemoved = true;
    public static boolean mEditMode;
    public static boolean mPause;
    public final static int RINGTONE_SUB_0 = 0;
    public final static int RINGTONE_SUB_1 = 1;

    public static int navigatingTabPosition;
    public static boolean isLaunchedFromQueryBrowser = false;

    public static long mPlayListId;
    // decoding and caching 20 bitmaps to overcome Out of memory exception.
    public static LruCache<String, Bitmap[]> mArtCache =
                                         new LruCache<String, Bitmap[]>(20);
    public static LruCache<String, Bitmap> mFolderCache = new LruCache<String, Bitmap>(
            20);
    public static LruCache<String, Bitmap[]> mAlbumArtCache =
            new LruCache<String, Bitmap[]>(20);

    public static HashMap<Integer, Cursor> cur = new HashMap<Integer, Cursor>();
    static Bitmap mAlbumArtsArray[];

    public static boolean mIsScreenOff = false;
    //cache the artwork to avoid the bitmap is released when the quick view the songs in play panel
    private final static int MAX_ARTWORK_CACHE_SIZE = 40;
    static Bitmap mAlbumArtWorkCacheArray[] = new Bitmap[MAX_ARTWORK_CACHE_SIZE];
    private static int counter = 0;

    public interface Defs {
        public final static int OPEN_URL = 0;
        public final static int ADD_TO_PLAYLIST = 1;
        public final static int USE_AS_RINGTONE = 2;
        public final static int PLAYLIST_SELECTED = 3;
        public final static int NEW_PLAYLIST = 4;
        public final static int PLAY_SELECTION = 5;
        public final static int GOTO_START = 6;
        public final static int GOTO_PLAYBACK = 7;
        public final static int PARTY_SHUFFLE = 8;
        public final static int SHUFFLE_ALL = 9;
        public final static int DELETE_ITEM = 10;
        public final static int SCAN_DONE = 11;
        public final static int QUEUE = 12;
        public final static int EFFECTS_PANEL = 13;
        public final static int MORE_MUSIC = 14;
        public final static int MORE_VIDEO = 15;
        public final static int USE_AS_RINGTONE_2 = 16;
        public final static int DRM_LICENSE_INFO = 17;
        public final static int CLOSE = 18;
        public final static int CHILD_MENU_BASE = 19;// this should be the last item;
    }

    private static void addDateToCache(String artistName,
            Bitmap[] mAlbumArtsArray2) {
        MusicUtils.putIntoLruCache(artistName, mAlbumArtsArray2, mArtCache);
    }

    public static void addArtworkToCache(Bitmap bitmap) {
        if(counter == MAX_ARTWORK_CACHE_SIZE){
            counter = 0;
        }
        mAlbumArtWorkCacheArray[counter++] = bitmap;
    }

    public static String makeAlbumsLabel(Context context, int numalbums,
            int numsongs, boolean isUnknown) {
        // There are two formats for the albums/songs information:
        // "N Song(s)" - used for unknown artist/album
        // "N Album(s)" - used for known albums
        StringBuilder songs_albums = new StringBuilder();
        Resources r = context.getResources();
        String f = r.getQuantityText(R.plurals.Nalbums, numalbums).toString();
        sFormatBuilder.setLength(0);
        sFormatter.format(f, Integer.valueOf(numalbums));
        songs_albums.append(sFormatBuilder);
        songs_albums.append(context.getString(R.string.albumsongseparator));
        return songs_albums.toString();
    }

    /**
     * This is now only used in artistlabum screen
     */
    public static String makeArtistAlbumsSongsLabel(Context context, int numsongs) {
        StringBuilder songs_albums = new StringBuilder();

        if (numsongs == 1) {
            songs_albums.append(context.getString(R.string.onesong));
        } else {
            Resources r = context.getResources();

            String f = r.getQuantityText(R.plurals.Nsongs, numsongs).toString();
            sFormatBuilder.setLength(0);
            sFormatter.format(f, Integer.valueOf(numsongs));
            songs_albums.append(sFormatBuilder);
        }
        return songs_albums.toString();
    }

    /**
     * This is now only used for the query screen
     */
    public static String makeAlbumsSongsLabel(Context context, int numalbums, int numsongs, boolean isUnknown) {
        // There are several formats for the albums/songs information:
        // "1 Song"   - used if there is only 1 song
        // "N Songs" - used for the "unknown artist" item
        // "1 Album"/"N Songs" 
        // "N Album"/"M Songs"
        // Depending on locale, these may need to be further subdivided
        StringBuilder songs_albums = new StringBuilder();

        if (numsongs == 1) {
            songs_albums.append(context.getString(R.string.onesong));
        } else {
            Resources r = context.getResources();
            if (! isUnknown) {
                String f = r.getQuantityText(R.plurals.Nalbums, numalbums).toString();
                sFormatBuilder.setLength(0);
                sFormatter.format(f, Integer.valueOf(numalbums));
                songs_albums.append(sFormatBuilder);
                songs_albums.append(context.getString(R.string.albumsongseparator));
            }
            String f = r.getQuantityText(R.plurals.Nsongs, numsongs).toString();
            sFormatBuilder.setLength(0);
            sFormatter.format(f, Integer.valueOf(numsongs));
            songs_albums.append(sFormatBuilder);
        }
        return songs_albums.toString();
    }

    public static IMediaPlaybackService sService = null;
    private static HashMap<Context, ServiceBinder> sConnectionMap = new HashMap<Context, ServiceBinder>();

    public static class ServiceToken {
        ContextWrapper mWrappedContext;
        ServiceToken(ContextWrapper context) {
            mWrappedContext = context;
        }
    }

    public static ServiceToken bindToService(Activity context) {
        return bindToService(context, null);
    }

    public static ServiceToken bindToService(Activity context, ServiceConnection callback) {
        Activity realActivity = context.getParent();
        if (realActivity == null) {
            realActivity = context;
        }
        ContextWrapper cw = new ContextWrapper(context);
        MusicUtils.startService(cw, new Intent(cw, MediaPlaybackService.class));
        ServiceBinder sb = new ServiceBinder(callback);
        if (cw.bindService((new Intent()).setClass(cw, MediaPlaybackService.class), sb, 0)) {
            sConnectionMap.put(cw, sb);
            return new ServiceToken(cw);
        }
        Log.e("Music", "Failed to bind to service");
        return null;
    }

    public static void updateGroupByFolder(Activity a) {
        if (a.getApplicationContext().getResources()
                .getBoolean(R.bool.group_by_folder)) {
            mGroupByFolder = true;
        } else {
            mGroupByFolder = false;
        }
    }

    public static void unbindFromService(ServiceToken token) {
        if (token == null) {
            Log.e("MusicUtils", "Trying to unbind with null token");
            return;
        }
        ContextWrapper cw = token.mWrappedContext;
        ServiceBinder sb = sConnectionMap.remove(cw);
        if (sb == null) {
            Log.e("MusicUtils", "Trying to unbind for unknown Context");
            return;
        }
        cw.unbindService(sb);
        if (sConnectionMap.isEmpty()) {
            // presumably there is nobody interested in the service at this point,
            // so don't hang on to the ServiceConnection
            sService = null;
        }
    }

    private static class ServiceBinder implements ServiceConnection {
        ServiceConnection mCallback;
        ServiceBinder(ServiceConnection callback) {
            mCallback = callback;
        }

        public void onServiceConnected(ComponentName className, android.os.IBinder service) {
            sService = IMediaPlaybackService.Stub.asInterface(service);
            initAlbumArtCache();
            if (mCallback != null) {
                mCallback.onServiceConnected(className, service);
            }
        }

        public void onServiceDisconnected(ComponentName className) {
            if (mCallback != null) {
                mCallback.onServiceDisconnected(className);
            }
            sService = null;
        }
    }

    public static long getCurrentAlbumId() {
        if (sService != null) {
            try {
                return sService.getAlbumId();
            } catch (RemoteException ex) {
            }
        }
        return -1;
    }

    public static long getCurrentArtistId() {
        if (MusicUtils.sService != null) {
            try {
                return sService.getArtistId();
            } catch (RemoteException ex) {
            }
        }
        return -1;
    }

    public static long getCurrentAudioId() {
        if (MusicUtils.sService != null) {
            try {
                return sService.getAudioId();
            } catch (RemoteException ex) {
            }
        }
        return -1;
    }

    public static String getCurrentData() {
        if (MusicUtils.sService != null) {
            try {
                return sService.getData();
            } catch (RemoteException ex) {
            }
        }
        return "";
    }

    public static int getCurrentShuffleMode() {
        int mode = MediaPlaybackService.SHUFFLE_NONE;
        if (sService != null) {
            try {
                mode = sService.getShuffleMode();
            } catch (RemoteException ex) {
            }
        }
        return mode;
    }

    public static long[] getSongListForFolder(Context context, long id) {
        final String[] ccols = new String[] {
                MediaStore.Audio.Media._ID
        };
        String where = MediaStore.Files.FileColumns.PARENT + "=" + id + " AND " +
                MediaStore.Audio.Media.IS_MUSIC + "=1";
        Cursor cursor = query(context, MediaStore.Files.getContentUri("external"),
                ccols, where, null, MediaStore.Audio.Media.TRACK);
        if (cursor != null) {
            long[] list = getSongListForCursor(cursor);
            cursor.close();
            return list;
        }
        return sEmptyList;
    }

    public static void togglePartyShuffle() {
        if (sService != null) {
            int shuffle = getCurrentShuffleMode();
            try {
                if (shuffle == MediaPlaybackService.SHUFFLE_AUTO) {
                    sService.setShuffleMode(MediaPlaybackService.SHUFFLE_NONE);
                } else {
                    sService.setShuffleMode(MediaPlaybackService.SHUFFLE_AUTO);
                    if (sService.getRepeatMode() == MediaPlaybackService.REPEAT_CURRENT)
                        sService.setRepeatMode(MediaPlaybackService.REPEAT_ALL);

                }
            } catch (RemoteException ex) {
            }
        }
    }

    public static void setPartyShuffleMenuIcon(Menu menu) {
        MenuItem item = menu.findItem(Defs.PARTY_SHUFFLE);
        if (item != null) {
            int shuffle = MusicUtils.getCurrentShuffleMode();
            if (shuffle == MediaPlaybackService.SHUFFLE_AUTO) {
                item.setIcon(R.drawable.ic_menu_party_shuffle);
                item.setTitle(R.string.party_shuffle_off);
            } else {
                item.setIcon(R.drawable.ic_menu_party_shuffle);
                item.setTitle(R.string.party_shuffle);
            }
        }
    }

    /*
     * Returns true if a file is currently opened for playback (regardless
     * of whether it's playing or paused).
     */
    public static boolean isMusicLoaded() {
        if (MusicUtils.sService != null) {
            try {
                return sService.getPath() != null;
            } catch (RemoteException ex) {
            }
        }
        return false;
    }

    private final static long [] sEmptyList = new long[0];

    public static long [] getSongListForCursor(Cursor cursor) {
        if (cursor == null) {
            return sEmptyList;
        }
        int len = cursor.getCount();
        long [] list = new long[len];
        cursor.moveToFirst();
        int colidx = -1;
        try {
            colidx = cursor.getColumnIndexOrThrow(MediaStore.Audio.Playlists.Members.AUDIO_ID);
        } catch (IllegalArgumentException ex) {
            colidx = cursor.getColumnIndexOrThrow(MediaStore.Audio.Media._ID);
        }
        for (int i = 0; i < len; i++) {
            list[i] = cursor.getLong(colidx);
            cursor.moveToNext();
        }
        return list;
    }

    public static long [] getSongListForArtist(Context context, long id) {
        final String[] ccols = new String[] { MediaStore.Audio.Media._ID };
        String where = MediaStore.Audio.Media.ARTIST_ID + "=" + id + " AND " + 
        MediaStore.Audio.Media.IS_MUSIC + "=1";
        Cursor cursor = query(context, MediaStore.Audio.Media.EXTERNAL_CONTENT_URI,
                ccols, where, null,
                MediaStore.Audio.Media.ALBUM_KEY + ","  + MediaStore.Audio.Media.TRACK);
        
        if (cursor != null) {
            long [] list = getSongListForCursor(cursor);
            cursor.close();
            return list;
        }
        return sEmptyList;
    }

    public static long [] getSongListForAlbum(Context context, long id) {
        final String[] ccols = new String[] { MediaStore.Audio.Media._ID };
        String where = MediaStore.Audio.Media.ALBUM_ID + "=" + id + " AND " + 
                MediaStore.Audio.Media.IS_MUSIC + "=1";
        Cursor cursor = query(context, MediaStore.Audio.Media.EXTERNAL_CONTENT_URI,
                ccols, where, null, MediaStore.Audio.Media.TRACK);

        if (cursor != null) {
            long [] list = getSongListForCursor(cursor);
            cursor.close();
            return list;
        }
        return sEmptyList;
    }

    public static long [] getSongListForPlaylist(Context context, long plid) {
        if (plid == -1) {
            return sEmptyList;
        }
        final String[] ccols = new String[] { MediaStore.Audio.Playlists.Members.AUDIO_ID };
        Cursor cursor = query(context, MediaStore.Audio.Playlists.Members.getContentUri("external", plid),
                ccols, null, null, MediaStore.Audio.Playlists.Members.DEFAULT_SORT_ORDER);
        
        if (cursor != null) {
            long[] list = getSongListForCursor(cursor);
            cursor.close();
            return list;
        }
        return sEmptyList;
    }

    public static void setPlayListId(long plid) {
        mPlayListId = plid;
    }

    public static long getPlayListId() {
        return mPlayListId;
    }

    public static void playPlaylist(Context context, long plid) {
        long [] list = getSongListForPlaylist(context, plid);
        mPlayListId = plid;
        if (list != null) {
            playAll(context, list, -1, false);
        }
    }

    public static long [] getAllSongs(Context context) {
        Cursor c = query(context, MediaStore.Audio.Media.EXTERNAL_CONTENT_URI,
                new String[] {MediaStore.Audio.Media._ID}, MediaStore.Audio.Media.IS_MUSIC + "=1",
                null, null);
        try {
            if (c == null || c.getCount() == 0) {
                return null;
            }
            int len = c.getCount();
            long [] list = new long[len];
            for (int i = 0; i < len; i++) {
                c.moveToNext();
                list[i] = c.getLong(0);
            }

            return list;
        } finally {
            if (c != null) {
                c.close();
            }
        }
    }

    /**
     * Fills out the given submenu with items for "new playlist" and
     * any existing playlists. When the user selects an item, the
     * application will receive PLAYLIST_SELECTED with the Uri of
     * the selected playlist, NEW_PLAYLIST if a new playlist
     * should be created, and QUEUE if the "current playlist" was
     * selected.
     * @param context The context to use for creating the menu items
     * @param sub The submenu to add the items to.
     */
    public static void makePlaylistMenu(Context context, SubMenu sub) {
        String[] cols = new String[] {
                MediaStore.Audio.Playlists._ID,
                MediaStore.Audio.Playlists.NAME
        };
        ContentResolver resolver = context.getContentResolver();
        if (resolver == null) {
            Log.e(TAG, "resolver null");
        } else {
            String whereclause = MediaStore.Audio.Playlists.NAME + " != ''";
            Cursor cur = resolver.query(MediaStore.Audio.Playlists.EXTERNAL_CONTENT_URI,
                cols, whereclause, null,
                MediaStore.Audio.Playlists.NAME);
            sub.clear();
            sub.add(1, Defs.QUEUE, 0, R.string.queue);
            sub.add(1, Defs.NEW_PLAYLIST, 0, R.string.new_playlist);
            if (cur != null && cur.getCount() > 0) {
                //sub.addSeparator(1, 0);
                cur.moveToFirst();
                while (! cur.isAfterLast()) {
                    Intent intent = new Intent();
                    intent.putExtra("playlist", cur.getLong(0));
//                    if (cur.getInt(0) == mLastPlaylistSelected) {
//                        sub.add(0, MusicBaseActivity.PLAYLIST_SELECTED, cur.getString(1)).setIntent(intent);
//                    } else {
                        String name = cur.getString(1);
                    // Do not add song to "My Favorite" playlist
                    if (name.equals("My Favorite")) {
                        cur.moveToNext();
                        continue;
                    }
                        if (cur.getString(1).equals("My recordings")) {
                            name = context.getResources()
                                    .getString(R.string.audio_db_playlist_name);
                        }

                        sub.add(1, Defs.PLAYLIST_SELECTED, 0, name).setIntent(intent);
//                    }
                    cur.moveToNext();
                }
            }
            if (cur != null) {
                cur.close();
            }
        }
    }

    public static void clearPlaylist(Context context, int plid) {

        Uri uri = MediaStore.Audio.Playlists.Members.getContentUri("external",
                plid);
        context.getContentResolver().delete(uri, null, null);
        return;
    }

    public static void deleteTrack(MediaPlaybackService mpbService, long mid, long artIndex) {
        if (mpbService == null) {
            return;
        }
        try {
             mpbService.removeTrack(mid);
             if (sArtCache != null) {
                 synchronized(sArtCache) {
                     sArtCache.remove(artIndex);
                 }
             }
             mpbService.getApplicationContext().getContentResolver().notifyChange(Uri.parse("content://media"), null);
        } catch (Exception e) {
            Log.e("MusicUtils", "Error occur when deleting music");
        }
    }

    public static Cursor deleteTracksPre(Context context, String where) {
        final String [] cols = new String [] { MediaStore.Audio.Media._ID,
                MediaStore.Audio.Media.DATA, MediaStore.Audio.Media.ALBUM_ID };

        Cursor c = query(context, MediaStore.Audio.Media.EXTERNAL_CONTENT_URI, cols,
                where, null, null);

        if (c != null) {

            // step 1: remove selected tracks from the current playlist, as well
            // as from the album art cache
            try {
                c.moveToFirst();
                while (! c.isAfterLast()) {
                    // remove from current playlist
                    long id = c.getLong(0);
                    // perform in the main thread
                    sService.removeTrack(id);
                    // remove from album art cache
                    long artIndex = c.getLong(2);
                    synchronized(sArtCache) {
                        sArtCache.remove(artIndex);
                    }
                    c.moveToNext();
                }
            } catch (RemoteException ex) {
            }
        }
        return c;
    }

    public static void deleteTracks(Context context, Cursor c, String where) {
        if (c != null) {
            //Time-consuming, can not be executed in the main thread.
            // step 2: remove selected tracks from the database
            context.getContentResolver().delete(MediaStore.Audio.Media.EXTERNAL_CONTENT_URI,
                    where, null);

            // step 3: remove files from card
            c.moveToFirst();
            while (! c.isAfterLast()) {
                String name = c.getString(1);
                File f = new File(name);
                try {  // File.delete can throw a security exception
                    if (!f.delete()) {
                        // I'm not sure if we'd ever get here (deletion would
                        // have to fail, but no exception thrown)
                        Log.e("MusicUtils", "Failed to delete file " + name);
                    }
                    c.moveToNext();
                } catch (SecurityException ex) {
                    c.moveToNext();
                }
            }
        }
    }

    public static void deleteTracksPost(Context context, int length) {
        String message = context.getResources().getQuantityString(
                R.plurals.NNNtracksdeleted, length, Integer.valueOf(length));
        // perform in the main thread
        Toast.makeText(context, message, Toast.LENGTH_SHORT).show();
        // We deleted a number of tracks, which could affect any number of things
        // in the media content domain, so update everything.
        context.getContentResolver().notifyChange(Uri.parse("content://media"), null);
    }
    
    public static void addToCurrentPlaylist(Context context, long [] list) {
        if (sService == null) {
            return;
        }
        try {
            sService.enqueue(list, MediaPlaybackService.LAST);
            String message = context.getResources().getQuantityString(
                    R.plurals.NNNtrackstoplaylist, list.length, Integer.valueOf(list.length));
            Toast.makeText(context, message, Toast.LENGTH_SHORT).show();
        } catch (RemoteException ex) {
        }
    }

    private static ContentValues[] sContentValuesCache = null;

    /**
     * @param ids The source array containing all the ids to be added to the playlist
     * @param offset Where in the 'ids' array we start reading
     * @param len How many items to copy during this pass
     * @param base The play order offset to use for this pass
     */
    private static void makeInsertItems(long[] ids, int offset, int len, int base) {
        // adjust 'len' if would extend beyond the end of the source array
        if (offset + len > ids.length) {
            len = ids.length - offset;
        }
        // allocate the ContentValues array, or reallocate if it is the wrong size
        if (sContentValuesCache == null || sContentValuesCache.length != len) {
            sContentValuesCache = new ContentValues[len];
        }
        // fill in the ContentValues array with the right values for this pass
        for (int i = 0; i < len; i++) {
            if (sContentValuesCache[i] == null) {
                sContentValuesCache[i] = new ContentValues();
            }

            sContentValuesCache[i].put(MediaStore.Audio.Playlists.Members.PLAY_ORDER, base + offset + i);
            sContentValuesCache[i].put(MediaStore.Audio.Playlists.Members.AUDIO_ID, ids[offset + i]);
        }
    }
    
    public static void addToPlaylist(Context context, long [] ids, long playlistid) {
        if (ids == null) {
            // this shouldn't happen (the menuitems shouldn't be visible
            // unless the selected item represents something playable
            Log.e("MusicBase", "ListSelection null");
        } else if (playlistid == -1) {
            Log.e(TAG,"addToPlaylist failed playlistid="+playlistid);
        } else {
            int size = ids.length;
            ContentResolver resolver = context.getContentResolver();
            // need to determine the number of items currently in the playlist,
            // so the play_order field can be maintained.
            String[] cols = new String[] {
                    MediaStore.Audio.Playlists._ID
            };
            Uri uri = MediaStore.Audio.Playlists.Members.getContentUri("external", playlistid);
            Cursor cur = resolver.query(uri, cols, null, null, null);
            int base = 0;
            if (cur != null) {
                if (cur.getCount()> 0) {
                    cur.moveToLast();
                    base = cur.getInt(0);
                }
                cur.close();
            }

            int numinserted = 0;
            for (int i = 0; i < size; i += 1000) {
                makeInsertItems(ids, i, 1000, base);
                numinserted += resolver.bulkInsert(uri, sContentValuesCache);
            }
            String message = context.getResources().getQuantityString(
                    R.plurals.NNNtrackstoplaylist, numinserted, numinserted);
            Toast.makeText(context, message, Toast.LENGTH_SHORT).show();
        }
    }

    public static int idForplaylist(Context context, String name) {
        Cursor c = query(context, MediaStore.Audio.Playlists.EXTERNAL_CONTENT_URI,
                new String[] {
                    MediaStore.Audio.Playlists._ID
                },
                MediaStore.Audio.Playlists.NAME + "=?",
                new String[] {
                    name
                },
                MediaStore.Audio.Playlists.NAME);
        int id = -1;
        if (c != null) {
            c.moveToFirst();
            if (!c.isAfterLast()) {
                id = c.getInt(0);
            }
            c.close();
        }
        return id;
    }

    public static Cursor query(Context context, Uri uri, String[] projection,
            String selection, String[] selectionArgs, String sortOrder, int limit) {
        try {
            ContentResolver resolver = context.getContentResolver();
            if (resolver == null) {
                return null;
            }
            if (limit > 0) {
                uri = uri.buildUpon().appendQueryParameter("limit", "" + limit).build();
            }
            return resolver.query(uri, projection, selection, selectionArgs, sortOrder);
         } catch (UnsupportedOperationException ex) {
            return null;
        }

    }

    public static Cursor query(Context context, Uri uri, String[] projection,
            String selection, String[] selectionArgs, String sortOrder) {
        return query(context, uri, projection, selection, selectionArgs, sortOrder, 0);
    }

    public static boolean isMediaScannerScanning(Context context) {
        boolean result = false;
        Cursor cursor = query(context, MediaStore.getMediaScannerUri(), 
                new String [] { MediaStore.MEDIA_SCANNER_VOLUME }, null, null, null);
        if (cursor != null) {
            if (cursor.getCount() == 1) {
                cursor.moveToFirst();
                result = "external".equals(cursor.getString(0));
            }
            cursor.close();
        }

        return result;
    }

    public static void setSpinnerState(Activity a) {
        if (isMediaScannerScanning(a)) {
            // start the progress spinner
            a.getWindow().setFeatureInt(
                    Window.FEATURE_INDETERMINATE_PROGRESS,
                    Window.PROGRESS_INDETERMINATE_ON);

            a.getWindow().setFeatureInt(
                    Window.FEATURE_INDETERMINATE_PROGRESS,
                    Window.PROGRESS_VISIBILITY_ON);
        } else {
            // stop the progress spinner
            a.getWindow().setFeatureInt(
                    Window.FEATURE_INDETERMINATE_PROGRESS,
                    Window.PROGRESS_VISIBILITY_OFF);
        }
    }

    private static String mLastSdStatus;

    public static void displayDatabaseError(Activity a) {
        if (a.isFinishing()) {
            // When switching tabs really fast, we can end up with a null
            // cursor (not sure why), which will bring us here.
            // Don't bother showing an error message in that case.
            return;
        }

        String status = Environment.getExternalStorageState();
        int title, message;

        if (android.os.Environment.isExternalStorageRemovable()) {
            title = R.string.sdcard_error_title;
            message = R.string.sdcard_error_message;
        } else {
            title = R.string.sdcard_error_title_nosdcard;
            message = R.string.sdcard_error_message_nosdcard;
        }

        if (status.equals(Environment.MEDIA_SHARED) ||
                status.equals(Environment.MEDIA_UNMOUNTED)) {
            if (android.os.Environment.isExternalStorageRemovable()) {
                title = R.string.sdcard_busy_title;
                message = R.string.sdcard_busy_message;
            } else {
                title = R.string.sdcard_busy_title_nosdcard;
                message = R.string.sdcard_busy_message_nosdcard;
            }
        } else if (status.equals(Environment.MEDIA_REMOVED)) {
            if (android.os.Environment.isExternalStorageRemovable()) {
                title = R.string.sdcard_missing_title;
                message = R.string.sdcard_missing_message;
            } else {
                title = R.string.sdcard_missing_title_nosdcard;
                message = R.string.sdcard_missing_message_nosdcard;
            }
        } else if (status.equals(Environment.MEDIA_MOUNTED)){
            // The card is mounted, but we didn't get a valid cursor.
            // This probably means the mediascanner hasn't started scanning the
            // card yet (there is a small window of time during boot where this
            // will happen).
            a.setTitle("");
            Intent intent = new Intent();
            intent.setClass(a, ScanningProgress.class);
            a.startActivityForResult(intent, Defs.SCAN_DONE);
        } else if (!TextUtils.equals(mLastSdStatus, status)) {
            mLastSdStatus = status;
            Log.d(TAG, "sd card: " + status);
        }

        a.setTitle(title);
        View v = a.findViewById(R.id.sd_message);
        if (v != null) {
            v.setVisibility(View.VISIBLE);
        }
        v = a.findViewById(R.id.sd_icon);
        if (v != null) {
            v.setVisibility(View.VISIBLE);
        }
        v = a.findViewById(android.R.id.list);
        if (v != null) {
            v.setVisibility(View.GONE);
        }
        v = a.findViewById(R.id.buttonbar);
        if (v != null) {
            v.setVisibility(View.GONE);
        }
        TextView tv = (TextView) a.findViewById(R.id.sd_message);
        if (tv != null) {
            tv.setText(message);
        }
    }

    public static void hideDatabaseError(Activity a) {
        View v = a.findViewById(R.id.sd_message);
        if (v != null) {
            v.setVisibility(View.GONE);
        }
        v = a.findViewById(R.id.sd_icon);
        if (v != null) {
            v.setVisibility(View.GONE);
        }
        v = a.findViewById(android.R.id.list);
        if (v != null) {
            v.setVisibility(View.VISIBLE);
        }
    }

    static protected Uri getContentURIForPath(String path) {
        return Uri.fromFile(new File(path));
    }

    
    /*  Try to use String.format() as little as possible, because it creates a
     *  new Formatter every time you call it, which is very inefficient.
     *  Reusing an existing Formatter more than tripled the speed of
     *  makeTimeString().
     *  This Formatter/StringBuilder are also used by makeAlbumSongsLabel()
     */
    private static StringBuilder sFormatBuilder = new StringBuilder();
    private static Formatter sFormatter = new Formatter(sFormatBuilder, Locale.getDefault());
    private static final Object[] sTimeArgs = new Object[5];

    public static String makeTimeString(Context context, long secs) {
        String durationformat = context.getString(
                secs < 3600 ? R.string.durationformatshort : R.string.durationformatlong);
        
        /* Provide multiple arguments so the format can be changed easily
         * by modifying the xml.
         */
        sFormatBuilder.setLength(0);

        final Object[] timeArgs = sTimeArgs;
        timeArgs[0] = secs / 3600;
        timeArgs[1] = secs / 60;
        timeArgs[2] = (secs / 60) % 60;
        timeArgs[3] = secs;
        timeArgs[4] = secs % 60;

        return sFormatter.format(durationformat, timeArgs).toString();
    }

    public static void shuffleAll(Context context, Cursor cursor) {
        playAll(context, cursor, -1, true);
    }

    public static void playAll(Context context, Cursor cursor) {
        mPlayAllFromMenu = true;
        playAll(context, cursor, 0, false);
    }

    public static void playAll(Context context, Cursor cursor, int position) {
        playAll(context, cursor, position, false);
    }

    public static void playAll(Context context, long[] list, int position) {
        playAll(context, list, position, false);
    }

    private static void playAll(Context context, Cursor cursor, int position,
            boolean force_shuffle) {

        long[] list = getSongListForCursor(cursor);
        playAll(context, list, position, force_shuffle);
    }

    private static void playAll(Context context, long[] list, int position,
            boolean force_shuffle) {
        if (isForbidPlaybackInCall(context)) {
            return;
        }
        if (list.length == 0 || sService == null) {
            Log.d("MusicUtils", "attempt to play empty song list");
            // Don't try to play empty playlists. Nothing good will come of it.
            String message = context.getString(R.string.emptyplaylist, list.length);
            Toast.makeText(context, message, Toast.LENGTH_SHORT).show();
            return;
        }
        try {
            if (force_shuffle) {
                sService.setShuffleMode(MediaPlaybackService.SHUFFLE_NORMAL);

                //If the repeat mode is REPEAT_CURRENT, we should change mode to REPEAT_ALL
                if (sService.getRepeatMode() == MediaPlaybackService.REPEAT_CURRENT) {
                    sService.setRepeatMode(MediaPlaybackService.REPEAT_ALL);
                }
            }

            if (mPlayAllFromMenu){
                sService.setShuffleMode(MediaPlaybackService.SHUFFLE_NONE);

                //If the repeat mode is REPEAT_CURRENT, we should change mode to REPEAT_ALL
                if (sService.getRepeatMode() == MediaPlaybackService.REPEAT_CURRENT) {
                    sService.setRepeatMode(MediaPlaybackService.REPEAT_ALL);
                }

                mPlayAllFromMenu = false;
            }

            long curid = sService.getAudioId();
            int curpos = sService.getQueuePosition();
            if (position != -1 && curpos == position && curid == list[position] && !mRepeatPlay) {
                // The selected file is the file that's currently playing;
                // figure out if we need to restart with a new playlist,
                // or just launch the playback activity.
                long [] playlist = sService.getQueue();
                if (Arrays.equals(list, playlist)) {
                    // we don't need to set a new list, but we should resume playback if needed
                    sService.play();
                    return; // the 'finally' block will still run
                }
            }
            if (position < 0) {
                position = 0;
            }
            sService.open(list, force_shuffle ? -1 : position);
            sService.play();
        } catch (RemoteException ex) {
        } finally {
            MediaPlaybackActivity playbackActivity = MediaPlaybackActivity.getInstance();
            if (playbackActivity != null) {
                updateNowPlaying(playbackActivity, playbackActivity.mIsPanelExpanded);
            }
            mRepeatPlay = false;
        }
    }

    public static void clearQueue() {
        try {
            sService.removeTracks(0, Integer.MAX_VALUE);
        } catch (RemoteException ex) {
        }
    }

    // A really simple BitmapDrawable-like class, that doesn't do
    // scaling, dithering or filtering.
    private static class FastBitmapDrawable extends Drawable {
        private Bitmap mBitmap;
        public FastBitmapDrawable(Bitmap b) {
            mBitmap = b;
        }
        @Override
        public void draw(Canvas canvas) {
            canvas.drawBitmap(mBitmap, 0, 0, null);
        }
        @Override
        public int getOpacity() {
            return PixelFormat.OPAQUE;
        }
        @Override
        public void setAlpha(int alpha) {
        }
        @Override
        public void setColorFilter(ColorFilter cf) {
        }
    }

    private static int sArtId = -2;
    private static Bitmap sCachedBitAlbum = null;
    private static Bitmap sCachedBitSong = null;
    private static long sLastSong = -1;
    private static long sLastAlbum = -1;
    private static final BitmapFactory.Options sBitmapOptionsCache = new BitmapFactory.Options();
    private static final BitmapFactory.Options sBitmapOptions = new BitmapFactory.Options();
    private static final Uri sArtworkUri = Uri
            .parse("content://media/external/audio/albumart");
    private static final LruCache<Long, Drawable> sArtCache = new LruCache<Long, Drawable>(20);
    private static int sArtCacheId = -1;

    static {
        // for the cache,
        // 565 is faster to decode and display
        // and we don't want to dither here because the image will be scaled
        // down later
        sBitmapOptionsCache.inPreferredConfig = Bitmap.Config.RGB_565;
        sBitmapOptionsCache.inDither = false;

        sBitmapOptions.inPreferredConfig = Bitmap.Config.RGB_565;
        sBitmapOptions.inDither = false;
    }

    public static void initAlbumArtCache() {
        try {
            int id = sService.getMediaMountedCount();
            if (id != sArtCacheId) {
                clearAlbumArtCache();
                sArtCacheId = id;
            }
        } catch (RemoteException e) {
            e.printStackTrace();
        }
    }

    public static void clearAlbumArtCache() {
        synchronized (sArtCache) {
            sArtCache.evictAll();
        }
    }

    public static Drawable getsArtCachedDrawable(Context context, long artIndex) {
        synchronized (sArtCache) {
            return sArtCache.get(artIndex);
        }
    }

    public static Drawable getCachedArtwork(Context context, long artIndex,
            BitmapDrawable defaultArtwork) {
        Drawable d = null;
        synchronized (sArtCache) {
            d = sArtCache.get(artIndex);
        }
        if (d == null) {
            d = defaultArtwork;
            final Bitmap icon = defaultArtwork.getBitmap();
            int w = icon.getWidth();
            int h = icon.getHeight();
            Bitmap b = MusicUtils.getArtworkQuick(context, artIndex, w, h);
            if (b != null) {
                d = new FastBitmapDrawable(b);
                synchronized (sArtCache) {
                    // the cache may have changed since we checked
                    Drawable value = sArtCache.get(artIndex);
                    if (value == null) {
                        sArtCache.put(artIndex, d);
                    } else {
                        d = value;
                    }
                }
            }
        }
        return d;
    }

    // Get album art for specified album. This method will not try to
    // fall back to getting artwork directly from the file, nor will
    // it attempt to repair the database.
    public static Bitmap getArtworkQuick(Context context, long album_id, int w,
            int h) {
        // NOTE: There is in fact a 1 pixel border on the right side in the ImageView
        // used to display this drawable. Take it into account now, so we don't have to
        // scale later.
        w -= 1;
        ContentResolver res = context.getContentResolver();
        Uri uri = ContentUris.withAppendedId(sArtworkUri, album_id);
        if (uri != null) {
            ParcelFileDescriptor fd = null;
            try {
                fd = res.openFileDescriptor(uri, "r");
                int sampleSize = 1;

                if (fd != null) {
                    // Compute the closest power-of-two scale factor
                    // and pass that to sBitmapOptionsCache.inSampleSize, which
                    // will result in faster decoding and better quality
                    sBitmapOptionsCache.inJustDecodeBounds = true;
                    BitmapFactory.decodeFileDescriptor(fd.getFileDescriptor(),
                            null, sBitmapOptionsCache);
                    int nextWidth = sBitmapOptionsCache.outWidth >> 1;
                    int nextHeight = sBitmapOptionsCache.outHeight >> 1;
                    while (nextWidth > w && nextHeight > h) {
                        sampleSize <<= 1;
                        nextWidth >>= 1;
                        nextHeight >>= 1;
                    }

                    sBitmapOptionsCache.inSampleSize = sampleSize;
                    sBitmapOptionsCache.inJustDecodeBounds = false;
                    Bitmap b = BitmapFactory.decodeFileDescriptor(
                            fd.getFileDescriptor(), null, sBitmapOptionsCache);

                    if (b != null) {
                        // finally rescale to exactly the size we need
                        if (sBitmapOptionsCache.outWidth != w
                                || sBitmapOptionsCache.outHeight != h) {
                            Bitmap tmp = Bitmap.createScaledBitmap(b, w, h,
                                    true);
                            // Bitmap.createScaledBitmap() can return the same
                            // bitmap
                            b = tmp;
                        }
                    }

                    return b;
                }
            } catch (FileNotFoundException e) {
            } finally {
                try {
                    if (fd != null)
                        fd.close();
                } catch (IOException e) {
                }
            }
        }
        return null;
    }

    /** Get album art for specified album. You should not pass in the album id
     * for the "unknown" album here (use -1 instead)
     * This method always returns the default album art icon when no album art is found.
     */
    public static Bitmap getArtwork(Context context, long song_id, long album_id) {
        return getArtwork(context, song_id, album_id, true);
    }

    /**
     * Get album art for specified album. You should not pass in the album id
     * for the "unknown" album here (use -1 instead)
     */
    public static Bitmap getArtwork(Context context, long song_id,
            long album_id, boolean allowdefault) {
        if (context == null) {
            Log.d(TAG, "getArtwork failed because context is null");
            return null;
        }

        if (album_id < 0) {
            // This is something that is not in the database, so get the album
            // art directly from the file.
            Bitmap bm = null;
            if (song_id >= 0) {
                bm = getArtworkFromFile(context, song_id, -1);
                if (bm != null) {
                    addArtworkToCache(bm);
                    return bm;
                }
            }
            if (allowdefault) {
                bm = getDefaultArtwork(context);
                if (bm != null && song_id >= 0) {
                    addArtworkToCache(bm);
                }
                return bm;
            }
            return null;
        }

        ContentResolver res = context.getContentResolver();
        Uri uri = ContentUris.withAppendedId(sArtworkUri, album_id);
        if (uri != null) {
            InputStream in = null;
            try {
                in = res.openInputStream(uri);
                Bitmap b= BitmapFactory.decodeStream(in, null, sBitmapOptions);
                if (b != null && song_id >= 0) {
                    addArtworkToCache(b);
                }
                return b;
            } catch (FileNotFoundException ex) {
                // The album art thumbnail does not actually exist. Maybe the
                // user deleted it, or
                // maybe it never existed to begin with.
                Bitmap bm = getArtworkFromFile(context, song_id, album_id);
                if (bm != null) {
                    if (bm.getConfig() == null) {
                        bm = bm.copy(Bitmap.Config.RGB_565, false);
                        if (bm == null && allowdefault) {
                            return getDefaultArtwork(context);
                        }
                    }
                } else if (allowdefault) {
                    bm = getDefaultArtwork(context);
                }
                if (bm != null && song_id >= 0) {
                    addArtworkToCache(bm);
                }
                return bm;
            } catch (OutOfMemoryError ex) {
                return null;
            } finally {
                try {
                    if (in != null) {
                        in.close();
                    }
                } catch (IOException ex) {
                }
            }
        }

        return null;
    }
    private static boolean isUriExisted(Context context, Uri uri) {
        //workaround for media provider AOB issue
        if (uri != null) {
            return false;
        }

        if (uri != null) {
            Cursor result = null;
            try {
               result = context.getContentResolver().query(uri, null, null, null, null);
             } catch (UnsupportedOperationException ex) {
                Log.e(TAG, "UnsupportedOperationException for " + uri);
             } finally {
                if (result != null) {
                    if (result.getCount() == 0) {
                        result.close();
                        return false;
                    }
                    return true;
                 }
             }
        }
        return false;
    }
    // get album art for specified file
    private static final String sExternalMediaUri = MediaStore.Audio.Media.EXTERNAL_CONTENT_URI.toString();
    private static Bitmap getArtworkFromFile(Context context, long songid, long albumid) {
        Bitmap bm = null;
        byte [] art = null;
        String path = null;
        Uri uri = null;

        if (albumid < 0 && songid < 0) {
            throw new IllegalArgumentException("Must specify an album or a song id");
        }
        ParcelFileDescriptor pfd = null;
        try {
            if (albumid < 0) {
                if (sLastSong == songid) {
                    return sCachedBitSong != null? sCachedBitSong : getDefaultArtwork(context);
                }
                uri = Uri.parse("content://media/external/audio/media/" + songid + "/albumart");
                pfd = context.getContentResolver().openFileDescriptor(uri, "r");
                if (pfd != null) {
                    FileDescriptor fd = pfd.getFileDescriptor();
                    bm = BitmapFactory.decodeFileDescriptor(fd);
                }
            } else {
                if (sLastAlbum == albumid) {
                   return sCachedBitAlbum != null?
                           sCachedBitAlbum : getDefaultArtworkImage(context);
                }
                uri = ContentUris.withAppendedId(sArtworkUri, albumid);
                pfd = context.getContentResolver().openFileDescriptor(uri, "r");
                if (pfd != null) {
                    FileDescriptor fd = pfd.getFileDescriptor();
                    bm = BitmapFactory.decodeFileDescriptor(fd);
                }
            }
        } catch (IllegalStateException ex) {
        } catch (FileNotFoundException ex) {
        } catch (IllegalArgumentException ex) {
            Log.e(TAG, "IllegalArgumentException for " + uri);
        } finally {
            try {
                if (pfd != null) {
                    pfd.close();
                }
            } catch (IOException e) {
            }
        }
        if (albumid < 0) {
           sCachedBitSong = bm;
           sLastSong = songid;
        } else {
           sCachedBitAlbum = bm;
           sLastAlbum = albumid;
        }
        return bm;
    }

    public static Bitmap getDefaultArtwork(Context context) {
        BitmapFactory.Options opts = new BitmapFactory.Options();
        opts.inPreferredConfig = Bitmap.Config.ARGB_8888;
        return BitmapFactory.decodeStream(
                context.getResources().openRawResource(R.drawable.album_cover_background),
                null, opts);
    }

    public static Bitmap getDefaultArtworkImage(Context context) {
        Resources r = context.getResources();
        Bitmap b = BitmapFactory.decodeResource(r, R.drawable.album_cover);
        return b;
    }

    static int getIntPref(Context context, String name, int def) {
        SharedPreferences prefs =
            context.getSharedPreferences(context.getPackageName(), Context.MODE_PRIVATE);
        return prefs.getInt(name, def);
    }

    static void setIntPref(Context context, String name, int value) {
        SharedPreferences prefs =
            context.getSharedPreferences(context.getPackageName(), Context.MODE_PRIVATE);
        Editor ed = prefs.edit();
        ed.putInt(name, value);
        ed.apply();
    }

    static void setRingtone(Context context, long id, int sub_id) {
        if (context == null) {
            Log.e(TAG, "context is null");
            return;
        }
        ContentResolver resolver = context.getContentResolver();
        // Set the flag in the database to mark this as a ringtone
        Uri ringUri = ContentUris.withAppendedId(MediaStore.Audio.Media.EXTERNAL_CONTENT_URI, id);
        try {
            ContentValues values = new ContentValues(2);
            values.put(MediaStore.Audio.Media.IS_RINGTONE, "1");
            values.put(MediaStore.Audio.Media.IS_ALARM, "1");
            resolver.update(ringUri, values, null, null);
        } catch (UnsupportedOperationException ex) {
            // most likely the card just got unmounted
            Log.e(TAG, "couldn't set ringtone flag for id " + id);
            return;
        }

        String[] cols = new String[] {
                MediaStore.Audio.Media._ID,
                MediaStore.Audio.Media.DATA,
                MediaStore.Audio.Media.TITLE
        };

        String where = MediaStore.Audio.Media._ID + "=" + id;
        Cursor cursor = query(context, MediaStore.Audio.Media.EXTERNAL_CONTENT_URI,
                cols, where , null, null);
        try {
            if (cursor != null && cursor.getCount() == 1) {
                // Set the system setting to make this the current ringtone
                cursor.moveToFirst();
                //TODO: DRM changes here.
                String message = null;
                try {
                    Settings.System.putString(resolver, Settings.System.RINGTONE,
                            ringUri.toString());
                    message = context.getString(R.string.ringtone_set, cursor.getString(2));
                } catch (Exception e) {
                    message = context.getString(R.string.ringtone_set_fail);
                } finally {
                    Toast.makeText(context, message, Toast.LENGTH_SHORT).show();
                }
            }
        } finally {
            if (cursor != null) {
                cursor.close();
            }
        }
    }

    /*
    static void setRingtone(Context context, long id, int sub_id) {
        ContentResolver resolver = context.getContentResolver();
        // Set the flag in the database to mark this as a ringtone
        Uri ringUri = ContentUris.withAppendedId(MediaStore.Audio.Media.EXTERNAL_CONTENT_URI, id);
        try {
            ContentValues values = new ContentValues(2);
            values.put(MediaStore.Audio.Media.IS_RINGTONE, "1");
            values.put(MediaStore.Audio.Media.IS_ALARM, "1");
            resolver.update(ringUri, values, null, null);
        } catch (UnsupportedOperationException ex) {
            // most likely the card just got unmounted
            Log.e(TAG, "couldn't set ringtone flag for id " + id);
            return;
        }

        String[] cols = new String[] {
                MediaStore.Audio.Media._ID,
                MediaStore.Audio.Media.DATA,
                MediaStore.Audio.Media.TITLE
        };

        String where = MediaStore.Audio.Media._ID + "=" + id;
        Cursor cursor = query(context, MediaStore.Audio.Media.EXTERNAL_CONTENT_URI,
                cols, where, null, null);
        try {
            if (cursor != null && cursor.getCount() == 1) {
                // Set the system setting to make this the current ringtone
                cursor.moveToFirst();
                String message = context.getString(R.string.ringtone_set, cursor.getString(2));

                String path = cursor.getString(1);
                if (DrmHelper.isDrmFile(path)) {
                    if(!DrmHelper.isDrmFLBlocking(context, path)){
                        Toast.makeText(context, R.string.drm_ringtone_error, Toast.LENGTH_LONG).show();
                        return;
                    }
                }

                Intent intent = new Intent("android.drmservice.intent.action.RING_TONE");
                intent.putExtra("DRM_TYPE", "OMAV1");
                intent.putExtra("DRM_FILE_PATH", path);
                context.sendBroadcast(intent);

                if (sub_id == RINGTONE_SUB_0) {
                    Settings.System.putString(resolver, Settings.System.RINGTONE , ringUri.toString());
                    if (TelephonyManager.getDefault().isMultiSimEnabled()) {
                        message = context.getString(R.string.ringtone_set_1, cursor.getString(2));
                    } else {
                        message = context.getString(R.string.ringtone_set, cursor.getString(2));
                    }
                } else if (sub_id == RINGTONE_SUB_1) {
                    Settings.System.putString(resolver, Settings.System.RINGTONE_2, ringUri.toString());
                    message = context.getString(R.string.ringtone_set_2, cursor.getString(2));
                }
                Toast.makeText(context, message, Toast.LENGTH_SHORT).show();
            }
        } finally {
            if (cursor != null) {
                cursor.close();
            }
        }
    }
*/
    static void setRingtone(Context context, long id) {
        setRingtone(context, id, RINGTONE_SUB_0);
    }

    static int sActiveTabIndex = -1;

    static boolean updateButtonBar(Activity a, int highlight) {
        final TabWidget ll = (TabWidget) a.findViewById(R.id.buttonbar);
        if (a.getApplicationContext().getResources().getBoolean(R.bool.group_by_folder)) {
            TextView song = (TextView) a.findViewById(R.id.songtab);
            mGroupByFolder = true;
            if (song != null) {
                song.setVisibility(View.GONE);
            }
        } else {
            TextView folder = (TextView) a.findViewById(R.id.foldertab);
            if (folder != null) {
                folder.setVisibility(View.GONE);
            }
        }
        boolean withtabs = false;
        Intent intent = a.getIntent();
        if (intent != null) {
            withtabs = intent.getBooleanExtra("withtabs", false);
        }

        if (highlight == 0 || !withtabs) {
            ll.setVisibility(View.GONE);
            return withtabs;
        } else if (withtabs) {
            ll.setVisibility(View.VISIBLE);
        }
        for (int i = ll.getChildCount() - 1; i >= 0; i--) {

            View v = ll.getChildAt(i);
            boolean isActive = (v.getId() == highlight);
            if (isActive) {
                ll.setCurrentTab(i);
                sActiveTabIndex = i;
            }
            v.setTag(i);
            v.setOnFocusChangeListener(new View.OnFocusChangeListener() {

                public void onFocusChange(View v, boolean hasFocus) {
                    if (hasFocus) {
                        for (int i = 0; i < ll.getTabCount(); i++) {
                            if (ll.getChildTabViewAt(i) == v) {
                                ll.setCurrentTab(i);
                                processTabClick((Activity)ll.getContext(), v, ll.getChildAt(sActiveTabIndex).getId());
                                break;
                            }
                        }
                    }
                }});
            
            v.setOnClickListener(new View.OnClickListener() {

                public void onClick(View v) {
                    processTabClick((Activity)ll.getContext(), v, ll.getChildAt(sActiveTabIndex).getId());
                }});
        }
        return withtabs;
    }

    private static void processDrawerItemClick(Activity a, int position) {
        Log.i("MusicUtils", "processDrawerItemClick "+position);
        activateScreen(a, position);
        setIntPref(a, "activetab", position);
    }

    static void activateScreen(Activity a, int position) {
        Intent intent = new Intent(Intent.ACTION_PICK);

        Log.i("MusicUtils", "processDrawerItemClick "+position);
        switch (position) {
            case 0:
                intent.setDataAndType(Uri.EMPTY, "vnd.android.cursor.dir/artistalbum");
                break;
            case 1:
                intent.setDataAndType(Uri.EMPTY, "vnd.android.cursor.dir/album");
                break;
            case 2:
                intent.setDataAndType(Uri.EMPTY, "vnd.android.cursor.dir/track");
                break;
            case 3:
                intent.setDataAndType(Uri.EMPTY, MediaStore.Audio.Playlists.CONTENT_TYPE);
                break;
                // fall through and return
            default:
                return;
        }
        intent.putExtra("withtabs", false);
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        a.startActivity(intent);
        a.finish();
        a.overridePendingTransition(0, 0);
    }

    static void processTabClick(Activity a, View v, int current) {
        int id = v.getId();
        if (id == current) {
            return;
        }

        final TabWidget ll = (TabWidget) a.findViewById(R.id.buttonbar);

        activateTab(a, id);
        ll.setCurrentTab((Integer) v.getTag());
        setIntPref(a, "activetab", id);
    }

    static void activateTab(Activity a, int id) {
        Intent intent = new Intent(Intent.ACTION_PICK);
        switch (id) {
            case R.id.artisttab:
                intent.setDataAndType(Uri.EMPTY, "vnd.android.cursor.dir/artistalbum");
                break;
            case R.id.albumtab:
                intent.setDataAndType(Uri.EMPTY, "vnd.android.cursor.dir/album");
                break;
            case R.id.songtab:
                intent.setDataAndType(Uri.EMPTY, "vnd.android.cursor.dir/track");
                break;
            case R.id.foldertab:
                intent.setDataAndType(Uri.EMPTY, "vnd.android.cursor.dir/folder");
                break;
            case R.id.playlisttab:
                intent.setDataAndType(Uri.EMPTY, MediaStore.Audio.Playlists.CONTENT_TYPE);
                break;
                // fall through and return
            default:
                return;
        }
        intent.putExtra("withtabs", false);
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        a.startActivity(intent);
        a.finish();
        a.overridePendingTransition(0, 0);
    }

    public static void updateNowPlaying(Activity a, boolean isExpanded) {
        if (sService == null) {
            return;
        }
        View nowPlayingView = ((MediaPlaybackActivity) a).getNowPlayingView();
        if (nowPlayingView == null) {
            Log.e(TAG, "Draglayout null");
            return;
        }
        try {
            String path = sService.getPath();
            if (path == null) {
                return;
            }
            if (true && MusicUtils.sService != null
                    && MusicUtils.sService.getAudioId() != -1) {
                nowPlayingView.setVisibility(View.VISIBLE);
                nowPlayingView.invalidate();
                nowPlayingView.requestLayout();
                TextView title = (TextView) nowPlayingView.findViewById(R.id.song_name);
                TextView artist = (TextView) nowPlayingView.findViewById(R.id.artist_name);
                ImageView image = (ImageView) nowPlayingView.findViewById(R.id.nowplay_icon);
                final ImageView albumIcon = (ImageView) nowPlayingView.findViewById(R.id.album);
                ImageButton currPlaylist = (ImageButton) nowPlayingView.findViewById(R.id.animViewcurrPlaylist);
                ImageButton overflow = (ImageButton) nowPlayingView.findViewById(R.id.menu_overflow_audio_header);
                View layout = nowPlayingView.findViewById(R.id.header_layout);
                if (isExpanded) {
                    image.setVisibility(View.GONE);
                    currPlaylist.setVisibility(View.VISIBLE);
                    overflow.setVisibility(View.VISIBLE);
                    layout.setBackgroundResource(R.drawable.playingbar_bg_rev);
                    title.setSelected(true);
                } else {
                    image.setVisibility(View.VISIBLE);
                    currPlaylist.setVisibility(View.GONE);
                    overflow.setVisibility(View.GONE);
                    layout.setBackgroundResource(R.drawable.playingbar_bg);
                    title.setSelected(false);
                    if (isPlaying()) {
                        image.setImageResource(R.drawable.play_pause);
                    } else {
                        image.setImageResource(R.drawable.play_arrow);
                    }
                }
                title.setText(MusicUtils.sService.getTrackName());
                String artistName = MusicUtils.sService.getArtistName();
                if (MediaStore.UNKNOWN_STRING.equals(artistName)) {
                    artistName = a.getString(R.string.unknown_artist_name);
                }
                artist.setText(artistName);
                return;
            } else if(MusicUtils.sService.getAudioId() == -1) {
                // we might get an audio id as -1 which will result in a blank/white screen.
                return;
            }
        } catch (RemoteException ex) {
        } catch (NullPointerException ex) {
            // we might not actually have the service yet
            ex.printStackTrace();
            return ;
        }
        ((MediaPlaybackActivity) a).getSlidingPanelLayout().setHookState(BoardState.HIDDEN);
    }

    static void setBackground(final View v, Bitmap bm) {

        if (bm == null) {
            v.setBackgroundResource(0);
            return;
        }

        int vwidth = v.getWidth();
        int vheight = v.getHeight();
        int bwidth = bm.getWidth();
        int bheight = bm.getHeight();
        float scalex = (float) vwidth / bwidth;
        float scaley = (float) vheight / bheight;
        float scale = Math.max(scalex, scaley) * 1.3f;

        Bitmap.Config config = Bitmap.Config.ARGB_8888;
        Bitmap bg = Bitmap.createBitmap(vwidth, vheight, config);
        Canvas c = new Canvas(bg);
        Paint paint = new Paint();
        paint.setAntiAlias(true);
        paint.setFilterBitmap(true);
        ColorMatrix greymatrix = new ColorMatrix();
        greymatrix.setSaturation(0);
        ColorMatrix darkmatrix = new ColorMatrix();
        darkmatrix.setScale(.3f, .3f, .3f, 1.0f);
        greymatrix.postConcat(darkmatrix);
        ColorFilter filter = new ColorMatrixColorFilter(greymatrix);
        paint.setColorFilter(filter);
        Matrix matrix = new Matrix();
        matrix.setTranslate(-bwidth/2, -bheight/2); // move bitmap center to origin
        matrix.postRotate(10);
        matrix.postScale(scale, scale);
        matrix.postTranslate(vwidth/2, vheight/2);  // Move bitmap center to view center
        c.drawBitmap(bm, matrix, paint);
        v.setBackgroundDrawable(new BitmapDrawable(bg));
    }

    static int getCardId(Context context) {
        ContentResolver res = context.getContentResolver();
        Cursor c = res.query(Uri.parse("content://media/external/fs_id"), null, null, null, null);
        int id = -1;
        if (c != null && c.getCount() > 0) {
            c.moveToFirst();
            id = c.getInt(0);
            c.close();
        }
        return id;
    }

    static class LogEntry {
        Object item;
        long time;

        LogEntry(Object o) {
            item = o;
            time = System.currentTimeMillis();
        }

        void dump(PrintWriter out) {
            sTime.set(time);
            out.print(sTime.toString() + " : ");
            if (item instanceof Exception) {
                ((Exception) item).printStackTrace(out);
            } else {
                out.println(item);
            }
        }
    }

    private static LogEntry[] sMusicLog = new LogEntry[100];
    private static int sLogPtr = 0;
    private static Time sTime = new Time();

    static void debugLog(Object o) {

        sMusicLog[sLogPtr] = new LogEntry(o);
        sLogPtr++;
        if (sLogPtr >= sMusicLog.length) {
            sLogPtr = 0;
        }
    }

    static void debugDump(PrintWriter out) {
        for (int i = 0; i < sMusicLog.length; i++) {
            int idx = (sLogPtr + i);
            if (idx >= sMusicLog.length) {
                idx -= sMusicLog.length;
            }
            LogEntry entry = sMusicLog[idx];
            if (entry != null) {
                entry.dump(out);
            }
        }
    }

    public static int getAudioIDFromPath(Context context, String path) {
        int id = 0;
        String[] columns = new String[] { MediaStore.Audio.Media._ID };
        String where = MediaStore.Audio.Media.DISPLAY_NAME + "=?";
        String[] selectionArgs = { path };
        Cursor c = null;
        try {
            c = query(context, MediaStore.Audio.Media.EXTERNAL_CONTENT_URI,
                    columns, where, selectionArgs, null);
            if (c != null && c.moveToFirst()) {
                int i = c.getColumnIndexOrThrow(MediaStore.Audio.Media._ID);
                id = c.getInt(i);
            }
        } finally {
            if (c != null) {
                c.close();
            }
        }
        return id;
    }

    /**
     *
     * @return true if one track is palying. Otherwise false
     */
    public static boolean isPlaying() {
        if (sService != null) {
            try {
                return sService.isPlaying();
            } catch (RemoteException ex) {
            }
        }
        return false;
    }

    public static boolean shouldHideNowPlayingBar() {
        return (sService != null) && !isPlaying();
    }

    public static boolean isGroupByFolder() {
        return mGroupByFolder;
    }

    public static void startSoundEffectActivity(Activity activity) {
        Intent i = new Intent(AudioEffect.ACTION_DISPLAY_AUDIO_EFFECT_CONTROL_PANEL);
        try {
            i.putExtra(AudioEffect.EXTRA_AUDIO_SESSION, sService.getAudioSessionId());
        } catch (RemoteException ex) {
        }
        activity.startActivityForResult(i, Defs.EFFECTS_PANEL);
    }


    public static void loadSongsListDrawables(Context context, Cursor cursor, int from, int to,
            android.os.Handler handler) {

        BitmapDrawable defaultArtwork = (BitmapDrawable) context.getResources()
                .getDrawable(R.drawable.unknown_artists);

        if (cursor != null && cursor.moveToPosition(from)) {

            do {

                if (TrackBrowserFragment.isScrolling) {
                    break;
                }

                from++;
                int artIndex = cursor
                        .getColumnIndex(MediaStore.Audio.Media.ALBUM_ID);
                int mTitleIdx = cursor
                        .getColumnIndex(MediaStore.Audio.Media.TITLE);
                long index = cursor.getLong(artIndex);
                final Bitmap icon = defaultArtwork.getBitmap();
                int w = icon.getWidth();
                int h = icon.getHeight();
                Bitmap b = MusicUtils.getArtworkQuick(context, index, w, h);
                if (b != null) {
                    Drawable d = new FastBitmapDrawable(b);
                    synchronized (sArtCache) {
                        if (sArtCache.get(index) == null) {
                            sArtCache.put(index, d);

                            if (handler != null && !mIsScreenOff) {
                                handler.sendEmptyMessage(0);
                            }
                        }
                    }
                }

            } while (from <= to && cursor.moveToNext());

            if (cursor != null && !cursor.isClosed()) {
                cursor.close();
            }
        }

    }

    public static String getSelectAudioPath(Context context, long mSelectedId) {
        String result = "";
        if (null == context) {
            return result;
        }
        try {
            final String[] ccols = new String[] { MediaStore.Audio.Media.DATA };
            String where = MediaStore.Audio.Media._ID + "='" + mSelectedId + "'";
            Cursor cursor = query(context, MediaStore.Audio.Media.EXTERNAL_CONTENT_URI,
                                  ccols, where, null, null);
            if (null != cursor && 0 != cursor.getCount()) {
                cursor.moveToFirst();

               result = cursor.getString(cursor.getColumnIndexOrThrow(MediaStore.Audio.Media.DATA));
                cursor.close();
                return result;
            }

            if (null != cursor) {
                cursor.close();
                return result;
            }
        } catch (Exception ex) {
        }

        return result;
    }

    public static void getAlbumArtsForArtist(Context context,
            BitmapDrawable mDefaultAlbumIcon, int from, int to,
            android.os.Handler handler) {
        Cursor artistCursor = null, childCursor = null;
        int maxGrid = 4;
        try {

            String[] artistcols = new String[] { MediaStore.Audio.Artists._ID,
                    MediaStore.Audio.Artists.ARTIST,
                    MediaStore.Audio.Artists.NUMBER_OF_ALBUMS };
            Uri uri = MediaStore.Audio.Artists.EXTERNAL_CONTENT_URI;
            artistCursor = query(context, uri, artistcols, null, null,
                    MediaStore.Audio.Artists.ARTIST_KEY);
            String[] cols = new String[] { MediaStore.Audio.Albums._ID,
                    MediaStore.Audio.Albums.ALBUM,
                    MediaStore.Audio.Albums.ALBUM_ART };
            Bitmap albumArt = null;
            int i = from;
            if (artistCursor.moveToPosition(from)) {
                do {
                    if (i == to)
                        break;
                    i++;
                    long id = artistCursor
                            .getLong(artistCursor
                                    .getColumnIndexOrThrow(MediaStore.Audio.Artists._ID));
                    int numalbums = artistCursor
                            .getInt(artistCursor
                                    .getColumnIndexOrThrow(MediaStore.Audio.Artists.NUMBER_OF_ALBUMS));
                    int mAlbumArtCount = 0;
                    String artistName = artistCursor
                            .getString(artistCursor
                                    .getColumnIndexOrThrow(MediaStore.Audio.Artists.ARTIST));

                    if (getFromLruCache(artistName, mArtCache) != null)
                        continue;

                    childCursor = query(context,
                            MediaStore.Audio.Artists.Albums.getContentUri(
                                    "external", id), cols, null, null,
                            MediaStore.Audio.Albums.DEFAULT_SORT_ORDER);
                    childCursor.moveToFirst();
                    if (numalbums > maxGrid) {
                        mAlbumArtsArray = new Bitmap[maxGrid];
                    } else {
                        mAlbumArtsArray = new Bitmap[numalbums];
                    }

                    do {
                        if (mAlbumArtCount >= maxGrid) {
                            if (childCursor != null)
                                childCursor.close();
                            break;
                        } else {
                            String name = childCursor
                                    .getString(childCursor
                                            .getColumnIndexOrThrow(MediaStore.Audio.Albums.ALBUM));
                            boolean unknownalbum = name == null
                                    || name.equals(MediaStore.UNKNOWN_STRING);
                            boolean unknownartist = artistName == null
                                    || artistName
                                            .equals(MediaStore.UNKNOWN_STRING);
                            albumArt = null;
                            if (unknownalbum || unknownartist) {
                                albumArt = mDefaultAlbumIcon.getBitmap();
                                mAlbumArtsArray[mAlbumArtCount] = albumArt;
                            } else {
                                int colIndex = childCursor
                                        .getColumnIndexOrThrow(MediaStore.Audio.Albums.ALBUM_ART);
                                String art = childCursor.getString(colIndex);
                                if (art != null)
                                    albumArt = BitmapFactory.decodeFile(art);
                                if (albumArt != null) {
                                    mAlbumArtsArray[mAlbumArtCount] = albumArt;
                                } else {
                                    albumArt = mDefaultAlbumIcon.getBitmap();
                                    mAlbumArtsArray[mAlbumArtCount] = albumArt;
                                }
                            }
                            ++mAlbumArtCount;
                        }
                    } while (childCursor.moveToNext());
                    if (childCursor != null && !childCursor.isClosed())
                        childCursor.close();
                    addDateToCache(artistName, mAlbumArtsArray);
                    if (handler != null && !mIsScreenOff)
                        handler.sendEmptyMessage(0);

                } while (artistCursor.moveToNext());
            }
        } catch (Exception e) {
            Log.e(TAG, "Exception = " + e.getMessage());
        } finally {
            if (childCursor != null && !childCursor.isClosed())
                childCursor.close();
            if (artistCursor != null && !artistCursor.isClosed())
                artistCursor.close();
        }

    }

    public static boolean canClosePlaylistItemFragment(
            FragmentManager fragmentManager) {
        if (fragmentManager.findFragmentByTag("track_fragment") != null) {
            if (fragmentManager.findFragmentByTag("track_fragment").isAdded()
                    && !fragmentManager.findFragmentByTag("track_fragment")
                            .isDetached()) {
                fragmentManager
                        .beginTransaction()
                        .remove(fragmentManager
                                .findFragmentByTag("track_fragment")).commit();
                return true;
            }
        }
        if (fragmentManager.findFragmentByTag("folder_fragment") != null) {
            if (fragmentManager.findFragmentByTag("folder_fragment").isAdded()
                    && !fragmentManager.findFragmentByTag("folder_fragment")
                            .isDetached()) {
                fragmentManager
                        .beginTransaction()
                        .remove(fragmentManager
                                .findFragmentByTag("folder_fragment")).commit();
                return true;
            }
        }
        return false;
    }

    static class BitmapDownloadThread extends Thread {

        int toPosition;
        int fromPosition;
        final int BUFFER_POS = 5;
        Context ctx = null;
        Handler handler = null;

        public BitmapDownloadThread(Context ctx, Handler handler, int from,
                int to) {
            this.toPosition = to;
            this.fromPosition = from;
            this.ctx = ctx;
            this.handler = handler;
            ensureTopBottomPos();
        }

        private void ensureTopBottomPos() {
            if (fromPosition > BUFFER_POS)
                fromPosition -= BUFFER_POS;
            toPosition += BUFFER_POS;
        }

        @Override
        public void run() {
            super.run();
            android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_BACKGROUND);
            MusicUtils.getAlbumArtsForArtist(ctx, (BitmapDrawable) ctx
                    .getResources().getDrawable(R.drawable.unknown_artists),
                    fromPosition, toPosition, handler);

        }
    }

    static class AlbumBitmapDownloadThread extends Thread {

        int toPosition;
        int fromPosition;
        Context context = null;;
        Handler handler = null;
        Cursor cursor;

        public AlbumBitmapDownloadThread(Context context, Cursor cursor,
                Handler handler, int from, int to) {
            this.toPosition = to;
            this.fromPosition = from;
            this.context = context;
            this.handler = handler;
            this.cursor = cursor;
        }

        @Override
        public void run() {
            super.run();
            android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_BACKGROUND);
            MusicUtils.loadSongsListDrawables(context, cursor, fromPosition, toPosition, handler);
         }
    }

    static class FolderBitmapThread extends Thread {

        // Handler handler = null;
        BitmapDrawable defaultArtwork;
        private long songId;
        ImageView img;
        Activity context;
        private Bitmap bitmap;

        public FolderBitmapThread(Activity context, long songId,
                BitmapDrawable defaultArtwork, ImageView img) {
            this.defaultArtwork = defaultArtwork;
            this.songId = songId;
            this.img = img;
            // this.handler = handler;
            this.context = context;
        }

        @Override
        public void run() {
            super.run();
            android.os.Process
                    .setThreadPriority(android.os.Process.THREAD_PRIORITY_BACKGROUND);
            if (songId + "" != null) {
                bitmap = mFolderCache.get(songId + "");
                if (bitmap == null) {
                    bitmap = getFolderBitmap(context, songId);
                    if(bitmap != null)
                    mFolderCache.put(songId + "", bitmap);
                }
            }

            if (img != null && !mIsScreenOff) {
                context.runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        // TODO Auto-generated method stub
                        if (bitmap != null)
                            img.setImageBitmap(bitmap);
                        else
                            img.setImageDrawable(defaultArtwork);
                    }
                });
            }
        }
    }

    public static Bitmap getFolderBitmap(Context context, long songId) {
        ContentResolver res = context.getContentResolver();
        Uri uri = null;
        int h = 100, w = 100;
        Bitmap bitmap = null;
        if (songId != -1) {
            String selection = MediaStore.Audio.Media._ID + " = " + songId + "";

            Cursor cursor = res.query(
                    MediaStore.Audio.Media.EXTERNAL_CONTENT_URI, new String[] {
                            MediaStore.Audio.Media._ID,
                            MediaStore.Audio.Media.ALBUM_ID }, selection, null,
                    null);

            if (cursor.moveToFirst()) {
                long albumId = cursor.getLong(cursor
                        .getColumnIndex(MediaStore.Audio.Media.ALBUM_ID));
                uri = ContentUris.withAppendedId(sArtworkUri, albumId);
            }
            cursor.close();

            if (uri != null) {
                ParcelFileDescriptor fd = null;
                try {
                    fd = res.openFileDescriptor(uri, "r");
                    int sampleSize = 1;

                    if (fd != null) {
                        sBitmapOptionsCache.inJustDecodeBounds = true;
                        BitmapFactory.decodeFileDescriptor(
                                fd.getFileDescriptor(), null,
                                sBitmapOptionsCache);
                        int nextWidth = sBitmapOptionsCache.outWidth >> 1;
                        int nextHeight = sBitmapOptionsCache.outHeight >> 1;
                        while (nextWidth > w && nextHeight > h) {
                            sampleSize <<= 1;
                            nextWidth >>= 1;
                            nextHeight >>= 1;
                        }

                        sBitmapOptionsCache.inSampleSize = sampleSize;
                        sBitmapOptionsCache.inJustDecodeBounds = false;
                        Bitmap b = BitmapFactory.decodeFileDescriptor(
                                fd.getFileDescriptor(), null,
                                sBitmapOptionsCache);

                        if (b != null) {
                            if (sBitmapOptionsCache.outWidth != w
                                    || sBitmapOptionsCache.outHeight != h) {
                                Bitmap tmp = Bitmap.createScaledBitmap(b, w, h,
                                        true);
                                b = tmp;
                            }
                        }

                        return b;
                    }
                } catch (FileNotFoundException e) {
                } finally {
                    try {
                        if (fd != null)
                            fd.close();
                    } catch (IOException e) {
                    }
                }
            }
        }
        return bitmap;

    }

    public static boolean isTelephonyCallInProgress(Context context) {
        if (context == null)
            return false;
        TelephonyManager telephonyManager = (TelephonyManager) context.getSystemService(Context.TELEPHONY_SERVICE);
        int state = telephonyManager.getCallState();
        return (state == TelephonyManager.CALL_STATE_OFFHOOK
                || state == TelephonyManager.CALL_STATE_RINGING);
    }

    public static boolean isForbidPlaybackInCall(Context context) {
        if (context == null)
            return false;

        if (isTelephonyCallInProgress(context)) {
            Log.d("MusicUtils", "playAll  in a call");
            // Don't try to play music in a call begin from android Q.
            Toast.makeText(context, R.string.cant_play_music_in_call, Toast.LENGTH_SHORT).show();
            return true;
        }

        return false;
    }

    public static void addSetRingtonMenu(Menu menu) {
        menu.add(0, Defs.USE_AS_RINGTONE, 0, R.string.ringtone_menu);
    }

    public static <K, V> V getFromLruCache(K key, LruCache<K, V> lruCache) {
        if (key == null || lruCache == null){
            return null;
        }

        return  lruCache.get(key);
    }

    public static <K, V> V putIntoLruCache(K key, V value, LruCache<K, V> lruCache) {
        if (key == null || value == null || lruCache == null){
            return null;
        }

        return  lruCache.put(key, value);
    }

    public static void startService(Context context, Intent intent) {
        if (context == null || intent == null){
            Log.e(TAG, "context or intent null");
            return ;
        }

        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O) {
            context.startForegroundService(intent);
        } else {
            context.startService(intent);
        }
        return;
    }

}
