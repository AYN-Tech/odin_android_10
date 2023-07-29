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

package com.android.car.settings.quicksettings;

import android.app.Activity;
import android.car.drivingstate.CarUxRestrictions;
import android.car.userlib.CarUserManagerHelper;
import android.content.Context;
import android.content.pm.UserInfo;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Bundle;
import android.os.SystemProperties;
import android.os.UserManager;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.LayoutRes;
import androidx.annotation.NonNull;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;
import androidx.recyclerview.widget.RecyclerView;

import com.android.car.settings.R;
import com.android.car.settings.common.BaseFragment;
import com.android.car.settings.home.HomepageFragment;
import com.android.car.settings.users.UserIconProvider;
import com.android.car.settings.users.UserSwitcherFragment;
import com.android.car.ui.toolbar.MenuItem;
import com.android.car.ui.toolbar.Toolbar;

import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/**
 * Shows a page to access frequently used settings.
 */
public class QuickSettingFragment extends BaseFragment {
    // Time to delay refreshing the build info, if the clock is not correct.
    private static final long BUILD_INFO_REFRESH_TIME_MS = TimeUnit.SECONDS.toMillis(5);
    /**
     * Indicates whether all Preferences are configured to ignore UX Restrictions Event.
     */
    private boolean mAllIgnoresUxRestrictions;

    /**
     * Set of the keys of Preferences that ignore UX Restrictions. When mAlwaysIgnoreUxRestrictions
     * is configured to be false, then only the Preferences whose keys are contained in this Set
     * ignore UX Restrictions.
     */
    private Set<String> mPreferencesIgnoringUxRestrictions;

    private CarUserManagerHelper mCarUserManagerHelper;
    private UserIconProvider mUserIconProvider;
    private QuickSettingGridAdapter mGridAdapter;
    private RecyclerView mListView;
    private MenuItem mFullSettingsBtn;
    private MenuItem mUserSwitcherBtn;
    private float mOpacityDisabled;
    private float mOpacityEnabled;
    private TextView mBuildInfo;

    @Override
    @LayoutRes
    protected int getLayoutId() {
        return R.layout.quick_settings;
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        Activity activity = requireActivity();

        mOpacityDisabled = activity.getResources().getFloat(R.dimen.opacity_disabled);
        mOpacityEnabled = activity.getResources().getFloat(R.dimen.opacity_enabled);
        mCarUserManagerHelper = new CarUserManagerHelper(activity);
        mUserIconProvider = new UserIconProvider(mCarUserManagerHelper);
        mListView = activity.findViewById(R.id.list);
        mGridAdapter = new QuickSettingGridAdapter(activity);
        mListView.setLayoutManager(mGridAdapter.getGridLayoutManager());

        setupUserButton(activity);

        mGridAdapter
                .addTile(new WifiTile(activity, mGridAdapter, getFragmentController()))
                .addTile(new BluetoothTile(activity, mGridAdapter, getFragmentController()))
                .addTile(new DayNightTile(activity, mGridAdapter, getFragmentController()))
                .addTile(new CelluarTile(activity, mGridAdapter))
                .addSeekbarTile(new BrightnessTile(activity));
        mListView.setAdapter(mGridAdapter);

        mPreferencesIgnoringUxRestrictions = new HashSet<String>(Arrays.asList(
                getContext().getResources().getStringArray(R.array.config_ignore_ux_restrictions)));
        mAllIgnoresUxRestrictions =
                getContext().getResources().getBoolean(R.bool.config_always_ignore_ux_restrictions);
    }


    @Override
    protected Toolbar.NavButtonMode getToolbarNavButtonStyle() {
        return Toolbar.NavButtonMode.CLOSE;
    }

    @Override
    protected Toolbar.State getToolbarState() {
        Activity activity = requireActivity();
        FragmentManager fragmentManager = ((FragmentActivity) activity).getSupportFragmentManager();
        if (fragmentManager.getBackStackEntryCount() == 1
                && fragmentManager.findFragmentByTag("0") != null
                && fragmentManager.findFragmentByTag("0").getClass().getName().equals(
                getString(R.string.config_settings_hierarchy_root_fragment))
                && !getContext().getResources()
                .getBoolean(R.bool.config_show_settings_root_exit_icon)) {
            return Toolbar.State.HOME;
        } else {
            return Toolbar.State.SUBPAGE;
        }
    }

    @Override
    public List<MenuItem> getToolbarMenuItems() {
        return Arrays.asList(mUserSwitcherBtn, mFullSettingsBtn);
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mUserSwitcherBtn = new MenuItem.Builder(getContext())
                .setTitle(getString(R.string.user_switch))
                .setOnClickListener(i ->
                        getFragmentController().launchFragment(new UserSwitcherFragment()))
                .setIcon(R.drawable.ic_user)
                .setShowIconAndTitle(true)
                .setVisible(showUserSwitcher())
                .setTinted(false)
                .build();
        mFullSettingsBtn = new MenuItem.Builder(getContext())
                .setTitle(getString(R.string.more_settings_label))
                .setOnClickListener(i ->
                        getFragmentController().launchFragment(new HomepageFragment()))
                .setIcon(R.drawable.ic_settings_gear)
                .setShowIconAndTitle(true)
                .setUxRestrictions(CarUxRestrictions.UX_RESTRICTIONS_NO_SETUP)
                .build();
    }

    @Override
    public void onViewCreated(@NonNull View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        mBuildInfo = view.requireViewById(R.id.build_info);
    }

    @Override
    public void onStart() {
        super.onStart();
        mGridAdapter.start();

        // In non-user builds (that is, user-debug, eng, etc), display some version information.
        if (!Build.IS_USER) {
            refreshBuildInfo();
        }
    }

    private void refreshBuildInfo() {
        if (!isAdded()) {
            // This can happen if the delayed post happens before we're stopped. Just give up
            // trying to get the right clock.
            return;
        }

        long buildTimeDiffDays =
                TimeUnit.MILLISECONDS.toDays(System.currentTimeMillis() - Build.TIME);
        if (buildTimeDiffDays < 0) {
            // If it's in the past, that likely means the current time is wrong (or the build time
            // could be wrong, but that's less likely). Reschedule this to run in a few seconds to
            // see whether the clock's been fixed.
            mBuildInfo.postDelayed(this::refreshBuildInfo, BUILD_INFO_REFRESH_TIME_MS);
        }

        String str = String.format(getResources().getString(R.string.build_info_fmt),
                Build.FINGERPRINT, SystemProperties.get("ro.build.date", "<unknown>"),
                buildTimeDiffDays < 0 ? "--" : Long.toString(buildTimeDiffDays));

        mBuildInfo.setVisibility(View.VISIBLE);
        mBuildInfo.setText(str);
    }

    @Override
    public void onStop() {
        super.onStop();
        mGridAdapter.stop();
    }

    private void setupUserButton(Context context) {
        UserInfo currentUserInfo = mCarUserManagerHelper.getCurrentForegroundUserInfo();
        Drawable userIcon = mUserIconProvider.getUserIcon(currentUserInfo, context);
        mUserSwitcherBtn.setIcon(userIcon);
        mUserSwitcherBtn.setTitle(currentUserInfo.name);
    }

    private boolean showUserSwitcher() {
        return !UserManager.isDeviceInDemoMode(getContext())
                && UserManager.supportsMultipleUsers()
                && !UserManager.get(getContext()).hasUserRestriction(
                UserManager.DISALLOW_USER_SWITCH);
    }
}
