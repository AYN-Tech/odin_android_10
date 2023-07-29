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

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.Fragment;
import android.app.FragmentManager;
import android.app.NotificationManager;
import android.app.SearchManager;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.ContentUris;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.ServiceConnection;
import android.content.res.Configuration;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.media.audiofx.AudioEffect;
import android.media.AudioManager;
import android.media.MediaPlayer;
import android.media.MediaPlayer.OnCompletionListener;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.RemoteException;
import android.os.SystemClock;
import android.provider.MediaStore;
import android.telephony.SubscriptionInfo;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;
import android.text.Layout;
import android.text.TextUtils;
import android.text.TextUtils.TruncateAt;
import android.util.Log;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.SubMenu;
import android.view.TouchDelegate;
import android.view.View;
import android.view.ViewAnimationUtils;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.view.Window;
import android.view.View.OnClickListener;
import android.view.animation.AccelerateInterpolator;
import android.widget.AbsListView;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.FrameLayout;
import android.widget.PopupMenu;
import android.widget.PopupMenu.OnMenuItemClickListener;
import android.widget.ProgressBar;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.Toolbar;
import android.widget.SeekBar.OnSeekBarChangeListener;

import com.codeaurora.music.custom.FragmentsFactory;
import com.codeaurora.music.custom.MusicPanelLayout;
import com.codeaurora.music.custom.MusicPanelLayout.BoardState;
import com.codeaurora.music.lyric.Lyric;
import com.codeaurora.music.lyric.LyricAdapter;
import com.codeaurora.music.lyric.LyricView;
import com.codeaurora.music.lyric.PlayListItem;

import java.io.File;
import java.util.Locale;
import java.util.Timer;
import java.util.TimerTask;

