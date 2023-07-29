/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.

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

package com.android.soundrecorder.filelist.listitem;

import android.database.Cursor;
import android.provider.MediaStore;
import android.util.Log;

public class MediaItem extends BaseListItem {
    static final String TAG = "MediaItem";
    private long mDateModified;
    private long mDuration;
    public enum PlayStatus {
        NONE, PLAYING, PAUSE
    }
    private PlayStatus mPlayStatus = PlayStatus.NONE;

    public interface ItemPlayStatusListener {
        void onPlayStatusChanged(MediaItem.PlayStatus status);
    }

    protected ItemPlayStatusListener mPlayStatusListener;

    public void setItemPlayStatusListener(ItemPlayStatusListener listener) {
        mPlayStatusListener = listener;
    }

    public MediaItem(Cursor cursor) {
        int idIndex = cursor.getColumnIndexOrThrow(MediaStore.Audio.Media._ID);
        int dataIndex = cursor.getColumnIndexOrThrow(MediaStore.Audio.Media.DATA);
        int titleIndex = cursor.getColumnIndexOrThrow(MediaStore.Audio.Media.DISPLAY_NAME);
        int durationIndex = cursor.getColumnIndexOrThrow(MediaStore.Audio.Media.DURATION);
        int modifiedIndex = cursor.getColumnIndexOrThrow(MediaStore.Audio.Media.DATE_MODIFIED);
        mId = cursor.getLong(idIndex);
        mPath = cursor.getString(dataIndex);
        mTitle = cursor.getString(titleIndex);
        mDuration = cursor.getLong(durationIndex);
        mDateModified = cursor.getLong(modifiedIndex) * 1000; // second to millisecond
        setItemType(BaseListItem.TYPE_MEDIA_ITEM);
        setSelectable(true);
        setSupportedOperation(SUPPORT_ALL);
    }

    public long getDateModified() {
        return mDateModified;
    }

    public long getDuration() {
        return mDuration;
    }

    public PlayStatus getPlayStatus() {
        return mPlayStatus;
    }

    public void setPlayStatus(PlayStatus status) {
        if (mPlayStatus != status) {
            mPlayStatus = status;
            if (mPlayStatusListener != null) {
                mPlayStatusListener.onPlayStatusChanged(status);
            }
        }
    }

    @Override
    public void copyFrom(BaseListItem item) {
        super.copyFrom(item);
        if (item instanceof MediaItem) {
            mDateModified = ((MediaItem) item).getDateModified();
            mDuration = ((MediaItem) item).getDuration();
            setPlayStatus(((MediaItem) item).getPlayStatus());
        }
    }
}
