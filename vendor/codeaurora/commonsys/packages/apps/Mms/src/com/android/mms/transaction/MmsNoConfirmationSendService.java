 /*
 * Copyright (c) 2015, 2017, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *    * Neither the name of The Linux Foundation nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
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

package com.android.mms.transaction;

import java.io.File;

import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.database.sqlite.SqliteWrapper;
import android.net.Uri;
import android.provider.Telephony.Mms;
import android.telephony.SubscriptionManager;
import android.util.Log;
import android.webkit.MimeTypeMap;
import android.widget.Toast;

import com.android.mms.MmsConfig;
import com.android.mms.R;
import com.android.mms.ui.MessageUtils;
import com.android.mms.ui.MessagingPreferenceActivity;
import com.android.mms.ui.NoConfirmationSendService;
import com.android.mms.ui.UriImage;
import com.android.mmswrapper.MediaFileWrapper;
import com.google.android.mms.InvalidHeaderValueException;
import com.google.android.mms.MmsException;
import com.google.android.mms.pdu.EncodedStringValue;
import com.google.android.mms.pdu.PduBody;
import com.google.android.mms.pdu.PduHeaders;
import com.google.android.mms.pdu.PduPart;
import com.google.android.mms.pdu.PduPersister;
import com.google.android.mms.pdu.SendReq;

public class MmsNoConfirmationSendService extends NoConfirmationSendService {
    private static final String TAG =
            MmsNoConfirmationSendService.class.getSimpleName();

    @Override
    protected void onHandleIntent(Intent intent) {
        if(intent != null) {
            String uriString = intent.getStringExtra(MmsConfig.EXTRA_URI);
            sendMms(Uri.parse(uriString), this.getApplicationContext());
        }
    }

    @Override
    public void onDestroy() {
        stopSelf();
        super.onDestroy();
    }

    public SendReq createSendReq(Uri attachment, Context context) {
        Log.v(TAG, "createSendReq");
        if (attachment == null) {
            return null;
        }

        SendReq mms = new SendReq();
        mms.addTo(new EncodedStringValue(MmsConfig.getMmsDestination()));
        mms.setDate(System.currentTimeMillis() / 1000L);
        try {
            mms.setPriority(PduHeaders.PRIORITY_NORMAL);
        } catch (InvalidHeaderValueException e1) {
            // TODO Auto-generated catch block
            e1.printStackTrace();
        }
        PduBody pduBody = new PduBody();
        // ATTACHMENTS
        if (attachment != null) {
            String filePath = MessageUtils.getFilePath(context, attachment);
            if (filePath == null) {
                Toast.makeText(context, R.string.file_not_found,
                        Toast.LENGTH_SHORT);
                return null;
            }
            File attFile = new File(filePath);
            String extn = MimeTypeMap.getFileExtensionFromUrl(attFile.toURI()
                    .toString());
            String mimetype = MimeTypeMap.getSingleton()
                    .getMimeTypeFromExtension(extn);
            if (mimetype == null) {
                Toast.makeText(
                        context,
                        context.getResources().getString(
                                R.string.unsupported_media_format,
                                R.string.type_attachment), Toast.LENGTH_SHORT);
                return null;
            }

            int fileType = MediaFileWrapper.getFileTypeForMimeType(mimetype);

            PduPart attPart = new PduPart();
            try {
                if (MediaFileWrapper.isVideoFileType(fileType)) {
                    if (MessageUtils.isTooLargeFile(this, attachment)) {
                        Log.w(TAG, "Too Large File cannot Send");
                        Toast.makeText(context,
                                R.string.exceed_message_size_limitation,
                                Toast.LENGTH_SHORT).show();
                        return null;
                    }
                    int index = filePath.lastIndexOf('.');
                    String kit = attFile.toString().replace(" ", "_");
                    String contentId = (index == -1) ? kit : kit.substring(0,
                            index);
                    attPart.setContentId(contentId.getBytes());
                    attPart.setContentType(mimetype.getBytes());
                    try {
                        // We save content-location as iso-8859-1 format
                        String contentLocation = new String(attFile.getName()
                                .getBytes());
                        attPart.setContentLocation(contentLocation.getBytes());
                        attPart.setDataUri(attachment);
                        pduBody.addPart(attPart);
                    } catch (Exception e) {
                        Log.w(TAG, "createPduPartForAttachment: "
                                + "exception while creating content-location");
                        Toast.makeText(context, R.string.cannot_send_message,
                                Toast.LENGTH_SHORT).show();
                        return null;
                    }
                } else if (MediaFileWrapper.isImageFileType(fileType)) {
                    if (MessageUtils.isTooLargeFile(this, attachment)) {
                        Log.w(TAG, "Too Large File cannot Send");
                        Toast.makeText(context,
                                R.string.resize_image_error_information,
                                Toast.LENGTH_SHORT).show();
                        return null;
                    }

                    UriImage image = new UriImage(context, attachment);
                    attPart = image.getResizedImageAsPart(
                            MmsConfig.getMaxImageWidth(),
                            MmsConfig.getMaxImageHeight(),
                            MmsConfig.getMaxMessageSize());
                    if (attPart == null) {
                        Toast.makeText(context,
                                R.string.failed_to_resize_image,
                                Toast.LENGTH_SHORT).show();
                        return null;
                    }
                    String contentLocation = new String(attFile.getName()
                            .getBytes());
                    attPart.setContentLocation(contentLocation.getBytes());
                    attPart.setContentType(image.getContentType().getBytes());
                    pduBody.addPart(attPart);
                } else {
                    Log.w(TAG, "Invalid Attachment Type");
                    Toast.makeText(
                            context,
                            context.getResources().getString(
                                    R.string.unsupported_media_format,
                                    R.string.type_attachment),
                            Toast.LENGTH_SHORT).show();
                    return null;
                }
            } catch (Exception e) {
                Toast.makeText(context,
                        R.string.indication_send_message_failed,
                        Toast.LENGTH_SHORT);
                Log.w(TAG, "Cannot Send Invalid Message");
                e.printStackTrace();
                return null;
            }
        }

        // Add PduBody to SendReq
        mms.setBody(pduBody);
        return mms;
    }

    public void sendMms(Uri uri, Context context) {
        SendReq req = createSendReq(uri, context);
        if (req == null) {
            Log.d(TAG, "Unable to Create MMS ");
            return;
        }
        Uri mmsuri = null;
        try {
            int mCurrentConvSubID = 0;
            PduPersister persister = PduPersister.getPduPersister(context);
            mmsuri = persister.persist(req, Mms.Draft.CONTENT_URI, true,
                    MessagingPreferenceActivity.getIsGroupMmsEnabled(context),
                    null);
            ContentValues values = new ContentValues();
            values.put(Mms.SUBSCRIPTION_ID, SubscriptionManager.getDefaultDataSubscriptionId());
            SqliteWrapper.update(context, context.getContentResolver(), mmsuri,
                    values, null, null);
            MessageSender msgSender = new MmsMessageSender(context, mmsuri,
                    PduHeaders.PRIORITY_NORMAL, mCurrentConvSubID);
            Cursor c = context.getContentResolver().query(mmsuri, null, null,
                    null, null);
            long threadId = 0;
            try {
                if (c != null && c.moveToFirst()) {
                    threadId = c.getLong(c.getColumnIndex(Mms.THREAD_ID));
                }
            } finally {
                if (c != null) {
                    c.close();
                }
            }
            if (isAirplaneMode()) {
                Log.w(TAG, "cannot send Message");
                Toast.makeText(context, R.string.message_queued,
                        Toast.LENGTH_SHORT).show();
            }
            try {
                Toast.makeText(context, R.string.try_to_send,
                        Toast.LENGTH_SHORT).show();
                msgSender.sendMessage(threadId);
            } catch (MmsException e) {
                e.printStackTrace();
            }
        } catch (MmsException e) {
            Log.w(TAG, "sendMessage: Exception while sending MMS");
            Toast.makeText(context, R.string.indication_send_message_failed,
                    Toast.LENGTH_SHORT).show();
            return;
        }
    }

    public boolean isAirplaneMode() {
        return android.provider.Settings.Global.getInt(getContentResolver(),
                android.provider.Settings.Global.AIRPLANE_MODE_ON, 0) > 0;
    }
}
