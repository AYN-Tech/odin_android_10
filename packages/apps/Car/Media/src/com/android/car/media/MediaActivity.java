/*
 * Copyright (C) 2016 The Android Open Source Project
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
package com.android.car.media;

import android.annotation.SuppressLint;
import android.app.AlertDialog;
import android.app.Application;
import android.app.PendingIntent;
import android.car.Car;
import android.car.drivingstate.CarUxRestrictions;
import android.content.ActivityNotFoundException;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.media.audiofx.AudioEffect;
import android.os.Bundle;
import android.support.v4.media.session.PlaybackStateCompat;
import android.text.TextUtils;
import android.util.Log;
import android.util.Size;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.view.GestureDetectorCompat;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.lifecycle.AndroidViewModel;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;
import androidx.lifecycle.ViewModelProviders;

import com.android.car.apps.common.CarUxRestrictionsUtil;
import com.android.car.apps.common.util.CarPackageManagerUtils;
import com.android.car.apps.common.util.ViewUtils;
import com.android.car.media.common.MediaConstants;
import com.android.car.media.common.MediaItemMetadata;
import com.android.car.media.common.MinimizedPlaybackControlBar;
import com.android.car.media.common.browse.MediaBrowserViewModel;
import com.android.car.media.common.playback.PlaybackViewModel;
import com.android.car.media.common.source.MediaSource;
import com.android.car.media.common.source.MediaSourceViewModel;
import com.android.car.media.widgets.AppBarView;
import com.android.car.ui.toolbar.Toolbar;
import com.android.car.ui.AlertDialogBuilder;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Stack;
import java.util.stream.Collectors;

/**
 * This activity controls the UI of media. It also updates the connection status for the media app
 * by broadcast.
 */
public class MediaActivity extends FragmentActivity implements BrowseFragment.Callbacks {
    private static final String TAG = "MediaActivity";

    /** Configuration (controlled from resources) */
    private int mFadeDuration;

    /** Models */
    private PlaybackViewModel.PlaybackController mPlaybackController;

    /** Layout views */
    private View mRootView;
    private AppBarView mAppBarView;
    private PlaybackFragment mPlaybackFragment;
    private BrowseFragment mSearchFragment;
    private BrowseFragment mBrowseFragment;
    private MinimizedPlaybackControlBar mMiniPlaybackControls;
    private ViewGroup mBrowseContainer;
    private ViewGroup mPlaybackContainer;
    private ViewGroup mErrorContainer;
    private ErrorFragment mErrorFragment;
    private ViewGroup mSearchContainer;

    private Toast mToast;
    private AlertDialog mDialog;

    /** Current state */
    private Mode mMode;
    private Intent mCurrentSourcePreferences;
    private boolean mCanShowMiniPlaybackControls;
    private boolean mShouldShowSoundSettings;
    private boolean mBrowseTreeHasChildren;
    private PlaybackViewModel.PlaybackStateWrapper mCurrentPlaybackStateWrapper;
    /**
     * Media items to display as tabs. If null, it means we haven't finished loading them yet. If
     * empty, it means there are no tabs to show
     */
    @Nullable
    private List<MediaItemMetadata> mTopItems;

    private CarPackageManagerUtils mCarPackageManagerUtils;
    private CarUxRestrictionsUtil mCarUxRestrictionsUtil;
    private CarUxRestrictions mActiveCarUxRestrictions;
    @CarUxRestrictions.CarUxRestrictionsInfo
    private int mRestrictions;
    private final CarUxRestrictionsUtil.OnUxRestrictionsChangedListener mListener =
            (carUxRestrictions) -> mActiveCarUxRestrictions = carUxRestrictions;
    private Intent mAppSelectorIntent;

    private boolean mAcceptTabSelection = true;

