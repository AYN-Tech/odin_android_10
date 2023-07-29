/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.server.wifi;

import android.annotation.NonNull;
import android.content.Context;
import android.content.Intent;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiInfo;
import android.net.wifi.ScanResult;
import android.net.Network;
import android.net.NetworkInfo;
import android.net.LinkProperties;
import android.os.Looper;
import android.os.Message;
import android.os.UserHandle;
import android.text.TextUtils;
import android.util.Log;
import android.os.Handler;
import android.os.WorkSource;

import android.net.wifi.WifiConfiguration;
import com.android.internal.util.IState;
import com.android.internal.util.State;
import com.android.internal.util.StateMachine;
import com.android.internal.util.AsyncChannel;
import com.android.server.wifi.WifiNative.InterfaceCallback;
import com.android.server.wifi.util.WifiAsyncChannel;
import com.android.server.wifi.util.WifiHandler;

import android.net.wifi.hotspot2.IProvisioningCallback;
import android.net.wifi.hotspot2.OsuProvider;
import android.net.wifi.hotspot2.PasspointConfiguration;

import java.io.FileDescriptor;
import java.io.PrintWriter;

import java.util.List;
import java.util.Map;
/**
 * Manager WiFi in Client Mode where we connect to configured networks.
 */
public class QtiClientModeManager implements ActiveModeManager {
    private static final String TAG = "QtiWifiClientModeManager";

    private final ClientModeStateMachine mStateMachine;

    private final Context mContext;
    private final WifiNative mWifiNative;

    private final WifiInjector mWifiInjector;
    private final Listener mListener;

    private String mClientInterfaceName;
    private boolean mIfaceIsUp = false;

    private final QtiClientModeImplHandler mQtiClientModeImplHandler;
    private final QtiClientModeImpl mQtiClientModeImpl;
    private final int mStaId;
    /**
     * Asynchronous channel to QtiClientModeImpl
     */
    AsyncChannel mQtiClientModeImplChannel;

    QtiClientModeManager(Context context, @NonNull Looper looper, WifiNative wifiNative,
            Listener listener, WifiInjector wifiInjector, int staId) {
        mContext = context;
        mWifiNative = wifiNative;
        mListener = listener;
        mWifiInjector = wifiInjector;
        mStaId = staId;
        mQtiClientModeImpl = wifiInjector.makeQtiClientModeImpl(staId, mListener);
        mStateMachine = new ClientModeStateMachine(looper);
        mQtiClientModeImplHandler = new QtiClientModeImplHandler(TAG, looper, new WifiAsyncChannel(TAG));
    }

    public QtiClientModeImpl getClientModeImpl() {
        return mQtiClientModeImpl;
    }

    public int getStaId() {
        return mStaId;
    }

    public AsyncChannel getClientImplChannel() {
        return mQtiClientModeImplChannel;
    }

    /**
     * Start client mode.
     */
    public void start() {
        mQtiClientModeImpl.enableVerboseLogging(mWifiInjector.getVerboseLogging());
        mQtiClientModeImpl.start();
        mStateMachine.sendMessage(ClientModeStateMachine.CMD_START);
    }

    /**
     * Disconnect from any currently connected networks and stop client mode.
     */
    public void stop() {
        Log.d(TAG, " currentstate: " + getCurrentStateName());
        if (mClientInterfaceName != null) {
            if (mIfaceIsUp) {
                updateWifiState(WifiManager.WIFI_STATE_DISABLING);
            } else {
                updateWifiState(WifiManager.WIFI_STATE_DISABLING);
            }
        }
        mStateMachine.quitNow();
        mQtiClientModeImpl.quit();
    }

    public @ScanMode int getScanMode() {
        return SCAN_WITH_HIDDEN_NETWORKS;
    }

    /**
     * Dump info about this ClientMode manager.
     */
    public void dump(FileDescriptor fd, PrintWriter pw, String[] args) {
        pw.println("--Dump of QtiClientModeManager--");

        pw.println("current StateMachine mode: " + getCurrentStateName());
        pw.println("mClientInterfaceName: " + mClientInterfaceName);
        pw.println("mIfaceIsUp: " + mIfaceIsUp);
    }

