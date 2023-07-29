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
import java.io.IOException;
import java.text.SimpleDateFormat;

import android.content.Context;
import android.media.AudioManager;
import android.media.MediaRecorder;
import android.os.Build;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Log;
import android.media.AudioManager;
import android.media.AudioManager.OnAudioFocusChangeListener;
import android.os.Handler;
import android.os.Message;

import com.android.soundrecorder.util.FileUtils;
import com.android.soundrecorder.util.StorageUtils;

public class Recorder implements MediaRecorder.OnInfoListener {
    static final String TAG = "Recorder";
    static final String SAMPLE_PREFIX = "recording";
    static final String SAMPLE_PATH_KEY = "sample_path";
    static final String SAMPLE_LENGTH_KEY = "sample_length";

    public static final int IDLE_STATE = 0;
    public static final int RECORDING_STATE = 1;
    public static final int PAUSE_STATE = 2;

    int mState = IDLE_STATE;

    public static final int NO_ERROR = 0;
    public static final int SDCARD_ACCESS_ERROR = 1;
    public static final int INTERNAL_ERROR = 2;
    public static final int IN_CALL_RECORD_ERROR = 3;
    public static final int UNSUPPORTED_FORMAT = 4;
    public static final int RECORD_INTERRUPTED = 5;
    public static final int RECORD_LOST_FOCUS = 6;

    static final int FOCUSCHANGE = 0;

    public int mChannels = 0;
    public int mSamplingRate = 0;
    private int mBitRate = 0;

    public String mStoragePath = null;

    private int mMaxDuration;

    public interface OnStateChangedListener {
        public void onStateChanged(int state);
        public void onError(int error);
        public void onInfo(int what, int extra);
    }
    OnStateChangedListener mOnStateChangedListener = null;

    MediaRecorder.OnErrorListener mMRErrorListener = new MediaRecorder.OnErrorListener() {
        public void onError(MediaRecorder mr, int what, int extra) {
            stop();
            setError(RECORD_INTERRUPTED);
        }
    };

    long mSampleStart = 0;       // time at which latest record or play operation started
    long mSampleLength = 0;      // length of current sample
    File mSampleFile = null;

    MediaRecorder mRecorder = null;
    private AudioManager mAudioManager;
    Context mContext = null;

    public Recorder(Context context) {
        if (context.getResources().getBoolean(R.bool.config_storage_path)) {
            mStoragePath = StorageUtils.applyCustomStoragePath(context);
        } else {
            mStoragePath = StorageUtils.getPhoneStoragePath();
        }
        mContext = context;
        mAudioManager = (AudioManager)mContext.getSystemService(Context.AUDIO_SERVICE);
    }

    public Recorder() {
    }

    public void saveState(Bundle recorderState) {
        recorderState.putString(SAMPLE_PATH_KEY, mSampleFile.getAbsolutePath());
        recorderState.putLong(SAMPLE_LENGTH_KEY, mSampleLength);
    }

    public int getMaxAmplitude() {
        if (mState != RECORDING_STATE)
            return 0;
        return mRecorder.getMaxAmplitude();
    }

    public void restoreState(Bundle recorderState) {
        String samplePath = recorderState.getString(SAMPLE_PATH_KEY);
        if (samplePath == null)
            return;
        long sampleLength = recorderState.getLong(SAMPLE_LENGTH_KEY, -1);
        if (sampleLength == -1)
            return;

        File file = new File(samplePath);
        if (!file.exists())
            return;
        if (mSampleFile != null
                && mSampleFile.getAbsolutePath().compareTo(file.getAbsolutePath()) == 0)
            return;

        delete();
        mSampleFile = file;
        mSampleLength = sampleLength;

        signalStateChanged(IDLE_STATE);
    }

    public void setOnStateChangedListener(OnStateChangedListener listener) {
        mOnStateChangedListener = listener;
    }

    public void setChannels(int nChannelsCount) {
        mChannels = nChannelsCount;
    }

    public void setSamplingRate(int samplingRate) {
        mSamplingRate = samplingRate;
    }

    public void setAudioEncodingBitRate(int bitRate) {
        mBitRate = bitRate;
    }

    public int state() {
        return mState;
    }

    public int progress() {
        if (mState == RECORDING_STATE) {
            return (int) ((mSampleLength + (System.currentTimeMillis() - mSampleStart)) / 1000);
        }
        return 0;
    }

    public int sampleLength() {
        return (int) (mSampleLength / 1000);
    }

    public long sampleLengthMillis() {
        return mSampleLength;
    }

    public File sampleFile() {
        return mSampleFile;
    }

    public void renameSampleFile(String newName) {
        mSampleFile = FileUtils.renameFile(mSampleFile, newName);
    }

