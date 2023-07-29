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

package com.android.car.dialer.ui;

import android.app.SearchManager;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.provider.CallLog;
import android.telecom.Call;
import android.telephony.PhoneNumberUtils;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;
import androidx.lifecycle.ViewModelProviders;
import androidx.preference.PreferenceManager;

import com.android.car.apps.common.util.Themes;
import com.android.car.dialer.Constants;
import com.android.car.dialer.R;
import com.android.car.dialer.log.L;
import com.android.car.dialer.notification.NotificationService;
import com.android.car.dialer.telecom.UiCallManager;
import com.android.car.dialer.ui.activecall.InCallActivity;
import com.android.car.dialer.ui.activecall.InCallViewModel;
import com.android.car.dialer.ui.calllog.CallHistoryFragment;
import com.android.car.dialer.ui.common.DialerBaseFragment;
import com.android.car.dialer.ui.contact.ContactListFragment;
import com.android.car.dialer.ui.dialpad.DialpadFragment;
import com.android.car.dialer.ui.favorite.FavoriteFragment;
import com.android.car.dialer.ui.search.ContactResultsFragment;
import com.android.car.dialer.ui.settings.DialerSettingsActivity;
import com.android.car.dialer.ui.warning.NoHfpFragment;
import com.android.car.ui.toolbar.MenuItem;
import com.android.car.ui.toolbar.Toolbar;

import java.util.List;

/**
 * Main activity for the Dialer app. It contains two layers:
 * <ul>
 * <li>Overlay layer for {@link NoHfpFragment}
 * <li>Content layer for {@link FavoriteFragment} {@link CallHistoryFragment} {@link
 * ContactListFragment} and {@link DialpadFragment}
 *
 * <p>Start {@link InCallActivity} if there are ongoing calls
 *
 * <p>Based on call and connectivity status, it will choose the right page to display.
 */
