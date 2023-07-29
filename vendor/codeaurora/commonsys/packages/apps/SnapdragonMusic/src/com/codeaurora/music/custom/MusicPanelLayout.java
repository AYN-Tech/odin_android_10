/*
 * Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.

 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

package com.codeaurora.music.custom;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.media.AudioManager;
import android.os.Parcel;
import android.os.Parcelable;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.support.v4.view.MotionEventCompat;
import android.support.v4.view.ViewCompat;
import android.util.Log;

import com.android.music.MediaPlaybackActivity;
import com.android.music.MusicBrowserActivity.SimplePanelSlideListener;
import com.android.music.MusicUtils;

public class MusicPanelLayout extends ViewGroup {
    private static final String TAG = "MusicPanelLayout";

    private static final int[] INITIAl_ATTRIBUTES = new int[] { android.R.attr.gravity };
    private final static int MARGIN_TOP = 45;
    private final static int MARGIN_BOTTOM = 8;
    private static final int INITIAl_BOARD_LENGTH = 64;
    private static final int INITIAL_TRACE_HEIGHT = 4;
    private final static int SCREEN_HEIGHT_CONTROL = 900;
    private final static int WINDOW_TOP_OF_BOTTOM_SPLIT_SCREEN = 250;

    private int mMinimumVelocityOfFling = 400;
    private int mFadeColourCovered = 0x99000000;
    private static final int INITIAL_OFFSET_PARALAX = 0;
    private static final int ZERO = 0;
    private final Paint mFadePaintOccupied = new Paint();
    private Drawable mTraceDrawable = null;
    private int mBoardHeight = -1;
    private int mTraceHeight = -1;
    private int mParOffset = -1;
    private boolean mIsSlippingUp;
    private boolean mOverlayData = false;
    private boolean mHookPanel = true;
    private View mSlipView;
    private int mSlipViewResId = -1;
    private View mSlippingView;
    private View mInitialView;
    public static View  mSeekBarView;
    public static View mSongsQueueView;
    private int[] mSeekBarCoordinate =  new int[2];
    private int[] mSongsQueueCoordinate = new int[2];

    public enum BoardState {
        EXPANDED, COLLAPSED, ANCHORED, HIDDEN, DRAGGING
    }

    public BoardState mSlipState = BoardState.COLLAPSED;
    public boolean mIsQueueEnabled = false;
    private boolean mIsLandscape = false;
    private BoardState mPreviuosStoppedDraggingSlipState = BoardState.COLLAPSED;
    private float mSlipOffset;
    private int mSlipRange;
    private boolean mIsCanNotDrag;
    private boolean mCheckTouchEnabled;
    private boolean mIsHookViewUsingTouchEvents;
    private float mDefaultXMotion;
    private float mDefaultYMotion;
    private float mMainStayPoint = 1.f;
    private ViewHookSlipListener mHookSlipListener;
    private final MusicPanelViewDragHelper mHookHelper;
    private boolean mPrimaryLayout = true;
    private final Rect mSampleRectangle = new Rect();

    public interface ViewHookSlipListener {

        public void onViewSlip(View view, float slipOffset);

        public void onViewClosed(View view);

        public void onViewOpened(View view);

        public void onViewMainLined(View panel);

        public void onViewBackStacked(View panel);
    }

    public MusicPanelLayout(Context context) {
        this(context, null);
    }

    public MusicPanelLayout(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public MusicPanelLayout(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);

        setGravity(Gravity.BOTTOM);

        final float density = context.getResources().getDisplayMetrics().density;
        if (mBoardHeight == -1) {
            mBoardHeight = (int) (INITIAl_BOARD_LENGTH * density + 0.5f);
        }
        if (mTraceHeight == -1) {
            mTraceHeight = (int) (INITIAL_TRACE_HEIGHT * density + 0.5f);
        }
        if (mParOffset == -1) {
            mParOffset = (int) (INITIAL_OFFSET_PARALAX * density);
        }
        setWillNotDraw(false);

        setHookSlipListener(new SimplePanelSlideListener());

        mHookHelper = MusicPanelViewDragHelper.create(this, 0.5f,
                new DragHelperCallback());
        mHookHelper.setPanelMinVelocity(mMinimumVelocityOfFling * density);

        mCheckTouchEnabled = true;
        configurationChanged();
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        if (mSlipViewResId != -1) {
            setDragView(findViewById(mSlipViewResId));
        }
    }

    public void setGravity(int gravity) {
        mIsSlippingUp = gravity == Gravity.BOTTOM;
        if (!mPrimaryLayout) {
            requestLayout();
        }
    }

    public void setCoveredFadeColor(int color) {
        mFadeColourCovered = color;
        invalidate();
    }

    public int getCoveredFadeColor() {
        return mFadeColourCovered;
    }

    public void setTouchEnabled(boolean enabled) {
        mCheckTouchEnabled = enabled;
    }

    public boolean isTouchEnabled() {
        return mCheckTouchEnabled && mSlippingView != null
                && mSlipState != BoardState.HIDDEN;
    }

    public void setPanelHeight(int val) {
        if (getHookHeight() == val) {
            return;
        }

        mBoardHeight = val;
        if (!mPrimaryLayout) {
            requestLayout();
        }

        if (getHookState() == BoardState.COLLAPSED) {
            smoothDragViewDown();
            invalidate();
            return;
        }
    }

    protected void smoothDragViewDown() {
        smoothSlipTo(0, 0, false);
    }

    public int getHookHeight() {
        return mBoardHeight;
    }

    public int getOnGoingParalaxOffset() {
        int offset = (int) (mParOffset * Math.max(mSlipOffset, 0));
        return mIsSlippingUp ? -offset : offset;
    }

    public void setHookSlipListener(ViewHookSlipListener listener) {
        mHookSlipListener = listener;
    }

    public void setDragView(View dragView) {
        if (mSlipView != null) {
            mSlipView.setOnClickListener(null);

        }
        mSlipView = dragView;
        if (mSlipView != null) {
            mSlipView.setClickable(true);
            mSlipView.setFocusable(false);
            mSlipView.setFocusableInTouchMode(false);
            View view = mSlipView
                    .findViewById(com.android.music.R.id.header_layout);
            view.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    if (mSlipState != BoardState.EXPANDED
                            && mSlipState != BoardState.ANCHORED) {
                        if (mMainStayPoint < 1.0f) {
                            setHookState(BoardState.ANCHORED);
                        } else {
                            setHookState(BoardState.EXPANDED);
                        }
                    } else {
                        setHookState(BoardState.COLLAPSED);
                    }
                }
            });
            ;
        }
    }

    public void setDragView(int dragViewResId) {
        mSlipViewResId = dragViewResId;
        setDragView(findViewById(dragViewResId));
    }

    public void setAnchorPoint(float anchorPoint) {
        if (anchorPoint > 0 && anchorPoint <= 1) {
            mMainStayPoint = anchorPoint;
        }
    }

    public float getAnchorPoint() {
        return mMainStayPoint;
    }

    public void setOverlayed(boolean overlayed) {
        mOverlayData = overlayed;
    }

    public boolean isOverlayed() {
        return mOverlayData;
    }

    public void setClipPanel(boolean clip) {
        mHookPanel = clip;
    }

    public boolean isClipPanel() {
        return mHookPanel;
    }

    void dispatchOnPanelSlide(View panel) {
        if (mHookSlipListener != null) {
            mHookSlipListener.onViewSlip(panel, mSlipOffset);
        }
    }

    void refreshObscuredViewVisibility() {
        if (getChildCount() == ZERO) {
            return;
        }
        int rBound = getWidth() - getPaddingEnd();
        int lBound = getPaddingStart();
        int bBound = getHeight() - getPaddingBottom();
        int tBound = getPaddingTop();

        int left = 0, right = 0, top = 0, bottom = 0;

        if (mSlippingView != null) {
            if (isBackgroundOpaque(mSlippingView)) {
                bottom = mSlippingView.getBottom();
                top = mSlippingView.getTop();
                left = mSlippingView.getLeft();
                right = mSlippingView.getRight();
            }
        } else {
            bottom = top = right = left = 0;
        }
        View childView = getChildAt(0);
        int cLeft = Math.max(lBound, childView.getLeft());
        int cTop = Math.max(tBound, childView.getTop());
        int cRight = Math.min(rBound, childView.getRight());
        int cBottom = Math.min(bBound, childView.getBottom());
        int vis;
        if (cLeft >= left && cTop >= top && cRight <= right
                && cBottom <= bottom) {
            vis = INVISIBLE;
        } else {
            vis = VISIBLE;

        }
        childView.setVisibility(vis);
    }

    void makeViewsVisible() {
        int childCount = getChildCount();
        int k = ZERO;
        while (k < childCount) {
            View c = getChildAt(k);
            if (c.getVisibility() == INVISIBLE) {
                c.setVisibility(VISIBLE);
            }
            k = k + 1;
        }
    }

    private static boolean isBackgroundOpaque(View view) {
        Drawable bg = view.getBackground();
        boolean res = bg != null && bg.getOpacity() == PixelFormat.OPAQUE;
        return res;
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        mPrimaryLayout = true;
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        mPrimaryLayout = true;
    }

    @Override
    protected void onMeasure(int widthSpec, int heightSpec) {
        int wSize = MeasureSpec.getSize(widthSpec);
        int hMode = MeasureSpec.getMode(heightSpec);
        int hSize = MeasureSpec.getSize(heightSpec);
        int wMode = MeasureSpec.getMode(widthSpec);

        if (hMode != MeasureSpec.EXACTLY) {
            throw new IllegalStateException(
                    "Height should have defined value or MATCH_PARENT");
        } else if (wMode != MeasureSpec.EXACTLY) {
            throw new IllegalStateException(
                    "Width should have defined value or MATCH_PARENT");
        }

        int cCount = getChildCount();
        mSlippingView = getChildAt(1);
        mInitialView = getChildAt(ZERO);
        if (mSlipView == null) {
            setDragView(mSlippingView);
        }

        if (mSlippingView.getVisibility() != VISIBLE) {
            // If onMeasure() run before onServiceConnected(), the sService will be null here,
            // but actually there is audio playing
            if (MusicUtils.shouldHideNowPlayingBar()) {
                mSlipState = BoardState.HIDDEN;
            }
        }

        int lWidth = wSize - getPaddingStart() - getPaddingEnd();
        int lHeight = hSize - getPaddingTop() - getPaddingBottom();

        for (int k = 0; k < cCount; k++) {
            View childView = getChildAt(k);
            MusicScreenParams lp = (MusicScreenParams) childView
                    .getLayoutParams();

            if (childView.getVisibility() == GONE && k == 0) {
                continue;
            }

            int h = lHeight;
            int w = lWidth;
            if (mInitialView == childView) {
                if (!mOverlayData) {
                    if (mSlipState != BoardState.HIDDEN) {
                        h = h - mBoardHeight;
                    } else {
                        lp.height = MusicScreenParams.MATCH_PARENT;
                    }
                }

                w -= lp.leftMargin + lp.rightMargin;
            } else if (mSlippingView == childView) {
                int statusBarHeight = MediaPlaybackActivity.getStatusBarHeight();
                // Do NOT need to reduce status bar height if we're in the bottom
                // window of split screen
                if (statusBarHeight < WINDOW_TOP_OF_BOTTOM_SPLIT_SCREEN) {
                    h = h - statusBarHeight;
                }
            }
            int childWSpec;
            if (lp.width == MusicScreenParams.WRAP_CONTENT) {
                childWSpec = MeasureSpec
                        .makeMeasureSpec(w, MeasureSpec.AT_MOST);
            } else if (lp.width == MusicScreenParams.MATCH_PARENT) {
                childWSpec = MeasureSpec
                        .makeMeasureSpec(w, MeasureSpec.EXACTLY);
            } else {
                childWSpec = MeasureSpec.makeMeasureSpec(lp.width,
                        MeasureSpec.EXACTLY);
            }

            int childHSpec;
            if (lp.height == MusicScreenParams.WRAP_CONTENT) {
                childHSpec = MeasureSpec
                        .makeMeasureSpec(h, MeasureSpec.AT_MOST);
            } else if (lp.height == MusicScreenParams.MATCH_PARENT) {
                childHSpec = MeasureSpec
                        .makeMeasureSpec(h, MeasureSpec.EXACTLY);
            } else {
                childHSpec = MeasureSpec.makeMeasureSpec(lp.height,
                        MeasureSpec.EXACTLY);
            }

            childView.measure(childWSpec, childHSpec);

            if (childView == mSlippingView) {
                mSlipRange = mSlippingView.getMeasuredHeight() - mBoardHeight;
            }
        }

        setMeasuredDimension(wSize, hSize);
    }

    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        int pTop = getPaddingTop();
        int cCount = getChildCount();
        int pLeft = getPaddingStart();

        if (mPrimaryLayout) {
            switch (mSlipState) {
            case HIDDEN:
                int top = computeHookViewTopPosition(0.0f)
                        + (mIsSlippingUp ? +mBoardHeight : -mBoardHeight);
                mSlipOffset = computeSlideOffset(top);
                break;
            case ANCHORED:
                mSlipOffset = mMainStayPoint;
                break;
            case EXPANDED:
                mSlipOffset = 1.0f;
                break;
            default:
                mSlipOffset = 0.f;
                break;
            }
        }

        for (int k = ZERO; k < cCount; k++) {
            View childView = getChildAt(k);
            MusicScreenParams lp = (MusicScreenParams) childView
                    .getLayoutParams();

            if ((k == 0 || mPrimaryLayout) && childView.getVisibility() == GONE) {
                continue;
            }

            int cTop = pTop;
            int cHeight = childView.getMeasuredHeight();

            if (mSlippingView == childView) {
                cTop = computeHookViewTopPosition(mSlipOffset);
            }

            if (!mIsSlippingUp) {
                if (childView == mInitialView && !mOverlayData) {
                    cTop = computeHookViewTopPosition(mSlipOffset)
                            + mSlippingView.getMeasuredHeight();
                }
            }

            int cLeft = pLeft + lp.leftMargin;
            int cBottom = cHeight + cTop;
            int cRight = cLeft + childView.getMeasuredWidth();

            childView.layout(cLeft, cTop, cRight, cBottom);
        }

        if (mPrimaryLayout) {
            refreshObscuredViewVisibility();
        }
        setParallaxForOnGoingSlipOffset();
        mPrimaryLayout = false;
    }

    @Override
    public boolean onTouchEvent(MotionEvent ev) {
        mHookHelper.computeInputTouchEvent(ev);
        return true;
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        setHookState(getHookState());
        if (h != oldh) {
            mPrimaryLayout = true;
        }
    }

    private int computeHookViewTopPosition(float slideOffset) {
        int slidingViewHeight = 0;
        int result = 0;
        if (mSlippingView != null) {
            slidingViewHeight = mSlippingView.getMeasuredHeight();
        }
        int slidePixelOffset = (int) (mSlipRange * slideOffset);
        if (mIsSlippingUp) {
            result = getMeasuredHeight() - getPaddingBottom() - mBoardHeight
                    - slidePixelOffset;
        } else {
            result = getPaddingTop() - slidingViewHeight + mBoardHeight
                    + slidePixelOffset;
        }
        return result;
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        final int action = MotionEventCompat.getActionMasked(ev);

        if (!isEnabled() || !isTouchEnabled()
                || (mIsCanNotDrag && action != MotionEvent.ACTION_DOWN)) {
            mHookHelper.cancel();
            return super.onInterceptTouchEvent(ev);
        }

        if (action == MotionEvent.ACTION_CANCEL
                || action == MotionEvent.ACTION_UP) {
            mHookHelper.cancel();
            return false;
        }

        final float x = ev.getX();
        final float y = ev.getY();

        mSeekBarView.getLocationOnScreen(mSeekBarCoordinate);
        mSongsQueueView.getLocationOnScreen(mSongsQueueCoordinate);
        if(mIsQueueEnabled && (y > mSongsQueueCoordinate[1])
                           && (y < mSeekBarCoordinate[1] || mIsLandscape)) {
           return false;
        }
        switch (action) {
        case MotionEvent.ACTION_DOWN: {
            mIsCanNotDrag = false;
            mDefaultXMotion = x;
            mDefaultYMotion = y;
            break;
        }

        case MotionEvent.ACTION_MOVE: {
            final float adx = Math.abs(x - mDefaultXMotion);
            final float ady = Math.abs(y - mDefaultYMotion);
            final int dragSlop = mHookHelper.getTouchableGradient();

            if (mIsHookViewUsingTouchEvents && adx > dragSlop && ady < dragSlop) {
                return super.onInterceptTouchEvent(ev);
            }

            if ((ady > dragSlop && adx > ady)
                    || !isDragViewUnder((int) mDefaultXMotion,
                            (int) mDefaultYMotion)) {
                mHookHelper.cancel();
                mIsCanNotDrag = true;
                return false;
            }
            break;
        }
        }

        return mHookHelper.decodeTouchEvents(ev);
    }

    private boolean isDragViewUnder(int x, int y) {
        if (mSlipView != null) {
            int[] disPos = new int[2];
            int[] topPos = new int[2];
            mSlipView.getLocationOnScreen(disPos);
            this.getLocationOnScreen(topPos);
            int posX = topPos[0] + x;
            int posY = topPos[1] + y;
            return posY >= disPos[1] && posX < disPos[0] + mSlipView.getWidth()
                    && posX >= disPos[0]
                    && posY < disPos[1] + mSlipView.getHeight();
        }
        return false;
    }

    private float computeSlideOffset(int topPosition) {
        final int topBoundCollapsed = computeHookViewTopPosition(0);
        return (mIsSlippingUp ? (float) (topBoundCollapsed - topPosition)
                / mSlipRange : (float) (topPosition - topBoundCollapsed)
                / mSlipRange);
    }

    public BoardState getHookState() {
        return mSlipState;
    }

    public void setHookState(BoardState state) {
        if (state == null || state == BoardState.DRAGGING) {
           return;
        }
        if (!isEnabled() || (mPrimaryLayout && mSlippingView == null))
            return;

        if (mPrimaryLayout) {
            mSlipState = state;
        } else {
           switch (state) {
            case ANCHORED:
                smoothSlipTo(mMainStayPoint, 0, false);
                break;
            case COLLAPSED:
                smoothSlipTo(0, 0, false);
                break;
            case EXPANDED:
                smoothSlipTo(1.0f, 0, false);
                break;
            case HIDDEN:
                int newTop = computeHookViewTopPosition(0.0f)
                        + (mIsSlippingUp ? +mBoardHeight : -mBoardHeight);
                smoothSlipTo(computeSlideOffset(newTop), 0, false);
                break;
            }
        }
    }

    @SuppressLint("NewApi")
    private void setParallaxForOnGoingSlipOffset() {
        if (mParOffset > 0) {
            int mainViewOffset = getOnGoingParalaxOffset();
            mInitialView.setTranslationY(mainViewOffset);

        }
    }

    private void onHookSlipped(int newTop) {
        mPreviuosStoppedDraggingSlipState = mSlipState;
        mSlipState = BoardState.DRAGGING;
        mSlipOffset = computeSlideOffset(newTop);
        setParallaxForOnGoingSlipOffset();
        dispatchOnPanelSlide(mSlippingView);
        MusicScreenParams lp = (MusicScreenParams) mInitialView
                .getLayoutParams();
        int defaultHeight = getHeight() - getPaddingBottom() - getPaddingTop()
                - mBoardHeight;

        if (mSlipOffset <= 0 && !mOverlayData) {
            lp.height = mIsSlippingUp ? (newTop - getPaddingBottom())
                    : (getHeight() - getPaddingBottom()
                            - mSlippingView.getMeasuredHeight() - newTop);
            mInitialView.requestLayout();
        } else if (lp.height != defaultHeight && !mOverlayData) {
            lp.height = defaultHeight;
            mInitialView.requestLayout();
        }
    }

    @Override
    protected boolean drawChild(Canvas canvas, View child, long drawingTime) {
        boolean result;
        final int save = canvas.save();

        if (mSlippingView != child) {
            canvas.getClipBounds(mSampleRectangle);
            if (!mOverlayData) {
                if (mIsSlippingUp) {
                    mSampleRectangle.bottom = Math.max(mSampleRectangle.bottom,
                            mSlippingView.getTop());
                } else {
                    mSampleRectangle.top = Math.min(mSampleRectangle.top,
                            mSlippingView.getBottom());
                }
            }
            if (mHookPanel) {
                canvas.clipRect(mSampleRectangle);
            }

            result = super.drawChild(canvas, child, drawingTime);

            if (mFadeColourCovered != 0 && mSlipOffset > 0) {
                final int baseAlpha = (mFadeColourCovered & 0xff000000) >>> 24;
                final int imag = (int) (baseAlpha * mSlipOffset);
                final int color = imag << 24 | (mFadeColourCovered & 0xffffff);
                mFadePaintOccupied.setColor(color);
                canvas.drawRect(mSampleRectangle, mFadePaintOccupied);
            }
        } else {
            try {
                result = super.drawChild(canvas, child, drawingTime);
            } catch (Exception e) {
                Log.d(TAG, ": drawChild    e="+e);
                result = false;
            }
        }

        canvas.restoreToCount(save);

        return result;
    }

    @Override
    protected void onDraw(Canvas canvas) {
        try {
            super.onDraw(canvas);
        } catch (Exception e) {
            Log.d(TAG, ": onDraw  e="+e);
        }
    }

    @Override
    protected ViewGroup.LayoutParams generateDefaultLayoutParams() {
        return new MusicScreenParams();
    }

    @Override
    public ViewGroup.LayoutParams generateLayoutParams(AttributeSet attrs) {
        return new MusicScreenParams(getContext(), attrs);
    }

    boolean smoothSlipTo(float slideOffset, int velocity, boolean hasFastScroll) {
        if (!isEnabled() || mSlippingView.getVisibility() != VISIBLE) {
            return false;
        }

        int panelTop = computeHookViewTopPosition(slideOffset);
        if (mHookHelper.smoothSlideViewTo(hasFastScroll,
                mSlippingView.getLeft(), panelTop, mSlippingView)) {
            makeViewsVisible();
            ViewCompat.postInvalidateOnAnimation(this);
            return true;
        }
        return false;
    }

    @Override
    public void computeScroll() {
        if (mHookHelper != null && mHookHelper.continueSettling(true)) {
            if (!isEnabled()) {
                mHookHelper.cancelAndQuit();
                return;
            }

            ViewCompat.postInvalidateOnAnimation(this);
        }
    }

    @Override
    public void draw(Canvas c) {
        super.draw(c);

        if (mTraceDrawable != null) {
            final int right = mSlippingView.getRight();
            final int top;
            final int bottom;
            if (mIsSlippingUp) {
                top = mSlippingView.getTop() - mTraceHeight;
                bottom = mSlippingView.getTop();
            } else {
                top = mSlippingView.getBottom();
                bottom = mSlippingView.getBottom() + mTraceHeight;
            }
            final int left = mSlippingView.getLeft();
            mTraceDrawable.setBounds(left, top, right, bottom);
            mTraceDrawable.draw(c);
        }
    }

    protected boolean isScrollable(View view, boolean checkV, int dx, int x,
            int y) {
        boolean matched = false;
        if (view instanceof ViewGroup) {
            ViewGroup viewGroup = (ViewGroup) view;
            int scrolY = view.getScrollY();
            int scrolX = view.getScrollX();
            int tot = viewGroup.getChildCount();
            int k = tot - 1;
            while (k >= ZERO) {
                final View child = viewGroup.getChildAt(k);
                if (x + scrolX >= child.getLeft()
                        && x + scrolX < child.getRight()
                        && y + scrolY >= child.getTop()
                        && y + scrolY < child.getBottom()
                        && isScrollable(child, true, dx,
                                x + scrolX - child.getLeft(), y + scrolY
                                        - child.getTop())) {
                    matched = true;
                }
                k--;
            }
        }
        if (matched)
            return matched;
        return checkV && ViewCompat.canScrollHorizontally(view, -dx);
    }

    @Override
    protected ViewGroup.LayoutParams generateLayoutParams(
            ViewGroup.LayoutParams p) {
        if (p instanceof MarginLayoutParams)
            return new MusicScreenParams((MarginLayoutParams) p);
        else
            return new MusicScreenParams(p);
    }

    @Override
    protected boolean checkLayoutParams(ViewGroup.LayoutParams p) {
        return super.checkLayoutParams(p) && p instanceof MusicScreenParams;
    }

    @Override
    public Parcelable onSaveInstanceState() {
        HookInstanceSavedState ss = new HookInstanceSavedState(
                super.onSaveInstanceState());
        if (BoardState.DRAGGING == mSlipState)
            ss.mSlipState = mPreviuosStoppedDraggingSlipState;
        else
            ss.mSlipState = mSlipState;
        return ss;
    }

    @Override
    public void onRestoreInstanceState(Parcelable state) {
        HookInstanceSavedState ss = (HookInstanceSavedState) state;
        super.onRestoreInstanceState(ss.getSuperState());
        mSlipState = ss.mSlipState;
    }

    private class DragHelperCallback extends
            MusicPanelViewDragHelper.MusicListener {
        @Override
        public void onPanelCaptured(View capturedChild, int activePointerId) {
            makeViewsVisible();
        }

        @Override
        public boolean captureView(View child, int pointerId) {
            if (mIsCanNotDrag || BoardState.HIDDEN == mSlipState) {
                return false;
            } else {
                return child == mSlippingView;
            }
        }

        @Override
        public int getPanelVerticalDragRange(View child) {
            return mSlipRange;
        }

        @Override
        public void onPanelDragStateChanged(int state) {
            if (mHookHelper.getPanelDragState() == MusicPanelViewDragHelper.STATE_IDLE) {
                mSlipOffset = computeSlideOffset(mSlippingView.getTop());
                setParallaxForOnGoingSlipOffset();

                if (mSlipOffset == ZERO) {
                    if (mSlipState != BoardState.COLLAPSED) {
                        mSlipState = BoardState.COLLAPSED;
                        if (mHookSlipListener != null) {
                            mHookSlipListener.onViewClosed(mSlippingView);
                        }
                        sendAccessibilityEvent(AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED);
                    }
                } else if (mSlipOffset == 1) {
                    if (mSlipState != BoardState.EXPANDED) {
                        refreshObscuredViewVisibility();
                        mSlipState = BoardState.EXPANDED;
                        if (mHookSlipListener != null) {
                            mHookSlipListener.onViewOpened(mSlippingView);
                        }
                        sendAccessibilityEvent(AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED);
                    }
                } else if (mSlipOffset < 0) {
                    mSlipState = BoardState.HIDDEN;
                    mSlippingView.setVisibility(View.INVISIBLE);
                    if (mHookSlipListener != null) {
                        mHookSlipListener.onViewBackStacked(mSlippingView);
                    }
                    sendAccessibilityEvent(AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED);
                } else if (mSlipState != BoardState.ANCHORED) {
                    refreshObscuredViewVisibility();
                    mSlipState = BoardState.ANCHORED;
                    if (mHookSlipListener != null) {
                        mHookSlipListener.onViewMainLined(mSlippingView);
                    }
                    sendAccessibilityEvent(AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED);
                }
            }
        }

        @Override
        public int clampPanelPositionVertical(View child, int top, int dy) {
            int collapsedTop = computeHookViewTopPosition(0.f);
            int expandedTop = computeHookViewTopPosition(1.0f);
            if (!mIsSlippingUp) {
                return Math.min(Math.max(top, collapsedTop), expandedTop);
            }
            return Math.min(Math.max(top, expandedTop), collapsedTop);
        }

        @Override
        public void onPanelPositionChanged(View changedView, int left, int top,
                int dx, int dy) {
            onHookSlipped(top);
            invalidate();
        }

        @Override
        public void onPanelReleased(View rChild, float valX, float valY) {

            float direction = mIsSlippingUp ? -valY : valY;

            // if (direction < ZERO) {
            // result = computeHookViewTopPosition(0.0f);
            // } else if (direction > ZERO) {
            // result = computeHookViewTopPosition(1.0f);
            // } else if (mMainStayPoint != 1
            // && mSlipOffset >= (1.f + mMainStayPoint) / 2) {
            // result = computeHookViewTopPosition(1.0f);
            // } else if (mMainStayPoint == 1 && mSlipOffset >= 0.5f) {
            // result = computeHookViewTopPosition(1.0f);
            // } else if (mMainStayPoint != 1 && mSlipOffset >= mMainStayPoint)
            // {
            // result = computeHookViewTopPosition(mMainStayPoint);
            // } else if (mMainStayPoint != 1 && mSlipOffset >= mMainStayPoint /
            // 2) {
            // result = computeHookViewTopPosition(mMainStayPoint);
            // } else {
            // result = computeHookViewTopPosition(0.0f);
            // }
            float parmVal = 0.0f;
            if (direction < ZERO) {
                parmVal = 0.0f;
            } else if (direction > ZERO) {
                parmVal = 1.0f;
            } else if (mMainStayPoint != 1
                    && mSlipOffset >= (1.f + mMainStayPoint) / 2) {
                parmVal = 1.0f;
            } else if (mMainStayPoint == 1 && mSlipOffset >= 0.5f) {
                parmVal = 1.0f;
            } else if (mMainStayPoint != 1 && mSlipOffset >= mMainStayPoint) {
                parmVal = mMainStayPoint;
            } else if (mMainStayPoint != 1 && mSlipOffset >= mMainStayPoint / 2) {
                parmVal = mMainStayPoint;
            } else {
                parmVal = 0.0f;
            }

            mHookHelper.settleCapturedViewAt(rChild.getLeft(),
                    computeHookViewTopPosition(parmVal));
            invalidate();
        }

    }

    public static class MusicScreenParams extends ViewGroup.MarginLayoutParams {
        private static final int[] ATTRS = new int[] { android.R.attr.layout_weight };

        public MusicScreenParams() {
            super(MATCH_PARENT, MATCH_PARENT);
        }

        public MusicScreenParams(Context c, AttributeSet attrs) {
            super(c, attrs);
            final TypedArray a = c.obtainStyledAttributes(attrs, ATTRS);
            a.recycle();
        }

        public MusicScreenParams(int width, int height) {
            super(width, height);
        }

        public MusicScreenParams(MarginLayoutParams start) {
            super(start);
        }

        public MusicScreenParams(MusicScreenParams start) {
            super(start);
        }

        public MusicScreenParams(android.view.ViewGroup.LayoutParams start) {
            super(start);
        }

    }

    static class HookInstanceSavedState extends BaseSavedState {
        BoardState mSlipState;

        HookInstanceSavedState(Parcelable superState) {
            super(superState);
        }

        public static final Parcelable.Creator<HookInstanceSavedState> CREATOR = new Parcelable.Creator<HookInstanceSavedState>() {
            @Override
            public HookInstanceSavedState[] newArray(int size) {
                return new HookInstanceSavedState[size];
            }

            @Override
            public HookInstanceSavedState createFromParcel(Parcel in) {
                return new HookInstanceSavedState(in);
            }

        };

        private HookInstanceSavedState(Parcel in) {
            super(in);
            try {
                mSlipState = Enum.valueOf(BoardState.class, in.readString());
            } catch (IllegalArgumentException ex) {
                mSlipState = BoardState.COLLAPSED;
            }
        }

        @Override
        public void writeToParcel(Parcel out, int flags) {
            super.writeToParcel(out, flags);
            out.writeString(mSlipState.toString());
        }

    }

    @Override
    protected void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        configurationChanged();
    }

    private void configurationChanged() {
        if (getResources().getConfiguration().orientation
                == Configuration.ORIENTATION_LANDSCAPE) {
            mIsLandscape = true;
        } else {
            mIsLandscape = false;
        }
    }
}
