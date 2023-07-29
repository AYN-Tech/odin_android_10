/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.

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

package com.android.soundrecorder.filelist;

import android.app.Activity;
import android.app.Fragment;
import android.os.Bundle;
import android.view.MenuItem;
import com.android.soundrecorder.R;
import com.android.soundrecorder.filelist.player.Player;
import com.android.soundrecorder.filelist.player.PlayerPanel;
import com.android.soundrecorder.util.PermissionUtils;

public class FileListActivity extends Activity {
    private Player mPlayer;
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.file_list_activity);
        PlayerPanel playerPanel = (PlayerPanel) findViewById(R.id.player_panel);
        mPlayer = new Player(getApplicationContext(), playerPanel);

        FileListFragment fragment = new FileListFragment();
        getFragmentManager().beginTransaction()
                .replace(android.R.id.content, fragment, FileListFragment.FRAGMENT_TAG)
                .commit();
    }

    /** This request result is from FileListFragment. */
    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions,
            int[] grantResults) {
        if (PermissionUtils.checkPermissionResult(permissions, grantResults)) {
            reloadFragmentAdapter();
        }
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case android.R.id.home:
                onBackPressed();
                return true;
            default:
                return super.onOptionsItemSelected(item);
        }
    }

    public void reloadFragmentAdapter() {
        Fragment fragment = getFragmentManager().findFragmentByTag(FileListFragment.FRAGMENT_TAG);
        if (fragment != null && fragment instanceof FileListFragment) {
            ((FileListFragment) fragment).reloadAdapter();
        }
    }

    public Player getPlayer() {
        return mPlayer;
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (getPlayer() != null) {
            getPlayer().pausePlayer();
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (getPlayer() != null) {
            getPlayer().stopPlayer();
        }
    }
}
