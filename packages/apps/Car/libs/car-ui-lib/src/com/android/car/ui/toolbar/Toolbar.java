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
package com.android.car.ui.toolbar;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.XmlRes;

import com.android.car.ui.R;
import com.android.car.ui.utils.CarUiUtils;
import com.android.car.ui.utils.CarUxRestrictionsUtil;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Objects;
import java.util.Set;
import java.util.concurrent.CompletableFuture;

/**
 * A toolbar for Android Automotive OS apps.
 *
 * <p>This isn't a toolbar in the android framework sense, it's merely a custom view that can be
 * added to a layout. (You can't call
 * {@link android.app.Activity#setActionBar(android.widget.Toolbar)} with it)
 *
 * <p>The toolbar supports a navigation button, title, tabs, search, and {@link MenuItem MenuItems}
 */
public class Toolbar extends FrameLayout {

    /** Callback that will be issued whenever the height of toolbar is changed. */
    public interface OnHeightChangedListener {
        /**
         * Will be called when the height of the toolbar is changed.
         *
         * @param height new height of the toolbar
         */
        void onHeightChanged(int height);
    }

    /** Back button listener */
    public interface OnBackListener {
        /**
         * Invoked when the user clicks on the back button. By default, the toolbar will call
         * the Activity's {@link android.app.Activity#onBackPressed()}. Returning true from
         * this method will absorb the back press and prevent that behavior.
         */
        boolean onBack();
    }

    /** Tab selection listener */
    public interface OnTabSelectedListener {
        /** Called when a {@link TabLayout.Tab} is selected */
        void onTabSelected(TabLayout.Tab tab);
    }

    /** Search listener */
    public interface OnSearchListener {
        /**
         * Invoked when the user edits a search query.
         *
         * <p>This is called for every letter the user types, and also empty strings if the user
         * erases everything.
         */
        void onSearch(String query);
    }

    /** Search completed listener */
    public interface OnSearchCompletedListener {
        /**
         * Invoked when the user submits a search query by clicking the keyboard's search / done
         * button.
         */
        void onSearchCompleted();
    }

    private static final String TAG = "CarUiToolbar";

    /** Enum of states the toolbar can be in. Controls what elements of the toolbar are displayed */
    public enum State {
        /**
         * In the HOME state, the logo will be displayed if there is one, and no navigation icon
         * will be displayed. The tab bar will be visible. The title will be displayed if there
         * is space. MenuItems will be displayed.
         */
        HOME,
        /**
         * In the SUBPAGE state, the logo will be replaced with a back button, the tab bar won't
         * be visible. The title and MenuItems will be displayed.
         */
        SUBPAGE,
        /**
         * In the SEARCH state, only the back button and the search bar will be visible.
         */
        SEARCH,
        /**
         * In the EDIT state, the search bar will look like a regular text box, but will be
         * functionally identical to the SEARCH state.
         */
        EDIT,
    }

    private final boolean mIsTabsInSecondRow;

    private ImageView mNavIcon;
    private ImageView mLogoInNavIconSpace;
    private ViewGroup mNavIconContainer;
    private TextView mTitle;
    private ImageView mTitleLogo;
    private ViewGroup mTitleLogoContainer;
    private TabLayout mTabLayout;
    private LinearLayout mMenuItemsContainer;
    private FrameLayout mSearchViewContainer;
    private SearchView mSearchView;

    // Cached values that we will send to views when they are inflated
    private CharSequence mSearchHint;
    private Drawable mSearchIcon;
    private String mSearchQuery;
    private final Set<OnSearchListener> mOnSearchListeners = new HashSet<>();
    private final Set<OnSearchCompletedListener> mOnSearchCompletedListeners = new HashSet<>();

    private final Set<OnBackListener> mOnBackListeners = new HashSet<>();
    private final Set<OnTabSelectedListener> mOnTabSelectedListeners = new HashSet<>();
    private final Set<OnHeightChangedListener> mOnHeightChangedListeners = new HashSet<>();

