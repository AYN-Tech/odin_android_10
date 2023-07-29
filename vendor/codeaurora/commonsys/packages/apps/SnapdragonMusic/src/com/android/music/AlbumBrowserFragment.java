/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
 * Not a Contribution.
 *
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

import com.android.music.MusicUtils.Defs;
import com.android.music.MusicUtils.ServiceToken;
import com.codeaurora.music.custom.FragmentsFactory;

import android.app.Activity;
import android.app.Fragment;
import android.app.SearchManager;
import android.content.AsyncQueryHandler;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.ServiceConnection;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.provider.MediaStore;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.SubMenu;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.AbsListView;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.AlphabetIndexer;
import android.widget.FrameLayout;
import android.widget.GridView;
import android.widget.ImageView;
import android.widget.PopupMenu;
import android.widget.PopupMenu.OnMenuItemClickListener;
import android.widget.SectionIndexer;
import android.widget.SimpleCursorAdapter;
import android.widget.TextView;

public class AlbumBrowserFragment extends Fragment implements MusicUtils.Defs,
        ServiceConnection {
    private String mCurrentAlbumId;
    private String mCurrentAlbumName;
    private String mCurrentArtistNameForAlbum;
    boolean mIsUnknownArtist;
    boolean mIsUnknownAlbum;
    private AlbumListAdapter mAdapter;
    private boolean mAdapterSent;
    private final static int SEARCH = CHILD_MENU_BASE;
    private static int mLastListPosCourse = -1;
    @SuppressWarnings("unused")
    private static int mLastListPosFine = -1;
    private ServiceToken mToken;
    private GridView mAlbumList;
    private TextView mSdErrorMessageView;
    private View mSdErrorMessageIcon;
    private static int mLastSelectedPosition = -1;
    private MediaPlaybackActivity mParentActivity;
    public PopupMenu mPopupMenu;
    private static SubMenu mSub = null;

    public AlbumBrowserFragment() {
    }

    public Activity getParentActivity() {
        return mParentActivity;
    }

    @Override
    public void onAttach(Activity activity) {
        // TODO Auto-generated method stub
        super.onAttach(activity);
        mParentActivity = (MediaPlaybackActivity) activity;
    }

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        mToken = MusicUtils.bindToService(mParentActivity, this);
        IntentFilter f = new IntentFilter();
        f.addAction(Intent.ACTION_MEDIA_SCANNER_STARTED);
        f.addAction(Intent.ACTION_MEDIA_SCANNER_FINISHED);
        f.addAction(Intent.ACTION_MEDIA_UNMOUNTED);
        f.addDataScheme("file");
        mParentActivity.registerReceiver(mScanListener, f);
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {

        View rootView = inflater.inflate(R.layout.media_picker_fragment_album,
                container, false);
        mSdErrorMessageView = (TextView) rootView.findViewById(R.id.sd_message);
        mSdErrorMessageIcon = rootView.findViewById(R.id.sd_icon);
        if (getArguments() != null) {
            mArtistId = getArguments().getString("artist");
        }
        mAlbumList = (GridView) rootView.findViewById(R.id.album_list);
        arrangeGridColums(mParentActivity.getResources().getConfiguration());
        mAlbumList.setOnItemClickListener(new OnItemClickListener() {
            public void onItemClick(AdapterView<?> parent, View v,
                    int position, long id) {
               enterAlbum(id);
            }
        });
        mAlbumList.setTextFilterEnabled(true);
        mAdapter = new AlbumListAdapter(mParentActivity.getApplication(), this,
                R.layout.track_list_item_album, mAlbumCursor, new String[] {},
                new int[] {});
        mAlbumList.setAdapter(mAdapter);
        mAlbumList.setOnScrollListener(new AbsListView.OnScrollListener() {
            @Override
            public void onScrollStateChanged(AbsListView view, int scrollState) {
                if (scrollState == SCROLL_STATE_FLING) {
                    mAdapter.mFastscroll = true;
                } else {
                    mAdapter.mFastscroll = false;
                    mAdapter.notifyDataSetChanged();
                }
            }

            @Override
            public void onScroll(AbsListView view, int firstVisibleItem,
                             int visibleItemCount, int totalItemCount) {
            }
        });
        getAlbumCursor(mAdapter.getQueryHandler(), null);
        return rootView;
    }

    private void enterAlbum(long albumID) {
        mParentActivity.findViewById(R.id.music_tool_bar)
           .setVisibility(View.GONE);
        Fragment fragment = new TrackBrowserActivityFragment();
        mLastSelectedPosition = mAlbumList.getSelectedItemPosition();
        Bundle args = new Bundle();
        args.putString("artist", mArtistId);
        args.putString("album", Long.valueOf(albumID).toString());
        fragment.setArguments(args);

        if (!mParentActivity.isFinishing() && !mParentActivity.isDestroyed()) {
            getFragmentManager()
                    .beginTransaction()
                    .replace(R.id.fragment_page, fragment, "track_fragment")
                    .commit();
        }

        MusicUtils.navigatingTabPosition = 1;
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
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        if (mPopupMenu != null ) {
            mPopupMenu.dismiss();
        }
        if (mSub != null) {
            mSub.close();
        }
        arrangeGridColums(newConfig);
        Fragment fragment = new AlbumBrowserFragment();
        Bundle args = getArguments();
        fragment.setArguments(args);
        getFragmentManager().beginTransaction()
                .replace(R.id.fragment_page, fragment, "album_fragment")
                .commitAllowingStateLoss();
    }

    public void arrangeGridColums(Configuration newConfig) {
        if (newConfig.orientation == Configuration.ORIENTATION_LANDSCAPE) {
            mAlbumList.setNumColumns(3);
        } else {
            mAlbumList.setNumColumns(2);
        }
    }

    @Override
    public void onDestroy() {
        if (mPopupMenu != null ) {
            mPopupMenu.dismiss();
        }
        if (mSub != null) {
            mSub.close();
        }
        if (mAlbumList != null) {
            mLastListPosCourse = mAlbumList.getFirstVisiblePosition();
            View cv = mAlbumList.getChildAt(0);
            if (cv != null) {
                mLastListPosFine = cv.getTop();
            }
        }
        MusicUtils.unbindFromService(mToken);
        // If we have an adapter and didn't send it off to another activity yet,
        // we should
        // close its cursor, which we do by assigning a null cursor to it. Doing
        // this
        // instead of closing the cursor directly keeps the framework from
        // accessing
        // the closed cursor later.
        if (!mAdapterSent && mAdapter != null) {
            mAdapter.changeCursor(null);
        }
        // Because we pass the adapter to the next activity, we need to make
        // sure it doesn't keep a reference to this activity. We can do this
        // by clearing its DatasetObservers, which setListAdapter(null) does.
        mAlbumList.setAdapter(null);
        mAdapter = null;
        mParentActivity.unregisterReceiver(mScanListener);
        super.onDestroy();
    }

    @Override
    public void onResume() {
        super.onResume();
        MusicUtils.setSpinnerState(mParentActivity);
    }

    private BroadcastReceiver mScanListener = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            MusicUtils.setSpinnerState(mParentActivity);
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
        mReScanHandler.removeCallbacksAndMessages(null);
        super.onPause();
    }

    public void init(Cursor c) {

        if (mAdapter == null) {
            return;
        }
        mAdapter.changeCursor(c); // also sets mAlbumCursor

        if (mAlbumCursor == null) {
            displayDatabaseError();
            mReScanHandler.sendEmptyMessageDelayed(0, 1000);
            return;
        }

        // restore previous position
        if (mLastListPosCourse >= 0) {
            mAlbumList.setSelection(mLastSelectedPosition);
            mLastListPosCourse = -1;
        }

        hideDatabaseError();
        if (mAlbumCursor.getCount() == 0) {
            mSdErrorMessageView.setVisibility(View.VISIBLE);
            mSdErrorMessageView.setText(R.string.no_music_found);
            mAlbumList.setVisibility(View.GONE);
        }
    }

    @Override
    public boolean onContextItemSelected(MenuItem item) {
        switch (item.getItemId()) {
        case PLAY_SELECTION: {
            // play the selected album
            long[] list = MusicUtils.getSongListForAlbum(mParentActivity,
                    Long.parseLong(mCurrentAlbumId));
            MusicUtils.playAll(mParentActivity, list, 0);
            return true;
        }

        case QUEUE: {
            long[] list = MusicUtils.getSongListForAlbum(mParentActivity,
                    Long.parseLong(mCurrentAlbumId));
            MusicUtils.addToCurrentPlaylist(mParentActivity, list);
            MusicUtils.addToPlaylist(mParentActivity, list,
                    MusicUtils.getPlayListId());
            return true;
        }

        case NEW_PLAYLIST: {
            Intent intent = new Intent();
            intent.setClass(mParentActivity, CreatePlaylist.class);
            startActivityForResult(intent, NEW_PLAYLIST);
            return true;
        }

        case PLAYLIST_SELECTED: {
            long[] list = MusicUtils.getSongListForAlbum(mParentActivity,
                    Long.parseLong(mCurrentAlbumId));
            long playlist = item.getIntent().getLongExtra("playlist", 0);
            MusicUtils.addToPlaylist(mParentActivity, list, playlist);
            return true;
        }

        case DELETE_ITEM: {
            long[] list = MusicUtils.getSongListForAlbum(mParentActivity,
                    Long.parseLong(mCurrentAlbumId));
            String f = getString(R.string.delete_album_desc);
            String desc = String.format(f, mCurrentAlbumName);
            Bundle b = new Bundle();
            b.putString("description", desc);
            b.putLongArray("items", list);
            Intent intent = new Intent();
            intent.setClass(mParentActivity, DeleteItems.class);
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

    @SuppressWarnings("static-access")
    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent intent) {
        switch (requestCode) {
        case SCAN_DONE:
            if (resultCode == mParentActivity.RESULT_CANCELED) {
                mParentActivity.finish();
            } else {
                getAlbumCursor(mAdapter.getQueryHandler(), null);
            }
            break;

        case NEW_PLAYLIST:
            if (resultCode == mParentActivity.RESULT_OK) {
                Uri uri = intent.getData();
                if (uri != null) {
                    long[] list = MusicUtils.getSongListForAlbum(
                            mParentActivity, Long.parseLong(mCurrentAlbumId));
                    MusicUtils.addToPlaylist(mParentActivity, list,
                            Long.parseLong(uri.getLastPathSegment()));
                }
            }
            break;
        }
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
                ret = MusicUtils.query(mParentActivity, uri, cols, null, null,
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
                ret = MusicUtils.query(mParentActivity, uri, cols, null, null,
                        MediaStore.Audio.Albums.DEFAULT_SORT_ORDER);
            }
        }
        return ret;
    }

    @SuppressWarnings("unused")
    public void displayDatabaseError() {

        String status = Environment.getExternalStorageState();
        int title, message;

        if (android.os.Environment.isExternalStorageRemovable()) {
            title = R.string.sdcard_error_title;
            message = R.string.sdcard_error_message;
        } else {
            title = R.string.sdcard_error_title_nosdcard;
            message = R.string.sdcard_error_message_nosdcard;
        }

        if (status.equals(Environment.MEDIA_SHARED)
                || status.equals(Environment.MEDIA_UNMOUNTED)) {
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
        } else if (status.equals(Environment.MEDIA_MOUNTED)) {
            // The card is mounted, but we didn't get a valid cursor.
            // This probably means the mediascanner hasn't started scanning the
            // card yet (there is a small window of time during boot where this
            // will happen).
            Intent intent = new Intent();
            intent.setClass(mParentActivity, ScanningProgress.class);
            mParentActivity.startActivityForResult(intent, Defs.SCAN_DONE);
        }
        if (mSdErrorMessageView != null) {
            mSdErrorMessageView.setVisibility(View.VISIBLE);
        }
        if (mSdErrorMessageIcon != null) {
            mSdErrorMessageIcon.setVisibility(View.VISIBLE);
        }
        if (mAlbumList != null) {
            mAlbumList.setVisibility(View.GONE);
        }
        mSdErrorMessageView.setText(message);
    }

    public void hideDatabaseError() {
        if (mSdErrorMessageView != null) {
            mSdErrorMessageView.setVisibility(View.GONE);
        }
        if (mSdErrorMessageIcon != null) {
            mSdErrorMessageIcon.setVisibility(View.GONE);
        }
        if (mAlbumList != null) {
            mAlbumList.setVisibility(View.VISIBLE);
        }
    }

    static class AlbumListAdapter extends SimpleCursorAdapter implements
            SectionIndexer {

        @SuppressWarnings("unused")
        private Drawable mNowPlayingOverlay;
        private final BitmapDrawable mDefaultAlbumIcon;
        private int mAlbumIdx;
        private int mArtistIdx;
        private int mAlbumArtIndex;
        private final Resources mResources;
        @SuppressWarnings("unused")
        private final StringBuilder mStringBuilder = new StringBuilder();
        private final String mUnknownAlbum;
        private final String mUnknownArtist;
        @SuppressWarnings("unused")
        private final String mAlbumSongSeparator;
        @SuppressWarnings("unused")
        private final Object[] mFormatArgs = new Object[1];
        private AlphabetIndexer mIndexer;
        private AlbumBrowserFragment mFragment;
        private AsyncQueryHandler mQueryHandler;
        private String mConstraint = null;
        private boolean mConstraintIsValid = false;
        private boolean mFastscroll = false;

        static class ViewHolder {
            TextView line1;
            TextView line2;
            ImageView popup_menu_button;
            ImageView icon;
            String albumID;
            String albumName;
            String artistNameForAlbum, mCurrentAlbumName;
        }

        class QueryHandler extends AsyncQueryHandler {
            QueryHandler(ContentResolver res) {
                super(res);
            }

            @Override
            protected void onQueryComplete(int token, Object cookie,
                    Cursor cursor) {
                mFragment.init(cursor);
            }
        }

        @SuppressWarnings("deprecation")
        AlbumListAdapter(Context context, AlbumBrowserFragment fragment,
                int layout, Cursor cursor, String[] from, int[] to) {
            super(context, layout, cursor, from, to);
            mQueryHandler = new QueryHandler(context.getContentResolver());
            mFragment = fragment;
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
                            mFragment.getParentActivity().getResources()
                                    .getString(R.string.fast_scroll_alphabet));
                }
            }
        }

        public void setFragment(AlbumBrowserFragment newFragment) {
            mFragment = newFragment;
        }

        public AsyncQueryHandler getQueryHandler() {
            return mQueryHandler;
        }

        @SuppressWarnings("deprecation")
        @Override
        public View newView(Context context, Cursor cursor, ViewGroup parent) {
            View v = super.newView(context, cursor, parent);
            ViewHolder vh = new ViewHolder();
            vh.line1 = (TextView) v.findViewById(R.id.line1);
            vh.line2 = (TextView) v.findViewById(R.id.line2);
            vh.popup_menu_button = (ImageView) v
                    .findViewById(R.id.list_menu_button);
            ((MediaPlaybackActivity) mFragment.getParentActivity())
                    .setTouchDelegate(vh.popup_menu_button);
            vh.icon = (ImageView) v.findViewById(R.id.icon);
            vh.icon.setBackgroundDrawable(mDefaultAlbumIcon);
            vh.icon.setPadding(0, 0, 1, 0);
            v.setTag(vh);
            return v;
        }

        class BitmapDownload extends AsyncTask {
            ImageView img = null;
            String art = null;
            String name = null;
            Context context;

            public BitmapDownload(Context context, ImageView img, String art, String name) {
                this.img = img;
                this.art = art;
                this.name = name;
                this.context = context;
            }

            @Override
            protected Object doInBackground(Object... params) {
                Bitmap albumArt[] = new Bitmap[1];
                if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.P){
                    int albumIdx = Integer.valueOf(art);
                    albumArt[0] = MusicUtils.getArtwork(context, -1, albumIdx, false);
                } else {
                    albumArt[0] = BitmapFactory.decodeFile(art);
                }
                MusicUtils.mAlbumArtCache.put(art, albumArt);
                return albumArt[0];
            }

            @Override
            protected void onPostExecute(Object result) {
                super.onPostExecute(result);
                if (img != null && art.equals(img.getTag())) {
                    if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.P){
                        if ((result != null) && !(((Bitmap) result).isRecycled())) {
                            img.setImageBitmap((Bitmap) result);
                        } else {
                            img.setImageDrawable(null);
                        }
                    } else {
                        img.setImageBitmap((Bitmap) result);
                    }
                }
            }
        }

        @Override
        public void bindView(View view, Context context, final Cursor cursor) {
            final ViewHolder vh = (ViewHolder) view.getTag();
            vh.albumID = cursor.getString(cursor
                    .getColumnIndexOrThrow(MediaStore.Audio.Albums._ID));
            String name = cursor.getString(mAlbumIdx);
            String displayname = name;
            vh.mCurrentAlbumName = displayname;
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
            if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.P){
                art = vh.albumID;
            }
            Bitmap albumArt[];
            if (art != null) {
                albumArt = MusicUtils.mAlbumArtCache.get(art);
            } else {
                albumArt = null;
            }
            if (mFastscroll || unknown || art == null || art.length() == 0) {
                iv.setImageDrawable(null);
                iv.setTag(null);
            } else {
                if (albumArt == null || albumArt[0] == null) {
                    iv.setTag(art);
                    new BitmapDownload(context, iv, art, name).execute();
                } else {
                    iv.setImageBitmap(albumArt[0]);
                }
            }

            view.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View arg0) {
                    mFragment.enterAlbum(Long.valueOf(vh.albumID));
                }
            });

            vh.popup_menu_button.setOnClickListener(new OnClickListener() {

                @Override
                public void onClick(View v) {
                    if (mFragment.mPopupMenu != null ) {
                        mFragment.mPopupMenu.dismiss();
                    }
                    mFragment.mCurrentAlbumName = vh.mCurrentAlbumName;
                    PopupMenu popup = new PopupMenu(mFragment
                            .getParentActivity(), vh.popup_menu_button);
                    popup.getMenu().add(0, PLAY_SELECTION, 0,
                            R.string.play_selection);
                    mSub = popup.getMenu().addSubMenu(0,
                            ADD_TO_PLAYLIST, 0, R.string.add_to_playlist);
                    MusicUtils.makePlaylistMenu(mFragment.getParentActivity(),
                            mSub);
                    popup.getMenu()
                            .add(0, DELETE_ITEM, 0, R.string.delete_item);
                    popup.show();
                    popup.setOnMenuItemClickListener(new OnMenuItemClickListener() {

                        @Override
                        public boolean onMenuItemClick(MenuItem item) {
                            // TODO Auto-generated method stub
                            mFragment.onContextItemSelected(item);
                            return true;
                        }
                    });
                    mFragment.mPopupMenu = popup;
                    mFragment.mCurrentAlbumId = vh.albumID;
                    mFragment.mCurrentAlbumName = vh.albumName;
                    mFragment.mCurrentArtistNameForAlbum = vh.artistNameForAlbum;
                    mFragment.mIsUnknownArtist = mFragment.mCurrentArtistNameForAlbum == null
                            || mFragment.mCurrentArtistNameForAlbum
                                    .equals(MediaStore.UNKNOWN_STRING);
                    mFragment.mIsUnknownAlbum = mFragment.mCurrentAlbumName == null
                            || mFragment.mCurrentAlbumName
                                    .equals(MediaStore.UNKNOWN_STRING);
                }
            });
        }

        @Override
        public void changeCursor(Cursor cursor) {
            if (mFragment.getParentActivity().isFinishing() && cursor != null) {
                cursor.close();
                cursor = null;
            }
            if (cursor != mFragment.mAlbumCursor) {
                if (mFragment.mAlbumCursor != null) {
                    mFragment.mAlbumCursor.close();
                    mFragment.mAlbumCursor = null;
                }
                mFragment.mAlbumCursor = cursor;
                if ((cursor != null && !cursor.isClosed()) || cursor == null) {
                    getColumnIndices(cursor);
                    super.changeCursor(cursor);
                }
            }
        }

        @Override
        public Cursor runQueryOnBackgroundThread(CharSequence constraint) {
            String s = constraint.toString();
            if (mConstraintIsValid
                    && ((s == null && mConstraint == null) || (s != null && s
                            .equals(mConstraint)))) {
                return getCursor();
            }
            Cursor c = mFragment.getAlbumCursor(null, s);
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
        ((MediaPlaybackActivity) mParentActivity)
                .updateNowPlaying(getParentActivity());
    }

    public void onServiceDisconnected(ComponentName name) {
        mParentActivity.finish();
    }
}
