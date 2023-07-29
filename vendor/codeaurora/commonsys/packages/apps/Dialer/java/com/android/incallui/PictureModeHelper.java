/**
 * Copyright (c) 2016 The Linux Foundation. All rights reserved.
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
import android.content.res.Resources;

import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.DialogInterface.OnDismissListener;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Point;
import android.os.SystemClock;
import android.renderscript.Allocation;
import android.renderscript.Element;
import android.renderscript.RenderScript;
import android.renderscript.ScriptIntrinsicBlur;
import android.provider.Settings;
import android.view.Surface;
import android.view.TextureView;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewTreeObserver;
import android.widget.CheckBox;
import android.widget.ImageView;
import android.widget.RelativeLayout;

import com.android.incallui.call.CallList;
import com.android.incallui.call.DialerCall;
import com.android.dialer.common.LogUtil;
import com.android.incallui.InCallPresenter.InCallDetailsListener;
import com.android.incallui.InCallPresenter.InCallStateListener;
import com.android.incallui.video.protocol.VideoCallScreenDelegate;
import com.android.incallui.video.impl.VideoCallFragment;

import java.util.ArrayList;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.List;

import com.google.common.base.Preconditions;

/**
 * This class displays the picture mode alert dialog and registers listener who wish to listen to
 * user selection for the preview video and the incoming video.
 */
