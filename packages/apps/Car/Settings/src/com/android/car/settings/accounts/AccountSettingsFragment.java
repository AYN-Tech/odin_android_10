/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.car.settings.accounts;

import android.car.userlib.CarUserManagerHelper;
import android.content.Context;
import android.os.Bundle;
import android.provider.Settings;

import androidx.annotation.XmlRes;

import com.android.car.settings.R;
import com.android.car.settings.common.SettingsFragment;
import com.android.car.ui.toolbar.MenuItem;

import java.util.Collections;
import java.util.List;

/**
 * Lists the user's accounts and any related options.
 */
public class AccountSettingsFragment extends SettingsFragment {
    private MenuItem mAddAccountButton;

    @Override
    @XmlRes
    protected int getPreferenceScreenResId() {
        return R.xml.account_settings_fragment;
    }

    @Override
    protected List<MenuItem> getToolbarMenuItems() {
        return Collections.singletonList(mAddAccountButton);
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        boolean canModifyAccounts = new CarUserManagerHelper(getContext())
                .canCurrentProcessModifyAccounts();

        mAddAccountButton = new MenuItem.Builder(getContext())
                .setTitle(R.string.user_add_account_menu)
                .setOnClickListener(i -> onAddAccountClicked())
                .setVisible(canModifyAccounts)
                .build();
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);

        String[] authorities = getActivity().getIntent().getStringArrayExtra(
                Settings.EXTRA_AUTHORITIES);
        if (authorities != null) {
            use(AccountListPreferenceController.class, R.string.pk_account_list)
                    .setAuthorities(authorities);
        }
    }

    private void onAddAccountClicked() {
        launchFragment(new ChooseAccountFragment());
    }
}
