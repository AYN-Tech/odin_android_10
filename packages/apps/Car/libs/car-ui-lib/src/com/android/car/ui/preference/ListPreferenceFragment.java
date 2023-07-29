/*
 * Copyright 2019 The Android Open Source Project
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

package com.android.car.ui.preference;

import static com.android.car.ui.preference.PreferenceDialogFragment.ARG_KEY;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.preference.DialogPreference;
import androidx.preference.ListPreference;
import androidx.preference.Preference;

import com.android.car.ui.R;
import com.android.car.ui.recyclerview.CarUiContentListItem;
import com.android.car.ui.recyclerview.CarUiListItem;
import com.android.car.ui.recyclerview.CarUiListItemAdapter;
import com.android.car.ui.recyclerview.CarUiRecyclerView;
import com.android.car.ui.toolbar.Toolbar;

import java.util.ArrayList;
import java.util.List;

/**
 * A fragment that provides a layout with a list of options associated with a {@link
 * ListPreference}.
 */
public class ListPreferenceFragment extends Fragment {

    private ListPreference mPreference;
    private CarUiContentListItem mSelectedItem;

    /**
     * Returns a new instance of {@link ListPreferenceFragment} for the {@link ListPreference} with
     * the given {@code key}.
     */
    static ListPreferenceFragment newInstance(String key) {
        ListPreferenceFragment fragment = new ListPreferenceFragment();
        Bundle b = new Bundle(/* capacity= */ 1);
        b.putString(ARG_KEY, key);
        fragment.setArguments(b);
        return fragment;
    }

    @Nullable
    @Override
    public View onCreateView(
            @NonNull LayoutInflater inflater, @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        return inflater.inflate(R.layout.car_ui_list_preference, container, false);
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        final CarUiRecyclerView carUiRecyclerView = view.requireViewById(R.id.list);
        final Toolbar toolbar = view.requireViewById(R.id.toolbar);

        carUiRecyclerView.setPadding(0, toolbar.getHeight(), 0, 0);
        toolbar.registerToolbarHeightChangeListener(newHeight -> {
            if (carUiRecyclerView.getPaddingTop() == newHeight) {
                return;
            }

            int oldHeight = carUiRecyclerView.getPaddingTop();
            carUiRecyclerView.setPadding(0, newHeight, 0, 0);
            carUiRecyclerView.scrollBy(0, oldHeight - newHeight);
        });

        carUiRecyclerView.setClipToPadding(false);
        mPreference = getListPreference();
        toolbar.setTitle(mPreference.getTitle());

        CharSequence[] entries = mPreference.getEntries();
        CharSequence[] entryValues = mPreference.getEntryValues();

        if (entries == null || entryValues == null) {
            throw new IllegalStateException(
                    "ListPreference requires an entries array and an entryValues array.");
        }

        if (entries.length != entryValues.length) {
            throw new IllegalStateException(
                    "ListPreference entries array length does not match entryValues array length.");
        }

        int selectedEntryIndex = mPreference.findIndexOfValue(mPreference.getValue());
        List<CarUiListItem> listItems = new ArrayList<>();
        CarUiListItemAdapter adapter = new CarUiListItemAdapter(listItems);

        for (int i = 0; i < entries.length; i++) {
            String entry = entries[i].toString();
            CarUiContentListItem item = new CarUiContentListItem();
            item.setAction(CarUiContentListItem.Action.RADIO_BUTTON);
            item.setTitle(entry);

            if (i == selectedEntryIndex) {
                item.setChecked(true);
                mSelectedItem = item;
            }

            item.setOnCheckedChangedListener((listItem, isChecked) -> {
                if (mSelectedItem != null) {
                    mSelectedItem.setChecked(false);
                    adapter.notifyItemChanged(listItems.indexOf(mSelectedItem));
                }
                mSelectedItem = listItem;
            });

            listItems.add(item);
        }

        toolbar.registerOnBackListener(() -> {
            if (mSelectedItem != null) {
                int selectedIndex = listItems.indexOf(mSelectedItem);
                String entryValue = entryValues[selectedIndex].toString();

                if (mPreference.callChangeListener(entryValue)) {
                    mPreference.setValue(entryValue);
                }
            }

            return false;
        });

        carUiRecyclerView.setAdapter(adapter);
    }

    private ListPreference getListPreference() {
        if (getArguments() == null) {
            throw new IllegalStateException("Preference arguments cannot be null");
        }

        String key = getArguments().getString(ARG_KEY);
        DialogPreference.TargetFragment fragment =
                (DialogPreference.TargetFragment) getTargetFragment();

        if (key == null) {
            throw new IllegalStateException(
                    "ListPreference key not found in Fragment arguments");
        }

        if (fragment == null) {
            throw new IllegalStateException(
                    "Target fragment must be registered before displaying ListPreference "
                            + "screen.");
        }

        Preference preference = fragment.findPreference(key);

        if (!(preference instanceof ListPreference)) {
            throw new IllegalStateException(
                    "Cannot use ListPreferenceFragment with a preference that is not of type "
                            + "ListPreference");
        }

        return (ListPreference) preference;
    }
}
