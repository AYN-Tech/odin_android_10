/* Copyright (c) 2020, The Linux Foundation. All rights reserved.
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

import com.android.dialer.common.LogUtil;
import com.android.incallui.call.CallList;
import com.android.incallui.call.DialerCall;
import com.android.incallui.call.DialerCallListener;
import com.android.incallui.call.state.DialerCallState;

public class AnswerUtils {

  private AnswerUtils() {}

  public static void disconnectAllAndAnswer(int videoState) {
    CallList callList = InCallPresenter.getInstance().getCallList();
    if (callList == null || callList.getIncomingCall() == null) {
      LogUtil.i("AnswerUtils.diconnectAllAndAnswer", "no valid call found");
      return;
    }
    for (DialerCall currentCall : callList.getAllCalls()) {
      if (DialerCallState.isConnectingOrConnected(currentCall.getState()) &&
          !(currentCall.getState() == DialerCallState.INCOMING)) {
        currentCall.setReleasedByAnsweringSecondCall(true);
        currentCall.addListener(new AnswerOnDisconnected(currentCall, videoState));
        currentCall.disconnect();
      }
    }
  }

  private static class AnswerOnDisconnected implements DialerCallListener {

    private final DialerCall disconnectingCall;
    private final int videoState;

    AnswerOnDisconnected(DialerCall disconnectingCall, int videoState) {
      this.disconnectingCall = disconnectingCall;
      this.videoState = videoState;
    }

    @Override
    public void onDialerCallDisconnect() {
      // Only answer when all the calls except Incoming call is disconnected.
      if (CallList.getInstance().hasIncomingCallOnly()) {
        LogUtil.i(
          "AnswerUtils.AnswerOnDisconnected", "call disconnected, answering new call");
          CallList callList = InCallPresenter.getInstance().getCallList();
          if (callList != null && callList.getIncomingCall() != null) {
            callList.getIncomingCall().answer(videoState);
          }
        }
      disconnectingCall.removeListener(this);
    }

    @Override
    public void onDialerCallUpdate() {}

    @Override
    public void onDialerCallChildNumberChange() {}

    @Override
    public void onDialerCallLastForwardedNumberChange() {}

    @Override
    public void onDialerCallUpgradeToVideo() {}

    @Override
    public void onDialerCallSessionModificationStateChange() {}

    @Override
    public void onWiFiToLteHandover() {}

    @Override
    public void onHandoverToWifiFailure() {}

    @Override
    public void onInternationalCallOnWifi() {}

    @Override
    public void onEnrichedCallSessionUpdate() {}

    @Override
    public void onSuplServiceMessage(String suplNotificationMessage) {}
  }
}
