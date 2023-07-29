/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.

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

import android.content.Context;
import android.support.v4.view.MotionEventCompat;
import android.support.v4.view.VelocityTrackerCompat;
import android.support.v4.view.ViewCompat;
import android.support.v4.widget.ScrollerCompat;
import android.view.MotionEvent;
import android.view.VelocityTracker;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.view.animation.Interpolator;
import java.util.Arrays;

public class MusicPanelViewDragHelper {
    private static final String TAG = "MusicPanelViewDragHelper";
    public static final int STATE_POSITIONING = 2;
    public static final int LEFT_CORNER = 1 << 0;
    public static final int RIGHT_CORNER = 1 << 1;
    public static final int TOP_CORNER = 1 << 2;
    public static final int BOTTOM_CORNER = 1 << 3;
    public static final int UNWANTED_POINTER = -1;
    public static final int STATE_IDLE = 0;
    public static final int STATE_SLIPPING = 1;
    public static final int ALL_CORNERS = LEFT_CORNER | TOP_CORNER
            | RIGHT_CORNER | BOTTOM_CORNER;
    private static final int BASE_POSITIONING_DURATION = 256;
    private static final int MAX_SETTLE_DURATION = 600;
    private static final int FASTSCROLL_DURATION = 100;
    public static final int HORIZONTALMOTION = 1 << 0;
    public static final int VERTICALMOTION = 1 << 1;
    public static final int ALLDIRECTION = HORIZONTALMOTION | VERTICALMOTION;

    private static final int CORNER_SIZE = 20;

    private int mSlipState;
    private int mTouchableGradient;

    private int mOnGoingPointerId = UNWANTED_POINTER;
    private float[] mInitialMotionX;
    private float[] mInitialMotionY;
    private float[] mLastMotionX;
    private float[] mLastMotionY;
    private int[] mFirstCornerTouched;
    private int[] mCornerSlipViewInProgress;
    private int[] mCornerHookLocked;
    private int mPointersDown;

    private VelocityTracker mVelocityTracker;
    private float mMaxVelocity;
    private float mMinVelocity;

    private int mEdgeSize;
    private int mCheckingCorners;

    private ScrollerCompat mScrollableView;

    private final MusicListener mCallback;

    private View mCalculatedLayout;
    private boolean mReleaseInProgress;

    private final ViewGroup mPrimaryLayoutView;
    private static final int ZERO = 0;

    public static abstract class MusicListener {
        public void onPanelDragStateChanged(int state) {
        }

        public void onPanelPositionChanged(View view, int dxPos, int dyPos,
                int left, int top) {
        }

        public void onPanelCaptured(View view, int id) {
        }

        public void onPanelReleased(View view, float xvel, float yvel) {
        }

        public void onPanelEdgeTouched(int flags, int id) {
        }

        public boolean onPanelEdgeLock(int flags) {
            return false;
        }

        public void onPanelDragStarted(int flags, int id) {
        }

        public int getPanelOrderedChildIndex(int index) {
            return index;
        }

        public int getPanelHorizontalDragRange(View view) {
            return 0;
        }

        public int getPanelVerticalDragRange(View view) {
            return 0;
        }

        public abstract boolean captureView(View view, int id);

        public int clampPanelPositionHorizontal(View view, int left, int dx) {
            return 0;
        }

        public int clampPanelPositionVertical(View view, int top, int dy) {
            return 0;
        }
    }

    private static final Interpolator mInterpolator = new Interpolator() {
        public float getInterpolation(float t) {
            t -= 1.0f;
            return 1.0f + (t * t * t * t * t);
        }
    };

    private final Runnable mSetRunnable = new Runnable() {
        public void run() {
            setSlpPosition(STATE_IDLE);
        }
    };

    public static MusicPanelViewDragHelper create(ViewGroup parent,
            MusicListener cb) {
        return new MusicPanelViewDragHelper(parent.getContext(), parent, cb);
    }

    public static MusicPanelViewDragHelper create(ViewGroup parent,
            float sensitivity, MusicListener cb) {
        final MusicPanelViewDragHelper h = create(parent, cb);
        h.mTouchableGradient = (int) (h.mTouchableGradient * (1 / sensitivity));
        return h;
    }

