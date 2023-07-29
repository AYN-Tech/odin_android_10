/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
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

import android.content.Context;
import android.content.Intent;
import android.media.projection.MediaProjectionManager;
import com.android.dialer.common.LogUtil;

public class ScreenShareHelper {

  public static final int REQUEST_MEDIA_PROJECTION = 1;
  public static final int VIDEO_SCREEN_SHARE = 4;

  //Outgoing Video Source
  public static final int NONE = 0;
  public static final int CAMERA = 1;
  public static final int SCREEN = 2;

  private static Intent mPermission = null;
  private static MediaProjectionManager mProjectionManager = null;

  private static InCallActivity getActivity() {
    final InCallActivity inCallActivity = InCallPresenter.getInstance().getActivity();
    if (inCallActivity == null) {
      LogUtil.w("ScreenShareUtil.getActivity", "inCallActivity is null");
      return null;
    }
    return inCallActivity;
  }

  public static MediaProjectionManager getProjectionManager() {
    if (mProjectionManager == null && getActivity() != null) {
      mProjectionManager = (MediaProjectionManager)
        getActivity().getSystemService(Context.MEDIA_PROJECTION_SERVICE);
    }
    return mProjectionManager;
  }

  public static void requestScreenSharePermission() {
    LogUtil.w("ScreenShareUtil.requestScreenSharePermission",
        "requesting screen share");
    if (getProjectionManager() != null && getActivity() != null) {
      getActivity().startActivityForResult(
        getProjectionManager().createScreenCaptureIntent(),
        REQUEST_MEDIA_PROJECTION);
    } else {
      LogUtil.w("ScreenShareUtil.requestScreenSharePermission()",
          "screen share not available");
    }
  }

  public static void onPermissionChanged(Intent data) {
    mPermission = data;
  }

  public static Intent getPermission() {
    return mPermission;
  }

  public static boolean screenShareRequested() {
    return mPermission != null;
  }
}
