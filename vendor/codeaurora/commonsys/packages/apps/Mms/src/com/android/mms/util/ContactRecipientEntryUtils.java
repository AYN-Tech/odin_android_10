/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 * Not a Contribution.
 * Copyright (C) 2015 The Android Open Source Project
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

package com.android.mms.util;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageManager;
import android.database.Cursor;
import android.net.Uri;
import android.provider.ContactsContract;
import android.provider.ContactsContract.CommonDataKinds.GroupMembership;
import android.provider.ContactsContract.CommonDataKinds.Email;
import android.provider.ContactsContract.CommonDataKinds.Phone;
import android.provider.ContactsContract.Data;
import android.provider.ContactsContract.Directory;
import android.provider.ContactsContract.DisplayNameSources;
import android.provider.ContactsContract.Groups;
import android.telephony.PhoneNumberUtils;
import android.text.TextUtils;
import android.util.Log;
import android.util.Patterns;

import com.android.mms.R;
import com.android.mms.data.Contact;
import com.android.ex.chips.RecipientEntry;
import com.google.i18n.phonenumbers.NumberParseException;
import com.google.i18n.phonenumbers.Phonenumber.PhoneNumber;
import com.google.i18n.phonenumbers.PhoneNumberUtil;
import com.google.i18n.phonenumbers.PhoneNumberUtil.PhoneNumberFormat;

import java.util.ArrayList;
import java.util.Hashtable;
import java.util.List;
import java.util.Locale;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Utility class including logic to list, filter, and lookup phone and emails in CP2.
 */
public class ContactRecipientEntryUtils {

    /**
     * Index of different columns in phone or email queries. All queries below should confirm to
     * this column content and ordering so that caller can use the uniformed way to process returned
     * cursors.
     */
    public static final int INDEX_CONTACT_ID = 0;
    public static final int INDEX_DISPLAY_NAME = 1;
    public static final int INDEX_PHOTO_URI = 2;
    public static final int INDEX_PHONE_EMAIL = 3;
    public static final int INDEX_PHONE_EMAIL_TYPE = 4;
    public static final int INDEX_PHONE_EMAIL_LABEL = 5;
    public static final int INDEX_LOOKUP_KEY = 6;

    private static final int MINIMUM_PHONE_NUMBER_LENGTH_TO_FORMAT = 6;

    private static final int INVALID_GROUP_ID = -1;
    private static final int GROUP_ENTRY_DESTINATION_ID = -1;
    private static final int GROUP_ENTRY_DATE_ID = -1;

    /**
     * Constants for listing and filtering phones.
     */
    public static class PhoneQuery {
        public static final String SORT_KEY = Phone.SORT_KEY_PRIMARY;

        public static final String[] PROJECTION = new String[] {
                Phone.CONTACT_ID, // 0
                Phone.DISPLAY_NAME_PRIMARY, // 1
                Phone.PHOTO_THUMBNAIL_URI, // 2
                Phone.NUMBER, // 3
                Phone.TYPE, // 4
                Phone.LABEL, // 5
                Phone.LOOKUP_KEY, // 6
                Phone._ID, // 7
                PhoneQuery.SORT_KEY, // 8
        };
    }

    /**
     * Constants for listing and filtering emails.
     */
    public static class EmailQuery {
        public static final String SORT_KEY = Email.SORT_KEY_PRIMARY;

        public static final String[] PROJECTION = new String[] {
                Email.CONTACT_ID, // 0
                Email.DISPLAY_NAME_PRIMARY, // 1
                Email.PHOTO_THUMBNAIL_URI, // 2
                Email.ADDRESS, // 3
                Email.TYPE, // 4
                Email.LABEL, // 5
                Email.LOOKUP_KEY, // 6
                Email._ID, // 7
                EmailQuery.SORT_KEY, // 8
        };
    }

    /**
     * A generated special contact which says "Send to xxx" in the contact list, which allows a user
     * to direct send an SMS to a number that was manually typed in.
     */
    public static final long CONTACT_ID_SENDTO_DESTINATION = -1001;

    /**
     * Matched Group in the contact list, which allows a user to direct send an SMS
     * to all the numbers that is the group.
     */
    public static final long CONTACT_ID_GROUP_DESTINATION = -1002;

    /**
     * Returns true if the given entry is a special send to number item.
     */
    public static boolean isSendToDestinationContact(final RecipientEntry entry) {
        return entry.getContactId() == CONTACT_ID_SENDTO_DESTINATION;
    }

    /**
     * Construct a special "Send to xxx" entry for a given destination.
     */
    public static RecipientEntry constructSendToDestinationEntry(final String destination) {
        final Uri avatarUri = null;
        return RecipientEntry.constructTopLevelEntry(null, DisplayNameSources.STRUCTURED_NAME,
                destination, RecipientEntry.INVALID_DESTINATION_TYPE, null,
                CONTACT_ID_SENDTO_DESTINATION, null, CONTACT_ID_SENDTO_DESTINATION, avatarUri,
                true, null);
    }