public class MediaPlaybackActivity extends Activity implements MusicUtils.Defs,
        ServiceConnection, OnCompletionListener, AbsListView.OnScrollListener{
    private static final int SAVE_AS_PLAYLIST = CHILD_MENU_BASE + 2;
    private static final int CLEAR_PLAYLIST = CHILD_MENU_BASE + 4;
    private static final int INVALID_PLAYLIST_ID = -1;

    private static final String TAG = "MediaPlaybackActivity";
    private boolean mSeeking = false;
    private boolean mDeviceHasDpad;
    private long mStartSeekPos = 0;
    private long mLastSeekEventTime;
    private IMediaPlaybackService mService = null;
    private RepeatingImageButton mPrevButton;
    private ImageButton mPauseButton;
    public ImageView mNowPlayingIcon;
    private RepeatingImageButton mNextButton;
    private ImageButton mRepeatButton;
    private ImageButton mShuffleButton;
    private ImageButton mCurrentPlaylist;
    private FrameLayout mQueueLayout;
    public Toolbar mToolbar;
    private Worker mAlbumArtWorker;
    private AlbumArtHandler mAlbumArtHandler;
    private Toast mToast;
    private int mTouchSlop;
    private ServiceToken mToken;
    private boolean mReceiverUnregistered = true;
    private static MediaPlaybackActivity mActivity;
    private MusicPanelLayout mSlidingPanelLayout;
    private ImageButton mMenuOverFlow;
    private View mNowPlayingView;
    Fragment mFragment;
    private boolean isBackPressed = false;
    private boolean mCheckIfSeekRequired = false;
    private ImageView mFavoriteIcon;
    private ImageView mLyricIcon;
    private LinearLayout mLandControlPanel;
    private LinearLayout mAudioPlayerBody;
    private LyricView mLyricView;
    private View mLyricPanel;
    private LyricAdapter mLyricAdapter;
    private PlayListItem mCurPlayListItem;
    private Lyric mLyric;
    private boolean mHasLrc = false;
    private boolean mIsLyricDisplay = false;
    private boolean mIsTouchTrigger = false;
    private boolean mOrientationChanged = false;
    private boolean mMetaChanged = false;
    private boolean mIsExeLyricAnimal = false;
    private int mFirstVisibleItem = 0;
    private int mLyricSrollState = SCROLL_STATE_IDLE;
    private String mStrTrackName = null;
    private ResumeScrollTask mResumeScrollTask;
    private final Timer mResumeTimer = new Timer("resumeTimer");
    private PopupMenu mPopupMenu;

    private static final int STATUS_DISABLE = 0;
    private static final int STATUS_PORT_UNSELECTED = 1;
    private static final int STATUS_LAND_UNSELECTED = 2;
    private static final int STATUS_SELECTED = 3;

    private static final int SWITCH_MUSIC_MAX_TIME = 2000;
    private int mLyricIconStatus;
    public boolean mIsPanelExpanded = false;
    private MediaButtonIntentReceiver mMediaButtonIntentReceiver = new MediaButtonIntentReceiver();

    public MediaPlaybackActivity() {
    }

    public static MediaPlaybackActivity getInstance() {
        return mActivity;
    }

    public View getNowPlayingView() {
        return mNowPlayingView;
    }

    public MusicPanelLayout getSlidingPanelLayout() {
        return mSlidingPanelLayout;
    }

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle icicle) {
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        super.onCreate(icicle);
        setVolumeControlStream(AudioManager.STREAM_MUSIC);
        mAlbumArtWorker = new Worker("album art worker");
        mAlbumArtHandler = new AlbumArtHandler(mAlbumArtWorker.getLooper());
        mActivity = this;
        IntentFilter metaChangeFilter = new IntentFilter();
        metaChangeFilter.addAction(MediaPlaybackService.META_CHANGED);
        metaChangeFilter.addAction(MediaPlaybackService.QUEUE_CHANGED);
        metaChangeFilter.addAction(MediaPlaybackService.PLAYSTATE_CHANGED);
        registerReceiver(mTrackListListener, metaChangeFilter);

        IntentFilter f = new IntentFilter();
        f.addAction(AudioManager.ACTION_AUDIO_BECOMING_NOISY);
        f.addAction(Intent.ACTION_MEDIA_BUTTON);
        registerReceiver(mMediaButtonIntentReceiver, f);

        new MusicUtils.BitmapDownloadThread(this, null, 0, 20).start();
    }

    public void init() {
        mToolbar = (Toolbar) findViewById(R.id.music_tool_bar);
        mNowPlayingView = findViewById(R.id.dragLayout);
        mSlidingPanelLayout = (MusicPanelLayout) findViewById(R.id.sliding_layout);
        mNowPlayingIcon = (ImageView) findViewById(R.id.nowplay_icon);
        mNowPlayingIcon.setOnClickListener(mPauseListener);
        setTouchDelegate(mNowPlayingIcon);
        mAlbum = (ImageView) findViewById(R.id.album);
        mArtistName = (TextView) findViewById(R.id.artist_name);
        mTrackName = (TextView) findViewById(R.id.song_name);
        mCurrentPlaylist = (ImageButton) findViewById(R.id.animViewcurrPlaylist);
        mCurrentPlaylist.setOnClickListener(mQueueListener);
        setTouchDelegate(mCurrentPlaylist);
        seekmethod = 1;

        mDeviceHasDpad
                = (getResources().getConfiguration().navigation == Configuration.NAVIGATION_DPAD);

        mTouchSlop = ViewConfiguration.get(this).getScaledTouchSlop();
        mMenuOverFlow = (ImageButton) findViewById(R.id.menu_overflow_audio_header);
        mMenuOverFlow.setOnClickListener(mActiveButtonPopUpMenuListener);
        setTouchDelegate(mMenuOverFlow);
        SysApplication.getInstance().addActivity(this);
        mLyricAdapter = new LyricAdapter(this);
        mAudioPlayerBody = (LinearLayout)findViewById(R.id.audio_player_body);
        initAudioPlayerBodyView();
    }

    private void initAudioPlayerBodyView() {
        mCurrentTime = (TextView) findViewById(R.id.currenttimer);
        mTotalTime = (TextView) findViewById(R.id.totaltimer);
        mProgress = (ProgressBar) findViewById(R.id.progress);
        mQueueLayout = (FrameLayout) findViewById(R.id.current_queue_view);
        mFavoriteIcon = (ImageView) findViewById(R.id.favorite);
        mFavoriteIcon.setVisibility(View.VISIBLE);
        setFavoriteIconImage(getFavoriteStatus());
        mFavoriteIcon.setOnClickListener(mFavoriteListener);
        mAlbumIcon = (ImageView) findViewById(R.id.album_icon_view);
        boolean isRtl = TextUtils.getLayoutDirectionFromLocale(Locale.getDefault()) ==
                            View.LAYOUT_DIRECTION_RTL;
        mPrevButton = (RepeatingImageButton) findViewById(R.id.previcon);
        mPrevButton.setOnClickListener(mPrevListener);
        mPrevButton.setRepeatListener(mRewListener, 260);
        mPrevButton.setImageResource(isRtl ? R.drawable.nex : R.drawable.pre);
        mPauseButton = (ImageButton) findViewById(R.id.play_pause);
        mPauseButton.requestFocus();
        mPauseButton.setOnClickListener(mPauseListener);
        mNextButton = (RepeatingImageButton) findViewById(R.id.nexticon);
        mNextButton.setOnClickListener(mNextListener);
        mNextButton.setRepeatListener(mFfwdListener, 260);
        mNextButton.setImageResource(isRtl ? R.drawable.pre : R.drawable.nex);
        MusicPanelLayout.mSeekBarView = mProgress;
        MusicPanelLayout.mSongsQueueView = mQueueLayout;
        mShuffleButton = (ImageButton) findViewById(R.id.randomicon);
        mShuffleButton.setOnClickListener(mShuffleListener);
        mRepeatButton = (ImageButton) findViewById(R.id.loop_view);
        mRepeatButton.setOnClickListener(mRepeatListener);
        if (mProgress instanceof SeekBar) {
            SeekBar seeker = (SeekBar) mProgress;
            seeker.setOnSeekBarChangeListener(mSeekListener);
        }
        mProgress.setMax(1000);
        mLyricPanel = findViewById(R.id.lyric_panel);
        mLyricView = (LyricView) findViewById(R.id.lv_lyric);
        mLyricView.setAdapter(mLyricAdapter);
        mLyricView.setSelection(mFirstVisibleItem);
        mLyricView.setOnScrollListener(this);
        mLyricIcon = (ImageView) findViewById(R.id.lyric);
        mLyricIcon.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mIsLyricDisplay) {
                    hideLyric();
                } else {
                    showLyric();
                }
            }
        });
        mLandControlPanel = (LinearLayout)findViewById(R.id.land_control_panel);
        if (!MusicUtils.isFragmentRemoved) {
            handleDisplay();
            changeFragment();
        }
    }

    private void handleDisplay() {
        mSlidingPanelLayout.mIsQueueEnabled = true;
        mAlbumIcon.setVisibility(View.GONE);
        mLyricIcon.setVisibility(View.GONE);
        mFavoriteIcon.setVisibility(View.GONE);
        mCurrentPlaylist.setImageResource(R.drawable.list_active);
        mMenuOverFlow.setOnClickListener(mPopUpMenuListener);
        if (mHasLrc) {
            mLyricPanel.setVisibility(View.GONE);
            stopQueueNextRefreshForLyric();
        }
        if (mLandControlPanel != null) {
            mLandControlPanel.setVisibility(View.GONE);
        }
    }

    private void changeFragment() {
        mFragment = new TrackBrowserFragment();
        Bundle args = new Bundle();
        args.putString("playlist", "nowplaying");
        args.putBoolean("editValue", true);
        mFragment.setArguments(args);
        getFragmentManager().beginTransaction()
                .replace(R.id.current_queue_view, mFragment, "track_fragment")
                .commitAllowingStateLoss();
    }

    int mInitialX = -1;
    int mLastX = -1;
    int mTextWidth = 0;
    int mViewWidth = 0;
    boolean mDraggingLabel = false;

    TextView textViewForContainer(View v) {
        View vv = v.findViewById(R.id.artistname);
        if (vv != null)
            return (TextView) vv;
        vv = v.findViewById(R.id.albumname);
        if (vv != null)
            return (TextView) vv;
        vv = v.findViewById(R.id.trackname);
        if (vv != null)
            return (TextView) vv;
        return null;
    }

    public boolean onTouch(View v, MotionEvent event) {
        int action = event.getAction();
        TextView tv = textViewForContainer(v);
        if (tv == null) {
            return false;
        }
        if (action == MotionEvent.ACTION_DOWN) {
            v.setBackgroundColor(0xff606060);
            mInitialX = mLastX = (int) event.getX();
            mDraggingLabel = false;
        } else if (action == MotionEvent.ACTION_UP
                || action == MotionEvent.ACTION_CANCEL) {
            v.setBackgroundColor(0xff000000);
            if (mDraggingLabel) {
                Message msg = mLabelScroller.obtainMessage(0, tv);
                mLabelScroller.sendMessageDelayed(msg, 1000);
            }
        } else if (action == MotionEvent.ACTION_MOVE) {
            if (mDraggingLabel) {
                int scrollx = tv.getScrollX();
                int x = (int) event.getX();
                int delta = mLastX - x;
                if (delta != 0) {
                    mLastX = x;
                    scrollx += delta;
                    if (scrollx > mTextWidth) {
                        // scrolled the text completely off the view to the left
                        scrollx -= mTextWidth;
                        scrollx -= mViewWidth;
                    }
                    if (scrollx < -mViewWidth) {
                        // scrolled the text completely off the view to the right
                        scrollx += mViewWidth;
                        scrollx += mTextWidth;
                    }
                    tv.scrollTo(scrollx, 0);
                }
                return true;
            }
            int delta = mInitialX - (int) event.getX();
            if (Math.abs(delta) > mTouchSlop) {
                // start moving
                mLabelScroller.removeMessages(0, tv);

                // Only turn ellipsizing off when it's not already off, because it
                // causes the scroll position to be reset to 0.
                if (tv.getEllipsize() != null) {
                    tv.setEllipsize(null);
                }
                Layout ll = tv.getLayout();
                // layout might be null if the text just changed, or ellipsizing
                // was just turned off
                if (ll == null) {
                    return false;
                }
                // get the non-ellipsized line width, to determine whether scrolling
                // should even be allowed
                mTextWidth = (int) tv.getLayout().getLineWidth(0);
                mViewWidth = tv.getWidth();
                if (mViewWidth > mTextWidth) {
                    tv.setEllipsize(TruncateAt.END);
                    v.cancelLongPress();
                    return false;
                }
                mDraggingLabel = true;
                tv.setHorizontalFadingEdgeEnabled(true);
                v.cancelLongPress();
                return true;
            }
        }
        return false;
    }

    Handler mLabelScroller = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            TextView tv = (TextView) msg.obj;
            int x = tv.getScrollX();
            x = x * 3 / 4;
            tv.scrollTo(x, 0);
            if (x == 0) {
                tv.setEllipsize(TruncateAt.END);
            } else {
                Message newmsg = obtainMessage(0, tv);
                mLabelScroller.sendMessageDelayed(newmsg, 15);
            }
        }
    };

    public boolean onLongClick(View view) {

        CharSequence title = null;
        String mime = null;
        String query = null;
        String artist;
        String album;
        String song;
        long audioid;

        try {
            artist = mService.getArtistName();
            album = mService.getAlbumName();
            song = mService.getTrackName();
            audioid = mService.getAudioId();
        } catch (RemoteException ex) {
            return true;
        } catch (NullPointerException ex) {
            // we might not actually have the service yet
            return true;
        }

        if (MediaStore.UNKNOWN_STRING.equals(album)
                && MediaStore.UNKNOWN_STRING.equals(artist) && song != null
                && song.startsWith("recording")) {
            // not music
            return false;
        }

        if (audioid < 0) {
            return false;
        }

        Cursor c = MusicUtils.query(this, ContentUris.withAppendedId(
                MediaStore.Audio.Media.EXTERNAL_CONTENT_URI, audioid),
                new String[] { MediaStore.Audio.Media.IS_MUSIC }, null, null,
                null);
        boolean ismusic = true;
        if (c != null) {
            if (c.moveToFirst()) {
                ismusic = c.getInt(0) != 0;
            }
            c.close();
        }
        if (!ismusic) {
            return false;
        }

        boolean knownartist = (artist != null)
                && !MediaStore.UNKNOWN_STRING.equals(artist);

        boolean knownalbum = (album != null)
                && !MediaStore.UNKNOWN_STRING.equals(album);

        if (knownartist && view.equals(mArtistName.getParent())) {
            title = artist;
            query = artist;
            mime = MediaStore.Audio.Artists.ENTRY_CONTENT_TYPE;
        } else if (knownalbum) {
            title = album;
            if (knownartist) {
                query = artist + " " + album;
            } else {
                query = album;
            }
            mime = MediaStore.Audio.Albums.ENTRY_CONTENT_TYPE;
        } else if (view.equals(mTrackName.getParent()) || !knownartist
                || !knownalbum) {
            if ((song == null) || MediaStore.UNKNOWN_STRING.equals(song)) {
                // A popup of the form "Search for null/'' using ..." is pretty
                // unhelpful, plus, we won't find any way to buy it anyway.
                return true;
            }

            title = song;
            if (knownartist) {
                query = artist + " " + song;
            } else {
                query = song;
            }
            // the specific type doesn't matter, so don't bother retrieving it
            mime = "audio/*";
        } else {
            throw new RuntimeException("shouldn't be here");
        }
        title = getString(R.string.mediasearch, title);

        Intent i = new Intent();
        i.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        i.setAction(MediaStore.INTENT_ACTION_MEDIA_SEARCH);
        i.putExtra(SearchManager.QUERY, query);
        if (knownartist) {
            i.putExtra(MediaStore.EXTRA_MEDIA_ARTIST, artist);
        }
        if (knownalbum) {
            i.putExtra(MediaStore.EXTRA_MEDIA_ALBUM, album);
        }
        i.putExtra(MediaStore.EXTRA_MEDIA_TITLE, song);
        i.putExtra(MediaStore.EXTRA_MEDIA_FOCUS, mime);

        startActivity(Intent.createChooser(i, title));
        return true;
    }

    private OnSeekBarChangeListener mSeekListener = new OnSeekBarChangeListener() {
        public void onStartTrackingTouch(SeekBar bar) {
            mLastSeekEventTime = 0;
            mFromTouch = true;
            mCheckIfSeekRequired = false;
        }

        public void onProgressChanged(SeekBar bar, int progress,
                boolean fromuser) {
            if (!fromuser || (mService == null))
                return;
            mCheckIfSeekRequired = true;
            long now = SystemClock.elapsedRealtime();
            mPosOverride = mDuration * progress / 1000;
            if ((now - mLastSeekEventTime) > 10) {
                mLastSeekEventTime = now;

                // trackball event, allow progress updates
                if (!mFromTouch) {
                    refreshNow();
                    mPosOverride = -1;
                }
            }
        }

        public void onStopTrackingTouch(SeekBar bar) {
            try {
                if (null != mService && mCheckIfSeekRequired) {
                    mService.seek(mPosOverride);
                }
            } catch (RemoteException ex) {
            }
            mPosOverride = -1;
            mFromTouch = false;
        }
    };

    public void closeQueuePanel(){
        mSlidingPanelLayout.mIsQueueEnabled = false;
        mAlbumIcon.setVisibility(View.VISIBLE);
        mLyricIcon.setVisibility(View.VISIBLE);
        if (mLandControlPanel != null) {
            mLandControlPanel.setVisibility(View.VISIBLE);
        }
        if (mHasLrc && mIsLyricDisplay) {
            mLyricPanel.setVisibility(View.VISIBLE);
            queueNextRefreshForLyric(REFRESH_MILLIONS);
            setLyricIconImage();
        }
        mFavoriteIcon.setVisibility(View.VISIBLE);
        mMenuOverFlow.setOnClickListener(mActiveButtonPopUpMenuListener);
        mCurrentPlaylist.setImageResource(R.drawable.list);
        if (mFragment != null) {
            MusicUtils.isFragmentRemoved = true;
            getFragmentManager().beginTransaction().remove(mFragment).commitAllowingStateLoss();
        }
    }

    private View.OnClickListener mQueueListener = new View.OnClickListener() {
        public void onClick(View v) {
            MusicBrowserActivity.mIsparentActivityFInishing = true;
            if (mAlbumIcon.getVisibility() == View.GONE) {
                mSlidingPanelLayout.mIsQueueEnabled = false;
                mAlbumIcon.setVisibility(View.VISIBLE);
                mLyricIcon.setVisibility(View.VISIBLE);
                mFavoriteIcon.setVisibility(View.VISIBLE);
                mMenuOverFlow.setOnClickListener(mActiveButtonPopUpMenuListener);
                mCurrentPlaylist.setImageResource(R.drawable.list);
                if (mFragment != null) {
                    MusicUtils.isFragmentRemoved = true;
                    getFragmentManager().beginTransaction().remove(mFragment)
                            .commit();
                }
                if (mHasLrc && mIsLyricDisplay) {
                    mLyricPanel.setVisibility(View.VISIBLE);
                    queueNextRefreshForLyric(REFRESH_MILLIONS);
                    setLyricIconImage();
                }
                if (mLandControlPanel != null) {
                    mLandControlPanel.setVisibility(View.VISIBLE);
                }
            } else {
                MusicUtils.isFragmentRemoved = false;
                mSlidingPanelLayout.mIsQueueEnabled = true;
                mAlbumIcon.setVisibility(View.GONE);
                mLyricIcon.setVisibility(View.GONE);
                mFavoriteIcon.setVisibility(View.GONE);
                mCurrentPlaylist.setImageResource(R.drawable.list_active);
                mMenuOverFlow.setOnClickListener(mPopUpMenuListener);
                mFragment = new TrackBrowserFragment();
                Bundle args = new Bundle();
                args.putString("playlist", "nowplaying");
                args.putBoolean("editValue", true);
                mFragment.setArguments(args);
                getFragmentManager()
                        .beginTransaction()
                        .add(R.id.current_queue_view, mFragment,
                                "track_fragment").commit();
                if (mHasLrc) {
                    mLyricPanel.setVisibility(View.GONE);
                    stopQueueNextRefreshForLyric();
                }
                if (mLandControlPanel != null) {
                    mLandControlPanel.setVisibility(View.GONE);
                }
            }
        }
    };

    private View.OnClickListener mFavoriteListener = new View.OnClickListener() {
        public void onClick(View v) {
            long audioid;
            try {
                audioid = mService.getAudioId();
            } catch (Exception ex) {
                return;
            }

            int playlistId = MusicUtils.idForplaylist(MediaPlaybackActivity.this, "My Favorite");
            int memberId = getMemberId(playlistId, audioid);
            if (memberId == -1) {
                addToFavoritePlaylist(playlistId, audioid);
            } else {
                removeFromPlaylist(playlistId, memberId);
            }
        }
    };

    private int getMemberId(int playlistId, long id) {
        Uri uri = MediaStore.Audio.Playlists.Members.getContentUri(
                "external", Long.valueOf(playlistId));

        StringBuilder where = new StringBuilder();
        where.append(MediaStore.Audio.Playlists.Members.AUDIO_ID + " = ");
        where.append(id);

        String[] mCursorCols = new String[] {
                MediaStore.Audio.Playlists.Members._ID
        };
        Cursor cursor = null;
        int memberId = -1;
        if (playlistId == -1) {
            return memberId;
        }
        try {
            cursor = getContentResolver().query(uri, mCursorCols, where.toString(), null, null);
            if (cursor != null && cursor.getCount() > 0) {
                cursor.moveToFirst();
                memberId = cursor.getInt(0);
            }
        } catch (Exception e) {
        } finally {
            if (cursor != null) {
                cursor.close();
            }
        }

        return memberId;
    }

    private void removeFromPlaylist(int playlistId, long memberId) {

        Uri uri = MediaStore.Audio.Playlists.Members.getContentUri(
                "external", Long.valueOf(playlistId));

        getContentResolver().delete(
                ContentUris.withAppendedId(uri, memberId), null, null);
        setFavoriteIconImage(false);

    }

    private void addToFavoritePlaylist(int playlistId, long audioId) {

        long[] list = new long[] {
                audioId
        };

        setFavoriteIconImage(true);

        Fragment fragment = getFragmentManager().findFragmentByTag("track_fragment");
        if (fragment != null) {
            View view = mActivity.findViewById(R.id.sd_message);
            if (view != null) {
                view.setVisibility(View.GONE);
            }
            view = mActivity.findViewById(R.id.sd_icon);
            if (view != null) {
                view.setVisibility(View.GONE);
            }
            view = mActivity.findViewById(R.id.list);
            if (view != null) {
                view.setVisibility(View.VISIBLE);
            }
        }

        MusicUtils.addToPlaylist(this, list,playlistId);

    }

    private boolean getFavoriteStatus() {
        long audioid;
        try {
            audioid = mService.getAudioId();
        } catch (Exception ex) {
            return false;
        }
        int playlistId = MusicUtils.idForplaylist(MediaPlaybackActivity.this, "My Favorite");
        if (playlistId == INVALID_PLAYLIST_ID) {
            return false;
        }
        int memberId = getMemberId(playlistId, audioid);
        if (memberId == -1) {
            return false;
        } else {
            return true;
        }

    }

    private View.OnClickListener mShuffleListener = new View.OnClickListener() {
        public void onClick(View v) {
            toggleShuffle();
        }
    };

    private View.OnClickListener mRepeatListener = new View.OnClickListener() {
        public void onClick(View v) {
            cycleRepeat();
        }
    };

    private View.OnClickListener mSoundEffectListener = new View.OnClickListener() {
        public void onClick(View v) {
            MusicUtils.startSoundEffectActivity(MediaPlaybackActivity.this);
        }
    };

    public View.OnClickListener mPauseListener = new View.OnClickListener() {
        public void onClick(View v) {
            if (MusicUtils.isForbidPlaybackInCall(MediaPlaybackActivity.this)){
                return;
            }
            doPauseResume();
        }
    };

    private View.OnClickListener mPrevListener = new View.OnClickListener() {
        public void onClick(View v) {
            if (mService == null)
                return;
            if (MusicUtils.isForbidPlaybackInCall(MediaPlaybackActivity.this)){
                return;
            }
            sendIsRespondMessage(GO_PRE);
        }
    };

    private View.OnClickListener mNextListener = new View.OnClickListener() {
        public void onClick(View v) {
            if (mService == null)
                return;
            if (MusicUtils.isForbidPlaybackInCall(MediaPlaybackActivity.this)){
                return;
            }
            sendIsRespondMessage(GO_NEXT);
        }
    };

    private void sendIsRespondMessage(int msgId) {
        if (!mHandler.hasMessages(msgId)) {
            mHandler.sendEmptyMessage(msgId);
        }
    }

    private View.OnClickListener mPopUpMenuListener = new View.OnClickListener() {
        public void onClick(View v) {
            if (mPopupMenu != null) {
                mPopupMenu.dismiss();
            }
            PopupMenu popup = new PopupMenu(mActivity, mMenuOverFlow);
            // icon will be set in onPrepareOptionsMenu()
            popup.getMenu()
                    .add(0, SAVE_AS_PLAYLIST, 0, R.string.save_as_playlist)
                    .setIcon(android.R.drawable.ic_menu_save);
            popup.getMenu().add(0, CLEAR_PLAYLIST, 0, R.string.clear_playlist)
                    .setIcon(R.drawable.ic_menu_clear_playlist);
            popup.show();
            popup.setOnMenuItemClickListener(new OnMenuItemClickListener() {

                @Override
                public boolean onMenuItemClick(MenuItem item) {
                    onOptionsItemSelected(item);
                    return true;
                }
            });
            mPopupMenu = popup;
        }
    };

    private View.OnClickListener mActiveButtonPopUpMenuListener = new View.OnClickListener() {
        public void onClick(View v) {
            if (mPopupMenu != null) {
                mPopupMenu.dismiss();
            }
            PopupMenu popup = new PopupMenu(mActivity, mMenuOverFlow);
            // icon will be set in onPrepareOptionsMenu()
            SubMenu sub = popup.getMenu().addSubMenu(0,
                    ADD_TO_PLAYLIST, 0, R.string.add_to_playlist);
            MusicUtils.makePlaylistMenu(MediaPlaybackActivity.this, sub);
            MusicUtils.addSetRingtonMenu(popup.getMenu());
            Intent i = new Intent(AudioEffect.ACTION_DISPLAY_AUDIO_EFFECT_CONTROL_PANEL);
            if (getPackageManager().resolveActivity(i, 0) != null) {
                popup.getMenu().add(0, EFFECTS_PANEL, 0, R.string.effectspanel);
            }
            popup.getMenu()
            .add(0, DELETE_ITEM, 0, R.string.delete_item);
            popup.show();
            popup.setOnMenuItemClickListener(new OnMenuItemClickListener() {

                @Override
                public boolean onMenuItemClick(MenuItem item) {
                    onOptionsItemSelected(item);
                    return true;
                }
            });
            mPopupMenu = popup;
        }
    };

    private RepeatingImageButton.RepeatListener mRewListener = new RepeatingImageButton.RepeatListener() {
        public void onRepeat(View v, long howlong, int repcnt) {
            scanBackward(repcnt, howlong);
        }
    };

    private RepeatingImageButton.RepeatListener mFfwdListener = new RepeatingImageButton.RepeatListener() {
        public void onRepeat(View v, long howlong, int repcnt) {
            scanForward(repcnt, howlong);
        }
    };

    public void setTouchDelegate(final View v) {
        final View parent = (View) v.getParent();
        parent.post(new Runnable() {
            // Post in the parent's message queue to make sure the parent
            // lays out its children before we call getHitRect()
            public void run() {
                Rect r = new Rect();
                v.getHitRect(r);
                r.top -= 24;
                r.bottom += 24;
                parent.setTouchDelegate(new TouchDelegate(r, v));
            }
        });
    }

    @Override
    public void onStop() {
        paused = true;
        unregisterReceiver(mScreenTimeoutListener);
        if (!mReceiverUnregistered) {
            mHandler.removeMessages(REFRESH);
            unregisterReceiver(mStatusListener);
            mReceiverUnregistered = true;
        }
        MusicUtils.unbindFromService(mToken);
        mService = null;
        super.onStop();
    }

    @Override
    public void onStart() {
        super.onStart();
        paused = false;
        mToken = MusicUtils.bindToService(this, this);
        if (mToken == null) {
            // something went wrong
            mHandler.sendEmptyMessage(QUIT);
        }

        IntentFilter f = new IntentFilter();
        f.addAction(MediaPlaybackService.PLAYSTATE_CHANGED);
        f.addAction(MediaPlaybackService.META_CHANGED);
        f.addAction(MediaPlaybackService.SHUFFLE_CHANGED);
        f.addAction(MediaPlaybackService.REPEAT_CHANGED);
        registerReceiver(mStatusListener, f);
        mReceiverUnregistered = false;

        IntentFilter s = new IntentFilter();
        s.addAction(Intent.ACTION_SCREEN_ON);
        s.addAction(Intent.ACTION_SCREEN_OFF);
        registerReceiver(mScreenTimeoutListener, s);
        updateTrackInfo();
        long next = refreshNow();
        queueNextRefresh(next);
    }

    @Override
    public void onNewIntent(Intent intent) {
        setIntent(intent);
    }

    @Override
    public void onResume() {
        super.onResume();
        mActivity = this;
        updateNowPlaying(this);
        updateTrackInfo();
        queueNextRefreshForLyric(REFRESH_MILLIONS);
    }

    @Override
    public void onPause() {
        super.onPause();
        stopQueueNextRefreshForLyric();
    }

    @Override
    public void onDestroy() {
        mAlbumArtWorker.quit();
        unregisterReceiver(mTrackListListener);
        if (mLyricAdapter != null) {
            mLyricAdapter.release();
        }
        if (mLyric != null) {
            mLyric.release();
        }
        mLyricPanel = null;

        unregisterReceiver(mMediaButtonIntentReceiver);
        super.onDestroy();
    }

    public boolean onOptionsItemSelected(MenuItem item) {
        Intent intent;
        try {
            switch (item.getItemId()) {
            case GOTO_START:
                intent = new Intent();
                intent.setClass(this, MusicBrowserActivity.class);
                intent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP
                        | Intent.FLAG_ACTIVITY_NEW_TASK);
                startActivity(intent);
                finish();
                break;
            case USE_AS_RINGTONE: {
                // Set the system setting to make this the current ringtone
                if (mService != null) {
                    MusicUtils.setRingtone(this, mService.getAudioId());
                }
                return true;
            }
            case USE_AS_RINGTONE_2: {
                // Set the system setting to make this the current ringtone for
                // SUB_1
                if (mService != null) {
                    MusicUtils.setRingtone(this, mService.getAudioId(),
                            MusicUtils.RINGTONE_SUB_1);
                }
                return true;
            }

            case SHUFFLE_ALL:
                Cursor cursor = MusicUtils.query(this,
                        MediaStore.Audio.Media.EXTERNAL_CONTENT_URI,
                        new String[] { MediaStore.Audio.Media._ID },
                        MediaStore.Audio.Media.IS_MUSIC + "=1", null,
                        MediaStore.Audio.Media.DEFAULT_SORT_ORDER);
                if (cursor != null) {
                    MusicUtils.shuffleAll(this, cursor);
                    cursor.close();
                }
                return true;

            case PARTY_SHUFFLE:
                MusicUtils.togglePartyShuffle();
                setShuffleButtonImage();
                setRepeatButtonImage();
                break;

            case NEW_PLAYLIST: {
                intent = new Intent();
                intent.setClass(this, CreatePlaylist.class);
                startActivityForResult(intent, NEW_PLAYLIST);
                return true;
            }

            case SAVE_AS_PLAYLIST:
                intent = new Intent();
                intent.setClass(this, CreatePlaylist.class);
                startActivityForResult(intent, SAVE_AS_PLAYLIST);
                return true;

            case CLEAR_PLAYLIST:
                // We only clear the current playlist
                MusicUtils.clearQueue();
                if (mIsPanelExpanded) {
                    mSlidingPanelLayout.setHookState(BoardState.HIDDEN);
                    mIsPanelExpanded = false;
                    mNowPlayingView.setVisibility(View.GONE);
                }
                return true;

            case PLAYLIST_SELECTED: {
                long[] list = new long[1];
                list[0] = MusicUtils.getCurrentAudioId();
                long playlist = item.getIntent().getLongExtra("playlist", 0);
                MusicUtils.addToPlaylist(this, list, playlist);
                return true;
            }

            case DELETE_ITEM: {
                if (mService != null) {
                    long[] list = new long[1];
                    list[0] = MusicUtils.getCurrentAudioId();
                    Bundle bundle = new Bundle();
                    String filter = getString(R.string.delete_song_desc,
                            mService.getTrackName());
                    bundle.putString("description", filter);
                    bundle.putLongArray("items", list);
                    intent = new Intent();
                    intent.setClass(this, DeleteItems.class);
                    intent.putExtras(bundle);
                    startActivityForResult(intent, -1);
                }
                return true;
            }

            case EFFECTS_PANEL:
                MusicUtils.startSoundEffectActivity(MediaPlaybackActivity.this);
                return true;

            case CLOSE: {
                try {
                    if (MusicUtils.sService != null) {
                        MusicUtils.sService.stop();
                    }
                } catch (RemoteException ex) {
                }
                SysApplication.getInstance().exit();
                return true;
            }
            }
        } catch (RemoteException ex) {
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode,
            Intent intent) {
        if (resultCode != RESULT_OK) {
            return;
        }
        switch (requestCode) {
        case NEW_PLAYLIST:
            if (resultCode == RESULT_OK) {
                long songID = -1;
                Uri uri = intent.getData();
                try {
                    if (mService != null) {
                        songID = mService.getAudioId();
                        if (songID != -1) {
                            if (uri != null) {
                                long[] list = new long[] { songID };
                                int playlist = Integer.parseInt(uri
                                        .getLastPathSegment());
                                MusicUtils.addToPlaylist(this, list, playlist);
                            }
                        } else {
                            Log.e(TAG,"Audio ID is -1");
                        }
                    }
                } catch (RemoteException e) {
                    Log.e(TAG,"Remote Exception is caught");
                }
            }
            break;
        case SAVE_AS_PLAYLIST:
            if (resultCode == RESULT_OK) {
                Uri uri = intent.getData();
                if (uri != null) {
                    Cursor c = null;
                    try {
                        c = updateTrackCursor();
                        if (c != null) {
                            long[] list = MusicUtils
                                    .getSongListForCursor(c);
                            int plid = Integer.parseInt(uri.getLastPathSegment());
                            MusicUtils.addToPlaylist(this, list, plid);
                        }
                    } catch (Exception e) {
                    } finally {
                        if (c != null) {
                            c.close();
                        }
                    }
                }
            }
            break;
        }
    }

    public Cursor updateTrackCursor() {
        String[] cols = new String[] { MediaStore.Audio.Media._ID,
                MediaStore.Audio.Media.TITLE, MediaStore.Audio.Media.DATA,
                MediaStore.Audio.Media.ALBUM, MediaStore.Audio.Media.ARTIST,
                MediaStore.Audio.Media.ARTIST_ID,
                MediaStore.Audio.Media.DURATION };
        Cursor currentPlaylistCursor;
        long[] nowPlaying;
        int size;
        try {
            nowPlaying = mService.getQueue();
        } catch (RemoteException ex) {
            nowPlaying = new long[0];
        }
        size = nowPlaying.length;
        if (size == 0) {
            return null;
        }
        StringBuilder where = new StringBuilder();
        where.append(MediaStore.Audio.Media._ID + " IN (");
        for (int i = 0; i < size; i++) {
            where.append(nowPlaying[i]);
            if (i < size - 1) {
                where.append(",");
            }
        }
        where.append(")");
        currentPlaylistCursor = MusicUtils.query(this,
                MediaStore.Audio.Media.EXTERNAL_CONTENT_URI, cols,
                where.toString(), null, MediaStore.Audio.Media._ID);
        return currentPlaylistCursor;
    }

    public void updateNowPlaying(Activity a) {
        MusicUtils.updateNowPlaying(a, mIsPanelExpanded);
        setFavoriteIconImage(getFavoriteStatus());
    }

    public static int getStatusBarHeight() {
        Rect rectangle = new Rect();
        Window window = MediaPlaybackActivity.getInstance().getWindow();
        window.getDecorView().getWindowVisibleDisplayFrame(rectangle);
        return rectangle.top;
    }

    public void loadPreviousFragment() {
        Fragment fragment = FragmentsFactory
                .loadFragment((int) MusicUtils.navigatingTabPosition);
        if (!fragment.isAdded()) {
            getFragmentManager().beginTransaction()
                    .replace(R.id.fragment_page, fragment).commit();
            mToolbar.setVisibility(View.VISIBLE);
            if (MusicUtils.isGroupByFolder()) {
                mToolbar.setTitle(getResources().getStringArray(
                        R.array.title_array_folder)[MusicUtils.navigatingTabPosition]);
            } else {
                mToolbar.setTitle(getResources().getStringArray(
                        R.array.title_array_songs)[MusicUtils.navigatingTabPosition]);

            }

        }
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (event.getAction() == KeyEvent.ACTION_UP
                && event.getKeyCode() == KeyEvent.KEYCODE_BACK) {
            FragmentManager fragmentManager = getFragmentManager();
            fragmentManager.executePendingTransactions();
            if (mIsPanelExpanded) {
                mSlidingPanelLayout.setHookState(BoardState.COLLAPSED);
                mIsPanelExpanded = false;
                return true;
            } else if (MusicUtils.canClosePlaylistItemFragment(fragmentManager)) {
                loadPreviousFragment();
                return true;
            }

            MusicUtils.isLaunchedFromQueryBrowser = false;
            if (MusicUtils.isLaunchedFromQueryBrowser) {
                MusicUtils.isLaunchedFromQueryBrowser = false;
                Intent intent = new Intent(MediaPlaybackActivity.this,
                        QueryBrowserActivity.class);
                startActivity(intent);
                return true;
            }
            if (isBackPressed) {
                isBackPressed = false;
                finish();
            }
            return true;
        } else if (event.getAction() == KeyEvent.ACTION_DOWN
                && event.getKeyCode() == KeyEvent.KEYCODE_BACK) {
            isBackPressed = true;
            return true;
        }
        return super.dispatchKeyEvent(event);
    }

    private final int keyboard[][] = {
            { KeyEvent.KEYCODE_Q, KeyEvent.KEYCODE_W, KeyEvent.KEYCODE_E,
                    KeyEvent.KEYCODE_R, KeyEvent.KEYCODE_T, KeyEvent.KEYCODE_Y,
                    KeyEvent.KEYCODE_U, KeyEvent.KEYCODE_I, KeyEvent.KEYCODE_O,
                    KeyEvent.KEYCODE_P, },
            { KeyEvent.KEYCODE_A, KeyEvent.KEYCODE_S, KeyEvent.KEYCODE_D,
                    KeyEvent.KEYCODE_F, KeyEvent.KEYCODE_G, KeyEvent.KEYCODE_H,
                    KeyEvent.KEYCODE_J, KeyEvent.KEYCODE_K, KeyEvent.KEYCODE_L,
                    KeyEvent.KEYCODE_DEL, },
            { KeyEvent.KEYCODE_Z, KeyEvent.KEYCODE_X, KeyEvent.KEYCODE_C,
                    KeyEvent.KEYCODE_V, KeyEvent.KEYCODE_B, KeyEvent.KEYCODE_N,
                    KeyEvent.KEYCODE_M, KeyEvent.KEYCODE_COMMA,
                    KeyEvent.KEYCODE_PERIOD, KeyEvent.KEYCODE_ENTER }

    };

    private int lastX;
    private int lastY;

    private boolean seekMethod1(int keyCode) {
        if (mService == null)
            return false;

        if (MusicUtils.isForbidPlaybackInCall(this)){
            return false;
        }

        for (int x = 0; x < 10; x++) {
            for (int y = 0; y < 3; y++) {
                if (keyboard[y][x] == keyCode) {
                    int dir = 0;
                    // top row
                    if (x == lastX && y == lastY)
                        dir = 0;
                    else if (y == 0 && lastY == 0 && x > lastX)
                        dir = 1;
                    else if (y == 0 && lastY == 0 && x < lastX)
                        dir = -1;
                    // bottom row
                    else if (y == 2 && lastY == 2 && x > lastX)
                        dir = -1;
                    else if (y == 2 && lastY == 2 && x < lastX)
                        dir = 1;
                    // moving up
                    else if (y < lastY && x <= 4)
                        dir = 1;
                    else if (y < lastY && x >= 5)
                        dir = -1;
                    // moving down
                    else if (y > lastY && x <= 4)
                        dir = -1;
                    else if (y > lastY && x >= 5)
                        dir = 1;
                    lastX = x;
                    lastY = y;
                    try {
                        mService.seek(mService.position() + dir * 5);
                    } catch (RemoteException ex) {
                    }
                    refreshNow();
                    return true;
                }
            }
        }
        lastX = -1;
        lastY = -1;
        return false;
    }

    private boolean seekMethod2(int keyCode) {
        if (mService == null)
            return false;

        if (MusicUtils.isForbidPlaybackInCall(this)){
            return false;
        }

        for (int i = 0; i < 10; i++) {
            if (keyboard[0][i] == keyCode) {
                int seekpercentage = 100 * i / 10;
                try {
                    mService.seek(mService.duration() * seekpercentage / 100);
                } catch (RemoteException ex) {
                }
                refreshNow();
                return true;
            }
        }
        return false;
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        try {
            switch (keyCode) {
            case KeyEvent.KEYCODE_DPAD_LEFT:
                if (!useDpadMusicControl()) {
                    break;
                }
                if (mService != null) {
                    if (MusicUtils.isForbidPlaybackInCall(this)){
                        return false;
                    }

                    if (!mSeeking && mStartSeekPos >= 0) {
                        mPauseButton.requestFocus();
                        if (mStartSeekPos < 1000) {
                            mService.prev();
                        } else {
                            mService.seek(0);
                        }
                    } else {
                        scanBackward(-1,
                                event.getEventTime() - event.getDownTime());
                        mPauseButton.requestFocus();
                        mStartSeekPos = -1;
                    }
                }
                mSeeking = false;
                mPosOverride = -1;
                return true;
            case KeyEvent.KEYCODE_DPAD_RIGHT:
                if (!useDpadMusicControl()) {
                    break;
                }

                if (mService != null) {
                    if (MusicUtils.isForbidPlaybackInCall(this)){
                        return false;
                    }
                    if (!mSeeking && mStartSeekPos >= 0) {
                        mPauseButton.requestFocus();
                        mService.next();
                    } else {
                        scanForward(-1,
                                event.getEventTime() - event.getDownTime());
                        mPauseButton.requestFocus();
                        mStartSeekPos = -1;
                    }
                }
                mSeeking = false;
                mPosOverride = -1;
                return true;
            }
        } catch (RemoteException ex) {
        }
        return super.onKeyUp(keyCode, event);
    }

    private boolean useDpadMusicControl() {

        if (mDeviceHasDpad && (mPrevButton.isFocused() ||
                mNextButton.isFocused() ||
                mPauseButton.isFocused())) {
            return true;
        }

        return false;
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        int direction = -1;
        int repcnt = event.getRepeatCount();

        if (keyCode >= KeyEvent.KEYCODE_BUTTON_2
                && keyCode <= KeyEvent.KEYCODE_BUTTON_8) {
            return true;
        }
        if ((seekmethod == 0) ? seekMethod1(keyCode) : seekMethod2(keyCode))
            return true;

        switch (keyCode) {
        /*
         * // image scale case KeyEvent.KEYCODE_Q: av.adjustParams(-0.05, 0.0,
         * 0.0, 0.0, 0.0,-1.0); break; case KeyEvent.KEYCODE_E: av.adjustParams(
         * 0.05, 0.0, 0.0, 0.0, 0.0, 1.0); break; // image translate case
         * KeyEvent.KEYCODE_W: av.adjustParams( 0.0, 0.0,-1.0, 0.0, 0.0, 0.0);
         * break; case KeyEvent.KEYCODE_X: av.adjustParams( 0.0, 0.0, 1.0, 0.0,
         * 0.0, 0.0); break; case KeyEvent.KEYCODE_A: av.adjustParams( 0.0,-1.0,
         * 0.0, 0.0, 0.0, 0.0); break; case KeyEvent.KEYCODE_D: av.adjustParams(
         * 0.0, 1.0, 0.0, 0.0, 0.0, 0.0); break; // camera rotation case
         * KeyEvent.KEYCODE_R: av.adjustParams( 0.0, 0.0, 0.0, 0.0, 0.0,-1.0);
         * break; case KeyEvent.KEYCODE_U: av.adjustParams( 0.0, 0.0, 0.0, 0.0,
         * 0.0, 1.0); break; // camera translate case KeyEvent.KEYCODE_Y:
         * av.adjustParams( 0.0, 0.0, 0.0, 0.0,-1.0, 0.0); break; case
         * KeyEvent.KEYCODE_N: av.adjustParams( 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);
         * break; case KeyEvent.KEYCODE_G: av.adjustParams( 0.0, 0.0, 0.0,-1.0,
         * 0.0, 0.0); break; case KeyEvent.KEYCODE_J: av.adjustParams( 0.0, 0.0,
         * 0.0, 1.0, 0.0, 0.0); break;
         */

        case KeyEvent.KEYCODE_SLASH:
            seekmethod = 1 - seekmethod;
            return true;

        case KeyEvent.KEYCODE_DPAD_LEFT:
            if (!useDpadMusicControl()) {
                break;
            }
            if (!mPrevButton.hasFocus()) {
                mPrevButton.requestFocus();
            }
            scanBackward(repcnt, event.getEventTime() - event.getDownTime());
            return true;
        case KeyEvent.KEYCODE_DPAD_RIGHT:
            if (!useDpadMusicControl()) {
                break;
            }
            if (!mNextButton.hasFocus()) {
                mNextButton.requestFocus();
            }
            scanForward(repcnt, event.getEventTime() - event.getDownTime());
            return true;

        case KeyEvent.KEYCODE_S:
            toggleShuffle();
            return true;

        case KeyEvent.KEYCODE_DPAD_CENTER:
        case KeyEvent.KEYCODE_SPACE:
            doPauseResume();
            return true;
        }
        return super.onKeyDown(keyCode, event);
    }

    private void scanBackward(int repcnt, long delta) {
        if (MusicUtils.isForbidPlaybackInCall(this)){
            return;
        }

        if (mService == null) {
            if (repcnt < 0)
                mPosOverride = -1;
            return;
        }
        try {
            if (repcnt == 0) {
                mStartSeekPos = mService.position();
                mLastSeekEventTime = 0;
                mSeeking = false;
            } else {
                mSeeking = true;
                if (delta < 5000) {
                    // seek at 10x speed for the first 5 seconds
                    delta = delta * 10;
                } else {
                    // seek at 40x after that
                    delta = 50000 + (delta - 5000) * 40;
                }
                long newpos = mStartSeekPos - delta;
                if (newpos < 0) {
                    // move to previous track
                    mService.prev();
                    long duration = mService.duration();
                    mStartSeekPos += duration;
                    newpos += duration;
                }
                if (((delta - mLastSeekEventTime) > 250) || repcnt < 0) {
                    mService.seek(newpos);
                    mLastSeekEventTime = delta;
                }
                if (repcnt >= 0) {
                    mPosOverride = newpos;
                } else {
                    mPosOverride = -1;
                }
                refreshNow();
            }
        } catch (RemoteException ex) {
        }
    }

    private void scanForward(int repcnt, long delta) {
        if (MusicUtils.isForbidPlaybackInCall(this)){
            return;
        }

        if (mService == null) {
            if (repcnt < 0)
                mPosOverride = -1;
            return;
        }
        try {
            if (repcnt == 0) {
                mStartSeekPos = mService.position();
                mLastSeekEventTime = 0;
                mSeeking = false;
            } else {
                mSeeking = true;
                if (delta < 5000) {
                    // seek at 10x speed for the first 5 seconds
                    delta = delta * 10;
                } else {
                    // seek at 40x after that
                    delta = 50000 + (delta - 5000) * 40;
                }
                long newpos = mStartSeekPos + delta;
                long duration = mService.duration();
                if (newpos >= duration) {
                    // move to next track
                    mService.next();
                    mStartSeekPos -= duration; // is OK to go negative
                    newpos -= duration;

                    // boundary check to ensure newpos not exceed duration of
                    // new track
                    duration = mService.duration();
                    if (newpos > duration) {
                        newpos = duration;
                    }
                }
                if (((delta - mLastSeekEventTime) > 250) || repcnt < 0) {
                    mService.seek(newpos);
                    mLastSeekEventTime = delta;
                }
                if (repcnt >= 0) {
                    mPosOverride = newpos;
                } else {
                    mPosOverride = -1;
                }
                refreshNow();
            }
        } catch (RemoteException ex) {
        }
    }

    private void doPauseResume() {
        try {
            if (mService != null) {
                if (mService.isPlaying()) {
                    mService.pause();
                } else {
                    mService.play();
                }
                refreshNow();
                setPauseButtonImage();
            }
        } catch (RemoteException ex) {
        }
    }

    private void toggleShuffle() {
        if (mService == null) {
            return;
        }
        try {
            int shuffle = mService.getShuffleMode();
            if (shuffle == MediaPlaybackService.SHUFFLE_NONE) {
                mService.setShuffleMode(MediaPlaybackService.SHUFFLE_NORMAL);
                if (mService.getRepeatMode() == MediaPlaybackService.REPEAT_CURRENT) {
                    mService.setRepeatMode(MediaPlaybackService.REPEAT_ALL);
                    setRepeatButtonImage();
                }
                showToast(R.string.shuffle_on_notif);
            } else if (shuffle == MediaPlaybackService.SHUFFLE_NORMAL
                    || shuffle == MediaPlaybackService.SHUFFLE_AUTO) {
                mService.setShuffleMode(MediaPlaybackService.SHUFFLE_NONE);
                showToast(R.string.shuffle_off_notif);
            } else {
                Log.e("MediaPlaybackActivity", "Invalid shuffle mode: "
                        + shuffle);
            }
            setShuffleButtonImage();
        } catch (RemoteException ex) {
        }
    }

    private void cycleRepeat() {
        if (mService == null) {
            return;
        }
        try {
            int mode = mService.getRepeatMode();
            if (mode == MediaPlaybackService.REPEAT_NONE) {
                mService.setRepeatMode(MediaPlaybackService.REPEAT_ALL);
                showToast(R.string.repeat_all_notif);
            } else if (mode == MediaPlaybackService.REPEAT_ALL) {
                mService.setRepeatMode(MediaPlaybackService.REPEAT_CURRENT);
                if (mService.getShuffleMode() != MediaPlaybackService.SHUFFLE_NONE) {
                    mService.setShuffleMode(MediaPlaybackService.SHUFFLE_NONE);
                    setShuffleButtonImage();
                }
                showToast(R.string.repeat_current_notif);
            } else {
                mService.setRepeatMode(MediaPlaybackService.REPEAT_NONE);
                showToast(R.string.repeat_off_notif);
            }
            setRepeatButtonImage();
        } catch (RemoteException ex) {
        }

    }

    private void showToast(int resid) {
        if (mToast == null) {
            mToast = Toast.makeText(this, "", Toast.LENGTH_SHORT);
        }
        mToast.setText(resid);
        mToastHandler.postDelayed(mRun, 600);
        mToast.show();
    }

    private Handler mToastHandler = new Handler();
    private Runnable mRun = new Runnable() {
        public void run() {
            mToast.cancel();
        }
    };

    private void startPlayback() {
        if (MusicUtils.isForbidPlaybackInCall(this)){
            return;
        }

        if (mService == null)
            return;
        Intent intent = getIntent();
        String filename = "";
        Uri uri = intent.getData();
        if (uri != null && uri.toString().length() > 0) {
            // If this is a file:// URI, just use the path directly instead
            // of going through the open-from-filedescriptor codepath.
            String scheme = uri.getScheme();
            if ("file".equals(scheme)) {
                filename = uri.getPath();
            } else {
                filename = uri.toString();
            }
            try {
                mService.stop();
                mService.openFile(filename);
                mService.play();
                setIntent(new Intent());
            } catch (Exception ex) {
                Log.d("MediaPlaybackActivity", "couldn't start playback: " + ex);
            }
        }

        updateTrackInfo();
        long next = refreshNow();
        queueNextRefresh(next);
    }

    private BroadcastReceiver mTrackListListener = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            try {
                if (mService != null) {
                    if (mService.getAudioId() >= 0 || mService.isPlaying()
                            || mService.getPath() != null) {
                        mNowPlayingView.setVisibility(View.VISIBLE);
                        if (mSlidingPanelLayout.getHookState() != BoardState.EXPANDED
                                      && (mSlidingPanelLayout.getHookState() != BoardState.COLLAPSED)) {
                           mSlidingPanelLayout
                                    .setHookState(BoardState.COLLAPSED);
                        }
                    }
                }
            } catch (RemoteException e) {
                Log.e(TAG, "Error caught");
            }
            updateNowPlaying(MediaPlaybackActivity.this);
        }
    };

    @Override
    public void onServiceConnected(ComponentName classname, IBinder obj) {
        updateNowPlaying(MediaPlaybackActivity.this);
        mService = IMediaPlaybackService.Stub.asInterface(obj);
        startPlayback();
        try {
            // Assume something is playing when the service says it is,
            // but also if the audio ID is valid but the service is paused.
            if (mService.getAudioId() >= 0 || mService.isPlaying()
                    || mService.getPath() != null) {
                // something is playing now, we're done
                mRepeatButton.setVisibility(View.VISIBLE);
                mShuffleButton.setVisibility(View.VISIBLE);
                setRepeatButtonImage();
                setShuffleButtonImage();
                setPauseButtonImage();
                setFavoriteIconImage(getFavoriteStatus());
                return;
            } else {
                mSlidingPanelLayout.setHookState(BoardState.HIDDEN);
                mNowPlayingView.setVisibility(View.GONE);
            }
        } catch (RemoteException ex) {
        }
    }

    @Override
    public void onServiceDisconnected(ComponentName classname) {
        mService = null;
    }

    private void setRepeatButtonImage() {
        if (mService == null)
            return;
        try {
            switch (mService.getRepeatMode()) {
            case MediaPlaybackService.REPEAT_ALL:
                mRepeatButton.setImageResource(R.drawable.loopall);
                break;
            case MediaPlaybackService.REPEAT_CURRENT:
                mRepeatButton
                        .setImageResource(R.drawable.loopone);
                break;
            default:
                mRepeatButton.setImageResource(R.drawable.normal);
                break;
            }
        } catch (RemoteException ex) {
        }
    }

    private void setShuffleButtonImage() {
        if (mService == null)
            return;
        try {
            switch (mService.getShuffleMode()) {
            case MediaPlaybackService.SHUFFLE_NONE:
                mShuffleButton.setImageResource(R.drawable.random);
                break;
            case MediaPlaybackService.SHUFFLE_AUTO:
                mShuffleButton.setImageResource(R.drawable.random_active);
                break;
            default:
                mShuffleButton.setImageResource(R.drawable.random_active);
                break;
            }
        } catch (RemoteException ex) {
        }
    }

    private void setPauseButtonImage() {
        try {
            if (mService != null && mService.isPlaying()) {
                mPauseButton.setImageResource(R.drawable.pause);
                mNowPlayingIcon.setImageResource(R.drawable.play_pause);
            } else {
                mPauseButton.setImageResource(R.drawable.play);
                mNowPlayingIcon.setImageResource(R.drawable.play_arrow);
            }
        } catch (RemoteException ex) {
        }
        updateLyricDisplay();
    }

    private void updateLyricDisplay() {
        try {
            if (mService != null && mService.isPlaying()) {
                if (mIsLyricDisplay) {
                    String trackName = mService.getTrackName();
                    if (!mHasLrc || (mMetaChanged && trackName != null
                            && !trackName.equals(mStrTrackName))) {
                        setLyric(mService.getData(), trackName);
                    } else {
                        mMetaChanged = false;
                    }
                    if (mHasLrc) {
                        queueNextRefreshForLyric(REFRESH_MILLIONS);
                    }
                } else {
                    mHasLrc = hasLyric(mService.getTrackName());
                }
            } else {
                stopQueueNextRefreshForLyric();
            }
            setLyricIconStatusAndImage(mHasLrc, mIsLyricDisplay);
            if (mHasLrc && mIsLyricDisplay && mLyricPanel.getVisibility() != View.VISIBLE &&
                    MusicUtils.isFragmentRemoved) {
                mLyricPanel.setVisibility(View.VISIBLE);
            }
        } catch (RemoteException ex) {
        }
    }

    private ImageView mAlbum;
    private ImageView mAlbumIcon;
    private TextView mCurrentTime;
    private TextView mTotalTime;
    private TextView mArtistName;
    private TextView mTrackName;
    private ProgressBar mProgress;
    private long mPosOverride = -1;
    private boolean mFromTouch = false;
    private long mDuration;
    private int seekmethod;
    private boolean paused;

    private static final int REFRESH = 1;
    private static final int QUIT = 2;
    private static final int GET_ALBUM_ART = 3;
    private static final int ALBUM_ART_DECODED = 4;
    private static final int GO_PRE = 5;
    private static final int GO_NEXT = 6;
    private static final int REFRESH_LYRIC = 1;

    private void queueNextRefresh(long delay) {
        if (!paused) {
            Message msg = mHandler.obtainMessage(REFRESH);
            mHandler.removeMessages(REFRESH);
            mHandler.sendMessageDelayed(msg, delay);
        }
    }

    private long refreshNow() {
        if (mService == null)
            return 500;
        try {
            long pos = mPosOverride < 0 ? mService.position() : mPosOverride;
            if ((pos >= 0) && (mDuration > 0)) {
                int progress = (int) (mProgress.getMax() * pos / mDuration);
                if (mService.isComplete() && !(pos < mDuration)) {
                    mCurrentTime.setText(MusicUtils.makeTimeString(this,
                            mDuration / 1000));
                    mProgress.setProgress(mProgress.getMax());
                } else {
                    mCurrentTime.setText(MusicUtils
                            .makeTimeString(this, pos / 1000));
                    mProgress.setProgress(progress);
                }

                if (mService.isPlaying()) {
                    mCurrentTime.setVisibility(View.VISIBLE);
                } else {
                    // blink the counter
                    int vis = mCurrentTime.getVisibility();
                    mCurrentTime
                            .setVisibility(vis == View.INVISIBLE ? View.VISIBLE
                                    : View.INVISIBLE);
                    return 500;
                }
            } else {
                mCurrentTime.setText("--:--");
                mProgress.setProgress(1000);
            }
            // calculate the number of milliseconds until the next full second, so
            // the counter can be updated at just the right time
            long remaining = 1000 - (pos % 1000);

            // approximate how often we would need to refresh the slider to
            // move it smoothly
            int width = mProgress.getWidth();
            if (width == 0)
                width = 320;
            long smoothrefreshtime = mDuration / width;

            if (smoothrefreshtime > remaining)
                return remaining;
            if (smoothrefreshtime < 20)
                return 20;
            return smoothrefreshtime;
        } catch (RemoteException ex) {
        }
        return 500;
    }

    private final Handler mHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
            case ALBUM_ART_DECODED:
                if (msg.obj != null && msg.obj instanceof Bitmap
                        && !(((Bitmap) msg.obj).isRecycled())) {
                    mAlbum.setImageBitmap((Bitmap) msg.obj);
                    mAlbum.getDrawable().setDither(true);
                    mAlbumIcon.setImageBitmap((Bitmap) msg.obj);
                    mAlbumIcon.getDrawable().setDither(true);
                } else {
                    mAlbum.setImageResource(R.drawable.album_cover_background);
                    mAlbum.getDrawable().setDither(true);
                    mAlbumIcon.setImageResource(R.drawable.album_cover_background);
                    mAlbumIcon.getDrawable().setDither(true);
                }
                break;

            case REFRESH:
                long next = refreshNow();
                queueNextRefresh(next);
                break;

            case QUIT:
                // This can be moved back to onCreate once the bug that prevents
                // Dialogs from being started from onCreate/onResume is fixed.
                new AlertDialog.Builder(MediaPlaybackActivity.this)
                        .setTitle(R.string.service_start_error_title)
                        .setMessage(R.string.service_start_error_msg)
                        .setPositiveButton(R.string.service_start_error_button,
                                new DialogInterface.OnClickListener() {
                                    public void onClick(DialogInterface dialog,
                                            int whichButton) {
                                        finish();
                                    }
                                }).setCancelable(false).show();
                break;
            case GO_PRE:
                try {
                    int shuffle = mService.getShuffleMode();
                    int histSize = mService.getHistSize();
                    if (isGoStart(shuffle, histSize)) {
                        mService.seek(0);
                        mService.play();
                    } else {
                        mService.prev();
                    }
                } catch (RemoteException ex) {
                    ex.printStackTrace();
                }
                break;
            case GO_NEXT:
                try {
                    mService.next();
                } catch (RemoteException ex) {
                    ex.printStackTrace();
                }
                break;

            default:
                break;
            }
        }
    };

    private boolean isGoStart(int shuffle, int histSize) {
        try {
            Boolean isOverTime = mService.position() < SWITCH_MUSIC_MAX_TIME ? true : false;
            Boolean isShuffleMode = shuffle == MediaPlaybackService.SHUFFLE_NORMAL ? true : false;
            Boolean isHistLess = histSize == 0 || histSize == 1 ? true : false;
            if ((isOverTime && isShuffleMode && isHistLess) || !isOverTime) {
                return true;
            } else {
                return false;
            }
        } catch (RemoteException e) {
            e.printStackTrace();
        }
        return false;
    }

    private BroadcastReceiver mStatusListener = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (action.equals(MediaPlaybackService.META_CHANGED)) {
                mMetaChanged = true;
                // redraw the artist/title info and
                // set new max for progress bar
                updateTrackInfo();
                setPauseButtonImage();
            } else if (action.equals(MediaPlaybackService.PLAYSTATE_CHANGED)) {
                setPauseButtonImage();
            } else if (action.equals(MediaPlaybackService.SHUFFLE_CHANGED)) {
                setShuffleButtonImage();
            } else if (action.equals(MediaPlaybackService.REPEAT_CHANGED)) {
                setRepeatButtonImage();
            }
        }
    };

    private BroadcastReceiver mScreenTimeoutListener = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (Intent.ACTION_SCREEN_ON.equals(intent.getAction())) {
                if (mReceiverUnregistered) {
                    IntentFilter f = new IntentFilter();
                    f.addAction(MediaPlaybackService.PLAYSTATE_CHANGED);
                    f.addAction(MediaPlaybackService.META_CHANGED);
                    f.addAction(MediaPlaybackService.SHUFFLE_CHANGED);
                    f.addAction(MediaPlaybackService.REPEAT_CHANGED);
                    registerReceiver(mStatusListener, f);
                    mReceiverUnregistered = false;
                }
                paused = false;

                if (mPosOverride > 0) {
                    mPosOverride = -1;
                }
                updateTrackInfo();
                long next = refreshNow();
                queueNextRefresh(next);
            } else if (Intent.ACTION_SCREEN_OFF.equals(intent.getAction())) {
                paused = false;

                if (!mReceiverUnregistered) {
                    mHandler.removeMessages(REFRESH);
                    unregisterReceiver(mStatusListener);
                    mReceiverUnregistered = true;
                }
            }
        }
    };

    private static class AlbumSongIdWrapper {
        public long albumid;
        public long songid;

        AlbumSongIdWrapper(long aid, long sid) {
            albumid = aid;
            songid = sid;
        }
    }

    private void updateTrackInfo() {
        if (mService == null) {
            return;
        }
        try {
            String path = mService.getPath();
            if (path == null) {
                mIsPanelExpanded = false;
                mSlidingPanelLayout.setHookState(BoardState.HIDDEN);
                NotificationManager nm = (NotificationManager)
                        getSystemService(Context.NOTIFICATION_SERVICE);
                nm.cancel(MediaPlaybackService.PLAYBACKSERVICE_STATUS);
                return;
            }

            long songid = mService.getAudioId();
            if (songid < 0 && path.toLowerCase().startsWith("http://")) {
                // Once we can get album art and meta data from MediaPlayer, we
                // can show that info again when streaming.
                ((View) mArtistName.getParent()).setVisibility(View.INVISIBLE);
                mAlbum.setVisibility(View.GONE);
                mTrackName.setText(path);
                mAlbumArtHandler.removeMessages(GET_ALBUM_ART);
                mAlbumArtHandler.obtainMessage(GET_ALBUM_ART,
                        new AlbumSongIdWrapper(-1, -1)).sendToTarget();
            } else {
                ((View) mArtistName.getParent()).setVisibility(View.VISIBLE);
                String artistName = mService.getArtistName();
                if (MediaStore.UNKNOWN_STRING.equals(artistName)) {
                    artistName = getString(R.string.unknown_artist_name);
                }
                mArtistName.setText(artistName);
                String albumName = mService.getAlbumName();
                long albumid = mService.getAlbumId();
                if (MediaStore.UNKNOWN_STRING.equals(albumName)) {
                    albumName = getString(R.string.unknown_album_name);
                    albumid = -1;
                }
                mTrackName.setText(mService.getTrackName());
                mAlbumArtHandler.removeMessages(GET_ALBUM_ART);
                mAlbumArtHandler.obtainMessage(GET_ALBUM_ART,
                        new AlbumSongIdWrapper(albumid, songid)).sendToTarget();
                mAlbum.setVisibility(View.VISIBLE);

            }

            // fetch clip duration from media database if available
            Cursor cursor = MusicUtils.query(this,
                    ContentUris.withAppendedId(MediaStore.Audio.Media.EXTERNAL_CONTENT_URI, songid),
                    new String [] { MediaStore.Audio.Media.DURATION }, null, null, null);
            if (cursor != null && cursor.moveToFirst()) {
                mDuration = cursor.getInt(0);
                // if duration in media database is not valid,
                // fall back to media service duration
                if (mDuration <= 0) {
                    mDuration = mService.duration();
                }
                cursor.close();
            } else {
                mDuration = mService.duration();
            }
            mTotalTime.setText(MusicUtils
                    .makeTimeString(this, mDuration / 1000));
        } catch (RemoteException ex) {
            finish();
        } catch (NullPointerException ex) {
            // we might not actually have the service yet
            ex.printStackTrace();
        } catch (IllegalStateException istateex) {
            Log.e(TAG,
                    "IllegalStateException in query uri. " + istateex.getMessage());
            if (mService == null) {
                Log.e(TAG, " service is null");
                return;
            }
        }
    }

    public class AlbumArtHandler extends Handler {
        private long mAlbumId = -1;

        public AlbumArtHandler(Looper looper) {
            super(looper);
        }

        @Override
        public void handleMessage(Message msg) {
            long albumid = ((AlbumSongIdWrapper) msg.obj).albumid;
            long songid = ((AlbumSongIdWrapper) msg.obj).songid;
            if (msg.what == GET_ALBUM_ART
                    && (mAlbumId != albumid || albumid < 0 || mOrientationChanged)) {
                mOrientationChanged = false;
                // while decoding the new image, show the default album art
                Message numsg = mHandler.obtainMessage(ALBUM_ART_DECODED,
                        MusicUtils.getDefaultArtwork(MediaPlaybackActivity.this));
                mHandler.removeMessages(ALBUM_ART_DECODED);
                mHandler.sendMessageDelayed(numsg, 600);
                // Don't allow default artwork here, because we want to fall back to
                // song-specific album art if we can't find anything for the album.
                Bitmap bm = MusicUtils.getArtwork(MediaPlaybackActivity.this,
                        songid, albumid, false);
                if (bm == null) {
                    bm = MusicUtils.getArtwork(MediaPlaybackActivity.this,
                            songid, -1);
                    albumid = -1;
                }
                if (bm != null) {
                    numsg = mHandler.obtainMessage(ALBUM_ART_DECODED, bm);
                    mHandler.removeMessages(ALBUM_ART_DECODED);
                    mHandler.sendMessage(numsg);
                }
                mAlbumId = albumid;
            }
        }
    }

    private static class Worker implements Runnable {
        private final Object mLock = new Object();
        private Looper mLooper;

        /**
         * Creates a worker thread with the given name. The thread then runs a
         * {@link android.os.Looper}.
         *
         * @param name
         *            A name for the new thread
         */
        Worker(String name) {
            Thread t = new Thread(null, this, name);
            t.setPriority(Thread.MIN_PRIORITY);
            t.start();
            synchronized (mLock) {
                while (mLooper == null) {
                    try {
                        mLock.wait();
                    } catch (InterruptedException ex) {
                    }
                }
            }
        }

        public Looper getLooper() {
            return mLooper;
        }

        public void run() {
            synchronized (mLock) {
                Looper.prepare();
                mLooper = Looper.myLooper();
                mLock.notifyAll();
            }
            Looper.loop();
        }

        public void quit() {
            mLooper.quit();
        }
    }

	@Override
	public void onCompletion(MediaPlayer mp) {
        // Leave 100ms for mediaplayer to change state.
        SystemClock.sleep(100);
        // isCompleted = true;
        mProgress.setProgress(mProgress.getMax());
        //updatePlayPause();
        }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        stopQueueNextRefreshForLyric();
        mAudioPlayerBody.removeAllViews();
        LayoutInflater inflater = LayoutInflater.from(this);
        View view = inflater.inflate(R.layout.audio_player_body, null);
        ViewGroup.LayoutParams layoutParams = mAudioPlayerBody.getLayoutParams();
        mAudioPlayerBody.addView(view, layoutParams);
        initAudioPlayerBodyView();
        mOrientationChanged = true;
        updateTrackInfo();
        setPauseButtonImage();
        setRepeatButtonImage();
        setShuffleButtonImage();

        if (newConfig.orientation == Configuration.ORIENTATION_PORTRAIT) {
            updateNowPlaying(mActivity);
        }
        mLyricAdapter.setInvalidate();
        queueNextRefreshForLyric(REFRESH_MILLIONS);
    }

    public static final String SRC_PATH = "/Music/lrc/";
    private static final long REFRESH_MILLIONS = 500;
    private static final int LYRIC_SCROLL_INTERVAL = 150;
    private static final int ENTER_LYRIC_ANIMAL_INTERVAL = 200;

    private void showLyric() {
        if (mIsExeLyricAnimal) {
            return;
        }
        mIsExeLyricAnimal = true;
        if (!mHasLrc || (!mIsLyricDisplay)) {
            try {
                stopQueueNextRefreshForLyric();
                setLyric(mService.getData(),mService.getTrackName());
            } catch (RemoteException ex) {
                return;
            }
        }
        if (mHasLrc) {
            queueNextRefreshForLyric(REFRESH_MILLIONS);
            enterLyricAnimal();
        } else {
            mIsExeLyricAnimal = false;
        }
    }

    private void enterLyricAnimal() {
        try {
            if (mLyricPanel != null) {
                int centerX = 0;
                int centerY = 0;
                int startWidth = 0;
                int startHeight = 0;
                if (getResources().getConfiguration().orientation
                        == Configuration.ORIENTATION_LANDSCAPE) {
                    centerX = mLyricPanel.getRight();
                    centerY = mLyricPanel.getBottom();
                    startWidth = 0;
                    startHeight = 0;
                } else {
                    Rect rect = new Rect(mLyricIcon.getLeft(),
                            mLyricIcon.getTop(),
                            mLyricIcon.getRight(),
                            mLyricIcon.getBottom());
                    centerX = rect.centerX();
                    centerY = rect.centerY();
                    startWidth = rect.width();
                    startHeight = rect.height();
                }
                AnimatorSet animatorSet = new AnimatorSet();
                ObjectAnimator alphaAnimator = ObjectAnimator.ofFloat(mLyricPanel,
                        "alpha", 0.0F, 1.0F);
                Animator animator = ViewAnimationUtils.createCircularReveal(
                        mLyricPanel,
                        centerX,
                        centerY,
                        (float) Math.hypot(startWidth, startHeight),
                        (float) Math.hypot(mLyricPanel.getWidth(), mLyricPanel.getHeight()));
                animatorSet.setDuration(ENTER_LYRIC_ANIMAL_INTERVAL);
                animatorSet.setInterpolator(new AccelerateInterpolator());
                animatorSet.playTogether(animator,alphaAnimator);
                animatorSet.addListener(new Animator.AnimatorListener() {
                    @Override
                    public void onAnimationStart(Animator animation) {
                        mLyricPanel.setVisibility(View.VISIBLE);
                    }

                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mIsLyricDisplay = true;
                        setLyricIconStatusAndImage(mHasLrc, mIsLyricDisplay);
                        setFavoriteIconImage(getFavoriteStatus());
                        mIsExeLyricAnimal = false;
                    }

                    @Override
                    public void onAnimationCancel(Animator animation) {
                        mIsExeLyricAnimal = false;
                    }

                    @Override
                    public void onAnimationRepeat(Animator animation) {
                    }
                });
                animatorSet.start();
            }
        } catch (IllegalStateException ex) {
        }
    }

    private void hideLyric() {
        if (mHasLrc) {
            if (mIsExeLyricAnimal) {
                return;
            }
            mIsExeLyricAnimal = true;
            stopQueueNextRefreshForLyric();
            exitLyricAnimal();
        }
    }

    private void exitLyricAnimal() {
        try {
            if (mLyricPanel != null) {
                int centerX = 0;
                int centerY = 0;
                int endWidth = 0;
                int endHeight = 0;
                if (getResources().getConfiguration().orientation
                        == Configuration.ORIENTATION_LANDSCAPE) {
                    centerX = mLyricPanel.getRight();
                    centerY = mLyricPanel.getBottom();
                    endWidth = 0;
                    endHeight = 0;
                } else {
                    Rect rect = new Rect(mLyricIcon.getLeft(),
                            mLyricIcon.getTop(),
                            mLyricIcon.getRight(),
                            mLyricIcon.getBottom());
                    centerX = rect.centerX();
                    centerY = rect.centerY();
                    endWidth = rect.width();
                    endHeight = rect.height();
                }
                AnimatorSet animatorSet = new AnimatorSet();
                ObjectAnimator alphaAnimator = ObjectAnimator.ofFloat(mLyricPanel,
                        "alpha", 1.0F, 0.0F);
                Animator animator = ViewAnimationUtils.createCircularReveal(
                        mLyricPanel,
                        centerX,
                        centerY,
                        (float) Math.hypot(mLyricPanel.getWidth(), mLyricPanel.getHeight()),
                        (float) Math.hypot(endWidth, endHeight));
                animatorSet.setDuration(ENTER_LYRIC_ANIMAL_INTERVAL);
                animatorSet.setInterpolator(new AccelerateInterpolator());
                animatorSet.playTogether(animator, alphaAnimator);
                animatorSet.addListener(new Animator.AnimatorListener() {
                    @Override
                    public void onAnimationStart(Animator animation) {
                    }

                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mLyricPanel.setVisibility(View.GONE);
                        mIsLyricDisplay = false;
                        setLyricIconStatusAndImage(mHasLrc, mIsLyricDisplay);
                        setFavoriteIconImage(getFavoriteStatus());
                        mIsExeLyricAnimal = false;
                    }

                    @Override
                    public void onAnimationCancel(Animator animation) {
                        mIsExeLyricAnimal = false;
                    }

                    @Override
                    public void onAnimationRepeat(Animator animation) {
                    }
                });
                animatorSet.start();
            }
        } catch (IllegalStateException ex) {
        }
    }

    public void setLyric(String path,String trackName) {
        if (trackName != null) {
            File lrcfile = null;
            mStrTrackName = trackName;
            if (Environment.getExternalStorageState().equals(
                    Environment.MEDIA_MOUNTED)) {
                String sdCardDir = Environment.getExternalStorageDirectory()
                        + SRC_PATH;
                lrcfile = new File(sdCardDir + trackName + ".lrc");
                if (lrcfile.exists()) {
                    setLrcFile(trackName, lrcfile);
                } else if ((lrcfile = setLocalLrc()) != null) {
                    setLrcFile(trackName, lrcfile);
                } else {
                    setLyricNolrc();
                }
            }
        }
    }

    private void setLrcFile(String trackName, File lrcFile) {
        mCurPlayListItem = new PlayListItem(trackName, 0L, true);
        long totalTime = 0;
        try {
            if (mService != null) {
                totalTime = mService.duration();
            }
        } catch (RemoteException e) {
        }
        mLyric = new Lyric(lrcFile, mCurPlayListItem, totalTime);
        setLyricView();
    }

    public void setLyricView() {
        if (mLyricPanel != null) {
            if (mMetaChanged) {
                mLyricAdapter.setLyric(mLyric);
                mMetaChanged = false;
                mLyricView.setSelection(0);
            }
            mHasLrc = true;
        }
    }

    private File setLocalLrc() {
        if (mService == null) {
            return null;
        }
        String trackPath = null;
        try {
            trackPath = mService.getData();
        } catch (RemoteException e) {
        }
        if (trackPath == null) {
            return null;
        }
        int index = trackPath.lastIndexOf(".");
        if (index == -1)
            return null;
        String LrcPath = trackPath.substring(0, index);
        LrcPath = LrcPath + ".lrc";
        File lrcfile = new File(LrcPath);
        if (lrcfile.exists()) {
            return lrcfile;
        } else {
            return null;
        }
    }

    private void setLyricNolrc() {
        mLyric = null;
        if (mLyricPanel != null) {
            mLyricPanel.setVisibility(View.GONE);
            mHasLrc = false;
            if (mIsLyricDisplay) {
                hideLyric();
            }
        }
    }

    public Handler mLyricHandler = new Handler() {
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case REFRESH_LYRIC:
                    try {
                        if (mService != null && mLyric != null) {
                            updateDuration(mService.position());
                        }
                    } catch (RemoteException e) {
                        // TODO Auto-generated catch block
                    }
                    if (mHasLrc) {
                        queueNextRefreshForLyric(REFRESH_MILLIONS);
                    }
                    break;
            }
        }
    };

    public void updateDuration(long duration) {
        if (mHasLrc && mLyricPanel != null) {
            mLyricAdapter.setTime(duration);
            int intentSelection = mLyricAdapter.getSelection();
            boolean shouldInvalidate = mLyricAdapter.shouldInvalidate();
            if (shouldInvalidate && mLyricSrollState == SCROLL_STATE_IDLE) {
                if (intentSelection < 0) {
                    intentSelection = 0;
                    mLyricView.setSelection(0);
                }
                mLyricView.smoothScrollToPositionFromTop(intentSelection,
                            mLyricView.getLyricMidPos(), LYRIC_SCROLL_INTERVAL);
                postUpdate();
                mLyricAdapter.invalidateComplete();
            }
        }
    }

    public void postUpdate() {
        mLyricHandler.post(mUpdateResults);
    }

    private void queueNextRefreshForLyric(long delay) {
        if (!paused && mHasLrc
                && (!mLyricHandler.hasMessages(REFRESH_LYRIC))) {
            Message msg = mLyricHandler.obtainMessage(REFRESH_LYRIC);
            mLyricHandler.sendMessageDelayed(msg, delay);
        }
    }

    private void stopQueueNextRefreshForLyric() {
        if (mLyricHandler != null) {
            mLyricHandler.removeMessages(REFRESH_LYRIC);
        }
    }

    Runnable mUpdateResults = new Runnable() {
        public void run() {
            if (mLyricView != null) {
                mLyricView.invalidate();
            }
        }
    };

    public IMediaPlaybackService getService() {
        return mService;
    }

    @Override
    public void onScrollStateChanged(AbsListView view, int scrollState) {
        if (scrollState  == SCROLL_STATE_TOUCH_SCROLL) {
            mIsTouchTrigger = true;
        }
        if (mIsTouchTrigger) {
            if (mResumeScrollTask != null) {
                mResumeScrollTask.cancel();
            }
            if (scrollState == SCROLL_STATE_IDLE) {
                mIsTouchTrigger = false;
                mResumeScrollTask = new ResumeScrollTask();
                mResumeTimer.schedule(mResumeScrollTask, 2000);
            } else {
                mLyricSrollState = scrollState;
            }
        }
    }

    @Override
    public void onScroll(AbsListView view,
            int firstVisibleItem,
            int visibleItemCount,
            int totalItemCount) {
        mFirstVisibleItem = firstVisibleItem;
    }

    private class ResumeScrollTask extends TimerTask {
        @Override
        public void run() {
            mLyricSrollState = SCROLL_STATE_IDLE;
        }
    }

    private void setFavoriteIconImage(boolean status) {
        if (status) {
            mFavoriteIcon.setImageResource(R.drawable.favorite_selected);
        } else {
            if (getResources().getConfiguration().orientation
                    == Configuration.ORIENTATION_PORTRAIT
                    && mLyricIconStatus != STATUS_SELECTED) {
                mFavoriteIcon.setImageResource(R.drawable.favorite_unselected_port);
            } else {
                mFavoriteIcon.setImageResource(R.drawable.favorite_unselected);
            }
        }
    }

    private void setLyricIconStatusAndImage(boolean hasLyric, boolean display) {
        setLyricIconStatus(hasLyric, display);
        setLyricIconImage();
    }

    private void setLyricIconStatus(boolean hasLyric, boolean display) {
        if (!hasLyric) {
            mLyricIconStatus = STATUS_DISABLE;
        } else if (display) {
            mLyricIconStatus = STATUS_SELECTED;
        } else {
            if (getResources().getConfiguration().orientation
                    == Configuration.ORIENTATION_PORTRAIT) {
                mLyricIconStatus = STATUS_PORT_UNSELECTED;
            } else {
                mLyricIconStatus = STATUS_LAND_UNSELECTED;
            }
        }
    }

    private void setLyricIconImage() {
        switch (mLyricIconStatus) {
            case STATUS_SELECTED:
                mLyricIcon.setImageResource(R.drawable.ic_lyric_selected);
                break;
            case STATUS_PORT_UNSELECTED:
                mLyricIcon.setImageResource(R.drawable.ic_lyric_unselected_port);
                break;
            case STATUS_LAND_UNSELECTED:
                mLyricIcon.setImageResource(R.drawable.ic_lyric_unselected);
                break;
            case STATUS_DISABLE:
                mLyricIcon.setImageResource(R.drawable.ic_lyric_disable);
                break;
            default:
                mLyricIcon.setImageResource(R.drawable.ic_lyric_disable);
                break;
        }
    }

    private boolean hasLyric(String trackName) {
        if (trackName != null) {
            File lrcFile;
            if (Environment.getExternalStorageState().equals(
                    Environment.MEDIA_MOUNTED)) {
                String sdCardDir = Environment.getExternalStorageDirectory()
                        + SRC_PATH;
                lrcFile = new File(sdCardDir + trackName + ".lrc");
                if (lrcFile.exists()) {
                    return true;
                }
                if (setLocalLrc() != null) {
                    return true;
                }
            }
        }
        return false;
    }
}
