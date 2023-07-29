/*
 * Copyright (C) 2017 The Android Open Source Project
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
import android.hardware.wifi.hostapd.V1_0.HostapdStatus;
import android.hardware.wifi.hostapd.V1_0.HostapdStatusCode;
import android.hardware.wifi.hostapd.V1_0.IHostapd;
import android.hidl.manager.V1_0.IServiceManager;
import android.hidl.manager.V1_0.IServiceNotification;
import android.net.wifi.WifiConfiguration;
import android.os.Handler;
import android.os.HwRemoteBinder;
import android.os.Looper;
import android.os.RemoteException;
import android.util.Log;

import com.android.internal.R;
import com.android.internal.annotations.VisibleForTesting;
import com.android.server.wifi.WifiNative.HostapdDeathEventHandler;
import com.android.server.wifi.util.ApConfigUtil;
import com.android.server.wifi.util.NativeUtil;
import com.android.server.wifi.WifiNative.SoftApListener;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.NoSuchElementException;
import java.util.Random;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import javax.annotation.concurrent.ThreadSafe;

import vendor.qti.hardware.wifi.hostapd.V1_0.IHostapdVendor;
import vendor.qti.hardware.wifi.hostapd.V1_0.IHostapdVendorIfaceCallback;

/**
 * To maintain thread-safety, the locking protocol is that every non-static method (regardless of
 * access level) acquires mLock.
 */
@ThreadSafe
public class HostapdHal {
    private static final String TAG = "HostapdHal";
    @VisibleForTesting
    public static final String HAL_INSTANCE_NAME = "default";
    @VisibleForTesting
    public static final long WAIT_FOR_DEATH_TIMEOUT_MS = 50L;

    private final Object mLock = new Object();
    private boolean mVerboseLoggingEnabled = false;
    private final Handler mEventHandler;
    private final boolean mEnableAcs;
    private final boolean mAcsIncludeDfs;
    private final boolean mEnableIeee80211AC;
    private final List<android.hardware.wifi.hostapd.V1_1.IHostapd.AcsChannelRange>
            mAcsChannelRanges;
    private boolean mForceApChannel = false;
    private int mForcedApChannel;
    private final List<vendor.qti.hardware.wifi.hostapd.V1_1.IHostapdVendor.AcsChannelRange>
            mVendorAcsChannelRanges;
    private String mCountryCode = null;

    // Hostapd HAL interface objects
    private IServiceManager mIServiceManager = null;
    private IHostapd mIHostapd;
    private IHostapdVendor mIHostapdVendor;
    private HashMap<String, WifiNative.SoftApListener> mSoftApListeners = new HashMap<>();
    private HostapdDeathEventHandler mDeathEventHandler;
    private ServiceManagerDeathRecipient mServiceManagerDeathRecipient;
    private HostapdDeathRecipient mHostapdDeathRecipient;
    private HostapdVendorDeathRecipient mHostapdVendorDeathRecipient;
    // Death recipient cookie registered for current supplicant instance.
    private long mDeathRecipientCookie = 0;

    private final IServiceNotification mServiceNotificationCallback =
            new IServiceNotification.Stub() {
        public void onRegistration(String fqName, String name, boolean preexisting) {
            synchronized (mLock) {
                if (mVerboseLoggingEnabled) {
                    Log.i(TAG, "IServiceNotification.onRegistration for: " + fqName
                            + ", " + name + " preexisting=" + preexisting);
                }
                if (!initHostapdService()) {
                    Log.e(TAG, "initalizing IHostapd failed.");
                    hostapdServiceDiedHandler(mDeathRecipientCookie);
                } else {
                    Log.i(TAG, "Completed initialization of IHostapd.");
                }
            }
        }
    };
    private class ServiceManagerDeathRecipient implements HwRemoteBinder.DeathRecipient {
        @Override
        public void serviceDied(long cookie) {
            mEventHandler.post(() -> {
                synchronized (mLock) {
                    Log.w(TAG, "IServiceManager died: cookie=" + cookie);
                    hostapdServiceDiedHandler(mDeathRecipientCookie);
                    mIServiceManager = null; // Will need to register a new ServiceNotification
                }
            });
        }
    }
    private class HostapdDeathRecipient implements HwRemoteBinder.DeathRecipient {
        @Override
        public void serviceDied(long cookie) {
            mEventHandler.post(() -> {
                synchronized (mLock) {
                    Log.w(TAG, "IHostapd/IHostapd died: cookie=" + cookie);
                    hostapdServiceDiedHandler(cookie);
                }
            });
        }
    }

    private class HostapdVendorDeathRecipient implements HwRemoteBinder.DeathRecipient {
        @Override
        public void serviceDied(long cookie) {
            mEventHandler.post(() -> {
                synchronized (mLock) {
                    Log.w(TAG, "IHostapdVendor died: cookie=" + cookie);
                    hostapdServiceDiedHandler(cookie);
                }
            });
        }
    }

    public HostapdHal(Context context, Looper looper) {
        mEventHandler = new Handler(looper);
        mEnableAcs = context.getResources().getBoolean(R.bool.config_wifi_softap_acs_supported);
        mAcsIncludeDfs = context.getResources().getBoolean(R.bool.config_wifi_softap_acs_include_dfs);
        mEnableIeee80211AC =
                context.getResources().getBoolean(R.bool.config_wifi_softap_ieee80211ac_supported);
        mAcsChannelRanges = toAcsChannelRanges(context.getResources().getString(
                R.string.config_wifi_softap_acs_supported_channel_list));

        mServiceManagerDeathRecipient = new ServiceManagerDeathRecipient();
        mHostapdDeathRecipient = new HostapdDeathRecipient();
        mHostapdVendorDeathRecipient = new HostapdVendorDeathRecipient();

        mVendorAcsChannelRanges = toVendorAcsChannelRanges(context.getResources().getString(
                R.string.config_wifi_softap_acs_supported_channel_list));
    }

