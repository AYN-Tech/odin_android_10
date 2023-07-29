/*
 * Copyright (C) 2009 The Android Open Source Project
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

package com.android.music;

import android.app.PendingIntent;
import android.appwidget.AppWidgetManager;
import android.appwidget.AppWidgetProvider;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.os.Environment;
import android.provider.MediaStore;
import android.view.View;
import android.widget.RemoteViews;

/**
 * Simple widget to show currently playing album art along
 * with play/pause and next track buttons.  
 */
public class MediaAppWidgetProvider extends AppWidgetProvider {
    static final String TAG = "MusicAppWidgetProvider";
    
    public static final String CMDAPPWIDGETUPDATE = "appwidgetupdate";
    private static final String UPDATE_WIDGET_ACTION = "com.android.music.updatewidget";

    private static MediaAppWidgetProvider sInstance;

    private static boolean mPauseState = false;
    private static boolean mNoneedperformUpdate = false;

    protected int mWidgetLayoutId;
    protected String mUpdateFlag;

    static synchronized MediaAppWidgetProvider getInstance() {
        if (sInstance == null) {
            sInstance = new MediaAppWidgetProvider();
        }
        return sInstance;
    }

    public MediaAppWidgetProvider() {
        init();
    }

    protected void init() {
        mWidgetLayoutId = R.layout.statusbar_appwidget_s;
        mUpdateFlag = CMDAPPWIDGETUPDATE;
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        String action = intent.getAction();
        if (action.equals(UPDATE_WIDGET_ACTION) && hasInstances(context)) {
            //receive from MediaPlaybackService onDestroy
            final RemoteViews views = new RemoteViews(context.getPackageName(), mWidgetLayoutId);
            AppWidgetManager appWidgetManager = AppWidgetManager.getInstance(context);
            int[] appWidgetIds = appWidgetManager.getAppWidgetIds(new ComponentName(context, this.getClass()));
            views.setViewVisibility(R.id.trackname, View.GONE);
            views.setTextViewText(R.id.artist, context.getText(R.string.widget_initial_text));
            views.setViewVisibility(R.id.album, View.GONE);
            views.setImageViewResource(R.id.icon, R.drawable.album_cover_background);
            views.setImageViewResource(R.id.pause, R.drawable.notification_play);
            /* everytime pushUpdate(updateAppWidget) should do linkButtons, otherwise the buttons will not work */
            linkButtons(context, views, false /* not playing */);
            pushUpdate(context, appWidgetIds, views);
            //this time the service is died,when service reactivate no need call performUpdate
            //otherwise title will be falsh
            mNoneedperformUpdate = true;
        }
        super.onReceive(context, intent);
    }