    /**
     * Creates a RecipientEntry for PhoneQuery result. The result is then displayed in the contact
     * search drop down or as replacement chips in the chips edit box.
     */
    public static RecipientEntry createRecipientEntryForPhoneQuery(final Cursor cursor,
            final boolean isFirstLevel) {
        final long contactId = cursor.getLong(INDEX_CONTACT_ID);
        final String displayName = cursor.getString(INDEX_DISPLAY_NAME);
        final String photoThumbnailUri = cursor.getString(INDEX_PHOTO_URI);
        final String destination = cursor.getString(INDEX_PHONE_EMAIL);
        final int destinationType = cursor.getInt(INDEX_PHONE_EMAIL_TYPE);
        final String destinationLabel = cursor.getString(INDEX_PHONE_EMAIL_LABEL);
        final String lookupKey = cursor.getString(INDEX_LOOKUP_KEY);

        // PhoneQuery uses the contact id as the data id ("_id").
        final long dataId = contactId;

        return createRecipientEntry(displayName,
                DisplayNameSources.STRUCTURED_NAME, destination, destinationType,
                destinationLabel, contactId, lookupKey, dataId, photoThumbnailUri,
                isFirstLevel);
    }

    /**
     * Creates a RecipientEntry by contact.
     */
    public static RecipientEntry createRecipientEntryByContact(final Contact contact,
            final boolean isFirstLevel) {
        final long contactId = contact.getPersonId();
        final String displayName = contact.getName();
        String photoThumbnailUri = null;
        if(contact.getThumbnailUri() != null) {
            photoThumbnailUri = contact.getThumbnailUri().toString();
        }
        final String destination = contact.getNumber();
        final int destinationType = contact.getContactMethodType();
        final String destinationLabel = contact.getLabel();
        final String lookupKey = contact.getLookup();
        final long dataId = contact.getContactMethodId();

        return createRecipientEntry(displayName,
                DisplayNameSources.STRUCTURED_NAME, destination, destinationType,
                destinationLabel, contactId, lookupKey, dataId, photoThumbnailUri,
                isFirstLevel);
    }


    /**
     * Creates a RecipientEntry from the provided data fields (from the contacts cursor).
     * @param firstLevel whether this item is the first entry of this contact in the list.
     */
    public static RecipientEntry createRecipientEntry(final String displayName,
            final int displayNameSource, final String destination, final int destinationType,
            final String destinationLabel, final long contactId, final String lookupKey,
            final long dataId, final String photoThumbnailUri, final boolean firstLevel) {
        if (firstLevel) {
            return RecipientEntry.constructTopLevelEntry(displayName, displayNameSource,
                    destination, destinationType, destinationLabel, contactId, null, dataId,
                    photoThumbnailUri, true, lookupKey);
        } else {
            return RecipientEntry.constructSecondLevelEntry(displayName, displayNameSource,
                    destination, destinationType, destinationLabel, contactId, null, dataId,
                    photoThumbnailUri, true, lookupKey);
        }
    }

    /**
     * Get a list of destinations (phone, email) matching the partial destination.
     */
    public static Cursor filterDestination(final Context context,
            final String destination) {
        if (!hasPermission(context, Manifest.permission.READ_CONTACTS)) {
            return performSynchronousQuery(context, null, null, null, null, null);
        }
        if (shouldFilterForEmail(destination)) {
            return filterEmails(context, destination);
        } else {
            return filterPhones(context, destination);
        }
    }

    private static Hashtable<String, Integer> sPermissions = new Hashtable<String, Integer>();

