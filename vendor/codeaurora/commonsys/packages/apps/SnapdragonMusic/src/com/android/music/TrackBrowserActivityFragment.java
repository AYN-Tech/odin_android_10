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
import com.android.music.TrackBrowserActivityFragment.TrackListAdapter.ViewHolder;
import java.util.Arrays;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.Fragment;
import android.app.FragmentManager;
import android.app.SearchManager;
import android.content.AsyncQueryHandler;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.ServiceConnection;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.database.AbstractCursor;
import android.database.CharArrayBuffer;
import android.database.Cursor;
//import android.drm.DrmManagerClientWrapper;
import android.drm.DrmStore.Action;
//import android.drm.DrmStore.DrmDeliveryType;
import android.drm.DrmStore.RightsStatus;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.Color;
import android.media.AudioManager;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.RemoteException;
import android.provider.MediaStore;
import android.provider.MediaStore.Audio.Playlists;
import android.provider.MediaStore.Video.VideoColumns;
import android.telephony.SubscriptionInfo;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;
import android.text.TextUtils;
import android.util.Log;
import android.view.ContextMenu;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.SubMenu;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.Window;
import android.view.ContextMenu.ContextMenuInfo;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.AlphabetIndexer;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.PopupMenu;
//import android.widget.SectionIndexer;
//import android.widget.SimpleCursorAdapter;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.AdapterView.AdapterContextMenuInfo;
import android.widget.PopupMenu.OnMenuItemClickListener;
import android.view.KeyEvent;

import com.android.music.SysApplication;
import com.android.music.TrackBrowserFragment.WaveView;

import java.text.Collator;
import java.util.Arrays;

