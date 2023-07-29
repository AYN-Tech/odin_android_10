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

import java.util.HashMap;

import android.app.Fragment;

import com.android.music.AlbumBrowserFragment;
import com.android.music.ArtistAlbumBrowserFragment;
import com.android.music.FolderBrowserFragment;
import com.android.music.MusicUtils;
import com.android.music.PlaylistBrowserFragment;
import com.android.music.TrackBrowserActivityFragment;
import com.android.music.TrackBrowserFragment;

public class FragmentsFactory {

    private static HashMap<Integer, Fragment> frg = new HashMap<Integer, Fragment>();

    static enum FragmentEnum {
        ARTIST_FRAG, ALBUM_FRAG, TRACK_FRAG, FOLDER_FRAG, PLAYLIST_FRAG, TRACK_BROWSER
    };

    public static Fragment loadFragment(int pos) {

        if (frg.get(pos) == null) {
            frg.put(pos, createFrag(pos));
        }
        return frg.get(pos);
    }

    private static Fragment createFrag(int position) {
        FragmentEnum frg = FragmentEnum.values()[position];
        switch (frg) {
        case ARTIST_FRAG:
            return new ArtistAlbumBrowserFragment();
        case ALBUM_FRAG:
            return new AlbumBrowserFragment();
        case TRACK_FRAG:
            return new TrackBrowserFragment();
        case FOLDER_FRAG:
            if (MusicUtils.isGroupByFolder()) {
                return new FolderBrowserFragment();
            }
            return new PlaylistBrowserFragment();
        case PLAYLIST_FRAG:
            return new PlaylistBrowserFragment();
        case TRACK_BROWSER:
            return new TrackBrowserActivityFragment();
        }
        return new ArtistAlbumBrowserFragment();
    }
}
