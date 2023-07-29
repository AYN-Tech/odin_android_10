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

package com.android.bluetooth.hfp;

import static android.Manifest.permission.MODIFY_PHONE_STATE;

import android.annotation.Nullable;
import android.bluetooth.BluetoothA2dp;
import android.bluetooth.BluetoothDevice;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothHeadset;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothUuid;
import android.bluetooth.IBluetoothHeadset;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.media.AudioManager;
import android.net.Uri;
import android.os.BatteryManager;
import android.os.HandlerThread;
import android.os.IDeviceIdleController;
import android.os.Looper;
import android.os.ParcelUuid;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.os.SystemProperties;
import android.os.UserHandle;
import android.telecom.PhoneAccount;
import android.util.Log;
import android.util.StatsLog;

import com.android.bluetooth.BluetoothMetricsProto;
import com.android.bluetooth.Utils;
import com.android.bluetooth.btservice.AdapterService;
import com.android.bluetooth.btservice.MetricsLogger;
import com.android.bluetooth.btservice.ProfileService;
import com.android.internal.annotations.VisibleForTesting;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.concurrent.Executor;
import java.util.HashMap;
import java.util.List;
import java.util.Objects;
import android.telecom.TelecomManager;


/**
 * Provides Bluetooth Headset and Handsfree profile, as a service in the Bluetooth application.
 *
 * Three modes for SCO audio:
 * Mode 1: Telecom call through {@link #phoneStateChanged(int, int, int, String, int, String,
 *         boolean)}
 * Mode 2: Virtual call through {@link #startScoUsingVirtualVoiceCall()}
 * Mode 3: Voice recognition through {@link #startVoiceRecognition(BluetoothDevice)}
 *
 * When one mode is active, other mode cannot be started. API user has to terminate existing modes
 * using the correct API or just {@link #disconnectAudio()} if user is a system service, before
 * starting a new mode.
 *
 * {@link #connectAudio()} will start SCO audio at one of the above modes, but won't change mode
 * {@link #disconnectAudio()} can happen in any mode to disconnect SCO
 *
 * When audio is disconnected, only Mode 1 Telecom call will be persisted, both Mode 2 virtual call
 * and Mode 3 voice call will be terminated upon SCO termination and client has to restart the mode.
 *
 * NOTE: SCO termination can either be initiated on the AG side or the HF side
 * TODO(b/79660380): As a workaround, voice recognition will be terminated if virtual call or
 * Telecom call is initiated while voice recognition is ongoing, in case calling app did not call
 * {@link #stopVoiceRecognition(BluetoothDevice)}
 *
 * AG - Audio Gateway, device running this {@link HeadsetService}, e.g. Android Phone
 * HF - Handsfree device, device running headset client, e.g. Wireless headphones or car kits
 */
public class HeadsetService extends ProfileService {
    private static final String TAG = "HeadsetService";
    private static final boolean DBG = true;
    private static final String DISABLE_INBAND_RINGING_PROPERTY =
            "persist.bluetooth.disableinbandringing";
    private static final ParcelUuid[] HEADSET_UUIDS = {BluetoothUuid.HSP, BluetoothUuid.Handsfree};
    private static final int[] CONNECTING_CONNECTED_STATES =
            {BluetoothProfile.STATE_CONNECTING, BluetoothProfile.STATE_CONNECTED};
    private static final int DIALING_OUT_TIMEOUT_MS = 10000;

    private int mMaxHeadsetConnections = 1;
    private int mSetMaxConfig;
    private BluetoothDevice mActiveDevice;
    private AdapterService mAdapterService;
    private HandlerThread mStateMachinesThread;
    // This is also used as a lock for shared data in HeadsetService
    private final HashMap<BluetoothDevice, HeadsetStateMachine> mStateMachines = new HashMap<>();
    private HeadsetNativeInterface mNativeInterface;
    private HeadsetSystemInterface mSystemInterface;
    private HeadsetA2dpSync mHfpA2dpSyncInterface;
    private boolean mAudioRouteAllowed = true;
    // Indicates whether SCO audio needs to be forced to open regardless ANY OTHER restrictions
    private boolean mForceScoAudio;
    private boolean mInbandRingingRuntimeDisable;
    private boolean mVirtualCallStarted;
    // Non null value indicates a pending dialing out event is going on
    private DialingOutTimeoutEvent mDialingOutTimeoutEvent;
    private boolean mVoiceRecognitionStarted;
    // Non null value indicates a pending voice recognition request from headset is going on
    private VoiceRecognitionTimeoutEvent mVoiceRecognitionTimeoutEvent;
    // Timeout when voice recognition is started by remote device
    @VisibleForTesting static int sStartVrTimeoutMs = 5000;
    private boolean mStarted;
    private boolean mCreated;
    private static HeadsetService sHeadsetService;
    private boolean mDisconnectAll;
    private boolean mIsTwsPlusEnabled = false;
    private boolean mIsTwsPlusShoEnabled = false;
    private vendorhfservice  mVendorHf;
    private Context mContext = null;
    private AudioServerStateCallback mServerStateCallback = new AudioServerStateCallback();

    @Override
    public IProfileServiceBinder initBinder() {
        return new BluetoothHeadsetBinder(this);
    }

    @Override
    protected void create() {
        Log.i(TAG, "create()");
        if (mCreated) {
            throw new IllegalStateException("create() called twice");
        }
        mCreated = true;
        mVendorHf = new vendorhfservice(this);
    }

    @Override
    protected boolean start() {
        Log.i(TAG, "start()");
        if (sHeadsetService != null) {
            Log.w(TAG, "HeadsetService is already running");
            return true;
        }
        // Step 1: Get adapter service, should never be null
        mAdapterService = Objects.requireNonNull(AdapterService.getAdapterService(),
                "AdapterService cannot be null when HeadsetService starts");
        // Step 2: Start handler thread for state machines
        mStateMachinesThread = new HandlerThread("HeadsetService.StateMachines");
        mStateMachinesThread.start();
        // Step 3: Initialize system interface
        mSystemInterface = HeadsetObjectsFactory.getInstance().makeSystemInterface(this);
        mSystemInterface.init();
        // Step 4: Initialize native interface
        mSetMaxConfig = mMaxHeadsetConnections = mAdapterService.getMaxConnectedAudioDevices();
        if(mAdapterService.isVendorIntfEnabled()) {
            String twsPlusEnabled = SystemProperties.get("persist.vendor.btstack.enable.twsplus");
            String twsPlusShoEnabled =
               SystemProperties.get("persist.vendor.btstack.enable.twsplussho");

            if (!twsPlusEnabled.isEmpty() && "true".equals(twsPlusEnabled)) {
                mIsTwsPlusEnabled = true;
            }
            if (!twsPlusShoEnabled.isEmpty() && "true".equals(twsPlusShoEnabled)) {
                if (mIsTwsPlusEnabled) {
                    mIsTwsPlusShoEnabled = true;
                } else {
                    Log.e(TAG, "no TWS+ SHO without TWS+ support!");
                    mIsTwsPlusShoEnabled = false;
                }
            }
            Log.i(TAG, "mIsTwsPlusEnabled: " + mIsTwsPlusEnabled);
            Log.i(TAG, "mIsTwsPlusShoEnabled: " + mIsTwsPlusShoEnabled);
            if (mIsTwsPlusEnabled && mMaxHeadsetConnections < 2){
               //set MaxConn to 2 if TWSPLUS enabled
               mMaxHeadsetConnections = 2;
            }
            if (mIsTwsPlusShoEnabled &&  mMaxHeadsetConnections < 3) {
               //set MaxConn to 3 if TWSPLUS enabled
               mMaxHeadsetConnections = 3;
            }
            //Only if the User set config 1 and TWS+ is enabled leaves
            //these maxConn at 2 and setMaxConfig to 1. this is to avoid
            //connecting to more than 1 legacy device even though max conns
            //is set 2 or 3because of TWS+ requirement
            Log.d(TAG, "Max_HFP_Connections  " + mMaxHeadsetConnections);
            Log.d(TAG, "mSetMaxConfig  " + mSetMaxConfig);
        }

        mNativeInterface = HeadsetObjectsFactory.getInstance().getNativeInterface();
        // Add 1 to allow a pending device to be connecting or disconnecting
        mNativeInterface.init(mMaxHeadsetConnections + 1, isInbandRingingEnabled());
        // Step 5: Check if state machine table is empty, crash if not
        if (mStateMachines.size() > 0) {
            throw new IllegalStateException(
                    "start(): mStateMachines is not empty, " + mStateMachines.size()
                            + " is already created. Was stop() called properly?");
        }
        // Step 6: Setup broadcast receivers
        IntentFilter filter = new IntentFilter();
        filter.addAction(Intent.ACTION_BATTERY_CHANGED);
        filter.addAction(AudioManager.VOLUME_CHANGED_ACTION);
        filter.addAction(BluetoothDevice.ACTION_CONNECTION_ACCESS_REPLY);
        filter.addAction(BluetoothDevice.ACTION_BOND_STATE_CHANGED);
        filter.addAction(BluetoothA2dp.ACTION_PLAYING_STATE_CHANGED);
        filter.addAction(BluetoothA2dp.ACTION_CONNECTION_STATE_CHANGED);
        filter.addAction(TelecomManager.ACTION_CALL_TYPE);
        registerReceiver(mHeadsetReceiver, filter);
        // Step 7: Mark service as started

        setHeadsetService(this);
        mStarted = true;

        mHfpA2dpSyncInterface = new HeadsetA2dpSync(mSystemInterface, this);
        if (mVendorHf != null) {
            mVendorHf.init();
            mVendorHf.enableSwb(isSwbEnabled());
        }

        Log.d(TAG, "registering audio server state callback");
        mContext = getApplicationContext();
        Executor exec = mContext.getMainExecutor();
        mSystemInterface.getAudioManager().setAudioServerStateCallback(exec, mServerStateCallback);

        Log.i(TAG, " HeadsetService Started ");
        return true;
    }

    @Override
    protected boolean stop() {
        Log.i(TAG, "stop()");
        if (!mStarted) {
            Log.w(TAG, "stop() called before start()");
            // Still return true because it is considered "stopped" and doesn't have any functional
            // impact on the user
            return true;
        }
        // Step 7: Mark service as stopped
        mStarted = false;
        setHeadsetService(null);
        // Step 6: Tear down broadcast receivers
        unregisterReceiver(mHeadsetReceiver);
        synchronized (mStateMachines) {
            // Reset active device to null
            mActiveDevice = null;
            mInbandRingingRuntimeDisable = false;
            mForceScoAudio = false;
            mAudioRouteAllowed = true;
            if(mAdapterService.isVendorIntfEnabled()) {
                //to enable TWS
                if (mIsTwsPlusEnabled) {
                    mMaxHeadsetConnections = 2;
                } else {
                   mMaxHeadsetConnections = 1;
                }
            } else {
                mMaxHeadsetConnections = 1;
            }
            mVoiceRecognitionStarted = false;
            mVirtualCallStarted = false;
            if (mDialingOutTimeoutEvent != null) {
                if (mStateMachinesThread != null) {
                    mStateMachinesThread.getThreadHandler()
                      .removeCallbacks(mDialingOutTimeoutEvent);
                }
                mDialingOutTimeoutEvent = null;
            }
            if (mVoiceRecognitionTimeoutEvent != null) {
                if (mStateMachinesThread != null) {
                    mStateMachinesThread.getThreadHandler()
                        .removeCallbacks(mVoiceRecognitionTimeoutEvent);
                }
                mVoiceRecognitionTimeoutEvent = null;
                if (mSystemInterface.getVoiceRecognitionWakeLock().isHeld()) {
                    mSystemInterface.getVoiceRecognitionWakeLock().release();
                }
            }
            // Step 5: Destroy state machines
            for (HeadsetStateMachine stateMachine : mStateMachines.values()) {
                HeadsetObjectsFactory.getInstance().destroyStateMachine(stateMachine);
            }
            mStateMachines.clear();
        }
        // Step 4: Destroy native interface
        mNativeInterface.cleanup();
        // Step 3: Destroy system interface
        mSystemInterface.stop();
        synchronized (mStateMachines) {
            // Step 2: Stop handler thread
            if (mStateMachinesThread != null) {
                mStateMachinesThread.quitSafely();
            }
            mStateMachinesThread = null;
            // Step 1: Clear
            mAdapterService = null;
        }
        return true;
    }

