/*
 * Copyright (C) 2008 The Android Open Source Project
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
import android.database.Cursor;
import android.media.AudioManager;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Message;
import android.provider.MediaStore;
import android.view.Window;
import android.view.WindowManager;

public class ScanningProgress extends Activity
{
    private final static int CHECK = 0;
    private final static int EXTERNAL_CHECK_OK = 1;
    private final static int EXTERNAL_CHECK_FAILED = 2;
    private Handler mHandler = new Handler() {
        @Override
        public void handleMessage(Message msg)
        {
            switch (msg.what) {
            case CHECK:
                String status = Environment.getExternalStorageState();
                if (!status.equals(Environment.MEDIA_MOUNTED)) {
                   // If the card suddenly got unmounted again, there's
                   // really no need to keep waiting for the media scanner.
                   finish();
                   return;
                }

                // new one thread to run the runnable,
                // the thread will quit it self after running
                new Thread(mCheckExternalMediaThread).start();

                break;
            case EXTERNAL_CHECK_OK:
                setResult(RESULT_OK);
                finish();
                return;
            case EXTERNAL_CHECK_FAILED:
                Message next = obtainMessage(CHECK);
                sendMessageDelayed(next, 3000);
                break;
            default:
                break;
            }
        }
    };

    private Runnable mCheckExternalMediaThread = new Runnable() {
         @Override
         public void run() {
             int result = isExternalMediaDatabaseReady() ?
                          EXTERNAL_CHECK_OK : EXTERNAL_CHECK_FAILED;
             Message message = mHandler.obtainMessage(result);
             mHandler.sendMessage(message);
         }
    };

    private boolean isExternalMediaDatabaseReady() {
        boolean isReady = false;
        Cursor cursor = null;
        try {
            cursor = MusicUtils.query(ScanningProgress.this,
                    MediaStore.Audio.Playlists.EXTERNAL_CONTENT_URI,
                    null, null, null, null);
            isReady = (cursor != null);
        } catch (Exception exception){
            isReady = false;
        } finally {
            if (cursor != null) {
                cursor.close();
            }
        }
        return isReady;
    }

    @Override
    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        setVolumeControlStream(AudioManager.STREAM_MUSIC);

        requestWindowFeature(Window.FEATURE_NO_TITLE);
        if (android.os.Environment.isExternalStorageRemovable()) {
            setContentView(R.layout.scanning);
        } else {
            setContentView(R.layout.scanning_nosdcard);
        }
        getWindow().setLayout(WindowManager.LayoutParams.WRAP_CONTENT,
                                    WindowManager.LayoutParams.WRAP_CONTENT);
        setResult(RESULT_CANCELED);
        
        Message msg = mHandler.obtainMessage(CHECK);
        mHandler.sendMessageDelayed(msg, 1000);
    }
    
    @Override
    public void onDestroy() {
        mHandler.removeMessages(CHECK);
        super.onDestroy();
    }
}
