/*
 * Copyright (c) 2013-2014, 2016-2017, The Linux Foundation. All rights reserved.
 * Not a Contribution.
 *
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

import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.InputStream;
import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.text.DecimalFormat;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.ListIterator;
import java.util.Map;
import java.util.Locale;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.ActivityNotFoundException;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.ComponentName;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnCancelListener;
import android.content.DialogInterface.OnClickListener;
import android.content.Intent;
import android.content.ContentResolver;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.content.SharedPreferences;
import android.content.res.TypedArray;
import android.database.Cursor;
import android.database.sqlite.SqliteWrapper;
import android.graphics.BitmapFactory;
import android.graphics.BitmapFactory.Options;
import android.graphics.drawable.Drawable;
import android.media.CamcorderProfile;
import android.media.MediaMetadataRetriever;
import android.media.RingtoneManager;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.NetworkInfo.State;
import android.net.wifi.WifiManager;
import android.net.Uri;
import android.os.Environment;
import android.os.Handler;
import android.os.Bundle;
import android.os.Message;
import android.os.Messenger;
import android.os.Parcelable;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.os.StatFs;
import android.preference.PreferenceManager;
import android.provider.ContactsContract;
import android.provider.ContactsContract.Contacts;
import android.provider.ContactsContract.CommonDataKinds.Phone;
import android.provider.ContactsContract.CommonDataKinds.StructuredName;
import android.provider.ContactsContract.Data;
import android.provider.MediaStore;
import android.provider.MediaStore.Audio;
import android.provider.Settings;
import android.provider.Telephony;
import android.provider.Telephony.Mms;
import android.provider.Telephony.Mms.Part;
import android.provider.Telephony.Sms;
import android.provider.Telephony.Threads;
import android.telecom.PhoneAccount;
import android.telecom.PhoneAccountHandle;
import android.telecom.TelecomManager;
import android.telephony.PhoneNumberUtils;
import android.telephony.ServiceState;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;
import android.text.TextUtils;
import android.os.AsyncTask;
import android.text.format.DateUtils;
import android.text.format.Time;
import android.text.style.URLSpan;
import android.util.Log;
import android.util.TypedValue;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.TextView;
import android.widget.Toast;

import com.android.mms.LogTag;
import com.android.mms.MmsApp;
import com.android.mms.MmsConfig;
import com.android.mms.R;
import com.android.mms.TempFileProvider;
import com.android.mms.data.Contact;
import com.android.mms.data.Conversation;
import com.android.mms.data.WorkingMessage;
import com.android.mms.model.ContentRestriction;
import com.android.mms.model.ContentRestrictionFactory;
import com.android.mms.model.ImageModel;
import com.android.mms.model.MediaModel;
import com.android.mms.model.SlideModel;
import com.android.mms.model.SlideshowModel;
import com.android.mms.model.VcardModel;
import com.android.mms.transaction.MessagingNotification;
import com.android.mms.transaction.MmsMessageSender;
import com.android.mms.util.AddressUtils;
import com.android.mms.util.ContactRecipientEntryUtils;
import com.android.mms.util.DownloadManager;
import com.android.mms.widget.MmsWidgetProvider;
import com.android.mmswrapper.ConnectivityManagerWrapper;
import com.android.mmswrapper.ConstantsWrapper;
import com.android.mmswrapper.SubscriptionManagerWrapper;
import com.android.mmswrapper.TelephonyManagerWrapper;
import com.android.mmswrapper.TelephonyWrapper;
import com.google.android.mms.ContentType;
import com.google.android.mms.MmsException;
import com.google.android.mms.pdu.CharacterSets;
import com.google.android.mms.pdu.EncodedStringValue;
import com.google.android.mms.pdu.MultimediaMessagePdu;
import com.google.android.mms.pdu.NotificationInd;
import com.google.android.mms.pdu.PduBody;
import com.google.android.mms.pdu.PduHeaders;
import com.google.android.mms.pdu.PduPart;
import com.google.android.mms.pdu.PduPersister;
import com.google.android.mms.pdu.RetrieveConf;
import com.google.android.mms.pdu.SendReq;
import org.codeaurora.internal.IExtTelephony;
import android.app.ActivityManager;
import android.app.Service;
import android.support.v4.content.FileProvider;

import com.android.internal.telephony.EncodeException;
import com.android.internal.telephony.GsmAlphabet;
import com.android.internal.telephony.PhoneConstants;
import com.android.internal.telephony.SmsHeader;
import com.android.internal.telephony.cdma.sms.BearerData;
import com.android.internal.telephony.cdma.sms.CdmaSmsAddress;
import com.android.internal.telephony.cdma.sms.UserData;
import android.telephony.PhoneNumberUtils;
import android.telephony.SmsManager;
import android.telephony.SmsMessage;
import android.telephony.SubscriptionManager;
import static android.telephony.SmsMessage.ENCODING_16BIT;
import static android.telephony.SmsMessage.ENCODING_7BIT;
import static android.telephony.SmsMessage.ENCODING_UNKNOWN;
import static android.telephony.SmsMessage.MAX_USER_DATA_BYTES;
import static android.telephony.SmsMessage.MAX_USER_DATA_SEPTETS;



/**
 * An utility class for managing messages.
 */
public class MessageUtils {
    interface ResizeImageResultCallback {
        void onResizeResult(PduPart part, boolean append);
    }

    private static final int SELECT_SYSTEM = 0;
    private static final int SELECT_EXTERNAL = 1;
    private static final boolean DEBUG = false;
    public static final int SUB_INVALID = -1;  //  for single card product
    public static final int SUB1 = 0;  // for DSDS product of slot one
    public static final int SUB2 = 1;  // for DSDS product of slot two
    public static final String SUBSCRIPTION_KEY = "subscription";
    public static final int MESSAGE_READ = 1;
    public static final int MESSAGE_SEEN = 1;
    // add manage mode of multi select action
    public static final int INVALID_MODE= -1;
    public static final int FORWARD_MODE = 0;
    public static final int SIM_MESSAGE_MODE = 1;
    public static final Uri ICC_URI = Uri.parse("content://sms/icc");
    public static final Uri ICC1_URI = Uri.parse("content://sms/icc1");
    public static final Uri ICC2_URI = Uri.parse("content://sms/icc2");
    private static final int TIMESTAMP_LENGTH = 7;  // See TS 23.040 9.2.3.11

    private static final String TAG = LogTag.TAG;
    private static final String PREFERRED_SIM_ICON_INDEX = "preferred_sim_icon_index";
    private static String sLocalNumber;
    private static String[] sNoSubjectStrings;
    public static String MULTI_SIM_NAME = "perferred_name_sub";
    private static final String VIEW_MODE_NAME = "current_view";
    // add for different search mode in SearchActivityExtend
    public static final int SEARCH_MODE_CONTENT = 0;
    public static final int SEARCH_MODE_NAME    = 1;
    public static final int SEARCH_MODE_NUMBER  = 2;
    // add for different match mode in classify search
    public static final int MATCH_BY_ADDRESS = 0;
    public static final int MATCH_BY_THREAD_ID = 1;

    public static final int PREFER_SMS_STORE_PHONE = 0;
    public static final int PREFER_SMS_STORE_CARD = 1;

    // Consider oct-strean as the content type of vCard
    public static final String OCT_STREAM = "application/oct-stream";
    public static final String VCARD = "vcf";

    public static final String NEW_LINE = "\n";

    // distinguish view vcard from mms but not from contacts.
    public static final String VIEW_VCARD = "VIEW_VCARD_FROM_MMS";
    // add for obtain mms data path
    private static final String MMS_DATA_DATA_DIR = "/data/data";
    private static final String MMS_DATA_DIR = "/data/phonedata";
    private static final String MMS_DATABASE_DIR =
            "/com.android.providers.telephony/databases/mmssms.db";
    // the remaining space , format as MB
    public static final long MIN_AVAILABLE_SPACE_MMS = 2 * 1024 * 1024;
    private static final long KILOBYTE_SIZE = 1024;
    private static final int DEFAULT_FONT_SIZE = 18;

    // add for query message count from iccsms table
    public static final Uri ICC_SMS_URI = Uri.parse("content://sms/iccsms");
    /**
     * Whether the LayoutDirection is RTL.
     */
    private static boolean isLayoutRtl = false;

    private static boolean sErrorDialogIsShown;

    private static final String EXIT_AFTER_RECORD = "exit_after_record";

    // Cache of both groups of space-separated ids to their full
    // comma-separated display names, as well as individual ids to
    // display names.
    // TODO: is it possible for canonical address ID keys to be
    // re-used?  SQLite does reuse IDs on NULL id_ insert, but does
    // anything ever delete from the mmssms.db canonical_addresses
    // table?  Nothing that I could find.
    private static final Map<String, String> sRecipientAddress =
            new ConcurrentHashMap<String, String>(20 /* initial capacity */);

    // When we pass a video record duration to the video recorder, use one of these values.
    private static final int[] sVideoDuration =
            new int[] {0, 5, 10, 15, 20, 30, 40, 50, 60, 90, 120};

    /**
     * MMS address parsing data structures
     */
    // allowable phone number separators
    private static final char[] NUMERIC_CHARS_SUGAR = {
        '-', '.', ',', '(', ')', ' ', '/', '\\', '*', '#', '+'
    };

    // Dialog item options for number
    private static final int DIALOG_ITEM_CALL         = 0;
    private static final int DIALOG_ITEM_SMS          = 1;
    private static final int DIALOG_ITEM_ADD_CONTACTS = 2;

    // Dialog item options for email
    private static final int DIALOG_ITEM_EMAIL_TO = 0;
    private static final int DIALOG_ITEM_EMAIL_ADD_CONTACTS = 1;

    private static final String MAIL_TO_PREFIX = "mailto:";

    private static HashMap numericSugarMap = new HashMap (NUMERIC_CHARS_SUGAR.length);

    public static String WAPPUSH = "Browser Information"; // Wap push key
    private static final String[] WEB_SCHEMA =
                        new String[] { "http://", "https://", "rtsp://" };

    // add for search
    public static final String SEARCH_KEY                = "is_search";
    public static final String SEARCH_KEY_TITLE          = "title";
    public static final String SEARCH_KEY_MODE_POSITION  = "mode_position";
    public static final String SEARCH_KEY_KEY_STRING     = "key_str";
    public static final String SEARCH_KEY_DISPLAY_STRING = "display_str";
    public static final String SEARCH_KEY_MATCH_WHOLE    = "match_whole";

    public static final String MAIL_BOX_ID    = "mailbox_id";

    private static final String REPLACE_QUOTES_1 = "'";
    private static final String REPLACE_QUOTES_2 = "''";

    public static final String KEY_SMS_FONTSIZE = "smsfontsize";
    public static final int DELAY_TIME = 200;
    public static final float FONT_SIZE_DEFAULT = 30f;
    public static final float MAX_FONT_SIZE = 80f;
    public static final float MIN_FONT_SIZE = 20f;
    public static final float FONT_SIZE_STEP = 5f;
    private static final char SBC_CHAR_START = 65281;
    private static final char SBC_CHAR_END   = 65373;

    protected static final int URL_OPTION_MENU_CONNECT = 0;
    protected static final int URL_OPTION_MENU_COPY_URL = 1;

    private static final String WIFI_CALL_TURNON = "wifi_call_turnon";
    private static final int WIFI_CALLING_DISABLED = 0;
    private static final int WIFI_CALLING_ENABLED = 1;

    //for showing memory status dialog.
    private static AlertDialog memoryStatusDialog = null;

    public static final int ALL_RECIPIENTS_VALID   = 0;
    public static final int ALL_RECIPIENTS_INVALID = -1;
    // Indentify RECIPIENT editText is empty
    public static final int ALL_RECIPIENTS_EMPTY   = -2;
    // Add phone feature uri
    private static final Uri URI_PHONE_FEATURE = Uri
            .parse("content://com.qualcomm.qti.phonefeature.FEATURE_PROVIDER");
    // Add keys for supporting SMSC
    private static final String METHOD_GET_SMSC = "get_smsc";
    private static final String METHOD_SET_SMSC = "set_smsc";
    public static final String EXTRA_SMSC = "smsc";
    public static final String EXTRA_SMSC_RESULT = "result";
    public static final String EXTRA_EXCEPTION = "exception";
    public static final String EXTRA_USEROBJ = "userobj";

    // add for obtaining all short message count
    public static final Uri MESSAGES_COUNT_URI = Uri.parse("content://mms-sms/messagescount");

    private static final int DEFAULT_MMS_SIZE = 50;
    private static final int MIN_MMS_SIZE = 3;
    private static final int DECIMAL_FORMATTOR_GROUPING_SIZE = 3;

    private static final String INTERNATIONAL_PREFIX_ENABLE = "international_prefix_enable";
    private static final String INTERNATIONAL_PREFIX_NUMBER = "international_prefix_number";
    private static final String IDP_PLUS = "+";
    private static final String IDP_IDN = "+62";
    private static final String IDP_ZERO = "0";
    private static final String IDP_DEFAULT_PREFIX = "01033";
    private static final int IDP_ENABLE = 1;
    private final static int CDMA_STATUS_SHIFT = 16;

    // Save the thread id for same recipient forward mms
    public static ArrayList<Long> sSameRecipientList = new ArrayList<Long>();

    private static final String PREF_SHOW_URL_WARNING = "pref_should_show_url_warning";
    private static final boolean PREF_SHOULD_SHOW_URL_WARNING = true;

    private static final String CELL_BROADCAST_PACKAGE_NAME =
            "com.android.cellbroadcastreceiver";
    private static final String CELL_BROADCAST_ACTIVITY_NAME =
            "com.android.cellbroadcastreceiver.CellBroadcastListActivity";
    private static final long COMPRESSION_FACTOR = 3;

    static {
        for (int i = 0; i < NUMERIC_CHARS_SUGAR.length; i++) {
            numericSugarMap.put(NUMERIC_CHARS_SUGAR[i], NUMERIC_CHARS_SUGAR[i]);
        }
    }

    // Filter numbers start with 106575, 1069, and numbers of carrier,
    // bank and public service such as 12306.
    private static Pattern sNotificationNumberPattern = Pattern.compile(
            "^1065(\\d)*$|^1069(\\d)*$|^100(\\d)*$|^95[15](\\d)*$|^12306$");

    private static final String NOTIFICATION_MSG_FLAG = "1";

    private static final String MIME_TYPE_3GPP_VIDEO = "video/3gpp";
    private static final String MIME_TYPE_3GPP_AUDIO = "audio/3gpp";
    private static final String MIME_TYPE_3GPP2_VIDEO = "video/3gpp2";
    private static final String MIME_TYPE_3GPP2_AUDIO = "audio/3gpp2";

    private static final int OFFSET_ADDRESS_LENGTH = 0;
    private static final int OFFSET_TOA = 1;
    private static final int OFFSET_ADDRESS_VALUE = 2;

    public static final String SMS_BOX_ID = "box_id";
    public static final String COPY_SMS_INTO_SIM_SUCCESS = "success";

    public static final int MESSAGE_REPORT_COLUMN_ID         = 0;
    public static final int MESSAGE_REPORT_COLUMN_MESSAGE_ID = 1;
    public static final int MESSAGE_REPORT_COLUMN_SUB_ID   = 2;
    public static final int COLUMN_SMS_STATUS = 10;
    public static final int COLUMN_MMS_DELIVERY_REPORT = 20;
    public static final int COLUMN_MMS_READ_REPORT = 21;
    public static final String[] MMS_REPORT_STATUS_PROJECTION = new String[] {
        "delivery_status",      //0
        "read_status"           //1
    };
    public static final int COLUMN_DELIVERY_STATUS     = 0;
    public static final int COLUMN_READ_STATUS         = 1;

    private static final String MMS_SELECTION =
                Mms.MESSAGE_TYPE + " = " + PduHeaders.MESSAGE_TYPE_SEND_REQ + " OR " +
                Mms.MESSAGE_TYPE + " = " + PduHeaders.MESSAGE_TYPE_RETRIEVE_CONF + " OR " +
                Mms.MESSAGE_TYPE + " = " + PduHeaders.MESSAGE_TYPE_NOTIFICATION_IND;