    /**
     * Resets the recorder state. If a sample was recorded, the file is deleted.
     */
    public void delete() {
        stop();

        if (mSampleFile != null){
            mSampleFile.delete();
        }

        mSampleFile = null;
        mSampleLength = 0;

        signalStateChanged(IDLE_STATE);
    }

    /**
     * Resets the recorder state. If a sample was recorded, the file is left on disk and will
     * be reused for a new recording.
     */
    public void clear() {
        stop();

        mSampleFile = null;
        mSampleLength = 0;

        signalStateChanged(IDLE_STATE);
    }

    /**
     * Resets the recorder state before prepared. If a sample was recorded, the file is deleted.
     */
    public void release(int err) {
        releaseRecording(err);

        if (mSampleFile != null){
            mSampleFile.delete();
        }
        mSampleFile = null;
        mSampleLength = 0;

        signalStateChanged(IDLE_STATE);
    }

    public void startRecording(int outputfileformat, String extension,
                   Context context, int audiosourcetype, int codectype) {
        stop();

        if (mSampleFile != null) {
            mSampleFile.delete();
            mSampleFile = null;
            mSampleLength = 0;
        }

        File sampleDir = new File(mStoragePath);

        if (!sampleDir.exists()) {
            sampleDir.mkdirs();
        }

        if (!sampleDir.canWrite()) // Workaround for broken sdcard support on the device.
            sampleDir = new File(StorageUtils.getSdStoragePath(context));

        try {
            String prefix = context.getResources().getString(R.string.def_save_name_prefix);
            if (!"".equals(prefix)) {
                //long index = FileUtils.getSuitableIndexOfRecording(prefix);
                //mSampleFile = createTempFile(prefix, Long.toString(index), extension, sampleDir);
                mSampleFile = createTempFile(context, prefix+"-", extension, sampleDir);
            } else {
                prefix = SAMPLE_PREFIX + '-';
                mSampleFile = createTempFile(context, prefix, extension, sampleDir);
            }
        } catch (IOException e) {
            setError(SDCARD_ACCESS_ERROR);
            return;
        }


        mRecorder = new MediaRecorder();

        try {
            mRecorder.setAudioSource(audiosourcetype);
        } catch(RuntimeException exception) {
            release(INTERNAL_ERROR);
            return;
        }

        //set channel for surround sound recording.
        if (mChannels > 0) {
            mRecorder.setAudioChannels(mChannels);
        }
        if (mSamplingRate > 0) {
            mRecorder.setAudioSamplingRate(mSamplingRate);
        }
        if (mBitRate > 0) {
            mRecorder.setAudioEncodingBitRate(mBitRate);
        }

        try {
            mRecorder.setOutputFormat(outputfileformat);
            mRecorder.setOnErrorListener(mMRErrorListener);

            mRecorder.setMaxDuration(mMaxDuration);
            mRecorder.setOnInfoListener(this);

            mRecorder.setAudioEncoder(codectype);
        } catch(RuntimeException exception) {
            release(UNSUPPORTED_FORMAT);
            return;
        }

        // Handle IOException
        try {
            mRecorder.setOutputFile(mSampleFile.getAbsolutePath());

            mRecorder.prepare();
        } catch(IOException | IllegalStateException exception) {
            release(INTERNAL_ERROR);
            return;
        }
        // Handle RuntimeException if the recording couldn't start
        Log.d(TAG,"audiosourcetype " +audiosourcetype);
        try {
            mRecorder.start();
        } catch (RuntimeException exception) {
            AudioManager audioMngr = (AudioManager)context.getSystemService(Context.AUDIO_SERVICE);
            boolean isInCall = ((audioMngr.getMode() == AudioManager.MODE_IN_CALL) ||
                    (audioMngr.getMode() == AudioManager.MODE_IN_COMMUNICATION));
            if (isInCall) {
                setError(IN_CALL_RECORD_ERROR);
            } else {
                setError(INTERNAL_ERROR);
            }
            delete();
            return;
        }
        mSampleStart = System.currentTimeMillis();
        setState(RECORDING_STATE);
        stopAudioPlayback();
    }

    public void pauseRecording() {
        if (mRecorder == null) {
            return;
        }
        try {
            mRecorder.pause();
        } catch (RuntimeException exception) {
            setError(INTERNAL_ERROR);
            Log.e(TAG, "Pause Failed");
        }
        mSampleLength = mSampleLength + (System.currentTimeMillis() - mSampleStart);
        setState(PAUSE_STATE);
    }

