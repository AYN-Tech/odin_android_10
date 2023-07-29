/*
 * Copyright 2018 The Android Open Source Project
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

import static com.android.car.arch.common.LiveDataFunctions.ifThenElse;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.content.Context;
import android.content.res.Resources;
import android.os.Handler;
import android.text.TextUtils;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.InputMethodManager;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.fragment.app.FragmentActivity;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;
import androidx.lifecycle.ViewModelProviders;
import androidx.recyclerview.widget.RecyclerView;

import com.android.car.apps.common.util.ViewUtils;
import com.android.car.arch.common.FutureData;
import com.android.car.media.browse.BrowseAdapter;
import com.android.car.media.common.GridSpacingItemDecoration;
import com.android.car.media.common.MediaItemMetadata;
import com.android.car.media.common.browse.MediaBrowserViewModel;
import com.android.car.media.common.source.MediaSource;
import com.android.car.media.common.source.MediaSourceViewModel;

import java.util.List;
import java.util.Stack;
import java.util.stream.Collectors;

/**
 * A view "controller" that implements the content forward browsing experience.
 *
 * This can be used to display either search or browse results at the root level. Deeper levels will
 * be handled the same way between search and browse, using a back stack to return to the root.
 */
public class BrowseFragment {
    private static final String TAG = "BrowseFragment";

    private static final String REGULAR_BROWSER_VIEW_MODEL_KEY
            = "com.android.car.media.regular_browser_view_model";
    private static final String SEARCH_BROWSER_VIEW_MODEL_KEY
            = "com.android.car.media.search_browser_view_model";

    private final Callbacks mCallbacks;
    private final View mFragmentContent;

    private RecyclerView mBrowseList;
    private ViewGroup mBrowseState;
    private ImageView mErrorIcon;
    private TextView mMessage;
    private BrowseAdapter mBrowseAdapter;
    private String mSearchQuery;
    private int mFadeDuration;
    private int mLoadingIndicatorDelay;
    private boolean mIsSearchFragment;
    // todo(b/130760002): Create new browse fragments at deeper levels.
    private MutableLiveData<Boolean> mShowSearchResults = new MutableLiveData<>();
    private Handler mHandler = new Handler();
    private Stack<MediaItemMetadata> mBrowseStack = new Stack<>();
    private MediaBrowserViewModel.WithMutableBrowseId mMediaBrowserViewModel;
    private BrowseAdapter.Observer mBrowseAdapterObserver = new BrowseAdapter.Observer() {

        @Override
        protected void onPlayableItemClicked(MediaItemMetadata item) {
            hideKeyboard();
            getParent().onPlayableItemClicked(item);
        }

        @Override
        protected void onBrowsableItemClicked(MediaItemMetadata item) {
            hideKeyboard();
            navigateInto(item);
        }
    };

    /**
     * Fragment callbacks (implemented by the hosting Activity)
     */
    public interface Callbacks {
        /**
         * Method invoked when the back stack changes (for example, when the user moves up or down
         * the media tree)
         */
        void onBackStackChanged();

        /**
         * Method invoked when the user clicks on a playable item
         *
         * @param item item to be played.
         */
        void onPlayableItemClicked(MediaItemMetadata item);

        FragmentActivity getActivity();
    }

    /**
     * Moves the user one level up in the browse tree. Returns whether that was possible.
     */
    boolean navigateBack() {
        boolean result = false;
        if (!isAtTopStack()) {
            mBrowseStack.pop();
            mMediaBrowserViewModel.search(mSearchQuery);
            mMediaBrowserViewModel.setCurrentBrowseId(getCurrentMediaItemId());
            getParent().onBackStackChanged();
            result = true;
        }
        if (isAtTopStack()) {
            mShowSearchResults.setValue(mIsSearchFragment);
        }
        return result;
    }

    void reopenSearch() {
        if (mIsSearchFragment) {
            mBrowseStack.clear();
            getParent().onBackStackChanged();
            mShowSearchResults.setValue(true);
        } else {
            Log.e(TAG, "reopenSearch called on browse fragment");
        }
    }

    @NonNull
    private Callbacks getParent() {
        return mCallbacks;
    }

    private FragmentActivity getActivity() {
        return mCallbacks.getActivity();
    }

    /**
     * @return whether the user is at the top of the browsing stack.
     */
    boolean isAtTopStack() {
        if (mIsSearchFragment) {
            return mBrowseStack.isEmpty();
        } else {
            // The mBrowseStack stack includes the tab...
            return mBrowseStack.size() <= 1;
        }
    }

    /**
     * Creates a new instance of this fragment. The root browse id will be the one provided to this
     * method.
     * @return a fully initialized {@link BrowseFragment}
     */
    public static BrowseFragment newInstance(Callbacks callbacks, ViewGroup container) {
        boolean isSearchFragment = false;
        return new BrowseFragment(callbacks, container, isSearchFragment);
    }

