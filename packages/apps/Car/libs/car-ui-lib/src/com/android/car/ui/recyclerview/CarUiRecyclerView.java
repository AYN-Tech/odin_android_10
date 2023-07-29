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
package com.android.car.ui.recyclerview;

import static java.lang.annotation.RetentionPolicy.SOURCE;

import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.content.res.TypedArray;
import android.os.Parcel;
import android.os.Parcelable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Log;
import android.util.SparseArray;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.ViewTreeObserver.OnGlobalLayoutListener;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.android.car.ui.R;
import com.android.car.ui.recyclerview.decorations.grid.GridDividerItemDecoration;
import com.android.car.ui.recyclerview.decorations.grid.GridOffsetItemDecoration;
import com.android.car.ui.recyclerview.decorations.linear.LinearDividerItemDecoration;
import com.android.car.ui.recyclerview.decorations.linear.LinearOffsetItemDecoration;
import com.android.car.ui.recyclerview.decorations.linear.LinearOffsetItemDecoration.OffsetPosition;
import com.android.car.ui.toolbar.Toolbar;
import com.android.car.ui.utils.CarUxRestrictionsUtil;

import java.lang.annotation.Retention;

/**
 * View that extends a {@link RecyclerView} and creates a nested {@code RecyclerView} which could
 * potentially include a scrollbar that has page up and down arrows. Interaction with this view is
 * similar to a {@code RecyclerView} as it takes the same adapter and the layout manager.
 */