    private final MenuItem mOverflowButton;
    private boolean mHasLogo = false;
    private boolean mShowMenuItemsWhileSearching;
    private State mState = State.HOME;
    private NavButtonMode mNavButtonMode = NavButtonMode.BACK;
    @NonNull
    private List<MenuItem> mMenuItems = Collections.emptyList();
    private List<MenuItem> mOverflowItems = new ArrayList<>();
    private final List<MenuItemRenderer> mMenuItemRenderers = new ArrayList<>();
    private CompletableFuture<Void> mMenuItemViewsFuture;
    private int mMenuItemsXmlId = 0;
    private AlertDialog mOverflowDialog;
    private boolean mNavIconSpaceReserved;
    private boolean mLogoFillsNavIconSpace;
    private boolean mShowLogo;
    private boolean mEatingTouch = false;
    private boolean mEatingHover = false;
    private ProgressBar mProgressBar;
    private MenuItem.Listener mOverflowItemListener = () -> {
        createOverflowDialog();
        setState(getState());
    };

    public Toolbar(Context context) {
        this(context, null);
    }

    public Toolbar(Context context, AttributeSet attrs) {
        this(context, attrs, R.attr.CarUiToolbarStyle);
    }

    public Toolbar(Context context, AttributeSet attrs, int defStyleAttr) {
        this(context, attrs, defStyleAttr, 0);
    }

    public Toolbar(Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);

        mOverflowButton = MenuItem.builder(getContext())
                .setIcon(R.drawable.car_ui_icon_overflow_menu)
                .setTitle(R.string.car_ui_toolbar_menu_item_overflow_title)
                .setOnClickListener(v -> {
                    if (mOverflowDialog == null) {
                        if (Log.isLoggable(TAG, Log.ERROR)) {
                            Log.e(TAG, "Overflow dialog was null when trying to show it!");
                        }
                    } else {
                        mOverflowDialog.show();
                    }
                })
                .build();

        TypedArray a = context.obtainStyledAttributes(
                attrs, R.styleable.CarUiToolbar, defStyleAttr, defStyleRes);

        try {

            mIsTabsInSecondRow = context.getResources().getBoolean(
                    R.bool.car_ui_toolbar_tabs_on_second_row);
            mNavIconSpaceReserved = context.getResources().getBoolean(
                    R.bool.car_ui_toolbar_nav_icon_reserve_space);
            mLogoFillsNavIconSpace = context.getResources().getBoolean(
                    R.bool.car_ui_toolbar_logo_fills_nav_icon_space);
            mShowLogo = context.getResources().getBoolean(
                    R.bool.car_ui_toolbar_show_logo);

            LayoutInflater inflater = (LayoutInflater) context
                    .getSystemService(Context.LAYOUT_INFLATER_SERVICE);
            inflater.inflate(getToolbarLayout(), this, true);

            mTabLayout = requireViewById(R.id.car_ui_toolbar_tabs);
            mNavIcon = requireViewById(R.id.car_ui_toolbar_nav_icon);
            mLogoInNavIconSpace = requireViewById(R.id.car_ui_toolbar_logo);
            mNavIconContainer = requireViewById(R.id.car_ui_toolbar_nav_icon_container);
            mMenuItemsContainer = requireViewById(R.id.car_ui_toolbar_menu_items_container);
            mTitle = requireViewById(R.id.car_ui_toolbar_title);
            mTitleLogoContainer = requireViewById(R.id.car_ui_toolbar_title_logo_container);
            mTitleLogo = requireViewById(R.id.car_ui_toolbar_title_logo);
            mSearchViewContainer = requireViewById(R.id.car_ui_toolbar_search_view_container);
            mProgressBar = requireViewById(R.id.car_ui_toolbar_progress_bar);

            mTitle.setText(a.getString(R.styleable.CarUiToolbar_title));
            setLogo(a.getResourceId(R.styleable.CarUiToolbar_logo, 0));
            setBackgroundShown(a.getBoolean(R.styleable.CarUiToolbar_showBackground, true));
            setMenuItems(a.getResourceId(R.styleable.CarUiToolbar_menuItems, 0));
            mShowMenuItemsWhileSearching = a.getBoolean(
                    R.styleable.CarUiToolbar_showMenuItemsWhileSearching, false);
            String searchHint = a.getString(R.styleable.CarUiToolbar_searchHint);
            if (searchHint != null) {
                setSearchHint(searchHint);
            }

            switch (a.getInt(R.styleable.CarUiToolbar_state, 0)) {
                case 0:
                    setState(State.HOME);
                    break;
                case 1:
                    setState(State.SUBPAGE);
                    break;
                case 2:
                    setState(State.SEARCH);
                    break;
                default:
                    if (Log.isLoggable(TAG, Log.WARN)) {
                        Log.w(TAG, "Unknown initial state");
                    }
                    break;
            }

            switch (a.getInt(R.styleable.CarUiToolbar_navButtonMode, 0)) {
                case 0:
                    setNavButtonMode(NavButtonMode.BACK);
                    break;
                case 1:
                    setNavButtonMode(NavButtonMode.CLOSE);
                    break;
                case 2:
                    setNavButtonMode(NavButtonMode.DOWN);
                    break;
                default:
                    if (Log.isLoggable(TAG, Log.WARN)) {
                        Log.w(TAG, "Unknown navigation button style");
                    }
                    break;
            }
        } finally {
            a.recycle();
        }

