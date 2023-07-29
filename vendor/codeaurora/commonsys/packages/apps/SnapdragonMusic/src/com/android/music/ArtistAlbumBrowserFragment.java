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

import com.android.music.AlbumBrowserFragment.AlbumListAdapter;
import com.android.music.MusicUtils.Defs;
import com.android.music.MusicUtils.ServiceToken;
import com.android.music.QueryBrowserActivity.QueryListAdapter.QueryHandler;
import com.android.music.MusicUtils;

import android.annotation.SuppressLint;
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
import android.database.CursorWrapper;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.media.AudioManager;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.Parcel;
import android.os.Parcelable;
import android.os.RemoteException;
import android.provider.MediaStore;
import android.text.TextUtils;
import android.util.Log;
import android.util.SparseArray;
import android.view.ContextMenu;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.SubMenu;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.ViewTreeObserver.OnScrollChangedListener;
import android.view.Window;
import android.view.ContextMenu.ContextMenuInfo;
import android.widget.AbsListView;
import android.widget.AbsListView.OnScrollListener;
import android.widget.AdapterView;
import android.widget.ExpandableListView;
import android.widget.ExpandableListView.OnGroupClickListener;
import android.widget.FrameLayout;
import android.widget.GridLayout;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.PopupMenu;
import android.widget.RelativeLayout;
import android.widget.PopupMenu.OnMenuItemClickListener;
import android.widget.SearchView;
import android.widget.SectionIndexer;
import android.widget.SimpleCursorTreeAdapter;
import android.widget.TextView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ExpandableListView.OnChildClickListener;
import android.widget.ExpandableListView.ExpandableListContextMenuInfo;
import android.view.KeyEvent;

import com.android.music.SysApplication;
import com.codeaurora.music.custom.FragmentsFactory;

import java.text.Collator;
import java.util.Arrays;
import java.util.HashMap;