    @Override
    protected void cleanup() {
        Log.i(TAG, "cleanup");
        if (!mCreated) {
            Log.w(TAG, "cleanup() called before create()");
        }
        if (mVendorHf != null) {
            mVendorHf.cleanup();
        }
        mCreated = false;
    }

    /**
     * Checks if this service object is able to accept binder calls
     *
     * @return True if the object can accept binder calls, False otherwise
     */
    public boolean isAlive() {
        return isAvailable() && mCreated && mStarted;
    }

    /**
     * Get the {@link Looper} for the state machine thread. This is used in testing and helper
     * objects
     *
     * @return {@link Looper} for the state machine thread
     */
    @VisibleForTesting
    public Looper getStateMachinesThreadLooper() {
        if (mStateMachinesThread != null) {
            return mStateMachinesThread.getLooper();
        }
        return null;
    }

    interface StateMachineTask {
        void execute(HeadsetStateMachine stateMachine);
    }

    // send message to all statemachines in connecting and connected state.
    private void doForEachConnectedConnectingStateMachine(StateMachineTask task) {
        synchronized (mStateMachines) {
            for (BluetoothDevice device :
                         getDevicesMatchingConnectionStates(CONNECTING_CONNECTED_STATES)) {
                HeadsetStateMachine stateMachine = mStateMachines.get(device);
                if (stateMachine == null) {
                    continue;
                }
                task.execute(stateMachine);
            }
        }
    }

    private boolean doForStateMachine(BluetoothDevice device, StateMachineTask task) {
        synchronized (mStateMachines) {
            HeadsetStateMachine stateMachine = mStateMachines.get(device);
            if (stateMachine == null) {
                return false;
            }
            task.execute(stateMachine);
        }
        return true;
    }

    private void doForEachConnectedStateMachine(StateMachineTask task) {
        synchronized (mStateMachines) {
            for (BluetoothDevice device : getConnectedDevices()) {
                HeadsetStateMachine stateMachine = mStateMachines.get(device);
                if (stateMachine == null) {
                    continue;
                }
                task.execute(stateMachine);
            }
        }
    }

    void onDeviceStateChanged(HeadsetDeviceState deviceState) {
        synchronized (mStateMachines) {
            doForEachConnectedStateMachine(
                stateMachine -> stateMachine.sendMessage(HeadsetStateMachine.DEVICE_STATE_CHANGED,
                        deviceState));
        }
    }

    /**
     * Handle messages from native (JNI) to Java. This needs to be synchronized to avoid posting
     * messages to state machine before start() is done
     *
     * @param stackEvent event from native stack
     */
    void messageFromNative(HeadsetStackEvent stackEvent) {
        Objects.requireNonNull(stackEvent.device,
                "Device should never be null, event: " + stackEvent);
        synchronized (mStateMachines) {
            HeadsetStateMachine stateMachine = mStateMachines.get(stackEvent.device);
            if (stackEvent.type == HeadsetStackEvent.EVENT_TYPE_CONNECTION_STATE_CHANGED) {
                switch (stackEvent.valueInt) {
                    case HeadsetHalConstants.CONNECTION_STATE_CONNECTED:
                    case HeadsetHalConstants.CONNECTION_STATE_CONNECTING: {
                        // Create new state machine if none is found
                        if (stateMachine == null) {
                            if (mStateMachinesThread != null) {
                                stateMachine = HeadsetObjectsFactory.getInstance()
                                    .makeStateMachine(stackEvent.device,
                                            mStateMachinesThread.getLooper(), this, mAdapterService,
                                            mNativeInterface, mSystemInterface);
                                mStateMachines.put(stackEvent.device, stateMachine);
                            } else {
                                Log.w(TAG, "messageFromNative: mStateMachinesThread is null");
                            }
                        }
                        break;
                    }
                }
            }
            if (stateMachine == null) {
                throw new IllegalStateException(
                        "State machine not found for stack event: " + stackEvent);
            }
            stateMachine.sendMessage(HeadsetStateMachine.STACK_EVENT, stackEvent);
        }
    }

    private class AudioServerStateCallback extends AudioManager.AudioServerStateCallback {
        @Override
        public void onAudioServerDown() {
            Log.d(TAG, "notifying onAudioServerDown");
        }

        @Override
        public void onAudioServerUp() {
            Log.d(TAG, "notifying onAudioServerUp");
            if (isAudioOn()) {
                Log.d(TAG, "onAudioServerUp: Audio is On, Notify HeadsetStateMachine");
                synchronized (mStateMachines) {
                    for (HeadsetStateMachine stateMachine : mStateMachines.values()) {
                        if (stateMachine.getAudioState()
                                == BluetoothHeadset.STATE_AUDIO_CONNECTED) {
                            stateMachine.onAudioServerUp();
                            break;
                        }
                    }
                }
            }
        }
    }