    private MusicPanelViewDragHelper(Context context, ViewGroup parent,
            MusicListener cb) {
        if (parent == null) {
            throw new IllegalArgumentException("view should not be null");
        }
        if (cb == null) {
            throw new IllegalArgumentException("callback should not be null");
        }

        mPrimaryLayoutView = parent;
        mCallback = cb;

        final ViewConfiguration conf = ViewConfiguration.get(context);
        final float den = context.getResources().getDisplayMetrics().density;
        mEdgeSize = (int) (CORNER_SIZE * den + 0.5f);

        mTouchableGradient = conf.getScaledTouchSlop();
        mMaxVelocity = conf.getScaledMaximumFlingVelocity();
        mMinVelocity = conf.getScaledMinimumFlingVelocity();
        mScrollableView = ScrollerCompat.create(context, mInterpolator);
    }

    public void setPanelMinVelocity(float mv) {
        mMinVelocity = mv;
    }

    private boolean captureViewAt(boolean speedScrollingExist,
            int xPosvelocityArg, int yPosvelocityArg, int topArg, int leftArg) {

        int pointTopPos = mCalculatedLayout.getTop();
        int pointLfPos = mCalculatedLayout.getLeft();
        int yPosCalculated = topArg - pointTopPos;
        int xPosCalculated = leftArg - pointLfPos;
        if (xPosCalculated == 0) {
            if (yPosCalculated == 0) {
                mScrollableView.abortAnimation();
                setSlpPosition(STATE_IDLE);
                return false;
            }
        }
        int dur = 0;
        if (speedScrollingExist) {
            dur = FASTSCROLL_DURATION;
        } else {
            dur = calculateSettleDuration(mCalculatedLayout, xPosCalculated,
                    yPosCalculated, xPosvelocityArg, yPosvelocityArg);
        }
        mScrollableView.startScroll(pointLfPos, pointTopPos, xPosCalculated,
                yPosCalculated, dur);
        setSlpPosition(STATE_POSITIONING);
        return true;
    }

    protected void cancelAndQuit() {
        cancel();
        if (mSlipState == STATE_POSITIONING) {
            mScrollableView.abortAnimation();
            final int newY = mScrollableView.getCurrY();
            final int newX = mScrollableView.getCurrX();
            mCallback.onPanelPositionChanged(mCalculatedLayout, newX
                    - (mScrollableView.getCurrX()),
                    newY - (mScrollableView.getCurrY()), newX, newY);
        }
        setSlpPosition(STATE_IDLE);
    }

    public float getPanelMinVelocity() {
        return mMinVelocity;
    }

    public int getPanelDragState() {
        return mSlipState;
    }

    public boolean settleCapturedViewAt(int left, int top) {
        if (!mReleaseInProgress) {
            throw new IllegalStateException("cannot set view");
        }

        return captureViewAt(false, (int) VelocityTrackerCompat.getXVelocity(
                mVelocityTracker, mOnGoingPointerId),
                (int) VelocityTrackerCompat.getYVelocity(mVelocityTracker,
                        mOnGoingPointerId), top, left);
    }

    public void setPanelEdgeTrackingEnabled(int ef) {
        mCheckingCorners = ef;
    }

    private void cacheLastMotion(MotionEvent me) {
        final int pCount = MotionEventCompat.getPointerCount(me);
        int k = 0;
        while (k < pCount) {
            int pointerId = MotionEventCompat.getPointerId(me, k);
            float x = MotionEventCompat.getX(me, k);
            float y = MotionEventCompat.getY(me, k);
            if (mLastMotionY != null && mLastMotionX != null) {
                mLastMotionY[pointerId] = y;
                mLastMotionX[pointerId] = x;
            }
            k++;
        }
    }

    public void getCalculatedChildLayout(View childLayout, int currentPointerId) {
        if (childLayout.getParent() != mPrimaryLayoutView) {
            throw new IllegalArgumentException(
                    "captureChildView: parameter must be a descendant "
                            + "of the ViewDragHelper's tracked parent view ("
                            + mPrimaryLayoutView + ")");
        }

        mCalculatedLayout = childLayout;
        mOnGoingPointerId = currentPointerId;
        mCallback.onPanelCaptured(childLayout, currentPointerId);
        setSlpPosition(STATE_SLIPPING);
    }

    public int getTouchableGradient() {
        return mTouchableGradient;
    }

