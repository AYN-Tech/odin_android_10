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

package com.android.incallui.call;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import java.util.Collections;
import java.util.Objects;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

/** Class used to notify interested parties of incoming video related events. */
public class InCallVideoCallCallbackNotifier {

  /** Singleton instance of this class. */
  private static InCallVideoCallCallbackNotifier instance = new InCallVideoCallCallbackNotifier();

  /**
   * ConcurrentHashMap constructor params: 8 is initial table size, 0.9f is load factor before
   * resizing, 1 means we only expect a single thread to access the map so make only a single shard
   */
  private final Set<SurfaceChangeListener> surfaceChangeListeners =
      Collections.newSetFromMap(new ConcurrentHashMap<SurfaceChangeListener, Boolean>(8, 0.9f, 1));
  private final Set<VideoEventListener> mVideoEventListeners = Collections.newSetFromMap(
       new ConcurrentHashMap<VideoEventListener, Boolean>(8, 0.9f, 1));

  /* Invalid call session event */
  public static final int CALL_SESSION_INVALID_EVENT = -1;

  /** Cache the call session event for cases where the call is in background and listeners
   * are unregistered.
   */
  private int mCallSessionEvent = CALL_SESSION_INVALID_EVENT;

  /** Private constructor. Instance should only be acquired through getRunningInstance(). */
  private InCallVideoCallCallbackNotifier() {}

  /** Static singleton accessor method. */
  public static InCallVideoCallCallbackNotifier getInstance() {
    return instance;
  }

  public void setUp() {
  }

  public void tearDown() {
    mCallSessionEvent = CALL_SESSION_INVALID_EVENT;
    mVideoEventListeners.clear();
    surfaceChangeListeners.clear();
  }

  /**
   * Adds a new {@link SurfaceChangeListener}.
   *
   * @param listener The listener.
   */
  public void addSurfaceChangeListener(@NonNull SurfaceChangeListener listener) {
    Objects.requireNonNull(listener);
    surfaceChangeListeners.add(listener);
  }

  /**
   * Remove a {@link SurfaceChangeListener}.
   *
   * @param listener The listener.
   */
  public void removeSurfaceChangeListener(@Nullable SurfaceChangeListener listener) {
    if (listener != null) {
      surfaceChangeListeners.remove(listener);
    }
  }

  /**
   * Inform listeners of a change to peer dimensions.
   *
   * @param call The call.
   * @param width New peer width.
   * @param height New peer height.
   */
  public void peerDimensionsChanged(DialerCall call, int width, int height) {
    for (SurfaceChangeListener listener : surfaceChangeListeners) {
      listener.onUpdatePeerDimensions(call, width, height);
    }
  }

  /**
   * Inform listeners of a change to camera dimensions.
   *
   * @param call The call.
   * @param width The new camera video width.
   * @param height The new camera video height.
   */
  public void cameraDimensionsChanged(DialerCall call, int width, int height) {
    for (SurfaceChangeListener listener : surfaceChangeListeners) {
      listener.onCameraDimensionsChange(call, width, height);
    }
  }

  /**
   * Inform listeners of a call session event.
   *
   * @param event The call session event.
   */
  public void callSessionEvent(int event) {
    mCallSessionEvent = event;
    for (VideoEventListener listener : mVideoEventListeners) {
      listener.onCallSessionEvent(mCallSessionEvent);
    }
  }

  /**
   * Adds a new {@link VideoEventListener}.
   *
   * @param listener The listener.
   */
  public void addVideoEventListener(VideoEventListener listener) {
    Objects.requireNonNull(listener);
    mVideoEventListeners.add(listener);
  }

  /**
   * Adds a new {@link VideoEventListener} and notifies the entity that registered
   * if flag notify is true.
   *
   * @param listener The listener.
   * @param notify true or false
   */
  public void addVideoEventListener(VideoEventListener listener, boolean notify) {
    addVideoEventListener(listener);

    // Notify registered listeners of cached call session event if it's a valid value
    if (notify && mCallSessionEvent != CALL_SESSION_INVALID_EVENT) {
       callSessionEvent(mCallSessionEvent);
     }
  }

  /**
   * Remove a {@link VideoEventListener}.
   *
   * @param listener The listener.
   */
  public void removeVideoEventListener(VideoEventListener listener) {
    if (listener != null) {
      mVideoEventListeners.remove(listener);
    }
  }

  /**
   * Listener interface for any class that wants to be notified of changes to the video surfaces.
   */
  public interface SurfaceChangeListener {

    /**
     * Called when the peer video feed changes dimensions. This can occur when the peer rotates
     * their device, changing the aspect ratio of the video signal.
     *
     * @param call The call which experienced a peer video
     */
    void onUpdatePeerDimensions(DialerCall call, int width, int height);

    /**
     * Called when the local camera changes dimensions. This occurs when a change in camera occurs.
     *
     * @param call The call which experienced the camera dimension change.
     * @param width The new camera video width.
     * @param height The new camera video height.
     */
    void onCameraDimensionsChange(DialerCall call, int width, int height);
  }

  /**
   * Listener interface for any class that wants to be notified of video events, including pause
   * and un-pause of peer video, video quality changes.
   */
  public interface VideoEventListener {
    /**
     * Called when call session event is raised.
     *
     * @param event The call session event.
     */
    public void onCallSessionEvent(int event);
  }
}