    private AppBarView.AppBarListener mAppBarListener = new AppBarView.AppBarListener() {
        @Override
        public void onTabSelected(MediaItemMetadata item) {
            if (mAcceptTabSelection) {
                showTopItem(item);
            }
        }

        @Override
        public void onBack() {
            BrowseFragment fragment = getCurrentBrowseFragment();
            if (fragment != null) {
                boolean success = fragment.navigateBack();
                if (!success && (fragment == mSearchFragment)) {
                    changeMode(Mode.BROWSING);
                }
            }
        }

        @Override
        public void onEqualizerSelection() {
            Intent i = new Intent(AudioEffect.ACTION_DISPLAY_AUDIO_EFFECT_CONTROL_PANEL);
            // Using startActivityForResult so that the control panel app can track changes for
            // the launching package name.
            startActivityForResult(i, 0);
        }

        @Override
        public void onSettingsSelection() {
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "onSettingsSelection");
            }
            try {
                if (mCurrentSourcePreferences != null) {
                    startActivity(mCurrentSourcePreferences);
                }
            } catch (ActivityNotFoundException e) {
                if (Log.isLoggable(TAG, Log.ERROR)) {
                    Log.e(TAG, "onSettingsSelection " + e);
                }
            }
        }

        @Override
        public void onSearchSelection() {
            if (mMode == Mode.SEARCHING) {
                mSearchFragment.reopenSearch();
            } else {
                changeMode(Mode.SEARCHING);
            }
        }

        @Override
        public void onAppSwitch() {
            MediaActivity.this.startActivity(mAppSelectorIntent);
        }

        @Override
        public void onHeightChanged(int height) {
            BrowseFragment fragment = getCurrentBrowseFragment();
            if (fragment != null) {
                fragment.onAppBarHeightChanged(height);
            }
        }

        @Override
        public void onSearch(String query) {
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "onSearch: " + query);
            }
            getInnerViewModel().setSearchQuery(query);
            mSearchFragment.updateSearchQuery(query);
        }
    };

    private PlaybackFragment.PlaybackFragmentListener mPlaybackFragmentListener =
            () -> changeMode(Mode.BROWSING);

    /**
     * Possible modes of the application UI
     */
    private enum Mode {
        /** The user is browsing a media source */
        BROWSING,
        /** The user is interacting with the full screen playback UI */
        PLAYBACK,
        /** The user is searching within a media source */
        SEARCHING,
        /** There's no browse tree and playback doesn't work. */
        FATAL_ERROR
    }

    private static final Map<Integer, Integer> ERROR_CODE_MESSAGES_MAP;

    static {
        Map<Integer, Integer> map = new HashMap<>();
        map.put(PlaybackStateCompat.ERROR_CODE_APP_ERROR, R.string.error_code_app_error);
        map.put(PlaybackStateCompat.ERROR_CODE_NOT_SUPPORTED, R.string.error_code_not_supported);
        map.put(PlaybackStateCompat.ERROR_CODE_AUTHENTICATION_EXPIRED,
                R.string.error_code_authentication_expired);
        map.put(PlaybackStateCompat.ERROR_CODE_PREMIUM_ACCOUNT_REQUIRED,
                R.string.error_code_premium_account_required);
        map.put(PlaybackStateCompat.ERROR_CODE_CONCURRENT_STREAM_LIMIT,
                R.string.error_code_concurrent_stream_limit);
        map.put(PlaybackStateCompat.ERROR_CODE_PARENTAL_CONTROL_RESTRICTED,
                R.string.error_code_parental_control_restricted);
        map.put(PlaybackStateCompat.ERROR_CODE_NOT_AVAILABLE_IN_REGION,
                R.string.error_code_not_available_in_region);
        map.put(PlaybackStateCompat.ERROR_CODE_CONTENT_ALREADY_PLAYING,
                R.string.error_code_content_already_playing);
        map.put(PlaybackStateCompat.ERROR_CODE_SKIP_LIMIT_REACHED,
                R.string.error_code_skip_limit_reached);
        map.put(PlaybackStateCompat.ERROR_CODE_ACTION_ABORTED, R.string.error_code_action_aborted);
        map.put(PlaybackStateCompat.ERROR_CODE_END_OF_QUEUE, R.string.error_code_end_of_queue);
        ERROR_CODE_MESSAGES_MAP = Collections.unmodifiableMap(map);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.media_activity);

        MediaSourceViewModel mediaSourceViewModel = getMediaSourceViewModel();
        PlaybackViewModel playbackViewModel = getPlaybackViewModel();
        ViewModel localViewModel = getInnerViewModel();
        // We can't rely on savedInstanceState to determine whether the model has been initialized
        // as on a config change savedInstanceState != null and the model is initialized, but if
        // the app was killed by the system then savedInstanceState != null and the model is NOT
        // initialized...
        if (localViewModel.needsInitialization()) {
            localViewModel.init(playbackViewModel);
        }
        mMode = localViewModel.getSavedMode();

        mRootView = findViewById(R.id.media_activity_root);
        mAppBarView = findViewById(R.id.app_bar);
        mAppBarView.setListener(mAppBarListener);
        mediaSourceViewModel.getPrimaryMediaSource().observe(this,
                this::onMediaSourceChanged);

        MediaBrowserViewModel mediaBrowserViewModel = getRootBrowserViewModel();
        mediaBrowserViewModel.getBrowseState().observe(this,
                browseState -> mBrowseFragment.setRootState(browseState,
                        mediaSourceViewModel.getPrimaryMediaSource().getValue()));
        mediaBrowserViewModel.getBrowsedMediaItems().observe(this, futureData -> {
            if (futureData.isLoading()) {
                if (Log.isLoggable(TAG, Log.INFO)) {
                    Log.i(TAG, "Loading browse tree...");
                }
                mBrowseTreeHasChildren = false;
                updateTabs(null);
                return;
            }
            final boolean browseTreeHasChildren =
                    futureData.getData() != null && !futureData.getData().isEmpty();
            if (Log.isLoggable(TAG, Log.INFO)) {
                Log.i(TAG, "Browse tree loaded, status (has children or not) changed: "
                        + mBrowseTreeHasChildren + " -> " + browseTreeHasChildren);
            }
            mBrowseTreeHasChildren = browseTreeHasChildren;
            handlePlaybackState(playbackViewModel.getPlaybackStateWrapper().getValue(), false);
            updateTabs(futureData.getData() != null ? futureData.getData() : new ArrayList<>());
        });
        mediaBrowserViewModel.supportsSearch().observe(this,
                mAppBarView::setSearchSupported);

        mPlaybackFragment = new PlaybackFragment();
        mPlaybackFragment.setListener(mPlaybackFragmentListener);


        Size maxArtSize = MediaAppConfig.getMediaItemsBitmapMaxSize(this);
        mMiniPlaybackControls = findViewById(R.id.minimized_playback_controls);
        mMiniPlaybackControls.setModel(playbackViewModel, this, maxArtSize);
        mMiniPlaybackControls.setOnClickListener(view -> changeMode(Mode.PLAYBACK));

        mFadeDuration = getResources().getInteger(R.integer.new_album_art_fade_in_duration);
        mBrowseContainer = findViewById(R.id.fragment_container);
        mErrorContainer = findViewById(R.id.error_container);
        mPlaybackContainer = findViewById(R.id.playback_container);
        mSearchContainer = findViewById(R.id.search_container);
        getSupportFragmentManager().beginTransaction()
                .replace(R.id.playback_container, mPlaybackFragment)
                .commit();

        mBrowseFragment = BrowseFragment.newInstance(this, mBrowseContainer);
        mSearchFragment = BrowseFragment.newSearchInstance(this, mSearchContainer);

        playbackViewModel.getPlaybackController().observe(this,
                playbackController -> {
                    if (playbackController != null) playbackController.prepare();
                    mPlaybackController = playbackController;
                });

        playbackViewModel.getPlaybackStateWrapper().observe(this,
                state -> handlePlaybackState(state, true));

        mCarPackageManagerUtils = CarPackageManagerUtils.getInstance(this);
        mCarUxRestrictionsUtil = CarUxRestrictionsUtil.getInstance(this);
        mRestrictions = CarUxRestrictions.UX_RESTRICTIONS_NO_SETUP;
        mCarUxRestrictionsUtil.register(mListener);
        mShouldShowSoundSettings = getResources().getBoolean(R.bool.show_sound_settings);

        mPlaybackContainer.setOnTouchListener(new ClosePlaybackDetector(this));
        mAppSelectorIntent = MediaSource.getSourceSelectorIntent(this, false);
        mAppBarView.setAppLauncherSupported(mAppSelectorIntent != null);

        localViewModel.getMiniControlsVisible().observe(this, visible -> {
            int topMargin = mAppBarView.getHeight();
            int bottomMargin = visible ? mMiniPlaybackControls.getHeight() : 0;
            mBrowseFragment.onPlaybackControlsChanged(visible, topMargin, bottomMargin);
            mSearchFragment.onPlaybackControlsChanged(visible, topMargin, bottomMargin);
        } );
    }

    @Override
    protected void onDestroy() {
        mCarUxRestrictionsUtil.unregister(mListener);
        super.onDestroy();
    }

    private boolean isUxRestricted() {
        return CarUxRestrictionsUtil.isRestricted(mRestrictions, mActiveCarUxRestrictions);
    }

    private void handlePlaybackState(PlaybackViewModel.PlaybackStateWrapper state,
            boolean ignoreSameState) {
        if (Log.isLoggable(TAG, Log.DEBUG)) {
            Log.d(TAG,
                    "handlePlaybackState(); state change: " + (mCurrentPlaybackStateWrapper != null
                            ? mCurrentPlaybackStateWrapper.getState() : null) + " -> " + (
                            state != null ? state.getState() : null));

        }

        // TODO(arnaudberry) rethink interactions between customized layouts and dynamic visibility.
        mCanShowMiniPlaybackControls = (state != null) && state.shouldDisplay();
        updateMiniPlaybackControls(true);

        if (state == null) {
            mCurrentPlaybackStateWrapper = null;
            return;
        }

        String displayedMessage = getDisplayedMessage(state);
        if (Log.isLoggable(TAG, Log.DEBUG)) {
            Log.d(TAG, "Displayed error message: [" + displayedMessage + "]");
        }
        if (ignoreSameState && mCurrentPlaybackStateWrapper != null
                && mCurrentPlaybackStateWrapper.getState() == state.getState()
                && TextUtils.equals(displayedMessage,
                getDisplayedMessage(mCurrentPlaybackStateWrapper))) {
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "Ignore same playback state.");
            }
            return;
        }

        mCurrentPlaybackStateWrapper = state;

        maybeCancelToast();
        maybeCancelDialog();

        Bundle extras = state.getExtras();
        PendingIntent intent = extras == null ? null : extras.getParcelable(
                MediaConstants.ERROR_RESOLUTION_ACTION_INTENT);
        String label = extras == null ? null : extras.getString(
                MediaConstants.ERROR_RESOLUTION_ACTION_LABEL);

        boolean isFatalError = false;
        if (!TextUtils.isEmpty(displayedMessage)) {
            if (mBrowseTreeHasChildren) {
                if (intent != null && !isUxRestricted()) {
                    showDialog(intent, displayedMessage, label, getString(android.R.string.cancel));
                } else {
                    showToast(displayedMessage);
                }
            } else {
                mErrorFragment = ErrorFragment.newInstance(displayedMessage, label, intent);
                setErrorFragment(mErrorFragment);
                isFatalError = true;
            }
        }
        if (isFatalError) {
            changeMode(Mode.FATAL_ERROR);
        } else if (mMode == Mode.FATAL_ERROR) {
            changeMode(Mode.BROWSING);
        }
    }

    private String getDisplayedMessage(@Nullable PlaybackViewModel.PlaybackStateWrapper state) {
        if (state == null) {
            return null;
        }
        if (!TextUtils.isEmpty(state.getErrorMessage())) {
            return state.getErrorMessage().toString();
        }
        // ERROR_CODE_UNKNOWN_ERROR means there is no error in PlaybackState.
        if (state.getErrorCode() != PlaybackStateCompat.ERROR_CODE_UNKNOWN_ERROR) {
            Integer messageId = ERROR_CODE_MESSAGES_MAP.get(state.getErrorCode());
            return messageId != null ? getString(messageId) : getString(
                    R.string.default_error_message);
        }
        if (state.getState() == PlaybackStateCompat.STATE_ERROR) {
            return getString(R.string.default_error_message);
        }
        return null;
    }

    private void showDialog(PendingIntent intent, String message, String positiveBtnText,
            String negativeButtonText) {
        AlertDialogBuilder dialog = new AlertDialogBuilder(this);
        mDialog = dialog.setMessage(message)
                .setNegativeButton(negativeButtonText, null)
                .setPositiveButton(positiveBtnText, (dialogInterface, i) -> {
                    try {
                        intent.send();
                    } catch (PendingIntent.CanceledException e) {
                        if (Log.isLoggable(TAG, Log.ERROR)) {
                            Log.e(TAG, "Pending intent canceled");
                        }
                    }
                })
                .show();
    }

    private void maybeCancelDialog() {
        if (mDialog != null) {
            mDialog.cancel();
            mDialog = null;
        }
    }

    private void showToast(String message) {
        mToast = Toast.makeText(this, message, Toast.LENGTH_LONG);
        mToast.show();
    }

    private void maybeCancelToast() {
        if (mToast != null) {
            mToast.cancel();
            mToast = null;
        }
    }

    @Override
    public void onBackPressed() {
        super.onBackPressed();
    }

    /**
     * Sets the media source being browsed.
     *
     * @param mediaSource the new media source we are going to try to browse
     */
    private void onMediaSourceChanged(@Nullable MediaSource mediaSource) {
        ComponentName savedMediaSource = getInnerViewModel().getSavedMediaSource();
        if (Log.isLoggable(TAG, Log.INFO)) {
            Log.i(TAG, "MediaSource changed from " + savedMediaSource + " to " + mediaSource);
        }

        savedMediaSource = mediaSource != null ? mediaSource.getBrowseServiceComponentName() : null;
        getInnerViewModel().saveMediaSource(savedMediaSource);

        mBrowseFragment.resetState();
        mSearchFragment.resetState();

        mBrowseTreeHasChildren = false;
        mCurrentPlaybackStateWrapper = null;
        maybeCancelToast();
        maybeCancelDialog();
        Drawable icon = mediaSource != null
                ? new BitmapDrawable(getResources(), mediaSource.getRoundPackageIcon())
                : null;
        mAppBarView.setLogo(icon);
        mAppBarView.setSearchIcon(icon);
        if (mediaSource != null) {
            if (Log.isLoggable(TAG, Log.INFO)) {
                Log.i(TAG, "Browsing: " + mediaSource.getDisplayName());
            }
            updateTabs(null);
            Mode mediaSourceMode = getInnerViewModel().getSavedMode();
            // Changes the mode regardless of its previous value so that the views can be updated.
            changeModeInternal(mediaSourceMode, false);
            updateSourcePreferences(mediaSource.getPackageName());
            // Always go through the trampoline activity to keep all the dispatching logic there.
            startActivity(new Intent(Car.CAR_INTENT_ACTION_MEDIA_TEMPLATE));
        } else {
            updateTabs(new ArrayList<>());
            updateSourcePreferences(null);
        }
    }

    // TODO(b/136274938): display the preference screen for each media service.
    private void updateSourcePreferences(@Nullable String packageName) {
        mCurrentSourcePreferences = null;
        if (packageName != null) {
            Intent prefsIntent = new Intent(Intent.ACTION_APPLICATION_PREFERENCES);
            prefsIntent.setPackage(packageName);
            ResolveInfo info = getPackageManager().resolveActivity(prefsIntent, 0);
            if (info != null && info.activityInfo != null && info.activityInfo.exported) {
                mCurrentSourcePreferences = new Intent(prefsIntent.getAction())
                        .setClassName(info.activityInfo.packageName, info.activityInfo.name);
                mAppBarView.setSettingsDistractionOptimized(
                        mCarPackageManagerUtils.isDistractionOptimized(info.activityInfo));
            }
        }
        mAppBarView.setHasSettings(mCurrentSourcePreferences != null);
        mAppBarView.setHasEqualizer(mShouldShowSoundSettings);
    }


    /**
     * Updates the tabs displayed on the app bar, based on the top level items on the browse tree.
     * If there is at least one browsable item, we show the browse content of that node. If there
     * are only playable items, then we show those items. If there are not items at all, we show the
     * empty message. If we receive null, we show the error message.
     *
     * @param items top level items, null if the items are still being loaded, or empty list if
     *              items couldn't be loaded.
     */
    private void updateTabs(@Nullable List<MediaItemMetadata> items) {
        // Keep mTopItems null when the items are being loaded so that updateAppBarTitle() can
        // handle that case specifically.
        List<MediaItemMetadata> browsableTopLevel = (items == null) ? null :
                items.stream().filter(MediaItemMetadata::isBrowsable).collect(Collectors.toList());

        if (Objects.equals(mTopItems, browsableTopLevel)) {
            // When coming back to the app, the live data sends an update even if the list hasn't
            // changed. Updating the tabs then recreates the browse fragment, which produces jank
            // (b/131830876), and also resets the navigation to the top of the first tab...
            return;
        }
        mTopItems = browsableTopLevel;

        if (mTopItems == null || mTopItems.isEmpty()) {
            mAppBarView.setItems(null);
            mAppBarView.setActiveItem(null);
            if (items != null) {
                // Only do this when not loading the tabs or we loose the saved one.
                showTopItem(null);
            }
            updateAppBar();
            return;
        }

        ViewModel innerModel = getInnerViewModel();
        MediaItemMetadata oldTab = innerModel.getSelectedTab();
        try {
            mAcceptTabSelection = false;
            mAppBarView.setItems(mTopItems.size() == 1 ? null : mTopItems);
            updateAppBar();

            if (browsableTopLevel.contains(oldTab)) {
                mAppBarView.setActiveItem(oldTab);
            } else {
                showTopItem(browsableTopLevel.get(0));
            }
        }  finally {
            mAcceptTabSelection = true;
        }
    }

    private void showTopItem(@Nullable MediaItemMetadata item) {
        getInnerViewModel().getBrowseStack().clear();
        mBrowseFragment.navigateInto(item);
    }

    private void setErrorFragment(Fragment fragment) {
        getSupportFragmentManager().beginTransaction()
                .replace(R.id.error_container, fragment)
                .commitAllowingStateLoss();
    }

    @Nullable
    private BrowseFragment getCurrentBrowseFragment() {
        return mMode == Mode.SEARCHING ? mSearchFragment : mBrowseFragment;
    }

    private void changeMode(Mode mode) {
        if (mMode == mode) {
            if (Log.isLoggable(TAG, Log.INFO)) {
                Log.i(TAG, "Mode " + mMode + " change is ignored");
            }
            return;
        }
        changeModeInternal(mode, true);
    }

    private void changeModeInternal(Mode mode, boolean hideViewAnimated) {
        if (Log.isLoggable(TAG, Log.INFO)) {
            Log.i(TAG, "Changing mode from: " + mMode + " to: " + mode);
        }
        int fadeOutDuration = hideViewAnimated ? mFadeDuration : 0;

        Mode oldMode = mMode;
        getInnerViewModel().saveMode(mode);
        mMode = mode;

        mPlaybackFragment.closeOverflowMenu();
        updateMiniPlaybackControls(hideViewAnimated);

        switch (mMode) {
            case FATAL_ERROR:
                ViewUtils.showViewAnimated(mErrorContainer, mFadeDuration);
                ViewUtils.hideViewAnimated(mPlaybackContainer, fadeOutDuration);
                ViewUtils.hideViewAnimated(mBrowseContainer, fadeOutDuration);
                ViewUtils.hideViewAnimated(mSearchContainer, fadeOutDuration);
                mAppBarView.setState(Toolbar.State.HOME);
                mAppBarView.showSearchIfSupported(false);
                break;
            case PLAYBACK:
                mPlaybackContainer.setY(0);
                mPlaybackContainer.setAlpha(0f);
                ViewUtils.hideViewAnimated(mErrorContainer, fadeOutDuration);
                ViewUtils.showViewAnimated(mPlaybackContainer, mFadeDuration);
                ViewUtils.hideViewAnimated(mBrowseContainer, fadeOutDuration);
                ViewUtils.hideViewAnimated(mSearchContainer, fadeOutDuration);
                ViewUtils.hideViewAnimated(mAppBarView, fadeOutDuration);
                break;
            case BROWSING:
                if (oldMode == Mode.PLAYBACK) {
                    ViewUtils.hideViewAnimated(mErrorContainer, 0);
                    ViewUtils.showViewAnimated(mBrowseContainer, 0);
                    ViewUtils.hideViewAnimated(mSearchContainer, 0);
                    ViewUtils.showViewAnimated(mAppBarView, 0);
                    mPlaybackContainer.animate()
                            .translationY(mRootView.getHeight())
                            .setDuration(fadeOutDuration)
                            .setListener(ViewUtils.hideViewAfterAnimation(mPlaybackContainer))
                            .start();
                } else {
                    ViewUtils.hideViewAnimated(mErrorContainer, fadeOutDuration);
                    ViewUtils.hideViewAnimated(mPlaybackContainer, fadeOutDuration);
                    ViewUtils.showViewAnimated(mBrowseContainer, mFadeDuration);
                    ViewUtils.hideViewAnimated(mSearchContainer, fadeOutDuration);
                    ViewUtils.showViewAnimated(mAppBarView, mFadeDuration);
                }
                updateAppBar();
                break;
            case SEARCHING:
                ViewUtils.hideViewAnimated(mErrorContainer, fadeOutDuration);
                ViewUtils.hideViewAnimated(mPlaybackContainer, fadeOutDuration);
                ViewUtils.hideViewAnimated(mBrowseContainer, fadeOutDuration);
                ViewUtils.showViewAnimated(mSearchContainer, mFadeDuration);
                ViewUtils.showViewAnimated(mAppBarView, mFadeDuration);
                updateAppBar();
                break;
        }
    }
    private void updateAppBarTitle() {
        BrowseFragment fragment = getCurrentBrowseFragment();
        boolean isStacked = fragment != null && !fragment.isAtTopStack();

        final CharSequence title;
        if (isStacked) {
            // If not at top level, show the current item as title
            title = fragment.getCurrentMediaItem().getTitle();
        } else if (mTopItems == null) {
            // If still loading the tabs, force to show an empty bar.
            title = "";
        } else if (mTopItems.size() == 1) {
            // If we finished loading tabs and there is only one, use that as title.
            title = mTopItems.get(0).getTitle();
        } else {
            // Otherwise (no tabs or more than 1 tabs), show the current media source title.
            MediaSourceViewModel mediaSourceViewModel = getMediaSourceViewModel();
            MediaSource mediaSource = mediaSourceViewModel.getPrimaryMediaSource().getValue();
            title = (mediaSource != null) ? mediaSource.getDisplayName()
                    : getResources().getString(R.string.media_app_title);
        }

        mAppBarView.setTitle(title);
    }

    /**
     * Update elements of the appbar that change depending on where we are in the browse.
     */
    private void updateAppBar() {
        BrowseFragment fragment = getCurrentBrowseFragment();
        boolean isStacked = fragment != null && !fragment.isAtTopStack();
        if (Log.isLoggable(TAG, Log.DEBUG)) {
            Log.d(TAG, "App bar is in stacked state: " + isStacked);
        }
        Toolbar.State unstackedState = mMode == Mode.SEARCHING
                ? Toolbar.State.SEARCH
                : Toolbar.State.HOME;
        updateAppBarTitle();
        mAppBarView.setState(isStacked ? Toolbar.State.SUBPAGE : unstackedState);

        boolean showSearchItem = mMode != Mode.FATAL_ERROR && mMode != Mode.SEARCHING;
        mAppBarView.showSearchIfSupported(showSearchItem);
    }

    private void updateMiniPlaybackControls(boolean hideViewAnimated) {
        int fadeOutDuration = hideViewAnimated ? mFadeDuration : 0;
        // Minimized control bar should be hidden in playback view.
        final boolean shouldShowMiniPlaybackControls =
                mCanShowMiniPlaybackControls && mMode != Mode.PLAYBACK;
        if (shouldShowMiniPlaybackControls) {
            ViewUtils.showViewAnimated(mMiniPlaybackControls, mFadeDuration);
        } else {
            ViewUtils.hideViewAnimated(mMiniPlaybackControls, fadeOutDuration);
        }
        getInnerViewModel().setMiniControlsVisible(shouldShowMiniPlaybackControls);
    }

    @Override
    public void onBackStackChanged() {
        updateAppBar();
    }

    @Override
    public void onPlayableItemClicked(MediaItemMetadata item) {
        mPlaybackController.playItem(item);
        boolean switchToPlayback = getResources().getBoolean(
                R.bool.switch_to_playback_view_when_playable_item_is_clicked);
        if (switchToPlayback) {
            changeMode(Mode.PLAYBACK);
        } else if (mMode == Mode.SEARCHING) {
            changeMode(Mode.BROWSING);
        }
        setIntent(null);
    }

    @Override
    public FragmentActivity getActivity() {
        return this;
    }

    public MediaSourceViewModel getMediaSourceViewModel() {
        return MediaSourceViewModel.get(getApplication());
    }

    public PlaybackViewModel getPlaybackViewModel() {
        return PlaybackViewModel.get(getApplication());
    }

    private MediaBrowserViewModel getRootBrowserViewModel() {
        return MediaBrowserViewModel.Factory.getInstanceForBrowseRoot(getMediaSourceViewModel(),
                ViewModelProviders.of(this));
    }

    public ViewModel getInnerViewModel() {
        return ViewModelProviders.of(this).get(ViewModel.class);
    }

    public static class ViewModel extends AndroidViewModel {

        static class MediaServiceState {
            Mode mMode = Mode.BROWSING;
            Stack<MediaItemMetadata> mBrowseStack = new Stack<>();
            Stack<MediaItemMetadata> mSearchStack = new Stack<>();
            String mSearchQuery;
            boolean mQueueVisible = false;
        }

        private boolean mNeedsInitialization = true;
        private PlaybackViewModel mPlaybackViewModel;
        private ComponentName mMediaSource;
        private final Map<ComponentName, MediaServiceState> mStates = new HashMap<>();
        private MutableLiveData<Boolean> mIsMiniControlsVisible = new MutableLiveData<>();

        public ViewModel(@NonNull Application application) {
            super(application);
        }

        void init(@NonNull PlaybackViewModel playbackViewModel) {
            if (mPlaybackViewModel == playbackViewModel) {
                return;
            }
            mPlaybackViewModel = playbackViewModel;
            mNeedsInitialization = false;
        }

        boolean needsInitialization() {
            return mNeedsInitialization;
        }

        void setMiniControlsVisible(boolean visible) {
            mIsMiniControlsVisible.setValue(visible);
        }

        LiveData<Boolean> getMiniControlsVisible() {
            return mIsMiniControlsVisible;
        }

        MediaServiceState getSavedState() {
            MediaServiceState state = mStates.get(mMediaSource);
            if (state == null) {
                state = new MediaServiceState();
                mStates.put(mMediaSource, state);
            }
            return state;
        }

        void saveMode(Mode mode) {
            getSavedState().mMode = mode;
        }

        Mode getSavedMode() {
            return getSavedState().mMode;
        }

        @Nullable MediaItemMetadata getSelectedTab() {
            Stack<MediaItemMetadata> stack = getSavedState().mBrowseStack;
            return (stack != null && !stack.empty()) ? stack.firstElement() : null;
        }

        void setQueueVisible(boolean visible) {
            getSavedState().mQueueVisible = visible;
        }

        boolean getQueueVisible() {
            return getSavedState().mQueueVisible;
        }

        void saveMediaSource(ComponentName mediaSource) {
            mMediaSource = mediaSource;
        }

        ComponentName getSavedMediaSource() {
            return mMediaSource;
        }

        Stack<MediaItemMetadata> getBrowseStack() {
            return getSavedState().mBrowseStack;
        }

        Stack<MediaItemMetadata> getSearchStack() {
            return getSavedState().mSearchStack;
        }

        String getSearchQuery() {
            return getSavedState().mSearchQuery;
        }

        void setSearchQuery(String searchQuery) {
            getSavedState().mSearchQuery = searchQuery;
        }
    }

    private class ClosePlaybackDetector extends GestureDetector.SimpleOnGestureListener
            implements View.OnTouchListener {

        private final ViewConfiguration mViewConfig;
        private final GestureDetectorCompat mDetector;


        ClosePlaybackDetector(Context context) {
            mViewConfig = ViewConfiguration.get(context);
            mDetector = new GestureDetectorCompat(context, this);
        }

        @SuppressLint("ClickableViewAccessibility")
        @Override
        public boolean onTouch(View v, MotionEvent event) {
            return mDetector.onTouchEvent(event);
        }

        @Override
        public boolean onDown(MotionEvent event) {
            return (mMode == Mode.PLAYBACK);
        }

        @Override
        public boolean onFling(MotionEvent e1, MotionEvent e2, float vX, float vY) {
            float dY = e2.getY() - e1.getY();
            if (dY > mViewConfig.getScaledTouchSlop() &&
                    Math.abs(vY) > mViewConfig.getScaledMinimumFlingVelocity()) {
                float dX = e2.getX() - e1.getX();
                float tan = Math.abs(dX) / dY;
                if (tan <= 0.58) { // Accept 30 degrees on each side of the down vector.
                    changeMode(Mode.BROWSING);
                }
            }
            return true;
        }
    }
}