    public void cancel() {
        mOnGoingPointerId = UNWANTED_POINTER;
        if (mInitialMotionX != null) {
            mPointersDown = 0;
            Arrays.fill(mInitialMotionY, 0);
            Arrays.fill(mLastMotionX, 0);
            Arrays.fill(mInitialMotionX, 0);
            Arrays.fill(mCornerSlipViewInProgress, 0);
            Arrays.fill(mLastMotionY, 0);
            Arrays.fill(mFirstCornerTouched, 0);
            Arrays.fill(mCornerHookLocked, 0);
        }
        if (mVelocityTracker != null) {
            mVelocityTracker.recycle();
            mVelocityTracker = null;
        }
    }

    private int calculateSettleDuration(View child, int dx, int dy, int xvel,
            int yvel) {
        xvel = getClampMag(xvel, (int) mMinVelocity, (int) mMaxVelocity);
        yvel = getClampMag(yvel, (int) mMinVelocity, (int) mMaxVelocity);
        int absYVel = Math.abs(yvel);
        int absDy = Math.abs(dy);
        int absXVel = Math.abs(xvel);
        int absDx = Math.abs(dx);
        int addedVel = absXVel + absYVel;
        int addedDistance = absDx + absDy;
        final int xdur = (dx == ZERO) ? dx : getAxisDuration(dx, xvel,
                mCallback.getPanelHorizontalDragRange(child));
        final int ydur = (dy == ZERO) ? dy : getAxisDuration(dy, yvel,
                mCallback.getPanelVerticalDragRange(child));
        float weightx = 0L;
        if (xvel == ZERO) {
            weightx = (float) absDx / addedDistance;
        } else {
            weightx = (float) absXVel / addedVel;
        }
        float weighty = 0L;
        if (yvel == ZERO) {
            weighty = (float) absDy / addedDistance;
        } else {
            weighty = (float) absYVel / addedVel;
        }
        return (int) (xdur * weightx + ydur * weighty);
    }

    boolean checkCalculateViewForSlip(View toCapture, int pointerId) {
        if (toCapture == mCalculatedLayout && mOnGoingPointerId == pointerId) {
            return true;
        }
        if (toCapture != null && mCallback.captureView(toCapture, pointerId)) {
            mOnGoingPointerId = pointerId;
            getCalculatedChildLayout(toCapture, pointerId);
            return true;
        }
        return false;
    }

    public boolean smoothSlideViewTo(boolean fastScrollExist, int finalLeft,
            int finalTop, View child) {

        mOnGoingPointerId = UNWANTED_POINTER;
        mCalculatedLayout = child;
        return captureViewAt(fastScrollExist, ZERO, ZERO, finalTop, finalLeft);
    }

    private int getAxisDuration(int delta, int velValue, int mRange) {
        if (delta == 0) {
            return 0;
        }

        int w = mPrimaryLayoutView.getWidth();
        int hw = w / 2;
        float dRatio = Math.min(1f, (float) Math.abs(delta) / w);
        dRatio -= 0.5f;
        dRatio *= 0.3f * Math.PI / 2.0f;
        dRatio = (float) Math.sin(dRatio);

        float dist = hw + hw * dRatio;

        int dur = ((Math.abs(velValue) > ZERO) ? (4 * Math.round(1000 * Math
                .abs(dist / Math.abs(velValue)))) : ((int) ((((float) Math
                .abs(delta) / mRange) + 1) * BASE_POSITIONING_DURATION)));
        if (dur < MAX_SETTLE_DURATION)
            return dur;

        return MAX_SETTLE_DURATION;
    }

    private int getClampMag(int val, int min, int max) {
        int absVal = Math.abs(val);
        if (absVal < min) {
            return ZERO;
        } else if (absVal > max) {
            if (val > ZERO)
                return max;
            else
                return -max;
        }
        return val;
    }

    private float getClampMag(float val, float min, float max) {
        float absVal = Math.abs(val);
        if (absVal < min) {
            return ZERO;
        } else if (absVal > max) {
            if (val > ZERO)
                return max;
            else
                return -max;
        }
        return val;
    }

