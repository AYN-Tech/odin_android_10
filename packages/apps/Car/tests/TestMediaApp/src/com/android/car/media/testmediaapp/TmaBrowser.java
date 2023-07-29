/*
 * Copyright (c) 2019, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.android.car.media.testmediaapp;

import static com.android.car.media.testmediaapp.prefs.TmaEnumPrefs.TmaBrowseNodeType.LEAF_CHILDREN;
import static com.android.car.media.testmediaapp.prefs.TmaEnumPrefs.TmaBrowseNodeType.QUEUE_ONLY;
import static com.android.car.media.testmediaapp.prefs.TmaEnumPrefs.TmaLoginEventOrder.PLAYBACK_STATE_UPDATE_FIRST;

import android.content.Context;
import android.media.AudioManager;
import android.os.Bundle;
import android.os.Handler;
import android.support.v4.media.MediaBrowserCompat.MediaItem;
import android.support.v4.media.session.MediaSessionCompat;
import android.support.v4.media.session.PlaybackStateCompat;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.media.MediaBrowserServiceCompat;

import com.android.car.media.testmediaapp.loader.TmaLoader;
import com.android.car.media.testmediaapp.prefs.TmaEnumPrefs.TmaAccountType;
import com.android.car.media.testmediaapp.prefs.TmaEnumPrefs.TmaReplyDelay;
import com.android.car.media.testmediaapp.prefs.TmaPrefs;

import java.util.ArrayList;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;


/**
 * Implementation of {@link MediaBrowserServiceCompat} that delivers {@link MediaItem}s based on
 * json configuration files stored in the application's assets. Those assets combined with a few
 * preferences (see: {@link TmaPrefs}), allow to create a variety of use cases (including error
 * states) to stress test the Car Media application. <p/>
 * The media items are cached in the {@link TmaLibrary}, and can be virtually played with
 * {@link TmaPlayer}.
 */
public class TmaBrowser extends MediaBrowserServiceCompat {
    private static final String TAG = "TmaBrowser";

    private static final int MAX_SEARCH_DEPTH = 4;
    private static final String MEDIA_SESSION_TAG = "TEST_MEDIA_SESSION";
    private static final String ROOT_ID = "_ROOT_ID_";
    private static final String SEARCH_SUPPORTED = "android.media.browse.SEARCH_SUPPORTED";
    /**
     * Extras key to allow Android Auto to identify the browse service from the media session.
     */
    private static final String BROWSE_SERVICE_FOR_SESSION_KEY =
        "android.media.session.BROWSE_SERVICE";

    private TmaPrefs mPrefs;
    private Handler mHandler;
    private MediaSessionCompat mSession;
    private TmaLibrary mLibrary;
    private TmaPlayer mPlayer;

    private BrowserRoot mRoot;

    @Override
    public void onCreate() {
        super.onCreate();
        mPrefs = TmaPrefs.getInstance(this);
        mHandler = new Handler();
        mSession = new MediaSessionCompat(this, MEDIA_SESSION_TAG);
        setSessionToken(mSession.getSessionToken());

        mLibrary = new TmaLibrary(new TmaLoader(this));
        AudioManager audioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        mPlayer = new TmaPlayer(this, mLibrary, audioManager, mHandler, mSession);

        mSession.setCallback(mPlayer);
        mSession.setFlags(MediaSessionCompat.FLAG_HANDLES_MEDIA_BUTTONS
                | MediaSessionCompat.FLAG_HANDLES_TRANSPORT_CONTROLS);
        Bundle mediaSessionExtras = new Bundle();
        mediaSessionExtras.putString(BROWSE_SERVICE_FOR_SESSION_KEY, TmaBrowser.class.getName());
        mSession.setExtras(mediaSessionExtras);

        mPrefs.mAccountType.registerChangeListener(
                (oldValue, newValue) -> onAccountChanged(newValue));

        mPrefs.mRootNodeType.registerChangeListener(
                (oldValue, newValue) -> invalidateRoot());

        mPrefs.mRootReplyDelay.registerChangeListener(
                (oldValue, newValue) -> invalidateRoot());

        Bundle browserRootExtras = new Bundle();
        browserRootExtras.putBoolean(SEARCH_SUPPORTED, true);
        mRoot = new BrowserRoot(ROOT_ID, browserRootExtras);

        updatePlaybackState(mPrefs.mAccountType.getValue());
    }

    @Override
    public void onDestroy() {
        mSession.release();
        mHandler = null;
        mPrefs = null;
        super.onDestroy();
    }

    private void onAccountChanged(TmaAccountType accountType) {
        if (PLAYBACK_STATE_UPDATE_FIRST.equals(mPrefs.mLoginEventOrder.getValue())) {
            updatePlaybackState(accountType);
            invalidateRoot();
        } else {
            invalidateRoot();
            (new Handler()).postDelayed(() -> {
                updatePlaybackState(accountType);
            }, 3000);
        }
    }