public class PictureModeHelper implements InCallDetailsListener,
        InCallStateListener, CallList.Listener {

    private AlertDialog mAlertDialog;

    /**
     * Indicates whether we should display camera preview video view
     */
    private static boolean mShowPreviewVideoView = true;

    /**
     * Indicates whether we should display incoming video view
     */
    private static boolean mShowIncomingVideoView = true;

    private VideoCallScreenDelegate mVideoCallScreenDelegate;

    /**
     * Property set to specify the size of the preview surface provided by the user/operator.
     * We will also use this for the camera preview size when the picture mode is selected as
     * camera preview mode (non PIP mode)
     */
    private static final String LOCAL_PREVIEW_SURFACE_SIZE_SETTING = "local_preview_surface_size";

    Context context;

    public PictureModeHelper(Context context) {
        this.context = context;
    }

    public void setUp(VideoCallPresenter videoCallPresenter) {
        mVideoCallScreenDelegate = videoCallPresenter;
        InCallActivity incallActivity = InCallPresenter.getInstance().getActivity();
        if (incallActivity == null) {
            return;
        }
        InCallPresenter.getInstance().addDetailsListener(this);
        InCallPresenter.getInstance().addListener(this);
        CallList.getInstance().addListener(this);
        addListener(videoCallPresenter);
    }

    public void tearDown(VideoCallPresenter videoCallPresenter) {
        mVideoCallScreenDelegate = null;
        InCallPresenter.getInstance().removeDetailsListener(this);
        InCallPresenter.getInstance().removeListener(this);
        CallList.getInstance().removeListener(this);
        removeListener(videoCallPresenter);
        mAlertDialog = null;
    }

    /**
     * Displays the alert dialog
     * Creates and displays the alert dialog
     */
    public void createAndShowDialog() {
        create();
        if (mAlertDialog != null) {
            mAlertDialog.show();
        }
    }

    /**
     * Listener interface to implement if any class is interested in listening to preview
     * video or incoming video selection changed
     */
    public interface Listener {
        public void onPreviewVideoSelectionChanged();
        public void onIncomingVideoSelectionChanged();
    }

    private final List<Listener> mListeners = new CopyOnWriteArrayList<>();

    /**
     * This method adds a new Listener. Users interested in listening to preview video selection
     * and incoming video selection changes must register to this class
     * @param Listener listener - the listener to be registered
     */
    public void addListener(Listener listener) {
        Preconditions.checkNotNull(listener);
        mListeners.add(listener);
    }

    /**
     * This method unregisters the listener listening to preview video selection and incoming
     * video selection
     * @param Listener listener - the listener to be un-registered
     */
    public void removeListener(Listener listener) {
        Preconditions.checkNotNull(listener);
        mListeners.remove(listener);
    }

    /**
     * Creates and displays the picture mode alert dialog to enable the user to switch
     * between picture modes - Picture in picture, Incoming mode or Camera preview mode
     * Once users makes their choice, they can save or cancel. Saving will apply the
     * new picture mode to the video call by notifying video call presenter of the change.
     * Cancel will dismiss the alert dialog without making any changes. Alert dialog is
     * cancelable so pressing home/back key will dismiss the dialog.
     * @param Context context - The application context.
     */
    public void create() {
        final ArrayList<CharSequence> items = new ArrayList<CharSequence>();
        InCallActivity incallActivity = InCallPresenter.getInstance().getActivity();
        if (incallActivity == null) {
            return;
        }

        final Resources res = incallActivity.getResources();

        final View checkboxView = incallActivity.getLayoutInflater().
                inflate(R.layout.qti_video_call_picture_mode_menu, null);

        AlertDialog.Builder builder = new AlertDialog.Builder(incallActivity);
        builder.setTitle(R.string.video_call_picture_mode_menu_title);
        builder.setView(checkboxView);
        builder.setCancelable(true);

        CheckBox previewVideo = (CheckBox) checkboxView.findViewById(R.id.preview_video);
        CheckBox incomingVideo = (CheckBox) checkboxView.findViewById(R.id.incoming_video);

        if (previewVideo == null || incomingVideo == null) {
            return;
        }

        previewVideo.setText(res.getText(R.string.video_call_picture_mode_preview_video));
        previewVideo.setTextColor(Color.BLACK);
        incomingVideo.setText(res.getText(R.string.video_call_picture_mode_incoming_video));
        incomingVideo.setTextColor(Color.BLACK);

        // Intialize the checkboxes with the proper checked values
        previewVideo.setChecked(mShowPreviewVideoView);
        incomingVideo.setChecked(mShowIncomingVideoView);

        // Ensure that at least one of the check boxes is enabled. Disable the other checkbox
        // if checkbox is un-checked and vice versa. Say for e.g: if preview video was unchecked,
        // disble incoming video and make it unclickable
        enable(previewVideo, mShowIncomingVideoView);
        enable(incomingVideo, mShowPreviewVideoView);

        previewVideo.setOnClickListener(v->enable(incomingVideo, ((CheckBox) v).isChecked()));

        incomingVideo.setOnClickListener(v->enable(previewVideo, ((CheckBox) v).isChecked()));

        builder.setPositiveButton(res.getText(R.string.video_call_picture_mode_save_option),
                new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int item) {
                    mShowPreviewVideoView = previewVideo.isChecked();
                    mShowIncomingVideoView = incomingVideo.isChecked();
                    notifyOnSelectionChanged();
                }
        });

        builder.setNegativeButton(res.getText(R.string.video_call_picture_mode_cancel_option),
                new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int item) {
                }
        });

        mAlertDialog = builder.create();
        setDismissListener();
    }

    private void setDismissListener() {
        mAlertDialog.setOnDismissListener(new OnDismissListener() {
                @Override
                public void onDismiss(DialogInterface dialog) {
                    mAlertDialog = null;
                }
        });
    }

    /**
     * This method enables or disables the checkbox passed in based on whether the flag enable
     * is set to true or false. Also toggle the checkbox being clickable.
     * @param CheckBox checkBox - the check Box to enable/disable
     * @param boolean enable - Flag to enable/disable checkbox (true/false)
     */
    private void enable(CheckBox checkBox, boolean enable) {
        checkBox.setEnabled(enable);
        checkBox.setClickable(enable);
    }

    /**
     * Determines if we can show the preview video view
     */
    public boolean canShowPreviewVideoView() {
        return mShowPreviewVideoView;
    }

    /**
     * Determines if we can show the incoming video view
     */
    public boolean canShowIncomingVideoView() {
        return mShowIncomingVideoView;
    }

    /**
     * Determines whether we are in Picture in Picture mode
     */
    public boolean isPipMode() {
        return canShowPreviewVideoView() && canShowIncomingVideoView();
    }

    /**
     * Notifies all registered classes of preview video or incoming video selection changed
     */
    public void notifyOnSelectionChanged() {
        Preconditions.checkNotNull(mListeners);
        for (Listener listener : mListeners) {
            listener.onPreviewVideoSelectionChanged();
            listener.onIncomingVideoSelectionChanged();
        }
    }

    /**
     * Listens to call details changed and dismisses the dialog if call has been downgraded to
     * voice
     * @param DialerCall call - The call for which details changed
     * @param android.telecom.Call.Details details - The changed details
     */
    @Override
    public void onDetailsChanged(DialerCall call, android.telecom.Call.Details details) {
        if (call == null) {
            return;
        }
        if (call != null && !call.isVideoCall() && mAlertDialog != null) {
            mAlertDialog.dismiss();
        }
    }

    /**
     * Handles call state changes
     *
     * @param InCallPresenter.InCallState oldState - The old call state
     * @param InCallPresenter.InCallState newState - The new call state
     * @param CallList callList - The call list.
     */
    @Override
    public void onStateChange(InCallPresenter.InCallState oldState,
            InCallPresenter.InCallState newState, CallList callList) {
        Log.d(this, "onStateChange oldState" + oldState + " newState=" + newState);
        if (newState == InCallPresenter.InCallState.NO_CALLS) {
            // Set both display preview video and incoming video to true as default display mode is
            // to show picture in picture.
            mShowPreviewVideoView = true;
            mShowIncomingVideoView = true;
        }
    }

    /**
     * Overrides onIncomingCall method of {@interface CallList.Listener}
     * @param DialerCall call - The incoming call
     */
    @Override
    public void onIncomingCall(DialerCall call) {
        if (mAlertDialog != null) {
            mAlertDialog.dismiss();
        }
    }

    /**
     * Overrides onCallListChange method of {@interface CallList.Listener}
     * Added for completeness
     */
    @Override
    public void onCallListChange(CallList list) {
        // no-op
    }

    /**
     * Overrides onUpgradeToVideo method of {@interface CallList.Listener}
     * @param DialerCall call - The call to be upgraded
     */
    @Override
    public void onUpgradeToVideo(DialerCall call) {
        if (mAlertDialog != null) {
            mAlertDialog.dismiss();
        }
    }

    public void maybeHideVideoViews() {
        InCallActivity incallActivity = InCallPresenter.getInstance().getActivity();
        if (incallActivity == null) {
            return;
        }
        LogUtil.i("PictureModeHelper.maybeHideVideoViews",
            "canShowPreviewVideoView = %b canShowIncomingVideoView = %b",
            mShowPreviewVideoView, mShowIncomingVideoView);
        TextureView previewTextureView =
            (TextureView) incallActivity.findViewById(R.id.videocall_video_preview);
        ImageView previewOffBlurredImageView =
            (ImageView) incallActivity.findViewById(
            R.id.videocall_preview_off_blurred_image_view);
        if (previewTextureView == null || previewOffBlurredImageView ==null) {
            LogUtil.e("PictureModeHelper.maybeHideVideoViews",
                "previewTextureView/previewOffBlurredImageView is null");
            return;
        }
        previewTextureView.setVisibility(mShowPreviewVideoView ? View.VISIBLE : View.GONE);
        previewOffBlurredImageView.setVisibility(mShowPreviewVideoView ?
            View.VISIBLE : View.GONE);

        TextureView remoteTextureView =
            (TextureView) incallActivity.findViewById(R.id.videocall_video_remote);
        remoteTextureView.setVisibility(mShowIncomingVideoView ? View.VISIBLE : View.GONE);

        ImageView remoteOffImageView =
            (ImageView) incallActivity.findViewById(
            R.id.videocall_remote_off_blurred_image_view);
        remoteOffImageView.setVisibility(mShowIncomingVideoView ? View.VISIBLE : View.GONE);
    }

    public void setPreviewVideoLayoutParams() {
        InCallActivity incallActivity = InCallPresenter.getInstance().getActivity();
        if (incallActivity == null || (!mShowPreviewVideoView && mShowIncomingVideoView)) {
            LogUtil.e("PictureModeHelper.setPreviewVideoLayoutParams",
                "Incallactivity is null or We are not in preview only mode");
            return;
        }
        TextureView previewTextureView =
            (TextureView) incallActivity.findViewById(R.id.videocall_video_preview);
        ImageView previewOffBlurredImageView =
            (ImageView) incallActivity.findViewById(
            R.id.videocall_preview_off_blurred_image_view);
        if (previewTextureView == null || previewOffBlurredImageView ==null) {
            LogUtil.e("PictureModeHelper.setPreviewVideoLayoutParams",
                "previewTextureView/previewOffBlurredImageView is null");
            return;
        }
        RelativeLayout.LayoutParams params = getLayoutParams();
        previewTextureView.setLayoutParams(params);
        previewOffBlurredImageView.setLayoutParams(params);
        previewTextureView.setOutlineProvider(
            mShowPreviewVideoView && mShowIncomingVideoView ?
            VideoCallFragment.circleOutlineProvider : null);
        previewOffBlurredImageView.setOutlineProvider(
            mShowPreviewVideoView && mShowIncomingVideoView ?
            VideoCallFragment.circleOutlineProvider : null);
        previewOffBlurredImageView.setClipToOutline(mShowPreviewVideoView
                && mShowIncomingVideoView);
        addOnGlobalLayoutListener(previewTextureView, previewOffBlurredImageView);
    }

    private void addOnGlobalLayoutListener(TextureView previewTextureView,
        ImageView previewOffBlurredImageView) {
        ViewTreeObserver observer = previewTextureView.getViewTreeObserver();
            observer.addOnGlobalLayoutListener(
                new ViewTreeObserver.OnGlobalLayoutListener() {
                    @Override
                    public void onGlobalLayout() {
                        PrimaryCallTracker primaryCallTracker =
                            BottomSheetHelper.getInstance().getPrimaryCallTracker();
                        boolean showOutgoing = false;
                        if (primaryCallTracker != null) {
                            final DialerCall primaryCall =
                                primaryCallTracker.getPrimaryCall();
                            InCallActivity incallActivity =
                                InCallPresenter.getInstance().getActivity();
                            if ((primaryCall != null && !primaryCall.isVideoCall())
                                || incallActivity == null) {
                                return;
                            }
                            showOutgoing = primaryCall != null
                                && VideoCallPresenter.showOutgoingVideo(
                                incallActivity, primaryCall.getVideoState(),
                                primaryCall.getVideoTech().getSessionModificationState());
                        }
                        updateBlurredImageView(
                            previewTextureView, previewOffBlurredImageView,
                            showOutgoing,
                            VideoCallFragment.BLUR_PREVIEW_RADIUS,
                            VideoCallFragment.BLUR_PREVIEW_SCALE_FACTOR);
                        // Remove the listener so we don't continually re-layout.
                        ViewTreeObserver observer = previewTextureView.getViewTreeObserver();
                        if (observer.isAlive()) {
                            observer.removeOnGlobalLayoutListener(this);
                        }
                    }
              });
    }

    private boolean hasValidPreviewSurfaceSize(Context context) {
        final String previewSurfaceSize = Settings.Global.getString(context.getContentResolver(),
            LOCAL_PREVIEW_SURFACE_SIZE_SETTING);
        return previewSurfaceSize != null && !previewSurfaceSize.isEmpty();
    }

    public boolean shouldShowPreviewOnly() {
        return mShowPreviewVideoView && !mShowIncomingVideoView;
    }

    private RelativeLayout.LayoutParams getLayoutParams() {
        InCallActivity incallActivity = InCallPresenter.getInstance().getActivity();
        if (incallActivity == null) {
            return null;
        }
        RelativeLayout.LayoutParams params;
        if (mShowPreviewVideoView && mShowIncomingVideoView) {
            Resources resources = incallActivity.getResources();
            params = new RelativeLayout.LayoutParams(
                (int) resources.getDimension(R.dimen.videocall_preview_width),
                (int) resources.getDimension(R.dimen.videocall_preview_height));
            params.setMargins(0, 0, 0,
                (int) resources.getDimension(R.dimen.videocall_preview_margin_bottom));
            if (isLandscape(incallActivity)) {
                params.addRule(RelativeLayout.ALIGN_PARENT_END);
                params.setMarginEnd((int) resources.getDimension(
                    R.dimen.videocall_preview_margin_end));
            } else {
                params.addRule(RelativeLayout.ALIGN_PARENT_START);
                params.setMarginStart((int) resources.getDimension(
                    R.dimen.videocall_preview_margin_start));
            }
            params.addRule(RelativeLayout.ALIGN_PARENT_BOTTOM);
            return params;
        } else {
            params = new RelativeLayout.LayoutParams(
                RelativeLayout.LayoutParams.MATCH_PARENT,
                RelativeLayout.LayoutParams.MATCH_PARENT);
            if (hasValidPreviewSurfaceSize(incallActivity)) {
                Point size = getPreviewSizeFromSetting(incallActivity);
                if (size != null) {
                    params.width = (isLandscape(incallActivity)) ? size.y : size.x;
                    params.height = (isLandscape(incallActivity))? size.x : size.y;
                    params.addRule(RelativeLayout.ALIGN_PARENT_BOTTOM);
                }
            } else {
                if (isLandscape(incallActivity)) {
                    params.addRule(RelativeLayout.ALIGN_PARENT_RIGHT);
                } else {
                    params.addRule(RelativeLayout.ALIGN_PARENT_START);
                    params.addRule(RelativeLayout.ALIGN_PARENT_TOP);
                }
            }
            return params;
        }
    }

    public Point getPreviewSizeFromSetting(Context context) {
        final String previewSurfaceSize = Settings.Global.getString(context.getContentResolver(),
          LOCAL_PREVIEW_SURFACE_SIZE_SETTING);

        if (previewSurfaceSize == null) {
          return null;
        }

        try {
            final String[] sizeDimensions = previewSurfaceSize.split("x");
            final int width = Integer.parseInt(sizeDimensions[0]);
            final int height = Integer.parseInt(sizeDimensions[1]);
            return new Point(width, height);
        } catch (Exception ex) {
            LogUtil.e("PictureModeHelper.setFixedPreviewSurfaceSize", "Exception in parsing " +
                LOCAL_PREVIEW_SURFACE_SIZE_SETTING + " - " + ex);
            return null;
        }
    }

    private boolean isLandscape(Context context) {
        int rotation = ((InCallActivity)context).getWindowManager().getDefaultDisplay().
            getRotation();
        return rotation == Surface.ROTATION_90 || rotation == Surface.ROTATION_270;
    }

    /**
     * Overrides onDisconnect method of {@interface CallList.Listener}
     * @param DialerCall call - The call to be disconnected
     */
    @Override
    public void onDisconnect(DialerCall call) {
        if (mAlertDialog != null) {
            mAlertDialog.dismiss();
        }
    }

    @Override
    public void onInternationalCallOnWifi(DialerCall call) {}

    @Override
    public void onHandoverToWifiFailed(DialerCall call) {}

    @Override
    public void onWiFiToLteHandover(DialerCall call) {}

    @Override
    public void onSessionModificationStateChange(DialerCall call) {}

    @Override
    public void onSuplServiceMessage(String suplNotificationMessage) {}

    public void updateBlurredImageView(
      TextureView textureView,
      ImageView blurredImageView,
      boolean isVideoEnabled,
      float blurRadius,
      float scaleFactor) {

    if (isVideoEnabled || context == null) {
      blurredImageView.setImageBitmap(null);
      blurredImageView.setVisibility(View.GONE);
      return;
    }

    long startTimeMillis = SystemClock.elapsedRealtime();
    int width = Math.round(textureView.getWidth() * scaleFactor);
    int height = Math.round(textureView.getHeight() * scaleFactor);

    LogUtil.i("VideoCallFragment.updateBlurredImageView", "width: %d, height: %d", width, height);

    // This call takes less than 10 milliseconds.
    Bitmap bitmap = textureView.getBitmap(width, height);

    if (bitmap == null) {
      blurredImageView.setImageBitmap(null);
      blurredImageView.setVisibility(View.GONE);
      return;
    }

    // TODO(mdooley): When the view is first displayed after a rotation the bitmap is empty
    // and thus this blur has no effect.
    // This call can take 100 milliseconds.
    final InCallActivity inCallActivity = InCallPresenter.getInstance().getActivity();
    if (inCallActivity == null) {
      return;
    }
    blur(inCallActivity, bitmap, blurRadius);

    // TODO(mdooley): Figure out why only have to apply the transform in landscape mode
    if (width > height) {
      bitmap =
          Bitmap.createBitmap(
              bitmap,
              0,
              0,
              bitmap.getWidth(),
              bitmap.getHeight(),
              textureView.getTransform(null),
              true);
    }

    blurredImageView.setImageBitmap(bitmap);
    blurredImageView.setVisibility(View.VISIBLE);

    LogUtil.i(
        "VideoCallFragment.updateBlurredImageView",
        "took %d millis",
        (SystemClock.elapsedRealtime() - startTimeMillis));
  }

  private void blur(Context context, Bitmap image, float blurRadius) {
    RenderScript renderScript = RenderScript.create(context);
    ScriptIntrinsicBlur blurScript =
        ScriptIntrinsicBlur.create(renderScript, Element.U8_4(renderScript));
    Allocation allocationIn = Allocation.createFromBitmap(renderScript, image);
    Allocation allocationOut = Allocation.createFromBitmap(renderScript, image);
    blurScript.setRadius(blurRadius);
    blurScript.setInput(allocationIn);
    blurScript.forEach(allocationOut);
    allocationOut.copyTo(image);
    blurScript.destroy();
    allocationIn.destroy();
    allocationOut.destroy();
  }
}