    public boolean continueSettling(boolean deferCallbacks) {
        // Make sure, there is a captured view
        if (mCalculatedLayout == null) {
            return false;
        }
        if (mSlipState == STATE_POSITIONING) {
            boolean keepGoing = mScrollableView.computeScrollOffset();
            final int x = mScrollableView.getCurrX();
            final int dx = x - mCalculatedLayout.getLeft();
            final int y = mScrollableView.getCurrY();
            final int dy = y - mCalculatedLayout.getTop();

            if (dx != 0) {
                mCalculatedLayout.offsetLeftAndRight(dx);
            }
            if (dy != 0) {
                mCalculatedLayout.offsetTopAndBottom(dy);
            }

            mCallback.onPanelPositionChanged(mCalculatedLayout, x, y, dx, dy);

            if (keepGoing && x == mScrollableView.getFinalX()
                    && y == mScrollableView.getFinalY()) {
                mScrollableView.abortAnimation();
                keepGoing = mScrollableView.isFinished();
            }

            if (!keepGoing) {
                if (deferCallbacks) {
                    mPrimaryLayoutView.post(mSetRunnable);
                } else {
                    setSlpPosition(STATE_IDLE);
                }
            }
        }

        return mSlipState == STATE_POSITIONING;
    }

    private void dispatchViewReleased(float xvel, float yvel) {
        mReleaseInProgress = true;
        mCallback.onPanelReleased(mCalculatedLayout, xvel, yvel);
        mReleaseInProgress = false;

        if (mSlipState == STATE_SLIPPING) {
            setSlpPosition(STATE_IDLE);
        }
    }

    private void doCleanMotHistory(int pId) {
        if (mInitialMotionX == null) {
            return;
        }
        mLastMotionY[pId] = 0;
        mFirstCornerTouched[pId] = 0;
        mCornerSlipViewInProgress[pId] = 0;
        mInitialMotionX[pId] = 0;
        mInitialMotionY[pId] = 0;
        mCornerHookLocked[pId] = 0;
        mPointersDown &= ~(1 << pId);
        mLastMotionX[pId] = 0;
    }

    private void cacheIntMotion(float x, float y, int pId) {
        if (mInitialMotionX == null || mInitialMotionX.length <= pId) {
            float[] ix = new float[pId + 1];
            float[] iy = new float[pId + 1];
            int[] iit = new int[pId + 1];
            int[] edl = new int[pId + 1];
            int[] edip = new int[pId + 1];
            float[] lx = new float[pId + 1];
            float[] ly = new float[pId + 1];

            if (mInitialMotionX != null) {
                System.arraycopy(mInitialMotionX, ZERO, ix, ZERO,
                        mInitialMotionX.length);
                System.arraycopy(mInitialMotionY, ZERO, iy, ZERO,
                        mInitialMotionY.length);
                System.arraycopy(mLastMotionX, ZERO, lx, ZERO,
                        mLastMotionX.length);
                System.arraycopy(mLastMotionY, ZERO, ly, ZERO,
                        mLastMotionY.length);
                System.arraycopy(mFirstCornerTouched, ZERO, iit, ZERO,
                        mFirstCornerTouched.length);
                System.arraycopy(mCornerSlipViewInProgress, ZERO, edip, ZERO,
                        mCornerSlipViewInProgress.length);
                System.arraycopy(mCornerHookLocked, ZERO, edl, ZERO,
                        mCornerHookLocked.length);
            }

            mFirstCornerTouched = iit;
            mCornerSlipViewInProgress = edip;
            mLastMotionX = lx;
            mLastMotionY = ly;
            mCornerHookLocked = edl;
            mInitialMotionX = ix;
            mInitialMotionY = iy;
        }

        mInitialMotionY[pId] = mLastMotionY[pId] = y;
        mInitialMotionX[pId] = mLastMotionX[pId] = x;
        mFirstCornerTouched[pId] = getCornersTouched((int) x, (int) y);
        mPointersDown |= 1 << pId;
    }

    void setSlpPosition(int s) {
        if (mSlipState != s) {
            mSlipState = s;
            mCallback.onPanelDragStateChanged(s);
            if (s == STATE_IDLE) {
                mCalculatedLayout = null;
            }
        }
    }

