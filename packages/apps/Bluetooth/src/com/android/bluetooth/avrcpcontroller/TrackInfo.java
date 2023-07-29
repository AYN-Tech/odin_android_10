/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.bluetooth.avrcpcontroller;

import android.media.MediaMetadata;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.media.MediaMetadata;
import android.net.Uri;
import android.util.Log;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

final class TrackInfo {

    private static final String TAG = "AvrcpTrackInfo";
    private static final boolean VDBG = AvrcpControllerService.VDBG;
    /*
     * Default values for each of the items from JNI
     */
    private static final int TRACK_NUM_INVALID = -1;
    private static final int TOTAL_TRACKS_INVALID = -1;
    private static final int TOTAL_TRACK_TIME_INVALID = -1;
    private static final String UNPOPULATED_ATTRIBUTE = "";

    /*
     *Element Id Values for GetMetaData  from JNI
     */
    private static final int MEDIA_ATTRIBUTE_TITLE = 0x01;
    private static final int MEDIA_ATTRIBUTE_ARTIST_NAME = 0x02;
    private static final int MEDIA_ATTRIBUTE_ALBUM_NAME = 0x03;
    private static final int MEDIA_ATTRIBUTE_TRACK_NUMBER = 0x04;
    private static final int MEDIA_ATTRIBUTE_TOTAL_TRACK_NUMBER = 0x05;
    private static final int MEDIA_ATTRIBUTE_GENRE = 0x06;
    private static final int MEDIA_ATTRIBUTE_PLAYING_TIME = 0x07;
    private static final int MEDIA_ATTRIBUTE_COVER_ART_HANDLE = 0x08;

    private final String mArtistName;
    private final String mTrackTitle;
    private final String mAlbumTitle;
    private final String mGenre;
    private final long mTrackNum; // number of audio file on original recording.
    private final long mTotalTracks; // total number of tracks on original recording
    private final long mTrackLen; // full length of AudioFile.
    private String mCoverArtHandle;
    private String mImageLocation;
    private String mThumbNailLocation;


    TrackInfo() {
        this(new ArrayList<Integer>(), new ArrayList<String>());
    }

    TrackInfo(List<Integer> attrIds, List<String> attrMap) {
        Map<Integer, String> attributeMap = new HashMap<>();
        for (int i = 0; i < attrIds.size(); i++) {
            attributeMap.put(attrIds.get(i), attrMap.get(i));
        }

        String attribute;
        mTrackTitle = attributeMap.getOrDefault(MEDIA_ATTRIBUTE_TITLE, UNPOPULATED_ATTRIBUTE);

        mArtistName = attributeMap.getOrDefault(MEDIA_ATTRIBUTE_ARTIST_NAME, UNPOPULATED_ATTRIBUTE);

        mAlbumTitle = attributeMap.getOrDefault(MEDIA_ATTRIBUTE_ALBUM_NAME, UNPOPULATED_ATTRIBUTE);

        attribute = attributeMap.get(MEDIA_ATTRIBUTE_TRACK_NUMBER);
        mTrackNum = (attribute != null && !attribute.isEmpty()) ? Long.valueOf(attribute)
                : TRACK_NUM_INVALID;

        attribute = attributeMap.get(MEDIA_ATTRIBUTE_TOTAL_TRACK_NUMBER);
        mTotalTracks = (attribute != null && !attribute.isEmpty()) ? Long.valueOf(attribute)
                : TOTAL_TRACKS_INVALID;

        mGenre = attributeMap.getOrDefault(MEDIA_ATTRIBUTE_GENRE, UNPOPULATED_ATTRIBUTE);

        attribute = attributeMap.get(MEDIA_ATTRIBUTE_PLAYING_TIME);
        mTrackLen = (attribute != null && !attribute.isEmpty()) ? Long.valueOf(attribute)
                : TOTAL_TRACK_TIME_INVALID;
        mCoverArtHandle = attributeMap.getOrDefault(MEDIA_ATTRIBUTE_COVER_ART_HANDLE,
                UNPOPULATED_ATTRIBUTE);
        mImageLocation = UNPOPULATED_ATTRIBUTE;
        mThumbNailLocation = UNPOPULATED_ATTRIBUTE;
    }

    boolean updateImageLocation(String mCAHandle, String mLocation) {
        if (VDBG) Log.d(TAG, " updateImageLocation hndl " + mCAHandle + " location " + mLocation);
        if (!mCAHandle.equals(mCoverArtHandle) || (mLocation == null)) {
            return false;
        }
        mImageLocation = mLocation;
        return true;
    }

    boolean updateThumbNailLocation(String mCAHandle, String mLocation) {
        if (VDBG) Log.d(TAG, " mCAHandle " + mCAHandle + " location " + mLocation);
        if (!mCAHandle.equals(mCoverArtHandle) || (mLocation == null)) {
            return false;
        }
        mThumbNailLocation = mLocation;
        return true;
    }

    public String toString() {
        return "Metadata [artist=" + mArtistName + " trackTitle= " + mTrackTitle + " albumTitle= "
                + mAlbumTitle + " genre= " + mGenre + " trackNum= " + Long.toString(mTrackNum)
                + " track_len : " + Long.toString(mTrackLen) + " TotalTracks " + Long.toString(
                mTotalTracks) + " mCoverArtHandle=" + mCoverArtHandle +
                " mImageLocation :"+mImageLocation+"]";
    }


