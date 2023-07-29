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

import android.app.ActionBar;
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
import android.database.CharArrayBuffer;
import android.database.Cursor;
import android.database.DatabaseUtils;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.AnimationDrawable;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ColorDrawable;
import android.media.AudioManager;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.provider.BaseColumns;
import android.provider.MediaStore;
import android.text.TextUtils;
import android.util.Log;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.SearchView;
import android.widget.SearchView.OnQueryTextListener;
import android.widget.SimpleCursorAdapter;
import android.widget.TextView;

import com.android.music.MusicUtils.ServiceToken;
import com.android.music.TrackBrowserFragment.TrackListAdapter.ViewHolder;

public class QueryBrowserActivity extends ListActivity implements
        MusicUtils.Defs, ServiceConnection, OnQueryTextListener {
    private final static int PLAY_NOW = 0;
    private final static int ADD_TO_QUEUE = 1;
    private final static int PLAY_NEXT = 2;
    private final static int PLAY_ARTIST = 3;
    private final static int EXPLORE_ARTIST = 4;
    private final static int PLAY_ALBUM = 5;
    private final static int EXPLORE_ALBUM = 6;
    private final static int REQUERY = 3;
    private QueryListAdapter mAdapter;
    private boolean mAdapterSent;
    private String mFilterString = "";
    private ServiceToken mToken;

    public QueryBrowserActivity() {
    }

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        if (icicle != null) {
            mFilterString = icicle.getString("filterString");
        }
        setVolumeControlStream(AudioManager.STREAM_MUSIC);
        mAdapter = (QueryListAdapter) getLastNonConfigurationInstance();
        mToken = MusicUtils.bindToService(this, this);
        // defer the real work until we're bound to the service
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.menu, menu);

        SearchManager searchManager = (SearchManager) getSystemService(Context.SEARCH_SERVICE);
        MenuItem searchMenuItem = menu.findItem(R.id.search);
        searchMenuItem.expandActionView();

        SearchView searchView = (SearchView) searchMenuItem.getActionView();
        ViewGroup.LayoutParams layoutParams = searchView.getLayoutParams();
        layoutParams.width = ViewGroup.LayoutParams.MATCH_PARENT;
        searchView.setSubmitButtonEnabled(false);
        searchView.setOnQueryTextListener(this);

        searchMenuItem.setOnActionExpandListener(new MenuItem.OnActionExpandListener() {
            @Override
            public boolean onMenuItemActionExpand(MenuItem item) {
                return true;
            }

            @Override
            public boolean onMenuItemActionCollapse(MenuItem item) {
                finish();
                return true;
            }
        });

        return true;
    }

    public void onServiceConnected(ComponentName name, IBinder service) {
        IntentFilter f = new IntentFilter();
        f.addAction(Intent.ACTION_MEDIA_SCANNER_STARTED);
        f.addAction(Intent.ACTION_MEDIA_UNMOUNTED);
        f.addDataScheme("file");
        registerReceiver(mScanListener, f);

        Intent intent = getIntent();
        String action = intent != null ? intent.getAction() : null;

        if (Intent.ACTION_VIEW.equals(action)) {
            // this is something we got from the search bar
            Uri uri = intent.getData();
            String path = uri.toString();
            if (path.startsWith("content://media/external/audio/media/")) {
                // This is a specific file
                String id = uri.getLastPathSegment();
                long[] list = new long[] { Long.valueOf(id) };
                MusicUtils.playAll(this, list, 0);
                finish();
                return;
            } else if (path
                    .startsWith("content://media/external/audio/albums/")) {
                // This is an album, show the songs on it
                Intent i = new Intent(QueryBrowserActivity.this,
                        MusicBrowserActivity.class);
                i.putExtra("album", uri.getLastPathSegment());
                MusicUtils.setIntPref(this, "activetab", 2);
                startActivity(i);
                finish();
                return;
            } else if (path
                    .startsWith("content://media/external/audio/artists/")) {
                // This is an artist, show the albums for that artist
                Intent i = new Intent(QueryBrowserActivity.this,
                        MusicBrowserActivity.class);
                i.putExtra("artist", uri.getLastPathSegment());
                MusicUtils.setIntPref(this, "activetab", 1);
                startActivity(i);
                finish();
                return;
            }
        }
        setContentView(R.layout.query_activity);
        ActionBar bar = getActionBar();
        // for color
        bar.setBackgroundDrawable(new ColorDrawable(Color.parseColor(this.getResources().getString(R.color.app_theme_color))));
        // for image
        getActionBar().setDisplayHomeAsUpEnabled(true);

        mTrackList = getListView();
        mTrackList.setDivider(null);
        mTrackList.setTextFilterEnabled(false);
        if (mAdapter == null) {
            mAdapter = new QueryListAdapter(getApplication(), this,
                    R.layout.track_list_item, null, // cursor
                    new String[] {}, new int[] {});
            setListAdapter(mAdapter);
            if (TextUtils.isEmpty(mFilterString)) {
                getQueryCursor(mAdapter.getQueryHandler(), null);
            } else {
                mTrackList.setFilterText(mFilterString);
                // It has no influence if it isn't set to be null
                // mFilterString = null;
            }
        } else {
            mAdapter.setActivity(this);
            setListAdapter(mAdapter);
            mQueryCursor = mAdapter.getCursor();
            if (mQueryCursor != null) {
                init(mQueryCursor);
            } else {
                getQueryCursor(mAdapter.getQueryHandler(), mFilterString);
            }
        }
    }

    public void onServiceDisconnected(ComponentName name) {

    }

    @Override
    public Object onRetainNonConfigurationInstance() {
        mAdapterSent = true;
        return mAdapter;
    }

    @Override
    public void onPause() {
        mReScanHandler.removeCallbacksAndMessages(null);
        super.onPause();
    }

    @Override
    public void onResume() {
        // When back to QueryBrowserActivity, we should update the cursor
        // and listview according to mFilterString.
        if (mAdapter != null) {
            getQueryCursor(mAdapter.getQueryHandler(), mFilterString);
        }
        super.onResume();
    }

    @Override
    public void onSaveInstanceState(Bundle outcicle) {
        outcicle.putString("filterString", mFilterString);
        super.onSaveInstanceState(outcicle);
    }

    @Override
    public void onDestroy() {
        MusicUtils.unbindFromService(mToken);
        unregisterReceiver(mScanListener);
        // If we have an adapter and didn't send it off to another activity yet,
        // we should close its cursor, which we do by assigning a null cursor to it. Doing
        // this instead of closing the cursor directly keeps the framework from
        // accessing the closed cursor later.
        if (!mAdapterSent && mAdapter != null) {
            mAdapter.changeCursor(null);
        }
        // Because we pass the adapter to the next activity, we need to make
        // sure it doesn't keep a reference to this activity. We can do this
        // by clearing its DatasetObservers, which setListAdapter(null) does.
        if (getListView() != null) {
            setListAdapter(null);
        }
        mAdapter = null;
        super.onDestroy();
    }

    /*
     * This listener gets called when the media scanner starts up, and when the
     * sd card is unmounted.
     */
    private BroadcastReceiver mScanListener = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            MusicUtils.setSpinnerState(QueryBrowserActivity.this);
            mReScanHandler.sendEmptyMessage(0);
        }
    };

    private Handler mReScanHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            if (mAdapter != null) {
                getQueryCursor(mAdapter.getQueryHandler(), null);
            }
            // if the query results in a null cursor, onQueryComplete() will
            // call init(), which will post a delayed message to this handler
            // in order to try again.
        }
    };

    @Override
    protected void onActivityResult(int requestCode, int resultCode,
            Intent intent) {
        switch (requestCode) {
        case SCAN_DONE:
            if (resultCode == RESULT_CANCELED) {
                finish();
            } else {
                getQueryCursor(mAdapter.getQueryHandler(), null);
            }
            break;
        }
    }

    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_MENU) {
            return true;
        }
        return super.onKeyDown(keyCode, event);
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
        mAdapter.changeCursor(c);

        if (mQueryCursor == null) {
            MusicUtils.displayDatabaseError(this);
            setListAdapter(null);
            mReScanHandler.sendEmptyMessageDelayed(0, 1000);
            return;
        }
        MusicUtils.hideDatabaseError(this);
    }

    @Override
    protected void onListItemClick(ListView l, View v, int position, long id) {
        // Dialog doesn't allow us to wait for a result, so we need to store
        // the info we need for when the dialog posts its result
        mQueryCursor.moveToPosition(position);
        if (mQueryCursor.isBeforeFirst() || mQueryCursor.isAfterLast()) {
            return;
        }
        String selectedType = mQueryCursor.getString(mQueryCursor
                .getColumnIndexOrThrow(MediaStore.Audio.Media.MIME_TYPE));

        if ("artist".equals(selectedType)) {
            Intent intent = new Intent(QueryBrowserActivity.this,
                    MusicBrowserActivity.class);
            intent.putExtra("artist", Long.valueOf(id).toString());
            MusicUtils.setIntPref(this, "activetab", 1);
            MusicUtils.isLaunchedFromQueryBrowser = true;
            startActivity(intent);
        } else if ("album".equals(selectedType)) {
            Intent intent = new Intent(QueryBrowserActivity.this,
                    MusicBrowserActivity.class);
            intent.putExtra("album", Long.valueOf(id).toString());
            MusicUtils.setIntPref(this, "activetab", 2);
            MusicUtils.isLaunchedFromQueryBrowser = true;
            startActivity(intent);
        } else if (position >= 0 && id >= 0) {
            long[] list = new long[] { id };
            MusicUtils.playAll(this, list, 0);
        } else {
            Log.e("QueryBrowser", "invalid position/id: " + position + "/" + id);
        }
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
        case USE_AS_RINGTONE: {
            // Set the system setting to make this the current ringtone
            MusicUtils.setRingtone(this, mTrackList.getSelectedItemId());
            return true;
        }
        case android.R.id.home: {
            // Set the system setting to make this the current ringtone

            finish();
            return true;
        }

        }
        return super.onOptionsItemSelected(item);
    }

    private Cursor getQueryCursor(AsyncQueryHandler async, String filter) {
        if (filter == null) {
            filter = "";
        }
        String[] ccols = new String[] {
                BaseColumns._ID, // this will be the artist, album or track ID
                MediaStore.Audio.Media.MIME_TYPE, // mimetype of audio file, or
                                                    // "artist" or "album"
                MediaStore.Audio.Artists.ARTIST, MediaStore.Audio.Albums.ALBUM,
                MediaStore.Audio.Media.TITLE, "data1", "data2" };

        Uri search = Uri.parse("content://media/external/audio/search/fancy/"
                + Uri.encode(filter));

        Cursor ret = null;
        if (async != null) {
            async.startQuery(0, null, search, ccols, null, null, null);
        } else {
            ret = MusicUtils.query(this, search, ccols, null, null, null);
        }
        return ret;
    }

    static class QueryListAdapter extends SimpleCursorAdapter {
        private QueryBrowserActivity mActivity = null;
        private final BitmapDrawable mDefaultAlbumIcon;
        private AsyncQueryHandler mQueryHandler;
        private String mConstraint = null;
        private boolean mConstraintIsValid = false;

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

        QueryListAdapter(Context context, QueryBrowserActivity currentactivity,
                int layout, Cursor cursor, String[] from, int[] to) {
            super(context, layout, cursor, from, to);
            mActivity = currentactivity;
            Resources r = mActivity.getResources();
            mDefaultAlbumIcon = (BitmapDrawable) r
                    .getDrawable(R.drawable.unknown_albums);
            mQueryHandler = new QueryHandler(context.getContentResolver());
        }

        static class ViewHolder {
            TextView line1;
            TextView line2;
            ImageView icon;
        }

        public void setActivity(QueryBrowserActivity newactivity) {
            mActivity = newactivity;
        }

        public AsyncQueryHandler getQueryHandler() {
            return mQueryHandler;
        }

        @Override
        public View newView(Context context, Cursor cursor, ViewGroup parent) {
            // TODO Auto-generated method stub
            View v = super.newView(context, cursor, parent);
            ViewHolder vh = new ViewHolder();
            vh.line1 = (TextView) v.findViewById(R.id.line1);
            vh.line2 = (TextView) v.findViewById(R.id.line2);
            vh.icon = (ImageView) v.findViewById(R.id.icon);
            v.findViewById(R.id.animView).setVisibility(View.INVISIBLE);
            v.findViewById(R.id.play_indicator).setVisibility(View.INVISIBLE);
            v.setTag(vh);
            return v;
        }

        @Override
        public void bindView(View view, Context context, Cursor cursor) {

            ViewHolder vh = (ViewHolder) view.getTag();
            ViewGroup.LayoutParams p = vh.icon.getLayoutParams();
            if (p == null) {
                // seen this happen, not sure why
                DatabaseUtils.dumpCursor(cursor);
                return;
            }
            String mimetype = cursor.getString(cursor
                    .getColumnIndexOrThrow(MediaStore.Audio.Media.MIME_TYPE));

            if (mimetype == null) {
                mimetype = "audio/";
            }
            String artistname = cursor.getString(cursor
                    .getColumnIndexOrThrow(MediaStore.Audio.Artists.ARTIST));
            Bitmap albumart[] = MusicUtils.getFromLruCache(
                    artistname, MusicUtils.mArtCache);
            if (albumart != null) {
                if (albumart[0] != null) {
                    vh.icon.setImageBitmap(albumart[0]);
                } else {
                    vh.icon.setImageBitmap(mDefaultAlbumIcon.getBitmap());
                }
            } else {
                vh.icon.setImageBitmap(mDefaultAlbumIcon.getBitmap());
            }
            if (mimetype.equals("artist")) {

                String name = cursor
                        .getString(cursor
                                .getColumnIndexOrThrow(MediaStore.Audio.Artists.ARTIST));

                String displayname = name;
                boolean isunknown = false;
                if (name == null || name.equals(MediaStore.UNKNOWN_STRING)) {
                    displayname = context
                            .getString(R.string.unknown_artist_name);
                    isunknown = true;
                }
                vh.line1.setText(displayname);

                int numalbums = cursor.getInt(cursor
                        .getColumnIndexOrThrow("data1"));
                int numsongs = cursor.getInt(cursor
                        .getColumnIndexOrThrow("data2"));

                String songs_albums = MusicUtils.makeAlbumsSongsLabel(context,
                        numalbums, numsongs, isunknown);

                vh.line2.setText(songs_albums);

            } else if (mimetype.equals("album")) {
                String name = cursor.getString(cursor
                        .getColumnIndexOrThrow(MediaStore.Audio.Albums.ALBUM));
                String displayname = name;
                if (name == null || name.equals(MediaStore.UNKNOWN_STRING)) {
                    displayname = context
                            .getString(R.string.unknown_album_name);
                }
                vh.line1.setText(displayname);

                name = cursor
                        .getString(cursor
                                .getColumnIndexOrThrow(MediaStore.Audio.Artists.ARTIST));
                displayname = name;
                if (name == null || name.equals(MediaStore.UNKNOWN_STRING)) {
                    displayname = context
                            .getString(R.string.unknown_artist_name);
                }
                vh.line2.setText(displayname);

            } else if (mimetype.startsWith("audio/")
                    || mimetype.equals("application/ogg")
                    || mimetype.equals("application/x-ogg")) {
                String name = cursor.getString(cursor
                        .getColumnIndexOrThrow(MediaStore.Audio.Media.TITLE));
                vh.line1.setText(name);

                String displayname = cursor
                        .getString(cursor
                                .getColumnIndexOrThrow(MediaStore.Audio.Artists.ARTIST));
                if (displayname == null
                        || displayname.equals(MediaStore.UNKNOWN_STRING)) {
                    displayname = context
                            .getString(R.string.unknown_artist_name);
                }
                name = cursor.getString(cursor
                        .getColumnIndexOrThrow(MediaStore.Audio.Albums.ALBUM));
                if (name == null || name.equals(MediaStore.UNKNOWN_STRING)) {
                    name = context.getString(R.string.unknown_album_name);
                }
                vh.line2.setText(displayname + " - " + name);
            }
        }

        @Override
        public void changeCursor(Cursor cursor) {
            if (mActivity.isFinishing() && cursor != null) {
                cursor.close();
                cursor = null;
            }
            if (cursor != mActivity.mQueryCursor && cursor != null && !cursor.isClosed()) {
                if (mActivity.mQueryCursor != null) {
                    mActivity.mQueryCursor.close();
                    mActivity.mQueryCursor = null;
                }
                mActivity.mQueryCursor = cursor;
                super.changeCursor(cursor);
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
            Cursor c = mActivity.getQueryCursor(null, s);
            mConstraint = s;
            mConstraintIsValid = true;
            return c;
        }
    }

    private ListView mTrackList;
    private Cursor mQueryCursor;

    @Override
    public boolean onQueryTextSubmit(String query) {
        // TODO Auto-generated method stub
        if (mTrackList != null) {
            mTrackList.setFilterText(query);
        }
        return false;
    }

    @Override
    public boolean onQueryTextChange(String newText) {
        if (mAdapter != null) {
            mAdapter.getFilter().filter(newText);
        }
        return false;
    }
}
