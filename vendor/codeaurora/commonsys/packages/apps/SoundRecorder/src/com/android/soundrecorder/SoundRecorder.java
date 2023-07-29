/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Not a Contribution.
 *
 * Copyright (C) 2011 The Android Open Source Project
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

package com.android.soundrecorder;

import java.io.File;
import java.util.Hashtable;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.Intent;
import android.content.Context;
import android.content.DialogInterface;
import android.content.IntentFilter;
import android.content.BroadcastReceiver;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.media.AudioManager;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Message;
import android.os.PowerManager;
import android.os.PowerManager.WakeLock;
import android.provider.MediaStore;
import android.util.Log;
import android.view.ContextThemeWrapper;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.InputMethodManager;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.TextView;
import android.widget.Toast;
import android.telephony.PhoneStateListener;
import android.telephony.TelephonyManager;
import android.telephony.SubscriptionManager;

import com.android.soundrecorder.util.DatabaseUtils;
import com.android.soundrecorder.filelist.FileListActivity;
import com.android.soundrecorder.util.FileUtils;
import com.android.soundrecorder.util.PermissionUtils;
import com.android.soundrecorder.util.StorageUtils;
import com.android.soundrecorder.util.Utils;
import org.codeaurora.wrapper.soundrecorder.util.SystemPropertiesWrapper;
import org.codeaurora.wrapper.soundrecorder.util.MediaRecorderWrapper;


/**
 * Calculates remaining recording time based on available disk space and
 * optionally a maximum recording file size.
 *
 * The reason why this is not trivial is that the file grows in blocks
 * every few seconds or so, while we want a smooth countdown.
 */

class RemainingTimeCalculator {
    public static final int UNKNOWN_LIMIT = 0;
    public static final int FILE_SIZE_LIMIT = 1;
    public static final int DISK_SPACE_LIMIT = 2;

    // which of the two limits we will hit (or have fit) first
    private int mCurrentLowerLimit = UNKNOWN_LIMIT;

     // State for tracking file size of recording.
    private File mRecordingFile;
    private long mMaxBytes;

    // Rate at which the file grows
    private int mBytesPerSecond;
    private int mPath = StorageUtils.STORAGE_PATH_PHONE_INDEX;

    // time at which number of free blocks last changed
    private long mBlocksChangedTime;
    // number of available blocks at that time
    private long mLastBlockSize;

    // time at which the size of the file has last changed
    private long mFileSizeChangedTime;
    // size of the file at that time
    private long mLastFileSize;
    private Context mContext;

    public RemainingTimeCalculator(Context context) {
        mContext = context;
    }

    /**
     * If called, the calculator will return the minimum of two estimates:
     * how long until we run out of disk space and how long until the file
     * reaches the specified size.
     *
     * @param file the file to watch
     * @param maxBytes the limit
     */

    public void setFileSizeLimit(File file, long maxBytes) {
        mRecordingFile = file;
        mMaxBytes = maxBytes;
    }

    /**
     * Resets the interpolation.
     */
    public void reset() {
        mCurrentLowerLimit = UNKNOWN_LIMIT;
        mBlocksChangedTime = -1;
        mFileSizeChangedTime = -1;
    }

    /**
     * Returns how long (in seconds) we can continue recording.
     */
    public long timeRemaining() {
        // Calculate how long we can record based on free disk space

        long blockSize = StorageUtils.getAvailableBlockSize(mContext, mPath);
        long now = System.currentTimeMillis();

        if (mBlocksChangedTime == -1 || blockSize != mLastBlockSize) {
            mBlocksChangedTime = now;
            mLastBlockSize = blockSize;
        }

        /* The calculation below always leaves one free block, since free space
           in the block we're currently writing to is not added. This
           last block might get nibbled when we close and flush the file, but
           we won't run out of disk. */

        // at mBlocksChangedTime we had this much time
        if (mBytesPerSecond == 0) {
            mBytesPerSecond = 1;
        }
        long result = mLastBlockSize / mBytesPerSecond;
        // so now we have this much time
        result -= (now - mBlocksChangedTime)/1000;

        if (mRecordingFile == null) {
            mCurrentLowerLimit = DISK_SPACE_LIMIT;
            return result;
        }

        // If we have a recording file set, we calculate a second estimate
        // based on how long it will take us to reach mMaxBytes.

        mRecordingFile = new File(mRecordingFile.getAbsolutePath());
        long fileSize = mRecordingFile.length();
        if (mFileSizeChangedTime == -1 || fileSize != mLastFileSize) {
            mFileSizeChangedTime = now;
            mLastFileSize = fileSize;
        }

        long result2 = (mMaxBytes - fileSize)/mBytesPerSecond;
        result2 -= (now - mFileSizeChangedTime)/1000;
        result2 -= 1; // just for safety

        mCurrentLowerLimit = result < result2
            ? DISK_SPACE_LIMIT : FILE_SIZE_LIMIT;

        return Math.min(result, result2);
    }

    /**
     * Indicates which limit we will hit (or have hit) first, by returning one
     * of FILE_SIZE_LIMIT or DISK_SPACE_LIMIT or UNKNOWN_LIMIT. We need this to
     * display the correct message to the user when we hit one of the limits.
     */
    public int currentLowerLimit() {
        return mCurrentLowerLimit;
    }

    /**
     * Sets the bit rate used in the interpolation.
     *
     * @param bitRate the bit rate to set in bits/sec.
     */
    public void setBitRate(int bitRate) {
        mBytesPerSecond = bitRate/8;
    }

    public void setStoragePath(int path) {
        mPath = path;
    }
}