    boolean checkToScroll(View v, boolean checkView, int dx, int dy, int x,
            int y) {
        if (v instanceof ViewGroup) {
            final ViewGroup group = (ViewGroup) v;
            final int scrlX = v.getScrollX();
            final int scrlY = v.getScrollY();
            int i = (group.getChildCount()) - 1;
            while (i >= 0) {
                final View child = group.getChildAt(i);
                if (x + scrlX >= child.getLeft()
                        && x + scrlX < child.getRight()
                        && y + scrlY >= child.getTop()
                        && y + scrlY < child.getBottom()
                        && checkToScroll(child, true, dx, dy,
                                x + scrlX - child.getLeft(),
                                y + scrlY - child.getTop())) {
                    return true;
                }
                i--;
            }
        }
        return checkView
                && (ViewCompat.canScrollHorizontally(v, -dx) || ViewCompat
                        .canScrollVertically(v, -dy));
    }

    private void actDecodeTouchDown(MotionEvent mv) {
        final int pId = MotionEventCompat.getPointerId(mv, 0);
        cacheIntMotion(mv.getX(), mv.getY(), pId);

        final View v = calculateMarginTopLayoutWithin((int) mv.getX(),
                (int) mv.getY());
        if (v == mCalculatedLayout && mSlipState == STATE_POSITIONING) {
            checkCalculateViewForSlip(v, pId);
        }

        int t = mFirstCornerTouched[pId];
        if ((t & mCheckingCorners) != 0) {
            mCallback.onPanelEdgeTouched(t & mCheckingCorners, pId);
        }
    }

    private void actDecodeTouchMove(MotionEvent mv) {
        int pCount = MotionEventCompat.getPointerCount(mv);
        for (int i = 0; i < pCount && mInitialMotionX != null
                && mInitialMotionY != null; i++) {
            final int pId = MotionEventCompat.getPointerId(mv, i);
            if (pId >= mInitialMotionX.length || pId >= mInitialMotionY.length) {
                continue;
            }
            final float x = MotionEventCompat.getX(mv, i);
            final float y = MotionEventCompat.getY(mv, i);
            final float dx = x - mInitialMotionX[pId];
            final float dy = y - mInitialMotionY[pId];

            cacheNewCornerViews(dx, dy, pId);
            if (mSlipState == STATE_SLIPPING) {
                break;
            }

            final View v = calculateMarginTopLayoutWithin(
                    (int) mInitialMotionX[pId], (int) mInitialMotionY[pId]);
            if (v != null && canTouchGradient(v, dx, dy)
                    && checkCalculateViewForSlip(v, pId)) {
                break;
            }
        }
        cacheLastMotion(mv);
    }

    private void actDecodeTouchPtrDown(MotionEvent mv) {
        int index = MotionEventCompat.getActionIndex(mv);
        int pId = MotionEventCompat.getPointerId(mv, index);
        final float x = MotionEventCompat.getX(mv, index);
        final float y = MotionEventCompat.getY(mv, index);

        cacheIntMotion(x, y, pId);

        if (mSlipState == STATE_IDLE) {
            final int t = mFirstCornerTouched[pId];
            if ((t & mCheckingCorners) != 0) {
                mCallback.onPanelEdgeTouched(t & mCheckingCorners, pId);
            }
        } else if (mSlipState == STATE_POSITIONING) {
            View v = calculateMarginTopLayoutWithin((int) x, (int) y);
            if (v == mCalculatedLayout) {
                checkCalculateViewForSlip(v, pId);
            }
        }
    }

    private void actDecodeTouchPtrUp(MotionEvent mv) {
        int index = MotionEventCompat.getActionIndex(mv);
        final int pointerId = MotionEventCompat.getPointerId(mv, index);
        if (mInitialMotionX != null) {
            doCleanMotHistory(pointerId);
        }
    }

    public boolean decodeTouchEvents(MotionEvent mv) {
        final int action = MotionEventCompat.getActionMasked(mv);
        if (action == MotionEvent.ACTION_DOWN) {
            cancel();
        }
        if (mVelocityTracker == null) {
            mVelocityTracker = VelocityTracker.obtain();
        }
        mVelocityTracker.addMovement(mv);
        switch (action) {
        case MotionEvent.ACTION_DOWN: {
            actDecodeTouchDown(mv);
            break;
        }
        case MotionEvent.ACTION_MOVE: {
            actDecodeTouchMove(mv);
            break;
        }
        case MotionEventCompat.ACTION_POINTER_DOWN: {
            actDecodeTouchPtrDown(mv);
            break;
        }
        case MotionEventCompat.ACTION_POINTER_UP: {
            actDecodeTouchPtrUp(mv);
            break;
        }
        case MotionEvent.ACTION_UP:
        case MotionEvent.ACTION_CANCEL: {
            cancel();
            break;
        }
        }
        return mSlipState == STATE_SLIPPING;
    }

