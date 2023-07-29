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

package com.android.car.ui.recyclerview;

import android.content.Context;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.android.car.ui.R;

import java.util.ArrayList;
import java.util.List;

/**
 * A {@link LinearLayoutManager} implementation which provides a fixed scrollbar when used with
 * viewholders that vary in height. This layout manager is only compatible with {@link
 * CarUiListItemAdapter}.
 */
public class CarUiListItemLayoutManager extends LinearLayoutManager {

    private final int mContentItemSize;
    private final int mHeaderItemSize;

    private List<Integer> mListItemHeights;
    private CarUiListItemAdapter mAdapter;

    public CarUiListItemLayoutManager(@NonNull Context context) {
        super(context);
        mContentItemSize = (int) context.getResources().getDimension(
                R.dimen.car_ui_list_item_height);
        mHeaderItemSize = (int) context.getResources().getDimension(
                R.dimen.car_ui_list_item_header_height);

        setSmoothScrollbarEnabled(false);
    }

    @Override
    public void onAttachedToWindow(RecyclerView recyclerView) {
        super.onAttachedToWindow(recyclerView);
        populateHeightMap(recyclerView.getAdapter());
    }

    @Override
    public void onAdapterChanged(RecyclerView.Adapter oldAdapter, RecyclerView.Adapter newAdapter) {
        super.onAdapterChanged(oldAdapter, newAdapter);
        populateHeightMap(newAdapter);
    }

    @Override
    public void onItemsAdded(@NonNull RecyclerView recyclerView, int positionStart, int itemCount) {
        super.onItemsAdded(recyclerView, positionStart, itemCount);
        populateHeightMap(recyclerView.getAdapter());
    }

    @Override
    public void onItemsRemoved(@NonNull RecyclerView recyclerView, int positionStart,
            int itemCount) {
        super.onItemsRemoved(recyclerView, positionStart, itemCount);
        populateHeightMap(recyclerView.getAdapter());
    }

    @Override
    public void onItemsUpdated(@NonNull RecyclerView recyclerView, int positionStart,
            int itemCount) {
        super.onItemsUpdated(recyclerView, positionStart, itemCount);
        populateHeightMap(recyclerView.getAdapter());
    }

    @Override
    public void onItemsChanged(@NonNull RecyclerView recyclerView) {
        super.onItemsChanged(recyclerView);
        populateHeightMap(recyclerView.getAdapter());
    }

    @Override
    public int computeVerticalScrollExtent(RecyclerView.State state) {
        final int count = getChildCount();
        return count > 0 ? mContentItemSize * 3 : 0;
    }

    @Override
    public int computeVerticalScrollRange(RecyclerView.State state) {
        return Math.max(mListItemHeights.get(mListItemHeights.size() - 1), 0);
    }

    @Override
    public int computeVerticalScrollOffset(RecyclerView.State state) {
        if (getChildCount() <= 0) {
            return 0;
        }

        int firstPos = findFirstVisibleItemPosition();
        if (firstPos == RecyclerView.NO_POSITION) {
            return 0;
        }

        View view = findViewByPosition(firstPos);
        if (view == null) {
            return 0;
        }

        final int top = getDecoratedTop(view);
        final int height = getDecoratedMeasuredHeight(view);

        int heightOfScreen;
        if (height <= 0) {
            heightOfScreen = 0;
        } else {
            CarUiListItem item = mAdapter.getItems().get(firstPos);
            if (item instanceof CarUiContentListItem) {
                heightOfScreen = Math.abs(mContentItemSize * top / height);
            } else if (item instanceof CarUiHeaderListItem) {
                heightOfScreen = Math.abs(mHeaderItemSize * top / height);
            } else {
                throw new IllegalStateException("Unknown list item type.");
            }
        }

        return mListItemHeights.get(firstPos) + heightOfScreen;
    }

    /**
     * Populates an internal list of cumulative heights at each position for the list to be laid out
     * for the adapter parameter.
     *
     * @param adapter the action type for the item.
     */
    private void populateHeightMap(RecyclerView.Adapter adapter) {
        if (!(adapter instanceof CarUiListItemAdapter)) {
            throw new IllegalStateException(
                    "Cannot use CarUiListItemLayoutManager with an adapter that is not of type "
                            + "CarUiListItemAdapter");
        }

        mAdapter = (CarUiListItemAdapter) adapter;
        List<CarUiListItem> itemList = mAdapter.getItems();
        mListItemHeights = new ArrayList<>();

        int cumulativeHeight = 0;
        mListItemHeights.add(cumulativeHeight);
        for (CarUiListItem item : itemList) {
            if (item instanceof CarUiContentListItem) {
                cumulativeHeight += mContentItemSize;
                mListItemHeights.add(cumulativeHeight);
            } else if (item instanceof CarUiHeaderListItem) {
                cumulativeHeight += mHeaderItemSize;
                mListItemHeights.add(cumulativeHeight);
            } else {
                throw new IllegalStateException("Unknown CarUiListItem type");
            }
        }
    }
}
