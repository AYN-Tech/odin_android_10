/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *        * Redistributions of source code must retain the above copyright
 *            notice, this list of conditions and the following disclaimer.
 *        * Redistributions in binary form must reproduce the above copyright
 *            notice, this list of conditions and the following disclaimer in the
 *            documentation and/or other materials provided with the distribution.
 *        * Neither the name of The Linux Foundation nor
 *            the names of its contributors may be used to endorse or promote
 *            products derived from this software without specific prior written
 *            permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.    IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

package com.android.music;

import android.app.ListActivity;
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
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.provider.MediaStore;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.SubMenu;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.AdapterView.AdapterContextMenuInfo;
import android.widget.AlphabetIndexer;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.SectionIndexer;
import android.widget.SimpleCursorAdapter;
import android.widget.TextView;

import com.android.music.MusicUtils.ServiceToken;
import com.android.music.SysApplication;

public class FolderBrowserActivity extends ListActivity
        implements View.OnCreateContextMenuListener, MusicUtils.Defs, ServiceConnection
{
    private String mCurretParent;
    private String mCurrentPath;
    private FolderListAdapter mAdapter;
    private boolean mAdapterSent;
    private static int mLastListPosCourse = -1;
    private static int mLastListPosFine = -1;
    private ServiceToken mToken;
    private Cursor mFilesCursor;

    @Override
    public void onCreate(Bundle icicle)
    {
        if (icicle != null) {
            mCurretParent = icicle.getString("selectedParent");
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

        setContentView(R.layout.media_picker_activity);

        MusicUtils.updateButtonBar(this, R.id.foldertab);
        ListView lv = getListView();
        lv.setOnCreateContextMenuListener(this);
        lv.setTextFilterEnabled(true);

        mAdapter = (FolderListAdapter) getLastNonConfigurationInstance();
        if (mAdapter == null) {
            mAdapter = new FolderListAdapter(
                    getApplication(),
                    this,
                    R.layout.track_list_item,
                    mFilesCursor,
                    new String[] {},
                    new int[] {});
            setListAdapter(mAdapter);
            setTitle(R.string.working_folders);
            getFolderCursor(mAdapter.getQueryHandler(), null);
        } else {
            mAdapter.setActivity(this);
            setListAdapter(mAdapter);
            mFilesCursor = mAdapter.getCursor();
            if (mFilesCursor != null) {
                init(mFilesCursor);
            } else {
                getFolderCursor(mAdapter.getQueryHandler(), null);
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
        outcicle.putString("selectedParent", mCurretParent);
        super.onSaveInstanceState(outcicle);
    }

    @Override
    public void onDestroy() {
        ListView lv = getListView();
        if (lv != null) {
            mLastListPosCourse = lv.getFirstVisiblePosition();
            View cv = lv.getChildAt(0);
            if (cv != null) {
                mLastListPosFine = cv.getTop();
            }
        }
        MusicUtils.unbindFromService(mToken);

        if (!mAdapterSent && mAdapter != null) {
            mAdapter.changeCursor(null);
        }

        setListAdapter(null);
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
            getListView().invalidateViews();
            MusicUtils.updateNowPlaying(FolderBrowserActivity.this, false);
        }
    };
    private BroadcastReceiver mScanListener = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            MusicUtils.setSpinnerState(FolderBrowserActivity.this);
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
                getFolderCursor(mAdapter.getQueryHandler(), null);
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
        if (event.getAction() == KeyEvent.ACTION_UP &&
                event.getKeyCode() == KeyEvent.KEYCODE_BACK) {
            finish();
            return true;
        }
        return super.dispatchKeyEvent(event);
    }

    public void init(Cursor c) {
        if (mAdapter == null) {
            return;
        }
        mAdapter.changeCursor(c);

        if (mFilesCursor == null) {
            MusicUtils.displayDatabaseError(this);
            closeContextMenu();
            mReScanHandler.sendEmptyMessageDelayed(0, 1000);
            return;
        }

        if (mLastListPosCourse >= 0) {
            getListView().setSelectionFromTop(mLastListPosCourse, mLastListPosFine);
            mLastListPosCourse = -1;
        }

        MusicUtils.hideDatabaseError(this);
        MusicUtils.updateButtonBar(this, R.id.foldertab);
        setTitle();
    }

    private void setTitle() {
        CharSequence fancyName = getText(R.string.unknown_folder_name);
        if (mFilesCursor != null && mFilesCursor.getCount() > 0) {
            mFilesCursor.moveToFirst();
            String path = mFilesCursor.getString(
                    mFilesCursor.getColumnIndex(MediaStore.Files.FileColumns.DATA));
            fancyName = getRootPath(path);
        }
        setTitle(fancyName);
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View view, ContextMenuInfo menuInfoIn) {
        menu.add(0, PLAY_SELECTION, 0, R.string.play_selection);
        SubMenu sub = menu.addSubMenu(0, ADD_TO_PLAYLIST, 0, R.string.add_to_playlist);
        MusicUtils.makePlaylistMenu(this, sub);
        AdapterContextMenuInfo mi = (AdapterContextMenuInfo) menuInfoIn;
        mFilesCursor.moveToPosition(mi.position);
        mCurretParent = mFilesCursor.getString(mFilesCursor
                .getColumnIndexOrThrow(MediaStore.Files.FileColumns.PARENT));
        mCurrentPath = mFilesCursor.getString(mFilesCursor
                .getColumnIndexOrThrow(MediaStore.Files.FileColumns.DATA));
        menu.setHeaderTitle(getRootPath(mCurrentPath));
    }

    @Override
    public boolean onContextItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case PLAY_SELECTION: {
                long[] list = MusicUtils.getSongListForFolder(this, Long.parseLong(mCurretParent));
                MusicUtils.playAll(this, list, 0);
                return true;
            }

            case QUEUE: {
                long[] list = MusicUtils.getSongListForFolder(this, Long.parseLong(mCurretParent));
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
                long[] list = MusicUtils
                        .getSongListForFolder(this, Long.parseLong(mCurretParent));
                long playlist = item.getIntent().getLongExtra("playlist", 0);
                MusicUtils.addToPlaylist(this, list, playlist);
                return true;
            }
        }
        return super.onContextItemSelected(item);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent intent) {
        switch (requestCode) {
            case SCAN_DONE:
                if (resultCode == RESULT_CANCELED) {
                    finish();
                } else {
                    getFolderCursor(mAdapter.getQueryHandler(), null);
                }
                break;

            case NEW_PLAYLIST:
                if (resultCode == RESULT_OK) {
                    Uri uri = intent.getData();
                    if (uri != null) {
                        long[] list = MusicUtils.getSongListForFolder(this,
                                Long.parseLong(mCurretParent));
                        MusicUtils.addToPlaylist(this, list,
                                Long.parseLong(uri.getLastPathSegment()));
                    }
                }
                break;
        }
    }

    @Override
    protected void onListItemClick(ListView l, View v, int position, long id) {
        Intent intent = new Intent(Intent.ACTION_PICK);
        intent.setDataAndType(Uri.EMPTY, "vnd.android.cursor.dir/track");
        Cursor c = (Cursor) mAdapter.getItem(position);
        int index = c.getColumnIndexOrThrow(MediaStore.Files.FileColumns.PARENT);
        int parent = c.getInt(index);
        intent.putExtra("parent", parent);
        index = c.getColumnIndexOrThrow(MediaStore.Files.FileColumns.DATA);
        String path = c.getString(index);
        intent.putExtra("rootPath", getRootPath(path));
        startActivity(intent);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        super.onCreateOptionsMenu(menu);
        menu.add(0, PARTY_SHUFFLE, 0, R.string.party_shuffle);
        menu.add(0, SHUFFLE_ALL, 0, R.string.shuffle_all).setIcon(R.drawable.ic_menu_shuffle);
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
            case PARTY_SHUFFLE:
                MusicUtils.togglePartyShuffle();
                AudioManager audioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
                audioManager.playSoundEffect(AudioManager.FX_KEY_CLICK);
                break;
            case SHUFFLE_ALL:
                cursor = MusicUtils.query(this, MediaStore.Audio.Media.EXTERNAL_CONTENT_URI,
                        new String[] {
                            MediaStore.Audio.Media._ID
                        },
                        MediaStore.Audio.Media.IS_MUSIC + "=1", null,
                        MediaStore.Audio.Media.DEFAULT_SORT_ORDER);
                if (cursor != null) {
                    MusicUtils.shuffleAll(this, cursor);
                    cursor.close();
                }
                return true;
        }
        return super.onOptionsItemSelected(item);
    }

    private Cursor getFolderCursor(AsyncQueryHandler async, String filter) {
        Cursor ret = null;
        String uriString = "content://media/external/audio/folder";
        Uri uri = Uri.parse(uriString);
        if (async != null) {
            async.startQuery(0, null, uri, null, null, null, null);
        } else {
            ret = MusicUtils.query(this, uri, null, null, null, null);
        }
        return ret;
    }

    public static String getRootPath(String path) {
        if (path != null) {
            String root = "";
            int pos = path.lastIndexOf("/");
            if (pos >= 0) {
                root = path.substring(0, pos);
            }
            return root;
        } else {
            return "";
        }
    }

    static class FolderListAdapter extends SimpleCursorAdapter implements SectionIndexer {

        private Drawable mNowPlaying;
        private final BitmapDrawable mDefaultAlbumIcon;
        private int mDataIdx;
        private int mCountIdx;
        private final Resources mResources;
        private final String mUnknownPath;
        private final String mUnknownCount;
        private AlphabetIndexer mIndexer;
        private FolderBrowserActivity mActivity;
        private AsyncQueryHandler mQueryHandler;
        private String mConstraint = null;
        private boolean mConstraintIsValid = false;
        private final Object[] mFormatArgs = new Object[1];

        static class ViewHolder {
            TextView line1;
            TextView line2;
            ImageView play_indicator;
            ImageView icon;
        }

        class QueryHandler extends AsyncQueryHandler {
            QueryHandler(ContentResolver res) {
                super(res);
            }

            @Override
            protected void onQueryComplete(int token, Object cookie, Cursor cursor) {
                mActivity.init(cursor);
            }
        }

        FolderListAdapter(Context context, FolderBrowserActivity currentactivity,
                int layout, Cursor cursor, String[] from, int[] to) {
            super(context, layout, cursor, from, to);

            mActivity = currentactivity;
            mQueryHandler = new QueryHandler(context.getContentResolver());
            mUnknownPath = context.getString(R.string.unknown_folder_name);
            mUnknownCount = context.getString(R.string.unknown_folder_count);
            Resources r = context.getResources();
            mNowPlaying = r.getDrawable(R.drawable.indicator_ic_mp_playing_list);
            Bitmap b = BitmapFactory.decodeResource(r, R.drawable.albumart_mp_unknown_list);
            mDefaultAlbumIcon = new BitmapDrawable(context.getResources(), b);
            mDefaultAlbumIcon.setFilterBitmap(false);
            mDefaultAlbumIcon.setDither(false);
            getColumnIndices(cursor);
            mResources = context.getResources();
        }

        private void getColumnIndices(Cursor cursor) {
            if (cursor != null) {
                mDataIdx = cursor.getColumnIndexOrThrow(MediaStore.Files.FileColumns.DATA);
                mCountIdx = cursor.getColumnIndexOrThrow("count(_id)");
                if (mIndexer != null) {
                    mIndexer.setCursor(cursor);
                } else {
                    mIndexer = new MusicAlphabetIndexer(cursor, mDataIdx, mResources.getString(
                            R.string.fast_scroll_alphabet));
                }
            }
        }

        public void setActivity(FolderBrowserActivity newactivity) {
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
            vh.play_indicator = (ImageView) v.findViewById(R.id.play_indicator);
            vh.icon = (ImageView) v.findViewById(R.id.icon);
            vh.icon.setBackgroundDrawable(mDefaultAlbumIcon);
            vh.icon.setPadding(0, 0, 1, 0);
            v.setTag(vh);
            return v;
        }

        @Override
        public void bindView(View view, Context context, Cursor cursor) {
            ViewHolder vh = (ViewHolder) view.getTag();

            String path = cursor.getString(mDataIdx);
            String rootPath = getRootPath(path);
            String name;
            if (path == null || path.trim().equals("")) {
                name = mUnknownPath;
            } else {
                String[] split = rootPath.split("/");
                name = split[split.length - 1];
            }
            vh.line1.setText(name);

            String numString = mUnknownCount;
            int count = cursor.getInt(mCountIdx);
            if (count == 1) {
                numString = context.getString(R.string.onesong);
            } else {
                final Object[] args = mFormatArgs;
                args[0] = count;
                numString = mResources.getQuantityString(R.plurals.Nsongs, count, args);
            }
            vh.line2.setText(numString);

            ImageView iv = vh.icon;
            iv.setImageDrawable(null);
            iv = vh.play_indicator;
            if (MusicUtils.isPlaying()) {
                mNowPlaying = mResources.getDrawable(R.drawable.indicator_ic_mp_playing_list);
            } else {
                mNowPlaying = mResources.getDrawable(R.drawable.indicator_ic_mp_pause_list);
            }
            String currentData = MusicUtils.getCurrentData();
            if (rootPath.equals(FolderBrowserActivity.getRootPath(currentData))) {
                iv.setImageDrawable(mNowPlaying);
            } else {
                iv.setImageDrawable(null);
            }
        }

        @Override
        public void changeCursor(Cursor cursor) {
            if (mActivity.isFinishing() && cursor != null) {
                cursor.close();
                cursor = null;
            }
            if (cursor != mActivity.mFilesCursor) {
                if (mActivity.mFilesCursor != null) {
                    mActivity.mFilesCursor.close();
                    mActivity.mFilesCursor = null;
                }
                mActivity.mFilesCursor = cursor;
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
            Cursor c = mActivity.getFolderCursor(null, s);
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

    public void onServiceConnected(ComponentName name, IBinder service) {
        MusicUtils.updateNowPlaying(this, false);
    }

    public void onServiceDisconnected(ComponentName name) {
        finish();
    }
}