    /**
     * Creates a new instance of this fragment, meant to display search results. The root browse
     * screen will be the search results for the provided query.
     *
     * @return a fully initialized {@link BrowseFragment}
     */
    static BrowseFragment newSearchInstance(Callbacks callbacks, ViewGroup container) {
        boolean isSearchFragment = true;
        return new BrowseFragment(callbacks, container, isSearchFragment);
    }

    void updateSearchQuery(@Nullable String query) {
        mSearchQuery = query;
        mMediaBrowserViewModel.search(query);
    }

    /**
     * Clears search state from this fragment, removes any UI elements from previous results.
     */
    void resetState() {
        MediaActivity.ViewModel viewModel = ViewModelProviders.of(mCallbacks.getActivity()).get(
                MediaActivity.ViewModel.class);

        if (mIsSearchFragment) {
            updateSearchQuery(viewModel.getSearchQuery());
            mShowSearchResults.setValue(false);
            mBrowseStack = viewModel.getSearchStack();
            mShowSearchResults.setValue(isAtTopStack());
        } else {
            mBrowseStack = viewModel.getBrowseStack();
            mShowSearchResults.setValue(false);
        }

        mBrowseAdapter.submitItems(null, null);
        stopLoadingIndicator();
        ViewUtils.hideViewAnimated(mErrorIcon, mFadeDuration);
        ViewUtils.hideViewAnimated(mMessage, mFadeDuration);

        mMediaBrowserViewModel.setCurrentBrowseId(getCurrentMediaItemId());
    }

    private BrowseFragment(Callbacks callbacks, ViewGroup container, boolean isSearchFragment) {
        mCallbacks = callbacks;

        FragmentActivity activity = callbacks.getActivity();

        mIsSearchFragment = isSearchFragment;
        mMediaBrowserViewModel = MediaBrowserViewModel.Factory.getInstanceWithMediaBrowser(
                mIsSearchFragment ? SEARCH_BROWSER_VIEW_MODEL_KEY : REGULAR_BROWSER_VIEW_MODEL_KEY,
                ViewModelProviders.of(activity),
                MediaSourceViewModel.get(activity.getApplication()).getConnectedMediaBrowser());

        int viewId = mIsSearchFragment ? R.layout.fragment_search : R.layout.fragment_browse;
        mFragmentContent =
                LayoutInflater.from(container.getContext()).inflate(viewId, container, false);
        mLoadingIndicatorDelay = mFragmentContent.getContext().getResources()
                .getInteger(R.integer.progress_indicator_delay);
        mBrowseList = mFragmentContent.findViewById(R.id.browse_list);
        mBrowseState = mFragmentContent.findViewById(R.id.browse_state);
        mErrorIcon = mFragmentContent.findViewById(R.id.error_icon);
        mMessage = mFragmentContent.findViewById(R.id.error_message);
        mFadeDuration = mFragmentContent.getContext().getResources().getInteger(
                R.integer.new_album_art_fade_in_duration);

        mBrowseList.addItemDecoration(new GridSpacingItemDecoration(
                activity.getResources().getDimensionPixelSize(R.dimen.grid_item_spacing)));

        mBrowseAdapter = new BrowseAdapter(mBrowseList.getContext());
        mBrowseList.setAdapter(mBrowseAdapter);
        mBrowseAdapter.registerObserver(mBrowseAdapterObserver);

        mMediaBrowserViewModel.rootBrowsableHint().observe(activity, hint ->
                mBrowseAdapter.setRootBrowsableViewType(hint));
        mMediaBrowserViewModel.rootPlayableHint().observe(activity, hint ->
                mBrowseAdapter.setRootPlayableViewType(hint));
        LiveData<FutureData<List<MediaItemMetadata>>> mediaItems = ifThenElse(mShowSearchResults,
                mMediaBrowserViewModel.getSearchedMediaItems(),
                mMediaBrowserViewModel.getBrowsedMediaItems());

        // TODO(b/145688665) merge with #update
        mediaItems.observe(activity, futureData ->
        {
            // Prevent showing loading spinner or any error messages if search is uninitialized
            if (mIsSearchFragment && TextUtils.isEmpty(mSearchQuery)) {
                return;
            }
            boolean isLoading = futureData.isLoading();
            if (isLoading) {
                // TODO(b/139759881) build a jank-free animation of the transition.
                mBrowseList.setAlpha(0f);
                startLoadingIndicator();
                mBrowseAdapter.submitItems(null, null);
                return;
            }
            stopLoadingIndicator();
            List<MediaItemMetadata> items = futureData.getData();
            if (items != null) {
                items = items.stream().filter(item ->
                        (item.isPlayable() || item.isBrowsable())).collect(Collectors.toList());
            }
            mBrowseAdapter.submitItems(getCurrentMediaItem(), items);
            if (items == null) {
                mMessage.setText(R.string.unknown_error);
                ViewUtils.hideViewAnimated(mBrowseList, mFadeDuration);
                ViewUtils.showViewAnimated(mMessage, mFadeDuration);
                ViewUtils.showViewAnimated(mErrorIcon, mFadeDuration);
            } else if (items.isEmpty()) {
                mMessage.setText(R.string.nothing_to_play);
                ViewUtils.hideViewAnimated(mBrowseList, mFadeDuration);
                ViewUtils.hideViewAnimated(mErrorIcon, mFadeDuration);
                ViewUtils.showViewAnimated(mMessage, mFadeDuration);
            } else {
                ViewUtils.showViewAnimated(mBrowseList, mFadeDuration);
                ViewUtils.hideViewAnimated(mErrorIcon, mFadeDuration);
                ViewUtils.hideViewAnimated(mMessage, mFadeDuration);
            }
        });

        container.addView(mFragmentContent);
    }