    public MediaMetadata getMediaMetaData() {
        if (VDBG) {
            Log.d(TAG, " TrackInfo " + toString());
        }
        MediaMetadata.Builder mMetaDataBuilder = new MediaMetadata.Builder();
        mMetaDataBuilder.putString(MediaMetadata.METADATA_KEY_ARTIST, mArtistName);
        mMetaDataBuilder.putString(MediaMetadata.METADATA_KEY_TITLE, mTrackTitle);
        mMetaDataBuilder.putString(MediaMetadata.METADATA_KEY_ALBUM, mAlbumTitle);
        mMetaDataBuilder.putString(MediaMetadata.METADATA_KEY_GENRE, mGenre);
        mMetaDataBuilder.putLong(MediaMetadata.METADATA_KEY_TRACK_NUMBER, mTrackNum);
        mMetaDataBuilder.putLong(MediaMetadata.METADATA_KEY_NUM_TRACKS, mTotalTracks);
        mMetaDataBuilder.putLong(MediaMetadata.METADATA_KEY_DURATION, mTrackLen);
        if (mImageLocation != UNPOPULATED_ATTRIBUTE) {
            Uri imageUri = Uri.parse(mImageLocation);
            if (VDBG) Log.d(TAG," updating image uri = " + imageUri.toString());
            mMetaDataBuilder.putString(MediaMetadata.METADATA_KEY_ALBUM_ART_URI,
                    imageUri.toString());
        }
        if (mThumbNailLocation != UNPOPULATED_ATTRIBUTE) {
            Bitmap mThumbNailBitmap = BitmapFactory.decodeFile(mThumbNailLocation);
            mMetaDataBuilder.putBitmap(MediaMetadata.METADATA_KEY_DISPLAY_ICON, mThumbNailBitmap);
        }
        return mMetaDataBuilder.build();
    }


    static MediaMetadata getMetadata(int[] attrIds, String[] attrMap) {
        MediaMetadata.Builder metaDataBuilder = new MediaMetadata.Builder();
        int attributeCount = Math.max(attrIds.length, attrMap.length);
        for (int i = 0; i < attributeCount; i++) {
            switch (attrIds[i]) {
                case MEDIA_ATTRIBUTE_TITLE:
                    metaDataBuilder.putString(MediaMetadata.METADATA_KEY_TITLE, attrMap[i]);
                    break;
                case MEDIA_ATTRIBUTE_ARTIST_NAME:
                    metaDataBuilder.putString(MediaMetadata.METADATA_KEY_ARTIST, attrMap[i]);
                    break;
                case MEDIA_ATTRIBUTE_ALBUM_NAME:
                    metaDataBuilder.putString(MediaMetadata.METADATA_KEY_ALBUM, attrMap[i]);
                    break;
                case MEDIA_ATTRIBUTE_TRACK_NUMBER:
                    try {
                        metaDataBuilder.putLong(MediaMetadata.METADATA_KEY_TRACK_NUMBER,
                                Long.valueOf(attrMap[i]));
                    } catch (java.lang.NumberFormatException e) {
                        // If Track Number doesn't parse, leave it unset
                    }
                    break;
                case MEDIA_ATTRIBUTE_TOTAL_TRACK_NUMBER:
                    try {
                        metaDataBuilder.putLong(MediaMetadata.METADATA_KEY_NUM_TRACKS,
                                Long.valueOf(attrMap[i]));
                    } catch (java.lang.NumberFormatException e) {
                        // If Total Track Number doesn't parse, leave it unset
                    }
                    break;
                case MEDIA_ATTRIBUTE_GENRE:
                    metaDataBuilder.putString(MediaMetadata.METADATA_KEY_GENRE, attrMap[i]);
                    break;
                case MEDIA_ATTRIBUTE_PLAYING_TIME:
                    try {
                        metaDataBuilder.putLong(MediaMetadata.METADATA_KEY_DURATION,
                                Long.valueOf(attrMap[i]));
                    } catch (java.lang.NumberFormatException e) {
                        // If Playing Time doesn't parse, leave it unset
                    }
                    break;
            }
        }

        return metaDataBuilder.build();
    }

    public String displayMetaData() {
        MediaMetadata metaData = getMediaMetaData();
        StringBuffer sb = new StringBuffer();
        // getDescription only contains artist, title and album
        sb.append(metaData.getDescription().toString() + " ");
        if (metaData.containsKey(MediaMetadata.METADATA_KEY_GENRE)) {
            sb.append(metaData.getString(MediaMetadata.METADATA_KEY_GENRE) + " ");
        }
        if (metaData.containsKey(MediaMetadata.METADATA_KEY_MEDIA_ID)) {
            sb.append(metaData.getString(MediaMetadata.METADATA_KEY_MEDIA_ID) + " ");
        }
        if (metaData.containsKey(MediaMetadata.METADATA_KEY_TRACK_NUMBER)) {
            sb.append(
                    Long.toString(metaData.getLong(MediaMetadata.METADATA_KEY_TRACK_NUMBER)) + " ");
        }
        if (metaData.containsKey(MediaMetadata.METADATA_KEY_NUM_TRACKS)) {
            sb.append(Long.toString(metaData.getLong(MediaMetadata.METADATA_KEY_NUM_TRACKS)) + " ");
        }
        if (metaData.containsKey(MediaMetadata.METADATA_KEY_TRACK_NUMBER)) {
            sb.append(Long.toString(metaData.getLong(MediaMetadata.METADATA_KEY_DURATION)) + " ");
        }
        if (metaData.containsKey(MediaMetadata.METADATA_KEY_TRACK_NUMBER)) {
            sb.append(Long.toString(metaData.getLong(MediaMetadata.METADATA_KEY_DURATION)) + " ");
        }
        return sb.toString();
    }

    String getCoverArtHandle() {
        return mCoverArtHandle;
    }

    void clearCoverArtData() {
        mCoverArtHandle = UNPOPULATED_ATTRIBUTE;
        mImageLocation = UNPOPULATED_ATTRIBUTE;
        mThumbNailLocation = UNPOPULATED_ATTRIBUTE;
    }

}
