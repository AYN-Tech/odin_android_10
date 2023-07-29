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

package com.android.mms.ui;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.IOException;
import java.util.Map;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.media.MediaPlayer;
import android.net.Uri;
import android.os.Environment;
import android.text.TextUtils;
import android.text.method.HideReturnsTransformationMethod;
import android.util.AttributeSet;
import android.util.Log;
import android.view.View;
import android.webkit.MimeTypeMap;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import com.android.mms.LogTag;
import com.android.mms.R;
import com.google.android.mms.ContentType;
import com.android.mms.model.LayoutModel;

/**
 * A simplified view of slide in the slides list.
 */
public class SlideListItemView extends LinearLayout implements SlideViewInterface {
    private static final String TAG = LogTag.TAG;
    private static final String FILE_DIR = Environment.getExternalStorageDirectory() + "/"
            + Environment.DIRECTORY_DOWNLOADS + "/";
    private static final int VIEW_OPTION = 0;
    private static final int SAVE_OPTION = 1;
    private static final int BUFFER_SIZE = 8 * 1024;
    private static final int INCREMENT_NUMBER = 2;
    private static final String CONTACTS = "contacts";
    private static final String COLUMN_CONTENT_TYPE = "ct";

    private TextView mTextPreview;
    private ImageView mImagePreview;
    private TextView mAttachmentName;
    private ImageView mAttachmentIcon;
    private Uri mImageUri;

    public SlideListItemView(Context context) {
        super(context);
    }

