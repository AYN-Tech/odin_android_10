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
import android.app.AlertDialog;
import android.app.AlertDialog.Builder;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;
import android.database.Cursor;
import android.media.AudioManager;
import android.os.Bundle;
import android.provider.MediaStore;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.Log;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;
import android.view.KeyEvent;
import android.content.Intent;
import android.net.Uri;

public class RenamePlaylist extends Activity
{
    private EditText mPlaylist;
    private TextView mPrompt;
    private Button mSaveButton;
    private long mRenameId;
    private String mOriginalName;

    @Override
    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        setVolumeControlStream(AudioManager.STREAM_MUSIC);

        requestWindowFeature(Window.FEATURE_NO_TITLE);
        setContentView(R.layout.create_playlist);
        getWindow().setLayout(WindowManager.LayoutParams.MATCH_PARENT,
                                    WindowManager.LayoutParams.WRAP_CONTENT);

        mPrompt = (TextView)findViewById(R.id.prompt);
        mPlaylist = (EditText)findViewById(R.id.playlist);
        mSaveButton = (Button) findViewById(R.id.create);
        mSaveButton.setText(R.string.button_ok);
        mSaveButton.setOnClickListener(mOpenClicked);

        ((Button)findViewById(R.id.cancel)).setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                finish();
            }
        });

        mRenameId = icicle != null ? icicle.getLong("rename")
                : getIntent().getLongExtra("rename", -1);
        mOriginalName = nameForId(mRenameId);
        String defaultname = icicle != null ? icicle.getString("defaultname") : mOriginalName;
        
        if (mRenameId < 0 || mOriginalName == null || defaultname == null) {
            Log.i("@@@@", "Rename failed: " + mRenameId + "/" + defaultname);
            finish();
            return;
        }
        
        String promptformat;
        if (mOriginalName.equals(defaultname)) {
            promptformat = getString(R.string.rename_playlist_same_prompt);
        } else {
            promptformat = getString(R.string.rename_playlist_diff_prompt);
        }
                
        String prompt = String.format(promptformat, mOriginalName, defaultname);
        mPrompt.setText(prompt);
        mPlaylist.setText(defaultname);
        mPlaylist.setSelection(defaultname.length());
        mPlaylist.addTextChangedListener(mTextWatcher);
        setSaveButton();
    }
    
    TextWatcher mTextWatcher = new TextWatcher() {
        public void beforeTextChanged(CharSequence s, int start, int count, int after) {
            // don't care about this one
        }
        public void onTextChanged(CharSequence s, int start, int before, int count) {
            // check if playlist with current name exists already, and warn the user if so.
            setSaveButton();
        };
        public void afterTextChanged(Editable s) {
            // don't care about this one
        }
    };
    
    private void setSaveButton() {
        String typedname = mPlaylist.getText().toString();
        if (typedname.trim().length() == 0) {
            mSaveButton.setEnabled(false);
        } else {
            mSaveButton.setEnabled(true);
        }

    }
    
    private int idForplaylist(String name) {
        Cursor c = MusicUtils.query(this, MediaStore.Audio.Playlists.EXTERNAL_CONTENT_URI,
                new String[] { MediaStore.Audio.Playlists._ID },
                MediaStore.Audio.Playlists.NAME + "=?",
                new String[] { name },
                MediaStore.Audio.Playlists.NAME);
        int id = -1;
        if (c != null) {
            c.moveToFirst();
            if (!c.isAfterLast()) {
                id = c.getInt(0);
            }
        }
        c.close();
        return id;
    }
    
    private String nameForId(long id) {
        Cursor c = MusicUtils.query(this, MediaStore.Audio.Playlists.EXTERNAL_CONTENT_URI,
                new String[] { MediaStore.Audio.Playlists.NAME },
                MediaStore.Audio.Playlists._ID + "=?",
                new String[] { Long.valueOf(id).toString() },
                MediaStore.Audio.Playlists.NAME);
        String name = null;
        if (c != null) {
            c.moveToFirst();
            if (!c.isAfterLast()) {
                name = c.getString(0);
                if (name.equals("My recordings")) {
                    name =  getResources().getString(R.string.audio_db_playlist_name);
                }
            }
        }
        c.close();
        return name;
    }
    
    
    @Override
    public void onSaveInstanceState(Bundle outcicle) {
        outcicle.putString("defaultname", mPlaylist.getText().toString());
        outcicle.putLong("rename", mRenameId);
    }
    
    @Override
    public void onResume() {
        super.onResume();
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (event.getAction() == KeyEvent.ACTION_UP &&
                event.getKeyCode() == KeyEvent.KEYCODE_BACK) {
            finish();
            return true;
        }
        return super.dispatchKeyEvent(event);
    }

    private View.OnClickListener mOpenClicked = new View.OnClickListener() {
        public void onClick(View v) {
            String name = mPlaylist.getText().toString();
            if (name != null && name.length() > 0) {
                if ((idForplaylist(name) >= 0) && (!mOriginalName.equals(name))) {
                    new AlertDialog.Builder(RenamePlaylist.this).setMessage(
                            getString(R.string.duplicate_playlist_name_alert, name))
                            .setPositiveButton(getString(R.string.button_ok), new CancelListener())
                            .show();
                } else {
                    if (mOriginalName.equals(name)) {
                        new AlertDialog.Builder(RenamePlaylist.this).setMessage(
                                getString(R.string.same_playlist_name_alert, name))
                                .setPositiveButton(getString(R.string.button_ok), new CancelListener())
                                .show();
                        return;
                    }
                    ContentResolver resolver = getContentResolver();
                    ContentValues values = new ContentValues(2);
                    values.put(MediaStore.Audio.Playlists.NAME, name);
                    values.put(MediaStore.Audio.Playlists.DATA, MediaStore.Audio.Playlists.EXTERNAL_CONTENT_URI + "/" + name);
                    resolver.update(MediaStore.Audio.Playlists.EXTERNAL_CONTENT_URI, values,
                            MediaStore.Audio.Playlists._ID + "=?", new String[] {
                                Long.valueOf(mRenameId).toString()
                            });

                    final Intent shortcut = new Intent();
                    shortcut.setAction(Intent.ACTION_VIEW);
                    shortcut.setDataAndType(Uri.EMPTY, "vnd.android.cursor.dir/playlist");
                    shortcut.putExtra("playlist", String.valueOf(mRenameId));

                    final Intent intent = new Intent();
                    intent.setAction("com.android.launcher.action.UPDATE_SHORTCUT");
                    intent.putExtra(Intent.EXTRA_SHORTCUT_INTENT, shortcut);
                    intent.putExtra("com.android.launcher.extra.shortcut.NEWNAME", name);
                    intent.putExtra(Intent.EXTRA_SHORTCUT_NAME, mOriginalName);
                    intent.putExtra(Intent.EXTRA_SHORTCUT_ICON_RESOURCE,
                            Intent.ShortcutIconResource.fromContext(RenamePlaylist.this,
                                    R.drawable.ic_launcher_shortcut_music_playlist));

                    setResult(RESULT_OK);
                    sendBroadcast(intent);
                    Toast.makeText(RenamePlaylist.this, R.string.playlist_renamed_message,
                            Toast.LENGTH_SHORT).show();
                    finish();
                }
            }
        }
    };

    private class CancelListener implements OnClickListener {
        public void onClick(DialogInterface dialog, int whichButton) {
            dialog.dismiss();
        }
    }
}