    private void actComputeTouchDown(MotionEvent mv) {
        int pId = MotionEventCompat.getPointerId(mv, ZERO);
        View v = calculateMarginTopLayoutWithin((int) mv.getX(),
                (int) mv.getY());

        cacheIntMotion(mv.getX(), mv.getY(), pId);
        checkCalculateViewForSlip(v, pId);
        if ((mFirstCornerTouched[pId] & mCheckingCorners) != 0) {
            mCallback.onPanelEdgeTouched(mFirstCornerTouched[pId]
                    & mCheckingCorners, pId);
        }
    }

    private void actComputeTouchPtrDown(MotionEvent mv) {
        int index = MotionEventCompat.getActionIndex(mv);
        int pId = MotionEventCompat.getPointerId(mv, index);
        float x = MotionEventCompat.getX(mv, index);
        float y = MotionEventCompat.getY(mv, index);

        cacheIntMotion(x, y, pId);

        if (mSlipState == STATE_IDLE) {
            final View v = calculateMarginTopLayoutWithin((int) x, (int) y);
            checkCalculateViewForSlip(v, pId);
            if ((mFirstCornerTouched[pId] & mCheckingCorners) != 0) {
                mCallback.onPanelEdgeTouched(mFirstCornerTouched[pId]
                        & mCheckingCorners, pId);
            }
        } else if (checkLayoutWithin(mCalculatedLayout, (int) x, (int) y)) {
            checkCalculateViewForSlip(mCalculatedLayout, pId);
        }
    }

    private void actComputeTouchMove(MotionEvent mv) {
        if (mSlipState == STATE_SLIPPING) {
            int i = MotionEventCompat.findPointerIndex(mv, mOnGoingPointerId);
            int idx = (int) (MotionEventCompat.getX(mv, i) - mLastMotionX[mOnGoingPointerId]);
            int idy = (int) (MotionEventCompat.getY(mv, i) - mLastMotionY[mOnGoingPointerId]);
            slipTo(mCalculatedLayout.getLeft() + idx,
                    mCalculatedLayout.getTop() + idy, idx, idy);
            cacheLastMotion(mv);
        } else {
            int pCount = MotionEventCompat.getPointerCount(mv);
            for (int i = 0; i < pCount; i++) {
                int pId = MotionEventCompat.getPointerId(mv, i);
                float x = MotionEventCompat.getX(mv, i);
                float y = MotionEventCompat.getY(mv, i);
                float dx = x - mInitialMotionX[pId];
                float dy = y - mInitialMotionY[pId];
                cacheNewCornerViews(dx, dy, pId);
                if (mSlipState == STATE_SLIPPING) {
                    break;
                }
                final View toCapture = calculateMarginTopLayoutWithin((int) x,
                        (int) y);
                if (canTouchGradient(toCapture, dx, dy)
                        && checkCalculateViewForSlip(toCapture, pId)) {
                    break;
                }
            }
            cacheLastMotion(mv);
        }
    }

    private void actComputeTouchPtrUp(MotionEvent mv) {
        final int pointerId = MotionEventCompat.getPointerId(mv,
                MotionEventCompat.getActionIndex(mv));
        if (mSlipState == STATE_SLIPPING && pointerId == mOnGoingPointerId) {
            int newPointer = UNWANTED_POINTER;
            final int pCount = MotionEventCompat.getPointerCount(mv);
            for (int k = 0; k < pCount; k++) {
                final int id = MotionEventCompat.getPointerId(mv, k);
                if (id == mOnGoingPointerId) {
                    continue;
                }
                if (calculateMarginTopLayoutWithin(
                        (int) MotionEventCompat.getX(mv, k),
                        (int) MotionEventCompat.getY(mv, k)) == mCalculatedLayout
                        && checkCalculateViewForSlip(mCalculatedLayout, id)) {
                    newPointer = mOnGoingPointerId;
                    break;
                }
            }
            if (newPointer == UNWANTED_POINTER) {
                mVelocityTracker.computeCurrentVelocity(1000, mMaxVelocity);
                final float xvelocity = getClampMag(VelocityTrackerCompat.getXVelocity(
                        mVelocityTracker, mOnGoingPointerId),
                        mMinVelocity, mMaxVelocity);
                final float yvelocity = getClampMag(VelocityTrackerCompat.getYVelocity(
                        mVelocityTracker, mOnGoingPointerId),
                        mMinVelocity, mMaxVelocity);
                dispatchViewReleased(xvelocity, yvelocity);
            }
        }
        doCleanMotHistory(pointerId);
    }

