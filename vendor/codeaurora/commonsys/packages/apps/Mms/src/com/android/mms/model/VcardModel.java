/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
       * Redistributions of source code must retain the above copyright
         notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
         copyright notice, this list of conditions and the following
         disclaimer in the documentation and/or other materials provided
         with the distribution.
       * Neither the name of The Linux Foundation nor the names of its
         contributors may be used to endorse or promote products derived
         from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
   ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
   BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
   BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
   OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
   IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

package com.android.mms.model;

import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.provider.ContactsContract;
import android.provider.DocumentsContract.Document;
import android.provider.Telephony.Mms.Part;
import android.text.TextUtils;
import android.util.Log;

import com.google.android.mms.ContentType;
import com.google.android.mms.MmsException;

import org.w3c.dom.events.Event;

import java.text.SimpleDateFormat;
import java.util.Date;

public class VcardModel extends MediaModel {
    private static final String TAG = MediaModel.TAG;

    private String mLookupUri = null;
    private String DOCUMENTS_AUTHORITY = "com.android.providers.downloads.documents";

    public VcardModel(Context context, Uri uri) throws MmsException {
        this(context, ContentType.TEXT_VCARD, null, uri);
        initModelFromUri(uri);
        mSrc = reSetAttachmentName();
    }

    public VcardModel(Context context, String contentType, String src, Uri uri)
            throws MmsException {
        super(context, SmilHelper.ELEMENT_TAG_REF, contentType, src, uri);
        if (!TextUtils.isEmpty(src)) {
            initLookupUri(uri);
        }
    }

    private void initModelFromUri(Uri uri) throws MmsException {
        String scheme = uri.getScheme();
        if (scheme == null) {
            Log.e(TAG, "The uri's scheme is null.");
            return;
        }

        if (isFileUri(uri)) {
            mSrc = uri.getLastPathSegment();
        } else if (TextUtils.equals(ContentResolver.SCHEME_CONTENT, scheme)){
            Cursor c = null;
            try {
                if (DOCUMENTS_AUTHORITY.equals(uri.getAuthority())) {
                    c = mContext.getContentResolver().query(uri, null, null, null, null);
                    if (c != null && c.moveToFirst()) {
                        mSrc = c.getString(c.getColumnIndex(Document.COLUMN_DISPLAY_NAME));
                    }
                } else {
                    c = getContentCursor(uri);
                    mLookupUri = getLookupUri(uri, c);
                    if ((uri.getPath() != null) && (uri.getPath().contains("as_multi_vcard"))) {
                        mSrc = createTempSrcForVcf();
                    } else {
                        mSrc = getLookupSrc(uri, c);
                    }
                }
            } finally {
                if (c != null) {
                    c.close();
                    c = null;
                } else {
                    throw new MmsException("Bad URI: " + uri);
                }
            }
        }
        initMediaDuration();
    }

    private String createTempSrcForVcf(){
        SimpleDateFormat sf = new SimpleDateFormat("yyyyMMddhhmmss");
        long temp = System.currentTimeMillis();
        Date date = new Date(temp);
        String dateStr = sf.format(date);
        return dateStr + ".vcf";
    }

    private Cursor getContentCursor(Uri uri) {
        if(isMmsUri(uri)) {
            return mContext.getContentResolver().query(uri, null, null, null, null);
        }
        return mContext.getContentResolver()
                .query(getExtraLookupUri(uri), null, null, null, null);
    }

    private String getLookupUri(Uri uri, Cursor c) {
        if(isMmsUri(uri))
            return getMmsLookupUri(uri,c);
        return getExtraLookupUri(uri).toString();
    }

    private String getMmsLookupUri(Uri uri, Cursor c) {
        if (c != null && c.moveToFirst())
            return c.getString(c.getColumnIndexOrThrow(Part.CONTENT_DISPOSITION));
        return "";
    }

    private Uri getExtraLookupUri(Uri uri) {
        String lookup = uri.getLastPathSegment();
        Uri lookupUri =
            Uri.withAppendedPath(ContactsContract.Contacts.CONTENT_LOOKUP_URI, lookup);
        return lookupUri;
    }

    private String getLookupSrc(Uri uri, Cursor c) throws MmsException {
        if(isMmsUri(uri))
            return getMmsLookupSrc(c);
        return getExtraSrc(uri,c);
    }

    private String getMmsLookupSrc(Cursor c) {
        if (c != null && c.moveToFirst()) {
            String path = c.getString(c.getColumnIndexOrThrow(Part._DATA));
            return path.substring(path.lastIndexOf('/') + 1);
        }
        return "";
    }

    private String getExtraSrc(Uri uri, Cursor c) throws MmsException {
        if (c != null && c.getCount() == 1 && c.moveToFirst()) {
            String displayName = c.getString(c.getColumnIndexOrThrow(ContactsContract
                    .Contacts.DISPLAY_NAME));
            return displayName + ".vcf";
        } else {
            throw new MmsException("Type of media is unknown.");
        }
    }

    private void initLookupUri(Uri uri) {
        if (isMmsUri(uri)) {
            ContentResolver cr = mContext.getContentResolver();
            Cursor c = cr.query(uri, null, null, null, null);
            try {
                if (c != null && c.moveToFirst()) {
                    mLookupUri = c.getString(c.getColumnIndexOrThrow(Part.CONTENT_DISPOSITION));
                }
            } finally {
                if (c != null) {
                    c.close();
                    c = null;
                }
            }
        }
    }

    public String getLookupUri() {
        return mLookupUri;
    }

    @Override
    public void handleEvent(Event evt) {
    }

    @Override
    protected boolean isPlayable() {
        return false;
    }

    @Override
    protected void initMediaDuration() throws MmsException {
        mDuration = 0;
    }

}