public class TrackBrowserActivityFragment extends Fragment
        implements MusicUtils.Defs, ServiceConnection,OnItemClickListener
{
    public static final String BUY_LICENSE = "android.drmservice.intent.action.BUY_LICENSE";
    private static final String TAG = "TrackBrowserActivityFragment";
    private static final int Q_SELECTED = CHILD_MENU_BASE;
    private static final int Q_ALL = CHILD_MENU_BASE + 1;
    private static final int SAVE_AS_PLAYLIST = CHILD_MENU_BASE + 2;
    private static final int PLAY_ALL = CHILD_MENU_BASE + 3;
    private static final int CLEAR_PLAYLIST = CHILD_MENU_BASE + 4;
    private static final int REMOVE = CHILD_MENU_BASE + 5;
    private static final int SEARCH = CHILD_MENU_BASE + 6;
    private static final int SHARE = CHILD_MENU_BASE + 7; // Menu to share audio
    private static final int DETAILS = CHILD_MENU_BASE + 100;

    private static final String LOGTAG = "TrackBrowser";
    static boolean mIsRepeatPlay = false;
    private String[] mCursorCols;
    private String[] mPlaylistMemberCols;
    private boolean mDeletedOneRow = false;
    private boolean mEditMode = false;
    private String mCurrentTrackName;
    private String mCurrentAlbumName;
    private String mCurrentArtistNameForAlbum;
    private String mCurrentTrackDuration;
    private String mCurrentTrackPath;
    private String mCurrentTrackSize;
    private ListView mTrackList;
    private static Cursor mTrackCursor;
    private TrackListAdapter mAdapter;
    private boolean mAdapterSent = false;
    private String mAlbumId;
    private String mArtistId;
    private String mPlaylist;
    private String mGenre;
    private String mSortOrder;
    private int mParent = -1;
    private String mRootPath;
    private static int mSelectedPosition;
    private long mSelectedId;
    private static int mLastListPosCourse = -1;
    private static int mLastListPosFine = -1;
    private boolean mUseLastListPos = false;
    private ServiceToken mToken;
    private SubMenu mSub = null;
    private ListView mListView;
    private TextView mTextView1;
    private TextView mTextView2;
    private MediaPlaybackActivity mParentActivity;
    private ImageView mImageView;
    private boolean mIsparentActivityFInishing;
    private BitmapDrawable mDefaultAlbumIcon;
    private static WaveView mAnimView;
    private FrameLayout mContainer = null;
    private static boolean mPause = false;
    private static int mOrientation = Configuration.ORIENTATION_UNDEFINED;
    public PopupMenu mPopupMenu;

    public TrackBrowserActivityFragment()
    {
    }

    public ListView getListView() {
        return mListView;
    }

    private void setListAdapter(TrackListAdapter adapter) {
        getListView().setAdapter(adapter);
    }

    public Activity getParentActivity(){
        return mParentActivity;
    }

    public void finishActivity(View v) {
        mParentActivity.finish();
    }
    public void repeatPlay() {
        int position = mSelectedPosition;
        MusicUtils.mRepeatPlay = true;
        MusicUtils.playAll(mParentActivity, mTrackCursor, position);
        TrackListAdapter ad = (TrackListAdapter) getListView().getAdapter();
        ad.notifyDataSetChanged();
    }

    @Override
    public void onAttach(Activity activity) {
        // TODO Auto-generated method stub
        super.onAttach(activity);
        mParentActivity = (MediaPlaybackActivity) activity;
    }
    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle icicle)
    {
        super.onCreate(icicle);
        Intent intent = mParentActivity.getIntent();
        if (intent != null) {
            if (intent.getBooleanExtra("withtabs", false)) {
                mParentActivity.requestWindowFeature(Window.FEATURE_NO_TITLE);
            }
        }
        mParentActivity.setVolumeControlStream(AudioManager.STREAM_MUSIC);
        if (icicle != null) {
            mSelectedId = icicle.getLong("selectedtrack");
            mAlbumId = icicle.getString("album");
            mArtistId = icicle.getString("artist");
            mPlaylist = icicle.getString("playlist");
            mGenre = icicle.getString("genre");
            if (MusicUtils.isGroupByFolder()) {
                mParent = icicle.getInt("parent", -1);
                mRootPath = icicle.getString("rootPath");
            }
            mEditMode = icicle.getBoolean("editmode", false);
        } else {
            mAlbumId = intent.getStringExtra("album");
            // If we have an album, show everything on the album, not just stuff
            // by a particular artist.
            mArtistId = intent.getStringExtra("artist");
            if (MusicUtils.isGroupByFolder()) {
                mParent = intent.getIntExtra("parent", -1);
                mRootPath = intent.getStringExtra("rootPath");
            }
        }

        mCursorCols = new String[] {
                MediaStore.Audio.Media._ID,
                MediaStore.Audio.Media.TITLE,
                MediaStore.Audio.Media.DATA,
                MediaStore.Audio.Media.ALBUM,
                MediaStore.Audio.Media.ARTIST,
                MediaStore.Audio.Media.ARTIST_ID,
                MediaStore.Audio.Media.DURATION,
                MediaStore.Audio.Media.SIZE
        };
        mPlaylistMemberCols = new String[] {
                MediaStore.Audio.Playlists.Members._ID,
                MediaStore.Audio.Media.TITLE,
                MediaStore.Audio.Media.DATA,
                MediaStore.Audio.Media.ALBUM,
                MediaStore.Audio.Media.ARTIST,
                MediaStore.Audio.Media.ARTIST_ID,
                MediaStore.Audio.Media.DURATION,
                MediaStore.Audio.Media.SIZE,
                MediaStore.Audio.Playlists.Members.PLAY_ORDER,
                MediaStore.Audio.Playlists.Members.AUDIO_ID,
                MediaStore.Audio.Media.IS_MUSIC
        };
        //SysApplication.getInstance().addActivity(parentActivity);
    }

    @Override
    public void onStart() {
        // TODO Auto-generated method stub
        super.onStart();
        mIsparentActivityFInishing = false;
    }

    @Override
    public void onStop() {
        // TODO Auto-generated method stub
        super.onStop();
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {
        // TODO Auto-generated method stub
        mContainer = new FrameLayout(getContext());
        View rootView = inflater.inflate(R.layout.media_picker_activity, container, false);
        if(getArguments()!=null){
            mAlbumId = getArguments().getString("album");
            mArtistId = getArguments().getString("artist");

            if (MusicUtils.isGroupByFolder()) {
                mParent = getArguments().getInt("parent", -1);
                mRootPath = getArguments().getString("rootPath");
            }

        }
        initViews(mContainer ,rootView);
        mToken = MusicUtils.bindToService(mParentActivity, this);
        return mContainer;
    }

    private void initViews(FrameLayout container ,View rootView) {
        mListView = (ListView)rootView. findViewById(R.id.media_list);
        mTextView1 = (TextView) rootView.findViewById(R.id.textView1);
        mTextView2 = (TextView)rootView.findViewById(R.id.textView2);
        mImageView = (ImageView) rootView.findViewById(R.id.imageView1);
        Resources r = getResources();
        mDefaultAlbumIcon = (BitmapDrawable)r.getDrawable(R.drawable.unknown_albums);

        ImageView searchIcon = (ImageView)rootView.findViewById(R.id.imageView3);
        searchIcon.setOnClickListener(new OnClickListener() {

            @Override
            public void onClick(View v) {
                // TODO Auto-generated method stub
                mIsparentActivityFInishing = true;
                Intent intent = new Intent(mParentActivity,
                        QueryBrowserActivity.class);
                startActivity(intent);
            }
        });
        ImageView backIcon = (ImageView) rootView.findViewById(R.id.imageView2);
        backIcon.setOnClickListener(new OnClickListener() {

            @Override
            public void onClick(View v) {
                // TODO Auto-generated method stub
                MusicUtils.canClosePlaylistItemFragment(getFragmentManager());
                mParentActivity.loadPreviousFragment();
            }
        });
        ImageView floatingpaly = (ImageView) rootView.findViewById(R.id.imageView4);
        floatingpaly.setOnClickListener(new OnClickListener() {

            @Override
            public void onClick(View v) {
                // TODO Auto-generated method stub
                repeatPlay();
            }
        });
        ImageView menuOverflow = (ImageView) rootView.findViewById(R.id.menu_overflow);
        mParentActivity.setTouchDelegate(menuOverflow);
        menuOverflow.setOnClickListener(new OnClickListener() {

            @Override
            public void onClick(View v) {
                // TODO Auto-generated method stub
                if (mPopupMenu != null) {
                    mPopupMenu.dismiss();
                }
                if (mAlbumId != null) {
                    mSelectedId = Long.valueOf(mAlbumId);
                } else {
                    mSelectedId = Long.valueOf(mArtistId);
                }
                PopupMenu popup = new PopupMenu(mParentActivity, v);
                popup.getMenu().add(0, PLAY_SELECTION, 0, R.string.play_selection);
                mSub = popup.getMenu().addSubMenu(0, ADD_TO_PLAYLIST, 0, R.string.add_to_playlist);
                MusicUtils.makePlaylistMenu(mParentActivity, mSub);
                if (mEditMode) {
                    popup.getMenu().add(0, REMOVE, 0, R.string.remove_from_playlist);
                }

                popup.getMenu().add(0, DELETE_ITEM, 0, R.string.delete_item);


                popup.setOnMenuItemClickListener(new PopupMenu.OnMenuItemClickListener() {
                    public boolean onMenuItemClick(MenuItem item) {
                        onOptionsItemSelected(item);
                        return true;
                    }
                });

                popup.show();
                mPopupMenu = popup;
            }
        });
        mTrackList = getListView();
        mTrackList.setCacheColorHint(0);
        mTrackList.setDividerHeight(0);
        if (mEditMode) {
            ((TouchInterceptor) mTrackList).setDropListener(mDropListener);
            ((TouchInterceptor) mTrackList).setRemoveListener(mRemoveListener);
            ((TouchInterceptor) mTrackList).registerContentObserver(mParentActivity.getApplicationContext());
            mTrackList.setDivider(null);
            mTrackList.setSelector(R.drawable.list_selector_background);
        } else {
            mTrackList.setTextFilterEnabled(true);
        }
        if (mAdapter != null) {
            mAdapter.setActivity(this);
            setListAdapter(mAdapter);
        }
        //attaching listview listener
        mTrackList.setOnItemClickListener(this);
        // don't set the album art until after the view has been layed out
        if (mGetArtworkAsyncTask != null
                && mGetArtworkAsyncTask.getStatus() != AsyncTask.Status.FINISHED) {
            mGetArtworkAsyncTask.cancel(true);
        }
        mGetArtworkAsyncTask = new GetArtworkAsyncTask();
        mGetArtworkAsyncTask.execute();
        container.removeAllViews();
        container.addView(rootView);
    }

    private AsyncTask<Void, Void, Bitmap> mGetArtworkAsyncTask = null;

    private class GetArtworkAsyncTask extends AsyncTask<Void, Void, Bitmap> {

        @Override
        protected Bitmap doInBackground(Void... params) {
            return getArtwork();
        }

        @Override
        protected void onPostExecute(Bitmap bitmap) {
            if (!isCancelled()) {
                setAlbumArtBackground(bitmap);
            }
        }
    }

    private Bitmap getArtwork() {
        if (!mEditMode) {
            long albumid = Long.valueOf(mAlbumId);
            Bitmap bm = MusicUtils.getArtwork(mParentActivity,
                    -1, albumid, false);
            return bm;
        }
        return null;
    }

    private void setAlbumArtBackground(Bitmap bm) {
        if (!mEditMode) {
            try {
                if (bm != null) {
                    mImageView.setImageBitmap(bm);
                    MusicUtils.setBackground(mImageView, bm);
                    mTrackList.setCacheColorHint(0);
                    return;
                } else {
                    mImageView.setImageResource(R.drawable.album_cover_background);
                }
            } catch (Exception ex) {
            }
        }
        //  mTrackList.setBackgroundColor(0xff000000);
        if (mTrackList != null) {
            mTrackList.setCacheColorHint(0);
        }
    }

    public void onServiceConnected(ComponentName name, IBinder service)
    {
        IntentFilter f = new IntentFilter();
        f.addAction(Intent.ACTION_MEDIA_SCANNER_STARTED);
        f.addAction(Intent.ACTION_MEDIA_SCANNER_FINISHED);
        f.addAction(Intent.ACTION_MEDIA_UNMOUNTED);
        f.addDataScheme("file");
        mParentActivity.registerReceiver(mScanListener, f);

        if (mAdapter == null) {
            //Log.i("@@@", "starting query");
             mAdapter = new TrackListAdapter(
                    mParentActivity.getApplication(), // need to use application context to avoid leaks
                    this,
                    R.layout.track_list_item_common1,
                    null, // cursor
                    new String[] {},
                    new int[] {},
                    "nowplaying".equals(mPlaylist),
                    mPlaylist != null &&
                    !(mPlaylist.equals("podcasts") || mPlaylist.equals("recentlyadded")));
            setListAdapter(mAdapter);
            mParentActivity.setTitle(R.string.working_songs);
            getTrackCursor(mAdapter.getQueryHandler(), null, true);
        } else {
            mTrackCursor = mAdapter.getCursor();
            // If mTrackCursor is null, this can be because it doesn't have
            // a cursor yet (because the initial query that sets its cursor
            // is still in progress), or because the query failed.
            // In order to not flash the error dialog at the user for the
            // first case, simply retry the query when the cursor is null.
            // Worst case, we end up doing the same query twice.
            if (mTrackCursor != null) {
                init(mTrackCursor, false);
            } else {
                mParentActivity.setTitle(R.string.working_songs);
                getTrackCursor(mAdapter.getQueryHandler(), null, true);
            }
        }
        mParentActivity.updateNowPlaying(getParentActivity());
    }

    public void onServiceDisconnected(ComponentName name) {
        // we can't really function without the service, so don't


        mParentActivity.finish();
    }

/*    @Override
    public Object onRetainNonConfigurationInstance() {
        TrackListAdapter a = mAdapter;
        mAdapterSent = true;
        return a;
    }*/

    @Override
    public void onDetach() {
        // TODO Auto-generated method stub
        super.onDetach();
        mParentActivity = null;
    }

    @Override
    public void onDestroy() {
        if (mPopupMenu != null ) {
            mPopupMenu.dismiss();
        }
        if (mSub != null) {
            mSub.close();
        }
        ListView lv = getListView();
        if (lv != null) {
            if (mUseLastListPos) {
                mLastListPosCourse = lv.getFirstVisiblePosition();
                View cv = lv.getChildAt(0);
                if (cv != null) {
                    mLastListPosFine = cv.getTop();
                }
            }
            if (mEditMode) {
                // clear the listeners so we won't get any more callbacks
                ((TouchInterceptor) lv).setDropListener(null);
                ((TouchInterceptor) lv).setRemoveListener(null);
                ((TouchInterceptor) lv).unregisterContentObserver(mParentActivity.getApplicationContext());
            }
        }

        MusicUtils.unbindFromService(mToken);
        try {
            if ("nowplaying".equals(mPlaylist)) {
                unregisterReceiverSafe(mNowPlayingListener);
            }
        } catch (IllegalArgumentException ex) {
            // we end up here in case we never registered the listeners
        }

        // If we have an adapter and didn't send it off to another activity yet, we should
        // close its cursor, which we do by assigning a null cursor to it. Doing this
        // instead of closing the cursor directly keeps the framework from accessing
        // the closed cursor later.
        if (!mAdapterSent && mAdapter != null) {
            mAdapter.changeCursor(null);
        }
        if (mTrackCursor != null) {
            mTrackCursor.close();
            mTrackCursor = null;
        }
        // Because we pass the adapter to the next activity, we need to make
        // sure it doesn't keep a reference to this activity. We can do this
        // by clearing its DatasetObservers, which setListAdapter(null) does.
        setListAdapter(null);
        mAdapter = null;
        unregisterReceiverSafe(mScanListener);
        super.onDestroy();
    }

    /**
     * Unregister a receiver, but eat the exception that is thrown if the
     * receiver was never registered to begin with. This is a little easier
     * than keeping track of whether the receivers have actually been
     * registered by the time onDestroy() is called.
     */
    private void unregisterReceiverSafe(BroadcastReceiver receiver) {
        try {
            mParentActivity.unregisterReceiver(receiver);
        } catch (IllegalArgumentException e) {
            // ignore
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        mPause = false;
        if (mTrackCursor != null) {
            getListView().invalidateViews();
        }
        MusicUtils.setSpinnerState(mParentActivity);
        if (mAlbumId != null && mTrackCursor != null){
            if (mTrackCursor.getCount() == 0){
                mParentActivity.setResult(mParentActivity.RESULT_OK);
                MusicUtils.canClosePlaylistItemFragment(getFragmentManager());
                mParentActivity.loadPreviousFragment();
            }
        }
        IntentFilter stateIntentfilter = new IntentFilter();
        stateIntentfilter.addAction(MediaPlaybackService.PLAYSTATE_CHANGED);
        mParentActivity.registerReceiver(mStatusListener, stateIntentfilter);
        mParentActivity.updateNowPlaying(mParentActivity);
    }
    @Override
    public void onPause() {
        mReScanHandler.removeCallbacksAndMessages(null);
        mParentActivity.unregisterReceiver(mStatusListener);
        mPause = true;
        if (mAnimView != null) mAnimView.animate(false);
        super.onPause();
    }

    /*
     * This listener gets called when the media scanner starts up or finishes, and
     * when the sd card is unmounted.
     */
    private BroadcastReceiver mScanListener = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (Intent.ACTION_MEDIA_SCANNER_STARTED.equals(action) ||
                    Intent.ACTION_MEDIA_SCANNER_FINISHED.equals(action)) {
                MusicUtils.setSpinnerState(mParentActivity);
            }
            mReScanHandler.sendEmptyMessage(0);
        }
    };

    // Receiver of PLAYSTATE_CHANGED to set the icon of play state
    private BroadcastReceiver mStatusListener = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (action.equals(MediaPlaybackService.PLAYSTATE_CHANGED)) {
                if (null != mAdapter)
                    getTrackCursor(mAdapter.getQueryHandler(), null,true);
                mParentActivity.updateNowPlaying(mParentActivity);
            }
        }
    };

    private Handler mReScanHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            if (mAdapter != null) {
                getTrackCursor(mAdapter.getQueryHandler(), null, true);
            }
            // if the query results in a null cursor, onQueryComplete() will
            // call init(), which will post a delayed message to this handler
            // in order to try again.
        }
    };

    public void onSaveInstanceState(Bundle outcicle) {
        // need to store the selected item so we don't lose it in case
        // of an orientation switch. Otherwise we could lose it while
        // in the middle of specifying a playlist to add the item to.
        outcicle.putLong("selectedtrack", mSelectedId);
        outcicle.putString("artist", mArtistId);
        outcicle.putString("album", mAlbumId);
        outcicle.putString("playlist", mPlaylist);
        outcicle.putString("genre", mGenre);
        outcicle.putBoolean("editmode", mEditMode);
        super.onSaveInstanceState(outcicle);
        //workaround to fix the illegal state exception.Put sth in outState after
        //super.onSaveInstanceState(outcicle), it will set mStateSaved(in
        //fragment manager) to false.
        if (outcicle.isEmpty()) {
            Log.d(LOGTAG, "Workaround fix");
            outcicle.putBoolean("bug:fix", true);
        }
    }

    public void init(Cursor newCursor, boolean isLimited) {
         if (mAdapter == null) {
            return;
        }
        mAdapter.changeCursor(newCursor); // also sets mTrackCursor

        if (mTrackCursor == null) {
            MusicUtils.displayDatabaseError(mParentActivity);
            mReScanHandler.sendEmptyMessageDelayed(0, 1000);
            return;
        }

        MusicUtils.hideDatabaseError(mParentActivity);
        setTitle();

        // Restore previous position
        if (mLastListPosCourse >= 0 && mUseLastListPos) {
            ListView lv = getListView();
            // this hack is needed because otherwise the position doesn't change
            // for the 2nd (non-limited) cursor
            lv.setAdapter(lv.getAdapter());
            lv.setSelectionFromTop(mLastListPosCourse, mLastListPosFine);
            if (!isLimited) {
                mLastListPosCourse = -1;
            }
        }

        // When showing the queue, position the selection on the currently playing track
        // Otherwise, position the selection on the first matching artist, if any
        IntentFilter f = new IntentFilter();
        f.addAction(MediaPlaybackService.META_CHANGED);
        f.addAction(MediaPlaybackService.QUEUE_CHANGED);
        if ("nowplaying".equals(mPlaylist)) {
            try {
                int cur = MusicUtils.sService.getQueuePosition();
                getListView().setSelection(cur);
                mParentActivity.registerReceiver(mNowPlayingListener, new IntentFilter(f));
                mNowPlayingListener.onReceive(mParentActivity,
                                        new Intent(MediaPlaybackService.META_CHANGED));
            } catch (RemoteException ex) {
            }
        } else {
            String key = mParentActivity.getIntent().getStringExtra("artist");
            if (key != null) {
                int keyidx = mTrackCursor.getColumnIndexOrThrow(MediaStore.Audio.Media.ARTIST_ID);
                mTrackCursor.moveToFirst();
                while (! mTrackCursor.isAfterLast()) {
                    String artist = mTrackCursor.getString(keyidx);
                    if (artist.equals(key)) {
                        getListView().setSelection(mTrackCursor.getPosition());
                        break;
                    }
                    mTrackCursor.moveToNext();
                }
            }
        }
    }



    private void setTitle() {

        CharSequence fancyName = null;
        if (mAlbumId != null) {
            int numresults = mTrackCursor != null ? mTrackCursor.getCount() : 0;
            if (numresults > 0) {
                mTrackCursor.moveToFirst();
                int idx = mTrackCursor.getColumnIndexOrThrow(MediaStore.Audio.Media.ALBUM);
                fancyName = mTrackCursor.getString(idx);
                // For compilation albums show only the album title,
                // but for regular albums show "artist - album".
                // To determine whether something is a compilation
                // album, do a query for the artist + album of the
                // first item, and see if it returns the same number
                // of results as the album query.
                String where = MediaStore.Audio.Media.ALBUM_ID + "='" + mAlbumId +
                        "' AND " + MediaStore.Audio.Media.ARTIST_ID + "=" +
                        mTrackCursor.getLong(mTrackCursor.getColumnIndexOrThrow(
                                MediaStore.Audio.Media.ARTIST_ID));
                Cursor cursor = MusicUtils.query(mParentActivity,
                                                    MediaStore.Audio.Media.EXTERNAL_CONTENT_URI,
                                                    new String[] {MediaStore.Audio.Media.ALBUM},
                                                    where, null, null);
                if (cursor != null) {
                    if (cursor.getCount() != numresults) {
                        // compilation album
                        fancyName = mTrackCursor.getString(idx);
                    }
                    cursor.close();
                }
            } else if (mRootPath != null) {
                fancyName = mRootPath;
                if (fancyName == null || fancyName.equals(MediaStore.UNKNOWN_STRING)) {
                    fancyName = getString(R.string.unknown_album_name);
                }
            }
        } else if (mPlaylist != null) {
            if (mPlaylist.equals("nowplaying")) {
                if (MusicUtils.getCurrentShuffleMode() == MediaPlaybackService.SHUFFLE_AUTO) {
                    fancyName = getText(R.string.partyshuffle_title);
                } else {
                    fancyName = getText(R.string.nowplaying_title);
                }
            } else if (mPlaylist.equals("podcasts")){
                fancyName = getText(R.string.podcasts_title);
            } else if (mPlaylist.equals("recentlyadded")){
                fancyName = getText(R.string.recentlyadded_title);
            } else {
                String [] cols = new String [] {
                MediaStore.Audio.Playlists.NAME
                };
                Cursor cursor = MusicUtils.query(mParentActivity,
                        ContentUris.withAppendedId(Playlists.EXTERNAL_CONTENT_URI, Long.valueOf(mPlaylist)),
                        cols, null, null, null);
                if (cursor != null) {
                    if (cursor.getCount() != 0) {
                        cursor.moveToFirst();
                        fancyName = cursor.getString(0);
                    }
                    cursor.close();
                }
            }
        } else if (mGenre != null) {
            String [] cols = new String [] {
            MediaStore.Audio.Genres.NAME
            };
            Cursor cursor = MusicUtils.query(mParentActivity,
                                ContentUris.withAppendedId(MediaStore.Audio.Genres.EXTERNAL_CONTENT_URI,
                                    Long.valueOf(mGenre)),
                                cols, null, null, null);
            if (cursor != null) {
                if (cursor.getCount() != 0) {
                    cursor.moveToFirst();
                    fancyName = cursor.getString(0);
                }
                cursor.close();
            }
        }

        if (fancyName != null) {
            if ("My recordings".equals(fancyName)) {
                mParentActivity.setTitle(R.string.audio_db_playlist_name);
            } else {
                mParentActivity.setTitle(fancyName);
            }
        } else {
            mParentActivity.setTitle(R.string.tracks_title);
        }
    }

    private TouchInterceptor.DropListener mDropListener =
        new TouchInterceptor.DropListener() {
        public void drop(int from, int to) {
            if (mTrackCursor instanceof NowPlayingCursor) {
                // update the currently playing list
                NowPlayingCursor c = (NowPlayingCursor) mTrackCursor;
                c.moveItem(from, to);
                ((TrackListAdapter)getListView().getAdapter()).notifyDataSetChanged();
                getListView().invalidateViews();
                mDeletedOneRow = true;
            } else {
                // update a saved playlist
                MediaStore.Audio.Playlists.Members.moveItem(mParentActivity.getContentResolver(),
                        Long.valueOf(mPlaylist), from, to);
            }
        }
    };

    private TouchInterceptor.RemoveListener mRemoveListener =
        new TouchInterceptor.RemoveListener() {
        public void remove(int which) {
            removePlaylistItem(which);
        }
    };

    private void removePlaylistItem(int which) {
        View v = mTrackList.getChildAt(which - mTrackList.getFirstVisiblePosition());
        if (v == null) {
            Log.d(LOGTAG, "No view when removing playlist item " + which);
            return;
        }
        try {
            if (MusicUtils.sService != null
                    && which != MusicUtils.sService.getQueuePosition()) {
                mDeletedOneRow = true;
            }
        } catch (RemoteException e) {
            // Service died, so nothing playing.
            mDeletedOneRow = true;
        }
        v.setVisibility(View.GONE);
        mTrackList.invalidateViews();
        if (mTrackCursor instanceof NowPlayingCursor) {
            ((NowPlayingCursor)mTrackCursor).removeItem(which);
        } else {
            int colidx = mTrackCursor.getColumnIndexOrThrow(
                    MediaStore.Audio.Playlists.Members._ID);
            mTrackCursor.moveToPosition(which);
            long id = mTrackCursor.getLong(colidx);
            Uri uri = MediaStore.Audio.Playlists.Members.getContentUri("external",
                    Long.valueOf(mPlaylist));
            mParentActivity.getContentResolver().delete(
                    ContentUris.withAppendedId(uri, id), null, null);
        }
        v.setVisibility(View.VISIBLE);
        if (mPopupMenu != null) {
            mPopupMenu.dismiss();
        }
        mTrackList.invalidateViews();
    }

    private BroadcastReceiver mNowPlayingListener = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (intent.getAction().equals(MediaPlaybackService.META_CHANGED)) {
                getListView().invalidateViews();
                mParentActivity.updateNowPlaying(mParentActivity);
            } else if (intent.getAction().equals(MediaPlaybackService.QUEUE_CHANGED)) {
                if (mDeletedOneRow) {
                    // This is the notification for a single row that was
                    // deleted previously, which is already reflected in
                    // the UI.
                    mDeletedOneRow = false;
                    return;
                }
                // The service could disappear while the broadcast was in flight,
                // so check to see if it's still valid
                if (MusicUtils.sService == null) {

                    mParentActivity.finish();
                    return;
                }
                if (mAdapter != null) {
                    Cursor c = new NowPlayingCursor(MusicUtils.sService, mCursorCols);
                    if (c.getCount() == 0) {

                        mParentActivity.finish();
                        return;
                    }
                    mAdapter.changeCursor(c);
                }
            }
        }
    };

    // Cursor should be positioned on the entry to be checked
    // Returns false if the entry matches the naming pattern used for recordings,
    // or if it is marked as not music in the database.
    private boolean isMusic(Cursor c) {
        int titleidx = c.getColumnIndex(MediaStore.Audio.Media.TITLE);
        int albumidx = c.getColumnIndex(MediaStore.Audio.Media.ALBUM);
        int artistidx = c.getColumnIndex(MediaStore.Audio.Media.ARTIST);

        String title = c.getString(titleidx);
        String album = c.getString(albumidx);
        String artist = c.getString(artistidx);
        if (MediaStore.UNKNOWN_STRING.equals(album) &&
                MediaStore.UNKNOWN_STRING.equals(artist) &&
                title != null &&
                title.startsWith("recording")) {
            // not music
            return false;
        }

        int ismusic_idx = c.getColumnIndex(MediaStore.Audio.Media.IS_MUSIC);
        boolean ismusic = true;
        if (ismusic_idx >= 0) {
            ismusic = mTrackCursor.getInt(ismusic_idx) != 0;
        }
        return ismusic;
    }

    private void onCreatePopupMenu(PopupMenu menu) {
        menu.getMenu().add(0, PLAY_SELECTION, 0, R.string.play_selection);
        mSub = menu.getMenu().addSubMenu(0, ADD_TO_PLAYLIST, 0, R.string.add_to_playlist);
        MusicUtils.makePlaylistMenu(mParentActivity, mSub);
        menu.getMenu().add(0, DELETE_ITEM, 0, R.string.delete_item);
        MusicUtils.addSetRingtonMenu(menu.getMenu());
        menu.getMenu().add(0, SHARE, 0, R.string.share);
        menu.getMenu().add(0, DETAILS, 0, R.string.details);
    }

    private boolean onContextItemSelected(MenuItem item, int position) {
        switch (item.getItemId()) {
            case PLAY_SELECTION: {
                // play the track
               // int position = mSelectedPosition;
                MusicUtils.playAll(mParentActivity, mTrackCursor, position);
                mAdapter.notifyDataSetChanged();
                return true;
            }

            case QUEUE: {
                long [] list = new long[] { mSelectedId };
                MusicUtils.addToCurrentPlaylist(mParentActivity, list);
                MusicUtils.addToPlaylist(mParentActivity, list, MusicUtils.getPlayListId());
                return true;
            }

            case NEW_PLAYLIST: {
                Intent intent = new Intent();
                intent.setClass(mParentActivity, CreatePlaylist.class);
                startActivityForResult(intent, NEW_PLAYLIST);
                return true;
            }

            case PLAYLIST_SELECTED: {
                long [] list = new long[] { mSelectedId };
                long playlist = item.getIntent().getLongExtra("playlist", 0);
                MusicUtils.addToPlaylist(mParentActivity, list, playlist);
                return true;
            }

            case USE_AS_RINGTONE:
                // Set the system setting to make this the current ringtone
                MusicUtils.setRingtone(mParentActivity, mSelectedId);
                return true;

            case USE_AS_RINGTONE_2:
                // Set the system setting to make this the current ringtone for SUB_1
                MusicUtils.setRingtone(mParentActivity, mSelectedId, MusicUtils.RINGTONE_SUB_1);
                return true;

            case DELETE_ITEM: {
                long [] list = new long[1];
                if (mSelectedId == Long.valueOf(mAlbumId)) {
                    list =  MusicUtils.getSongListForAlbum(
                            mParentActivity, mSelectedId);
                } else {
                    list = new long[] { mSelectedId };
                }
                Bundle b = new Bundle();
                String f = getString(R.string.delete_song_desc);
                String desc = String.format(f, mCurrentTrackName);
                b.putString("description", desc);
                b.putLongArray("items", list);
                Intent intent = new Intent();
                intent.setClass(mParentActivity, DeleteItems.class);
                intent.putExtras(b);
                startActivityForResult(intent, DELETE_ITEM);
                return true;
            }

            case REMOVE:
                removePlaylistItem(mSelectedPosition);
                return true;

            case DRM_LICENSE_INFO:
                String path = MusicUtils.getSelectAudioPath(mParentActivity.getApplicationContext(),
                                mSelectedId);
                path = path.replace("/storage/emulated/0", "/storage/emulated/legacy");
                Intent intent = new Intent("android.drmservice.intent.action.SHOW_PROPERTIES");
                intent.putExtra("DRM_FILE_PATH", path);
                intent.putExtra("DRM_TYPE", "OMAV1");
                Log.d(LOGTAG, "onContextItemSelected:------filepath===" + path);
                mParentActivity.sendBroadcast(intent);
                return true;

            case SEARCH:
                doSearch();
                return true;

            case DETAILS:
                showDetails();
                return true;
            case SHARE:
                // Send intent to share audio
                long id;
                Intent shareIntent = new Intent(Intent.ACTION_SEND);
                shareIntent.setType("audio/*");
                mTrackCursor.moveToPosition(position);
                if (mEditMode && !mPlaylist.equals("nowplaying")) {
                    id = mTrackCursor.getLong(
                                        mTrackCursor.getColumnIndexOrThrow(
                                                    MediaStore.Audio.Playlists.Members.AUDIO_ID));
                } else {
                    id = mTrackCursor.getLong(
                                        mTrackCursor.getColumnIndexOrThrow(MediaStore.Audio.Media._ID));
                }
                Uri uri = ContentUris.withAppendedId(
                                        MediaStore.Audio.Media.EXTERNAL_CONTENT_URI, id);

                boolean canBeShared = true;
                String filepath = null;
                String scheme = uri.getScheme();
                if ("file".equals(scheme)) {
                    filepath = uri.getPath();
                } else {
                    Cursor cursor = null;
                    try {
                        cursor = mParentActivity.getContentResolver().query(uri,
                        new String[] {VideoColumns.DATA}, null, null, null);
                        if (cursor != null && cursor.moveToNext()) {
                            filepath = cursor.getString(0);
                        }
                    } catch (Throwable t) {
                        Log.w(LOGTAG, "cannot get path from: " + uri);
                    } finally {
                        if (cursor != null) cursor.close();
                    }
                }

//TODO: DRM changes here.
//                if (filepath != null && (filepath.endsWith(".dcf") || filepath.endsWith(".dm"))) {
//                    DrmManagerClientWrapper drmClient = new DrmManagerClientWrapper(mParentActivity);
//                    ContentValues values = drmClient.getMetadata(filepath);
//                    int drmType = values.getAsInteger("DRM-TYPE");
//                    Log.d(LOGTAG, "SHARE:drmType returned= " + Integer.toString(drmType)
//                            + " for path= " + filepath);
//                    if (drmType != DrmDeliveryType.SEPARATE_DELIVERY) {
//                        canBeShared = false;
//                        Toast.makeText(mParentActivity,
//                                        R.string.no_permission_for_drm,Toast.LENGTH_LONG)
//                             .show();
//                        return true;
//                    } else {
//                        canBeShared = true;
//                    }
//                    if (drmClient != null) drmClient.release();
//                } else {
//                    canBeShared = true;
//                }

                shareIntent.putExtra(Intent.EXTRA_STREAM, uri);
                if (canBeShared) startActivity(shareIntent);
                return true;
        }
        return super.onContextItemSelected(item);
    }

    private void showDetails() {
        // TODO Auto-generated method stub
        AlertDialog.Builder mdetaildialog = new AlertDialog.Builder(this.getActivity());
        View detaillayout = LayoutInflater.from(this.getActivity()).inflate(R.layout.file_info,
                null);
        mdetaildialog.setTitle(R.string.details);
        mdetaildialog.setView(detaillayout);
        mdetaildialog.setPositiveButton(R.string.button_ok, new DialogInterface.OnClickListener() {
            public void onClick(DialogInterface dialoginterface, int i) {
                dialoginterface.cancel();
            }
        });
        fillData(detaillayout);
        mdetaildialog.show();

    }

    private void fillData(View contentView) {
        // TODO Auto-generated method stub
        TextView songName = (TextView) contentView.findViewById(R.id.song_name);
        TextView albumName = (TextView) contentView.findViewById(R.id.album_name);
        TextView artistName = (TextView) contentView.findViewById(R.id.artist_name);
        TextView durationName = (TextView) contentView.findViewById(R.id.duration_name);
        TextView pathName = (TextView) contentView.findViewById(R.id.path_name);
        TextView sizeName = (TextView) contentView.findViewById(R.id.size_name);
        songName.setText(mCurrentTrackName);
        albumName.setText(mCurrentAlbumName);
        artistName.setText(mCurrentArtistNameForAlbum);
        durationName.setText(mCurrentTrackDuration);
        pathName.setText(mCurrentTrackPath);
        sizeName.setText(mCurrentTrackSize);
    }

    void doSearch() {
        CharSequence title = null;
        String query = null;

        Intent i = new Intent();
        i.setAction(MediaStore.INTENT_ACTION_MEDIA_SEARCH);
        i.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        title = mCurrentTrackName;
        if (MediaStore.UNKNOWN_STRING.equals(mCurrentArtistNameForAlbum)) {
            query = mCurrentTrackName;
        } else {
            query = mCurrentArtistNameForAlbum + " " + mCurrentTrackName;
            i.putExtra(MediaStore.EXTRA_MEDIA_ARTIST, mCurrentArtistNameForAlbum);
        }
        if (MediaStore.UNKNOWN_STRING.equals(mCurrentAlbumName)) {
            i.putExtra(MediaStore.EXTRA_MEDIA_ALBUM, mCurrentAlbumName);
        }
        i.putExtra(MediaStore.EXTRA_MEDIA_FOCUS, "audio/*");
        title = getString(R.string.mediasearch, title);
        i.putExtra(SearchManager.QUERY, query);

        startActivity(Intent.createChooser(i, title));
    }

    private void removeItem() {
        int curcount = mTrackCursor != null ? mTrackCursor.getCount() : 0;
        int curpos = mTrackList.getSelectedItemPosition();
        if (curcount == 0 || curpos < 0) {
            return;
        }

        if ("nowplaying".equals(mPlaylist)) {
            // remove track from queue

            // Work around bug 902971. To get quick visual feedback
            // of the deletion of the item, hide the selected view.
            try {
                if (curpos != MusicUtils.sService.getQueuePosition()) {
                    mDeletedOneRow = true;
                }
            } catch (RemoteException ex) {
            }
            View v = mTrackList.getSelectedView();
            v.setVisibility(View.GONE);
            mTrackList.invalidateViews();
            ((NowPlayingCursor)mTrackCursor).removeItem(curpos);
            v.setVisibility(View.VISIBLE);
            mTrackList.invalidateViews();
        } else {
            // remove track from playlist
            int colidx = mTrackCursor.getColumnIndexOrThrow(
                    MediaStore.Audio.Playlists.Members._ID);
            mTrackCursor.moveToPosition(curpos);
            long id = mTrackCursor.getLong(colidx);
            Uri uri = MediaStore.Audio.Playlists.Members.getContentUri("external",
                    Long.valueOf(mPlaylist));
            mParentActivity.getContentResolver().delete(
                    ContentUris.withAppendedId(uri, id), null, null);
            curcount--;
            if (curcount == 0) {
                mParentActivity.finish();
            } else {
                mTrackList.setSelection(curpos < curcount ? curpos : curcount);
            }
        }
    }

    private void moveItem(boolean up) {
        int curcount = mTrackCursor != null ? mTrackCursor.getCount() : 0;
        int curpos = mTrackList.getSelectedItemPosition();
        if ( (up && curpos < 1) || (!up  && curpos >= curcount - 1)) {
            return;
        }

        if (mTrackCursor instanceof NowPlayingCursor) {
            NowPlayingCursor c = (NowPlayingCursor) mTrackCursor;
            c.moveItem(curpos, up ? curpos - 1 : curpos + 1);
            ((TrackListAdapter)getListView().getAdapter()).notifyDataSetChanged();
            getListView().invalidateViews();
            mDeletedOneRow = true;
            if (up) {
                mTrackList.setSelection(curpos - 1);
            } else {
                mTrackList.setSelection(curpos + 1);
            }
        } else {
            int colidx = mTrackCursor.getColumnIndexOrThrow(
                    MediaStore.Audio.Playlists.Members.PLAY_ORDER);
            mTrackCursor.moveToPosition(curpos);
            int currentplayidx = mTrackCursor.getInt(colidx);
            Uri baseUri = MediaStore.Audio.Playlists.Members.getContentUri("external",
                    Long.valueOf(mPlaylist));
            ContentValues values = new ContentValues();
            String where = MediaStore.Audio.Playlists.Members._ID + "=?";
            String [] wherearg = new String[1];
            ContentResolver res = mParentActivity.getContentResolver();
            if (up) {
                values.put(MediaStore.Audio.Playlists.Members.PLAY_ORDER, currentplayidx - 1);
                wherearg[0] = mTrackCursor.getString(0);
                res.update(baseUri, values, where, wherearg);
                mTrackCursor.moveToPrevious();
            } else {
                values.put(MediaStore.Audio.Playlists.Members.PLAY_ORDER, currentplayidx + 1);
                wherearg[0] = mTrackCursor.getString(0);
                res.update(baseUri, values, where, wherearg);
                mTrackCursor.moveToNext();
            }
            values.put(MediaStore.Audio.Playlists.Members.PLAY_ORDER, currentplayidx);
            wherearg[0] = mTrackCursor.getString(0);
            res.update(baseUri, values, where, wherearg);
        }
    }

    View prevV;

    protected void onListItemClick(ListView l, View v, int position, long id)
    {

        ViewHolder vh = (ViewHolder)v.getTag();
        if ((mTrackCursor == null) || (mTrackCursor.getCount() == 0)) {
            return;
        }

        long [] list = MusicUtils.getSongListForCursor(mTrackCursor);
        long songid = list[position];
        String sUri = MediaStore.Audio.Media.EXTERNAL_CONTENT_URI + "/" + songid;
        String path = null;
        String mime = null;
        final String[] ccols = new String[] { MediaStore.Audio.Media.DATA, MediaStore.Audio.Media.MIME_TYPE };
        String where = MediaStore.Audio.Media._ID + "='" + songid + "'";
        ContentResolver resolver = mParentActivity.getApplicationContext().getContentResolver();
        Cursor cursor = resolver.query(MediaStore.Audio.Media.EXTERNAL_CONTENT_URI, ccols, where, null, null);
        if (null != cursor) {
            if (0 != cursor.getCount()) {
                cursor.moveToFirst();
                path = cursor.getString(cursor.getColumnIndexOrThrow(MediaStore.Audio.Media.DATA));
                mime = cursor.getString(cursor.getColumnIndexOrThrow(MediaStore.Audio.Media.MIME_TYPE));
           }
           cursor.close();
        }
        //TODO: DRM changes here.
//        Log.d(LOGTAG, "onListItemClick:path = " + path);
//        if (path.endsWith(".dcf") || path.endsWith(".dm")) {
//            DrmManagerClientWrapper drmClient = new DrmManagerClientWrapper(mParentActivity);
//            path = path.replace("/storage/emulated/0", "/storage/emulated/legacy");
//            int status = drmClient.checkRightsStatus(path, Action.PLAY);
//            Log.d(LOGTAG, "onListItemClick:status from checkRightsStatus is " + Integer.toString(status));
//            if (RightsStatus.RIGHTS_VALID != status) {
//                ContentValues values = drmClient.getMetadata(path);
//                String address = values.getAsString("Rights-Issuer");
//                Log.d(LOGTAG, "onListItemClick:address = " + address);
//                Intent intent = new Intent(BUY_LICENSE);
//                intent.putExtra("DRM_FILE_PATH", address);
//                mParentActivity.sendBroadcast(intent);
//                return;
//            }
//
//            if (drmClient != null) drmClient.release();
//        }

        // When selecting a track from the queue, just jump there instead of
        // reloading the queue. This is both faster, and prevents accidentally
        // dropping out of party shuffle.
        if (mTrackCursor instanceof NowPlayingCursor) {
            if (MusicUtils.sService != null) {
                try {
                    MusicUtils.sService.setQueuePosition(position);
                    return;
                } catch (RemoteException ex) {
                }
            }
        }

        if (mEditMode && !mPlaylist.equals("nowplaying")) {
            MusicUtils.setPlayListId(Long.valueOf(mPlaylist));
        }
        if(prevV!=null)
        {
            ViewHolder vh1 = (ViewHolder) prevV.getTag();
            vh1.anim_icon.setVisibility(View.INVISIBLE);

        }
        MusicUtils.playAll(mParentActivity, mTrackCursor, position);
        vh.anim_icon.setVisibility(View.VISIBLE);
        prevV= v;
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent intent) {
        switch (requestCode) {
        case SCAN_DONE:
            if (resultCode == mParentActivity.RESULT_CANCELED) {
                mParentActivity.finish();
            } else {
                getTrackCursor(mAdapter.getQueryHandler(), null, true);
            }
            break;

        case NEW_PLAYLIST:
            if (resultCode == mParentActivity.RESULT_OK) {
                Uri uri = intent.getData();
                if (uri != null) {
                    long[] list;
                    try {
                        if (mSelectedId == Long.valueOf(mAlbumId)) {
                            list =  MusicUtils.getSongListForAlbum(
                                    mParentActivity, mSelectedId);
                        } else {
                            list = new long[] { mSelectedId };
                        }
                        MusicUtils.addToPlaylist(mParentActivity, list,
                                Integer.valueOf(uri.getLastPathSegment()));
                    } catch (NumberFormatException execption) {
                        Log.w(TAG,"Invalid albumID "+ mAlbumId);
                    }
                }
            }
            break;

        case SAVE_AS_PLAYLIST:
            if (resultCode == mParentActivity.RESULT_OK) {
                Uri uri = intent.getData();
                if (uri != null) {
                    long[] list = MusicUtils.getSongListForCursor(mTrackCursor);
                    int plid = Integer.parseInt(uri.getLastPathSegment());
                    MusicUtils.addToPlaylist(mParentActivity, list, plid);
                }
            }
            break;
        case DELETE_ITEM:
            mTrackList.setAdapter(mTrackList.getAdapter());
            break;
        }
    }

    private boolean onCreateOptionsMenu(PopupMenu menu) {
         /*This activity is used for a number of different browsing modes, and the menu can
         * be different for each of them:
         * - all tracks, optionally restricted to an album, artist or playlist
         * - the list of currently playing songs*/
        if (mPlaylist == null) {
            menu.getMenu().add(0, PLAY_ALL, 0, R.string.play_all).setIcon(R.drawable.ic_menu_play_clip);
        }
        // icon will be set in onPrepareOptionsMenu()
        menu.getMenu().add(0, PARTY_SHUFFLE, 0, R.string.party_shuffle);
        menu.getMenu().add(0, SHUFFLE_ALL, 0, R.string.shuffle_all).setIcon(R.drawable.ic_menu_shuffle);
        if (mPlaylist != null) {
            menu.getMenu().add(0, SAVE_AS_PLAYLIST, 0,
                            R.string.save_as_playlist).setIcon(android.R.drawable.ic_menu_save);
            if (!mPlaylist.equals("recentlyadded")) {
                menu.getMenu().add(0, CLEAR_PLAYLIST, 0,
                                R.string.clear_playlist).setIcon(R.drawable.ic_menu_clear_playlist);
            }
        }
        menu.getMenu().add(0, CLOSE, 0, R.string.close_music).setIcon(R.drawable.quick_panel_music_close);
        if (getResources().getBoolean(R.bool.def_music_add_more_video_enabled))
            menu.getMenu().add(0, MORE_MUSIC, 0, R.string.more_music).setIcon(
                    R.drawable.ic_menu_music_library);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        Intent intent;
        Cursor cursor;
        switch (item.getItemId()) {
            case PLAY_SELECTION: {
            // play the track
                MusicUtils.playAll(mParentActivity, mTrackCursor, 0);
                mAdapter.notifyDataSetChanged();
                return true;
            }
            case PLAY_ALL: {
                MusicUtils.playAll(mParentActivity, mTrackCursor);
                return true;
            }

            case QUEUE: {
                long [] list =  MusicUtils.getSongListForAlbum(
                        mParentActivity, mSelectedId);
                MusicUtils.addToCurrentPlaylist(mParentActivity, list);
                MusicUtils.addToPlaylist(mParentActivity, list, MusicUtils.getPlayListId());
                return true;
            }

            case NEW_PLAYLIST: {
                intent = new Intent();
                intent.setClass(mParentActivity, CreatePlaylist.class);
                startActivityForResult(intent, NEW_PLAYLIST);
                return true;
            }

            case PLAYLIST_SELECTED: {
                long [] list =  MusicUtils.getSongListForAlbum(
                        mParentActivity, mSelectedId);;
                long playlist = item.getIntent().getLongExtra("playlist", 0);
                MusicUtils.addToPlaylist(mParentActivity, list, playlist);
                return true;
            }

            case DELETE_ITEM: {
                long [] list = new long[1];
                if (mSelectedId == Long.valueOf(mAlbumId)) {
                    list =  MusicUtils.getSongListForAlbum(
                            mParentActivity, mSelectedId);
                } else {
                    list = new long[] { mSelectedId };
                }
                Bundle b = new Bundle();
                String f = getString(R.string.delete_song_desc);
                String desc = String.format(f, mCurrentAlbumName);
                b.putString("description", desc);
                b.putLongArray("items", list);
                intent = new Intent();
                intent.setClass(mParentActivity, DeleteItems.class);
                intent.putExtras(b);
                startActivityForResult(intent, DELETE_ITEM);
                return true;
            }

            case SAVE_AS_PLAYLIST:
                intent = new Intent();
                intent.setClass(mParentActivity, CreatePlaylist.class);
                startActivityForResult(intent, SAVE_AS_PLAYLIST);
                return true;

        }
        return super.onOptionsItemSelected(item);
    }

 /*   @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent intent) {
        switch (requestCode) {
            case SCAN_DONE:
                if (resultCode == RESULT_CANCELED) {
                    finish();
                } else {
                    getTrackCursor(mAdapter.getQueryHandler(), null, true);
                }
                break;

            case NEW_PLAYLIST:
                if (resultCode == RESULT_OK) {
                    Uri uri = intent.getData();
                    if (uri != null) {
                        long [] list = new long[] { mSelectedId };
                        MusicUtils.addToPlaylist(this, list, Integer.valueOf(uri.getLastPathSegment()));
                    }
                }
                break;

            case SAVE_AS_PLAYLIST:
                if (resultCode == RESULT_OK) {
                    Uri uri = intent.getData();
                    if (uri != null) {
                        long [] list = MusicUtils.getSongListForCursor(mTrackCursor);
                        int plid = Integer.parseInt(uri.getLastPathSegment());
                        MusicUtils.addToPlaylist(this, list, plid);
                    }
                }
                break;
            case DELETE_ITEM:
                ListView lv = getListView();
                lv.setAdapter(lv.getAdapter());
                break;
        }
    }*/

    private Cursor getTrackCursor(TrackListAdapter.TrackQueryHandler queryhandler, String filter,
            boolean async) {

        if (queryhandler == null) {
            throw new IllegalArgumentException();
        }

        Cursor ret = null;
        mSortOrder = MediaStore.Audio.Media.TITLE_KEY;
        StringBuilder where = new StringBuilder();
        where.append(MediaStore.Audio.Media.TITLE + " != ''");

        if (mGenre != null) {
            Uri uri = MediaStore.Audio.Genres.Members.getContentUri("external",
                    Integer.valueOf(mGenre));
            if (!TextUtils.isEmpty(filter)) {
                uri = uri.buildUpon().appendQueryParameter("filter", Uri.encode(filter)).build();
            }
            mSortOrder = MediaStore.Audio.Genres.Members.DEFAULT_SORT_ORDER;
            ret = queryhandler.doQuery(uri,
                    mCursorCols, where.toString(), null, mSortOrder, async);
        } else if (mPlaylist != null) {
            if (mPlaylist.equals("nowplaying")) {
                if (MusicUtils.sService != null) {
                    ret = new NowPlayingCursor(MusicUtils.sService, mCursorCols);
                    if (ret.getCount() == 0) {

                        mParentActivity.finish();
                    }
                } else {
                    // Nothing is playing.
                }
            } else if (mPlaylist.equals("podcasts")) {
                where.append(" AND " + MediaStore.Audio.Media.IS_PODCAST + "=1");
                Uri uri = MediaStore.Audio.Media.EXTERNAL_CONTENT_URI;
                if (!TextUtils.isEmpty(filter)) {
                    uri = uri.buildUpon().appendQueryParameter("filter", Uri.encode(filter)).build();
                }
                ret = queryhandler.doQuery(uri,
                        mCursorCols, where.toString(), null,
                        MediaStore.Audio.Media.DEFAULT_SORT_ORDER, async);
            } else if (mPlaylist.equals("recentlyadded")) {
                // do a query for all songs added in the last X weeks
                Uri uri = MediaStore.Audio.Media.EXTERNAL_CONTENT_URI;
                if (!TextUtils.isEmpty(filter)) {
                    uri = uri.buildUpon().appendQueryParameter("filter", Uri.encode(filter)).build();
                }
                int X = MusicUtils.getIntPref(mParentActivity, "numweeks", 2) * (3600 * 24 * 7);
                where.append(" AND " + MediaStore.MediaColumns.DATE_ADDED + ">");
                where.append(System.currentTimeMillis() / 1000 - X);
                ret = queryhandler.doQuery(uri,
                        mCursorCols, where.toString(), null,
                        MediaStore.Audio.Media.DEFAULT_SORT_ORDER, async);
            } else {
                Uri uri = MediaStore.Audio.Playlists.Members.getContentUri("external",
                        Long.valueOf(mPlaylist));
                if (!TextUtils.isEmpty(filter)) {
                    uri = uri.buildUpon().appendQueryParameter("filter", Uri.encode(filter)).build();
                }
                mSortOrder = MediaStore.Audio.Playlists.Members.DEFAULT_SORT_ORDER;
                ret = queryhandler.doQuery(uri, mPlaylistMemberCols,
                        where.toString(), null, mSortOrder, async);
            }
        } else if (MusicUtils.isGroupByFolder() && mParent >= 0) {
            String uriString = "content://media/external/audio/folder/" + mParent;
            Uri uri = Uri.parse(uriString);
            where.append(" AND " + MediaStore.Audio.Media.IS_MUSIC + "=1");
            ret = queryhandler.doQuery(uri,
                    null, where.toString(), null, mSortOrder, async);
            return ret;
        } else {
            if (mAlbumId != null) {
                where.append(" AND " + MediaStore.Audio.Media.ALBUM_ID + "=" + mAlbumId);
                mSortOrder = MediaStore.Audio.Media.TRACK + ", " + mSortOrder;
            }
            if (mArtistId != null) {
                where.append(" AND " + MediaStore.Audio.Media.ARTIST_ID + "=" + mArtistId);
            }
            where.append(" AND " + MediaStore.Audio.Media.IS_MUSIC + "=1");
            Uri uri = MediaStore.Audio.Media.EXTERNAL_CONTENT_URI;
            if (!TextUtils.isEmpty(filter)) {
                uri = uri.buildUpon().appendQueryParameter("filter", Uri.encode(filter)).build();
            }
            ret = queryhandler.doQuery(uri,
                    mCursorCols, where.toString() , null, mSortOrder, async);
        }

        // This special case is for the "nowplaying" cursor, which cannot be handled
        // asynchronously using AsyncQueryHandler, so we do some extra initialization here.
        if (ret != null && async) {
            init(ret, false);
            setTitle();
        }
        return ret;
    }

    private class NowPlayingCursor extends AbstractCursor
    {
        public NowPlayingCursor(IMediaPlaybackService service, String [] cols)
        {
            mCols = cols;
            mService  = service;
            makeNowPlayingCursor();
        }

        private void makeNowPlayingCursor() {
            if (mCurrentPlaylistCursor != null) {
                mCurrentPlaylistCursor.close();
            }
            mCurrentPlaylistCursor = null;
            try {
                mNowPlaying = mService.getQueue();
            } catch (RemoteException ex) {
                mNowPlaying = new long[0];
            }
            mSize = mNowPlaying.length;
            if (mSize == 0) {
                return;
            }

            StringBuilder where = new StringBuilder();
            where.append(MediaStore.Audio.Media._ID + " IN (");
            for (int i = 0; i < mSize; i++) {
                where.append(mNowPlaying[i]);
                if (i < mSize - 1) {
                    where.append(",");
                }
            }
            where.append(")");

            mCurrentPlaylistCursor = MusicUtils.query(mParentActivity,
                    MediaStore.Audio.Media.EXTERNAL_CONTENT_URI,
                    mCols, where.toString(), null, MediaStore.Audio.Media._ID);

            if (mCurrentPlaylistCursor == null) {
                mSize = 0;
                return;
            }

            int size = mCurrentPlaylistCursor.getCount();
            mCursorIdxs = new long[size];
            mCurrentPlaylistCursor.moveToFirst();
            int colidx = mCurrentPlaylistCursor.getColumnIndexOrThrow(MediaStore.Audio.Media._ID);
            for (int i = 0; i < size; i++) {
                mCursorIdxs[i] = mCurrentPlaylistCursor.getLong(colidx);
                mCurrentPlaylistCursor.moveToNext();
            }
            mCurrentPlaylistCursor.moveToFirst();
            mCurPos = -1;

            // At this point we can verify the 'now playing' list we got
            // earlier to make sure that all the items in there still exist
            // in the database, and remove those that aren't. This way we
            // don't get any blank items in the list.
            try {
                int removed = 0;
                for (int i = mNowPlaying.length - 1; i >= 0; i--) {
                    long trackid = mNowPlaying[i];
                    int crsridx = Arrays.binarySearch(mCursorIdxs, trackid);
                    if (crsridx < 0) {
                        //Log.i("@@@@@", "item no longer exists in db: " + trackid);
                        removed += mService.removeTrack(trackid);
                    }
                }
                if (removed > 0) {
                    mNowPlaying = mService.getQueue();
                    mSize = mNowPlaying.length;
                    if (mSize == 0) {
                        mCursorIdxs = null;
                        return;
                    }
                }
            } catch (RemoteException ex) {
                mNowPlaying = new long[0];
            }
        }

        @Override
        public int getCount()
        {
            return mSize;
        }

        @Override
        public boolean onMove(int oldPosition, int newPosition)
        {
            if (oldPosition == newPosition)
                return true;

            if (mNowPlaying == null || mCursorIdxs == null || newPosition >= mNowPlaying.length) {
                return false;
            }

            // The cursor doesn't have any duplicates in it, and is not ordered
            // in queue-order, so we need to figure out where in the cursor we
            // should be.

            long newid = mNowPlaying[newPosition];
            int crsridx = Arrays.binarySearch(mCursorIdxs, newid);
            mCurrentPlaylistCursor.moveToPosition(crsridx);
            mCurPos = newPosition;

            return true;
        }

        public boolean removeItem(int which)
        {
            try {
                if (mService.removeTracks(which, which) == 0) {
                    return false; // delete failed
                }
                int i = (int) which;
                mSize--;
                while (i < mSize) {
                    mNowPlaying[i] = mNowPlaying[i+1];
                    i++;
                }
                onMove(-1, (int) mCurPos);
            } catch (RemoteException ex) {
            }
            return true;
        }

        public void moveItem(int from, int to) {
            try {
                mService.moveQueueItem(from, to);
                mNowPlaying = mService.getQueue();
                onMove(-1, mCurPos); // update the underlying cursor
            } catch (RemoteException ex) {
            }
        }

        private void dump() {
            String where = "(";
            for (int i = 0; i < mSize; i++) {
                where += mNowPlaying[i];
                if (i < mSize - 1) {
                    where += ",";
                }
            }
            where += ")";
            Log.i("NowPlayingCursor: ", where);
        }

        @Override
        public String getString(int column)
        {
            try {
                return mCurrentPlaylistCursor.getString(column);
            } catch (Exception ex) {
                onChange(true);
                return "";
            }
        }

        @Override
        public short getShort(int column)
        {
            return mCurrentPlaylistCursor.getShort(column);
        }

        @Override
        public int getInt(int column)
        {
            try {
                return mCurrentPlaylistCursor.getInt(column);
            } catch (Exception ex) {
                onChange(true);
                return 0;
            }
        }

        @Override
        public long getLong(int column)
        {
            try {
                return mCurrentPlaylistCursor.getLong(column);
            } catch (Exception ex) {
                onChange(true);
                return 0;
            }
        }

        @Override
        public float getFloat(int column)
        {
            return mCurrentPlaylistCursor.getFloat(column);
        }

        @Override
        public double getDouble(int column)
        {
            return mCurrentPlaylistCursor.getDouble(column);
        }

        @Override
        public int getType(int column) {
            return mCurrentPlaylistCursor.getType(column);
        }

        @Override
        public boolean isNull(int column)
        {
            return mCurrentPlaylistCursor.isNull(column);
        }

        @Override
        public String[] getColumnNames()
        {
            return mCols;
        }

        @Override
        public void deactivate()
        {
            if (mCurrentPlaylistCursor != null)
                mCurrentPlaylistCursor.deactivate();
        }

        @Override
        public boolean requery()
        {
            makeNowPlayingCursor();
            return true;
        }

        @Override
        public void close() {
            super.close();
            if (mCurrentPlaylistCursor != null) {
                mCurrentPlaylistCursor.close();
                mCurrentPlaylistCursor = null;
            }
        }

        private String [] mCols;
        private Cursor mCurrentPlaylistCursor;     // updated in onMove
        private int mSize;          // size of the queue
        private long[] mNowPlaying;
        private long[] mCursorIdxs;
        private int mCurPos;
        private IMediaPlaybackService mService;
    }

    static class TrackListAdapter extends android.widget.SimpleCursorAdapter
                                    implements android.widget.SectionIndexer {
        boolean mIsNowPlaying;
        boolean mDisableNowPlayingIndicator;
        private final BitmapDrawable mDefaultAlbumIcon;

        int mTitleIdx;
        int mArtistIdx;
        int mDurationIdx;
        int mAudioIdIdx;
        int mSongIdx;
        int mAlbumIdx;
        int mDataIdx = -1;

        private final StringBuilder mBuilder = new StringBuilder();
        private final String mUnknownArtist;
        private final String mUnknownAlbum;

        private AlphabetIndexer mIndexer;

        private TrackBrowserActivityFragment mActivity = null;
        private TrackQueryHandler mQueryHandler;
        private String mConstraint = null;
        private boolean mConstraintIsValid = false;

        static class ViewHolder {
            TextView line1;
            TextView line2, positionview;
            TextView duration;
            CharArrayBuffer buffer1;
            char [] buffer2;
            ImageView drm_icon;
            ImageView icon;
            WaveView anim_icon;
            int position = -1;
            ImageView playMenu;
            String mCurrentTrackName;
            String mCurrentAlbumName;
            String mCurrentArtistNameForAlbum;
            String mCurrentTrackDuration;
            String mCurrentTrackPath;
            String mCurrentTrackSize;
            long  mSelectedId;
        }

        class TrackQueryHandler extends AsyncQueryHandler {

            class QueryArgs {
                public Uri uri;
                public String [] projection;
                public String selection;
                public String [] selectionArgs;
                public String orderBy;
            }

            TrackQueryHandler(ContentResolver res) {
                super(res);
            }

            public Cursor doQuery(Uri uri, String[] projection,
                    String selection, String[] selectionArgs,
                    String orderBy, boolean async) {
                if (async) {
                    // Get 500 results first, which is enough to allow the user to start scrolling,
                    // while still being very fast.
                    Uri limituri = uri.buildUpon().appendQueryParameter("limit", "500").build();
                    QueryArgs args = new QueryArgs();
                    args.uri = uri;
                    args.projection = projection;
                    args.selection = selection;
                    args.selectionArgs = selectionArgs;
                    args.orderBy = orderBy;

                    startQuery(0, args, limituri, projection, selection, selectionArgs, orderBy);
                    return null;
                }
                return MusicUtils.query(mActivity.getParentActivity(),
                        uri, projection, selection, selectionArgs, orderBy);
            }

            @Override
            protected void onQueryComplete(int token, Object cookie, Cursor cursor) {
                //Log.i("@@@", "query complete: " + cursor.getCount() + "   " + mActivity);
                mActivity.init(cursor, cookie != null);
                if (token == 0 && cookie != null && cursor != null &&
                    !cursor.isClosed() && cursor.getCount() >= 100) {
                    QueryArgs args = (QueryArgs) cookie;
                    startQuery(1, null, args.uri, args.projection, args.selection,
                            args.selectionArgs, args.orderBy);
                }
            }
        }

        TrackListAdapter(Context context, TrackBrowserActivityFragment currentactivity,
                int layout, Cursor cursor, String[] from, int[] to,
                boolean isnowplaying, boolean disablenowplayingindicator) {
            super(context, layout, cursor, from, to);
            mActivity = currentactivity;
            getColumnIndices(cursor);
            mIsNowPlaying = isnowplaying;
            mDisableNowPlayingIndicator = disablenowplayingindicator;
            mUnknownArtist = context.getString(R.string.unknown_artist_name);
            mUnknownAlbum = context.getString(R.string.unknown_album_name);
            Resources r = context.getResources();
            mDefaultAlbumIcon = (BitmapDrawable)r.getDrawable(R.drawable.unknown_albums);
            mQueryHandler = new TrackQueryHandler(context.getContentResolver());
        }

        public void setActivity(TrackBrowserActivityFragment newactivity) {
            mActivity = newactivity;
        }

        public TrackQueryHandler getQueryHandler() {
            return mQueryHandler;
        }

        private void getColumnIndices(Cursor cursor) {
            if (cursor != null) {
                mTitleIdx = cursor.getColumnIndexOrThrow(MediaStore.Audio.Media.TITLE);
                mArtistIdx = cursor.getColumnIndexOrThrow(MediaStore.Audio.Media.ARTIST);
                mDurationIdx = cursor.getColumnIndexOrThrow(MediaStore.Audio.Media.DURATION);
                mSongIdx = cursor.getColumnIndexOrThrow(MediaStore.Audio.Media.TITLE);
                mAlbumIdx = cursor.getColumnIndexOrThrow(MediaStore.Audio.Media.ALBUM);
                try {
                    mAudioIdIdx = cursor.getColumnIndexOrThrow(
                            MediaStore.Audio.Playlists.Members.AUDIO_ID);
                } catch (IllegalArgumentException ex) {
                    mAudioIdIdx = cursor.getColumnIndexOrThrow(MediaStore.Audio.Media._ID);
                }

                if (mIndexer != null) {
                    mIndexer.setCursor(cursor);
                } else if (!mActivity.mEditMode && mActivity.mAlbumId == null) {
                    String alpha = mActivity.getString(R.string.fast_scroll_alphabet);

                    mIndexer = new MusicAlphabetIndexer(cursor, mTitleIdx, alpha);
                }
                try {
                    mDataIdx = cursor.getColumnIndexOrThrow(MediaStore.Audio.Media.DATA);
                } catch (IllegalArgumentException ex) {
                    Log.w(LOGTAG, "_data column not found. Exception : " + ex);
                }
            }
        }
        int testpos=0;
        @Override
        public View newView(Context context, Cursor cursor, ViewGroup parent) {
            View v = super.newView(context, cursor, parent);
            ViewHolder vh = new ViewHolder();
            vh.line1 = (TextView) v.findViewById(R.id.line1);
            vh.line2 = (TextView) v.findViewById(R.id.line2);
            vh.buffer1 = new CharArrayBuffer(100);
            vh.buffer2 = new char[200];
            vh.anim_icon = (WaveView) v.findViewById(R.id.play_animator);
            vh.playMenu = (ImageView) v.findViewById(R.id.select_artist);
            ((MediaPlaybackActivity) mActivity.getParentActivity()).setTouchDelegate(vh.playMenu);
            vh.position = testpos;
             testpos+=1;
            v.setTag(vh);
            return v;
        }

        @Override
        public void bindView(View view, Context context, final Cursor cursor) {
            final ViewHolder vh = (ViewHolder) view.getTag();
            cursor.copyStringToBuffer(mTitleIdx, vh.buffer1);
            String songName = cursor.getString(mSongIdx);
            vh.line1.setText(String.valueOf(cursor.getPosition()+1)+". "+songName);
            vh.mCurrentTrackName = songName;
            vh.mSelectedId = cursor.getLong(mAudioIdIdx);
            String albumName = cursor.getString(mAlbumIdx);
            if (albumName == null || albumName.equals(MediaStore.UNKNOWN_STRING)) {
                albumName = context.getString(R.string.unknown_album_name);
            }
            vh.mCurrentAlbumName = albumName;
            mActivity.mTextView1.setText(albumName);
            mActivity.mCurrentAlbumName = albumName;
            vh.line1.setTextColor(Color.BLACK);
            vh.playMenu.setTag(cursor.getPosition());
            vh.playMenu.setOnClickListener(new OnClickListener() {

                @Override
                public void onClick(final View v) {
                    if (mActivity.mPopupMenu != null) {
                        mActivity.mPopupMenu.dismiss();
                    }
                    mActivity.mCurrentTrackName = vh.mCurrentTrackName;
                   mActivity.mCurrentArtistNameForAlbum = vh.mCurrentArtistNameForAlbum;
                    mActivity.mCurrentAlbumName = vh.mCurrentAlbumName;
                    mActivity.mCurrentTrackDuration = vh.mCurrentTrackDuration;
                    mActivity.mCurrentTrackPath = vh.mCurrentTrackPath;
                    mActivity.mCurrentTrackSize = vh.mCurrentTrackSize;
                    mActivity.mSelectedId =vh.mSelectedId;
                    PopupMenu popup = new PopupMenu(mActivity.getParentActivity(), v);
                    mActivity.onCreatePopupMenu(popup);
                    popup.show();
                    popup.setOnMenuItemClickListener(new PopupMenu.OnMenuItemClickListener() {
                        public boolean onMenuItemClick(MenuItem item) {
                            mActivity.onContextItemSelected(item, Integer.parseInt(v.getTag().toString()));
                            return true;
                        }
                    });
                    mActivity.mPopupMenu = popup;
                }
            });
            int secs = cursor.getInt(mDurationIdx) / 1000;
            vh.mCurrentTrackDuration = MusicUtils.makeTimeString(context, secs)
                    + context.getString(R.string.duration_unit);
            /*
             * if (secs == 0) { vh.duration.setText(""); } else {
             * vh.duration.setText(MusicUtils.makeTimeString(context, secs)); }
             */

            String trackPath = cursor.getString(cursor
                    .getColumnIndexOrThrow(MediaStore.Audio.Media.DATA));
            vh.mCurrentTrackPath = trackPath;

            String trackSize = cursor.getString(cursor
                    .getColumnIndexOrThrow(MediaStore.Audio.Media.SIZE));
            vh.mCurrentTrackSize = getHumanReadableSize(context, Integer.parseInt(trackSize));

            final StringBuilder builder = mBuilder;
            builder.delete(0, builder.length());

            String name = cursor.getString(mArtistIdx);
            if (name == null || name.equals(MediaStore.UNKNOWN_STRING)) {
                // Reload the "unknown_artist_name" string in order to
                // avoid that this string doesn't change when user
                // changes the system language setting.
                name = context.getString(R.string.unknown_artist_name);
            }
            builder.append(name);
            vh.mCurrentArtistNameForAlbum = name;
            mActivity.mTextView2.setText(name);

            int len = builder.length();
            if (vh.buffer2.length < len) {
                vh.buffer2 = new char[len];
            }
            builder.getChars(0, len, vh.buffer2, 0);

            vh.line2.setText("    "+name);
            vh.line2.setTextColor(Color.BLACK);
            WaveView iv1 = vh.anim_icon;
            long id = -1;
            if (MusicUtils.sService != null) {
                // TODO: IPC call on each bind??
                try {
                    if (mIsNowPlaying) {
                        id = MusicUtils.sService.getQueuePosition();
                    } else {
                        id = MusicUtils.sService.getAudioId();
                    }
                } catch (RemoteException ex) {
                }
            }

           mAnimView = vh.anim_icon;

            // Determining whether and where to show the "now playing indicator
            // is tricky, because we don't actually keep track of where the songs
            // in the current playlist came from after they've started playing.
            //
            // If the "current playlists" is shown, then we can simply match by position,
            // otherwise, we need to match by id. Match-by-id gets a little weird if
            // a song appears in a playlist more than once, and you're in edit-playlist
            // mode. In that case, both items will have the "now playing" indicator.
            // For this reason, we don't show the play indicator at all when in edit
            // playlist mode (except when you're viewing the "current playlist",
            // which is not really a playlist)
            if ((mIsNowPlaying && cursor.getPosition() == id) ||
                    (!mIsNowPlaying && cursor.getLong(mAudioIdIdx) == id)) {
                mAnimView.setVisibility(View.VISIBLE);
                mAnimView.animate(MusicUtils.isPlaying());
            } else {
                mAnimView.animate(false);
                mAnimView.setVisibility(View.INVISIBLE);
            }
        }

        private String getHumanReadableSize(Context context, int size) {
            // TODO Auto-generated method stub
            Resources res = context.getResources();
            final int[] magnitude = {
                                     R.string.size_bytes,
                                     R.string.size_kilobytes,
                                     R.string.size_megabytes,
                                     R.string.size_gigabytes
                                    };

            double aux = size;
            int cc = magnitude.length;
            for (int i = 0; i < cc; i++) {
                if (aux < 1024) {
                    double cleanSize = Math.round(aux * 100);
                    return Double.toString(cleanSize / 100) +
                            " " + res.getString(magnitude[i]); //$NON-NLS-1$
                } else {
                    aux = aux / 1024;
                }
            }
            double cleanSize = Math.round(aux * 100);
            return Double.toString(cleanSize / 100) +
                    " " + res.getString(magnitude[cc - 1]); //$NON-NLS-1$
        }

        @Override
        public void changeCursor(Cursor cursor) {
            if (mActivity.getParentActivity() != null
                    && mActivity.getParentActivity().isFinishing()
                    && cursor != null) {
                cursor.close();
                cursor = null;
            }
            if (cursor != null && !cursor.isClosed() && cursor != mActivity.mTrackCursor) {
                if (mActivity.mTrackCursor != null) {
                    mActivity.mTrackCursor.close();
                    mActivity.mTrackCursor = null;
                }
                mActivity.mTrackCursor = cursor;
                super.changeCursor(cursor);
                getColumnIndices(cursor);
            } else {
                cursor = null;
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
            Cursor c = mActivity.getTrackCursor(mQueryHandler, s, false);
            mConstraint = s;
            mConstraintIsValid = true;
            return c;
        }

        // SectionIndexer methods

        public Object[] getSections() {
            if (mIndexer != null) {
                return mIndexer.getSections();
            } else {
                return new String [] { " " };
            }
        }

        public int getPositionForSection(int section) {
            if (mIndexer != null) {
                return mIndexer.getPositionForSection(section);
            }
            return 0;
        }

        public int getSectionForPosition(int position) {
            return 0;
        }
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position,
            long id) {
            // play the track
         MusicUtils.playAll(mParentActivity, mTrackCursor, position);
         mAdapter.notifyDataSetChanged();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        if (mPopupMenu != null) {
            mPopupMenu.dismiss();
        }
        if (mSub != null) {
            mSub.close();
        }
        LayoutInflater inflater = LayoutInflater.from(getContext());
        View view = inflater.inflate(R.layout.media_picker_activity, null);
        initViews(mContainer ,view);
    }
}
