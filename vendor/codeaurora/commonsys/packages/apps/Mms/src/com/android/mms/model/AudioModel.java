/*
 * Copyright (C) 2008 Esmertec AG.
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

package com.android.mms.model;

import java.util.HashMap;
import java.util.Map;

import org.w3c.dom.events.Event;

import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SqliteWrapper;
import android.net.Uri;
import android.provider.DocumentsContract.Document;
import android.provider.MediaStore.Audio;
import android.provider.Telephony.Mms.Part;
import android.text.TextUtils;
import android.util.Log;
import android.webkit.MimeTypeMap;

import com.android.mms.ContentRestrictionException;
import com.android.mms.dom.events.EventImpl;
import com.android.mms.dom.smil.SmilMediaElementImpl;
import com.google.android.mms.MmsException;

public class AudioModel extends MediaModel {
    private static final String TAG = MediaModel.TAG;
    private static final boolean DEBUG = false;
    private static final boolean LOCAL_LOGV = false;

    private final HashMap<String, String> mExtras;

    public AudioModel(Context context, Uri uri) throws MmsException {
        this(context, null, null, uri);
        initModelFromUri(uri);
        mSrc = reSetAttachmentName();
        checkContentRestriction();
    }

    public AudioModel(Context context, String contentType, String src, Uri uri) throws MmsException {
        super(context, SmilHelper.ELEMENT_TAG_AUDIO, contentType, src, uri);
        mExtras = new HashMap<String, String>();
    }

    private void initModelFromUri(Uri uri) throws MmsException {
        ContentResolver cr = mContext.getContentResolver();
        Cursor c = null;
        boolean initFromFile = false;
        Log.d(TAG, "Audio uri:" + uri);
        if (isFileUri(uri)) {
            initFromFile = true;
            c = cr.query(Audio.Media.EXTERNAL_CONTENT_URI, null,
                    Audio.Media.DATA + "=?", new String[] { uri.getPath() }, null);
        } else {
            c = SqliteWrapper.query(mContext, cr, uri, null, null, null, null);
        }
        if (c != null) {
            try {
                if (c.moveToFirst()) {
                    String path;
                    boolean isFromMms = isMmsUri(uri);

                    // FIXME We suppose that there should be only two sources
                    // of the audio, one is the media store, the other is
                    // our MMS database.
                    if (isFromMms) {
                        path = c.getString(c.getColumnIndexOrThrow(Part._DATA));
                        mContentType = c.getString(c.getColumnIndexOrThrow(Part.CONTENT_TYPE));
                        mSrc = path.substring(path.lastIndexOf('/') + 1);
                    } else {
                        mSrc = c.getString(c.getColumnIndexOrThrow(Document.COLUMN_DISPLAY_NAME));
                        mContentType = cr.getType(uri);
                        if (TextUtils.isEmpty(mContentType)) {
                            try {
                                mContentType = c.getString(c.getColumnIndexOrThrow(
                                        Document.COLUMN_MIME_TYPE));
                            } catch (IllegalArgumentException ex) {
                                Log.e(TAG, "initFromFile: cannot fetch mime type");
                            }
                        }
                        // Get more extras information which would be useful
                        // to the user.
                        if (initFromFile) {
                            try {
                                String album = c.getString(c.getColumnIndexOrThrow("album"));
                                if (!TextUtils.isEmpty(album)) {
                                    mExtras.put("album", album);
                                }

                                String artist = c.getString(c.getColumnIndexOrThrow("artist"));
                                if (!TextUtils.isEmpty(artist)) {
                                    mExtras.put("artist", artist);
                                }
                            }  catch (IllegalArgumentException e) {
                            }
                        }
                    }
                    if (TextUtils.isEmpty(mContentType)) {
                        throw new MmsException("Type of media is unknown.");
                    }

                    if (LOCAL_LOGV) {
                        Log.v(TAG, "New AudioModel created:"
                                + " mSrc=" + mSrc
                                + " mContentType=" + mContentType
                                + " mUri=" + uri
                                + " mExtras=" + mExtras);
                    }
                } else if (initFromFile) {
                    // The audio file on sdcard but hasn't been scanned into media database.
                    mContentType = MimeTypeMap.getSingleton().getMimeTypeFromExtension(
                            getExtension(uri));
                    // if mContentType is null , it will cause a RuntimeException, to avoid
                    // this, set it as empty.
                    if (null == mContentType) {
                        Log.e(TAG, "initFromFile: mContentType is null!");
                        mContentType = "";
                    }
                    mExtras.put("album", "sdcard");
                    mExtras.put("artist", "<unknown>");
                } else {
                    throw new MmsException("Nothing found: " + uri);
                }
            } finally {
                c.close();
            }
        } else {
            throw new MmsException("Bad URI: " + uri);
        }

        initMediaDuration();
    }

    private String getExtension(Uri uri) {
        String path = uri.getPath();
        mSrc = path.substring(path.lastIndexOf('/') + 1);
        String extension = MimeTypeMap.getFileExtensionFromUrl(mSrc);
        if (TextUtils.isEmpty(extension)) {
            // getMimeTypeFromExtension() doesn't handle spaces in filenames
            // nor can it handle urlEncoded strings. Let's try one last time
            // at finding the extension.
            int dotPos = mSrc.lastIndexOf('.');
            if (0 <= dotPos) {
                extension = mSrc.substring(dotPos + 1);
            }
        }
        return extension;
    }

    public void stop() {
        appendAction(MediaAction.STOP);
        notifyModelChanged(false);
    }

    public void handleEvent(Event evt) {
        String evtType = evt.getType();
        if (LOCAL_LOGV) {
            Log.v(TAG, "Handling event: " + evtType + " on " + this);
        }

        MediaAction action = MediaAction.NO_ACTIVE_ACTION;
        if (evtType.equals(SmilMediaElementImpl.SMIL_MEDIA_START_EVENT)) {
            action = MediaAction.START;
            // if the Music player app is playing audio, we should pause that so it won't
            // interfere with us playing audio here.
            pauseMusicPlayer();
        } else if (evtType.equals(SmilMediaElementImpl.SMIL_MEDIA_END_EVENT)) {
            action = MediaAction.STOP;
        } else if (evtType.equals(SmilMediaElementImpl.SMIL_MEDIA_PAUSE_EVENT)) {
            action = MediaAction.PAUSE;
        } else if (evtType.equals(SmilMediaElementImpl.SMIL_MEDIA_SEEK_EVENT)) {
            action = MediaAction.SEEK;
            mSeekTo = ((EventImpl) evt).getSeekTo();
        }

        appendAction(action);
        notifyModelChanged(false);
    }

    public Map<String, ?> getExtras() {
        return mExtras;
    }

    protected void checkContentRestriction() throws ContentRestrictionException {
        ContentRestriction cr = ContentRestrictionFactory.getContentRestriction();
        cr.checkAudioContentType(mContentType);
    }

    @Override
    protected boolean isPlayable() {
        return true;
    }
}