public class TelecomActivity extends FragmentActivity implements
        DialerBaseFragment.DialerFragmentParent, FragmentManager.OnBackStackChangedListener,
        Toolbar.OnHeightChangedListener {
    private static final String TAG = "CD.TelecomActivity";
    private LiveData<String> mBluetoothErrorMsgLiveData;
    private LiveData<Integer> mDialerAppStateLiveData;
    private LiveData<List<Call>> mOngoingCallListLiveData;
    // View objects for this activity.
    private TelecomPageTab.Factory mTabFactory;
    private Toolbar mCarUiToolbar;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        L.d(TAG, "onCreate");
        setContentView(R.layout.telecom_activity);

        mCarUiToolbar = findViewById(R.id.car_ui_toolbar);
        mCarUiToolbar.registerToolbarHeightChangeListener(this);

        setupTabLayout();

        TelecomActivityViewModel viewModel = ViewModelProviders.of(this).get(
                TelecomActivityViewModel.class);
        mBluetoothErrorMsgLiveData = viewModel.getErrorMessage();
        mDialerAppStateLiveData = viewModel.getDialerAppState();
        mDialerAppStateLiveData.observe(this,
                dialerAppState -> updateCurrentFragment(dialerAppState));
        MutableLiveData<Integer> toolbarTitleMode = viewModel.getToolbarTitleMode();
        toolbarTitleMode.setValue(Themes.getAttrInteger(this, R.attr.toolbarTitleMode));

        InCallViewModel inCallViewModel = ViewModelProviders.of(this).get(InCallViewModel.class);
        mOngoingCallListLiveData = inCallViewModel.getOngoingCallList();
        // The mOngoingCallListLiveData needs to be active to get calculated.
        mOngoingCallListLiveData.observe(this, this::maybeStartInCallActivity);

        handleIntent();
    }

    @Override
    public void onStart() {
        getSupportFragmentManager().addOnBackStackChangedListener(this);
        onBackStackChanged();
        super.onStart();
        L.d(TAG, "onStart");
    }

    @Override
    public void onStop() {
        super.onStop();
        L.d(TAG, "onStop");
        getSupportFragmentManager().removeOnBackStackChangedListener(this);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        mCarUiToolbar.unregisterToolbarHeightChangeListener(this);
    }

    @Override
    protected void onNewIntent(Intent i) {
        super.onNewIntent(i);
        setIntent(i);
        handleIntent();
    }

    private void handleIntent() {
        Intent intent = getIntent();
        String action = intent != null ? intent.getAction() : null;
        L.d(TAG, "handleIntent, intent: %s, action: %s", intent, action);
        if (action == null || action.length() == 0) {
            return;
        }

        String number;
        switch (action) {
            case Intent.ACTION_DIAL:
                number = PhoneNumberUtils.getNumberFromIntent(intent, this);
                if (TelecomActivityViewModel.DialerAppState.BLUETOOTH_ERROR
                        != mDialerAppStateLiveData.getValue()) {
                    showDialPadFragment(number);
                }
                break;

            case Intent.ACTION_CALL:
                number = PhoneNumberUtils.getNumberFromIntent(intent, this);
                UiCallManager.get().placeCall(number);
                break;

            case Intent.ACTION_SEARCH:
                String searchQuery = intent.getStringExtra(SearchManager.QUERY);
                navigateToContactResultsFragment(searchQuery);
                break;

            case Constants.Intents.ACTION_SHOW_PAGE:
                if (TelecomActivityViewModel.DialerAppState.BLUETOOTH_ERROR
                        != mDialerAppStateLiveData.getValue()) {
                    showTabPage(intent.getStringExtra(Constants.Intents.EXTRA_SHOW_PAGE));
                    if (intent.getBooleanExtra(Constants.Intents.EXTRA_ACTION_READ_MISSED, false)) {
                        NotificationService.readAllMissedCall(this);
                    }
                }
                break;
            case Intent.ACTION_VIEW:
                if (CallLog.Calls.CONTENT_TYPE.equals(intent.getType())) {
                    if (TelecomActivityViewModel.DialerAppState.BLUETOOTH_ERROR
                            != mDialerAppStateLiveData.getValue()) {
                        showTabPage(TelecomPageTab.Page.CALL_HISTORY);
                    }
                }
                break;
            default:
                // Do nothing.
        }

        setIntent(null);

        // This is to start the incall activity when user taps on the dialer launch icon rapidly
        maybeStartInCallActivity(mOngoingCallListLiveData.getValue());
    }

    /**
     * Update the current visible fragment of this Activity based on the state of the application.
     * <ul>
     * <li> If bluetooth is not connected or there is an active call, show overlay, lock drawer,
     * hide action bar and hide the content layer.
     * <li> Otherwise, show the content layer, show action bar, hide the overlay and reset drawer
     * lock mode.
     */
    private void updateCurrentFragment(
            @TelecomActivityViewModel.DialerAppState int dialerAppState) {
        L.d(TAG, "updateCurrentFragment, dialerAppState: %d", dialerAppState);

        boolean isOverlayFragmentVisible =
                TelecomActivityViewModel.DialerAppState.DEFAULT != dialerAppState;
        findViewById(R.id.content_container)
                .setVisibility(isOverlayFragmentVisible ? View.GONE : View.VISIBLE);
        findViewById(R.id.overlay_container)
                .setVisibility(isOverlayFragmentVisible ? View.VISIBLE : View.GONE);

        switch (dialerAppState) {
            case TelecomActivityViewModel.DialerAppState.BLUETOOTH_ERROR:
                showNoHfpOverlay(mBluetoothErrorMsgLiveData.getValue());
                break;

            case TelecomActivityViewModel.DialerAppState.EMERGENCY_DIALPAD:
                setOverlayFragment(DialpadFragment.newEmergencyDialpad());
                break;

            case TelecomActivityViewModel.DialerAppState.DEFAULT:
            default:
                clearOverlayFragment();
                break;
        }
    }

    private void showNoHfpOverlay(String errorMsg) {
        Fragment overlayFragment = getCurrentOverlayFragment();
        if (overlayFragment instanceof NoHfpFragment) {
            ((NoHfpFragment) overlayFragment).setErrorMessage(errorMsg);
        } else {
            setOverlayFragment(NoHfpFragment.newInstance(errorMsg));
        }
    }

    private void setOverlayFragment(@NonNull Fragment overlayFragment) {
        L.d(TAG, "setOverlayFragment: %s", overlayFragment);

        getSupportFragmentManager()
                .beginTransaction()
                .replace(R.id.overlay_container, overlayFragment)
                .commitNow();
    }

    private void clearOverlayFragment() {
        L.d(TAG, "clearOverlayFragment");

        Fragment overlayFragment = getCurrentOverlayFragment();
        if (overlayFragment == null) {
            return;
        }

        getSupportFragmentManager()
                .beginTransaction()
                .remove(overlayFragment)
                .commitNow();
    }

    /**
     * Returns the fragment that is currently being displayed as the overlay view on top.
     */
    @Nullable
    private Fragment getCurrentOverlayFragment() {
        return getSupportFragmentManager().findFragmentById(R.id.overlay_container);
    }

    private void setupTabLayout() {
        boolean wasContentFragmentRestored = false;
        mTabFactory = new TelecomPageTab.Factory(this, getSupportFragmentManager());
        for (int i = 0; i < mTabFactory.getTabCount(); i++) {
            TelecomPageTab tab = mTabFactory.createTab(getBaseContext(), i);
            mCarUiToolbar.addTab(tab);

            if (tab.wasFragmentRestored()) {
                mCarUiToolbar.selectTab(i);
                wasContentFragmentRestored = true;
            }
        }

        // Select the starting tab and set up the fragment for it.
        if (!wasContentFragmentRestored) {
            int startTabIndex = getTabFromSharedPreference();
            TelecomPageTab startTab = (TelecomPageTab) mCarUiToolbar.getTab(startTabIndex);
            mCarUiToolbar.selectTab(startTabIndex);
            setContentFragment(startTab.getFragment(), startTab.getFragmentTag());
        }

        mCarUiToolbar.registerOnTabSelectedListener(
                tab -> {
                    TelecomPageTab telecomPageTab = (TelecomPageTab) tab;
                    Fragment fragment = telecomPageTab.getFragment();
                    setContentFragment(fragment, telecomPageTab.getFragmentTag());
                });
    }

    /**
     * Switch to {@link DialpadFragment} and set the given number as dialed number.
     */
    private void showDialPadFragment(String number) {
        int dialpadTabIndex = showTabPage(TelecomPageTab.Page.DIAL_PAD);

        if (dialpadTabIndex == -1) {
            return;
        }

        TelecomPageTab dialpadTab = (TelecomPageTab) mCarUiToolbar.getTab(dialpadTabIndex);
        Fragment fragment = dialpadTab.getFragment();
        if (fragment instanceof DialpadFragment) {
            ((DialpadFragment) fragment).setDialedNumber(number);
        } else {
            L.w(TAG, "Current tab is not a dialpad fragment!");
        }
    }

    private int showTabPage(@TelecomPageTab.Page String tabPage) {
        int tabIndex = mTabFactory.getTabIndex(tabPage);
        if (tabIndex == -1) {
            L.w(TAG, "Page %s is not a tab.", tabPage);
            return -1;
        }
        getSupportFragmentManager().executePendingTransactions();
        while (isBackNavigationAvailable()) {
            getSupportFragmentManager().popBackStackImmediate();
        }

        mCarUiToolbar.selectTab(tabIndex);
        return tabIndex;
    }

    private void setContentFragment(Fragment fragment, String fragmentTag) {
        L.d(TAG, "setContentFragment: %s", fragment);

        getSupportFragmentManager().executePendingTransactions();
        while (getSupportFragmentManager().getBackStackEntryCount() > 0) {
            getSupportFragmentManager().popBackStackImmediate();
        }

        getSupportFragmentManager()
                .beginTransaction()
                .replace(R.id.content_fragment_container, fragment, fragmentTag)
                .addToBackStack(fragmentTag)
                .commit();
    }

    @Override
    public void pushContentFragment(@NonNull Fragment topContentFragment, String fragmentTag) {
        L.d(TAG, "pushContentFragment: %s", topContentFragment);

        getSupportFragmentManager()
                .beginTransaction()
                .replace(R.id.content_fragment_container, topContentFragment, fragmentTag)
                .addToBackStack(fragmentTag)
                .commit();
    }

    @Override
    public void onBackStackChanged() {
        L.d(TAG, "onBackStackChanged");
        Fragment topFragment = getSupportFragmentManager().findFragmentById(
                R.id.content_fragment_container);
        if (topFragment instanceof DialerBaseFragment) {
            ((DialerBaseFragment) topFragment).setupToolbar(mCarUiToolbar);
        }
    }

    @Override
    public void onHeightChanged(int height) {
        Fragment topFragment = getSupportFragmentManager().findFragmentById(
                R.id.content_fragment_container);
        if (topFragment instanceof DialerBaseFragment) {
            ((DialerBaseFragment) topFragment).setToolbarHeight(height);
        }
    }

    @Override
    public boolean onNavigateUp() {
        if (isBackNavigationAvailable()) {
            onBackPressed();
            return true;
        }
        return super.onNavigateUp();
    }

    @Override
    public void onBackPressed() {
        // By default onBackPressed will pop all the fragments off the backstack and then finish
        // the activity. We want to finish the activity while there is still one fragment on the
        // backstack, because we use onBackStackChanged() to set up our fragments.
        if (isBackNavigationAvailable()) {
            super.onBackPressed();
        } else {
            finishAfterTransition();
        }
    }

    /**
     * Handles the click action on the menu items.
     */
    public void onMenuItemClicked(MenuItem item) {
        switch (item.getId()) {
            case R.id.menu_item_search:
                Intent searchIntent = new Intent(getApplicationContext(), TelecomActivity.class);
                searchIntent.setAction(Intent.ACTION_SEARCH);
                startActivity(searchIntent);
                break;
            case R.id.menu_item_setting:
                Intent settingsIntent = new Intent(getApplicationContext(),
                        DialerSettingsActivity.class);
                startActivity(settingsIntent);
                break;
        }
    }

    private void navigateToContactResultsFragment(String query) {
        Fragment topFragment = getSupportFragmentManager().findFragmentById(
                R.id.content_fragment_container);

        // Top fragment is ContactResultsFragment, update search query
        if (topFragment instanceof ContactResultsFragment) {
            ((ContactResultsFragment) topFragment).setSearchQuery(query);
            return;
        }

        ContactResultsFragment fragment = ContactResultsFragment.newInstance(query);
        pushContentFragment(fragment, ContactResultsFragment.FRAGMENT_TAG);
    }

    private void maybeStartInCallActivity(List<Call> callList) {
        if (callList == null || callList.isEmpty()) {
            return;
        }

        L.d(TAG, "Start InCallActivity");
        Intent launchIntent = new Intent(getApplicationContext(), InCallActivity.class);
        startActivity(launchIntent);
    }

    /**
     * If the back button on action bar is available to navigate up.
     */
    private boolean isBackNavigationAvailable() {
        return getSupportFragmentManager().getBackStackEntryCount() > 1;
    }

    private int getTabFromSharedPreference() {
        String key = getResources().getString(R.string.pref_start_page_key);
        String defaultValue = getResources().getStringArray(R.array.tabs_config)[0];
        SharedPreferences sharedPreferences = PreferenceManager.getDefaultSharedPreferences(this);
        return mTabFactory.getTabIndex(sharedPreferences.getString(key, defaultValue));
    }

    /**
     * Sets the background of the Activity's tool bar to a {@link Drawable}
     */
    public void setShowToolbarBackground(boolean showToolbarBackground) {
        mCarUiToolbar.setBackgroundShown(showToolbarBackground);
    }
}