    public void computeInputTouchEvent(MotionEvent mv) {
        int action = MotionEventCompat.getActionMasked(mv);

        if (action == MotionEvent.ACTION_DOWN) {
            cancel();
        }

        if (mVelocityTracker == null) {
            mVelocityTracker = VelocityTracker.obtain();
        }
        mVelocityTracker.addMovement(mv);

        switch (action) {
        case MotionEvent.ACTION_DOWN: {
            actComputeTouchDown(mv);

            break;
        }

        case MotionEventCompat.ACTION_POINTER_DOWN: {
            actComputeTouchPtrDown(mv);
            break;
        }

        case MotionEvent.ACTION_MOVE: {
            actComputeTouchMove(mv);
            break;
        }

        case MotionEventCompat.ACTION_POINTER_UP: {
            actComputeTouchPtrUp(mv);
            break;
        }

        case MotionEvent.ACTION_UP: {
            if (mSlipState == STATE_SLIPPING) {
                mVelocityTracker.computeCurrentVelocity(1000, mMaxVelocity);
                dispatchViewReleased(
                        getClampMag(VelocityTrackerCompat.getXVelocity(
                                mVelocityTracker, mOnGoingPointerId),
                                mMinVelocity, mMaxVelocity),
                        getClampMag(VelocityTrackerCompat.getYVelocity(
                                mVelocityTracker, mOnGoingPointerId),
                                mMinVelocity, mMaxVelocity));
            }
            cancel();
            break;
        }

        case MotionEvent.ACTION_CANCEL: {
            if (mSlipState == STATE_SLIPPING) {
                dispatchViewReleased(0, 0);
            }
            cancel();
            break;
        }
        }
    }

    private void cacheNewCornerViews(float x, float y, int pId) {
        int dStarted = ZERO;
        if (getLatestHook(x, y, pId, LEFT_CORNER)) {
            dStarted = dStarted | LEFT_CORNER;
        }
        if (getLatestHook(y, x, pId, TOP_CORNER)) {
            dStarted = dStarted | TOP_CORNER;
        }
        if (getLatestHook(x, y, pId, RIGHT_CORNER)) {
            dStarted = dStarted | RIGHT_CORNER;
        }
        if (getLatestHook(y, x, pId, BOTTOM_CORNER)) {
            dStarted = dStarted | BOTTOM_CORNER;
        }

        if (dStarted != ZERO) {
            mCornerSlipViewInProgress[pId] = mCornerSlipViewInProgress[pId]
                    | dStarted;
            mCallback.onPanelDragStarted(dStarted, pId);
        }
    }

    private boolean getLatestHook(float delta, float odelta, int pId, int edge) {
        boolean hookFound = false;
        if ((mFirstCornerTouched[pId] & edge) != edge
                || (mCheckingCorners & edge) == ZERO
                || (mCornerHookLocked[pId] & edge) == edge
                || (mCornerSlipViewInProgress[pId] & edge) == edge
                || (Math.abs(delta) <= mTouchableGradient && Math.abs(odelta) <= mTouchableGradient)) {
            hookFound = false;
        } else if (Math.abs(delta) < Math.abs(odelta) * 0.5f
                && mCallback.onPanelEdgeLock(edge)) {
            mCornerHookLocked[pId] = mCornerHookLocked[pId] | edge;
            hookFound = false;
        } else {
            hookFound = (mCornerSlipViewInProgress[pId] & edge) == ZERO
                    && Math.abs(delta) > mTouchableGradient;
        }
        return hookFound;
    }

    private boolean canTouchGradient(View view, float dx, float dy) {
        boolean touched = false;
        if (view != null) {
            if ((mCallback.getPanelHorizontalDragRange(view) > ZERO)
                    && (mCallback.getPanelVerticalDragRange(view) > ZERO)) {
                touched = dx * dx + dy * dy > mTouchableGradient
                        * mTouchableGradient;
            } else if (mCallback.getPanelHorizontalDragRange(view) > ZERO) {
                touched = Math.abs(dx) > mTouchableGradient;
            } else if (mCallback.getPanelVerticalDragRange(view) > ZERO) {
                touched = Math.abs(dy) > mTouchableGradient;
            }
        }
        return touched;
    }