    private Runnable mLoadingIndicatorRunnable = new Runnable() {
        @Override
        public void run() {
            mMessage.setText(R.string.browser_loading);
            ViewUtils.showViewAnimated(mMessage, mFadeDuration);
        }
    };

    private void startLoadingIndicator() {
        // Display the indicator after a certain time, to avoid flashing the indicator constantly,
        // even when performance is acceptable.
        mHandler.postDelayed(mLoadingIndicatorRunnable, mLoadingIndicatorDelay);
    }

    private void stopLoadingIndicator() {
        mHandler.removeCallbacks(mLoadingIndicatorRunnable);
        ViewUtils.hideViewAnimated(mMessage, mFadeDuration);
    }

    void navigateInto(@Nullable MediaItemMetadata item) {
        if (item != null) {
            mBrowseStack.push(item);
            mMediaBrowserViewModel.setCurrentBrowseId(item.getId());
        } else {
            mMediaBrowserViewModel.setCurrentBrowseId(null);
        }

        mShowSearchResults.setValue(false);
        getParent().onBackStackChanged();
    }

    /**
     * @return the current item being displayed
     */
    @Nullable
    MediaItemMetadata getCurrentMediaItem() {
        return mBrowseStack.isEmpty() ? null : mBrowseStack.lastElement();
    }

    @Nullable
    private String getCurrentMediaItemId() {
        MediaItemMetadata currentItem = getCurrentMediaItem();
        return currentItem != null ? currentItem.getId() : null;
    }

    public void onAppBarHeightChanged(int height) {
        if (mBrowseList == null) {
            return;
        }

        mBrowseList.setPadding(mBrowseList.getPaddingLeft(), height,
                mBrowseList.getPaddingRight(), mBrowseList.getPaddingBottom());
    }

    void onPlaybackControlsChanged(boolean visible, int browseStateTopMargin,
            int browseStateBottomMargin) {
        ViewGroup.MarginLayoutParams params =
                (ViewGroup.MarginLayoutParams) mBrowseState.getLayoutParams();
        params.topMargin = browseStateTopMargin;
        params.bottomMargin = browseStateBottomMargin;

        if (mBrowseList == null) {
            return;
        }

        Resources res = getActivity().getResources();
        int bottomPadding = visible
                ? res.getDimensionPixelOffset(R.dimen.browse_fragment_bottom_padding)
                : 0;
        mBrowseList.setPadding(mBrowseList.getPaddingLeft(), mBrowseList.getPaddingTop(),
                mBrowseList.getPaddingRight(), bottomPadding);
    }

    private void hideKeyboard() {
        InputMethodManager in =
                (InputMethodManager) getActivity().getSystemService(Context.INPUT_METHOD_SERVICE);
        in.hideSoftInputFromWindow(mFragmentContent.getWindowToken(), 0);
    }

    /**
     * Updates the state of this fragment.
     */
    void setRootState(@NonNull MediaBrowserViewModel.BrowseState state,
            @Nullable MediaSource mediaSource) {
        mHandler.removeCallbacks(mLoadingIndicatorRunnable);
        update(state, mediaSource);
    }

    private void update(@NonNull MediaBrowserViewModel.BrowseState state,
            @Nullable MediaSource mediaSource) {
        switch (state) {
            case LOADING:
                // Display the indicator after a certain time, to avoid flashing the indicator
                // constantly, even when performance is acceptable.
                startLoadingIndicator();
                ViewUtils.hideViewAnimated(mErrorIcon, 0);
                ViewUtils.hideViewAnimated(mMessage, 0);
                break;
            case ERROR:
                ViewUtils.showViewAnimated(mErrorIcon, 0);
                ViewUtils.showViewAnimated(mMessage, 0);
                mMessage.setText(getActivity().getString(
                        R.string.cannot_connect_to_app,
                        mediaSource != null
                                ? mediaSource.getDisplayName()
                                : getActivity().getString(
                                        R.string.unknown_media_provider_name)));
                break;
            case EMPTY:
                ViewUtils.hideViewAnimated(mErrorIcon, 0);
                ViewUtils.showViewAnimated(mMessage, 0);
                mMessage.setText(getActivity().getString(R.string.nothing_to_play));
                break;
            case LOADED:
                Log.d(TAG, "Updated with LOADED state, ignoring.");
                // Do nothing.
                break;
            default:
                // Fail fast on any other state.
                throw new IllegalStateException("Invalid state for this fragment: " + state);
        }
    }
}