    /* Basic permissions SMS application should have */
    public static String[] sSMSBasicPermissions = new String[] {
        Manifest.permission.READ_SMS, Manifest.permission.READ_PHONE_STATE,
        Manifest.permission.READ_CONTACTS
    };


    public static String [] sSMSExtendPermissions = new String [] {
        Manifest.permission.READ_EXTERNAL_STORAGE
    };

    public static String[] sSMSBasicPermissions_FOR_DEFAULT_MESSAGING = new String[] {
            Manifest.permission.READ_SMS, Manifest.permission.READ_PHONE_STATE,
            Manifest.permission.READ_CONTACTS, Manifest.permission.SEND_SMS,
            Manifest.permission.RECEIVE_SMS, Manifest.permission.WRITE_CONTACTS,
            Manifest.permission.CALL_PHONE,
    };

    public static String [] sSMSExtendPermissions_FOR_DEFAULT_MESSAGING = new String [] {
            Manifest.permission.READ_EXTERNAL_STORAGE,
            Manifest.permission.WRITE_EXTERNAL_STORAGE
    };

    private MessageUtils() {
        // Forbidden being instantiated.
    }

    public static boolean is3GPP(String ext) {
        return "3gp".equals(ext) || "3gpp".equals(ext);
    }

    public static boolean is3GPP2(String ext) {
        return "3g2".equals(ext) || "3gpp2".equals(ext);
    }

    public static String getMimeType(String absolutePath, String ext) {
        MediaMetadataRetriever retriever = new MediaMetadataRetriever();
        try {
            retriever.setDataSource(absolutePath);
            boolean hasVideo = retriever
                    .extractMetadata(MediaMetadataRetriever.METADATA_KEY_HAS_VIDEO) != null;
            if (is3GPP(ext)) {
                return hasVideo ? MIME_TYPE_3GPP_VIDEO : MIME_TYPE_3GPP_AUDIO;
            } else if (is3GPP2(ext)) {
                return hasVideo ? MIME_TYPE_3GPP2_VIDEO : MIME_TYPE_3GPP2_AUDIO;
            }
        } catch (RuntimeException e) {
            Log.e(TAG, "Unable to open 3GP file to determine mimetype");
        } finally {
            retriever.release();
        }
        // Default to video 3gp if the file is unreadable as this was the default before
        // ambiguous resolution support was added.
        return MIME_TYPE_3GPP_VIDEO;
    }

    /**
     * cleanseMmsSubject will take a subject that's says, "<Subject: no subject>", and return
     * a null string. Otherwise it will return the original subject string.
     * @param context a regular context so the function can grab string resources
     * @param subject the raw subject
     * @return
     */
    public static String cleanseMmsSubject(Context context, String subject) {
        if (TextUtils.isEmpty(subject)) {
            return subject;
        }
        if (sNoSubjectStrings == null) {
            sNoSubjectStrings =
                    context.getResources().getStringArray(R.array.empty_subject_strings);

        }
        final int len = sNoSubjectStrings.length;
        for (int i = 0; i < len; i++) {
            if (subject.equalsIgnoreCase(sNoSubjectStrings[i])) {
                return null;
            }
        }
        return subject;
    }

    public static String getMessageDetails(Context context, Cursor cursor, int size) {
        if (cursor == null || cursor.getCount() == 0) {
            return null;
        }
        isLayoutRtl = (TextUtils.getLayoutDirectionFromLocale(Locale.getDefault())
                == View.LAYOUT_DIRECTION_RTL);

        if ("mms".equals(cursor.getString(MessageListAdapter.COLUMN_MSG_TYPE))) {
            int type = cursor.getInt(MessageListAdapter.COLUMN_MMS_MESSAGE_TYPE);
            switch (type) {
                case PduHeaders.MESSAGE_TYPE_NOTIFICATION_IND:
                    return getNotificationIndDetails(context, cursor);
                case PduHeaders.MESSAGE_TYPE_RETRIEVE_CONF:
                case PduHeaders.MESSAGE_TYPE_SEND_REQ:
                    return getMultimediaMessageDetails(context, cursor, size);
                default:
                    Log.w(TAG, "No details could be retrieved.");
                    return "";
            }
        } else {
            return getTextMessageDetails(context, cursor);
        }
    }

    private static String getNotificationIndDetails(Context context, Cursor cursor) {
        StringBuilder details = new StringBuilder();
        Resources res = context.getResources();

        long id = cursor.getLong(MessageListAdapter.COLUMN_ID);
        Uri uri = ContentUris.withAppendedId(Mms.CONTENT_URI, id);
        NotificationInd nInd;

        try {
            nInd = (NotificationInd) PduPersister.getPduPersister(
                    context).load(uri);
        } catch (MmsException e) {
            Log.e(TAG, "Failed to load the message: " + uri, e);
            return context.getResources().getString(R.string.cannot_get_details);
        }

        // Message Type: Mms Notification.
        details.append(res.getString(R.string.message_type_label));
        details.append(res.getString(R.string.multimedia_notification));

        // From: ***
        String from = extractEncStr(context, nInd.getFrom());

        // Make sure the "from" display normally for RTL.
        if (isLayoutRtl) {
            from = '\u202D'+from+'\u202C';
        }
        details.append('\n');
        details.append(res.getString(R.string.from_label));
        details.append(!TextUtils.isEmpty(from)? from:
                                 res.getString(R.string.hidden_sender_address));

        // Date: ***
        details.append('\n');
        details.append(res.getString(
                                R.string.expire_on,
                                MessageUtils.formatTimeStampString(
                                        context, nInd.getExpiry() * 1000L, true)));

        // Subject: ***
        details.append('\n');
        details.append(res.getString(R.string.subject_label));

        EncodedStringValue subject = nInd.getSubject();
        if (subject != null) {
            details.append(subject.getString());
        }

        // Message class: Personal/Advertisement/Infomational/Auto
        details.append('\n');
        details.append(res.getString(R.string.message_class_label));
        details.append(new String(nInd.getMessageClass()));

        // Message size: *** KB
        details.append('\n');
        details.append(res.getString(R.string.message_size_label));
        details.append(String.valueOf((nInd.getMessageSize() + 1023) / KILOBYTE_SIZE));
        details.append(context.getString(R.string.kilobyte));

        return details.toString();
    }

    private static String getMultimediaMessageDetails(
            Context context, Cursor cursor, int size) {
        int type = cursor.getInt(MessageListAdapter.COLUMN_MMS_MESSAGE_TYPE);
        if (type == PduHeaders.MESSAGE_TYPE_NOTIFICATION_IND) {
            return getNotificationIndDetails(context, cursor);
        }

        StringBuilder details = new StringBuilder();
        Resources res = context.getResources();

        long id = cursor.getLong(MessageListAdapter.COLUMN_ID);
        Uri uri = ContentUris.withAppendedId(Mms.CONTENT_URI, id);
        MultimediaMessagePdu msg;

        try {
            msg = (MultimediaMessagePdu) PduPersister.getPduPersister(
                    context).load(uri);
        } catch (MmsException e) {
            Log.e(TAG, "Failed to load the message: " + uri, e);
            return context.getResources().getString(R.string.cannot_get_details);
        }

        // Message Type: Text message.
        details.append(res.getString(R.string.message_type_label));
        details.append(res.getString(R.string.multimedia_message));

        if (msg instanceof RetrieveConf) {
            // From: ***
            String from = extractEncStr(context, ((RetrieveConf) msg).getFrom());

            // Make sure the "from" display normally for RTL.
            if (isLayoutRtl) {
                from = '\u202D'+from+'\u202C';
            }
            details.append('\n');
            details.append(res.getString(R.string.from_label));
            details.append(!TextUtils.isEmpty(from)? from:
                                  res.getString(R.string.hidden_sender_address));
        }

        // To: ***
        details.append('\n');
        details.append(res.getString(R.string.to_address_label));
        EncodedStringValue[] to = msg.getTo();
        if (to != null) {
            details.append(EncodedStringValue.concat(to));
        }
        else {
            Log.w(TAG, "recipient list is empty!");
        }


        // Bcc: ***
        if (msg instanceof SendReq) {
            EncodedStringValue[] values = ((SendReq) msg).getBcc();
            if ((values != null) && (values.length > 0)) {
                details.append('\n');
                details.append(res.getString(R.string.bcc_label));
                details.append(EncodedStringValue.concat(values));
            }
        }

        // Date: ***
        details.append('\n');
        int msgBox = cursor.getInt(MessageListAdapter.COLUMN_MMS_MESSAGE_BOX);
        if (msgBox == Mms.MESSAGE_BOX_DRAFTS) {
            details.append(res.getString(R.string.saved_label));
        } else if (msgBox == Mms.MESSAGE_BOX_INBOX) {
            details.append(res.getString(R.string.received_label));
        } else {
            details.append(res.getString(R.string.sent_label));
        }

        details.append(MessageUtils.formatTimeStampString(
                context, msg.getDate() * 1000L, true));

        // Subject: ***
        details.append('\n');
        details.append(res.getString(R.string.subject_label));

        EncodedStringValue subject = msg.getSubject();
        if (subject != null) {
            String subStr = subject.getString();
            details.append(subStr);
        }

        // Priority: High/Normal/Low
        details.append('\n');
        details.append(res.getString(R.string.priority_label));
        details.append(getPriorityDescription(context, msg.getPriority()));

        // Message size: *** KB
        details.append('\n');
        details.append(res.getString(R.string.message_size_label));
        details.append((size + SlideshowModel.SLIDESHOW_SLOP -1)
                / SlideshowModel.SLIDESHOW_SLOP + 1);
        details.append(" KB");

        if (TelephonyWrapper.Sms.isOutgoingFolder(msgBox)) {
            // Delivery report
            ReportStatus reportStatus = getMmsReportStatus(context, id);
            String deliveryReport = cursor.getString(COLUMN_MMS_DELIVERY_REPORT);
            int deliveryReportInt = 0;
            if (deliveryReport != null) {
                deliveryReportInt = Integer.parseInt(deliveryReport);
            }
            if (deliveryReportInt == PduHeaders.VALUE_YES) {
                details.append(NEW_LINE);
                details.append(res.getString(R.string.delivery_report));
                if (reportStatus == null) {
                    details.append(res.getString(R.string.status_pending));
                } else {
                    switch (reportStatus.mDeliveryStatus) {
                        case 0: // No delivery report received so far.
                            details.append(res.getString(R.string.status_pending));
                            break;
                        case PduHeaders.STATUS_FORWARDED:
                        case PduHeaders.STATUS_RETRIEVED:
                            details.append(res.getString(R.string.status_received));
                            break;
                        case PduHeaders.STATUS_REJECTED:
                            details.append(res.getString(R.string.status_rejected));
                            break;
                        case PduHeaders.STATUS_EXPIRED:
                            details.append(res.getString(R.string.status_expired));
                            break;
                        default:
                            details.append(res.getString(R.string.status_failed));
                            break;
                    }
                }
            }

            // Read report
            String readReport = cursor.getString(COLUMN_MMS_READ_REPORT);
            int readReportInt = 0;
            if (readReport != null) {
                readReportInt = Integer.parseInt(readReport);
            }
            if (readReportInt == PduHeaders.VALUE_YES) {
                details.append(NEW_LINE);
                details.append(res.getString(R.string.read_report));
                if (reportStatus == null) {
                    details.append(res.getString(R.string.status_unread));
                } else {
                    switch (reportStatus.mReadStatus) {
                        case PduHeaders.READ_STATUS_READ:
                            details.append(res.getString(R.string.status_read));
                            break;
                        case PduHeaders.READ_STATUS__DELETED_WITHOUT_BEING_READ:
                            details.append(res.getString(R.string.status_deleted_unread));
                            break;
                        default:
                            details.append(res.getString(R.string.status_unread));
                            break;
                    }
                }
            }
        }

        return details.toString();
    }

    public static ReportStatus getMmsReportStatus(Context context, long messageId) {
        Uri uri = Uri.withAppendedPath(Mms.REPORT_STATUS_URI,
                String.valueOf(messageId));
        Cursor c = SqliteWrapper.query(context, context.getContentResolver(), uri,
                MMS_REPORT_STATUS_PROJECTION, null, null, null);
        ReportStatus status = null;

        if (c == null) {
            return status;
        }

        try {
            while (c.moveToNext()) {
                status = new ReportStatus(
                        c.getInt(COLUMN_DELIVERY_STATUS),
                        c.getInt(COLUMN_READ_STATUS));
            }
        } finally {
            c.close();
        }
        return status;
    }

    private static class ReportStatus {
        final int mDeliveryStatus;
        final int mReadStatus;

        public ReportStatus(int deliveryStatus, int readStatus) {
            mDeliveryStatus = deliveryStatus;
            mReadStatus = readStatus;
        }
    }

    public static String getTextMessageDetails(Context context, Cursor cursor) {
        Log.d(TAG, "getTextMessageDetails");

        StringBuilder details = new StringBuilder();
        Resources res = context.getResources();

        // Message Type: Text message.
        details.append(res.getString(R.string.message_type_label));
        details.append(res.getString(R.string.text_message));
        // Address: ***
        details.append('\n');
        int smsType = cursor.getInt(cursor.getColumnIndexOrThrow(Sms.TYPE));
        if (TelephonyWrapper.Sms.isOutgoingFolder(smsType)) {
            details.append(res.getString(R.string.to_address_label));
        } else {
            details.append(res.getString(R.string.from_label));
        }

        String address = "";
        if (cursor.getString(MessageListAdapter.COLUMN_SMS_ADDRESS).contains(WAPPUSH)) {
            String[] mAddresses = cursor.getString(
                    MessageListAdapter.COLUMN_SMS_ADDRESS).split(":");
            address = mAddresses[context.getResources().getInteger(
                    R.integer.wap_push_address_index)];
        } else {
            address = cursor.getString(cursor.getColumnIndexOrThrow(Sms.ADDRESS));
        }
        // Make sure the address display normally for RTL
        if (isLayoutRtl) {
            details.append('\u202D' + address + '\u202C');
        } else {
            details.append(address);
        }

        // Sent: ***
        if (smsType == Sms.MESSAGE_TYPE_INBOX) {
            long date_sent = cursor.getLong(cursor.getColumnIndexOrThrow(Sms.DATE_SENT));
            if (date_sent > 0) {
                details.append('\n');
                details.append(res.getString(R.string.sent_label));
                details.append(MessageUtils.formatTimeStampString(context, date_sent, true));
            }
        }

        // Received: ***
        details.append('\n');
        if (smsType == Sms.MESSAGE_TYPE_DRAFT) {
            details.append(res.getString(R.string.saved_label));
        } else if (smsType == Sms.MESSAGE_TYPE_INBOX) {
            details.append(res.getString(R.string.received_label));
        } else {
            details.append(res.getString(R.string.sent_label));
        }

        long date = cursor.getLong(cursor.getColumnIndexOrThrow(Sms.DATE));
        details.append(MessageUtils.formatTimeStampString(context, date, true));

        // Delivery report
        if (TelephonyWrapper.Sms.isOutgoingFolder(smsType)) {
            long status = cursor.getLong(COLUMN_SMS_STATUS);
            // If the 31-16 bits is not 0, means this is a CDMA sms.
            if ((status >> CDMA_STATUS_SHIFT) > 0) {
                status = status >> CDMA_STATUS_SHIFT;
            }
            details.append(NEW_LINE);
            details.append(res.getString(R.string.delivery_report));
            if (status == Sms.STATUS_NONE) {
                // No delivery report requested
                details.append(res.getString(R.string.status_none));
            } else if (status >= Sms.STATUS_FAILED) {
                // Failure
                details.append(res.getString(R.string.status_failed));
            } else if (status >= Sms.STATUS_PENDING) {
                // Pending
                details.append(res.getString(R.string.status_pending));
            } else {
                // Success
                details.append(res.getString(R.string.status_received));
            }
        }

        // Error code: ***
        int errorCode = cursor.getInt(cursor.getColumnIndexOrThrow(Sms.ERROR_CODE));
        if (errorCode != 0) {
            details.append('\n')
                .append(res.getString(R.string.error_code_label))
                .append(errorCode);
        }

        return details.toString();
    }

