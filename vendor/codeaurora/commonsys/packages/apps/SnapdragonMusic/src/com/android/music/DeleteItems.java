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
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.database.Cursor;
import android.media.AudioManager;
import android.os.AsyncTask;
import android.os.Bundle;
import android.provider.MediaStore;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.TextView;
import android.view.KeyEvent;
import android.net.Uri;
import android.widget.Toast;

public class DeleteItems extends Activity
{
    private TextView mPrompt;
    private Button mButton;
    private long [] mItemList;
    private int mItemListHashCode;
    private Uri mPlaylistUri;
    private Uri mVideoUri;
    private static String DELETE_VIDEO_ITEM = "delete.video.file";

    @Override
    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        setVolumeControlStream(AudioManager.STREAM_MUSIC);

        requestWindowFeature(Window.FEATURE_NO_TITLE);
        setContentView(R.layout.confirm_delete);
        getWindow().setLayout(WindowManager.LayoutParams.WRAP_CONTENT,
                                    WindowManager.LayoutParams.WRAP_CONTENT);

        mPrompt = (TextView)findViewById(R.id.prompt);
        mButton = (Button) findViewById(R.id.delete);
        mButton.setOnClickListener(mButtonClicked);

        ((Button)findViewById(R.id.cancel)).setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                finish();
            }
        });

        Bundle b = getIntent().getExtras();
        String desc = b.getString("description");
        if (DELETE_VIDEO_ITEM.equals(desc)) {
            String videoName = b.getString("videoName");
            desc = videoName;
            mVideoUri = b.getParcelable("videoUri");
        } else {
            mPlaylistUri = b.getParcelable("Playlist");
            if (mPlaylistUri == null) {
                mItemList = b.getLongArray("items");
            }
        }
        mPrompt.setText(desc);

        // Register broadcast receiver can monitor system language change.
        IntentFilter filter = new IntentFilter(Intent.ACTION_LOCALE_CHANGED);
        registerReceiver(mLanguageChangeReceiver, filter);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        // Unregister broadcast receiver can monitor system language change.
        unregisterReceiver(mLanguageChangeReceiver);
    }

    @Override
    public void onUserLeaveHint() {
        finish();
        super.onUserLeaveHint();
    }

    // Broadcast receiver monitor system language change, if language changed
    // will finsh current activity, same as system alter dialog.
    private BroadcastReceiver mLanguageChangeReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (action.equals(Intent.ACTION_LOCALE_CHANGED)) {
                finish();
            }
        }
    };
    
    private View.OnClickListener mButtonClicked = new View.OnClickListener() {
        public void onClick(View v) {
            // delete the selected video item
            if (mVideoUri != null) {
               getContentResolver().delete(mVideoUri, null, null);
               Toast.makeText(DeleteItems.this,
                               R.string.video_deleted_message,
                               Toast.LENGTH_SHORT).show();
            } else if (mPlaylistUri != null) {
                       getContentResolver().delete(mPlaylistUri, null, null);
                       Toast.makeText(DeleteItems.this,
                               R.string.playlist_deleted_message,
                               Toast.LENGTH_SHORT).show();
            } else {
                if (mItemListHashCode == mItemList.hashCode()) {
                    return;
                }
                mItemListHashCode = mItemList.hashCode();
                // delete the selected item(s)
                final String where = getItemListString(mItemList);
                new AsyncTask<Void, Void, Void>() {
                    private int mLength;
                    private Cursor mCursor = null;

                    @Override
                    protected void onPreExecute() {
                        super.onPreExecute();
                        mCursor = MusicUtils.deleteTracksPre(DeleteItems.this, where);
                        mLength = mItemList.length;
                    }

                    @Override
                    protected Void doInBackground(Void... params) {
                        MusicUtils.deleteTracks(DeleteItems.this, mCursor, where);
                        if (mCursor != null) {
                            mCursor.close();
                        }
                        return null;
                    }

                    @Override
                    protected void onPostExecute(Void result) {
                        super.onPostExecute(result);
                        MusicUtils.deleteTracksPost(DeleteItems.this, mLength);
                        DeleteItems.this.finish();
                    }
                }.execute();
                return;
            }
            finish();
        }
    };

    private String getItemListString(long [] list) {
        StringBuilder sbWhere = new StringBuilder();
        sbWhere.append(MediaStore.Audio.Media._ID + " IN (");
        for (int i = 0; i < list.length; i++) {
            sbWhere.append(list[i]);
            if (i < list.length - 1) {
                sbWhere.append(",");
            }
        }
        sbWhere.append(")");
        return sbWhere.toString();
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (event.getAction() == KeyEvent.ACTION_UP &&
                event.getKeyCode() == KeyEvent.KEYCODE_BACK) {
            finish();
            return true;
        }
        return super.dispatchKeyEvent(event);
    };
}
