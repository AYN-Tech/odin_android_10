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

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.doReturn;

import android.car.userlib.CarUserManagerHelper;
import android.content.pm.UserInfo;

import com.android.car.settings.CarSettingsRobolectricTestRunner;
import com.android.car.settings.R;
import com.android.car.settings.testutils.FragmentController;
import com.android.car.settings.testutils.ShadowAccountManager;
import com.android.car.settings.testutils.ShadowCarUserManagerHelper;
import com.android.car.settings.testutils.ShadowContentResolver;
import com.android.car.ui.toolbar.Toolbar;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

/**
 * Tests for AccountSettingsFragment class.
 */
@RunWith(CarSettingsRobolectricTestRunner.class)
@Config(shadows = {ShadowCarUserManagerHelper.class, ShadowAccountManager.class,
        ShadowContentResolver.class})
public class AccountSettingsFragmentTest {
    private FragmentController<AccountSettingsFragment> mFragmentController;
    private AccountSettingsFragment mFragment;

    @Mock
    private CarUserManagerHelper mMockCarUserManagerHelper;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        // Set up user info
        ShadowCarUserManagerHelper.setMockInstance(mMockCarUserManagerHelper);
        doReturn(new UserInfo()).when(
                mMockCarUserManagerHelper).getCurrentProcessUserInfo();
    }

    @After
    public void tearDown() {
        ShadowCarUserManagerHelper.reset();
    }

    @Test
    public void cannotModifyUsers_addAccountButtonShouldNotBeVisible() {
        doReturn(false).when(mMockCarUserManagerHelper).canCurrentProcessModifyAccounts();
        initFragment();

        Toolbar toolbar = mFragment.requireActivity().requireViewById(R.id.toolbar);
        assertThat(toolbar.getMenuItems()).hasSize(1);
        assertThat(toolbar.getMenuItems().get(0).isVisible()).isFalse();
    }

    @Test
    public void canModifyUsers_addAccountButtonShouldBeVisible() {
        doReturn(true).when(mMockCarUserManagerHelper).canCurrentProcessModifyAccounts();
        initFragment();

        Toolbar toolbar = mFragment.requireActivity().requireViewById(R.id.toolbar);
        assertThat(toolbar.getMenuItems()).hasSize(1);
        assertThat(toolbar.getMenuItems().get(0).isVisible()).isTrue();
    }

    @Test
    public void clickAddAccountButton_shouldOpenChooseAccountFragment() {
        doReturn(true).when(mMockCarUserManagerHelper).canCurrentProcessModifyAccounts();
        initFragment();

        Toolbar toolbar = mFragment.requireActivity().requireViewById(R.id.toolbar);
        toolbar.getMenuItems().get(0).performClick();

        assertThat(mFragment.getFragmentManager().findFragmentById(
                R.id.fragment_container)).isInstanceOf(ChooseAccountFragment.class);
    }

    private void initFragment() {
        mFragment = new AccountSettingsFragment();
        mFragmentController = FragmentController.of(mFragment);
        mFragmentController.setup();
    }
}