public class ArtistAlbumBrowserFragment extends Fragment implements
        View.OnCreateContextMenuListener, MusicUtils.Defs, ServiceConnection,
        OnChildClickListener, OnScrollListener {
    private String mCurrentArtistId;
    private String mCurrentArtistName;
    private String mCurrentAlbumId;
    private String mCurrentAlbumName;
    private String mCurrentArtistNameForAlbum;
    boolean mIsUnknownArtist;
    boolean mIsUnknownAlbum;
    private ArtistAlbumListAdapter mAdapter;
    private boolean mAdapterSent;
    private final static int SEARCH = CHILD_MENU_BASE;
    private static int mLastListPosCourse = -1;
    private static int mLastListPosFine = -1;
    private ServiceToken mToken;
    private MediaPlaybackActivity mActivity;
    private TextView mSdErrorMessageView;
    private View mSdErrorMessageIcon;
    private ExpandableListView mExpandableListView;
    int mExpandPos = -1;
    boolean isExpanded = false;
    private View mPrevView = null;
    private boolean isPaused;
    private int lastVisiblePos = 0;
    private boolean isScrolling = true;
    public PopupMenu mPopupMenu;
    private static SubMenu mSub = null;

    OnGroupClickListener onGroupClickListener = new OnGroupClickListener() {
        @SuppressLint("ResourceAsColor")
        @Override
        public boolean onGroupClick(ExpandableListView parent, View v,
                int groupPosition, long id) {
            // TODO Auto-generated method stub
            if (isExpanded) {
                isExpanded = false;
                mPrevView.setBackground(null);
                mPrevView.setBackgroundResource(R.color.background);
                if (mExpandPos == groupPosition) {
                    mExpandableListView.collapseGroup(mExpandPos);
                    return true;
                }
                mExpandableListView.collapseGroup(mExpandPos);
            }
            if (!isExpanded) {
                mExpandableListView.expandGroup(groupPosition, true);
                isExpanded = true;
                mExpandPos = groupPosition;
                mPrevView = v;
                v.setBackground(null);
                v.setBackgroundResource(R.color.appwidget_text);
            }
            return true;
        }
    };

    public Activity getParentActivity() {
        return mActivity;
    }

    @Override
    public void onAttach(Activity activity) {
        // TODO Auto-generated method stub
        super.onAttach(activity);
        mActivity = (MediaPlaybackActivity) activity;
    }

    /** Called when the activity is first created. */
    @SuppressLint("ResourceAsColor")
    @Override
    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        if (icicle != null) {
            mCurrentAlbumId = icicle.getString("selectedalbum");
            mCurrentAlbumName = icicle.getString("selectedalbumname");
            mCurrentArtistId = icicle.getString("selectedartist");
            mCurrentArtistName = icicle.getString("selectedartistname");
        }
        // The fragment is cached in FragmentsFactory, when it is loaded,
        // it may not be a new instance, so its isExpanded may keep old value
        isExpanded = false;
        mToken = MusicUtils.bindToService(mActivity, this);

    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {

        View rootView = inflater.inflate(
                R.layout.media_picker_activity_expanding, container, false);
        mSdErrorMessageView = (TextView) rootView.findViewById(R.id.sd_message);
        mSdErrorMessageIcon = rootView.findViewById(R.id.sd_icon);
        mExpandableListView = (ExpandableListView) rootView
                .findViewById(R.id.artist_list);
        mExpandableListView.setOnScrollListener(this);
        mExpandableListView.setGroupIndicator(null);
        mExpandableListView.setTextFilterEnabled(true);
        mExpandableListView.setOnChildClickListener(this);
        mExpandableListView.setDividerHeight(0);
        mExpandableListView.setOnGroupClickListener(onGroupClickListener);
        mAdapter = new ArtistAlbumListAdapter(mActivity.getApplication(),
                this,
                null, // cursor
                R.layout.track_list_item_group, new String[] {}, new int[] {},
                R.layout.track_list_item_child, new String[] {}, new int[] {});
        mExpandableListView.setAdapter(mAdapter);
        getArtistCursor(mAdapter.getQueryHandler(), null);
        return rootView;
    }

    @Override
    public void onSaveInstanceState(Bundle outcicle) {
        // need to store the selected item so we don't lose it in case
        // of an orientation switch. Otherwise we could lose it while
        // in the middle of specifying a playlist to add the item to.
        outcicle.putString("selectedalbum", mCurrentAlbumId);
        outcicle.putString("selectedalbumname", mCurrentAlbumName);
        outcicle.putString("selectedartist", mCurrentArtistId);
        outcicle.putString("selectedartistname", mCurrentArtistName);
        super.onSaveInstanceState(outcicle);
    }

    @Override
    public void onDestroy() {
        if (mPopupMenu != null ) {
            mPopupMenu.dismiss();
        }
        if (mSub != null) {
            mSub.close();
        }
        if (mExpandableListView != null) {
            mLastListPosCourse = mExpandableListView.getFirstVisiblePosition();
            View cv = mExpandableListView.getChildAt(0);
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
        mAdapter = null;
        mExpandableListView.setAdapter(mAdapter);
        super.onDestroy();
    }

    @Override
    public void onResume() {
        super.onResume();
        if (isPaused)
            new MusicUtils.BitmapDownloadThread(mActivity, handler,
                    lastVisiblePos, lastVisiblePos + 10).start();

        isPaused = false;

    }

    private BroadcastReceiver mScanListener = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            MusicUtils.setSpinnerState(mActivity);
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
                getArtistCursor(mAdapter.getQueryHandler(), null);
            }
        }
    };

    public void init(Cursor c) {
        if (mAdapter == null) {
            return;
        }
        mAdapter.changeCursor(c); // also sets mArtistCursor
        if (mArtistCursor == null) {
            displayDatabaseError();
            mReScanHandler.sendEmptyMessageDelayed(0, 1000);
            return;
        }
        // restore previous position
        if (mLastListPosCourse >= 0) {
            mExpandableListView.setSelectionFromTop(mLastListPosCourse,
                    mLastListPosFine);
            mLastListPosCourse = -1;
        }

        hideDatabaseError();
        if (mArtistCursor.getCount() == 0) {
            mSdErrorMessageView.setVisibility(View.VISIBLE);
            mSdErrorMessageView.setText(R.string.no_music_found);
            mExpandableListView.setVisibility(View.GONE);
        }
    }

    @Override
    public void onPause() {
        mReScanHandler.removeCallbacksAndMessages(null);
        lastVisiblePos = mExpandableListView.getFirstVisiblePosition();
        super.onStop();
        isPaused = true;
    }

    @Override
    public boolean onChildClick(ExpandableListView parent, View v,
            int groupPosition, int childPosition, long id) {

        String albumId = mCurrentAlbumId;
        MusicUtils.navigatingTabPosition = 0;
        mActivity.findViewById(R.id.music_tool_bar).setVisibility(View.GONE);
        Fragment fragment = null;
        fragment = new TrackBrowserActivityFragment();
        Bundle args = new Bundle();
        Cursor c = null;
        try {
            c = (Cursor) mExpandableListView.getExpandableListAdapter()
                    .getChild(groupPosition, childPosition);
            String album = c.getString(c
                    .getColumnIndex(MediaStore.Audio.Albums.ALBUM));
            albumId = c.getString(c
                    .getColumnIndex(MediaStore.Audio.Albums.ALBUM_ID));
        } catch (Exception e) {
        } finally {
            if (c != null) {
                c.close();
            }
        }
        mArtistCursor.moveToPosition(groupPosition);
        mCurrentArtistId = mArtistCursor.getString(mArtistCursor
                .getColumnIndex(MediaStore.Audio.Artists._ID));
        args.putString("artist", mCurrentArtistId);
        args.putString("album", albumId);
        fragment.setArguments(args);
        getFragmentManager().beginTransaction()
                .replace(R.id.fragment_page, fragment, "track_fragment")
                .commit();
        return true;
    }

    public boolean onContextItemSelected(MenuItem item) {
        switch (item.getItemId()) {
        case PLAY_SELECTION: {
            // play everything by the selected artist
            long[] list = mCurrentArtistId != null ? MusicUtils
                    .getSongListForArtist(mActivity,
                            Long.parseLong(mCurrentArtistId)) : MusicUtils
                    .getSongListForAlbum(mActivity,
                            Long.parseLong(mCurrentAlbumId));

            MusicUtils.playAll(mActivity, list, 0);
            return true;
        }

        case QUEUE: {
            long[] list = mCurrentArtistId != null ? MusicUtils
                    .getSongListForArtist(mActivity,
                            Long.parseLong(mCurrentArtistId)) : MusicUtils
                    .getSongListForAlbum(mActivity,
                            Long.parseLong(mCurrentAlbumId));
            MusicUtils.addToCurrentPlaylist(mActivity, list);
            MusicUtils.addToPlaylist(mActivity, list,
                    MusicUtils.getPlayListId());
            return true;
        }

        case NEW_PLAYLIST: {
            Intent intent = new Intent();
            intent.setClass(mActivity, CreatePlaylist.class);
            startActivityForResult(intent, NEW_PLAYLIST);
            return true;
        }

        case PLAYLIST_SELECTED: {
            long[] list = mCurrentArtistId != null ? MusicUtils
                    .getSongListForArtist(mActivity,
                            Long.parseLong(mCurrentArtistId)) : MusicUtils
                    .getSongListForAlbum(mActivity,
                            Long.parseLong(mCurrentAlbumId));
            long playlist = item.getIntent().getLongExtra("playlist", 0);
            MusicUtils.addToPlaylist(mActivity, list, playlist);
            return true;
        }

        case DELETE_ITEM: {
            long[] list;
            String desc;
            if (mCurrentArtistId != null) {
                list = MusicUtils.getSongListForArtist(mActivity,
                        Long.parseLong(mCurrentArtistId));
                String f = getString(R.string.delete_artist_desc);
                desc = String.format(f, mCurrentArtistName);
            } else {
                list = MusicUtils.getSongListForAlbum(mActivity,
                        Long.parseLong(mCurrentAlbumId));
                String f = getString(R.string.delete_album_desc);
                desc = String.format(f, mCurrentAlbumName);
            }
            Bundle b = new Bundle();
            b.putString("description", desc);
            b.putLongArray("items", list);
            Intent intent = new Intent();
            intent.setClass(mActivity, DeleteItems.class);
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
        String query = null;

        Intent i = new Intent();
        i.setAction(MediaStore.INTENT_ACTION_MEDIA_SEARCH);
        i.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        if (mCurrentArtistId != null) {
            title = mCurrentArtistName;
            query = mCurrentArtistName;
            i.putExtra(MediaStore.EXTRA_MEDIA_ARTIST, mCurrentArtistName);
            i.putExtra(MediaStore.EXTRA_MEDIA_FOCUS,
                    MediaStore.Audio.Artists.ENTRY_CONTENT_TYPE);
        } else {
            if (mIsUnknownAlbum) {
                title = query = mCurrentArtistNameForAlbum;
            } else {
                title = query = mCurrentAlbumName;
                if (!mIsUnknownArtist) {
                    query = query + " " + mCurrentArtistNameForAlbum;
                }
            }
            i.putExtra(MediaStore.EXTRA_MEDIA_ARTIST,
                    mCurrentArtistNameForAlbum);
            i.putExtra(MediaStore.EXTRA_MEDIA_ALBUM, mCurrentAlbumName);
            i.putExtra(MediaStore.EXTRA_MEDIA_FOCUS,
                    MediaStore.Audio.Albums.ENTRY_CONTENT_TYPE);
        }
        title = getString(R.string.mediasearch, title);
        i.putExtra(SearchManager.QUERY, query);

        startActivity(Intent.createChooser(i, title));
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent intent) {
        switch (requestCode) {
        case SCAN_DONE:
            if (resultCode == mActivity.RESULT_CANCELED) {
                mActivity.finish();
            } else {
                getArtistCursor(mAdapter.getQueryHandler(), null);
            }
            break;

        case NEW_PLAYLIST:
            if (resultCode == mActivity.RESULT_OK) {
                Uri uri = intent.getData();
                if (uri != null) {
                    long[] list = null;
                    if (mCurrentArtistId != null) {
                        list = MusicUtils.getSongListForArtist(mActivity,
                                Long.parseLong(mCurrentArtistId));
                    } else if (mCurrentAlbumId != null) {
                        list = MusicUtils.getSongListForAlbum(mActivity,
                                Long.parseLong(mCurrentAlbumId));
                    }
                    MusicUtils.addToPlaylist(mActivity, list,
                            Long.parseLong(uri.getLastPathSegment()));
                }
            }
            break;
        }
    }

    private Cursor getArtistCursor(AsyncQueryHandler async, String filter) {

        String[] cols = new String[] { MediaStore.Audio.Artists._ID,
                MediaStore.Audio.Artists.ARTIST,
                MediaStore.Audio.Artists.NUMBER_OF_ALBUMS,
                MediaStore.Audio.Artists.NUMBER_OF_TRACKS };

        Uri uri = MediaStore.Audio.Artists.EXTERNAL_CONTENT_URI;
        if (!TextUtils.isEmpty(filter)) {
            uri = uri.buildUpon()
                    .appendQueryParameter("filter", Uri.encode(filter)).build();
        }

        Cursor ret = null;
        if (async != null) {
            async.startQuery(0, null, uri, cols, null, null,
                    MediaStore.Audio.Artists.ARTIST_KEY);
        } else {
            ret = MusicUtils.query(mActivity, uri, cols, null, null,
                    MediaStore.Audio.Artists.ARTIST_KEY);
        }
        return ret;
    }

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
            // a.setTitle("");
            Intent intent = new Intent();
            intent.setClass(mActivity, ScanningProgress.class);
            mActivity.startActivityForResult(intent, Defs.SCAN_DONE);
        }
        if (mSdErrorMessageView != null) {
            mSdErrorMessageView.setVisibility(View.VISIBLE);
        }
        if (mSdErrorMessageIcon != null) {
            mSdErrorMessageIcon.setVisibility(View.VISIBLE);
        }
        if (mExpandableListView != null) {
            mExpandableListView.setVisibility(View.GONE);
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
        if (mExpandableListView != null) {
            mExpandableListView.setVisibility(View.VISIBLE);
        }
    }

    static class ArtistAlbumListAdapter extends SimpleCursorTreeAdapter
            implements SectionIndexer {

        private Drawable mNowPlayingOverlay;
        private final BitmapDrawable mDefaultArtistIcon;
        private final BitmapDrawable mDefaultAlbumIcon;
        private int mGroupArtistIdIdx;
        private int mGroupArtistIdx;
        private int mGroupAlbumIdx;
        private int mGroupSongIdx;
        private final Context mContext;
        private final Resources mResources;
        private final String mAlbumSongSeparator;
        private final String mUnknownAlbum;
        private final StringBuilder mBuffer = new StringBuilder();
        private final Object[] mFormatArgs = new Object[1];
        private final Object[] mFormatArgs3 = new Object[3];
        private MusicAlphabetIndexer mIndexer;
        private ArtistAlbumBrowserFragment mFragment;
        private AsyncQueryHandler mQueryHandler;
        private String mConstraint = null;
        private String mUnknownArtist;
        private boolean mConstraintIsValid = false;
        Cursor mAlbumCursor;

        static class ViewHolder {
            TextView line1;
            TextView line2;
            ImageView play_indicator;
            ImageView mChild1, mChild2, mChild3, mChild4, icon;
            GridLayout mImgHolder;
            boolean isViewAdded;
            String aName;
            View view = null;
            String mCurrentArtistID, mCurrentAlbumID, mCurrentArtistName,
                    mCurrentAlbumName;
        }

        class QueryHandler extends AsyncQueryHandler {
            QueryHandler(ContentResolver res) {
                super(res);
            }

            @Override
            protected void onQueryComplete(int token, Object cookie,
                    Cursor cursor) {
                // Log.i("@@@", "query complete");
                mFragment.init(cursor);
            }
        }

        ArtistAlbumListAdapter(Context context,
                ArtistAlbumBrowserFragment fragment, Cursor cursor,
                int glayout, String[] gfrom, int[] gto, int clayout,
                String[] cfrom, int[] cto) {
            super(context, cursor, glayout, gfrom, gto, clayout, cfrom, cto);
            mQueryHandler = new QueryHandler(context.getContentResolver());
            mFragment = fragment;
            Resources r = context.getResources();
            mNowPlayingOverlay = r
                    .getDrawable(R.drawable.indicator_ic_mp_playing_list);
            mDefaultArtistIcon = (BitmapDrawable) r
                    .getDrawable(R.drawable.unknown_artists);
            mDefaultAlbumIcon = (BitmapDrawable) r
                    .getDrawable(R.drawable.unknown_albums);
            // no filter or dither, it's a lot faster and we can't tell the
            // difference
            mDefaultAlbumIcon.setFilterBitmap(false);
            mDefaultAlbumIcon.setDither(false);
            mContext = context;
            getColumnIndices(cursor);
            mResources = context.getResources();
            mAlbumSongSeparator = context
                    .getString(R.string.albumsongseparator);
            mUnknownAlbum = context.getString(R.string.unknown_album_name);
            mUnknownArtist = context.getString(R.string.unknown_artist_name);
        }

        private void getColumnIndices(Cursor cursor) {
            if (cursor != null) {

                mGroupArtistIdIdx = cursor
                        .getColumnIndexOrThrow(MediaStore.Audio.Artists._ID);
                mGroupArtistIdx = cursor
                        .getColumnIndexOrThrow(MediaStore.Audio.Artists.ARTIST);
                mGroupAlbumIdx = cursor
                        .getColumnIndexOrThrow(MediaStore.Audio.Artists.NUMBER_OF_ALBUMS);
                mGroupSongIdx = cursor
                        .getColumnIndexOrThrow(MediaStore.Audio.Artists.NUMBER_OF_TRACKS);

                if (mIndexer != null) {
                    mIndexer.setCursor(cursor);
                } else {
                    mIndexer = new MusicAlphabetIndexer(cursor,
                            mGroupArtistIdx,
                            mResources.getString(R.string.fast_scroll_alphabet));
                }
            }
        }

        public void setFragment(ArtistAlbumBrowserFragment newFragment) {
            mFragment = newFragment;
        }

        public AsyncQueryHandler getQueryHandler() {
            return mQueryHandler;
        }

        @Override
        public View newGroupView(Context context, Cursor cursor,
                boolean isExpanded, ViewGroup parent) {
            View v = super.newGroupView(context, cursor, isExpanded, parent);
            ViewHolder vh = new ViewHolder();
            vh.line1 = (TextView) v.findViewById(R.id.line1);
            vh.line2 = (TextView) v.findViewById(R.id.line2);
            vh.play_indicator = (ImageView) v.findViewById(R.id.play_indicator);
            ((MediaPlaybackActivity) mFragment.getParentActivity())
                    .setTouchDelegate(vh.play_indicator);
            vh.mImgHolder = (GridLayout) v.findViewById(R.id.iconTabbedView);
            vh.mChild1 = (ImageView) v.findViewById(R.id.img1);
            vh.mChild2 = (ImageView) v.findViewById(R.id.img2);
            vh.mChild3 = (ImageView) v.findViewById(R.id.img3);
            vh.mChild4 = (ImageView) v.findViewById(R.id.img4);
            v.setTag(vh);
            return v;
        }

        @Override
        public View newChildView(Context context, Cursor cursor,
                boolean isLastChild, ViewGroup parent) {
            View v = super.newChildView(context, cursor, isLastChild, parent);
            ViewHolder vh = new ViewHolder();
            vh.line1 = (TextView) v.findViewById(R.id.line1);
            vh.line2 = (TextView) v.findViewById(R.id.line2);
            vh.play_indicator = (ImageView) v
                    .findViewById(R.id.play_indicator_child);
            ((MediaPlaybackActivity) mFragment.getParentActivity())
                    .setTouchDelegate(vh.play_indicator);
            vh.mImgHolder = (GridLayout) v.findViewById(R.id.iconTabbedView);
            vh.icon = (ImageView) v.findViewById(R.id.icon);
            vh.icon.setBackgroundDrawable(mDefaultAlbumIcon);
            v.setTag(vh);
            return v;
        }

        @SuppressWarnings("deprecation")
        @Override
        public void bindGroupView(View view, Context context, Cursor cursor,
                boolean isexpanded) {

            final ViewHolder vh = (ViewHolder) view.getTag();
            ImageView layout[] = new ImageView[] { vh.mChild1, vh.mChild2,
                    vh.mChild3, vh.mChild4 };
            for(ImageView img:layout) img.setVisibility(View.GONE);

            vh.mImgHolder.setBackgroundResource(R.drawable.unknown_artists);
            String artist = cursor.getString(mGroupArtistIdx);
            vh.mCurrentArtistID = cursor.getString(cursor
                    .getColumnIndexOrThrow(MediaStore.Audio.Artists._ID));
            String displayartist = artist;
            vh.mCurrentArtistName = displayartist;
            boolean unknown = artist == null
                    || artist.equals(MediaStore.UNKNOWN_STRING);
            mUnknownArtist = mResources.getString(R.string.unknown_artist_name);
            int numalbums = cursor.getInt(mGroupAlbumIdx);
            int numsongs = cursor.getInt(mGroupSongIdx);
            if (unknown) {
                displayartist = mUnknownArtist;
                vh.mImgHolder.setBackgroundDrawable(mDefaultArtistIcon);
            } else {
                Bitmap[] albumArts = MusicUtils.getFromLruCache(
                        displayartist, MusicUtils.mArtCache);
                if (albumArts == null) {
                } else {
                    if (numalbums == 1) {
                        for (int i = 0; i < layout.length; i++) {
                            layout[i].setVisibility(View.GONE);
                        }
                        if (albumArts[0] != null) {
                            vh.mImgHolder.setBackground(new BitmapDrawable(
                                    mFragment.getResources(), albumArts[0]));
                        } else {
                            vh.mImgHolder.setBackground(mDefaultAlbumIcon);
                        }
                    } else if (numalbums > 1) {
                        vh.mImgHolder.setBackground(null);
                        if (numalbums == 2) {
                            for (int i = 0; i < layout.length; i++) {
                                layout[i].setVisibility(View.VISIBLE);
                            }
                            if (albumArts != null && albumArts.length == 2) {
                                layout[0].setImageBitmap(albumArts[0]);
                                layout[1].setImageBitmap(albumArts[1]);
                                layout[2].setImageBitmap(albumArts[1]);
                                layout[3].setImageBitmap(albumArts[0]);
                            } else {
                                for (int i = 0; i < layout.length; i++) {
                                    layout[i].setImageBitmap(mDefaultAlbumIcon
                                            .getBitmap());
                                }
                            }

                        } else {
                            if(numalbums == 3){
                                layout[3].setVisibility(View.GONE);
                            }
                            for (int i = 0; i < albumArts.length; i++) {
                                layout[i].setVisibility(View.VISIBLE);
                                if (albumArts[i] != null) {
                                    layout[i].setImageBitmap(albumArts[i]);
                                } else {
                                    layout[i].setImageBitmap(mDefaultAlbumIcon
                                            .getBitmap());
                                }
                            }
                        }
                    }
                }
            }
            vh.line1.setText(displayartist);
            String songs_albums = MusicUtils.makeAlbumsLabel(context,
                    numalbums, numsongs, unknown);

            vh.line2.setText(songs_albums);
            // We set different icon according to different play state
            if (MusicUtils.isPlaying()) {
                mNowPlayingOverlay = mResources
                        .getDrawable(R.drawable.indicator_ic_mp_playing_list);
            } else {
                mNowPlayingOverlay = mResources
                        .getDrawable(R.drawable.indicator_ic_mp_pause_list);
            }
            vh.play_indicator.setOnClickListener(new OnClickListener() {

                @SuppressLint("NewApi")
                @Override
                public void onClick(View v) {
                    if (mFragment.mPopupMenu != null ) {
                        mFragment.mPopupMenu.dismiss();
                    }
                    // TODO Auto-generated method stub
                    mFragment.mCurrentArtistId = vh.mCurrentArtistID;
                    mFragment.mCurrentArtistName = vh.mCurrentArtistName;
                    PopupMenu popup = new PopupMenu(mFragment
                            .getParentActivity(), vh.play_indicator);
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
                }
            });
        }

        @Override
        public void bindChildView(View view, Context context, Cursor cursor,
                boolean islast) {

            final ViewHolder vh = (ViewHolder) view.getTag();
            vh.mCurrentArtistID = cursor.getString(mGroupArtistIdIdx);
            String name = cursor.getString(cursor
                    .getColumnIndexOrThrow(MediaStore.Audio.Albums.ALBUM));
            String displayname = name;
            boolean unknown = name == null
                    || name.equals(MediaStore.UNKNOWN_STRING);
            if (unknown) {
                displayname = mUnknownAlbum;
            }
            vh.mCurrentArtistName = cursor.getString(mGroupArtistIdx);
            vh.mCurrentAlbumID = cursor.getString(cursor
                    .getColumnIndexOrThrow(MediaStore.Audio.Albums.ALBUM_ID));
            vh.line1.setText(displayname);
            vh.mCurrentAlbumName = displayname;
            int numsongs = cursor
                    .getInt(cursor
                            .getColumnIndexOrThrow(MediaStore.Audio.Albums.NUMBER_OF_SONGS));
            int numartistsongs = cursor
                    .getInt(cursor
                            .getColumnIndexOrThrow(MediaStore.Audio.Albums.NUMBER_OF_SONGS_FOR_ARTIST));

            //final StringBuilder builder = mBuffer;
            //builder.delete(0, builder.length());
            if (unknown) {
                numsongs = numartistsongs;
            }
            String albumSongNoString = MusicUtils.makeArtistAlbumsSongsLabel(context, numartistsongs);
            vh.line2.setText(albumSongNoString);

            ImageView iv = vh.icon;
            // We don't actually need the path to the thumbnail file,
            // we just use it to see if there is album art or not
            String art = cursor.getString(cursor
                    .getColumnIndexOrThrow(MediaStore.Audio.Albums.ALBUM_ART));
            if (unknown || art == null || art.length() == 0) {
                vh.icon.setBackgroundDrawable(mDefaultAlbumIcon);
                iv.setImageDrawable(null);
            } else {
                long artIndex = cursor.getLong(0);
                Drawable d = MusicUtils.getCachedArtwork(context, artIndex,
                        mDefaultAlbumIcon);
                vh.icon.setImageDrawable(d);
            }

            long currentalbumid = MusicUtils.getCurrentAlbumId();
            long aid = cursor.getLong(0);
            iv = vh.play_indicator;

            // We set different icon according to different play state
            Resources res = context.getResources();
            if (MusicUtils.isPlaying()) {
                mNowPlayingOverlay = res
                        .getDrawable(R.drawable.indicator_ic_mp_playing_list);
            } else {
                mNowPlayingOverlay = res
                        .getDrawable(R.drawable.indicator_ic_mp_pause_list);
            }

            mFragment.mCurrentAlbumId = cursor.getString(cursor
                    .getColumnIndexOrThrow(MediaStore.Audio.Albums.ALBUM_ID));

            iv.setOnClickListener(new OnClickListener() {

                @SuppressLint("NewApi")
                @Override
                public void onClick(View v) {
                    if (mFragment.mPopupMenu != null ) {
                        mFragment.mPopupMenu.dismiss();
                    }
                    // TODO Auto-generated method stub
                    // mFragment.mCurrentArtistId = vh.mCurrentArtistID;
                    mFragment.mCurrentArtistName = vh.mCurrentArtistName;
                    mFragment.mCurrentAlbumName = vh.mCurrentAlbumName;
                    mFragment.mCurrentAlbumId = vh.mCurrentAlbumID;
                    mFragment.mCurrentArtistId = null;
                    PopupMenu popup = new PopupMenu(mFragment
                            .getParentActivity(), vh.play_indicator);
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
                }
            });
        }

        @Override
        protected Cursor getChildrenCursor(Cursor groupCursor) {

            long id = groupCursor.getLong(groupCursor
                    .getColumnIndexOrThrow(MediaStore.Audio.Artists._ID));

            String[] cols = new String[] { MediaStore.Audio.Albums._ID,
                    MediaStore.Audio.Albums.ALBUM_ID,
                    MediaStore.Audio.Albums.ALBUM,
                    MediaStore.Audio.Albums.NUMBER_OF_SONGS,
                    MediaStore.Audio.Albums.NUMBER_OF_SONGS_FOR_ARTIST,
                    MediaStore.Audio.Albums.ALBUM_ART };
            Cursor c = MusicUtils.query(mFragment.getParentActivity(),
                    MediaStore.Audio.Artists.Albums.getContentUri("external",
                            id), cols, null, null,
                    MediaStore.Audio.Albums.DEFAULT_SORT_ORDER);

            class MyCursorWrapper extends CursorWrapper {
                String mArtistName;
                int mMagicColumnIdx;

                MyCursorWrapper(Cursor c, String artist) {
                    super(c);
                    mArtistName = artist;
                    if (mArtistName == null
                            || mArtistName.equals(MediaStore.UNKNOWN_STRING)) {
                        mArtistName = mUnknownArtist;
                    }
                    if (c != null)
                        mMagicColumnIdx = c.getColumnCount();
                }

                @Override
                public String getString(int columnIndex) {
                    if (columnIndex != mMagicColumnIdx) {
                        return super.getString(columnIndex);
                    }
                    return mArtistName;
                }

                @Override
                public int getColumnIndexOrThrow(String name) {
                    if (MediaStore.Audio.Albums.ARTIST.equals(name)) {
                        return mMagicColumnIdx;
                    }
                    return super.getColumnIndexOrThrow(name);
                }

                @Override
                public String getColumnName(int idx) {
                    if (idx != mMagicColumnIdx) {
                        return super.getColumnName(idx);
                    }
                    return MediaStore.Audio.Albums.ARTIST;
                }

                @Override
                public int getColumnCount() {
                    return super.getColumnCount() + 1;
                }
            }
            return new MyCursorWrapper(c,
                    groupCursor.getString(mGroupArtistIdx));
        }

        @Override
        public void changeCursor(Cursor cursor) {
            if (mFragment.getParentActivity().isFinishing() && cursor != null) {
                cursor.close();
                cursor = null;
            }
            if (cursor != mFragment.mArtistCursor) {
                if (mFragment.mArtistCursor != null) {
                    mFragment.mArtistCursor.close();
                    mFragment.mArtistCursor = null;
                }
                mFragment.mArtistCursor = cursor;
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
            Cursor c = mFragment.getArtistCursor(null, s);
            mConstraint = s;
            mConstraintIsValid = true;
            return c;
        }

        public Object[] getSections() {
            return mIndexer.getSections();
        }

        public int getPositionForSection(int sectionIndex) {
            return mIndexer.getPositionForSection(sectionIndex);
        }

        public int getSectionForPosition(int position) {
            return 0;
        }
    }

    private Cursor mArtistCursor;

    public void onServiceConnected(ComponentName name, IBinder service) {
        ((MediaPlaybackActivity) mActivity)
                .updateNowPlaying(getParentActivity());
    }

    public void onServiceDisconnected(ComponentName name) {
        mActivity.finish();
    }

    Handler handler = new Handler() {
        public void handleMessage(Message msg) {
            if (mAdapter != null)
                mAdapter.notifyDataSetChanged();
        };
    };

    public void onScrollStateChanged(final AbsListView view, int scrollState) {
        switch (scrollState) {
        case OnScrollListener.SCROLL_STATE_IDLE:
            isScrolling = false;
            int from = view.getFirstVisiblePosition();
            int to = view.getLastVisiblePosition();
            new MusicUtils.BitmapDownloadThread(getParentActivity(), handler,
                     from, to).start();
            break;
        case OnScrollListener.SCROLL_STATE_TOUCH_SCROLL:
            isScrolling = true;
            break;
        case OnScrollListener.SCROLL_STATE_FLING:
            isScrolling = true;
            break;
        }
    }



    @Override
    public void onScroll(AbsListView view, int firstVisibleItem,
            int visibleItemCount, int totalItemCount) {
        // TODO Auto-generated method stub

    }

    @Override
    public void onConfigurationChanged(Configuration config) {
        super.onConfigurationChanged(config);
        if (mPopupMenu != null) {
            mPopupMenu.dismiss();
        }
        if (mSub != null) {
            mSub.close();
        }
        Fragment fragment = new ArtistAlbumBrowserFragment();
        Bundle args = getArguments();
        fragment.setArguments(args);
        getFragmentManager().beginTransaction()
                .replace(R.id.fragment_page, fragment, "artist_fragment")
                .commitAllowingStateLoss();
    }

}