public final class CarUiRecyclerView extends RecyclerView implements
        Toolbar.OnHeightChangedListener {

    private static final boolean DEBUG = false;
    private static final String TAG = "CarUiRecyclerView";

    private final CarUxRestrictionsUtil mCarUxRestrictionsUtil;
    private final CarUxRestrictionsUtil.OnUxRestrictionsChangedListener mListener;

    private boolean mScrollBarEnabled;
    private int mScrollBarContainerWidth;
    @ScrollBarPosition
    private int mScrollBarPosition;
    private boolean mScrollBarAboveRecyclerView;
    private String mScrollBarClass;
    private boolean mFullyInitialized;
    private float mScrollBarPaddingStart;
    private float mScrollBarPaddingEnd;
    private Context mContext;

    @Gutter
    private int mGutter;
    private int mGutterSize;
    @VisibleForTesting
    RecyclerView mNestedRecyclerView;
    private boolean mIsNestedRecyclerViewInitialized;
    private Adapter<?> mAdapter;
    private ScrollBar mScrollBar;
    private int mInitialTopPadding;

    private GridOffsetItemDecoration mOffsetItemDecoration;
    private GridDividerItemDecoration mDividerItemDecoration;
    @CarUiRecyclerViewLayout
    int mCarUiRecyclerViewLayout;
    private int mNumOfColumns;

    /**
     * The possible values for @{link #setGutter}. The default value is actually {@link
     * CarUiRecyclerView.Gutter#BOTH}.
     */
    @IntDef({
            Gutter.NONE,
            Gutter.START,
            Gutter.END,
            Gutter.BOTH,
    })
    @Retention(SOURCE)
    public @interface Gutter {
        /**
         * No gutter on either side of the list items. The items will span the full width of the
         * RecyclerView
         */
        int NONE = 0;

        /** Include a gutter only on the start side (that is, the same side as the scroll bar). */
        int START = 1;

        /** Include a gutter only on the end side (that is, the opposite side of the scroll bar). */
        int END = 2;

        /** Include a gutter on both sides of the list items. This is the default behaviour. */
        int BOTH = 3;
    }

    /**
     * The possible values for setScrollbarPosition. The default value is actually {@link
     * CarUiRecyclerView.ScrollBarPosition#START}.
     */
    @IntDef({
            ScrollBarPosition.START,
            ScrollBarPosition.END,
    })
    @Retention(SOURCE)
    public @interface ScrollBarPosition {
        /** Position the scrollbar to the left of the screen. This is default. */
        int START = 0;

        /** Position scrollbar to the right of the screen. */
        int END = 1;
    }

    /**
     * The possible values for setScrollbarPosition. The default value is actually {@link
     * CarUiRecyclerViewLayout#LINEAR}.
     */
    @IntDef({
            CarUiRecyclerViewLayout.LINEAR,
            CarUiRecyclerViewLayout.GRID,
    })
    @Retention(SOURCE)
    public @interface CarUiRecyclerViewLayout {
        /** Position the scrollbar to the left of the screen. This is default. */
        int LINEAR = 0;

        /** Position scrollbar to the right of the screen. */
        int GRID = 2;
    }

    /**
     * Interface for a {@link RecyclerView.Adapter} to cap the number of items.
     *
     * <p>NOTE: it is still up to the adapter to use maxItems in {@link
     * RecyclerView.Adapter#getItemCount()}.
     *
     * <p>the recommended way would be with:
     *
     * <pre>{@code
     * {@literal@}Override
     * public int getItemCount() {
     *   return Math.min(super.getItemCount(), mMaxItems);
     * }
     * }</pre>
     */
    public interface ItemCap {
        /** A value to pass to {@link #setMaxItems(int)} that indicates there should be no limit. */
        int UNLIMITED = -1;

        /**
         * Sets the maximum number of items available in the adapter. A value less than '0' means
         * the
         * list should not be capped.
         */
        void setMaxItems(int maxItems);
    }

    /**
     * Custom layout manager for the outer recyclerview. Since paddings should be applied by the
     * inner
     * recycler view within its bounds, this layout manager should always have 0 padding.
     */
    private static class CarUiRecyclerViewLayoutManager extends LinearLayoutManager {
        CarUiRecyclerViewLayoutManager(Context context) {
            super(context);
        }

        @Override
        public int getPaddingTop() {
            return 0;
        }

        @Override
        public int getPaddingBottom() {
            return 0;
        }

        @Override
        public int getPaddingStart() {
            return 0;
        }

        @Override
        public int getPaddingEnd() {
            return 0;
        }

        @Override
        public boolean canScrollHorizontally() {
            return false;
        }

        @Override
        public boolean canScrollVertically() {
            return false;
        }
    }

    public CarUiRecyclerView(@NonNull Context context) {
        this(context, null);
    }

    public CarUiRecyclerView(@NonNull Context context, @Nullable AttributeSet attrs) {
        this(context, attrs, R.attr.carUiRecyclerViewStyle);
    }

    public CarUiRecyclerView(@NonNull Context context, @Nullable AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);

        mCarUxRestrictionsUtil = CarUxRestrictionsUtil.getInstance(context);
        mListener = this::updateCarUxRestrictions;

        init(context, attrs, defStyle);
    }

    private void init(Context context, AttributeSet attrs, int defStyleAttr) {
        TypedArray a = context.obtainStyledAttributes(
                attrs,
                R.styleable.CarUiRecyclerView,
                defStyleAttr,
                R.style.Widget_CarUi_CarUiRecyclerView);

        mScrollBarEnabled = context.getResources().getBoolean(R.bool.car_ui_scrollbar_enable);
        mFullyInitialized = false;

        if (mScrollBarEnabled) {
            mNestedRecyclerView =
                    new RecyclerView(new ContextThemeWrapper(context,
                            R.style.Widget_CarUi_CarUiRecyclerView_NestedRecyclerView), attrs,
                            R.style.Widget_CarUi_CarUiRecyclerView_NestedRecyclerView);
            mNestedRecyclerView.setVerticalScrollBarEnabled(false);
        }

        mScrollBarPaddingStart =
                context.getResources().getDimension(R.dimen.car_ui_scrollbar_padding_start);
        mScrollBarPaddingEnd =
                context.getResources().getDimension(R.dimen.car_ui_scrollbar_padding_end);

        mCarUiRecyclerViewLayout =
                a.getInt(R.styleable.CarUiRecyclerView_layoutStyle, CarUiRecyclerViewLayout.LINEAR);
        mNumOfColumns = a.getInt(R.styleable.CarUiRecyclerView_numOfColumns, /* defValue= */ 2);
        boolean enableDivider =
                a.getBoolean(R.styleable.CarUiRecyclerView_enableDivider, /* defValue= */ false);

        if (mCarUiRecyclerViewLayout == CarUiRecyclerViewLayout.LINEAR) {

            int linearTopOffset =
                    a.getInteger(R.styleable.CarUiRecyclerView_startOffset, /* defValue= */ 0);
            int linearBottomOffset =
                    a.getInteger(R.styleable.CarUiRecyclerView_endOffset, /* defValue= */ 0);

            if (enableDivider) {
                RecyclerView.ItemDecoration dividerItemDecoration =
                        new LinearDividerItemDecoration(
                                context.getDrawable(R.drawable.car_ui_recyclerview_divider));
                addItemDecoration(dividerItemDecoration);
            }
            RecyclerView.ItemDecoration topOffsetItemDecoration =
                    new LinearOffsetItemDecoration(linearTopOffset, OffsetPosition.START);

            RecyclerView.ItemDecoration bottomOffsetItemDecoration =
                    new LinearOffsetItemDecoration(linearBottomOffset, OffsetPosition.END);

            addItemDecoration(topOffsetItemDecoration);
            addItemDecoration(bottomOffsetItemDecoration);
            setLayoutManager(new LinearLayoutManager(mContext));
        } else {
            int gridTopOffset =
                    a.getInteger(R.styleable.CarUiRecyclerView_startOffset, /* defValue= */ 0);
            int gridBottomOffset =
                    a.getInteger(R.styleable.CarUiRecyclerView_endOffset, /* defValue= */ 0);

            if (enableDivider) {
                mDividerItemDecoration =
                        new GridDividerItemDecoration(
                                context.getDrawable(R.drawable.car_ui_divider),
                                context.getDrawable(R.drawable.car_ui_divider),
                                mNumOfColumns);
                addItemDecoration(mDividerItemDecoration);
            }

            mOffsetItemDecoration =
                    new GridOffsetItemDecoration(gridTopOffset, mNumOfColumns,
                            OffsetPosition.START);

            GridOffsetItemDecoration bottomOffsetItemDecoration =
                    new GridOffsetItemDecoration(gridBottomOffset, mNumOfColumns,
                            OffsetPosition.END);

            addItemDecoration(mOffsetItemDecoration);
            addItemDecoration(bottomOffsetItemDecoration);
            setLayoutManager(new GridLayoutManager(mContext, mNumOfColumns));
            setNumOfColumns(mNumOfColumns);
        }

        if (!mScrollBarEnabled) {
            a.recycle();
            mFullyInitialized = true;
            return;
        }

        super.setLayoutManager(new CarUiRecyclerViewLayoutManager(context));
        super.setAdapter(new CarUiRecyclerViewAdapter());
        super.setNestedScrollingEnabled(false);
        super.setClipToPadding(false);

        // Gutter
        mGutter = context.getResources().getInteger(R.integer.car_ui_scrollbar_gutter);
        mGutterSize = getResources().getDimensionPixelSize(R.dimen.car_ui_scrollbar_margin);

        mScrollBarContainerWidth =
                (int) context.getResources().getDimension(
                        R.dimen.car_ui_scrollbar_container_width);

        mScrollBarPosition = context.getResources().getInteger(
                R.integer.car_ui_scrollbar_position);

        mScrollBarAboveRecyclerView =
                context.getResources().getBoolean(R.bool.car_ui_scrollbar_above_recycler_view);
        mScrollBarClass = context.getResources().getString(R.string.car_ui_scrollbar_component);
        a.recycle();
        this.mContext = context;
        // Apply inner RV layout changes after the layout has been calculated for this view.
        this.getViewTreeObserver()
                .addOnGlobalLayoutListener(
                        new OnGlobalLayoutListener() {
                            @Override
                            public void onGlobalLayout() {
                                // View holder layout is still pending.
                                if (CarUiRecyclerView.this.findViewHolderForAdapterPosition(0)
                                        == null) {
                                    return;
                                }
                                CarUiRecyclerView.this.getViewTreeObserver()
                                        .removeOnGlobalLayoutListener(this);
                                initNestedRecyclerView();
                                setNestedViewLayout();

                                mNestedRecyclerView.setHasFixedSize(true);
                                mNestedRecyclerView
                                        .getViewTreeObserver()
                                        .addOnGlobalLayoutListener(
                                                new OnGlobalLayoutListener() {
                                                    @Override
                                                    public void onGlobalLayout() {
                                                        mNestedRecyclerView
                                                                .getViewTreeObserver()
                                                                .removeOnGlobalLayoutListener(this);
                                                        createScrollBarFromConfig();
                                                        if (mInitialTopPadding == 0) {
                                                            mInitialTopPadding = getPaddingTop();
                                                        }
                                                        mFullyInitialized = true;
                                                    }
                                                });
                            }
                        });
    }

    @Override
    public void onHeightChanged(int height) {
        setPaddingRelative(getPaddingStart(), mInitialTopPadding + height,
                getPaddingEnd(), getPaddingBottom());
    }

    /**
     * Returns {@code true} if the {@CarUiRecyclerView} is fully drawn. Using a global layout
     * mListener
     * may not necessarily signify that this view is fully drawn (i.e. when the scrollbar is
     * enabled).
     * This is because the inner views (scrollbar and inner recycler view) are drawn after the
     * outer
     * views are finished.
     */
    public boolean fullyInitialized() {
        return mFullyInitialized;
    }

    /** Sets the number of columns in which grid needs to be divided. */
    public void setNumOfColumns(int numberOfColumns) {
        mNumOfColumns = numberOfColumns;
        if (mOffsetItemDecoration != null) {
            mOffsetItemDecoration.setNumOfColumns(mNumOfColumns);
        }
        if (mDividerItemDecoration != null) {
            mDividerItemDecoration.setNumOfColumns(mNumOfColumns);
        }
    }

    /**
     * Returns the {@link LayoutManager} for the {@link RecyclerView} displaying the content.
     *
     * <p>In cases where the scroll bar is visible and the nested {@link RecyclerView} is displaying
     * content, {@link #getLayoutManager()} cannot be used because it returns the {@link
     * LayoutManager} of the outer {@link RecyclerView}. {@link #getLayoutManager()} could not be
     * overridden to return the effective manager due to interference with accessibility node tree
     * traversal.
     */
    @Nullable
    public LayoutManager getEffectiveLayoutManager() {
        if (mScrollBarEnabled) {
            return mNestedRecyclerView.getLayoutManager();
        }
        return super.getLayoutManager();
    }

    /**
     * Refer to {@link CarUiRecyclerView#getEffectiveLayoutManager()} for usage in applications.
     */
    @Override
    public LayoutManager getLayoutManager() {
        return super.getLayoutManager();
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        mCarUxRestrictionsUtil.register(mListener);
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        mCarUxRestrictionsUtil.unregister(mListener);
    }

    private void updateCarUxRestrictions(CarUxRestrictions carUxRestrictions) {
        // If the adapter does not implement ItemCap, then the max items on it cannot be updated.
        if (!(mAdapter instanceof ItemCap)) {
            return;
        }

        int maxItems = ItemCap.UNLIMITED;
        if ((carUxRestrictions.getActiveRestrictions()
                & CarUxRestrictions.UX_RESTRICTIONS_LIMIT_CONTENT)
                != 0) {
            maxItems = carUxRestrictions.getMaxCumulativeContentItems();
        }

        int originalCount = mAdapter.getItemCount();
        ((ItemCap) mAdapter).setMaxItems(maxItems);
        int newCount = mAdapter.getItemCount();

        if (newCount == originalCount) {
            return;
        }

        if (newCount < originalCount) {
            mAdapter.notifyItemRangeRemoved(newCount, originalCount - newCount);
        } else {
            mAdapter.notifyItemRangeInserted(originalCount, newCount - originalCount);
        }
    }

    @Override
    public void setClipToPadding(boolean clipToPadding) {
        if (mScrollBarEnabled) {
            mNestedRecyclerView.setClipToPadding(clipToPadding);
        } else {
            super.setClipToPadding(clipToPadding);
        }
    }

    @SuppressWarnings("rawtypes")
    @Override
    public void setAdapter(@Nullable Adapter adapter) {

        this.mAdapter = adapter;
        if (mScrollBarEnabled) {
            mNestedRecyclerView.setAdapter(adapter);
        } else {
            super.setAdapter(adapter);
        }
    }

    @Nullable
    @Override
    public Adapter<?> getAdapter() {
        if (mScrollBarEnabled) {
            return mNestedRecyclerView.getAdapter();
        }
        return super.getAdapter();
    }

    @Override
    public void setLayoutManager(@Nullable LayoutManager layout) {
        if (mScrollBarEnabled) {
            mNestedRecyclerView.setLayoutManager(layout);
        } else {
            super.setLayoutManager(layout);
        }
    }

    @Override
    public void setOnScrollChangeListener(OnScrollChangeListener l) {
        if (mScrollBarEnabled) {
            mNestedRecyclerView.setOnScrollChangeListener(l);
        } else {
            super.setOnScrollChangeListener(l);
        }
    }

    @Override
    public void setVerticalFadingEdgeEnabled(boolean verticalFadingEdgeEnabled) {
        if (mScrollBarEnabled) {
            mNestedRecyclerView.setVerticalFadingEdgeEnabled(verticalFadingEdgeEnabled);
        } else {
            super.setVerticalFadingEdgeEnabled(verticalFadingEdgeEnabled);
        }
    }

    @Override
    public void setFadingEdgeLength(int length) {
        if (mScrollBarEnabled) {
            mNestedRecyclerView.setFadingEdgeLength(length);
        } else {
            super.setFadingEdgeLength(length);
        }
    }

    @Override
    public void addItemDecoration(@NonNull ItemDecoration decor, int index) {
        if (mScrollBarEnabled) {
            mNestedRecyclerView.addItemDecoration(decor, index);
        } else {
            super.addItemDecoration(decor, index);
        }
    }

    @Override
    public void addItemDecoration(@NonNull ItemDecoration decor) {
        if (mScrollBarEnabled) {
            mNestedRecyclerView.addItemDecoration(decor);
        } else {
            super.addItemDecoration(decor);
        }
    }

    @Override
    public void setItemAnimator(@Nullable ItemAnimator animator) {
        if (mScrollBarEnabled) {
            mNestedRecyclerView.setItemAnimator(animator);
        } else {
            super.setItemAnimator(animator);
        }
    }

    @Override
    public void setPadding(int left, int top, int right, int bottom) {
        if (mScrollBarEnabled) {
            mNestedRecyclerView.setPadding(left, top, right, bottom);
            if (mScrollBar != null) {
                mScrollBar.requestLayout();
            }
        } else {
            super.setPadding(left, top, right, bottom);
        }
    }

    @Override
    public void setPaddingRelative(int start, int top, int end, int bottom) {
        if (mScrollBarEnabled) {
            mNestedRecyclerView.setPaddingRelative(start, top, end, bottom);
            if (mScrollBar != null) {
                mScrollBar.requestLayout();
            }
        } else {
            super.setPaddingRelative(start, top, end, bottom);
        }
    }

    @Override
    public ViewHolder findViewHolderForLayoutPosition(int position) {
        if (mScrollBarEnabled) {
            return mNestedRecyclerView.findViewHolderForLayoutPosition(position);
        } else {
            return super.findViewHolderForLayoutPosition(position);
        }
    }

    @Override
    public ViewHolder findViewHolderForAdapterPosition(int position) {
        if (mScrollBarEnabled && mIsNestedRecyclerViewInitialized) {
            return mNestedRecyclerView.findViewHolderForAdapterPosition(position);
        } else {
            return super.findViewHolderForAdapterPosition(position);
        }
    }

    @Override
    public ViewHolder findContainingViewHolder(View view) {
        if (mScrollBarEnabled) {
            return mNestedRecyclerView.findContainingViewHolder(view);
        } else {
            return super.findContainingViewHolder(view);
        }
    }

    @Override
    @Nullable
    public View findChildViewUnder(float x, float y) {
        if (mScrollBarEnabled) {
            return mNestedRecyclerView.findChildViewUnder(x, y);
        } else {
            return super.findChildViewUnder(x, y);
        }
    }

    @Override
    public void addOnScrollListener(@NonNull OnScrollListener listener) {
        if (mScrollBarEnabled) {
            mNestedRecyclerView.addOnScrollListener(listener);
        } else {
            super.addOnScrollListener(listener);
        }
    }

    @Override
    public void removeOnScrollListener(@NonNull OnScrollListener listener) {
        if (mScrollBarEnabled) {
            mNestedRecyclerView.removeOnScrollListener(listener);
        } else {
            super.removeOnScrollListener(listener);
        }
    }

    @Override
    public int getPaddingStart() {
        return mScrollBarEnabled ? mNestedRecyclerView.getPaddingStart() : super.getPaddingStart();
    }

    @Override
    public int getPaddingEnd() {
        return mScrollBarEnabled ? mNestedRecyclerView.getPaddingEnd() : super.getPaddingEnd();
    }

    @Override
    public int getPaddingTop() {
        return mScrollBarEnabled ? mNestedRecyclerView.getPaddingTop() : super.getPaddingTop();
    }

    @Override
    public int getPaddingBottom() {
        return mScrollBarEnabled ? mNestedRecyclerView.getPaddingBottom()
                : super.getPaddingBottom();
    }

    @Override
    public void setVisibility(int visibility) {
        super.setVisibility(visibility);
        if (mScrollBarEnabled) {
            mNestedRecyclerView.setVisibility(visibility);
        }
    }

    private void initNestedRecyclerView() {
        CarUiRecyclerViewAdapter.NestedRowViewHolder vh =
                (CarUiRecyclerViewAdapter.NestedRowViewHolder)
                        this.findViewHolderForAdapterPosition(0);
        if (vh == null) {
            throw new Error("Outer RecyclerView failed to initialize.");
        }

        vh.frameLayout.addView(mNestedRecyclerView);
        mIsNestedRecyclerViewInitialized = true;
    }

    private void createScrollBarFromConfig() {
        if (DEBUG) {
            Log.d(TAG, "createScrollBarFromConfig");
        }

        Class<?> cls;
        try {
            cls = !TextUtils.isEmpty(mScrollBarClass)
                    ? getContext().getClassLoader().loadClass(mScrollBarClass)
                    : DefaultScrollBar.class;
        } catch (Throwable t) {
            throw andLog("Error loading scroll bar component: " + mScrollBarClass, t);
        }
        try {
            mScrollBar = (ScrollBar) cls.getDeclaredConstructor().newInstance();
        } catch (Throwable t) {
            throw andLog("Error creating scroll bar component: " + mScrollBarClass, t);
        }

        mScrollBar.initialize(
                mNestedRecyclerView, mScrollBarContainerWidth, mScrollBarPosition,
                mScrollBarAboveRecyclerView);

        mScrollBar.setPadding((int) mScrollBarPaddingStart, (int) mScrollBarPaddingEnd);

        if (DEBUG) {
            Log.d(TAG, "started " + mScrollBar.getClass().getSimpleName());
        }
    }

    /**
     * Sets the scrollbar's padding start (top) and end (bottom).
     * This padding is applied in addition to the padding of the inner RecyclerView.
     */
    public void setScrollBarPadding(int paddingStart, int paddingEnd) {
        if (mScrollBarEnabled) {
            mScrollBarPaddingStart = paddingStart;
            mScrollBarPaddingEnd = paddingEnd;

            if (mScrollBar != null) {
                mScrollBar.setPadding(paddingStart, paddingEnd);
            }
        }
    }

    /**
     * Calls {@link #layout(int, int, int, int)} for both this RecyclerView and the nested one.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public void layoutBothForTesting(int l, int t, int r, int b) {
        super.layout(l, t, r, b);
        if (mScrollBarEnabled) {
            mNestedRecyclerView.layout(l, t, r, b);
        }
    }

    /**
     * Set the nested view's layout to the specified value.
     *
     * <p>The mGutter is the space to the start/end of the list view items and will be equal in size
     * to
     * the scroll bars. By default, there is a mGutter to both the left and right of the list view
     * items, to account for the scroll bar.
     */
    private void setNestedViewLayout() {
        int startMargin = 0;
        int endMargin = 0;
        if ((mGutter & Gutter.START) != 0) {
            startMargin = mGutterSize;
        }
        if ((mGutter & Gutter.END) != 0) {
            endMargin = mGutterSize;
        }

        MarginLayoutParams layoutParams =
                (MarginLayoutParams) mNestedRecyclerView.getLayoutParams();

        layoutParams.setMarginStart(startMargin);
        layoutParams.setMarginEnd(endMargin);

        layoutParams.height = LayoutParams.MATCH_PARENT;
        layoutParams.width = super.getLayoutManager().getWidth() - startMargin - endMargin;
        // requestLayout() isn't sufficient because we also need to resolveLayoutParams().
        mNestedRecyclerView.setLayoutParams(layoutParams);

        // If there's a mGutter, set ClipToPadding to false so that CardView's shadow will still
        // appear outside of the padding.
        mNestedRecyclerView.setClipToPadding(startMargin == 0 && endMargin == 0);
    }

    private static RuntimeException andLog(String msg, Throwable t) {
        Log.e(TAG, msg, t);
        throw new RuntimeException(msg, t);
    }

    @Override
    public Parcelable onSaveInstanceState() {
        Parcelable superState = super.onSaveInstanceState();
        SavedState ss = new SavedState(superState);
        if (mScrollBarEnabled) {
            mNestedRecyclerView.saveHierarchyState(ss.mNestedRecyclerViewState);
        }
        return ss;
    }

    @Override
    public void onRestoreInstanceState(Parcelable state) {
        if (!(state instanceof SavedState)) {
            Log.w(TAG, "onRestoreInstanceState called with an unsupported state");
            super.onRestoreInstanceState(state);
        } else {
            SavedState ss = (SavedState) state;
            super.onRestoreInstanceState(ss.getSuperState());
            if (mScrollBarEnabled) {
                mNestedRecyclerView.restoreHierarchyState(ss.mNestedRecyclerViewState);
            }
        }
    }

    static class SavedState extends BaseSavedState {
        SparseArray<Parcelable> mNestedRecyclerViewState;

        SavedState(Parcelable superState) {
            super(superState);
            mNestedRecyclerViewState = new SparseArray<>();
        }

        private SavedState(Parcel in, ClassLoader classLoader) {
            super(in, classLoader);
            mNestedRecyclerViewState = in.readSparseArray(classLoader);
        }

        @SuppressWarnings("unchecked")
        @Override
        public void writeToParcel(Parcel out, int flags) {
            super.writeToParcel(out, flags);
            out.writeSparseArray((SparseArray<Object>) (Object) mNestedRecyclerViewState);
        }

        public static final Parcelable.Creator<SavedState> CREATOR =
                new Parcelable.Creator<SavedState>() {
                    @Override
                    public SavedState createFromParcel(Parcel in) {
                        return new SavedState(in, getClass().getClassLoader());
                    }

                    @Override
                    public SavedState[] newArray(int size) {
                        return new SavedState[size];
                    }
                };
    }
}
