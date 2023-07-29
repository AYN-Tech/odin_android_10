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
import android.app.ListActivity;
import android.app.SearchManager;
import android.content.AsyncQueryHandler;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.ServiceConnection;
import android.content.res.Resources;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.media.AudioManager;
import android.media.audiofx.AudioEffect;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.RemoteException;
import android.provider.MediaStore;
import android.text.TextUtils;
import android.util.Log;
import android.view.ContextMenu;
import android.view.Menu;
import android.view.MenuItem;
import android.view.SubMenu;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.Window;
import android.view.ContextMenu.ContextMenuInfo;
import android.widget.Adapter;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.AlphabetIndexer;
import android.widget.CursorAdapter;
import android.widget.ExpandableListView;
import android.widget.GridView;
import android.widget.ImageView;
import android.widget.ListAdapter;
import android.widget.ListView;
import android.widget.PopupMenu;
import android.widget.PopupMenu.OnMenuItemClickListener;
import android.widget.RelativeLayout;
import android.widget.SectionIndexer;
import android.widget.SimpleCursorAdapter;
import android.widget.TextView;
import android.widget.AdapterView.AdapterContextMenuInfo;
import android.view.KeyEvent;
import com.android.music.SysApplication;

import java.text.Collator;

public class AlbumBrowserActivity extends Activity implements
        View.OnCreateContextMenuListener, MusicUtils.Defs, ServiceConnection {
    private String mCurrentAlbumId;
    private String mCurrentAlbumName;
    private String mCurrentArtistNameForAlbum;
    boolean mIsUnknownArtist;
    boolean mIsUnknownAlbum;
    private AlbumListAdapter mAdapter;
    private boolean mAdapterSent;
    private final static int SEARCH = CHILD_MENU_BASE;
    private static int mLastListPosCourse = -1;
    private static int mLastListPosFine = -1;
    private ServiceToken mToken;
    private GridView mAlbumList;

    public AlbumBrowserActivity() {
    }

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle icicle) {
        if (icicle != null) {
            mCurrentAlbumId = icicle.getString("selectedalbum");
            mArtistId = icicle.getString("artist");
        } else {
            mArtistId = getIntent().getStringExtra("artist");
        }
        super.onCreate(icicle);
        requestWindowFeature(Window.FEATURE_INDETERMINATE_PROGRESS);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        setVolumeControlStream(AudioManager.STREAM_MUSIC);
        mToken = MusicUtils.bindToService(this, this);

        IntentFilter f = new IntentFilter();
        f.addAction(Intent.ACTION_MEDIA_SCANNER_STARTED);
        f.addAction(Intent.ACTION_MEDIA_SCANNER_FINISHED);
        f.addAction(Intent.ACTION_MEDIA_UNMOUNTED);
        f.addDataScheme("file");
        registerReceiver(mScanListener, f);

        setContentView(R.layout.media_picker_activity_album);

        mAlbumList = (GridView) findViewById(R.id.album_list);
        mAlbumList.setOnItemClickListener(new OnItemClickListener() {
            public void onItemClick(AdapterView<?> parent, View v,
                    int position, long id) {
                Intent intent = new Intent(Intent.ACTION_PICK);
                intent.setDataAndType(Uri.EMPTY, "vnd.android.cursor.dir/track");
                intent.putExtra("album", Long.valueOf(id).toString());
                intent.putExtra("artist", mArtistId);
                startActivity(intent);
            }
        });
        mAlbumList.setTextFilterEnabled(true);

        mAdapter = (AlbumListAdapter) getLastNonConfigurationInstance();
        if (mAdapter == null) {
            mAdapter = new AlbumListAdapter(getApplication(), this,
                    R.layout.track_list_item_album, mAlbumCursor,
                    new String[] {}, new int[] {});
            mAlbumList.setAdapter(mAdapter);
            setTitle(R.string.working_albums);
            getAlbumCursor(mAdapter.getQueryHandler(), null);
        } else {
            mAdapter.setActivity(this);
            mAlbumList.setAdapter(mAdapter);
            mAlbumCursor = mAdapter.getCursor();
            if (mAlbumCursor != null) {
                init(mAlbumCursor);
            } else {
                getAlbumCursor(mAdapter.getQueryHandler(), null);
            }
        }
        SysApplication.getInstance().addActivity(this);
    }

    @Override
    public Object onRetainNonConfigurationInstance() {
        mAdapterSent = true;
        return mAdapter;
    }

    @Override
    public void onSaveInstanceState(Bundle outcicle) {
        // need to store the selected item so we don't lose it in case
        // of an orientation switch. Otherwise we could lose it while
        // in the middle of specifying a playlist to add the item to.
        outcicle.putString("selectedalbum", mCurrentAlbumId);
        outcicle.putString("artist", mArtistId);
        super.onSaveInstanceState(outcicle);
    }

    @Override
    public void onDestroy() {
        if (mAlbumList != null) {
            mLastListPosCourse = mAlbumList.getFirstVisiblePosition();
            View cv = mAlbumList.getChildAt(0);
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
        mAlbumList.setAdapter(null);
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
    }

    private BroadcastReceiver mTrackListListener = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            mAlbumList.invalidateViews();
            MusicUtils.updateNowPlaying(AlbumBrowserActivity.this, false);
        }
    };
    private BroadcastReceiver mScanListener = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            MusicUtils.setSpinnerState(AlbumBrowserActivity.this);
            mReScanHandler.sendEmptyMessage(0);
            if (intent.getAction().equals(Intent.ACTION_MEDIA_UNMOUNTED)) {
                MusicUtils.clearAlbumArtCache();
            }
        }
    };

    private Handler mReScanHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            if (mAdapter != null) {
                getAlbumCursor(mAdapter.getQueryHandler(), null);
            }
        }
    };

    @Override
    public void onPause() {
        unregisterReceiver(mTrackListListener);
        mReScanHandler.removeCallbacksAndMessages(null);
        super.onPause();
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (event.getAction() == KeyEvent.ACTION_UP
                && event.getKeyCode() == KeyEvent.KEYCODE_BACK) {
            finish();
            return true;
        }

        return super.dispatchKeyEvent(event);
    }

    public void init(Cursor c) {

        if (mAdapter == null) {
            return;
        }
        mAdapter.changeCursor(c); // also sets mAlbumCursor

        if (mAlbumCursor == null) {
            MusicUtils.displayDatabaseError(this);
            closeContextMenu();
            mReScanHandler.sendEmptyMessageDelayed(0, 1000);
            return;
        }

        // restore previous position
        if (mLastListPosCourse >= 0) {
            mLastListPosCourse = -1;
        }

        MusicUtils.hideDatabaseError(this);
        setTitle();
    }

    private void setTitle() {
        CharSequence fancyName = "";
        if (mAlbumCursor != null && mAlbumCursor.getCount() > 0) {
            mAlbumCursor.moveToFirst();
            fancyName = mAlbumCursor.getString(mAlbumCursor
                    .getColumnIndex(MediaStore.Audio.Albums.ARTIST));
            if (fancyName == null
                    || fancyName.equals(MediaStore.UNKNOWN_STRING))
                fancyName = getText(R.string.unknown_artist_name);
        }

        if (mArtistId != null && fancyName != null)
            setTitle(fancyName);
        else
            setTitle(R.string.albums_title);
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View view,
            ContextMenuInfo menuInfoIn) {
        menu.add(0, PLAY_SELECTION, 0, R.string.play_selection);
        SubMenu sub = menu.addSubMenu(0, ADD_TO_PLAYLIST, 0,
                R.string.add_to_playlist);
        MusicUtils.makePlaylistMenu(this, sub);
        menu.add(0, DELETE_ITEM, 0, R.string.delete_item);

        AdapterContextMenuInfo mi = (AdapterContextMenuInfo) menuInfoIn;
        mAlbumCursor.moveToPosition(mi.position);
        mCurrentAlbumId = mAlbumCursor.getString(mAlbumCursor
                .getColumnIndexOrThrow(MediaStore.Audio.Albums._ID));
        mCurrentAlbumName = mAlbumCursor.getString(mAlbumCursor
                .getColumnIndexOrThrow(MediaStore.Audio.Albums.ALBUM));
        mCurrentArtistNameForAlbum = mAlbumCursor.getString(mAlbumCursor
                .getColumnIndexOrThrow(MediaStore.Audio.Albums.ARTIST));
        mIsUnknownArtist = mCurrentArtistNameForAlbum == null
                || mCurrentArtistNameForAlbum.equals(MediaStore.UNKNOWN_STRING);
        mIsUnknownAlbum = mCurrentAlbumName == null
                || mCurrentAlbumName.equals(MediaStore.UNKNOWN_STRING);
        if (mIsUnknownAlbum) {
            menu.setHeaderTitle(getString(R.string.unknown_album_name));
        } else {
            menu.setHeaderTitle(mCurrentAlbumName);
        }
        if (!mIsUnknownAlbum || !mIsUnknownArtist) {
            menu.add(0, SEARCH, 0, R.string.search_title);
        }
    }

    @Override
    public boolean onContextItemSelected(MenuItem item) {
        switch (item.getItemId()) {
        case PLAY_SELECTION: {
            // play the selected album
            long[] list = MusicUtils.getSongListForAlbum(this,
                    Long.parseLong(mCurrentAlbumId));
            MusicUtils.playAll(this, list, 0);
            return true;
        }

        case QUEUE: {
            long[] list = MusicUtils.getSongListForAlbum(this,
                    Long.parseLong(mCurrentAlbumId));
            MusicUtils.addToCurrentPlaylist(this, list);
            MusicUtils.addToPlaylist(this, list, MusicUtils.getPlayListId());
            return true;
        }

        case NEW_PLAYLIST: {
            Intent intent = new Intent();
            intent.setClass(this, CreatePlaylist.class);
            startActivityForResult(intent, NEW_PLAYLIST);
            return true;
        }

        case PLAYLIST_SELECTED: {
            long[] list = MusicUtils.getSongListForAlbum(this,
                    Long.parseLong(mCurrentAlbumId));
            long playlist = item.getIntent().getLongExtra("playlist", 0);
            MusicUtils.addToPlaylist(this, list, playlist);
            return true;
        }
        case DELETE_ITEM: {
            long[] list = MusicUtils.getSongListForAlbum(this,
                    Long.parseLong(mCurrentAlbumId));
            String f = getString(R.string.delete_album_desc);
            String desc = String.format(f, mCurrentAlbumName);
            Bundle b = new Bundle();
            b.putString("description", desc);
            b.putLongArray("items", list);
            Intent intent = new Intent();
            intent.setClass(this, DeleteItems.class);
            intent.putExtras(b);
            startActivityForResult(intent, -1);
            return true;
        }
        case SEARCH:
            doSearch();
            return true;

        }
        return super.onContextItemSelected(item);
    }

    void doSearch() {
        CharSequence title = null;
        String query = "";
        Intent i = new Intent();
        i.setAction(MediaStore.INTENT_ACTION_MEDIA_SEARCH);
        i.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        title = "";
        if (!mIsUnknownAlbum) {
            query = mCurrentAlbumName;
            i.putExtra(MediaStore.EXTRA_MEDIA_ALBUM, mCurrentAlbumName);
            title = mCurrentAlbumName;
        }
        if (!mIsUnknownArtist) {
            query = query + " " + mCurrentArtistNameForAlbum;
            i.putExtra(MediaStore.EXTRA_MEDIA_ARTIST,
                    mCurrentArtistNameForAlbum);
            title = title + " " + mCurrentArtistNameForAlbum;
        }
        // Since we hide the 'search' menu item when both album and artist are
        // unknown, the query and title strings will have at least one of those.
        i.putExtra(MediaStore.EXTRA_MEDIA_FOCUS,
                MediaStore.Audio.Albums.ENTRY_CONTENT_TYPE);
        title = getString(R.string.mediasearch, title);
        i.putExtra(SearchManager.QUERY, query);

        startActivity(Intent.createChooser(i, title));
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode,
            Intent intent) {
        switch (requestCode) {
        case SCAN_DONE:
            if (resultCode == RESULT_CANCELED) {
                finish();
            } else {
                getAlbumCursor(mAdapter.getQueryHandler(), null);
            }
            break;

        case NEW_PLAYLIST:
            if (resultCode == RESULT_OK) {
                Uri uri = intent.getData();
                if (uri != null) {
                    long[] list = MusicUtils.getSongListForAlbum(this,
                            Long.parseLong(mCurrentAlbumId));
                    MusicUtils.addToPlaylist(this, list,
                            Long.parseLong(uri.getLastPathSegment()));
                }
            }
            break;
        }
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        super.onCreateOptionsMenu(menu);
        // icon will be set in onPrepareOptionsMenu()
        menu.add(0, PARTY_SHUFFLE, 0, R.string.party_shuffle);
        menu.add(0, SHUFFLE_ALL, 0, R.string.shuffle_all).setIcon(
                R.drawable.ic_menu_shuffle);
        menu.add(0, CLOSE, 0, R.string.close_music).setIcon(
                R.drawable.quick_panel_music_close);
        if (getResources().getBoolean(R.bool.def_music_add_more_video_enabled))
            menu.add(0, MORE_MUSIC, 0, R.string.more_music).setIcon(
                    R.drawable.ic_menu_music_library);
        return true;
    }

    @Override
    public boolean onPrepareOptionsMenu(Menu menu) {
        MusicUtils.setPartyShuffleMenuIcon(menu);
        return super.onPrepareOptionsMenu(menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        Intent intent;
        Cursor cursor;
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

        case SHUFFLE_ALL:
            cursor = MusicUtils.query(this,
                    MediaStore.Audio.Media.EXTERNAL_CONTENT_URI,
                    new String[] { MediaStore.Audio.Media._ID },
                    MediaStore.Audio.Media.IS_MUSIC + "=1", null,
                    MediaStore.Audio.Media.DEFAULT_SORT_ORDER);
            if (cursor != null) {
                MusicUtils.shuffleAll(this, cursor);
                cursor.close();
            }
            return true;

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

    private Cursor getAlbumCursor(AsyncQueryHandler async, String filter) {
        String[] cols = new String[] { MediaStore.Audio.Albums._ID,
                MediaStore.Audio.Albums.ARTIST, MediaStore.Audio.Albums.ALBUM,
                MediaStore.Audio.Albums.ALBUM_ART };

        Cursor ret = null;
        if (mArtistId != null) {
            Uri uri = MediaStore.Audio.Artists.Albums.getContentUri("external",
                    Long.valueOf(mArtistId));
            if (!TextUtils.isEmpty(filter)) {
                uri = uri.buildUpon()
                        .appendQueryParameter("filter", Uri.encode(filter))
                        .build();
            }
            if (async != null) {
                async.startQuery(0, null, uri, cols, null, null,
                        MediaStore.Audio.Albums.DEFAULT_SORT_ORDER);
            } else {
                ret = MusicUtils.query(this, uri, cols, null, null,
                        MediaStore.Audio.Albums.DEFAULT_SORT_ORDER);
            }
        } else {
            Uri uri = MediaStore.Audio.Albums.EXTERNAL_CONTENT_URI;
            if (!TextUtils.isEmpty(filter)) {
                uri = uri.buildUpon()
                        .appendQueryParameter("filter", Uri.encode(filter))
                        .build();
            }
            if (async != null) {
                async.startQuery(0, null, uri, cols, null, null,
                        MediaStore.Audio.Albums.DEFAULT_SORT_ORDER);
            } else {
                ret = MusicUtils.query(this, uri, cols, null, null,
                        MediaStore.Audio.Albums.DEFAULT_SORT_ORDER);
            }
        }
        return ret;
    }

    static class AlbumListAdapter extends SimpleCursorAdapter implements
            SectionIndexer {

        private Drawable mNowPlayingOverlay;
        private final BitmapDrawable mDefaultAlbumIcon;
        private int mAlbumIdx;
        private int mArtistIdx;
        private int mAlbumArtIndex;
        private final Resources mResources;
        private final StringBuilder mStringBuilder = new StringBuilder();
        private final String mUnknownAlbum;
        private final String mUnknownArtist;
        private final String mAlbumSongSeparator;
        private final Object[] mFormatArgs = new Object[1];
        private AlphabetIndexer mIndexer;
        private AlbumBrowserActivity mActivity;
        private AsyncQueryHandler mQueryHandler;
        private String mConstraint = null;
        private boolean mConstraintIsValid = false;

        static class ViewHolder {
            TextView line1;
            TextView line2;
            ImageView popup_menu_button;
            ImageView icon;
            String albumID;
            String albumName;
            String artistNameForAlbum;
        }

        class QueryHandler extends AsyncQueryHandler {
            QueryHandler(ContentResolver res) {
                super(res);
            }

            @Override
            protected void onQueryComplete(int token, Object cookie,
                    Cursor cursor) {
                mActivity.init(cursor);
            }
        }

        AlbumListAdapter(Context context, AlbumBrowserActivity currentactivity,
                int layout, Cursor cursor, String[] from, int[] to) {
            super(context, layout, cursor, from, to);

            mActivity = currentactivity;
            mQueryHandler = new QueryHandler(context.getContentResolver());

            mUnknownAlbum = context.getString(R.string.unknown_album_name);
            mUnknownArtist = context.getString(R.string.unknown_artist_name);
            mAlbumSongSeparator = context
                    .getString(R.string.albumsongseparator);

            Resources r = context.getResources();
            mNowPlayingOverlay = r
                    .getDrawable(R.drawable.indicator_ic_mp_playing_list);

            Bitmap b = BitmapFactory.decodeResource(r, R.drawable.album_cover);
            mDefaultAlbumIcon = new BitmapDrawable(context.getResources(), b);
            // no filter or dither, it's a lot faster and we can't tell the
            // difference
            mDefaultAlbumIcon.setFilterBitmap(false);
            mDefaultAlbumIcon.setDither(false);
            getColumnIndices(cursor);
            mResources = context.getResources();
        }

        private void getColumnIndices(Cursor cursor) {
            if (cursor != null) {
                mAlbumIdx = cursor
                        .getColumnIndexOrThrow(MediaStore.Audio.Albums.ALBUM);
                mArtistIdx = cursor
                        .getColumnIndexOrThrow(MediaStore.Audio.Albums.ARTIST);
                mAlbumArtIndex = cursor
                        .getColumnIndexOrThrow(MediaStore.Audio.Albums.ALBUM_ART);

                if (mIndexer != null) {
                    mIndexer.setCursor(cursor);
                } else {
                    mIndexer = new MusicAlphabetIndexer(cursor, mAlbumIdx,
                            mResources.getString(R.string.fast_scroll_alphabet));
                }
            }
        }

        public void setActivity(AlbumBrowserActivity newactivity) {
            mActivity = newactivity;
        }

        public AsyncQueryHandler getQueryHandler() {
            return mQueryHandler;
        }

        @Override
        public View newView(Context context, Cursor cursor, ViewGroup parent) {
            View v = super.newView(context, cursor, parent);
            ViewHolder vh = new ViewHolder();
            vh.line1 = (TextView) v.findViewById(R.id.line1);
            vh.line2 = (TextView) v.findViewById(R.id.line2);
            vh.popup_menu_button = (ImageView) v
                    .findViewById(R.id.list_menu_button);
            vh.icon = (ImageView) v.findViewById(R.id.icon);
            vh.icon.setBackgroundDrawable(mDefaultAlbumIcon);
            vh.icon.setPadding(0, 0, 1, 0);
            v.setTag(vh);
            return v;
        }

        @Override
        public void bindView(View view, Context context, final Cursor cursor) {

            final ViewHolder vh = (ViewHolder) view.getTag();

            vh.albumID = cursor.getString(cursor
                    .getColumnIndexOrThrow(MediaStore.Audio.Albums._ID));
            String name = cursor.getString(mAlbumIdx);
            String displayname = name;
            vh.albumName = name;
            boolean unknown = name == null
                    || name.equals(MediaStore.UNKNOWN_STRING);
            if (unknown) {
                displayname = mUnknownAlbum;
            }
            vh.line1.setText(displayname);

            name = cursor.getString(mArtistIdx);
            vh.artistNameForAlbum = name;
            displayname = name;
            if (name == null || name.equals(MediaStore.UNKNOWN_STRING)) {
                displayname = mUnknownArtist;
            }
            vh.line2.setText(displayname);

            ImageView iv = vh.icon;
            // We don't actually need the path to the thumbnail file,
            // we just use it to see if there is album art or not
            String art = cursor.getString(mAlbumArtIndex);
            long aid = cursor.getLong(0);
            if (unknown || art == null || art.length() == 0) {
                iv.setImageDrawable(null);
            } else {
                iv.setImageBitmap(BitmapFactory.decodeFile(art));
            }

            vh.popup_menu_button.setOnClickListener(new OnClickListener() {

                @Override
                public void onClick(View v) {
                    PopupMenu popup = new PopupMenu(mActivity,
                            vh.popup_menu_button);
                    popup.getMenu().add(0, PLAY_SELECTION, 0,
                            R.string.play_selection);
                    SubMenu sub = popup.getMenu().addSubMenu(0,
                            ADD_TO_PLAYLIST, 0, R.string.add_to_playlist);
                    MusicUtils.makePlaylistMenu(mActivity, sub);
                    popup.getMenu()
                            .add(0, DELETE_ITEM, 0, R.string.delete_item);
                    popup.show();
                    popup.setOnMenuItemClickListener(new OnMenuItemClickListener() {
                        @Override
                        public boolean onMenuItemClick(MenuItem item) {
                            // TODO Auto-generated method stub
                            mActivity.onContextItemSelected(item);
                            return true;
                        }
                    });
                    mActivity.mCurrentAlbumId = vh.albumID;
                    mActivity.mCurrentAlbumName = vh.albumName;
                    mActivity.mCurrentArtistNameForAlbum = vh.artistNameForAlbum;
                    mActivity.mIsUnknownArtist = mActivity.mCurrentArtistNameForAlbum == null
                            || mActivity.mCurrentArtistNameForAlbum
                                    .equals(MediaStore.UNKNOWN_STRING);
                    mActivity.mIsUnknownAlbum = mActivity.mCurrentAlbumName == null
                            || mActivity.mCurrentAlbumName
                                    .equals(MediaStore.UNKNOWN_STRING);
                    if (!mActivity.mIsUnknownAlbum
                            || !mActivity.mIsUnknownArtist) {
                        popup.getMenu()
                                .add(0, SEARCH, 0, R.string.search_title);
                    }
                }
            });
        }

        @Override
        public void changeCursor(Cursor cursor) {
            if (mActivity.isFinishing() && cursor != null) {
                cursor.close();
                cursor = null;
            }
            if (cursor != mActivity.mAlbumCursor) {
                if (mActivity.mAlbumCursor != null) {
                    mActivity.mAlbumCursor.close();
                    mActivity.mAlbumCursor = null;
                }
                mActivity.mAlbumCursor = cursor;
                getColumnIndices(cursor);
                super.changeCursor(cursor);
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
            Cursor c = mActivity.getAlbumCursor(null, s);
            mConstraint = s;
            mConstraintIsValid = true;
            return c;
        }

        public Object[] getSections() {
            return mIndexer.getSections();
        }

        public int getPositionForSection(int section) {
            return mIndexer.getPositionForSection(section);
        }

        public int getSectionForPosition(int position) {
            return 0;
        }
    }

    private Cursor mAlbumCursor;
    private String mArtistId;

    public void onServiceConnected(ComponentName name, IBinder service) {
        MusicUtils.updateNowPlaying(this, false);
    }

    public void onServiceDisconnected(ComponentName name) {
        finish();
    }
}
