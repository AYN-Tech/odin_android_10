/*
 * Copyright (C) 2012 The Android Open Source Project
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
package com.android.dreams.phototable;

import android.content.res.Resources;
import android.content.SharedPreferences;
import android.service.dreams.DreamService;
import android.view.View;
import android.os.Build;

import java.util.Iterator;
import java.util.LinkedList;

/**
 * Example interactive screen saver: flick photos onto a table.
 */
public class PhotoTableDream extends DreamService {
    public static final String TAG = "PhotoTableDream";
    @Override
    public void onDreamingStarted() {
        super.onDreamingStarted();
        setInteractive(false);

        BummerView bummer = (BummerView) findViewById(R.id.bummer);
        if (bummer != null) {
            Resources resources = getResources();
            bummer.setAnimationParams(true,
                    resources.getInteger(R.integer.table_drop_period),
                    resources.getInteger(R.integer.fast_drop));
        }

        PhotoTable table = (PhotoTable) findViewById(R.id.table);
        if (table != null) {
            table.setDream(this);
        }
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();
        AlbumSettings settings = AlbumSettings.getAlbumSettings(
                getSharedPreferences(PhotoTableDreamSettings.PREFS_NAME, 0));
        // screensaver show blue photos after delete folders
        if (settings.isConfigured() && checkImageCount()) {
            setContentView(R.layout.table);
        } else {
            setContentView(R.layout.bummer);
        }
        setFullscreen(true);
        hideNavigationBar();
    }

    // Navigationbar display not clearly, hide it
    private void hideNavigationBar() {
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.HONEYCOMB
                &&  Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) {
            View v = this.getWindow().getDecorView();
            v.setSystemUiVisibility(View.GONE);
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            View decorView = getWindow().getDecorView();
            int uiOptions = View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_FULLSCREEN;
            decorView.setSystemUiVisibility(uiOptions);
        }
    }

    // screensaver show blue photos after delete folders
    private boolean checkImageCount() {
        SharedPreferences settings = getSharedPreferences(PhotoTableDream.TAG, 0);
        PhotoSourcePlexor photoSource = new PhotoSourcePlexor(this, settings);
        AlbumSettings albumSettings = AlbumSettings.getAlbumSettings(settings);
        LinkedList<PhotoSource.AlbumData> list = new LinkedList<PhotoSource.AlbumData>(
                photoSource.findAlbums());
        Iterator<PhotoSource.AlbumData> it = list.iterator();
        while (it.hasNext()) {
            if (albumSettings.isAlbumEnabled(it.next().id)) {
                return true;
            }
        }
        return false;
    }
}