    public SlideListItemView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        mTextPreview = (TextView) findViewById(R.id.text_preview_bottom);
        mTextPreview.setTransformationMethod(HideReturnsTransformationMethod.getInstance());
        mImagePreview = (ImageView) findViewById(R.id.image_preview);
        mAttachmentName = (TextView) findViewById(R.id.attachment_name);
        mAttachmentIcon = (ImageView) findViewById(R.id.attachment_icon);
    }

    public void startAudio() {
        // Playing audio is not needed in this view.
    }

    public void startVideo() {
        // Playing audio is not needed in this view.
    }

    public void setAudio(Uri audio, String name, Map<String, ?> extras) {
        if (name != null) {
            mAttachmentName.setText(name);
            if (mContext instanceof MobilePaperShowActivity) {
                mAttachmentIcon.setImageResource(R.drawable.ic_attach_capture_audio_holo_light);
                setOnClickListener(new ViewAttachmentListener(audio, name, false));
            } else {
                mAttachmentIcon.setImageResource(R.drawable.ic_mms_music);
            }
        } else {
            mAttachmentName.setText("");
            mAttachmentIcon.setImageDrawable(null);
        }
    }

    public void setImage(String name, Bitmap bitmap) {
        try {
            if (null == bitmap) {
                bitmap = BitmapFactory.decodeResource(getResources(),
                        R.drawable.ic_missing_thumbnail_picture);
            }

            mImagePreview.setImageBitmap(bitmap);
            if (mContext instanceof MobilePaperShowActivity && mImageUri != null) {
                mImagePreview.setVisibility(View.VISIBLE);
                mImagePreview.setOnClickListener(
                        new ViewAttachmentListener(mImageUri, name, false));
            }
        } catch (java.lang.OutOfMemoryError e) {
            Log.e(TAG, "setImage: out of memory: ", e);
        }
    }

     class ViewAttachmentListener implements OnClickListener {
        private final Uri mAttachmentUri;
        private final String mAttachmentName;
        private final boolean mImportVcard;
        private final Uri mLookupUri;

        public ViewAttachmentListener(Uri uri, String name, boolean vcard) {
            mAttachmentUri = uri;
            mAttachmentName = name;
            mImportVcard = vcard;
            mLookupUri = null;
        }

        public ViewAttachmentListener(Uri uri, String name, boolean vcard, Uri lookup) {
            mAttachmentUri = uri;
            mAttachmentName = name;
            mImportVcard = vcard;
            mLookupUri = lookup;
        }

        @Override
        public void onClick(View v) {
            if (mAttachmentUri != null && !TextUtils.isEmpty(mAttachmentName)) {
                String[] options = new String[] {
                        mContext.getString(mImportVcard ? R.string.import_vcard : R.string.view),
                        mContext.getString(R.string.save)
                };
                DialogInterface.OnClickListener clickListener = new DialogInterface
                        .OnClickListener() {
                    @Override
                    public final void onClick(DialogInterface dialog, int which) {
                        if (which == VIEW_OPTION) {
                            try {
                                Intent intent = new Intent(Intent.ACTION_VIEW);
                                intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
                                if (mImportVcard) {
                                    intent.setDataAndType(mAttachmentUri,
                                            ContentType.TEXT_VCARD.toLowerCase());
                                    intent.putExtra(MessageUtils.VIEW_VCARD, true);
                                } else {
                                    if (mLookupUri != null) {
                                        intent.setData(mLookupUri);
                                    } else {
                                        intent.setData(mAttachmentUri);
                                    }
                                }
                                mContext.startActivity(intent);
                            } catch (Exception e) {
                                Log.e(TAG, "Can't open " + mAttachmentUri);
                            }
                        } else if (which == SAVE_OPTION) {
                            if (MessageUtils.hasStoragePermission()) {
                                save();
                            } else {
                                if (mContext instanceof MobilePaperShowActivity) {
                                    MobilePaperShowActivity activity = (MobilePaperShowActivity)mContext;
                                    activity.setViewAttachmentListener(ViewAttachmentListener.this);
                                    activity.requestPermissions(
                                            new String[] {
                                                Manifest.permission.WRITE_EXTERNAL_STORAGE
                                            },
                                            MobilePaperShowActivity.SAVE_SLIDER_ATTACHMENT_PERMISSION_REQUEST_CODE);
                                }
                            }
                        }
                        dialog.dismiss();
                    }
                };
                new AlertDialog.Builder(mContext)
                        .setTitle(R.string.select_link_title)
                        .setCancelable(true)
                        .setItems(options, clickListener)
                        .show();
            }
        }
        public void save(){
            int resId = R.string.copy_to_sdcard_fail;
            Uri saveUri = saveAttachment(mAttachmentUri, mAttachmentName);
            if (saveUri != null) {
                resId = R.string.copy_to_sdcard_success;
                mContext.sendBroadcast(new Intent(
                        Intent.ACTION_MEDIA_SCANNER_SCAN_FILE, saveUri));
            }
            Toast.makeText(mContext, resId, Toast.LENGTH_SHORT).show();
        }
    }

    private Uri saveAttachment(Uri attachmentUri, String attachmentName) {
        InputStream input = null;
        FileOutputStream fout = null;
        Uri saveUri = null;
        try {
            input = mContext.getContentResolver().openInputStream(attachmentUri);
            if (input instanceof FileInputStream) {
                File saveFile = createSaveFile(attachmentUri, attachmentName);
                File parentFile = saveFile.getParentFile();
                if (!parentFile.exists() && !parentFile.mkdirs()) {
                    Log.e(TAG, "[MMS] copyPart: mkdirs for " + parentFile.getPath() + " failed!");
                    return null;
                }

                FileInputStream fin = (FileInputStream) input;
                fout = new FileOutputStream(saveFile);
                byte[] buffer = new byte[BUFFER_SIZE];
                int size = 0;
                while ((size = fin.read(buffer)) != -1) {
                    fout.write(buffer, 0, size);
                }
                saveUri = Uri.fromFile(saveFile);
            }
        } catch (Exception e) {
            Log.e(TAG, "Exception caught while save attachment: ", e);
            return null;
        } finally {
            if (null != input) {
                try {
                    input.close();
                } catch (Exception e) {
                    Log.e(TAG, "Exception caught while closing input: ", e);
                    return null;
                }
            }
            if (null != fout) {
                try {
                    fout.close();
                } catch (Exception e) {
                    Log.e(TAG, "Exception caught while closing output: ", e);
                    return null;
                }
            }
        }
        return saveUri;
    }

    private File createSaveFile(Uri attachmentUri, String attachmentName) {
        String fileName = new File(attachmentName).getName();
        String extension = "";
        int index;
        if ((index = fileName.lastIndexOf('.')) != -1) {
            extension = fileName.substring(index + 1, fileName.length());
            fileName = fileName.substring(0, index);
        } else {
            Cursor cursor = mContext.getContentResolver().query(attachmentUri, null,
                    null, null, null);
            try {
                if (cursor != null && cursor.moveToFirst()) {
                    String type = cursor.getString(cursor
                            .getColumnIndexOrThrow(COLUMN_CONTENT_TYPE));
                    extension = MimeTypeMap.getSingleton()
                            .getExtensionFromMimeType(type);
                }
            } finally {
                if (cursor != null) {
                    cursor.close();
                }
            }
        }
        fileName = fileName.replaceAll("^\\.", "");

        File file = new File(FILE_DIR + fileName + "." + extension);
        // Add incrementing number after file name if have existed
        // the file has same name.
        for (int i = INCREMENT_NUMBER; file.exists(); i++) {
            file = new File(FILE_DIR + fileName + "_" + i + "." + extension);
        }
        return file;
    }

    public void setUri(Uri uri) {
        mImageUri= uri;
    }

    public void setImageRegionFit(String fit) {
        // TODO Auto-generated method stub
    }

    public void setImageVisibility(boolean visible) {
        // TODO Auto-generated method stub
    }

    public void setText(String name, String text) {
        mTextPreview.setText(text);
        mTextPreview.setVisibility(TextUtils.isEmpty(text) ? View.GONE : View.VISIBLE);
    }

    public void setTextVisibility(boolean visible) {
        // TODO Auto-generated method stub
    }

    public void setVideo(String name, Uri video) {
        mAttachmentName.setText(name);
        if (name != null) {
            if (mContext instanceof MobilePaperShowActivity) {
                setOnClickListener(new ViewAttachmentListener(video, name, false));
                mAttachmentIcon.setImageResource(R.drawable.ic_menu_movie);
            } else {
                mAttachmentIcon.setImageResource(R.drawable.movie);
            }
        } else {
            mAttachmentName.setText("");
            mAttachmentIcon.setImageDrawable(null);
        }

        // TODO: get a thumbnail from the video
        mImagePreview.setImageBitmap(null);
    }

    public void setVideoThumbnail(String name, Bitmap thumbnail) {
        try {
            mImagePreview.setImageBitmap(thumbnail);
            mImagePreview.setVisibility(VISIBLE);
        } catch (java.lang.OutOfMemoryError e) {
            Log.e(TAG, "setVideo: out of memory: ", e);
        }
    }

    public void setVideoVisibility(boolean visible) {
        // TODO Auto-generated method stub
    }

    public void stopAudio() {
        // Stopping audio is not needed in this view.
    }

    public void stopVideo() {
        // Stopping video is not needed in this view.
    }

    public void reset() {
        // TODO Auto-generated method stub
    }

    public void setVisibility(boolean visible) {
        // TODO Auto-generated method stub
    }

    public void pauseAudio() {
        // TODO Auto-generated method stub

    }

    public void pauseVideo() {
        // TODO Auto-generated method stub

    }

    public void seekAudio(int seekTo) {
        // TODO Auto-generated method stub

    }

    public void seekVideo(int seekTo) {
        // TODO Auto-generated method stub

    }

    @Override
    public void setVcard(Uri lookupUri, String name) {
    }

    public void setVcard(Uri uri, String lookupUri, String name) {
        if (name != null) {
            mAttachmentName.setText(name);
            if (mContext instanceof MobilePaperShowActivity) {
                mAttachmentIcon.setImageResource(R.drawable.ic_attach_vcard);
                Uri attUri = uri;
                Uri lookup = null;
                // If vCard uri is not from contacts, we need import this vCard
                boolean needImport = !(lookupUri != null && lookupUri.contains(CONTACTS));
                if (!needImport) {
                    lookup = Uri.parse(lookupUri);
                }
                ViewAttachmentListener l =
                        new ViewAttachmentListener(attUri, name, needImport, lookup);
                setOnClickListener(l);
            }
        }
    }

    public void setLayoutModel(int model) {
        if (model == LayoutModel.LAYOUT_TOP_TEXT) {
            mTextPreview = (TextView) findViewById(R.id.text_preview_top);
        } else {
            mTextPreview = (TextView) findViewById(R.id.text_preview_bottom);
        }
        mTextPreview.setTransformationMethod(HideReturnsTransformationMethod.getInstance());
    }

    public TextView getContentText() {
        return mTextPreview;
    }
}
