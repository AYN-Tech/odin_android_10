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

import android.Manifest.permission;
import android.app.ActivityManager;
import android.app.ActivityManager.RunningAppProcessInfo;
import android.app.AlertDialog;
import android.app.Notification;
import android.app.Notification.Builder;
import android.app.NotificationManager;
import android.app.NotificationChannel;
import android.app.PendingIntent;
import android.app.Service;
import android.appwidget.AppWidgetManager;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.BroadcastReceiver;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.content.res.Resources;

import android.database.Cursor;
import android.database.sqlite.SQLiteException;
//import android.drm.DrmManagerClientWrapper;
import android.drm.DrmStore.Action;
import android.drm.DrmStore.RightsStatus;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.media.audiofx.AudioEffect;
import android.media.AudioManager;
import android.media.AudioManager.OnAudioFocusChangeListener;
import android.media.MediaMetadataRetriever;
import android.media.MediaPlayer;
import android.media.MediaPlayer.OnCompletionListener;
import android.media.RemoteControlClient;
import android.media.RemoteControlClient.OnPlaybackPositionUpdateListener;
import android.media.RemoteControlClient.MetadataEditor;
import android.net.Uri;
import android.os.FileObserver;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.Message;
import android.os.PowerManager;
import android.os.SystemClock;
import android.os.PowerManager.WakeLock;
import android.provider.MediaStore;
import android.support.v4.media.session.MediaSessionCompat;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.RemoteViews;
import android.widget.Toast;
import android.support.v4.app.NotificationCompat;
import java.io.FileDescriptor;
import java.io.IOException;
import java.io.PrintWriter;
import java.lang.ref.SoftReference;
import java.util.List;
import java.util.Random;
import java.util.Vector;
import java.util.HashMap;

/**
 * Provides "background" audio playback capabilities, allowing the
 * user to switch between activities without stopping playback.
 */

public class MediaPlaybackService extends Service {
    /** used to specify whether enqueue() should start playing
     * the new list of files right away, next or once all the currently
     * queued files have been played
     */
    public static final int NOW = 1;
    public static final int NEXT = 2;
    public static final int LAST = 3;
    public static final int PLAYBACKSERVICE_STATUS = 1;

    public static final int SHUFFLE_NONE = 0;
    public static final int SHUFFLE_NORMAL = 1;
    public static final int SHUFFLE_AUTO = 2;

    public static final int REPEAT_NONE = 0;
    public static final int REPEAT_CURRENT = 1;
    public static final int REPEAT_ALL = 2;
    private HashMap<Byte, Boolean> mAttributePairs = new HashMap<Byte, Boolean>();

    public static final String PLAYSTATE_CHANGED = "com.android.music.playstatechanged";
    public static final String META_CHANGED = "com.android.music.metadatachanged";
    public static final String SHUFFLE_CHANGED = "com.android.music.shuffle";
    public static final String REPEAT_CHANGED = "com.android.music.repeat";
    public static final String QUEUE_CHANGED = "com.android.music.queuechanged";
    private static final String ACTION_DELETE_MUSIC = "com.android.fileexplorer.action.DELETE_MUSIC";

    public static final String SERVICECMD = "com.android.music.musicservicecommand";
    public static final String CMDNAME = "command";
    public static final String CMDTOGGLEPAUSE = "togglepause";
    public static final String CMDSTOP = "stop";
    public static final String CMDPAUSE = "pause";
    public static final String CMDPLAY = "play";
    public static final String CMDPREVIOUS = "previous";
    public static final String CMDNEXT = "next";
    public static final String CMDGET = "get";
    public static final String CMDSET = "set";

    public static final String TOGGLEPAUSE_ACTION = "org.codeaurora.android.music.musicservicecommand.togglepause";
    public static final String PAUSE_ACTION = "org.codeaurora.android.music.musicservicecommand.pause";
    public static final String PREVIOUS_ACTION = "org.codeaurora.android.music.musicservicecommand.previous";
    public static final String NEXT_ACTION = "org.codeaurora.android.music.musicservicecommand.next";
    public static final String EXIT_ACTION = "org.codeaurora.android.music.musicservicecommand.exit";
    private static final String PLAYSTATUS_REQUEST = "org.codeaurora.android.music.playstatusrequest";
    private static final String PLAYSTATUS_RESPONSE = "org.codeaurora.music.playstatusresponse";
    private static final String PLAYERSETTINGS_REQUEST = "org.codeaurora.music.playersettingsrequest";
    private static final String PLAYERSETTINGS_RESPONSE = "org.codeaurora.music.playersettingsresponse";
    private static final String SET_ADDRESSED_PLAYER = "org.codeaurora.music.setaddressedplayer";
    private static final String EXTRA_SHUFFLE_VAL = "shuffle";
    private static final String EXTRA_REPEAT_VAL = "repeat";
    private static final String UPDATE_WIDGET_ACTION = "com.android.music.updatewidget";

    private static final int TRACK_ENDED = 1;
    private static final int RELEASE_WAKELOCK = 2;
    private static final int SERVER_DIED = 3;
    private static final int FOCUSCHANGE = 4;
    private static final int FADEDOWN = 5;
    private static final int FADEUP = 6;
    private static final int TRACK_WENT_TO_NEXT = 7;
    private static final int GOTO_NEXT = 8;
    private static final int GOTO_PREV = 9;
    private static final int ERROR = 10 ;
    private static final int MAX_HISTORY_SIZE = 100;
    private static final int DEFAULT_REPEAT_VAL = 0;
    private static final int DEFAULT_SHUFFLE_VAL = 0;
    private static final int SET_BROWSED_PLAYER = 1001;
    private static final int SET_PLAY_ITEM = 1002;
    private static final int GET_NOW_PLAYING_ENTRIES = 1003;

    private static final int SCOPE_FILE_SYSTEM = 0x01;
    private static final int SCOPE_NOW_PLAYING = 0x03;
    private static final int INVALID_SONG_UID = 0xffffffff;
    private static final float PLAYBACK_SPEED_1X = 1.0f;

    private RemoteViews views;
    private RemoteViews viewsLarge;
    private Notification status = new Notification();

    private boolean mControlInStatusBar = false;

    private MultiPlayer mPlayer;
    private String mFileToPlay;
    private int mShuffleMode = SHUFFLE_NONE;
    private int mRepeatMode = REPEAT_NONE;
    private int mMediaMountedCount = 0;
    private long [] mAutoShuffleList = null;
    private long [] mPlayList = null;
    private int mPlayListLen = 0;
    private Vector<Integer> mHistory = new Vector<Integer>(MAX_HISTORY_SIZE);
    private TrackInfo mCurrentTrackInfo;
    private int mPlayPos = -1;
    private int mNextPlayPos = -1;
    private static final String LOGTAG = "MediaPlaybackService";
    private final Shuffler mRand = new Shuffler();
    private int mOpenFailedCounter = 0;
    String[] mCursorCols = new String[] {
            "_id",             // index must match IDCOLIDX below
            MediaStore.Audio.Media.ARTIST,
            MediaStore.Audio.Media.ALBUM,
            MediaStore.Audio.Media.TITLE,
            MediaStore.Audio.Media.DATA,
            MediaStore.Audio.Media.MIME_TYPE,
            MediaStore.Audio.Media.ALBUM_ID,
            MediaStore.Audio.Media.ARTIST_ID,
            MediaStore.Audio.Media.IS_PODCAST, // index must match PODCASTCOLIDX below
            MediaStore.Audio.Media.BOOKMARK    // index must match BOOKMARKCOLIDX below
    };
    private final static int IDCOLIDX = 0;
    private final static int PODCASTCOLIDX = 8;
    private final static int BOOKMARKCOLIDX = 9;
    private BroadcastReceiver mUnmountReceiver = null;
    private BroadcastReceiver mA2dpReceiver = null;
    private WakeLock mWakeLock;
    private int mServiceStartId = -1;
    private boolean mServiceInUse = false;
    private boolean mIsSupposedToBePlaying = false;
    private boolean mQuietMode = false;
    private AudioManager mAudioManager;
    private boolean mQueueIsSaveable = true;
    // used to track what type of audio focus loss caused the playback to pause
    private boolean mPausedByTransientLossOfFocus = false;
    public static final byte ATTRIBUTE_ALL = -1;
    public static final byte ERROR_NOTSUPPORTED = -1;
    public static final byte ATTRIBUTE_EQUALIZER = 1;
    public static final byte ATTRIBUTE_REPEATMODE = 2;
    public static final byte ATTRIBUTE_SHUFFLEMODE = 3;
    public static final byte ATTRIBUTE_SCANMODE = 4;

    private byte [] supportedAttributes = new byte[] {
                          ATTRIBUTE_REPEATMODE,
                          ATTRIBUTE_SHUFFLEMODE
                          };

    public static final byte VALUE_REPEATMODE_OFF = 1;
    public static final byte VALUE_REPEATMODE_SINGLE = 2;
    public static final byte VALUE_REPEATMODE_ALL = 3;
    public static final byte VALUE_REPEATMODE_GROUP = 4;

    private byte [] supportedRepeatValues = new byte [] {
                            VALUE_REPEATMODE_OFF,
                            VALUE_REPEATMODE_SINGLE,
                            VALUE_REPEATMODE_ALL
                            };

    public static final byte VALUE_SHUFFLEMODE_OFF = 1;
    public static final byte VALUE_SHUFFLEMODE_ALL = 2;
    public static final byte VALUE_SHUFFLEMODE_GROUP = 3;

    private byte [] supportedShuffleValues = new byte [] {
                            VALUE_SHUFFLEMODE_OFF,
                            VALUE_SHUFFLEMODE_ALL
                            };

    String [] AttrStr = new String[] {
                                "",
                                "Equalizer",
                                "Repeat Mode",
                                "Shuffle Mode",
                                "Scan Mode"
                                };

    private byte [] unsupportedList = new byte [] {
                                    0
                                    };
    private static final String EXTRA_GET_COMMAND = "commandExtra";
    private static final String EXTRA_GET_RESPONSE = "Response";
    private static final int GET_ATTRIBUTE_IDS = 0;
    private static final int GET_VALUE_IDS = 1;
    private static final int GET_ATTRIBUTE_TEXT = 2;
    private static final int GET_VALUE_TEXT        = 3;
    private static final int GET_ATTRIBUTE_VALUES = 4;
    private static final int NOTIFY_ATTRIBUTE_VALUES = 5;
    private static final int SET_ATTRIBUTE_VALUES  = 6;
    private static final int GET_INVALID = 0xff;
    private static final byte GET_ATTR_INVALID = 0x7f;

    private static final String EXTRA_ATTRIBUTE_ID = "Attribute";
    private static final String EXTRA_VALUE_STRING_ARRAY = "ValueStrings";
    private static final String EXTRA_ATTRIB_VALUE_PAIRS = "AttribValuePairs";
    private static final String EXTRA_ATTRIBUTE_STRING_ARRAY = "AttributeStrings";
    private static final String EXTRA_VALUE_ID_ARRAY = "Values";
    private static final String EXTRA_ATTIBUTE_ID_ARRAY = "Attributes";
    private boolean mIsReadGranted = false;


    private static final String CHANNEL_ONE_ID = "Channel-music";
    private static final String CHANNEL_ONE_NAME = "com.android.music";

    private SharedPreferences mPreferences;
    // We use this to distinguish between different cards when saving/restoring playlists.
    // This will have to change if we want to support multiple simultaneous cards.
    private int mCardId;

    private MediaAppWidgetProvider mAppWidgetProvider = MediaAppWidgetProvider.getInstance();
    private MediaAppWidgetProviderLarge mAppWidgetProviderLarge =
            MediaAppWidgetProviderLarge.getInstance();

    // interval after which we stop the service when idle
    private static final int IDLE_DELAY = 60000;

    private RemoteControlClient mRemoteControlClient;

    //hoffc fix media button delay receied issue
    private MediaSessionCompat mSessionCompat;
    private static final int MSG_LONG_PRESS_TIMEOUT = 1;
    private static final int LONG_PRESS_DELAY = 1000;
    private boolean mLaunched = false;


    private class TrackInfo {
        public long mId;
        public String mArtistName;
        public long mArtistId;
        public String mAlbumName;
        public long mAlbumId;
        public String mTrackName;
        public String mData;
        public int mPodcast;
        public long mBookmark;
    }

    private Handler mMediaplayerHandler = new Handler() {
        float mCurrentVolume = 1.0f;
        @Override
        public void handleMessage(Message msg) {
            MusicUtils.debugLog("mMediaplayerHandler.handleMessage " + msg.what);
            switch (msg.what) {
                case FADEDOWN:
                    mCurrentVolume -= .05f;
                    if (mCurrentVolume > .2f) {
                        mMediaplayerHandler.sendEmptyMessageDelayed(FADEDOWN, 10);
                    } else {
                        mCurrentVolume = .2f;
                    }
                    mPlayer.setVolume(mCurrentVolume);
                    break;
                case FADEUP:
                    mCurrentVolume += .01f;
                    if (mCurrentVolume < 1.0f) {
                        mMediaplayerHandler.sendEmptyMessageDelayed(FADEUP, 10);
                    } else {
                        mCurrentVolume = 1.0f;
                    }
                    mPlayer.setVolume(mCurrentVolume);
                    break;
                case SERVER_DIED:
                    if (mPlayer == null) {
                        break;
                    }
                    if (isPlaying()) {
                        pause(false);
                        if (isAppOnForeground(MediaPlaybackService.this)) {
                            AlertDialog alertDialog =
                                    new AlertDialog.Builder(MediaPlaybackService.this)
                                    .setTitle(R.string.service_start_error_title)
                                    .setMessage(R.string.service_start_error_msg)
                                    .setPositiveButton(R.string.button_ok, null)
                                    .create();

                            alertDialog.getWindow()
                                    .setType(WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY);
                            alertDialog.show();
                        }
                    }
                    openCurrentAndNext();
                    break;
                case TRACK_WENT_TO_NEXT:
                    mPlayPos = mNextPlayPos;
                    if (mPlayPos < 0) {
                        gotoIdleState();
                        if (mIsSupposedToBePlaying) {
                            mIsSupposedToBePlaying = false;
                            notifyChange(PLAYSTATE_CHANGED);
                        }
                        break;
                    }
                    if (mPlayPos >= 0 && mPlayPos < mPlayList.length) {
                        mCurrentTrackInfo = getTrackInfoFromId(mPlayList[mPlayPos]);
                    }
                    notifyChange(META_CHANGED);
                    notifyChange(PLAYSTATE_CHANGED);
                    updateNotification();
                    setNextTrack();
                    break;
                case TRACK_ENDED:
                    if (mRepeatMode == REPEAT_CURRENT) {
                        if (isPlaying()) {
                            seek(0);
                            play();
                         }
                    } else {
                        gotoNext(false);
                    }
                    break;
                case RELEASE_WAKELOCK:
                    mWakeLock.release();
                    break;

                case FOCUSCHANGE:
                    // This code is here so we can better synchronize it with the code that
                    // handles fade-in
                    switch (msg.arg1) {
                        case AudioManager.AUDIOFOCUS_LOSS:
                            Log.v(LOGTAG, "AudioFocus: received AUDIOFOCUS_LOSS");
                            if(isPlaying()) {
                                mPausedByTransientLossOfFocus = false;
                            }
                            // Don't idle to avoid being killed by LMK
                            pause(false);
                            break;
                        case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK:
                            mMediaplayerHandler.removeMessages(FADEUP);
                            mMediaplayerHandler.sendEmptyMessage(FADEDOWN);
                            break;
                        case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT:
                            Log.v(LOGTAG, "AudioFocus: received AUDIOFOCUS_LOSS_TRANSIENT");
                            if(isPlaying()) {
                                mPausedByTransientLossOfFocus = true;
                            }
                            // Don't idle to avoid being killed by LMK
                            pause(false);
                            break;
                        case AudioManager.AUDIOFOCUS_GAIN:
                            Log.v(LOGTAG, "AudioFocus: received AUDIOFOCUS_GAIN");
                            if(!isPlaying() && mPausedByTransientLossOfFocus) {
                                mPausedByTransientLossOfFocus = false;
                                mCurrentVolume = 0f;
                                mPlayer.setVolume(mCurrentVolume);
                                play(); // also queues a fade-in
                            } else {
                                mMediaplayerHandler.removeMessages(FADEDOWN);
                                mMediaplayerHandler.sendEmptyMessage(FADEUP);
                            }
                            break;
                        default:
                            Log.e(LOGTAG, "Unknown audio focus change code");
                    }
                    break;
                case GOTO_NEXT:
                    gotoNext(true);
                    break;
                case GOTO_PREV:
                    prev();
                    break;
                case ERROR:
                    Toast.makeText(MediaPlaybackService.this, R.string.open_failed,
                            Toast.LENGTH_SHORT).show();

                    if (mRepeatMode == REPEAT_CURRENT) {
                        //Called from onError when current clip is played in
                        //repeat only mode.
                        Log.e(LOGTAG," ERROR: Pause the clip");
                        pause();
                    } else {
                        if (mIsSupposedToBePlaying) {
                            gotoNext(true);
                        } else {
                            openCurrentAndNext();
                        }
                    }
                    break;
                default:
                    break;
            }
        }
    };