    public void resumeRecording() {
        if (mRecorder == null) {
            return;
        }
        stopAudioPlayback();
        try {
            if(Build.VERSION.SDK_INT >= 23){
                mRecorder.resume();
            }else{
                mRecorder.start();
            }
        } catch (RuntimeException exception) {
            setError(INTERNAL_ERROR);
            Log.e(TAG, "Resume Failed");
        }
        mSampleStart = System.currentTimeMillis();
        setState(RECORDING_STATE);
    }

    public void stopRecording() {
        if (mRecorder == null){
            return;
        }

        try {
            if ((PAUSE_STATE == mState) && (Build.VERSION.SDK_INT >= 23)){
                resumeRecording();
                setState(RECORDING_STATE);
            }
            mRecorder.stop();
        }catch (RuntimeException exception){
            if(sampleLength() > 1){
                setError(INTERNAL_ERROR);
                Log.e(TAG, "Stop Failed");
            }else{
                Log.e(TAG, "Quickly Stop Failed");
            }
        }

        mRecorder.reset();
        mRecorder.release();
        mRecorder = null;
        mChannels = 0;
        mSamplingRate = 0;
        if (mState == RECORDING_STATE) {
            mSampleLength = mSampleLength + (System.currentTimeMillis() - mSampleStart);
        }
        setState(IDLE_STATE);
    }


    public void releaseRecording(int err) {

        setError(err);

        if (mRecorder == null){
            return;
        }

        mRecorder.reset();
        mRecorder.release();
        mRecorder = null;

    }

    public void stop() {
        stopRecording();
        mAudioManager.abandonAudioFocus(mAudioFocusListener);
    }

    private void setState(int state) {
        if (state == mState)
            return;

        mState = state;
        signalStateChanged(mState);
    }

    private void signalStateChanged(int state) {
        if (mOnStateChangedListener != null)
            mOnStateChangedListener.onStateChanged(state);
    }

    private void setError(int error) {
        if (mOnStateChangedListener != null)
            mOnStateChangedListener.onError(error);
    }

    public void setStoragePath(String path) {
        mStoragePath = path;
    }

    public File createTempFile(String prefix, String fileName, String suffix, File directory)
            throws IOException {
        // Force a prefix null check first
        if (prefix.length() < 3) {
            throw new IllegalArgumentException("prefix must be at least 3 characters");
        }
        if (suffix == null) {
            suffix = ".tmp";
        }
        File tmpDirFile = directory;
        if (tmpDirFile == null) {
            String tmpDir = System.getProperty("java.io.tmpdir", ".");
            tmpDirFile = new File(tmpDir);
        }

        File result;
        do {
            result = new File(tmpDirFile, prefix + fileName + suffix);
        } while (!result.createNewFile());
        return result;
    }

    public File createTempFile(Context context, String prefix, String suffix, File directory)
            throws IOException {
        String nameFormat = context.getResources().getString(R.string.def_save_name_format);
        SimpleDateFormat df = new SimpleDateFormat(nameFormat);
        String currentTime = df.format(System.currentTimeMillis());
        if (!TextUtils.isEmpty(currentTime)) {
            currentTime = currentTime.replaceAll("[\\\\*|\":<>/?]", "_").replaceAll(" ",
                    "\\\\" + " ");
        }
        return createTempFile(prefix, currentTime, suffix, directory);
    }

    public void setMaxDuration(int duration) {
        mMaxDuration = duration;
    }

    @Override
    public void onInfo(MediaRecorder mr, int what, int extra) {
        if (mOnStateChangedListener != null) {
            mOnStateChangedListener.onInfo(what, extra);
        }
    }

    /*
     * Make sure we're not recording music playing in the background, ask
     * the MediaPlaybackService to pause playback.
     */
    private void stopAudioPlayback() {
        AudioManager am = (AudioManager)mContext.getSystemService(Context.AUDIO_SERVICE);
        am.requestAudioFocus(mAudioFocusListener,
                AudioManager.STREAM_MUSIC, AudioManager.AUDIOFOCUS_GAIN);
    }

    private OnAudioFocusChangeListener mAudioFocusListener =
        new OnAudioFocusChangeListener() {
            public void onAudioFocusChange(int focusChange) {
                mRecorderHandler.obtainMessage(FOCUSCHANGE, focusChange, 0)
                    .sendToTarget();
        }
    };

    private Handler mRecorderHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case FOCUSCHANGE:
                    switch (msg.arg1) {
                        case AudioManager.AUDIOFOCUS_LOSS:
                            if (state() == Recorder.RECORDING_STATE) {
                                stop();
                                setError(RECORD_LOST_FOCUS);
                            }
                            break;

                        default:
                            break;
                    }
                    break;

                default:
                    break;
            }
        }
    };

}
