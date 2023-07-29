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

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.PointF;
import android.view.View;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.android.car.ui.CarUiRobolectricTestRunner;
import com.android.car.ui.TestConfig;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

@RunWith(CarUiRobolectricTestRunner.class)
@Config(manifest = TestConfig.MANIFEST_PATH, sdk = TestConfig.SDK_VERSION)
public class CarUiSnapHelperTest {

    private Context mContext;
    private CarUiSnapHelper mCarUiSnapHelper;

    @Mock
    private RecyclerView mRecyclerView;
    @Mock
    private LinearLayoutManager mLayoutManager;
    @Mock
    private RecyclerView.Adapter mAdapter;
    @Mock
    private View mChild;
    @Mock
    private RecyclerView.LayoutParams mLayoutParams;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = RuntimeEnvironment.application;

        mCarUiSnapHelper = new CarUiSnapHelper(mContext);

        when(mRecyclerView.getContext()).thenReturn(mContext);
        mCarUiSnapHelper.attachToRecyclerView(mRecyclerView);
    }

    @Test
    public void smoothScrollBy_invalidSnapPosition_shouldCallRecylerViewSmoothScrollBy() {
        when(mRecyclerView.getLayoutManager()).thenReturn(mLayoutManager);

        mCarUiSnapHelper.smoothScrollBy(10);

        verify(mRecyclerView).smoothScrollBy(0, 10);
    }

    @Test
    public void smoothScrollBy_invalidSnapPositionNoItem_shouldCallRecylerViewSmoothScrollBy() {
        when(mRecyclerView.getLayoutManager()).thenReturn(mLayoutManager);
        when(mLayoutManager.getItemCount()).thenReturn(0);

        mCarUiSnapHelper.smoothScrollBy(10);

        verify(mRecyclerView).smoothScrollBy(0, 10);
    }

    @Test
    public void smoothScrollBy_invalidSnapPositionNoView_shouldCallRecylerViewSmoothScrollBy() {
        when(mRecyclerView.getLayoutManager()).thenReturn(mLayoutManager);
        when(mLayoutManager.getItemCount()).thenReturn(10);
        when(mLayoutManager.canScrollVertically()).thenReturn(false);
        when(mLayoutManager.canScrollHorizontally()).thenReturn(false);

        mCarUiSnapHelper.smoothScrollBy(10);

        verify(mRecyclerView).smoothScrollBy(0, 10);
    }

    @Test
    public void smoothScrollBy_invalidSnapPositionNoVectore_shouldCallRecylerViewSmoothScrollBy() {
        when(mRecyclerView.getLayoutManager()).thenReturn(mLayoutManager);
        when(mLayoutManager.getItemCount()).thenReturn(10);
        when(mLayoutManager.canScrollVertically()).thenReturn(true);
        when(mLayoutManager.getChildCount()).thenReturn(1);
        when(mChild.getLayoutParams()).thenReturn(mLayoutParams);
        when(mLayoutManager.getChildAt(0)).thenReturn(mChild);

        mCarUiSnapHelper.smoothScrollBy(10);

        verify(mRecyclerView).smoothScrollBy(0, 10);
    }

    @Test
    public void smoothScrollBy_invalidSnapPositionNoDelta_shouldCallRecylerViewSmoothScrollBy() {
        when(mRecyclerView.getLayoutManager()).thenReturn(mLayoutManager);
        when(mLayoutManager.getItemCount()).thenReturn(1);
        when(mLayoutManager.canScrollVertically()).thenReturn(true);
        when(mLayoutManager.getChildCount()).thenReturn(1);
        // no delta
        when(mLayoutManager.getDecoratedBottom(any())).thenReturn(0);
        when(mChild.getLayoutParams()).thenReturn(mLayoutParams);
        when(mLayoutManager.getChildAt(0)).thenReturn(mChild);

        PointF vectorForEnd = new PointF(100, 100);
        when(mLayoutManager.computeScrollVectorForPosition(0)).thenReturn(vectorForEnd);

        mCarUiSnapHelper.smoothScrollBy(10);

        verify(mRecyclerView).smoothScrollBy(0, 10);
    }

    @Test
    public void smoothScrollBy_validSnapPosition_shouldCallRecylerViewSmoothScrollBy() {
        when(mRecyclerView.getLayoutManager()).thenReturn(mLayoutManager);
        when(mLayoutManager.getItemCount()).thenReturn(1);
        when(mLayoutManager.canScrollVertically()).thenReturn(true);
        when(mLayoutManager.getChildCount()).thenReturn(1);
        // some delta
        when(mLayoutManager.getDecoratedBottom(any())).thenReturn(10);
        when(mChild.getLayoutParams()).thenReturn(mLayoutParams);
        when(mLayoutManager.getChildAt(0)).thenReturn(mChild);

        PointF vectorForEnd = new PointF(100, 100);
        when(mLayoutManager.computeScrollVectorForPosition(0)).thenReturn(vectorForEnd);

        mCarUiSnapHelper.smoothScrollBy(10);

        verify(mLayoutManager).startSmoothScroll(any(RecyclerView.SmoothScroller.class));
    }

    @Test
    public void calculateDistanceToFinalSnap_shouldReturnTopMarginDifference() {
        when(mRecyclerView.getLayoutManager()).thenReturn(mLayoutManager);
        when(mLayoutManager.getItemCount()).thenReturn(1);
        when(mLayoutManager.canScrollVertically()).thenReturn(true);
        when(mLayoutManager.getChildCount()).thenReturn(1);
        // some delta
        when(mLayoutManager.getDecoratedTop(any())).thenReturn(10);
        when(mChild.getLayoutParams()).thenReturn(mLayoutParams);

        int[] distance = mCarUiSnapHelper.calculateDistanceToFinalSnap(mLayoutManager, mChild);

        assertThat(distance[1]).isEqualTo(10);
    }

    @Test
    public void calculateScrollDistance_shouldScrollHeightOfView() {
        when(mRecyclerView.getLayoutManager()).thenReturn(mLayoutManager);
        when(mLayoutManager.getItemCount()).thenReturn(1);
        when(mLayoutManager.canScrollVertically()).thenReturn(true);
        when(mLayoutManager.getChildCount()).thenReturn(1);
        // some delta
        when(mLayoutManager.getDecoratedTop(any())).thenReturn(10);
        when(mChild.getLayoutParams()).thenReturn(mLayoutParams);
        when(mLayoutManager.getChildAt(0)).thenReturn(mChild);
        when(mLayoutManager.getHeight()).thenReturn(-50);

        int[] distance = mCarUiSnapHelper.calculateScrollDistance(0, 10);

        assertThat(distance[1]).isEqualTo(50);
    }
}