    private final BroadcastReceiver mHeadsetReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (action == null) {
                Log.w(TAG, "mHeadsetReceiver, action is null");
                return;
            }
            switch (action) {
                case Intent.ACTION_BATTERY_CHANGED: {
                    int batteryLevel = intent.getIntExtra(BatteryManager.EXTRA_LEVEL, -1);
                    int scale = intent.getIntExtra(BatteryManager.EXTRA_SCALE, -1);
                    if (batteryLevel < 0 || scale <= 0) {
                        Log.e(TAG, "Bad Battery Changed intent: batteryLevel=" + batteryLevel
                                + ", scale=" + scale);
                        return;
                    }
                    int cindBatteryLevel = Math.round(batteryLevel * 5 / ((float) scale));
                    mSystemInterface.getHeadsetPhoneState().setCindBatteryCharge(cindBatteryLevel);
                    break;
                }
                case AudioManager.VOLUME_CHANGED_ACTION: {
                    int streamType = intent.getIntExtra(AudioManager.EXTRA_VOLUME_STREAM_TYPE, -1);
                    if (streamType == AudioManager.STREAM_BLUETOOTH_SCO) {
                       synchronized (mStateMachines) {
                          doForEachConnectedStateMachine(stateMachine -> stateMachine.sendMessage(
                                HeadsetStateMachine.INTENT_SCO_VOLUME_CHANGED, intent));
                       }
                    }
                    break;
                }
                case BluetoothDevice.ACTION_CONNECTION_ACCESS_REPLY: {
                    int requestType = intent.getIntExtra(BluetoothDevice.EXTRA_ACCESS_REQUEST_TYPE,
                            BluetoothDevice.REQUEST_TYPE_PHONEBOOK_ACCESS);
                    BluetoothDevice device =
                            intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
                    logD("Received BluetoothDevice.ACTION_CONNECTION_ACCESS_REPLY, device=" + device
                            + ", type=" + requestType);
                    if (requestType == BluetoothDevice.REQUEST_TYPE_PHONEBOOK_ACCESS) {
                        synchronized (mStateMachines) {
                            final HeadsetStateMachine stateMachine = mStateMachines.get(device);
                            if (stateMachine == null) {
                                Log.wtfStack(TAG, "Cannot find state machine for " + device);
                                return;
                            }
                            stateMachine.sendMessage(
                                    HeadsetStateMachine.INTENT_CONNECTION_ACCESS_REPLY, intent);
                        }
                    }
                    break;
                }
                case BluetoothDevice.ACTION_BOND_STATE_CHANGED: {
                    int state = intent.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE,
                            BluetoothDevice.ERROR);
                    BluetoothDevice device = Objects.requireNonNull(
                            intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE),
                            "ACTION_BOND_STATE_CHANGED with no EXTRA_DEVICE");
                    logD("Bond state changed for device: " + device + " state: " + state);
                    if (state != BluetoothDevice.BOND_NONE) {
                        break;
                    }
                    synchronized (mStateMachines) {
                        HeadsetStateMachine stateMachine = mStateMachines.get(device);
                        if (stateMachine == null) {
                            break;
                        }
                        if (stateMachine.getConnectionState()
                                != BluetoothProfile.STATE_DISCONNECTED) {
                            break;
                        }
                        removeStateMachine(device);
                    }
                    break;
                }
                case BluetoothA2dp.ACTION_PLAYING_STATE_CHANGED: {
                    logD("Received BluetoothA2dp Play State changed");
                    mHfpA2dpSyncInterface.updateA2DPPlayingState(intent);
                    break;
                }
                case BluetoothA2dp.ACTION_CONNECTION_STATE_CHANGED: {
                    logD("Received BluetoothA2dp Connection State changed");
                    mHfpA2dpSyncInterface.updateA2DPConnectionState(intent);
                    break;
                }
                case TelecomManager.ACTION_CALL_TYPE: {
                    logD("Received BluetoothHeadset action call type");
                    synchronized (mStateMachines) {
                        doForEachConnectedStateMachine(stateMachine -> stateMachine.sendMessage(
                            HeadsetStateMachine.UPDATE_CALL_TYPE, intent));
                    }
                }
                default:
                    Log.w(TAG, "Unknown action " + action);
            }
        }
    };

    /**
     * Handlers for incoming service calls
     */
    private static class BluetoothHeadsetBinder extends IBluetoothHeadset.Stub
            implements IProfileServiceBinder {
        private volatile HeadsetService mService;

        BluetoothHeadsetBinder(HeadsetService svc) {
            mService = svc;
        }

        @Override
        public void cleanup() {
            mService = null;
        }

        private HeadsetService getService() {
            final HeadsetService service = mService;
            if (!Utils.checkCallerAllowManagedProfiles(service)) {
                Log.w(TAG, "Headset call not allowed for non-active user");
                return null;
            }
            if (service == null) {
                Log.w(TAG, "Service is null");
                return null;
            }
            if (!service.isAlive()) {
                Log.w(TAG, "Service is not alive");
                return null;
            }
            return service;
        }

        @Override
        public boolean connect(BluetoothDevice device) {
            HeadsetService service = getService();
            if (service == null) {
                return false;
            }
            return service.connect(device);
        }

        @Override
        public boolean disconnect(BluetoothDevice device) {
            HeadsetService service = getService();
            if (service == null) {
                return false;
            }
            return service.disconnect(device);
        }

        @Override
        public List<BluetoothDevice> getConnectedDevices() {
            HeadsetService service = getService();
            if (service == null) {
                return new ArrayList<BluetoothDevice>(0);
            }
            return service.getConnectedDevices();
        }

        @Override
        public List<BluetoothDevice> getDevicesMatchingConnectionStates(int[] states) {
            HeadsetService service = getService();
            if (service == null) {
                return new ArrayList<BluetoothDevice>(0);
            }
            return service.getDevicesMatchingConnectionStates(states);
        }

        public List<BluetoothDevice> getAllDevicesMatchingConnectionStates(int[] states) {
            HeadsetService service = getService();
            if (service == null) {
                return new ArrayList<BluetoothDevice>(0);
            }
            return service.getAllDevicesMatchingConnectionStates(states);
        }


        @Override
        public int getConnectionState(BluetoothDevice device) {
            HeadsetService service = getService();
            if (service == null) {
                return BluetoothProfile.STATE_DISCONNECTED;
            }
            return service.getConnectionState(device);
        }

        @Override
        public boolean setPriority(BluetoothDevice device, int priority) {
            HeadsetService service = getService();
            if (service == null) {
                return false;
            }
            return service.setPriority(device, priority);
        }

        @Override
        public int getPriority(BluetoothDevice device) {
            HeadsetService service = getService();
            if (service == null) {
                return BluetoothProfile.PRIORITY_UNDEFINED;
            }
            return service.getPriority(device);
        }

        @Override
        public boolean startVoiceRecognition(BluetoothDevice device) {
            HeadsetService service = getService();
            if (service == null) {
                return false;
            }
            return service.startVoiceRecognition(device);
        }

        @Override
        public boolean stopVoiceRecognition(BluetoothDevice device) {
            HeadsetService service = getService();
            if (service == null) {
                return false;
            }
            return service.stopVoiceRecognition(device);
        }

        @Override
        public boolean isAudioOn() {
            HeadsetService service = getService();
            if (service == null) {
                return false;
            }
            return service.isAudioOn();
        }

        @Override
        public boolean isAudioConnected(BluetoothDevice device) {
            HeadsetService service = getService();
            if (service == null) {
                return false;
            }
            return service.isAudioConnected(device);
        }

        @Override
        public int getAudioState(BluetoothDevice device) {
            HeadsetService service = getService();
            if (service == null) {
                return BluetoothHeadset.STATE_AUDIO_DISCONNECTED;
            }
            return service.getAudioState(device);
        }

        @Override
        public boolean connectAudio() {
            HeadsetService service = getService();
            if (service == null) {
                return false;
            }
            return service.connectAudio();
        }

        @Override
        public boolean disconnectAudio() {
            HeadsetService service = getService();
            if (service == null) {
                return false;
            }
            return service.disconnectAudio();
        }

        @Override
        public void setAudioRouteAllowed(boolean allowed) {
            HeadsetService service = getService();
            if (service == null) {
                return;
            }
            service.setAudioRouteAllowed(allowed);
        }

        @Override
        public boolean getAudioRouteAllowed() {
            HeadsetService service = getService();
            if (service != null) {
                return service.getAudioRouteAllowed();
            }
            return false;
        }

        @Override
        public void setForceScoAudio(boolean forced) {
            HeadsetService service = getService();
            if (service == null) {
                return;
            }
            service.setForceScoAudio(forced);
        }

        @Override
        public boolean startScoUsingVirtualVoiceCall() {
            HeadsetService service = getService();
            if (service == null) {
                return false;
            }
            return service.startScoUsingVirtualVoiceCall();
        }

        @Override
        public boolean stopScoUsingVirtualVoiceCall() {
            HeadsetService service = getService();
            if (service == null) {
                return false;
            }
            return service.stopScoUsingVirtualVoiceCall();
        }

        @Override
        public void phoneStateChanged(int numActive, int numHeld, int callState, String number,
                int type, String name) {
            HeadsetService service = getService();
            if (service == null) {
                return;
            }
            service.phoneStateChanged(numActive, numHeld, callState, number, type, name, false);
        }

        @Override
        public void clccResponse(int index, int direction, int status, int mode, boolean mpty,
                String number, int type) {
            HeadsetService service = getService();
            if (service == null) {
                return;
            }
            service.clccResponse(index, direction, status, mode, mpty, number, type);
        }

        @Override
        public boolean sendVendorSpecificResultCode(BluetoothDevice device, String command,
                String arg) {
            HeadsetService service = getService();
            if (service == null) {
                return false;
            }
            return service.sendVendorSpecificResultCode(device, command, arg);
        }

        @Override
        public boolean setActiveDevice(BluetoothDevice device) {
            HeadsetService service = getService();
            if (service == null) {
                return false;
            }
            return service.setActiveDevice(device);
        }

        @Override
        public BluetoothDevice getActiveDevice() {
            HeadsetService service = getService();
            if (service == null) {
                return null;
            }
            return service.getActiveDevice();
        }

        @Override
        public boolean isInbandRingingEnabled() {
            HeadsetService service = getService();
            if (service == null) {
                return false;
            }
            return service.isInbandRingingEnabled();
        }
    }

    // API methods
    public static synchronized HeadsetService getHeadsetService() {
        if (sHeadsetService == null) {
            Log.w(TAG, "getHeadsetService(): service is NULL");
            return null;
        }
        if (!sHeadsetService.isAvailable()) {
            Log.w(TAG, "getHeadsetService(): service is not available");
            return null;
        }
        logD("getHeadsetService(): returning " + sHeadsetService);
        return sHeadsetService;
    }

    private static synchronized void setHeadsetService(HeadsetService instance) {
        logD("setHeadsetService(): set to: " + instance);
        sHeadsetService = instance;
    }

    public BluetoothDevice getTwsPlusConnectedPeer(BluetoothDevice device) {
        AdapterService adapterService = AdapterService.getAdapterService();
        if (!adapterService.isTwsPlusDevice(device)) {
            logD("getTwsPlusConnectedPeer: Not a TWS+ device");
            return null;
        }
        List<BluetoothDevice> connDevices = getConnectedDevices();

        int size = connDevices.size();
        for(int i = 0; i < size; i++) {
            BluetoothDevice ConnectedDevice = connDevices.get(i);
            if (adapterService.getTwsPlusPeerAddress(device).equals(ConnectedDevice.getAddress())) {
                return ConnectedDevice;
            }
        }
        return null;
    }

    private BluetoothDevice getConnectedOrConnectingTwspDevice() {
        List<BluetoothDevice> connDevices =
            getAllDevicesMatchingConnectionStates(CONNECTING_CONNECTED_STATES);
        int size = connDevices.size();
        for(int i = 0; i < size; i++) {
            BluetoothDevice ConnectedDevice = connDevices.get(i);
            if (mAdapterService.isTwsPlusDevice(ConnectedDevice)) {
                logD("getConnectedorConnectingTwspDevice: found" + ConnectedDevice);
                return ConnectedDevice;
            }
        }
        return null;
    }

    /*
     * This function determines possible connections allowed with both Legacy
     * TWS+ earbuds.
     * N is the maximum audio connections set (defaults to 5)
     * In TWS+ connections
     *    - There can be only ONE set of TWS+ earbud connected at any point
     *      of time
     *    - Once one of the TWS+ earbud is connected, another slot will be
     *      reserved for the TWS+ peer earbud. Hence there can be maximum
     *      of N-2 legacy device connections when an earbud is connected.
     *    - If user wants to connect to another TWS+ earbud set. Existing TWS+
     *      connection need to be removed explicitly
     * In Legacy(Non-TWS+) Connections
     *    -Maximum allowed connections N set by user is used to determine number
     * of Legacy connections
     */
    private boolean isConnectionAllowed(BluetoothDevice device,
                                           List<BluetoothDevice> connDevices
                                           ) {
        AdapterService adapterService = AdapterService.getAdapterService();
        boolean allowSecondHfConnection = false;
        int reservedSlotForTwspPeer = 0;

        if (!mIsTwsPlusEnabled && adapterService.isTwsPlusDevice(device)) {
           logD("No TWSPLUS connections as It is not Enabled");
           return false;
        }

        if (connDevices.size() == 0) {
            allowSecondHfConnection = true;
        } else {
            BluetoothDevice connectedOrConnectingTwspDev =
                    getConnectedOrConnectingTwspDevice();
            if (connectedOrConnectingTwspDev != null) {
                // There is TWSP connected earbud
                if (adapterService.isTwsPlusDevice(device)) {
                   if (adapterService.getTwsPlusPeerAddress
                           (device).equals(
                             connectedOrConnectingTwspDev.getAddress())) {
                       //Allow connection only if the outgoing
                       //is peer of TWS connected earbud
                       allowSecondHfConnection = true;
                   } else {
                       allowSecondHfConnection = false;
                   }
                } else {
                   reservedSlotForTwspPeer = 0;
                   if (getTwsPlusConnectedPeer(
                                connectedOrConnectingTwspDev) == null) {
                       //Peer of Connected Tws+ device is not Connected
                       //yet, reserve one slot
                       reservedSlotForTwspPeer = 1;
                   }
                   if (connDevices.size() <
                          (mMaxHeadsetConnections - reservedSlotForTwspPeer)
                          && mIsTwsPlusShoEnabled) {
                       allowSecondHfConnection = true;
                   } else {
                           allowSecondHfConnection = false;
                           if (!mIsTwsPlusShoEnabled) {
                               logD("Not Allowed as TWS+ SHO is not enabled");
                           } else {
                               logD("Max Connections have reached");
                           }
                   }
                }
            } else {
                //There is no TWSP connected device
                if (adapterService.isTwsPlusDevice(device)) {
                    if (mIsTwsPlusShoEnabled) {
                        //outgoing connection is TWSP
                        if ((mMaxHeadsetConnections-connDevices.size()) >= 2) {
                            allowSecondHfConnection = true;
                        } else {
                            allowSecondHfConnection = false;
                            logD("Not enough available slots for TWSP");
                        }
                    } else {
                        allowSecondHfConnection = false;
                    }
                } else {
                    //Outgoing connection is legacy device
                    //For legacy case use the config set by User
                   if (connDevices.size() < mSetMaxConfig) {
                      allowSecondHfConnection = true;
                   } else {
                      allowSecondHfConnection = false;
                   }
               }
            }
            Log.v(TAG, "isTwsPlusDevice for " + device +
                     "is"+ adapterService.isTwsPlusDevice(device));
            Log.v(TAG, "TWS Peer Addr: " +
                      adapterService.getTwsPlusPeerAddress(device));
            if (connectedOrConnectingTwspDev != null) {
                Log.v(TAG, "Connected or Connecting device"
                         + connectedOrConnectingTwspDev.getAddress());
            } else {
                Log.v(TAG, "No Connected TWSP devices");
            }
        }

        Log.v(TAG, "allowSecondHfConnection: " + allowSecondHfConnection);
        Log.v(TAG, "DisconnectAll: " + mDisconnectAll);
        return allowSecondHfConnection;
    }

    public boolean connect(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_ADMIN_PERM, "Need BLUETOOTH ADMIN permission");
        if (getPriority(device) == BluetoothProfile.PRIORITY_OFF) {
            Log.w(TAG, "connect: PRIORITY_OFF, device=" + device + ", " + Utils.getUidPidString());
            return false;
        }
        synchronized (mStateMachines) {
            ParcelUuid[] featureUuids = mAdapterService.getRemoteUuids(device);
            if (!BluetoothUuid.containsAnyUuid(featureUuids, HEADSET_UUIDS)) {
                Log.e(TAG, "connect: Cannot connect to " + device + ": no headset UUID, "
                    + Utils.getUidPidString());
                return false;
            }
            Log.i(TAG, "connect: device=" + device + ", " + Utils.getUidPidString());
            HeadsetStateMachine stateMachine = mStateMachines.get(device);
            if (stateMachine == null) {
                if (mStateMachinesThread != null) {
                    stateMachine = HeadsetObjectsFactory.getInstance()
                        .makeStateMachine(device, mStateMachinesThread.getLooper(), this,
                                mAdapterService, mNativeInterface, mSystemInterface);
                    mStateMachines.put(device, stateMachine);
                } else {
                    Log.w(TAG, "connect: mStateMachinesThread is null");
                }
            }
            int connectionState = stateMachine.getConnectionState();
            if (connectionState == BluetoothProfile.STATE_CONNECTED
                    || connectionState == BluetoothProfile.STATE_CONNECTING) {
                Log.w(TAG, "connect: device " + device
                        + " is already connected/connecting, connectionState=" + connectionState);
                return false;
            }
            List<BluetoothDevice> connectingConnectedDevices =
                    getAllDevicesMatchingConnectionStates(CONNECTING_CONNECTED_STATES);
            boolean disconnectExisting = false;
            mDisconnectAll = false;
            if (connectingConnectedDevices.size() == 0) {
                 Log.e(TAG, "No Connected devices!");
            }
            if (!isConnectionAllowed(device, connectingConnectedDevices)) {
                // When there is maximum one device, we automatically disconnect the current one
                if (mSetMaxConfig == 1) {
                    if (!mIsTwsPlusEnabled && mAdapterService.isTwsPlusDevice(device)) {
                        Log.w(TAG, "Connection attemp to TWS+ when not enabled, Rejecting it");
                        return false;
                    } else {
                        disconnectExisting = true;
                    }
                } else if (mDisconnectAll) {
                    //In Dual HF case
                    disconnectExisting = true;
                } else {
                    Log.w(TAG, "Max connection has reached, rejecting connection to " + device);
                    return false;
                }
            }
            if (disconnectExisting) {
                for (BluetoothDevice connectingConnectedDevice : connectingConnectedDevices) {
                    disconnect(connectingConnectedDevice);
                }
                setActiveDevice(null);
            }
            stateMachine.sendMessage(HeadsetStateMachine.CONNECT, device);
        }
        return true;
    }

    public boolean disconnect(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_ADMIN_PERM, "Need BLUETOOTH_ADMIN permission");
        Log.i(TAG, "disconnect: device=" + device + ", " + Utils.getUidPidString());
        synchronized (mStateMachines) {
            HeadsetStateMachine stateMachine = mStateMachines.get(device);
            if (stateMachine == null) {
                Log.w(TAG, "disconnect: device " + device + " not ever connected/connecting");
                return false;
            }
            int connectionState = stateMachine.getConnectionState();
            if (connectionState != BluetoothProfile.STATE_CONNECTED
                    && connectionState != BluetoothProfile.STATE_CONNECTING) {
                Log.w(TAG, "disconnect: device " + device
                        + " not connected/connecting, connectionState=" + connectionState);
                return false;
            }
            stateMachine.sendMessage(HeadsetStateMachine.DISCONNECT, device);
        }
        return true;
    }

    public boolean isInCall() {
        boolean isCallOngoing = mSystemInterface.isInCall();
        Log.d(TAG," isInCall " + isCallOngoing);
        return isCallOngoing;
    }

    public boolean isRinging() {
        boolean isRingOngoing = mSystemInterface.isRinging();
        Log.d(TAG," isRinging " + isRingOngoing);
        return isRingOngoing;
    }

    public List<BluetoothDevice> getConnectedDevices() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        ArrayList<BluetoothDevice> devices = new ArrayList<>();
        synchronized (mStateMachines) {
            for (HeadsetStateMachine stateMachine : mStateMachines.values()) {
                if (stateMachine.getConnectionState() == BluetoothProfile.STATE_CONNECTED) {
                    devices.add(stateMachine.getDevice());
                }
            }
        }
        return devices;
    }

    /**
     * Helper method to get all devices with matching connection state
     *
     */
    private List<BluetoothDevice> getAllDevicesMatchingConnectionStates(int[] states) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        ArrayList<BluetoothDevice> devices = new ArrayList<>();
        if (states == null) {
            Log.e(TAG, "->States is null");
            return devices;
        }
        synchronized (mStateMachines) {
            final BluetoothDevice[] bondedDevices = mAdapterService.getBondedDevices();
            if (bondedDevices == null) {
                Log.e(TAG, "->Bonded device is null");
                return devices;
            }
            for (BluetoothDevice device : bondedDevices) {

                int connectionState = getConnectionState(device);
                Log.e(TAG, "Connec state for: " + device + "is" + connectionState);
                for (int state : states) {
                    if (connectionState == state) {
                        devices.add(device);
                        Log.e(TAG, "Adding device: " + device);
                        break;
                    }
                }
            }
        }
        return devices;
    }

    /**
     * Same as the API method {@link BluetoothHeadset#getDevicesMatchingConnectionStates(int[])}
     *
     * @param states an array of states from {@link BluetoothProfile}
     * @return a list of devices matching the array of connection states
     */
    @VisibleForTesting
    public List<BluetoothDevice> getDevicesMatchingConnectionStates(int[] states) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        ArrayList<BluetoothDevice> devices = new ArrayList<>();
        synchronized (mStateMachines) {
            if (states == null || mAdapterService == null) {
                return devices;
            }
            final BluetoothDevice[] bondedDevices = mAdapterService.getBondedDevices();
            if (bondedDevices == null) {
                return devices;
            }
            for (BluetoothDevice device : bondedDevices) {

                int connectionState = getConnectionState(device);
                for (int state : states) {
                    if (connectionState == state) {
                        devices.add(device);
                        break;
                    }
                }
            }
        }
        return devices;
    }

    public int getConnectionState(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        synchronized (mStateMachines) {
            final HeadsetStateMachine stateMachine = mStateMachines.get(device);
            if (stateMachine == null) {
                return BluetoothProfile.STATE_DISCONNECTED;
            }
            return stateMachine.getConnectionState();
        }
    }

    public boolean setPriority(BluetoothDevice device, int priority) {
        enforceCallingOrSelfPermission(BLUETOOTH_ADMIN_PERM, "Need BLUETOOTH_ADMIN permission");
        Log.i(TAG, "setPriority: device=" + device + ", priority=" + priority + ", "
                + Utils.getUidPidString());
        AdapterService adapterService = AdapterService.getAdapterService();
        if (adapterService != null)
            adapterService.getDatabase()
                .setProfilePriority(device, BluetoothProfile.HEADSET, priority);
        else
            Log.i(TAG, "adapter service is null");
        return true;
    }

    public int getPriority(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterService.getDatabase()
                .getProfilePriority(device, BluetoothProfile.HEADSET);
    }

    boolean startVoiceRecognition(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        Log.i(TAG, "startVoiceRecognition: device=" + device + ", " + Utils.getUidPidString());
        synchronized (mStateMachines) {
            // TODO(b/79660380): Workaround in case voice recognition was not terminated properly
            if (mVoiceRecognitionStarted) {
                boolean status = stopVoiceRecognition(mActiveDevice);
                Log.w(TAG, "startVoiceRecognition: voice recognition is still active, just called "
                        + "stopVoiceRecognition, returned " + status + " on " + mActiveDevice
                        + ", please try again");
                mVoiceRecognitionStarted = false;
                return false;
            }
            if (!isAudioModeIdle()) {
                Log.w(TAG, "startVoiceRecognition: audio mode not idle, active device is "
                        + mActiveDevice);
                return false;
            }
            // Audio should not be on when no audio mode is active
            if (isAudioOn()) {
                // Disconnect audio so that API user can try later
                boolean status = disconnectAudio();
                Log.w(TAG, "startVoiceRecognition: audio is still active, please wait for audio to"
                        + " be disconnected, disconnectAudio() returned " + status
                        + ", active device is " + mActiveDevice);
                return false;
            }
            if (device == null) {
                Log.i(TAG, "device is null, use active device " + mActiveDevice + " instead");
                device = mActiveDevice;
            }
            boolean pendingRequestByHeadset = false;
            if (mVoiceRecognitionTimeoutEvent != null) {
                if (!mVoiceRecognitionTimeoutEvent.mVoiceRecognitionDevice.equals(device)) {
                    // TODO(b/79660380): Workaround when target device != requesting device
                    Log.w(TAG, "startVoiceRecognition: device " + device
                            + " is not the same as requesting device "
                            + mVoiceRecognitionTimeoutEvent.mVoiceRecognitionDevice
                            + ", fall back to requesting device");
                    device = mVoiceRecognitionTimeoutEvent.mVoiceRecognitionDevice;
                }
                if (mStateMachinesThread != null) {
                    mStateMachinesThread.getThreadHandler()
                           .removeCallbacks(mVoiceRecognitionTimeoutEvent);
                } else {
                    Log.w(TAG, "startVoiceRecognition: mStateMachinesThread is null");
                }
                mVoiceRecognitionTimeoutEvent = null;
                if (mSystemInterface.getVoiceRecognitionWakeLock().isHeld()) {
                    mSystemInterface.getVoiceRecognitionWakeLock().release();
                }
                pendingRequestByHeadset = true;
            }
            if (!Objects.equals(device, mActiveDevice) &&
                  !mAdapterService.isTwsPlusDevice(device) && !setActiveDevice(device)) {
                Log.w(TAG, "startVoiceRecognition: failed to set " + device + " as active");
                return false;
            }
            final HeadsetStateMachine stateMachine = mStateMachines.get(device);
            if (stateMachine == null) {
                Log.w(TAG, "startVoiceRecognition: " + device + " is never connected");
                return false;
            }
            int connectionState = stateMachine.getConnectionState();
            if (connectionState != BluetoothProfile.STATE_CONNECTED
                    && connectionState != BluetoothProfile.STATE_CONNECTING) {
                Log.w(TAG, "startVoiceRecognition: " + device + " is not connected or connecting");
                return false;
            }
            mVoiceRecognitionStarted = true;
            if (pendingRequestByHeadset) {
                stateMachine.sendMessage(HeadsetStateMachine.VOICE_RECOGNITION_RESULT,
                        1 /* success */, 0, device);
            } else {
                stateMachine.sendMessage(HeadsetStateMachine.VOICE_RECOGNITION_START, device);
            }
        }
        /* VR calls always use SWB if supported on remote*/
        if(isSwbEnabled()) {
            enableSwbCodec(true);
        }
        return true;
    }

    boolean stopVoiceRecognition(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        Log.i(TAG, "stopVoiceRecognition: device=" + device + ", " + Utils.getUidPidString());
        synchronized (mStateMachines) {
            if (!Objects.equals(mActiveDevice, device)) {
                Log.w(TAG, "startVoiceRecognition: requested device " + device
                        + " is not active, use active device " + mActiveDevice + " instead");
                device = mActiveDevice;
            }
            if (mAdapterService.isTwsPlusDevice(device) &&
                    !isAudioConnected(device)) {
                BluetoothDevice peerDevice = getTwsPlusConnectedPeer(device);
                if (peerDevice != null && isAudioConnected(peerDevice)) {
                    Log.w(TAG, "startVoiceRecognition: requested TWS+ device " + device
                            + " is not audio connected, use TWS+ peer device " + peerDevice
                            + " instead");
                    device = peerDevice;
                } else {
                    Log.w(TAG, "stopVoiceRecognition: both earbuds are not audio connected, resume A2DP");
                    mVoiceRecognitionStarted = false;
                    mHfpA2dpSyncInterface.releaseA2DP(null);
                    return false;
                }
            }
            final HeadsetStateMachine stateMachine = mStateMachines.get(device);
            if (stateMachine == null) {
                Log.w(TAG, "stopVoiceRecognition: " + device + " is never connected");
                return false;
            }
            int connectionState = stateMachine.getConnectionState();
            if (connectionState != BluetoothProfile.STATE_CONNECTED
                    && connectionState != BluetoothProfile.STATE_CONNECTING) {
                Log.w(TAG, "stopVoiceRecognition: " + device + " is not connected or connecting");
                return false;
            }
            if (!mVoiceRecognitionStarted) {
                Log.w(TAG, "stopVoiceRecognition: voice recognition was not started");
                return false;
            }
            mVoiceRecognitionStarted = false;
            stateMachine.sendMessage(HeadsetStateMachine.VOICE_RECOGNITION_STOP, device);

            if (isAudioOn()) {
                stateMachine.sendMessage(HeadsetStateMachine.DISCONNECT_AUDIO, device);
            } else {
                Log.w(TAG, "SCO is not connected and VR stopped, resuming A2DP");
                stateMachine.sendMessage(HeadsetStateMachine.RESUME_A2DP);
            }
        }
        return true;
    }

    public boolean isAudioOn() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        int numConnectedAudioDevices = getNonIdleAudioDevices().size();
        Log.d(TAG," isAudioOn: The number of audio connected devices "
                 + numConnectedAudioDevices);
        return numConnectedAudioDevices > 0;
    }

    public boolean isScoOrCallActive() {
      Log.d(TAG, "isScoOrCallActive(): Call Active:" + mSystemInterface.isInCall() +
                                       "Call is Ringing:" + mSystemInterface.isInCall() +
                                       "SCO is Active:" + isAudioOn());
      if (mSystemInterface.isInCall() || (mSystemInterface.isRinging()) || isAudioOn()) {
          return true;
      } else {
          return false;
      }
    }
    boolean isAudioConnected(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        synchronized (mStateMachines) {
            final HeadsetStateMachine stateMachine = mStateMachines.get(device);
            if (stateMachine == null) {
                return false;
            }
            return stateMachine.getAudioState() == BluetoothHeadset.STATE_AUDIO_CONNECTED;
        }
    }

    int getAudioState(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        synchronized (mStateMachines) {
            final HeadsetStateMachine stateMachine = mStateMachines.get(device);
            if (stateMachine == null) {
                return BluetoothHeadset.STATE_AUDIO_DISCONNECTED;
            }
            return stateMachine.getAudioState();
        }
    }

    public void setAudioRouteAllowed(boolean allowed) {
        enforceCallingOrSelfPermission(BLUETOOTH_ADMIN_PERM, "Need BLUETOOTH_ADMIN permission");
        Log.i(TAG, "setAudioRouteAllowed: allowed=" + allowed + ", " + Utils.getUidPidString());
        mAudioRouteAllowed = allowed;
        mNativeInterface.setScoAllowed(allowed);
    }

    public boolean getAudioRouteAllowed() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAudioRouteAllowed;
    }

    public void setForceScoAudio(boolean forced) {
        enforceCallingOrSelfPermission(BLUETOOTH_ADMIN_PERM, "Need BLUETOOTH_ADMIN permission");
        Log.i(TAG, "setForceScoAudio: forced=" + forced + ", " + Utils.getUidPidString());
        mForceScoAudio = forced;
    }

    @VisibleForTesting
    public boolean getForceScoAudio() {
        return mForceScoAudio;
    }

    /**
     * Get first available device for SCO audio
     *
     * @return first connected headset device
     */
    @VisibleForTesting
    @Nullable
    public BluetoothDevice getFirstConnectedAudioDevice() {
        ArrayList<HeadsetStateMachine> stateMachines = new ArrayList<>();
        synchronized (mStateMachines) {
            List<BluetoothDevice> availableDevices =
                    getDevicesMatchingConnectionStates(CONNECTING_CONNECTED_STATES);
            for (BluetoothDevice device : availableDevices) {
                final HeadsetStateMachine stateMachine = mStateMachines.get(device);
                if (stateMachine == null) {
                    continue;
                }
                stateMachines.add(stateMachine);
            }
        }
        stateMachines.sort(Comparator.comparingLong(HeadsetStateMachine::getConnectingTimestampMs));
        if (stateMachines.size() > 0) {
            return stateMachines.get(0).getDevice();
        }
        return null;
    }

    /**
     * Process a change in the silence mode for a {@link BluetoothDevice}.
     *
     * @param device the device to change silence mode
     * @param silence true to enable silence mode, false to disable.
     * @return true on success, false on error
     */
    @VisibleForTesting
    public boolean setSilenceMode(BluetoothDevice device, boolean silence) {
        Log.d(TAG, "setSilenceMode(" + device + "): " + silence);

        if (silence && Objects.equals(mActiveDevice, device)) {
            setActiveDevice(null);
        } else if (!silence && mActiveDevice == null) {
            // Set the device as the active device if currently no active device.
            setActiveDevice(device);
        }
        synchronized (mStateMachines) {
            final HeadsetStateMachine stateMachine = mStateMachines.get(device);
            if (stateMachine == null) {
                Log.w(TAG, "setSilenceMode: device " + device
                        + " was never connected/connecting");
                return false;
            }
            stateMachine.setSilenceDevice(silence);
        }

        return true;
    }

    /**
     * Set the active device.
     *
     * @param device the active device
     * @return true on success, otherwise false
     */
    public boolean setActiveDevice(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_ADMIN_PERM, "Need BLUETOOTH_ADMIN permission");
        Log.i(TAG, "setActiveDevice: device=" + device + ", " + Utils.getUidPidString());
        synchronized (mStateMachines) {
            if (device == null) {
                // Clear the active device
                if (mVoiceRecognitionStarted) {
                    if (!stopVoiceRecognition(mActiveDevice)) {
                        Log.w(TAG, "setActiveDevice: fail to stopVoiceRecognition from "
                                + mActiveDevice);
                    }
                }
                if (mVirtualCallStarted) {
                    if (!stopScoUsingVirtualVoiceCall()) {
                        Log.w(TAG, "setActiveDevice: fail to stopScoUsingVirtualVoiceCall from "
                                + mActiveDevice);
                    }
                }
                if (getAudioState(mActiveDevice) != BluetoothHeadset.STATE_AUDIO_DISCONNECTED) {
                    if (!disconnectAudio(mActiveDevice)) {
                        Log.w(TAG, "setActiveDevice: disconnectAudio failed on " + mActiveDevice);
                    }
                }
                if (!mNativeInterface.setActiveDevice(null)) {
                    Log.w(TAG, "setActiveDevice: Cannot set active device as null in native layer");
                }
                mActiveDevice = null;
                broadcastActiveDevice(null);
                return true;
            }
            if (device.equals(mActiveDevice)) {
                Log.i(TAG, "setActiveDevice: device " + device + " is already active");
                return true;
            }
            if (getConnectionState(device) != BluetoothProfile.STATE_CONNECTED) {
                Log.e(TAG, "setActiveDevice: Cannot set " + device
                        + " as active, device is not connected");
                return false;
            }
            if (mActiveDevice != null && mAdapterService.isTwsPlusDevice(device) &&
                mAdapterService.isTwsPlusDevice(mActiveDevice) &&
                !Objects.equals(device, mActiveDevice) &&
                getConnectionState(mActiveDevice) == BluetoothProfile.STATE_CONNECTED) {
                Log.d(TAG,"Ignore setActiveDevice request");
                return false;
            }

            if (!mNativeInterface.setActiveDevice(device)) {
                Log.e(TAG, "setActiveDevice: Cannot set " + device + " as active in native layer");
                return false;
            }
            BluetoothDevice previousActiveDevice = mActiveDevice;
            mActiveDevice = device;
            int audioStateOfPrevActiveDevice = BluetoothHeadset.STATE_AUDIO_DISCONNECTED;
            boolean activeSwitchBetweenEbs = false;
            if (previousActiveDevice != null &&
                    mAdapterService.isTwsPlusDevice(previousActiveDevice)) {
                BluetoothDevice peerDevice =
                           getTwsPlusConnectedPeer(previousActiveDevice);
                if (mActiveDevice != null &&
                      mAdapterService.isTwsPlusDevice(mActiveDevice)) {
                      Log.d(TAG, "Active device switch b/n ebs");
                      activeSwitchBetweenEbs = true;
                } else {
                    if (getAudioState(previousActiveDevice) !=
                               BluetoothHeadset.STATE_AUDIO_DISCONNECTED ||
                        getAudioState(peerDevice) !=
                               BluetoothHeadset.STATE_AUDIO_DISCONNECTED) {
                            audioStateOfPrevActiveDevice =
                                   BluetoothHeadset.STATE_AUDIO_CONNECTED;
                    }
                }
                if (audioStateOfPrevActiveDevice ==
                        BluetoothHeadset.STATE_AUDIO_CONNECTED &&
                        getAudioState(previousActiveDevice) ==
                        BluetoothHeadset.STATE_AUDIO_DISCONNECTED) {
                    Log.w(TAG, "Update previousActiveDevice with" + peerDevice);
                    previousActiveDevice = peerDevice;
                }
            } else {
                audioStateOfPrevActiveDevice =
                               getAudioState(previousActiveDevice);
            }
            if (!activeSwitchBetweenEbs && audioStateOfPrevActiveDevice !=
                               BluetoothHeadset.STATE_AUDIO_DISCONNECTED) {
                if (!disconnectAudio(previousActiveDevice)) {
                    Log.e(TAG, "setActiveDevice: fail to disconnectAudio from "
                            + previousActiveDevice);
                    mActiveDevice = previousActiveDevice;
                    mNativeInterface.setActiveDevice(previousActiveDevice);
                    return false;
                }
                broadcastActiveDevice(mActiveDevice);
            } else if (shouldPersistAudio()) {
                boolean isPts = SystemProperties.getBoolean("vendor.bt.pts.certification", false);
                if (!isPts) {
                    if (!connectAudio(mActiveDevice)) {
                        Log.e(TAG, "setActiveDevice: fail to connectAudio to " + mActiveDevice);
                        mActiveDevice = previousActiveDevice;
                        mNativeInterface.setActiveDevice(previousActiveDevice);
                        return false;
                    }
                }
                broadcastActiveDevice(mActiveDevice);
            } else {
                broadcastActiveDevice(mActiveDevice);
            }
        }
        return true;
    }

    /**
     * Get the active device.
     *
     * @return the active device or null if no device is active
     */
    public BluetoothDevice getActiveDevice() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        synchronized (mStateMachines) {
            return mActiveDevice;
        }
    }

    boolean connectAudio() {
        enforceCallingOrSelfPermission(BLUETOOTH_ADMIN_PERM, "Need BLUETOOTH_ADMIN permission");
        synchronized (mStateMachines) {
            BluetoothDevice device = mActiveDevice;
            if (device == null) {
                Log.w(TAG, "connectAudio: no active device, " + Utils.getUidPidString());
                return false;
            }
            return connectAudio(device);
        }
    }

    boolean connectAudio(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_ADMIN_PERM, "Need BLUETOOTH_ADMIN permission");
        Log.i(TAG, "connectAudio: device=" + device + ", " + Utils.getUidPidString());
        synchronized (mStateMachines) {
            if (!isScoAcceptable(device)) {
                Log.w(TAG, "connectAudio, rejected SCO request to " + device);
                return false;
            }
            final HeadsetStateMachine stateMachine = mStateMachines.get(device);
            if (stateMachine == null) {
                Log.w(TAG, "connectAudio: device " + device + " was never connected/connecting");
                return false;
            }
            if (stateMachine.getConnectionState() != BluetoothProfile.STATE_CONNECTED) {
                Log.w(TAG, "connectAudio: profile not connected");
                return false;
            }
            if (stateMachine.getAudioState() != BluetoothHeadset.STATE_AUDIO_DISCONNECTED) {
                logD("connectAudio: audio is not idle for device " + device);
                return true;
            }
            if (isAudioOn()) {
                //SCO is connecting or connected.
                //Return true to telephony
                Log.w(TAG, "connectAudio: audio is not idle, current audio devices are: "
                        + Arrays.toString(getNonIdleAudioDevices().toArray()) +
                        " ,returning true");
                return true;
            }
            stateMachine.sendMessage(HeadsetStateMachine.CONNECT_AUDIO, device);
        }
        return true;
    }

    private List<BluetoothDevice> getNonIdleAudioDevices() {
        ArrayList<BluetoothDevice> devices = new ArrayList<>();
        synchronized (mStateMachines) {
            for (HeadsetStateMachine stateMachine : mStateMachines.values()) {
                if (stateMachine.getAudioState() != BluetoothHeadset.STATE_AUDIO_DISCONNECTED) {
                    devices.add(stateMachine.getDevice());
                }
            }
        }
        return devices;
    }

    boolean disconnectAudio() {
        enforceCallingOrSelfPermission(BLUETOOTH_ADMIN_PERM, "Need BLUETOOTH_ADMIN permission");
        boolean result = false;
        synchronized (mStateMachines) {
            for (BluetoothDevice device : getNonIdleAudioDevices()) {
                if (disconnectAudio(device)) {
                    result = true;
                } else {
                    Log.e(TAG, "disconnectAudio() from " + device + " failed");
                }
            }
        }
        if (!result) {
            logD("disconnectAudio() no active audio connection");
        }
        return result;
    }

    boolean disconnectAudio(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_ADMIN_PERM, "Need BLUETOOTH_ADMIN permission");
        synchronized (mStateMachines) {
            Log.i(TAG, "disconnectAudio: device=" + device + ", " + Utils.getUidPidString());
            final HeadsetStateMachine stateMachine = mStateMachines.get(device);
            if (stateMachine == null) {
                Log.w(TAG, "disconnectAudio: device " + device + " was never connected/connecting");
                return false;
            }
            if (stateMachine.getAudioState() == BluetoothHeadset.STATE_AUDIO_DISCONNECTED) {
                Log.w(TAG, "disconnectAudio, audio is already disconnected for " + device);
                return false;
            }
            stateMachine.sendMessage(HeadsetStateMachine.DISCONNECT_AUDIO, device);
        }
        return true;
    }

    boolean isVirtualCallStarted() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        synchronized (mStateMachines) {
            return mVirtualCallStarted;
        }
    }

    boolean isVRStarted() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        synchronized (mStateMachines) {
            return mVoiceRecognitionStarted;
        }
    }

    private boolean startScoUsingVirtualVoiceCall() {
        enforceCallingOrSelfPermission(BLUETOOTH_ADMIN_PERM, "Need BLUETOOTH_ADMIN permission");
        Log.i(TAG, "startScoUsingVirtualVoiceCall: " + Utils.getUidPidString());
        synchronized (mStateMachines) {
            // TODO(b/79660380): Workaround in case voice recognition was not terminated properly
            if (mVoiceRecognitionStarted) {
                boolean status = stopVoiceRecognition(mActiveDevice);
                Log.w(TAG, "startScoUsingVirtualVoiceCall: voice recognition is still active, "
                        + "just called stopVoiceRecognition, returned " + status + " on "
                        + mActiveDevice + ", please try again");
                mVoiceRecognitionStarted = false;
                return false;
            }
            if (!isAudioModeIdle()) {
                Log.w(TAG, "startScoUsingVirtualVoiceCall: audio mode not idle, active device is "
                        + mActiveDevice);
                return false;
            }
            // Audio should not be on when no audio mode is active
            if (isAudioOn()) {
                // Disconnect audio so that API user can try later
                boolean status = disconnectAudio();
                Log.w(TAG, "startScoUsingVirtualVoiceCall: audio is still active, please wait for "
                        + "audio to be disconnected, disconnectAudio() returned " + status
                        + ", active device is " + mActiveDevice);
                return false;
            }
            if (mActiveDevice == null) {
                Log.w(TAG, "startScoUsingVirtualVoiceCall: no active device");
                return false;
            }
            mVirtualCallStarted = true;
            mSystemInterface.getHeadsetPhoneState().setIsCsCall(false);
            // Send virtual phone state changed to initialize SCO
            phoneStateChanged(0, 0, HeadsetHalConstants.CALL_STATE_DIALING, "", 0, "", true);
            phoneStateChanged(0, 0, HeadsetHalConstants.CALL_STATE_ALERTING, "", 0, "", true);
            phoneStateChanged(1, 0, HeadsetHalConstants.CALL_STATE_IDLE, "", 0, "", true);
            return true;
        }
    }

    boolean stopScoUsingVirtualVoiceCall() {
        enforceCallingOrSelfPermission(BLUETOOTH_ADMIN_PERM, "Need BLUETOOTH_ADMIN permission");
        Log.i(TAG, "stopScoUsingVirtualVoiceCall: " + Utils.getUidPidString());
        synchronized (mStateMachines) {
            // 1. Check if virtual call has already started
            if (!mVirtualCallStarted) {
                Log.w(TAG, "stopScoUsingVirtualVoiceCall: virtual call not started");
                return false;
            }
            mVirtualCallStarted = false;
            // 2. Send virtual phone state changed to close SCO
            phoneStateChanged(0, 0, HeadsetHalConstants.CALL_STATE_IDLE, "", 0, "", true);
        }
        return true;
    }

    class DialingOutTimeoutEvent implements Runnable {
        BluetoothDevice mDialingOutDevice;

        DialingOutTimeoutEvent(BluetoothDevice fromDevice) {
            mDialingOutDevice = fromDevice;
        }

        @Override
        public void run() {
            synchronized (mStateMachines) {
                mDialingOutTimeoutEvent = null;
                doForStateMachine(mDialingOutDevice, stateMachine -> stateMachine.sendMessage(
                        HeadsetStateMachine.DIALING_OUT_RESULT, 0 /* fail */, 0,
                        mDialingOutDevice));
            }
        }

        @Override
        public String toString() {
            return "DialingOutTimeoutEvent[" + mDialingOutDevice + "]";
        }
    }

    public boolean isTwsPlusActive(BluetoothDevice device) {
        boolean ret = false;
        if (mAdapterService.isTwsPlusDevice(device)) {
            if (device.equals(getActiveDevice())) {
                ret = true;
            } else {
                BluetoothDevice peerTwsDevice = mAdapterService.getTwsPlusPeerDevice(device);
                if (peerTwsDevice != null &&
                    peerTwsDevice.equals(getActiveDevice())) {
                    ret = true;
                }
            }
        }
        Log.d(TAG, "isTwsPlusActive returns" + ret);
        return ret;
    }

    /**
     * Dial an outgoing call as requested by the remote device
     *
     * @param fromDevice remote device that initiated this dial out action
     * @param dialNumber number to dial
     * @return true on successful dial out
     */
    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public boolean dialOutgoingCall(BluetoothDevice fromDevice, String dialNumber) {
        synchronized (mStateMachines) {
            Log.i(TAG, "dialOutgoingCall: from " + fromDevice);
            if (!isOnStateMachineThread()) {
                Log.e(TAG, "dialOutgoingCall must be called from state machine thread");
                return false;
            }
            if (mDialingOutTimeoutEvent != null) {
                Log.e(TAG, "dialOutgoingCall, already dialing by " + mDialingOutTimeoutEvent);
                return false;
            }
            if (isVirtualCallStarted()) {
                if (!stopScoUsingVirtualVoiceCall()) {
                    Log.e(TAG, "dialOutgoingCall failed to stop current virtual call");
                    return false;
                }
            }
            if (!isTwsPlusActive(fromDevice) &&
                !setActiveDevice(fromDevice)) {
                Log.e(TAG, "dialOutgoingCall failed to set active device to " + fromDevice);
                return false;
            }
            Intent intent = new Intent(Intent.ACTION_CALL_PRIVILEGED,
                    Uri.fromParts(PhoneAccount.SCHEME_TEL, dialNumber, null));
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            startActivity(intent);
            mDialingOutTimeoutEvent = new DialingOutTimeoutEvent(fromDevice);
            if (mStateMachinesThread != null) {
                mStateMachinesThread.getThreadHandler()
                    .postDelayed(mDialingOutTimeoutEvent, DIALING_OUT_TIMEOUT_MS);
            } else {
                Log.w(TAG, "dialOutgoingCall: mStateMachinesThread is null");
            }
            return true;
        }
    }

    /**
     * Check if any connected headset has started dialing calls
     *
     * @return true if some device has started dialing calls
     */
    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public boolean hasDeviceInitiatedDialingOut() {
        synchronized (mStateMachines) {
            return mDialingOutTimeoutEvent != null;
        }
    }

    class VoiceRecognitionTimeoutEvent implements Runnable {
        BluetoothDevice mVoiceRecognitionDevice;

        VoiceRecognitionTimeoutEvent(BluetoothDevice device) {
            mVoiceRecognitionDevice = device;
        }

        @Override
        public void run() {
            synchronized (mStateMachines) {
                if (mSystemInterface.getVoiceRecognitionWakeLock().isHeld()) {
                    mSystemInterface.getVoiceRecognitionWakeLock().release();
                }
                mVoiceRecognitionTimeoutEvent = null;
                doForStateMachine(mVoiceRecognitionDevice, stateMachine -> stateMachine.sendMessage(
                        HeadsetStateMachine.VOICE_RECOGNITION_RESULT, 0 /* fail */, 0,
                        mVoiceRecognitionDevice));
            }
        }

        @Override
        public String toString() {
            return "VoiceRecognitionTimeoutEvent[" + mVoiceRecognitionDevice + "]";
        }
    }

    boolean startVoiceRecognitionByHeadset(BluetoothDevice fromDevice) {
        synchronized (mStateMachines) {
            Log.i(TAG, "startVoiceRecognitionByHeadset: from " + fromDevice);
            // TODO(b/79660380): Workaround in case voice recognition was not terminated properly
            if (mVoiceRecognitionStarted) {
                boolean status = stopVoiceRecognition(mActiveDevice);
                Log.w(TAG, "startVoiceRecognitionByHeadset: voice recognition is still active, "
                        + "just called stopVoiceRecognition, returned " + status + " on "
                        + mActiveDevice + ", please try again");
                mVoiceRecognitionStarted = false;
                return false;
            }
            if (fromDevice == null) {
                Log.e(TAG, "startVoiceRecognitionByHeadset: fromDevice is null");
                return false;
            }
            if (!isAudioModeIdle()) {
                Log.w(TAG, "startVoiceRecognitionByHeadset: audio mode not idle, active device is "
                        + mActiveDevice);
                return false;
            }
            // Audio should not be on when no audio mode is active
            if (isAudioOn()) {
                // Disconnect audio so that user can try later
                boolean status = disconnectAudio();
                Log.w(TAG, "startVoiceRecognitionByHeadset: audio is still active, please wait for"
                        + " audio to be disconnected, disconnectAudio() returned " + status
                        + ", active device is " + mActiveDevice);
                return false;
            }
            // Do not start new request until the current one is finished or timeout
            if (mVoiceRecognitionTimeoutEvent != null) {
                Log.w(TAG, "startVoiceRecognitionByHeadset: failed request from " + fromDevice
                        + ", already pending by " + mVoiceRecognitionTimeoutEvent);
                return false;
            }
            if (!isTwsPlusActive(fromDevice) && !setActiveDevice(fromDevice)) {
                Log.w(TAG, "startVoiceRecognitionByHeadset: failed to set " + fromDevice
                        + " as active");
                return false;
            }
            IDeviceIdleController deviceIdleController = IDeviceIdleController.Stub.asInterface(
                    ServiceManager.getService(Context.DEVICE_IDLE_CONTROLLER));
            if (deviceIdleController == null) {
                Log.w(TAG, "startVoiceRecognitionByHeadset: deviceIdleController is null, device="
                        + fromDevice);
                return false;
            }
            try {
                deviceIdleController.exitIdle("voice-command");
            } catch (RemoteException e) {
                Log.w(TAG,
                        "startVoiceRecognitionByHeadset: failed to exit idle, device=" + fromDevice
                                + ", error=" + e.getMessage());
                return false;
            }
            if (!mSystemInterface.activateVoiceRecognition()) {
                Log.w(TAG, "startVoiceRecognitionByHeadset: failed request from " + fromDevice);
                return false;
            }
            mVoiceRecognitionTimeoutEvent = new VoiceRecognitionTimeoutEvent(fromDevice);
            if (mStateMachinesThread != null) {
                mStateMachinesThread.getThreadHandler()
                    .postDelayed(mVoiceRecognitionTimeoutEvent, sStartVrTimeoutMs);
            } else {
                Log.w(TAG, "startVoiceRecognitionByHeadset: mStateMachinesThread is null");
            }
            if (!mSystemInterface.getVoiceRecognitionWakeLock().isHeld()) {
                mSystemInterface.getVoiceRecognitionWakeLock().acquire(sStartVrTimeoutMs);
            }
            /* VR calls always use SWB if supported on remote*/
            if(isSwbEnabled()) {
                enableSwbCodec(true);
            }
            return true;
        }
    }

    boolean stopVoiceRecognitionByHeadset(BluetoothDevice fromDevice) {
        synchronized (mStateMachines) {
            Log.i(TAG, "stopVoiceRecognitionByHeadset: from " + fromDevice);
            if (!Objects.equals(fromDevice, mActiveDevice)) {
                Log.w(TAG, "stopVoiceRecognitionByHeadset: " + fromDevice
                        + " is not active, active device is " + mActiveDevice);
                return false;
            }
            if (!mVoiceRecognitionStarted && mVoiceRecognitionTimeoutEvent == null) {
                Log.w(TAG, "stopVoiceRecognitionByHeadset: voice recognition not started, device="
                        + fromDevice);
                return false;
            }
            if (mVoiceRecognitionTimeoutEvent != null) {
                if (mSystemInterface.getVoiceRecognitionWakeLock().isHeld()) {
                    mSystemInterface.getVoiceRecognitionWakeLock().release();
                }
                if (mStateMachinesThread != null) {
                    mStateMachinesThread.getThreadHandler()
                        .removeCallbacks(mVoiceRecognitionTimeoutEvent);
                } else {
                    Log.w(TAG, "stopVoiceRecognitionByHeadset: mStateMachinesThread is null");
                }
                mVoiceRecognitionTimeoutEvent = null;
            }
            if (mVoiceRecognitionStarted) {
                if (isAudioOn()) {
                    if (!disconnectAudio()) {
                        Log.w(TAG, "stopVoiceRecognitionByHeadset: failed to disconnect audio from "
                            + fromDevice);
                    }
                } else {
                    Log.w(TAG, "stopVoiceRecognitionByHeadset: No SCO connected, resume A2DP");
                    mHfpA2dpSyncInterface.releaseA2DP(null);
                }
                mVoiceRecognitionStarted = false;
            }
            if (!mSystemInterface.deactivateVoiceRecognition()) {
                Log.w(TAG, "stopVoiceRecognitionByHeadset: failed request from " + fromDevice);
                return false;
            }
            return true;
        }
    }

    private void phoneStateChanged(int numActive, int numHeld, int callState, String number,
            int type, String name, boolean isVirtualCall) {
        enforceCallingOrSelfPermission(MODIFY_PHONE_STATE, "Need MODIFY_PHONE_STATE permission");
        synchronized (mStateMachines) {
            if (mStateMachinesThread == null) {
                Log.w(TAG, "mStateMachinesThread is null, returning");
                return;
            }
            // Should stop all other audio mode in this case
            if ((numActive + numHeld) > 0 || callState != HeadsetHalConstants.CALL_STATE_IDLE) {
                if (!isVirtualCall && mVirtualCallStarted) {
                    // stop virtual voice call if there is an incoming Telecom call update
                    stopScoUsingVirtualVoiceCall();
                }
                if (mVoiceRecognitionStarted) {
                    // stop voice recognition if there is any incoming call
                    stopVoiceRecognition(mActiveDevice);
                }
            } else {
                // ignore CS non-call state update when virtual call started
                if (!isVirtualCall && mVirtualCallStarted) {
                    Log.i(TAG, "Ignore CS non-call state update");
                    return;
                }
            }
            if (mDialingOutTimeoutEvent != null) {
                // Send result to state machine when dialing starts
                if (callState == HeadsetHalConstants.CALL_STATE_DIALING) {
                    mStateMachinesThread.getThreadHandler()
                            .removeCallbacks(mDialingOutTimeoutEvent);
                    doForStateMachine(mDialingOutTimeoutEvent.mDialingOutDevice,
                            stateMachine -> stateMachine.sendMessage(
                                    HeadsetStateMachine.DIALING_OUT_RESULT, 1 /* success */, 0,
                                    mDialingOutTimeoutEvent.mDialingOutDevice));
                } else if (callState == HeadsetHalConstants.CALL_STATE_ACTIVE
                        || callState == HeadsetHalConstants.CALL_STATE_IDLE) {
                    // Clear the timeout event when the call is connected or disconnected
                    if (!mStateMachinesThread.getThreadHandler()
                            .hasCallbacks(mDialingOutTimeoutEvent)) {
                        mDialingOutTimeoutEvent = null;
                    }
                }
            }
            mStateMachinesThread.getThreadHandler().post(() -> {
                mSystemInterface.getHeadsetPhoneState().setNumActiveCall(numActive);
                mSystemInterface.getHeadsetPhoneState().setNumHeldCall(numHeld);
                mSystemInterface.getHeadsetPhoneState().setCallState(callState);
            });
            List<BluetoothDevice> availableDevices =
                        getDevicesMatchingConnectionStates(CONNECTING_CONNECTED_STATES);
            if(availableDevices.size() > 0) {
                Log.i(TAG, "Update the phoneStateChanged status to connecting and " +
                           "connected devices");
                doForEachConnectedConnectingStateMachine(
                   stateMachine -> stateMachine.sendMessage(HeadsetStateMachine.CALL_STATE_CHANGED,
                        new HeadsetCallState(numActive, numHeld, callState, number, type, name)));
                mStateMachinesThread.getThreadHandler().post(() -> {
                    if (!(mSystemInterface.isInCall() || mSystemInterface.isRinging())) {
                        Log.i(TAG, "no call, sending resume A2DP message to state machines");
                        for (BluetoothDevice device : availableDevices) {
                            HeadsetStateMachine stateMachine = mStateMachines.get(device);
                            if (stateMachine == null) {
                                Log.w(TAG, "phoneStateChanged: device " + device +
                                       " was never connected/connecting");
                                continue;
                            }
                            stateMachine.sendMessage(HeadsetStateMachine.RESUME_A2DP);
                        }
                    }
                });
            } else {
                mStateMachinesThread.getThreadHandler().post(() -> {
                    if (!(mSystemInterface.isInCall() || mSystemInterface.isRinging())) {
                        //If no device is connected, resume A2DP if there is no call
                        Log.i(TAG, "No device is connected and no call, " +
                                   "set A2DPsuspended to false");
                        mHfpA2dpSyncInterface.releaseA2DP(null);
                    } else {
                        //if call/ ring is ongoing, suspendA2DP to true
                        Log.i(TAG, "No device is connected and call/ring is ongoing, " +
                                   "set A2DPsuspended to true");
                        mHfpA2dpSyncInterface.suspendA2DP(HeadsetA2dpSync.
                                                      A2DP_SUSPENDED_BY_CS_CALL, null);
                    }
                });
            }
        }
    }

    private void clccResponse(int index, int direction, int status, int mode, boolean mpty,
            String number, int type) {
        enforceCallingOrSelfPermission(MODIFY_PHONE_STATE, "Need MODIFY_PHONE_STATE permission");
        synchronized (mStateMachines) {
           doForEachConnectedStateMachine(
                stateMachine -> stateMachine.sendMessage(HeadsetStateMachine.SEND_CCLC_RESPONSE,
                        new HeadsetClccResponse(index, direction, status, mode, mpty, number,
                                type)));
        }
    }

    private boolean sendVendorSpecificResultCode(BluetoothDevice device, String command,
            String arg) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        synchronized (mStateMachines) {
            final HeadsetStateMachine stateMachine = mStateMachines.get(device);
            if (stateMachine == null) {
                Log.w(TAG, "sendVendorSpecificResultCode: device " + device
                        + " was never connected/connecting");
                return false;
            }
            int connectionState = stateMachine.getConnectionState();
            if (connectionState != BluetoothProfile.STATE_CONNECTED) {
                return false;
            }
            // Currently we support only "+ANDROID".
            if (!command.equals(BluetoothHeadset.VENDOR_RESULT_CODE_COMMAND_ANDROID)) {
                Log.w(TAG, "Disallowed unsolicited result code command: " + command);
                return false;
            }
            stateMachine.sendMessage(HeadsetStateMachine.SEND_VENDOR_SPECIFIC_RESULT_CODE,
                    new HeadsetVendorSpecificResultCode(device, command, arg));
        }
        return true;
    }

    boolean isInbandRingingEnabled() {
        boolean returnVal;
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        returnVal = BluetoothHeadset.isInbandRingingSupported(this) && !SystemProperties.getBoolean(
                DISABLE_INBAND_RINGING_PROPERTY, true) && !mInbandRingingRuntimeDisable;
        Log.d(TAG, "isInbandRingingEnabled returning: " + returnVal);
        return returnVal;
    }

    /**
     * Called from {@link HeadsetStateMachine} in state machine thread when there is a connection
     * state change
     *
     * @param device remote device
     * @param fromState from which connection state is the change
     * @param toState to which connection state is the change
     */
    @VisibleForTesting
    public void onConnectionStateChangedFromStateMachine(BluetoothDevice device, int fromState,
            int toState) {
        synchronized (mStateMachines) {
            List<BluetoothDevice> audioConnectableDevices =
                    getDevicesMatchingConnectionStates(CONNECTING_CONNECTED_STATES);
            if (fromState != BluetoothProfile.STATE_CONNECTED
                    && toState == BluetoothProfile.STATE_CONNECTED) {
                if (audioConnectableDevices.size() > 1 && isInbandRingingEnabled()) {
                    mInbandRingingRuntimeDisable = true;
                    doForEachConnectedStateMachine(
                            stateMachine -> stateMachine.sendMessage(HeadsetStateMachine.SEND_BSIR,
                                    0));
                }
                MetricsLogger.logProfileConnectionEvent(BluetoothMetricsProto.ProfileId.HEADSET);
            }
            if (fromState != BluetoothProfile.STATE_DISCONNECTED
                    && toState == BluetoothProfile.STATE_DISCONNECTED) {
                if (audioConnectableDevices.size() <= 1 ) {
                    mInbandRingingRuntimeDisable = false;
                    if(isInbandRingingEnabled()) {
                        doForEachConnectedStateMachine(
                            stateMachine -> stateMachine.sendMessage(HeadsetStateMachine.SEND_BSIR,
                                   1));
                    }
                }
                if (device.equals(mActiveDevice)) {
                    AdapterService adapterService = AdapterService.getAdapterService();
                    if (adapterService.isTwsPlusDevice(device)) {
                        //if the disconnected device is a tws+ device
                        // and if peer device is connected, set the peer
                        // as an active device
                        BluetoothDevice peerDevice = getTwsPlusConnectedPeer(device);
                        if (peerDevice != null) {
                            setActiveDevice(peerDevice);
                        }
                    } else {
                        setActiveDevice(null);
                    }
                }
            }
        }

        // if active device is null, SLC connected, make this device as active.
        if (fromState == BluetoothProfile.STATE_CONNECTING &&
             toState == BluetoothProfile.STATE_CONNECTED &&
             mActiveDevice == null) {
             Log.i(TAG, "onConnectionStateChangedFromStateMachine: SLC connected, no active"
                              + " is present. Setting active device to " + device);
             setActiveDevice(device);
        }
    }

    public HeadsetA2dpSync getHfpA2DPSyncInterface(){
        return mHfpA2dpSyncInterface;
    }

    public void sendA2dpStateChangeUpdate(int state) {
        Log.d(TAG," sendA2dpStateChange newState = " + state);
        doForEachConnectedConnectingStateMachine(
              stateMachine -> stateMachine.sendMessage(HeadsetStateMachine.A2DP_STATE_CHANGED,
                                    state));
    }

    /**
     * Check if no audio mode is active
     *
     * @return false if virtual call, voice recognition, or Telecom call is active, true if all idle
     */
    private boolean isAudioModeIdle() {
        synchronized (mStateMachines) {
            if (mVoiceRecognitionStarted || mVirtualCallStarted || !mSystemInterface.isCallIdle()) {
                Log.i(TAG, "isAudioModeIdle: not idle, mVoiceRecognitionStarted="
                        + mVoiceRecognitionStarted + ", mVirtualCallStarted=" + mVirtualCallStarted
                        + ", isCallIdle=" + mSystemInterface.isCallIdle());
                return false;
            }
            return true;
        }
    }

    private boolean shouldCallAudioBeActive() {
        boolean retVal = false;
        // When the call is active/held, the call audio must be active
        if (mSystemInterface.getHeadsetPhoneState().getNumActiveCall() > 0 ||
            mSystemInterface.getHeadsetPhoneState().getNumHeldCall() > 0 ) {
            Log.d(TAG, "shouldCallAudioBeActive(): returning true, since call is active/held");
            return true;
        }
        // When call is in  ringing state, SCO should not be accepted if
        // in-band ringtone is not enabled
        retVal = (mSystemInterface.isInCall() && !mSystemInterface.isRinging() )||
                 (mSystemInterface.isRinging() && isInbandRingingEnabled());
        Log.d(TAG, "shouldCallAudioBeActive() returning " + retVal);
        return retVal;
    }

    /**
     * Only persist audio during active device switch when call audio is supposed to be active and
     * virtual call has not been started. Virtual call is ignored because AudioService and
     * applications should reconnect SCO during active device switch and forcing SCO connection
     * here will make AudioService think SCO is started externally instead of by one of its SCO
     * clients.
     *
     * @return true if call audio should be active and no virtual call is going on
     */
    private boolean shouldPersistAudio() {
        return !mVirtualCallStarted && shouldCallAudioBeActive();
    }

    /**
     * Called from {@link HeadsetStateMachine} in state machine thread when there is a audio
     * connection state change
     *
     * @param device remote device
     * @param fromState from which audio connection state is the change
     * @param toState to which audio connection state is the change
     */
    @VisibleForTesting
    public void onAudioStateChangedFromStateMachine(BluetoothDevice device, int fromState,
            int toState) {
        Log.w(TAG, "onAudioStateChangedFromStateMachine: " + fromState + "->" + toState);
        synchronized (mStateMachines) {
            if (toState == BluetoothHeadset.STATE_AUDIO_DISCONNECTED) {
                //Transfer SCO is not needed for TWS+ devices
                if (mAdapterService != null && mAdapterService.isTwsPlusDevice(device) &&
                   mActiveDevice != null &&
                   mAdapterService.isTwsPlusDevice(mActiveDevice) &&
                   isAudioOn()) {
                    Log.w(TAG, "Sco transfer is not needed btween earbuds");
                } else {
                    // trigger SCO after SCO disconnected with previous active
                    // device
                    Log.w(TAG, "onAudioStateChangedFromStateMachine:"
                            + "shouldPersistAudio() returns"
                            + shouldPersistAudio());
                    if (mAdapterService != null && mAdapterService.isTwsPlusDevice(device) &&
                                   isAudioOn()) {
                        Log.w(TAG, "TWS: Don't stop VR or VOIP");
                    } else {
                        if (mVoiceRecognitionStarted) {
                            if (!stopVoiceRecognitionByHeadset(device)) {
                                Log.w(TAG,"onAudioStateChangedFromStateMachine:"
                                    + " failed to stop voice"
                                    + " recognition");
                            } else {
                                final HeadsetStateMachine stateMachine
                                                  = mStateMachines.get(device);
                                if (stateMachine != null) {
                                Log.d(TAG, "onAudioStateChanged" +
                                        "FromStateMachine: send +bvra:0");
                                  stateMachine.sendMessage(
                                    HeadsetStateMachine.VOICE_RECOGNITION_STOP,
                                    device);
                                }
                            }
                        }
                        if (mVirtualCallStarted) {
                            if (!stopScoUsingVirtualVoiceCall()) {
                                Log.w(TAG,"onAudioStateChangedFromStateMachine:"
                                    + " failed to stop virtual "
                                    + " voice call");
                            }
                        }
                    }

                    if (mActiveDevice != null &&
                                 !mActiveDevice.equals(device) &&
                                 shouldPersistAudio()) {
                       if (mAdapterService != null && mAdapterService.isTwsPlusDevice(device)
                                                   && isAudioOn()) {
                           Log.d(TAG, "TWS: Wait for both eSCO closed");
                       } else {
                           if (mAdapterService != null && mAdapterService.isTwsPlusDevice(device) &&
                               isTwsPlusActive(mActiveDevice)) {
                               /* If the device for which SCO got disconnected
                                  is a TwsPlus device and TWS+ set is active
                                  device. This should be the case where User
                                  transferred from BT to Phone Speaker from
                                  Call UI*/
                                  Log.d(TAG,"don't transfer SCO. It is an" +
                                             "explicit voice transfer from UI");
                                  return;
                           }
                           Log.d(TAG, "onAudioStateChangedFromStateMachine:"
                              + " triggering SCO with device "
                              + mActiveDevice);
                           if (!connectAudio(mActiveDevice)) {
                               Log.w(TAG, "onAudioStateChangedFromStateMachine,"
                               + " failed to connect"
                               + " audio to new " + "active device "
                               + mActiveDevice
                               + ", after " + device
                               + " is disconnected from SCO");
                           }
                       }
                    }
                }
            }
        }
    }

    private void broadcastActiveDevice(BluetoothDevice device) {
        logD("broadcastActiveDevice: " + device);
        StatsLog.write(StatsLog.BLUETOOTH_ACTIVE_DEVICE_CHANGED, BluetoothProfile.HEADSET,
                mAdapterService.obfuscateAddress(device));
        Intent intent = new Intent(BluetoothHeadset.ACTION_ACTIVE_DEVICE_CHANGED);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
        intent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY_BEFORE_BOOT
                | Intent.FLAG_RECEIVER_INCLUDE_BACKGROUND);
        sendBroadcastAsUser(intent, UserHandle.ALL, HeadsetService.BLUETOOTH_PERM);
    }

    /**
     * Check whether it is OK to accept a headset connection from a remote device
     *
     * @param device remote device that initiates the connection
     * @return true if the connection is acceptable
     */
    public boolean okToAcceptConnection(BluetoothDevice device) {
        // Check if this is an incoming connection in Quiet mode.
        boolean isPts = SystemProperties.getBoolean("vendor.bt.pts.certification", false);
        if (mAdapterService.isQuietModeEnabled()) {
            Log.w(TAG, "okToAcceptConnection: return false as quiet mode enabled");
            return false;
        }
        if(!isPts) {
            // Check priority and accept or reject the connection.
            int priority = getPriority(device);
            int bondState = mAdapterService.getBondState(device);
            // Allow this connection only if the device is bonded. Any attempt to connect while
            // bonding would potentially lead to an unauthorized connection.
            if (bondState != BluetoothDevice.BOND_BONDED) {
                Log.w(TAG, "okToAcceptConnection: return false, bondState=" + bondState);
                return false;
            } else if (priority != BluetoothProfile.PRIORITY_UNDEFINED
                    && priority != BluetoothProfile.PRIORITY_ON
                    && priority != BluetoothProfile.PRIORITY_AUTO_CONNECT) {
                // Otherwise, reject the connection if priority is not valid.
                Log.w(TAG, "okToAcceptConnection: return false, priority=" + priority);
                return false;
            }
        }
        List<BluetoothDevice> connectingConnectedDevices =
                getAllDevicesMatchingConnectionStates(CONNECTING_CONNECTED_STATES);
        if (!isConnectionAllowed(device, connectingConnectedDevices)) {
            Log.w(TAG, "Maximum number of connections " + mSetMaxConfig
                    + " was reached, rejecting connection from " + device);
            return false;
        }
        return true;
    }
    /**
     * Checks if SCO should be connected at current system state
     *
     * @param device device for SCO to be connected
     * @return true if SCO is allowed to be connected
     */
    public boolean isScoAcceptable(BluetoothDevice device) {
        synchronized (mStateMachines) {
            //allow 2nd eSCO from non-active tws+ earbud as well
            if (!mAdapterService.isTwsPlusDevice(device)) {
                if (device == null || !device.equals(mActiveDevice)) {
                    Log.w(TAG, "isScoAcceptable: rejected SCO since " + device
                        + " is not the current active device " + mActiveDevice);
                    return false;
                }
            }
            if (mForceScoAudio) {
                return true;
            }
            if (!mAudioRouteAllowed) {
                Log.w(TAG, "isScoAcceptable: rejected SCO since audio route is not allowed");
                return false;
            }
            /* if in-band ringtone is not enabled and if there is
                no active/held/dialling/alerting call, return false */
            if (isRinging() && !isInbandRingingEnabled() &&
                !(mSystemInterface.getHeadsetPhoneState().getNumActiveCall() > 0 ||
                  mSystemInterface.getHeadsetPhoneState().getNumHeldCall() > 0 )) {
                Log.w(TAG, "isScoAcceptable: rejected SCO since MT call in ringing," +
                            "in-band ringing not enabled");
                return false;
            }
            if (mVoiceRecognitionStarted || mVirtualCallStarted) {
                return true;
            }
            if (shouldCallAudioBeActive()) {
                return true;
            }
            Log.w(TAG, "isScoAcceptable: rejected SCO, inCall=" + mSystemInterface.isInCall()
                    + ", voiceRecognition=" + mVoiceRecognitionStarted + ", ringing="
                    + mSystemInterface.isRinging() + ", inbandRinging=" + isInbandRingingEnabled()
                    + ", isVirtualCallStarted=" + mVirtualCallStarted);
            return false;
        }
    }

    /**
     * Remove state machine in {@link #mStateMachines} for a {@link BluetoothDevice}
     *
     * @param device device whose state machine is to be removed.
     */
    void removeStateMachine(BluetoothDevice device) {
        synchronized (mStateMachines) {
            HeadsetStateMachine stateMachine = mStateMachines.get(device);
            if (stateMachine == null) {
                Log.w(TAG, "removeStateMachine(), " + device + " does not have a state machine");
                return;
            }
            Log.i(TAG, "removeStateMachine(), removing state machine for device: " + device);
            HeadsetObjectsFactory.getInstance().destroyStateMachine(stateMachine);
            mStateMachines.remove(device);
        }
    }

    private boolean isOnStateMachineThread() {
        final Looper myLooper = Looper.myLooper();
        return myLooper != null && (mStateMachinesThread != null) && (myLooper.getThread().getId()
                == mStateMachinesThread.getId());
    }

    @Override
    public void dump(StringBuilder sb) {
        synchronized (mStateMachines) {
            super.dump(sb);
            ProfileService.println(sb, "mMaxHeadsetConnections: " + mMaxHeadsetConnections);
            ProfileService.println(sb, "DefaultMaxHeadsetConnections: "
                    + mAdapterService.getMaxConnectedAudioDevices());
            ProfileService.println(sb, "mActiveDevice: " + mActiveDevice);
            ProfileService.println(sb, "isInbandRingingEnabled: " + isInbandRingingEnabled());
            ProfileService.println(sb,
                    "isInbandRingingSupported: " + BluetoothHeadset.isInbandRingingSupported(this));
            ProfileService.println(sb,
                    "mInbandRingingRuntimeDisable: " + mInbandRingingRuntimeDisable);
            ProfileService.println(sb, "mAudioRouteAllowed: " + mAudioRouteAllowed);
            ProfileService.println(sb, "mVoiceRecognitionStarted: " + mVoiceRecognitionStarted);
            ProfileService.println(sb,
                    "mVoiceRecognitionTimeoutEvent: " + mVoiceRecognitionTimeoutEvent);
            ProfileService.println(sb, "mVirtualCallStarted: " + mVirtualCallStarted);
            ProfileService.println(sb, "mDialingOutTimeoutEvent: " + mDialingOutTimeoutEvent);
            ProfileService.println(sb, "mForceScoAudio: " + mForceScoAudio);
            ProfileService.println(sb, "mCreated: " + mCreated);
            ProfileService.println(sb, "mStarted: " + mStarted);
            ProfileService.println(sb,
                    "AudioManager.isBluetoothScoOn(): " + mSystemInterface.getAudioManager()
                            .isBluetoothScoOn());
            ProfileService.println(sb, "Telecom.isInCall(): " + mSystemInterface.isInCall());
            ProfileService.println(sb, "Telecom.isRinging(): " + mSystemInterface.isRinging());
            for (HeadsetStateMachine stateMachine : mStateMachines.values()) {
                ProfileService.println(sb,
                        "==== StateMachine for " + stateMachine.getDevice() + " ====");
                stateMachine.dump(sb);
            }
        }
    }

    public void enableSwbCodec(boolean enable) {
        mVendorHf.enableSwb(enable);
    }

    public boolean isSwbEnabled() {
	if(mAdapterService.isSWBVoicewithAptxAdaptiveAG()) {
            return mAdapterService.isSwbEnabled();
        }
        return false;
    }

    public boolean isSwbPmEnabled() {
        if(mAdapterService.isSWBVoicewithAptxAdaptiveAG() &&
           mAdapterService.isSwbEnabled()) {
            return mAdapterService.isSwbPmEnabled();
        }
        return false;
    }

    private static void logD(String message) {
        if (DBG) {
            Log.d(TAG, message);
        }
    }
}