    /**
     * Check if the app has the specified permission. If it does not, the app needs to use
     * {@link android.app.Activity#requestPermission}. Note that if it returns true, it cannot
     * return false in the same process as the OS kills the process when any permission is revoked.
     * @param permission A permission from {@link android.Manifest.permission}
     */
    public static boolean hasPermission(final Context context, final String permission) {
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.M) {
            // It is safe to cache the PERMISSION_GRANTED result as the process gets killed if the
            // user revokes the permission setting. However, PERMISSION_DENIED should not be
            // cached as the process does not get killed if the user enables the permission setting.
            if (!sPermissions.containsKey(permission)
                    || sPermissions.get(permission) == PackageManager.PERMISSION_DENIED) {
                // final Context context = Factory.get().getApplicationContext();
                final int permissionState = context.checkSelfPermission(permission);
                sPermissions.put(permission, permissionState);
            }
            return sPermissions.get(permission) == PackageManager.PERMISSION_GRANTED;
        } else {
            return true;
        }
    }

    /**
     * Returns whether the search text indicates an email based search or a phone number based one.
     */
    private static boolean shouldFilterForEmail(final String searchText) {
        return searchText != null && searchText.contains("@");
    }

    private static Cursor filterEmails(final Context context, final String query) {
        final Uri uri = Email.CONTENT_FILTER_URI.buildUpon()
                .appendPath(query).appendQueryParameter(
                        ContactsContract.DIRECTORY_PARAM_KEY, String.valueOf(Directory.DEFAULT))
                .build();
        return performSynchronousQuery(context, uri, EmailQuery.PROJECTION, null,
                null, EmailQuery.SORT_KEY);
    }

    private static Cursor filterPhones(final Context context, final String query) {
        final Uri uri = Phone.CONTENT_FILTER_URI.buildUpon()
                .appendPath(query).appendQueryParameter(
                        ContactsContract.DIRECTORY_PARAM_KEY, String.valueOf(Directory.DEFAULT))
                .build();
        return performSynchronousQuery(context, uri, PhoneQuery.PROJECTION, null,
                null, PhoneQuery.SORT_KEY);
    }

    private static Cursor performSynchronousQuery(final Context context, final Uri uri,
            final String[] projection,
            final String selection, final String[] selectionArgs, final String sortOrder) {
        if (uri == null) {
            return null;
        } else {
            return context.getContentResolver().query(uri, projection, selection,
                    selectionArgs, sortOrder);
        }
    }

    /**
     * Gets the display name for contact list only. For most cases this is the same as the normal
     * contact name, but there are cases where these two differ. For example, for the send to typed
     * number item, we'd like to show "Send to xxx" in the contact list. However, when this item is
     * actually added to the chips edit box, we would like to show just the phone number (i.e. no
     * display name).
     */
    public static String getDisplayNameForContactList(final Context context,
            final RecipientEntry entry) {
        if (entry.getContactId() == CONTACT_ID_SENDTO_DESTINATION) {
            return context.getResources().getString(
                    R.string.contact_list_send_to_text, formatDestination(entry));
        } else if (!TextUtils.isEmpty(entry.getDisplayName())) {
            return entry.getDisplayName();
        } else {
            return formatDestination(entry);
        }
    }

    public static String formatDestination(final RecipientEntry entry) {
        return formatForDisplay(entry.getDestination());
    }

    /**
     * Format a phone number for displaying, using system locale country. If the country code
     * matches between the system locale and the input phone number, it will be formatted into
     * NATIONAL format, otherwise, the INTERNATIONAL format
     * @param phoneText The original phone text
     * @return formatted number
     */
    public static String formatForDisplay(final String phoneText) {
        // Only format a valid number which length >=6
        if (TextUtils.isEmpty(phoneText) ||
                phoneText.replaceAll("\\D", "").length() < MINIMUM_PHONE_NUMBER_LENGTH_TO_FORMAT) {
            return phoneText;
        }
        final PhoneNumberUtil phoneNumberUtil = PhoneNumberUtil.getInstance();
        final String systemCountry = getLocaleCountry();
        final int systemCountryCode = phoneNumberUtil.getCountryCodeForRegion(systemCountry);
        try {
            final PhoneNumber parsedNumber = phoneNumberUtil.parse(phoneText, systemCountry);
            final PhoneNumberFormat phoneNumberFormat =
                    (systemCountryCode > 0 && parsedNumber.getCountryCode() == systemCountryCode) ?
                            PhoneNumberFormat.NATIONAL : PhoneNumberFormat.INTERNATIONAL;
            return phoneNumberUtil.format(parsedNumber, phoneNumberFormat);
        } catch (NumberParseException e) {
            return phoneText;
        }
    }

    /**
     * Get the ISO country code from system locale setting
     * @return the ISO country code from system locale
     */
    private static String getLocaleCountry() {
        final String country = Locale.getDefault().getCountry();
        if (TextUtils.isEmpty(country)) {
            return null;
        }
        return country.toUpperCase();
    }

    /**
     * Returns whether the given destination is valid for sending SMS/MMS message.
     */
    public static boolean isValidSmsMmsDestination(final String destination) {
        return PhoneNumberUtils.isWellFormedSmsAddress(destination) ||
                isEmailAddress(destination);
    }

    /**
     * Returns true if the address is an email address
     * @param address the input address to be tested
     * @return true if address is an email address
     */
    public static boolean isEmailAddress(final String address) {
        if (TextUtils.isEmpty(address)) {
            return false;
        }

        final String s = extractAddrSpec(address);
        final Matcher match = Patterns.EMAIL_ADDRESS.matcher(s);
        return match.matches();
    }

    /**
     * mailbox = name-addr name-addr = [display-name] angle-addr angle-addr = [CFWS] "<" addr-spec
     * ">" [CFWS]
     */
    public static final Pattern NAME_ADDR_EMAIL_PATTERN =
            Pattern.compile("\\s*(\"[^\"]*\"|[^<>\"]+)\\s*<([^<>]+)>\\s*");

    public static String extractAddrSpec(final String address) {
        final Matcher match = NAME_ADDR_EMAIL_PATTERN.matcher(address);

        if (match.matches()) {
            return match.group(2);
        }
        return address;
    }
}