        mTabLayout.addListener(new TabLayout.Listener() {
            @Override
            public void onTabSelected(TabLayout.Tab tab) {
                for (OnTabSelectedListener listener : mOnTabSelectedListeners) {
                    listener.onTabSelected(tab);
                }
            }
        });
    }

    /**
     * Override this in a subclass to allow for different toolbar layouts within a single app.
     *
     * <p>Non-system apps should not use this, as customising the layout isn't possible with RROs
     */
    protected int getToolbarLayout() {
        if (mIsTabsInSecondRow) {
            return R.layout.car_ui_toolbar_two_row;
        }

        return R.layout.car_ui_toolbar;
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        for (OnHeightChangedListener listener : mOnHeightChangedListeners) {
            listener.onHeightChanged(getHeight());
        }
    }

    /**
     * Returns {@code true} if a two row layout in enabled for the toolbar.
     */
    public boolean isTabsInSecondRow() {
        return mIsTabsInSecondRow;
    }

    private final CarUxRestrictionsUtil.OnUxRestrictionsChangedListener
            mOnUxRestrictionsChangedListener = restrictions -> {
                for (MenuItemRenderer renderer : mMenuItemRenderers) {
                    renderer.setCarUxRestrictions(restrictions);
                }
            };

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        CarUxRestrictionsUtil.getInstance(getContext())
                .register(mOnUxRestrictionsChangedListener);
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        CarUxRestrictionsUtil.getInstance(getContext())
                .unregister(mOnUxRestrictionsChangedListener);
    }

    /**
     * Sets the title of the toolbar to a string resource.
     *
     * <p>The title may not always be shown, for example with one row layout with tabs.
     */
    public void setTitle(@StringRes int title) {
        mTitle.setText(title);
        setState(getState());
    }

    /**
     * Sets the title of the toolbar to a CharSequence.
     *
     * <p>The title may not always be shown, for example with one row layout with tabs.
     */
    public void setTitle(CharSequence title) {
        mTitle.setText(title);
        setState(getState());
    }

    public CharSequence getTitle() {
        return mTitle.getText();
    }

    /**
     * Gets the {@link TabLayout} for this toolbar.
     */
    public TabLayout getTabLayout() {
        return mTabLayout;
    }

    /**
     * Adds a tab to this toolbar. You can listen for when it is selected via
     * {@link #registerOnTabSelectedListener(OnTabSelectedListener)}.
     */
    public void addTab(TabLayout.Tab tab) {
        mTabLayout.addTab(tab);
        setState(getState());
    }

    /** Removes all the tabs. */
    public void clearAllTabs() {
        mTabLayout.clearAllTabs();
        setState(getState());
    }

    /**
     * Gets a tab added to this toolbar. See
     * {@link #addTab(TabLayout.Tab)}.
     */
    public TabLayout.Tab getTab(int position) {
        return mTabLayout.get(position);
    }

    /**
     * Selects a tab added to this toolbar. See
     * {@link #addTab(TabLayout.Tab)}.
     */
    public void selectTab(int position) {
        mTabLayout.selectTab(position);
    }

    /**
     * Sets the logo to display in this toolbar. If navigation icon is being displayed, this logo
     * will be displayed next to the title.
     */
    public void setLogo(@DrawableRes int resId) {
        setLogo(resId != 0 ? getContext().getDrawable(resId) : null);
    }

    /**
     * Sets the logo to display in this toolbar. If navigation icon is being displayed, this logo
     * will be displayed next to the title.
     */
    public void setLogo(Drawable drawable) {
        if (!mShowLogo) {
            // If no logo should be shown then we act as if we never received one.
            return;
        }
        if (drawable != null) {
            mLogoInNavIconSpace.setImageDrawable(drawable);
            mTitleLogo.setImageDrawable(drawable);
            mHasLogo = true;
        } else {
            mHasLogo = false;
        }
        setState(mState);
    }

    /** Sets the hint for the search bar. */
    public void setSearchHint(@StringRes int resId) {
        setSearchHint(getContext().getString(resId));
    }

    /** Sets the hint for the search bar. */
    public void setSearchHint(CharSequence hint) {
        if (!Objects.equals(hint, mSearchHint)) {
            mSearchHint = hint;
            if (mSearchView != null) {
                mSearchView.setHint(mSearchHint);
            }
        }
    }

    /** Gets the search hint */
    public CharSequence getSearchHint() {
        return mSearchHint;
    }

    /**
     * Sets the icon to display in the search box.
     *
     * <p>The icon will be lost on configuration change, make sure to set it in onCreate() or
     * a similar place.
     */
    public void setSearchIcon(@DrawableRes int resId) {
        setSearchIcon(getContext().getDrawable(resId));
    }

    /**
     * Sets the icon to display in the search box.
     *
     * <p>The icon will be lost on configuration change, make sure to set it in onCreate() or
     * a similar place.
     */
    public void setSearchIcon(Drawable d) {
        if (!Objects.equals(d, mSearchIcon)) {
            mSearchIcon = d;
            if (mSearchView != null) {
                mSearchView.setIcon(mSearchIcon);
            }
        }
    }

    /**
     * An enum of possible styles the nav button could be in. All styles will still call
     * {@link OnBackListener#onBack()}.
     */
    public enum NavButtonMode {
        /** A back button */
        BACK,
        /** A close button */
        CLOSE,
        /** A down button, used to indicate that the page will animate down when navigating away */
        DOWN
    }

    /** Sets the {@link NavButtonMode} */
    public void setNavButtonMode(NavButtonMode style) {
        if (style != mNavButtonMode) {
            mNavButtonMode = style;
            setState(mState);
        }
    }

    /** Gets the {@link NavButtonMode} */
    public NavButtonMode getNavButtonMode() {
        return mNavButtonMode;
    }

    /**
     * setBackground is disallowed, to prevent apps from deviating from the intended style too much.
     */
    @Override
    public void setBackground(Drawable d) {
        throw new UnsupportedOperationException(
                "You can not change the background of a CarUi toolbar, use "
                        + "setBackgroundShown(boolean) or an RRO instead.");
    }

    /** Show/hide the background. When hidden, the toolbar is completely transparent. */
    public void setBackgroundShown(boolean shown) {
        if (shown) {
            super.setBackground(getContext().getDrawable(R.drawable.car_ui_toolbar_background));
        } else {
            super.setBackground(null);
        }
    }

    /** Returns true is the toolbar background is shown */
    public boolean getBackgroundShown() {
        return super.getBackground() != null;
    }

    private void setMenuItemsInternal(@Nullable List<MenuItem> items) {
        if (items == null) {
            items = Collections.emptyList();
        }

        if (items.equals(mMenuItems)) {
            return;
        }

        // Copy the list so that if the list is modified and setMenuItems is called again,
        // the equals() check will fail. Note that the MenuItems are not copied here.
        mMenuItems = new ArrayList<>(items);

        mOverflowItems.clear();
        mMenuItemRenderers.clear();
        mMenuItemsContainer.removeAllViews();

        List<CompletableFuture<View>> viewFutures = new ArrayList<>();
        for (MenuItem item : mMenuItems) {
            if (item.getDisplayBehavior() == MenuItem.DisplayBehavior.NEVER) {
                mOverflowItems.add(item);
                item.setListener(mOverflowItemListener);
            } else {
                MenuItemRenderer renderer = new MenuItemRenderer(item, mMenuItemsContainer);
                mMenuItemRenderers.add(renderer);
                viewFutures.add(renderer.createView());
            }
        }

        if (!mOverflowItems.isEmpty()) {
            MenuItemRenderer renderer = new MenuItemRenderer(mOverflowButton, mMenuItemsContainer);
            mMenuItemRenderers.add(renderer);
            viewFutures.add(renderer.createView());
            createOverflowDialog();
        }

        if (mMenuItemViewsFuture != null) {
            mMenuItemViewsFuture.cancel(false);
        }

        mMenuItemViewsFuture = CompletableFuture.allOf(
            viewFutures.toArray(new CompletableFuture[0]));
        mMenuItemViewsFuture.thenRunAsync(() -> {
            for (CompletableFuture<View> future : viewFutures) {
                mMenuItemsContainer.addView(future.join());
            }
            mMenuItemViewsFuture = null;
        }, getContext().getMainExecutor());

        setState(mState);
    }

    /**
     * Sets the {@link MenuItem Menuitems} to display.
     */
    public void setMenuItems(@Nullable List<MenuItem> items) {
        mMenuItemsXmlId = 0;
        setMenuItemsInternal(items);
    }

    /**
     * Sets the {@link MenuItem Menuitems} to display to a list defined in XML.
     *
     * <p>If this method is called twice with the same argument (and {@link #setMenuItems(List)}
     * wasn't called), nothing will happen the second time, even if the MenuItems were changed.
     *
     * <p>The XML file must have one <MenuItems> tag, with a variable number of <MenuItem>
     * child tags. See CarUiToolbarMenuItem in CarUi's attrs.xml for a list of available attributes.
     *
     * Example:
     * <pre>
     * <MenuItems>
     *     <MenuItem
     *         app:title="Foo"/>
     *     <MenuItem
     *         app:title="Bar"
     *         app:icon="@drawable/ic_tracklist"
     *         app:onClick="xmlMenuItemClicked"/>
     *     <MenuItem
     *         app:title="Bar"
     *         app:checkable="true"
     *         app:uxRestrictions="FULLY_RESTRICTED"
     *         app:onClick="xmlMenuItemClicked"/>
     * </MenuItems>
     * </pre>
     *
     * @see #setMenuItems(List)
     * @return The MenuItems that were loaded from XML.
     */
    public List<MenuItem> setMenuItems(@XmlRes int resId) {
        if (mMenuItemsXmlId != 0 && mMenuItemsXmlId == resId) {
            return mMenuItems;
        }

        mMenuItemsXmlId = resId;
        List<MenuItem> menuItems = MenuItemRenderer.readMenuItemList(getContext(), resId);
        setMenuItemsInternal(menuItems);
        return menuItems;
    }

    /** Gets the {@link MenuItem MenuItems} currently displayed */
    @NonNull
    public List<MenuItem> getMenuItems() {
        return Collections.unmodifiableList(mMenuItems);
    }

    /** Gets a {@link MenuItem} by id. */
    @Nullable
    public MenuItem findMenuItemById(int id) {
        for (MenuItem item : mMenuItems) {
            if (item.getId() == id) {
                return item;
            }
        }
        return null;
    }

    /** Gets a {@link MenuItem} by id. Will throw an exception if not found. */
    @NonNull
    public MenuItem requireMenuItemById(int id) {
        MenuItem result = findMenuItemById(id);

        if (result == null) {
            throw new IllegalArgumentException("ID does not reference a MenuItem on this Toolbar");
        }

        return result;
    }

    private int countVisibleOverflowItems() {
        int numVisibleItems = 0;
        for (MenuItem item : mOverflowItems) {
            if (item.isVisible()) {
                numVisibleItems++;
            }
        }
        return numVisibleItems;
    }

    private void createOverflowDialog() {
        // TODO(b/140564530) Use a carui alert with a (car ui)recyclerview here
        // TODO(b/140563930) Support enabled/disabled overflow items

        CharSequence[] itemTitles = new CharSequence[countVisibleOverflowItems()];
        int i = 0;
        for (MenuItem item : mOverflowItems) {
            if (item.isVisible()) {
                itemTitles[i++] = item.getTitle();
            }
        }

        mOverflowDialog = new AlertDialog.Builder(getContext())
                .setItems(itemTitles, (dialog, which) -> {
                    MenuItem item = mOverflowItems.get(which);
                    MenuItem.OnClickListener listener = item.getOnClickListener();
                    if (listener != null) {
                        listener.onClick(item);
                    }
                })
                .create();
    }

    /**
     * Set whether or not to show the {@link MenuItem MenuItems} while searching. Default false.
     * Even if this is set to true, the {@link MenuItem} created by
     * {@link MenuItem.Builder#setToSearch()} will still be hidden.
     */
    public void setShowMenuItemsWhileSearching(boolean showMenuItems) {
        mShowMenuItemsWhileSearching = showMenuItems;
        setState(mState);
    }

    /** Returns if {@link MenuItem MenuItems} are shown while searching */
    public boolean getShowMenuItemsWhileSearching() {
        return mShowMenuItemsWhileSearching;
    }

    /**
     * Sets the search query.
     */
    public void setSearchQuery(String query) {
        if (!Objects.equals(mSearchQuery, query)) {
            mSearchQuery = query;
            if (mSearchView != null) {
                mSearchView.setSearchQuery(query);
            } else {
                for (OnSearchListener listener : mOnSearchListeners) {
                    listener.onSearch(query);
                }
            }
        }
    }

    /**
     * Sets the state of the toolbar. This will show/hide the appropriate elements of the toolbar
     * for the desired state.
     */
    public void setState(State state) {
        mState = state;

        if (mSearchView == null && (state == State.SEARCH || state == State.EDIT)) {
            SearchView searchView = new SearchView(getContext());
            searchView.setHint(mSearchHint);
            searchView.setIcon(mSearchIcon);
            searchView.setSearchListeners(mOnSearchListeners);
            searchView.setSearchCompletedListeners(mOnSearchCompletedListeners);
            searchView.setSearchQuery(mSearchQuery);
            searchView.setVisibility(View.GONE);

            FrameLayout.LayoutParams layoutParams = new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.MATCH_PARENT);
            mSearchViewContainer.addView(searchView, layoutParams);

            mSearchView = searchView;
        }

        for (MenuItemRenderer renderer : mMenuItemRenderers) {
            renderer.setToolbarState(mState);
        }

        View.OnClickListener backClickListener = (v) -> {
            boolean absorbed = false;
            List<OnBackListener> listenersCopy = new ArrayList<>(mOnBackListeners);
            for (OnBackListener listener : listenersCopy) {
                absorbed = absorbed || listener.onBack();
            }

            if (!absorbed) {
                Activity activity = CarUiUtils.getActivity(getContext());
                if (activity != null) {
                    activity.onBackPressed();
                }
            }
        };

        switch (mNavButtonMode) {
            case CLOSE:
                mNavIcon.setImageResource(R.drawable.car_ui_icon_close);
                break;
            case DOWN:
                mNavIcon.setImageResource(R.drawable.car_ui_icon_down);
                break;
            default:
                mNavIcon.setImageResource(R.drawable.car_ui_icon_arrow_back);
                break;
        }

        mNavIcon.setVisibility(state != State.HOME ? VISIBLE : GONE);

        // Show the logo in the nav space if that's enabled, we have a logo,
        // and we're in the Home state.
        mLogoInNavIconSpace.setVisibility(mHasLogo
                && state == State.HOME
                && mLogoFillsNavIconSpace
                ? VISIBLE : INVISIBLE);

        // Show logo next to the title if we're in the subpage state or we're configured to not show
        // the logo in the nav icon space.
        mTitleLogoContainer.setVisibility(mHasLogo
                && (state == State.SUBPAGE || !mLogoFillsNavIconSpace)
                ? VISIBLE : GONE);

        // Show the nav icon container if we're not in the home space or the logo fills the nav icon
        // container. If car_ui_toolbar_nav_icon_reserve_space is true, hiding it will still reserve
        // its space
        mNavIconContainer.setVisibility(state != State.HOME || (mHasLogo && mLogoFillsNavIconSpace)
                ? VISIBLE : (mNavIconSpaceReserved ? INVISIBLE : GONE));
        mNavIconContainer.setOnClickListener(state != State.HOME ? backClickListener : null);
        mNavIconContainer.setClickable(state != State.HOME);

        boolean hasTabs = mTabLayout.getTabCount() > 0;
        // Show the title if we're in the subpage state, or in the home state with no tabs or tabs
        // on the second row
        mTitle.setVisibility(state == State.SUBPAGE
                || (state == State.HOME && (!hasTabs || mIsTabsInSecondRow))
                ? VISIBLE : GONE);
        mTabLayout.setVisibility(state == State.HOME && hasTabs ? VISIBLE : GONE);

        if (mSearchView != null) {
            if (state == State.SEARCH || state == State.EDIT) {
                mSearchView.setPlainText(state == State.EDIT);
                mSearchView.setVisibility(VISIBLE);
            } else {
                mSearchView.setVisibility(GONE);
            }
        }

        boolean showButtons = (state != State.SEARCH && state != State.EDIT)
                || mShowMenuItemsWhileSearching;
        mMenuItemsContainer.setVisibility(showButtons ? VISIBLE : GONE);
        mOverflowButton.setVisible(showButtons && countVisibleOverflowItems() > 0);
    }

    /** Gets the current {@link State} of the toolbar. */
    public State getState() {
        return mState;
    }

    @Override
    public boolean onTouchEvent(MotionEvent ev) {
        // Copied from androidx.appcompat.widget.Toolbar

        // Toolbars always eat touch events, but should still respect the touch event dispatch
        // contract. If the normal View implementation doesn't want the events, we'll just silently
        // eat the rest of the gesture without reporting the events to the default implementation
        // since that's what it expects.

        final int action = ev.getActionMasked();
        if (action == MotionEvent.ACTION_DOWN) {
            mEatingTouch = false;
        }

        if (!mEatingTouch) {
            final boolean handled = super.onTouchEvent(ev);
            if (action == MotionEvent.ACTION_DOWN && !handled) {
                mEatingTouch = true;
            }
        }

        if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
            mEatingTouch = false;
        }

        return true;
    }

    @Override
    public boolean onHoverEvent(MotionEvent ev) {
        // Copied from androidx.appcompat.widget.Toolbar

        // Same deal as onTouchEvent() above. Eat all hover events, but still
        // respect the touch event dispatch contract.

        final int action = ev.getActionMasked();
        if (action == MotionEvent.ACTION_HOVER_ENTER) {
            mEatingHover = false;
        }

        if (!mEatingHover) {
            final boolean handled = super.onHoverEvent(ev);
            if (action == MotionEvent.ACTION_HOVER_ENTER && !handled) {
                mEatingHover = true;
            }
        }

        if (action == MotionEvent.ACTION_HOVER_EXIT || action == MotionEvent.ACTION_CANCEL) {
            mEatingHover = false;
        }

        return true;
    }

    /**
     * Registers a new {@link OnHeightChangedListener} to the list of listeners. Register a
     * {@link com.android.car.ui.recyclerview.CarUiRecyclerView} only if there is a toolbar at
     * the top and a {@link com.android.car.ui.recyclerview.CarUiRecyclerView} in the view and
     * nothing else. {@link com.android.car.ui.recyclerview.CarUiRecyclerView} will
     * automatically adjust its height according to the height of the Toolbar.
     */
    public void registerToolbarHeightChangeListener(
            OnHeightChangedListener listener) {
        mOnHeightChangedListeners.add(listener);
    }

    /** Unregisters an existing {@link OnHeightChangedListener} from the list of listeners. */
    public boolean unregisterToolbarHeightChangeListener(
            OnHeightChangedListener listener) {
        return mOnHeightChangedListeners.remove(listener);
    }

    /** Registers a new {@link OnTabSelectedListener} to the list of listeners. */
    public void registerOnTabSelectedListener(OnTabSelectedListener listener) {
        mOnTabSelectedListeners.add(listener);
    }

    /** Unregisters an existing {@link OnTabSelectedListener} from the list of listeners. */
    public boolean unregisterOnTabSelectedListener(OnTabSelectedListener listener) {
        return mOnTabSelectedListeners.remove(listener);
    }

    /** Registers a new {@link OnSearchListener} to the list of listeners. */
    public void registerOnSearchListener(OnSearchListener listener) {
        mOnSearchListeners.add(listener);
    }

    /** Unregisters an existing {@link OnSearchListener} from the list of listeners. */
    public boolean unregisterOnSearchListener(OnSearchListener listener) {
        return mOnSearchListeners.remove(listener);
    }

    /** Registers a new {@link OnSearchCompletedListener} to the list of listeners. */
    public void registerOnSearchCompletedListener(OnSearchCompletedListener listener) {
        mOnSearchCompletedListeners.add(listener);
    }

    /** Unregisters an existing {@link OnSearchCompletedListener} from the list of listeners. */
    public boolean unregisterOnSearchCompletedListener(OnSearchCompletedListener listener) {
        return mOnSearchCompletedListeners.remove(listener);
    }

    /** Registers a new {@link OnBackListener} to the list of listeners. */
    public void registerOnBackListener(OnBackListener listener) {
        mOnBackListeners.add(listener);
    }

    /** Unregisters an existing {@link OnTabSelectedListener} from the list of listeners. */
    public boolean unregisterOnBackListener(OnBackListener listener) {
        return mOnBackListeners.remove(listener);
    }

    /** Shows the progress bar */
    public void showProgressBar() {
        mProgressBar.setVisibility(View.VISIBLE);
    }

    /** Hides the progress bar */
    public void hideProgressBar() {
        mProgressBar.setVisibility(View.GONE);
    }

    /** Returns the progress bar */
    public ProgressBar getProgressBar() {
        return mProgressBar;
    }
}
