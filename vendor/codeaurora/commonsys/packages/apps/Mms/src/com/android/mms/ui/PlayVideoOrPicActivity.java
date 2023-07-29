/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

package com.android.mms.ui;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.media.MediaPlayer;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Environment;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.MimeTypeMap;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.VideoView;

import com.android.mms.LogTag;
import com.android.mms.R;
import com.android.mms.data.WorkingMessage;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

import android.util.Log;

public class PlayVideoOrPicActivity extends Activity {
    private static final String TAG = LogTag.TAG;

    public static final String VIDEO_PIC_TYPE = "type";
    public static final String VIDEO_PIC_PATH = "path";

    private static final String VIDEO_DURATION_INITIAL_STATUS = "00:00";

    private static final int POST_DELAY = 100;
    private static final int BUFFER_SIZE = 8000;

    private ImageView mImage;
    private VideoView mVideo;
    private ImageView mPlayVideoButton;
    private VideoProgressBar mProgressBar;
    private ViewGroup mVideoControllerLayout;
    private TextView mCurPlayTime;
    private TextView mVideoDuration;
    private Runnable mUpdateThread = new UpdateThread();
    private int mType;
    private String mPath;

    private boolean mHasVideoPaused;
    private int mVideoPosition;
    private int mLastSystemUiVis;
    private View mRootView;
    private boolean mFullScreen = false;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (MessageUtils.checkPermissionsIfNeeded(this)) {
            return;
        }
        setContentView(R.layout.mms_play_video_pic);
        mRootView = findViewById(R.id.mms_play_video_pic);
        mImage = (ImageView) this.findViewById(R.id.mms_play_image);
        mRootView.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View arg0) {
                if (mFullScreen) {
                    setImmersiveMode(false);
                } else {
                    setImmersiveMode(true);
                }

            }
        });
        mVideo = (VideoView) this.findViewById(R.id.mms_play_video);
        mVideoControllerLayout = (ViewGroup) findViewById(R.id.video_controller_layout);
        mProgressBar = (VideoProgressBar) this.findViewById(R.id.video_progress);
        mVideoDuration = (TextView) findViewById(R.id.video_duration);
        mCurPlayTime = (TextView) findViewById(R.id.cur_play_time);
        mCurPlayTime.setText(VIDEO_DURATION_INITIAL_STATUS);
        mPlayVideoButton = (ImageView) findViewById(R.id.mms_play_video_button);
        mPlayVideoButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mVideo.isPlaying()) {
                    pauseVideo();
                    showPlayButton();
                } else {
                    resetVideoUI();
                    playVideo();
                }
            }
        });
        getActionBar().setDisplayHomeAsUpEnabled(true);
        getActionBar().setBackgroundDrawable(new ColorDrawable(Color.BLACK));

        Intent intent = getIntent();
        mType = intent.getIntExtra(VIDEO_PIC_TYPE, WorkingMessage.UNKNOWN_ERROR);
        mPath = intent.getStringExtra(VIDEO_PIC_PATH);
        switch (mType) {
            case WorkingMessage.IMAGE:
                mVideo.setVisibility(View.GONE);
                mVideoControllerLayout.setVisibility(View.GONE);
                mPlayVideoButton.setVisibility(View.GONE);
                if (mPath != null) {
                    mImage.setImageURI(Uri.parse(mPath));
                }
                break;
            case WorkingMessage.VIDEO:
                mImage.setVisibility(View.GONE);
                mVideoControllerLayout.setVisibility(View.VISIBLE);
                mVideo.setVideoPath(mPath);
                mVideo.setOnPreparedListener(new MediaPlayer.OnPreparedListener() {
                    @Override
                    public void onPrepared(MediaPlayer mp) {
                        mVideoDuration.setText(MessageUtils.getDisplayTime(mVideo.getDuration()));
                        playVideo();
                    }
                });
                mVideo.setOnCompletionListener(new MediaPlayer.OnCompletionListener() {
                    @Override
                    public void onCompletion(MediaPlayer mp) {
                        resetVideo();
                    }
                });
                mVideo.setOnTouchListener(new View.OnTouchListener() {
                    @Override
                    public boolean onTouch(View v, MotionEvent event) {
                        if (event.getAction() == MotionEvent.ACTION_DOWN) {
                            if (mVideo.isPlaying()) {
                                showPauseButton();
                            }
                            return true;
                        }
                        return false;
                    }
                });
                break;
            default:
                finish();
                break;
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (mType == WorkingMessage.VIDEO && mHasVideoPaused) {
            mHasVideoPaused = false;
            mVideo.seekTo(mVideoPosition);
        }
    }

    private void setImmersiveMode(boolean enable) {
        int flags = 0;
        if (enable) {
            flags = View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_IMMERSIVE;

        } else {
            flags = View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN;

        }
        mRootView.setSystemUiVisibility(flags);
        mFullScreen = enable;
    }

    @Override
    public void onBackPressed() {
        if (mFullScreen) {
            setImmersiveMode(false);
        } else {
            super.onBackPressed();
        }
    }

    private void showPlayButton() {
        mPlayVideoButton.setVisibility(View.VISIBLE);
        mPlayVideoButton.setImageDrawable(getDrawable(R.drawable.video_play));
        mPlayVideoButton.postInvalidate();
    }

    private void showPauseButton() {
        mPlayVideoButton.setVisibility(View.VISIBLE);
        mPlayVideoButton.setImageDrawable(getDrawable(R.drawable.video_pause));
        mPlayVideoButton.postInvalidate();
    }

    private void hideButton() {
        mPlayVideoButton.setVisibility(View.GONE);
        mPlayVideoButton.postInvalidate();
    }

    private void playVideo() {
        mVideo.post(mUpdateThread);
        mVideo.start();
        hideButton();
    }

    private void pauseVideo() {
        mHasVideoPaused = true;
        mVideoPosition = mVideo.getCurrentPosition();
        mVideo.pause();
        showPlayButton();
    }

    private void resetVideo() {
        resetVideoUI();
        showPlayButton();
    }

    private void resetVideoUI() {
        mVideo.seekTo(0);
        mVideo.removeCallbacks(mUpdateThread);
        mProgressBar.updateLocation(0f);
        mCurPlayTime.setText(VIDEO_DURATION_INITIAL_STATUS);
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (mType == WorkingMessage.VIDEO && mVideo.isPlaying()) {
            pauseVideo();
        }
    }

    private class UpdateThread implements Runnable {

        @Override
        public void run() {
            mProgressBar.updateLocation((float)
                    (mVideo.getCurrentPosition() * 1.0 / mVideo.getDuration()));
            mVideoDuration.setText(MessageUtils.getDisplayTime(mVideo.getDuration()));
            mCurPlayTime.setText(MessageUtils.getDisplayTime(mVideo.getCurrentPosition()));
            mVideo.postDelayed(mUpdateThread, POST_DELAY);
        }
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.play_video_pic_menu, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case android.R.id.home:
                onBackPressed();
                break;
            case R.id.video_pic_menu_save:
                new SaveAttachmentTask().execute(Uri.parse(mPath));
                break;
            case R.id.video_pic_menu_share:
                Intent intent = new Intent(Intent.ACTION_SEND);
                if (mType == WorkingMessage.VIDEO) {
                    intent.setType("video/*");
                } else {
                    intent.setType("image/*");
                }
                intent.putExtra(Intent.EXTRA_STREAM, Uri.parse(mPath));
                intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
                startActivity(intent);
                break;
            default:
                break;
        }
        return true;
    }

    private class SaveAttachmentTask extends AsyncTask<Uri,Void,Uri> {

        @Override
        protected Uri doInBackground(Uri... params) {
            Uri uri = params[0];
            final File appDir = new File(Environment.getExternalStoragePublicDirectory(
                    Environment.DIRECTORY_PICTURES),
                    getResources().getString(R.string.app_label));
            String contentType = new UriImage(PlayVideoOrPicActivity.this, uri)
                    .getContentType();
            return saveContent(uri, appDir, contentType);
        }

        @Override
        protected void onPostExecute(Uri result) {
            super.onPostExecute(result);
            String message;
            if (result != null) {
                if (mType == WorkingMessage.VIDEO) {
                    message = getResources().getString(R.string.mms_video_attachment_saved_title,
                            getResources().getString(R.string.app_label));
                } else {
                    message = getResources().getString(R.string.mms_photo_attachment_saved_title,
                            getResources().getString(R.string.app_label));
                }
            } else {
                message = getResources().getString(R.string.mms_attachment_saved_fail_title);
            }
            Toast.makeText(getApplicationContext(), message, Toast.LENGTH_LONG).show();
        }
    }

    private Uri saveContent(
            final Uri sourceUri, final File outputDir, final String contentType) {
        if (!outputDir.exists() && !outputDir.mkdirs()) {
            Log.e(TAG, "Error creating " + outputDir.getAbsolutePath());
            return null;
        }

        Uri targetUri = null;
        try {
            targetUri = Uri.fromFile(createNewFile(outputDir, contentType));
        } catch (final Exception ex) {
            Log.e(TAG, "Error while create new file ", ex);
            return null;
        }

        return copy(sourceUri, targetUri) ? targetUri : null;
    }

    private synchronized File createNewFile(
            File directory, String contentType) throws IOException {
        MimeTypeMap mimeTypeMap = MimeTypeMap.getSingleton();
        String fileExtension = mimeTypeMap.getExtensionFromMimeType(contentType);
        String fileNameFormat = getResources().getString(R.string.mms_new_file_name_format);
        final Date date = new Date(System.currentTimeMillis());
        final SimpleDateFormat dateFormat = new SimpleDateFormat(fileNameFormat);
        final String fileName = dateFormat.format(date) + "." + fileExtension;
        File f = new File(directory, fileName);
        try {
            if (!f.exists()) {
                f.createNewFile();
                return f;
            }
        } catch (final IOException e) {
            Log.e(TAG, "Error creating new file in " + directory.getAbsolutePath());
        }
        return null;
    }

    private boolean copy(final Uri sourceUri, final Uri targetUri) {
        FileInputStream fin = null;
        FileOutputStream fout = null;
        try {
            fin = (FileInputStream) getContentResolver().openInputStream(sourceUri);
            fout = (FileOutputStream) getContentResolver().openOutputStream(targetUri);
            byte[] buffer = new byte[BUFFER_SIZE];
            int size = 0;
            while ((size = fin.read(buffer)) != -1) {
                fout.write(buffer, 0, size);
            }
            // Notify other applications listening to scanner events
            // that a media file has been added to the sd card
            sendBroadcast(new Intent(Intent.ACTION_MEDIA_SCANNER_SCAN_FILE, targetUri));
        } catch (final Exception ex) {
            Log.e(TAG, "Error while copying content ", ex);
            return false;
        } finally {
            if (fin != null) {
                try {
                    fin.close();
                } catch (final IOException e) {
                    Log.e(TAG, "IOException caught while closing stream", e);
                }
            }
            if (fout != null) {
                try {
                    fout.close();
                } catch (IOException e) {
                    Log.e(TAG, "IOException caught while closing stream", e);
                }
            }
        }
        return true;
    }
}
