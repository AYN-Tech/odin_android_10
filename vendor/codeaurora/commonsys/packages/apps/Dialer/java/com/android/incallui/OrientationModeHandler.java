/* Copyright (c) 2015 - 2019, The Linux Foundation. All rights reserved.
 *
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
 *
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

package com.android.incallui;

import android.content.pm.ActivityInfo;
import android.os.Bundle;
import android.telecom.VideoProfile;
import com.android.incallui.call.DialerCall;
import com.android.incallui.call.state.DialerCallState;
import com.android.incallui.InCallPresenter.InCallDetailsListener;
import com.android.incallui.InCallPresenter.InCallEventListener;
import com.android.incallui.InCallPresenter.InCallUiListener;
import com.android.incallui.videotech.utils.SessionModificationState;
import org.codeaurora.ims.QtiCallConstants;

/**
 * This class listens to incoming events from the {@class InCallDetailsListener}.
 * When call details change, this class is notified and we parse the extras from the details to
 * figure out if orientation mode has changed and if changed, we call setRequestedOrientation
 * on the activity to set the orientation mode for the device.
 *
 */
public class OrientationModeHandler implements InCallDetailsListener, InCallUiListener,
    PrimaryCallTracker.PrimaryCallChangeListener, InCallEventListener {

    private static OrientationModeHandler sOrientationModeHandler;

    private PrimaryCallTracker mPrimaryCallTracker;

    private int mOrientationMode = QtiCallConstants.ORIENTATION_MODE_UNSPECIFIED;

    private int mVideoState = VideoProfile.STATE_AUDIO_ONLY;

    /**
     * Returns a singleton instance of {@class OrientationModeHandler}
     */
    public static synchronized OrientationModeHandler getInstance() {
        if (sOrientationModeHandler == null) {
            sOrientationModeHandler = new OrientationModeHandler();
        }
        return sOrientationModeHandler;
    }

    /**
     * Private constructor. Must use getInstance() to get this singleton.
     */
    private OrientationModeHandler() {
    }

    /**
     * Handles set up of the {@class OrientationModeHandler}. Registers primary call tracker to
     * listen to call state changes and registers this class to listen to call details changes.
     */
    public void setUp() {
        mPrimaryCallTracker = new PrimaryCallTracker();
        InCallPresenter.getInstance().addListener(mPrimaryCallTracker);
        InCallPresenter.getInstance().addIncomingCallListener(mPrimaryCallTracker);
        InCallPresenter.getInstance().addDetailsListener(this);
        InCallPresenter.getInstance().addInCallUiListener(this);
        InCallPresenter.getInstance().addInCallEventListener(this);
        mPrimaryCallTracker.addListener(this);
    }

    /**
     * Handles tear down of the {@class OrientationModeHandler}. Unregisters primary call tracker
     * from listening to call state changes and unregisters this class from listening to call
     * details changes.
     */
    public void tearDown() {
        InCallPresenter.getInstance().removeListener(mPrimaryCallTracker);
        InCallPresenter.getInstance().removeIncomingCallListener(mPrimaryCallTracker);
        InCallPresenter.getInstance().removeDetailsListener(this);
        InCallPresenter.getInstance().removeInCallUiListener(this);
        if (mPrimaryCallTracker != null) {
            mPrimaryCallTracker.removeListener(this);
            mPrimaryCallTracker = null;
        }
        InCallPresenter.getInstance().removeInCallEventListener(this);
        mOrientationMode = QtiCallConstants.ORIENTATION_MODE_UNSPECIFIED;
    }

    /**
     * Overrides onDetailsChanged method of {@class InCallDetailsListener}. We are
     * notified when call details change and extract the orientation mode from the
     * extras, detect if the mode has changed and set the orientation mode for the device.
     */
    @Override
    public void onDetailsChanged(DialerCall call, android.telecom.Call.Details details) {
        Log.d(this, "onDetailsChanged: - call: " + call + "details: " + details);
        if (details == null) {
          Log.e(this, "onDetailsChanged: details is null");
          return;
        }
        if (!mPrimaryCallTracker.isPrimaryCall(call)) {
          Log.e(this, "onDetailsChanged: call is non-primary call");
          return;
        }
        if (mPrimaryCallTracker != null && !mPrimaryCallTracker.isPrimaryCall(call)) {
          Log.e(this, "onDetailsChanged: call is non-primary call");
          return;
        }
        mayBeUpdateOrientationMode(call, details.getExtras());
    }

    /**
     * This API conveys if incall experience is showing or not.
     *
     * @param showing TRUE if incall experience is showing else FALSE
     */
    @Override
    public void onUiShowing(boolean showing) {
       if (mPrimaryCallTracker == null) {
           Log.e(this, "onUiShowing showing: " + showing + " PrimaryCallTracker is null");
           return;
       }
        DialerCall call = mPrimaryCallTracker.getPrimaryCall();
        Log.d(this, "onUiShowing showing: " + showing + " call = " + call);

        if (!showing || call == null) {
            return;
        }

        mayBeUpdateOrientationMode(call, call.getExtras());
    }

    private void mayBeUpdateOrientationMode(DialerCall call, Bundle extras) {
        final int orientationMode = (extras != null) ? extras.getInt(
                QtiCallConstants.ORIENTATION_MODE_EXTRA_KEY,
                QtiCallConstants.ORIENTATION_MODE_UNSPECIFIED) :
                QtiCallConstants.ORIENTATION_MODE_UNSPECIFIED;

        Log.d(this, "mayBeUpdateOrientationMode : orientationMode: " + orientationMode +
                " mOrientationMode : " + mOrientationMode);
        if (InCallPresenter.getInstance().getActivity() == null) {
            Log.w(this, "mayBeUpdateOrientationMode : InCallActivity is null");
            return;
        }

        final int videoState = call.getVideoState();
        if (videoState != mVideoState) {
            mVideoState = videoState;
            onScreenOrientationChanged(call, getOrientation(call));
        } else if (orientationMode != mOrientationMode && orientationMode !=
                QtiCallConstants.ORIENTATION_MODE_UNSPECIFIED) {
            mOrientationMode = orientationMode;
            onScreenOrientationChanged(call, QtiCallUtils.toScreenOrientation(mOrientationMode));
        }
    }

    /**
     * Handles any screen orientation changes in the call.
     *
     * @param call The call for which orientation changed.
     * @param orientation The new screen orientation of the device
     * {@link ActivityInfo#ScreenOrientation}
     */
    private void onScreenOrientationChanged(DialerCall call, int orientation) {
        Log.d(this, "onScreenOrientationChanged: Call : " + call + " screen orientation = " +
                orientation);
        if (mPrimaryCallTracker != null && !mPrimaryCallTracker.isPrimaryCall(call)) {
            Log.e(this, "Can't set requested orientation on a non-primary call");
            return;
        }
        InCallPresenter.getInstance().setInCallAllowsOrientationChange(orientation);
    }

    /**
     * Returns the current orientation mode based on the receipt of DISPLAY_MODE_EVT from lower
     * layers and whether the call is a video call or not. If we have a video call and we
     * did receive a valid orientation mode, return the corresponding
     * {@link ActivityInfo#ScreenOrientation} else return
     * InCallOrientationEventListener.ACTIVITY_PREFERENCE_ALLOW_ROTATION.
     * If we are in a voice call, return
     * InCallOrientationEventListener.ACTIVITY_PREFERENCE_DISALLOW_ROTATION.
     *
     * @param call The current call.
     */
    public int getOrientation(DialerCall call) {
        // When VT call is put on hold, user is presented with VoLTE UI.
        // Hence, restricting held VT call to change orientation.
        if (isVideoOrUpgrade(call) && (call.getNonConferenceState() != DialerCallState.ONHOLD)
                && (call.getNonConferenceState() != DialerCallState.DISCONNECTED)) {
            return (mOrientationMode == QtiCallConstants.ORIENTATION_MODE_UNSPECIFIED) ?
                    InCallOrientationEventListener.ACTIVITY_PREFERENCE_ALLOW_ROTATION :
                    QtiCallUtils.toScreenOrientation(mOrientationMode);
        } else {
            return InCallOrientationEventListener.ACTIVITY_PREFERENCE_DISALLOW_ROTATION;
        }
    }

    private static boolean isVideoOrUpgrade(DialerCall call) {
      return call != null && (call.isVideoCall() || call.hasSentVideoUpgradeRequest()
          || call.hasReceivedVideoUpgradeRequest());
    }

    @Override
    public void onPrimaryCallChanged(DialerCall call) {
        // If primary call is null, invalidate the cached orientation value
        // Otherwise update the orientation mode in InCallUI.
        if (call == null) {
            mOrientationMode = QtiCallConstants.ORIENTATION_MODE_UNSPECIFIED;
            return;
        }
        mayBeUpdateOrientationMode(call, call.getExtras());
    }

    public boolean isOrientationDynamic() {
        if (mPrimaryCallTracker == null) {
          return false;
        }
        DialerCall call = mPrimaryCallTracker.getPrimaryCall();
        return (call != null && call.isVideoCall()
            && mOrientationMode == QtiCallConstants.ORIENTATION_MODE_DYNAMIC);
    }

    @Override
    public void onSessionModificationStateChange(DialerCall call) {
       Log.v(this,"onSessionModificationStateChange");
       if (call == null) {
         Log.w(this,"Call is null");
         return;
       }
       if (mPrimaryCallTracker != null && mPrimaryCallTracker.isPrimaryCall(call)){
           if (call.getVideoTech().getSessionModificationState()
                  == SessionModificationState.NO_REQUEST) {
              onScreenOrientationChanged(call, getOrientation(call));
          }
       } else {
           Log.w(this,"Primary Call Tracker is null or call is not a primary call");
       }
    }
    @Override
    public void onFullscreenModeChanged(boolean isFullscreenMode) {}

    @Override
    public void onSendStaticImageStateChanged(boolean isEnabled) {}

    @Override
    public void onOutgoingVideoSourceChanged(int videoSource) {}
}
