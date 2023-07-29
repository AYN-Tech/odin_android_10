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

package com.android.car.dialer.ui.common;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.TextView;

import com.android.car.dialer.R;
import com.android.car.telephony.common.Contact;
import com.android.car.telephony.common.PhoneNumber;

import java.util.ArrayList;
import java.util.List;

/**
 * {@link BaseAdapter} that presents the {@link PhoneNumber} and its type as two line list
 * item with stars to indicate favorite state or user selection to add to favorite. Currently
 * favorite phone number is set to disabled so user can not take any action for an existing favorite
 * phone number.
 */
public class FavoritePhoneNumberListAdapter extends BaseAdapter {
    private final Context mContext;
    private final FavoritePhoneNumberPresenter mFavoritePhoneNumberPresenter;
    private final List<PhoneNumber> mPhoneNumbers;
    private Contact mContact;

    /**
     * A presenter that presents the favorite state for phone number and provides the click
     * listener.
     */
    public interface FavoritePhoneNumberPresenter {
        /**
         * Provides the click listener for the given phone number and its present view.
         */
        void onItemClicked(PhoneNumber phoneNumber, View itemView);
    }

    public FavoritePhoneNumberListAdapter(Context context,
            FavoritePhoneNumberPresenter favoritePhoneNumberPresenter) {
        mContext = context;
        mFavoritePhoneNumberPresenter = favoritePhoneNumberPresenter;
        mPhoneNumbers = new ArrayList<>();
    }

    /**
     * Sets the phone numbers to display
     */
    public void setPhoneNumbers(Contact contact, List<PhoneNumber> phoneNumbers) {
        mPhoneNumbers.clear();
        mContact = contact;
        mPhoneNumbers.addAll(phoneNumbers);

        notifyDataSetChanged();
    }

    public Contact getContact() {
        return mContact;
    }

    @Override
    public int getCount() {
        return mPhoneNumbers.size();
    }

    @Override
    public PhoneNumber getItem(int position) {
        return mPhoneNumbers.get(position);
    }

    @Override
    public long getItemId(int position) {
        return position;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        View itemView;
        if (convertView == null) {
            itemView = LayoutInflater.from(mContext).inflate(
                    R.layout.add_favorite_number_list_item, parent, false);
        } else {
            itemView = convertView;
        }
        PhoneNumber phoneNumber = getItem(position);
        bind(phoneNumber, itemView);
        return itemView;
    }

    void bind(PhoneNumber phoneNumber, View itemView) {
        TextView phoneNumberView = itemView.findViewById(R.id.phone_number);
        TextView phoneNumberDescriptionView = itemView.findViewById(R.id.phone_number_description);
        phoneNumberView.setText(phoneNumber.getRawNumber());
        CharSequence readableLabel = phoneNumber.getReadableLabel(itemView.getResources());

        if (phoneNumber.isFavorite()) {
            phoneNumberDescriptionView.setText(
                    itemView.getResources().getString(R.string.favorite_number_description,
                            readableLabel));
            itemView.setActivated(true);
            itemView.setEnabled(false);
        } else {
            phoneNumberDescriptionView.setText(readableLabel);
            itemView.setActivated(false);
            itemView.setEnabled(true);
            itemView.setOnClickListener(
                    view -> mFavoritePhoneNumberPresenter.onItemClicked(phoneNumber, itemView));
        }
    }
}