    static private String getPriorityDescription(Context context, int PriorityValue) {
        Resources res = context.getResources();
        switch(PriorityValue) {
            case PduHeaders.PRIORITY_HIGH:
                return res.getString(R.string.priority_high);
            case PduHeaders.PRIORITY_LOW:
                return res.getString(R.string.priority_low);
            case PduHeaders.PRIORITY_NORMAL:
            default:
                return res.getString(R.string.priority_normal);
        }
    }

    public static String convertToVcardType(String src) {
        if (!TextUtils.isEmpty(src)) {
            int index = src.lastIndexOf('.');
            if (index > 0) {
                String extension = src.substring(index + 1, src.length());
                if (extension.toLowerCase().equals(VCARD)) {
                    return ContentType.TEXT_VCARD.toLowerCase();
                }
            }
        }
        return "";
    }

    public static int getAttachmentType(SlideshowModel model, MultimediaMessagePdu mmp) {
        if (model == null || mmp == null) {
            return MessageItem.ATTACHMENT_TYPE_NOT_LOADED;
        }

        int numberOfSlides = model.size();
        if (numberOfSlides > 1) {
            return WorkingMessage.SLIDESHOW;
        } else if (numberOfSlides == 1) {
            // Only one slide in the slide-show.
            SlideModel slide = model.get(0);
            if (slide.hasVideo()) {
                return WorkingMessage.VIDEO;
            }

            if (slide.hasAudio() && slide.hasImage()) {
                return WorkingMessage.SLIDESHOW;
            }

            if (slide.hasAudio()) {
                return WorkingMessage.AUDIO;
            }

            if (slide.hasImage()) {
                return WorkingMessage.IMAGE;
            }

            if (slide.hasVcard()) {
                return WorkingMessage.VCARD;
            }

            if (slide.hasText()) {
                return WorkingMessage.TEXT;
            }

            // Handle the multimedia message only has subject
            String subject = mmp.getSubject() != null ? mmp.getSubject().getString() : null;
            if (!TextUtils.isEmpty(subject)) {
                return WorkingMessage.TEXT;
            }
        }

        return MessageItem.ATTACHMENT_TYPE_NOT_LOADED;
    }

    public static String getDisplayTime(int time) {
        time = time / 1000;
        int min = time / 60;
        int sec = time % 60;
        StringBuilder builder = new StringBuilder();
        if (min < 10) {
            builder.append("0");
        }
        builder.append(min);
        builder.append(":");
        if (sec < 10) {
            builder.append("0");
        }
        builder.append(sec);
        return builder.toString();
    }

    public static void markAsNotificationThreadIfNeed(final Context context,
            final long threadId, final String number) {
        new Thread(new Runnable() {
            @Override
            public void run() {
                Conversation conversation = Conversation.get(context, threadId, true);
                if (conversation != null && conversation.getRecipients().size() > 1) {
                    return;
                }

                String stripedNumber = PhoneNumberUtils.stripSeparators(number);
                boolean needMarkAsNotificationThread = false;

                Matcher matcher = sNotificationNumberPattern.matcher(stripedNumber);
                if (matcher.matches()) {
                    needMarkAsNotificationThread = true;
                }

                if (needMarkAsNotificationThread) {
                    Uri uri = Conversation.getUri(threadId);
                    ContentValues values = new ContentValues();
                    values.put(TelephonyWrapper.NOTIFICATION, NOTIFICATION_MSG_FLAG);
                    try {
                        context.getContentResolver().update(uri, values, null, null);
                    } catch (Exception e) {
                        Log.e(TAG, "Update Threads.NOTIFICATION Error", e);
                    }
                }
            }
        }).start();
    }

    public static String getPath(Context context, MessageItem messageItem) throws IOException {
        String path = null;
        Uri partURI = null;
        if (messageItem == null || messageItem.getSlideshow() == null) {
            return null;
        }
        ListIterator<SlideModel> itera = messageItem.getSlideshow().listIterator();
        while (itera.hasNext()) {
            SlideModel sm = itera.next();
            int type = messageItem.mAttachmentType;
            if (type == WorkingMessage.AUDIO && sm.getAudio() != null) {
                partURI = sm.getAudio().getUri();
                break;
            } else if (type == WorkingMessage.VIDEO && sm.getVideo() != null) {
                partURI = sm.getVideo().getUri();
                break;
            } else if (type == WorkingMessage.IMAGE && sm.getImage() != null) {
                partURI = sm.getImage().getUri();
                break;
            }
        }
        if (partURI == null) {
            return null;
        }
        return partURI.toString();
    }

    public static void updateThreadAttachTypeByThreadId(Context context, long threadId) {
        String attachmentInfo = Conversation.getAttachmentInfo(context,
                Conversation.getLatestMessageAttachmentUri(context, threadId));
        Uri uri = Conversation.getUri(threadId);
        ContentValues values = new ContentValues();
        values.put(TelephonyWrapper.ATTACHMENT_INFO, attachmentInfo);
        try {
            context.getContentResolver().update(uri, values, null, null);
        } catch (Exception e) {
            Log.e(TAG, "Update Thread Attachment Type Error", e);
        }
    }

    public static String formatTimeStampString(Context context, long when) {
        return formatTimeStampString(context, when, false);
    }

    public static String formatTimeStampString(Context context, long when, boolean fullFormat) {
        Time then = new Time();
        then.set(when);
        Time now = new Time();
        now.setToNow();

        // Basic settings for formatDateTime() we want for all cases.
        int format_flags = DateUtils.FORMAT_NO_NOON_MIDNIGHT |
                           DateUtils.FORMAT_ABBREV_ALL |
                           DateUtils.FORMAT_CAP_AMPM;

        // If the caller has asked for full details, make sure to show the date
        // and time no matter what we've determined above (but still make showing
        // the year only happen if it is a different year from today).
        if (fullFormat) {
            format_flags |= (DateUtils.FORMAT_SHOW_DATE | DateUtils.FORMAT_SHOW_TIME);
        } else {
            long timeDiff = System.currentTimeMillis() - when;
            if (timeDiff < DateUtils.MINUTE_IN_MILLIS) {
                // If the message is in 1 minute, show "now"
                return context.getResources().getString(R.string.posted_now);
            } else if (timeDiff < DateUtils.HOUR_IN_MILLIS) {
                // If the message is 1 minute ago but in 1 hour, show x=1,2... minute(s)
                long count = (timeDiff / DateUtils.MINUTE_IN_MILLIS);
                String format = context.getResources().getQuantityString(
                        R.plurals.num_minutes_ago, (int) count);
                return String.format(format, count);
            } else if (then.yearDay == now.yearDay) {
                // If the message is 1 hour ago but in 1 day, show exactly time, sample as 15:08
                format_flags |= DateUtils.FORMAT_SHOW_TIME;
            } else if (timeDiff < DateUtils.WEEK_IN_MILLIS) {
                // If the message is 1 day ago but in 1 week, show weekday and exactly time
                format_flags |= DateUtils.FORMAT_SHOW_WEEKDAY | DateUtils.FORMAT_SHOW_TIME;
            } else if (then.year == now.year) {
                // If the message is 1 week ago but in 1 year, show date and exactly time
                format_flags |= DateUtils.FORMAT_SHOW_DATE | DateUtils.FORMAT_SHOW_TIME;
            } else {
                // If the message is from a different year, show the date and year.
                format_flags |= DateUtils.FORMAT_SHOW_YEAR | DateUtils.FORMAT_SHOW_DATE;
            }
        }

        return DateUtils.formatDateTime(context, when, format_flags);
    }

    public static void selectAudio(final Activity activity, final int requestCode) {
        Intent audioIntent = new Intent();
        audioIntent.setAction(Intent.ACTION_PICK);
        audioIntent.setData(MediaStore.Audio.Media.EXTERNAL_CONTENT_URI);
        try {
            activity.startActivityForResult(audioIntent, requestCode);
        } catch (ActivityNotFoundException ex) {
            Toast.makeText(activity, R.string.audio_picker_app_not_found,
                    Toast.LENGTH_SHORT).show();
        }
    }

    public static void recordSound(Activity activity, int requestCode, long sizeLimit) {
        Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
        intent.setType(ContentType.AUDIO_AMR);
        intent.setClassName("com.android.soundrecorder",
                "com.android.soundrecorder.SoundRecorder");
        intent.putExtra(android.provider.MediaStore.Audio.Media.EXTRA_MAX_BYTES, sizeLimit);
        intent.putExtra(EXIT_AFTER_RECORD, true);
        activity.startActivityForResult(intent, requestCode);
    }

    public static void recordVideo(Activity activity, int requestCode, long sizeLimit) {
        // The video recorder can sometimes return a file that's larger than the max we
        // say we can handle. Try to handle that overshoot by specifying an 85% limit.
        sizeLimit *= .85F;

        int durationLimit = getVideoCaptureDurationLimit(sizeLimit);

        if (Log.isLoggable(LogTag.APP, Log.VERBOSE)) {
            log("recordVideo: durationLimit: " + durationLimit +
                    " sizeLimit: " + sizeLimit);
        }

        Intent intent = new Intent(MediaStore.ACTION_VIDEO_CAPTURE);
        intent.putExtra(MediaStore.EXTRA_VIDEO_QUALITY, 0);
        intent.putExtra("android.intent.extra.sizeLimit", sizeLimit);
        intent.putExtra("android.intent.extra.durationLimit", durationLimit);
        intent.putExtra(MediaStore.EXTRA_OUTPUT, TempFileProvider.SCRAP_CONTENT_URI);
        activity.startActivityForResult(intent, requestCode);
    }

    public static void capturePicture(Activity activity, int requestCode) {
        Intent intent = new Intent(MediaStore.ACTION_IMAGE_CAPTURE);
        intent.putExtra(MediaStore.EXTRA_OUTPUT, TempFileProvider.SCRAP_CONTENT_URI);
        activity.startActivityForResult(intent, requestCode);
    }

    // Public for until tests
    public static int getVideoCaptureDurationLimit(long bytesAvailable) {
        CamcorderProfile camcorder = null;
        try {
            camcorder = CamcorderProfile.get(CamcorderProfile.QUALITY_LOW);
        } catch (RuntimeException ex) {
            Log.e(TAG, "RuntimeException caught while getting camera info", ex);
        }

        if (camcorder == null) {
            return 0;
        }
        bytesAvailable *= 8;        // convert to bits
        long seconds = bytesAvailable / (camcorder.audioBitRate + camcorder.videoBitRate);

        // Find the best match for one of the fixed durations
        for (int i = sVideoDuration.length - 1; i >= 0; i--) {
            if (seconds >= sVideoDuration[i]) {
                return sVideoDuration[i];
            }
        }
        return 0;
    }

    public static void selectVideo(Context context, int requestCode) {
        selectMediaByType(context, requestCode, ContentType.VIDEO_UNSPECIFIED, true);
    }

    public static void selectImage(Context context, int requestCode) {
        selectMediaByType(context, requestCode, ContentType.IMAGE_UNSPECIFIED, false);
    }

    private static void selectMediaByType(
            Context context, int requestCode, String contentType, boolean localFilesOnly) {
         if (context instanceof Activity) {

            Intent innerIntent = new Intent(Intent.ACTION_GET_CONTENT);

            innerIntent.setType(contentType);
            if (localFilesOnly) {
                innerIntent.putExtra(Intent.EXTRA_LOCAL_ONLY, true);
            }

            Intent wrapperIntent = Intent.createChooser(innerIntent, null);

            ((Activity) context).startActivityForResult(wrapperIntent, requestCode);
        }
    }

    public static void viewSimpleSlideshow(Context context, SlideshowModel slideshow) {
        if (!slideshow.isSimple()) {
            throw new IllegalArgumentException(
                    "viewSimpleSlideshow() called on a non-simple slideshow");
        }
        SlideModel slide = slideshow.get(0);
        MediaModel mm = null;
        if (slide.hasImage()) {
            mm = slide.getImage();
        } else if (slide.hasVideo()) {
            mm = slide.getVideo();
        } else if (slide.hasVcard()) {
            mm = slide.getVcard();
            String lookupUri = ((VcardModel) mm).getLookupUri();

            Intent intent = new Intent(Intent.ACTION_VIEW);
            if (!TextUtils.isEmpty(lookupUri) && lookupUri.contains("contacts")) {
                // if the uri is from the contact, we suggest to view the contact.
                intent.setData(Uri.parse(lookupUri));
            } else {
                // we need open the saved part.
                intent.setDataAndType(mm.getUri(), ContentType.TEXT_VCARD.toLowerCase());
                intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            }
            // distinguish view vcard from mms or contacts.
            intent.putExtra(VIEW_VCARD, true);
            context.startActivity(intent);
            return;
        }

        if (mm == null) {
            return;
        }

        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        intent.putExtra("SingleItemOnly", true); // So we don't see "surrounding" images in Gallery

        String contentType;
        contentType = mm.getContentType();
        intent.setDataAndType(getUriFromFile(context, mm),
                contentType);
        context.startActivity(intent);
    }

    private static Uri getUriFromFile(Context context, MediaModel mm) {
        Uri uri = mm.getUri();
        if (uri != null && mm.isFileUri(uri)) {
            return FileProvider.getUriForFile(context,
                    context.getPackageName() + ".fileProvider",
                    new File(uri.getPath()));
        }
        return uri;
    }