    private BroadcastReceiver mLocaleReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (views != null && viewsLarge != null && status != null) {
               updateNotification();
            }
        }
    };

    private BroadcastReceiver mIntentReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            String cmd = intent.getStringExtra("command");
            MusicUtils.debugLog("mIntentReceiver.onReceive " + action + " / " + cmd);

            if (MusicUtils.isForbidPlaybackInCall(context)){
                return;
            }

            if (CMDNEXT.equals(cmd) || NEXT_ACTION.equals(action)) {
                sendEmptyMessageIfNo(GOTO_NEXT);
            } else if (CMDPREVIOUS.equals(cmd) || PREVIOUS_ACTION.equals(action)) {
                sendEmptyMessageIfNo(GOTO_PREV);
            } else if (CMDTOGGLEPAUSE.equals(cmd) || TOGGLEPAUSE_ACTION.equals(action)) {
                if (isPlaying()) {
                    pause(false);
                    mPausedByTransientLossOfFocus = false;
                } else {
                    play();
                }
            } else if (CMDPAUSE.equals(cmd) || PAUSE_ACTION.equals(action)) {
                pause();
                mPausedByTransientLossOfFocus = false;
            } else if (EXIT_ACTION.equals(action)) {
                stop();
                SysApplication.getInstance().exit();

            } else if (CMDPLAY.equals(cmd)) {
                play();
            } else if (CMDSTOP.equals(cmd)) {
                pause();
                mPausedByTransientLossOfFocus = false;
                seek(0);
            } else if (MediaAppWidgetProvider.CMDAPPWIDGETUPDATE.equals(cmd)) {
                // Someone asked us to refresh a set of specific widgets, probably
                // because they were just added.
                int[] appWidgetIds = intent.getIntArrayExtra(AppWidgetManager.EXTRA_APPWIDGET_IDS);
                mAppWidgetProvider.performUpdate(MediaPlaybackService.this, appWidgetIds);
            } else if (MediaAppWidgetProviderLarge.CMDAPPWIDGETUPDATE_LARGE.equals(cmd)) {
                // Someone asked us to refresh a set of specific widgets, probably
                // because they were just added.
                int[] appWidgetIds = intent.getIntArrayExtra(AppWidgetManager.EXTRA_APPWIDGET_IDS);
                mAppWidgetProviderLarge.performUpdate(MediaPlaybackService.this, appWidgetIds);
            }
        }
    };

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        IntentFilter commandFilter = new IntentFilter();
        commandFilter.addAction(SERVICECMD);
        commandFilter.addAction(TOGGLEPAUSE_ACTION);
        commandFilter.addAction(PAUSE_ACTION);
        commandFilter.addAction(NEXT_ACTION);
        commandFilter.addAction(PREVIOUS_ACTION);
        commandFilter.addAction(EXIT_ACTION);
        registerReceiver(mIntentReceiver, commandFilter);
    }

    private void sendEmptyMessageIfNo(int msgId) {
        if (!mMediaplayerHandler.hasMessages(msgId)) {
            mMediaplayerHandler.sendEmptyMessage(msgId);
        }
    }

    private boolean isAppOnForeground(Context context) {
        ActivityManager activityManager = (ActivityManager) context
                .getSystemService(Context.ACTIVITY_SERVICE);
        List<RunningAppProcessInfo> appProcesses = activityManager
                .getRunningAppProcesses();
        if (appProcesses == null) {
            return false;
        }
        final String packageName = context.getPackageName();
        for (RunningAppProcessInfo appProcess : appProcesses) {
            if (appProcess.importance == RunningAppProcessInfo.IMPORTANCE_FOREGROUND
                    && appProcess.processName.equals(packageName)) {
                return true;
            }
        }
        return false;
    }

    private OnAudioFocusChangeListener mAudioFocusListener = new OnAudioFocusChangeListener() {
        public void onAudioFocusChange(int focusChange) {
            mMediaplayerHandler.obtainMessage(FOCUSCHANGE, focusChange, 0)
                    .sendToTarget();
        }
    };

    private OnPlaybackPositionUpdateListener mPosListener = new OnPlaybackPositionUpdateListener() {
        public void onPlaybackPositionUpdate(long newPositionMs) {
            if (newPositionMs > duration()) {
                boolean wasPlaying = isPlaying();
                gotoNext(true);
                if (!wasPlaying) {
                    pause();
                }
            } else {
                seek(newPositionMs);
            }
        }
    };

    public MediaPlaybackService() {
    }

    @Override
    public void onCreate() {
        super.onCreate();
        if (checkSelfPermission(permission.READ_EXTERNAL_STORAGE) !=
                PackageManager.PERMISSION_GRANTED) {
            stopSelf();
            return;
        } else {
            mIsReadGranted = true;
        }
        // add for clear the notification when the service restart
        stopForeground(true);

        mAudioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        ComponentName componentName = new ComponentName(getPackageName(),
                MediaButtonIntentReceiver.class.getName());

        Intent i = new Intent(Intent.ACTION_MEDIA_BUTTON);
        i.setComponent(componentName);
        PendingIntent pi = PendingIntent.getBroadcast(this /*context*/,
                0 /*requestCode, ignored*/, i /*intent*/, 0 /*flags*/);
        mRemoteControlClient = new RemoteControlClient(pi);

        int flags = RemoteControlClient.FLAG_KEY_MEDIA_PREVIOUS
                | RemoteControlClient.FLAG_KEY_MEDIA_NEXT
                | RemoteControlClient.FLAG_KEY_MEDIA_PLAY
                | RemoteControlClient.FLAG_KEY_MEDIA_POSITION_UPDATE
                | RemoteControlClient.FLAG_KEY_MEDIA_PAUSE
                | RemoteControlClient.FLAG_KEY_MEDIA_PLAY_PAUSE
                | RemoteControlClient.FLAG_KEY_MEDIA_STOP;
        mRemoteControlClient.setTransportControlFlags(flags);
        mRemoteControlClient.setPlaybackPositionUpdateListener(mPosListener);

        //fix media button delay receied issue
        mSessionCompat = new MediaSessionCompat(this, "MediaPlaybackService",
                componentName, null);
        mSessionCompat.setFlags(MediaSessionCompat.FLAG_HANDLES_MEDIA_BUTTONS
                | MediaSessionCompat.FLAG_HANDLES_TRANSPORT_CONTROLS);
        mSessionCompat.setCallback(new MediaSessionCallback());
        mSessionCompat.setActive(true);

        mPreferences = getSharedPreferences("Music", MODE_PRIVATE);
        mRepeatMode = mPreferences.getInt("repeatmode", REPEAT_NONE);

        mCardId = MusicUtils.getCardId(this);

        registerExternalStorageListener();
        registerA2dpServiceListener();

        // Needs to be done in this thread, since otherwise ApplicationContext.getPowerManager() crashes.
        mPlayer = new MultiPlayer();
        mPlayer.setHandler(mMediaplayerHandler);

        reloadQueue();
        notifyChange(QUEUE_CHANGED);
        notifyChange(META_CHANGED);

        mAttributePairs.put(ATTRIBUTE_EQUALIZER, false);
        mAttributePairs.put(ATTRIBUTE_REPEATMODE, true);
        mAttributePairs.put(ATTRIBUTE_SHUFFLEMODE, true);

        IntentFilter commandFilter = new IntentFilter();
        commandFilter.addAction(SERVICECMD);
        commandFilter.addAction(TOGGLEPAUSE_ACTION);
        commandFilter.addAction(PAUSE_ACTION);
        commandFilter.addAction(NEXT_ACTION);
        commandFilter.addAction(PREVIOUS_ACTION);
        commandFilter.addAction(EXIT_ACTION);
        registerReceiver(mIntentReceiver, commandFilter);


        IntentFilter s = new IntentFilter();
        s.addAction(Intent.ACTION_SCREEN_ON);
        s.addAction(Intent.ACTION_SCREEN_OFF);
        registerReceiver(mScreenTimeoutListener, s);
        //Updating notification after language has been changed.
        IntentFilter localeFilter = new IntentFilter();
        localeFilter.addAction(Intent.ACTION_LOCALE_CHANGED);
        registerReceiver(mLocaleReceiver,localeFilter);

        PowerManager pm = (PowerManager)getSystemService(Context.POWER_SERVICE);
        mWakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, this.getClass().getName());
        mWakeLock.setReferenceCounted(false);

        // If the service was idle, but got killed before it stopped itself, the
        // system will relaunch it. Make sure it gets stopped again in that case.
        Message msg = mDelayedStopHandler.obtainMessage();
        mDelayedStopHandler.sendMessageDelayed(msg, IDLE_DELAY);

        mControlInStatusBar = getApplicationContext().getResources().getBoolean(R.bool.control_in_statusbar);

        updateNotification();
        startForeground(PLAYBACKSERVICE_STATUS, status);

    }

    private BroadcastReceiver mScreenTimeoutListener = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (Intent.ACTION_SCREEN_ON.equals(intent.getAction())) {
                MusicUtils.mIsScreenOff = false;
            }

            else if (Intent.ACTION_SCREEN_OFF.equals(intent.getAction())) {
                MusicUtils.mIsScreenOff = true;
            }
        }
    };

    @Override
    public void onDestroy() {
        if (!mIsReadGranted)
            return;
        // Check that we're not being destroyed while something is still playing.
        if (isPlaying()) {
            Log.e(LOGTAG, "Service being destroyed while still playing.");
        }
        // release all MediaPlayer resources, including the native player and wakelocks
        Intent i = new Intent(AudioEffect.ACTION_CLOSE_AUDIO_EFFECT_CONTROL_SESSION);
        i.putExtra(AudioEffect.EXTRA_AUDIO_SESSION, getAudioSessionId());
        i.putExtra(AudioEffect.EXTRA_PACKAGE_NAME, getPackageName());
        sendBroadcast(i);
        mPlayer.release();
        mPlayer = null;

        mAudioManager.abandonAudioFocus(mAudioFocusListener);

        if (mSessionCompat != null) {
            mSessionCompat.release();
            mSessionCompat = null;
            mMediaButtonHandler.removeCallbacksAndMessages(null);
        }

        // make sure there aren't any other messages coming
        mDelayedStopHandler.removeCallbacksAndMessages(null);
        mMediaplayerHandler.removeCallbacksAndMessages(null);

        unregisterReceiver(mIntentReceiver);
        if (mUnmountReceiver != null) {
            unregisterReceiver(mUnmountReceiver);
            mUnmountReceiver = null;
        }

        if (mLocaleReceiver != null) {
            unregisterReceiver(mLocaleReceiver);
            mLocaleReceiver = null;
        }

        if (mA2dpReceiver != null) {
            unregisterReceiver(mA2dpReceiver);
            mA2dpReceiver = null;
        }

        if (mAttributePairs != null) {
            mAttributePairs.clear();
        }

        if (mScreenTimeoutListener != null)
            unregisterReceiver(mScreenTimeoutListener);

        //notify music widget update state
        //because of service is died, so can not use performUpdate
        Intent intent = new Intent(UPDATE_WIDGET_ACTION);
        sendBroadcast(intent);
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O){
            NotificationManager nm = (NotificationManager) getSystemService(
                    Context.NOTIFICATION_SERVICE);
            nm.deleteNotificationChannel(CHANNEL_ONE_ID);
        }

        mWakeLock.release();
        super.onDestroy();
        mServiceInUse = false;
    }

    private final char hexdigits [] = new char [] {
            '0', '1', '2', '3',
            '4', '5', '6', '7',
            '8', '9', 'a', 'b',
            'c', 'd', 'e', 'f'
    };

    private void saveQueue(boolean full) {
        if (!mQueueIsSaveable || mPreferences == null) {
            return;
        }

        Editor ed = mPreferences.edit();
        //long start = System.currentTimeMillis();
        if (full) {
            StringBuilder q = new StringBuilder();

            // The current playlist is saved as a list of "reverse hexadecimal"
            // numbers, which we can generate faster than normal decimal or
            // hexadecimal numbers, which in turn allows us to save the playlist
            // more often without worrying too much about performance.
            // (saving the full state takes about 40 ms under no-load conditions
            // on the phone)
            int len = mPlayListLen;
            for (int i = 0; i < len; i++) {
                long n = mPlayList[i];
                if (n < 0) {
                    continue;
                } else if (n == 0) {
                    q.append("0;");
                } else {
                    while (n != 0) {
                        int digit = (int)(n & 0xf);
                        n >>>= 4;
                        q.append(hexdigits[digit]);
                    }
                    q.append(";");
                }
            }
            //Log.i("@@@@ service", "created queue string in " + (System.currentTimeMillis() - start) + " ms");
            ed.putString("queue", q.toString());
            ed.putInt("cardid", mCardId);
            if (mShuffleMode != SHUFFLE_NONE) {
                // In shuffle mode we need to save the history too
                len = mHistory.size();
                q.setLength(0);
                for (int i = 0; i < len; i++) {
                    int n = mHistory.get(i);
                    if (n == 0) {
                        q.append("0;");
                    } else {
                        while (n != 0) {
                            int digit = (n & 0xf);
                            n >>>= 4;
                            q.append(hexdigits[digit]);
                        }
                        q.append(";");
                    }
                }
                ed.putString("history", q.toString());
            }
        }
        ed.putInt("curpos", mPlayPos);
        if (mPlayer.isInitialized()) {
            if (mPlayer.isComplete() && mControlInStatusBar) {
                ed.putLong("seekpos", mPlayer.duration());
            } else {
                ed.putLong("seekpos", mPlayer.position());
            }
        }
        ed.putInt("repeatmode", mRepeatMode);
        ed.putInt("shufflemode", mShuffleMode);
        ed.apply();

        //Log.i("@@@@ service", "saved state in " + (System.currentTimeMillis() - start) + " ms");
    }

    private void reloadQueue() {
        String q = null;

        boolean newstyle = false;
        int id = mCardId;
        if (mPreferences.contains("cardid")) {
            newstyle = true;
            id = mPreferences.getInt("cardid", ~mCardId);
        }
        if (id == mCardId) {
            // Only restore the saved playlist if the card is still
            // the same one as when the playlist was saved
            q = mPreferences.getString("queue", "");
        }
        int qlen = q != null ? q.length() : 0;
        if (qlen > 1) {
            //Log.i("@@@@ service", "loaded queue: " + q);
            int plen = 0;
            int n = 0;
            int shift = 0;
            for (int i = 0; i < qlen; i++) {
                char c = q.charAt(i);
                if (c == ';') {
                    ensurePlayListCapacity(plen + 1);
                    mPlayList[plen] = n;
                    plen++;
                    n = 0;
                    shift = 0;
                } else {
                    if (c >= '0' && c <= '9') {
                        n += ((c - '0') << shift);
                    } else if (c >= 'a' && c <= 'f') {
                        n += ((10 + c - 'a') << shift);
                    } else {
                        // bogus playlist data
                        plen = 0;
                        break;
                    }
                    shift += 4;
                }
            }
            mPlayListLen = plen;

            int pos = mPreferences.getInt("curpos", 0);
            if (pos < 0 || pos >= mPlayListLen) {
                // The saved playlist is bogus, discard it
                mPlayListLen = 0;
                return;
            }
            mPlayPos = pos;

            // When reloadQueue is called in response to a card-insertion,
            // we might not be able to query the media provider right away.
            // To deal with this, try querying for the current file, and if
            // that fails, wait a while and try again. If that too fails,
            // assume there is a problem and don't restore the state.
            Cursor crsr = MusicUtils.query(this,
                        MediaStore.Audio.Media.EXTERNAL_CONTENT_URI,
                        new String [] {"_id"}, "_id=" + mPlayList[mPlayPos] , null, null);
            if (crsr == null || crsr.getCount() == 0) {
                // wait a bit and try again
                if (crsr != null) {
                    crsr.close();
                    crsr = null;
                }
                SystemClock.sleep(3000);
                crsr = getContentResolver().query(
                        MediaStore.Audio.Media.EXTERNAL_CONTENT_URI,
                        mCursorCols, "_id=" + mPlayList[mPlayPos] , null, null);
            }
            if (crsr != null) {
                crsr.close();
            }

            // Make sure we don't auto-skip to the next song, since that
            // also starts playback. What could happen in that case is:
            // - music is paused
            // - go to UMS and delete some files, including the currently playing one
            // - come back from UMS
            // (time passes)
            // - music app is killed for some reason (out of memory)
            // - music service is restarted, service restores state, doesn't find
            //   the "current" file, goes to the next and: playback starts on its
            //   own, potentially at some random inconvenient time.
            mOpenFailedCounter = 20;
            mQuietMode = true;
            openCurrentAndNext();
            mQuietMode = false;
            if (!mPlayer.isInitialized()) {
                // couldn't restore the saved state
                mPlayListLen = 0;
                return;
            }

            long seekpos = mPreferences.getLong("seekpos", 0);
            seek(seekpos >= 0 && seekpos <= duration() ? seekpos : 0);
            Log.d(LOGTAG, "restored queue, currently at position "
                    + position() + "/" + duration()
                    + " (requested " + seekpos + ")");

            int repmode = mPreferences.getInt("repeatmode", REPEAT_NONE);
            if (repmode != REPEAT_ALL && repmode != REPEAT_CURRENT) {
                repmode = REPEAT_NONE;
            }
            mRepeatMode = repmode;

            int shufmode = mPreferences.getInt("shufflemode", SHUFFLE_NONE);
            if (shufmode != SHUFFLE_AUTO && shufmode != SHUFFLE_NORMAL) {
                shufmode = SHUFFLE_NONE;
            }
            if (shufmode != SHUFFLE_NONE) {
                // in shuffle mode we need to restore the history too
                q = mPreferences.getString("history", "");
                qlen = q != null ? q.length() : 0;
                if (qlen > 1) {
                    plen = 0;
                    n = 0;
                    shift = 0;
                    mHistory.clear();
                    for (int i = 0; i < qlen; i++) {
                        char c = q.charAt(i);
                        if (c == ';') {
                            if (n >= mPlayListLen) {
                                // bogus history data
                                mHistory.clear();
                                break;
                            }
                            mHistory.add(n);
                            n = 0;
                            shift = 0;
                        } else {
                            if (c >= '0' && c <= '9') {
                                n += ((c - '0') << shift);
                            } else if (c >= 'a' && c <= 'f') {
                                n += ((10 + c - 'a') << shift);
                            } else {
                                // bogus history data
                                mHistory.clear();
                                break;
                            }
                            shift += 4;
                        }
                    }
                }
            }
            if (shufmode == SHUFFLE_AUTO) {
                if (! makeAutoShuffleList()) {
                    shufmode = SHUFFLE_NONE;
                }
            }
            mShuffleMode = shufmode;
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        mDelayedStopHandler.removeCallbacksAndMessages(null);
        mServiceInUse = true;
        return mBinder;
    }

    @Override
    public void onRebind(Intent intent) {
        mDelayedStopHandler.removeCallbacksAndMessages(null);
        mServiceInUse = true;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (!mIsReadGranted) {
            Toast.makeText(getApplicationContext(),
                    R.string.dialog_content, Toast.LENGTH_LONG).show();
            return START_STICKY;
        }

        updateNotification();
        startForeground(PLAYBACKSERVICE_STATUS, status);

        mServiceStartId = startId;
        mDelayedStopHandler.removeCallbacksAndMessages(null);

        if (intent != null) {
            String action = intent.getAction();
            String cmd = intent.getStringExtra("command");
            MusicUtils.debugLog("onStartCommand " + action + " / " + cmd);

            if (CMDNEXT.equals(cmd) || NEXT_ACTION.equals(action)) {
                sendEmptyMessageIfNo(GOTO_NEXT);
            } else if (CMDPREVIOUS.equals(cmd) || PREVIOUS_ACTION.equals(action)) {
                if (position() < 2000) {
                    sendEmptyMessageIfNo(GOTO_PREV);
                } else {
                    seek(0);
                    play();
                }
            } else if (CMDTOGGLEPAUSE.equals(cmd) || TOGGLEPAUSE_ACTION.equals(action)) {
                if (isPlaying()) {
                    pause(false);
                    mPausedByTransientLossOfFocus = false;
                } else {
                    play();
                }
            } else if (CMDPAUSE.equals(cmd) || PAUSE_ACTION.equals(action)) {
                pause();
                mPausedByTransientLossOfFocus = false;
            } else if (CMDPLAY.equals(cmd)) {
                play();
            } else if (CMDSTOP.equals(cmd)) {
                pause();
                mPausedByTransientLossOfFocus = false;
                seek(0);
            }
        }

        // make sure the service will shut down on its own if it was
        // just started but not bound to and nothing is playing
        mDelayedStopHandler.removeCallbacksAndMessages(null);
        Message msg = mDelayedStopHandler.obtainMessage();
        mDelayedStopHandler.sendMessageDelayed(msg, IDLE_DELAY);
        return START_STICKY;
    }

    @Override
    public boolean onUnbind(Intent intent) {
        mServiceInUse = false;

        // Take a snapshot of the current playlist
        saveQueue(true);

        if (isPlaying() || mPausedByTransientLossOfFocus) {
            // something is currently playing, or will be playing once 
            // an in-progress action requesting audio focus ends, so don't stop the service now.
            return true;
        }

        // If there is a playlist but playback is paused, then wait a while
        // before stopping the service, so that pause/resume isn't slow.
        // Also delay stopping the service if we're transitioning between tracks.
        if (mPlayListLen > 0  || mMediaplayerHandler.hasMessages(TRACK_ENDED)) {
            Message msg = mDelayedStopHandler.obtainMessage();
            mDelayedStopHandler.sendMessageDelayed(msg, IDLE_DELAY);
            return true;
        }

        // No active playlist, OK to stop the service right now
        stopSelf(mServiceStartId);
        return true;
    }

    private Handler mDelayedStopHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            // Check again to make sure nothing is playing right now
            if (isPlaying() || mPausedByTransientLossOfFocus || mServiceInUse
                    || mMediaplayerHandler.hasMessages(TRACK_ENDED)) {
                return;
            }
            // save the queue again, because it might have changed
            // since the user exited the music app (because of
            // party-shuffle or because the play-position changed)
            saveQueue(true);
            stopSelf(mServiceStartId);
        }
    };

    /**
     * Called when we receive a ACTION_MEDIA_EJECT notification.
     *
     * @param storagePath path to mount point for the removed media
     */
    public void closeExternalStorageFiles(String storagePath) {
        // stop playback and clean up if the SD card is going to be unmounted.
        stop(true);
        notifyChange(QUEUE_CHANGED);
        notifyChange(META_CHANGED);
    }

    /**
     * Registers an intent to listen for ACTION_MEDIA_EJECT notifications.
     * The intent will call closeExternalStorageFiles() if the external media
     * is going to be ejected, so applications can clean up any files they have open.
     */
    public void registerExternalStorageListener() {
        if (mUnmountReceiver == null) {
            mUnmountReceiver = new BroadcastReceiver() {
                @Override
                public void onReceive(Context context, Intent intent) {
                    String action = intent.getAction();
                    if (action.equals(Intent.ACTION_MEDIA_EJECT)) {
                        if (mCurrentTrackInfo != null ) {
                            String curTrackPath = mCurrentTrackInfo.mData;
                            // if the currently playing track is on the SD card
                            if (curTrackPath != null && curTrackPath.contains(intent.getData().getPath())) {
                                saveQueue(true);
                                mQueueIsSaveable = false;
                                closeExternalStorageFiles(intent.getData().getPath());
                            }
                        }
                    } else if (action.equals(Intent.ACTION_MEDIA_MOUNTED)) {
                        // when play music in background, delete file in filemanager will not effect music to play
                        if (intent.getStringExtra("FileChange") != null) {
                            notifyChange(QUEUE_CHANGED);
                            notifyChange(META_CHANGED);
                            return;
                        }
                        mMediaMountedCount++;
                        mCardId = MusicUtils.getCardId(MediaPlaybackService.this);

                        // pause playback
                        pause();
                        reloadQueue();
                        mQueueIsSaveable = true;

                        notifyChange(PLAYSTATE_CHANGED);
                        notifyChange(QUEUE_CHANGED);
                        notifyChange(META_CHANGED);
                    } else if (action.equals(ACTION_DELETE_MUSIC)) {
                        long id = intent.getLongExtra("mid", -1);
                        long artindex = intent.getLongExtra("artindex", -1);
                        if (id != -1 && artindex != -1) {
                             MusicUtils.deleteTrack(MediaPlaybackService.this, id, artindex);
                        }
                    }
                }
            };
            IntentFilter iFilter = new IntentFilter();
            iFilter.addAction(Intent.ACTION_MEDIA_EJECT);
            iFilter.addAction(Intent.ACTION_MEDIA_MOUNTED);
            iFilter.addAction(ACTION_DELETE_MUSIC);
            iFilter.addDataScheme("file");
            registerReceiver(mUnmountReceiver, iFilter);
        }
    }

    public void registerA2dpServiceListener() {
        mA2dpReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                String action = intent.getAction();
                String cmd = intent.getStringExtra("command");

                if (MusicUtils.isForbidPlaybackInCall(context)){
                    return;
                }

                if (action.equals(SET_ADDRESSED_PLAYER)) {
                    play(); // this ensures audio focus change is called and play the media
                } else if (action.equals(PLAYSTATUS_REQUEST)) {
                    notifyChange(PLAYSTATUS_RESPONSE);
                } else if (PLAYERSETTINGS_REQUEST.equals(action)) {
                    if (CMDGET.equals(cmd)) {
                        int getCommand = intent.getIntExtra(EXTRA_GET_COMMAND,
                                                            GET_INVALID);
                        byte attribute;
                        byte [] attrIds; byte [] valIds;
                        switch (getCommand) {
                            case GET_ATTRIBUTE_IDS:
                                notifyAttributeIDs(PLAYERSETTINGS_RESPONSE);
                            break;
                            case GET_VALUE_IDS:
                                attribute =
                                    intent.getByteExtra(EXTRA_ATTRIBUTE_ID,
                                                        GET_ATTR_INVALID);
                                notifyValueIDs(PLAYERSETTINGS_RESPONSE, attribute);
                            break;
                            case GET_ATTRIBUTE_TEXT:
                                attrIds = intent.getByteArrayExtra(
                                                     EXTRA_ATTIBUTE_ID_ARRAY);
                                notifyAttributesText(PLAYERSETTINGS_RESPONSE, attrIds);
                            break;
                            case GET_VALUE_TEXT:
                                 attribute =
                                 intent.getByteExtra(EXTRA_ATTRIBUTE_ID,
                                                    GET_ATTR_INVALID);
                                 valIds = intent.getByteArrayExtra(
                                                     EXTRA_VALUE_ID_ARRAY);
                                 notifyAttributeValuesText(
                                     PLAYERSETTINGS_RESPONSE, attribute, valIds);
                            break;
                            case GET_ATTRIBUTE_VALUES:
                                 notifyAttributeValues(PLAYERSETTINGS_RESPONSE,
                                             mAttributePairs, GET_ATTRIBUTE_VALUES);
                            break;
                            default:
                               Log.e(LOGTAG, "invalid getCommand"+getCommand);
                            break;
                        }
                    } else if (CMDSET.equals(cmd)){
                        byte[] attribValuePairs = intent.getByteArrayExtra(
                                                    EXTRA_ATTRIB_VALUE_PAIRS);
                        setValidAttributes(attribValuePairs);
                    }
                }
            }
        };
        IntentFilter iFilter = new IntentFilter();
        iFilter.addAction(SET_ADDRESSED_PLAYER);
        iFilter.addAction(PLAYSTATUS_REQUEST);
        iFilter.addAction(PLAYERSETTINGS_REQUEST);
        registerReceiver(mA2dpReceiver, iFilter);
    }

    /**
     * Notify the change-receivers that something has changed.
     * The intent that is sent contains the following data
     * for the currently playing track:
     * "id" - Integer: the database row ID
     * "artist" - String: the name of the artist
     * "album" - String: the name of the album
     * "track" - String: the name of the track
     * The intent has an action that is one of
     * "com.android.music.metadatachanged"
     * "com.android.music.queuechanged",
     * "com.android.music.playbackcomplete"
     * "com.android.music.playstatechanged"
     * respectively indicating that a new track has
     * started playing, that the playback queue has
     * changed, that playback has stopped because
     * the last file in the list has been played,
     * or that the play-state changed (paused/resumed).
     */
    private void notifyChange(String what) {

        Intent i = new Intent(what);
        i.putExtra("id", Long.valueOf(getAudioId()));
        i.putExtra("artist", getArtistName());
        i.putExtra("album",getAlbumName());
        i.putExtra("track", getTrackName());
        i.putExtra("playing", isPlaying());
        sendStickyBroadcast(i);

        if (what.equals(PLAYSTATE_CHANGED)) {
            updatePlaybackState(false);
        } else if (what.equals(META_CHANGED)) {
            RemoteControlClient.MetadataEditor ed = mRemoteControlClient.editMetadata(true);
            ed.putString(MediaMetadataRetriever.METADATA_KEY_TITLE, getTrackName());
            ed.putString(MediaMetadataRetriever.METADATA_KEY_ALBUM, getAlbumName());
            ed.putString(MediaMetadataRetriever.METADATA_KEY_ARTIST, getArtistName());
            ed.putLong(MediaMetadataRetriever.METADATA_KEY_DURATION, duration());
            if ((mPlayList != null) && (mPlayPos >= 0) && (mPlayPos < mPlayList.length)) {
                ed.putLong(MediaMetadataRetriever.METADATA_KEY_DISC_NUMBER,
                                                            mPlayList[mPlayPos]);
            } else {
                ed.putLong(MediaMetadataRetriever.METADATA_KEY_DISC_NUMBER,
                                                                INVALID_SONG_UID);
            }
            ed.putLong(MediaMetadataRetriever.METADATA_KEY_CD_TRACK_NUMBER, mPlayPos);
            try {
                ed.putLong(MediaMetadataRetriever.METADATA_KEY_NUM_TRACKS, mPlayListLen);
            } catch (IllegalArgumentException e) {
                Log.e(LOGTAG, "METADATA_KEY_NUM_TRACKS: failed: " + e);
            }
            Bitmap b = MusicUtils.getArtwork(this, getAudioId(), getAlbumId(), false);
            if (b != null) {
                ed.putBitmap(MetadataEditor.BITMAP_KEY_ARTWORK, b);
            } else {
                Context context = MediaPlaybackService.this;
                Resources r = context.getResources();
                b = BitmapFactory.decodeResource(r, R.drawable.album_cover);
                ed.putBitmap(MetadataEditor.BITMAP_KEY_ARTWORK, b);
            }
            ed.apply();
        }

        if (what.equals(QUEUE_CHANGED)) {
            saveQueue(true);
        } else {
            saveQueue(false);
        }

        // Share this notification directly with our widgets
        mAppWidgetProvider.notifyChange(this, what);
        mAppWidgetProviderLarge.notifyChange(this, what);
    }

    private void ensurePlayListCapacity(int size) {
        if (mPlayList == null || size > mPlayList.length) {
            // reallocate at 2x requested size so we don't
            // need to grow and copy the array for every
            // insert
            long [] newlist = new long[size * 2];
            int len = mPlayList != null ? mPlayList.length : mPlayListLen;
            for (int i = 0; i < len; i++) {
                newlist[i] = mPlayList[i];
            }
            mPlayList = newlist;
        }
        // FIXME: shrink the array when the needed size is much smaller
        // than the allocated size
    }

    // insert the list of songs at the specified position in the playlist
    private void addToPlayList(long [] list, int position) {
        int addlen = list.length;
        if (position < 0) { // overwrite
            mPlayListLen = 0;
            position = 0;
        }
        ensurePlayListCapacity(mPlayListLen + addlen);
        if (position > mPlayListLen) {
            position = mPlayListLen;
        }

        // move part of list after insertion point
        int tailsize = mPlayListLen - position;
        for (int i = tailsize ; i > 0 ; i--) {
            mPlayList[position + i] = mPlayList[position + i - addlen]; 
        }

        // copy list into playlist
        for (int i = 0; i < addlen; i++) {
            mPlayList[position + i] = list[i];
        }
        mPlayListLen += addlen;
        if (mPlayListLen == 0) {
            mCurrentTrackInfo = null;
            notifyChange(META_CHANGED);
        }
    }

    /**
     * Appends a list of tracks to the current playlist.
     * If nothing is playing currently, playback will be started at
     * the first track.
     * If the action is NOW, playback will switch to the first of
     * the new tracks immediately.
     * @param list The list of tracks to append.
     * @param action NOW, NEXT or LAST
     */
    public void enqueue(long [] list, int action) {
        synchronized(this) {
            if (action == NEXT && mPlayPos + 1 < mPlayListLen) {
                addToPlayList(list, mPlayPos + 1);
                notifyChange(QUEUE_CHANGED);
            } else {
                // action == LAST || action == NOW || mPlayPos + 1 == mPlayListLen
                addToPlayList(list, Integer.MAX_VALUE);
                notifyChange(QUEUE_CHANGED);
                if (action == NOW) {
                    mPlayPos = mPlayListLen - list.length;
                    openCurrentAndNext();
                    play();
                    notifyChange(META_CHANGED);
                    return;
                }
            }
            if (mPlayPos < 0) {
                mPlayPos = 0;
                openCurrentAndNext();
                play();
                notifyChange(META_CHANGED);
            }
        }
    }

    /**
     * Replaces the current playlist with a new list,
     * and prepares for starting playback at the specified
     * position in the list, or a random position if the
     * specified position is 0.
     * @param list The new list of tracks.
     */
    public void open(long [] list, int position) {
        synchronized (this) {
            if (mShuffleMode == SHUFFLE_AUTO) {
                mShuffleMode = SHUFFLE_NORMAL;
            }
            long oldId = getAudioId();
            int listlength = list.length;
            boolean newlist = true;
            if (mPlayListLen == listlength) {
                // possible fast path: list might be the same
                newlist = false;
                for (int i = 0; i < listlength; i++) {
                    if (list[i] != mPlayList[i]) {
                        newlist = true;
                        break;
                    }
                }
            }
            if (newlist) {
                addToPlayList(list, -1);
                notifyChange(QUEUE_CHANGED);
            }
            int oldpos = mPlayPos;
            if (position >= 0) {
                mPlayPos = position;
            } else {
                mPlayPos = mRand.nextInt(mPlayListLen);
            }
            mHistory.clear();

            saveBookmarkIfNeeded();
            // avoid "Selected playlist is empty" flicks in the music widget
            mAppWidgetProvider.setPauseState(true);
            mAppWidgetProviderLarge.setPauseState(true);
            openCurrentAndNext();
            if (oldId != getAudioId()) {
                notifyChange(META_CHANGED);
            }
            mAppWidgetProvider.setPauseState(false);
            mAppWidgetProviderLarge.setPauseState(false);
        }
    }

    /**
     * Moves the item at index1 to index2.
     * @param index1
     * @param index2
     */
    public void moveQueueItem(int index1, int index2) {
        synchronized (this) {
            if (index1 >= mPlayListLen) {
                index1 = mPlayListLen - 1;
            }
            if (index2 >= mPlayListLen) {
                index2 = mPlayListLen - 1;
            }
            if (index1 < index2) {
                long tmp = mPlayList[index1];
                for (int i = index1; i < index2; i++) {
                    mPlayList[i] = mPlayList[i+1];
                }
                mPlayList[index2] = tmp;
                if (mPlayPos == index1) {
                    mPlayPos = index2;
                } else if (mPlayPos >= index1 && mPlayPos <= index2) {
                        mPlayPos--;
                }
            } else if (index2 < index1) {
                long tmp = mPlayList[index1];
                for (int i = index1; i > index2; i--) {
                    mPlayList[i] = mPlayList[i-1];
                }
                mPlayList[index2] = tmp;
                if (mPlayPos == index1) {
                    mPlayPos = index2;
                } else if (mPlayPos >= index2 && mPlayPos <= index1) {
                        mPlayPos++;
                }
            }
            setNextTrack();
            notifyChange(QUEUE_CHANGED);
        }
    }

    /**
     * Returns the current play list
     * @return An array of integers containing the IDs of the tracks in the play list
     */
    public long [] getQueue() {
        synchronized (this) {
            int len = mPlayListLen;
            long [] list = new long[len];
            for (int i = 0; i < len; i++) {
                list[i] = mPlayList[i];
            }
            return list;
        }
    }

    private TrackInfo getTrackInfoFromId(long lid) {
        String id = String.valueOf(lid);
        TrackInfo ret = null;
        Cursor c = null;
        try {
            c = getContentResolver().query(
                    MediaStore.Audio.Media.EXTERNAL_CONTENT_URI,
                    mCursorCols, "_id=" + id, null, null);
            if (c != null && c.getCount() > 0) {
                c.moveToFirst();
                ret = parseTrackInfoFromCurrentTrackInfo(c);
            } else {
                if (c != null) {
                    c.close();
                    c = null;
                }
            }
        } catch (Exception e) {
            ret = null;
        } finally {
            if (c != null) {
                c.close();
                c = null;
            }
        }
        return ret;
    }

    private TrackInfo parseTrackInfoFromCurrentTrackInfo(Cursor c) {
        TrackInfo ret = new TrackInfo();
        try {
            ret.mId = c.getLong(IDCOLIDX);
            ret.mAlbumId = c.getLong(c.getColumnIndexOrThrow(MediaStore.Audio.Media.ALBUM_ID));
            ret.mAlbumName = c.getString(c.getColumnIndexOrThrow(MediaStore.Audio.Media.ALBUM));
            ret.mArtistId = c.getLong(c.getColumnIndexOrThrow(MediaStore.Audio.Media.ARTIST_ID));
            ret.mArtistName = c.getString(c.getColumnIndexOrThrow(MediaStore.Audio.Media.ARTIST));
            ret.mTrackName = c.getString(c.getColumnIndexOrThrow(MediaStore.Audio.Media.TITLE));
            ret.mData = c.getString(c.getColumnIndexOrThrow(MediaStore.Audio.Media.DATA));
            ret.mPodcast = c.getInt(PODCASTCOLIDX);
            ret.mBookmark = c.getLong(c.getColumnIndexOrThrow(MediaStore.Audio.Media.BOOKMARK));
        } catch (IllegalArgumentException e) {
            ret = null;
        }
        return ret;
    }

    private void openCurrentAndNext() {
        synchronized (this) {
            if (mPlayListLen == 0) {
                return;
            }
            updatePlaybackState(true);
            stop(false);

            mCurrentTrackInfo = getTrackInfoFromId(mPlayList[mPlayPos]);
            if (null == mCurrentTrackInfo) return;
            while(true) {
                if (open(MediaStore.Audio.Media.EXTERNAL_CONTENT_URI + "/" +
                                mCurrentTrackInfo.mId)) {
                    break;
                }
                // if we get here then opening the file failed. We can close the cursor now, because
                // we're either going to create a new one next, or stop trying
                if (mOpenFailedCounter++ < 10 &&  mPlayListLen > 1) {
                    int pos = getNextPosition(false);
                    if (pos < 0) {
                        gotoIdleState();
                        if (mIsSupposedToBePlaying) {
                            mIsSupposedToBePlaying = false;
                            notifyChange(PLAYSTATE_CHANGED);
                        }
                        return;
                    }
                    mPlayPos = pos;
                    stop(false);
                    mPlayPos = pos;
                    mCurrentTrackInfo = getTrackInfoFromId(mPlayList[mPlayPos]);
                } else {
                    mOpenFailedCounter = 0;
                    if (!mQuietMode) {
                        Toast.makeText(this, R.string.playback_failed, Toast.LENGTH_SHORT).show();
                    }
                    Log.d(LOGTAG, "Failed to open file for playback");
                    gotoIdleState();
                    if (mIsSupposedToBePlaying) {
                        mIsSupposedToBePlaying = false;
                        notifyChange(PLAYSTATE_CHANGED);
                    }
                    return;
                }
            }

            // go to bookmark if needed
            if (isPodcast()) {
                long bookmark = getBookmark();
                // Start playing a little bit before the bookmark,
                // so it's easier to get back in to the narrative.
                seek(bookmark - 5000);
            }
            setNextTrack();
        }
    }

    private void setNextTrack() {
        mNextPlayPos = getNextPosition(false);

        // remove the tail which is the next track
        // from history vector in shuffle mode.
        int histsize = mHistory.size();
        if (histsize > 0 && mShuffleMode == SHUFFLE_NORMAL) {
            mHistory.remove(histsize - 1);
        }
        if (mNextPlayPos >= 0) {
            long id = mPlayList[mNextPlayPos];
            mPlayer.setNextDataSource(MediaStore.Audio.Media.EXTERNAL_CONTENT_URI + "/" + id);
        } else {
            mPlayer.setNextDataSource(null);
        }
    }

    public String getRealPathFromContentURI(Context context, Uri contentUri) {
          Cursor cursor = null;
          try {
            String[] projection = { MediaStore.Images.Media.DATA };
            cursor = context.getContentResolver().query(contentUri,  projection, null, null, null);
            if (cursor == null) {
                return null;
            }
            int colIndex = cursor.getColumnIndexOrThrow(MediaStore.Audio.Media.DATA);
            cursor.moveToFirst();
            return cursor.getString(colIndex);
          } finally {
            if (cursor != null) {
              cursor.close();
            }
          }
        }

    /**
     * Opens the specified file and readies it for playback.
     *
     * @param path The full path of the file to be opened.
     */
    public boolean open(String path) {
        synchronized (this) {
            if (path == null) {
                return false;
            }
            String actualFilePath = "";
            int status = 0;
            // if mCurrentTrackInfo is null, try to associate path with a database cursor
            if (mCurrentTrackInfo == null) {
                ContentResolver resolver = getContentResolver();
                Uri uri;
                String where;
                String selectionArgs[];
                if (path.startsWith("content://media/")) {
                    uri = Uri.parse(path);
                    where = null;
                    selectionArgs = null;
                } else {
                   uri = MediaStore.Audio.Media.getContentUriForPath(path);
                   where = MediaStore.Audio.Media.DATA + "=?";
                   selectionArgs = new String[] { path };
                }
                Cursor c = null;
                try {
                    c = resolver.query(uri, mCursorCols, where, selectionArgs, null);
                    if  (c != null) {
                        if (c.getCount() == 0) {
                            c.close();
                            c = null;
                            mCurrentTrackInfo = null;
                        } else {
                            c.moveToNext();
                            mCurrentTrackInfo = parseTrackInfoFromCurrentTrackInfo(c);
                            ensurePlayListCapacity(1);
                            mPlayListLen = 1;
                            mPlayList[0] = mCurrentTrackInfo.mId;
                            mPlayPos = 0;
                        }
                    }
                    notifyChange(META_CHANGED);
                } catch (Exception ex) {
                    mCurrentTrackInfo = null;
                } finally {
                    if (c != null) {
                        c.close();
                        c = null;
                    }
                }
            }

            if (mCurrentTrackInfo != null) {
                actualFilePath = mCurrentTrackInfo.mData;
            }
            //TODO: DRM changes here.
//            if (actualFilePath != null
//                    && (actualFilePath.endsWith(".dm")
//                            || actualFilePath.endsWith(".dcf"))) {
//                DrmManagerClientWrapper drmClient = new DrmManagerClientWrapper(this);
//                actualFilePath = actualFilePath.replace("/storage/emulated/0", "/storage/emulated/legacy");
//                status = drmClient.checkRightsStatus(actualFilePath, Action.PLAY);
//                if (RightsStatus.RIGHTS_VALID != status) {
//                    Toast.makeText(this, R.string.drm_right_expired,
//                            Toast.LENGTH_SHORT).show();
//                }
//                if (drmClient != null) drmClient.release();
//            }
            mFileToPlay = path;
            mPlayer.setDataSource(mFileToPlay);
            if (mPlayer.isInitialized()) {
                mOpenFailedCounter = 0;
                return true;
            }
            stop(true);
            return false;
        }
    }

    /**
     * Starts playback of a previously opened file.
     */
    public void play() {
        if (mAudioManager == null) {
            mAudioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        }
        mAudioManager.requestAudioFocus(mAudioFocusListener, AudioManager.STREAM_MUSIC,
                AudioManager.AUDIOFOCUS_GAIN);

        if (mPlayer.isInitialized()) {
            // if we are at the end of the song, playing it again.
            if (mControlInStatusBar) {
                long duration = mPlayer.duration();
                long currentpos = mPlayer.position();
                if (mRepeatMode != REPEAT_CURRENT && mRepeatMode != REPEAT_ALL
                        && mPlayPos >= mPlayListLen - 1 && currentpos > 0
                        && currentpos == duration) {
                    seek(0);
                }
            }
            mPlayer.start();
            // make sure we fade in, in case a previous fadein was stopped because
            // of another focus loss
            mMediaplayerHandler.removeMessages(FADEDOWN);
            mMediaplayerHandler.sendEmptyMessage(FADEUP);

            if (!mIsSupposedToBePlaying) {
                mIsSupposedToBePlaying = true;
                notifyChange(PLAYSTATE_CHANGED);
            }

            updateNotification();

        } else if (mPlayListLen <= 0) {
            // This is mostly so that if you press 'play' on a bluetooth headset
            // without every having played anything before, it will still play
            // something.
            setShuffleMode(SHUFFLE_AUTO);
        }

        //update the playback status to RCC
        updatePlaybackState(false);

        if (views != null && viewsLarge != null && status != null) {
            // Reset notification play function to pause function
            views.setImageViewResource(R.id.pause,
                    isPlaying() ? R.drawable.notification_pause
                            : R.drawable.notification_play);
            viewsLarge.setImageViewResource(R.id.pause,
                    isPlaying() ? R.drawable.notification_pause
                            : R.drawable.notification_play);
            Intent pauseIntent = new Intent(PAUSE_ACTION);
            PendingIntent pausePendingIntent = PendingIntent.getBroadcast(this,
                    0 /* no requestCode */, pauseIntent, 0 /* no flags */);
            views.setOnClickPendingIntent(R.id.pause, pausePendingIntent);
            viewsLarge.setOnClickPendingIntent(R.id.pause, pausePendingIntent);
            status.flags = Notification.FLAG_ONGOING_EVENT;
            startForeground(PLAYBACKSERVICE_STATUS, status);

        }
    }

    @Override
    public void onTaskRemoved(Intent rootIntent) {
        super.onTaskRemoved(rootIntent);
        NotificationManager nm = (NotificationManager) getSystemService
                (Context.NOTIFICATION_SERVICE);
        nm.cancel(PLAYBACKSERVICE_STATUS);
    }

    private void updateNotification() {

        views = new RemoteViews(getPackageName(), R.layout.statusbar_appwidget_s);
        viewsLarge = new RemoteViews(getPackageName(), R.layout.statusbar_appwidget_l);
        if (getApplicationContext().getResources().getBoolean(R.bool.exit_in_notification)) {
            views.setViewVisibility(R.id.exit, View.VISIBLE);
            viewsLarge.setViewVisibility(R.id.exit, View.VISIBLE);
        }
        Bitmap icon = MusicUtils.getArtwork(this, getAudioId(), getAlbumId(),
                true);
        if (icon == null) {
            views.setImageViewResource(R.id.icon, R.drawable.album_cover_background);
            viewsLarge.setImageViewResource(R.id.icon, R.drawable.album_cover_background);
        } else {
            views.setImageViewBitmap(R.id.icon, icon);
            viewsLarge.setImageViewBitmap(R.id.icon, icon);
        }

        if (MusicUtils.isForbidPlaybackInCall(getApplicationContext()) == false){
            Intent prevIntent = new Intent(PREVIOUS_ACTION);
            PendingIntent prevPendingIntent = PendingIntent.getBroadcast(this,
                    0 /* no requestCode */, prevIntent, 0 /* no flags */);
            views.setOnClickPendingIntent(R.id.prev, prevPendingIntent);
            viewsLarge.setOnClickPendingIntent(R.id.prev, prevPendingIntent);

            Intent toggleIntent = new Intent(MediaPlaybackService.TOGGLEPAUSE_ACTION);
            PendingIntent togglePendingIntent = PendingIntent.getBroadcast(this,
                    0 /* no requestCode */, toggleIntent, 0 /* no flags */);
            views.setOnClickPendingIntent(R.id.pause, togglePendingIntent);
            viewsLarge.setOnClickPendingIntent(R.id.pause, togglePendingIntent);

            Intent nextIntent = new Intent(NEXT_ACTION);
            PendingIntent nextPendingIntent = PendingIntent.getBroadcast(this,
                    0 /* no requestCode */, nextIntent, 0 /* no flags */);
            views.setOnClickPendingIntent(R.id.next, nextPendingIntent);
            viewsLarge.setOnClickPendingIntent(R.id.next, nextPendingIntent);

            Intent exitIntent = new Intent(EXIT_ACTION);
            PendingIntent exitPendingIntent = PendingIntent.getBroadcast(this,
                    0 /* no requestCode */, exitIntent, 0 /* no flags */);
            views.setOnClickPendingIntent(R.id.exit, exitPendingIntent);
            viewsLarge.setOnClickPendingIntent(R.id.exit, exitPendingIntent);
        }

        if (getAudioId() < 0) {
            // streaming
            views.setTextViewText(R.id.trackname, getPath());
            views.setTextViewText(R.id.artist, null);
            views.setTextViewText(R.id.album, null);

            viewsLarge.setTextViewText(R.id.trackname, getPath());
            viewsLarge.setTextViewText(R.id.artist, null);
            viewsLarge.setTextViewText(R.id.album, null);
        } else {
            String artist = getArtistName();
            views.setTextViewText(R.id.trackname, getTrackName());
            viewsLarge.setTextViewText(R.id.trackname, getTrackName());
            if (artist == null || artist.equals(MediaStore.UNKNOWN_STRING)) {
                artist = getString(R.string.unknown_artist_name);
            }
            String album = getAlbumName();
            if (album == null || album.equals(MediaStore.UNKNOWN_STRING)) {
                album = getString(R.string.unknown_album_name);
            }
            views.setTextViewText(R.id.artist, artist);
            views.setTextViewText(R.id.album, album);
            viewsLarge.setTextViewText(R.id.artist, artist);
            viewsLarge.setTextViewText(R.id.album, album);
        }

        views.setImageViewResource(R.id.pause,
                (isPlaying() ? R.drawable.notification_pause
                        : R.drawable.notification_play));

        viewsLarge.setImageViewResource(R.id.pause,
                (isPlaying() ? R.drawable.notification_pause
                        : R.drawable.notification_play));

        NotificationManager nm = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        NotificationCompat.Builder status1 = new NotificationCompat.Builder(
                this);
        status1.setContent(views);
        status1.setContentIntent(PendingIntent.getActivity(this, 0, new Intent(
                "com.android.music.PLAYBACK_VIEWER")
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK), 0));
        status1.setSmallIcon(R.drawable.stat_notify_musicplayer);

        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O){
            NotificationChannel notificationChannel = null;

            notificationChannel = new NotificationChannel(CHANNEL_ONE_ID,
                    CHANNEL_ONE_NAME, NotificationManager.IMPORTANCE_LOW);
            notificationChannel.enableLights(true);
            notificationChannel.setLightColor(Color.RED);
            notificationChannel.setShowBadge(true);
            notificationChannel.setLockscreenVisibility(Notification.VISIBILITY_PUBLIC);
            nm.createNotificationChannel(notificationChannel);
            status1.setChannelId(CHANNEL_ONE_ID);
        }

        status = status1.build();
        status.bigContentView = viewsLarge;
        if (isPlaying()) {
            status.flags |= Notification.FLAG_ONGOING_EVENT;
        } else {
            status.flags = 0;
        }
        nm.notify(PLAYBACKSERVICE_STATUS, status);

    }

    private void stop(boolean remove_status_icon) {
        if (remove_status_icon) {
            mIsSupposedToBePlaying = false;
            notifyChange(PLAYSTATE_CHANGED);
        }
        if (mPlayer != null && mPlayer.isInitialized()) {
            mPlayer.stop();
        }
        mFileToPlay = null;
        mCurrentTrackInfo = null;
        if (remove_status_icon) {
            gotoIdleState();
            NotificationManager nm = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
            nm.cancel(PLAYBACKSERVICE_STATUS);
        } else {
            stopForeground(false);
        }
    }

    /**
     * Stops playback.
     */
    public void stop() {
        stop(true);
    }

    /**
     * Pauses playback (call play() to resume)
     */
    public void pause() {
        pause(true);
    }

    /**
     * Pauses playback (based on input to decide idle or not)
     */
    public void pause(boolean idle) {
        synchronized (this) {
            mMediaplayerHandler.removeMessages(FADEUP);
            if (isPlaying()) {
                mPlayer.pause();
                mIsSupposedToBePlaying = false;
                if (idle) {
                    gotoIdleState();
                } else {
                    updateNotification();
                }
                notifyChange(PLAYSTATE_CHANGED);
                saveBookmarkIfNeeded();
                // Reset notification pause function to play function
                views.setImageViewResource(R.id.pause,
                        R.drawable.notification_play);
                viewsLarge.setImageViewResource(R.id.pause,
                        R.drawable.notification_play);
                Intent playIntent = new Intent(TOGGLEPAUSE_ACTION);
                PendingIntent playPendingIntent = PendingIntent
                        .getBroadcast(this, 0 /* no requestCode */, playIntent,
                                0 /* no flags */);
                views.setOnClickPendingIntent(R.id.pause, playPendingIntent);
                viewsLarge.setOnClickPendingIntent(R.id.pause, playPendingIntent);
                status.flags = 0;
                NotificationManager nm = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
                nm.notify(PLAYBACKSERVICE_STATUS, status);
            }
        }
    }

    /** Returns whether something is currently playing
     *
     * @return true if something is playing (or will be playing shortly, in case
     * we're currently transitioning between tracks), false if not.
     */
    public boolean isPlaying() {
        return mIsSupposedToBePlaying;
    }

    /*
      Desired behavior for prev/next/shuffle:

      - NEXT will move to the next track in the list when not shuffling, and to
        a track randomly picked from the not-yet-played tracks when shuffling.
        If all tracks have already been played, pick from the full set, but
        avoid picking the previously played track if possible.
      - when shuffling, PREV will go to the previously played track. Hitting PREV
        again will go to the track played before that, etc. When the start of the
        history has been reached, PREV is a no-op.
        When not shuffling, PREV will go to the sequentially previous track (the
        difference with the shuffle-case is mainly that when not shuffling, the
        user can back up to tracks that are not in the history).

        Example:
        When playing an album with 10 tracks from the start, and enabling shuffle
        while playing track 5, the remaining tracks (6-10) will be shuffled, e.g.
        the final play order might be 1-2-3-4-5-8-10-6-9-7.
        When hitting 'prev' 8 times while playing track 7 in this example, the
        user will go to tracks 9-6-10-8-5-4-3-2. If the user then hits 'next',
        a random track will be picked again. If at any time user disables shuffling
        the next/previous track will be picked in sequential order again.
     */

    public void prev() {
        synchronized (this) {
            if (mShuffleMode == SHUFFLE_NORMAL) {
                // go to previously-played track and remove it from the history
                int histsize = mHistory.size();
                if (histsize == 0) {
                    // prev is a no-op
                    return;
                }
                Integer pos = mHistory.remove(histsize - 1);
                mPlayPos = pos.intValue();
                if (mPlayPos >= mPlayList.length) {
                    mPlayPos = mRand.nextInt(mPlayListLen);
                    mHistory.clear();
                }
            } else {
                if (mPlayPos > 0) {
                    mPlayPos--;
                } else {
                    mPlayPos = mPlayListLen - 1;
                }
            }
            saveBookmarkIfNeeded();
            stop(false);
            openCurrentAndNext();
            play();
            notifyChange(META_CHANGED);
            notifyChange(PLAYSTATE_CHANGED);
        }
    }

    /**
     * Get the mHistory size to decide whether to prev
     */
    public int getHistSize() {
        return mHistory.size();
    }

    /**
     * Get the next position to play. Note that this may actually modify mPlayPos
     * if playback is in SHUFFLE_AUTO mode and the shuffle list window needed to
     * be adjusted. Either way, the return value is the next value that should be
     * assigned to mPlayPos;
     */
    private int getNextPosition(boolean force) {
        if (mRepeatMode == REPEAT_CURRENT) {
            if (!force) {
                return mPlayPos;
            } else if ((mPlayPos < 0)||(mPlayPos >= mPlayListLen - 1)) {
                return 0;
            } else {
                return mPlayPos + 1;
            }
        } else if (mShuffleMode == SHUFFLE_NORMAL) {
            // Pick random next track from the not-yet-played ones
            // TODO: make it work right after adding/removing items in the queue.

            // Store the current file in the history, but keep the history at a
            // reasonable size
            if (mPlayPos >= 0) {
                mHistory.add(mPlayPos);
            }
            if (mHistory.size() > MAX_HISTORY_SIZE) {
                mHistory.removeElementAt(0);
            }

            int numTracks = mPlayListLen;
            int[] tracks = new int[numTracks];
            for (int i=0;i < numTracks; i++) {
                tracks[i] = i;
            }

            int numHistory = mHistory.size();
            int numUnplayed = numTracks;
            for (int i=0;i < numHistory; i++) {
                int idx = mHistory.get(i).intValue();
                if (idx < numTracks && tracks[idx] >= 0) {
                    numUnplayed--;
                    tracks[idx] = -1;
                }
            }

            // 'numUnplayed' now indicates how many tracks have not yet
            // been played, and 'tracks' contains the indices of those
            // tracks.
            if (numUnplayed <=0) {
                // everything's already been played
                if (mRepeatMode == REPEAT_ALL || force) {
                    if(numTracks <= 0) {
                        return -1;
                    }
                    //pick from full set
                    numUnplayed = numTracks;
                    for (int i=0;i < numTracks; i++) {
                        tracks[i] = i;
                    }
                } else {
                    // all done
                    return -1;
                }
            }
            int skip = mRand.nextInt(numUnplayed);
            int cnt = -1;
            while (true) {
                while (tracks[++cnt] < 0)
                    ;
                skip--;
                if (skip < 0) {
                    break;
                }
            }
            return cnt;
        } else if (mShuffleMode == SHUFFLE_AUTO) {
            doAutoShuffleUpdate();
            return mPlayPos + 1;
        } else {
            if (mPlayPos >= mPlayListLen - 1) {
                // we're at the end of the list
                if (mRepeatMode == REPEAT_NONE && !force) {
                    // all done
                    return -1;
                } else if (mRepeatMode == REPEAT_ALL || force) {
                    return 0;
                }
                return -1;
            } else {
                return mPlayPos + 1;
            }
        }
    }

    public void gotoNext(boolean force) {
        synchronized (this) {
            if (mPlayListLen <= 0) {
                Log.d(LOGTAG, "No play queue");
                return;
            }
            int pos = getNextPosition(force);
            if (pos < 0) {
                gotoIdleState();
                if (mIsSupposedToBePlaying) {
                    mIsSupposedToBePlaying = false;
                    notifyChange(PLAYSTATE_CHANGED);
                }

                // no more clip, then reset playback state icon in status bar
                if (views != null && viewsLarge != null){
                    views.setImageViewResource(R.id.pause, R.drawable.notification_play);
                    viewsLarge.setImageViewResource(R.id.pause, R.drawable.notification_play);
                    Intent playIntent = new Intent(TOGGLEPAUSE_ACTION);
                    PendingIntent playPendingIntent = PendingIntent.getBroadcast(this,
                            0 /* no requestCode */, playIntent, 0 /* no flags */);
                    views.setOnClickPendingIntent(R.id.pause, playPendingIntent);
                    viewsLarge.setOnClickPendingIntent(R.id.pause, playPendingIntent);
                   // startForeground(PLAYBACKSERVICE_STATUS, status);
                    status.flags = Notification.FLAG_ONGOING_EVENT;
                    NotificationManager nm = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
                    nm.notify(PLAYBACKSERVICE_STATUS, status);
                }
                return;
            }
            mPlayPos = pos;
            saveBookmarkIfNeeded();
            // avoid "Selected playlist is empty" flicks in the music widget
            mAppWidgetProvider.setPauseState(true);
            mAppWidgetProviderLarge.setPauseState(true);
            stop(false);
            mPlayPos = pos;
            openCurrentAndNext();
            mAppWidgetProvider.setPauseState(false);
            mAppWidgetProviderLarge.setPauseState(false);
            play();
            notifyChange(META_CHANGED);
            notifyChange(PLAYSTATE_CHANGED);
        }
    }

    private void gotoIdleState() {
        mDelayedStopHandler.removeCallbacksAndMessages(null);
        Message msg = mDelayedStopHandler.obtainMessage();
        mDelayedStopHandler.sendMessageDelayed(msg, IDLE_DELAY);
        if (! mControlInStatusBar) {
            stopForeground(false);
        }
    }

    private void saveBookmarkIfNeeded() {
        try {
            if (isPodcast()) {
                long pos = position();
                long bookmark = getBookmark();
                long duration = duration();
                if ((pos < bookmark && (pos + 10000) > bookmark) ||
                        (pos > bookmark && (pos - 10000) < bookmark)) {
                    // The existing bookmark is close to the current
                    // position, so don't update it.
                    return;
                }
                if (pos < 15000 || (pos + 10000) > duration) {
                    // if we're near the start or end, clear the bookmark
                    pos = 0;
                }

                // write 'pos' to the bookmark field
                ContentValues values = new ContentValues();
                values.put(MediaStore.Audio.Media.BOOKMARK, pos);
                Uri uri = ContentUris.withAppendedId(
                        MediaStore.Audio.Media.EXTERNAL_CONTENT_URI, mCurrentTrackInfo.mId);
                getContentResolver().update(uri, values, null, null);
            }
        } catch (SQLiteException ex) {
        }
    }

    // Make sure there are at least 5 items after the currently playing item
    // and no more than 10 items before.
    private void doAutoShuffleUpdate() {
        boolean notify = false;

        // remove old entries
        if (mPlayPos > 10) {
            removeTracks(0, mPlayPos - 9);
            notify = true;
        }
        // add new entries if needed
        int to_add = 7 - (mPlayListLen - (mPlayPos < 0 ? -1 : mPlayPos));
        for (int i = 0; i < to_add; i++) {
            // pick something at random from the list

            int lookback = mHistory.size();
            int idx = -1;
            while(true) {
                idx = mRand.nextInt(mAutoShuffleList.length);
                if (!wasRecentlyUsed(idx, lookback)) {
                    break;
                }
                lookback /= 2;
            }
            mHistory.add(idx);
            if (mHistory.size() > MAX_HISTORY_SIZE) {
                mHistory.remove(0);
            }
            ensurePlayListCapacity(mPlayListLen + 1);
            mPlayList[mPlayListLen++] = mAutoShuffleList[idx];
            notify = true;
        }
        if (notify) {
            notifyChange(QUEUE_CHANGED);
        }
    }

    // check that the specified idx is not in the history (but only look at at
    // most lookbacksize entries in the history)
    private boolean wasRecentlyUsed(int idx, int lookbacksize) {

        // early exit to prevent infinite loops in case idx == mPlayPos
        if (lookbacksize == 0) {
            return false;
        }

        int histsize = mHistory.size();
        if (histsize < lookbacksize) {
            Log.d(LOGTAG, "lookback too big");
            lookbacksize = histsize;
        }
        int maxidx = histsize - 1;
        for (int i = 0; i < lookbacksize; i++) {
            long entry = mHistory.get(maxidx - i);
            if (entry == idx) {
                return true;
            }
        }
        return false;
    }

    // A simple variation of Random that makes sure that the
    // value it returns is not equal to the value it returned
    // previously, unless the interval is 1.
    private static class Shuffler {
        private int mPrevious;
        private Random mRandom = new Random();
        public int nextInt(int interval) {
            int ret;
            do {
                ret = mRandom.nextInt(interval);
            } while (ret == mPrevious && interval > 1);
            mPrevious = ret;
            return ret;
        }
    };

    private boolean makeAutoShuffleList() {
        ContentResolver res = getContentResolver();
        Cursor c = null;
        try {
            c = res.query(MediaStore.Audio.Media.EXTERNAL_CONTENT_URI,
                    new String[] {MediaStore.Audio.Media._ID}, MediaStore.Audio.Media.IS_MUSIC + "=1",
                    null, null);
            if (c == null || c.getCount() == 0) {
                return false;
            }
            int len = c.getCount();
            long [] list = new long[len];
            for (int i = 0; i < len; i++) {
                c.moveToNext();
                list[i] = c.getLong(0);
            }
            mAutoShuffleList = list;
            return true;
        } catch (RuntimeException ex) {
        } finally {
            if (c != null) {
                c.close();
            }
        }
        return false;
    }

    /**
     * Removes the range of tracks specified from the play list. If a file within the range is
     * the file currently being played, playback will move to the next file after the
     * range.
     * @param first The first file to be removed
     * @param last The last file to be removed
     * @return the number of tracks deleted
     */
    public int removeTracks(int first, int last) {
        // if the next track is to be removed, clear data source
        // of the next player
        if (mNextPlayPos >= 0) {
            long nextPlayId = mPlayList[mNextPlayPos];
            if (nextPlayId >= first && nextPlayId <= last) {
                mPlayer.setNextDataSource(null);
            }
        }
        int numremoved = removeTracksInternal(first, last);
        if (numremoved > 0) {
            notifyChange(QUEUE_CHANGED);
        }
        return numremoved;
    }

    private int removeTracksInternal(int first, int last) {
        synchronized (this) {
            if (last < first) return 0;
            if (first < 0) first = 0;
            if (last >= mPlayListLen) last = mPlayListLen - 1;

            boolean gotonext = false;
            if (first <= mPlayPos && mPlayPos <= last) {
                mPlayPos = first;
                gotonext = true;
            } else if (mPlayPos > last) {
                mPlayPos -= (last - first + 1);
            }
            int num = mPlayListLen - last - 1;
            for (int i = 0; i < num; i++) {
                mPlayList[first + i] = mPlayList[last + 1 + i];
            }
            mPlayListLen -= last - first + 1;

            if (gotonext) {
                if (mPlayListLen == 0) {
                    stop(true);
                    mPlayPos = -1;
                    mCurrentTrackInfo = null;
                } else {
                    if (mPlayPos >= mPlayListLen) {
                        mPlayPos = 0;
                    }
                    boolean wasPlaying = isPlaying();
                    stop(false);
                    openCurrentAndNext();
                    if (wasPlaying) {
                        play();
                    }
                }
                notifyChange(META_CHANGED);
            }
            return last - first + 1;
        }
    }

    /**
     * Removes all instances of the track with the given id
     * from the playlist.
     * @param id The id to be removed
     * @return how many instances of the track were removed
     */
    public int removeTrack(long id) {
        // if the next track is to be removed, clear data source
        // of the next player
        if (mNextPlayPos >= 0) {
            long nextPlayId = mPlayList[mNextPlayPos];
            if (nextPlayId == id) {
                mPlayer.setNextDataSource(null);
            }
        }
        int numremoved = 0;
        synchronized (this) {
            for (int i = 0; i < mPlayListLen; i++) {
                if (mPlayList[i] == id) {
                    numremoved += removeTracksInternal(i, i);
                    i--;
                }
            }
        }
        if (numremoved > 0) {
            notifyChange(QUEUE_CHANGED);
        }
        return numremoved;
    }

    public void setShuffleMode(int shufflemode) {
        synchronized(this) {
            if (mShuffleMode == shufflemode && mPlayListLen > 0) {
            /**
             * Some carkits send Shuffle Values same as our current
             * values. In such cases we need to respond back to ck
             */
                notifyAttributeValues(PLAYERSETTINGS_RESPONSE,
                            mAttributePairs, SET_ATTRIBUTE_VALUES);
                return;
            }
            mShuffleMode = shufflemode;
            if (mShuffleMode == SHUFFLE_AUTO) {
                if (makeAutoShuffleList()) {
                    mPlayListLen = 0;
                    doAutoShuffleUpdate();
                    mPlayPos = 0;
                    openCurrentAndNext();
                    play();
                    notifyChange(META_CHANGED);
                    notifyAttributeValues(PLAYERSETTINGS_RESPONSE,
                            mAttributePairs, SET_ATTRIBUTE_VALUES);
                    return;
                } else {
                    // failed to build a list of files to shuffle
                    mShuffleMode = SHUFFLE_NONE;
                }
            }
            notifyAttributeValues(PLAYERSETTINGS_RESPONSE,
                            mAttributePairs, SET_ATTRIBUTE_VALUES);
            notifyChange(SHUFFLE_CHANGED);
            saveQueue(false);
        }
    }
    public int getShuffleMode() {
        return mShuffleMode;
    }

    public void setRepeatMode(int repeatmode) {
        synchronized(this) {
            /* mPlayList not initialized */
            if (mPlayList == null) {
                Log.e(LOGTAG, "mPlayList not initialized");
                return;
            }
            mRepeatMode = repeatmode;
            setNextTrack();
            notifyAttributeValues(PLAYERSETTINGS_RESPONSE,
                            mAttributePairs, SET_ATTRIBUTE_VALUES);
            notifyChange(REPEAT_CHANGED);
            saveQueue(false);
        }
    }
    public int getRepeatMode() {
        return mRepeatMode;
    }

    public int getMediaMountedCount() {
        return mMediaMountedCount;
    }

    /**
     * Returns the path of the currently playing file, or null if
     * no file is currently playing.
     */
    public String getPath() {
        return mFileToPlay;
    }

    /**
     * Returns the rowid of the currently playing file, or -1 if
     * no file is currently playing.
     */
    public long getAudioId() {
        synchronized (this) {
            if (mPlayPos >= 0 && mPlayer!=null && mPlayer.isInitialized()) {
                return mPlayList[mPlayPos];
            }
        }
        return -1;
    }

    /**
     * Returns the position in the queue
     * @return the position in the queue
     */
    public int getQueuePosition() {
        synchronized(this) {
            return mPlayPos;
        }
    }

    /**
     * Starts playing the track at the given position in the queue.
     * @param pos The position in the queue of the track that will be played.
     */
    public void setQueuePosition(int pos) {
        synchronized(this) {
            stop(false);
            mPlayPos = pos;
            openCurrentAndNext();
            play();
            notifyChange(META_CHANGED);
            if (mShuffleMode == SHUFFLE_AUTO) {
                doAutoShuffleUpdate();
            }
        }
    }

    public String getArtistName() {
        synchronized(this) {
            if (mCurrentTrackInfo == null) {
                return null;
            }
            return mCurrentTrackInfo.mArtistName;
        }
    }

    public long getArtistId() {
        synchronized (this) {
            if (mCurrentTrackInfo == null) {
                return -1;
            }
            return mCurrentTrackInfo.mArtistId;
        }
    }

    public String getAlbumName() {
        synchronized (this) {
            if (mCurrentTrackInfo == null) {
                return null;
            }
            return mCurrentTrackInfo.mAlbumName;
        }
    }

    public long getAlbumId() {
        synchronized (this) {
            if (mCurrentTrackInfo == null) {
                return -1;
            }
            return mCurrentTrackInfo.mAlbumId;
        }
    }

    public String getTrackName() {
        synchronized (this) {
            if (mCurrentTrackInfo == null) {
                return null;
            }
            return mCurrentTrackInfo.mTrackName;
        }
    }

    public String getData() {
        synchronized (this) {
            if (mCurrentTrackInfo == null) {
                return null;
            }
            return mCurrentTrackInfo.mData;
        }
    }

    private boolean isPodcast() {
        synchronized (this) {
            if (mCurrentTrackInfo == null) {
                return false;
            }
            return (mCurrentTrackInfo.mPodcast > 0);
        }
    }

    private long getBookmark() {
        synchronized (this) {
            if (mCurrentTrackInfo == null) {
                return 0;
            }
            return mCurrentTrackInfo.mBookmark;
        }
    }

    /**
     * Returns the duration of the file in milliseconds.
     * Currently this method returns -1 for the duration of MIDI files.
     */
    public long duration() {
        if (mPlayer.isInitialized()) {
            return mPlayer.duration();
        }
        return -1;
    }

    public boolean isComplete() {
        if (mPlayer.isInitialized()) {
            return mPlayer.isComplete();
        }
        return false;
    }

    /**
     * Returns the current playback position in milliseconds
     */
    public long position() {
        if (mPlayer != null && mPlayer.isInitialized()) {
            return mPlayer.position();
        }
        return -1;
    }

    /**
     * Seeks to the position specified.
     *
     * @param pos The position to seek to, in milliseconds
     */
    public long seek(long pos) {
        if (mPlayer != null && mPlayer.isInitialized()) {
            if (pos < 0) pos = 0;
            if (pos > mPlayer.duration()) pos = mPlayer.duration();

            mRemoteControlClient.setPlaybackState((isPlaying() ?
                    RemoteControlClient.PLAYSTATE_PLAYING : RemoteControlClient.PLAYSTATE_PAUSED),
                    pos, PLAYBACK_SPEED_1X);
            return mPlayer.seek(pos);
        }
        return -1;
    }

    /**
     * Sets the audio session ID.
     *
     * @param sessionId: the audio session ID.
     */
    public void setAudioSessionId(int sessionId) {
        synchronized (this) {
            mPlayer.setAudioSessionId(sessionId);
        }
    }

    /**
     * Returns the audio session ID.
     */
    public int getAudioSessionId() {
        synchronized (this) {
            return mPlayer.getAudioSessionId();
        }
    }

    /**
     * Returns the player supported attribute IDs.
     */
    private void notifyAttributeIDs(String what) {
        Intent i = new Intent(what);
        i.putExtra(EXTRA_GET_RESPONSE, GET_ATTRIBUTE_IDS);
        i.putExtra(EXTRA_ATTIBUTE_ID_ARRAY, supportedAttributes);
        Log.e(LOGTAG, "notifying attributes");
        sendBroadcast(i);
    }

    /**
     * Returns the player supported value IDs for given attrib.
     */
    private void notifyValueIDs(String what, byte attribute) {
        Intent intent = new Intent(what);
        intent.putExtra(EXTRA_GET_RESPONSE, GET_VALUE_IDS);
        intent.putExtra(EXTRA_ATTRIBUTE_ID, attribute);
        switch (attribute) {
            case ATTRIBUTE_REPEATMODE:
                intent.putExtra(EXTRA_VALUE_ID_ARRAY, supportedRepeatValues);
            break;
            case ATTRIBUTE_SHUFFLEMODE:
                intent.putExtra(EXTRA_VALUE_ID_ARRAY, supportedShuffleValues);
            break;
            default:
                Log.e(LOGTAG,"unsupported attribute"+attribute);
                intent.putExtra(EXTRA_VALUE_ID_ARRAY, unsupportedList);
            break;
        }
        sendBroadcast(intent);
    }

    /**
     * Returns the player supported attrib text for given IDs.
     */
    private void notifyAttributesText(String what, byte [] attrIds) {
        String [] AttribStrings = new String [attrIds.length];
        Intent intent = new Intent(what);
        intent.putExtra(EXTRA_GET_RESPONSE, GET_ATTRIBUTE_TEXT);
        for (int i = 0; i < attrIds.length; i++) {
            if (attrIds[i] >= AttrStr.length) {
                Log.e(LOGTAG, "attrib id is"+attrIds[i]+"which is not supported");
                AttribStrings[i] = "";
            } else {
                AttribStrings[i] = AttrStr[attrIds[i]];
            }
        }
        intent.putExtra(EXTRA_ATTRIBUTE_STRING_ARRAY, AttribStrings);
        sendBroadcast(intent);
    }

    /**
     * Returns the player supported value text for given IDs.
     */
    private void notifyAttributeValuesText(String what, int attribute,
                                           byte [] valIds) {
        Intent intent = new Intent(what);
        String [] ValueStrings = new String [valIds.length];
        intent.putExtra(EXTRA_GET_RESPONSE,GET_VALUE_TEXT);
        intent.putExtra(EXTRA_ATTRIBUTE_ID, attribute);
        Log.e(LOGTAG, "attrib is "+ attribute);
        String [] valueStrs = null;
        switch (attribute) {
            case ATTRIBUTE_REPEATMODE:
                valueStrs = new String[] {
                                             "",
                                             getString(R.string.repeat_off_notif),
                                             getString(R.string.repeat_current_notif),
                                             getString(R.string.repeat_all_notif),
                                          };
            break;
            case ATTRIBUTE_SHUFFLEMODE:
                valueStrs = new String[] {
                                           "",
                                           getString(R.string.shuffle_off_notif),
                                           getString(R.string.shuffle_on_notif),
                                          };
            break;
        }
        for (int i = 0; i < valIds.length; i++) {
            if ((valueStrs == null) ||
                (valIds[i] >= valueStrs.length)) {
                Log.e(LOGTAG, "value id is" + valIds[i] + "which is not supported");
                ValueStrings[i] = "";
            } else {
                ValueStrings[i] = valueStrs[valIds[i]];
            }
        }
        intent.putExtra(EXTRA_VALUE_STRING_ARRAY, ValueStrings);
        sendBroadcast(intent);
    }

    /**
     * Returns the player current values for given attrib IDs.
     */
    private void notifyAttributeValues(String what, HashMap<Byte, Boolean> attrIds, int extra) {
        Intent intent = new Intent(what);
        intent.putExtra(EXTRA_GET_RESPONSE, extra);
        int j = 0;
        byte [] retValarray = new byte [attrIds.size()*2];
        for (int i = 0; i < attrIds.size()*2; i++) {
            retValarray[i] = 0x0;
        }

        for (Byte attribute : attrIds.keySet()) {
            if(attrIds.get(attribute)) {
                retValarray[j] = attribute;
                if (attribute == ATTRIBUTE_REPEATMODE) {
                    retValarray[j+1] = getMappingRepeatVal(mRepeatMode);
                } else if (attribute == ATTRIBUTE_SHUFFLEMODE) {
                    retValarray[j+1] = getMappingShuffleVal(mShuffleMode);
                }
                j += 2;
            } else {
                retValarray[j] = attribute;
                retValarray[j+1] = ERROR_NOTSUPPORTED;
                j += 2;
            }
        }
        intent.putExtra(EXTRA_ATTRIB_VALUE_PAIRS, retValarray);
        sendBroadcast(intent);
    }

    /**
     * Sets the values to current player for given attrib IDs.
     */
    private void setValidAttributes(byte [] attribValuePairs) {
        byte attrib, value;

        for (int i = 0; i < (attribValuePairs.length-1); i += 2) {
           attrib = attribValuePairs[i];
           value = attribValuePairs[i+1];
           switch(attrib) {
                case ATTRIBUTE_REPEATMODE:
                    if (isValidRepeatMode(value)) {
                        setRepeatMode(getMappingRepeatMode(value));
                    }
                break;
                case ATTRIBUTE_SHUFFLEMODE:
                    if (isValidShuffleMode(value)) {
                        setShuffleMode(getMappingShuffleMode(value));
                    }
                break;
                default:
                   Log.e(LOGTAG,"Unknown attribute"+attrib);
                   notifyAttributeValues(PLAYERSETTINGS_RESPONSE,
                            mAttributePairs, SET_ATTRIBUTE_VALUES);
                break;
           }
        }
    }

    byte getMappingRepeatVal (int repeatMode) {
        switch (repeatMode) {
            case REPEAT_NONE:
                return VALUE_REPEATMODE_OFF;
            case REPEAT_CURRENT:
                return VALUE_REPEATMODE_SINGLE;
            case REPEAT_ALL:
                return VALUE_REPEATMODE_ALL;
            default:
                return VALUE_REPEATMODE_OFF;
        }
    }

    byte getMappingShuffleVal (int shuffleMode) {
        switch (shuffleMode) {
            case SHUFFLE_NONE:
                return VALUE_SHUFFLEMODE_OFF;
            case SHUFFLE_NORMAL:
                /*
                 * Repeat_current mode cannot support shuttle mode,
                 * so need sync its setting value to shuttle off.
                */
                if (getRepeatMode() == REPEAT_CURRENT) {
                   return VALUE_SHUFFLEMODE_OFF;
                }
                return VALUE_SHUFFLEMODE_ALL;
            case SHUFFLE_AUTO:
                /*
                 * Repeat_current mode cannot support shuttle mode,
                 * so need sync its setting value to shuttle off.
                */
                if (getRepeatMode() == REPEAT_CURRENT) {
                   return VALUE_SHUFFLEMODE_OFF;
                }
                return VALUE_SHUFFLEMODE_ALL;
            default:
                return VALUE_SHUFFLEMODE_OFF;
        }
    }

    int getMappingRepeatMode (byte repeatVal) {
        switch (repeatVal) {
            case VALUE_REPEATMODE_OFF:
                return REPEAT_NONE;
            case VALUE_REPEATMODE_SINGLE:
                return REPEAT_CURRENT;
            case VALUE_REPEATMODE_ALL:
            case VALUE_REPEATMODE_GROUP:
                return REPEAT_ALL;
            default:
                return REPEAT_NONE;
        }
    }

    int getMappingShuffleMode (byte shuffleVal) {
        switch (shuffleVal) {
            case VALUE_SHUFFLEMODE_OFF:
                return SHUFFLE_NONE;
            case VALUE_SHUFFLEMODE_ALL:
            case VALUE_SHUFFLEMODE_GROUP:
                return SHUFFLE_NORMAL;
            default:
                return SHUFFLE_NONE;
        }
    }

    /**
     * Validates the value with CMDSET for Repeat mode.
     */
    private boolean isValidRepeatMode(byte value) {
        if (value == 0) {
            return false;
        }
        value--;
        if ((value >= REPEAT_NONE) && ( value <= REPEAT_ALL)) {
            return true;
        }
        return false;
    }

    /**
     * Validates the value with CMDSET for Shuffle mode.
     */
    private boolean isValidShuffleMode(byte value) {
        if (value == 0) {
            return false;
        }
        value--;
        // check the mapping for local suffle and argument
        if ((value >= SHUFFLE_NONE) && ( value <= SHUFFLE_AUTO)) {
            return true;
        }
        return false;
    }

    /**
     * Provides a unified interface for dealing with midi files and
     * other media files.
     */
    private class MultiPlayer {
        private CompatMediaPlayer mCurrentMediaPlayer = new CompatMediaPlayer();
        private CompatMediaPlayer mNextMediaPlayer;
        private Handler mHandler;
        private HandlerThread mPrepareNextHandlerThread;
        private Handler mSetNextMediaPlayerHandler = null;
        private Runnable mSetNextMediaPlayerRunnable = null;
        private boolean mIsInitialized = false;
        private boolean mIsComplete = false;

        public MultiPlayer() {
            mCurrentMediaPlayer.setWakeMode(MediaPlaybackService.this, PowerManager.PARTIAL_WAKE_LOCK);
            mPrepareNextHandlerThread = new HandlerThread("parpareNext");
            mPrepareNextHandlerThread.start();
        }

        public void setDataSource(String path) {
            mIsInitialized = setDataSourceImpl(mCurrentMediaPlayer, path, false);
            if (mIsInitialized) {
                setNextDataSource(null);
            }
        }

        private MediaPlayer.OnPreparedListener mNextPreparedListener =
                new MediaPlayer.OnPreparedListener(){
            @Override
            public void onPrepared(MediaPlayer mp){
                Log.d(LOGTAG, "next MediaPlayer Prepared");

                if (mp != null && mp == mCurrentMediaPlayer) {
                    Log.d(LOGTAG, "Ignore to set next MediaPlayer as self");
                    return;
                }

                if (mCurrentMediaPlayer != mp
                        && mIsSupposedToBePlaying
                        && mCurrentMediaPlayer != null
                        && mIsInitialized
                        && mCurrentMediaPlayer.isPlaying()) {
                    Log.d(LOGTAG, "setNextMediaPlayer");
                    mCurrentMediaPlayer.setNextMediaPlayer(mp);
                    if (mNextMediaPlayer != null) {
                        mNextMediaPlayer.release();
                    }
                    mNextMediaPlayer = (CompatMediaPlayer)mp;
                } else {
                    mp.release();
                }
            }
        };

        private boolean setDataSourceImpl(MediaPlayer player, String path, boolean isNext) {
            try {
                player.reset();
                if (isNext) {
                    player.setOnPreparedListener(mNextPreparedListener);
                }
                if (path.startsWith("content://")) {
                    player.setDataSource(MediaPlaybackService.this, Uri.parse(path));
                } else {
                    player.setDataSource(path);
                }
                player.setAudioStreamType(AudioManager.STREAM_MUSIC);
                player.prepare();
            } catch (IOException ex) {
                if (!mQuietMode && (player == mCurrentMediaPlayer)) {
                    Toast.makeText(MediaPlaybackService.this, R.string.open_failed, Toast.LENGTH_SHORT).show();
                }
                return false;
            } catch (IllegalArgumentException ex) {
                // TODO: notify the user why the file couldn't be opened
                return false;
            } catch (IllegalStateException ex) {
                return false;
            } catch (RuntimeException ex) {
                return false;
            }
            player.setOnCompletionListener(listener);
            player.setOnErrorListener(errorListener);
            int sessionId = getAudioSessionId();
            if (sessionId >= 0) {
                Intent i = new Intent(AudioEffect.ACTION_OPEN_AUDIO_EFFECT_CONTROL_SESSION);
                i.putExtra(AudioEffect.EXTRA_AUDIO_SESSION, getAudioSessionId());
                i.putExtra(AudioEffect.EXTRA_PACKAGE_NAME, getPackageName());
                sendBroadcast(i);
            }
            return true;
        }

        public void setNextDataSource(final String path) {
            mIsComplete = false;
            if (mIsInitialized == false) {
                return;
            }
            mCurrentMediaPlayer.setNextMediaPlayer(null);
            if (mNextMediaPlayer != null) {
                mNextMediaPlayer.release();
                mNextMediaPlayer = null;
            }
            if (path == null) {
                return;
            }
            final CompatMediaPlayer mp = new CompatMediaPlayer();
            mp.setWakeMode(MediaPlaybackService.this, PowerManager.PARTIAL_WAKE_LOCK);
            mp.setAudioSessionId(getAudioSessionId());
            if (mSetNextMediaPlayerHandler == null) {
                mSetNextMediaPlayerHandler = new Handler(mPrepareNextHandlerThread.getLooper());
            }
            if (mSetNextMediaPlayerRunnable != null) {
               mSetNextMediaPlayerHandler.removeCallbacks(mSetNextMediaPlayerRunnable);
            }
            mSetNextMediaPlayerRunnable = new Runnable() {
                @Override
                public void run() {
                    if (!setDataSourceImpl(mp, path, true)) {
                        // failed to open next, we'll transition the old fashioned way,
                        // which will skip over the faulty file
                        releaseCurrentAndNext(mp);
                    }
                }
            };
            mSetNextMediaPlayerHandler.post(mSetNextMediaPlayerRunnable);
        }

        private void releaseCurrentAndNext(MediaPlayer current) {
            if (current != null) {
                current.release();
            }
            if (mNextMediaPlayer != null) {
                mNextMediaPlayer.release();
                mNextMediaPlayer = null;
            }
        }

        public boolean isInitialized() {
            return mIsInitialized;
        }

        public void start() {
            MusicUtils.debugLog(new Exception("MultiPlayer.start called"));
            mCurrentMediaPlayer.start();
            mIsComplete = false;
        }

        public void stop() {
            mCurrentMediaPlayer.reset();
            mIsInitialized = false;
        }

        /**
         * You CANNOT use this player anymore after calling release()
         */
        public void release() {
            stop();
            if (mNextMediaPlayer != null) {
                mNextMediaPlayer.reset();
                mNextMediaPlayer.release();
            }
            mCurrentMediaPlayer.release();
        }

        public void pause() {
            mCurrentMediaPlayer.pause();
        }

        public void setHandler(Handler handler) {
            mHandler = handler;
        }

        MediaPlayer.OnCompletionListener listener = new MediaPlayer.OnCompletionListener() {
            public void onCompletion(MediaPlayer mp) {
                mIsComplete = true;
                updatePlaybackState(false);
                if (mp == mCurrentMediaPlayer && mNextMediaPlayer != null) {
                    if (mPlayPos == mNextPlayPos) {
                        mCurrentTrackInfo = getTrackInfoFromId(mPlayList[mNextPlayPos]);
                        if (mCurrentTrackInfo == null) {
                           stop();
                           mNextMediaPlayer.release();
                           mNextMediaPlayer = null;
                           mFileToPlay = null;
                           notifyChange(META_CHANGED);
                           return;
                        }
                    }
                    mCurrentMediaPlayer.release();
                    mCurrentMediaPlayer = mNextMediaPlayer;
                    mNextMediaPlayer = null;
                    mHandler.sendEmptyMessage(TRACK_WENT_TO_NEXT);
                } else {
                    // Acquire a temporary wakelock, since when we return from
                    // this callback the MediaPlayer will release its wakelock
                    // and allow the device to go to sleep.
                    // This temporary wakelock is released when the RELEASE_WAKELOCK
                    // message is processed, but just in case, put a timeout on it.
                    mWakeLock.acquire(30000);
                    mHandler.sendEmptyMessageDelayed(TRACK_ENDED, 60);
                    mHandler.sendEmptyMessage(RELEASE_WAKELOCK);
                }
            }
        };

        MediaPlayer.OnErrorListener errorListener = new MediaPlayer.OnErrorListener() {
            public boolean onError(MediaPlayer mp, int what, int extra) {
                switch (what) {
                case MediaPlayer.MEDIA_ERROR_SERVER_DIED:
                    mIsInitialized = false;
                    mCurrentMediaPlayer.release();
                    if (mNextMediaPlayer != null) {
                        mNextMediaPlayer.release();
                        mNextMediaPlayer = null;
                    }
                    // Creating a new MediaPlayer and settings its wakemode does not
                    // require the media service, so it's OK to do this now, while the
                    // service is still being restarted
                    mCurrentMediaPlayer = new CompatMediaPlayer();
                    mCurrentMediaPlayer.setWakeMode(MediaPlaybackService.this, PowerManager.PARTIAL_WAKE_LOCK);
                    mHandler.sendMessageDelayed(mHandler.obtainMessage(SERVER_DIED), 2000);
                    return true;
                default:
                    Log.e("MultiPlayer", "Error: " + what + "," + extra);
                    if (mRepeatMode == REPEAT_CURRENT) {
                        Log.e("MultiPlayer", "Error:Repeat track-sendMessage");
                        mHandler.sendMessageDelayed(mHandler.obtainMessage(ERROR),0);
                        return true;
                    }
                    mIsInitialized = false;
                    mCurrentMediaPlayer.release();
                    if (mNextMediaPlayer != null) {
                        mNextMediaPlayer.release();
                        mNextMediaPlayer = null;
                    }
                    mCurrentMediaPlayer = new CompatMediaPlayer();
                    mCurrentMediaPlayer.setWakeMode(MediaPlaybackService.this, PowerManager.PARTIAL_WAKE_LOCK);
                    mHandler.sendMessageDelayed(mHandler.obtainMessage(ERROR), 2000);
                    return true;
                }
           }
        };

        public boolean isComplete() {
            return mIsComplete;
        }

        public long duration() {
            return mCurrentMediaPlayer.getDuration();
        }

        public long position() {
            return mCurrentMediaPlayer.getCurrentPosition();
        }

        public long seek(long whereto) {
            mCurrentMediaPlayer.seekTo((int) whereto);
            return whereto;
        }

        public void setVolume(float vol) {
            try {
                mCurrentMediaPlayer.setVolume(vol, vol);
            } catch (Exception e) {
                Log.d(LOGTAG, "setVolume failed: " + e);
            }
        }

        public void setAudioSessionId(int sessionId) {
            mCurrentMediaPlayer.setAudioSessionId(sessionId);
        }

        public int getAudioSessionId() {
            try {
                return mCurrentMediaPlayer.getAudioSessionId();
            } catch (Exception e) {
                Log.d(LOGTAG, "getAudioSessionId failed: " + e);
                return -1;
            }
        }
    }

    static class CompatMediaPlayer extends MediaPlayer implements OnCompletionListener {

        private boolean mCompatMode = true;
        private MediaPlayer mNextPlayer;
        private OnCompletionListener mCompletion;

        public CompatMediaPlayer() {
            mCompatMode = false;
        }

        public void setNextMediaPlayer(MediaPlayer next) {
            if (mCompatMode) {
                mNextPlayer = next;
            } else {
                super.setNextMediaPlayer(next);
            }
        }

        @Override
        public void setOnCompletionListener(OnCompletionListener listener) {
            if (mCompatMode) {
                mCompletion = listener;
            } else {
                super.setOnCompletionListener(listener);
            }
        }

        @Override
        public void onCompletion(MediaPlayer mp) {
            if (mNextPlayer != null) {
                // as it turns out, starting a new MediaPlayer on the completion
                // of a previous player ends up slightly overlapping the two
                // playbacks, so slightly delaying the start of the next player
                // gives a better user experience
                SystemClock.sleep(50);
                mNextPlayer.start();
            }
            mCompletion.onCompletion(this);
        }
    }

    /*
     * By making this a static class with a WeakReference to the Service, we
     * ensure that the Service can be GCd even when the system process still
     * has a remote reference to the stub.
     */
    static class ServiceStub extends IMediaPlaybackService.Stub {
        //changing weak ref to softref to prevent media playercrash
        SoftReference<MediaPlaybackService> mService;

        ServiceStub(MediaPlaybackService service) {
            mService = new SoftReference<MediaPlaybackService>(service);
        }

        public void openFile(String path)
        {
            mService.get().open(path);
        }
        public void open(long [] list, int position) {
            mService.get().open(list, position);
        }
        public int getQueuePosition() {
            return mService.get().getQueuePosition();
        }
        public void setQueuePosition(int index) {
            mService.get().setQueuePosition(index);
        }
        public boolean isPlaying() {
            return mService.get().isPlaying();
        }
        public void stop() {
            mService.get().stop();
        }
        public void pause() {
            mService.get().pause();
        }
        public void play() {
            mService.get().play();
        }
        public void prev() {
            mService.get().prev();
        }
        public void next() {
            mService.get().gotoNext(true);
        }
        public int getHistSize() {
            return mService.get().getHistSize();
        }
        public boolean isComplete() {
            return mService.get().isComplete();
        }
        public String getTrackName() {
            return mService.get().getTrackName();
        }
        public String getAlbumName() {
            return mService.get().getAlbumName();
        }
        public long getAlbumId() {
            return mService.get().getAlbumId();
        }
        public String getArtistName() {
            return mService.get().getArtistName();
        }
        public long getArtistId() {
            return mService.get().getArtistId();
        }
        public String getData() {
            return mService.get().getData();
        }
        public void enqueue(long [] list , int action) {
            mService.get().enqueue(list, action);
        }
        public long [] getQueue() {
            return mService.get().getQueue();
        }
        public void moveQueueItem(int from, int to) {
            mService.get().moveQueueItem(from, to);
        }
        public String getPath() {
            return mService.get().getPath();
        }
        public long getAudioId() {
            return mService.get().getAudioId();
        }
        public long position() {
            return mService.get().position();
        }
        public long duration() {
            return mService.get().duration();
        }
        public long seek(long pos) {
            return mService.get().seek(pos);
        }
        public void setShuffleMode(int shufflemode) {
            mService.get().setShuffleMode(shufflemode);
        }
        public int getShuffleMode() {
            return mService.get().getShuffleMode();
        }
        public int removeTracks(int first, int last) {
            return mService.get().removeTracks(first, last);
        }
        public int removeTrack(long id) {
            return mService.get().removeTrack(id);
        }
        public void setRepeatMode(int repeatmode) {
            mService.get().setRepeatMode(repeatmode);
        }
        public int getRepeatMode() {
            return mService.get().getRepeatMode();
        }
        public int getMediaMountedCount() {
            return mService.get().getMediaMountedCount();
        }
        public int getAudioSessionId() {
            return mService.get().getAudioSessionId();
        }
    }

    @Override
    protected void dump(FileDescriptor fd, PrintWriter writer, String[] args) {
        writer.println("" + mPlayListLen + " items in queue, currently at index " + mPlayPos);
        writer.println("Currently loaded:");
        writer.println(getArtistName());
        writer.println(getAlbumName());
        writer.println(getTrackName());
        writer.println(getPath());
        writer.println("playing: " + mIsSupposedToBePlaying);
        writer.println("actual: " + mPlayer.mCurrentMediaPlayer.isPlaying());
        writer.println("shuffle mode: " + mShuffleMode);
        MusicUtils.debugDump(writer);
    }

    private final IBinder mBinder = new ServiceStub(this);

    private void updatePlaybackState(boolean pause) {
        long pos = (mPlayer != null) ? position() : 0;
        if (pos < 0) pos = 0;

        int state = RemoteControlClient.PLAYSTATE_PAUSED;
        if (pause) {
            state = RemoteControlClient.PLAYSTATE_PAUSED;
        } else {
            state = isPlaying() ?
                    RemoteControlClient.PLAYSTATE_PLAYING :
                    RemoteControlClient.PLAYSTATE_PAUSED;
        }
        mRemoteControlClient.setPlaybackState(state, pos, PLAYBACK_SPEED_1X);
    }

    private Handler mMediaButtonHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case MSG_LONG_PRESS_TIMEOUT:
                    if (!mLaunched) {
                        Context context = (Context)msg.obj;
                        Intent i = new Intent();
                        i.putExtra("autoshuffle", "true");
                        i.setClass(context, MusicBrowserActivity.class);
                        i.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TOP);
                        context.startActivity(i);
                        mLaunched = true;
                    }
                    break;
            }
        }
    };

    private class MediaSessionCallback extends MediaSessionCompat.Callback {
        private long mLastClickTime = 0;
        private boolean mDown = false;
        private PowerManager.WakeLock mWakeLock = null;

        @Override
        public boolean onMediaButtonEvent(Intent mediaButtonEvent) {

            Context context = MediaPlaybackService.this;
            if (mWakeLock == null) {
                PowerManager pm = (PowerManager) context.getSystemService(
                        Context.POWER_SERVICE);
                mWakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK,
                        this.getClass().getName());
                mWakeLock.setReferenceCounted(false);
            }

            // hold wakelock for 3s as to ensure button press event full processed.
            mWakeLock.acquire(3000);

            KeyEvent event = (KeyEvent)
                    mediaButtonEvent.getParcelableExtra(Intent.EXTRA_KEY_EVENT);
            if (event == null) {
                return super.onMediaButtonEvent(mediaButtonEvent);
            }

            int keycode = event.getKeyCode();
            int action = event.getAction();
            long eventTime = event.getEventTime();

            // single quick press: pause/resume.
            // double press: next track
            // long press: start auto-shuffle mode.
            String command = null;
            switch (keycode) {
                case KeyEvent.KEYCODE_MEDIA_STOP:
                    command = MediaPlaybackService.CMDSTOP;
                    break;
                case KeyEvent.KEYCODE_HEADSETHOOK:
                case KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE:
                    command = MediaPlaybackService.CMDTOGGLEPAUSE;
                    break;
                case KeyEvent.KEYCODE_MEDIA_NEXT:
                    command = MediaPlaybackService.CMDNEXT;
                    break;
                case KeyEvent.KEYCODE_MEDIA_PREVIOUS:
                    command = MediaPlaybackService.CMDPREVIOUS;
                    break;
                case KeyEvent.KEYCODE_MEDIA_PAUSE:
                    command = MediaPlaybackService.CMDPAUSE;
                    break;
                case KeyEvent.KEYCODE_MEDIA_PLAY:
                    command = MediaPlaybackService.CMDPLAY;
                    break;
            }

            if (command != null) {
                if (action == KeyEvent.ACTION_DOWN) {
                    if (mDown) {
                        if ((MediaPlaybackService.CMDTOGGLEPAUSE.equals(command) ||
                                MediaPlaybackService.CMDPLAY.equals(command))
                                && mLastClickTime != 0
                                && eventTime - mLastClickTime > LONG_PRESS_DELAY) {
                            mMediaButtonHandler.sendMessage(
                                    mMediaButtonHandler.obtainMessage(MSG_LONG_PRESS_TIMEOUT,
                                            context));
                        }
                    } else if (event.getRepeatCount() == 0) {
                        // only consider the first event in a sequence, not the repeat events,
                        // so that we don't trigger in cases where the first event went to
                        // a different app (e.g. when the user ends a phone call by
                        // long pressing the headset button)

                        // The service may or may not be running, but we need to send it
                        // a command.
                        Intent i = new Intent(context, MediaPlaybackService.class);
                        i.setAction(MediaPlaybackService.SERVICECMD);
                        if (keycode == KeyEvent.KEYCODE_HEADSETHOOK &&
                                eventTime - mLastClickTime < 300) {
                            i.putExtra(MediaPlaybackService.CMDNAME, MediaPlaybackService.CMDNEXT);
                            MusicUtils.startService(context, i);
                            mLastClickTime = 0;
                        } else {
                            i.putExtra(MediaPlaybackService.CMDNAME, command);
                            MusicUtils.startService(context, i);
                            mLastClickTime = eventTime;
                        }

                        mLaunched = false;
                        mDown = true;
                    }
                } else {
                    mMediaButtonHandler.removeMessages(MSG_LONG_PRESS_TIMEOUT);
                    mDown = false;
                }
            }

            return super.onMediaButtonEvent(mediaButtonEvent);
        }
    }
}
