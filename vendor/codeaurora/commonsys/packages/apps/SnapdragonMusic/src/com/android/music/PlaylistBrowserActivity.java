/*
 * Copyright (C) 2007 The Android Open Source Project
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

import com.android.music.MusicUtils.ServiceToken;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.AsyncQueryHandler;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.ServiceConnection;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.database.MergeCursor;
import android.database.sqlite.SQLiteException;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.media.AudioManager;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.RemoteException;
import android.provider.MediaStore;
import android.util.Log;
import android.view.ContextMenu;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.ContextMenu.ContextMenuInfo;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.GridView;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.SimpleCursorAdapter;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.AdapterView.AdapterContextMenuInfo;
import android.view.KeyEvent;
import android.content.Intent;
import android.content.res.Resources;

import com.android.music.SysApplication;

import java.text.Collator;
import java.util.ArrayList;

public class PlaylistBrowserActivity extends Activity implements
        View.OnCreateContextMenuListener, MusicUtils.Defs {
    private static final String TAG = "PlaylistBrowserActivity";
    private static final int DELETE_PLAYLIST = CHILD_MENU_BASE + 1;
    private static final int EDIT_PLAYLIST = CHILD_MENU_BASE + 2;
    private static final int RENAME_PLAYLIST = CHILD_MENU_BASE + 3;
    private static final int CHANGE_WEEKS = CHILD_MENU_BASE + 4;
    private static final int CLEAR_ALL_PLAYLISTS = CHILD_MENU_BASE + 5;
    private static final long RECENTLY_ADDED_PLAYLIST = -1;
    private static final long ALL_SONGS_PLAYLIST = -2;
    private static final long PODCASTS_PLAYLIST = -3;
    private PlaylistListAdapter mAdapter;
    boolean mAdapterSent;
    private static int mLastListPosCourse = -1;
    private static int mLastListPosFine = -1;
    private CharSequence mTitle;
    private boolean mCreateShortcut;
    private ServiceToken mToken;
    private GridView mGridView;

    public PlaylistBrowserActivity() {
    }

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);

        final Intent intent = getIntent();
        final String action = intent.getAction();
        if (Intent.ACTION_CREATE_SHORTCUT.equals(action)) {
            mCreateShortcut = true;
        }

        requestWindowFeature(Window.FEATURE_INDETERMINATE_PROGRESS);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        setVolumeControlStream(AudioManager.STREAM_MUSIC);
        mToken = MusicUtils.bindToService(this, new ServiceConnection() {
            public void onServiceConnected(ComponentName classname, IBinder obj) {
                if (Intent.ACTION_VIEW.equals(action)) {
                    Bundle b = intent.getExtras();
                    if (b == null) {
                        Log.w(TAG, "Unexpected:getExtras() returns null.");
                    } else {
                        try {
                            long id = Long.parseLong(b.getString("playlist"));
                            if (id == RECENTLY_ADDED_PLAYLIST) {
                                playRecentlyAdded();
                            } else if (id == PODCASTS_PLAYLIST) {
                                playPodcasts();
                            } else if (id == ALL_SONGS_PLAYLIST) {
                                long[] list = MusicUtils
                                        .getAllSongs(PlaylistBrowserActivity.this);
                                if (list != null) {
                                    MusicUtils.playAll(
                                            PlaylistBrowserActivity.this, list,
                                            0);
                                } else {
                                    Toast.makeText(
                                            PlaylistBrowserActivity.this,
                                            R.string.list_empty,
                                            Toast.LENGTH_SHORT).show();
                                }
                            } else {
                                MusicUtils.playPlaylist(
                                        PlaylistBrowserActivity.this, id);
                            }
                        } catch (NumberFormatException e) {
                            Log.w(TAG, "Playlist id missing or broken");
                        }
                    }
                    finish();
                    return;
                }
                MusicUtils
                        .updateNowPlaying(PlaylistBrowserActivity.this, false);
            }

            public void onServiceDisconnected(ComponentName classname) {
            }

        });
        IntentFilter f = new IntentFilter();
        f.addAction(Intent.ACTION_MEDIA_SCANNER_STARTED);
        f.addAction(Intent.ACTION_MEDIA_SCANNER_FINISHED);
        f.addAction(Intent.ACTION_MEDIA_UNMOUNTED);
        f.addDataScheme("file");
        registerReceiver(mScanListener, f);
        setContentView(R.layout.media_picker_activity_album);
        mGridView = (GridView) findViewById(R.id.album_list);
        mGridView.setOnCreateContextMenuListener(this);
        mGridView.setTextFilterEnabled(true);
        mGridView.setOnItemClickListener(new OnItemClickListener() {

            @Override
            public void onItemClick(AdapterView<?> parent, View view,
                    int position, long id) {
                if (mCreateShortcut) {
                    final Intent shortcut = new Intent();
                    shortcut.setAction(Intent.ACTION_VIEW);
                    shortcut.setDataAndType(Uri.EMPTY,
                            "vnd.android.cursor.dir/playlist");
                    shortcut.putExtra("playlist", String.valueOf(id));

                    final Intent intent = new Intent();
                    intent.putExtra(Intent.EXTRA_SHORTCUT_INTENT, shortcut);
                    intent.putExtra(Intent.EXTRA_SHORTCUT_NAME,
                            ((TextView) view.findViewById(R.id.line1))
                                    .getText());
                    intent.putExtra(
                            Intent.EXTRA_SHORTCUT_ICON_RESOURCE,
                            Intent.ShortcutIconResource
                                    .fromContext(
                                            PlaylistBrowserActivity.this,
                                            R.drawable.ic_launcher_shortcut_music_playlist));

                    setResult(RESULT_OK, intent);
                    finish();
                    return;
                }
                if (id == RECENTLY_ADDED_PLAYLIST) {
                    Intent intent = new Intent(Intent.ACTION_PICK);
                    intent.setDataAndType(Uri.EMPTY,
                            "vnd.android.cursor.dir/track");
                    intent.putExtra("playlist", "recentlyadded");
                    startActivity(intent);
                } else if (id == PODCASTS_PLAYLIST) {
                    Intent intent = new Intent(Intent.ACTION_PICK);
                    intent.setDataAndType(Uri.EMPTY,
                            "vnd.android.cursor.dir/track");
                    intent.putExtra("playlist", "podcasts");
                    startActivity(intent);
                } else {
                    Intent intent = new Intent(Intent.ACTION_EDIT);
                    intent.setDataAndType(Uri.EMPTY,
                            "vnd.android.cursor.dir/track");
                    intent.putExtra("playlist", Long.valueOf(id).toString());
                    startActivity(intent);
                }

            }
        });

        mAdapter = (PlaylistListAdapter) getLastNonConfigurationInstance();
        if (mAdapter == null) {
            mAdapter = new PlaylistListAdapter(getApplication(), this,
                    R.layout.track_list_common_playlist, mPlaylistCursor,
                    new String[] { MediaStore.Audio.Playlists.NAME },
                    new int[] { android.R.id.text1 });
            mGridView.setAdapter(mAdapter);
            setTitle(R.string.working_playlists);
            getPlaylistCursor(mAdapter.getQueryHandler(), null);
        } else {
            mAdapter.setActivity(this);
            mGridView.setAdapter(mAdapter);
            mPlaylistCursor = mAdapter.getCursor();
            // If mPlaylistCursor is null, this can be because it doesn't have
            // a cursor yet (because the initial query that sets its cursor
            // is still in progress), or because the query failed.
            // In order to not flash the error dialog at the user for the
            // first case, simply retry the query when the cursor is null.
            // Worst case, we end up doing the same query twice.
            if (mPlaylistCursor != null) {
                init(mPlaylistCursor);
            } else {
                setTitle(R.string.working_playlists);
                getPlaylistCursor(mAdapter.getQueryHandler(), null);
            }
        }
        SysApplication.getInstance().addActivity(this);
    }

    @Override
    public Object onRetainNonConfigurationInstance() {
        PlaylistListAdapter a = mAdapter;
        mAdapterSent = true;
        return a;
    }

    @Override
    public void onDestroy() {
        if (mGridView != null) {
            mLastListPosCourse = mGridView.getFirstVisiblePosition();
            View cv = mGridView.getChildAt(0);
            if (cv != null) {
                mLastListPosFine = cv.getTop();
            }
        }
        MusicUtils.unbindFromService(mToken);
        // If we have an adapter and didn't send it off to another activity yet,
        // we should close its cursor, which we do by assigning a null cursor to it.
        // Doing this instead of closing the cursor directly keeps the framework from
        // accessing the closed cursor later.
        if (!mAdapterSent && mAdapter != null) {
            mAdapter.changeCursor(null);
        }
        // Because we pass the adapter to the next activity, we need to make
        // sure it doesn't keep a reference to this activity. We can do this
        // by clearing its DatasetObservers, which setListAdapter(null) does.
        mGridView.setAdapter(null);
        mAdapter = null;
        unregisterReceiver(mScanListener);
        super.onDestroy();
    }

    @Override
    public void onResume() {
        super.onResume();

        IntentFilter f = new IntentFilter();
        f.addAction(MediaPlaybackService.META_CHANGED);
        f.addAction(MediaPlaybackService.QUEUE_CHANGED);
        registerReceiver(mTrackListListener, f);
        mTrackListListener.onReceive(null, null);
        MusicUtils.setSpinnerState(this);
        MusicUtils.updateNowPlaying(PlaylistBrowserActivity.this, false);
        // When system language is changed, the name of "Recently added" is also
        // changed at the same time. So we should update the cursor to refresh the listview.
        if (mAdapter != null) {
            getPlaylistCursor(mAdapter.getQueryHandler(), null);
        }
    }

    @Override
    public void onPause() {
        unregisterReceiver(mTrackListListener);
        mReScanHandler.removeCallbacksAndMessages(null);
        super.onPause();
    }

    private BroadcastReceiver mTrackListListener = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            mGridView.invalidateViews();
            MusicUtils.updateNowPlaying(PlaylistBrowserActivity.this, false);
        }
    };
    private BroadcastReceiver mScanListener = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            MusicUtils.setSpinnerState(PlaylistBrowserActivity.this);
            mReScanHandler.sendEmptyMessage(0);
        }
    };

    private Handler mReScanHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            if (mAdapter != null) {
                getPlaylistCursor(mAdapter.getQueryHandler(), null);
            }
        }
    };

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (event.getAction() == KeyEvent.ACTION_UP
                && event.getKeyCode() == KeyEvent.KEYCODE_BACK) {
            finish();
            return true;
        }

        return super.dispatchKeyEvent(event);
    }

    public void init(Cursor cursor) {

        if (mAdapter == null) {
            return;
        }
        mAdapter.changeCursor(cursor);

        if (mPlaylistCursor == null) {
            MusicUtils.displayDatabaseError(this);
            closeContextMenu();
            mReScanHandler.sendEmptyMessageDelayed(0, 1000);
            return;
        }

        // restore previous position
        if (mLastListPosCourse >= 0) {
            mGridView.setSelectionFromTop(mLastListPosCourse, mLastListPosFine);
            mLastListPosCourse = -1;
        }
        MusicUtils.hideDatabaseError(this);
        setTitle();
    }

    private void setTitle() {
        setTitle(R.string.playlists_title);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        if (!mCreateShortcut) {
            // icon will be set in onPrepareOptionsMenu()
            menu.add(0, PARTY_SHUFFLE, 0, R.string.party_shuffle);

            menu.add(0, CLEAR_ALL_PLAYLISTS, 0, R.string.clear_all_playlists)
                    .setIcon(R.drawable.ic_menu_clear_playlist);
            menu.add(0, CLOSE, 0, R.string.close_music).setIcon(
                    R.drawable.quick_panel_music_close);
            if (getResources().getBoolean(
                    R.bool.def_music_add_more_video_enabled))
                menu.add(0, MORE_MUSIC, 0, R.string.more_music).setIcon(
                        R.drawable.ic_menu_music_library);
        }
        return super.onCreateOptionsMenu(menu);
    }

    @Override
    public boolean onPrepareOptionsMenu(Menu menu) {
        MusicUtils.setPartyShuffleMenuIcon(menu);
        return super.onPrepareOptionsMenu(menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        Intent intent = new Intent();
        switch (item.getItemId()) {
        case MORE_MUSIC:
            Uri MoreUri = Uri.parse(getResources().getString(
                    R.string.def_music_add_more_music));
            Intent MoreIntent = new Intent(Intent.ACTION_VIEW, MoreUri);
            startActivity(MoreIntent);
            break;
        case PARTY_SHUFFLE:
            MusicUtils.togglePartyShuffle();
            AudioManager audioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
            audioManager.playSoundEffect(AudioManager.FX_KEY_CLICK);
            break;
        case CLEAR_ALL_PLAYLISTS:
            String desc = getString(R.string.clear_all_playlists_message);
            Uri uri = MediaStore.Audio.Playlists.EXTERNAL_CONTENT_URI;
            Bundle b = new Bundle();
            b.putString("description", desc);
            b.putParcelable("Playlist", uri);
            intent.setClass(this, DeleteItems.class);
            intent.putExtras(b);
            startActivityForResult(intent, -1);
            break;
        case CLOSE:
            try {
                if (MusicUtils.sService != null) {
                    MusicUtils.sService.stop();
                }
            } catch (RemoteException ex) {
            }
            SysApplication.getInstance().exit();
            break;
        }
        return super.onOptionsItemSelected(item);
    }

    public void onCreateContextMenu(ContextMenu menu, View view,
            ContextMenuInfo menuInfoIn) {
        if (mCreateShortcut) {
            return;
        }

        AdapterContextMenuInfo mi = (AdapterContextMenuInfo) menuInfoIn;

        menu.add(0, PLAY_SELECTION, 0, R.string.play_selection);

        if (mi.id >= 0) {
            menu.add(0, DELETE_PLAYLIST, 0, R.string.delete_playlist_menu);
        }

        if (mi.id == RECENTLY_ADDED_PLAYLIST) {
            menu.add(0, EDIT_PLAYLIST, 0, R.string.edit_playlist_menu);
        }

        if (mi.id >= 0) {
            menu.add(0, RENAME_PLAYLIST, 0, R.string.rename_playlist_menu);
            MenuItem menuItem = menu.findItem(RENAME_PLAYLIST);
            menuItem.setVisible(false);
        }

        mPlaylistCursor.moveToPosition(mi.position);
        mTitle = mPlaylistCursor.getString(mPlaylistCursor
                .getColumnIndexOrThrow(MediaStore.Audio.Playlists.NAME));
        menu.setHeaderTitle(mTitle.equals("My recordings") ? getResources()
                .getString(R.string.audio_db_playlist_name) : mTitle);
    }

    @Override
    public boolean onContextItemSelected(MenuItem item) {
        AdapterContextMenuInfo mi = (AdapterContextMenuInfo) item.getMenuInfo();
        Intent intent = new Intent();
        switch (item.getItemId()) {
        case PLAY_SELECTION:
            if (mi.id == RECENTLY_ADDED_PLAYLIST) {
                playRecentlyAdded();
            } else if (mi.id == PODCASTS_PLAYLIST) {
                playPodcasts();
            } else {
                MusicUtils.playPlaylist(this, mi.id);
            }
            break;
        case DELETE_PLAYLIST:
            // it may not convenient to users when delete new or exist
            // playlist without any notification.
            // show a dialog to confirm deleting this playlist.
            // get playlist name
            if ("My recordings".equals(mTitle)) {
                mTitle = this.getResources().getString(
                        R.string.audio_db_playlist_name);
            }
            String desc = getString(R.string.delete_playlist_message, mTitle);
            Uri uri = ContentUris.withAppendedId(
                    MediaStore.Audio.Playlists.EXTERNAL_CONTENT_URI, mi.id);
            Bundle b = new Bundle();
            b.putString("description", desc);
            b.putParcelable("Playlist", uri);
            intent.setClass(this, DeleteItems.class);
            intent.putExtras(b);
            startActivityForResult(intent, -1);
            break;
        case EDIT_PLAYLIST:
            if (mi.id == RECENTLY_ADDED_PLAYLIST) {
                intent.setClass(this, WeekSelector.class);
                startActivityForResult(intent, CHANGE_WEEKS);
                return true;
            } else {
                Log.e(TAG, "should not be here");
            }
            break;
        case RENAME_PLAYLIST:
            intent.setClass(this, RenamePlaylist.class);
            intent.putExtra("rename", mi.id);
            startActivityForResult(intent, RENAME_PLAYLIST);
            break;
        }
        return true;
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode,
            Intent intent) {
        switch (requestCode) {
        case SCAN_DONE:
            if (resultCode == RESULT_CANCELED) {
                finish();
            } else if (mAdapter != null) {
                getPlaylistCursor(mAdapter.getQueryHandler(), null);
            }
            break;
        }
    }

    private void playRecentlyAdded() {
        // do a query for all songs added in the last X weeks
        int X = MusicUtils.getIntPref(this, "numweeks", 2) * (3600 * 24 * 7);
        final String[] ccols = new String[] { MediaStore.Audio.Media._ID };
        String where = MediaStore.MediaColumns.DATE_ADDED + ">"
                + (System.currentTimeMillis() / 1000 - X);
        Cursor cursor = MusicUtils.query(this,
                MediaStore.Audio.Media.EXTERNAL_CONTENT_URI, ccols, where,
                null, MediaStore.Audio.Media.DEFAULT_SORT_ORDER);

        if (cursor == null) {
            return;
        }
        try {
            int len = cursor.getCount();
            long[] list = new long[len];
            for (int i = 0; i < len; i++) {
                cursor.moveToNext();
                list[i] = cursor.getLong(0);
            }
            MusicUtils.playAll(this, list, 0);
        } catch (SQLiteException ex) {
        } finally {
            cursor.close();
        }
    }

    private void playPodcasts() {
        // do a query for all files that are podcasts
        final String[] ccols = new String[] { MediaStore.Audio.Media._ID };
        Cursor cursor = MusicUtils.query(this,
                MediaStore.Audio.Media.EXTERNAL_CONTENT_URI, ccols,
                MediaStore.Audio.Media.IS_PODCAST + "=1", null,
                MediaStore.Audio.Media.DEFAULT_SORT_ORDER);

        if (cursor == null) {
            // Todo: show a message
            return;
        }
        try {
            int len = cursor.getCount();
            long[] list = new long[len];
            for (int i = 0; i < len; i++) {
                cursor.moveToNext();
                list[i] = cursor.getLong(0);
            }
            MusicUtils.playAll(this, list, 0);
        } catch (SQLiteException ex) {
        } finally {
            cursor.close();
        }
    }

    String[] mCols = new String[] { MediaStore.Audio.Playlists._ID,
            MediaStore.Audio.Playlists.NAME };

    private Cursor getPlaylistCursor(AsyncQueryHandler async,
            String filterstring) {

        StringBuilder where = new StringBuilder();
        where.append(MediaStore.Audio.Playlists.NAME + " != ''");

        // Add in the filtering constraints
        String[] keywords = null;
        if (filterstring != null) {
            String[] searchWords = filterstring.split(" ");
            keywords = new String[searchWords.length];
            Collator col = Collator.getInstance();
            col.setStrength(Collator.PRIMARY);
            for (int i = 0; i < searchWords.length; i++) {
                keywords[i] = '%' + searchWords[i] + '%';
            }
            for (int i = 0; i < searchWords.length; i++) {
                where.append(" AND ");
                where.append(MediaStore.Audio.Playlists.NAME + " LIKE ?");
            }
        }

        String whereclause = where.toString();

        if (async != null) {
            async.startQuery(0, null,
                    MediaStore.Audio.Playlists.EXTERNAL_CONTENT_URI, mCols,
                    whereclause, keywords, MediaStore.Audio.Playlists.NAME);
            return null;
        }
        Cursor c = null;
        c = MusicUtils.query(this,
                MediaStore.Audio.Playlists.EXTERNAL_CONTENT_URI, mCols,
                whereclause, keywords, MediaStore.Audio.Playlists.NAME);

        return mergedCursor(c);
    }

    private Cursor mergedCursor(Cursor c) {
        if (c == null) {
            return null;
        }
        if (c instanceof MergeCursor) {
            // this shouldn't happen, but fail gracefully
            Log.d("PlaylistBrowserActivity", "Already wrapped");
            return c;
        }
        MatrixCursor autoplaylistscursor = new MatrixCursor(mCols);
        if (mCreateShortcut) {
            ArrayList<Object> all = new ArrayList<Object>(2);
            all.add(ALL_SONGS_PLAYLIST);
            all.add(getString(R.string.play_all));
            autoplaylistscursor.addRow(all);
        }
        ArrayList<Object> recent = new ArrayList<Object>(2);
        recent.add(RECENTLY_ADDED_PLAYLIST);
        recent.add(getString(R.string.recentlyadded));
        autoplaylistscursor.addRow(recent);

        // check if there are any podcasts
        Cursor counter = MusicUtils.query(this,
                MediaStore.Audio.Media.EXTERNAL_CONTENT_URI,
                new String[] { "count(*)" }, "is_podcast=1", null, null);
        if (counter != null) {
            counter.moveToFirst();
            int numpodcasts = counter.getInt(0);
            counter.close();
            if (numpodcasts > 0) {
                ArrayList<Object> podcasts = new ArrayList<Object>(2);
                podcasts.add(PODCASTS_PLAYLIST);
                podcasts.add(getString(R.string.podcasts_listitem));
                autoplaylistscursor.addRow(podcasts);
            }
        }
        Cursor cc = new MergeCursor(new Cursor[] { autoplaylistscursor, c });
        return cc;
    }

    static class PlaylistListAdapter extends SimpleCursorAdapter {
        int mTitleIdx;
        int mIdIdx;
        private PlaylistBrowserActivity mActivity = null;
        private AsyncQueryHandler mQueryHandler;
        private String mConstraint = null;
        private boolean mConstraintIsValid = false;
        private final BitmapDrawable mDefaultAlbumIcon;

        class QueryHandler extends AsyncQueryHandler {
            QueryHandler(ContentResolver res) {
                super(res);
            }

            @Override
            protected void onQueryComplete(int token, Object cookie,
                    Cursor cursor) {
                if (cursor != null) {
                    cursor = mActivity.mergedCursor(cursor);
                }
                mActivity.init(cursor);
            }
        }

        PlaylistListAdapter(Context context,
                PlaylistBrowserActivity currentactivity, int layout,
                Cursor cursor, String[] from, int[] to) {
            super(context, layout, cursor, from, to);
            mActivity = currentactivity;
            getColumnIndices(cursor);
            mQueryHandler = new QueryHandler(context.getContentResolver());
            Resources r = context.getResources();
            mDefaultAlbumIcon = (BitmapDrawable) r
                    .getDrawable(R.drawable.unknown_albums);
        }

        private void getColumnIndices(Cursor cursor) {
            if (cursor != null) {
                mTitleIdx = cursor
                        .getColumnIndexOrThrow(MediaStore.Audio.Playlists.NAME);
                mIdIdx = cursor
                        .getColumnIndexOrThrow(MediaStore.Audio.Playlists._ID);
            }
        }

        public void setActivity(PlaylistBrowserActivity newactivity) {
            mActivity = newactivity;
        }

        public AsyncQueryHandler getQueryHandler() {
            return mQueryHandler;
        }

        @Override
        public void bindView(View view, Context context, Cursor cursor) {

            TextView tv = (TextView) view.findViewById(R.id.line1);

            String name = cursor.getString(mTitleIdx);

            if (name.equals("My recordings")) {
                name = mActivity.getResources().getString(
                        R.string.audio_db_playlist_name);
            }

            tv.setText(name);
            ImageView iv = (ImageView) view.findViewById(R.id.icon);
            ImageView iv1 = (ImageView) view.findViewById(R.id.icon1);
            ImageView iv2 = (ImageView) view.findViewById(R.id.icon2);
            ImageView iv3 = (ImageView) view.findViewById(R.id.icon3);
            Uri uri = MediaStore.Audio.Media.EXTERNAL_CONTENT_URI;
            String mPlaylistMemberCols[] = new String[] {
                    MediaStore.Audio.Playlists.Members._ID,
                    MediaStore.Audio.Media.TITLE, MediaStore.Audio.Media.DATA,
                    MediaStore.Audio.Media.ALBUM,
                    MediaStore.Audio.Media.ARTIST,
                    MediaStore.Audio.Media.ARTIST_ID,
                    MediaStore.Audio.Media.DURATION,
                    MediaStore.Audio.Playlists.Members.PLAY_ORDER,
                    MediaStore.Audio.Playlists.Members.AUDIO_ID,
                    MediaStore.Audio.Media.IS_MUSIC };

            String mCursorCols[] = new String[] { MediaStore.Audio.Media._ID,
                    MediaStore.Audio.Media.TITLE, MediaStore.Audio.Media.DATA,
                    MediaStore.Audio.Media.ALBUM,
                    MediaStore.Audio.Media.ARTIST,
                    MediaStore.Audio.Media.ARTIST_ID,
                    MediaStore.Audio.Media.DURATION };

            Cursor c = MusicUtils.query(mActivity, uri, mCursorCols, null,
                    null, MediaStore.Audio.Albums.DEFAULT_SORT_ORDER);
            Bitmap b = null;
            Cursor c1 = null;
            try {
                if (c.moveToFirst()) {
                    do {
                        String name1 = c.getString(c.getColumnIndexOrThrow(MediaStore.Audio.Media.ALBUM));
                        boolean unknownalbum = (name1 == null)
                                || name1.equals(MediaStore.UNKNOWN_STRING);
                        if (unknownalbum) {
                            b = mDefaultAlbumIcon.getBitmap();
                        } else {
                            String[] cols = new String[] {
                                    MediaStore.Audio.Albums._ID,
                                    MediaStore.Audio.Albums.ARTIST,
                                    MediaStore.Audio.Albums.ALBUM,
                                    MediaStore.Audio.Albums.ALBUM_ART };
                            Uri uri1 = MediaStore.Audio.Artists.Albums.getContentUri("external",
					Long.valueOf(c.getColumnIndexOrThrow(MediaStore.Audio.Media.ARTIST_ID)));
                            c1 = MusicUtils.query(mActivity, uri1, cols, null,
                                    null,
                                    MediaStore.Audio.Albums.DEFAULT_SORT_ORDER);
                            c1.moveToFirst();
                            int colIndex = c1
                                    .getColumnIndexOrThrow(MediaStore.Audio.Albums.ALBUM_ART);
                            String art = c1.getString(colIndex);
                            b = BitmapFactory.decodeFile(art);
                            c1.close();
                        }
                    } while (c.moveToNext());
                }
            } catch (Exception e) {
                Log.e(TAG, "Exception caught");
            } finally {
                if (c != null) {
                    c.close();
                }
                if (c1 != null) {
                    c1.close();
                }
            }
            if (b != null) {
                iv.setImageBitmap(b);
                iv1.setImageBitmap(b);
                iv2.setImageBitmap(b);
                iv3.setImageBitmap(b);
            }
        }

        @Override
        public void changeCursor(Cursor cursor) {
            if (mActivity.isFinishing() && cursor != null) {
                cursor.close();
                cursor = null;
            }
            if (cursor != mActivity.mPlaylistCursor) {
                if (mActivity.mPlaylistCursor != null) {
                    mActivity.mPlaylistCursor.close();
                    mActivity.mPlaylistCursor = null;
                }
                mActivity.mPlaylistCursor = cursor;
                super.changeCursor(cursor);
                getColumnIndices(cursor);
            }
        }

        @Override
        public Cursor runQueryOnBackgroundThread(CharSequence constraint) {
            String s = constraint.toString();
            if (mConstraintIsValid && (
                    (s == null && mConstraint == null) ||
                    (s != null && s.equals(mConstraint)))) {
                return getCursor();
            }
            Cursor c = mActivity.getPlaylistCursor(null, s);
            mConstraint = s;
            mConstraintIsValid = true;
            return c;
        }
    }

    private Cursor mPlaylistCursor;
}