    /**
     * Enable/Disable verbose logging.
     *
     * @param enable true to enable, false to disable.
     */
    void enableVerboseLogging(boolean enable) {
        synchronized (mLock) {
            mVerboseLoggingEnabled = enable;
            setLogLevel(enable);
        }
    }

    /**
     * Uses the IServiceManager to check if the device is running V1_1 of the HAL from the VINTF for
     * the device.
     * @return true if supported, false otherwise.
     */
    private boolean isV1_1() {
        synchronized (mLock) {
            if (mIServiceManager == null) {
                Log.e(TAG, "isV1_1: called but mServiceManager is null!?");
                return false;
            }
            try {
                return (mIServiceManager.getTransport(
                        android.hardware.wifi.hostapd.V1_1.IHostapd.kInterfaceName,
                        HAL_INSTANCE_NAME)
                        != IServiceManager.Transport.EMPTY);
            } catch (RemoteException e) {
                Log.e(TAG, "Exception while operating on IServiceManager: " + e);
                handleRemoteException(e, "getTransport");
                return false;
            }
        }
    }

    /**
     * Link to death for IServiceManager object.
     * @return true on success, false otherwise.
     */
    private boolean linkToServiceManagerDeath() {
        synchronized (mLock) {
            if (mIServiceManager == null) return false;
            try {
                if (!mIServiceManager.linkToDeath(mServiceManagerDeathRecipient, 0)) {
                    Log.wtf(TAG, "Error on linkToDeath on IServiceManager");
                    hostapdServiceDiedHandler(mDeathRecipientCookie);
                    mIServiceManager = null; // Will need to register a new ServiceNotification
                    return false;
                }
            } catch (RemoteException e) {
                Log.e(TAG, "IServiceManager.linkToDeath exception", e);
                mIServiceManager = null; // Will need to register a new ServiceNotification
                return false;
            }
            return true;
        }
    }

    /**
     * Registers a service notification for the IHostapd service, which triggers intialization of
     * the IHostapd and IHostapdVendor
     * @return true if the service notification was successfully registered
     */
    public boolean initialize() {
        synchronized (mLock) {
            if (mVerboseLoggingEnabled) {
                Log.i(TAG, "Registering IHostapd service ready callback.");
            }
            mIHostapd = null;
            mIHostapdVendor = null;
            if (mIServiceManager != null) {
                // Already have an IServiceManager and serviceNotification registered, don't
                // don't register another.
                return true;
            }
            try {
                mIServiceManager = getServiceManagerMockable();
                if (mIServiceManager == null) {
                    Log.e(TAG, "Failed to get HIDL Service Manager");
                    return false;
                }
                if (!linkToServiceManagerDeath()) {
                    return false;
                }
                /* TODO(b/33639391) : Use the new IHostapd.registerForNotifications() once it
                   exists */
                if (!mIServiceManager.registerForNotifications(
                        IHostapd.kInterfaceName, "default", mServiceNotificationCallback)) {
                    Log.e(TAG, "Failed to register for notifications to "
                            + IHostapd.kInterfaceName);
                    mIServiceManager = null; // Will need to register a new ServiceNotification
                    return false;
                }
            } catch (RemoteException e) {
                Log.e(TAG, "Exception while trying to register a listener for IHostapd service: "
                        + e);
                hostapdServiceDiedHandler(mDeathRecipientCookie);
                mIServiceManager = null; // Will need to register a new ServiceNotification
                return false;
            }
            return true;
        }
    }

    /**
     * Link to death for IHostapd object.
     * @return true on success, false otherwise.
     */
    private boolean linkToHostapdDeath(HwRemoteBinder.DeathRecipient deathRecipient, long cookie) {
        synchronized (mLock) {
            if (mIHostapd == null) return false;
            try {
                if (!mIHostapd.linkToDeath(deathRecipient, cookie)) {
                    Log.wtf(TAG, "Error on linkToDeath on IHostapd");
                    hostapdServiceDiedHandler(mDeathRecipientCookie);
                    return false;
                }
            } catch (RemoteException e) {
                Log.e(TAG, "IHostapd.linkToDeath exception", e);
                return false;
            }
            return true;
        }
    }