    private void updatePlaybackState(TmaAccountType accountType) {
        if (accountType == TmaAccountType.NONE) {
            mSession.setMetadata(null);
            mPlayer.onStop();
            mPlayer.setPlaybackState(
                    new TmaMediaEvent(TmaMediaEvent.EventState.ERROR,
                            TmaMediaEvent.StateErrorCode.AUTHENTICATION_EXPIRED,
                            getResources().getString(R.string.no_account),
                            getResources().getString(R.string.select_account),
                            TmaMediaEvent.ResolutionIntent.PREFS,
                            TmaMediaEvent.Action.NONE, 0, null));
        } else {
            // TODO don't reset error in all cases...
            PlaybackStateCompat.Builder playbackState = new PlaybackStateCompat.Builder();
            playbackState.setState(PlaybackStateCompat.STATE_PAUSED, 0, 0);
            playbackState.setActions(PlaybackStateCompat.ACTION_PREPARE);
            mSession.setPlaybackState(playbackState.build());
        }
    }

    private void invalidateRoot() {
        notifyChildrenChanged(ROOT_ID);
    }

    @Override
    public BrowserRoot onGetRoot(
            @NonNull String clientPackageName, int clientUid, Bundle rootHints) {
        if (rootHints == null) {
            Log.e(TAG, "Client " + clientPackageName + " didn't set rootHints.");
            throw new NullPointerException("rootHints is null");
        }
        Log.i(TAG, "onGetroot client: " + clientPackageName + " EXTRA_MEDIA_ART_SIZE_HINT_PIXELS: "
                + rootHints.getInt(MediaKeys.EXTRA_MEDIA_ART_SIZE_HINT_PIXELS, 0));
        return mRoot;
    }

    @Override
    public void onLoadChildren(@NonNull String parentId, @NonNull Result<List<MediaItem>> result) {
        getMediaItemsWithDelay(parentId, result, null);

        if (QUEUE_ONLY.equals(mPrefs.mRootNodeType.getValue()) && ROOT_ID.equals(parentId)) {
            TmaMediaItem queue = mLibrary.getRoot(LEAF_CHILDREN);
            if (queue != null) {
                mSession.setQueue(queue.buildQueue());
                mPlayer.prepareMediaItem(queue.getPlayableByIndex(0));
            }
        }
    }

    @Override
    public void onSearch(@NonNull String query, Bundle extras,
            @NonNull Result<List<MediaItem>> result) {
        getMediaItemsWithDelay(ROOT_ID, result, query);
    }

    private void getMediaItemsWithDelay(@NonNull String parentId,
            @NonNull Result<List<MediaItem>> result, @Nullable String filter) {
        // TODO: allow per item override of the delay ?
        TmaReplyDelay delay = mPrefs.mRootReplyDelay.getValue();
        Runnable task = () -> {
            TmaMediaItem node;
            if (TmaAccountType.NONE.equals(mPrefs.mAccountType.getValue())) {
                node = null;
            } else if (ROOT_ID.equals(parentId)) {
                node = mLibrary.getRoot(mPrefs.mRootNodeType.getValue());
            } else {
                node = mLibrary.getMediaItemById(parentId);
            }

            if (node == null) {
                result.sendResult(null);
            } else if (filter != null) {
                List<MediaItem> hits = new ArrayList<>(50);
                Pattern pat = Pattern.compile(Pattern.quote(filter), Pattern.CASE_INSENSITIVE);
                addSearchResults(node, pat.matcher(""), hits, MAX_SEARCH_DEPTH);
                result.sendResult(hits);
            } else {
                List<MediaItem> items = new ArrayList<>(node.mChildren.size());
                for (TmaMediaItem child : node.mChildren) {
                    items.add(child.toMediaItem());
                }
                result.sendResult(items);
            }
        };
        if (delay == TmaReplyDelay.NONE) {
            task.run();
        } else {
            result.detach();
            mHandler.postDelayed(task, delay.mReplyDelayMs);
        }
    }

    private void addSearchResults(@Nullable TmaMediaItem node, Matcher matcher,
            List<MediaItem> hits, int currentDepth) {
        if (node == null || currentDepth <= 0) {
            return;
        }

        for (TmaMediaItem child : node.mChildren) {
            MediaItem item = child.toMediaItem();
            CharSequence title = item.getDescription().getTitle();
            if (title != null) {
                matcher.reset(title);
                if (matcher.find()) {
                    hits.add(item);
                }
            }

            // Ask the library to load the grand children
            child = mLibrary.getMediaItemById(child.getMediaId());
            addSearchResults(child, matcher, hits, currentDepth - 1);
        }
    }
}