    public static void showErrorDialog(Activity activity,
            String title, String message) {
        if (activity.isFinishing()) {
            return;
        }
        if (!sErrorDialogIsShown) {
            AlertDialog.Builder builder = new AlertDialog.Builder(activity);

            builder.setIcon(R.drawable.ic_sms_mms_not_delivered);
            builder.setTitle(title);
            builder.setMessage(message);
            builder.setPositiveButton(android.R.string.ok, new OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int which) {
                    if (which == DialogInterface.BUTTON_POSITIVE) {
                        dialog.dismiss();
                    }
                }
            });
            builder.setOnDismissListener(new DialogInterface.OnDismissListener() {
                @Override
                public void onDismiss(DialogInterface dialog) {
                    sErrorDialogIsShown = false;
                }
            });
            builder.show();
            sErrorDialogIsShown = true;
        }
    }

    /**
     * The quality parameter which is used to compress JPEG images.
     */
    public static final int IMAGE_COMPRESSION_QUALITY = 95;
    /**
     * The minimum quality parameter which is used to compress JPEG images.
     */
    public static final int MINIMUM_IMAGE_COMPRESSION_QUALITY = 50;

    /**
     * Message overhead that reduces the maximum image byte size.
     * 5000 is a realistic overhead number that allows for user to also include
     * a small MIDI file or a couple pages of text along with the picture.
     */
    public static final int MESSAGE_OVERHEAD = 5000;

    public static void resizeImageAsync(final Context context,
            final Uri imageUri, final int currentMsgSize, final Handler handler,
            final ResizeImageResultCallback cb,
            final boolean append) {

        // Show a progress toast if the resize hasn't finished
        // within one second.
        // Stash the runnable for showing it away so we can cancel
        // it later if the resize completes ahead of the deadline.
        final Runnable showProgress = new Runnable() {
            @Override
            public void run() {
                Toast.makeText(context, R.string.compressing, Toast.LENGTH_SHORT).show();
            }
        };
        // Schedule it for one second from now.
        handler.postDelayed(showProgress, 1000);

        new Thread(new Runnable() {
            @Override
            public void run() {
                final PduPart part;
                try {
                    UriImage image = new UriImage(context, imageUri);
                    int widthLimit = MmsConfig.getMaxImageWidth();
                    int heightLimit = MmsConfig.getMaxImageHeight();
                    // In mms_config.xml, the max width has always been declared larger than the max
                    // height. Swap the width and height limits if necessary so we scale the picture
                    // as little as possible.
                    if (image.getHeight() > image.getWidth()) {
                        int temp = widthLimit;
                        widthLimit = heightLimit;
                        heightLimit = temp;
                    }

                   int maxSize = MmsConfig.getMaxRestrictedMessageSize();
                   ContentRestriction cr = ContentRestrictionFactory.getContentRestriction();
                   int creationMode = ContentRestrictionFactory.getUsedCreationMode();
                   switch (creationMode) {
                   case MmsConfig.CREATIONMODE_RESTRICTED:
                   case MmsConfig.CREATIONMODE_WARNING:
                        maxSize = MmsConfig.getMaxRestrictedMessageSize();
                        break;
                   case MmsConfig.CREATIONMODE_FREE:
                   default:
                        maxSize = MmsConfig.getMaxMessageSize();
                        break;
                    }
                    part = image.getResizedImageAsPart(
                        widthLimit,
                        heightLimit,
                        MmsConfig.getMaxMessageSize() - currentMsgSize - MESSAGE_OVERHEAD);
                } finally {
                    // Cancel pending show of the progress toast if necessary.
                    handler.removeCallbacks(showProgress);
                }

                handler.post(new Runnable() {
                    @Override
                    public void run() {
                        cb.onResizeResult(part, append);
                    }
                });
            }
        }, "MessageUtils.resizeImageAsync").start();
    }

    public static void showDiscardDraftConfirmDialog(Context context,
            OnClickListener listener, int validNum) {
        // the alert icon shoud has black triangle and white exclamation mark in white background.
        new AlertDialog.Builder(context)
                .setIconAttribute(android.R.attr.alertDialogIcon)
                .setTitle(R.string.discard_message)
                .setMessage(getDiscardMessageId(validNum))
                .setPositiveButton(R.string.yes, listener)
                .setNegativeButton(R.string.no, null)
                .show();
    }

    /**
    * Return discard message id.
    */
    private static int getDiscardMessageId(int validNum) {
        int msgId = R.string.discard_message_reason;
        // If validNum != ALL_RECIPIENTS_EMPTY, means recipient is not empty.
        // If validNum ==  ALL_RECIPIENTS_VALID, means all of the recipients are valid.
        // If validNum > ALL_RECIPIENTS_VALID, means there some recipients are invalid.
        // If validNum == ALL_RECIPIENTS_INVALID, means all of the recipients are invalid.
        if (ALL_RECIPIENTS_EMPTY != validNum) {
            msgId = validNum > ALL_RECIPIENTS_VALID ? R.string.discard_message_reason_some_invalid
                : R.string.discard_message_reason_all_invalid;
        }
        return msgId;
    }

    public static String getLocalNumber() {
        return getLocalNumber(SubscriptionManager.getDefaultDataSubscriptionId());
    }

    public static String getLocalNumber(int subId) {
        TelephonyManager tm = (TelephonyManager) MmsApp.getApplication().
                getSystemService(Context.TELEPHONY_SERVICE);
        sLocalNumber = TelephonyManagerWrapper.getLine1Number(tm, subId);
        return sLocalNumber;
    }

    public static void markAsReadForNotificationMessage(Context context) {
        HashSet<Long> ids = getUnreadNotificationThreadIds(context);
        Iterator<Long> it = ids.iterator();
        while (it.hasNext()) {
            long threadId = it.next();
            Conversation conv = Conversation.get(context, threadId, true);
            if (conv != null) {
                conv.markAsRead();
            }
        }
    }

    public static HashSet<Long> getUnreadNotificationThreadIds(Context context) {
        HashSet<Long> unreadNotificationThreadIds = new HashSet<Long>();
        Cursor cursor = null;
        try {
            cursor = SqliteWrapper.query(context, context.getContentResolver(),
                    Conversation.sAllThreadsUri, new String[] {Threads._ID, Threads.READ},
                    TelephonyWrapper.NOTIFICATION + " = 1 AND " + Threads.READ + " = 0 ", null, null);
            if (cursor != null && cursor.getCount() != 0) {
                while (cursor.moveToNext()) {
                    long threadId = cursor.getLong(cursor.getColumnIndexOrThrow(Threads._ID));
                    unreadNotificationThreadIds.add(threadId);
                }
            }
        } finally {
            if (cursor != null) {
                cursor.close();
            }
        }

        return unreadNotificationThreadIds;
    }

    public static boolean isLocalNumber(String number) {
        if (number == null) {
            return false;
        }

        // we don't use Mms.isEmailAddress() because it is too strict for comparing addresses like
        // "foo+caf_=6505551212=tmomail.net@gmail.com", which is the 'from' address from a forwarded email
        // message from Gmail. We don't want to treat "foo+caf_=6505551212=tmomail.net@gmail.com" and
        // "6505551212" to be the same.
        if (number.indexOf('@') >= 0) {
            return false;
        }

        return PhoneNumberUtils.compare(number, getLocalNumber());
    }

    public static void handleReadReport(final Context context,
            final Collection<Long> threadIds,
            final int status,
            final Runnable callback) {
        StringBuilder selectionBuilder = new StringBuilder(Mms.MESSAGE_TYPE + " = "
                + PduHeaders.MESSAGE_TYPE_RETRIEVE_CONF
                + " AND " + Mms.READ + " = 0"
                + " AND " + Mms.READ_REPORT + " = " + PduHeaders.VALUE_YES);

        String[] selectionArgs = null;
        if (threadIds != null && threadIds.size() > 0) {
            String threadIdSelection = null;
            StringBuilder buf = new StringBuilder(Mms.THREAD_ID);
            buf.append(" in (");
            int i = 0;

            for (long threadId : threadIds) {
                if (i > 0) {
                    buf.append(",");
                }
                buf.append(threadId);
                i++;
            }
            buf.append(")");
            threadIdSelection = buf.toString();

            selectionBuilder.append(" AND (" + threadIdSelection + ")");
        }

        final Cursor c = SqliteWrapper.query(context, context.getContentResolver(),
                        Mms.Inbox.CONTENT_URI, new String[] {
                            Mms._ID, Mms.MESSAGE_ID, Mms.SUBSCRIPTION_ID},
                        selectionBuilder.toString(), null, null);

        if (c == null) {
            return;
        }

        final Map<String, String> addressMap = new HashMap<String, String>();
        final Map<String, Integer> phoneIdMap = new HashMap<String, Integer>();
        try {
            if (c.getCount() == 0) {
                if (callback != null) {
                    callback.run();
                }
                return;
            }

            while (c.moveToNext()) {
                Uri uri = ContentUris.withAppendedId(Mms.CONTENT_URI,
                        c.getLong(MESSAGE_REPORT_COLUMN_ID));
                addressMap.put(c.getString(MESSAGE_REPORT_COLUMN_MESSAGE_ID),
                        AddressUtils.getFrom(context, uri));
                phoneIdMap.put(c.getString(MESSAGE_REPORT_COLUMN_MESSAGE_ID),
                        c.getInt(MESSAGE_REPORT_COLUMN_SUB_ID));
            }
        } finally {
            c.close();
        }

        OnClickListener positiveListener = new OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                for (final Map.Entry<String, String> entry : addressMap.entrySet()) {
                    String mId = entry.getKey();
                    int phoneId = phoneIdMap.get(mId);
                    MmsMessageSender.sendReadRec(context, entry.getValue(),
                            mId, phoneId, status);
                }

                if (callback != null) {
                    callback.run();
                }
                dialog.dismiss();
            }
        };

        OnClickListener negativeListener = new OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                if (callback != null) {
                    callback.run();
                }
                dialog.dismiss();
            }
        };

        OnCancelListener cancelListener = new OnCancelListener() {
            @Override
            public void onCancel(DialogInterface dialog) {
                if (callback != null) {
                    callback.run();
                }
                dialog.dismiss();
            }
        };

        confirmReadReportDialog(context, positiveListener,
                                         negativeListener,
                                         cancelListener);
    }

    private static void confirmReadReportDialog(Context context,
            OnClickListener positiveListener, OnClickListener negativeListener,
            OnCancelListener cancelListener) {
        AlertDialog.Builder builder = new AlertDialog.Builder(context);
        builder.setCancelable(true);
        builder.setTitle(R.string.confirm);
        builder.setMessage(R.string.message_send_read_report);
        builder.setPositiveButton(R.string.yes, positiveListener);
        builder.setNegativeButton(R.string.no, negativeListener);
        builder.setOnCancelListener(cancelListener);
        builder.show();
    }

    public static String extractEncStrFromCursor(Cursor cursor,
            int columnRawBytes, int columnCharset) {
        String rawBytes = cursor.getString(columnRawBytes);
        int charset = cursor.getInt(columnCharset);

        if (TextUtils.isEmpty(rawBytes)) {
            return "";
        } else if (charset == CharacterSets.ANY_CHARSET) {
            return rawBytes;
        } else {
            return new EncodedStringValue(charset, PduPersister.getBytes(rawBytes)).getString();
        }
    }

    private static String extractEncStr(Context context, EncodedStringValue value) {
        if (value != null) {
            return value.getString();
        } else {
            return "";
        }
    }

    public static ArrayList<String> extractUris(URLSpan[] spans) {
        int size = spans.length;
        ArrayList<String> accumulator = new ArrayList<String>();

        for (int i = 0; i < size; i++) {
            accumulator.add(spans[i].getURL());
        }
        return accumulator;
    }

    public static String getRecipientsByIds(Context context,
            String recipientIds, boolean allowQuery) {
        String value = sRecipientAddress.get(recipientIds);
        if (value != null) {
            return value;
        }
        if (!TextUtils.isEmpty(recipientIds)) {
            StringBuilder addressBuf = extractIdsToAddresses(context, recipientIds, allowQuery);
            if (addressBuf == null) {
                // temporary error? Don't memoize.
                return "";
            }
            value = addressBuf.toString();
        } else {
            value = "";
        }
        sRecipientAddress.put(recipientIds, value);
        return value;
    }

    private static StringBuilder extractIdsToAddresses(Context context, String recipients,
            boolean allowQuery) {
        StringBuilder addressBuf = new StringBuilder();
        String[] recipientIds = recipients.split(" ");
        boolean firstItem = true;
        for (String recipientId : recipientIds) {
            String value = sRecipientAddress.get(recipientId);

            if (value == null) {
                if (!allowQuery) {
                    // when allowQuery is false, if any value from
                    // sRecipientAddress.get() is null,
                    // return null for the whole thing. We don't want to stick
                    // partial result
                    // into sRecipientAddress for multiple recipient ids.
                    return null;
                }

                Uri uri = Uri.parse("content://mms-sms/canonical-address/" + recipientId);
                Cursor c = SqliteWrapper.query(context, context.getContentResolver(), uri, null,
                        null, null, null);
                if (c != null) {
                    try {
                        if (c.moveToFirst()) {
                            value = c.getString(0);
                            sRecipientAddress.put(recipientId, value);
                        }
                    } finally {
                        c.close();
                    }
                }
            }
            if (value == null) {
                continue;
            }
            if (firstItem) {
                firstItem = false;
            } else {
                addressBuf.append(";");
            }
            addressBuf.append(value);
        }

        return (addressBuf.length() == 0) ? null : addressBuf;
    }

    public static String getAddressByThreadId(Context context, long threadId) {
        String[] projection = new String[] {
                Threads.RECIPIENT_IDS
        };

        Uri.Builder builder = Threads.CONTENT_URI.buildUpon();
        builder.appendQueryParameter("simple", "true");
        Cursor cursor = SqliteWrapper.query(context, context.getContentResolver(), builder.build(),
                projection, Threads._ID + "=" + threadId, null, null);

        if (cursor != null) {
            try {
                if ((cursor.getCount() == 1) && cursor.moveToFirst()) {
                    String address = getRecipientsByIds(context, cursor.getString(0),
                            true /* allow query */);
                    if (!TextUtils.isEmpty(address)) {
                        return address;
                    }
                }
            } finally {
                cursor.close();
            }
        }
        return null;
    }

    /**
     * Play/view the message attachments.
     * TOOD: We need to save the draft before launching another activity to view the attachments.
     *       This is hacky though since we will do saveDraft twice and slow down the UI.
     *       We should pass the slideshow in intent extra to the view activity instead of
     *       asking it to read attachments from database.
     * @param activity
     * @param msgUri the MMS message URI in database
     * @param slideshow the slideshow to save
     * @param persister the PDU persister for updating the database
     * @param sendReq the SendReq for updating the database
     */
    public static void viewMmsMessageAttachment(Activity activity, Uri msgUri,
            SlideshowModel slideshow, AsyncDialog asyncDialog) {
        viewMmsMessageAttachment(activity, msgUri, slideshow, 0, asyncDialog);
    }

    public static void viewMmsMessageAttachment(final Activity activity, final Uri msgUri,
            final SlideshowModel slideshow, final int requestCode, AsyncDialog asyncDialog) {
        boolean isSimple = (slideshow == null) ? false : slideshow.isSimple();
        if (isSimple) {
            // In attachment-editor mode, we only ever have one slide.
            MessageUtils.viewSimpleSlideshow(activity, slideshow);
        } else {
            // The user wants to view the slideshow. We have to persist the slideshow parts
            // in a background task. If the task takes longer than a half second, a progress dialog
            // is displayed. Once the PDU persisting is done, another runnable on the UI thread get
            // executed to start the SlideshowActivity.
            asyncDialog.runAsync(new Runnable() {
                @Override
                public void run() {
                    // If a slideshow was provided, save it to disk first.
                    if (slideshow != null) {
                        PduPersister persister = PduPersister.getPduPersister(activity);
                        try {
                            PduBody pb = slideshow.toPduBody();
                            persister.updateParts(msgUri, pb, null);
                            slideshow.sync(pb);
                        } catch (MmsException e) {
                            Log.e(TAG, "Unable to save message for preview");
                            return;
                        }
                    }
                }
            }, new Runnable() {
                @Override
                public void run() {
                    // Once the above background thread is complete, this runnable is run
                    // on the UI thread to launch the slideshow activity.
                    launchSlideshowActivity(activity, msgUri, requestCode);
                }
            }, R.string.building_slideshow_title);
        }
    }

    public static void launchSlideshowActivity(Context context, Uri msgUri, int requestCode) {
        // Launch the slideshow activity to play/view.
        Intent intent = new Intent(context, MobilePaperShowActivity.class);
        intent.setData(msgUri);
        intent.setFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP);
        if (requestCode > 0 && context instanceof Activity) {
            ((Activity)context).startActivityForResult(intent, requestCode);
        } else {
            context.startActivity(intent);
        }

    }

    public static void showSmsMessageContent(Context context, long msgId) {
        Intent i = new Intent(context, MailBoxMessageContent.class);
        i.setData(ContentUris.withAppendedId(Sms.CONTENT_URI, msgId));
        context.startActivity(i);
    }

    /**
     * Debugging
     */
    public static void writeHprofDataToFile(){
        String filename = Environment.getExternalStorageDirectory() + "/mms_oom_hprof_data";
        try {
            android.os.Debug.dumpHprofData(filename);
            Log.i(TAG, "##### written hprof data to " + filename);
        } catch (IOException ex) {
            Log.e(TAG, "writeHprofDataToFile: caught " + ex);
        }
    }

    // An alias (or commonly called "nickname") is:
    // Nickname must begin with a letter.
    // Only letters a-z, numbers 0-9, or . are allowed in Nickname field.
    public static boolean isAlias(String string) {
        if (!MmsConfig.isAliasEnabled()) {
            return false;
        }

        int len = string == null ? 0 : string.length();

        if (len < MmsConfig.getAliasMinChars() || len > MmsConfig.getAliasMaxChars()) {
            return false;
        }

        if (!Character.isLetter(string.charAt(0))) {    // Nickname begins with a letter
            return false;
        }
        for (int i = 1; i < len; i++) {
            char c = string.charAt(i);
            if (!(Character.isLetterOrDigit(c) || c == '.')) {
                return false;
            }
        }

        return true;
    }

    /**
     * Given a phone number, return the string without syntactic sugar, meaning parens,
     * spaces, slashes, dots, dashes, etc. If the input string contains non-numeric
     * non-punctuation characters, return null.
     */
    private static String parsePhoneNumberForMms(String address) {
        StringBuilder builder = new StringBuilder();
        int len = address.length();

        for (int i = 0; i < len; i++) {
            char c = address.charAt(i);

            // accept the first '+' in the address
            if (c == '+' && builder.length() == 0) {
                builder.append(c);
                continue;
            }

            if (Character.isDigit(c)) {
                builder.append(c);
                continue;
            }

            if (numericSugarMap.get(c) == null) {
                return null;
            }
        }
        return builder.toString();
    }

    /**
     * Returns true if the address passed in is a valid MMS address.
     */
    public static boolean isValidMmsAddress(String address) {
        String retVal = parseMmsAddress(address);
        return (retVal != null);
    }

    /**
     * Returns true if the address passed in is a Browser wap push MMS address.
     */
    public static boolean isWapPushNumber(String address) {
        if (TextUtils.isEmpty(address)) {
            return false;
        } else {
            return address.contains(WAPPUSH);
        }
    }

    public static String getWapPushNumber(String address) {
        String[] number = address.split(":");
        int index = MmsApp.getApplication().getResources()
                .getInteger(R.integer.wap_push_address_index);
        return number[index];
    }

    /**
     * Returns true if the address is callable.
     */
    public static boolean isRecipientCallable(String address) {
        return (!ContactRecipientEntryUtils.isEmailAddress(address) && !isWapPushNumber(address));
    }

    /**
     * parse the input address to be a valid MMS address.
     * - if the address is an email address, leave it as is.
     * - if the address can be parsed into a valid MMS phone number, return the parsed number.
     * - if the address is a compliant alias address, leave it as is.
     */
    public static String parseMmsAddress(String address) {
        // if it's a valid Email address, use that.
        if (ContactRecipientEntryUtils.isEmailAddress(address)) {
            return address;
        }

        // if we are able to parse the address to a MMS compliant phone number, take that.
        String retVal = parsePhoneNumberForMms(address);
        if (retVal != null && retVal.length() != 0) {
            return retVal;
        }

        // if it's an alias compliant address, use that.
        if (isAlias(address)) {
            return address;
        }

        // it's not a valid MMS address, return null
        return null;
    }

    public static void dialRecipient(Context context, String address, int subscription) {
        if (!ContactRecipientEntryUtils.isEmailAddress(address)) {
            Intent dialIntent = new Intent(Intent.ACTION_CALL, Uri.parse("tel:" + address));
            if (isMultiSimEnabledMms()) {
                dialIntent.putExtra(ConstantsWrapper.Phone.SUBSCRIPTION_KEY, subscription);
            }
            context.startActivity(dialIntent);
        }
    }

    public static void dialNumber(Context context, String number) {
        Intent dialIntent = new Intent(Intent.ACTION_CALL, Uri.parse("tel:"
                + number));
        context.startActivity(dialIntent);
    }

    /**
     * Return whether it has card in according slot -the input subscription is 0
     * or 1 -It is only used in DSDS
     */
    public static boolean hasIccCard(int subscription) {
        boolean hasCard = false;
        TelephonyManager telephonyManager = (TelephonyManager)MmsApp.
                getApplication().getSystemService(Context.TELEPHONY_SERVICE);
        if (isMultiSimEnabledMms()) {
            hasCard = TelephonyManagerWrapper.hasIccCard(telephonyManager, subscription);
        } else {
            if (subscription == SubscriptionManager.getDefaultSubscriptionId()) {
                hasCard = telephonyManager.hasIccCard();
            }
        }
        return hasCard;
    }

    /**
     * Return whether it has card no matter in DSDS or not
     */
    public static boolean hasIccCard() {
        TelephonyManager tm = (TelephonyManager)MmsApp.
                getApplication().getSystemService(Context.TELEPHONY_SERVICE);
        return tm.hasIccCard();
    }

    public static void setSmscForSub(Context context, int sub, String smsc, Message callback) {
        if (callback == null) {
            return;
        } else {
            callback.replyTo = new Messenger(callback.getTarget());
        }
        IExtTelephony extTelephony = IExtTelephony.Stub.asInterface(ServiceManager
                .getService("extphone"));
        boolean ret = false;
        Bundle bundle = new Bundle();
        try {
            ret = extTelephony.setSmscAddress(sub, smsc);
        } catch (NullPointerException e) {
            log(METHOD_SET_SMSC + e);
            bundle.putSerializable(EXTRA_EXCEPTION, e);
        } catch (RemoteException ex) {
            log(METHOD_SET_SMSC + ex);
            bundle.putSerializable(EXTRA_EXCEPTION, ex);
        }
        log("Set: sub = " + sub + " smsc= " + smsc + " ret=" + ret);
        bundle.putBoolean(EXTRA_SMSC_RESULT, ret);
        smscResponse(bundle, callback);
    }

    public static void getSmscFromSub(Context context, int sub, Message callback) {
        if (callback == null) {
            return;
        }
        IExtTelephony extTelephony = IExtTelephony.Stub.asInterface(ServiceManager
                .getService("extphone"));
        String smsc = null;
        Bundle bundle = new Bundle();
        try {
            smsc = extTelephony.getSmscAddress(sub);
        } catch (NullPointerException e) {
            log(METHOD_GET_SMSC + e);
            bundle.putSerializable(EXTRA_EXCEPTION, e);
        } catch (RemoteException ex) {
            log(METHOD_GET_SMSC + ex);
            bundle.putSerializable(EXTRA_EXCEPTION, ex);
        }
        log("Get: sub = " + sub + " smsc=" + smsc);
        bundle.putString(EXTRA_SMSC, smsc);
        bundle.putBoolean(EXTRA_SMSC_RESULT, (null != smsc) ? true : false);
        smscResponse(bundle, callback);
    }

    public static void smscResponse(Bundle bundle, Message callback) {
        if (callback.obj != null && callback.obj instanceof Parcelable) {
            if (bundle == null) {
                bundle = new Bundle();
            }
            bundle.putParcelable(EXTRA_USEROBJ, (Parcelable) callback.obj);
        }
        callback.obj = bundle;
        if (callback.replyTo != null) {
            try {
                callback.replyTo.send(callback);
            } catch (RemoteException e) {
                Log.d(TAG, "failed to response result", e);
            }
        } else if (callback.getTarget() != null) {
            callback.sendToTarget();
        } else {
            Log.d(TAG, "smscResponse, replyTo and target are all null!");
        }
    }


    /**
     * Return whether the card is activated according to Subscription
     * used for DSDS
     */
    public static boolean isIccCardActivated(int subscription) {
        TelephonyManager tm = (TelephonyManager)MmsApp.
                getApplication().getSystemService(Context.TELEPHONY_SERVICE);
        int simState = TelephonyManagerWrapper.getSimState(tm, subscription);
        return (simState != TelephonyManager.SIM_STATE_ABSENT)
                    && (simState != TelephonyManager.SIM_STATE_UNKNOWN);
    }
    public static boolean isIccCardActivated() {
        TelephonyManager tm = (TelephonyManager)MmsApp.
                getApplication().getSystemService(Context.TELEPHONY_SERVICE);
        int simState = tm.getSimState();
        return (simState != TelephonyManager.SIM_STATE_ABSENT)
                    && (simState != TelephonyManager.SIM_STATE_UNKNOWN);
    }

    public static boolean hasActivatedIccCard(int subscription) {
        TelephonyManager tm = (TelephonyManager)MmsApp.
                getApplication().getSystemService(Context.TELEPHONY_SERVICE);
        return TelephonyManagerWrapper.isMultiSimEnabled(tm) ?
                isIccCardActivated(subscription) :
                isIccCardActivated(SubscriptionManager.getDefaultSubscriptionId());
    }

    public static Drawable getMultiSimIcon(Context context, int subscription) {
        if (context == null) {
            // If the context is null, return 0 as no resource found.
            return null;
        }

        final TelecomManager telecomManager = (TelecomManager) context
                .getSystemService(Context.TELECOM_SERVICE);
        final TelephonyManager telephonyManager = (TelephonyManager) context
                .getSystemService(Context.TELEPHONY_SERVICE);

        Iterator<PhoneAccountHandle> pHandles = telecomManager.
                getCallCapablePhoneAccounts().listIterator();

        while (pHandles.hasNext()) {
            PhoneAccount account = telecomManager.getPhoneAccount(pHandles.next());
            int subId = TelephonyManagerWrapper.getSubIdForPhoneAccount(
                        telephonyManager, account);
            if (subId == subscription) {
                return account.getIcon().loadDrawable(context);
            }
        }

        Log.d(TAG, "phoneAccountHandle is null");
        return null;
    }

    private static void log(String msg) {
        Log.d(TAG, "[MsgUtils] " + msg);
    }

    public static boolean isMailboxMode() {
        SharedPreferences sp = PreferenceManager.getDefaultSharedPreferences(MmsApp
                .getApplication());
        boolean ViewMode = sp.getBoolean(VIEW_MODE_NAME, false);
        return ViewMode;
    }

    public static void setMailboxMode(boolean mode) {
        SharedPreferences sp = PreferenceManager.getDefaultSharedPreferences(MmsApp
                .getApplication());
        sp.edit().putBoolean(VIEW_MODE_NAME, mode).commit();
    }

    public static int getFontSize() {
        SharedPreferences sp = PreferenceManager
                .getDefaultSharedPreferences(MmsApp
                        .getApplication());
        int mFontSize = Integer
                .parseInt(sp.getString(
                        MessagingPreferenceActivity.FONT_SIZE_SETTING,
                        Integer.toString(DEFAULT_FONT_SIZE)));
        return mFontSize;
    }

    /**
     * Return the sim name of subscription.
     */
    public static String getMultiSimName(Context context, int subscription) {
        TelephonyManager tm = (TelephonyManager)context.getSystemService(Context.TELEPHONY_SERVICE);
        if (subscription >= tm.getPhoneCount() || subscription < 0) {
            return null;
        }
        //String multiSimName = Settings.System.getString(context.getContentResolver(),
        //        MULTI_SIM_NAME + (subscription + 1));
        //if (multiSimName == null) {
            if (subscription == SUB1) {
                return context.getString(R.string.slot1);
            } else if (subscription == SUB2) {
                return context.getString(R.string.slot2);
            }
        //}
        return context.getString(R.string.slot1);
    }

    public static long getSimThreadByPhoneId(int phoneId) {
        switch (phoneId) {
            case SUB1:
                return MessagingNotification.THREAD_SIM1;
            case SUB2:
                return MessagingNotification.THREAD_SIM2;
            default:
                return MessagingNotification.THREAD_SIM;
        }
    }

    public static String getAddressByName(Context context, String name) {
        String resultAddr = "";
        if (TextUtils.isEmpty(name)) {
            return resultAddr;
        }
        Cursor c = null;
        Uri nameUri = null;
        // Replace the ' to avoid SQL injection.
        name = name.replace(REPLACE_QUOTES_1, REPLACE_QUOTES_2);

        try {
            c = context.getContentResolver().query(ContactsContract.Data.CONTENT_URI,
                    new String[] {ContactsContract.Data.RAW_CONTACT_ID},
                    ContactsContract.Data.MIMETYPE + " =? AND " + StructuredName.DISPLAY_NAME
                    + " like '" + name + "%' ", new String[] {StructuredName.CONTENT_ITEM_TYPE},
                    null);

            if (c == null) {
                return resultAddr;
            }

            StringBuilder sb = new StringBuilder();
            while (c.moveToNext()) {
                long raw_contact_id = c.getLong(c.getColumnIndex(ContactsContract.Data.
                        RAW_CONTACT_ID));
                if (sb.length() > 0) {
                    sb.append(",");
                }
                sb.append(queryPhoneNumbersWithRaw(context, raw_contact_id));
            }

            resultAddr = sb.toString();
        } finally {
            if (c != null) {
                c.close();
            }
        }

        return resultAddr;
    }

    private static String queryPhoneNumbersWithRaw(Context context, long rawContactId) {
        Cursor c = null;
        String addrs = "";
        try {
            c = context.getContentResolver().query(
                    ContactsContract.CommonDataKinds.Phone.CONTENT_URI,
                    new String[] {Phone.NUMBER}, Phone.RAW_CONTACT_ID + " = " + rawContactId,
                    null, null);

            if (c != null) {
                int i = 0;
                while (c.moveToNext()) {
                    String addrValue = c.getString(c.getColumnIndex(Phone.NUMBER));
                    if (!TextUtils.isEmpty(addrValue)) {
                        if (i == 0) {
                            addrs = addrValue;
                        } else {
                            addrs = addrs + "," + addrValue;
                        }
                        i++;
                    }
                }
            }
        } finally {
            if (c != null) {
                c.close();
            }
        }
        return addrs;
    }

    /**
     * Return the activated card number
     */
    public static int getActivatedIccCardCount() {
        int phoneCount = getPhoneCount();
        if(DEBUG) Log.d(TAG, "isIccCardActivated phoneCount " + phoneCount);
        int count = 0;
        for (int i = 0; i < phoneCount; i++) {
            // Because the status of slot1/2 will return SIM_STATE_UNKNOWN under airplane mode.
            // So we add check about SIM_STATE_UNKNOWN.
            if (isIccCardActivated(i)) {
                count++;
            }
        }
        return count;
   }

    /**
     * Decide whether the current product  is DSDS in MMS
     */
    public static boolean isMultiSimEnabledMms() {
        TelephonyManager tm = (TelephonyManager)MmsApp.
                getApplication().getSystemService(Context.TELEPHONY_SERVICE);
        return TelephonyManagerWrapper.isMultiSimEnabled(tm);
    }

    public static int getPhoneCount(){
        TelephonyManager tm = (TelephonyManager)MmsApp.
                getApplication().getSystemService(Context.TELEPHONY_SERVICE);
        return tm.getPhoneCount();
    }

    public static boolean isCDMAPhone(int subscription) {
        boolean isCDMA = false;
        TelephonyManager tm = (TelephonyManager)MmsApp.
                getApplication().getSystemService(Context.TELEPHONY_SERVICE);
        int activePhone = isMultiSimEnabledMms()
                ? TelephonyManagerWrapper.getCurrentPhoneType(tm, subscription)
                        : tm.getPhoneType();
        if (TelephonyManager.PHONE_TYPE_CDMA == activePhone) {
            isCDMA = true;
        }
        return isCDMA;
    }

    public static boolean isWebUrl(String url) {
        for (String schema : WEB_SCHEMA) {
            if (url.startsWith(schema)) {
                return true;
            }
        }
        return false;
    }

    private static boolean isNetworkRoaming(int subscription) {
        TelephonyManager tm = (TelephonyManager)MmsApp.
                getApplication().getSystemService(Context.TELEPHONY_SERVICE);
        return isMultiSimEnabledMms()
                ? TelephonyManagerWrapper.isNetworkRoaming(tm, subscription)
                : tm.isNetworkRoaming();
    }

    public static boolean isCDMAInternationalRoaming(int subscription) {
        return isCDMAPhone(subscription) && isNetworkRoaming(subscription);
    }

    private static String getUrlWithMailPrefix(Context context, String url) {
        // If prefix string is "mailto" then translate it.
        final String mailPrefix = "mailto:";
        if (!TextUtils.isEmpty(url)) {
            if (url.startsWith(mailPrefix)) {
                url = context.getResources().getString(R.string.mail_to) +
                        url.substring(mailPrefix.length());
                return url;
            }
        }
        return "";
    }

    public static boolean isMsimIccCardActive() {
        if (isMultiSimEnabledMms()) {
            if (isIccCardActivated(MessageUtils.SUB1) && isIccCardActivated(MessageUtils.SUB2)) {
                return true;
            }
        }
        return false;
    }

    public static boolean isMobileDataDisabled(Context context) {
        ConnectivityManager mConnService = (ConnectivityManager) context
                .getSystemService(Context.CONNECTIVITY_SERVICE);
        return !ConnectivityManagerWrapper.getMobileDataEnabled(mConnService);
    }

    public static boolean isMobileDataEnabled(Context context, int subId) {
        return TelephonyManagerWrapper.isMobileDataEnabled(
                MmsApp.getApplication().getTelephonyManager(), subId);
    }

    public static boolean isAirplaneModeOn(Context context) {
        return Settings.Global.getInt(context.getContentResolver(),
                Settings.Global.AIRPLANE_MODE_ON, 0) == 1;
    }

    public static float onFontSizeScale(ArrayList<TextView> list, float scale,
            float mFontSizeForSave) {
        float mCurrentSize = 0;
        if (list.size() > 0) {
            mCurrentSize = list.get(0).getTextSize();
        }
        if (scale < 0.999999 || scale > 1.00001) {
            float zoomInSize = mCurrentSize + FONT_SIZE_STEP;
            float zoomOutSize = mCurrentSize - FONT_SIZE_STEP;
            if (scale > 1.0 && zoomInSize <= MAX_FONT_SIZE) {
                for (TextView view : list) {
                    view.setTextSize(TypedValue.COMPLEX_UNIT_PX, zoomInSize);
                    if (mFontSizeForSave != zoomInSize) {
                        mFontSizeForSave = zoomInSize;
                    }
                }
            } else if (scale < 1.0 && zoomOutSize >= MIN_FONT_SIZE) {
                for (TextView view : list) {
                    view.setTextSize(TypedValue.COMPLEX_UNIT_PX, zoomOutSize);
                    if (mFontSizeForSave != zoomOutSize) {
                        mFontSizeForSave = zoomOutSize;
                    }
                }
            }
        }
        return mFontSizeForSave;
    }

    public static void saveTextFontSize(Context context, float value) {
        SharedPreferences prefsms = PreferenceManager.getDefaultSharedPreferences(context);
        prefsms.edit().putString(KEY_SMS_FONTSIZE, String.valueOf(value)).commit();
    }

    public static float getTextFontSize(Context context) {
        SharedPreferences prefsms = PreferenceManager.
                getDefaultSharedPreferences(context);
        String textSize = prefsms.getString(KEY_SMS_FONTSIZE, String.valueOf(FONT_SIZE_DEFAULT));
        float size = Float.parseFloat(textSize);
        /* this function is a common function add this to make sure if did not save the size */
        if (size < MIN_FONT_SIZE) {
            return MIN_FONT_SIZE;
        } else if (size > MAX_FONT_SIZE) {
            return MAX_FONT_SIZE;
        }
        return size;
    }

    public static int getMmsDownloadStatus(int mmsStatus) {
        if(DownloadManager.STATE_PERMANENT_FAILURE == mmsStatus) {
            return mmsStatus;
        } else if (!DownloadManager.getInstance().isAuto()
                && DownloadManager.STATE_PRE_DOWNLOADING != mmsStatus) {
            return mmsStatus & ~DownloadManager.DEFERRED_MASK;
        }
        return mmsStatus;
    }

    public static void markAsRead(final Context context, final Uri msgUri) {
        new Thread(new Runnable() {
            public void run() {
                try {
                    ContentValues values = new ContentValues(2);
                    values.put(Mms.READ, MessageUtils.MESSAGE_READ);
                    values.put(Mms.SEEN, MessageUtils.MESSAGE_SEEN);
                    SqliteWrapper.update(context,
                            context.getContentResolver(),
                            msgUri, values, null, null);
                    MmsWidgetProvider.notifyDatasetChanged(context);
                    MessagingNotification.blockingUpdateNewMessageIndicator(
                            context,
                            MessagingNotification.THREAD_NONE, false);
                } catch (Exception e) {
                    Log.e(TAG, "Update Read Error", e);
                }
            }
        }).start();
    }

    public static boolean cancelFailedToDeliverNotification(Intent intent, Context context) {
        if (MessagingNotification.isFailedToDeliver(intent)) {
            // Cancel any failed message notifications
            MessagingNotification.cancelNotification(context,
                        MessagingNotification.MESSAGE_FAILED_NOTIFICATION_ID);
            return true;
        }
        return false;
    }

    public static boolean cancelFailedDownloadNotification(Intent intent, Context context) {
        if (MessagingNotification.isFailedToDownload(intent)) {
            // Cancel any failed download notifications
            MessagingNotification.cancelNotification(context,
                        MessagingNotification.DOWNLOAD_FAILED_NOTIFICATION_ID);
            return true;
        }
        return false;
    }

    /**
     * Return the icc uri according to subscription
     */
    public static Uri getIccUriBySubscription(int slotId) {
        Log.d(TAG, "getIccUriBySubscription slotId="+slotId);
        switch (slotId) {
            case SUB1:
                return ICC1_URI;
            case SUB2:
                return ICC2_URI;
            default:
                return ICC_URI;
        }
    }

    public static void onMessageContentClick(final Context context, final TextView contentText) {
        // Check for links. If none, do nothing; if 1, open it; if >1, ask user to pick one
        final URLSpan[] spans = contentText.getUrls();
        if (spans.length == 1) {
            String url = spans[0].getURL();
            if (isWebUrl(url)) {
                showUrlOptions(context, url);
            } else {
                final String telPrefix = "tel:";
                if (url.startsWith(telPrefix)) {
                    url = url.substring(telPrefix.length());
                    if (PhoneNumberUtils.isWellFormedSmsAddress(url)) {
                        showNumberOptions(context, url);
                    }
                } else if (url.startsWith(MAIL_TO_PREFIX)) {
                    url = url.substring(MAIL_TO_PREFIX.length());
                    showEmailOptions(context, url);
                } else {
                    spans[0].onClick(contentText);
                }
            }
        } else if (spans.length > 1) {
            ArrayAdapter<URLSpan> adapter = new ArrayAdapter<URLSpan>(context,
                    android.R.layout.select_dialog_item, spans) {

                @Override
                public View getView(int position, View convertView, ViewGroup parent) {
                    View v = super.getView(position, convertView, parent);
                    URLSpan span = getItem(position);
                    String url = span.getURL();
                    Uri uri = Uri.parse(url);
                    TextView tv = (TextView) v;
                    try {
                        Drawable d = context.getPackageManager().getActivityIcon(
                                new Intent(Intent.ACTION_VIEW, uri));
                        if (d != null) {
                            d.setBounds(0, 0, d.getIntrinsicHeight(),
                                    d.getIntrinsicHeight());
                            tv.setCompoundDrawablePadding(10);
                            tv.setCompoundDrawables(d, null, null, null);
                        }
                    } catch (android.content.pm.PackageManager.NameNotFoundException ex) {
                        // it's ok if we're unable to set the drawable for this view - the user
                        // can still use it.
                        tv.setCompoundDrawables(null,null,null,null);
                        Log.d(TAG,"NameNotFoundException");
                    }
                    String tmpUrl = null;
                    if (url != null) {
                        if (url.startsWith(MAIL_TO_PREFIX)) {
                            url = url.substring(MAIL_TO_PREFIX.length());
                        }
                        tmpUrl = url.replaceAll("tel:", "");
                    }
                    tv.setText(tmpUrl);
                    return v;
                }
            };

            AlertDialog.Builder b = new AlertDialog.Builder(context);
            DialogInterface.OnClickListener click = new DialogInterface.OnClickListener() {
                @Override
                public final void onClick(DialogInterface dialog, int which) {
                    if (which >= 0) {
                        String url = spans[which].getURL();
                        if (isWebUrl(url)) {
                            showUrlOptions(context, url);
                        } else {
                            final String telPrefix = "tel:";
                            if (url.startsWith(telPrefix)) {
                                url = url.substring(telPrefix.length());
                                if (PhoneNumberUtils.isWellFormedSmsAddress(url)) {
                                    showNumberOptions(context, url);
                                }
                            } else if (url.startsWith(MAIL_TO_PREFIX)) {
                                url = url.substring(MAIL_TO_PREFIX.length());
                                showEmailOptions(context, url);
                            } else {
                                spans[0].onClick(contentText);
                            }
                        }
                    }
                    dialog.dismiss();
                }
            };

            b.setTitle(R.string.select_link_title);
            b.setCancelable(true);
            b.setAdapter(adapter, click);
            b.setNegativeButton(android.R.string.cancel, new DialogInterface.OnClickListener() {
                @Override
                public final void onClick(DialogInterface dialog, int which) {
                    dialog.dismiss();
                }
            });
            b.show();
        }
    }

    public static int getSmsPreferStoreLocation(Context context, int subId) {
        SharedPreferences prefsms = PreferenceManager.getDefaultSharedPreferences(context);
        int preferStore = PREFER_SMS_STORE_PHONE;

        if (isMultiSimEnabledMms()) {
            if (subId == SUB1) {
                preferStore = Integer.parseInt(prefsms.getString("pref_key_sms_store_card1", "0"));
            } else {
                preferStore = Integer.parseInt(prefsms.getString("pref_key_sms_store_card2", "0"));
            }
        } else {
            preferStore = Integer.parseInt(prefsms.getString("pref_key_sms_store_single", "0"));
        }

        return preferStore;
    }

    public static long getStoreUnused() {
        File path = new File(MMS_DATA_DATA_DIR);
        StatFs stat = new StatFs(path.getPath());
        long blockSize = stat.getBlockSize();
        long availableBlocks = stat.getAvailableBlocks();
        return availableBlocks * blockSize;
    }

    public static boolean isPhoneMemoryFull() {
        long available = getStoreUnused();
        return available < MIN_AVAILABLE_SPACE_MMS ;
    }

    /* Used for check whether have memory for save mms */
    public static boolean isMmsMemoryFull() {
        boolean isMemoryFull = isPhoneMemoryFull();
        if (isMemoryFull) {
            Log.d(TAG, "Mms emory is full ");
            return true;
        }
        return false;
    }

    public static boolean isPhoneSmsCountFull(Context context) {
        int maxSmsMessageCount = context.getResources().getInteger(R.integer.max_sms_message_count);
        if (maxSmsMessageCount != -1) {
            int msgCount = getSmsMessageCount(context);
            return msgCount >= maxSmsMessageCount;
        }
        return false;
    }

    // Used for check whether have memory for save SMS.
    public static boolean checkIsPhoneMessageFull(Context context) {
        boolean isFull = isPhoneMemoryFull() || isPhoneSmsCountFull(context);
        MessagingNotification.updateMessageFullIndicator(context, true, isFull);
        return isFull;
    }

    // Used for check whether have memory for save MMS.
    public static boolean checkIsPhoneMemoryFull(Context context) {
        boolean isFull = isPhoneMemoryFull();
        MessagingNotification.updateMessageFullIndicator(context, false, isFull);
        return isFull;
    }

    private static String getMmsDataDir() {
        File data_file = new File(MMS_DATA_DIR);
        if (data_file.exists()) {
            return MMS_DATA_DIR;
        }
        return MMS_DATA_DATA_DIR;
    }

    private static class MessageInfo {
        private int count = 0;
        private long size = 0;
    }

    private static MessageInfo getSmsUsed(Context mContext) {
        int count = 0;
        long size = 0;
        Cursor cursor = SqliteWrapper.query(mContext, mContext.getContentResolver(),
                Sms.CONTENT_URI,
                new String[] {
                        "body"
                }, null, null, null);
        if (cursor != null) {
            count = cursor.getCount();
            try {
                while (cursor.moveToNext()) {
                    String body = cursor.getString(0);
                    if (body != null) {
                        size += body.getBytes().length;
                    }
                }
            } finally {
                cursor.close();
            }
        }
        MessageInfo msgInfo = new MessageInfo();
        msgInfo.count = count;
        msgInfo.size = size;
        Log.d(TAG, "SMS count = " + count + " size = " + size);
        return msgInfo;
    }

    private static MessageInfo getMmsUsed(Context mContext) {
        int mmsCount = 0;
        long mmsfileSize = 0;
        Cursor cursor = SqliteWrapper.query(mContext, mContext.getContentResolver(),
                Telephony.Mms.CONTENT_URI,
                new String[] {
                        "m_size"
                }, MMS_SELECTION, null, null);

        if (cursor != null) {
            mmsCount = cursor.getCount();
            try {
                while (cursor.moveToNext()) {
                    mmsfileSize += (cursor.getInt(0) == 0 ? DEFAULT_MMS_SIZE
                            * KILOBYTE_SIZE : cursor.getInt(0));
                }
            } finally {
                cursor.close();
            }
        }
        Log.v(TAG, "MMS count = " + mmsCount + " size = " + mmsfileSize);
        MessageInfo msgInfo = new MessageInfo();
        msgInfo.count = mmsCount;
        long mmsMinSize = mmsCount * MIN_MMS_SIZE * KILOBYTE_SIZE;
        msgInfo.size = (mmsfileSize < mmsMinSize ? mmsMinSize : mmsfileSize);
        return msgInfo;
    }

    public static long getStoreAll() {
        File path = new File(getMmsDataDir());
        StatFs stat = new StatFs(path.getPath());
        long blockSize = stat.getBlockSize();
        long allBlocks = stat.getBlockCount();
        return allBlocks * blockSize;
    }

    public static long getStoreUsed() {
        return getStoreAll() - getStoreUnused();
    }

    public static String formatMemorySize(long size) {
        String suffix = null;
        String kbStr = null;
        boolean hasMb = false;
        DecimalFormat formatter = new DecimalFormat();

        // add KB or MB suffix if size is greater than 1K or 1M
        if (size >= KILOBYTE_SIZE) {
            suffix = " KB";
            size /= KILOBYTE_SIZE;
            kbStr = formatter.format(size);
            if (size >= KILOBYTE_SIZE) {
                suffix = " MB";
                size /= KILOBYTE_SIZE;
                hasMb = true;
            }
        } else {
            suffix = " B";
        }

        formatter.setGroupingSize(DECIMAL_FORMATTOR_GROUPING_SIZE);
        String result = formatter.format(size);

        if (suffix != null) {
            if (hasMb && kbStr != null) {
                result = result + suffix + " (" + kbStr + " KB)";
            } else {
                result = result + suffix;
            }
        }
        return result;
    }

    public static int getSmsMessageCount(Context context) {
        int msgCount = 0;

        Cursor cursor = SqliteWrapper.query(context, context.getContentResolver(),
                MESSAGES_COUNT_URI, null, null, null, null);

        if (cursor != null) {
            try {
                if (cursor.moveToFirst()) {
                    msgCount = cursor.getInt(0);
                } else {
                    Log.d(TAG, "getSmsMessageCount returned no rows!");
                }
            } finally {
                cursor.close();
            }
        }

        Log.d(TAG, "getSmsMessageCount : msgCount = " + msgCount);
        return msgCount;
    }

    public static void removeDialogs() {
        if(memoryStatusDialog != null && memoryStatusDialog.isShowing()) {
            memoryStatusDialog.dismiss();
            memoryStatusDialog = null;
        }
    }

    public static void showMemoryStatusDialog(Context context) {
       new ShowDialog(context).execute();
    }

    public static void copyToClipboard(Context context, String str) {
        ClipboardManager clipboard = (ClipboardManager) context
                .getSystemService(Context.CLIPBOARD_SERVICE);
        clipboard.setPrimaryClip(ClipData.newPlainText(null, str));
    }

    private static void showUrlOptions(final Context slideContext, final String messageUrl) {
        final String[] texts = new String[] {
                slideContext.getString(R.string.menu_connect_url),
                slideContext.getString(R.string.menu_copy_url, messageUrl)
        };
        AlertDialog.Builder builder = new AlertDialog.Builder(slideContext);
        builder.setTitle(slideContext.getString(R.string.message_options));
        builder.setCancelable(true);
        builder.setItems(texts, new DialogInterface.OnClickListener() {
            @Override
            public final void onClick(DialogInterface dialog, int which) {
                switch (which) {
                    case URL_OPTION_MENU_CONNECT:
                        loadUrlDialog(slideContext, messageUrl);
                        break;
                    case URL_OPTION_MENU_COPY_URL:
                        copyToClipboard(slideContext, messageUrl);
                        break;
                }
            }
        });
        builder.setOnCancelListener(new DialogInterface.OnCancelListener() {
            @Override
            public void onCancel(DialogInterface dialog) {
                dialog.dismiss();
            }
        });
        builder.show();
    }

    private static void loadUrlDialog(final Context context, final String urlString) {
        AlertDialog.Builder builder = new AlertDialog.Builder(context);
        builder.setTitle(R.string.menu_connect_url);
        builder.setMessage(context.getString(R.string.loadurlinfo_str));
        builder.setCancelable(true);
        builder.setPositiveButton(R.string.yes, new OnClickListener() {
            @Override
            final public void onClick(DialogInterface dialog, int which) {
                loadUrl(context, urlString);
            }
        });
        builder.setNegativeButton(R.string.no, new OnClickListener() {
            @Override
            final public void onClick(DialogInterface dialog, int which) {
                dialog.dismiss();
            }
        });
        builder.setOnCancelListener(new DialogInterface.OnCancelListener() {
            @Override
            public void onCancel(DialogInterface dialog) {
                dialog.dismiss();
            }
        });
        builder.show();

    }

    private static void loadUrl(Context context, String url) {
        if (!url.regionMatches(true, 0, "http://", 0, 7)
                && !url.regionMatches(true, 0, "https://", 0, 8)
                && !url.regionMatches(true, 0, "rtsp://", 0, 7)) {
            url = "http://" + url;
        }
        url = url.replace("Http://", "http://");
        url = url.replace("Https://", "https://");
        url = url.replace("HTTP://", "http://");
        url = url.replace("HTTPS://", "https://");
        url = url.replace("Rtsp://", "rtsp://");
        url = url.replace("RTSP://", "rtsp://");
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        intent.addCategory(Intent.CATEGORY_BROWSABLE);

        if ((url.substring(url.length() - 4).compareToIgnoreCase(".mp4") == 0)
                || (url.substring(url.length() - 4).compareToIgnoreCase(".3gp") == 0)) {
            intent.setDataAndType(Uri.parse(url), "video/*");
        }
        try {
            context.startActivity(intent);
        } catch (ActivityNotFoundException e) {
            return;
        }
    }

    private static class ShowDialog extends AsyncTask<String, Void, StringBuilder> {
        private Context mContext;
        public ShowDialog(Context context) {
            mContext = context;
        }

        @Override
        protected StringBuilder doInBackground(String... params) {
            StringBuilder memoryStatus = new StringBuilder();
            memoryStatus.append(mContext.getString(R.string.sms_phone_used));
            MessageInfo smsInfo = getSmsUsed(mContext);
            memoryStatus.append(" " + smsInfo.count + "\n");
            memoryStatus.append(mContext.getString(R.string.mms_phone_used));
            MessageInfo mmsInfo = getMmsUsed(mContext);
            memoryStatus.append(" " + mmsInfo.count + "\n");
            memoryStatus.append(mContext.getString(R.string.total_memory_used));
            memoryStatus.append(" " + formatMemorySize(smsInfo.size + mmsInfo.size) + "\n");
            return memoryStatus;
        }

        @Override
        protected void onPostExecute(StringBuilder memoryStatus) {
            if (((Activity) mContext).isFinishing()) {
                return;
            }
            if (memoryStatus != null && !memoryStatus.toString().isEmpty()) {
                AlertDialog.Builder builder = new AlertDialog.Builder(mContext);
                builder.setTitle(R.string.memory_status_title);
                builder.setCancelable(true);
                builder.setPositiveButton(R.string.yes, null);
                builder.setMessage(memoryStatus);
                memoryStatusDialog = builder.create();
                memoryStatusDialog.show();
            }
        }
    }

    public static void showEmailOptions(final Context context, final String address) {
        AlertDialog.Builder builder = new AlertDialog.Builder(context).setTitle(address)
                .setCancelable(true);
        builder.setItems(R.array.email_options, new OnClickListener() {

            @Override
            public void onClick(DialogInterface dialog, int which) {
                switch (which) {
                    case DIALOG_ITEM_EMAIL_TO:
                        try {
                            context.startActivity(new Intent(Intent.ACTION_VIEW, Uri
                                    .parse(MAIL_TO_PREFIX + address))
                                    .setFlags(Intent.FLAG_ACTIVITY_CLEAR_WHEN_TASK_RESET));
                        } catch (ActivityNotFoundException e) {
                            Toast.makeText(context, context.getString(R.string.email_app_not_found),
                                    Toast.LENGTH_LONG).show();
                        }
                        break;
                    case DIALOG_ITEM_EMAIL_ADD_CONTACTS:
                        context.startActivity(ConversationList.createAddContactIntent(address));
                        break;
                    default:
                        break;
                }
                dialog.dismiss();
            }
        }).show();
    }

    public static void showNumberOptions(Context context, String number) {
        final Context localContext = context;
        final String extractNumber = number;
        AlertDialog.Builder builder = new AlertDialog.Builder(localContext);
        builder.setTitle(number);
        builder.setCancelable(true);
        builder.setItems(R.array.number_options,
                new DialogInterface.OnClickListener() {
            public void onClick(DialogInterface dialog, int which) {
                switch (which) {
                    case DIALOG_ITEM_CALL:
                        Intent dialIntent = new Intent(Intent.ACTION_CALL,
                                Uri.parse("tel:" + extractNumber));
                        localContext.startActivity(dialIntent);
                        break;
                    case DIALOG_ITEM_SMS:
                        Intent smsIntent = new Intent(Intent.ACTION_SENDTO,
                                Uri.parse("smsto:" + extractNumber));
                        localContext.startActivity(smsIntent);
                        break;
                    case DIALOG_ITEM_ADD_CONTACTS:
                        Intent intent = ConversationList
                                .createAddContactIntent(extractNumber);
                        localContext.startActivity(intent);
                        break;
                    default:
                        break;
                }
                dialog.dismiss();
            }
        });
        builder.show();
    }

    public static Intent getCellBroadcastIntent() {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.setComponent(new ComponentName(
                CELL_BROADCAST_PACKAGE_NAME,
                CELL_BROADCAST_ACTIVITY_NAME));
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return intent;
    }

    public static String convertIdp(Context context, String number, int subscription) {
        String convertedNumber = number;
        if (context.getResources().getBoolean(R.bool.customize_env_phone_checkidp)) {
            String idpPrefix = getIdpPrefixNumber(context, subscription);
            if (number.indexOf(idpPrefix) == 0) {
                convertedNumber = IDP_PLUS + number.substring(idpPrefix.length());
            }
        }
        Log.d(TAG, "convertIdp : number = "  + number + " converted number = " + convertedNumber);
        return convertedNumber;
    }

    public static boolean isRoaming(int subscription) {
        TelephonyManager tm = (TelephonyManager)MmsApp.
                getApplication().getSystemService(Context.TELEPHONY_SERVICE);
       return TelephonyManagerWrapper.isNetworkRoaming(tm, subscription);
    }

    private static boolean checkIdpEnable(Context context, int subscription) {
        if (context.getResources().getBoolean(R.bool.customize_env_phone_checkidp)
                && Settings.System.getInt(context.getContentResolver(),
                INTERNATIONAL_PREFIX_ENABLE + subscription, IDP_ENABLE) == IDP_ENABLE) {
            return true;
        }
        return false;
    }

    private static String getIdpPrefixNumber(Context context, int subscription) {
        String idpPrefix = Settings.System.getString(context.getContentResolver(),
                INTERNATIONAL_PREFIX_NUMBER + subscription);
        return TextUtils.isEmpty(idpPrefix) ? IDP_DEFAULT_PREFIX : idpPrefix;
    }

    public static String checkIdp(Context context, String number, int subscription) {
        String checkedNumber = number;
        boolean isCDMA = isCDMAPhone(subscription);
        boolean isEnable = checkIdpEnable(context, subscription);
        boolean isRoaming = isRoaming(subscription);
        if (isEnable && isCDMA) {
            int indexIdn = number.indexOf(IDP_IDN);
            int indexPlus = number.indexOf(IDP_PLUS);

            if ((indexIdn != -1) && (!isRoaming(subscription))) {
                checkedNumber = number.substring(0, indexIdn) +
                    IDP_ZERO + number.substring(indexIdn + IDP_IDN.length());
            } else if (indexPlus != -1) {
                 checkedNumber = number.substring(0, indexPlus)
                      + getIdpPrefixNumber(context, subscription)
                      + number.substring(indexPlus + IDP_PLUS.length());
            }
        }

        Log.d(TAG, "checkIdp : number = "  + number + " sub = "  + subscription +
            " isCDMA = "  + isCDMA  + " isEnable = " + isEnable +
            " checked number = "  + checkedNumber);

        return checkedNumber;
    }

    public static boolean isTooLargeFile(final Context ctx, final Uri uri) {
        boolean isTooLarge = false;
        InputStream input = null;

        try {
            input = ctx.getContentResolver().openInputStream(uri);
            if (input.available() / COMPRESSION_FACTOR > MmsConfig
                    .getMaxMessageSize()) {
                isTooLarge = true;
            }
        } catch (Exception e) {
            Log.e(TAG, e.getMessage(), e);
        } finally {
            if (input != null) {
                try {
                    input.close();
                } catch (IOException e) {
                    Log.e(TAG, e.getMessage(), e);
                }
            }
        }
        return isTooLarge;
    }

    public static boolean hasInvalidSmsRecipient(Context context,
            CopyOnWriteArrayList<MessageItem> msgItems) {
        for (MessageItem msgItem : msgItems) {
            String address = msgItem.mAddress;
            String[] number = null;
            if (MessageUtils.isWapPushNumber(address)) {
                number = address.split(":");
                address = number[0];
            }
            if (!isValidAddress(context, address)) {
                if (MmsConfig.getEmailGateway() == null) {
                    return true;
                } else if (!MessageUtils.isAlias(address)) {
                    return true;
                }
            }
        }
        return false;
    }

    private static boolean isValidAddress(Context context, String number) {
        if (hasInvalidCharacter(context, number)) {
            return false;
        }

        return PhoneNumberUtils.isWellFormedSmsAddress(number);
    }

    private static boolean hasInvalidCharacter(Context context, String number) {
        char[] charNumber = number.trim().toCharArray();
        int count = charNumber.length;
        if (context.getResources().getBoolean(R.bool.config_filter_char_address)) {
            for (int i = 0; i < count; i++) {
                if (i == 0 && charNumber[i] == '+') {
                    continue;
                }
                if (!isValidCharacter(charNumber[i])) {
                    return true;
                }
            }
        } else {
            for (int i = 0; i < count; i++) {
                if (isSBCCharacter(charNumber, i)) {
                    return true;
                }
            }
        }
        return false;
    }

    private static boolean isValidCharacter(char c) {
        return (c >= '0' && c <= '9') || c == '-' || c == '(' || c == ')' || c == ' ';
    }

    private static boolean isSBCCharacter(char[] charNumber, int i) {
        return charNumber[i] >= SBC_CHAR_START && charNumber[i] <= SBC_CHAR_END;
    }

    public static String getFilePath(final Context context, final Uri uri) {
        String path = null;
        if (uri.toString().contains("content://")) {
            Cursor c = SqliteWrapper.query(context, context
                    .getContentResolver(), uri, null, null, null, null);
            try {
                if (c != null && c.moveToFirst()) {
                    if (ImageModel.isMmsUri(uri)) {
                        path = c.getString(c.getColumnIndexOrThrow(Part._DATA));

                    } else if ("media".equals(uri.getAuthority())) {
                        path = c.getString(c
                                .getColumnIndexOrThrow(Audio.Media.DATA));
                    }
                } else {
                    return null;
                }
            } catch (Exception e) {
                Log.e(TAG, e.getMessage());
            } finally {
                if (c != null) {
                    c.close();
                }
            }
        } else if (uri.getScheme().equals("file")) {
            path = uri.getSchemeSpecificPart();
        }

        return path;
    }

    private static boolean isWIFICallingEnable(final Context context)
            throws Settings.SettingNotFoundException {
        return Settings.Global.getInt(context.getContentResolver(), WIFI_CALL_TURNON)
                == WIFI_CALLING_ENABLED;
    }

    public static boolean isWfcUnavailable(final Context context)
            throws Settings.SettingNotFoundException {
        return  isWIFICallingEnable(context)
                && !isServiceStateAvailable(context)
                && (isWifiOn(context) && !isWifiConnected(context));
    }

    public static void pupConnectWifiAlertDialog(final Context context) {
            if (LogTag.DEBUG) {
                Log.d(TAG, "pupConnectWifiAlertDialog");
            }
            AlertDialog.Builder builder = new AlertDialog.Builder(context);
            builder.setCancelable(true);
            builder.setPositiveButton(R.string.yes, new DialogInterface.OnClickListener() {
                public void onClick(DialogInterface dialog, int which) {
                    Intent intent = new Intent();
                    intent.setAction(android.provider.Settings.ACTION_WIFI_SETTINGS);
                    context.startActivity(intent);
                }
            });
            builder.setMessage(R.string.no_network_alert_when_send_message);
            builder.show();
    }

    public static void pupConnectWifiNotification(final Context context) {

            final NotificationManager notiManager =
                    (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);

            Intent intent = new Intent();
            intent.setAction(android.provider.Settings.ACTION_WIFI_SETTINGS);
            PendingIntent pendingIntent =
                    PendingIntent.getActivity(
                            context, 1, intent, PendingIntent.FLAG_UPDATE_CURRENT);

            Notification.Builder builder = new Notification.Builder(context);
            builder.setOngoing(false);
            builder.setWhen(0);
            builder.setContentIntent(pendingIntent);
            builder.setAutoCancel(true);
            builder.setSmallIcon(R.drawable.wifi_calling_on_notification);
            builder.setContentTitle(context.getResources().getString(
                    R.string.no_network_notification_when_send_message));
            builder.setContentText(context.getResources().getString(
                    R.string.alert_user_connect_to_wifi_for_message_text));
            notiManager.notify(1, builder.build());
            Handler handler = new Handler(context.getMainLooper());
            handler.postDelayed(new Runnable() {
                public void run() {
                    notiManager.cancel(1);
                }
            }, 5000);
    }

    private static boolean isWifiOn(Context context) {
        WifiManager mWifiManager = (WifiManager)context.getSystemService(Context.WIFI_SERVICE);
        if (mWifiManager.isWifiEnabled())
            return true;
        return false;
    }

    private static boolean isWifiConnected(Context context) {
        ConnectivityManager mConnectivityManager = (ConnectivityManager) context.
                                                    getSystemService(Context.CONNECTIVITY_SERVICE);
        NetworkInfo mWifiNetworkInfo = mConnectivityManager.getNetworkInfo(
                                                    ConnectivityManager.TYPE_WIFI);
        if (mWifiNetworkInfo != null) {
            State wifi = mWifiNetworkInfo.getState();
            if (wifi == State.CONNECTED)
                return true;
        }
        return false;
    }

    private static boolean isServiceStateAvailable(Context context) {
        boolean available = false;
        TelephonyManager telephonyManager = (TelephonyManager)MmsApp.
                getApplication().getSystemService(Context.TELEPHONY_SERVICE);
        int count = telephonyManager.getPhoneCount();
        for (int i = 0; i < count; i++) {
            int[] subId = SubscriptionManagerWrapper.getSubId(i);
            ServiceState ss = TelephonyManagerWrapper.
                        getServiceStateForSubscriber(telephonyManager, subId[0]);
            if(ss != null && ss.getState() == ServiceState.STATE_IN_SERVICE) {
                available = true;
            }
        }
        return available;
    }

    /***
     * added for Android 6.0 + for runtime permission
     */

    /**
     * Check whether we have the runtime permission given, suppose api level is
     * 23 or higher
     *
     * @return true or false
     */
    public static boolean hasPermission(final String strPermission) {
        final Context context = MmsApp.getApplication().getApplicationContext();
        final int state = context.checkSelfPermission(strPermission);
        return state == PackageManager.PERMISSION_GRANTED;
    }

    /**
     * Check whether have the permissions given
     *
     * @return true or false
     */
    public static boolean hasPermissions(final String[] strPermissions) {
        for (final String permission : strPermissions) {
            if (!hasPermission(permission)) {
                return false;
            }
        }
        return true;
    }

    /**
     * Check whether have the basic permission as default SMS application
     */
    public static boolean hasBasicPermissions() {
        return hasPermissions(sSMSBasicPermissions);
    }

    /**
     * Launch permission check
     */
    public static void launchPermissionCheckActivity(Activity activity,String [] permissions) {
        final Intent intent = new Intent(activity, PermissionGuardActivity.class);
        intent.putExtra(PermissionGuardActivity.ORIGINAL_INTENT, activity.getIntent());
        intent.putExtra(PermissionGuardActivity.EXT_PERMISSIONS, permissions);
        activity.startActivity(intent);
    }

    /**
     * Check basic permission before enter app
     */
    public static boolean checkPermissionsIfNeeded(Activity activity) {
        boolean hasBasicPermission = hasBasicPermissions();
        boolean hasExtPermission = hasPermissions(sSMSExtendPermissions);

        if (Log.isLoggable(LogTag.APP, Log.VERBOSE)) {
            log("checkPermission: Basic=" + hasBasicPermission +
                    " Ext=" + hasExtPermission);
        }
        if (hasBasicPermission) {
            MmsApp.getApplication().initPermissionRelated();
        }
        if (hasBasicPermission && hasExtPermission) {
            return false;
        } else {
            launchPermissionCheckActivity(activity,
                    hasExtPermission ? null : sSMSExtendPermissions);
        }
        activity.finish();
        return true;
    }

    public static String[] getMissingPermissions(String[] permissions) {
        final ArrayList<String> missingList = new ArrayList<String>();
        for (final String permission : permissions) {
            if (!hasPermission(permission)) {
                missingList.add(permission);
            }
        }

        final String[] missingArray = new String[missingList.size()];
        missingList.toArray(missingArray);
        return missingArray;

    }

    public static String[] getMissingBasicPermissions() {
        if (MmsConfig.isSmsEnabled(MmsApp.getApplication())) {
            return getMissingPermissions(sSMSBasicPermissions_FOR_DEFAULT_MESSAGING);
        } else {
            return getMissingPermissions(sSMSBasicPermissions);
        }
    }

    public static String[] getMissingExternalPermissions() {
        if (MmsConfig.isSmsEnabled(MmsApp.getApplication())) {
            return getMissingPermissions(sSMSExtendPermissions_FOR_DEFAULT_MESSAGING);
        } else {
            return getMissingPermissions(sSMSExtendPermissions);
        }
    }

    public static boolean hasStoragePermission() {
        return hasPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE);
    }

    public static String trimV4AddrZeros(String addr) {
        if (addr == null) return null;
        String[] octets = addr.split("\\.");
        if (octets.length != 4) return addr;
        StringBuilder builder = new StringBuilder(16);
        String result = null;
        for (int i = 0; i < 4; i++) {
            try {
                if (octets[i].length() > 3) return addr;
                builder.append(Integer.parseInt(octets[i]));
            } catch (NumberFormatException e) {
                return addr;
            }
            if (i < 3) builder.append('.');
        }
        result = builder.toString();
        return result;
    }

    public static String appendSMSSignatureInfo(Context context, String msgText) {
        // Add user's signature first if this feature is enabled.
        SharedPreferences sp = PreferenceManager.getDefaultSharedPreferences(context);
        if (sp.getBoolean("pref_key_enable_signature", false)) {
            String signature = (sp.getString("pref_key_edit_signature", "")).trim();
            if (signature.length() > 0) {
                String sigBlock = "\n" + signature;
                if (msgText != null && !msgText.endsWith(sigBlock)) {
                    // Signature should be written behind the text in a
                    // newline while the signature has changed.
                    msgText += sigBlock;
                }
            }
        }

        return msgText;
    }

    public static String getPicSuffix(String pic_path) {
        if (pic_path == null || pic_path.indexOf(".") == -1) {
            return "";
        }
        return pic_path.substring(pic_path.lastIndexOf(".") + 1).
                trim().toLowerCase();
    }

    public static boolean insertMessageIntoIcc(int subId, String address,
            String message, int boxId, long timestamp) {
        Log.d(TAG, "MessageUtils: insertMessageIntoIcc subId: " + subId
            + "; address: " + address
            + "; message: " + message
            + "; boxId: " + boxId
            + "; timestamp: " + timestamp);
        byte pdu[] = null;
        int status;
        if (Sms.isOutgoingFolder(boxId)) {
            pdu = SmsMessage.getSubmitPdu(null, address, message, false,
                    subId).encodedMessage;
            status = SmsManager.STATUS_ON_ICC_SENT;
        } else {
            pdu = getDeliveryPdu(null, address, message, timestamp, subId);
            status = SmsManager.STATUS_ON_ICC_READ;
        }
        boolean result = SmsManager.getSmsManagerForSubscriptionId(subId)
                .copyMessageToIcc(null, pdu, status);
        return result;
    }

    /**
     * Generate a Delivery PDU byte array. see getSubmitPdu for reference.
     */
    private static byte[] getDeliveryPdu(String scAddress,
            String destinationAddress, String message, long date,
            int subId) {
        if (isCDMAPhone(subId)) {
            return getCdmaDeliveryPdu(scAddress, destinationAddress, message,
                    date);
        } else {
            return getGsmDeliveryPdu(scAddress, destinationAddress, message,
                    date, null, ENCODING_UNKNOWN);
        }
    }

    public static byte[] getCdmaDeliveryPdu(String scAddress,
            String destinationAddress, String message, long date) {
        // Perform null parameter checks.
        if (message == null || destinationAddress == null) {
            Log.d(TAG, "getCDMADeliveryPdu,message =null");
            return null;
        }

        // according to submit pdu encoding as written in privateGetSubmitPdu

        // MTI = SMS-DELIVERY, UDHI = header != null
        byte[] header = null;
        byte mtiByte = (byte) (0x00 | (header != null ? 0x40 : 0x00));
        ByteArrayOutputStream headerStream = getDeliveryPduHeader(
                destinationAddress, mtiByte);

        ByteArrayOutputStream byteStream = new ByteArrayOutputStream(
                MAX_USER_DATA_BYTES + 40);

        DataOutputStream dos = new DataOutputStream(byteStream);
        // int status,Status of message. See TS 27.005 3.1, "<stat>"

        /* 0 = "REC UNREAD" */
        /* 1 = "REC READ" */
        /* 2 = "STO UNSENT" */
        /* 3 = "STO SENT" */

        try {
            // int uTeleserviceID;
            int uTeleserviceID = 0; // .TELESERVICE_CT_WAP;// int
            dos.writeInt(uTeleserviceID);

            // unsigned char bIsServicePresent
            byte bIsServicePresent = 0;// byte
            dos.writeInt(bIsServicePresent);

            // uServicecategory
            int uServicecategory = 0;// int
            dos.writeInt(uServicecategory);

            // RIL_CDMA_SMS_Address
            // digit_mode
            // number_mode
            // number_type
            // number_plan
            // number_of_digits
            // digits[]
            CdmaSmsAddress destAddr = CdmaSmsAddress.parse(PhoneNumberUtils
                    .cdmaCheckAndProcessPlusCode(destinationAddress));
            if (destAddr == null)
                return null;
            dos.writeByte(destAddr.digitMode);// int
            dos.writeByte(destAddr.numberMode);// int
            dos.writeByte(destAddr.ton);// int
            Log.d(TAG, "message type=" + destAddr.ton + "destination add="
                    + destinationAddress + "message=" + message);
            dos.writeByte(destAddr.numberPlan);// int
            dos.writeByte(destAddr.numberOfDigits);// byte
            dos.write(destAddr.origBytes, 0, destAddr.origBytes.length); // digits

            // RIL_CDMA_SMS_Subaddress
            // Subaddress is not supported.
            dos.writeByte(0); // subaddressType int
            dos.writeByte(0); // subaddr_odd byte
            dos.writeByte(0); // subaddr_nbr_of_digits byte

            SmsHeader smsHeader = new SmsHeader()
                    .fromByteArray(headerStream.toByteArray());
            UserData uData = new UserData();
            uData.payloadStr = message;
            // uData.userDataHeader = smsHeader;
            uData.msgEncodingSet = true;
            uData.msgEncoding = UserData.ENCODING_UNICODE_16;

            BearerData bearerData = new BearerData();
            bearerData.messageType = BearerData.MESSAGE_TYPE_DELIVER;

            bearerData.deliveryAckReq = false;
            bearerData.userAckReq = false;
            bearerData.readAckReq = false;
            bearerData.reportReq = false;

            bearerData.userData = uData;

            byte[] encodedBearerData = BearerData.encode(bearerData);
            if (null != encodedBearerData) {
                // bearer data len
                dos.writeByte(encodedBearerData.length);// int
                Log.d(TAG,
                        "encodedBearerData length=" + encodedBearerData.length);

                // aBearerData
                dos.write(encodedBearerData, 0, encodedBearerData.length);
            } else {
                dos.writeByte(0);
            }

        } catch (IOException e) {
            Log.e(TAG, "Error writing dos", e);
        } finally {
            try {
                if (null != byteStream) {
                    byteStream.close();
                }

                if (null != dos) {
                    dos.close();
                }

                if (null != headerStream) {
                    headerStream.close();
                }
            } catch (IOException e) {
                Log.e(TAG, "Error close dos", e);
            }
        }

        return byteStream.toByteArray();
    }

    /**
     * Generate a Delivery PDU byte array. see getSubmitPdu for reference.
     */
    public static byte[] getGsmDeliveryPdu(String scAddress,
            String destinationAddress, String message, long date, byte[] header,
            int encoding) {
        // Perform null parameter checks.
        if (message == null || destinationAddress == null) {
            return null;
        }

        // MTI = SMS-DELIVERY, UDHI = header != null
        byte mtiByte = (byte) (0x00 | (header != null ? 0x40 : 0x00));
        ByteArrayOutputStream bo = getDeliveryPduHeader(destinationAddress,
                mtiByte);
        // User Data (and length)
        byte[] userData;
        if (encoding == ENCODING_UNKNOWN) {
            // First, try encoding it with the GSM alphabet
            encoding = ENCODING_7BIT;
        }
        try {
            if (encoding == ENCODING_7BIT) {
                userData = GsmAlphabet.stringToGsm7BitPackedWithHeader(message,
                        header, 0, 0);
            } else { // assume UCS-2
                try {
                    userData = encodeUCS2(message, header);
                } catch (UnsupportedEncodingException uex) {
                    Log.e("GSM", "Implausible UnsupportedEncodingException ",
                            uex);
                    return null;
                }
            }
        } catch (EncodeException ex) {
            // Encoding to the 7-bit alphabet failed. Let's see if we can
            // encode it as a UCS-2 encoded message
            try {
                userData = encodeUCS2(message, header);
                encoding = ENCODING_16BIT;
            } catch (UnsupportedEncodingException uex) {
                Log.e("GSM", "Implausible UnsupportedEncodingException ", uex);
                return null;
            }
        }

        if (encoding == ENCODING_7BIT) {
            if ((0xff & userData[0]) > MAX_USER_DATA_SEPTETS) {
                // Message too long
                return null;
            }
            bo.write(0x00);
        } else { // assume UCS-2
            if ((0xff & userData[0]) > MAX_USER_DATA_BYTES) {
                // Message too long
                return null;
            }
            // TP-Data-Coding-Scheme
            // Class 3, UCS-2 encoding, uncompressed
            bo.write(0x0b);
        }
        byte[] timestamp = getTimestamp(date);
        bo.write(timestamp, 0, timestamp.length);

        bo.write(userData, 0, userData.length);
        return bo.toByteArray();
    }

    private static ByteArrayOutputStream getDeliveryPduHeader(
            String destinationAddress, byte mtiByte) {
        ByteArrayOutputStream bo = new ByteArrayOutputStream(
                MAX_USER_DATA_BYTES + 40);
        bo.write(mtiByte);

        byte[] daBytes;
        daBytes = PhoneNumberUtils
                .networkPortionToCalledPartyBCD(destinationAddress);

        if (daBytes == null) {
            Log.d(TAG,
                    "The number can not convert to BCD, it's an An alphanumeric address, "
                            + "destinationAddress = " + destinationAddress);
            // Convert address to GSM 7 bit packed bytes.
            try {
                byte[] numberdata = GsmAlphabet
                        .stringToGsm7BitPacked(destinationAddress);
                // Get the real address data
                byte[] addressData = new byte[numberdata.length - 1];
                System.arraycopy(numberdata, 1, addressData, 0,
                        addressData.length);

                daBytes = new byte[addressData.length + OFFSET_ADDRESS_VALUE];
                // Get the address length
                int addressLen = numberdata[0];
                daBytes[OFFSET_ADDRESS_LENGTH] = (byte) ((addressLen * 7
                        % 4 != 0 ? addressLen * 7 / 4 + 1
                                : addressLen * 7 / 4));
                // Set address type to Alphanumeric according to 3GPP TS 23.040
                // [9.1.2.5]
                daBytes[OFFSET_TOA] = (byte) 0xd0;
                System.arraycopy(addressData, 0, daBytes, OFFSET_ADDRESS_VALUE,
                        addressData.length);
            } catch (Exception e) {
                Log.e(TAG, "Exception when encoding to 7 bit data.");
            }
        } else {
            // destination address length in BCD digits, ignoring TON byte and
            // pad
            // TODO Should be better.
            bo.write((daBytes.length - 1) * 2
                    - ((daBytes[daBytes.length - 1] & 0xf0) == 0xf0 ? 1 : 0));
        }

        // destination address
        bo.write(daBytes, 0, daBytes.length);

        // TP-Protocol-Identifier
        bo.write(0);
        return bo;
    }

    private static byte[] encodeUCS2(String message, byte[] header)
            throws UnsupportedEncodingException {
        byte[] userData, textPart;
        textPart = message.getBytes("utf-16be");

        if (header != null) {
            // Need 1 byte for UDHL
            userData = new byte[header.length + textPart.length + 1];

            userData[0] = (byte) header.length;
            System.arraycopy(header, 0, userData, 1, header.length);
            System.arraycopy(textPart, 0, userData, header.length + 1,
                    textPart.length);
        } else {
            userData = textPart;
        }
        byte[] ret = new byte[userData.length + 1];
        ret[0] = (byte) (userData.length & 0xff);
        System.arraycopy(userData, 0, ret, 1, userData.length);
        return ret;
    }

    private static byte[] getTimestamp(long time) {
        // See TS 23.040 9.2.3.11
        byte[] timestamp = new byte[TIMESTAMP_LENGTH];
        SimpleDateFormat sdf = new SimpleDateFormat("yyMMddkkmmss:Z",
                Locale.US);
        String[] date = sdf.format(time).split(":");
        // generate timezone value
        String timezone = date[date.length - 1];
        String signMark = timezone.substring(0, 1);
        int hour = Integer.parseInt(timezone.substring(1, 3));
        int min = Integer.parseInt(timezone.substring(3));
        int timezoneValue = hour * 4 + min / 15;
        // append timezone value to date[0] (time string)
        String timestampStr = date[0] + timezoneValue;

        int digitCount = 0;
        for (int i = 0; i < timestampStr.length(); i++) {
            char c = timestampStr.charAt(i);
            int shift = ((digitCount & 0x01) == 1) ? 4 : 0;
            timestamp[(digitCount >> 1)] |= (byte) ((charToBCD(c)
                    & 0x0F) << shift);
            digitCount++;
        }

        if (signMark.equals("-")) {
            timestamp[timestamp.length
                    - 1] = (byte) (timestamp[timestamp.length - 1] | 0x08);
        }

        return timestamp;
    }

    private static int charToBCD(char c) {
        if (c >= '0' && c <= '9') {
            return c - '0';
        } else {
            throw new RuntimeException("invalid char for BCD " + c);
        }
    }

}
