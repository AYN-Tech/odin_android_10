/*
* Copyright (c) 2017, The Linux Foundation. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
* * Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
* * Redistributions in binary form must reproduce the above
* copyright notice, this list of conditions and the following
* disclaimer in the documentation and/or other materials provided
* with the distribution.
* * Neither the name of The Linux Foundation nor the names of its
* contributors may be used to endorse or promote products derived
* from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

package com.android.mmswrapper;

import android.app.PendingIntent;
import android.content.ContentResolver;
import android.content.Context;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.net.Uri;
import android.provider.Telephony;

import com.android.internal.telephony.ISms;

public class TelephonyWrapper {
    private static final String TAG = "TelephonyWrapper";
    public static final String ATTACHMENT_INFO = "attachment_info";
    public static final String NOTIFICATION = "notification";

    public static final class Sms {

        public static boolean moveMessageToFolder(Context context,
                                                  Uri uri, int folder, int error) {
            return Telephony.Sms.moveMessageToFolder(context, uri, folder, error);
        }

        public static boolean isOutgoingFolder(int messageType) {
            boolean ret = Telephony.Sms.isOutgoingFolder(messageType);
            LogUtils.logi(TAG, "isOutgoingFolder Type=" + messageType + "|ret=" + ret);
            return ret;
        }

        public static Uri addMessageToUri(int subId, ContentResolver resolver,
                                          Uri uri, String address, String body, String subject,
                                          Long date, boolean read, boolean deliveryReport,
                                          long threadId) {
            Uri ret = Telephony.Sms.addMessageToUri(subId, resolver, uri, address,
                    body, subject, date, read, deliveryReport, threadId);
            LogUtils.logi(TAG, "addMessageToUri ret=" + ret);
            return ret;
        }

        public static final class Inbox {

            public static Uri addMessage(int subId, ContentResolver resolver,
                                         String address, String body, String subject, Long date,
                                         boolean read) {
                Uri ret = Telephony.Sms.Inbox.addMessage(subId, resolver, address,
                        body, subject, date, read);
                LogUtils.logi(TAG, "addMessage ret=" + ret);
                return ret;
            }
        }

        public static final class Sent {

            public static Uri addMessage(int subId, ContentResolver resolver,
                                         String address, String body, String subject, Long date) {
                Uri ret = Telephony.Sms.Sent.addMessage(subId, resolver, address,
                        body, subject, date);
                LogUtils.logi(TAG, "addMessage ret=" + ret);
                return ret;
            }
        }
    }

    public static final class Mms {

        public static String extractAddrSpec(String address) {
            String ret = Telephony.Mms.extractAddrSpec(address);
            LogUtils.logi(TAG, "extractAddrSpec:" + address + "=" + ret);
            return ret;
        }

        public static boolean isPhoneNumber(String number) {
            boolean ret = Telephony.Mms.isPhoneNumber(number);
            LogUtils.logi(TAG, "isPhoneNumber:" + number + "=" + ret);
            return ret;
        }
    }

    public static void sendTextForSubscriberWithSelfPermissions(int subId, String callingPackage,
                                                                String destAddr, String scAddr,
                                                                String text,
                                                                PendingIntent sentIntent,
                                                                PendingIntent deliveryIntent,
                                                                boolean persistMessage)
            throws RemoteException {
        ISms mISms = ISms.Stub.asInterface(ServiceManager.getService("isms"));
        if (null != mISms) {
            mISms.sendTextForSubscriberWithSelfPermissions(subId,
                    callingPackage, destAddr, scAddr, text, sentIntent, deliveryIntent,
                    persistMessage);
        }
    }
}
