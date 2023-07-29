/*
 * Copyright (C) 2019 The Android Open Source Project
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

package com.android.car.dialer.ui.search;

import android.app.Application;
import android.content.Context;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.net.Uri;
import android.provider.ContactsContract;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.lifecycle.AndroidViewModel;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.MediatorLiveData;
import androidx.lifecycle.MutableLiveData;

import com.android.car.dialer.R;
import com.android.car.dialer.livedata.SharedPreferencesLiveData;
import com.android.car.dialer.ui.common.entity.ContactSortingInfo;
import com.android.car.telephony.common.Contact;
import com.android.car.telephony.common.InMemoryPhoneBook;
import com.android.car.telephony.common.ObservableAsyncQuery;
import com.android.car.telephony.common.QueryParam;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * {link AndroidViewModel} used for search functionality.
 */
public class ContactResultsViewModel extends AndroidViewModel {
    private static final String[] CONTACT_DETAILS_PROJECTION = {
            ContactsContract.Contacts._ID,
            ContactsContract.Contacts.LOOKUP_KEY
    };

    private final ContactResultsLiveData mContactSearchResultsLiveData;
    private final MutableLiveData<String> mSearchQueryLiveData;
    private final SharedPreferencesLiveData mSharedPreferencesLiveData;

    public ContactResultsViewModel(@NonNull Application application) {
        super(application);
        mSearchQueryLiveData = new MutableLiveData<>();
        mSharedPreferencesLiveData =
                new SharedPreferencesLiveData(application, R.string.sort_order_key);
        mContactSearchResultsLiveData = new ContactResultsLiveData(application,
                mSearchQueryLiveData, mSharedPreferencesLiveData);
    }

    void setSearchQuery(String searchQuery) {
        if (TextUtils.equals(mSearchQueryLiveData.getValue(), searchQuery)) {
            return;
        }

        mSearchQueryLiveData.setValue(searchQuery);
    }

    LiveData<List<Contact>> getContactSearchResults() {
        return mContactSearchResultsLiveData;
    }

    String getSearchQuery() {
        return mSearchQueryLiveData.getValue();
    }

    private static class ContactResultsLiveData extends MediatorLiveData<List<Contact>> {
        private final Context mContext;
        private final SearchQueryParamProvider mSearchQueryParamProvider;
        private final ObservableAsyncQuery mObservableAsyncQuery;
        private final LiveData<String> mSearchQueryLiveData;
        private final LiveData<List<Contact>> mContactListLiveData;
        private final SharedPreferencesLiveData mSharedPreferencesLiveData;

        ContactResultsLiveData(Context context,
                LiveData<String> searchQueryLiveData,
                SharedPreferencesLiveData sharedPreferencesLiveData) {
            mContext = context;
            mSearchQueryParamProvider = new SearchQueryParamProvider(searchQueryLiveData);
            mObservableAsyncQuery = new ObservableAsyncQuery(mSearchQueryParamProvider,
                    context.getContentResolver(), this::onQueryFinished);

            mContactListLiveData = InMemoryPhoneBook.get().getContactsLiveData();
            addSource(mContactListLiveData, this::onContactsChange);
            mSearchQueryLiveData = searchQueryLiveData;
            addSource(mSearchQueryLiveData, this::onSearchQueryChanged);

            mSharedPreferencesLiveData = sharedPreferencesLiveData;
            addSource(mSharedPreferencesLiveData, this::onSortOrderChanged);
        }

        private void onContactsChange(List<Contact> contactList) {
            if (contactList == null || contactList.isEmpty()) {
                mObservableAsyncQuery.stopQuery();
                setValue(Collections.emptyList());
            } else {
                onSearchQueryChanged(mSearchQueryLiveData.getValue());
            }
        }

        private void onSearchQueryChanged(String searchQuery) {
            if (TextUtils.isEmpty(searchQuery)) {
                mObservableAsyncQuery.stopQuery();
                List<Contact> contacts = mContactListLiveData.getValue();
                setValue(contacts == null ? Collections.emptyList() : contacts);
            } else {
                mObservableAsyncQuery.startQuery();
            }
        }

        private void onSortOrderChanged(SharedPreferences unusedSharedPreferences) {
            setValue(getValue());
        }

        private void onQueryFinished(@Nullable Cursor cursor) {
            if (cursor == null) {
                setValue(Collections.emptyList());
                return;
            }

            List<Contact> contacts = new ArrayList<>();
            while (cursor.moveToNext()) {
                int lookupKeyColIdx = cursor.getColumnIndex(ContactsContract.Contacts.LOOKUP_KEY);
                List<Contact> lookupResults = InMemoryPhoneBook.get().lookupContactByKey(
                        cursor.getString(lookupKeyColIdx));
                contacts.addAll(lookupResults);
            }
            setValue(contacts);
            cursor.close();
        }

        @Override
        public void setValue(List<Contact> contacts) {
            if (contacts != null && !contacts.isEmpty()) {
                Collections.sort(contacts,
                        ContactSortingInfo.getSortingInfo(mContext,
                                mSharedPreferencesLiveData).first);
            }
            super.setValue(contacts);
        }
    }

    private static class SearchQueryParamProvider implements QueryParam.Provider {
        private final LiveData<String> mSearchQueryLiveData;

        private SearchQueryParamProvider(LiveData<String> searchQueryLiveData) {
            mSearchQueryLiveData = searchQueryLiveData;
        }

        @Nullable
        @Override
        public QueryParam getQueryParam() {
            Uri lookupUri = Uri.withAppendedPath(ContactsContract.Contacts.CONTENT_FILTER_URI,
                    Uri.encode(mSearchQueryLiveData.getValue()));
            return new QueryParam(lookupUri, CONTACT_DETAILS_PROJECTION,
                    ContactsContract.Contacts.HAS_PHONE_NUMBER + "!=0",
                    /* selectionArgs= */null, /* orderBy= */null);
        }
    }
}
