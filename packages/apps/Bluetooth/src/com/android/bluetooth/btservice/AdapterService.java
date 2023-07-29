/*
 * Copyright (C) 2017, The Linux Foundation. All rights reserved.
 * Not a Contribution.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the
 * disclaimer below) provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *
 * * Neither the name of The Linux Foundation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
 * GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
 * HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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

package com.android.bluetooth.btservice;

import android.app.ActivityManager;
import android.app.AlarmManager;
import android.app.AppOpsManager;
import android.app.PendingIntent;
import android.app.Service;
import android.bluetooth.BluetoothActivityEnergyInfo;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothClass;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothProtoEnums;
import android.bluetooth.IBluetooth;
import android.bluetooth.IBluetoothCallback;
import android.bluetooth.IBluetoothMetadataListener;
import android.bluetooth.IBluetoothSocketManager;
import android.bluetooth.OobData;
import android.bluetooth.UidTraffic;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.os.AsyncTask;
import android.os.BatteryStats;
import android.os.Binder;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.ParcelUuid;
import android.os.PowerManager;
import android.os.Process;
import android.os.RemoteCallbackList;
import android.os.RemoteException;
import android.os.ResultReceiver;
import android.os.ServiceManager;
import android.os.SystemClock;
import android.os.SystemProperties;
import android.os.UserHandle;
import android.os.UserManager;
import android.provider.Settings;
import android.text.TextUtils;
import android.util.Base64;
import android.util.Log;
import android.util.Slog;
import android.util.SparseArray;
import android.util.StatsLog;

import com.android.bluetooth.BluetoothMetricsProto;
import com.android.bluetooth.Utils;
import com.android.bluetooth.btservice.RemoteDevices.DeviceProperties;
import com.android.bluetooth.btservice.storage.DatabaseManager;
import com.android.bluetooth.btservice.storage.MetadataDatabase;
import com.android.bluetooth.gatt.GattService;
import com.android.bluetooth.sdp.SdpManager;
import com.android.bluetooth.ba.BATService;
import com.android.internal.R;
import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.app.IBatteryStats;

import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.net.NetworkInfo;
import android.net.wifi.WifiConfiguration;

import com.google.protobuf.InvalidProtocolBufferException;

import java.io.FileDescriptor;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;

public class AdapterService extends Service {
    private static final String TAG = "BluetoothAdapterService";
    private static final boolean DBG = true;
    private static final boolean VERBOSE = false;
    private static final int MIN_ADVT_INSTANCES_FOR_MA = 5;
    private static final int MIN_OFFLOADED_FILTERS = 10;
    private static final int MIN_OFFLOADED_SCAN_STORAGE_BYTES = 1024;

    private final Object mEnergyInfoLock = new Object();
    private int mStackReportedState;
    private long mTxTimeTotalMs;
    private long mRxTimeTotalMs;
    private long mIdleTimeTotalMs;
    private long mEnergyUsedTotalVoltAmpSecMicro;
    private WifiManager mWifiManager;
    private final SparseArray<UidTraffic> mUidTraffic = new SparseArray<>();

    private final ArrayList<ProfileService> mRegisteredProfiles = new ArrayList<>();
    private final ArrayList<ProfileService> mRunningProfiles = new ArrayList<>();

    public static final String ACTION_LOAD_ADAPTER_PROPERTIES =
            "com.android.bluetooth.btservice.action.LOAD_ADAPTER_PROPERTIES";
    public static final String ACTION_SERVICE_STATE_CHANGED =
            "com.android.bluetooth.btservice.action.STATE_CHANGED";
    public static final String EXTRA_ACTION = "action";
    public static final int PROFILE_CONN_REJECTED = 2;

    private static final String ACTION_ALARM_WAKEUP =
            "com.android.bluetooth.btservice.action.ALARM_WAKEUP";

    static final String BLUETOOTH_BTSNOOP_LOG_MODE_PROPERTY = "persist.bluetooth.btsnooplogmode";
    static final String BLUETOOTH_BTSNOOP_DEFAULT_MODE_PROPERTY =
            "persist.bluetooth.btsnoopdefaultmode";
    private String mSnoopLogSettingAtEnable = "empty";
    private String mDefaultSnoopLogSettingAtEnable = "empty";

    public static final String BLUETOOTH_ADMIN_PERM = android.Manifest.permission.BLUETOOTH_ADMIN;
    public static final String BLUETOOTH_PRIVILEGED =
            android.Manifest.permission.BLUETOOTH_PRIVILEGED;
    static final String BLUETOOTH_PERM = android.Manifest.permission.BLUETOOTH;
    static final String LOCAL_MAC_ADDRESS_PERM = android.Manifest.permission.LOCAL_MAC_ADDRESS;
    static final String RECEIVE_MAP_PERM = android.Manifest.permission.RECEIVE_BLUETOOTH_MAP;

    private static final String PHONEBOOK_ACCESS_PERMISSION_PREFERENCE_FILE =
            "phonebook_access_permission";
    private static final String MESSAGE_ACCESS_PERMISSION_PREFERENCE_FILE =
            "message_access_permission";
    private static final String SIM_ACCESS_PERMISSION_PREFERENCE_FILE = "sim_access_permission";

    private static final int CONTROLLER_ENERGY_UPDATE_TIMEOUT_MILLIS = 30;

    private final ArrayList<DiscoveringPackage> mDiscoveringPackages = new ArrayList<>();

    static {
        System.loadLibrary("bluetooth_jni");
        classInitNative();
    }

    private static AdapterService sAdapterService;

    public static synchronized AdapterService getAdapterService() {
        Log.d(TAG, "getAdapterService() - returning " + sAdapterService);
        return sAdapterService;
    }

    private static synchronized void setAdapterService(AdapterService instance) {
        Log.d(TAG, "setAdapterService() - trying to set service to " + instance);
        if (instance == null) {
            return;
        }
        sAdapterService = instance;
    }

    private static synchronized void clearAdapterService(AdapterService current) {
        if (sAdapterService == current) {
            sAdapterService = null;
        }
    }

    private AdapterProperties mAdapterProperties;
    private AdapterState mAdapterStateMachine;
    private Vendor mVendor;
    private boolean mVendorAvailble;
    private BondStateMachine mBondStateMachine;
    private JniCallbacks mJniCallbacks;
    private RemoteDevices mRemoteDevices;

    /* TODO: Consider to remove the search API from this class, if changed to use call-back */
    private SdpManager mSdpManager = null;

    private boolean mNativeAvailable;
    private boolean mCleaningUp;
    private final HashMap<BluetoothDevice, ArrayList<IBluetoothMetadataListener>>
            mMetadataListeners = new HashMap<>();
    private final HashMap<String, Integer> mProfileServicesState = new HashMap<String, Integer>();
    //Only BluetoothManagerService should be registered
    private RemoteCallbackList<IBluetoothCallback> mCallbacks;
    private int mCurrentRequestId;
    private boolean mQuietmode = false;

    private AlarmManager mAlarmManager;
    private PendingIntent mPendingAlarm;
    private IBatteryStats mBatteryStats;
    private PowerManager mPowerManager;
    private PowerManager.WakeLock mWakeLock;
    private String mWakeLockName;
    private UserManager mUserManager;
    private static BluetoothAdapter mAdapter;

    private ProfileObserver mProfileObserver;
    private PhonePolicy mPhonePolicy;
    private ActiveDeviceManager mActiveDeviceManager;
    private DatabaseManager mDatabaseManager;
    private SilenceDeviceManager mSilenceDeviceManager;
    private AppOpsManager mAppOps;
    private VendorSocket mVendorSocket;

    /**
     * Register a {@link ProfileService} with AdapterService.
     *
     * @param profile the service being added.
     */
    public void addProfile(ProfileService profile) {
        mHandler.obtainMessage(MESSAGE_PROFILE_SERVICE_REGISTERED, profile).sendToTarget();
    }

    /**
     * Unregister a ProfileService with AdapterService.
     *
     * @param profile the service being removed.
     */
    public void removeProfile(ProfileService profile) {
        mHandler.obtainMessage(MESSAGE_PROFILE_SERVICE_UNREGISTERED, profile).sendToTarget();
    }

    /**
     * Notify AdapterService that a ProfileService has started or stopped.
     *
     * @param profile the service being removed.
     * @param state {@link BluetoothAdapter#STATE_ON} or {@link BluetoothAdapter#STATE_OFF}
     */
    public void onProfileServiceStateChanged(ProfileService profile, int state) {
        if (state != BluetoothAdapter.STATE_ON && state != BluetoothAdapter.STATE_OFF) {
            throw new IllegalArgumentException(BluetoothAdapter.nameForState(state));
        }
        Message m = mHandler.obtainMessage(MESSAGE_PROFILE_SERVICE_STATE_CHANGED);
        m.obj = profile;
        m.arg1 = state;
        mHandler.sendMessage(m);
    }

    public boolean getProfileInfo(int profile_id , int profile_info) {
        if (isVendorIntfEnabled()) {
            return mVendor.getProfileInfo(profile_id, profile_info);
        } else {
            return false;
        }
    }

    private void fetchWifiState() {
        if (!isVendorIntfEnabled()) {
            return;
        }
        boolean isWifiConnected = false;
        WifiManager wifiMgr = (WifiManager) getSystemService(Context.WIFI_SERVICE);
        if ((wifiMgr != null) && (wifiMgr.isWifiEnabled())) {
            WifiInfo wifiInfo = wifiMgr.getConnectionInfo();
            if((wifiInfo != null) && (wifiInfo.getNetworkId() != -1)) {
                isWifiConnected = true;
            }
        }
        mVendor.setWifiState(isWifiConnected);
    }

    public void StartHCIClose() {
        if (isVendorIntfEnabled()) {
            mVendor.HCIClose();
        }
    }

     public void voipNetworkWifiInfo(boolean isVoipStarted, boolean isNetworkWifi) {
        Log.i(TAG, "In voipNetworkWifiInfo, isVoipStarted: " + isVoipStarted +
                    ", isNetworkWifi: " + isNetworkWifi);
        mVendor.voipNetworkWifiInformation(isVoipStarted, isNetworkWifi);
    }

    public String getSocName() {
        return mVendor.getSocName();
    }

    public String getA2apOffloadCapability() {
        return mVendor.getA2apOffloadCapability();
    }

    public boolean isSplitA2dpEnabled() {
        return mVendor.isSplitA2dpEnabled();
    }

    public boolean isSwbEnabled() {
        return mVendor.isSwbEnabled();
    }
    public boolean isSwbPmEnabled() {
        return mVendor.isSwbPmEnabled();
    }

    private static final int MESSAGE_PROFILE_SERVICE_STATE_CHANGED = 1;
    private static final int MESSAGE_PROFILE_SERVICE_REGISTERED = 2;
    private static final int MESSAGE_PROFILE_SERVICE_UNREGISTERED = 3;

    class AdapterServiceHandler extends Handler {
        @Override
        public void handleMessage(Message msg) {
            verboseLog("handleMessage() - Message: " + msg.what);

            switch (msg.what) {
                case MESSAGE_PROFILE_SERVICE_STATE_CHANGED:
                    verboseLog("handleMessage() - MESSAGE_PROFILE_SERVICE_STATE_CHANGED");
                    processProfileServiceStateChanged((ProfileService) msg.obj, msg.arg1);
                    break;
                case MESSAGE_PROFILE_SERVICE_REGISTERED:
                    verboseLog("handleMessage() - MESSAGE_PROFILE_SERVICE_REGISTERED");
                    registerProfileService((ProfileService) msg.obj);
                    break;
                case MESSAGE_PROFILE_SERVICE_UNREGISTERED:
                    verboseLog("handleMessage() - MESSAGE_PROFILE_SERVICE_UNREGISTERED");
                    unregisterProfileService((ProfileService) msg.obj);
                    break;
            }
        }

        private void registerProfileService(ProfileService profile) {
            if (mRegisteredProfiles.contains(profile)) {
                Log.e(TAG, profile.getName() + " already registered.");
                return;
            }
            mRegisteredProfiles.add(profile);
        }

        private void unregisterProfileService(ProfileService profile) {
            if (!mRegisteredProfiles.contains(profile)) {
                Log.e(TAG, profile.getName() + " not registered (UNREGISTER).");
                return;
            }
            mRegisteredProfiles.remove(profile);
        }

        private void processProfileServiceStateChanged(ProfileService profile, int state) {
            switch (state) {
                case BluetoothAdapter.STATE_ON:
                    if (!mRegisteredProfiles.contains(profile)) {
                        Log.e(TAG, profile.getName() + " not registered (STATE_ON).");
                        return;
                    }
                    if (mRunningProfiles.contains(profile)) {
                        Log.e(TAG, profile.getName() + " already running.");
                        return;
                    }
                    mRunningProfiles.add(profile);
                    if (GattService.class.getSimpleName().equals(profile.getName())) {
                        Log.w(TAG,"onProfileServiceStateChange() - Gatt profile service started..");
                        enableNative();
                    } else if (mRegisteredProfiles.size() == Config.getSupportedProfiles().length
                            && mRegisteredProfiles.size() == mRunningProfiles.size()) {
                        Log.w(TAG,"onProfileServiceStateChange() - All profile services started..");
                        mAdapterProperties.onBluetoothReady();
                        updateUuids();
                        setBluetoothClassFromConfig();
                        getAdapterPropertyNative(AbstractionLayer.BT_PROPERTY_LOCAL_IO_CAPS);
                        getAdapterPropertyNative(AbstractionLayer.BT_PROPERTY_LOCAL_IO_CAPS_BLE);
                        mAdapterStateMachine.sendMessage(AdapterState.BREDR_STARTED);
                        //update wifi state to lower layers
                        fetchWifiState();
                        if (isVendorIntfEnabled()) {
                            if (isPowerbackRequired()) {
                                mVendor.setPowerBackoff(true);
                            } else {
                                mVendor.setPowerBackoff(false);
                            }
                        }
                    }
                    break;
                case BluetoothAdapter.STATE_OFF:
                    if (!mRegisteredProfiles.contains(profile)) {
                        Log.e(TAG, profile.getName() + " not registered (STATE_OFF).");
                        return;
                    }
                    if (!mRunningProfiles.contains(profile)) {
                        Log.e(TAG, profile.getName() + " not running.");
                        return;
                    }
                    mRunningProfiles.remove(profile);
                    // If only GATT is left, send BREDR_STOPPED.
                    if ((mRunningProfiles.size() == 1 && (GattService.class.getSimpleName()
                            .equals(mRunningProfiles.get(0).getName())))) {
                        Log.w(TAG,"onProfileServiceStateChange() - All profile services except gatt stopped..");
                        mAdapterStateMachine.sendMessage(AdapterState.BREDR_STOPPED);
                    } else if (mRunningProfiles.size() == 0) {
                        Log.w(TAG,"onProfileServiceStateChange() - All profile services stopped..");
                        mAdapterStateMachine.sendMessage(AdapterState.BLE_STOPPED);
                        disableNative();
                    }
                    break;
                default:
                    Log.e(TAG, "Unhandled profile state: " + state);
            }
        }
    }

    private final AdapterServiceHandler mHandler = new AdapterServiceHandler();

    private void updateInteropDatabase() {
        interopDatabaseClearNative();

        String interopString = Settings.Global.getString(getContentResolver(),
                Settings.Global.BLUETOOTH_INTEROPERABILITY_LIST);
        if (interopString == null) {
            return;
        }
        Log.d(TAG, "updateInteropDatabase: [" + interopString + "]");

        String[] entries = interopString.split(";");
        for (String entry : entries) {
            String[] tokens = entry.split(",");
            if (tokens.length != 2) {
                continue;
            }

            // Get feature
            int feature = 0;
            try {
                feature = Integer.parseInt(tokens[1]);
            } catch (NumberFormatException e) {
                Log.e(TAG, "updateInteropDatabase: Invalid feature '" + tokens[1] + "'");
                continue;
            }

            // Get address bytes and length
            int length = (tokens[0].length() + 1) / 3;
            if (length < 1 || length > 6) {
                Log.e(TAG, "updateInteropDatabase: Malformed address string '" + tokens[0] + "'");
                continue;
            }

            byte[] addr = new byte[6];
            int offset = 0;
            for (int i = 0; i < tokens[0].length(); ) {
                if (tokens[0].charAt(i) == ':') {
                    i += 1;
                } else {
                    try {
                        addr[offset++] = (byte) Integer.parseInt(tokens[0].substring(i, i + 2), 16);
                    } catch (NumberFormatException e) {
                        offset = 0;
                        break;
                    }
                    i += 2;
                }
            }

            // Check if address was parsed ok, otherwise, move on...
            if (offset == 0) {
                continue;
            }

            // Add entry
            interopDatabaseAddNative(feature, addr, length);
        }
    }

    @Override
    public void onCreate() {
        super.onCreate();
        debugLog("onCreate()");
        mAdapter = BluetoothAdapter.getDefaultAdapter();
        mRemoteDevices = new RemoteDevices(this, Looper.getMainLooper());
        mRemoteDevices.init();
        clearDiscoveringPackages();
        mBinder = new AdapterServiceBinder(this);
        mAdapterProperties = new AdapterProperties(this);
        mVendor = new Vendor(this);
        mAdapterStateMachine =  AdapterState.make(this);
        mJniCallbacks = new JniCallbacks(this, mAdapterProperties);
        mVendorSocket = new VendorSocket(this);
        initNative(isGuest(), isNiapMode());
        mNativeAvailable = true;
        mCallbacks = new RemoteCallbackList<IBluetoothCallback>();
        mAppOps = getSystemService(AppOpsManager.class);
        //Load the name and address
        getAdapterPropertyNative(AbstractionLayer.BT_PROPERTY_BDADDR);
        getAdapterPropertyNative(AbstractionLayer.BT_PROPERTY_BDNAME);
        getAdapterPropertyNative(AbstractionLayer.BT_PROPERTY_CLASS_OF_DEVICE);
        mAlarmManager = (AlarmManager) getSystemService(Context.ALARM_SERVICE);
        mPowerManager = (PowerManager) getSystemService(Context.POWER_SERVICE);
        mUserManager = (UserManager) getSystemService(Context.USER_SERVICE);
        mBatteryStats = IBatteryStats.Stub.asInterface(
                ServiceManager.getService(BatteryStats.SERVICE_NAME));

        mSdpManager = SdpManager.init(this);
        registerReceiver(mAlarmBroadcastReceiver, new IntentFilter(ACTION_ALARM_WAKEUP));
        IntentFilter wifiFilter = new IntentFilter();
        wifiFilter.addAction(WifiManager.NETWORK_STATE_CHANGED_ACTION);
        wifiFilter.addAction(WifiManager.WIFI_AP_STATE_CHANGED_ACTION);
        wifiFilter.addAction(WifiManager.WIFI_STATE_CHANGED_ACTION);
        registerReceiver(mWifiStateBroadcastReceiver, wifiFilter);
        mProfileObserver = new ProfileObserver(getApplicationContext(), this, new Handler());
        mProfileObserver.start();

        // Phone policy is specific to phone implementations and hence if a device wants to exclude
        // it out then it can be disabled by using the flag below.
        if (getResources().getBoolean(com.android.bluetooth.R.bool.enable_phone_policy)) {
            Log.i(TAG, "Phone policy enabled");
            mPhonePolicy = new PhonePolicy(this, new ServiceFactory());
            mPhonePolicy.start();
        } else {
            Log.i(TAG, "Phone policy disabled");
        }
        mBondStateMachine = BondStateMachine.make(mPowerManager, this, mAdapterProperties, mRemoteDevices);

        mActiveDeviceManager = new ActiveDeviceManager(this, new ServiceFactory());
        mActiveDeviceManager.start();

        mDatabaseManager = new DatabaseManager(this);
        mDatabaseManager.start(MetadataDatabase.createDatabase(this));

        mSilenceDeviceManager = new SilenceDeviceManager(this, new ServiceFactory(),
                Looper.getMainLooper());
        mSilenceDeviceManager.start();

        setAdapterService(this);

        // First call to getSharedPreferences will result in a file read into
        // memory cache. Call it here asynchronously to avoid potential ANR
        // in the future
        new AsyncTask<Void, Void, Void>() {
            @Override
            protected Void doInBackground(Void... params) {
                getSharedPreferences(PHONEBOOK_ACCESS_PERMISSION_PREFERENCE_FILE,
                        Context.MODE_PRIVATE);
                getSharedPreferences(MESSAGE_ACCESS_PERMISSION_PREFERENCE_FILE,
                        Context.MODE_PRIVATE);
                getSharedPreferences(SIM_ACCESS_PERMISSION_PREFERENCE_FILE, Context.MODE_PRIVATE);
                return null;
            }
        }.execute();
        mVendor.init();
        mVendorAvailble = mVendor.getQtiStackStatus();
        mVendorSocket.init();

        try {
            int systemUiUid = getApplicationContext().getPackageManager().getPackageUidAsUser(
                    "com.android.systemui", PackageManager.MATCH_SYSTEM_ONLY,
                    UserHandle.USER_SYSTEM);
            Utils.setSystemUiUid(systemUiUid);
            setSystemUiUidNative(systemUiUid);
        } catch (PackageManager.NameNotFoundException e) {
            // Some platforms, such as wearables do not have a system ui.
            Log.w(TAG, "Unable to resolve SystemUI's UID.", e);
        }

        IntentFilter filter = new IntentFilter(Intent.ACTION_USER_SWITCHED);
        getApplicationContext().registerReceiverAsUser(sUserSwitchedReceiver, UserHandle.ALL,
                filter, null, null);
        int fuid = ActivityManager.getCurrentUser();
        Utils.setForegroundUserId(fuid);
        setForegroundUserIdNative(fuid);

        // Reset |mRemoteDevices| whenever BLE is turned off then on
        // This is to replace the fact that |mRemoteDevices| was
        // reinitialized in previous code.
        //
        // TODO(apanicke): The reason is unclear but
        // I believe it is to clear the variable every time BLE was
        // turned off then on. The same effect can be achieved by
        // calling cleanup but this may not be necessary at all
        // We should figure out why this is needed later
        mRemoteDevices.reset();
        mAdapterProperties.init(mRemoteDevices);
    }

    @Override
    public IBinder onBind(Intent intent) {
        debugLog("onBind()");
        return mBinder;
    }

    @Override
    public boolean onUnbind(Intent intent) {
        Log.w(TAG, "onUnbind, calling cleanup");
        cleanup();
        return super.onUnbind(intent);
    }

    @Override
    public void onDestroy() {
        debugLog("onDestroy()");
        mProfileObserver.stop();
        if (!isMock()) {
            // TODO(b/27859763)
            Log.i(TAG, "Force exit to cleanup internal state in Bluetooth stack");
            System.exit(0);
        }
    }

    public static final BroadcastReceiver sUserSwitchedReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (Intent.ACTION_USER_SWITCHED.equals(intent.getAction())) {
                int fuid = intent.getIntExtra(Intent.EXTRA_USER_HANDLE, 0);
                Utils.setForegroundUserId(fuid);
                setForegroundUserIdNative(fuid);
            }
        }
    };

    void bringUpBle() {
        debugLog("bleOnProcessStart()");
        /* To reload profile support in BLE turning ON state. So even if profile support
         *  is disabled in Bluetooth Adapter turned off state(10), this flag will ensure
         *  rechecking profile support after BT is turned ON
         */
        if (getResources().getBoolean(
                com.android.bluetooth.R.bool.reload_supported_profiles_when_enabled)) {
            Config.init(getApplicationContext());
        }

        debugLog("BleOnProcessStart() - Make Bond State Machine");

        mJniCallbacks.init(mBondStateMachine, mRemoteDevices);

        try {
            mBatteryStats.noteResetBleScan();
        } catch (RemoteException e) {
            Log.w(TAG, "RemoteException trying to send a reset to BatteryStats");
        }
        StatsLog.write_non_chained(StatsLog.BLE_SCAN_STATE_CHANGED, -1, null,
                StatsLog.BLE_SCAN_STATE_CHANGED__STATE__RESET, false, false, false);

        //Start Gatt service
        setProfileServiceState(GattService.class, BluetoothAdapter.STATE_ON);
    }

    void bringDownBle() {
        stopGattProfileService();
    }

    void stateChangeCallback(int status) {
        if (status == AbstractionLayer.BT_STATE_OFF) {
            debugLog("stateChangeCallback: disableNative() completed");
            mAdapterStateMachine.sendMessage(AdapterState.STACK_DISABLED);
        } else if (status == AbstractionLayer.BT_STATE_ON) {
            String BT_SOC = getSocName();

            if (BT_SOC.equals("pronto")) {
                Log.i(TAG, "setting max audio connection to 2");
                mAdapterProperties.setMaxConnectedAudioDevices(2);
            }
            mAdapterStateMachine.sendMessage(AdapterState.BLE_STARTED);
        } else {
            Log.e(TAG, "Incorrect status " + status + " in stateChangeCallback");
        }
    }

    void ssrCleanupCallback() {
        disableProfileServices(false);
        Log.e(TAG, "Killing the process to force restart as part of fault tolerance");
        android.os.Process.killProcess(android.os.Process.myPid());
    }

    /**
     * Sets the Bluetooth CoD value of the local adapter if there exists a config value for it.
     */
    void setBluetoothClassFromConfig() {
        int bluetoothClassConfig = retrieveBluetoothClassConfig();
        if (bluetoothClassConfig != 0) {
            mAdapterProperties.setBluetoothClass(new BluetoothClass(bluetoothClassConfig));
        }
    }

    private int retrieveBluetoothClassConfig() {
        return Settings.Global.getInt(
                getContentResolver(), Settings.Global.BLUETOOTH_CLASS_OF_DEVICE, 0);
    }

    private boolean storeBluetoothClassConfig(int bluetoothClass) {
        boolean result = Settings.Global.putInt(
                getContentResolver(), Settings.Global.BLUETOOTH_CLASS_OF_DEVICE, bluetoothClass);

        if (!result) {
            Log.e(TAG, "Error storing BluetoothClass config - " + bluetoothClass);
        }

        return result;
    }

    void startBluetoothDisable() {
        mAdapterStateMachine.sendMessage(AdapterState.BEGIN_BREDR_STOP);
    }

    void startProfileServices() {
        debugLog("startCoreServices()");
        Class[] supportedProfileServices = Config.getSupportedProfiles();
        if (supportedProfileServices.length == 1 && GattService.class.getSimpleName()
                .equals(supportedProfileServices[0].getSimpleName())) {
            mAdapterProperties.onBluetoothReady();
            updateUuids();
            setBluetoothClassFromConfig();
            mAdapterStateMachine.sendMessage(AdapterState.BREDR_STARTED);
        } else {
            setAllProfileServiceStates(supportedProfileServices, BluetoothAdapter.STATE_ON);
        }
    }

    void startBrEdrStartup(){
        if (isVendorIntfEnabled()) {
            mVendor.bredrStartup();
        }
    }

    void startBrEdrCleanup(){
        mAdapterProperties.onBluetoothDisable();
        if (isVendorIntfEnabled()) {
            mVendor.bredrCleanup();
        } else {
            mAdapterStateMachine.sendMessage(
            mAdapterStateMachine.obtainMessage(AdapterState.BEGIN_BREDR_STOP));
        }
    }

    void stopProfileServices() {
        Class[] supportedProfileServices = Config.getSupportedProfiles();
        if (supportedProfileServices.length == 1 && (mRunningProfiles.size() == 1
                && GattService.class.getSimpleName().equals(mRunningProfiles.get(0).getName()))) {
            debugLog("stopProfileServices() - No profiles services to stop or already stopped.");
            mAdapterStateMachine.sendMessage(AdapterState.BREDR_STOPPED);
        } else {
            setAllProfileServiceStates(supportedProfileServices, BluetoothAdapter.STATE_OFF);
        }
    }

    void disableProfileServices(boolean onlyGatt) {
        Class[] services = Config.getSupportedProfiles();
        for (int i = 0; i < services.length; i++) {
            if (onlyGatt && !(GattService.class.getSimpleName().equals(services[i].getSimpleName())))
                continue;
            boolean res = false;
            String serviceName = services[i].getName();
            mProfileServicesState.put(serviceName,BluetoothAdapter.STATE_OFF);
            Intent intent = new Intent(this,services[i]);
            intent.putExtra(EXTRA_ACTION,ACTION_SERVICE_STATE_CHANGED);
            intent.putExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.STATE_OFF);
            intent.addFlags(Intent.FLAG_RECEIVER_FOREGROUND);
            res = stopService(intent);
            Log.d(TAG, "disableProfileServices() - Stopping service "
                + serviceName + " with result: " + res);
            if(onlyGatt)
                break;
        }
        return;
    }

    private void stopGattProfileService() {
        mAdapterProperties.onBleDisable();
        if (mRunningProfiles.size() == 0) {
            debugLog("stopGattProfileService() - No profiles services to stop.");
            mAdapterStateMachine.sendMessage(AdapterState.BLE_STOPPED);
        }
        setProfileServiceState(GattService.class, BluetoothAdapter.STATE_OFF);
    }

    void updateAdapterState(int prevState, int newState) {
        mAdapterProperties.setState(newState);
        if (mCallbacks != null) {
            int n = mCallbacks.beginBroadcast();
            debugLog("updateAdapterState() - Broadcasting state " + BluetoothAdapter.nameForState(
                    newState) + " to " + n + " receivers.");
            for (int i = 0; i < n; i++) {
                try {
                    mCallbacks.getBroadcastItem(i).onBluetoothStateChange(prevState, newState);
                } catch (RemoteException e) {
                    debugLog("updateAdapterState() - Callback #" + i + " failed (" + e + ")");
                }
            }
            mCallbacks.finishBroadcast();
        }

        // Turn the Adapter all the way off if we are disabling and the snoop log setting changed.
        if (newState == BluetoothAdapter.STATE_BLE_TURNING_ON) {
            mSnoopLogSettingAtEnable =
                    SystemProperties.get(BLUETOOTH_BTSNOOP_LOG_MODE_PROPERTY, "empty");
            mDefaultSnoopLogSettingAtEnable =
                    Settings.Global.getString(getContentResolver(),
                            Settings.Global.BLUETOOTH_BTSNOOP_DEFAULT_MODE);
            SystemProperties.set(BLUETOOTH_BTSNOOP_DEFAULT_MODE_PROPERTY,
                    mDefaultSnoopLogSettingAtEnable);
        } else if (newState == BluetoothAdapter.STATE_BLE_ON
                   && prevState != BluetoothAdapter.STATE_OFF) {
            String snoopLogSetting =
                    SystemProperties.get(BLUETOOTH_BTSNOOP_LOG_MODE_PROPERTY, "empty");
            String snoopDefaultModeSetting =
                    Settings.Global.getString(getContentResolver(),
                            Settings.Global.BLUETOOTH_BTSNOOP_DEFAULT_MODE);

            if (!TextUtils.equals(mSnoopLogSettingAtEnable, snoopLogSetting)
                    || !TextUtils.equals(mDefaultSnoopLogSettingAtEnable,
                            snoopDefaultModeSetting)) {
                mAdapterStateMachine.sendMessage(AdapterState.BLE_TURN_OFF);
            }
        }
    }

    void cleanup() {
        debugLog("cleanup()");
        if (mCleaningUp) {
            errorLog("cleanup() - Service already starting to cleanup, ignoring request...");
            return;
        }

        // Unregistering Bluetooth Adapter
        if ( mAdapter!= null ){
            mAdapter.unregisterAdapter();
            mAdapter = null;
        }

        clearAdapterService(this);

        mCleaningUp = true;

        unregisterReceiver(mAlarmBroadcastReceiver);
        unregisterReceiver(mWifiStateBroadcastReceiver);

        if (mPendingAlarm != null) {
            mAlarmManager.cancel(mPendingAlarm);
            mPendingAlarm = null;
        }

        // This wake lock release may also be called concurrently by
        // {@link #releaseWakeLock(String lockName)}, so a synchronization is needed here.
        synchronized (this) {
            if (mWakeLock != null) {
                if (mWakeLock.isHeld()) {
                    mWakeLock.release();
                }
                mWakeLock = null;
            }
        }

        if (mDatabaseManager != null) {
            mDatabaseManager.cleanup();
        }

        if (mAdapterStateMachine != null) {
            mAdapterStateMachine.doQuit();
        }

        if (mBondStateMachine != null) {
            mBondStateMachine.doQuit();
        }

        if (mRemoteDevices != null) {
            mRemoteDevices.cleanup();
        }

        if (mSdpManager != null) {
            mSdpManager.cleanup();
            mSdpManager = null;
        }

        if (mNativeAvailable) {
            debugLog("cleanup() - Cleaning up adapter native");
            cleanupNative();
            mNativeAvailable = false;
        }

        if (mAdapterProperties != null) {
            mAdapterProperties.cleanup();
        }

        if (mVendor != null) {
            mVendor.cleanup();
        }

        if (mVendorSocket!= null) {
            mVendorSocket.cleanup();
        }

        if (mJniCallbacks != null) {
            mJniCallbacks.cleanup();
        }

        if (mPhonePolicy != null) {
            mPhonePolicy.cleanup();
        }

        if (mSilenceDeviceManager != null) {
            mSilenceDeviceManager.cleanup();
        }

        if (mActiveDeviceManager != null) {
            mActiveDeviceManager.cleanup();
        }

        if (mProfileServicesState != null) {
            mProfileServicesState.clear();
        }

        if (mBinder != null) {
            mBinder.cleanup();
            mBinder = null;  //Do not remove. Otherwise Binder leak!
        }

        if (mCallbacks != null) {
            mCallbacks.kill();
        }
    }

    private void setProfileServiceState(Class service, int state) {
        Intent intent = new Intent(this, service);
        intent.putExtra(EXTRA_ACTION, ACTION_SERVICE_STATE_CHANGED);
        intent.putExtra(BluetoothAdapter.EXTRA_STATE, state);
        intent.addFlags(Intent.FLAG_RECEIVER_FOREGROUND);
        startService(intent);
    }

    private void setAllProfileServiceStates(Class[] services, int state) {
        for (Class service : services) {
            if (GattService.class.getSimpleName().equals(service.getSimpleName())) {
                continue;
            }
            setProfileServiceState(service, state);
        }
    }

    private boolean isAvailable() {
        return !mCleaningUp;
    }

    /**
     * Handlers for incoming service calls
     */
    private AdapterServiceBinder mBinder;

    /**
     * The Binder implementation must be declared to be a static class, with
     * the AdapterService instance passed in the constructor. Furthermore,
     * when the AdapterService shuts down, the reference to the AdapterService
     * must be explicitly removed.
     *
     * Otherwise, a memory leak can occur from repeated starting/stopping the
     * service...Please refer to android.os.Binder for further details on
     * why an inner instance class should be avoided.
     *
     */
    private static class AdapterServiceBinder extends IBluetooth.Stub {
        private AdapterService mService;

        AdapterServiceBinder(AdapterService svc) {
            mService = svc;
        }

        public void cleanup() {
            mService = null;
        }

        public AdapterService getService() {
            if (mService != null && mService.isAvailable()) {
                return mService;
            }
            return null;
        }

        @Override
        public boolean isEnabled() {
            // don't check caller, may be called from system UI
            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.isEnabled();
        }

        @Override
        public int getState() {
            // don't check caller, may be called from system UI
            AdapterService service = getService();
            if (service == null) {
                return BluetoothAdapter.STATE_OFF;
            }
            return service.getState();
        }

        @Override
        public boolean enable() {
            if ((Binder.getCallingUid() != Process.SYSTEM_UID) && (!Utils.checkCaller())) {
                Log.w(TAG, "enable() - Not allowed for non-active user and non system user");
                return false;
            }
            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.enable();
        }

        @Override
        public boolean enableNoAutoConnect() {
            if ((Binder.getCallingUid() != Process.SYSTEM_UID) && (!Utils.checkCaller())) {
                Log.w(TAG, "enableNoAuto() - Not allowed for non-active user and non system user");
                return false;
            }

            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.enableNoAutoConnect();
        }

        @Override
        public boolean disable() {
            if ((Binder.getCallingUid() != Process.SYSTEM_UID) && (!Utils.checkCaller())) {
                Log.w(TAG, "disable() - Not allowed for non-active user and non system user");
                return false;
            }

            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.disable();
        }

        @Override
        public String getAddress() {
            if ((Binder.getCallingUid() != Process.SYSTEM_UID)
                    && (!Utils.checkCallerAllowManagedProfiles(mService))) {
                Log.w(TAG, "getAddress() - Not allowed for non-active user and non system user");
                return null;
            }

            AdapterService service = getService();
            if (service == null) {
                return null;
            }
            return service.getAddress();
        }

        @Override
        public ParcelUuid[] getUuids() {
            if (!Utils.checkCaller()) {
                Log.w(TAG, "getUuids() - Not allowed for non-active user");
                return new ParcelUuid[0];
            }

            AdapterService service = getService();
            if (service == null) {
                return new ParcelUuid[0];
            }
            return service.getUuids();
        }

        @Override
        public String getName() {
            if ((Binder.getCallingUid() != Process.SYSTEM_UID) && (!Utils.checkCaller())) {
                Log.w(TAG, "getName() - Not allowed for non-active user and non system user");
                return null;
            }

            AdapterService service = getService();
            if (service == null) {
                return null;
            }
            return service.getName();
        }

        @Override
        public boolean setName(String name) {
            if (!Utils.checkCaller()) {
                Log.w(TAG, "setName() - Not allowed for non-active user");
                return false;
            }

            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.setName(name);
        }

        @Override
        public BluetoothClass getBluetoothClass() {
            if (!Utils.checkCaller()) {
                Log.w(TAG, "getBluetoothClass() - Not allowed for non-active user");
                return null;
            }

            AdapterService service = getService();
            if (service == null) return null;
            return service.getBluetoothClass();
        }

        @Override
        public boolean setBluetoothClass(BluetoothClass bluetoothClass) {
            if (!Utils.checkCaller()) {
                Log.w(TAG, "setBluetoothClass() - Not allowed for non-active user");
                return false;
            }

            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.setBluetoothClass(bluetoothClass);
        }

        @Override
        public int getIoCapability() {
            if (!Utils.checkCaller()) {
                Log.w(TAG, "setBluetoothClass() - Not allowed for non-active user");
                return BluetoothAdapter.IO_CAPABILITY_UNKNOWN;
            }

            AdapterService service = getService();
            if (service == null) return BluetoothAdapter.IO_CAPABILITY_UNKNOWN;
            return service.getIoCapability();
        }

        @Override
        public boolean setIoCapability(int capability) {
            if (!Utils.checkCaller()) {
                Log.w(TAG, "setBluetoothClass() - Not allowed for non-active user");
                return false;
            }

            AdapterService service = getService();
            if (service == null) return false;
            return service.setIoCapability(capability);
        }

        @Override
        public int getLeIoCapability() {
            if (!Utils.checkCaller()) {
                Log.w(TAG, "setBluetoothClass() - Not allowed for non-active user");
                return BluetoothAdapter.IO_CAPABILITY_UNKNOWN;
            }

            AdapterService service = getService();
            if (service == null) return BluetoothAdapter.IO_CAPABILITY_UNKNOWN;
            return service.getLeIoCapability();
        }

        @Override
        public boolean setLeIoCapability(int capability) {
            if (!Utils.checkCaller()) {
                Log.w(TAG, "setBluetoothClass() - Not allowed for non-active user");
                return false;
            }

            AdapterService service = getService();
            if (service == null) return false;
            return service.setLeIoCapability(capability);
        }

        @Override
        public int getScanMode() {
            if (!Utils.checkCallerAllowManagedProfiles(mService)) {
                Log.w(TAG, "getScanMode() - Not allowed for non-active user");
                return BluetoothAdapter.SCAN_MODE_NONE;
            }

            AdapterService service = getService();
            if (service == null) {
                return BluetoothAdapter.SCAN_MODE_NONE;
            }
            return service.getScanMode();
        }

        @Override
        public boolean setScanMode(int mode, int duration) {
            if (!Utils.checkCaller()) {
                Log.w(TAG, "setScanMode() - Not allowed for non-active user");
                return false;
            }

            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.setScanMode(mode, duration);
        }

        @Override
        public int getDiscoverableTimeout() {
            if (!Utils.checkCaller()) {
                Log.w(TAG, "getDiscoverableTimeout() - Not allowed for non-active user");
                return 0;
            }

            AdapterService service = getService();
            if (service == null) {
                return 0;
            }
            return service.getDiscoverableTimeout();
        }

        @Override
        public boolean setDiscoverableTimeout(int timeout) {
            if (!Utils.checkCaller()) {
                Log.w(TAG, "setDiscoverableTimeout() - Not allowed for non-active user");
                return false;
            }

            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.setDiscoverableTimeout(timeout);
        }

        @Override
        public boolean startDiscovery(String callingPackage) {
            if (!Utils.checkCaller()) {
                Log.w(TAG, "startDiscovery() - Not allowed for non-active user");
                return false;
            }

            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.startDiscovery(callingPackage);
        }

        @Override
        public boolean cancelDiscovery() {
            if (!Utils.checkCaller()) {
                Log.w(TAG, "cancelDiscovery() - Not allowed for non-active user");
                return false;
            }

            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.cancelDiscovery();
        }

        @Override
        public boolean isDiscovering() {
            if (!Utils.checkCallerAllowManagedProfiles(mService)) {
                Log.w(TAG, "isDiscovering() - Not allowed for non-active user");
                return false;
            }

            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.isDiscovering();
        }

        @Override
        public long getDiscoveryEndMillis() {
            if (!Utils.checkCaller()) {
                Log.w(TAG, "getDiscoveryEndMillis() - Not allowed for non-active user");
                return -1;
            }

            AdapterService service = getService();
            if (service == null) {
                return -1;
            }
            return service.getDiscoveryEndMillis();
        }

        @Override
        public BluetoothDevice[] getBondedDevices() {
            // don't check caller, may be called from system UI
            AdapterService service = getService();
            if (service == null) {
                return new BluetoothDevice[0];
            }
            return service.getBondedDevices();
        }

        @Override
        public int getAdapterConnectionState() {
            // don't check caller, may be called from system UI
            AdapterService service = getService();
            if (service == null) {
                return BluetoothAdapter.STATE_DISCONNECTED;
            }
            return service.getAdapterConnectionState();
        }

        @Override
        public int getProfileConnectionState(int profile) {
            if (!Utils.checkCallerAllowManagedProfiles(mService)) {
                Log.w(TAG, "getProfileConnectionState- Not allowed for non-active user");
                return BluetoothProfile.STATE_DISCONNECTED;
            }

            AdapterService service = getService();
            if (service == null) {
                return BluetoothProfile.STATE_DISCONNECTED;
            }
            return service.getProfileConnectionState(profile);
        }

        @Override
        public boolean createBond(BluetoothDevice device, int transport) {
            if (!Utils.checkCallerAllowManagedProfiles(mService)) {
                Log.w(TAG, "createBond() - Not allowed for non-active user");
                return false;
            }

            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.createBond(device, transport, null);
        }

        @Override
        public boolean createBondOutOfBand(BluetoothDevice device, int transport, OobData oobData) {
            if (!Utils.checkCallerAllowManagedProfiles(mService)) {
                Log.w(TAG, "createBondOutOfBand() - Not allowed for non-active user");
                return false;
            }

            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.createBond(device, transport, oobData);
        }

        @Override
        public boolean cancelBondProcess(BluetoothDevice device) {
            if (!Utils.checkCaller()) {
                Log.w(TAG, "cancelBondProcess() - Not allowed for non-active user");
                return false;
            }

            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.cancelBondProcess(device);
        }

        @Override
        public boolean removeBond(BluetoothDevice device) {
            if (!Utils.checkCaller()) {
                Log.w(TAG, "removeBond() - Not allowed for non-active user");
                return false;
            }

            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.removeBond(device);
        }

        @Override
        public int getBondState(BluetoothDevice device) {
            // don't check caller, may be called from system UI
            AdapterService service = getService();
            if (service == null) {
                return BluetoothDevice.BOND_NONE;
            }
            return service.getBondState(device);
        }

        @Override
        public boolean isBondingInitiatedLocally(BluetoothDevice device) {
            // don't check caller, may be called from system UI
            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.isBondingInitiatedLocally(device);
        }

        @Override
        public void setBondingInitiatedLocally(BluetoothDevice device, boolean localInitiated) {
            // don't check caller, may be called from system UI
            AdapterService service = getService();
            if (service == null) {
                return;
            }
            service.setBondingInitiatedLocally(device,localInitiated);
            return;
        }

        @Override
        public long getSupportedProfiles() {
            AdapterService service = getService();
            if (service == null) {
                return 0;
            }
            return service.getSupportedProfiles();
        }

        @Override
        public int getConnectionState(BluetoothDevice device) {
            AdapterService service = getService();
            if (service == null) {
                return 0;
            }
            return service.getConnectionState(device);
        }

        @Override
        public String getRemoteName(BluetoothDevice device) {
            if (!Utils.checkCallerAllowManagedProfiles(mService)) {
                Log.w(TAG, "getRemoteName() - Not allowed for non-active user");
                return null;
            }

            AdapterService service = getService();
            if (service == null) {
                return null;
            }
            return service.getRemoteName(device);
        }

        @Override
        public int getRemoteType(BluetoothDevice device) {
            if (!Utils.checkCallerAllowManagedProfiles(mService)) {
                Log.w(TAG, "getRemoteType() - Not allowed for non-active user");
                return BluetoothDevice.DEVICE_TYPE_UNKNOWN;
            }

            AdapterService service = getService();
            if (service == null) {
                return BluetoothDevice.DEVICE_TYPE_UNKNOWN;
            }
            return service.getRemoteType(device);
        }

        @Override
        public String getRemoteAlias(BluetoothDevice device) {
            if (!Utils.checkCallerAllowManagedProfiles(mService)) {
                Log.w(TAG, "getRemoteAlias() - Not allowed for non-active user");
                return null;
            }

            AdapterService service = getService();
            if (service == null) {
                return null;
            }
            return service.getRemoteAlias(device);
        }

        @Override
        public boolean setRemoteAlias(BluetoothDevice device, String name) {
            if (!Utils.checkCaller()) {
                Log.w(TAG, "setRemoteAlias() - Not allowed for non-active user");
                return false;
            }

            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.setRemoteAlias(device, name);
        }

        @Override
        public int getRemoteClass(BluetoothDevice device) {
            if (!Utils.checkCallerAllowManagedProfiles(mService)) {
                Log.w(TAG, "getRemoteClass() - Not allowed for non-active user");
                return 0;
            }

            AdapterService service = getService();
            if (service == null) {
                return 0;
            }
            return service.getRemoteClass(device);
        }

        @Override
        public ParcelUuid[] getRemoteUuids(BluetoothDevice device) {
            if (!Utils.checkCallerAllowManagedProfiles(mService)) {
                Log.w(TAG, "getRemoteUuids() - Not allowed for non-active user");
                return new ParcelUuid[0];
            }

            AdapterService service = getService();
            if (service == null) {
                return new ParcelUuid[0];
            }
            return service.getRemoteUuids(device);
        }

        @Override
        public boolean fetchRemoteUuids(BluetoothDevice device) {
            if (!Utils.checkCallerAllowManagedProfiles(mService)) {
                Log.w(TAG, "fetchRemoteUuids() - Not allowed for non-active user");
                return false;
            }

            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.fetchRemoteUuids(device);
        }


        @Override
        public boolean setPin(BluetoothDevice device, boolean accept, int len, byte[] pinCode) {
            if (!Utils.checkCaller()) {
                Log.w(TAG, "setPin() - Not allowed for non-active user");
                return false;
            }

            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.setPin(device, accept, len, pinCode);
        }

        @Override
        public boolean setPasskey(BluetoothDevice device, boolean accept, int len, byte[] passkey) {
            if (!Utils.checkCaller()) {
                Log.w(TAG, "setPasskey() - Not allowed for non-active user");
                return false;
            }

            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.setPasskey(device, accept, len, passkey);
        }

        @Override
        public boolean setPairingConfirmation(BluetoothDevice device, boolean accept) {
            if (!Utils.checkCaller()) {
                Log.w(TAG, "setPairingConfirmation() - Not allowed for non-active user");
                return false;
            }

            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.setPairingConfirmation(device, accept);
        }

        @Override
        public int getPhonebookAccessPermission(BluetoothDevice device) {
            if (!Utils.checkCaller()) {
                Log.w(TAG, "getPhonebookAccessPermission() - Not allowed for non-active user");
                return BluetoothDevice.ACCESS_UNKNOWN;
            }

            AdapterService service = getService();
            if (service == null) {
                return BluetoothDevice.ACCESS_UNKNOWN;
            }
            return service.getPhonebookAccessPermission(device);
        }

        @Override
        public boolean setSilenceMode(BluetoothDevice device, boolean silence) {
            if (!Utils.checkCaller()) {
                Log.w(TAG, "setSilenceMode() - Not allowed for non-active user");
                return false;
            }

            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.setSilenceMode(device, silence);
        }

        @Override
        public boolean getSilenceMode(BluetoothDevice device) {
            if (!Utils.checkCaller()) {
                Log.w(TAG, "getSilenceMode() - Not allowed for non-active user");
                return false;
            }

            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.getSilenceMode(device);
        }

        @Override
        public boolean setPhonebookAccessPermission(BluetoothDevice device, int value) {
            if (!Utils.checkCaller()) {
                Log.w(TAG, "setPhonebookAccessPermission() - Not allowed for non-active user");
                return false;
            }

            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.setPhonebookAccessPermission(device, value);
        }

        @Override
        public int getMessageAccessPermission(BluetoothDevice device) {
            if (!Utils.checkCaller()) {
                Log.w(TAG, "getMessageAccessPermission() - Not allowed for non-active user");
                return BluetoothDevice.ACCESS_UNKNOWN;
            }

            AdapterService service = getService();
            if (service == null) {
                return BluetoothDevice.ACCESS_UNKNOWN;
            }
            return service.getMessageAccessPermission(device);
        }

        @Override
        public boolean setMessageAccessPermission(BluetoothDevice device, int value) {
            if (!Utils.checkCaller()) {
                Log.w(TAG, "setMessageAccessPermission() - Not allowed for non-active user");
                return false;
            }

            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.setMessageAccessPermission(device, value);
        }

        @Override
        public int getSimAccessPermission(BluetoothDevice device) {
            if (!Utils.checkCaller()) {
                Log.w(TAG, "getSimAccessPermission() - Not allowed for non-active user");
                return BluetoothDevice.ACCESS_UNKNOWN;
            }

            AdapterService service = getService();
            if (service == null) {
                return BluetoothDevice.ACCESS_UNKNOWN;
            }
            return service.getSimAccessPermission(device);
        }

        @Override
        public boolean setSimAccessPermission(BluetoothDevice device, int value) {
            if (!Utils.checkCaller()) {
                Log.w(TAG, "setSimAccessPermission() - Not allowed for non-active user");
                return false;
            }

            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.setSimAccessPermission(device, value);
        }

        @Override
        public void sendConnectionStateChange(BluetoothDevice device, int profile, int state,
                int prevState) {
            AdapterService service = getService();
            if (service == null) {
                return;
            }
            service.sendConnectionStateChange(device, profile, state, prevState);
        }

        @Override
        public IBluetoothSocketManager getSocketManager() {
            AdapterService service = getService();
            if (service == null) {
                return null;
            }
            return service.getSocketManager();
        }

        @Override
        public boolean sdpSearch(BluetoothDevice device, ParcelUuid uuid) {
            if (!Utils.checkCaller()) {
                Log.w(TAG, "sdpSea(): not allowed for non-active user");
                return false;
            }

            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.sdpSearch(device, uuid);
        }

        public boolean isTwsPlusDevice(BluetoothDevice device) {
            if (!Utils.checkCaller()) {
                Log.w(TAG,"(): isTws+device(): not allowed for non-active user");
                return false;
            }

            AdapterService service = getService();
            if (service == null) return false;
            return service.isTwsPlusDevice(device);
        }

        public String getTwsPlusPeerAddress(BluetoothDevice device) {
            if (!Utils.checkCaller()) {
                Log.w(TAG,"getTws+peerAddress(): not allowed for non-active user");
                return null;
            }

            AdapterService service = getService();
            if (service == null) return null;
            return service.getTwsPlusPeerAddress(device);
        }

        @Override
        public int getBatteryLevel(BluetoothDevice device) {
            if (!Utils.checkCaller()) {
                Log.w(TAG, "getBatteryLevel(): not allowed for non-active user");
                return BluetoothDevice.BATTERY_LEVEL_UNKNOWN;
            }

            AdapterService service = getService();
            if (service == null) {
                return BluetoothDevice.BATTERY_LEVEL_UNKNOWN;
            }
            return service.getBatteryLevel(device);
        }

        @Override
        public int getMaxConnectedAudioDevices() {
            // don't check caller, may be called from system UI
            AdapterService service = getService();
            if (service == null) {
                return AdapterProperties.MAX_CONNECTED_AUDIO_DEVICES_LOWER_BOND;
            }
            return service.getMaxConnectedAudioDevices();
        }

        //@Override
        public boolean isA2dpOffloadEnabled() {
            // don't check caller, may be called from system UI
            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.isA2dpOffloadEnabled();
        }

        @Override
        public boolean factoryReset() {
            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            if ((getState() == BluetoothAdapter.STATE_BLE_ON) ||
                (getState() == BluetoothAdapter.STATE_BLE_TURNING_ON)) {
                service.onBrEdrDown();
            } else {
                service.disable();
            }
            return service.factoryReset();
        }

        @Override
        public void registerCallback(IBluetoothCallback cb) {
            AdapterService service = getService();
            if (service == null) {
                return;
            }
            service.registerCallback(cb);
        }

        @Override
        public void unregisterCallback(IBluetoothCallback cb) {
            AdapterService service = getService();
            if (service == null) {
                return;
            }
            service.unregisterCallback(cb);
        }

        @Override
        public boolean isMultiAdvertisementSupported() {
            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.isMultiAdvertisementSupported();
        }

        @Override
        public boolean isOffloadedFilteringSupported() {
            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            int val = service.getNumOfOffloadedScanFilterSupported();
            return (val >= MIN_OFFLOADED_FILTERS);
        }

        @Override
        public boolean isOffloadedScanBatchingSupported() {
            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            int val = service.getOffloadedScanResultStorage();
            return (val >= MIN_OFFLOADED_SCAN_STORAGE_BYTES);
        }

        @Override
        public boolean isLe2MPhySupported() {
            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.isLe2MPhySupported();
        }

        @Override
        public boolean isLeCodedPhySupported() {
            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.isLeCodedPhySupported();
        }

        @Override
        public boolean isLeExtendedAdvertisingSupported() {
            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.isLeExtendedAdvertisingSupported();
        }

        @Override
        public boolean isLePeriodicAdvertisingSupported() {
            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.isLePeriodicAdvertisingSupported();
        }

        @Override
        public int getLeMaximumAdvertisingDataLength() {
            AdapterService service = getService();
            if (service == null) {
                return 0;
            }
            return service.getLeMaximumAdvertisingDataLength();
        }

        @Override
        public boolean isActivityAndEnergyReportingSupported() {
            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.isActivityAndEnergyReportingSupported();
        }

        @Override
        public BluetoothActivityEnergyInfo reportActivityInfo() {
            AdapterService service = getService();
            if (service == null) {
                return null;
            }
            return service.reportActivityInfo();
        }

        @Override
        public boolean registerMetadataListener(IBluetoothMetadataListener listener,
                BluetoothDevice device) {
            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.registerMetadataListener(listener, device);
        }

        @Override
        public boolean unregisterMetadataListener(BluetoothDevice device) {
            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.unregisterMetadataListener(device);
        }

        @Override
        public boolean setMetadata(BluetoothDevice device, int key, byte[] value) {
            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.setMetadata(device, key, value);
        }

        @Override
        public byte[] getMetadata(BluetoothDevice device, int key) {
            AdapterService service = getService();
            if (service == null) {
                return null;
            }
            return service.getMetadata(device, key);
        }

        @Override
        public void requestActivityInfo(ResultReceiver result) {
            Bundle bundle = new Bundle();
            bundle.putParcelable(BatteryStats.RESULT_RECEIVER_CONTROLLER_KEY, reportActivityInfo());
            result.send(0, bundle);
        }

        @Override
        public void onLeServiceUp() {
            AdapterService service = getService();
            if (service == null) {
                return;
            }
            service.onLeServiceUp();
        }

        @Override
        public void updateQuietModeStatus(boolean quietMode) {
            AdapterService service = getService();
            if (service == null) {
                return;
            }
            service.updateQuietModeStatus(quietMode);
        }


        @Override
        public void onBrEdrDown() {
            AdapterService service = getService();
            if (service == null) {
                return;
            }
            service.onBrEdrDown();
        }

        public int setSocketOpt(int type, int channel, int optionName, byte [] optionVal,
                                                    int optionLen) {
            if (!Utils.checkCaller()) {
                Log.w(TAG,"setSocketOpt(): not allowed for non-active user");
                return -1;
            }

            AdapterService service = getService();
            if (service == null) return -1;
            return service.setSocketOpt(type, channel, optionName, optionVal, optionLen);
        }

        public int getSocketOpt(int type, int channel, int optionName, byte [] optionVal) {
            if (!Utils.checkCaller()) {
                Log.w(TAG,"getSocketOpt(): not allowed for non-active user");
                return -1;
            }

            AdapterService service = getService();
            if (service == null) return -1;
            return service.getSocketOpt(type, channel, optionName, optionVal);
        }

        @Override
        public void dump(FileDescriptor fd, String[] args) {
            PrintWriter writer = new PrintWriter(new FileOutputStream(fd));
            AdapterService service = getService();
            if (service == null) {
                return;
            }
            service.dump(fd, writer, args);
            writer.close();
        }
    }

    ;

    // ----API Methods--------

    public boolean isEnabled() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.getState() == BluetoothAdapter.STATE_ON;
    }

    public boolean isVendorIntfEnabled() {
        return mVendorAvailble;
    }

    public int getState() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        if (mAdapterProperties != null) {
            return mAdapterProperties.getState();
        }
        return BluetoothAdapter.STATE_OFF;
    }

    public boolean enable() {
        return enable(false);
    }

    public boolean enableNoAutoConnect() {
        return enable(true);
    }

    public synchronized boolean enable(boolean quietMode) {
        enforceCallingOrSelfPermission(BLUETOOTH_ADMIN_PERM, "Need BLUETOOTH ADMIN permission");

        // Enforce the user restriction for disallowing Bluetooth if it was set.
        if (mUserManager.hasUserRestriction(UserManager.DISALLOW_BLUETOOTH, UserHandle.SYSTEM)) {
            debugLog("enable() called when Bluetooth was disallowed");
            return false;
        }

        debugLog("enable() - Enable called with quiet mode status =  " + quietMode);
        mQuietmode = quietMode;
        mAdapterStateMachine.sendMessage(AdapterState.BLE_TURN_ON);
        return true;
    }

    boolean disable() {
        enforceCallingOrSelfPermission(BLUETOOTH_ADMIN_PERM, "Need BLUETOOTH ADMIN permission");

        debugLog("disable() called with mRunningProfiles.size() = " + mRunningProfiles.size());
        mAdapterStateMachine.sendMessage(AdapterState.USER_TURN_OFF);
        return true;
    }

    String getAddress() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        enforceCallingOrSelfPermission(LOCAL_MAC_ADDRESS_PERM, "Need LOCAL_MAC_ADDRESS permission");

        String addrString = null;
        byte[] address = mAdapterProperties.getAddress();
        return Utils.getAddressStringFromByte(address);
    }

    ParcelUuid[] getUuids() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");

        return mAdapterProperties.getUuids();
    }

    public String getName() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");

        try {
            return mAdapterProperties.getName();
        } catch (Throwable t) {
            debugLog("getName() - Unexpected exception (" + t + ")");
        }
        return null;
    }

    boolean setName(String name) {
        enforceCallingOrSelfPermission(BLUETOOTH_ADMIN_PERM, "Need BLUETOOTH ADMIN permission");

        return mAdapterProperties.setName(name);
    }

    BluetoothClass getBluetoothClass() {
        enforceCallingOrSelfPermission(BLUETOOTH_ADMIN_PERM, "Need BLUETOOTH ADMIN permission");

        return mAdapterProperties.getBluetoothClass();
    }

    /**
     * Sets the Bluetooth CoD on the local adapter and also modifies the storage config for it.
     *
     * <p>Once set, this value persists across reboots.
     */
    boolean setBluetoothClass(BluetoothClass bluetoothClass) {
        enforceCallingOrSelfPermission(BLUETOOTH_PRIVILEGED,
                "Need BLUETOOTH PRIVILEGED permission");
        debugLog("setBluetoothClass() to " + bluetoothClass);
        boolean result = mAdapterProperties.setBluetoothClass(bluetoothClass);
        if (!result) {
            Log.e(TAG, "setBluetoothClass() to " + bluetoothClass + " failed");
        }

        return result && storeBluetoothClassConfig(bluetoothClass.getClassOfDevice());
    }

    private boolean validateInputOutputCapability(int capability) {
        if (capability < 0 || capability >= BluetoothAdapter.IO_CAPABILITY_MAX) {
            Log.e(TAG, "Invalid IO capability value - " + capability);
            return false;
        }

        return true;
    }

    int getIoCapability() {
        enforceCallingOrSelfPermission(BLUETOOTH_ADMIN_PERM, "Need BLUETOOTH ADMIN permission");

        return mAdapterProperties.getIoCapability();
    }

    boolean setIoCapability(int capability) {
        enforceCallingOrSelfPermission(
                BLUETOOTH_PRIVILEGED, "Need BLUETOOTH PRIVILEGED permission");
        if (!validateInputOutputCapability(capability)) {
            return false;
        }

        return mAdapterProperties.setIoCapability(capability);
    }

    int getLeIoCapability() {
        enforceCallingOrSelfPermission(BLUETOOTH_ADMIN_PERM, "Need BLUETOOTH ADMIN permission");

        return mAdapterProperties.getLeIoCapability();
    }

    boolean setLeIoCapability(int capability) {
        enforceCallingOrSelfPermission(
                BLUETOOTH_PRIVILEGED, "Need BLUETOOTH PRIVILEGED permission");
        if (!validateInputOutputCapability(capability)) {
            return false;
        }

        return mAdapterProperties.setLeIoCapability(capability);
    }

    boolean setTwsPlusDevType(byte[] address, short twsPlusDevType) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        BluetoothDevice device = mRemoteDevices.getDevice(address);
        DeviceProperties deviceProp;
        if (device == null) {
            deviceProp = mRemoteDevices.addDeviceProperties(address);
        } else {
            deviceProp = mRemoteDevices.getDeviceProperties(device);
        }
        if(deviceProp != null) {
            deviceProp.setTwsPlusDevType(twsPlusDevType);
            return true;
        }
        return false;
    }

    boolean setTwsPlusPeerEbAddress(byte[] address, byte[] peerEbAddress) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        BluetoothDevice device = mRemoteDevices.getDevice(address);
        BluetoothDevice peerDevice = null;
        DeviceProperties deviceProp;
        DeviceProperties peerDeviceProp;

        if(peerEbAddress != null)
            peerDevice = mRemoteDevices.getDevice(peerEbAddress);

        if (device == null) {
            deviceProp = mRemoteDevices.addDeviceProperties(address);
        } else {
            deviceProp = mRemoteDevices.getDeviceProperties(device);
        }
        if (peerDevice == null && peerEbAddress != null) {
            peerDeviceProp = mRemoteDevices.addDeviceProperties(peerEbAddress);
            peerDevice = mRemoteDevices.getDevice(peerEbAddress);
        }
        if(deviceProp != null) {
            deviceProp.setTwsPlusPeerEbAddress(peerDevice, peerEbAddress);
            return true;
        }
        return false;
    }

    boolean setTwsPlusAutoConnect(byte[] address, boolean autoConnect) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        BluetoothDevice device = mRemoteDevices.getDevice(address);
        BluetoothDevice peerDevice = null;
        DeviceProperties deviceProp;

        if (device == null) {
            deviceProp = mRemoteDevices.addDeviceProperties(address);
        } else {
            deviceProp = mRemoteDevices.getDeviceProperties(device);
        }
        if(deviceProp != null) {
            deviceProp.setTwsPlusAutoConnect(peerDevice, autoConnect);
            return true;
        }
        return false;
    }

    void updateHostFeatureSupport(byte[] val) {
        mAdapterProperties.updateHostFeatureSupport(val);
    }

    void updateSocFeatureSupport(byte[] val) {
        mAdapterProperties.updateSocFeatureSupport(val);
    }

    int getScanMode() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");

        return mAdapterProperties.getScanMode();
    }

    boolean setScanMode(int mode, int duration) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");

        setDiscoverableTimeout(duration);

        int newMode = convertScanModeToHal(mode);
        return mAdapterProperties.setScanMode(newMode);
    }

    int getDiscoverableTimeout() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");

        return mAdapterProperties.getDiscoverableTimeout();
    }

    boolean setDiscoverableTimeout(int timeout) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");

        return mAdapterProperties.setDiscoverableTimeout(timeout);
    }

    ArrayList<DiscoveringPackage> getDiscoveringPackages() {
        return mDiscoveringPackages;
    }

    void clearDiscoveringPackages() {
        synchronized (mDiscoveringPackages) {
            mDiscoveringPackages.clear();
        }
    }

    boolean startDiscovery(String callingPackage) {
        UserHandle callingUser = UserHandle.of(UserHandle.getCallingUserId());
        debugLog("startDiscovery");
        enforceCallingOrSelfPermission(BLUETOOTH_ADMIN_PERM, "Need BLUETOOTH ADMIN permission");
        mAppOps.checkPackage(Binder.getCallingUid(), callingPackage);
        boolean isQApp = Utils.isQApp(this, callingPackage);
        String permission = null;
        if (Utils.checkCallerHasNetworkSettingsPermission(this)) {
            permission = android.Manifest.permission.NETWORK_SETTINGS;
        } else if (Utils.checkCallerHasNetworkSetupWizardPermission(this)) {
            permission = android.Manifest.permission.NETWORK_SETUP_WIZARD;
        } else if (isQApp) {
            if (!Utils.checkCallerHasFineLocation(this, mAppOps, callingPackage, callingUser)) {
                return false;
            }
            permission = android.Manifest.permission.ACCESS_FINE_LOCATION;
        } else {
            if (!Utils.checkCallerHasCoarseLocation(this, mAppOps, callingPackage, callingUser)) {
                return false;
            }
            permission = android.Manifest.permission.ACCESS_COARSE_LOCATION;
        }

        if (mAdapterProperties.isDiscovering()) {
            Log.i(TAG,"discovery already active, ignore startDiscovery");
            return false;
        }
        synchronized (mDiscoveringPackages) {
            mDiscoveringPackages.add(new DiscoveringPackage(callingPackage, permission));
        }
        return startDiscoveryNative();
    }

    boolean cancelDiscovery() {
        debugLog("cancelDiscovery");
        enforceCallingOrSelfPermission(BLUETOOTH_ADMIN_PERM, "Need BLUETOOTH ADMIN permission");

        if (!mAdapterProperties.isDiscovering()) {
            Log.i(TAG,"discovery not active, ignore cancelDiscovery");
            return false;
        }
        return cancelDiscoveryNative();
    }

    boolean isDiscovering() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");

        return mAdapterProperties.isDiscovering();
    }

    long getDiscoveryEndMillis() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");

        return mAdapterProperties.discoveryEndMillis();
    }

    /**
     * Same as API method {@link BluetoothAdapter#getBondedDevices()}
     *
     * @return array of bonded {@link BluetoothDevice} or null on error
     */
    public BluetoothDevice[] getBondedDevices() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.getBondedDevices();
    }

    /**
     * Get the database manager to access Bluetooth storage
     *
     * @return {@link DatabaseManager} or null on error
     */
    @VisibleForTesting
    public DatabaseManager getDatabase() {
        return mDatabaseManager;
    }

    int getAdapterConnectionState() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.getConnectionState();
    }

    int getProfileConnectionState(int profile) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");

        return mAdapterProperties.getProfileConnectionState(profile);
    }

    boolean sdpSearch(BluetoothDevice device, ParcelUuid uuid) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        if (mSdpManager != null) {
            mSdpManager.sdpSearch(device, uuid);
            return true;
        } else {
            return false;
        }
    }

    public boolean isTwsPlusDevice(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        DeviceProperties deviceProp = mRemoteDevices.getDeviceProperties(device);
        if (deviceProp == null) {
            return false;
        }
        return (deviceProp.getTwsPlusPeerAddress() != null);
    }

    public String getTwsPlusPeerAddress(BluetoothDevice device)  {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        DeviceProperties deviceProp = mRemoteDevices.getDeviceProperties(device);
        if (deviceProp == null) {
            return null;
        }
        byte[] address = deviceProp.getTwsPlusPeerAddress();
        return Utils.getAddressStringFromByte(address);
    }

    public BluetoothDevice getTwsPlusPeerDevice(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        DeviceProperties deviceProp = mRemoteDevices.getDeviceProperties(device);
        if (deviceProp == null) {
            return null;
        }
        byte[] address = deviceProp.getTwsPlusPeerAddress();
        return mRemoteDevices.getDevice(address);
    }

    boolean createBond(BluetoothDevice device, int transport, OobData oobData) {
        enforceCallingOrSelfPermission(BLUETOOTH_ADMIN_PERM, "Need BLUETOOTH ADMIN permission");
        DeviceProperties deviceProp = mRemoteDevices.getDeviceProperties(device);
        if (deviceProp != null && deviceProp.getBondState() != BluetoothDevice.BOND_NONE) {
            return false;
        }

        mRemoteDevices.setBondingInitiatedLocally(Utils.getByteAddress(device));

        // Pairing is unreliable while scanning, so cancel discovery
        // Note, remove this when native stack improves
        if (!mAdapterProperties.isDiscovering()) {
            Log.i(TAG,"discovery not active, no need to send cancelDiscovery");
        } else {
            cancelDiscoveryNative();
        }

        Message msg = mBondStateMachine.obtainMessage(BondStateMachine.CREATE_BOND);
        msg.obj = device;
        msg.arg1 = transport;

        if (oobData != null) {
            Bundle oobDataBundle = new Bundle();
            oobDataBundle.putParcelable(BondStateMachine.OOBDATA, oobData);
            msg.setData(oobDataBundle);
        }
        mBondStateMachine.sendMessage(msg);
        return true;
    }

    public boolean isQuietModeEnabled() {
        debugLog("isQuetModeEnabled() - Enabled = " + mQuietmode);
        return mQuietmode;
    }

    public void updateUuids() {
        debugLog("updateUuids() - Updating UUIDs for bonded devices");
        BluetoothDevice[] bondedDevices = getBondedDevices();
        if (bondedDevices == null) {
            return;
        }

        for (BluetoothDevice device : bondedDevices) {
            mRemoteDevices.updateUuids(device);
        }
    }

    /**
     * Update device UUID changed to {@link BondStateMachine}
     *
     * @param device remote device of interest
     */
    public void deviceUuidUpdated(BluetoothDevice device) {
        // Notify BondStateMachine for SDP complete / UUID changed.
        Message msg = mBondStateMachine.obtainMessage(BondStateMachine.UUID_UPDATE);
        msg.obj = device;
        mBondStateMachine.sendMessage(msg);
    }

    boolean cancelBondProcess(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_ADMIN_PERM, "Need BLUETOOTH ADMIN permission");
        byte[] addr = Utils.getBytesFromAddress(device.getAddress());

        DeviceProperties deviceProp = mRemoteDevices.getDeviceProperties(device);
        if (deviceProp != null) {
            deviceProp.setBondingInitiatedLocally(false);
        }

        return cancelBondNative(addr);
    }

    boolean removeBond(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_ADMIN_PERM, "Need BLUETOOTH ADMIN permission");
        DeviceProperties deviceProp = mRemoteDevices.getDeviceProperties(device);
        if (deviceProp == null || deviceProp.getBondState() != BluetoothDevice.BOND_BONDED) {
            return false;
        }
        deviceProp.setBondingInitiatedLocally(false);
        if (device.isTwsPlusDevice()) {
            mActiveDeviceManager.notify_active_device_unbonding(device);
        }
        Message msg = mBondStateMachine.obtainMessage(BondStateMachine.REMOVE_BOND);
        msg.obj = device;
        mBondStateMachine.sendMessage(msg);
        return true;
    }

    /**
     * Get the bond state of a particular {@link BluetoothDevice}
     *
     * @param device remote device of interest
     * @return bond state <p>Possible values are
     * {@link BluetoothDevice#BOND_NONE},
     * {@link BluetoothDevice#BOND_BONDING},
     * {@link BluetoothDevice#BOND_BONDED}.
     */
    @VisibleForTesting
    public int getBondState(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        DeviceProperties deviceProp = mRemoteDevices.getDeviceProperties(device);
        if (deviceProp == null) {
            return BluetoothDevice.BOND_NONE;
        }
        return deviceProp.getBondState();
    }

    boolean isBondingInitiatedLocally(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        DeviceProperties deviceProp = mRemoteDevices.getDeviceProperties(device);
        if (deviceProp == null) {
            return false;
        }
        return deviceProp.isBondingInitiatedLocally();
    }

    void setBondingInitiatedLocally(BluetoothDevice device, boolean localInitiated) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        DeviceProperties deviceProp = mRemoteDevices.getDeviceProperties(device);
        if (deviceProp == null) {
            return;
        }
        Log.w(TAG," localInitiated " + localInitiated);
        deviceProp.setBondingInitiatedLocally(localInitiated);
        return;
    }

    long getSupportedProfiles() {
        return Config.getSupportedProfilesBitMask();
    }

    int getConnectionState(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        byte[] addr = Utils.getBytesFromAddress(device.getAddress());
        return getConnectionStateNative(addr);
    }

    /**
     * Same as API method {@link BluetoothDevice#getName()}
     *
     * @param device remote device of interest
     * @return remote device name
     */
    public String getRemoteName(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        if (device.getAddress().equals(BATService.mBAAddress)) {
            Log.d(TAG," Request Name for BA device ");
            return "Broadcast_Audio";
        }
        if (mRemoteDevices == null) {
            return null;
        }
        DeviceProperties deviceProp = mRemoteDevices.getDeviceProperties(device);
        if (deviceProp == null) {
            return null;
        }
        return deviceProp.getName();
    }

    int getRemoteType(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        DeviceProperties deviceProp = mRemoteDevices.getDeviceProperties(device);
        if (deviceProp == null) {
            return BluetoothDevice.DEVICE_TYPE_UNKNOWN;
        }
        return deviceProp.getDeviceType();
    }

    String getRemoteAlias(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        DeviceProperties deviceProp = mRemoteDevices.getDeviceProperties(device);
        if (deviceProp == null) {
            return null;
        }
        return deviceProp.getAlias();
    }

    boolean setRemoteAlias(BluetoothDevice device, String name) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        DeviceProperties deviceProp = mRemoteDevices.getDeviceProperties(device);
        if (deviceProp == null) {
            return false;
        }
        deviceProp.setAlias(device, name);
        return true;
    }

    int getRemoteClass(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        DeviceProperties deviceProp = mRemoteDevices.getDeviceProperties(device);
        if (deviceProp == null) {
            return 0;
        }

        return deviceProp.getBluetoothClass();
    }

    /**
     * Get UUIDs for service supported by a remote device
     *
     * @param device the remote device that we want to get UUIDs from
     * @return
     */
    @VisibleForTesting
    public ParcelUuid[] getRemoteUuids(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        DeviceProperties deviceProp = mRemoteDevices.getDeviceProperties(device);
        if (deviceProp == null) {
            return null;
        }
        return deviceProp.getUuids();
    }

    boolean fetchRemoteUuids(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        if (device.getAddress().equals(BATService.mBAAddress)) {
            Log.d(TAG," Update from BA, don't check UUIDS, bail out");
            return false;
        }
        mRemoteDevices.fetchUuids(device);
        return true;
    }

    int getBatteryLevel(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        DeviceProperties deviceProp = mRemoteDevices.getDeviceProperties(device);
        if (deviceProp == null) {
            return BluetoothDevice.BATTERY_LEVEL_UNKNOWN;
        }
        return deviceProp.getBatteryLevel();
    }

    boolean setPin(BluetoothDevice device, boolean accept, int len, byte[] pinCode) {
        enforceCallingOrSelfPermission(BLUETOOTH_ADMIN_PERM, "Need BLUETOOTH ADMIN permission");
        DeviceProperties deviceProp = mRemoteDevices.getDeviceProperties(device);
        // Only allow setting a pin in bonding state, or bonded state in case of security upgrade.
        if (deviceProp == null || (deviceProp.getBondState() != BluetoothDevice.BOND_BONDING
                && deviceProp.getBondState() != BluetoothDevice.BOND_BONDED)) {
            return false;
        }

        if (pinCode.length != len) {
            android.util.EventLog.writeEvent(0x534e4554, "139287605", -1,
                    "PIN code length mismatch");
            return false;
        }

        StatsLog.write(StatsLog.BLUETOOTH_BOND_STATE_CHANGED,
                obfuscateAddress(device), 0, device.getType(),
                BluetoothDevice.BOND_BONDING,
                BluetoothProtoEnums.BOND_SUB_STATE_LOCAL_PIN_REPLIED,
                accept ? 0 : BluetoothDevice.UNBOND_REASON_AUTH_REJECTED);
        byte[] addr = Utils.getBytesFromAddress(device.getAddress());
        return pinReplyNative(addr, accept, len, pinCode);
    }

    boolean setPasskey(BluetoothDevice device, boolean accept, int len, byte[] passkey) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        DeviceProperties deviceProp = mRemoteDevices.getDeviceProperties(device);
        if (deviceProp == null || deviceProp.getBondState() != BluetoothDevice.BOND_BONDING) {
            return false;
        }

        if (passkey.length != len) {
            android.util.EventLog.writeEvent(0x534e4554, "139287605", -1,
                    "Passkey length mismatch");
            return false;
        }

        StatsLog.write(StatsLog.BLUETOOTH_BOND_STATE_CHANGED,
                obfuscateAddress(device), 0, device.getType(),
                BluetoothDevice.BOND_BONDING,
                BluetoothProtoEnums.BOND_SUB_STATE_LOCAL_SSP_REPLIED,
                accept ? 0 : BluetoothDevice.UNBOND_REASON_AUTH_REJECTED);
        byte[] addr = Utils.getBytesFromAddress(device.getAddress());
        return sspReplyNative(addr, AbstractionLayer.BT_SSP_VARIANT_PASSKEY_ENTRY, accept,
                Utils.byteArrayToInt(passkey));
    }

    boolean setPairingConfirmation(BluetoothDevice device, boolean accept) {
        enforceCallingOrSelfPermission(BLUETOOTH_PRIVILEGED,
                "Need BLUETOOTH PRIVILEGED permission");
        DeviceProperties deviceProp = mRemoteDevices.getDeviceProperties(device);
        if (deviceProp == null || deviceProp.getBondState() != BluetoothDevice.BOND_BONDING) {
            return false;
        }

        StatsLog.write(StatsLog.BLUETOOTH_BOND_STATE_CHANGED,
                obfuscateAddress(device), 0, device.getType(),
                BluetoothDevice.BOND_BONDING,
                BluetoothProtoEnums.BOND_SUB_STATE_LOCAL_SSP_REPLIED,
                accept ? 0 : BluetoothDevice.UNBOND_REASON_AUTH_REJECTED);
        byte[] addr = Utils.getBytesFromAddress(device.getAddress());
        return sspReplyNative(addr, AbstractionLayer.BT_SSP_VARIANT_PASSKEY_CONFIRMATION, accept,
                0);
    }

    int getPhonebookAccessPermission(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        SharedPreferences pref = getSharedPreferences(PHONEBOOK_ACCESS_PERMISSION_PREFERENCE_FILE,
                Context.MODE_PRIVATE);
        if (!pref.contains(device.getAddress())) {
            return BluetoothDevice.ACCESS_UNKNOWN;
        }
        return pref.getBoolean(device.getAddress(), false) ? BluetoothDevice.ACCESS_ALLOWED
                : BluetoothDevice.ACCESS_REJECTED;
    }

    boolean setSilenceMode(BluetoothDevice device, boolean silence) {
        enforceCallingOrSelfPermission(BLUETOOTH_PRIVILEGED,
                "Need BLUETOOTH PRIVILEGED permission");
        mSilenceDeviceManager.setSilenceMode(device, silence);
        return true;
    }

    boolean getSilenceMode(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_PRIVILEGED,
                "Need BLUETOOTH PRIVILEGED permission");
        return mSilenceDeviceManager.getSilenceMode(device);
    }

    boolean setPhonebookAccessPermission(BluetoothDevice device, int value) {
        enforceCallingOrSelfPermission(BLUETOOTH_PRIVILEGED,
                "Need BLUETOOTH PRIVILEGED permission");
        SharedPreferences pref = getSharedPreferences(PHONEBOOK_ACCESS_PERMISSION_PREFERENCE_FILE,
                Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = pref.edit();
        if (value == BluetoothDevice.ACCESS_UNKNOWN) {
            editor.remove(device.getAddress());
        } else {
            editor.putBoolean(device.getAddress(), value == BluetoothDevice.ACCESS_ALLOWED);
        }
        editor.apply();
        return true;
    }

    int getMessageAccessPermission(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        SharedPreferences pref = getSharedPreferences(MESSAGE_ACCESS_PERMISSION_PREFERENCE_FILE,
                Context.MODE_PRIVATE);
        if (!pref.contains(device.getAddress())) {
            return BluetoothDevice.ACCESS_UNKNOWN;
        }
        return pref.getBoolean(device.getAddress(), false) ? BluetoothDevice.ACCESS_ALLOWED
                : BluetoothDevice.ACCESS_REJECTED;
    }

    boolean setMessageAccessPermission(BluetoothDevice device, int value) {
        enforceCallingOrSelfPermission(BLUETOOTH_PRIVILEGED,
                "Need BLUETOOTH PRIVILEGED permission");
        SharedPreferences pref = getSharedPreferences(MESSAGE_ACCESS_PERMISSION_PREFERENCE_FILE,
                Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = pref.edit();
        if (value == BluetoothDevice.ACCESS_UNKNOWN) {
            editor.remove(device.getAddress());
        } else {
            editor.putBoolean(device.getAddress(), value == BluetoothDevice.ACCESS_ALLOWED);
        }
        editor.apply();
        return true;
    }

    int getSimAccessPermission(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        SharedPreferences pref =
                getSharedPreferences(SIM_ACCESS_PERMISSION_PREFERENCE_FILE, Context.MODE_PRIVATE);
        if (!pref.contains(device.getAddress())) {
            return BluetoothDevice.ACCESS_UNKNOWN;
        }
        return pref.getBoolean(device.getAddress(), false) ? BluetoothDevice.ACCESS_ALLOWED
                : BluetoothDevice.ACCESS_REJECTED;
    }

    boolean setSimAccessPermission(BluetoothDevice device, int value) {
        enforceCallingOrSelfPermission(BLUETOOTH_PRIVILEGED,
                "Need BLUETOOTH PRIVILEGED permission");
        SharedPreferences pref =
                getSharedPreferences(SIM_ACCESS_PERMISSION_PREFERENCE_FILE, Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = pref.edit();
        if (value == BluetoothDevice.ACCESS_UNKNOWN) {
            editor.remove(device.getAddress());
        } else {
            editor.putBoolean(device.getAddress(), value == BluetoothDevice.ACCESS_ALLOWED);
        }
        editor.apply();
        return true;
    }

    void sendConnectionStateChange(BluetoothDevice device, int profile, int state, int prevState) {
        // TODO(BT) permission check?
        // Since this is a binder call check if Bluetooth is on still
        if (getState() == BluetoothAdapter.STATE_OFF) {
            return;
        }

        mAdapterProperties.sendConnectionStateChange(device, profile, state, prevState);

    }

    IBluetoothSocketManager getSocketManager() {
        android.os.IBinder obj = getSocketManagerNative();
        if (obj == null) {
            return null;
        }

        return IBluetoothSocketManager.Stub.asInterface(obj);
    }

    boolean factoryReset() {
        enforceCallingOrSelfPermission(BLUETOOTH_PRIVILEGED, "Need BLUETOOTH permission");
        if (mDatabaseManager != null) {
            mDatabaseManager.factoryReset();
        }
        return factoryResetNative();
    }

    void registerCallback(IBluetoothCallback cb) {
        mCallbacks.register(cb);
    }

    void unregisterCallback(IBluetoothCallback cb) {
        mCallbacks.unregister(cb);
    }

    public int getNumOfAdvertisementInstancesSupported() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.getNumOfAdvertisementInstancesSupported();
    }

    public boolean isMultiAdvertisementSupported() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return getNumOfAdvertisementInstancesSupported() >= MIN_ADVT_INSTANCES_FOR_MA;
    }

    public boolean isRpaOffloadSupported() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.isRpaOffloadSupported();
    }

    public int getNumOfOffloadedIrkSupported() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.getNumOfOffloadedIrkSupported();
    }

    public int getNumOfOffloadedScanFilterSupported() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.getNumOfOffloadedScanFilterSupported();
    }

    public int getOffloadedScanResultStorage() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.getOffloadedScanResultStorage();
    }

    private boolean isActivityAndEnergyReportingSupported() {
        enforceCallingOrSelfPermission(BLUETOOTH_PRIVILEGED, "Need BLUETOOTH permission");
        return mAdapterProperties.isActivityAndEnergyReportingSupported();
    }

    public boolean isLe2MPhySupported() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.isLe2MPhySupported();
    }

    public boolean isLeCodedPhySupported() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.isLeCodedPhySupported();
    }

    public boolean isLeExtendedAdvertisingSupported() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.isLeExtendedAdvertisingSupported();
    }

    public boolean isLePeriodicAdvertisingSupported() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.isLePeriodicAdvertisingSupported();
    }

    public int getLeMaximumAdvertisingDataLength() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.getLeMaximumAdvertisingDataLength();
    }

    /**
     * Get the maximum number of connected audio devices.
     *
     * @return the maximum number of connected audio devices
     */
    public int getMaxConnectedAudioDevices() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.getMaxConnectedAudioDevices();
    }

    /**
     * Check whether A2DP offload is enabled.
     *
     * @return true if A2DP offload is enabled
     */
    public boolean isA2dpOffloadEnabled() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.isA2dpOffloadEnabled();
    }

    /**
     * Check whether Wipower Fastboot enabled.
     *
     * @return true if Wipower fastboot is enabled
     */
    public boolean isWipowerFastbootEnabled() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.isWipowerFastbootEnabled();
    }

    /**
     * Check whether Split A2DP Scramble Data Required
     *
     * @return true if Split A2DP Scramble Data Required is enabled
     */
    public boolean isSplitA2DPScrambleDataRequired() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.isSplitA2DPScrambleDataRequired();
    }

    /**
     * Check whether Split A2DP 44.1Khz Sample Freq enabled.
     *
     * @return true if Split A2DP 44.1Khz Sample Freq is enabled
     */
    public boolean isSplitA2DP44p1KhzSampleFreq() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.isSplitA2DP44p1KhzSampleFreq();
    }

    /**
     * Check whether Split A2DP 48Khz Sample Freq enabled.
     *
     * @return true if  Split A2DP 48Khz Sample Freq is enabled
     */
    public boolean isSplitA2DP48KhzSampleFreq() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.isSplitA2DP48KhzSampleFreq();
    }

    /**
     * Check whether Split A2DP Single VS Command Support enabled.
     *
     * @return true if Split A2DP Single VSCommand Support is enabled
     */
    public boolean isSplitA2DPSingleVSCommandSupport() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.isSplitA2DPSingleVSCommandSupport();
    }

    /**
     * Check whether Split A2DP Source SBC Encoding enabled.
     *
     * @return true if Split A2DP Source SBC Encoding is enabled
     */
    public boolean isSplitA2DPSourceSBCEncoding() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.isSplitA2DPSourceSBCEncoding();
    }

    /**
     * Check whether Split A2DP Source SBC  enabled.
     *
     * @return true if Split A2DP Source SBC  is enabled
     */
    public boolean isSplitA2DPSourceSBC() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.isSplitA2DPSourceSBC();
    }

    /**
     * Check whether Split A2DP Source MP3  enabled.
     *
     * @return true if Split A2DP Source MP3  is enabled
     */
    public boolean isSplitA2DPSourceMP3() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.isSplitA2DPSourceMP3();
    }

    /**
     * Check whether Split A2DP Source AAC  enabled.
     *
     * @return true if Split A2DP Source AAC  is enabled
     */
    public boolean isSplitA2DPSourceAAC() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.isSplitA2DPSourceAAC();
    }

    /**
     * Check whether Split A2DP Source LDAC  enabled.
     *
     * @return true if Split A2DP Source LDAC  is enabled
     */
    public boolean isSplitA2DPSourceLDAC() {
        String BT_SOC = getSocName();
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return (!mAdapterProperties.isAddonFeaturesCmdSupported() && BT_SOC.equals("cherokee")) ||
            mAdapterProperties.isSplitA2DPSourceLDAC();
    }

    /**
     * Check whether Split A2DP Source APTX  enabled.
     *
     * @return true if Split A2DP Source APTX  is enabled
     */
    public boolean isSplitA2DPSourceAPTX() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.isSplitA2DPSourceAPTX();
    }

    /**
     * Check whether Split A2DP Source APTX HD  enabled.
     *
     * @return true if Split A2DP Source APTX HD  is enabled
     */
    public boolean isSplitA2DPSourceAPTXHD() {
        String BT_SOC = getSocName();
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return (!mAdapterProperties.isAddonFeaturesCmdSupported() && BT_SOC.equals("cherokee")) ||
            mAdapterProperties.isSplitA2DPSourceAPTXHD();
    }

    /**
     * Check whether Split A2DP Source APTX ADAPTIVE  enabled.
     *
     * @return true if Split A2DP Source APTX ADAPTIVE  is enabled
     */
    public boolean isSplitA2DPSourceAPTXADAPTIVE() {
        String BT_SOC = getSocName();
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return (!mAdapterProperties.isAddonFeaturesCmdSupported() && BT_SOC.equals("cherokee")) ||
            mAdapterProperties.isSplitA2DPSourceAPTXADAPTIVE();
    }

    /**
     * Check whether Split A2DP Source APTX TWS+  enabled.
     *
     * @return true if Split A2DP Source APTX TWS+  is enabled
     */
    public boolean isSplitA2DPSourceAPTXTWSPLUS() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.isSplitA2DPSourceAPTXTWSPLUS();
    }

    /**
     * Check whether Split A2DP Sink SBC  enabled.
     *
     * @return true if Split A2DP Sink SBC  is enabled
     */
    public boolean isSplitA2DPSinkSBC() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.isSplitA2DPSinkSBC();
    }

    /**
     * Check whether Split A2DP Sink MP3  enabled.
     *
     * @return true if Split A2DP Sink MP3  is enabled
     */
    public boolean isSplitA2DPSinkMP3() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.isSplitA2DPSinkMP3();
    }

    /**
     * Check whether Split A2DP Sink AAC  enabled.
     *
     * @return true if Split A2DP Sink AAC  is enabled
     */
    public boolean isSplitA2DPSinkAAC() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.isSplitA2DPSinkAAC();
    }

    /**
     * Check whether Split A2DP Sink LDAC  enabled.
     *
     * @return true if Split A2DP Sink LDAC  is enabled
     */
    public boolean isSplitA2DPSinkLDAC() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.isSplitA2DPSinkLDAC();
    }

    /**
     * Check whether Split A2DP Sink APTX  enabled.
     *
     * @return true if Split A2DP Sink APTX  is enabled
     */
    public boolean isSplitA2DPSinkAPTX() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.isSplitA2DPSinkAPTX();
    }

    /**
     * Check whether Split A2DP Sink APTX HD  enabled.
     *
     * @return true if Split A2DP Sink APTX HD  is enabled
     */
    public boolean isSplitA2DPSinkAPTXHD() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.isSplitA2DPSinkAPTXHD();
    }

    /**
     * Check whether Split A2DP Sink APTX ADAPTIVE  enabled.
     *
     * @return true if Split A2DP Sink APTX ADAPTIVE  is enabled
     */
    public boolean isSplitA2DPSinkAPTXADAPTIVE() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.isSplitA2DPSinkAPTXADAPTIVE();
    }

    /**
     * Check whether Split A2DP Sink APTX TWS+  enabled.
     *
     * @return true if Split A2DP Sink APTX TWS+  is enabled
     */
    public boolean isSplitA2DPSinkAPTXTWSPLUS() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.isSplitA2DPSinkAPTXTWSPLUS();
    }

    /**
     * Check whether Voice Dual SCO  enabled.
     *
     * @return true if Voice Dual SCO is enabled
     */
    public boolean isVoiceDualSCO() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.isVoiceDualSCO();
    }

    /**
     * Check whether Voice TWS+ eSCO AG enabled.
     *
     * @return true if Voice TWS+ eSCO AG  is enabled
     */
    public boolean isVoiceTWSPLUSeSCOAG() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.isVoiceTWSPLUSeSCOAG();
    }

    /**
     * Check whether SWB Voice with Aptx Adaptive AG  enabled.
     *
     * @return true if SWB Voice with Aptx Adaptive AG  is enabled
     */
    public boolean isSWBVoicewithAptxAdaptiveAG() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.isSWBVoicewithAptxAdaptiveAG();
    }

    /**
     * Check whether Broadcast Audio Tx with EC-2:5 enabled.
     *
     * @return true if Broadcast Audio Tx with EC-2:5 is enabled
     */
    public boolean isBroadcastAudioTxwithEC_2_5() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.isBroadcastAudioTxwithEC_2_5();
    }

    /**
     * Check whether Broadcast Audio Tx with EC_3:9 enabled.
     *
     * @return true if Broadcast Audio Tx with EC_3:9 is enabled
     */
    public boolean isBroadcastAudioTxwithEC_3_9() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.isBroadcastAudioTxwithEC_3_9();
    }

    /**
     * Check whether Broadcast Audio Rx with EC_2:5 enabled.
     *
     * @return true if Broadcast Audio Rx with EC_2:5 is enabled
     */
    public boolean isBroadcastAudioRxwithEC_2_5() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.isBroadcastAudioRxwithEC_2_5();
    }

    /**
     * Check whether Broadcast Audio Rx with EC_3:9 enabled.
     *
     * @return true if Broadcast Audio Rx with EC_3:9 is enabled
     */
    public boolean isBroadcastAudioRxwithEC_3_9() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.isBroadcastAudioRxwithEC_3_9();
    }

    /**
     * Check  AddonFeatures Cmd Support.
     *
     * @return true if AddonFeatures Cmd is Supported
     */
    public boolean isAddonFeaturesCmdSupported() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.isAddonFeaturesCmdSupported();
    }

    private BluetoothActivityEnergyInfo reportActivityInfo() {
        enforceCallingOrSelfPermission(BLUETOOTH_PRIVILEGED, "Need BLUETOOTH permission");
        if (mAdapterProperties.getState() != BluetoothAdapter.STATE_ON
                || !mAdapterProperties.isActivityAndEnergyReportingSupported()) {
            return null;
        }

        // Pull the data. The callback will notify mEnergyInfoLock.
        readEnergyInfo();

        synchronized (mEnergyInfoLock) {
            try {
                mEnergyInfoLock.wait(CONTROLLER_ENERGY_UPDATE_TIMEOUT_MILLIS);
            } catch (InterruptedException e) {
                // Just continue, the energy data may be stale but we won't miss anything next time
                // we query.
            }

            final BluetoothActivityEnergyInfo info =
                    new BluetoothActivityEnergyInfo(SystemClock.elapsedRealtime(),
                            mStackReportedState, mTxTimeTotalMs, mRxTimeTotalMs, mIdleTimeTotalMs,
                            mEnergyUsedTotalVoltAmpSecMicro);

            // Count the number of entries that have byte counts > 0
            int arrayLen = 0;
            for (int i = 0; i < mUidTraffic.size(); i++) {
                final UidTraffic traffic = mUidTraffic.valueAt(i);
                if (traffic.getTxBytes() != 0 || traffic.getRxBytes() != 0) {
                    arrayLen++;
                }
            }

            // Copy the traffic objects whose byte counts are > 0
            final UidTraffic[] result = arrayLen > 0 ? new UidTraffic[arrayLen] : null;
            int putIdx = 0;
            for (int i = 0; i < mUidTraffic.size(); i++) {
                final UidTraffic traffic = mUidTraffic.valueAt(i);
                if (traffic.getTxBytes() != 0 || traffic.getRxBytes() != 0) {
                    result[putIdx++] = traffic.clone();
                }
            }

            info.setUidTraffic(result);

            return info;
        }
    }

    public int getTotalNumOfTrackableAdvertisements() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        return mAdapterProperties.getTotalNumOfTrackableAdvertisements();
    }

    void updateQuietModeStatus(boolean quietMode) {
        debugLog("updateQuietModeStatus()-updateQuietModeStatus called with quiet mode status:"
                   + quietMode);
        mQuietmode = quietMode;
    }

    void onLeServiceUp() {
        mAdapterStateMachine.sendMessage(AdapterState.USER_TURN_ON);
    }

    void onBrEdrDown() {
        mAdapterStateMachine.sendMessage(AdapterState.BLE_TURN_OFF);
    }

    int setSocketOpt(int type, int channel, int optionName, byte [] optionVal,
             int optionLen) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");

        return mVendorSocket.setSocketOpt(type, channel, optionName, optionVal, optionLen);
    }

    int getSocketOpt(int type, int channel, int optionName, byte [] optionVal) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");

        return mVendorSocket.getSocketOpt(type, channel, optionName, optionVal);
    }

    private static int convertScanModeToHal(int mode) {
        switch (mode) {
            case BluetoothAdapter.SCAN_MODE_NONE:
                return AbstractionLayer.BT_SCAN_MODE_NONE;
            case BluetoothAdapter.SCAN_MODE_CONNECTABLE:
                return AbstractionLayer.BT_SCAN_MODE_CONNECTABLE;
            case BluetoothAdapter.SCAN_MODE_CONNECTABLE_DISCOVERABLE:
                return AbstractionLayer.BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE;
        }
        // errorLog("Incorrect scan mode in convertScanModeToHal");
        return -1;
    }

    static int convertScanModeFromHal(int mode) {
        switch (mode) {
            case AbstractionLayer.BT_SCAN_MODE_NONE:
                return BluetoothAdapter.SCAN_MODE_NONE;
            case AbstractionLayer.BT_SCAN_MODE_CONNECTABLE:
                return BluetoothAdapter.SCAN_MODE_CONNECTABLE;
            case AbstractionLayer.BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE:
                return BluetoothAdapter.SCAN_MODE_CONNECTABLE_DISCOVERABLE;
        }
        //errorLog("Incorrect scan mode in convertScanModeFromHal");
        return -1;
    }

    // This function is called from JNI. It allows native code to set a single wake
    // alarm. If an alarm is already pending and a new request comes in, the alarm
    // will be rescheduled (i.e. the previously set alarm will be cancelled).
    private boolean setWakeAlarm(long delayMillis, boolean shouldWake) {
        synchronized (this) {
            if (mPendingAlarm != null) {
                mAlarmManager.cancel(mPendingAlarm);
            }

            long wakeupTime = SystemClock.elapsedRealtime() + delayMillis;
            int type = shouldWake ? AlarmManager.ELAPSED_REALTIME_WAKEUP
                    : AlarmManager.ELAPSED_REALTIME;

            Intent intent = new Intent(ACTION_ALARM_WAKEUP);
            mPendingAlarm =
                    PendingIntent.getBroadcast(this, 0, intent, PendingIntent.FLAG_ONE_SHOT);
            mAlarmManager.setExact(type, wakeupTime, mPendingAlarm);
            return true;
        }
    }

    // This function is called from JNI. It allows native code to acquire a single wake lock.
    // If the wake lock is already held, this function returns success. Although this function
    // only supports acquiring a single wake lock at a time right now, it will eventually be
    // extended to allow acquiring an arbitrary number of wake locks. The current interface
    // takes |lockName| as a parameter in anticipation of that implementation.
    private boolean acquireWakeLock(String lockName) {
        synchronized (this) {
            if (mWakeLock == null) {
                mWakeLockName = lockName;
                mWakeLock = mPowerManager.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, lockName);
            }

            if (!mWakeLock.isHeld()) {
                mWakeLock.acquire();
            }
        }
        return true;
    }

    // This function is called from JNI. It allows native code to release a wake lock acquired
    // by |acquireWakeLock|. If the wake lock is not held, this function returns failure.
    // Note that the release() call is also invoked by {@link #cleanup()} so a synchronization is
    // needed here. See the comment for |acquireWakeLock| for an explanation of the interface.
    private boolean releaseWakeLock(String lockName) {
        synchronized (this) {
            if (mWakeLock == null) {
                errorLog("Repeated wake lock release; aborting release: " + lockName);
                return false;
            }

            if (mWakeLock.isHeld()) {
                mWakeLock.release();
            }
        }
        return true;
    }

    private void energyInfoCallback(int status, int ctrlState, long txTime, long rxTime,
            long idleTime, long energyUsed, UidTraffic[] data) throws RemoteException {
        if (ctrlState >= BluetoothActivityEnergyInfo.BT_STACK_STATE_INVALID
                && ctrlState <= BluetoothActivityEnergyInfo.BT_STACK_STATE_STATE_IDLE) {
            // Energy is product of mA, V and ms. If the chipset doesn't
            // report it, we have to compute it from time
            if (energyUsed == 0) {
                try {
                    final long txMah = Math.multiplyExact(txTime, getTxCurrentMa());
                    final long rxMah = Math.multiplyExact(rxTime, getRxCurrentMa());
                    final long idleMah = Math.multiplyExact(idleTime, getIdleCurrentMa());
                    energyUsed = (long) (Math.addExact(Math.addExact(txMah, rxMah), idleMah)
                            * getOperatingVolt());
                } catch (ArithmeticException e) {
                    Slog.wtf(TAG, "overflow in bluetooth energy callback", e);
                    // Energy is already 0 if the exception was thrown.
                }
            }

            synchronized (mEnergyInfoLock) {
                mStackReportedState = ctrlState;
                long totalTxTimeMs;
                long totalRxTimeMs;
                long totalIdleTimeMs;
                long totalEnergy;
                try {
                    totalTxTimeMs = Math.addExact(mTxTimeTotalMs, txTime);
                    totalRxTimeMs = Math.addExact(mRxTimeTotalMs, rxTime);
                    totalIdleTimeMs = Math.addExact(mIdleTimeTotalMs, idleTime);
                    totalEnergy = Math.addExact(mEnergyUsedTotalVoltAmpSecMicro, energyUsed);
                } catch (ArithmeticException e) {
                    // This could be because we accumulated a lot of time, or we got a very strange
                    // value from the controller (more likely). Discard this data.
                    Slog.wtf(TAG, "overflow in bluetooth energy callback", e);
                    totalTxTimeMs = mTxTimeTotalMs;
                    totalRxTimeMs = mRxTimeTotalMs;
                    totalIdleTimeMs = mIdleTimeTotalMs;
                    totalEnergy = mEnergyUsedTotalVoltAmpSecMicro;
                }

                mTxTimeTotalMs = totalTxTimeMs;
                mRxTimeTotalMs = totalRxTimeMs;
                mIdleTimeTotalMs = totalIdleTimeMs;
                mEnergyUsedTotalVoltAmpSecMicro = totalEnergy;

                for (UidTraffic traffic : data) {
                    UidTraffic existingTraffic = mUidTraffic.get(traffic.getUid());
                    if (existingTraffic == null) {
                        mUidTraffic.put(traffic.getUid(), traffic);
                    } else {
                        existingTraffic.addRxBytes(traffic.getRxBytes());
                        existingTraffic.addTxBytes(traffic.getTxBytes());
                    }
                }
                mEnergyInfoLock.notifyAll();
            }
        }

        verboseLog("energyInfoCallback() status = " + status + "txTime = " + txTime + "rxTime = "
                + rxTime + "idleTime = " + idleTime + "energyUsed = " + energyUsed + "ctrlState = "
                + ctrlState + "traffic = " + Arrays.toString(data));
    }

    boolean registerMetadataListener(IBluetoothMetadataListener listener,
            BluetoothDevice device) {
        if (mMetadataListeners == null) {
            return false;
        }

        ArrayList<IBluetoothMetadataListener> list = mMetadataListeners.get(device);
        if (list == null) {
            list = new ArrayList<>();
        } else if (list.contains(listener)) {
            // The device is already registered with this listener
            return true;
        }
        list.add(listener);
        mMetadataListeners.put(device, list);
        return true;
    }

    boolean unregisterMetadataListener(BluetoothDevice device) {
        if (mMetadataListeners == null) {
            return false;
        }
        if (mMetadataListeners.containsKey(device)) {
            mMetadataListeners.remove(device);
        }
        return true;
    }

    boolean setMetadata(BluetoothDevice device, int key, byte[] value) {
        if (value.length > BluetoothDevice.METADATA_MAX_LENGTH) {
            Log.e(TAG, "setMetadata: value length too long " + value.length);
            return false;
        }
        return mDatabaseManager.setCustomMeta(device, key, value);
    }

    byte[] getMetadata(BluetoothDevice device, int key) {
        return mDatabaseManager.getCustomMeta(device, key);
    }

    /**
     * Update metadata change to registered listeners
     */
    @VisibleForTesting
    public void metadataChanged(String address, int key, byte[] value) {
        BluetoothDevice device = mRemoteDevices.getDevice(Utils.getBytesFromAddress(address));
        if (mMetadataListeners.containsKey(device)) {
            ArrayList<IBluetoothMetadataListener> list = mMetadataListeners.get(device);
            for (IBluetoothMetadataListener listener : list) {
                try {
                    listener.onMetadataChanged(device, key, value);
                } catch (RemoteException e) {
                    Log.w(TAG, "RemoteException when onMetadataChanged");
                }
            }
        }
    }

    private int getIdleCurrentMa() {
        return getResources().getInteger(R.integer.config_bluetooth_idle_cur_ma);
    }

    private int getTxCurrentMa() {
        return getResources().getInteger(R.integer.config_bluetooth_tx_cur_ma);
    }

    private int getRxCurrentMa() {
        return getResources().getInteger(R.integer.config_bluetooth_rx_cur_ma);
    }

    private double getOperatingVolt() {
        return getResources().getInteger(R.integer.config_bluetooth_operating_voltage_mv) / 1000.0;
    }

    @Override
    protected void dump(FileDescriptor fd, PrintWriter writer, String[] args) {
        enforceCallingOrSelfPermission(android.Manifest.permission.DUMP, TAG);

        if (args.length == 0) {
            writer.println("Skipping dump in APP SERVICES, see bluetooth_manager section.");
            writer.println("Use --print argument for dumpsys direct from AdapterService.");
            return;
        }

        verboseLog("dumpsys arguments, check for protobuf output: " + TextUtils.join(" ", args));
        if (args[0].equals("--proto-bin")) {
            dumpMetrics(fd);
            return;
        }

        writer.println();
        mAdapterProperties.dump(fd, writer, args);
        writer.println("mSnoopLogSettingAtEnable = " + mSnoopLogSettingAtEnable);
        writer.println("mDefaultSnoopLogSettingAtEnable = " + mDefaultSnoopLogSettingAtEnable);

        writer.println();
        mAdapterStateMachine.dump(fd, writer, args);

        StringBuilder sb = new StringBuilder();
        for (ProfileService profile : mRegisteredProfiles) {
            profile.dump(sb);
        }
        mSilenceDeviceManager.dump(fd, writer, args);

        writer.write(sb.toString());
        writer.flush();

        dumpNative(fd, args);
    }

    private void dumpMetrics(FileDescriptor fd) {
        BluetoothMetricsProto.BluetoothLog.Builder metricsBuilder =
                BluetoothMetricsProto.BluetoothLog.newBuilder();
        byte[] nativeMetricsBytes = dumpMetricsNative();
        debugLog("dumpMetrics: native metrics size is " + nativeMetricsBytes.length);
        if (nativeMetricsBytes.length > 0) {
            try {
                metricsBuilder.mergeFrom(nativeMetricsBytes);
            } catch (InvalidProtocolBufferException ex) {
                Log.w(TAG, "dumpMetrics: problem parsing metrics protobuf, " + ex.getMessage());
                return;
            }
        }
        metricsBuilder.setNumBondedDevices(getBondedDevices().length);
        MetricsLogger.dumpProto(metricsBuilder);
        for (ProfileService profile : mRegisteredProfiles) {
            profile.dumpProto(metricsBuilder);
        }
        byte[] metricsBytes = Base64.encode(metricsBuilder.build().toByteArray(), Base64.DEFAULT);
        debugLog("dumpMetrics: combined metrics size is " + metricsBytes.length);
        try (FileOutputStream protoOut = new FileOutputStream(fd)) {
            protoOut.write(metricsBytes);
        } catch (IOException e) {
            errorLog("dumpMetrics: error writing combined protobuf to fd, " + e.getMessage());
        }
    }

    private void debugLog(String msg) {
        if (DBG) {
            Log.d(TAG, msg);
        }
    }

    private void verboseLog(String msg) {
        if (VERBOSE) {
            Log.v(TAG, msg);
        }
    }

    private void errorLog(String msg) {
        Log.e(TAG, msg);
    }

    private final BroadcastReceiver mAlarmBroadcastReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            synchronized (AdapterService.this) {
                mPendingAlarm = null;
                alarmFiredNative();
            }
        }
    };

    private final BroadcastReceiver mWifiStateBroadcastReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = (intent != null) ? intent.getAction():null;
            debugLog(action);
            if (action == null) return;
            if (isEnabled() && (action.equals(WifiManager.NETWORK_STATE_CHANGED_ACTION))) {
                fetchWifiState();
             } else if (isEnabled() &&
                        (action.equals(WifiManager.WIFI_STATE_CHANGED_ACTION) ||
                        (action.equals(WifiManager.WIFI_AP_STATE_CHANGED_ACTION)))){
                 if (isPowerbackRequired()) {
                     mVendor.setPowerBackoff(true);
                 } else {
                     mVendor.setPowerBackoff(false);
                 }
             }
         }
    };

    private boolean isGuest() {
        return UserManager.get(this).isGuestUser();
    }

    private boolean isNiapMode() {
        return Settings.Global.getInt(getContentResolver(), "niap_mode", 0) == 1;
    }

    /**
     *  Obfuscate Bluetooth MAC address into a PII free ID string
     *
     *  @param device Bluetooth device whose MAC address will be obfuscated
     *  @return a byte array that is unique to this MAC address on this device,
     *          or empty byte array when either device is null or obfuscateAddressNative fails
     */
    public byte[] obfuscateAddress(BluetoothDevice device) {
         if (device == null) {
           return new byte[0];
         }
         return obfuscateAddressNative(Utils.getByteAddress(device));
    }

    private boolean isPowerbackRequired() {
        try {

            WifiManager mWifiManager = (WifiManager)getSystemService(Context.WIFI_SERVICE);
            final WifiConfiguration config = mWifiManager.getWifiApConfiguration();
            if ((mWifiManager != null) && ((mWifiManager.isWifiEnabled() ||
                ((mWifiManager.getWifiApState() == WifiManager.WIFI_AP_STATE_ENABLED) &&
                (config.apBand == WifiConfiguration.AP_BAND_5GHZ))))) {
                return true;
            }
            return false;
        } catch(SecurityException e) {
            debugLog(e.toString());
        }
        return false;
   }

    static native void classInitNative();

    native boolean initNative(boolean startRestricted, boolean isNiapMode);

    native void cleanupNative();

    /*package*/
    native boolean enableNative();

    /*package*/
    native boolean disableNative();

    /*package*/
    native boolean setAdapterPropertyNative(int type, byte[] val);

    /*package*/
    native boolean getAdapterPropertiesNative();

    /*package*/
    native boolean getAdapterPropertyNative(int type);

    /*package*/
    native boolean setAdapterPropertyNative(int type);

    /*package*/
    native boolean setDevicePropertyNative(byte[] address, int type, byte[] val);

    /*package*/
    native boolean getDevicePropertyNative(byte[] address, int type);

    /*package*/
    native boolean createBondNative(byte[] address, int transport);

    /*package*/
    native boolean createBondOutOfBandNative(byte[] address, int transport, OobData oobData);

    /*package*/
    native boolean removeBondNative(byte[] address);

    /*package*/
    native boolean cancelBondNative(byte[] address);

    /*package*/
    native boolean sdpSearchNative(byte[] address, byte[] uuid);

    /*package*/
    native int getConnectionStateNative(byte[] address);

    private native boolean startDiscoveryNative();

    private native boolean cancelDiscoveryNative();

    private native boolean pinReplyNative(byte[] address, boolean accept, int len, byte[] pin);

    private native boolean sspReplyNative(byte[] address, int type, boolean accept, int passkey);

    /*package*/
    native boolean getRemoteServicesNative(byte[] address);

    /*package*/
    native boolean getRemoteMasInstancesNative(byte[] address);

    private native int readEnergyInfo();

    private native IBinder getSocketManagerNative();

    private native void setSystemUiUidNative(int systemUiUid);

    private static native void setForegroundUserIdNative(int foregroundUserId);

    /*package*/
    native boolean factoryResetNative();

    private native void alarmFiredNative();

    private native void dumpNative(FileDescriptor fd, String[] arguments);

    private native byte[] dumpMetricsNative();

    private native void interopDatabaseClearNative();

    private native void interopDatabaseAddNative(int feature, byte[] address, int length);

    private native byte[] obfuscateAddressNative(byte[] address);

    // Returns if this is a mock object. This is currently used in testing so that we may not call
    // System.exit() while finalizing the object. Otherwise GC of mock objects unfortunately ends up
    // calling finalize() which in turn calls System.exit() and the process crashes.
    //
    // Mock this in your testing framework to return true to avoid the mentioned behavior. In
    // production this has no effect.
    public boolean isMock() {
        return false;
    }
}
