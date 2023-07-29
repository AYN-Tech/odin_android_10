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

import java.util.ArrayList;

import android.content.ContentResolver;
import android.util.Log;

import com.android.mms.ContentRestrictionException;
import com.android.mms.ExceedMessageSizeException;
import com.android.mms.LogTag;
import com.android.mms.MmsConfig;
import com.android.mms.ResolutionException;
import com.android.mms.UnsupportContentTypeException;
import com.google.android.mms.ContentType;

public class CarrierContentRestriction implements ContentRestriction {
    private static ArrayList<String> sSupportedImageTypes;
    private static ArrayList<String> sSupportedAudioTypes;
    private static ArrayList<String> sSupportedVideoTypes;
    private static final String IMAGE_BMP = "image/bmp";
    private static final String AUDIO_WAVE_2CH_LPCM = "audio/wave_2ch_lpcm";

    private static int mCreationMode;

    private static final boolean DEBUG = true;

    static {
        sSupportedImageTypes = ContentType.getImageTypes();
        // Add a ContentType for bmp
        if (!sSupportedImageTypes.contains(IMAGE_BMP)) {
            sSupportedImageTypes.add(IMAGE_BMP);
        }

        sSupportedAudioTypes = ContentType.getAudioTypes();
        if (!sSupportedAudioTypes.contains(AUDIO_WAVE_2CH_LPCM)) {
            sSupportedAudioTypes.add(AUDIO_WAVE_2CH_LPCM);
        }
        sSupportedVideoTypes = ContentType.getVideoTypes();
    }

    public CarrierContentRestriction() {
    }

    public CarrierContentRestriction(int creationMode) {
        mCreationMode = creationMode;
        switch (creationMode) {
        case MmsConfig.CREATIONMODE_RESTRICTED:
        case MmsConfig.CREATIONMODE_WARNING:
            sSupportedImageTypes = new ArrayList<String>();
            sSupportedImageTypes.add(ContentType.IMAGE_JPEG);
            sSupportedImageTypes.add(ContentType.IMAGE_GIF);
            sSupportedImageTypes.add(ContentType.IMAGE_WBMP);

            sSupportedAudioTypes = new ArrayList<String>();
            sSupportedAudioTypes.add(ContentType.AUDIO_AMR);

            sSupportedVideoTypes = new ArrayList<String>();
            sSupportedVideoTypes.add(ContentType.VIDEO_3GPP);
            sSupportedVideoTypes.add(ContentType.VIDEO_H263);
            break;
        case MmsConfig.CREATIONMODE_FREE:
        default:
            sSupportedAudioTypes = ContentType.getAudioTypes();
            if (!sSupportedAudioTypes.contains(AUDIO_WAVE_2CH_LPCM)) {
                sSupportedAudioTypes.add(AUDIO_WAVE_2CH_LPCM);
            }
            break;
        }
    }


    public void checkMessageSize(int messageSize, int increaseSize, ContentResolver resolver)
            throws ContentRestrictionException {
        if (DEBUG) {
            Log.d(LogTag.APP, "CarrierContentRestriction.checkMessageSize messageSize: " +
                        messageSize + " increaseSize: " + increaseSize +
                        " MmsConfig.getMaxMessageSize: " + MmsConfig.getMaxMessageSize());
        }
        if ( (messageSize < 0) || (increaseSize < 0) ) {
            throw new ContentRestrictionException("Negative message size"
                    + " or increase size");
        }
        int newSize = messageSize + increaseSize;

        // 0, and MmsConfig.getMaxMessageSize() - SlideshowModel.SLIDESHOW_SLOP is limitation.
        // Reserve SlideshowModel.SLIDESHOW_SLOP(1k) for overhead.
        if ( (newSize < 0) || ((newSize + SlideshowModel.SLIDESHOW_SLOP)
                      > MmsConfig.getMaxMessageSize() )) {
            throw new ExceedMessageSizeException(
                    "Exceed message size limitation").putMmsSize(newSize);
        }
    }

    public void checkResolution(int width, int height) throws ContentRestrictionException {
        if ( (width > MmsConfig.getMaxImageWidth()) || (height > MmsConfig.getMaxImageHeight()) ) {
            throw new ResolutionException("content resolution exceeds restriction.");
        }
    }

    public void checkImageContentType(String contentType)
            throws ContentRestrictionException {
        if (null == contentType) {
            throw new ContentRestrictionException("Null content type to be check");
        }

        if (!sSupportedImageTypes.contains(contentType)) {
            throw new UnsupportContentTypeException("Unsupported image content type : "
                    + contentType).putContentType(contentType);
        }
    }

    public void checkAudioContentType(String contentType)
            throws ContentRestrictionException {
        if (null == contentType) {
            throw new ContentRestrictionException("Null content type to be check");
        }

        if (!sSupportedAudioTypes.contains(contentType)) {
            throw new UnsupportContentTypeException("Unsupported audio content type : "
                    + contentType).putContentType(contentType);
        }
    }

    public void checkVideoContentType(String contentType)
            throws ContentRestrictionException {
        if (null == contentType) {
            throw new ContentRestrictionException("Null content type to be check");
        }

        if (!sSupportedVideoTypes.contains(contentType)) {
            throw new UnsupportContentTypeException("Unsupported video content type : "
                    + contentType).putContentType(contentType);
        }
    }
}