    /**
     * Listener for ClientMode state changes.
     */
    public interface Listener {
        /**
         * Invoke when wifi state changes.
         * @param state new wifi state
         */
        void onStateChanged(int staId, int state);

        /**
         * Invoke when RSSI changes.
         * @param rssi new RSSI value.
         */
        void onRssiChanged(int staId, int rssi);

        /**
         * Invoke when Link configuration changes.
         * @param linkProperties Link Property object.
         */
        void onLinkConfigurationChanged(int staId, LinkProperties lp);

        /**
         * Invoke when network state changes.
         * @param networkInfo network info corresponding to bssid.
         */
        void onNetworkStateChanged(int staId, NetworkInfo netInfo);
    }

    private String getCurrentStateName() {
        IState currentState = mStateMachine.getCurrentState();

        if (currentState != null) {
            return currentState.getName();
        }

        return "StateMachine not active";
    }

    /**
     * Update Wifi state and send the broadcast.
     * @param newState new Wifi state
     */
    private void updateWifiState(int newState) {
        mListener.onStateChanged(mStaId, newState);
        mQtiClientModeImpl.setWifiStateForApiCalls(newState);
    }

    private class ClientModeStateMachine extends StateMachine {
        // Commands for the state machine.
        public static final int CMD_START = 0;
        public static final int CMD_INTERFACE_STATUS_CHANGED = 3;
        public static final int CMD_INTERFACE_DESTROYED = 4;
        public static final int CMD_INTERFACE_DOWN = 5;
        private final State mIdleState = new IdleState();
        private final State mStartedState = new StartedState();

        private final InterfaceCallback mWifiNativeInterfaceCallback = new InterfaceCallback() {
            @Override
            public void onDestroyed(String ifaceName) {
                if (mClientInterfaceName != null && mClientInterfaceName.equals(ifaceName)) {
                    Log.d(TAG, "STA iface " + ifaceName + " was destroyed, stopping client mode");

                    // we must immediately clean up state in ClientModeImpl to unregister
                    // all client mode related objects
                    // Note: onDestroyed is only called from the ClientModeImpl thread
                    mQtiClientModeImpl.handleIfaceDestroyed();

                    sendMessage(CMD_INTERFACE_DESTROYED);
                }
            }

            @Override
            public void onUp(String ifaceName) {
                if (mClientInterfaceName != null && mClientInterfaceName.equals(ifaceName)) {
                    sendMessage(CMD_INTERFACE_STATUS_CHANGED, 1);
                }
            }

            @Override
            public void onDown(String ifaceName) {
                if (mClientInterfaceName != null && mClientInterfaceName.equals(ifaceName)) {
                    sendMessage(CMD_INTERFACE_STATUS_CHANGED, 0);
                }
            }
        };

        ClientModeStateMachine(Looper looper) {
            super(TAG, looper);

            addState(mIdleState);
            addState(mStartedState);

            setInitialState(mIdleState);
            start();
        }

        private class IdleState extends State {

            @Override
            public void enter() {
                Log.d(TAG, "entering IdleState");
                mClientInterfaceName = null;
                mIfaceIsUp = false;
            }

            @Override
            public boolean processMessage(Message message) {
                switch (message.what) {
                    case CMD_START:
                        updateWifiState(WifiManager.WIFI_STATE_ENABLING);

                        mClientInterfaceName =
                                mWifiNative.setupInterfaceForClientInConnectivityMode(
                                mWifiNativeInterfaceCallback, /* lowPrioritySta */ true);
                        if (TextUtils.isEmpty(mClientInterfaceName)) {
                            Log.e(TAG, "Failed to create ClientInterface. Sit in Idle");
                            updateWifiState(WifiManager.WIFI_STATE_UNKNOWN);
                            updateWifiState(WifiManager.WIFI_STATE_DISABLED);
                            break;
                        }
                        transitionTo(mStartedState);
                        break;
                    default:
                        Log.d(TAG, "received an invalid message: " + message);
                        return NOT_HANDLED;
                }
                return HANDLED;
            }
        }

        private class StartedState extends State {

