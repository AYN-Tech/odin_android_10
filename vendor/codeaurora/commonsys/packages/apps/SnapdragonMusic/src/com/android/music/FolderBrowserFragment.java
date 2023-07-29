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

import android.app.Activity;
import android.app.Fragment;
import android.app.ListActivity;
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
import android.media.AudioManager;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.provider.MediaStore;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.View.OnClickListener;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.SubMenu;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.AdapterView;
import android.widget.AdapterView.AdapterContextMenuInfo;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.PopupMenu.OnMenuItemClickListener;
import android.widget.AlphabetIndexer;
import android.widget.ExpandableListView;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.PopupMenu;
import android.widget.SectionIndexer;
import android.widget.SimpleCursorAdapter;
import android.widget.TextView;

import com.android.music.MusicUtils.Defs;
import com.android.music.MusicUtils.ServiceToken;
import com.android.music.SysApplication;

public class FolderBrowserFragment extends Fragment
        implements View.OnCreateContextMenuListener, MusicUtils.Defs, ServiceConnection, OnItemClickListener
{
    private static String mCurretParent;
    private static String mCurrentPath;
    private FolderListAdapter mAdapter;
    private boolean mAdapterSent;
    private static int mLastListPosCourse = -1;
    private static int mLastListPosFine = -1;
    private ServiceToken mToken;
    private static Cursor mFilesCursor;
    private TextView mSdErrorMessageView;
    private View mSdErrorMessageIcon;
    private ListView mFolderList;
    private MediaPlaybackActivity mActivity;
    public PopupMenu mPopupMenu;
    private static SubMenu mSub = null;

    public Activity getParentActivity() {
        return mActivity;
    }

    @Override
    public void onAttach(Activity activity) {
        super.onAttach(activity);
        mActivity = (MediaPlaybackActivity) activity;
    }
    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {
        View rootView = inflater.inflate(
                R.layout.media_picker_folder, container, false);
        mSdErrorMessageView = (TextView) rootView.findViewById(R.id.sd_message);
        mSdErrorMessageIcon = rootView.findViewById(R.id.sd_icon);
        mFolderList = (ListView) rootView.findViewById(R.id.folder_list);
        mFolderList.setTextFilterEnabled(true);
        mFolderList.setOnItemClickListener(this);
        mFolderList.setDividerHeight(0);

        mAdapter = new FolderListAdapter(
                mActivity.getApplicationContext(),
                this,
                R.layout.track_list_item,
                mFilesCursor,
                new String[] {},
                new int[] {});
        mFolderList.setAdapter(mAdapter);
        getFolderCursor(mAdapter.getQueryHandler(), null);
        return rootView;
    }
    @Override
    public void onCreate(Bundle icicle)
    {
        super.onCreate(icicle);
        if (icicle != null) {
            mCurretParent = icicle.getString("selectedParent");
        }
        mToken = MusicUtils.bindToService(mActivity, this);
    }

   /* @Override
    public Object onRetainNonConfigurationInstance() {
        mAdapterSent = true;
        return mAdapter;
    }*/

    @Override
    public void onSaveInstanceState(Bundle outcicle) {
        outcicle.putString("selectedParent", mCurretParent);
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
        if (mFolderList != null) {
            mLastListPosCourse = mFolderList.getFirstVisiblePosition();
            View cv = mFolderList.getChildAt(0);
            if (cv != null) {
                mLastListPosFine = cv.getTop();
            }
        }
        MusicUtils.unbindFromService(mToken);

        if (!mAdapterSent && mAdapter != null) {
            mAdapter.changeCursor(null);
        }

        mFolderList.setAdapter(null);
        mAdapter = null;
        super.onDestroy();
    }

    @Override
    public void onResume() {
        super.onResume();
        IntentFilter f = new IntentFilter();
        f.addAction(MediaPlaybackService.META_CHANGED);
        f.addAction(MediaPlaybackService.QUEUE_CHANGED);
        mActivity.registerReceiver(mTrackListListener, f);
        mTrackListListener.onReceive(null, null);

       // MusicUtils.setSpinnerState(this);
    }

    private BroadcastReceiver mTrackListListener = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            mFolderList.invalidateViews();
            mActivity.updateNowPlaying(mActivity);
        }
    };
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
                getFolderCursor(mAdapter.getQueryHandler(), null);
            }
        }
    };

    @Override
    public void onPause() {
        mActivity.unregisterReceiver(mTrackListListener);
        mReScanHandler.removeCallbacksAndMessages(null);
        super.onPause();
    }

    /*@Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (event.getAction() == KeyEvent.ACTION_UP &&
                event.getKeyCode() == KeyEvent.KEYCODE_BACK) {
            finish();
            return true;
        }
        return super.dispatchKeyEvent(event);
    }*/

    public void init(Cursor c) {
        if (mAdapter == null) {
            return;
        }
        mAdapter.changeCursor(c);

        if (mFilesCursor == null) {
            displayDatabaseError();
            mReScanHandler.sendEmptyMessageDelayed(0, 1000);
            return;
        }

        if (mLastListPosCourse >= 0) {
            mFolderList.setSelectionFromTop(mLastListPosCourse, mLastListPosFine);
            mLastListPosCourse = -1;
        }

        hideDatabaseError();
        if (mFilesCursor.getCount() == 0) {
            mSdErrorMessageView.setVisibility(View.VISIBLE);
            mSdErrorMessageView.setText(R.string.no_music_found);
            mFolderList.setVisibility(View.GONE);
        }
        //setTitle();
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
        if (mFolderList != null) {
            mFolderList.setVisibility(View.GONE);
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
        if (mFolderList != null) {
            mFolderList.setVisibility(View.VISIBLE);
        }
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View view, ContextMenuInfo menuInfoIn) {
        menu.add(0, PLAY_SELECTION, 0, R.string.play_selection);
        mSub = menu.addSubMenu(0, ADD_TO_PLAYLIST, 0, R.string.add_to_playlist);
        MusicUtils.makePlaylistMenu(mActivity, mSub);
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
                long[] list = MusicUtils.getSongListForFolder(mActivity, Long.parseLong(mCurretParent));
                MusicUtils.playAll(mActivity, list, 0);
                return true;
            }

            case QUEUE: {
                long[] list = MusicUtils.getSongListForFolder(mActivity, Long.parseLong(mCurretParent));
                MusicUtils.addToCurrentPlaylist(mActivity, list);
                MusicUtils.addToPlaylist(mActivity, list, MusicUtils.getPlayListId());
                return true;
            }

            case NEW_PLAYLIST: {
                Intent intent = new Intent();
                intent.setClass(mActivity, CreatePlaylist.class);
                startActivityForResult(intent, NEW_PLAYLIST);
                return true;
            }

            case PLAYLIST_SELECTED: {
                long[] list = MusicUtils
                        .getSongListForFolder(mActivity, Long.parseLong(mCurretParent));
                long playlist = item.getIntent().getLongExtra("playlist", 0);
                MusicUtils.addToPlaylist(mActivity, list, playlist);
                return true;
            }
        }
        return super.onContextItemSelected(item);
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent intent) {
        switch (requestCode) {
            case SCAN_DONE:
                if (resultCode == mActivity.RESULT_CANCELED) {
                    mActivity.finish();
                } else {
                    getFolderCursor(mAdapter.getQueryHandler(), null);
                }
                break;

            case NEW_PLAYLIST:
                if (resultCode == mActivity.RESULT_OK) {
                    Uri uri = intent.getData();
                    if (uri != null) {
                        long[] list = MusicUtils.getSongListForFolder(mActivity,
                                Long.parseLong(mCurretParent));
                        MusicUtils.addToPlaylist(mActivity, list,
                                Long.parseLong(uri.getLastPathSegment()));
                    }
                }
                break;
        }
    }

    public String getAlbumName(String path){
        String name;
        String mUnknownPath = mActivity.getString(R.string.unknown_folder_name);
        String rootPath = getRootPath(path);
          if (path == null || path.trim().equals("")) {
              name = mUnknownPath;
          } else {
              String[] split = rootPath.split("/");
              name = split[split.length - 1];
          }
          return name;
    }

    @Override
    public void onItemClick(AdapterView<?> parentview, View view, int position,
            long id) {
        //Intent intent = new Intent(Intent.ACTION_PICK);
        //intent.setDataAndType(Uri.EMPTY, "vnd.android.cursor.dir/track");
        Cursor c = (Cursor) mAdapter.getItem(position);
        int index = c.getColumnIndexOrThrow(MediaStore.Files.FileColumns.PARENT);
        int parent = c.getInt(index);
        //intent.putExtra("parent", parent);
        index = c.getColumnIndexOrThrow(MediaStore.Files.FileColumns.DATA);
        String path = c.getString(index);
      //  intent.putExtra("rootPath", getRootPath(path));
       // startActivity(intent);

        Fragment fragment = new TrackBrowserFragment();
        Bundle args = new Bundle();
        args.putInt("parent", parent);
        args.putString("rootPath", getRootPath(path));
        args.putString("folder_name", getAlbumName(path));

        fragment.setArguments(args);
        getFragmentManager()
                .beginTransaction()
                .replace(R.id.fragment_page, fragment, "folder_fragment")
                .commit();
        MusicUtils.navigatingTabPosition = 3;
    }

    private Cursor getFolderCursor(AsyncQueryHandler async, String filter) {
        Cursor ret = null;
        String uriString = "content://media/external/file";
        String selection = "is_music = 1 & 1=1 ) GROUP BY (parent" ;
        String[] projection = {"count(_id)","_id","_data","parent"};
        Uri uri = Uri.parse(uriString);
        if (async != null) {
            async.startQuery(0, null, uri, projection, selection, null, null);
        } else {
            ret = MusicUtils.query(mActivity, uri, projection, selection, null, null);
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

     class FolderListAdapter extends SimpleCursorAdapter implements SectionIndexer {

        private Drawable mNowPlaying;
        private final BitmapDrawable mDefaultAlbumIcon;
        private int mDataIdx;
        private int mCountIdx;
        private final Resources mResources;
        private final String mUnknownPath;
        private final String mUnknownCount;
        private AlphabetIndexer mIndexer;
        private FolderBrowserFragment mFragment;
        private AsyncQueryHandler mQueryHandler;
        private String mConstraint = null;
        private boolean mConstraintIsValid = false;
        private final Object[] mFormatArgs = new Object[1];

        class ViewHolder {
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
                mFragment.init(cursor);
            }
        }

        FolderListAdapter(Context context, FolderBrowserFragment currentfragment,
                int layout, Cursor cursor, String[] from, int[] to) {
            super(context, layout, cursor, from, to);

            mFragment = currentfragment;
            mQueryHandler = new QueryHandler(context.getContentResolver());
            mUnknownPath = context.getString(R.string.unknown_folder_name);
            mUnknownCount = context.getString(R.string.unknown_folder_count);
            Resources r = context.getResources();
            mNowPlaying = r.getDrawable(R.drawable.indicator_ic_mp_playing_list);
            Bitmap b = BitmapFactory.decodeResource(r, R.drawable.album_cover_background);
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

        public void setActivity(FolderBrowserFragment newfragment) {
            mFragment = newfragment;
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
            View animView = v.findViewById(R.id.animView);
            animView.setVisibility(View.GONE);
            //vh.icon = (ImageView) v.findViewById(R.id.icon);
            //vh.icon.setBackgroundDrawable(mDefaultAlbumIcon);
            //vh.icon.setPadding(0, 0, 1, 0);
            v.setTag(vh);
            return v;
        }

        @Override
        public void bindView(View view, Context context, final Cursor cursor) {
          final  ViewHolder vh = (ViewHolder) view.getTag();

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

             final int p = cursor.getPosition();
            vh.line2.setText(numString);

            vh.play_indicator.setOnClickListener(new OnClickListener() {

                @Override
                public void onClick(View v) {
                   // mFragment.mCurrentAlbumName = vh.mCurrentAlbumName;
                    if (mFragment.mPopupMenu != null ) {
                        mFragment.mPopupMenu.dismiss();
                    }
                    PopupMenu popup = new PopupMenu(mFragment
                            .getParentActivity(), vh.play_indicator);
                    popup.getMenu().add(0, PLAY_SELECTION, 0,
                            R.string.play_selection);
                    mSub = popup.getMenu().addSubMenu(0,
                            ADD_TO_PLAYLIST, 0, R.string.add_to_playlist);
                    MusicUtils.makePlaylistMenu(mFragment.getParentActivity(),
                            mSub);

                    popup.show();
                    popup.setOnMenuItemClickListener(new OnMenuItemClickListener() {

                        @Override
                        public boolean onMenuItemClick(MenuItem item) {
                            mFragment.onContextItemSelected(item);
                            return true;
                        }
                    });
                    mFragment.mPopupMenu = popup;

                    mFilesCursor.moveToPosition(p);
                    mCurretParent = mFilesCursor.getString(mFilesCursor
                            .getColumnIndexOrThrow(MediaStore.Files.FileColumns.PARENT));
                    mCurrentPath = mFilesCursor.getString(mFilesCursor
                            .getColumnIndexOrThrow(MediaStore.Files.FileColumns.DATA));
                }
            });
        }

        @Override
        public void changeCursor(Cursor cursor) {
            if (mFragment.getParentActivity() != null
                    && mFragment.getParentActivity().isFinishing()
                    && cursor != null) {
                cursor.close();
                cursor = null;
            }
            if (cursor != mFragment.mFilesCursor) {
                if (mFragment.mFilesCursor != null) {
                    mFragment.mFilesCursor.close();
                    mFragment.mFilesCursor = null;
                }
                mFragment.mFilesCursor = cursor;
                if ((cursor != null && !cursor.isClosed()) || cursor == null) {
                    getColumnIndices(cursor);
                    super.changeCursor(cursor);
                }
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
            Cursor c = mFragment.getFolderCursor(null, s);
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
        ((MediaPlaybackActivity) mActivity)
        .updateNowPlaying(getActivity());
    }

    public void onServiceDisconnected(ComponentName name) {
        mActivity.finish();
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
        Fragment fragment = new FolderBrowserFragment();
        Bundle args = getArguments();
        fragment.setArguments(args);
        getFragmentManager().beginTransaction()
                .replace(R.id.fragment_page, fragment, "folder_fragment")
                .commitAllowingStateLoss();
    }
}