    private boolean registerCallback(
            android.hardware.wifi.hostapd.V1_1.IHostapdCallback callback) {
        synchronized (mLock) {
            String methodStr = "registerCallback_1_1";
            try {
                android.hardware.wifi.hostapd.V1_1.IHostapd iHostapdV1_1 = getHostapdMockableV1_1();
                if (iHostapdV1_1 == null) return false;
                HostapdStatus status =  iHostapdV1_1.registerCallback(callback);
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * Initialize the IHostapd object.
     * @return true on success, false otherwise.
     */
    private boolean initHostapdService() {
        synchronized (mLock) {
            try {
                mIHostapd = getHostapdMockable();
            } catch (RemoteException e) {
                Log.e(TAG, "IHostapd.getService exception: " + e);
                return false;
            } catch (NoSuchElementException e) {
                Log.e(TAG, "IHostapd.getService exception: " + e);
                return false;
            }
            if (mIHostapd == null) {
                Log.e(TAG, "Got null IHostapd service. Stopping hostapd HIDL startup");
                return false;
            }
            if (!linkToHostapdDeath(mHostapdDeathRecipient, ++mDeathRecipientCookie)) {
                mIHostapd = null;
                return false;
            }
            // Register for callbacks for 1.1 hostapd.
            if (isV1_1() && !registerCallback(new HostapdCallback())) {
                mIHostapd = null;
                return false;
            }
        }

        if (!initHostapdVendorService())
            Log.e(TAG, "Failed to init HostapdVendor service");

        return true;
    }


    /**
     * Enable force-soft-AP-channel mode which takes effect when soft AP starts next time
     * @param forcedApChannel The forced IEEE channel number
     */
    void enableForceSoftApChannel(int forcedApChannel) {
        mForceApChannel = true;
        mForcedApChannel = forcedApChannel;
    }

    /**
     * Disable force-soft-AP-channel mode which take effect when soft AP starts next time
     */
    void disableForceSoftApChannel() {
        mForceApChannel = false;
    }

    /**
     * Add and start a new access point.
     *
     * @param ifaceName Name of the interface.
     * @param config Configuration to use for the AP.
     * @param listener Callback for AP events.
     * @return true on success, false otherwise.
     */
    public boolean addAccessPoint(@NonNull String ifaceName, @NonNull WifiConfiguration config,
                                  @NonNull WifiNative.SoftApListener listener) {
        synchronized (mLock) {
            final String methodStr = "addAccessPoint";
            IHostapd.IfaceParams ifaceParams = new IHostapd.IfaceParams();
            ifaceParams.ifaceName = ifaceName;
            ifaceParams.hwModeParams.enable80211N = true;
            ifaceParams.hwModeParams.enable80211AC = mEnableIeee80211AC;
            try {
                ifaceParams.channelParams.band = getBand(config);
            } catch (IllegalArgumentException e) {
                Log.e(TAG, "Unrecognized apBand " + config.apBand);
                return false;
            }
            if (mForceApChannel) {
                ifaceParams.channelParams.enableAcs = false;
                ifaceParams.channelParams.channel = mForcedApChannel;
                if (mForcedApChannel <= ApConfigUtil.HIGHEST_2G_AP_CHANNEL) {
                    ifaceParams.channelParams.band = IHostapd.Band.BAND_2_4_GHZ;
                } else {
                    ifaceParams.channelParams.band = IHostapd.Band.BAND_5_GHZ;
                }
            } else if (mEnableAcs) {
                ifaceParams.channelParams.enableAcs = true;
                if(!mAcsIncludeDfs) {
                    ifaceParams.channelParams.acsShouldExcludeDfs = true;
                }
            } else {
                // Downgrade IHostapd.Band.BAND_ANY to IHostapd.Band.BAND_2_4_GHZ if ACS
                // is not supported.
                // We should remove this workaround once channel selection is moved from
                // ApConfigUtil to here.
                if (ifaceParams.channelParams.band == IHostapd.Band.BAND_ANY) {
                    Log.d(TAG, "ACS is not supported on this device, using 2.4 GHz band.");
                    ifaceParams.channelParams.band = IHostapd.Band.BAND_2_4_GHZ;
                }
                ifaceParams.channelParams.enableAcs = false;
                ifaceParams.channelParams.channel = config.apChannel;
            }

            IHostapd.NetworkParams nwParams = new IHostapd.NetworkParams();
            // TODO(b/67745880) Note that config.SSID is intended to be either a
            // hex string or "double quoted".
            // However, it seems that whatever is handing us these configurations does not obey
            // this convention.
            nwParams.ssid.addAll(NativeUtil.stringToByteArrayList(config.SSID));
            nwParams.isHidden = config.hiddenSSID;
            nwParams.encryptionType = getEncryptionType(config);
            nwParams.pskPassphrase = (config.preSharedKey != null) ? config.preSharedKey : "";
            if (!checkHostapdAndLogFailure(methodStr)) return false;
            try {
                HostapdStatus status;
                if (isV1_1()) {
                    android.hardware.wifi.hostapd.V1_1.IHostapd.IfaceParams ifaceParams1_1 =
                            new android.hardware.wifi.hostapd.V1_1.IHostapd.IfaceParams();
                    ifaceParams1_1.V1_0 = ifaceParams;
                    if (mEnableAcs) {
                        ifaceParams1_1.channelParams.acsChannelRanges.addAll(mAcsChannelRanges);
                    }
                    android.hardware.wifi.hostapd.V1_1.IHostapd iHostapdV1_1 =
                            getHostapdMockableV1_1();
                    if (iHostapdV1_1 == null) return false;
                    status = iHostapdV1_1.addAccessPoint_1_1(ifaceParams1_1, nwParams);
                } else {
                    status = mIHostapd.addAccessPoint(ifaceParams, nwParams);
                }
                if (!checkStatusAndLogFailure(status, methodStr)) {
                    return false;
                }
                mSoftApListeners.put(ifaceName, listener);
                return true;
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * Remove a previously started access point.
     *
     * @param ifaceName Name of the interface.
     * @return true on success, false otherwise.
     */
    public boolean removeAccessPoint(@NonNull String ifaceName) {
        synchronized (mLock) {
            final String methodStr = "removeAccessPoint";
            if (!checkHostapdAndLogFailure(methodStr)) return false;
            try {
                HostapdStatus status = mIHostapd.removeAccessPoint(ifaceName);
                if (!checkStatusAndLogFailure(status, methodStr)) {
                    return false;
                }
                mSoftApListeners.remove(ifaceName);
                return true;
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }


    /**
     * Registers a death notification for hostapd.
     * @return Returns true on success.
     */
    public boolean registerDeathHandler(@NonNull HostapdDeathEventHandler handler) {
        if (mDeathEventHandler != null) {
            Log.e(TAG, "Death handler already present");
        }
        mDeathEventHandler = handler;
        return true;
    }

    /**
     * Deregisters a death notification for hostapd.
     * @return Returns true on success.
     */
    public boolean deregisterDeathHandler() {
        if (mDeathEventHandler == null) {
            Log.e(TAG, "No Death handler present");
        }
        mDeathEventHandler = null;
        return true;
    }

    /**
     * Clear internal state.
     */
    private void clearState() {
        synchronized (mLock) {
            mIHostapd = null;
            mIHostapdVendor = null;
        }
    }

    /**
     * Handle hostapd death.
     */
    private void hostapdServiceDiedHandler(long cookie) {
        synchronized (mLock) {
            if (mDeathRecipientCookie != cookie) {
                Log.i(TAG, "Ignoring stale death recipient notification");
                return;
            }
            clearState();
            if (mDeathEventHandler != null) {
                mDeathEventHandler.onDeath();
            }
        }
    }

    /**
     * Signals whether Initialization completed successfully.
     */
    public boolean isInitializationStarted() {
        synchronized (mLock) {
            return mIServiceManager != null;
        }
    }

    /**
     * Signals whether Initialization completed successfully.
     */
    public boolean isInitializationComplete() {
        synchronized (mLock) {
            return mIHostapd != null;
        }
    }

    /**
     * Start the hostapd daemon.
     *
     * @return true on success, false otherwise.
     */
    public boolean startDaemon() {
        synchronized (mLock) {
            try {
                // This should startup hostapd daemon using the lazy start HAL mechanism.
                getHostapdMockable();
            } catch (RemoteException e) {
                Log.e(TAG, "Exception while trying to start hostapd: "
                        + e);
                hostapdServiceDiedHandler(mDeathRecipientCookie);
                return false;
            } catch (NoSuchElementException e) {
                // We're starting the daemon, so expect |NoSuchElementException|.
                Log.d(TAG, "Successfully triggered start of hostapd using HIDL");
            }
            return true;
        }
    }

    /**
     * Terminate the hostapd daemon & wait for it's death.
     */
    public void terminate() {
        synchronized (mLock) {
            // Register for a new death listener to block until hostapd is dead.
            final long waitForDeathCookie = new Random().nextLong();
            final CountDownLatch waitForDeathLatch = new CountDownLatch(1);
            linkToHostapdDeath((cookie) -> {
                Log.d(TAG, "IHostapd died: cookie=" + cookie);
                if (cookie != waitForDeathCookie) return;
                waitForDeathLatch.countDown();
            }, waitForDeathCookie);

            final String methodStr = "terminate";
            if (!checkHostapdAndLogFailure(methodStr)) return;
            try {
                mIHostapd.terminate();
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
            }

            // Now wait for death listener callback to confirm that it's dead.
            try {
                if (!waitForDeathLatch.await(WAIT_FOR_DEATH_TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
                    Log.w(TAG, "Timed out waiting for confirmation of hostapd death");
                }
            } catch (InterruptedException e) {
                Log.w(TAG, "Failed to wait for hostapd death");
            }
        }
    }

    /**
     * Terminate the hostapd daemon if already running, otherwise ignore.
     */
    public void terminateIfRunning() {
        synchronized (mLock) {
            WifiInjector wifiInjector = WifiInjector.getInstance();
            if (wifiInjector.getPropertyService().get("init.svc.hostapd", "").equals("running") ||
                     wifiInjector.getPropertyService().get("init.svc.hostapd_fst", "").equals("running")) {
                try {
                    Log.i(TAG, "Hostapd daemon is running, Terminate gracefully");
                    IHostapd.getService().terminate();
                } catch (RemoteException e) {
                    Log.e(TAG, "IHostapd.getService.terminate exception: " + e);
                } catch (NoSuchElementException e) {
                    // |NoSuchElementException| expected when hostapd is not running
                    // It shouldn't happen as we are calling getService only when hostapd is running
                    Log.e(TAG, "NoSuchElementException on calling IHostapd.getService though hostapd service is running.");
                }
            }
        }
    }

    /**
     * Wrapper functions to access static HAL methods, created to be mockable in unit tests
     */
    @VisibleForTesting
    protected IServiceManager getServiceManagerMockable() throws RemoteException {
        synchronized (mLock) {
            return IServiceManager.getService();
        }
    }

    @VisibleForTesting
    protected IHostapd getHostapdMockable() throws RemoteException {
        synchronized (mLock) {
            return IHostapd.getService();
        }
    }

    @VisibleForTesting
    protected android.hardware.wifi.hostapd.V1_1.IHostapd getHostapdMockableV1_1()
            throws RemoteException {
        synchronized (mLock) {
            try {
                return android.hardware.wifi.hostapd.V1_1.IHostapd.castFrom(mIHostapd);
            } catch (NoSuchElementException e) {
                Log.e(TAG, "Failed to get IHostapd", e);
                return null;
            }
        }
    }

    private static int getEncryptionType(WifiConfiguration localConfig) {
        int encryptionType;
        switch (localConfig.getAuthType()) {
            case WifiConfiguration.KeyMgmt.NONE:
                encryptionType = IHostapd.EncryptionType.NONE;
                break;
            case WifiConfiguration.KeyMgmt.WPA_PSK:
                encryptionType = IHostapd.EncryptionType.WPA;
                break;
            case WifiConfiguration.KeyMgmt.WPA2_PSK:
                encryptionType = IHostapd.EncryptionType.WPA2;
                break;
            default:
                // We really shouldn't default to None, but this was how NetworkManagementService
                // used to do this.
                encryptionType = IHostapd.EncryptionType.NONE;
                break;
        }
        return encryptionType;
    }

    private static int getVendorEncryptionType(WifiConfiguration localConfig) {
        int encryptionType;
        switch (localConfig.getAuthType()) {
            case WifiConfiguration.KeyMgmt.NONE:
                encryptionType = vendor.qti.hardware.wifi.hostapd.V1_1.IHostapdVendor.VendorEncryptionType.NONE;
                break;
            case WifiConfiguration.KeyMgmt.WPA_PSK:
                encryptionType = vendor.qti.hardware.wifi.hostapd.V1_1.IHostapdVendor.VendorEncryptionType.WPA;
                break;
            case WifiConfiguration.KeyMgmt.WPA2_PSK:
                encryptionType = vendor.qti.hardware.wifi.hostapd.V1_1.IHostapdVendor.VendorEncryptionType.WPA2;
                break;
            case WifiConfiguration.KeyMgmt.SAE:
                encryptionType = vendor.qti.hardware.wifi.hostapd.V1_1.IHostapdVendor.VendorEncryptionType.SAE;
                break;
            case WifiConfiguration.KeyMgmt.OWE:
                encryptionType = vendor.qti.hardware.wifi.hostapd.V1_1.IHostapdVendor.VendorEncryptionType.OWE;
                break;
            default:
                // We really shouldn't default to None, but this was how NetworkManagementService
                // used to do this.
                encryptionType = vendor.qti.hardware.wifi.hostapd.V1_1.IHostapdVendor.VendorEncryptionType.NONE;
                break;
        }
        return encryptionType;
    }

    private static int getBand(WifiConfiguration localConfig) {
        int bandType;
        switch (localConfig.apBand) {
            case WifiConfiguration.AP_BAND_2GHZ:
                bandType = IHostapd.Band.BAND_2_4_GHZ;
                break;
            case WifiConfiguration.AP_BAND_5GHZ:
                bandType = IHostapd.Band.BAND_5_GHZ;
                break;
            case WifiConfiguration.AP_BAND_ANY:
                bandType = IHostapd.Band.BAND_ANY;
                break;
            default:
                throw new IllegalArgumentException();
        }
        return bandType;
    }

    /**
     * Convert channel list string like '1-6,11' to list of AcsChannelRanges
     */
    private List<android.hardware.wifi.hostapd.V1_1.IHostapd.AcsChannelRange>
            toAcsChannelRanges(String channelListStr) {
        ArrayList<android.hardware.wifi.hostapd.V1_1.IHostapd.AcsChannelRange> acsChannelRanges =
                new ArrayList<>();
        String[] channelRanges = channelListStr.split(",");
        for (String channelRange : channelRanges) {
            android.hardware.wifi.hostapd.V1_1.IHostapd.AcsChannelRange acsChannelRange =
                    new android.hardware.wifi.hostapd.V1_1.IHostapd.AcsChannelRange();
            try {
                if (channelRange.contains("-")) {
                    String[] channels  = channelRange.split("-");
                    if (channels.length != 2) {
                        Log.e(TAG, "Unrecognized channel range, length is " + channels.length);
                        continue;
                    }
                    int start = Integer.parseInt(channels[0]);
                    int end = Integer.parseInt(channels[1]);
                    if (start > end) {
                        Log.e(TAG, "Invalid channel range, from " + start + " to " + end);
                        continue;
                    }
                    acsChannelRange.start = start;
                    acsChannelRange.end = end;
                } else {
                    acsChannelRange.start = Integer.parseInt(channelRange);
                    acsChannelRange.end = acsChannelRange.start;
                }
            } catch (NumberFormatException e) {
                // Ignore malformed value
                Log.e(TAG, "Malformed channel value detected: " + e);
                continue;
            }
            acsChannelRanges.add(acsChannelRange);
        }
        return acsChannelRanges;
    }

    /**
     * Returns false if Hostapd is null, and logs failure to call methodStr
     */
    private boolean checkHostapdAndLogFailure(String methodStr) {
        synchronized (mLock) {
            if (mIHostapd == null) {
                Log.e(TAG, "Can't call " + methodStr + ", IHostapd is null");
                return false;
            }
            return true;
        }
    }

    /**
     * Returns true if provided status code is SUCCESS, logs debug message and returns false
     * otherwise
     */
    private boolean checkStatusAndLogFailure(HostapdStatus status,
            String methodStr) {
        synchronized (mLock) {
            if (status.code != HostapdStatusCode.SUCCESS) {
                Log.e(TAG, "IHostapd." + methodStr + " failed: " + status.code
                        + ", " + status.debugMessage);
                return false;
            } else {
                if (mVerboseLoggingEnabled) {
                    Log.d(TAG, "IHostapd." + methodStr + " succeeded");
                }
                return true;
            }
        }
    }


    private void handleRemoteException(RemoteException e, String methodStr) {
        synchronized (mLock) {
            hostapdServiceDiedHandler(mDeathRecipientCookie);
            Log.e(TAG, "IHostapd." + methodStr + " failed with exception", e);
        }
    }

    private class HostapdCallback extends
            android.hardware.wifi.hostapd.V1_1.IHostapdCallback.Stub {
        @Override
        public void onFailure(String ifaceName) {
            Log.w(TAG, "Failure on iface " + ifaceName);
            WifiNative.SoftApListener listener = mSoftApListeners.get(ifaceName);
            if (listener != null) {
                listener.onFailure();
            }
        }
    }


    /* ######################### Hostapd Vendor change ###################### */
    // Keep hostapd vendor changes below this line to have minimal conflicts during merge/upgrade

    /**
     * Uses the IServiceManager to check if the device is running V1_1 of the hostapd vendor HAL from
     * the VINTF for the device.
     * @return true if supported, false otherwise.
     */
    private boolean isVendorV1_1() {
        synchronized (mLock) {
            if (mIServiceManager == null) {
                Log.e(TAG, "isVendorV1_1: called but mServiceManager is null!?");
                return false;
            }
            try {
                return (mIServiceManager.getTransport(
                        vendor.qti.hardware.wifi.hostapd.V1_1.IHostapdVendor.kInterfaceName,
                        HAL_INSTANCE_NAME)
                        != IServiceManager.Transport.EMPTY);
            } catch (RemoteException e) {
                Log.e(TAG, "Exception while operating on IServiceManager: " + e);
                handleRemoteException(e, "getTransport");
                return false;
            }
        }
    }

    /**
     * Link to death for IHostapdVendor object.
     * @return true on success, false otherwise.
     */
    private boolean linkToHostapdVendorDeath() {
        synchronized (mLock) {
            if (mIHostapdVendor == null) return false;
            try {
                if (!mIHostapdVendor.linkToDeath(mHostapdVendorDeathRecipient, mDeathRecipientCookie)) {
                    Log.wtf(TAG, "Error on linkToDeath on IHostapdVendor");
                    hostapdServiceDiedHandler(mDeathRecipientCookie);
                    return false;
                }
            } catch (RemoteException e) {
                Log.e(TAG, "IHostapdVendor.linkToDeath exception", e);
                return false;
            }
            return true;
        }
    }

    /**
     * Initialize the IHostapdVendor object.
     * @return true on success, false otherwise.
     */
    public boolean initHostapdVendorService() {
        synchronized (mLock) {
            try {
                mIHostapdVendor = getHostapdVendorMockable();
            } catch (RemoteException e) {
                Log.e(TAG, "IHostapdVendor.getService exception: " + e);
                return false;
            }
            if (mIHostapdVendor == null) {
                Log.e(TAG, "Got null IHostapdVendor service. Stopping hostapdVendor HIDL startup");
                return false;
            }
            if (!linkToHostapdVendorDeath()) {
                mIHostapdVendor = null;
                return false;
            }
        }
        return true;
    }

    /**
     * Add and start a new vendor access point.
     *
     * @param ifaceName Name of the softap interface.
     * @param config Configuration to use for the AP.
     * @param listener Callback for AP events.
     * @return true on success, false otherwise.
     */
    public boolean addVendorAccessPoint(@NonNull String ifaceName, @NonNull WifiConfiguration config, SoftApListener listener) {
        synchronized (mLock) {
            final String methodStr = "addVendorAccessPoint";
            WifiApConfigStore apConfigStore = WifiInjector.getInstance().getWifiApConfigStore();
            IHostapdVendor.VendorIfaceParams vendorIfaceParams = new IHostapdVendor.VendorIfaceParams();
            IHostapd.IfaceParams ifaceParams = vendorIfaceParams.ifaceParams;
            ifaceParams.ifaceName = ifaceName;
            ifaceParams.hwModeParams.enable80211N = true;
            ifaceParams.hwModeParams.enable80211AC = mEnableIeee80211AC;
            vendorIfaceParams.countryCode = (mCountryCode == null) ? "" : mCountryCode;
            vendorIfaceParams.bridgeIfaceName = "";
            try {
                ifaceParams.channelParams.band = getBand(config);
            } catch (IllegalArgumentException e) {
                Log.e(TAG, "Unrecognized apBand " + config.apBand);
                return false;
            }
            if (mEnableAcs) {
                ifaceParams.channelParams.enableAcs = true;
                if(!mAcsIncludeDfs) {
                    ifaceParams.channelParams.acsShouldExcludeDfs = true;
                }
            } else {
                // Downgrade IHostapd.Band.BAND_ANY to IHostapd.Band.BAND_2_4_GHZ if ACS
                // is not supported.
                // We should remove this workaround once channel selection is moved from
                // ApConfigUtil to here.
                if (ifaceParams.channelParams.band == IHostapd.Band.BAND_ANY) {
                    Log.d(TAG, "ACS is not supported on this device, using 2.4 GHz band.");
                    ifaceParams.channelParams.band = IHostapd.Band.BAND_2_4_GHZ;
                }
                ifaceParams.channelParams.enableAcs = false;
                ifaceParams.channelParams.channel = config.apChannel;
            }

            IHostapd.NetworkParams nwParams = new IHostapd.NetworkParams();
            nwParams.ssid.addAll(NativeUtil.stringToByteArrayList(config.SSID));
            nwParams.isHidden = config.hiddenSSID;
            nwParams.encryptionType = getEncryptionType(config);
            nwParams.pskPassphrase = (config.preSharedKey != null) ? config.preSharedKey : "";

            if (!checkHostapdVendorAndLogFailure(methodStr)) return false;
            try {
                if (apConfigStore.getDualSapStatus()) {
                    String bridgeIfaceName = apConfigStore.getBridgeInterface();
                    vendorIfaceParams.bridgeIfaceName = (bridgeIfaceName != null) ? bridgeIfaceName : "";
                }

                if (isVendorV1_1()) {
                    vendor.qti.hardware.wifi.hostapd.V1_1.IHostapdVendor iHostapdVendorV1_1 =
                        getHostapdVendorMockableV1_1();
                    if (iHostapdVendorV1_1 == null) {
                        Log.e(TAG, "Failed to get V1_1.IHostapdVendor");
                        return false;
                    }
                    setLogLevel(mVerboseLoggingEnabled);
                    vendor.qti.hardware.wifi.hostapd.V1_1.IHostapdVendor.VendorIfaceParams
                         vendorIfaceParams1_1 =
                            new vendor.qti.hardware.wifi.hostapd.V1_1.IHostapdVendor.VendorIfaceParams();
                    vendorIfaceParams1_1.VendorV1_0 = vendorIfaceParams;
                    vendorIfaceParams1_1.vendorChannelParams.channelParams = ifaceParams.channelParams;
                    vendorIfaceParams1_1.vendorEncryptionType = getVendorEncryptionType(config);
                    vendorIfaceParams1_1.oweTransIfaceName = (config.oweTransIfaceName != null) ? config.oweTransIfaceName : "";
                    if (mEnableAcs) {
                        vendorIfaceParams1_1.vendorChannelParams.acsChannelRanges.addAll(mVendorAcsChannelRanges);
                    }
                    HostapdStatus status =
                        iHostapdVendorV1_1.addVendorAccessPoint_1_1(vendorIfaceParams1_1, nwParams);
                    if (checkVendorStatusAndLogFailure(status, methodStr)) {
                        HostapdVendorIfaceHalCallbackV1_1 vendorcallback_1_1 =
                            new HostapdVendorIfaceHalCallbackV1_1(ifaceName, listener);
                        if (vendorcallback_1_1 != null) {
                            if (!registerVendorCallback_1_1(ifaceParams.ifaceName, vendorcallback_1_1)) {
                                Log.e(TAG, "Failed to register Hostapd Vendor callback");
                                return false;
                            }
                        } else {
                            Log.e(TAG, "Failed to create vendorcallback instance");
                        }
                        return true;
                    }
                } else {
                    HostapdStatus status = mIHostapdVendor.addVendorAccessPoint(vendorIfaceParams, nwParams);
                    if (checkVendorStatusAndLogFailure(status, methodStr) && (mIHostapdVendor != null)) {
                        IHostapdVendorIfaceCallback vendorcallback = new HostapdVendorIfaceHalCallback(ifaceName, listener);
                        if (vendorcallback != null) {
                            if (!registerVendorCallback(ifaceParams.ifaceName, mIHostapdVendor, vendorcallback)) {
                                Log.e(TAG, "Failed to register Hostapd Vendor callback");
                                return false;
                            }
                        } else {
                            Log.e(TAG, "Failed to create vendorcallback instance");
                        }
                        return true;
                    }
                }
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
            return false;
        }
    }

    /**
     * Remove a previously started access point.
     *
     * @param ifaceName Name of the interface.
     * @return true on success, false otherwise.
     */
    public boolean removeVendorAccessPoint(@NonNull String ifaceName) {
        synchronized (mLock) {
            final String methodStr = "removeVendorAccessPoint";
            WifiApConfigStore apConfigStore = WifiInjector.getInstance().getWifiApConfigStore();

            if (!checkHostapdVendorAndLogFailure(methodStr)) return false;
            try {
                HostapdStatus status = mIHostapdVendor.removeVendorAccessPoint(ifaceName);
                return checkVendorStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }


    /**
     * Set hostapd parameters via QSAP command.
     *
     * This would call QSAP library APIs via hostapd hidl.
     *
     * @param cmd QSAP command.
     * @return true on success, false otherwise.
     */
    public boolean setHostapdParams(@NonNull String cmd) {
        synchronized (mLock) {
            final String methodStr = "setHostapdParams";
            if (!checkHostapdVendorAndLogFailure(methodStr)) return false;
            try {
                HostapdStatus status = mIHostapdVendor.setHostapdParams(cmd);
                return checkVendorStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    @VisibleForTesting
    protected IHostapdVendor getHostapdVendorMockable() throws RemoteException {
        synchronized (mLock) {
            return IHostapdVendor.getService();
        }
    }

    @VisibleForTesting
    protected vendor.qti.hardware.wifi.hostapd.V1_1.IHostapdVendor getHostapdVendorMockableV1_1()
            throws RemoteException {
        synchronized (mLock) {
            try {
                return vendor.qti.hardware.wifi.hostapd.V1_1.IHostapdVendor.castFrom(mIHostapdVendor);
            } catch (NoSuchElementException e) {
                Log.e(TAG, "Failed to get IHostapdVendorV1_1", e);
                return null;
            }
        }
    }

    /**
     * Convert channel list string like '1-6,11' to list of AcsChannelRanges
     */
    private List<vendor.qti.hardware.wifi.hostapd.V1_1.IHostapdVendor.AcsChannelRange>
            toVendorAcsChannelRanges(String channelListStr) {
        ArrayList<vendor.qti.hardware.wifi.hostapd.V1_1.IHostapdVendor.AcsChannelRange> acsChannelRanges =
                new ArrayList<>();
        String[] channelRanges = channelListStr.split(",");
        for (String channelRange : channelRanges) {
            vendor.qti.hardware.wifi.hostapd.V1_1.IHostapdVendor.AcsChannelRange acsChannelRange =
                    new vendor.qti.hardware.wifi.hostapd.V1_1.IHostapdVendor.AcsChannelRange();
            try {
                if (channelRange.contains("-")) {
                    String[] channels  = channelRange.split("-");
                    if (channels.length != 2) {
                        Log.e(TAG, "Unrecognized channel range, length is " + channels.length);
                        continue;
                    }
                    int start = Integer.parseInt(channels[0]);
                    int end = Integer.parseInt(channels[1]);
                    if (start > end) {
                        Log.e(TAG, "Invalid channel range, from " + start + " to " + end);
                        continue;
                    }
                    acsChannelRange.start = start;
                    acsChannelRange.end = end;
                } else {
                    acsChannelRange.start = Integer.parseInt(channelRange);
                    acsChannelRange.end = acsChannelRange.start;
                }
            } catch (NumberFormatException e) {
                // Ignore malformed value
                Log.e(TAG, "Malformed channel value detected: " + e);
                continue;
            }
            acsChannelRanges.add(acsChannelRange);
        }
        return acsChannelRanges;
    }

    /**
     * set country code for vendor hostapd.
     */
    public void setCountryCode(String countryCode) {
        mCountryCode = countryCode;
    }

    /**
     * Check if the device is running hostapd vendor service.
     * @return
     */
    public boolean isVendorHostapdHal() {
        return mIHostapdVendor != null;
    }

    /**
     * Returns false if HostapdVendor is null, and logs failure to call methodStr
     */
    private boolean checkHostapdVendorAndLogFailure(String methodStr) {
        synchronized (mLock) {
            if (mIHostapdVendor == null) {
                Log.e(TAG, "Can't call " + methodStr + ", IHostapdVendor is null");
                return false;
            }
            return true;
        }
    }

    /**
     * Returns true if provided status code is SUCCESS, logs debug message and returns false
     * otherwise
     */
    private boolean checkVendorStatusAndLogFailure(HostapdStatus status,
            String methodStr) {
        synchronized (mLock) {
            if (status.code != HostapdStatusCode.SUCCESS) {
                Log.e(TAG, "IHostapdVendor." + methodStr + " failed: " + status.code
                        + ", " + status.debugMessage);
                return false;
            } else {
                if (mVerboseLoggingEnabled) {
                    Log.e(TAG, "IHostapdVendor." + methodStr + " succeeded");
                }
                return true;
            }
        }
    }

    private class HostapdVendorIfaceHalCallback extends IHostapdVendorIfaceCallback.Stub {
        private SoftApListener mSoftApListener;

        HostapdVendorIfaceHalCallback(@NonNull String ifaceName, SoftApListener listener) {
           mSoftApListener = listener;
        }

        @Override
        public void onStaConnected(byte[/* 6 */] bssid) {
                String bssidStr = NativeUtil.macAddressFromByteArray(bssid);
                mSoftApListener.onStaConnected(bssidStr);
        }

        @Override
        public void onStaDisconnected(byte[/* 6 */] bssid) {
                String bssidStr = NativeUtil.macAddressFromByteArray(bssid);
                mSoftApListener.onStaDisconnected(bssidStr);
        }
    }

    /** See IHostapdVendor.hal for documentation */
    private boolean registerVendorCallback(@NonNull String ifaceName,
            IHostapdVendor service, IHostapdVendorIfaceCallback callback) {
        synchronized (mLock) {
            final String methodStr = "registerVendorCallback";
            if (service == null) return false;
            try {
                HostapdStatus status =  service.registerVendorCallback(ifaceName, callback);
                return checkVendorStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    private class HostapdVendorIfaceHalCallbackV1_1 extends
            vendor.qti.hardware.wifi.hostapd.V1_1.IHostapdVendorIfaceCallback.Stub {
        private SoftApListener mSoftApListener;

        HostapdVendorIfaceHalCallbackV1_1(@NonNull String ifaceName, SoftApListener listener) {
           mSoftApListener = listener;
        }

        @Override
        public void onStaConnected(byte[/* 6 */] bssid) {
                String bssidStr = NativeUtil.macAddressFromByteArray(bssid);
                mSoftApListener.onStaConnected(bssidStr);
        }

        @Override
        public void onStaDisconnected(byte[/* 6 */] bssid) {
                String bssidStr = NativeUtil.macAddressFromByteArray(bssid);
                mSoftApListener.onStaDisconnected(bssidStr);
        }
        @Override
        public void onFailure(String ifaceName) {
            Log.w(TAG, "Failure on iface " + ifaceName);
            mSoftApListener.onFailure();
        }
    }

    private boolean registerVendorCallback_1_1(@NonNull String ifaceName,
            vendor.qti.hardware.wifi.hostapd.V1_1.IHostapdVendorIfaceCallback callback) {
        synchronized (mLock) {
            String methodStr = "registerVendorCallback_1_1";
            try {
                vendor.qti.hardware.wifi.hostapd.V1_1.IHostapdVendor iHostapdVendorV1_1 =
                    getHostapdVendorMockableV1_1();
                if (iHostapdVendorV1_1 == null) return false;
                HostapdStatus status =  iHostapdVendorV1_1.registerVendorCallback_1_1(
                    ifaceName, callback);
                return checkVendorStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * Set the debug log level for hostapd
     *
     * @param turnOnVerbose Whether to turn on verbose logging or not.
     * @return true if request is sent successfully, false otherwise.
     */
    public boolean setLogLevel(boolean turnOnVerbose) {
        synchronized (mLock) {
            if (!isVendorV1_1()) return false;
            int logLevel = turnOnVerbose
                    ? vendor.qti.hardware.wifi.hostapd.V1_1.IHostapdVendor.DebugLevel.DEBUG
                    : vendor.qti.hardware.wifi.hostapd.V1_1.IHostapdVendor.DebugLevel.INFO;
            return setDebugParams(logLevel, false, false);
        }
    }

    /** See IHostapdVendor.hal for documentation */
    private boolean setDebugParams(int level, boolean showTimestamp, boolean showKeys) {
        synchronized (mLock) {
            final String methodStr = "setDebugParams";
            try {
                vendor.qti.hardware.wifi.hostapd.V1_1.IHostapdVendor iHostapdVendorV1_1 =
                    getHostapdVendorMockableV1_1();
                if (iHostapdVendorV1_1 == null) return false;
                HostapdStatus status =  iHostapdVendorV1_1.setDebugParams(level, false, false);
                return checkVendorStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }
}
