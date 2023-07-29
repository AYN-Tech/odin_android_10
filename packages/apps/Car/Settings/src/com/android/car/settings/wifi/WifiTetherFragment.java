/*
 * Copyright (C) 2019 The Android Open Source Project
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
package com.android.car.settings.wifi;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.widget.ProgressBar;

import androidx.annotation.XmlRes;

import com.android.car.settings.R;
import com.android.car.settings.common.SettingsFragment;
import com.android.car.ui.toolbar.MenuItem;

import java.util.Collections;
import java.util.List;

/**
 * Fragment to host tethering-related preferences.
 */
public class WifiTetherFragment extends SettingsFragment {

    private CarWifiManager mCarWifiManager;
    private ConnectivityManager mConnectivityManager;
    private ProgressBar mProgressBar;
    private MenuItem mTetherSwitch;

    private final ConnectivityManager.OnStartTetheringCallback mOnStartTetheringCallback =
            new ConnectivityManager.OnStartTetheringCallback() {
                @Override
                public void onTetheringFailed() {
                    super.onTetheringFailed();
                    mTetherSwitch.setChecked(false);
                    mTetherSwitch.setEnabled(true);
                }
            };

    @Override
    public List<MenuItem> getToolbarMenuItems() {
        return Collections.singletonList(mTetherSwitch);
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mTetherSwitch = new MenuItem.Builder(getContext())
                .setCheckable()
                .setChecked(mCarWifiManager.isWifiApEnabled())
                .setOnClickListener(i -> {
                    if (!mTetherSwitch.isChecked()) {
                        mConnectivityManager.stopTethering(ConnectivityManager.TETHERING_WIFI);
                    } else {
                        mConnectivityManager.startTethering(ConnectivityManager.TETHERING_WIFI,
                                /* showProvisioningUi= */ true,
                                mOnStartTetheringCallback, new Handler(Looper.getMainLooper()));
                    }
                })
                .build();
    }

    @Override
    @XmlRes
    protected int getPreferenceScreenResId() {
        return R.xml.wifi_tether_fragment;
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);

        mCarWifiManager = new CarWifiManager(context);
        mConnectivityManager = (ConnectivityManager) getContext().getSystemService(
                Context.CONNECTIVITY_SERVICE);
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        mProgressBar = getToolbar().getProgressBar();
    }

    @Override
    public void onStart() {
        super.onStart();

        mCarWifiManager.start();
        getContext().registerReceiver(mReceiver,
                new IntentFilter(WifiManager.WIFI_AP_STATE_CHANGED_ACTION));
    }

    @Override
    public void onStop() {
        super.onStop();
        mCarWifiManager.stop();
        getContext().unregisterReceiver(mReceiver);
        mProgressBar.setVisibility(View.GONE);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mCarWifiManager.destroy();
    }

    private final BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            final int state = intent.getIntExtra(
                    WifiManager.EXTRA_WIFI_AP_STATE, WifiManager.WIFI_AP_STATE_FAILED);
            handleWifiApStateChanged(state);
        }
    };

    private void handleWifiApStateChanged(int state) {
        switch (state) {
            case WifiManager.WIFI_AP_STATE_ENABLING:
                mTetherSwitch.setEnabled(false);
                break;
            case WifiManager.WIFI_AP_STATE_ENABLED:
                mTetherSwitch.setEnabled(true);
                if (!mTetherSwitch.isChecked()) {
                    mTetherSwitch.setChecked(true);
                }
                break;
            case WifiManager.WIFI_AP_STATE_DISABLING:
                mTetherSwitch.setEnabled(false);
                if (mTetherSwitch.isChecked()) {
                    mTetherSwitch.setChecked(false);
                }
                break;
            case WifiManager.WIFI_AP_STATE_DISABLED:
                mTetherSwitch.setChecked(false);
                mTetherSwitch.setEnabled(true);
                break;
            default:
                mTetherSwitch.setChecked(false);
                mTetherSwitch.setEnabled(true);
                break;
        }
    }
}
