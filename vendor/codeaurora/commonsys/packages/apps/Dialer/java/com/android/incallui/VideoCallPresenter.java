/*
 * Copyright (C) 2014 The Android Open Source Project
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
 * limitations under the License
 */

package com.android.incallui;

import android.app.Activity;
import android.content.Context;
import android.content.pm.ActivityInfo;
import android.hardware.display.DisplayManager;
import android.hardware.display.VirtualDisplay;
import android.graphics.Point;
import android.media.projection.MediaProjection;
import android.media.projection.MediaProjectionManager;
import android.net.Uri;
import android.os.Handler;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.telecom.Connection.VideoProvider;
import android.telecom.InCallService.VideoCall;
import android.telecom.VideoProfile;
import android.telecom.VideoProfile.CameraCapabilities;
import android.util.DisplayMetrics;
import android.view.Surface;
import android.view.SurfaceView;
import com.android.dialer.common.Assert;
import com.android.dialer.common.LogUtil;
import com.android.dialer.configprovider.ConfigProviderComponent;
import com.android.dialer.util.PermissionsUtil;
import com.android.incallui.QtiCallUtils;
import com.android.incallui.InCallPresenter.InCallDetailsListener;
import com.android.incallui.InCallPresenter.InCallOrientationListener;
import com.android.incallui.InCallPresenter.InCallStateListener;
import com.android.incallui.InCallPresenter.IncomingCallListener;
import com.android.incallui.call.CallList;
import com.android.incallui.call.DialerCall;
import com.android.incallui.call.DialerCall.CameraDirection;
import com.android.incallui.call.InCallVideoCallCallbackNotifier;
import com.android.incallui.call.InCallVideoCallCallbackNotifier.SurfaceChangeListener;
import com.android.incallui.call.state.DialerCallState;
import com.android.incallui.call.InCallVideoCallCallbackNotifier.VideoEventListener;
import com.android.incallui.util.AccessibilityUtil;
import com.android.incallui.video.protocol.VideoCallScreen;
import com.android.incallui.video.protocol.VideoCallScreenDelegate;
import com.android.incallui.videosurface.protocol.VideoSurfaceDelegate;
import com.android.incallui.videosurface.protocol.VideoSurfaceTexture;
import com.android.incallui.videotech.utils.SessionModificationState;
import com.android.incallui.videotech.utils.VideoUtils;
import java.util.Objects;

import org.codeaurora.ims.ImsScreenShareListenerBase;
import org.codeaurora.ims.ImsScreenShareManager;
import org.codeaurora.ims.QtiImsException;
import org.codeaurora.ims.QtiImsExtConnector;
import org.codeaurora.ims.QtiImsExtManager;
import org.codeaurora.ims.utils.QtiImsExtUtils;

/**
 * Logic related to the {@link VideoCallScreen} and for managing changes to the video calling
 * surfaces based on other user interface events and incoming events from the {@class
 * VideoCallListener}.
 *
 * <p>When a call's video state changes to bi-directional video, the {@link
 * com.android.incallui.VideoCallPresenter} performs the following negotiation with the telephony
 * layer:
 *
 * <ul>
 *   <li>{@code VideoCallPresenter} creates and informs telephony of the display surface.
 *   <li>{@code VideoCallPresenter} creates the preview surface.
 *   <li>{@code VideoCallPresenter} informs telephony of the currently selected camera.
 *   <li>Telephony layer sends {@link CameraCapabilities}, including the dimensions of the video for
 *       the current camera.
 *   <li>{@code VideoCallPresenter} adjusts size of the preview surface to match the aspect ratio of
 *       the camera.
 *   <li>{@code VideoCallPresenter} informs telephony of the new preview surface.
 * </ul>
 *
 * <p>When downgrading to an audio-only video state, the {@code VideoCallPresenter} nulls both
 * surfaces.
 */