            private void onUpChanged(boolean isUp) {
                if (isUp == mIfaceIsUp) {
                    return;  // no change
                }
                mIfaceIsUp = isUp;
                if (isUp) {
                    Log.d(TAG, "Wifi is ready to use for client mode");
                    mQtiClientModeImpl.setOperationalMode(ClientModeImpl.CONNECT_MODE,
                                                       mClientInterfaceName);
                    updateWifiState(WifiManager.WIFI_STATE_ENABLED);
                } else {
                    if (mQtiClientModeImpl.isConnectedMacRandomizationEnabled()) {
                        // Handle the error case where our underlying interface went down if we
                        // do not have mac randomization enabled (b/72459123).
                        return;
                    }
                    // if the interface goes down we should exit and go back to idle state.
                    Log.d(TAG, "interface down!");
                    updateWifiState(WifiManager.WIFI_STATE_UNKNOWN);
                    mStateMachine.sendMessage(CMD_INTERFACE_DOWN);
                }
            }

            @Override
            public void enter() {
                Log.d(TAG, "entering StartedState");
                mIfaceIsUp = false;
                onUpChanged(mWifiNative.isInterfaceUp(mClientInterfaceName));
            }

            @Override
            public boolean processMessage(Message message) {
                switch(message.what) {
                    case CMD_START:
                        // Already started, ignore this command.
                        break;
                    case CMD_INTERFACE_DOWN:
                        Log.e(TAG, "Detected an interface down");
                        updateWifiState(WifiManager.WIFI_STATE_DISABLING);
                        transitionTo(mIdleState);
                        break;
                    case CMD_INTERFACE_STATUS_CHANGED:
                        boolean isUp = message.arg1 == 1;
                        onUpChanged(isUp);
                        break;
                    case CMD_INTERFACE_DESTROYED:
                        Log.d(TAG, "interface destroyed - client mode stopping");

                        updateWifiState(WifiManager.WIFI_STATE_DISABLING);
                        mClientInterfaceName = null;
                        transitionTo(mIdleState);
                        break;
                    default:
                        return NOT_HANDLED;
                }
                return HANDLED;
            }

            /**
             * Clean up state, unregister listeners and update wifi state.
             */
            @Override
            public void exit() {
                mQtiClientModeImpl.setOperationalMode(ClientModeImpl.DISABLED_MODE, null);

                if (mClientInterfaceName != null) {
                    mWifiNative.teardownInterface(mClientInterfaceName);
                    mClientInterfaceName = null;
                    mIfaceIsUp = false;
                }

                updateWifiState(WifiManager.WIFI_STATE_DISABLED);

                // once we leave started, nothing else to do...  stop the state machine
                mStateMachine.quitNow();
                mQtiClientModeImpl.quit();
            }
        }
    }

    /**
     * Handles interaction with QtiClientModeImpl for all sync calls which expects a reply.
     */
    private class QtiClientModeImplHandler extends WifiHandler {
        private AsyncChannel mCmiChannel;

        QtiClientModeImplHandler(String tag, Looper looper, AsyncChannel asyncChannel) {
            super(tag, looper);
            mCmiChannel = asyncChannel;
            mCmiChannel.connect(mContext, this, mQtiClientModeImpl.getHandler());
        }

        @Override
        public void handleMessage(Message msg) {
            super.handleMessage(msg);
            switch (msg.what) {
                case AsyncChannel.CMD_CHANNEL_HALF_CONNECTED: {
                    if (msg.arg1 == AsyncChannel.STATUS_SUCCESSFUL) {
                        mQtiClientModeImplChannel = mCmiChannel;
                    } else {
                        Log.e(TAG, "ClientModeImpl connection failure, error=" + msg.arg1);
                        mQtiClientModeImplChannel = null;
                    }
                    break;
                }
                case AsyncChannel.CMD_CHANNEL_DISCONNECTED: {
                    Log.e(TAG, "ClientModeImpl channel lost, msg.arg1 =" + msg.arg1);
                    mQtiClientModeImplChannel = null;
                    //Re-establish connection to state machine
                    mCmiChannel.connect(mContext, this, mQtiClientModeImpl.getHandler());
                    break;
                }
                default: {
                    Log.d(TAG, "QtiClientModeImplHandler.handleMessage ignoring msg=" + msg);
                    break;
                }
            }
        }
    }
}