    public boolean canTouchGradient(int directions) {
        int i = ZERO;
        boolean touched = false;
        while (i < mInitialMotionX.length) {
            if (canTouchGradient(directions, i)) {
                touched = true;
                break;
            }
            i++;
        }
        return touched;
    }

    private void slipTo(int left, int top, int dx, int dy) {
        int clampedX = left;
        int clampedY = top;
        int prevLeft = mCalculatedLayout.getLeft();
        int prevTop = mCalculatedLayout.getTop();
        if (dx != ZERO) {
            clampedX = mCallback.clampPanelPositionHorizontal(
                    mCalculatedLayout, left, dx);
            mCalculatedLayout.offsetLeftAndRight(clampedX - prevLeft);
        }
        if (dy != ZERO) {
            clampedY = mCallback.clampPanelPositionVertical(mCalculatedLayout,
                    top, dy);
            mCalculatedLayout.offsetTopAndBottom(clampedY - prevTop);
        }
        if (dx != ZERO || dy != ZERO) {
            int clampedDx = clampedX - prevLeft;
            int clampedDy = clampedY - prevTop;
            mCallback.onPanelPositionChanged(mCalculatedLayout, clampedX,
                    clampedY, clampedDx, clampedDy);
        }
    }

    public boolean canTouchGradient(int directions, int pointerId) {
        boolean touched = false;
        if (((mPointersDown & 1 << pointerId) != ZERO)) {
            final float dx = mLastMotionX[pointerId]
                    - mInitialMotionX[pointerId];
            final float dy = mLastMotionY[pointerId]
                    - mInitialMotionY[pointerId];
            if (((directions & HORIZONTALMOTION) == HORIZONTALMOTION)
                    && ((directions & VERTICALMOTION) == VERTICALMOTION)) {
                touched = dx * dx + dy * dy > mTouchableGradient
                        * mTouchableGradient;
            } else if (((directions & HORIZONTALMOTION) == HORIZONTALMOTION)) {
                touched = Math.abs(dx) > mTouchableGradient;
            } else if ((directions & VERTICALMOTION) == VERTICALMOTION) {
                touched = Math.abs(dy) > mTouchableGradient;
            }
        }
        return touched;
    }

    public boolean isCornerReached(int edges) {
        int i = ZERO;
        boolean reached = false;
        while (i < mFirstCornerTouched.length) {
            if (isCornerReached(edges, i)) {
                reached = true;
                break;
            }
            i++;
        }
        return reached;
    }

    public boolean isCornerReached(int corners, int pointerId) {
        boolean reached = false;
        reached = ((mPointersDown & 1 << pointerId) != ZERO)
                && (mFirstCornerTouched[pointerId] & corners) != ZERO;
        return reached;
    }

    public boolean checkLayoutWithin(View view, int x, int y) {
        boolean withIn = false;
        if (view != null
                && (x >= view.getLeft() && x < view.getRight()
                        && y >= view.getTop() && y < view.getBottom())) {
            withIn = true;
        }
        return withIn;
    }

    public View calculateMarginTopLayoutWithin(int x, int y) {
        int i = mPrimaryLayoutView.getChildCount() - 1;
        View innerView = null;
        while (i >= 0) {
            innerView = mPrimaryLayoutView.getChildAt(mCallback
                    .getPanelOrderedChildIndex(i));
            if (x >= innerView.getLeft() && x < innerView.getRight()
                    && y >= innerView.getTop() && y < innerView.getBottom()) {
                break;
            }
            i--;
        }
        return innerView;
    }

    private int getCornersTouched(int x, int y) {
        int result = 0;

        if (x < mPrimaryLayoutView.getLeft() + mEdgeSize)
            result |= LEFT_CORNER;
        if (y < mPrimaryLayoutView.getTop() + mEdgeSize)
            result |= TOP_CORNER;
        if (x > mPrimaryLayoutView.getRight() - mEdgeSize)
            result |= RIGHT_CORNER;
        if (y > mPrimaryLayoutView.getBottom() - mEdgeSize)
            result |= BOTTOM_CORNER;

        return result;
    }
}