public class SoundRecorder extends Activity
        implements Button.OnClickListener, Recorder.OnStateChangedListener {
    static final String TAG = "SoundRecorder";
    static final String STATE_FILE_NAME = "soundrecorder.state";
    static final String RECORDER_STATE_KEY = "recorder_state";
    static final String SAMPLE_INTERRUPTED_KEY = "sample_interrupted";
    static final String MAX_FILE_SIZE_KEY = "max_file_size";
    private static final String EXIT_AFTER_RECORD = "exit_after_record";

    static final String AUDIO_3GPP = "audio/3gpp";
    static final String AUDIO_AMR = "audio/amr";
    static final String AUDIO_EVRC = "audio/evrc";
    static final String AUDIO_QCELP = "audio/qcelp";
    static final String AUDIO_AAC_MP4 = "audio/aac_mp4";
    static final String AUDIO_WAVE_6CH_LPCM = "audio/wave_6ch_lpcm";
    static final String AUDIO_WAVE_2CH_LPCM = "audio/wave_2ch_lpcm";
    static final String AUDIO_AAC_5POINT1_CHANNEL = "audio/aac_5point1_channel";
    static final String AUDIO_AMR_WB = "audio/amr-wb";
    static final String AUDIO_MPEGH = "audio/mhas";
    static final String AUDIO_ANY = "audio/*";
    static final String ANY_ANY = "*/*";

    static final int FOCUSCHANGE = 0;

    static final String VENDOR_SOUNDRECORDER_DEBUG_ENABLE = "vendor.soundrecorder.debug.enable";

    static final int SETTING_TYPE_STORAGE_LOCATION = 0;
    static final int SETTING_TYPE_FILE_TYPE = 1;

    static final int BITRATE_AMR = 12800; // bits/sec
    static final int BITRATE_AAC = 156000;
    static final int BITRATE_EVRC = 8500;
    static final int BITRATE_LPCM = 4608000;
    static final int BITRATE_QCELP = 13300;
    static final int BITRATE_3GPP = 12800;
    static final int SAMPLERATE_MULTI_CH = 48000;
    static final int BITRATE_AMR_WB = 23850;
    static final int SAMPLERATE_AMR_WB = 16000;
    static final int BITRATE_MPEGH = 307200;
    static final int SAMPLERATE_MPEGH = 48000;
    static final int SAMPLERATE_8000 = 8000;
    static final long STOP_WAIT = 300;
    static final long BACK_KEY_WAIT = 400;
    int mAudioOutputFormat = MediaRecorderWrapper.OutputFormat.AMR_WB;
    String mAmrWidebandExtension = ".awb";
    private AudioManager mAudioManager;
    private boolean mRecorderStop = false;
    private boolean mRecorderProcessed = false;
    private boolean mDataExist = false;
    private boolean mWAVSupport = true;
    private boolean mExitAfterRecord = false;
    private boolean mIsGetContentAction = false;
    private boolean mSdExist = true;
    private boolean mRenameDialogShown = false;

    private ProgressDialog mProgressDialog;
    private final int MSG_DISMISS_PROGRESS_DIALOG = 1100;

    int mAudioSourceType = MediaRecorderWrapper.AudioSource.MIC;
    int mPhoneCount = 0;
    private Hashtable<String, Integer> mCallStateMap = new Hashtable<String, Integer>();
    static int mCallState = TelephonyManager.CALL_STATE_IDLE;
    WakeLock mWakeLock;
    String mRequestedType = AUDIO_ANY;
    Recorder mRecorder;
    boolean mSampleInterrupted = false;
    static boolean bSSRSupported;
    String mErrorUiMessage = null; // Some error messages are displayed in the UI,
                                   // not a dialog. This happens when a recording
                                   // is interrupted for some reason.

    long mMaxFileSize = -1;        // can be specified in the intent
    RemainingTimeCalculator mRemainingTimeCalculator = null;

    String mTimerFormat;
    final Handler mHandler = new Handler();
    Runnable mUpdateTimer = new Runnable() {
        public void run() {
            updateTimerView();
        }
    };

    ImageButton mRecordButton;
    ImageButton mStopButton;
    ImageButton mListButton;

    TextView mStateMessage1;
    TextView mStateMessage2;
    TextView mTimerView;

    VUMeter mVUMeter;
    private BroadcastReceiver mSDCardMountEventReceiver = null;
    private BroadcastReceiver mPowerOffReceiver = null;
    private TelephonyManager mTelephonyManager;
    private PhoneStateListener[] mPhoneStateListener;
    private int mFileType = 0;
    private int mPath = StorageUtils.STORAGE_PATH_PHONE_INDEX;
    private String mStoragePath = StorageUtils.getPhoneStoragePath();
    private SharedPreferences mSharedPreferences;
    private Editor mPrefsStoragePathEditor;

    private IntentFilter mMountFilter = new IntentFilter();

    private PhoneStateListener getPhoneStateListener() {
        PhoneStateListener phoneStateListener = new PhoneStateListener() {
            @Override
            public void onCallStateChanged(int state, String incomingNumber) {
               mCallStateMap.put(incomingNumber+' ', state);

               switch (state) {
                      case TelephonyManager.CALL_STATE_IDLE:
                      if ((mCallState == TelephonyManager.CALL_STATE_OFFHOOK)
                               && !(mAudioSourceType == MediaRecorderWrapper.AudioSource.MIC)){
                         mRecorder.stop();
                          // TODO show toast here.
                         mAudioSourceType = MediaRecorderWrapper.AudioSource.MIC;
                      }

                      case TelephonyManager.CALL_STATE_RINGING:
                      case TelephonyManager.CALL_STATE_OFFHOOK:

                      if (mCallStateMap.containsValue(TelephonyManager.CALL_STATE_OFFHOOK)) {
                          mCallState = TelephonyManager.CALL_STATE_OFFHOOK;
                      } else if(mCallStateMap.containsValue(TelephonyManager.CALL_STATE_RINGING)) {
                          mCallState = TelephonyManager.CALL_STATE_RINGING;
                      } else {
                          mCallState = TelephonyManager.CALL_STATE_IDLE;
                      }

                      break;

                      default:
                      // The control should not come here
                      Log.e(TAG,"Unknown call state");
                      break;
                }
            }
        };
        return phoneStateListener;
    }

    private BroadcastReceiver mMountReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (mPath == StorageUtils.STORAGE_PATH_PHONE_INDEX
                    && StorageUtils.isPhoneStorageMounted()) {
                mErrorUiMessage = null;
                updateUi();
            } else if (mPath == StorageUtils.STORAGE_PATH_SD_INDEX
                    && StorageUtils.isSdMounted(SoundRecorder.this)) {
                mErrorUiMessage = null;
                mSdExist = true;
                updateUi();
            } else if (StorageUtils.isSdMounted(SoundRecorder.this) &&
                    !StorageUtils.diskSpaceAvailable(SoundRecorder.this, mPath)) {
                mSampleInterrupted = true;
                mErrorUiMessage = getResources().getString(R.string.storage_is_full);
                updateUi();
            }
        }
    };

    @Override
    public void onCreate(Bundle icycle) {
        super.onCreate(icycle);
        if (getResources().getBoolean(R.bool.config_storage_path)) {
            mStoragePath = StorageUtils.applyCustomStoragePath(this);
        }

        mSharedPreferences = getSharedPreferences("storage_Path", Context.MODE_PRIVATE);
        mPrefsStoragePathEditor = mSharedPreferences.edit();
        StorageUtils.getExternalFilesDirPath(this.getApplicationContext());

        int maxDuration = 0;
        Intent i = getIntent();
        if (i != null) {
            String s = i.getType();
            if (AUDIO_AMR.equals(s) || AUDIO_3GPP.equals(s) || AUDIO_ANY.equals(s)
                    || ANY_ANY.equals(s)) {
                mRequestedType = s;
                mWAVSupport = false;
            } else if (s != null) {
                // we only support amr and 3gpp formats right now
                setResult(RESULT_CANCELED);
                finish();
                return;
            }

            final String EXTRA_MAX_BYTES
                = android.provider.MediaStore.Audio.Media.EXTRA_MAX_BYTES;
            mMaxFileSize = i.getLongExtra(EXTRA_MAX_BYTES, -1);

            mIsGetContentAction = Intent.ACTION_GET_CONTENT.equals(i.getAction());

            mExitAfterRecord = i.getBooleanExtra(EXIT_AFTER_RECORD, mIsGetContentAction);
            maxDuration = i.getIntExtra(MediaStore.Audio.Media.DURATION, 0);
        }

        if (AUDIO_ANY.equals(mRequestedType) || ANY_ANY.equals(mRequestedType)) {
            mRequestedType = AUDIO_AMR;
        }

        mPath = mSharedPreferences.getInt("path", mPath);
        if (!mExitAfterRecord) {
            // Don't reload cached encoding type,if it's assigned by external intent.
            mRequestedType = mSharedPreferences.getString("requestedType",
                    getResources().getString(R.string.def_save_mimetype));
        }
        mFileType = mSharedPreferences.getInt("fileType",
                getResources().getInteger(R.integer.def_save_type));
        mStoragePath = StorageUtils.getPhoneStoragePath();
        if (!mWAVSupport && (AUDIO_WAVE_2CH_LPCM.equals(mRequestedType))) {
            mRequestedType = AUDIO_AMR;
            mFileType = 0;
        }

        setContentView(R.layout.main);
        mAudioManager = (AudioManager)getSystemService(Context.AUDIO_SERVICE);
        mRecorder = new Recorder(this);
        mRecorder.setOnStateChangedListener(this);
        mRecorder.setMaxDuration(maxDuration);

        mRemainingTimeCalculator = new RemainingTimeCalculator(this);

        PowerManager pm
            = (PowerManager) getSystemService(Context.POWER_SERVICE);
        mWakeLock = pm.newWakeLock(PowerManager.SCREEN_DIM_WAKE_LOCK,
                                    "SoundRecorder");

        initResourceRefs();

        mRecorderStop = false;
        mRecorderProcessed = false;
        mDataExist = false;

        setResult(RESULT_CANCELED);
        registerExternalStorageListener();
        registerPowerOffListener();
        if (icycle != null) {
            Bundle recorderState = icycle.getBundle(RECORDER_STATE_KEY);
            if (recorderState != null) {
                mRecorder.restoreState(recorderState);
                mSampleInterrupted = recorderState.getBoolean(SAMPLE_INTERRUPTED_KEY, false);
                mMaxFileSize = recorderState.getLong(MAX_FILE_SIZE_KEY, -1);
            }
        }
        mTelephonyManager = (TelephonyManager) getSystemService(Context.TELEPHONY_SERVICE);
        mPhoneCount = mTelephonyManager.getPhoneCount();
        mPhoneStateListener = new PhoneStateListener[mPhoneCount];
        for(int j = 0; j < mPhoneCount; j++) {
            int[] subId = SubscriptionManager.getSubId(j);

            // adapt case: disabled telephony feature or activate card failure
            if (null != subId && subId.length > 0) {
                mPhoneStateListener[j] = getPhoneStateListener();
            } else {
                mPhoneStateListener[j] = null;
            }
        }

        String ssrRet = SystemPropertiesWrapper.get("ro.vendor.qc.sdk.audio.ssr","false");
        if (ssrRet.contains("true")) {
            Log.d(TAG,"Surround sound recording is supported");
            bSSRSupported = true;
        } else {
            Log.d(TAG,"Surround sound recording is not supported");
            bSSRSupported = false;
        }

        mMountFilter.addAction(Intent.ACTION_MEDIA_MOUNTED);
        mMountFilter.addAction(Intent.ACTION_MEDIA_UNMOUNTED);
        mMountFilter.addDataScheme("file");
        registerReceiver(mMountReceiver, mMountFilter);
        updateUi();
    }

    @Override
    protected void onResume() {
        super.onResume();
        // While we're in the foreground, listen for phone state changes.
        mTelephonyManager = (TelephonyManager) getSystemService(Context.TELEPHONY_SERVICE);
        for(int i = 0; i < mPhoneCount; i++) {
            // adapt case: disabled telephony feature or activate card failure
            int[] subId = SubscriptionManager.getSubId(i);
            if (null != mPhoneStateListener[i] && (null != subId && subId.length > 0)) {
                mTelephonyManager
                    .createForSubscriptionId(subId[0])
                    .listen(mPhoneStateListener[i], PhoneStateListener.LISTEN_CALL_STATE);
            }
        }
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);

        setContentView(R.layout.main);
        initResourceRefs();
        updateUi();
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);

        if (mRecorder.sampleLength() == 0)
            return;

        Bundle recorderState = new Bundle();

        mRecorder.saveState(recorderState);
        recorderState.putBoolean(SAMPLE_INTERRUPTED_KEY, mSampleInterrupted);
        recorderState.putLong(MAX_FILE_SIZE_KEY, mMaxFileSize);

        outState.putBundle(RECORDER_STATE_KEY, recorderState);
    }

    /*
     * Whenever the UI is re-created (due f.ex. to orientation change) we have
     * to reinitialize references to the views.
     */
    private void initResourceRefs() {
        mRecordButton = (ImageButton) findViewById(R.id.recordButton);
        mStopButton = (ImageButton) findViewById(R.id.stopButton);
        mListButton = (ImageButton) findViewById(R.id.listButton);

        mStateMessage1 = (TextView) findViewById(R.id.stateMessage1);
        mStateMessage2 = (TextView) findViewById(R.id.stateMessage2);
        mTimerView = (TextView) findViewById(R.id.timerView);

        mVUMeter = (VUMeter) findViewById(R.id.uvMeter);

        mRecordButton.setOnClickListener(this);
        mStopButton.setOnClickListener(this);
        mListButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                startListActivity();
            }
        });

        mTimerFormat = getResources().getString(R.string.timer_format);

        mVUMeter.setRecorder(mRecorder);
    }

    private String[] getOperationPermissionName(int operation) {
        switch (operation) {
        case R.id.recordButton:
            return PermissionUtils.getOperationPermissions(PermissionUtils.PermissionType.RECORD);
        default:
            return null;
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions,
                int[] grantResults) {
        if (PermissionUtils.checkPermissionResult(permissions, grantResults)) {
            processClickEvent(requestCode);
        }
    }

    /*
     * Handle the buttons.
     */
    public void onClick(View button) {
        if (!button.isEnabled())
            return;
        if (Build.VERSION.SDK_INT >= 23) {
            String[] permissions = getOperationPermissionName(button.getId());
            if (PermissionUtils.checkPermissions(this, permissions, button.getId()))
                processClickEvent(button.getId());
        } else {
            processClickEvent(button.getId());
        }
    }

    private Handler mMsgHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            super.handleMessage(msg);
            switch (msg.what) {
                case MSG_DISMISS_PROGRESS_DIALOG:
                    if (mProgressDialog != null && mProgressDialog.isShowing()) {
                        mProgressDialog.dismiss();
                        mProgressDialog = null;
                    }
                    break;
            }
        }
    };

    private class RecordingParams {
        int mOutputFileFormat;
        String mExtName;
        Context mContext;
        int mAudioSourceType;
        int mCodecType;

        public RecordingParams(int outputFileFormat,
                               String extName,
                               Context context,
                               int audioSourceType,
                               int codecType) {
            mOutputFileFormat = outputFileFormat;
            mExtName = extName;
            mContext = context;
            mAudioSourceType = audioSourceType;
            mCodecType = codecType;
        }
    }

    private class StartRecordingTask extends AsyncTask<RecordingParams, Integer, String> {
        @Override
        protected void onPreExecute() {
            String title = getString(R.string.title_please_wait);
            String message = getString(R.string.msg_starting_recording);
            mProgressDialog = ProgressDialog.show(SoundRecorder.this,
                    message, title);
            super.onPreExecute();
        }

        @Override
        protected String doInBackground(RecordingParams... params) {
            RecordingParams param = params[0];
            mRecorder.startRecording(param.mOutputFileFormat, param.mExtName, param.mContext,
                    param.mAudioSourceType, param.mCodecType);
            return null;
        }

        @Override
        protected void onCancelled(String s) {
            super.onCancelled(s);
            Message msg = Message.obtain();
            msg.what = MSG_DISMISS_PROGRESS_DIALOG;
            mMsgHandler.sendMessageDelayed(msg, 600);
        }

        @Override
        protected void onPostExecute(String s) {
            super.onPostExecute(s);
            Message msg = Message.obtain();
            msg.what = MSG_DISMISS_PROGRESS_DIALOG;
            mMsgHandler.sendMessageDelayed(msg, 600);
        }
    }

    private void processClickEvent(int viewId) {
        switch (viewId) {
        case R.id.recordButton:
            if (mRecorder.state() == Recorder.PAUSE_STATE) {
                mRecorder.resumeRecording();
                updateUi();
                return;
            } else if (mRecorder.state() == Recorder.RECORDING_STATE) {
                mRecorder.pauseRecording();
                updateUi();
                return;
            }
            mRemainingTimeCalculator.reset();
            mRemainingTimeCalculator.setStoragePath(mPath);
            mRecorder.setStoragePath(mStoragePath);
            if (mPath == StorageUtils.STORAGE_PATH_PHONE_INDEX
                    && !StorageUtils.isPhoneStorageMounted()) {
                mSampleInterrupted = true;
                mErrorUiMessage = getResources().getString(R.string.no_phonestorage);
                updateUi();
            } else if (mPath == StorageUtils.STORAGE_PATH_SD_INDEX
                    && !StorageUtils.isSdMounted(SoundRecorder.this)) {
                mSampleInterrupted = true;
                mErrorUiMessage = getResources().getString(R.string.insert_sd_card);
                updateUi();
            } else if (!StorageUtils.diskSpaceAvailable(SoundRecorder.this, mPath)) {
                mSampleInterrupted = true;
                mErrorUiMessage = getResources().getString(R.string.storage_is_full);
                updateUi();
            } else {

                if ((mCallState == TelephonyManager.CALL_STATE_OFFHOOK) &&
                    (mAudioSourceType == MediaRecorderWrapper.AudioSource.MIC)) {
                    mAudioSourceType = MediaRecorderWrapper.AudioSource.VOICE_UPLINK;
                    Log.e(TAG, "Selected Voice Tx only Source: sourcetype" + mAudioSourceType);
                }
                if (AUDIO_AMR.equals(mRequestedType)) {
                    mRemainingTimeCalculator.setBitRate(BITRATE_AMR);
                    mRecorder.setChannels(1);
                    mRecorder.setSamplingRate(SAMPLERATE_8000);
                    new StartRecordingTask().execute(new RecordingParams(
                            MediaRecorderWrapper.OutputFormat.RAW_AMR, ".amr", this,
                            mAudioSourceType, MediaRecorderWrapper.AudioEncoder.AMR_NB));
                } else if (AUDIO_EVRC.equals(mRequestedType)) {
                    mRemainingTimeCalculator.setBitRate(BITRATE_EVRC);
                    mRecorder.setChannels(1);
                    mRecorder.setSamplingRate(SAMPLERATE_8000);
                    new StartRecordingTask().execute(new RecordingParams(
                            MediaRecorderWrapper.OutputFormat.QCP, ".qcp", this,
                            mAudioSourceType, MediaRecorderWrapper.AudioEncoder.EVRC));
                } else if (AUDIO_QCELP.equals(mRequestedType)) {
                    mRemainingTimeCalculator.setBitRate(BITRATE_QCELP);
                    mRecorder.setSamplingRate(SAMPLERATE_8000);
                    mRecorder.setChannels(1);
                    new StartRecordingTask().execute(new RecordingParams(
                            MediaRecorderWrapper.OutputFormat.QCP, ".qcp", this,
                            mAudioSourceType, MediaRecorderWrapper.AudioEncoder.QCELP));
                } else if (AUDIO_3GPP.equals(mRequestedType)) {
                    mRemainingTimeCalculator.setBitRate(BITRATE_3GPP);
                    new StartRecordingTask().execute(new RecordingParams(
                            MediaRecorderWrapper.OutputFormat.THREE_GPP, ".3gpp", this,
                            mAudioSourceType, MediaRecorderWrapper.AudioEncoder.AMR_NB));
                } else if (AUDIO_AAC_MP4.equals(mRequestedType)) {
                    setBitRate(BITRATE_AAC);
                    mRecorder.setSamplingRate(SAMPLERATE_MULTI_CH);
                    mRecorder.setChannels(2);
                    new StartRecordingTask().execute(new RecordingParams(
                            MediaRecorderWrapper.OutputFormat.THREE_GPP, ".aac", this,
                            mAudioSourceType, MediaRecorderWrapper.AudioEncoder.AAC));
                } else if (AUDIO_AAC_5POINT1_CHANNEL.equals(mRequestedType)) {
                    //AAC  2-channel recording
                    if (true == bSSRSupported) {
                        mRemainingTimeCalculator.setBitRate(BITRATE_AAC);
                        mRecorder.setChannels(6);
                        mRecorder.setSamplingRate(SAMPLERATE_MULTI_CH);
                        mAudioSourceType = MediaRecorderWrapper.AudioSource.MIC;
                        new StartRecordingTask().execute(new RecordingParams(
                                MediaRecorderWrapper.OutputFormat.THREE_GPP, ".3gpp", this,
                                mAudioSourceType, MediaRecorderWrapper.AudioEncoder.AAC));
                    } else {
                      throw new IllegalArgumentException("Invalid output file type requested");
                    }
                } else if (AUDIO_WAVE_6CH_LPCM.equals(mRequestedType)) {
                    //WAVE LPCM  6-channel recording
                    if (true == bSSRSupported) {
                        mRemainingTimeCalculator.setBitRate(BITRATE_LPCM);
                        mRecorder.setChannels(6);
                        mRecorder.setSamplingRate(SAMPLERATE_MULTI_CH);
                        mAudioSourceType = MediaRecorderWrapper.AudioSource.MIC;
                        new StartRecordingTask().execute(new RecordingParams(
                                MediaRecorderWrapper.OutputFormat.WAVE, ".wav", this,
                                mAudioSourceType, MediaRecorderWrapper.AudioEncoder.LPCM));
                    } else {
                      throw new IllegalArgumentException("Invalid output file type requested");
                    }
                } else if (AUDIO_WAVE_2CH_LPCM.equals(mRequestedType)) {
                    // WAVE LPCM 2-channel recording
                    mRemainingTimeCalculator.setBitRate(BITRATE_LPCM);
                    mRecorder.setChannels(2);
                    mRecorder.setSamplingRate(SAMPLERATE_MULTI_CH);
                    new StartRecordingTask().execute(new RecordingParams(
                            MediaRecorderWrapper.OutputFormat.WAVE,
                            ".wav", this, mAudioSourceType,
                            MediaRecorderWrapper.AudioEncoder.LPCM));
                } else if (AUDIO_AMR_WB.equals(mRequestedType)) {
                    mRemainingTimeCalculator.setBitRate(BITRATE_AMR_WB);
                    mRecorder.setSamplingRate(BITRATE_AMR_WB);
                    new StartRecordingTask().execute(new RecordingParams(
                            mAudioOutputFormat, mAmrWidebandExtension, this,
                            mAudioSourceType, MediaRecorderWrapper.AudioEncoder.AMR_WB));
                } else if (AUDIO_MPEGH.equals(mRequestedType)) {
                    mRemainingTimeCalculator.setBitRate(BITRATE_MPEGH);
                    mRecorder.setSamplingRate(SAMPLERATE_MPEGH);
                    mRecorder.setAudioEncodingBitRate(BITRATE_MPEGH);
                    mRecorder.setChannels(4);
                    mAudioSourceType = MediaRecorderWrapper.AudioSource.MIC;
                    new StartRecordingTask().execute(new RecordingParams(
                            mAudioOutputFormat, ".mp4", this,
                            mAudioSourceType, MediaRecorderWrapper.AudioEncoder.MPEGH));
                } else {
                    throw new IllegalArgumentException("Invalid output file type requested");
                }

                if (mMaxFileSize != -1) {
                    mRemainingTimeCalculator.setFileSizeLimit(
                            mRecorder.sampleFile(), mMaxFileSize);
                }
                mRecorderStop = false;
                mRecorderProcessed = false;
            }
            invalidateOptionsMenu();
            break;
        case R.id.stopButton:
            mRecorder.stop();
            showRenameDialogIfNeed();
            mVUMeter.resetAngle();
            invalidateOptionsMenu();
            break;
        }
    }

    private boolean acceptSample(String newName) {
        boolean isExists = FileUtils.exists(mRecorder.sampleFile());
        if (!isExists) {
            Toast.makeText(SoundRecorder.this, R.string.file_deleted,Toast.LENGTH_SHORT).show();
        }
        if (newName != null && !newName.equals(getLastFileName(false))) {
            mRecorder.renameSampleFile(newName);
        }
        mSampleInterrupted = false;
        mRecorder.stop();
        mRecorderProcessed = true;
        if (isExists) {
            saveSample(false);
        } else {
            // reset mRecorder and restore UI.
            mRecorder.clear();
            updateUi();
        }
        mVUMeter.resetAngle();
        if (mExitAfterRecord) {
            finish();
            return false;
        }
        return true;
    }

    private void discardSample() {
        mSampleInterrupted = false;
        mRecorder.delete();
        mRecorderProcessed = true;
        mVUMeter.resetAngle();
    }

    private void setBitRate(int bitRate) {
        mRemainingTimeCalculator.setBitRate(bitRate);
        mRecorder.setAudioEncodingBitRate(bitRate);
    }

    private void openOptionDialog(int optionType) {
        final Context dialogContext = new ContextThemeWrapper(this,
                android.R.style.Theme_DeviceDefault_Light_Dialog_Alert);
        final Resources res = dialogContext.getResources();
        final LayoutInflater dialogInflater = (LayoutInflater) dialogContext
                .getSystemService(Context.LAYOUT_INFLATER_SERVICE);

        final ArrayAdapter<Integer> adapter = new ArrayAdapter<Integer>(this,
                android.R.layout.simple_list_item_single_choice) {
            @Override
            public View getView(int position, View convertView, ViewGroup parent) {
                if (convertView == null) {
                    convertView = dialogInflater.inflate(
                            android.R.layout.simple_list_item_single_choice, parent, false);
                }

                final int resId = this.getItem(position);
                ((TextView) convertView).setText(resId);
                return convertView;
            }
        };
        if (optionType == SETTING_TYPE_FILE_TYPE) {
            adapter.add(R.string.format_setting_amr_item);
            adapter.add(R.string.format_setting_3gpp_item);
            adapter.add(R.string.format_setting_aac_item);
            if (mWAVSupport) {
                adapter.add(R.string.format_setting_wav_item);
            }
        } else if (optionType == SETTING_TYPE_STORAGE_LOCATION) {
            adapter.add(R.string.storage_setting_local_item);
            adapter.add(R.string.storage_setting_sdcard_item);
        }

        final DialogInterface.OnClickListener clickListener =
                new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int which) {
                        dialog.dismiss();
                        final int resId = adapter.getItem(which);
                        switch (resId) {
                            case R.string.format_setting_amr_item:
                                mRequestedType = AUDIO_AMR;
                                mFileType = 0;
                                mPrefsStoragePathEditor.putString("requestedType", mRequestedType);
                                mPrefsStoragePathEditor.putInt("fileType", mFileType);
                                mPrefsStoragePathEditor.commit();
                                break;
                            case R.string.format_setting_3gpp_item:
                                mRequestedType = AUDIO_3GPP;
                                mFileType = 1;
                                mPrefsStoragePathEditor.putString("requestedType", mRequestedType);
                                mPrefsStoragePathEditor.putInt("fileType", mFileType);
                                mPrefsStoragePathEditor.commit();
                                // Keep 40KB size in the Recording file for Mpeg4Writer to write
                                // Moov.
                                if ((mMaxFileSize != -1) && (mMaxFileSize > 40 * 1024))
                                    mMaxFileSize = mMaxFileSize - 40 * 1024;
                                break;
                            case R.string.format_setting_aac_item:
                                mRequestedType = AUDIO_AAC_MP4;
                                mFileType = 2;
                                mPrefsStoragePathEditor.putString("requestedType", mRequestedType);
                                mPrefsStoragePathEditor.putInt("fileType", mFileType);
                                mPrefsStoragePathEditor.commit();
                                break;
                            case R.string.format_setting_wav_item:
                                mRequestedType = AUDIO_WAVE_2CH_LPCM;
                                mFileType = 3;
                                mPrefsStoragePathEditor.putString("requestedType", mRequestedType);
                                mPrefsStoragePathEditor.putInt("fileType", mFileType);
                                mPrefsStoragePathEditor.commit();
                                break;
                            case R.string.storage_setting_sdcard_item:
                                mStoragePath = StorageUtils.getSdStoragePath(SoundRecorder.this);
                                mPath = StorageUtils.STORAGE_PATH_SD_INDEX;
                                mPrefsStoragePathEditor.putString("storagePath", mStoragePath);
                                mPrefsStoragePathEditor.putInt("path", mPath);
                                mPrefsStoragePathEditor.commit();
                                if (mPath == StorageUtils.STORAGE_PATH_SD_INDEX
                                        && !StorageUtils.isSdMounted(SoundRecorder.this)) {
                                    mSdExist = false;
                                    mSampleInterrupted = true;
                                    mErrorUiMessage = getResources()
                                            .getString(R.string.insert_sd_card);
                                } else {
                                    mErrorUiMessage = null;
                                }
                                updateUi();
                                break;
                            case R.string.storage_setting_local_item:
                                mSdExist = true;
                                mStoragePath = StorageUtils.getPhoneStoragePath();
                                mPath = StorageUtils.STORAGE_PATH_PHONE_INDEX;
                                mPrefsStoragePathEditor.putString("storagePath", mStoragePath);
                                mPrefsStoragePathEditor.putInt("path", mPath);
                                mPrefsStoragePathEditor.commit();
                                mSampleInterrupted = false;
                                mErrorUiMessage = null;
                                updateUi();
                                break;

                            default: {
                                Log.e(TAG, "Unexpected resource: "
                                        + getResources().getResourceEntryName(resId));
                            }
                        }
                    }
                };

        AlertDialog ad = null;
        if (optionType == SETTING_TYPE_STORAGE_LOCATION) {
            ad = new AlertDialog.Builder(this)
                    .setTitle(R.string.storage_setting)
                    .setSingleChoiceItems(adapter, mPath, clickListener)
                    .create();
        } else if (optionType == SETTING_TYPE_FILE_TYPE) {
            ad = new AlertDialog.Builder(this)
                    .setTitle(R.string.format_setting)
                    .setSingleChoiceItems(adapter, mFileType, clickListener)
                    .create();
        }
        ad.setCanceledOnTouchOutside(true);
        ad.show();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // TODO Auto-generated method stub
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.main_menu, menu);
        return super.onCreateOptionsMenu(menu);
    }

    @Override
    public boolean onPrepareOptionsMenu(Menu menu) {
        super.onPrepareOptionsMenu(menu);
        menu.findItem(R.id.menu_item_keyboard).setEnabled(mRecorder.state() == Recorder.IDLE_STATE);
        menu.findItem(R.id.menu_item_filetype).setEnabled(
                (mRecorder.state() == Recorder.IDLE_STATE) && (!mExitAfterRecord));
        menu.findItem(R.id.menu_item_storage).setEnabled(mRecorder.state() == Recorder.IDLE_STATE);
        if (SystemPropertiesWrapper.getBoolean(VENDOR_SOUNDRECORDER_DEBUG_ENABLE, false)) {
            menu.findItem(R.id.menu_item_keyboard).setVisible(true);
        } else {
            menu.findItem(R.id.menu_item_keyboard).setVisible(false);
        }

        if (mRecorderStop && !mRecorderProcessed) {
            menu.findItem(R.id.menu_item_keyboard).setEnabled(false);
            menu.findItem(R.id.menu_item_filetype).setEnabled(false);
            menu.findItem(R.id.menu_item_storage).setEnabled(false);
        }
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // TODO Auto-generated method stub
        switch (item.getItemId()) {
            case R.id.menu_item_keyboard:
                if(mRecorder.state() == Recorder.IDLE_STATE) {
                    InputMethodManager inputMgr =
                            (InputMethodManager)getSystemService(Context.INPUT_METHOD_SERVICE);
                    inputMgr.toggleSoftInput(0, 0);
                }
                break;
            case R.id.menu_item_filetype:
                if(mRecorder.state() == Recorder.IDLE_STATE) {
                    openOptionDialog(SETTING_TYPE_FILE_TYPE);
                }
                break;
            case R.id.menu_item_storage:
                if(mRecorder.state() == Recorder.IDLE_STATE) {
                    openOptionDialog(SETTING_TYPE_STORAGE_LOCATION);
                }
                break;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        //show softkeyboard after the "menu" key is pressed and released(key up)
        if(keyCode == KeyEvent.KEYCODE_MENU) {
            InputMethodManager inputMgr = (InputMethodManager)getSystemService(
                                                    Context.INPUT_METHOD_SERVICE);
            inputMgr.toggleSoftInput(0, 0);
            return true;
        } else {
            return super.onKeyUp(keyCode, event);
        }
    }

    @Override
    public void onBackPressed() {
        switch (mRecorder.state()) {
            case Recorder.IDLE_STATE:
                super.onBackPressed();
                break;
            case Recorder.PAUSE_STATE:
            case Recorder.RECORDING_STATE:
                try {
                    Thread.sleep(BACK_KEY_WAIT);
                } catch (InterruptedException ex) {
                }
                mRecorder.stop();
                showRenameDialogIfNeed();
                break;
        }
    }

    // Voicememo Adding UI choice for the user to get the format needed
    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
         Log.v(TAG, "dispatchKeyEvent with key event" + event);

    if(event.getKeyCode() == KeyEvent.KEYCODE_6 && event.getAction() == event.ACTION_UP){
       //Ignore ACTION_DOWN to avoid showing error dialog twice
       if((mAudioSourceType == MediaRecorderWrapper.AudioSource.VOICE_CALL) ||
          (mAudioSourceType == MediaRecorderWrapper.AudioSource.VOICE_DOWNLINK)||
          (mAudioSourceType == MediaRecorderWrapper.AudioSource.VOICE_UPLINK ) ||
          ((mAudioSourceType == MediaRecorderWrapper.AudioSource.MIC) &&
           (mAudioManager.getMode() == AudioManager.MODE_IN_CALL))) {
          mAudioSourceType = MediaRecorderWrapper.AudioSource.MIC;//Default type
          Resources res = getResources();
          String message = null;
          message = res.getString(R.string.error_mediadb_aacincall);
          new AlertDialog.Builder(this)
          .setTitle(R.string.app_name)
          .setMessage(message)
          .setPositiveButton(R.string.button_ok, null)
          .setCancelable(false)
          .show();
          return super.dispatchKeyEvent(event);
       }
    }

    if((event.getKeyCode() == KeyEvent.KEYCODE_1 || event.getKeyCode() == KeyEvent.KEYCODE_2)
         && (event.getAction() == event.ACTION_UP)){
       //Ignore ACTION_DOWN to avoid showing error dialog twice
        if((mAudioManager.getMode() != AudioManager.MODE_IN_CALL) ||
               (AUDIO_AAC_MP4.equals(mRequestedType))) {

            mAudioSourceType = MediaRecorderWrapper.AudioSource.MIC;//Default type
            Resources res = getResources();
            String message = null;

            if(mAudioManager.getMode() != AudioManager.MODE_IN_CALL) {
                message = res.getString(R.string.error_mediadb_incall);
            } else {
                message = res.getString(R.string.error_mediadb_aacincall);
            }

            new AlertDialog.Builder(this)
                .setTitle(R.string.app_name)
                .setMessage(message)
                .setPositiveButton(R.string.button_ok, null)
                .setCancelable(false)
                .show();

            return super.dispatchKeyEvent(event);
        }
    }
        // Intercept some events before they get dispatched to our views.
        boolean ret = false;
        switch (event.getKeyCode()) {
            case KeyEvent.KEYCODE_0: // MIC source (Camcorder)
            {
              Log.e(TAG, "Selected MIC Source: Key Event" + KeyEvent.KEYCODE_0);
              mAudioSourceType = MediaRecorderWrapper.AudioSource.MIC;
              if ((mAudioManager.getMode() == AudioManager.MODE_IN_CALL) &&
                  (event.getAction() == event.ACTION_UP)) {
                  mAudioSourceType = MediaRecorderWrapper.AudioSource.VOICE_UPLINK;
                  Log.e(TAG, "Selected Voice Tx only Source: sourcetype" + mAudioSourceType);
              }
              ret = true;
              break;
            }

            case KeyEvent.KEYCODE_1: // Voice Rx Only (Only during Call(
            {
              Log.e(TAG, "Selected Voice Rx only Source: Key Event" + KeyEvent.KEYCODE_1);
              mAudioSourceType = MediaRecorderWrapper.AudioSource.VOICE_DOWNLINK;
              ret = true;
              break;
            }

            case KeyEvent.KEYCODE_2: // Voice Rx+Tx (Only during Call)
            {
              Log.e(TAG, "Selected Voice Tx+Rx Source: Key Event" + KeyEvent.KEYCODE_2);
              mAudioSourceType = MediaRecorderWrapper.AudioSource.VOICE_CALL;
              ret = true;
              break;
            }

            case KeyEvent.KEYCODE_3: // Selected AMR codec type
            {
              Log.e(TAG, "Selected AUDIO_AMR Codec: Key Event" + KeyEvent.KEYCODE_3);
              mRequestedType = AUDIO_AMR;
              ret = true;
              break;
            }

            case KeyEvent.KEYCODE_4: // Selected EVRC codec type
            {
              Log.e(TAG, "Selected Voice AUDIO_EVRC Codec: Key Event" + KeyEvent.KEYCODE_4);
              mRequestedType = AUDIO_EVRC;
              ret = true;
              break;
            }

            case KeyEvent.KEYCODE_5: // Selected QCELP codec type
            {
              Log.e(TAG, "Selected AUDIO_QCELP Codec: Key Event" + KeyEvent.KEYCODE_5);
              mRequestedType = AUDIO_QCELP;
              ret = true;
              break;
            }
            case KeyEvent.KEYCODE_6: // Selected AAC codec type
            {
              Log.e(TAG, "Selected AUDIO_AAC_MP4 Codec: Key Event" + KeyEvent.KEYCODE_6);
              mRequestedType = AUDIO_AAC_MP4;
              ret = true;
              break;
            }
            case KeyEvent.KEYCODE_7: // Selected 6 channel wave lpcm codec type
            {
              if (true == bSSRSupported) {
                Log.e(TAG, "Selected multichannel AAC Codec: Key Event" + KeyEvent.KEYCODE_7);
                mRequestedType = AUDIO_AAC_5POINT1_CHANNEL;
                ret = true;
              }
              break;
            }
            case KeyEvent.KEYCODE_8: // Selected 6 channel AAC recording
            {
                if (true == bSSRSupported) {
                Log.e(TAG, "Selected linear pcm Codec: Key Event" + KeyEvent.KEYCODE_7);
                mRequestedType = AUDIO_WAVE_6CH_LPCM;
                ret = true;
              }
              break;
            }
            case KeyEvent.KEYCODE_9: // Selected amr-wb codec type in .awb file format
            {
              Log.e(TAG, "### Selected amr wb Codec in .awb: Key Event" + KeyEvent.KEYCODE_8);
              mRequestedType = AUDIO_AMR_WB;
              mAudioOutputFormat = MediaRecorderWrapper.OutputFormat.AMR_WB;
              mAmrWidebandExtension = ".awb";
              ret = true;
              break;
            }
            case KeyEvent.KEYCODE_A: // Selected amr-wb codec type in .3gpp file format
            {
              Log.e(TAG, "### Selected awr wb Codec in 3gp: Key Event" + KeyEvent.KEYCODE_9);
              mRequestedType = AUDIO_AMR_WB;
              mAmrWidebandExtension = ".3gpp";
              mAudioOutputFormat = MediaRecorderWrapper.OutputFormat.THREE_GPP;
              ret = true;
              break;
            }
            case KeyEvent.KEYCODE_B: // Selected mpegh codec type in .mp4 file format
            {
              Log.e(TAG, "### Selected mpegh_Enc : Key Event" + KeyEvent.KEYCODE_B);
              mRequestedType = AUDIO_MPEGH;
              mAudioOutputFormat = MediaRecorderWrapper.OutputFormat.MPEG_4;
              ret = true;
              break;
            }
            default:
                break;
        }

        return ret?ret:super.dispatchKeyEvent(event);
    }

    @Override
    public void onStop() {
        mRecorder.stop();
        super.onStop();
    }

    @Override
    protected void onPause() {
        // Stop listening for phone state changes.
        for(int i = 0; i < mPhoneCount; i++) {
            // adapt case: disabled telephony feature or activate card failure
            int[] subId = SubscriptionManager.getSubId(i);
            if (null != mPhoneStateListener[i] && (null != subId && subId.length > 0)) {
                mTelephonyManager
                    .createForSubscriptionId(subId[0])
                    .listen(mPhoneStateListener[i], PhoneStateListener.LISTEN_NONE);
            }
        }
        mRecorder.stop();
        // if dialog is shown, dialog processing the logic.
        if (!mRenameDialogShown) {
            if (mRecorder.sampleLength() > 0) {
                mRecorderStop = true;
                saveSample(true);
            } else {
                mRecorder.delete();
            }
        }
        super.onPause();
    }

    /*
     * If we have just recorded a sample, this adds it to the media data base
     * and sets the result to the sample's URI.
     */
    private boolean saveSample(boolean showToast) {
        Uri uri = null;

        if (mRecorder.sampleLength() <= 0) {
            mRecorder.delete();
            return false;
        }

        try {
            mDataExist = DatabaseUtils.isDataExist(getContentResolver(), mRecorder.sampleFile());
            if (!mDataExist) {
                uri = DatabaseUtils.addToMediaDB(SoundRecorder.this, mRecorder.sampleFile(),
                        mRecorder.sampleLengthMillis(), mRequestedType);
            }
        } catch(UnsupportedOperationException ex) {  // Database manipulation failure
            return false;
        } finally {
            if (uri == null && !mDataExist) {
                return false;
            }
        }

        if (showToast) {
            showSavedToast();
        }

        // reset mRecorder and restore UI.
        mRecorder.clear();
        updateUi();
        setResult(RESULT_OK, new Intent().setData(uri)
                .setFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION));
        return true;
    }

    private String getLastFileName(boolean withExtension) {
        return FileUtils.getLastFileName(mRecorder.sampleFile(), withExtension);
    }

    // Show a dialog to rename file name.
    private void showRenameDialogIfNeed() {
        if (mRecorder == null) return;
        if (mRecorder.sampleLength() > 0) {
            mRecorderStop = true;
            RenameDialogBuilder builder = new RenameDialogBuilder(this, mRecorder.sampleFile());
            builder.setTitle(R.string.save_dialog_title);
            builder.setNegativeButton(R.string.discard, null);
            builder.setPositiveButton(R.string.rename_dialog_save,
                    new RenameDialogBuilder.OnPositiveListener() {
                        @Override
                        public void onClick(DialogInterface dialog, int which, String newName) {
                            if (acceptSample(newName)) {
                                startListActivity();
                            }
                        }
                    });
            builder.setOnDismissListener(new DialogInterface.OnDismissListener() {
                @Override
                public void onDismiss(DialogInterface dialog) {
                    discardSample();
                    mRenameDialogShown = false;
                }
            });
            builder.setOnCancelListener(new DialogInterface.OnCancelListener() {
                @Override
                public void onCancel(DialogInterface dialog) {
                    discardSample();
                    mRenameDialogShown = false;
                }
            });
            builder.setEditTextContent(getLastFileName(false));
            builder.show();
            mRenameDialogShown = true;
        } else {
            mRecorder.delete();
        }
    }

    private void startListActivity() {
        Intent intent = new Intent(SoundRecorder.this, FileListActivity.class);
        startActivity(intent);
    }

    private void showSavedToast() {
        String info = getResources().getString(R.string.file_saved_interrupt);
        Toast.makeText(SoundRecorder.this, info, Toast.LENGTH_SHORT).show();
    }

    /*
     * Called on destroy to unregister the SD card mount event receiver.
     */
    @Override
    public void onDestroy() {
        if (mSDCardMountEventReceiver != null) {
            unregisterReceiver(mSDCardMountEventReceiver);
            mSDCardMountEventReceiver = null;
        }
        if (mPowerOffReceiver != null) {
            unregisterReceiver(mPowerOffReceiver);
            mPowerOffReceiver = null;
        }
        unregisterReceiver(mMountReceiver);

        if (null != mProgressDialog && mProgressDialog.isShowing()) {
            mProgressDialog.dismiss();
            mProgressDialog = null;
        }

        if (mMsgHandler.hasMessages(MSG_DISMISS_PROGRESS_DIALOG)) {
            mMsgHandler.removeMessages(MSG_DISMISS_PROGRESS_DIALOG);
        }

        super.onDestroy();
    }

    /*
     * Registers an intent to listen for ACTION_SHUTDOWN notifications.
     */
    private void registerPowerOffListener() {
        if (mPowerOffReceiver == null) {
            mPowerOffReceiver = new BroadcastReceiver() {
                @Override
                public void onReceive(Context context, Intent intent) {
                    String action = intent.getAction();

                    if (mRecorder != null) {
                        mRecorder.stop();
                    }

                    if (action.equals(Intent.ACTION_SHUTDOWN)) {
                        mRecorder.delete();
                    }
                }
            };
            IntentFilter iFilter = new IntentFilter();
            iFilter.addAction(Intent.ACTION_SHUTDOWN);
            iFilter.addAction(Intent.ACTION_SCREEN_OFF);
            registerReceiver(mPowerOffReceiver, iFilter);
        }
    }

    /*
     * Registers an intent to listen for ACTION_MEDIA_EJECT/ACTION_MEDIA_MOUNTED
     * notifications.
     */
    private void registerExternalStorageListener() {
        if (mSDCardMountEventReceiver == null) {
            mSDCardMountEventReceiver = new BroadcastReceiver() {
                @Override
                public void onReceive(Context context, Intent intent) {
                    String action = intent.getAction();
                    if (action.equals(Intent.ACTION_MEDIA_EJECT)) {
                        mRecorder.delete();
                    } else if (action.equals(Intent.ACTION_MEDIA_MOUNTED)) {
                        mRecorder.delete();
                    }
                }
            };
            IntentFilter iFilter = new IntentFilter();
            iFilter.addAction(Intent.ACTION_MEDIA_EJECT);
            iFilter.addAction(Intent.ACTION_MEDIA_MOUNTED);
            iFilter.addDataScheme("file");
            registerReceiver(mSDCardMountEventReceiver, iFilter);
        }
    }

    /**
     * Update the big MM:SS timer. If we are in playback, also update the
     * progress bar.
     */
    private void updateTimerView() {
        int state = mRecorder.state();

        boolean ongoing = state == Recorder.RECORDING_STATE;

        long time = ongoing ? mRecorder.progress() : mRecorder.sampleLength();
        mTimerView.setText(Utils.timeToString(mTimerView.getContext(), time));

        if (state == Recorder.RECORDING_STATE) {
            updateTimeRemaining();
        }

        if (ongoing)
            mHandler.postDelayed(mUpdateTimer, 1000);
    }

    /*
     * Called when we're in recording state. Find out how much longer we can
     * go on recording. If it's under 5 minutes, we display a count-down in
     * the UI. If we've run out of time, stop the recording.
     */
    private void updateTimeRemaining() {
        long t = mRemainingTimeCalculator.timeRemaining();

        if (t <= 0) {
            mSampleInterrupted = true;

            int limit = mRemainingTimeCalculator.currentLowerLimit();
            switch (limit) {
                case RemainingTimeCalculator.DISK_SPACE_LIMIT:
                    mErrorUiMessage
                        = getResources().getString(R.string.storage_is_full);
                    break;
                case RemainingTimeCalculator.FILE_SIZE_LIMIT:
                    mErrorUiMessage
                        = getResources().getString(R.string.max_length_reached);
                    break;
                default:
                    mErrorUiMessage = null;
                    break;
            }

            mRecorder.stop();
            showRenameDialogIfNeed();
            return;
        }

        Resources res = getResources();
        String timeStr = "";

        // display available time, if less than 10 minutes
        if (t < 599)
        {
            timeStr = String.format(mTimerFormat, t / 60, t % 60);
        }

        mStateMessage1.setText(timeStr);
    }

    /**
     * Shows/hides the appropriate child views for the new state.
     */
    private void updateUi() {
        Resources res = getResources();

        switch (mRecorder.state()) {
            case Recorder.IDLE_STATE:
                if (mRecorder.sampleLength() == 0) {
                    mRecordButton.setImageResource(R.drawable.record);
                    mRecordButton.setEnabled(true);
                    mRecordButton.setFocusable(false);
                    mStopButton.setEnabled(false);
                    mStopButton.setFocusable(false);

                    mStateMessage1.setVisibility(View.INVISIBLE);
                    mStateMessage2.setVisibility(View.VISIBLE);
                    if (true == bSSRSupported) {
                        mStateMessage2.setText(res.getString(R.string.press_record_ssr));
                    } else {
                        if (SystemPropertiesWrapper.getBoolean(VENDOR_SOUNDRECORDER_DEBUG_ENABLE, false)) {
                            mStateMessage2.setText(res.getString(R.string.press_record));
                        } else {
                            mStateMessage2.setText(res.getString(R.string.press_record2));
                        }
                    }
                    mVUMeter.resetAngle();

                    setTitle(res.getString(R.string.record_your_message));
                } else {
                    mRecordButton.setImageResource(R.drawable.record);
                    mRecordButton.setEnabled(true);
                    mRecordButton.setFocusable(true);
                    mStopButton.setEnabled(false);
                    mStopButton.setFocusable(false);

                    mStateMessage1.setVisibility(View.INVISIBLE);
                    mStateMessage2.setVisibility(View.VISIBLE);
                    mStateMessage2.setText(res.getString(R.string.recording_stopped));

                    setTitle(res.getString(R.string.message_recorded));
                }

                if (mSampleInterrupted) {
                    //TODO: Set decent message and icon resources
                    mStateMessage2.setVisibility(View.VISIBLE);
                    mStateMessage2.setText(res.getString(R.string.recording_stopped));
                }

                if (mErrorUiMessage != null) {
                    mStateMessage1.setText(mErrorUiMessage);
                    mStateMessage1.setVisibility(View.VISIBLE);
                }

                // disable list button if start from ACTION_GET_CONTENT
                if (mIsGetContentAction) {
                    mListButton.setEnabled(false);
                    mListButton.setFocusable(false);
                } else {
                    mListButton.setEnabled(true);
                    mListButton.setFocusable(true);
                }
                break;
            case Recorder.RECORDING_STATE:
                mRecordButton.setImageResource(R.drawable.pause);
                mRecordButton.setEnabled(true);
                mRecordButton.setFocusable(true);
                mStopButton.setEnabled(true);
                mStopButton.setFocusable(true);
                mListButton.setEnabled(false);
                mListButton.setFocusable(false);

                mStateMessage1.setVisibility(View.VISIBLE);
                mStateMessage2.setVisibility(View.VISIBLE);
                mStateMessage2.setText(res.getString(R.string.recording));

                setTitle(res.getString(R.string.record_your_message));

                break;
            case Recorder.PAUSE_STATE:
                mRecordButton.setImageResource(R.drawable.record);
                mRecordButton.setEnabled(true);
                mRecordButton.setFocusable(true);
                mStopButton.setEnabled(true);
                mStopButton.setFocusable(true);
                mListButton.setEnabled(false);
                mListButton.setFocusable(false);

                mStateMessage1.setVisibility(View.VISIBLE);
                mStateMessage2.setVisibility(View.VISIBLE);
                mStateMessage2.setText(res.getString(R.string.recording_paused));

                mVUMeter.resetAngle();

                setTitle(res.getString(R.string.record_your_message));

                break;
        }

        // If the SD card does not exist and mPath is SD card, disable the record button.
        if (mPath == StorageUtils.STORAGE_PATH_SD_INDEX && !mSdExist) {
            mStateMessage2.setText("");
            mRecordButton.setEnabled(false);
            mRecordButton.setFocusable(false);
        }
        if (mErrorUiMessage == null) {
            mStateMessage1.setText("");
        }
        updateTimerView();
        mVUMeter.invalidate();
    }

    /*
     * Called when Recorder changed it's state.
     */
    public void onStateChanged(int state) {
        if (state == Recorder.RECORDING_STATE) {
            mSampleInterrupted = false;
            mErrorUiMessage = null;
        }

        if (state == Recorder.RECORDING_STATE) {
            mWakeLock.acquire(); // we don't want to go to sleep while recording
        } else {
            if (mWakeLock.isHeld())
                mWakeLock.release();
        }

        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                updateUi();
                invalidateOptionsMenu();
            }
        });
    }

    /*
     * Called when MediaPlayer encounters an error.
     */
    public void onError(int error) {
        final int iError = error;

        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Resources res = getResources();
                boolean isExit = false;

                String message = null;
                switch (iError) {
                    case Recorder.RECORD_INTERRUPTED:
                        message = res.getString(R.string.error_record_interrupted);
                        break;
                    case Recorder.SDCARD_ACCESS_ERROR:
                        message = res.getString(R.string.error_sdcard_access);
                        break;
                    case Recorder.IN_CALL_RECORD_ERROR:
                        // TODO: update error message to reflect that the recording could not be
                        //       performed during a call.
                        message = res.getString(R.string.in_call_record_error);
                        isExit = true;
                        break;
                    case Recorder.INTERNAL_ERROR:
                        message = res.getString(R.string.error_app_internal);
                        isExit = true;
                        break;
                    case Recorder.UNSUPPORTED_FORMAT:
                        message = res.getString(R.string.error_app_unsupported);
                        isExit = true;
                        break;
                    case Recorder.RECORD_LOST_FOCUS:
                        showRenameDialogIfNeed();
                        break;
                }
                if (message != null) {
                    new AlertDialog.Builder(SoundRecorder.this)
                            .setTitle(R.string.app_name)
                            .setMessage(message)
                            .setPositiveButton(R.string.button_ok, (true==isExit)?
                                    (new DialogInterface.OnClickListener() {
                                        public void onClick(DialogInterface dialog, int whichButton) {
                                            finish();
                                        }}):null)
                            .setCancelable(false)
                            .show();
                }
            }
        });
    }

    public void onInfo(int what, int extra) {
        final int iWhat = what;

        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (iWhat == MediaRecorderWrapper.MEDIA_RECORDER_INFO_MAX_DURATION_REACHED) {
                    mRecorder.stop();
                    showRenameDialogIfNeed();
                    mVUMeter.resetAngle();
                    invalidateOptionsMenu();
                }
            }
        });
    }
}