public class VideoCallPresenter
    implements IncomingCallListener,
        InCallOrientationListener,
        InCallStateListener,
        InCallDetailsListener,
        SurfaceChangeListener,
        InCallPresenter.InCallEventListener,
        VideoCallScreenDelegate,
        CallList.Listener,
        VideoEventListener,
        PictureModeHelper.Listener {

  private static boolean isVideoMode = false;

  private final Handler handler = new Handler();
  private VideoCallScreen videoCallScreen;

  /** The current context. */
  private Context context;

  /** The call the video surfaces are currently related to */
  private DialerCall primaryCall;
  /**
   * The {@link VideoCall} used to inform the video telephony layer of changes to the video
   * surfaces.
   */
  private VideoCall videoCall;
  /** Determines if the current UI state represents a video call. */
  private int currentVideoState;
  /** DialerCall's current state */
  private int currentCallState = DialerCallState.INVALID;
  /** Determines the device orientation (portrait/lanscape). */
  private int deviceOrientation = InCallOrientationEventListener.SCREEN_ORIENTATION_UNKNOWN;
  /** Tracks the state of the preview surface negotiation with the telephony layer. */
  private int previewSurfaceState = PreviewSurfaceState.NONE;
  /**
   * Determines whether video calls should automatically enter full screen mode after {@link
   * #autoFullscreenTimeoutMillis} milliseconds.
   */
  private boolean isAutoFullscreenEnabled = false;
  /**
   * Determines the number of milliseconds after which a video call will automatically enter
   * fullscreen mode. Requires {@link #isAutoFullscreenEnabled} to be {@code true}.
   */
  private int autoFullscreenTimeoutMillis = 0;
  /**
   * Determines if the countdown is currently running to automatically enter full screen video mode.
   */
  private boolean autoFullScreenPending = false;

  /** Stores current orientation mode for primary call.*/
  private int currentOrientationMode = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;

  /** Whether if the call is remotely held. */
  private boolean isRemotelyHeld = false;

  /**
   * Cache the size set in the "local_preview_surface_size" settings db property
   */
  private Point mFixedPreviewSurfaceSize;
  // Holds TRUE if default image should be used as static image else holds FALSE
  private static boolean sUseDefaultImage = false;
  // Holds TRUE if static image needs to be transmitted instead of video preview stream
  private static boolean sShallTransmitStaticImage = false;

  // Screen Share Query
  private static final int NO_PENDING_REQUEST = -1;
  private static final int REQUEST_TO_START = 0;
  private static final int REQUEST_TO_STOP = 1;

  private static int mScreenShareQuery = NO_PENDING_REQUEST;

  private static PictureModeHelper mPictureModeHelper;

  /**
   * Determines if the incoming video is available. If the call session resume event has been
   * received (i.e PLAYER_START has been received from lower layers), incoming video is
   * available. If the call session pause event has been received (i.e PLAYER_STOP has been
   * received from lower layers), incoming video is not available.
   */
  private static boolean mIsIncomingVideoAvailable = false;

  /**
   * Runnable which is posted to schedule automatically entering fullscreen mode. Will not auto
   * enter fullscreen mode if the dialpad is visible (doing so would make it impossible to exit the
   * dialpad).
   */
  private Runnable autoFullscreenRunnable =
      new Runnable() {
        @Override
        public void run() {
          if (autoFullScreenPending
              && !InCallPresenter.getInstance().isDialpadVisible()
              && isVideoMode) {

            LogUtil.v("VideoCallPresenter.mAutoFullScreenRunnable", "entering fullscreen mode");
            InCallPresenter.getInstance().setFullScreen(true);
            autoFullScreenPending = false;
          } else {
            LogUtil.v(
                "VideoCallPresenter.mAutoFullScreenRunnable",
                "skipping scheduled fullscreen mode.");
          }
        }
      };

  private boolean isVideoCallScreenUiReady;
  private VirtualDisplay mVirtualDisplay = null;
  private ImsScreenShareManager mImsScreenShareManager = null;
  private int mDisplayDpi;
  private MediaProjection mMediaProjection;
  private QtiImsExtConnector mQtiImsExtConnector;
  private QtiImsExtManager mQtiImsExtManager = null;

  /* ImsScreenShareListenerBase instance to handle screen share response */
  private ImsScreenShareListenerBase mImsScreenShareListener =
      new ImsScreenShareListenerBase() {

     /* Handle screen share response */
     @Override
     public void onRecordingSurfaceChanged(int phoneId, Surface surface, int width, int height) {
       LogUtil.i("VideoCallPresenter.onRecordingSurfaceChanged", "surface: " + surface);
       if (mScreenShareQuery == REQUEST_TO_START && surface != null) {
         setupVirtualDisplay(width, height, surface);
       } else if (mScreenShareQuery == REQUEST_TO_STOP && surface == null) {
         ScreenShareHelper.onPermissionChanged(null);
         enableCamera(primaryCall, isCameraRequired());
         clearScreenShareStates();
       } else {
         LogUtil.e("VideoCallPresenter.processRecordingSurfaceChanged",
         "mismatch in expected surface from lower layer");
       }
       mScreenShareQuery = NO_PENDING_REQUEST;
     }
  };

  private void maybeCreateQtiImsExtConnector(Context context) {
    try {
      mQtiImsExtConnector = new QtiImsExtConnector(context,
          new QtiImsExtConnector.IListener() {
            @Override
            public void onConnectionAvailable(QtiImsExtManager qtiImsExtManager) {
              mQtiImsExtManager = qtiImsExtManager;
              setScreenShareListener();
            }
            @Override
            public void onConnectionUnavailable() {
              mQtiImsExtManager = null;
            }
          });
      mQtiImsExtConnector.connect();
    } catch (QtiImsException e) {
      LogUtil.e("BottomSheetHelper.createQtiImsExtConnector",
          "Unable to create QtiImsExtConnector");
    }
  }

  private void setupVirtualDisplay(int width, int height, Surface surface) {
    LogUtil.i("VideoCallPresenter.setupVirtualDisplay", " width: " + width + " height: " + height);
      if (ScreenShareHelper.getProjectionManager() != null) {
        mMediaProjection = ScreenShareHelper.getProjectionManager().getMediaProjection(
                               Activity.RESULT_OK,
                               ScreenShareHelper.getPermission());
        mVirtualDisplay = mMediaProjection.createVirtualDisplay("ScreenCapture", width, height,
                              mDisplayDpi, DisplayManager.VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR,
                              surface, null, null);
     }
   }

  private boolean isCameraRequired(int videoState, int sessionModificationState) {
    return isCameraRequired(videoState, sessionModificationState, isIncomingVideoCall(primaryCall));
  }

  /*
   * Checks if camera is required
   * This is used in conjunction with enableCamera(DialerCall call, boolean isCameraRequired)
   * By default the cached primaryCall information is used, but in instances where the current call
   * is passed into enableCamera, directly call this API as well.
   *
   * @param videoState The video state of either the current call, or primaryCall
   * @param sessionModificationState The session modification state (current call or primarycall)
   * @param isIncomingVideoCall returns true if incoming video call (current or primaryCall)
   */
  private boolean isCameraRequired(int videoState, int sessionModificationState,
        boolean isIncomingVideoCall) {
    // while in ACTIVE call, if we get an INCOMING call we would persist
    // the same camera state (open or close) of ACTIVE call.
    return !shallTransmitStaticImage() &&
        !ScreenShareHelper.screenShareRequested() &&
        !isModifyToVideoRxType(primaryCall) &&
        (((VideoProfile.isBidirectional(videoState)
        || VideoProfile.isTransmissionEnabled(videoState)) &&
        (!isIncomingVideoCall || QtiImsExtUtils.canAcceptAsOneWayVideo(
                BottomSheetHelper.getInstance().getPhoneId(), context)))
        || (VideoProfile.isAudioOnly(videoState) && isVideoUpgrade(sessionModificationState)));
  }

  private void setPauseImage(VideoCall videoCall) {
    String uriStr = null;
    Uri uri = null;

    if (!QtiImsExtUtils.shallTransmitStaticImage(
            BottomSheetHelper.getInstance().getPhoneId(), context)
        || videoCall == null) {
        return;
    }

    if (shallTransmitStaticImage()) {
        uriStr = sUseDefaultImage ? "" :
            QtiImsExtUtils.getStaticImageUriStr(context.getContentResolver());
    }

    uri = uriStr != null ? Uri.parse(uriStr) : null;
    LogUtil.d("VideoCallPresenter.setPauseImage"," parsed uri = " + uri + " sUseDefaultImage = "
        + sUseDefaultImage);
    videoCall.setPauseImage(uri);
  }

  @Override
  public void setPauseImage() {
    setPauseImage(videoCall);
  }

  @Override
  public boolean shallTransmitStaticImage() {
    return sShallTransmitStaticImage;
  }

  @Override
  public boolean isUseDefaultImage() {
    return sUseDefaultImage;
  }

  @Override
  public void setUseDefaultImage(boolean useDefaultImage) {
    sUseDefaultImage = useDefaultImage;
  }
  /**
   * Determines if the incoming video surface should be shown based on the current videoState and
   * callState. The video surface is shown when incoming video is not paused, the call is active or
   * dialing and video reception is enabled.
   *
   * @param videoState The current video state.
   * @param callState The current call state.
   * @return {@code true} if the incoming video surface should be shown, {@code false} otherwise.
   */
  static boolean showIncomingVideo(int videoState, int callState) {

    boolean isPaused = VideoProfile.isPaused(videoState);
    boolean isCallActive = callState == DialerCallState.ACTIVE;
    // Show incoming Video for dialing calls to support early media
    boolean isCallOutgoingPending =
        DialerCallState.isDialing(callState) || callState == DialerCallState.CONNECTING;

    return !isPaused
        && (isCallActive || isCallOutgoingPending)
        && VideoProfile.isReceptionEnabled(videoState) && mIsIncomingVideoAvailable;
  }

  /**
   * Determines if the outgoing video surface should be shown based on the current videoState. The
   * video surface is shown if video transmission is enabled.
   *
   * @return {@code true} if the the outgoing video surface should be shown, {@code false}
   *     otherwise.
   */
  public static boolean showOutgoingVideo(
      Context context, int videoState, int sessionModificationState) {
    if (!VideoUtils.hasCameraPermissionAndShownPrivacyToast(context)) {
      LogUtil.i("VideoCallPresenter.showOutgoingVideo", "Camera permission is disabled by user.");
      return false;
    }

    return VideoProfile.isTransmissionEnabled(videoState)
        || isVideoUpgrade(sessionModificationState);
  }

  private static boolean showOutgoingVideo(
      Context context, int videoState, int sessionModificationState,
      boolean isModifyToVideoRxType) {
    if (!VideoUtils.hasCameraPermissionAndShownPrivacyToast(context)) {
      LogUtil.i("VideoCallPresenter.showOutgoingVideo", "Camera permission is disabled by user.");
      return false;
    }

    return VideoProfile.isTransmissionEnabled(videoState)
        || (!isModifyToVideoRxType &&
        VideoProfile.isAudioOnly(videoState) &&
        isVideoUpgrade(sessionModificationState));
  }

  private static void updateCameraSelection(DialerCall call) {
    LogUtil.v("VideoCallPresenter.updateCameraSelection", "call=" + call);
    LogUtil.v("VideoCallPresenter.updateCameraSelection", "call=" + toSimpleString(call));

    final DialerCall activeCall = CallList.getInstance().getActiveCall();
    int cameraDir;

    // this function should never be called with null call object, however if it happens we
    // should handle it gracefully.
    if (call == null) {
      cameraDir = CameraDirection.CAMERA_DIRECTION_UNKNOWN;
      LogUtil.e(
          "VideoCallPresenter.updateCameraSelection",
          "call is null. Setting camera direction to default value (CAMERA_DIRECTION_UNKNOWN)");
    }

    // Clear camera direction if this is not a video call.
    else if (isAudioCall(call) && !isVideoUpgrade(call)) {
      cameraDir = CameraDirection.CAMERA_DIRECTION_UNKNOWN;
      call.setCameraDir(cameraDir);
    }

    // If this is a waiting video call, default to active call's camera,
    // since we don't want to change the current camera for waiting call
    // without user's permission.
    else if (isVideoCall(activeCall) && isIncomingVideoCall(call)) {
      cameraDir = activeCall.getCameraDir();
    }

    // Infer the camera direction from the video state and store it,
    // if this is an outgoing video call.
    else if (isOutgoingVideoCall(call) && !isCameraDirectionSet(call)) {
      cameraDir = toCameraDirection(call.getVideoState());
      call.setCameraDir(cameraDir);
    }

    // Use the stored camera dir if this is an outgoing video call for which camera direction
    // is set.
    else if (isOutgoingVideoCall(call)) {
      cameraDir = call.getCameraDir();
    }

    // Infer the camera direction from the video state and store it,
    // if this is an active video call and camera direction is not set.
    else if (isActiveVideoCall(call) && !isCameraDirectionSet(call)) {
      cameraDir = toCameraDirection(call.getVideoState());
      call.setCameraDir(cameraDir);
    }

    // Use the stored camera dir if this is an active video call for which camera direction
    // is set.
    else if (isActiveVideoCall(call)) {
      cameraDir = call.getCameraDir();
    }

    // For all other cases infer the camera direction but don't store it in the call object.
    else {
      cameraDir = toCameraDirection(call.getVideoState());
    }

    LogUtil.i(
        "VideoCallPresenter.updateCameraSelection",
        "setting camera direction to %d, call: %s",
        cameraDir,
        call);
    final InCallCameraManager cameraManager =
        InCallPresenter.getInstance().getInCallCameraManager();
    cameraManager.setUseFrontFacingCamera(
        cameraDir == CameraDirection.CAMERA_DIRECTION_FRONT_FACING);
  }

  private static int toCameraDirection(int videoState) {
    return VideoProfile.isTransmissionEnabled(videoState)
            && !VideoProfile.isBidirectional(videoState)
        ? CameraDirection.CAMERA_DIRECTION_BACK_FACING
        : CameraDirection.CAMERA_DIRECTION_FRONT_FACING;
  }

  private static boolean isCameraDirectionSet(DialerCall call) {
    return isVideoCall(call) && call.getCameraDir() != CameraDirection.CAMERA_DIRECTION_UNKNOWN;
  }

  private static String toSimpleString(DialerCall call) {
    return call == null ? null : call.toSimpleString();
  }

  /**
   * Initializes the presenter.
   *
   * @param context The current context.
   */
  @Override
  public void initVideoCallScreenDelegate(Context context, VideoCallScreen videoCallScreen) {
    this.context = context;
    this.videoCallScreen = videoCallScreen;
    mPictureModeHelper = new PictureModeHelper(context);
    isAutoFullscreenEnabled =
        this.context.getResources().getBoolean(R.bool.video_call_auto_fullscreen);
    autoFullscreenTimeoutMillis =
        this.context.getResources().getInteger(R.integer.video_call_auto_fullscreen_timeout);
    setFixedPreviewSurfaceSize();
  }

  /** Called when the user interface is ready to be used. */
  @Override
  public void onVideoCallScreenUiReady(VideoCallScreen videoCallScreen) {
    LogUtil.v("VideoCallPresenter.onVideoCallScreenUiReady", "");
    Assert.checkState(!isVideoCallScreenUiReady);

    this.videoCallScreen = videoCallScreen;
    deviceOrientation = InCallOrientationEventListener.getCurrentOrientation();

    // Register for call state changes last
    InCallPresenter.getInstance().addListener(this);
    InCallPresenter.getInstance().addDetailsListener(this);
    InCallPresenter.getInstance().addIncomingCallListener(this);
    InCallPresenter.getInstance().addOrientationListener(this);
    // To get updates of video call details changes
    InCallPresenter.getInstance().addInCallEventListener(this);
    InCallPresenter.getInstance().getLocalVideoSurfaceTexture().setDelegate(new LocalDelegate());
    InCallPresenter.getInstance().getRemoteVideoSurfaceTexture().setDelegate(new RemoteDelegate());

    CallList.getInstance().addListener(this);

    // Register for surface and video events from {@link InCallVideoCallListener}s.
    InCallVideoCallCallbackNotifier.getInstance().addSurfaceChangeListener(this);
    mPictureModeHelper.setUp(this);
    currentVideoState = VideoProfile.STATE_AUDIO_ONLY;
    currentCallState = DialerCallState.INVALID;

    InCallPresenter.InCallState inCallState = InCallPresenter.getInstance().getInCallState();
    onStateChange(inCallState, inCallState, CallList.getInstance());
    isVideoCallScreenUiReady = true;
    InCallVideoCallCallbackNotifier.getInstance().addVideoEventListener(this,
        VideoProfile.isVideo(currentVideoState));
  }

  /** Called when the user interface is no longer ready to be used. */
  @Override
  public void onVideoCallScreenUiUnready() {
    LogUtil.v("VideoCallPresenter.onVideoCallScreenUiUnready", "");
    Assert.checkState(isVideoCallScreenUiReady);

    checkForOrientationAllowedChange(primaryCall);
    cancelAutoFullScreen();

    InCallPresenter.getInstance().removeListener(this);
    InCallPresenter.getInstance().removeDetailsListener(this);
    InCallPresenter.getInstance().removeIncomingCallListener(this);
    InCallPresenter.getInstance().removeOrientationListener(this);
    InCallPresenter.getInstance().removeInCallEventListener(this);
    InCallPresenter.getInstance().getLocalVideoSurfaceTexture().setDelegate(null);

    CallList.getInstance().removeListener(this);

    InCallVideoCallCallbackNotifier.getInstance().removeSurfaceChangeListener(this);
    InCallVideoCallCallbackNotifier.getInstance().removeVideoEventListener(this);
    mPictureModeHelper.tearDown(this);

    // Ensure that the call's camera direction is updated (most likely to UNKNOWN). Normally this
    // happens after any call state changes but we're unregistering from InCallPresenter above so
    // we won't get any more call state changes. See a bug.
    if (primaryCall != null) {
      if (!primaryCall.isVideoCall() && isVideoMode()) {
        exitVideoMode();
      }
      maybeUnsetPauseImage();
      updateCameraSelection(primaryCall);
    }
    InCallPresenter.getInstance().enableScreenTimeout(true);

    videoCallScreen = null;
    isVideoCallScreenUiReady = false;
  }

  public static void cleanUp() {
    LogUtil.v("VideoCallPresenter.cleanUp", "");
    sShallTransmitStaticImage = false;
    sUseDefaultImage = false;
    mIsIncomingVideoAvailable = false;
    isVideoMode = false;
  }

  /**
   * Handles clicks on the video surfaces. If not currently in fullscreen mode, will set fullscreen.
   */
  private void onSurfaceClick() {
    LogUtil.i("VideoCallPresenter.onSurfaceClick", "");
    cancelAutoFullScreen();
    if (!InCallPresenter.getInstance().isFullscreen()) {
      InCallPresenter.getInstance().setFullScreen(true);
    } else {
      InCallPresenter.getInstance().setFullScreen(false);
      maybeAutoEnterFullscreen(primaryCall);
      // If Activity is not multiwindow, fullscreen will be driven by SystemUI visibility changes
      // instead. See #onSystemUiVisibilityChange(boolean)

      // TODO (keyboardr): onSystemUiVisibilityChange isn't being called the first time
      // visibility changes after orientation change, so this is currently always done as a backup.
    }
  }

  @Override
  public void onSystemUiVisibilityChange(boolean visible) {
    // If the SystemUI has changed to be visible, take us out of fullscreen mode
    LogUtil.i("VideoCallPresenter.onSystemUiVisibilityChange", "visible: " + visible);
    if (visible) {
      InCallPresenter.getInstance().setFullScreen(false);
      maybeAutoEnterFullscreen(primaryCall);
    }
  }

  @Override
  public VideoSurfaceTexture getLocalVideoSurfaceTexture() {
    return InCallPresenter.getInstance().getLocalVideoSurfaceTexture();
  }

  @Override
  public VideoSurfaceTexture getRemoteVideoSurfaceTexture() {
    return InCallPresenter.getInstance().getRemoteVideoSurfaceTexture();
  }

  @Override
  public void setSurfaceViews(SurfaceView preview, SurfaceView remote) {
    throw Assert.createUnsupportedOperationFailException();
  }

  @Override
  public int getDeviceOrientation() {
    return deviceOrientation;
  }

  /**
   * This should only be called when user approved the camera permission, which is local action and
   * does NOT change any call states.
   */
  @Override
  public void onCameraPermissionGranted() {
    LogUtil.i("VideoCallPresenter.onCameraPermissionGranted", "");
    if (primaryCall == null) {
      LogUtil.w("VideoCallPresenter.onCameraPermissionGranted",
          "Primary call is null. Not enabling camera");
      return;
    }
    PermissionsUtil.setCameraPrivacyToastShown(context);
    enableCamera(primaryCall, isCameraRequired());
    showVideoUi(
        primaryCall.getVideoState(),
        primaryCall.getState(),
        primaryCall.getVideoTech().getSessionModificationState(),
        primaryCall.isRemotelyHeld());
    InCallPresenter.getInstance().getInCallCameraManager().onCameraPermissionGranted();
  }

  /**
   * Called when the user interacts with the UI. If a fullscreen timer is pending then we start the
   * timer from scratch to avoid having the UI disappear while the user is interacting with it.
   */
  @Override
  public void resetAutoFullscreenTimer() {
    if (autoFullScreenPending) {
      LogUtil.i("VideoCallPresenter.resetAutoFullscreenTimer", "resetting");
      handler.removeCallbacks(autoFullscreenRunnable);
      handler.postDelayed(autoFullscreenRunnable, autoFullscreenTimeoutMillis);
    }
  }

  /**
   * Handles incoming calls.
   *
   * @param oldState The old in call state.
   * @param newState The new in call state.
   * @param call The call.
   */
  @Override
  public void onIncomingCall(
      InCallPresenter.InCallState oldState, InCallPresenter.InCallState newState, DialerCall call) {
    // If video call screen ui is already destroyed, this shouldn't be called. But the UI may be
    // updated synchronized by {@link CallCardPresenter#onIncomingCall} before this is called, this
    // could still be called. Thus just do nothing in this case.
    if (!isVideoCallScreenUiReady) {
      LogUtil.i("VideoCallPresenter.onIncomingCall", "UI is not ready");
      return;
    }
    // same logic should happen as with onStateChange()
    onStateChange(oldState, newState, CallList.getInstance());
  }

  /**
   * Handles state changes (including incoming calls)
   *
   * @param newState The in call state.
   * @param callList The call list.
   */
  @Override
  public void onStateChange(
      InCallPresenter.InCallState oldState,
      InCallPresenter.InCallState newState,
      CallList callList) {
    LogUtil.v(
        "VideoCallPresenter.onStateChange",
        "oldState: %s, newState: %s, isVideoMode: %b",
        oldState,
        newState,
        isVideoMode());

    if (newState == InCallPresenter.InCallState.NO_CALLS) {
      if (isVideoMode()) {
        exitVideoMode();
      }

      cleanUp();
      InCallPresenter.getInstance().cleanupSurfaces();
    }

    // Determine the primary active call).
    DialerCall primary = null;

    // Determine the call which is the focus of the user's attention.  In the case of an
    // incoming call waiting call, the primary call is still the active video call, however
    // the determination of whether we should be in fullscreen mode is based on the type of the
    // incoming call, not the active video call.
    DialerCall currentCall = null;

    if (newState == InCallPresenter.InCallState.INCOMING) {
      // We don't want to replace active video call (primary call)
      // with a waiting call, since user may choose to ignore/decline the waiting call and
      // this should have no impact on current active video call, that is, we should not
      // change the camera or UI unless the waiting VT call becomes active.
      primary = callList.getActiveCall();
      currentCall = callList.getIncomingCall();
      if (!isActiveVideoCall(primary)) {
        primary = callList.getIncomingCall();
      }
    } else if (newState == InCallPresenter.InCallState.OUTGOING) {
      currentCall = primary = callList.getOutgoingCall();
    } else if (newState == InCallPresenter.InCallState.PENDING_OUTGOING) {
      currentCall = primary = callList.getPendingOutgoingCall();
    } else if (newState == InCallPresenter.InCallState.INCALL) {
      currentCall = primary = callList.getActiveCall();
    }

    final boolean primaryChanged = !Objects.equals(primaryCall, primary);
    LogUtil.i(
        "VideoCallPresenter.onStateChange",
        "primaryChanged: %b, primary: %s, primaryCall: %s",
        primaryChanged,
        primary,
        primaryCall);
    if (primaryChanged) {
      onPrimaryCallChanged(primary);
    } else if (primaryCall != null) {
      updateVideoCall(primary);
    }
    updateCallCache(primary);

    // If the call context changed, potentially exit fullscreen or schedule auto enter of
    // fullscreen mode.
    // If the current call context is no longer a video call, exit fullscreen mode.
    maybeExitFullscreen(currentCall);
    // Schedule auto-enter of fullscreen mode if the current call context is a video call
    maybeAutoEnterFullscreen(currentCall);
  }

  /**
   * Handles a change to the fullscreen mode of the app.
   *
   * @param isFullscreenMode {@code true} if the app is now fullscreen, {@code false} otherwise.
   */
  @Override
  public void onFullscreenModeChanged(boolean isFullscreenMode) {
    cancelAutoFullScreen();
    if (primaryCall != null) {
      updateFullscreenAndGreenScreenMode(
          primaryCall.getState(), primaryCall.getVideoTech().getSessionModificationState());
    } else {
      updateFullscreenAndGreenScreenMode(
          DialerCallState.INVALID, SessionModificationState.NO_REQUEST);
    }
  }

  /**
   * Handles a change to the video call hide me selection
   *
   * @param shallTransmitStaticImage {@code true} if the app should show static image in preview,
   * {@code false} otherwise.
   */
   @Override
   public void onSendStaticImageStateChanged(boolean shallTransmitStaticImage) {
    LogUtil.d("VideoCallPresenter.onSendStaticImageStateChanged"," shallTransmitStaticImage: "
        + shallTransmitStaticImage + " primaryCall: " + primaryCall);

    sShallTransmitStaticImage = shallTransmitStaticImage;

    if (!isActiveVideoCall(primaryCall)) {
      LogUtil.w("VideoCallPresenter.onSendStaticImageStateChanged",
          " received for non-active video call");
      return;
    }

    if (videoCall == null || videoCallScreen == null) {
      LogUtil.w("VideoCallPresenter.onSendStaticImageStateChanged",
          " mVideoCall/mVideoCallScreen is null");
      return;
    }

    enableCamera(primaryCall, isCameraRequired(currentVideoState,
        SessionModificationState.NO_REQUEST));

    if (shallTransmitStaticImage) {
      // Handle showing static image in preview based on external storage permissions
      videoCallScreen.onRequestReadStoragePermission();
    } else {
      /* When not required to transmit static image, update video ui visibility
         to reflect streaming video in preview */
      showVideoUi(
          currentVideoState,
          currentCallState,
          SessionModificationState.NO_REQUEST,
          false /* isRemotelyHeld */);
      videoCall.setPauseImage(null);
    }
  }

  @Override
  public void onOutgoingVideoSourceChanged(int videoSource) {
    LogUtil.i("VideoCallPresenter.onOutgoingVideoSourceChanged",
        " videoSource = " + videoSource);
    if (videoSource == ScreenShareHelper.SCREEN) {
      if (primaryCall == null) {
        LogUtil.d("VideoCallPresenter.onOutgoingVideoSourceChanged",
            "primaryCall is null");
        return;
      }
      if (primaryCall.isVideoCall()) {
        enterScreenShare();
      }
    } else {
      exitScreenShare();
    }
  }

  private void enterScreenShare() {
    LogUtil.i("VideoCallPresenter.enterScreenShare", "enter screen share");
    if (mQtiImsExtConnector == null) {
      maybeCreateQtiImsExtConnector(context);
    }
    enableCamera(primaryCall, false);
  }

  private void setScreenShareListener() {
     if (mQtiImsExtManager == null) {
       LogUtil.i("VideoCallPresenter.setScreenShareListener",
           "mQtiImsExtManager is null");
       return;
     }
     try {
       mImsScreenShareManager = mQtiImsExtManager.createImsScreenShareManager(
           BottomSheetHelper.getInstance().getPhoneId());
     } catch (QtiImsException e) {
       LogUtil.e("VideoCallPresenter.setScreenShareListener", "exception " + e);
     }
     try {
       LogUtil.i("VideoCallPresenter.setScreenShareListener", "setScreenShareListener");
       mImsScreenShareManager.setScreenShareListener(mImsScreenShareListener);
       startScreenShare();
     } catch (QtiImsException e) {
       LogUtil.e("VideoCallPresenter.setScreenShareListener", "exception " + e);
     }
   }

   /**
    * Start Screen Share by requesting pre configured
    * Surface from vendor,
    * if a valid Surface is returned via callback
    * onRecordingSurfaceChanged(), UI tries to
    * setup virtual display.
    */
   private void startScreenShare() {
     if (videoCallScreen == null) {
       LogUtil.w("VideoCallPresenter.startScreenShare",
           " VideoCallScreen is null");
       return;
     }
     DisplayMetrics metrics = new DisplayMetrics();
     Activity activity = videoCallScreen.getVideoCallScreenFragment().getActivity();
     if (activity == null) {
       LogUtil.w("VideoCallPresenter.startScreenShare", "activity is null");
       return;
     }
     activity.getWindowManager().getDefaultDisplay().getMetrics(metrics);
     mDisplayDpi = metrics.densityDpi;
     try {
       if (mImsScreenShareManager == null) {
         LogUtil.i("VideoCallPresenter.startScreenShare",
             "mImsScreenShareManager is null");
         return;
       }
       LogUtil.i("VideoCallPresenter.startScreenShare", "startScreenShare");
       mImsScreenShareManager.startScreenShare(metrics.widthPixels, metrics.heightPixels);
       mScreenShareQuery = REQUEST_TO_START;
     } catch (QtiImsException e) {
       LogUtil.e("VideoCallPresenter.startScreenShare", "exception " + e);
       clearScreenShareStates();
     }
  }

  /**
   * Stop Screen Share if a null Surface is returned
   * via callback onRecordingSurfaceChanged().
   */
  private void exitScreenShare() {
     try {
       if (mImsScreenShareManager == null) {
         LogUtil.i("VideoCallPresenter.exitScreenShare",
             "mImsScreenShareManager is null");
         return;
       }
       LogUtil.i("VideoCallPresenter.stopScreenShare", "stopScreenShare");
       mImsScreenShareManager.stopScreenShare();
       mScreenShareQuery = REQUEST_TO_STOP;
     } catch (QtiImsException e) {
       LogUtil.e("VideoCallPresenter.stopScreenShare", "exception " + e);
       clearScreenShareStates();
     }
  }

  @Override
  public void onReadStoragePermissionResponse(boolean isGranted) {
    LogUtil.d("VideoCallPresenter.onReadStoragePermissionResponse"," granted = " + isGranted);

    // Use default image when permissions are not granted
    sUseDefaultImage = !isGranted;
    if (!isGranted) {
      QtiCallUtils.displayToast(context, R.string.qti_ims_defaultImage_fallback);
    }

    showVideoUi(
        currentVideoState,
        currentCallState,
        SessionModificationState.NO_REQUEST,
        false /* isRemotelyHeld */);
  }

  private void checkForVideoStateChange(DialerCall call) {
    final boolean shouldShowVideoUi = shouldShowVideoUiForCall(call);
    final boolean hasVideoStateChanged = currentVideoState != call.getVideoState();

    LogUtil.v(
        "VideoCallPresenter.checkForVideoStateChange",
        "shouldShowVideoUi: %b, hasVideoStateChanged: %b, isVideoMode: %b, previousVideoState: %s,"
            + " newVideoState: %s",
        shouldShowVideoUi,
        hasVideoStateChanged,
        isVideoMode(),
        VideoProfile.videoStateToString(currentVideoState),
        VideoProfile.videoStateToString(call.getVideoState()));
    if (!hasVideoStateChanged) {
      return;
    }

    maybeUnsetPauseImage();
    updateCameraSelection(call);

    if (shouldShowVideoUi) {
      adjustVideoMode(call);
    } else if (isVideoMode()) {
      exitVideoMode();
    }
  }

  private void maybeUnsetPauseImage() {
    if (QtiImsExtUtils.shallTransmitStaticImage(
            BottomSheetHelper.getInstance().getPhoneId(), context) &&
        shallTransmitStaticImage() &&
        !isTransmissionEnabled(primaryCall) &&
        videoCall != null) {
      /* Unset the pause image when Tx is disabled for eg. when video call
         that is transmitting static image is downgraded to Rx or to voice */
      videoCall.setPauseImage(null);
    }
  }

  private void checkForCallStateChange(DialerCall call) {
    final boolean shouldShowVideoUi = shouldShowVideoUiForCall(call);
    final boolean hasCallStateChanged =
        currentCallState != call.getState() || isRemotelyHeld != call.isRemotelyHeld();
    isRemotelyHeld = call.isRemotelyHeld();

    LogUtil.v(
        "VideoCallPresenter.checkForCallStateChange",
        "shouldShowVideoUi: %b, hasCallStateChanged: %b, isVideoMode: %b",
        shouldShowVideoUi,
        hasCallStateChanged,
        isVideoMode());

    if (!hasCallStateChanged) {
      return;
    }

    if (shouldShowVideoUi) {
      final InCallCameraManager cameraManager =
          InCallPresenter.getInstance().getInCallCameraManager();

      String prevCameraId = cameraManager.getActiveCameraId();
      updateCameraSelection(call);
      String newCameraId = cameraManager.getActiveCameraId();

      if (!Objects.equals(prevCameraId, newCameraId) && isActiveVideoCall(call)) {
        enableCamera(call, true);
      }
    }

    // Make sure we hide or show the video UI if needed.
    showVideoUi(
        call.getVideoState(),
        call.getState(),
        call.getVideoTech().getSessionModificationState(),
        call.isRemotelyHeld());
  }

  private void onPrimaryCallChanged(DialerCall newPrimaryCall) {
    final boolean shouldShowVideoUi = shouldShowVideoUiForCall(newPrimaryCall);
    final boolean isVideoMode = isVideoMode();

    LogUtil.v(
        "VideoCallPresenter.onPrimaryCallChanged",
        "shouldShowVideoUi: %b, isVideoMode: %b",
        shouldShowVideoUi,
        isVideoMode);

    if (!shouldShowVideoUi && isVideoMode) {
      // Terminate video mode if new primary call is not a video call
      // and we are currently in video mode.
      LogUtil.i("VideoCallPresenter.onPrimaryCallChanged", "exiting video mode...");
      exitVideoMode();
    } else if (shouldShowVideoUi) {
      LogUtil.i("VideoCallPresenter.onPrimaryCallChanged", "entering video mode...");

      checkForOrientationAllowedChange(newPrimaryCall);
      updateCameraSelection(newPrimaryCall);

      if (ScreenShareHelper.screenShareRequested()) {
        LogUtil.i("VideoCallPresenter.onPrimaryCallChanged", "starting screen share");
        enterScreenShare();
      }

      // Existing call is put on hold and new call is in incoming state does mean that
      // user is trying to answer the call
      if (isIncomingVideoCall(newPrimaryCall) &&
          isTransmissionEnabled(primaryCall) &&
          primaryCall.getState() == DialerCallState.ONHOLD) {
        // Close camera on mPrimaryCall
        LogUtil.v("VideoCallPresenter.onPrimaryCallChanged", "closing camera");
        enableCamera(primaryCall, false);
      }
      adjustVideoMode(newPrimaryCall);
    }
  }

  private boolean isVideoMode() {
    return isVideoMode;
  }

  private void updateCallCache(DialerCall call) {
    if (call == null) {
      currentVideoState = VideoProfile.STATE_AUDIO_ONLY;
      currentCallState = DialerCallState.INVALID;
      videoCall = null;
      primaryCall = null;
    } else {
      currentVideoState = call.getVideoState();
      videoCall = call.getVideoCall();
      currentCallState = call.getState();
      primaryCall = call;
    }
  }

  /**
   * Handles changes to the details of the call. The {@link VideoCallPresenter} is interested in
   * changes to the video state.
   *
   * @param call The call for which the details changed.
   * @param details The new call details.
   */
  @Override
  public void onDetailsChanged(DialerCall call, android.telecom.Call.Details details) {
    LogUtil.v(
        "VideoCallPresenter.onDetailsChanged",
        "call: %s, details: %s, primaryCall: %s",
        call,
        details,
        primaryCall);
    if (call == null) {
      return;
    }
    // If the details change is not for the currently active call no update is required.
    if (!call.equals(primaryCall)) {
      LogUtil.v("VideoCallPresenter.onDetailsChanged", "details not for current active call");
      return;
    }

    updateVideoCall(call);

    updateCallCache(call);
  }

  private void updateVideoCall(DialerCall call) {
    checkForVideoCallChange(call);
    checkForVideoStateChange(call);
    checkForCallStateChange(call);
    checkForOrientationAllowedChange(call);
    updateFullscreenAndGreenScreenMode(
        call.getState(), call.getVideoTech().getSessionModificationState());
  }

  private void checkForOrientationAllowedChange(@Nullable DialerCall call) {
    LogUtil.d("VideoCallPresenter.checkForOrientationAllowedChange","call : "+ call +
        " currentOrientationMode = " + currentOrientationMode);
    int orientation = OrientationModeHandler.getInstance().getOrientation(call);
    // Call could be null when video call ended. This check could prevent unwanted orientation
    // change before incall UI gets destroyed.
    if (call != null && orientation != currentOrientationMode &&
        InCallPresenter.getInstance().setInCallAllowsOrientationChange(orientation)) {
      currentOrientationMode = orientation;
    }
  }

  private void updateFullscreenAndGreenScreenMode(
      int callState, @SessionModificationState int sessionModificationState) {
    if (videoCallScreen != null) {
      boolean hasVideoCallSentVideoUpgradeRequest =
          isVideoCall(primaryCall)
          && VideoUtils.hasSentVideoUpgradeRequest(sessionModificationState);

      boolean shouldShowFullscreen = InCallPresenter.getInstance().isFullscreen();

      /*
       * Do not enter green screen mode:
       * 1. For VoLTE to VT-RX upgrade
       * 2. If a video call is waiting for upgrade to video response
       *    for eg. VT->VT-RX/VT-TX or VT-TX/VT-RX->VT etc.,
       * 3. If incoming video is available for dialing call to support
       *    early media
       */
      boolean shouldShowGreenScreen =
          ((callState == DialerCallState.DIALING
              || callState == DialerCallState.CONNECTING) && !mIsIncomingVideoAvailable)
              || callState == DialerCallState.INCOMING
              || (!hasVideoCallSentVideoUpgradeRequest
              && !isModifyToVideoRxType(primaryCall)
              && isVideoUpgrade(sessionModificationState));
      videoCallScreen.updateFullscreenAndGreenScreenMode(
          shouldShowFullscreen, shouldShowGreenScreen);
    }
  }

  @Override
  public boolean isIncomingVideoAvailableForEarlyMedia() {
      return primaryCall != null
          && (primaryCall.getState() == DialerCallState.DIALING
                  || primaryCall.getState() == DialerCallState.CONNECTING)
          && mIsIncomingVideoAvailable;
  }

  /** Checks for a change to the video call and changes it if required. */
  private void checkForVideoCallChange(DialerCall call) {
    final VideoCall videoCall = call.getVideoCall();
    LogUtil.v(
        "VideoCallPresenter.checkForVideoCallChange",
        "videoCall: %s, mVideoCall: %s",
        videoCall,
        this.videoCall);
    if (!Objects.equals(videoCall, this.videoCall)) {
      changeVideoCall(call);
    }
  }

  /**
   * Handles a change to the video call. Sets the surfaces on the previous call to null and sets the
   * surfaces on the new video call accordingly.
   *
   * @param call The new video call.
   */
  private void changeVideoCall(DialerCall call) {
    final VideoCall videoCall = call == null ? null : call.getVideoCall();
    LogUtil.i(
        "VideoCallPresenter.changeVideoCall",
        "videoCall: %s, mVideoCall: %s",
        videoCall,
        this.videoCall);
    final boolean hasChanged = this.videoCall == null && videoCall != null;

    this.videoCall = videoCall;
    if (this.videoCall == null) {
      LogUtil.v("VideoCallPresenter.changeVideoCall", "video call or primary call is null. Return");
      return;
    }

    if (shouldShowVideoUiForCall(call) && hasChanged) {
      adjustVideoMode(call);
    }
  }

  private boolean isCameraRequired() {
    return primaryCall != null
        && isCameraRequired(
            primaryCall.getVideoState(), primaryCall.getVideoTech().getSessionModificationState());
  }

  /**
   * Adjusts the current video mode by setting up the preview and display surfaces as necessary.
   * Expected to be called whenever the video state associated with a call changes (e.g. a user
   * turns their camera on or off) to ensure the correct surfaces are shown/hidden. TODO(vt): Need
   * to adjust size and orientation of preview surface here.
   */
  private void adjustVideoMode(DialerCall call) {
    VideoCall videoCall = call.getVideoCall();
    int newVideoState = call.getVideoState();

    LogUtil.i(
        "VideoCallPresenter.adjustVideoMode",
        "videoCall: %s, videoState: %d",
        videoCall,
        newVideoState);
    if (videoCallScreen == null) {
      LogUtil.e("VideoCallPresenter.adjustVideoMode", "error VideoCallScreen is null so returning");
      return;
    }

    showVideoUi(
        newVideoState,
        call.getState(),
        call.getVideoTech().getSessionModificationState(),
        call.isRemotelyHeld());

    // Communicate the current camera to telephony and make a request for the camera
    // capabilities.
    if (videoCall != null) {
      Surface surface = getRemoteVideoSurfaceTexture().getSavedSurface();
      if (surface != null) {
        LogUtil.v(
            "VideoCallPresenter.adjustVideoMode", "calling setDisplaySurface with: " + surface);
        videoCall.setDisplaySurface(surface);
      }

      Assert.checkState(
          deviceOrientation != InCallOrientationEventListener.SCREEN_ORIENTATION_UNKNOWN);
      videoCall.setDeviceOrientation(deviceOrientation);
      enableCamera(
          call, isCameraRequired(newVideoState, call.getVideoTech().getSessionModificationState(),
                isIncomingVideoCall(call)));

      if (QtiImsExtUtils.shallShowStaticImageUi(BottomSheetHelper.getInstance().getPhoneId(),
          context) && shallTransmitStaticImage()) {
        /* when call downgrades and later upgrades, mVideoCall can be null that prevents setting
           pause image to lower layers so invoke setPauseImage with videocall obj as parameter */
        setPauseImage(videoCall);
      }
    }
    int previousVideoState = currentVideoState;
    currentVideoState = newVideoState;
    isVideoMode = true;

    // adjustVideoMode may be called if we are already in a 1-way video state.  In this case
    // we do not want to trigger auto-fullscreen mode.
    if (!isVideoCall(previousVideoState) && isVideoCall(newVideoState)) {
      maybeAutoEnterFullscreen(call);
    }
  }

  private static boolean shouldShowVideoUiForCall(@Nullable DialerCall call) {
    if (call == null) {
      return false;
    }

    if (isVideoCall(call)) {
      return true;
    }

    if (isVideoUpgrade(call)) {
      return true;
    }

    return false;
  }

  private void enableCamera(DialerCall call, boolean isCameraRequired) {
    LogUtil.v("VideoCallPresenter.enableCamera", "call: %s, enabling: %b", call, isCameraRequired);
    if (call == null) {
      LogUtil.i("VideoCallPresenter.enableCamera", "call is null");
      return;
    }

    boolean hasCameraPermission = VideoUtils.hasCameraPermissionAndShownPrivacyToast(context);
    if (!hasCameraPermission) {
      call.getVideoTech().setCamera(null);
      previewSurfaceState = PreviewSurfaceState.NONE;
      // TODO(wangqi): Inform remote party that the video is off. This is similar to a bug.
    } else if (isCameraRequired) {
      InCallCameraManager cameraManager = InCallPresenter.getInstance().getInCallCameraManager();
      call.getVideoTech().setCamera(cameraManager.getActiveCameraId());
      InCallZoomController.getInstance().onCameraEnabled(cameraManager.getActiveCameraId());
      previewSurfaceState = PreviewSurfaceState.CAMERA_SET;
    } else {
      previewSurfaceState = PreviewSurfaceState.NONE;
      call.getVideoTech().setCamera(null);
      InCallZoomController.getInstance().onCameraEnabled(null);
    }
  }

  /** Exits video mode by hiding the video surfaces and making other adjustments (eg. audio). */
  private void exitVideoMode() {
    LogUtil.i("VideoCallPresenter.exitVideoMode", "");

    showVideoUi(
        VideoProfile.STATE_AUDIO_ONLY,
        DialerCallState.ACTIVE,
        SessionModificationState.NO_REQUEST,
        false /* isRemotelyHeld */);
    enableCamera(primaryCall, false);
    InCallPresenter.getInstance().setFullScreen(false);
    checkForOrientationAllowedChange(primaryCall);
    InCallPresenter.getInstance().enableScreenTimeout(true);

    if (ScreenShareHelper.screenShareRequested()) {
      exitScreenShare();
      clearScreenShareStates();
    }

    if (primaryCall != null &&
        videoCall != null &&
        QtiImsExtUtils.shallTransmitStaticImage(
                BottomSheetHelper.getInstance().getPhoneId(), context) &&
        isTransmissionEnabled(primaryCall) &&
        primaryCall.getState() != DialerCallState.ONHOLD) {
      LogUtil.v("VideoCallPresenter.exitVideoMode", "setPauseImage(null)");
      videoCall.setPauseImage(null);
    }

    isVideoMode = false;
  }

  private void clearScreenShareStates() {
    ScreenShareHelper.onPermissionChanged(null);
    mScreenShareQuery = NO_PENDING_REQUEST;
    mImsScreenShareManager = null;
    if (mVirtualDisplay != null) {
        mVirtualDisplay.release();
        mVirtualDisplay = null;
    }
    if (mMediaProjection != null) {
        mMediaProjection.stop();
        mMediaProjection = null;
    }
    if (mQtiImsExtConnector != null) {
      mQtiImsExtConnector.disconnect();
      mQtiImsExtConnector = null;
      mQtiImsExtManager = null;
    }
  }

  /**
   * Based on the current video state and call state, show or hide the incoming and outgoing video
   * surfaces. The outgoing video surface is shown any time video is transmitting. The incoming
   * video surface is shown whenever the video is un-paused and active.
   *
   * @param videoState The video state.
   * @param callState The call state.
   */
  private void showVideoUi(
      int videoState,
      int callState,
      @SessionModificationState int sessionModificationState,
      boolean isRemotelyHeld) {
    if (videoCallScreen == null) {
      LogUtil.e("VideoCallPresenter.showVideoUi", "videoCallScreen is null returning");
      return;
    }
    boolean isModifyToVideoRxType = isModifyToVideoRxType(primaryCall);
    boolean showIncomingVideo = showIncomingVideo(videoState, callState);
    boolean showOutgoingVideo = showOutgoingVideo(context, videoState, sessionModificationState,
        isModifyToVideoRxType);
    LogUtil.i(
        "VideoCallPresenter.showVideoUi",
        "showIncoming: %b, showOutgoing: %b, isRemotelyHeld: %b shallTransmitStaticImage: %b" +
         " isModifyToVideoRx: %b",
        showIncomingVideo,
        showOutgoingVideo,
        isRemotelyHeld,
        shallTransmitStaticImage(),
        isModifyToVideoRxType);
    updateRemoteVideoSurfaceDimensions();
    videoCallScreen.showVideoViews(showOutgoingVideo && !shallTransmitStaticImage() &&
        !QtiCallUtils.hasVideoCrbtVoLteCall(context), showIncomingVideo, isRemotelyHeld);
    if (BottomSheetHelper.getInstance().canDisablePipMode() && mPictureModeHelper != null) {
      mPictureModeHelper.setPreviewVideoLayoutParams();
    }

    InCallPresenter.getInstance().enableScreenTimeout(VideoProfile.isAudioOnly(videoState));
    updateFullscreenAndGreenScreenMode(callState, sessionModificationState);
    if (BottomSheetHelper.getInstance().canDisablePipMode() && mPictureModeHelper != null) {
      mPictureModeHelper.maybeHideVideoViews();
    }
  }

  /**
   * Handles peer video dimension changes.
   *
   * @param call The call which experienced a peer video dimension change.
   * @param width The new peer video width .
   * @param height The new peer video height.
   */
  @Override
  public void onUpdatePeerDimensions(DialerCall call, int width, int height) {
    LogUtil.i("VideoCallPresenter.onUpdatePeerDimensions", "width: %d, height: %d", width, height);
    if (videoCallScreen == null) {
      LogUtil.e("VideoCallPresenter.onUpdatePeerDimensions", "videoCallScreen is null");
      return;
    }
    if (!call.equals(primaryCall)) {
      LogUtil.e(
          "VideoCallPresenter.onUpdatePeerDimensions", "current call is not equal to primary");
      return;
    }

    // Change size of display surface to match the peer aspect ratio
    if (width > 0 && height > 0 && videoCallScreen != null) {
      getRemoteVideoSurfaceTexture().setSourceVideoDimensions(new Point(width, height));
      videoCallScreen.onRemoteVideoDimensionsChanged();
    }
  }

  /**
   * Handles a change to the dimensions of the local camera. Receiving the camera capabilities
   * triggers the creation of the video
   *
   * @param call The call which experienced the camera dimension change.
   * @param width The new camera video width.
   * @param height The new camera video height.
   */
  @Override
  public void onCameraDimensionsChange(DialerCall call, int width, int height) {
    LogUtil.i(
        "VideoCallPresenter.onCameraDimensionsChange",
        "call: %s, width: %d, height: %d videoCall: %s",
        call,
        width,
        height,
        call.getVideoCall());
    if (videoCallScreen == null) {
      LogUtil.e("VideoCallPresenter.onCameraDimensionsChange", "ui is null");
      return;
    }

    if (!call.equals(primaryCall)) {
      LogUtil.e("VideoCallPresenter.onCameraDimensionsChange", "not the primary call");
      return;
    }

    VideoCall videocall = call.getVideoCall();
    if (shallTransmitStaticImage()) {
      setPauseImage(videocall);
    }

    if (previewSurfaceState == PreviewSurfaceState.NONE) {
      LogUtil.w("VideoCallPresenter.onCameraDimensionsChange",
          "capabilities received when camera is OFF.");
      return;
    }

    previewSurfaceState = PreviewSurfaceState.CAPABILITIES_RECEIVED;
    changePreviewDimensions(width, height);

    // Check if the preview surface is ready yet; if it is, set it on the {@code VideoCall}.
    // If it not yet ready, it will be set when when creation completes.
    Surface surface = getLocalVideoSurfaceTexture().getSavedSurface();
    if (surface != null && videocall != null) {
      previewSurfaceState = PreviewSurfaceState.SURFACE_SET;
      videoCall.setPreviewSurface(surface);
    }
  }

  /**
   * Changes the dimensions of the preview surface.
   *
   * @param width The new width.
   * @param height The new height.
   */
  private void changePreviewDimensions(int width, int height) {
    if (videoCallScreen == null) {
      return;
    }

    Point previewSize = (mFixedPreviewSurfaceSize != null) ? mFixedPreviewSurfaceSize :
        new Point(width, height);
    LogUtil.i("VideoCallPresenter.changePreviewDimensions", "width: %d, height: %d", previewSize.x,
        previewSize.y);
    // Resize the surface used to display the preview video
    getLocalVideoSurfaceTexture().setSurfaceDimensions(previewSize);
    videoCallScreen.onLocalVideoDimensionsChanged();
  }

  /**
   * Handles changes to the device orientation.
   *
   * @param orientation The screen orientation of the device (one of: {@link
   *     InCallOrientationEventListener#SCREEN_ORIENTATION_0}, {@link
   *     InCallOrientationEventListener#SCREEN_ORIENTATION_90}, {@link
   *     InCallOrientationEventListener#SCREEN_ORIENTATION_180}, {@link
   *     InCallOrientationEventListener#SCREEN_ORIENTATION_270}).
   */
  @Override
  public void onDeviceOrientationChanged(int orientation) {
    LogUtil.i(
        "VideoCallPresenter.onDeviceOrientationChanged",
        "orientation: %d -> %d",
        deviceOrientation,
        orientation);
    deviceOrientation = orientation;

    if (videoCallScreen == null) {
      LogUtil.e("VideoCallPresenter.onDeviceOrientationChanged", "videoCallScreen is null");
      return;
    }

    Point previewDimensions = getLocalVideoSurfaceTexture().getSurfaceDimensions();
    if (previewDimensions == null) {
      return;
    }
    LogUtil.v(
        "VideoCallPresenter.onDeviceOrientationChanged",
        "orientation: %d, size: %s",
        orientation,
        previewDimensions);
    changePreviewDimensions(previewDimensions.x, previewDimensions.y);

    videoCallScreen.onLocalVideoOrientationChanged();
  }

  /**
   * Exits fullscreen mode if the current call context has changed to a non-video call.
   *
   * @param call The call.
   */
  protected void maybeExitFullscreen(DialerCall call) {
    if (call == null) {
      return;
    }

    if (!isVideoCall(call) || call.getState() == DialerCallState.INCOMING) {
      LogUtil.i("VideoCallPresenter.maybeExitFullscreen", "exiting fullscreen");
      InCallPresenter.getInstance().setFullScreen(false);
    }
  }

  /**
   * Schedules auto-entering of fullscreen mode. Will not enter full screen mode if any of the
   * following conditions are met: 1. No call 2. DialerCall is not active 3. The current video state
   * is not bi-directional. 4. Already in fullscreen mode 5. In accessibility mode
   *
   * @param call The current call.
   */
  protected void maybeAutoEnterFullscreen(DialerCall call) {
    if (!isAutoFullscreenEnabled) {
      return;
    }

    if (call == null
        || call.getState() != DialerCallState.ACTIVE
        || !isBidirectionalVideoCall(call)
        || InCallPresenter.getInstance().isFullscreen()
        || (context != null && AccessibilityUtil.isTouchExplorationEnabled(context))) {
      // Ensure any previously scheduled attempt to enter fullscreen is cancelled.
      cancelAutoFullScreen();
      return;
    }

    if (autoFullScreenPending) {
      LogUtil.v("VideoCallPresenter.maybeAutoEnterFullscreen", "already pending.");
      return;
    }
    LogUtil.v("VideoCallPresenter.maybeAutoEnterFullscreen", "scheduled");
    autoFullScreenPending = true;
    handler.removeCallbacks(autoFullscreenRunnable);
    handler.postDelayed(autoFullscreenRunnable, autoFullscreenTimeoutMillis);
  }

  /** Cancels pending auto fullscreen mode. */
  @Override
  public void cancelAutoFullScreen() {
    if (!autoFullScreenPending) {
      LogUtil.v("VideoCallPresenter.cancelAutoFullScreen", "none pending.");
      return;
    }
    LogUtil.v("VideoCallPresenter.cancelAutoFullScreen", "cancelling pending");
    autoFullScreenPending = false;
    handler.removeCallbacks(autoFullscreenRunnable);
  }

  @Override
  public boolean shouldShowCameraPermissionToast() {
    if (primaryCall == null) {
      LogUtil.i("VideoCallPresenter.shouldShowCameraPermissionToast", "null call");
      return false;
    }
    if (primaryCall.didShowCameraPermission()) {
      LogUtil.i(
          "VideoCallPresenter.shouldShowCameraPermissionToast", "already shown for this call");
      return false;
    }
    if (!ConfigProviderComponent.get(context)
        .getConfigProvider()
        .getBoolean("camera_permission_dialog_allowed", true)) {
      LogUtil.i("VideoCallPresenter.shouldShowCameraPermissionToast", "disabled by config");
      return false;
    }
    return !VideoUtils.hasCameraPermission(context)
        || !PermissionsUtil.hasCameraPrivacyToastShown(context);
  }

  @Override
  public void onCameraPermissionDialogShown() {
    if (primaryCall != null) {
      primaryCall.setDidShowCameraPermission(true);
    }
  }

  private void updateRemoteVideoSurfaceDimensions() {
    if (videoCallScreen == null) {
      LogUtil.i("VideoCallPresenter.updateRemoteVideoSurfaceDimensions",
          "mVideoCallScreen is null");
      return;
    }

    Activity activity = videoCallScreen.getVideoCallScreenFragment().getActivity();
    if (activity != null) {
      Point screenSize = new Point();
      activity.getWindowManager().getDefaultDisplay().getSize(screenSize);
      getRemoteVideoSurfaceTexture().setSurfaceDimensions(screenSize);
    }
  }

  private static boolean isVideoUpgrade(DialerCall call) {
    return call != null
        && (call.hasSentVideoUpgradeRequest() || call.hasReceivedVideoUpgradeRequest());
  }

  private static boolean isVideoUpgrade(@SessionModificationState int state) {
    return VideoUtils.hasSentVideoUpgradeRequest(state)
        || VideoUtils.hasReceivedVideoUpgradeRequest(state);
  }

  @Override
  public void onIncomingCall(DialerCall call) {}

  @Override
  public void onUpgradeToVideo(DialerCall call) {}

  @Override
  public void onSessionModificationStateChange(DialerCall call) {
    Log.v(this,"onSessionModificationStateChange");
    if (call == null) {
      Log.w(this,"Call is null");
      return;
    }
    if (!call.equals(primaryCall)) {
      LogUtil.e(
          "VideoCallPresenter.onSessionModificationStateChange", "call is not equal to primary");
      return;
    }
    int sessionModifyState = call.getVideoTech().getSessionModificationState();
    if (ScreenShareHelper.screenShareRequested()
        && mScreenShareQuery == REQUEST_TO_START
        && (sessionModifyState == SessionModificationState.REQUEST_FAILED
            || sessionModifyState == SessionModificationState.UPGRADE_TO_VIDEO_REQUEST_TIMED_OUT
            || sessionModifyState == SessionModificationState.UPGRADE_TO_VIDEO_REQUEST_FAILED
            || sessionModifyState == SessionModificationState.REQUEST_REJECTED)) {
      clearScreenShareStates();
    }
  }

  @Override
  public void onCallListChange(CallList callList) {}

  @Override
  public void onDisconnect(DialerCall call) {}

  @Override
  public void onWiFiToLteHandover(DialerCall call) {
    if (videoCallScreen == null) {
      LogUtil.e("VideoCallPresenter.onWiFiToLteHandover", "no UI");
      return;
    }
    if (call.isVideoCall() || call.hasSentVideoUpgradeRequest()) {
      videoCallScreen.onHandoverFromWiFiToLte();
    }
  }

  @Override
  public void onHandoverToWifiFailed(DialerCall call) {}

  @Override
  public void onInternationalCallOnWifi(@NonNull DialerCall call) {}

  @Override
  public void onSuplServiceMessage(String suplNotificationMessage) {}

  private class LocalDelegate implements VideoSurfaceDelegate {
    @Override
    public void onSurfaceCreated(VideoSurfaceTexture videoCallSurface) {
      if (videoCallScreen == null) {
        LogUtil.e("VideoCallPresenter.LocalDelegate.onSurfaceCreated", "no UI");
        return;
      }
      if (videoCall == null) {
        LogUtil.e("VideoCallPresenter.LocalDelegate.onSurfaceCreated", "no video call");
        return;
      }

      // If the preview surface has just been created and we have already received camera
      // capabilities, but not yet set the surface, we will set the surface now.
      if (previewSurfaceState == PreviewSurfaceState.CAPABILITIES_RECEIVED) {
        previewSurfaceState = PreviewSurfaceState.SURFACE_SET;
        videoCall.setPreviewSurface(videoCallSurface.getSavedSurface());
      } else if (previewSurfaceState == PreviewSurfaceState.NONE && isCameraRequired()) {
        enableCamera(primaryCall, true);
      }
    }

    @Override
    public void onSurfaceReleased(VideoSurfaceTexture videoCallSurface) {
      if (videoCall == null) {
        LogUtil.e("VideoCallPresenter.LocalDelegate.onSurfaceReleased", "no video call");
        return;
      }

      videoCall.setPreviewSurface(null);
      enableCamera(primaryCall, false);
    }

    @Override
    public void onSurfaceDestroyed(VideoSurfaceTexture videoCallSurface) {
      if (videoCall == null) {
        LogUtil.e("VideoCallPresenter.LocalDelegate.onSurfaceDestroyed", "no video call");
        return;
      }

      boolean isChangingConfigurations = InCallPresenter.getInstance().isChangingConfigurations();
      if (!isChangingConfigurations) {
        enableCamera(primaryCall, false);
      } else {
        LogUtil.i(
            "VideoCallPresenter.LocalDelegate.onSurfaceDestroyed",
            "activity is being destroyed due to configuration changes. Not closing the camera.");
      }
    }

    @Override
    public void onSurfaceClick(VideoSurfaceTexture videoCallSurface) {
      // Show zoom control when preview surface is clicked.
      LogUtil.i("VideoCallPresenter.onSurfaceClick", "");
      if (shallTransmitStaticImage()) {
        VideoCallPresenter.this.onSurfaceClick();
      } else if (mPictureModeHelper != null && mPictureModeHelper.canShowPreviewVideoView()
          && isActiveVideoCall(primaryCall) && isTransmissionEnabled(primaryCall)) {
        // Set fullscreen to true when showing the zoom controls as the
        // buttons on the left panel conflict with the zoom control bar.
        cancelAutoFullScreen();
        if (!InCallPresenter.getInstance().isFullscreen()) {
          InCallPresenter.getInstance().setFullScreen(true);
        }
        InCallZoomController.getInstance().onPreviewSurfaceClicked(videoCall);
      }
    }
  }

  private class RemoteDelegate implements VideoSurfaceDelegate {
    @Override
    public void onSurfaceCreated(VideoSurfaceTexture videoCallSurface) {
      if (videoCallScreen == null) {
        LogUtil.e("VideoCallPresenter.RemoteDelegate.onSurfaceCreated", "no UI");
        return;
      }
      if (videoCall == null) {
        LogUtil.e("VideoCallPresenter.RemoteDelegate.onSurfaceCreated", "no video call");
        return;
      }
      videoCall.setDisplaySurface(videoCallSurface.getSavedSurface());
    }

    @Override
    public void onSurfaceReleased(VideoSurfaceTexture videoCallSurface) {
      if (videoCall == null) {
        LogUtil.e("VideoCallPresenter.RemoteDelegate.onSurfaceReleased", "no video call");
        return;
      }
      videoCall.setDisplaySurface(null);
    }

    @Override
    public void onSurfaceDestroyed(VideoSurfaceTexture videoCallSurface) {}

    @Override
    public void onSurfaceClick(VideoSurfaceTexture videoCallSurface) {
      VideoCallPresenter.this.onSurfaceClick();
    }
  }

  /** Defines the state of the preview surface negotiation with the telephony layer. */
  private static class PreviewSurfaceState {

    /**
     * The camera has not yet been set on the {@link VideoCall}; negotiation has not yet started.
     */
    private static final int NONE = 0;

    /**
     * The camera has been set on the {@link VideoCall}, but camera capabilities have not yet been
     * received.
     */
    private static final int CAMERA_SET = 1;

    /**
     * The camera capabilties have been received from telephony, but the surface has not yet been
     * set on the {@link VideoCall}.
     */
    private static final int CAPABILITIES_RECEIVED = 2;

    /** The surface has been set on the {@link VideoCall}. */
    private static final int SURFACE_SET = 3;
  }

  private static boolean isBidirectionalVideoCall(DialerCall call) {
    return VideoProfile.isBidirectional(call.getVideoState());
  }

  public static boolean isTransmissionEnabled(DialerCall call) {
    return call != null && VideoProfile.isTransmissionEnabled(call.getVideoState());
  }

  private static boolean isIncomingVideoCall(DialerCall call) {
    if (!isVideoCall(call)) {
      return false;
    }
    final int state = call.getState();
    return (state == DialerCallState.INCOMING) || (state == DialerCallState.CALL_WAITING);
  }

  private static boolean isActiveVideoCall(DialerCall call) {
    return isVideoCall(call) && call.getState() == DialerCallState.ACTIVE;
  }

  private static boolean isOutgoingVideoCall(DialerCall call) {
    if (!isVideoCall(call)) {
      return false;
    }
    final int state = call.getState();
    return DialerCallState.isDialing(state)
        || state == DialerCallState.CONNECTING
        || state == DialerCallState.SELECT_PHONE_ACCOUNT;
  }

  private static boolean isAudioCall(DialerCall call) {
    return call != null && VideoProfile.isAudioOnly(call.getVideoState());
  }

  private static boolean isVideoCall(@Nullable DialerCall call) {
    return call != null && call.isVideoCall();
  }

  private static boolean isVideoCall(int videoState) {
    return VideoProfile.isTransmissionEnabled(videoState)
        || VideoProfile.isReceptionEnabled(videoState);
  }

  private static boolean isModifyToVideoRxType(DialerCall call) {
    return call != null
        && (call.getVideoTech().getUpgradeToVideoState() == VideoProfile.STATE_RX_ENABLED ||
        call.getVideoTech().getRequestedVideoState() == VideoProfile.STATE_RX_ENABLED);
  }

  /**
   * Called when call session event is raised.
   *
   * @param event The call session event.
   */
  @Override
  public void onCallSessionEvent(int event) {
    StringBuilder sb = new StringBuilder();
    sb.append("call session event = ");

    switch (event) {
      case VideoProvider.SESSION_EVENT_RX_PAUSE:
      case VideoProvider.SESSION_EVENT_RX_RESUME:
        mIsIncomingVideoAvailable =
            event == VideoProvider.SESSION_EVENT_RX_RESUME;
        if (primaryCall == null) {
          return;
        }
        showVideoUi(
          primaryCall.getVideoState(),
          primaryCall.getState(),
          primaryCall.getVideoTech().getSessionModificationState(),
          primaryCall.isRemotelyHeld());
        sb.append(mIsIncomingVideoAvailable ? "rx_resume" : "rx_pause");
        break;
      case VideoProvider.SESSION_EVENT_CAMERA_FAILURE:
        sb.append("camera_failure");
        break;
      case VideoProvider.SESSION_EVENT_CAMERA_READY:
        sb.append("camera_ready");
        break;
      default:
        sb.append("unknown event = ");
        sb.append(event);
        break;
    }
    LogUtil.i("VideoCallPresenter.onCallSessionEvent", sb.toString());
  }

  /**
   * Reads the fixed preview size from global settings and caches it
   */
  private void setFixedPreviewSurfaceSize() {
    if (mPictureModeHelper != null) {
      mFixedPreviewSurfaceSize = mPictureModeHelper.getPreviewSizeFromSetting(context);
    }
  }

  /**
   * Gets called when preview video selection changes
   * @param boolean previewVideoSelection - New value for preview video selection
   */
  @Override
  public void onPreviewVideoSelectionChanged() {
    if (primaryCall == null) {
      return;
    }
    setFixedPreviewSurfaceSize();
    if (mFixedPreviewSurfaceSize != null) {
      changePreviewDimensions(mFixedPreviewSurfaceSize.x, mFixedPreviewSurfaceSize.y);
    }
    showVideoUi(
        primaryCall.getVideoState(),
        primaryCall.getState(),
        primaryCall.getVideoTech().getSessionModificationState(),
        primaryCall.isRemotelyHeld());
  }

  /**
   * Gets called when incoming video selection changes
   * @param boolean incomingVideoSelection - New value for incoming video selection
   */
  @Override
  public void onIncomingVideoSelectionChanged() {
    if (primaryCall == null) {
      return;
    }
    showVideoUi(
        primaryCall.getVideoState(),
        primaryCall.getState(),
        primaryCall.getVideoTech().getSessionModificationState(),
        primaryCall.isRemotelyHeld());
  }

  public static PictureModeHelper getPictureModeHelper() {
    return mPictureModeHelper;
  }

  public static void showPipModeMenu() {
    if (mPictureModeHelper != null) {
      mPictureModeHelper.createAndShowDialog();
    }
  }
}