    @Override
    public void onUpdate(Context context, AppWidgetManager appWidgetManager, int[] appWidgetIds) {
        mNoneedperformUpdate = false;
        defaultAppWidget(context, appWidgetIds);
        
        // Send broadcast intent to any running MediaPlaybackService so it can
        // wrap around with an immediate update.
        Intent updateIntent = new Intent(MediaPlaybackService.SERVICECMD);
        updateIntent.putExtra(MediaPlaybackService.CMDNAME, mUpdateFlag);
        updateIntent.putExtra(AppWidgetManager.EXTRA_APPWIDGET_IDS, appWidgetIds);
        updateIntent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY);
        context.sendBroadcast(updateIntent);
    }
    
    /**
     * Initialize given widgets to default state, where we launch Music on default click
     * and hide actions if service not running.
     */
    private void defaultAppWidget(Context context, int[] appWidgetIds) {
        final Resources res = context.getResources();
        final RemoteViews views = new RemoteViews(context.getPackageName(), mWidgetLayoutId);

        views.setViewVisibility(R.id.trackname, View.GONE);
        views.setTextViewText(R.id.artist, res.getText(R.string.widget_initial_text));
        views.setViewVisibility(R.id.album, View.GONE);

        linkButtons(context, views, false /* not playing */);
        pushUpdate(context, appWidgetIds, views);
    }
    
    private void pushUpdate(Context context, int[] appWidgetIds, RemoteViews views) {
        // Update specific list of appWidgetIds if given, otherwise default to all
        final AppWidgetManager gm = AppWidgetManager.getInstance(context);
        if (appWidgetIds != null) {
            gm.updateAppWidget(appWidgetIds, views);
        } else {
            gm.updateAppWidget(new ComponentName(context, this.getClass()), views);
        }
    }
    
    /**
     * Check against {@link AppWidgetManager} if there are any instances of this widget.
     */
    private boolean hasInstances(Context context) {
        AppWidgetManager appWidgetManager = AppWidgetManager.getInstance(context);
        int[] appWidgetIds = appWidgetManager.getAppWidgetIds(
                new ComponentName(context, this.getClass()));
        return (appWidgetIds.length > 0);
    }

    void setPauseState(boolean mPaused) {
        mPauseState = mPaused;
    }

    /**
     * Handle a change notification coming over from {@link MediaPlaybackService}
     */
    void notifyChange(MediaPlaybackService service, String what) {
        if (hasInstances(service)) {
            if (MediaPlaybackService.META_CHANGED.equals(what) ||
                    MediaPlaybackService.PLAYSTATE_CHANGED.equals(what)) {
                performUpdate(service, null);
            }
        }
    }
    
    /**
     * Update all active widget instances by pushing changes 
     */
    void performUpdate(MediaPlaybackService service, int[] appWidgetIds) {
        if (mNoneedperformUpdate) {
            //this time no need performUpdate,otherwise title will be flash
            mNoneedperformUpdate = false;
            return;
        }
        final Resources res = service.getResources();
        final RemoteViews views = new RemoteViews(service.getPackageName(), mWidgetLayoutId);
        Bitmap icon = MusicUtils.getArtwork(service, service.getAudioId(), service.getAlbumId(),
                true);
        if (icon == null) {
            views.setImageViewResource(R.id.icon, R.drawable.album_cover_background);
        } else {
            views.setImageViewBitmap(R.id.icon, icon);
        }

        CharSequence titleName = service.getTrackName();
        CharSequence artistName = service.getArtistName();
        CharSequence albumName = service.getAlbumName();
        if (MediaStore.UNKNOWN_STRING.equals(artistName)) {
            artistName = res.getString(R.string.unknown_artist_name);
        }
        CharSequence errorState = null;
        
        // Format title string with track number, or show SD card message
        String status = Environment.getExternalStorageState();
        if (status.equals(Environment.MEDIA_SHARED) ||
                status.equals(Environment.MEDIA_UNMOUNTED)) {
            if (android.os.Environment.isExternalStorageRemovable()) {
                errorState = res.getText(R.string.sdcard_busy_title);
            } else {
                errorState = res.getText(R.string.sdcard_busy_title_nosdcard);
            }
        } else if (status.equals(Environment.MEDIA_REMOVED)) {
            if (android.os.Environment.isExternalStorageRemovable()) {
                errorState = res.getText(R.string.sdcard_missing_title);
            } else {
                errorState = res.getText(R.string.sdcard_missing_title_nosdcard);
            }
        }
        
        if (errorState != null) {
            // Show error state to user
            views.setViewVisibility(R.id.trackname, View.GONE);
            views.setTextViewText(R.id.artist, errorState);
            views.setViewVisibility(R.id.album, View.GONE);

        } else {
            // No error, so show normal titles
            views.setViewVisibility(R.id.trackname, View.VISIBLE);
            views.setTextViewText(R.id.trackname, titleName);
            views.setTextViewText(R.id.artist, artistName);
            views.setViewVisibility(R.id.album, View.VISIBLE);
            views.setTextViewText(R.id.album, albumName);
        }
        
        // Set correct drawable for pause state
        final boolean playing = service.isPlaying();
        if (playing) {
            views.setImageViewResource(R.id.pause, R.drawable.notification_pause);
        } else {
            if (titleName == null && !mPauseState && (errorState == null)) {
                views.setViewVisibility(R.id.trackname, View.GONE);
                views.setTextViewText(R.id.artist, res.getText(R.string.emptyplaylist));
                views.setViewVisibility(R.id.album, View.GONE);
            }
            views.setImageViewResource(R.id.pause, R.drawable.notification_play);
        }

        // Link actions buttons to intents
        linkButtons(service, views, playing);
        
        pushUpdate(service, appWidgetIds, views);
    }

    /**
     * Link up various button actions using {@link PendingIntents}.
     * 
     * @param playerActive True if player is active in background, which means
     *            widget click will launch {@link MediaPlaybackActivity},
     *            otherwise we launch {@link MusicBrowserActivity}.
     */
    private void linkButtons(Context context, RemoteViews views, boolean playerActive) {
        // Connect up various buttons and touch events
        Intent intent;
        PendingIntent pendingIntent;
        
        final ComponentName serviceName = new ComponentName(context, MediaPlaybackService.class);
        
        if (playerActive) {
            intent = new Intent(context, MusicBrowserActivity.class);
            pendingIntent = PendingIntent.getActivity(context,
                    0 /* no requestCode */, intent, 0 /* no flags */);
            views.setOnClickPendingIntent(R.id.music_info, pendingIntent);
            views.setOnClickPendingIntent(R.id.icon, pendingIntent);
        } else {
            intent = new Intent(context, MusicBrowserActivity.class);
            pendingIntent = PendingIntent.getActivity(context,
                    0 /* no requestCode */, intent, 0 /* no flags */);
            views.setOnClickPendingIntent(R.id.music_info, pendingIntent);
            views.setOnClickPendingIntent(R.id.icon, pendingIntent);
        }

        intent = new Intent(MediaPlaybackService.PREVIOUS_ACTION);
        intent.setComponent(serviceName);
        pendingIntent = PendingIntent.getService(context,
                0 /* no requestCode */, intent, 0 /* no flags */);
        views.setOnClickPendingIntent(R.id.prev, pendingIntent);
        
        intent = new Intent(MediaPlaybackService.TOGGLEPAUSE_ACTION);
        intent.setComponent(serviceName);
        pendingIntent = PendingIntent.getService(context,
                0 /* no requestCode */, intent, 0 /* no flags */);
        views.setOnClickPendingIntent(R.id.pause, pendingIntent);
        
        intent = new Intent(MediaPlaybackService.NEXT_ACTION);
        intent.setComponent(serviceName);
        pendingIntent = PendingIntent.getService(context,
                0 /* no requestCode */, intent, 0 /* no flags */);
        views.setOnClickPendingIntent(R.id.next, pendingIntent);
    }
}
