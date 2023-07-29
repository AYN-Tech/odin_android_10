/*
 * Copyright (C) 2012 The Android Open Source Project
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

import android.content.Context;
import android.database.Cursor;
import android.support.v4.util.Pair;
import android.text.TextUtils;
import android.text.util.Rfc822Token;
import android.util.Log;
import android.widget.Filter;
import android.widget.Filterable;

import com.android.ex.chips.BaseRecipientAdapter;
import com.android.ex.chips.RecipientEntry;
import com.android.mms.LogTag;
import com.android.mms.R;
import com.android.mms.util.ContactRecipientEntryUtils;

import java.text.Collator;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;

public class ChipsRecipientAdapter extends BaseRecipientAdapter {
    private static final int DEFAULT_PREFERRED_MAX_RESULT_COUNT = 10;

    public ChipsRecipientAdapter(Context context) {
        // The Chips UI is email-centric by default. By setting QUERY_TYPE_PHONE, the chips UI
        // will operate with phone numbers instead of emails.
        super(context, DEFAULT_PREFERRED_MAX_RESULT_COUNT, QUERY_TYPE_PHONE);
    }

    // Will be called from {@link AutoCompleteTextView} to prepare auto-complete list.
    @Override
    public Filter getFilter() {
        return new ContactFilter();
    }

    /**
     * An asynchronous filter used for loading two data sets: email rows from the local
     * contact provider and the list of {@link Directory}'s.
     */
    private final class ContactFilter extends Filter {

        // Used to sort filtered contacts when it has combined results from email and phone.
        private final RecipientEntryComparator mComparator = new RecipientEntryComparator();

        private Pair<Cursor, Boolean> getFilteredResultsCursor(final Context context,
                final String searchText) {
            Cursor resultsCursor = ContactRecipientEntryUtils.filterDestination(
                    getContext(), searchText);
            return Pair.create(resultsCursor, true);
        }

        @Override
        protected FilterResults performFiltering(final CharSequence constraint) {
            final FilterResults results = new FilterResults();

            // No query, return empty results.
            if (TextUtils.isEmpty(constraint)) {
                clearTempEntries();
                return results;
            }

            final String searchText = constraint.toString();

            // Query for auto-complete results, since performFiltering() is not done on the
            // main thread, perform the cursor loader queries directly.
            final Pair<Cursor, Boolean> filteredResults = getFilteredResultsCursor(getContext(),
                    searchText);
            final Cursor cursor = filteredResults.first;
            final boolean sorted = filteredResults.second;
            if (cursor != null) {
                final List<RecipientEntry> entries = new ArrayList<RecipientEntry>();

                // First check if the constraint is a valid SMS destination. If so, add the
                // destination as a suggestion item to the drop down.
                if (ContactRecipientEntryUtils.isValidSmsMmsDestination(searchText)) {
                    entries.add(ContactRecipientEntryUtils
                            .constructSendToDestinationEntry(searchText));
                }

                HashSet<Long> existingContactIds = new HashSet<Long>();
                while (cursor.moveToNext()) {
                    // Make sure there's only one first-level contact (i.e. contact for which
                    // we show the avatar picture and name) for every contact id.
                    final long contactId = cursor
                            .getLong(ContactRecipientEntryUtils.INDEX_CONTACT_ID);
                    final boolean isFirstLevel = !existingContactIds.contains(contactId);
                    if (isFirstLevel) {
                        existingContactIds.add(contactId);
                    }
                    entries.add(ContactRecipientEntryUtils.createRecipientEntryForPhoneQuery(
                            cursor,
                            isFirstLevel));
                }

                if (!sorted) {
                    Collections.sort(entries, mComparator);
                }
                results.values = entries;
                results.count = 1;
                cursor.close();
            }
            return results;
        }

        @Override
        protected void publishResults(final CharSequence constraint, FilterResults results) {
            mCurrentConstraint = constraint;
            clearTempEntries();
            try {
                if (results.values != null) {
                    @SuppressWarnings("unchecked")
                    final List<RecipientEntry> entries = (List<RecipientEntry>) results.values;
                    updateEntries(entries);
                } else {
                    updateEntries(Collections.<RecipientEntry>emptyList());
                }
            } catch (IllegalArgumentException e) {
                Log.e(LogTag.TAG, "ChipsRecipientAdapter publishResults exception:" + e);
            }
        }

        // Subclasses of Filter should override convertResultToString() to convert their results.
        @Override
        public CharSequence convertResultToString(Object resultValue) {
            final RecipientEntry entry = (RecipientEntry)resultValue;
            final String displayName = entry.getDisplayName();
            final String emailAddress = entry.getDestination();
            if (TextUtils.isEmpty(displayName) || TextUtils.equals(displayName, emailAddress)) {
                 return emailAddress;
            } else {
                return new Rfc822Token(displayName, emailAddress, null).toString();
            }
        }

        private class RecipientEntryComparator implements Comparator<RecipientEntry> {
            private final Collator mCollator;

            public RecipientEntryComparator() {
                mCollator = Collator.getInstance(Locale.getDefault());
                mCollator.setStrength(Collator.PRIMARY);
            }

            /**
             * Compare two RecipientEntry's, first by locale-aware display name comparison, then by
             * contact id comparison, finally by first-level-ness comparison.
             */
            @Override
            public int compare(RecipientEntry lhs, RecipientEntry rhs) {
                // Send-to-destinations always appear before everything else.
                final boolean sendToLhs = ContactRecipientEntryUtils
                        .isSendToDestinationContact(lhs);
                final boolean sendToRhs = ContactRecipientEntryUtils
                        .isSendToDestinationContact(lhs);
                if (sendToLhs != sendToRhs) {
                    if (sendToLhs) {
                        return -1;
                    } else if (sendToRhs) {
                        return 1;
                    }
                }

                final int displayNameCompare = mCollator.compare(lhs.getDisplayName(),
                        rhs.getDisplayName());
                if (displayNameCompare != 0) {
                    return displayNameCompare;
                }

                // Long.compare could accomplish the following three lines, but this is only
                // available in API 19+
                final long lhsContactId = lhs.getContactId();
                final long rhsContactId = rhs.getContactId();
                final int contactCompare = lhsContactId < rhsContactId ? -1 :
                        (lhsContactId == rhsContactId ? 0 : 1);
                if (contactCompare != 0) {
                    return contactCompare;
                }

                // These are the same contact. Make sure first-level contacts always
                // appear at the front.
                if (lhs.isFirstLevel()) {
                    return -1;
                } else if (rhs.isFirstLevel()) {
                    return 1;
                } else {
                    return 0;
                }
            }
        }
    }
}
