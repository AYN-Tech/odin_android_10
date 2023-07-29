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

import static android.net.wifi.WifiManager.WIFI_FEATURE_DPP;
import static android.net.wifi.WifiManager.WIFI_FEATURE_OWE;
import static android.net.wifi.WifiManager.WIFI_FEATURE_WPA3_SAE;
import static android.net.wifi.WifiManager.WIFI_FEATURE_WPA3_SUITE_B;

import static com.android.server.wifi.hotspot2.anqp.Constants.ANQPElementType.ANQP3GPPNetwork;
import static com.android.server.wifi.hotspot2.anqp.Constants.ANQPElementType.ANQPDomName;
import static com.android.server.wifi.hotspot2.anqp.Constants.ANQPElementType.ANQPIPAddrAvailability;
import static com.android.server.wifi.hotspot2.anqp.Constants.ANQPElementType.ANQPNAIRealm;
import static com.android.server.wifi.hotspot2.anqp.Constants.ANQPElementType.ANQPRoamingConsortium;
import static com.android.server.wifi.hotspot2.anqp.Constants.ANQPElementType.ANQPVenueName;
import static com.android.server.wifi.hotspot2.anqp.Constants.ANQPElementType.HSConnCapability;
import static com.android.server.wifi.hotspot2.anqp.Constants.ANQPElementType.HSFriendlyName;
import static com.android.server.wifi.hotspot2.anqp.Constants.ANQPElementType.HSOSUProviders;
import static com.android.server.wifi.hotspot2.anqp.Constants.ANQPElementType.HSWANMetrics;

import static android.net.wifi.WifiDppConfig.DppResult.DPP_EVENT_AUTH_SUCCESS;
import static android.net.wifi.WifiDppConfig.DppResult.DPP_EVENT_CONF;

import android.annotation.NonNull;
import android.content.Context;
import android.hardware.wifi.supplicant.V1_0.ISupplicant;
import vendor.qti.hardware.wifi.supplicant.V2_0.ISupplicantVendor;
import vendor.qti.hardware.wifi.supplicant.V2_0.ISupplicantVendorIface;
import vendor.qti.hardware.wifi.supplicant.V2_0.ISupplicantVendorStaIface;
import vendor.qti.hardware.wifi.supplicant.V2_0.ISupplicantVendorNetwork;
import vendor.qti.hardware.wifi.supplicant.V2_0.ISupplicantVendorStaNetwork;
import vendor.qti.hardware.wifi.supplicant.V2_0.ISupplicantVendorStaIfaceCallback;
import vendor.qti.hardware.wifi.supplicant.V2_1.WifiGenerationStatus;
import android.hardware.wifi.supplicant.V1_0.ISupplicantIface;
import android.hardware.wifi.supplicant.V1_0.ISupplicantNetwork;
import android.hardware.wifi.supplicant.V1_0.ISupplicantStaIface;
import android.hardware.wifi.supplicant.V1_0.ISupplicantStaIfaceCallback;
import android.hardware.wifi.supplicant.V1_0.ISupplicantStaNetwork;
import android.hardware.wifi.supplicant.V1_0.IfaceType;
import android.hardware.wifi.supplicant.V1_0.SupplicantStatus;
import android.hardware.wifi.supplicant.V1_0.SupplicantStatusCode;
import android.hardware.wifi.supplicant.V1_0.WpsConfigMethods;
import android.hardware.wifi.supplicant.V1_2.DppAkm;
import android.hardware.wifi.supplicant.V1_2.DppFailureCode;
import android.hidl.manager.V1_0.IServiceManager;
import android.hidl.manager.V1_0.IServiceNotification;
import android.net.IpConfiguration;
import android.net.wifi.SupplicantState;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiSsid;
import android.os.Handler;
import android.os.HidlSupport.Mutable;
import android.net.wifi.WifiDppConfig;
import android.net.wifi.WifiDppConfig.DppResult;
import android.os.HwRemoteBinder;
import android.os.Looper;
import android.os.Process;
import android.os.RemoteException;
import android.text.TextUtils;
import android.util.Log;
import android.util.MutableBoolean;
import android.util.MutableInt;
import android.util.Pair;
import android.util.SparseArray;
import android.util.MutableInt;

import com.android.internal.annotations.VisibleForTesting;
import com.android.server.wifi.WifiNative.DppEventCallback;
import com.android.server.wifi.WifiNative.SupplicantDeathEventHandler;
import com.android.server.wifi.hotspot2.AnqpEvent;
import com.android.server.wifi.hotspot2.IconEvent;
import com.android.server.wifi.hotspot2.WnmData;
import com.android.server.wifi.hotspot2.anqp.ANQPElement;
import com.android.server.wifi.hotspot2.anqp.ANQPParser;
import com.android.server.wifi.hotspot2.anqp.Constants;
import com.android.server.wifi.util.NativeUtil;

import java.io.IOException;
import java.nio.BufferUnderflowException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.NoSuchElementException;
import java.util.Objects;
import java.util.Random;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import javax.annotation.concurrent.ThreadSafe;

/**
 * Hal calls for bring up/shut down of the supplicant daemon and for
 * sending requests to the supplicant daemon
 * To maintain thread-safety, the locking protocol is that every non-static method (regardless of
 * access level) acquires mLock.
 */
@ThreadSafe
public class SupplicantStaIfaceHal {
    private static final String TAG = "SupplicantStaIfaceHal";
    @VisibleForTesting
    public static final String HAL_INSTANCE_NAME = "default";
    @VisibleForTesting
    public static final String INIT_START_PROPERTY = "ctl.start";
    @VisibleForTesting
    public static final String INIT_STOP_PROPERTY = "ctl.stop";
    @VisibleForTesting
    public static final String INIT_SERVICE_NAME = "wpa_supplicant";
    @VisibleForTesting
    public static final long WAIT_FOR_DEATH_TIMEOUT_MS = 50L;
    /**
     * Regex pattern for extracting the wps device type bytes.
     * Matches a strings like the following: "<categ>-<OUI>-<subcateg>";
     */
    private static final Pattern WPS_DEVICE_TYPE_PATTERN =
            Pattern.compile("^(\\d{1,2})-([0-9a-fA-F]{8})-(\\d{1,2})$");

    private final Object mLock = new Object();
    private boolean mVerboseLoggingEnabled = false;

    // Supplicant HAL interface objects
    private IServiceManager mIServiceManager = null;
    private ISupplicant mISupplicant;
    private ISupplicantVendor mISupplicantVendor; // Supplicant Vendor HAL interface objects
    private HashMap<String, ISupplicantStaIface> mISupplicantStaIfaces = new HashMap<>();
    private HashMap<String, ISupplicantVendorStaIface> mISupplicantVendorStaIfaces = new HashMap<>();
    private HashMap<String, ISupplicantStaIfaceCallback> mISupplicantStaIfaceCallbacks =
            new HashMap<>();
    private HashMap<String, ISupplicantVendorStaIfaceCallback> mISupplicantVendorStaIfaceCallbacks =
            new HashMap<>();
    private HashMap<String, SupplicantStaNetworkHal> mCurrentNetworkRemoteHandles = new HashMap<>();
    private HashMap<String, WifiConfiguration> mCurrentNetworkLocalConfigs = new HashMap<>();
    private HashMap<String, ArrayList<Pair<SupplicantStaNetworkHal, WifiConfiguration>>>
            mLinkedNetworkLocalAndRemoteConfigs = new HashMap<>();
    private SupplicantDeathEventHandler mDeathEventHandler;
    private ServiceManagerDeathRecipient mServiceManagerDeathRecipient;
    private SupplicantDeathRecipient mSupplicantDeathRecipient;
    // Death recipient cookie registered for current supplicant instance.
    private long mDeathRecipientCookie = 0;
    private final Context mContext;
    private final WifiMonitor mWifiMonitor;
    private final PropertyService mPropertyService;
    private final Handler mEventHandler;
    private DppEventCallback mDppCallback = null;

    private final IServiceNotification mServiceNotificationCallback =
            new IServiceNotification.Stub() {
        public void onRegistration(String fqName, String name, boolean preexisting) {
            synchronized (mLock) {
                if (mVerboseLoggingEnabled) {
                    Log.i(TAG, "IServiceNotification.onRegistration for: " + fqName
                            + ", " + name + " preexisting=" + preexisting);
                }
                if (!initSupplicantService()) {
                    Log.e(TAG, "initalizing ISupplicant failed.");
                    supplicantServiceDiedHandler(mDeathRecipientCookie);
                } else {
                    Log.i(TAG, "Completed initialization of ISupplicant.");
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
                    supplicantServiceDiedHandler(mDeathRecipientCookie);
                    mIServiceManager = null; // Will need to register a new ServiceNotification
                }
            });
        }
    }
    private class SupplicantDeathRecipient implements HwRemoteBinder.DeathRecipient {
        @Override
        public void serviceDied(long cookie) {
            mEventHandler.post(() -> {
                synchronized (mLock) {
                    Log.w(TAG, "ISupplicant died: cookie=" + cookie);
                    supplicantServiceDiedHandler(cookie);
                }
            });
        }
    }

    private final HwRemoteBinder.DeathRecipient mSupplicantVendorDeathRecipient =
            cookie -> {
                synchronized (mLock) {
                    Log.w(TAG, "ISupplicantVendor/ISupplicantVendorStaIface died: cookie=" + cookie);
                    supplicantvendorServiceDiedHandler();
                }
            };

    public SupplicantStaIfaceHal(Context context, WifiMonitor monitor,
                                 PropertyService propertyService, Looper looper) {
        mContext = context;
        mWifiMonitor = monitor;
        mPropertyService = propertyService;
        mEventHandler = new Handler(looper);

        mServiceManagerDeathRecipient = new ServiceManagerDeathRecipient();
        mSupplicantDeathRecipient = new SupplicantDeathRecipient();
    }

    /**
     * Enable/Disable verbose logging.
     *
     * @param enable true to enable, false to disable.
     */
    void enableVerboseLogging(boolean enable) {
        synchronized (mLock) {
            mVerboseLoggingEnabled = enable;
        }
    }

    private boolean linkToServiceManagerDeath() {
        synchronized (mLock) {
            if (mIServiceManager == null) return false;
            try {
                if (!mIServiceManager.linkToDeath(mServiceManagerDeathRecipient, 0)) {
                    Log.wtf(TAG, "Error on linkToDeath on IServiceManager");
                    supplicantServiceDiedHandler(mDeathRecipientCookie);
                    mIServiceManager = null; // Will need to register a new ServiceNotification
                    return false;
                }
            } catch (RemoteException e) {
                Log.e(TAG, "IServiceManager.linkToDeath exception", e);
                return false;
            }
            return true;
        }
    }

    /**
     * Registers a service notification for the ISupplicant service, which triggers initialization
     * of the ISupplicantStaIface
     * @return true if the service notification was successfully registered
     */
    public boolean initialize() {
        synchronized (mLock) {
            if (mVerboseLoggingEnabled) {
                Log.i(TAG, "Registering ISupplicant service ready callback.");
            }
            mISupplicant = null;
            mISupplicantVendor = null;
            mISupplicantStaIfaces.clear();
            mISupplicantVendorStaIfaces.clear();
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
                /* TODO(b/33639391) : Use the new ISupplicant.registerForNotifications() once it
                   exists */
                if (!mIServiceManager.registerForNotifications(
                        ISupplicant.kInterfaceName, "default", mServiceNotificationCallback)) {
                    Log.e(TAG, "Failed to register for notifications to "
                            + ISupplicant.kInterfaceName);
                    mIServiceManager = null; // Will need to register a new ServiceNotification
                    return false;
                }
            } catch (RemoteException e) {
                Log.e(TAG, "Exception while trying to register a listener for ISupplicant service: "
                        + e);
                supplicantServiceDiedHandler(mDeathRecipientCookie);
            }
            return true;
        }
    }

    private boolean linkToSupplicantDeath(
            HwRemoteBinder.DeathRecipient deathRecipient, long cookie) {
        synchronized (mLock) {
            if (mISupplicant == null) return false;
            try {
                if (!mISupplicant.linkToDeath(deathRecipient, cookie)) {
                    Log.wtf(TAG, "Error on linkToDeath on ISupplicant");
                    supplicantServiceDiedHandler(mDeathRecipientCookie);
                    return false;
                }
            } catch (RemoteException e) {
                Log.e(TAG, "ISupplicant.linkToDeath exception", e);
                return false;
            }
            return true;
        }
    }

    private boolean linkToSupplicantVendorDeath() {
        synchronized (mLock) {
            if (mISupplicantVendor == null) return false;
            try {
                if (!mISupplicantVendor.linkToDeath(mSupplicantVendorDeathRecipient, 0)) {
                    Log.wtf(TAG, "Error on linkToDeath on ISupplicantVendor");
                    supplicantvendorServiceDiedHandler();
                    return false;
                }
            } catch (RemoteException e) {
                Log.e(TAG, "ISupplicantVendor.linkToDeath exception", e);
                return false;
            }
            return true;
        }
    }

    private boolean initSupplicantService() {
        synchronized (mLock) {
            try {
                mISupplicant = getSupplicantMockable();
            } catch (RemoteException e) {
                Log.e(TAG, "ISupplicant.getService exception: " + e);
                return false;
            } catch (NoSuchElementException e) {
                Log.e(TAG, "ISupplicant.getService exception: " + e);
                return false;
            }

            if (mISupplicant == null) {
                Log.e(TAG, "Got null ISupplicant service. Stopping supplicant HIDL startup");
                return false;
            }
            if (!linkToSupplicantDeath(mSupplicantDeathRecipient, ++mDeathRecipientCookie)) {
                return false;
            }
        }
        if (!initSupplicantVendorService())
            Log.e(TAG, "Failed to init SupplicantVendor service");
        return true;
    }

    private boolean initSupplicantVendorService() {
        synchronized (mLock) {
            try {
            // Discovering supplicantvendor service
                mISupplicantVendor = getSupplicantVendorMockable();
                if (mISupplicantVendor != null) {
                   Log.e(TAG, "Discover ISupplicantVendor service successfull");
                }
            } catch (RemoteException e) {
                Log.e(TAG, "ISupplicantVendor.getService exception: " + e);
                return false;
            }
            if (mISupplicantVendor == null) {
                Log.e(TAG, "Got null ISupplicantVendor service. Stopping supplicantVendor HIDL startup");
                return false;
            }
            // check mISupplicantVendor service and trigger death service
            if (!linkToSupplicantVendorDeath()) {
                return false;
            }
        }
        return true;
    }

    private boolean linkToSupplicantVendorStaIfaceDeath(ISupplicantVendorStaIface iface) {
        synchronized (mLock) {
            if (iface == null) return false;
            try {
                if (!iface.linkToDeath(mSupplicantVendorDeathRecipient, 0)) {
                    Log.wtf(TAG, "Error on linkToDeath on ISupplicantVendorStaIface");
                    supplicantvendorServiceDiedHandler();
                    return false;
                }
            } catch (RemoteException e) {
                Log.e(TAG, "ISupplicantVendorStaIface.linkToDeath exception", e);
                return false;
            }
            return true;
        }
    }

    private int getCurrentNetworkId(@NonNull String ifaceName) {
        synchronized (mLock) {
            WifiConfiguration currentConfig = getCurrentNetworkLocalConfig(ifaceName);
            if (currentConfig == null) {
                return WifiConfiguration.INVALID_NETWORK_ID;
            }
            return currentConfig.networkId;
        }
    }

    /**
     * Setup a STA interface for the specified iface name.
     *
     * @param ifaceName Name of the interface.
     * @return true on success, false otherwise.
     */
    public boolean setupIface(@NonNull String ifaceName) {
        synchronized (mLock) {
            final String methodStr = "setupIface";
            if (checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr) != null) return false;
            if (!checkSupplicantAndLogFailure(methodStr)) return false;
            ISupplicantIface ifaceHwBinder;

            if (isV1_1()) {
                ifaceHwBinder = addIfaceV1_1(ifaceName);
            } else {
                ifaceHwBinder = getIfaceV1_0(ifaceName);
            }
            if (ifaceHwBinder == null) {
                Log.e(TAG, "setupIface got null iface");
                return false;
            }
            SupplicantStaIfaceHalCallback callback = new SupplicantStaIfaceHalCallback(ifaceName);

            if (isV1_2()) {
                android.hardware.wifi.supplicant.V1_2.ISupplicantStaIface iface =
                        getStaIfaceMockableV1_2(ifaceHwBinder);

                SupplicantStaIfaceHalCallbackV1_1 callbackV11 =
                        new SupplicantStaIfaceHalCallbackV1_1(ifaceName, callback);

                SupplicantStaIfaceHalCallbackV1_2 callbackV12 =
                        new SupplicantStaIfaceHalCallbackV1_2(callbackV11);

                if (!registerCallbackV1_2(iface, callbackV12)) {
                    return false;
                }
                mISupplicantStaIfaces.put(ifaceName, iface);
                mISupplicantStaIfaceCallbacks.put(ifaceName, callbackV11);
            } else if (isV1_1()) {
                android.hardware.wifi.supplicant.V1_1.ISupplicantStaIface iface =
                    getStaIfaceMockableV1_1(ifaceHwBinder);
            SupplicantStaIfaceHalCallbackV1_1 callbackV1_1 =
                    new SupplicantStaIfaceHalCallbackV1_1(ifaceName, callback);

                if (!registerCallbackV1_1(iface, callbackV1_1)) {
                    return false;
                }
                mISupplicantStaIfaces.put(ifaceName, iface);
                mISupplicantStaIfaceCallbacks.put(ifaceName, callbackV1_1);
            } else {
                ISupplicantStaIface iface = getStaIfaceMockable(ifaceHwBinder);

                if (!registerCallback(iface, callback)) {
                    return false;
                }
                mISupplicantStaIfaces.put(ifaceName, iface);
                mISupplicantStaIfaceCallbacks.put(ifaceName, callback);
            }
            /** creation vendor sta iface binder */
            if (!vendor_setupIface(ifaceName, callback))
                Log.e(TAG, "Failed to create vendor setupiface");

            return true;
        }
    }

    /**
     * Setup a Vendor STA interface for the specified iface name.
     *
     * @param ifaceName Name of the interface.
     * @return true on success, false otherwise.
     */
    public boolean vendor_setupIface(@NonNull String ifaceName, SupplicantStaIfaceHalCallback callback) {
        final String methodStr = "vendor_setupIface";
        if (checkSupplicantVendorStaIfaceAndLogFailure(ifaceName, methodStr) != null) {
            Log.e(TAG, "Already created vendor setupinterface");
            return true;
        }
        ISupplicantVendorIface Vendor_ifaceHwBinder = null;

        if (isVendor_2_0()) {
            Log.e(TAG, "Try to get Vendor HIDL@2.0 interface");
            Vendor_ifaceHwBinder = getVendorIfaceV2_0(ifaceName);
        }
        if (Vendor_ifaceHwBinder == null) {
            Log.e(TAG, "Failed to get vendor iface binder");
            return false;
        }

        ISupplicantVendorStaIface vendor_iface = getVendorStaIfaceMockable(Vendor_ifaceHwBinder);
        if (vendor_iface == null) {
            Log.e(TAG, "Failed to get ISupplicantVendorStaIface proxy");
            return false;
        }
        else
            Log.e(TAG, "Successful get Vendor sta interface");

        if (!linkToSupplicantVendorStaIfaceDeath(vendor_iface)) {
            return false;
        }

        if (vendor_iface != null) {
            ISupplicantVendorStaIfaceCallback vendorcallback =
                    new SupplicantVendorStaIfaceHalCallback(ifaceName, callback);
            if (!registerVendorCallback(vendor_iface, vendorcallback)) {
                Log.e(TAG, "Failed to register Vendor callback");
            } else {
                mISupplicantVendorStaIfaces.put(ifaceName, vendor_iface);
                if (vendorcallback != null)
                    mISupplicantVendorStaIfaceCallbacks.put(ifaceName, vendorcallback);
            }
        }
        return true;
    }

    /**
     * Get a Vendor STA interface for the specified iface name.
     *
     * @param ifaceName Name of the interface.
     * @return true on success, false otherwise.
     */
    private ISupplicantVendorIface getVendorIfaceV2_0(@NonNull String ifaceName) {
        synchronized (mLock) {
            /** List all supplicant Ifaces */
            final ArrayList<ISupplicant.IfaceInfo> supplicantIfaces = new ArrayList<>();
            try {
                final String methodStr = "listVendorInterfaces";
                if (!checkSupplicantVendorAndLogFailure(methodStr)) return null;
                mISupplicantVendor.listVendorInterfaces((SupplicantStatus status,
                                             ArrayList<ISupplicant.IfaceInfo> ifaces) -> {
                    if (!checkSupplicantVendorStatusAndLogFailure(status, methodStr)) {
                        return;
                    }
                    supplicantIfaces.addAll(ifaces);
                });
            } catch (RemoteException e) {
                Log.e(TAG, "ISupplicantVendor.listInterfaces exception: " + e);
                supplicantvendorServiceDiedHandler();
                return null;
            }
            if (supplicantIfaces.size() == 0) {
                Log.e(TAG, "Got zero HIDL supplicant vendor ifaces. Stopping supplicant vendor HIDL startup.");
                return null;
            }
            Mutable<ISupplicantVendorIface> supplicantVendorIface = new Mutable<>();
            for (ISupplicant.IfaceInfo ifaceInfo : supplicantIfaces) {
                if (ifaceInfo.type == IfaceType.STA && ifaceName.equals(ifaceInfo.name)) {
                    try {
                        final String methodStr = "getVendorInterface";
                        if (!checkSupplicantVendorAndLogFailure(methodStr)) return null;
                        mISupplicantVendor.getVendorInterface(ifaceInfo,
                                (SupplicantStatus status, ISupplicantVendorIface iface) -> {
                                    if (!checkSupplicantVendorStatusAndLogFailure(status, methodStr)) {
                                        return;
                                    }
                                    supplicantVendorIface.value = iface;
                                });
                    } catch (RemoteException e) {
                        Log.e(TAG, "ISupplicantVendor.getInterface exception: " + e);
                        supplicantvendorServiceDiedHandler();
                        return null;
                    }
                    break;
                }
            }
            return supplicantVendorIface.value;
        }
    }

    /**
     * Get a STA interface for the specified iface name.
     *
     * @param ifaceName Name of the interface.
     * @return true on success, false otherwise.
     */
    private ISupplicantIface getIfaceV1_0(@NonNull String ifaceName) {
        synchronized (mLock) {
            final String methodStr = "getIfaceV1_0";
            if (mISupplicant == null) {
                return null;
            }

            /** List all supplicant Ifaces */
            final ArrayList<ISupplicant.IfaceInfo> supplicantIfaces = new ArrayList<>();
            if (!checkSupplicantAndLogFailure(methodStr)) return null;
            try {
                mISupplicant.listInterfaces((SupplicantStatus status,
                                             ArrayList<ISupplicant.IfaceInfo> ifaces) -> {
                    if (status.code != SupplicantStatusCode.SUCCESS) {
                        Log.e(TAG, "Getting Supplicant Interfaces failed: " + status.code);
                        return;
                    }
                    supplicantIfaces.addAll(ifaces);
                });
            } catch (RemoteException e) {
                Log.e(TAG, "ISupplicant.listInterfaces exception: " + e);
                handleRemoteException(e, "listInterfaces");
                return null;
            }
            if (supplicantIfaces.size() == 0) {
                Log.e(TAG, "Got zero HIDL supplicant ifaces. Stopping supplicant HIDL startup.");
                return null;
            }
            Mutable<ISupplicantIface> supplicantIface = new Mutable<>();
            for (ISupplicant.IfaceInfo ifaceInfo : supplicantIfaces) {
                if (ifaceInfo.type == IfaceType.STA && ifaceName.equals(ifaceInfo.name)) {
                    try {
                        mISupplicant.getInterface(ifaceInfo,
                                (SupplicantStatus status, ISupplicantIface iface) -> {
                                    if (status.code != SupplicantStatusCode.SUCCESS) {
                                        Log.e(TAG, "Failed to get ISupplicantIface " + status.code);
                                        return;
                                    }
                                    supplicantIface.value = iface;
                                });
                    } catch (RemoteException e) {
                        Log.e(TAG, "ISupplicant.getInterface exception: " + e);
                        handleRemoteException(e, "getInterface");
                        return null;
                    }
                    break;
                }
            }
            return supplicantIface.value;
        }
    }

    /**
     * Create a STA interface for the specified iface name.
     *
     * @param ifaceName Name of the interface.
     * @return true on success, false otherwise.
     */
    private ISupplicantIface addIfaceV1_1(@NonNull String ifaceName) {
        synchronized (mLock) {
            ISupplicant.IfaceInfo ifaceInfo = new ISupplicant.IfaceInfo();
            ifaceInfo.name = ifaceName;
            ifaceInfo.type = IfaceType.STA;
            Mutable<ISupplicantIface> supplicantIface = new Mutable<>();
            try {
                android.hardware.wifi.supplicant.V1_1.ISupplicant ISupplicantV1_1 = getSupplicantMockableV1_1();
                if(ISupplicantV1_1 == null) {
                    Log.e(TAG, "Cannot add network, ISupplicant is null");
                    return null;
                }
                ISupplicantV1_1.addInterface(ifaceInfo,
                        (SupplicantStatus status, ISupplicantIface iface) -> {
                            if (status.code != SupplicantStatusCode.SUCCESS
                                    && status.code != SupplicantStatusCode.FAILURE_IFACE_EXISTS) {
                                Log.e(TAG, "Failed to create ISupplicantIface " + status.code);
                                return;
                            }
                            supplicantIface.value = iface;
                        });
            } catch (RemoteException e) {
                Log.e(TAG, "ISupplicant.addInterface exception: " + e);
                handleRemoteException(e, "addInterface");
                return null;
            } catch (NoSuchElementException e) {
                Log.e(TAG, "ISupplicant.addInterface exception: " + e);
                handleNoSuchElementException(e, "addInterface");
                return null;
            }
            return supplicantIface.value;
        }
    }

    /**
     * Teardown a STA interface for the specified iface name.
     *
     * @param ifaceName Name of the interface.
     * @return true on success, false otherwise.
     */
    public boolean teardownIface(@NonNull String ifaceName) {
        synchronized (mLock) {
            final String methodStr = "teardownIface";
            if (checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr) == null) return false;
            if (isV1_1()) {
                if (!removeIfaceV1_1(ifaceName)) {
                    Log.e(TAG, "Failed to remove iface = " + ifaceName);
                    return false;
                }
            }
            if (mISupplicantStaIfaces.remove(ifaceName) == null) {
                Log.e(TAG, "Trying to teardown unknown inteface");
                return false;
            }
            if (mISupplicantVendorStaIfaces.remove(ifaceName) == null) {
                Log.e(TAG, "Trying to teardown unknown vendor interface");
                return false;
            }
            mISupplicantStaIfaceCallbacks.remove(ifaceName);
            mISupplicantVendorStaIfaceCallbacks.remove(ifaceName);
            return true;
        }
    }

    /**
     * Remove a STA interface for the specified iface name.
     *
     * @param ifaceName Name of the interface.
     * @return true on success, false otherwise.
     */
    private boolean removeIfaceV1_1(@NonNull String ifaceName) {
        synchronized (mLock) {
            try {
                ISupplicant.IfaceInfo ifaceInfo = new ISupplicant.IfaceInfo();
                ifaceInfo.name = ifaceName;
                ifaceInfo.type = IfaceType.STA;
                android.hardware.wifi.supplicant.V1_1.ISupplicant ISupplicantV1_1 = getSupplicantMockableV1_1();
                if(ISupplicantV1_1 == null) {
                    Log.e(TAG, "Failed to remove iface, ISupplicant is null");
                    return false;
		}
                SupplicantStatus status = ISupplicantV1_1.removeInterface(ifaceInfo);
                if (status.code != SupplicantStatusCode.SUCCESS) {
                    Log.e(TAG, "Failed to remove iface " + status.code);
                    return false;
                }
            } catch (RemoteException e) {
                Log.e(TAG, "ISupplicant.removeInterface exception: " + e);
                handleRemoteException(e, "removeInterface");
                return false;
            } catch (NoSuchElementException e) {
                Log.e(TAG, "ISupplicant.removeInterface exception: " + e);
                handleNoSuchElementException(e, "removeInterface");
                return false;
            }
            return true;
        }
    }

    /**
     * Registers a death notification for supplicant.
     * @return Returns true on success.
     */
    public boolean registerDeathHandler(@NonNull SupplicantDeathEventHandler handler) {
        synchronized (mLock) {
            if (mDeathEventHandler != null) {
                Log.e(TAG, "Death handler already present");
            }
            mDeathEventHandler = handler;
            return true;
        }
    }

    /**
     * Deregisters a death notification for supplicant.
     * @return Returns true on success.
     */
    public boolean deregisterDeathHandler() {
        synchronized (mLock) {
            if (mDeathEventHandler == null) {
                Log.e(TAG, "No Death handler present");
            }
            mDeathEventHandler = null;
            return true;
        }
    }


    private void clearState() {
        synchronized (mLock) {
            mISupplicant = null;
            mISupplicantVendor = null;
            mISupplicantStaIfaces.clear();
            mISupplicantVendorStaIfaces.clear();
            mCurrentNetworkLocalConfigs.clear();
            mCurrentNetworkRemoteHandles.clear();
            mLinkedNetworkLocalAndRemoteConfigs.clear();
        }
    }

    private void supplicantvendorServiceDiedHandler() {
        synchronized (mLock) {
            mISupplicantVendor = null;
            mISupplicantVendorStaIfaces.clear();
        }
    }

    private void supplicantServiceDiedHandler(long cookie) {
        synchronized (mLock) {
            if (mDeathRecipientCookie != cookie) {
                Log.i(TAG, "Ignoring stale death recipient notification");
                return;
            }
            for (String ifaceName : mISupplicantStaIfaces.keySet()) {
                mWifiMonitor.broadcastSupplicantDisconnectionEvent(ifaceName);
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
            return mISupplicant != null;
        }
    }


    /**
     * Start the supplicant daemon for V1_1 service.
     *
     * @return true on success, false otherwise.
     */
    private boolean startDaemon_V1_1() {
        synchronized (mLock) {
            try {
                // This should startup supplicant daemon using the lazy start HAL mechanism.
                getSupplicantMockableV1_1();
            } catch (RemoteException e) {
                Log.e(TAG, "Exception while trying to start supplicant: "
                        + e);
                supplicantServiceDiedHandler(mDeathRecipientCookie);
                return false;
            } catch (NoSuchElementException e) {
                // We're starting the daemon, so expect |NoSuchElementException|.
                Log.d(TAG, "Successfully triggered start of supplicant using HIDL");
            }
            return true;
        }
    }

    /**
     * Start the supplicant daemon.
     *
     * @return true on success, false otherwise.
     */
    public boolean startDaemon() {
        synchronized (mLock) {
            if (isV1_1()) {
                Log.i(TAG, "Starting supplicant using HIDL");
                return startDaemon_V1_1();
            } else {
                Log.i(TAG, "Starting supplicant using init");
                mPropertyService.set(INIT_START_PROPERTY, INIT_SERVICE_NAME);
                return true;
            }
        }
    }

    /**
     * Terminate the supplicant daemon for V1_1 service.
     */
    private void terminate_V1_1() {
        synchronized (mLock) {
            final String methodStr = "terminate";
            if (!checkSupplicantAndLogFailure(methodStr)) return;
            try {
                android.hardware.wifi.supplicant.V1_1.ISupplicant ISupplicantV1_1 = getSupplicantMockableV1_1();
                if(ISupplicantV1_1 != null)
                    ISupplicantV1_1.terminate();
                else
                    Log.e(TAG, "Cannot terminate ISupplicant. ISupplicant is Null.");
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
            } catch (NoSuchElementException e) {
                handleNoSuchElementException(e, methodStr);
            }
        }
    }

    /**
     * Terminate the supplicant daemon & wait for it's death.
     */
    public void terminate() {
        synchronized (mLock) {
            // Register for a new death listener to block until supplicant is dead.
            final long waitForDeathCookie = new Random().nextLong();
            final CountDownLatch waitForDeathLatch = new CountDownLatch(1);
            linkToSupplicantDeath((cookie) -> {
                Log.d(TAG, "ISupplicant died: cookie=" + cookie);
                if (cookie != waitForDeathCookie) return;
                waitForDeathLatch.countDown();
            }, waitForDeathCookie);

            if (isV1_1()) {
                Log.i(TAG, "Terminating supplicant using HIDL");
                terminate_V1_1();
            } else {
                Log.i(TAG, "Terminating supplicant using init");
                mPropertyService.set(INIT_STOP_PROPERTY, INIT_SERVICE_NAME);
            }

            // Now wait for death listener callback to confirm that it's dead.
            try {
                if (!waitForDeathLatch.await(WAIT_FOR_DEATH_TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
                    Log.w(TAG, "Timed out waiting for confirmation of supplicant death");
                }
            } catch (InterruptedException e) {
                Log.w(TAG, "Failed to wait for supplicant death");
            }
        }
    }

    /**
     * Wrapper functions to access static HAL methods, created to be mockable in unit tests
     */
    protected IServiceManager getServiceManagerMockable() throws RemoteException {
        synchronized (mLock) {
            return IServiceManager.getService();
        }
    }

    protected ISupplicant getSupplicantMockable() throws RemoteException, NoSuchElementException {
        synchronized (mLock) {
            return ISupplicant.getService();
        }
    }

    protected ISupplicantVendor getSupplicantVendorMockable() throws RemoteException {
        synchronized (mLock) {
            try {
                return ISupplicantVendor.getService();
            } catch (NoSuchElementException e) {
                Log.e(TAG, "Failed to get ISupplicant", e);
                return null;
            }
        }
    }

    protected android.hardware.wifi.supplicant.V1_1.ISupplicant getSupplicantMockableV1_1()
            throws RemoteException, NoSuchElementException {
        synchronized (mLock) {
            return android.hardware.wifi.supplicant.V1_1.ISupplicant.castFrom(
                    ISupplicant.getService());
        }
    }

    protected android.hardware.wifi.supplicant.V1_2.ISupplicant getSupplicantMockableV1_2()
            throws RemoteException, NoSuchElementException {
        synchronized (mLock) {
            return android.hardware.wifi.supplicant.V1_2.ISupplicant.castFrom(
                    ISupplicant.getService());
        }
    }

    protected ISupplicantStaIface getStaIfaceMockable(ISupplicantIface iface) {
        synchronized (mLock) {
            return ISupplicantStaIface.asInterface(iface.asBinder());
        }
    }

    protected android.hardware.wifi.supplicant.V1_1.ISupplicantStaIface
            getStaIfaceMockableV1_1(ISupplicantIface iface) {
        synchronized (mLock) {
            return android.hardware.wifi.supplicant.V1_1.ISupplicantStaIface
                    .asInterface(iface.asBinder());
        }
    }

    protected ISupplicantVendorStaIface getVendorStaIfaceMockable(ISupplicantVendorIface iface) {
        synchronized (mLock) {
            return ISupplicantVendorStaIface.asInterface(iface.asBinder());
        }
    }

    protected ISupplicantVendorStaNetwork getVendorStaNetworkMockable(ISupplicantVendorNetwork network) {
        synchronized (mLock) {
            return ISupplicantVendorStaNetwork.asInterface(network.asBinder());
        }
    }

    protected android.hardware.wifi.supplicant.V1_2.ISupplicantStaIface
            getStaIfaceMockableV1_2(ISupplicantIface iface) {
        synchronized (mLock) {
            return android.hardware.wifi.supplicant.V1_2.ISupplicantStaIface
                    .asInterface(iface.asBinder());
        }
    }

    /**
     * Uses the IServiceManager to check if the device is running V1_1 of the HAL from the VINTF for
     * the device.
     * @return true if supported, false otherwise.
     */
    private boolean isV1_1() {
        return checkHalVersionByInterfaceName(
                android.hardware.wifi.supplicant.V1_1.ISupplicant.kInterfaceName);
    }

    /**
     * Uses the IServiceManager to check if the device is running V1_2 of the HAL from the VINTF for
     * the device.
     * @return true if supported, false otherwise.
     */
    private boolean isV1_2() {
        return checkHalVersionByInterfaceName(
                android.hardware.wifi.supplicant.V1_2.ISupplicant.kInterfaceName);
    }

    private boolean checkHalVersionByInterfaceName(String interfaceName) {
        if (interfaceName == null) {
            return false;
        }
        synchronized (mLock) {
            if (mIServiceManager == null) {
                Log.e(TAG, "checkHalVersionByInterfaceName: called but mServiceManager is null");
                return false;
            }
            try {
                return (mIServiceManager.getTransport(
                        interfaceName,
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
     * Check if the device is running V2_0 supplicant vendor service.
     * @return
     */
    private boolean isVendor_2_0() {
        synchronized (mLock) {
            try {
                return (getSupplicantVendorMockable() != null);
            } catch (RemoteException e) {
                Log.e(TAG, "ISupplicantVendor.getService exception: " + e);
                supplicantServiceDiedHandler(mDeathRecipientCookie);
                return false;
            }
        }
    }

    /**
     * Helper method to look up the iface object for the specified iface.
     */
    private ISupplicantStaIface getStaIface(@NonNull String ifaceName) {
        return mISupplicantStaIfaces.get(ifaceName);
    }

    /**
     * Helper method to look up the vendor_iface object for the specified iface.
     */
    private ISupplicantVendorStaIface getVendorStaIface(@NonNull String ifaceName) {
        return mISupplicantVendorStaIfaces.get(ifaceName);
    }

    /**

     * Helper method to look up the network object for the specified iface.
     */
    private SupplicantStaNetworkHal getCurrentNetworkRemoteHandle(@NonNull String ifaceName) {
        return mCurrentNetworkRemoteHandles.get(ifaceName);
    }

    /**
     * Helper method to look up the network config or the specified iface.
     */
    private WifiConfiguration getCurrentNetworkLocalConfig(@NonNull String ifaceName) {
        return mCurrentNetworkLocalConfigs.get(ifaceName);
    }

    /**
     * Add a network configuration to wpa_supplicant.
     *
     * @param config Config corresponding to the network.
     * @return a Pair object including SupplicantStaNetworkHal and WifiConfiguration objects
     * for the current network.
     */
    private Pair<SupplicantStaNetworkHal, WifiConfiguration>
            addNetworkAndSaveConfig(@NonNull String ifaceName, WifiConfiguration config) {
        synchronized (mLock) {
            logi("addSupplicantStaNetwork via HIDL");
            if (config == null) {
                loge("Cannot add NULL network!");
                return null;
            }
            SupplicantStaNetworkHal network = addNetwork(ifaceName);
            if (network == null) {
                loge("Failed to add a network!");
                return null;
            }
            network.getId();
            network.setVendorStaNetwork(getVendorNetwork
                                        (ifaceName, network.getNetworkId()));
            boolean saveSuccess = false;
            try {
                saveSuccess = network.saveWifiConfiguration(config);
            } catch (IllegalArgumentException e) {
                Log.e(TAG, "Exception while saving config params: " + config, e);
            }
            if (!saveSuccess) {
                loge("Failed to save variables for: " + config.configKey());
                if (!removeAllNetworks(ifaceName)) {
                    loge("Failed to remove all networks on failure.");
                }
                return null;
            }
            return new Pair(network, new WifiConfiguration(config));
        }
    }

    /**
     * Add the provided network configuration to wpa_supplicant and initiate connection to it.
     * This method does the following:
     * 1. If |config| is different to the current supplicant network, removes all supplicant
     * networks and saves |config|.
     * 2. Select the new network in wpa_supplicant.
     *
     * @param ifaceName Name of the interface.
     * @param config WifiConfiguration parameters for the provided network.
     * @return {@code true} if it succeeds, {@code false} otherwise
     */
    public boolean connectToNetwork(@NonNull String ifaceName, @NonNull WifiConfiguration config) {
        synchronized (mLock) {
            boolean isAscii = WifiGbk.isAllAscii(WifiGbk.getSsidBytes(config.SSID, "UTF-8")); // wifigbk++
            logd("connectToNetwork " + config.configKey() + " isAscii=" + isAscii);
            WifiConfiguration currentConfig = getCurrentNetworkLocalConfig(ifaceName);
            if (WifiConfigurationUtil.isSameNetwork(config, currentConfig) && isAscii) {
                String networkSelectionBSSID = config.getNetworkSelectionStatus()
                        .getNetworkSelectionBSSID();
                String networkSelectionBSSIDCurrent =
                        currentConfig.getNetworkSelectionStatus().getNetworkSelectionBSSID();
                if (Objects.equals(networkSelectionBSSID, networkSelectionBSSIDCurrent)) {
                    logd("Network is already saved, will not trigger remove and add operation.");
                } else {
                    logd("Network is already saved, but need to update BSSID.");
                    if (!setCurrentNetworkBssid(
                            ifaceName,
                            config.getNetworkSelectionStatus().getNetworkSelectionBSSID())) {
                        loge("Failed to set current network BSSID.");
                        return false;
                    }
                    mCurrentNetworkLocalConfigs.put(ifaceName, new WifiConfiguration(config));
                }
            } else {
                mCurrentNetworkRemoteHandles.remove(ifaceName);
                mCurrentNetworkLocalConfigs.remove(ifaceName);
                mLinkedNetworkLocalAndRemoteConfigs.remove(ifaceName);
                if (!removeAllNetworks(ifaceName)) {
                    loge("Failed to remove existing networks");
                    return false;
                }
               /**
                * Handle connection to saved FILS network when wifi is
                * restarted with altered driver configuration.
                */
                if (!getCapabilities(ifaceName, "key_mgmt").contains("FILS-SHA256"))
                    config.allowedKeyManagement.clear(WifiConfiguration.KeyMgmt.FILS_SHA256);

                if (!getCapabilities(ifaceName, "key_mgmt").contains("FILS-SHA384"))
                    config.allowedKeyManagement.clear(WifiConfiguration.KeyMgmt.FILS_SHA384);

                Pair<SupplicantStaNetworkHal, WifiConfiguration> pair =
                        addNetworkAndSaveConfig(ifaceName, config);
                if (pair == null) {
                    loge("Failed to add/save network configuration: " + config.configKey());
                    return false;
                }
                mCurrentNetworkRemoteHandles.put(ifaceName, pair.first);
                mCurrentNetworkLocalConfigs.put(ifaceName, pair.second);
            }
            getCapabilities(ifaceName, "key_mgmt");
            SupplicantStaNetworkHal networkHandle =
                    checkSupplicantStaNetworkAndLogFailure(ifaceName, "connectToNetwork");
            if (networkHandle == null || !networkHandle.select()) {
                loge("Failed to select network configuration: " + config.configKey());
                return false;
            }
            return true;
        }
    }

    /**
     * Initiates roaming to the already configured network in wpa_supplicant. If the network
     * configuration provided does not match the already configured network, then this triggers
     * a new connection attempt (instead of roam).
     * 1. First check if we're attempting to connect to the same network as we currently have
     * configured.
     * 2. Set the new bssid for the network in wpa_supplicant.
     * 3. Trigger reassociate command to wpa_supplicant.
     *
     * @param ifaceName Name of the interface.
     * @param config WifiConfiguration parameters for the provided network.
     * @return {@code true} if it succeeds, {@code false} otherwise
     */
    public boolean roamToNetwork(@NonNull String ifaceName, WifiConfiguration config) {
        synchronized (mLock) {
            if (getCurrentNetworkId(ifaceName) != config.networkId) {
                Log.w(TAG, "Cannot roam to a different network, initiate new connection. "
                        + "Current network ID: " + getCurrentNetworkId(ifaceName));
                return connectToNetwork(ifaceName, config);
            }
            String bssid = config.getNetworkSelectionStatus().getNetworkSelectionBSSID();
            logd("roamToNetwork" + config.configKey() + " (bssid " + bssid + ")");

            SupplicantStaNetworkHal networkHandle =
                    checkSupplicantStaNetworkAndLogFailure(ifaceName, "roamToNetwork");
            if (networkHandle == null || !networkHandle.setBssid(bssid)) {
                loge("Failed to set new bssid on network: " + config.configKey());
                return false;
            }
            if (!reassociate(ifaceName)) {
                loge("Failed to trigger reassociate");
                return false;
            }
            return true;
        }
    }

    /**
     * Load all the configured networks from wpa_supplicant.
     *
     * @param ifaceName     Name of the interface.
     * @param configs       Map of configuration key to configuration objects corresponding to all
     *                      the networks.
     * @param networkExtras Map of extra configuration parameters stored in wpa_supplicant.conf
     * @return true if succeeds, false otherwise.
     */
    public boolean loadNetworks(@NonNull String ifaceName, Map<String, WifiConfiguration> configs,
                                SparseArray<Map<String, String>> networkExtras) {
        synchronized (mLock) {
            List<Integer> networkIds = listNetworks(ifaceName);
            if (networkIds == null) {
                Log.e(TAG, "Failed to list networks");
                return false;
            }
            for (Integer networkId : networkIds) {
                SupplicantStaNetworkHal network = getNetwork(ifaceName, networkId);
                if (network == null) {
                    Log.e(TAG, "Failed to get network with ID: " + networkId);
                    return false;
                }
                WifiConfiguration config = new WifiConfiguration();
                Map<String, String> networkExtra = new HashMap<>();
                boolean loadSuccess = false;
                try {
                    loadSuccess = network.loadWifiConfiguration(config, networkExtra);
                } catch (IllegalArgumentException e) {
                    Log.wtf(TAG, "Exception while loading config params: " + config, e);
                }
                if (!loadSuccess) {
                    Log.e(TAG, "Failed to load wifi configuration for network with ID: " + networkId
                            + ". Skipping...");
                    continue;
                }
                // Set the default IP assignments.
                config.setIpAssignment(IpConfiguration.IpAssignment.DHCP);
                config.setProxySettings(IpConfiguration.ProxySettings.NONE);

                networkExtras.put(networkId, networkExtra);
                String configKey =
                        networkExtra.get(SupplicantStaNetworkHal.ID_STRING_KEY_CONFIG_KEY);
                final WifiConfiguration duplicateConfig = configs.put(configKey, config);
                if (duplicateConfig != null) {
                    // The network is already known. Overwrite the duplicate entry.
                    Log.i(TAG, "Replacing duplicate network: " + duplicateConfig.networkId);
                    removeNetwork(ifaceName, duplicateConfig.networkId);
                    networkExtras.remove(duplicateConfig.networkId);
                }
            }
            return true;
        }
    }

    /**
     * Remove the request |networkId| from supplicant if it's the current network,
     * if the current configured network matches |networkId|.
     *
     * @param ifaceName Name of the interface.
     * @param networkId network id of the network to be removed from supplicant.
     */
    public void removeNetworkIfCurrent(@NonNull String ifaceName, int networkId) {
        synchronized (mLock) {
            if (getCurrentNetworkId(ifaceName) == networkId) {
                // Currently we only save 1 network in supplicant.
                removeAllNetworks(ifaceName);
            }
        }
    }

    /**
     * Remove all networks from supplicant
     *
     * @param ifaceName Name of the interface.
     */
    public boolean removeAllNetworks(@NonNull String ifaceName) {
        synchronized (mLock) {
            ArrayList<Integer> networks = listNetworks(ifaceName);
            if (networks == null) {
                Log.e(TAG, "removeAllNetworks failed, got null networks");
                return false;
            }
            for (int id : networks) {
                if (!removeNetwork(ifaceName, id)) {
                    Log.e(TAG, "removeAllNetworks failed to remove network: " + id);
                    return false;
                }
            }
            // Reset current network info.  Probably not needed once we add support to remove/reset
            // current network on receiving disconnection event from supplicant (b/32898136).
            mCurrentNetworkRemoteHandles.remove(ifaceName);
            mCurrentNetworkLocalConfigs.remove(ifaceName);
            mLinkedNetworkLocalAndRemoteConfigs.remove(ifaceName);
            return true;
        }
    }

    /**
     * Set the currently configured network's bssid.
     *
     * @param ifaceName Name of the interface.
     * @param bssidStr Bssid to set in the form of "XX:XX:XX:XX:XX:XX"
     * @return true if succeeds, false otherwise.
     */
    public boolean setCurrentNetworkBssid(@NonNull String ifaceName, String bssidStr) {
        synchronized (mLock) {
            SupplicantStaNetworkHal networkHandle =
                    checkSupplicantStaNetworkAndLogFailure(ifaceName, "setCurrentNetworkBssid");
            if (networkHandle == null) return false;
            return networkHandle.setBssid(bssidStr);
        }
    }

    /**
     * Get the currently configured network's WPS NFC token.
     *
     * @param ifaceName Name of the interface.
     * @return Hex string corresponding to the WPS NFC token.
     */
    public String getCurrentNetworkWpsNfcConfigurationToken(@NonNull String ifaceName) {
        synchronized (mLock) {
            SupplicantStaNetworkHal networkHandle =
                    checkSupplicantStaNetworkAndLogFailure(
                            ifaceName, "getCurrentNetworkWpsNfcConfigurationToken");
            if (networkHandle == null) return null;
            return networkHandle.getWpsNfcConfigurationToken();
        }
    }

    /**
     * Get the eap anonymous identity for the currently configured network.
     *
     * @param ifaceName Name of the interface.
     * @return anonymous identity string if succeeds, null otherwise.
     */
    public String getCurrentNetworkEapAnonymousIdentity(@NonNull String ifaceName) {
        synchronized (mLock) {
            SupplicantStaNetworkHal networkHandle =
                    checkSupplicantStaNetworkAndLogFailure(
                            ifaceName, "getCurrentNetworkEapAnonymousIdentity");
            if (networkHandle == null) return null;
            return networkHandle.fetchEapAnonymousIdentity();
        }
    }

    /**
     * Send the eap identity response for the currently configured network.
     *
     * @param ifaceName Name of the interface.
     * @param identity identity used for EAP-Identity
     * @param encryptedIdentity encrypted identity used for EAP-AKA/EAP-SIM
     * @return true if succeeds, false otherwise.
     */
    public boolean sendCurrentNetworkEapIdentityResponse(
            @NonNull String ifaceName, @NonNull String identity, String encryptedIdentity) {
        synchronized (mLock) {
            SupplicantStaNetworkHal networkHandle =
                    checkSupplicantStaNetworkAndLogFailure(
                            ifaceName, "sendCurrentNetworkEapIdentityResponse");
            if (networkHandle == null) return false;
            return networkHandle.sendNetworkEapIdentityResponse(identity, encryptedIdentity);
        }
    }

    /**
     * Send the eap sim gsm auth response for the currently configured network.
     *
     * @param ifaceName Name of the interface.
     * @param paramsStr String to send.
     * @return true if succeeds, false otherwise.
     */
    public boolean sendCurrentNetworkEapSimGsmAuthResponse(
            @NonNull String ifaceName, String paramsStr) {
        synchronized (mLock) {
            SupplicantStaNetworkHal networkHandle =
                    checkSupplicantStaNetworkAndLogFailure(
                            ifaceName, "sendCurrentNetworkEapSimGsmAuthResponse");
            if (networkHandle == null) return false;
            return networkHandle.sendNetworkEapSimGsmAuthResponse(paramsStr);
        }
    }

    /**
     * Send the eap sim gsm auth failure for the currently configured network.
     *
     * @param ifaceName Name of the interface.
     * @return true if succeeds, false otherwise.
     */
    public boolean sendCurrentNetworkEapSimGsmAuthFailure(@NonNull String ifaceName) {
        synchronized (mLock) {
            SupplicantStaNetworkHal networkHandle =
                    checkSupplicantStaNetworkAndLogFailure(
                            ifaceName, "sendCurrentNetworkEapSimGsmAuthFailure");
            if (networkHandle == null) return false;
            return networkHandle.sendNetworkEapSimGsmAuthFailure();
        }
    }

    /**
     * Send the eap sim umts auth response for the currently configured network.
     *
     * @param ifaceName Name of the interface.
     * @param paramsStr String to send.
     * @return true if succeeds, false otherwise.
     */
    public boolean sendCurrentNetworkEapSimUmtsAuthResponse(
            @NonNull String ifaceName, String paramsStr) {
        synchronized (mLock) {
            SupplicantStaNetworkHal networkHandle =
                    checkSupplicantStaNetworkAndLogFailure(
                            ifaceName, "sendCurrentNetworkEapSimUmtsAuthResponse");
            if (networkHandle == null) return false;
            return networkHandle.sendNetworkEapSimUmtsAuthResponse(paramsStr);
        }
    }

    /**
     * Send the eap sim umts auts response for the currently configured network.
     *
     * @param ifaceName Name of the interface.
     * @param paramsStr String to send.
     * @return true if succeeds, false otherwise.
     */
    public boolean sendCurrentNetworkEapSimUmtsAutsResponse(
            @NonNull String ifaceName, String paramsStr) {
        synchronized (mLock) {
            SupplicantStaNetworkHal networkHandle =
                    checkSupplicantStaNetworkAndLogFailure(
                            ifaceName, "sendCurrentNetworkEapSimUmtsAutsResponse");
            if (networkHandle == null) return false;
            return networkHandle.sendNetworkEapSimUmtsAutsResponse(paramsStr);
        }
    }

    /**
     * Send the eap sim umts auth failure for the currently configured network.
     *
     * @param ifaceName Name of the interface.
     * @return true if succeeds, false otherwise.
     */
    public boolean sendCurrentNetworkEapSimUmtsAuthFailure(@NonNull String ifaceName) {
        synchronized (mLock) {
            SupplicantStaNetworkHal networkHandle =
                    checkSupplicantStaNetworkAndLogFailure(
                            ifaceName, "sendCurrentNetworkEapSimUmtsAuthFailure");
            if (networkHandle == null) return false;
            return networkHandle.sendNetworkEapSimUmtsAuthFailure();
        }
    }

    /**
     * Adds a new network.
     *
     * @return The ISupplicantNetwork object for the new network, or null if the call fails
     */
    private SupplicantStaNetworkHal addNetwork(@NonNull String ifaceName) {
        synchronized (mLock) {
            final String methodStr = "addNetwork";
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return null;
            Mutable<ISupplicantNetwork> newNetwork = new Mutable<>();
            try {
                iface.addNetwork((SupplicantStatus status,
                        ISupplicantNetwork network) -> {
                    if (checkStatusAndLogFailure(status, methodStr)) {
                        newNetwork.value = network;
                    }
                });
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
            }
            if (newNetwork.value != null) {
                return getStaNetworkMockable(
                        ifaceName,
                        ISupplicantStaNetwork.asInterface(newNetwork.value.asBinder()));
            } else {
                return null;
            }
        }
    }

    /**
     * Remove network from supplicant with network Id
     *
     * @return true if request is sent successfully, false otherwise.
     */
    private boolean removeNetwork(@NonNull String ifaceName, int id) {
        synchronized (mLock) {
            final String methodStr = "removeNetwork";
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return false;
            try {
                SupplicantStatus status = iface.removeNetwork(id);
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * Use this to mock the creation of SupplicantStaNetworkHal instance.
     *
     * @param ifaceName Name of the interface.
     * @param iSupplicantStaNetwork ISupplicantStaNetwork instance retrieved from HIDL.
     * @return The ISupplicantNetwork object for the given SupplicantNetworkId int, returns null if
     * the call fails
     */
    protected SupplicantStaNetworkHal getStaNetworkMockable(
            @NonNull String ifaceName, ISupplicantStaNetwork iSupplicantStaNetwork) {
        synchronized (mLock) {
            SupplicantStaNetworkHal network =
                    new SupplicantStaNetworkHal(iSupplicantStaNetwork, ifaceName, mContext,
                            mWifiMonitor);
            if (network != null) {
                network.enableVerboseLogging(mVerboseLoggingEnabled);
            }
            return network;
        }
    }

    /**
     * @return The ISupplicantNetwork object for the given SupplicantNetworkId int, returns null if
     * the call fails
     */
    private SupplicantStaNetworkHal getNetwork(@NonNull String ifaceName, int id) {
        synchronized (mLock) {
            final String methodStr = "getNetwork";
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return null;
            Mutable<ISupplicantNetwork> gotNetwork = new Mutable<>();
            try {
                iface.getNetwork(id, (SupplicantStatus status, ISupplicantNetwork network) -> {
                    if (checkStatusAndLogFailure(status, methodStr)) {
                        gotNetwork.value = network;
                    }
                });
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
            }
            if (gotNetwork.value != null) {
                return getStaNetworkMockable(
                        ifaceName,
                        ISupplicantStaNetwork.asInterface(gotNetwork.value.asBinder()));
            } else {
                return null;
            }
        }
    }

    /**
     * @return The ISupplicantVendorStaNetwork object for the given SupplicantNetworkId int, returns null if
     * the call fails
     */
    private ISupplicantVendorStaNetwork getVendorNetwork(@NonNull String ifaceName, int id) {
        synchronized (mLock) {
            final String methodStr = "getVendorNetwork";
            ISupplicantVendorStaIface iface = checkSupplicantVendorStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return null;
            Mutable<ISupplicantVendorNetwork> gotNetwork = new Mutable<>();
            try {
                iface.getVendorNetwork(id, (SupplicantStatus status, ISupplicantVendorNetwork network) -> {
                    if (checkStatusAndLogFailure(status, methodStr)) {
                        gotNetwork.value = network;
                    }
                });
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
            }
            if (gotNetwork.value != null) {
                return getVendorStaNetworkMockable(gotNetwork.value);
            } else {
                return null;
            }
        }
    }

    /** See ISupplicantStaNetwork.hal for documentation */
    private boolean registerCallback(
            ISupplicantStaIface iface, ISupplicantStaIfaceCallback callback) {
        synchronized (mLock) {
            final String methodStr = "registerCallback";
            if (iface == null) return false;
            try {
                SupplicantStatus status =  iface.registerCallback(callback);
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    private boolean registerCallbackV1_1(
            android.hardware.wifi.supplicant.V1_1.ISupplicantStaIface iface,
            android.hardware.wifi.supplicant.V1_1.ISupplicantStaIfaceCallback callback) {
        synchronized (mLock) {
            String methodStr = "registerCallback_1_1";

            if (iface == null) return false;
            try {
                SupplicantStatus status =  iface.registerCallback_1_1(callback);
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    private boolean registerCallbackV1_2(
            android.hardware.wifi.supplicant.V1_2.ISupplicantStaIface iface,
            android.hardware.wifi.supplicant.V1_2.ISupplicantStaIfaceCallback callback) {
        synchronized (mLock) {
            String methodStr = "registerCallback_1_2";

            if (iface == null) return false;
            try {
                SupplicantStatus status =  iface.registerCallback_1_2(callback);
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /** See ISupplicantVendorStaIface.hal for documentation */
    private boolean registerVendorCallback(
            ISupplicantVendorStaIface iface, ISupplicantVendorStaIfaceCallback callback) {
        synchronized (mLock) {
            final String methodStr = "registerVendorCallback";
            if (iface == null) return false;
            try {
                SupplicantStatus status =  iface.registerVendorCallback(callback);
                return checkVendorStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * @return a list of SupplicantNetworkID ints for all networks controlled by supplicant, returns
     * null if the call fails
     */
    private java.util.ArrayList<Integer> listNetworks(@NonNull String ifaceName) {
        synchronized (mLock) {
            final String methodStr = "listNetworks";
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return null;
            Mutable<ArrayList<Integer>> networkIdList = new Mutable<>();
            try {
                iface.listNetworks((SupplicantStatus status, ArrayList<Integer> networkIds) -> {
                    if (checkStatusAndLogFailure(status, methodStr)) {
                        networkIdList.value = networkIds;
                    }
                });
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
            }
            return networkIdList.value;
        }
    }

    /**
     * Set WPS device name.
     *
     * @param ifaceName Name of the interface.
     * @param name String to be set.
     * @return true if request is sent successfully, false otherwise.
     */
    public boolean setWpsDeviceName(@NonNull String ifaceName, String name) {
        synchronized (mLock) {
            final String methodStr = "setWpsDeviceName";
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return false;
            try {
                SupplicantStatus status = iface.setWpsDeviceName(name);
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * Set WPS device type.
     *
     * @param ifaceName Name of the interface.
     * @param typeStr Type specified as a string. Used format: <categ>-<OUI>-<subcateg>
     * @return true if request is sent successfully, false otherwise.
     */
    public boolean setWpsDeviceType(@NonNull String ifaceName, String typeStr) {
        synchronized (mLock) {
            try {
                Matcher match = WPS_DEVICE_TYPE_PATTERN.matcher(typeStr);
                if (!match.find() || match.groupCount() != 3) {
                    Log.e(TAG, "Malformed WPS device type " + typeStr);
                    return false;
                }
                short categ = Short.parseShort(match.group(1));
                byte[] oui = NativeUtil.hexStringToByteArray(match.group(2));
                short subCateg = Short.parseShort(match.group(3));

                byte[] bytes = new byte[8];
                ByteBuffer byteBuffer = ByteBuffer.wrap(bytes).order(ByteOrder.BIG_ENDIAN);
                byteBuffer.putShort(categ);
                byteBuffer.put(oui);
                byteBuffer.putShort(subCateg);
                return setWpsDeviceType(ifaceName, bytes);
            } catch (IllegalArgumentException e) {
                Log.e(TAG, "Illegal argument " + typeStr, e);
                return false;
            }
        }
    }

    private boolean setWpsDeviceType(@NonNull String ifaceName, byte[/* 8 */] type) {
        synchronized (mLock) {
            final String methodStr = "setWpsDeviceType";
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return false;
            try {
                SupplicantStatus status = iface.setWpsDeviceType(type);
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * Set WPS manufacturer.
     *
     * @param ifaceName Name of the interface.
     * @param manufacturer String to be set.
     * @return true if request is sent successfully, false otherwise.
     */
    public boolean setWpsManufacturer(@NonNull String ifaceName, String manufacturer) {
        synchronized (mLock) {
            final String methodStr = "setWpsManufacturer";
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return false;
            try {
                SupplicantStatus status = iface.setWpsManufacturer(manufacturer);
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * Set WPS model name.
     *
     * @param ifaceName Name of the interface.
     * @param modelName String to be set.
     * @return true if request is sent successfully, false otherwise.
     */
    public boolean setWpsModelName(@NonNull String ifaceName, String modelName) {
        synchronized (mLock) {
            final String methodStr = "setWpsModelName";
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return false;
            try {
                SupplicantStatus status = iface.setWpsModelName(modelName);
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * Set WPS model number.
     *
     * @param ifaceName Name of the interface.
     * @param modelNumber String to be set.
     * @return true if request is sent successfully, false otherwise.
     */
    public boolean setWpsModelNumber(@NonNull String ifaceName, String modelNumber) {
        synchronized (mLock) {
            final String methodStr = "setWpsModelNumber";
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return false;
            try {
                SupplicantStatus status = iface.setWpsModelNumber(modelNumber);
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * Set WPS serial number.
     *
     * @param ifaceName Name of the interface.
     * @param serialNumber String to be set.
     * @return true if request is sent successfully, false otherwise.
     */
    public boolean setWpsSerialNumber(@NonNull String ifaceName, String serialNumber) {
        synchronized (mLock) {
            final String methodStr = "setWpsSerialNumber";
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return false;
            try {
                SupplicantStatus status = iface.setWpsSerialNumber(serialNumber);
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * Set WPS config methods
     *
     * @param ifaceName Name of the interface.
     * @param configMethodsStr List of config methods.
     * @return true if request is sent successfully, false otherwise.
     */
    public boolean setWpsConfigMethods(@NonNull String ifaceName, String configMethodsStr) {
        synchronized (mLock) {
            short configMethodsMask = 0;
            String[] configMethodsStrArr = configMethodsStr.split("\\s+");
            for (int i = 0; i < configMethodsStrArr.length; i++) {
                configMethodsMask |= stringToWpsConfigMethod(configMethodsStrArr[i]);
            }
            return setWpsConfigMethods(ifaceName, configMethodsMask);
        }
    }

    private boolean setWpsConfigMethods(@NonNull String ifaceName, short configMethods) {
        synchronized (mLock) {
            final String methodStr = "setWpsConfigMethods";
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return false;
            try {
                SupplicantStatus status = iface.setWpsConfigMethods(configMethods);
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * Trigger a reassociation even if the iface is currently connected.
     *
     * @param ifaceName Name of the interface.
     * @return true if request is sent successfully, false otherwise.
     */
    public boolean reassociate(@NonNull String ifaceName) {
        synchronized (mLock) {
            final String methodStr = "reassociate";
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return false;
            try {
                SupplicantStatus status = iface.reassociate();
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * Trigger a reconnection if the iface is disconnected.
     *
     * @param ifaceName Name of the interface.
     * @return true if request is sent successfully, false otherwise.
     */
    public boolean reconnect(@NonNull String ifaceName) {
        synchronized (mLock) {
            final String methodStr = "reconnect";
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return false;
            try {
                SupplicantStatus status = iface.reconnect();
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * Trigger a disconnection from the currently connected network.
     *
     * @param ifaceName Name of the interface.
     * @return true if request is sent successfully, false otherwise.
     */
    public boolean disconnect(@NonNull String ifaceName) {
        synchronized (mLock) {
            final String methodStr = "disconnect";
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return false;
            try {
                SupplicantStatus status = iface.disconnect();
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * Enable or disable power save mode.
     *
     * @param ifaceName Name of the interface.
     * @param enable true to enable, false to disable.
     * @return true if request is sent successfully, false otherwise.
     */
    public boolean setPowerSave(@NonNull String ifaceName, boolean enable) {
        synchronized (mLock) {
            final String methodStr = "setPowerSave";
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return false;
            try {
                SupplicantStatus status = iface.setPowerSave(enable);
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * Initiate TDLS discover with the specified AP.
     *
     * @param ifaceName Name of the interface.
     * @param macAddress MAC Address of the AP.
     * @return true if request is sent successfully, false otherwise.
     */
    public boolean initiateTdlsDiscover(@NonNull String ifaceName, String macAddress) {
        synchronized (mLock) {
            try {
                return initiateTdlsDiscover(
                        ifaceName, NativeUtil.macAddressToByteArray(macAddress));
            } catch (IllegalArgumentException e) {
                Log.e(TAG, "Illegal argument " + macAddress, e);
                return false;
            }
        }
    }
    /** See ISupplicantStaIface.hal for documentation */
    private boolean initiateTdlsDiscover(@NonNull String ifaceName, byte[/* 6 */] macAddress) {
        synchronized (mLock) {
            final String methodStr = "initiateTdlsDiscover";
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return false;
            try {
                SupplicantStatus status = iface.initiateTdlsDiscover(macAddress);
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * Initiate TDLS setup with the specified AP.
     *
     * @param ifaceName Name of the interface.
     * @param macAddress MAC Address of the AP.
     * @return true if request is sent successfully, false otherwise.
     */
    public boolean initiateTdlsSetup(@NonNull String ifaceName, String macAddress) {
        synchronized (mLock) {
            try {
                return initiateTdlsSetup(ifaceName, NativeUtil.macAddressToByteArray(macAddress));
            } catch (IllegalArgumentException e) {
                Log.e(TAG, "Illegal argument " + macAddress, e);
                return false;
            }
        }
    }
    /** See ISupplicantStaIface.hal for documentation */
    private boolean initiateTdlsSetup(@NonNull String ifaceName, byte[/* 6 */] macAddress) {
        synchronized (mLock) {
            final String methodStr = "initiateTdlsSetup";
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return false;
            try {
                SupplicantStatus status = iface.initiateTdlsSetup(macAddress);
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * Initiate TDLS teardown with the specified AP.
     * @param ifaceName Name of the interface.
     * @param macAddress MAC Address of the AP.
     * @return true if request is sent successfully, false otherwise.
     */
    public boolean initiateTdlsTeardown(@NonNull String ifaceName, String macAddress) {
        synchronized (mLock) {
            try {
                return initiateTdlsTeardown(
                        ifaceName, NativeUtil.macAddressToByteArray(macAddress));
            } catch (IllegalArgumentException e) {
                Log.e(TAG, "Illegal argument " + macAddress, e);
                return false;
            }
        }
    }

    /** See ISupplicantStaIface.hal for documentation */
    private boolean initiateTdlsTeardown(@NonNull String ifaceName, byte[/* 6 */] macAddress) {
        synchronized (mLock) {
            final String methodStr = "initiateTdlsTeardown";
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return false;
            try {
                SupplicantStatus status = iface.initiateTdlsTeardown(macAddress);
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * Request the specified ANQP elements |elements| from the specified AP |bssid|.
     *
     * @param ifaceName Name of the interface.
     * @param bssid BSSID of the AP
     * @param infoElements ANQP elements to be queried. Refer to ISupplicantStaIface.AnqpInfoId.
     * @param hs20SubTypes HS subtypes to be queried. Refer to ISupplicantStaIface.Hs20AnqpSubTypes.
     * @return true if request is sent successfully, false otherwise.
     */
    public boolean initiateAnqpQuery(@NonNull String ifaceName, String bssid,
                                     ArrayList<Short> infoElements,
                                     ArrayList<Integer> hs20SubTypes) {
        synchronized (mLock) {
            try {
                return initiateAnqpQuery(
                        ifaceName,
                        NativeUtil.macAddressToByteArray(bssid), infoElements, hs20SubTypes);
            } catch (IllegalArgumentException e) {
                Log.e(TAG, "Illegal argument " + bssid, e);
                return false;
            }
        }
    }

    /** See ISupplicantStaIface.hal for documentation */
    private boolean initiateAnqpQuery(@NonNull String ifaceName, byte[/* 6 */] macAddress,
            java.util.ArrayList<Short> infoElements, java.util.ArrayList<Integer> subTypes) {
        synchronized (mLock) {
            final String methodStr = "initiateAnqpQuery";
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return false;
            try {
                SupplicantStatus status = iface.initiateAnqpQuery(
                        macAddress, infoElements, subTypes);
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * Request the specified ANQP ICON from the specified AP |bssid|.
     *
     * @param ifaceName Name of the interface.
     * @param bssid BSSID of the AP
     * @param fileName Name of the file to request.
     * @return true if request is sent successfully, false otherwise.
     */
    public boolean initiateHs20IconQuery(@NonNull String ifaceName, String bssid, String fileName) {
        synchronized (mLock) {
            try {
                return initiateHs20IconQuery(
                        ifaceName, NativeUtil.macAddressToByteArray(bssid), fileName);
            } catch (IllegalArgumentException e) {
                Log.e(TAG, "Illegal argument " + bssid, e);
                return false;
            }
        }
    }

    /** See ISupplicantStaIface.hal for documentation */
    private boolean initiateHs20IconQuery(@NonNull String ifaceName,
                                          byte[/* 6 */] macAddress, String fileName) {
        synchronized (mLock) {
            final String methodStr = "initiateHs20IconQuery";
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return false;
            try {
                SupplicantStatus status = iface.initiateHs20IconQuery(macAddress, fileName);
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * Makes a callback to HIDL to getMacAddress from supplicant
     *
     * @param ifaceName Name of the interface.
     * @return string containing the MAC address, or null on a failed call
     */
    public String getMacAddress(@NonNull String ifaceName) {
        synchronized (mLock) {
            final String methodStr = "getMacAddress";
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return null;
            Mutable<String> gotMac = new Mutable<>();
            try {
                iface.getMacAddress((SupplicantStatus status,
                        byte[/* 6 */] macAddr) -> {
                    if (checkStatusAndLogFailure(status, methodStr)) {
                        gotMac.value = NativeUtil.macAddressFromByteArray(macAddr);
                    }
                });
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
            }
            return gotMac.value;
        }
    }

    /**
     * Start using the added RX filters.
     *
     * @param ifaceName Name of the interface.
     * @return true if request is sent successfully, false otherwise.
     */
    public boolean startRxFilter(@NonNull String ifaceName) {
        synchronized (mLock) {
            final String methodStr = "startRxFilter";
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return false;
            try {
                SupplicantStatus status = iface.startRxFilter();
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * Stop using the added RX filters.
     *
     * @param ifaceName Name of the interface.
     * @return true if request is sent successfully, false otherwise.
     */
    public boolean stopRxFilter(@NonNull String ifaceName) {
        synchronized (mLock) {
            final String methodStr = "stopRxFilter";
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return false;
            try {
                SupplicantStatus status = iface.stopRxFilter();
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * Add an RX filter.
     *
     * @param ifaceName Name of the interface.
     * @param type one of {@link WifiNative#RX_FILTER_TYPE_V4_MULTICAST}
     *        {@link WifiNative#RX_FILTER_TYPE_V6_MULTICAST} values.
     * @return true if request is sent successfully, false otherwise.
     */
    public boolean addRxFilter(@NonNull String ifaceName, int type) {
        synchronized (mLock) {
            byte halType;
            switch (type) {
                case WifiNative.RX_FILTER_TYPE_V4_MULTICAST:
                    halType = ISupplicantStaIface.RxFilterType.V4_MULTICAST;
                    break;
                case WifiNative.RX_FILTER_TYPE_V6_MULTICAST:
                    halType = ISupplicantStaIface.RxFilterType.V6_MULTICAST;
                    break;
                default:
                    Log.e(TAG, "Invalid Rx Filter type: " + type);
                    return false;
            }
            return addRxFilter(ifaceName, halType);
        }
    }

    private boolean addRxFilter(@NonNull String ifaceName, byte type) {
        synchronized (mLock) {
            final String methodStr = "addRxFilter";
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return false;
            try {
                SupplicantStatus status = iface.addRxFilter(type);
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * Remove an RX filter.
     *
     * @param ifaceName Name of the interface.
     * @param type one of {@link WifiNative#RX_FILTER_TYPE_V4_MULTICAST}
     *        {@link WifiNative#RX_FILTER_TYPE_V6_MULTICAST} values.
     * @return true if request is sent successfully, false otherwise.
     */
    public boolean removeRxFilter(@NonNull String ifaceName, int type) {
        synchronized (mLock) {
            byte halType;
            switch (type) {
                case WifiNative.RX_FILTER_TYPE_V4_MULTICAST:
                    halType = ISupplicantStaIface.RxFilterType.V4_MULTICAST;
                    break;
                case WifiNative.RX_FILTER_TYPE_V6_MULTICAST:
                    halType = ISupplicantStaIface.RxFilterType.V6_MULTICAST;
                    break;
                default:
                    Log.e(TAG, "Invalid Rx Filter type: " + type);
                    return false;
            }
            return removeRxFilter(ifaceName, halType);
        }
    }

    private boolean removeRxFilter(@NonNull String ifaceName, byte type) {
        synchronized (mLock) {
            final String methodStr = "removeRxFilter";
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return false;
            try {
                SupplicantStatus status = iface.removeRxFilter(type);
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * Set Bt co existense mode.
     *
     * @param ifaceName Name of the interface.
     * @param mode one of the above {@link WifiNative#BLUETOOTH_COEXISTENCE_MODE_DISABLED},
     *             {@link WifiNative#BLUETOOTH_COEXISTENCE_MODE_ENABLED} or
     *             {@link WifiNative#BLUETOOTH_COEXISTENCE_MODE_SENSE}.
     * @return true if request is sent successfully, false otherwise.
     */
    public boolean setBtCoexistenceMode(@NonNull String ifaceName, int mode) {
        synchronized (mLock) {
            byte halMode;
            switch (mode) {
                case WifiNative.BLUETOOTH_COEXISTENCE_MODE_ENABLED:
                    halMode = ISupplicantStaIface.BtCoexistenceMode.ENABLED;
                    break;
                case WifiNative.BLUETOOTH_COEXISTENCE_MODE_DISABLED:
                    halMode = ISupplicantStaIface.BtCoexistenceMode.DISABLED;
                    break;
                case WifiNative.BLUETOOTH_COEXISTENCE_MODE_SENSE:
                    halMode = ISupplicantStaIface.BtCoexistenceMode.SENSE;
                    break;
                default:
                    Log.e(TAG, "Invalid Bt Coex mode: " + mode);
                    return false;
            }
            return setBtCoexistenceMode(ifaceName, halMode);
        }
    }

    private boolean setBtCoexistenceMode(@NonNull String ifaceName, byte mode) {
        synchronized (mLock) {
            final String methodStr = "setBtCoexistenceMode";
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return false;
            try {
                SupplicantStatus status = iface.setBtCoexistenceMode(mode);
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /** Enable or disable BT coexistence mode.
     *
     * @param ifaceName Name of the interface.
     * @param enable true to enable, false to disable.
     * @return true if request is sent successfully, false otherwise.
     */
    public boolean setBtCoexistenceScanModeEnabled(@NonNull String ifaceName, boolean enable) {
        synchronized (mLock) {
            final String methodStr = "setBtCoexistenceScanModeEnabled";
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return false;
            try {
                SupplicantStatus status =
                        iface.setBtCoexistenceScanModeEnabled(enable);
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * Enable or disable suspend mode optimizations.
     *
     * @param ifaceName Name of the interface.
     * @param enable true to enable, false otherwise.
     * @return true if request is sent successfully, false otherwise.
     */
    public boolean setSuspendModeEnabled(@NonNull String ifaceName, boolean enable) {
        synchronized (mLock) {
            final String methodStr = "setSuspendModeEnabled";
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return false;
            try {
                SupplicantStatus status = iface.setSuspendModeEnabled(enable);
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * Querry driver capabilities
     *
     * @param ifaceName Name of the interface.
     * @param capaType ASCII string, capability type ex: key_mgmt
     * @return String of capabilities fetched from driver
     */
    public String getCapabilities(@NonNull String ifaceName, String capaType) {
        synchronized (mLock) {
            final String methodStr = "getCapabilities";
            final Mutable<String> capability = new Mutable<>();

            capability.value = "";
            ISupplicantVendorStaIface iface =
               checkSupplicantVendorStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return capability.value;
            try {
                 iface.getCapabilities(capaType,
                        (SupplicantStatus status, String capaVal) -> {
                         if(checkVendorStatusAndLogFailure(status, methodStr)) {
                       capability.value = capaVal;
                    }
                });
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
            }
            return capability.value;
         }
    }

    /**
     * Set country code.
     *
     * @param ifaceName Name of the interface.
     * @param codeStr 2 byte ASCII string. For ex: US, CA.
     * @return true if request is sent successfully, false otherwise.
     */
    public boolean setCountryCode(@NonNull String ifaceName, String codeStr) {
        synchronized (mLock) {
            if (TextUtils.isEmpty(codeStr)) return false;
            byte[] countryCodeBytes = NativeUtil.stringToByteArray(codeStr);
            if (countryCodeBytes.length != 2) return false;
            return setCountryCode(ifaceName, countryCodeBytes);
        }
    }

    /** See ISupplicantStaIface.hal for documentation */
    private boolean setCountryCode(@NonNull String ifaceName, byte[/* 2 */] code) {
        synchronized (mLock) {
            final String methodStr = "setCountryCode";
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return false;
            try {
                SupplicantStatus status = iface.setCountryCode(code);
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * Flush all previously configured HLPs.
     *
     * @param ifaceName Name of the interface.
     * @return true if request is sent successfully, false otherwise.
     */
    public boolean flushAllHlp(@NonNull String ifaceName) {
        synchronized (mLock) {
            final String methodStr = "filsHlpFlushRequest";
            ISupplicantVendorStaIface iface =
                checkSupplicantVendorStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return false;
            try {
                SupplicantStatus status = iface.filsHlpFlushRequest();
                return checkVendorStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * Set FILS HLP packet.
     *
     * @param ifaceName Name of the interface.
     * @param dst Destination MAC address.
     * @param hlpPacket Hlp Packet data in hex.
     * @return true if request is sent successfully, false otherwise.
     */
    public boolean addHlpReq(@NonNull String ifaceName, String dst, String hlpPacket) {
        synchronized (mLock) {
            final String methodStr = "filsHlpAddRequest";
            ISupplicantVendorStaIface iface =
                   checkSupplicantVendorStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return false;
            try {
                SupplicantStatus status = iface.filsHlpAddRequest(
                                               NativeUtil.macAddressToByteArray(dst),
                                               NativeUtil.hexOrQuotedStringToBytes(hlpPacket));
                return checkVendorStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }


    /**
     * Start WPS pin registrar operation with the specified peer and pin.
     *
     * @param ifaceName Name of the interface.
     * @param bssidStr BSSID of the peer.
     * @param pin Pin to be used.
     * @return true if request is sent successfully, false otherwise.
     */
    public boolean startWpsRegistrar(@NonNull String ifaceName, String bssidStr, String pin) {
        synchronized (mLock) {
            if (TextUtils.isEmpty(bssidStr) || TextUtils.isEmpty(pin)) return false;
            try {
                return startWpsRegistrar(
                        ifaceName, NativeUtil.macAddressToByteArray(bssidStr), pin);
            } catch (IllegalArgumentException e) {
                Log.e(TAG, "Illegal argument " + bssidStr, e);
                return false;
            }
        }
    }

    /** See ISupplicantStaIface.hal for documentation */
    private boolean startWpsRegistrar(@NonNull String ifaceName, byte[/* 6 */] bssid, String pin) {
        synchronized (mLock) {
            final String methodStr = "startWpsRegistrar";
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return false;
            try {
                SupplicantStatus status = iface.startWpsRegistrar(bssid, pin);
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * Start WPS pin display operation with the specified peer.
     *
     * @param ifaceName Name of the interface.
     * @param bssidStr BSSID of the peer. Use empty bssid to indicate wildcard.
     * @return true if request is sent successfully, false otherwise.
     */
    public boolean startWpsPbc(@NonNull String ifaceName, String bssidStr) {
        synchronized (mLock) {
            try {
                return startWpsPbc(ifaceName, NativeUtil.macAddressToByteArray(bssidStr));
            } catch (IllegalArgumentException e) {
                Log.e(TAG, "Illegal argument " + bssidStr, e);
                return false;
            }
        }
    }

    /** See ISupplicantStaIface.hal for documentation */
    private boolean startWpsPbc(@NonNull String ifaceName, byte[/* 6 */] bssid) {
        synchronized (mLock) {
            final String methodStr = "startWpsPbc";
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return false;
            try {
                SupplicantStatus status = iface.startWpsPbc(bssid);
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * Start WPS pin keypad operation with the specified pin.
     *
     * @param ifaceName Name of the interface.
     * @param pin Pin to be used.
     * @return true if request is sent successfully, false otherwise.
     */
    public boolean startWpsPinKeypad(@NonNull String ifaceName, String pin) {
        if (TextUtils.isEmpty(pin)) return false;
        synchronized (mLock) {
            final String methodStr = "startWpsPinKeypad";
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return false;
            try {
                SupplicantStatus status = iface.startWpsPinKeypad(pin);
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * Start WPS pin display operation with the specified peer.
     *
     * @param ifaceName Name of the interface.
     * @param bssidStr BSSID of the peer. Use empty bssid to indicate wildcard.
     * @return new pin generated on success, null otherwise.
     */
    public String startWpsPinDisplay(@NonNull String ifaceName, String bssidStr) {
        synchronized (mLock) {
            try {
                return startWpsPinDisplay(ifaceName, NativeUtil.macAddressToByteArray(bssidStr));
            } catch (IllegalArgumentException e) {
                Log.e(TAG, "Illegal argument " + bssidStr, e);
                return null;
            }
        }
    }

    /** See ISupplicantStaIface.hal for documentation */
    private String startWpsPinDisplay(@NonNull String ifaceName, byte[/* 6 */] bssid) {
        synchronized (mLock) {
            final String methodStr = "startWpsPinDisplay";
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return null;
            final Mutable<String> gotPin = new Mutable<>();
            try {
                iface.startWpsPinDisplay(bssid,
                        (SupplicantStatus status, String pin) -> {
                            if (checkStatusAndLogFailure(status, methodStr)) {
                                gotPin.value = pin;
                            }
                        });
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
            }
            return gotPin.value;
        }
    }

    /**
     * Cancels any ongoing WPS requests.
     *
     * @param ifaceName Name of the interface.
     * @return true if request is sent successfully, false otherwise.
     */
    public boolean cancelWps(@NonNull String ifaceName) {
        synchronized (mLock) {
            final String methodStr = "cancelWps";
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return false;
            try {
                SupplicantStatus status = iface.cancelWps();
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * Sets whether to use external sim for SIM/USIM processing.
     *
     * @param ifaceName Name of the interface.
     * @param useExternalSim true to enable, false otherwise.
     * @return true if request is sent successfully, false otherwise.
     */
    public boolean setExternalSim(@NonNull String ifaceName, boolean useExternalSim) {
        synchronized (mLock) {
            final String methodStr = "setExternalSim";
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return false;
            try {
                SupplicantStatus status = iface.setExternalSim(useExternalSim);
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /** See ISupplicant.hal for documentation */
    public boolean enableAutoReconnect(@NonNull String ifaceName, boolean enable) {
        synchronized (mLock) {
            final String methodStr = "enableAutoReconnect";
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return false;
            try {
                SupplicantStatus status = iface.enableAutoReconnect(enable);
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * Set the debug log level for wpa_supplicant
     *
     * @param turnOnVerbose Whether to turn on verbose logging or not.
     * @return true if request is sent successfully, false otherwise.
     */
    public boolean setLogLevel(boolean turnOnVerbose) {
        synchronized (mLock) {
            int logLevel = turnOnVerbose
                    ? ISupplicant.DebugLevel.DEBUG
                    : ISupplicant.DebugLevel.INFO;
            return setDebugParams(logLevel, false, false);
        }
    }

    /** See ISupplicant.hal for documentation */
    private boolean setDebugParams(int level, boolean showTimestamp, boolean showKeys) {
        synchronized (mLock) {
            final String methodStr = "setDebugParams";
            if (!checkSupplicantAndLogFailure(methodStr)) return false;
            try {
                SupplicantStatus status =
                        mISupplicant.setDebugParams(level, showTimestamp, showKeys);
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * Set concurrency priority between P2P & STA operations.
     *
     * @param isStaHigherPriority Set to true to prefer STA over P2P during concurrency operations,
     *                            false otherwise.
     * @return true if request is sent successfully, false otherwise.
     */
    public boolean setConcurrencyPriority(boolean isStaHigherPriority) {
        synchronized (mLock) {
            if (isStaHigherPriority) {
                return setConcurrencyPriority(IfaceType.STA);
            } else {
                return setConcurrencyPriority(IfaceType.P2P);
            }
        }
    }

    /** See ISupplicant.hal for documentation */
    private boolean setConcurrencyPriority(int type) {
        synchronized (mLock) {
            final String methodStr = "setConcurrencyPriority";
            if (!checkSupplicantAndLogFailure(methodStr)) return false;
            try {
                SupplicantStatus status = mISupplicant.setConcurrencyPriority(type);
                return checkStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * Returns false if Supplicant is null, and logs failure to call methodStr
     */
    private boolean checkSupplicantAndLogFailure(final String methodStr) {
        synchronized (mLock) {
            if (mISupplicant == null) {
                Log.e(TAG, "Can't call " + methodStr + ", ISupplicant is null");
                return false;
            }
            return true;
        }
    }

    /**
     * Returns false if SupplicantVendor is null, and logs failure to call methodStr
     */
    private boolean checkSupplicantVendorAndLogFailure(final String methodStr) {
        synchronized (mLock) {
            if (mISupplicantVendor == null) {
                Log.e(TAG, "Can't call " + methodStr + ", ISupplicantVendor is null");
                return false;
            }
            return true;
        }
    }

    /**
     * Returns false if SupplicantStaIface is null, and logs failure to call methodStr
     */
    private ISupplicantStaIface checkSupplicantStaIfaceAndLogFailure(
            @NonNull String ifaceName, final String methodStr) {
        synchronized (mLock) {
            ISupplicantStaIface iface = getStaIface(ifaceName);
            if (iface == null) {
                Log.e(TAG, "Can't call " + methodStr + ", ISupplicantStaIface is null");
                return null;
            }
            return iface;
        }
    }

    /**
     * Returns false if SupplicantVendorStaIface is null, and logs failure to call methodStr
     */
    private ISupplicantVendorStaIface checkSupplicantVendorStaIfaceAndLogFailure(
            @NonNull String ifaceName, final String methodStr) {
        synchronized (mLock) {
            ISupplicantVendorStaIface iface = getVendorStaIface(ifaceName);
            if (iface == null) {
                Log.e(TAG, "Can't call " + methodStr + ", ISupplicantVendorStaIface is null");
                return null;
            }
            return iface;
        }
    }

    /**
     * Returns false if SupplicantStaNetwork is null, and logs failure to call methodStr
     */
    private SupplicantStaNetworkHal checkSupplicantStaNetworkAndLogFailure(
            @NonNull String ifaceName, final String methodStr) {
        synchronized (mLock) {
            SupplicantStaNetworkHal networkHal = getCurrentNetworkRemoteHandle(ifaceName);
            if (networkHal == null) {
                Log.e(TAG, "Can't call " + methodStr + ", SupplicantStaNetwork is null");
                return null;
            }
            return networkHal;
        }
    }

    /**
     * Returns true if provided status code is SUCCESS, logs debug message and returns false
     * otherwise
     */
    private boolean checkStatusAndLogFailure(SupplicantStatus status,
            final String methodStr) {
        synchronized (mLock) {
            if (status.code != SupplicantStatusCode.SUCCESS) {
                Log.e(TAG, "ISupplicantStaIface." + methodStr + " failed: " + status);
                return false;
            } else {
                if (mVerboseLoggingEnabled) {
                    Log.d(TAG, "ISupplicantStaIface." + methodStr + " succeeded");
                }
                return true;
            }
        }
    }

    /**
     * Returns true if provided supplicant vendor status code is SUCCESS, logs debug message and returns false
     * otherwise
     */
    private boolean checkSupplicantVendorStatusAndLogFailure(SupplicantStatus status,
            final String methodStr) {
        synchronized (mLock) {
            if (status.code != SupplicantStatusCode.SUCCESS) {
                Log.e(TAG, "ISupplicantVendor." + methodStr + " failed: " + status);
                return false;
            } else {
                if (mVerboseLoggingEnabled) {
                    Log.d(TAG, "ISupplicantVendor." + methodStr + " succeeded");
                }
                return true;
            }
        }
    }

    /**
     * Returns true if provided Vendor status code is SUCCESS, logs debug message and returns false
     * otherwise
     */
    private boolean checkVendorStatusAndLogFailure(SupplicantStatus status,
            final String methodStr) {
        synchronized (mLock) {
            if (status.code != SupplicantStatusCode.SUCCESS) {
                Log.e(TAG, "ISupplicantVendorStaIface." + methodStr + " failed: " + status);
                return false;
            } else {
                if (mVerboseLoggingEnabled) {
                    Log.d(TAG, "ISupplicantVendorStaIface." + methodStr + " succeeded");
                }
                return true;
            }
        }
    }

    /**
     * Helper function to log callbacks.
     */
    private void logCallback(final String methodStr) {
        synchronized (mLock) {
            if (mVerboseLoggingEnabled) {
                Log.d(TAG, "ISupplicantStaIfaceCallback." + methodStr + " received");
            }
        }
    }

    private void handleNoSuchElementException(NoSuchElementException e, String methodStr) {
        synchronized (mLock) {
            clearState();
            Log.e(TAG, "ISupplicantStaIface." + methodStr + " failed with exception", e);
        }
    }

    private void handleRemoteException(RemoteException e, String methodStr) {
        synchronized (mLock) {
            clearState();
            Log.e(TAG, "ISupplicantStaIface." + methodStr + " failed with exception", e);
        }
    }

    /**
     * Converts the Wps config method string to the equivalent enum value.
     */
    private static short stringToWpsConfigMethod(String configMethod) {
        switch (configMethod) {
            case "usba":
                return WpsConfigMethods.USBA;
            case "ethernet":
                return WpsConfigMethods.ETHERNET;
            case "label":
                return WpsConfigMethods.LABEL;
            case "display":
                return WpsConfigMethods.DISPLAY;
            case "int_nfc_token":
                return WpsConfigMethods.INT_NFC_TOKEN;
            case "ext_nfc_token":
                return WpsConfigMethods.EXT_NFC_TOKEN;
            case "nfc_interface":
                return WpsConfigMethods.NFC_INTERFACE;
            case "push_button":
                return WpsConfigMethods.PUSHBUTTON;
            case "keypad":
                return WpsConfigMethods.KEYPAD;
            case "virtual_push_button":
                return WpsConfigMethods.VIRT_PUSHBUTTON;
            case "physical_push_button":
                return WpsConfigMethods.PHY_PUSHBUTTON;
            case "p2ps":
                return WpsConfigMethods.P2PS;
            case "virtual_display":
                return WpsConfigMethods.VIRT_DISPLAY;
            case "physical_display":
                return WpsConfigMethods.PHY_DISPLAY;
            default:
                throw new IllegalArgumentException(
                        "Invalid WPS config method: " + configMethod);
        }
    }

    /**
     * Converts the supplicant state received from HIDL to the equivalent framework state.
     */
    private static SupplicantState supplicantHidlStateToFrameworkState(int state) {
        switch (state) {
            case ISupplicantStaIfaceCallback.State.DISCONNECTED:
                return SupplicantState.DISCONNECTED;
            case ISupplicantStaIfaceCallback.State.IFACE_DISABLED:
                return SupplicantState.INTERFACE_DISABLED;
            case ISupplicantStaIfaceCallback.State.INACTIVE:
                return SupplicantState.INACTIVE;
            case ISupplicantStaIfaceCallback.State.SCANNING:
                return SupplicantState.SCANNING;
            case ISupplicantStaIfaceCallback.State.AUTHENTICATING:
                return SupplicantState.AUTHENTICATING;
            case ISupplicantStaIfaceCallback.State.ASSOCIATING:
                return SupplicantState.ASSOCIATING;
            case ISupplicantStaIfaceCallback.State.ASSOCIATED:
                return SupplicantState.ASSOCIATED;
            case ISupplicantStaIfaceCallback.State.FOURWAY_HANDSHAKE:
                return SupplicantState.FOUR_WAY_HANDSHAKE;
            case ISupplicantStaIfaceCallback.State.GROUP_HANDSHAKE:
                return SupplicantState.GROUP_HANDSHAKE;
            case ISupplicantStaIfaceCallback.State.COMPLETED:
                return SupplicantState.COMPLETED;
            default:
                throw new IllegalArgumentException("Invalid state: " + state);
        }
    }

    private class SupplicantVendorStaIfaceHalCallback extends ISupplicantVendorStaIfaceCallback.Stub {
        private String mIfaceName;
        private SupplicantStaIfaceHalCallback mSupplicantStaIfacecallback;

        SupplicantVendorStaIfaceHalCallback(@NonNull String ifaceName, SupplicantStaIfaceHalCallback callback) {
            mIfaceName = ifaceName;
            mSupplicantStaIfacecallback = callback;
        }

        @Override
        public void onVendorStateChanged(int newState, byte[/* 6 */] bssid, int id,
                                   ArrayList<Byte> ssid, boolean filsHlpSent) {
            synchronized (mLock) {
                logCallback("onVendorStateChanged");
                SupplicantState newSupplicantState = supplicantHidlStateToFrameworkState(newState);
                WifiSsid wifiSsid = // wifigbk++
                        WifiGbk.createWifiSsidFromByteArray(NativeUtil.byteArrayFromArrayList(ssid));
                String bssidStr = NativeUtil.macAddressFromByteArray(bssid);
                mSupplicantStaIfacecallback.updateStateIsFourway(
                        (newState == ISupplicantStaIfaceCallback.State.FOURWAY_HANDSHAKE));
                SupplicantStaNetworkHal networkHal = getCurrentNetworkRemoteHandle(mIfaceName);
                if (networkHal != null && id != networkHal.getNetworkId()) {
                    Log.i(TAG, "onVendorStateChanged: network id changed, newState = " + newState
                                + ", SSID = " + wifiSsid + ", bssid = " + bssidStr
                                + ", networkId = " + id);
                    ArrayList<Pair<SupplicantStaNetworkHal, WifiConfiguration>> linkedNetworkHandles =
                        mLinkedNetworkLocalAndRemoteConfigs.get(mIfaceName);

                    for (Pair<SupplicantStaNetworkHal, WifiConfiguration> pair : linkedNetworkHandles) {
                        if (pair.first.getNetworkId() != id) continue;

                        Log.i(TAG, "onVendorStateChanged: make linked network as current network");
                        mCurrentNetworkRemoteHandles.put(mIfaceName, pair.first);
                        mCurrentNetworkLocalConfigs.put(mIfaceName, pair.second);
                    }
                }
                if (newSupplicantState == SupplicantState.COMPLETED) {
                    if (filsHlpSent == false) {
                        mWifiMonitor.broadcastNetworkConnectionEvent(
                                mIfaceName, getCurrentNetworkId(mIfaceName), bssidStr);
                    } else {
                        mWifiMonitor.broadcastFilsNetworkConnectionEvent(
                                mIfaceName, getCurrentNetworkId(mIfaceName), bssidStr);
                    }
                }
                mWifiMonitor.broadcastSupplicantStateChangeEvent(
                        mIfaceName, getCurrentNetworkId(mIfaceName), wifiSsid, bssidStr, newSupplicantState);
            }
        }

        /* DPP Callbacks Start */
        @Override
        public void onDppAuthSuccess(boolean initiator) {
            logCallback("onDppAuthSuccess");
            synchronized (mLock) {
                DppResult result = new DppResult();
                result.initiator = initiator;
                mWifiMonitor.broadcastDppEvent(mIfaceName, DPP_EVENT_AUTH_SUCCESS, result);
            }
        }

        @Override
        public void onDppConf(byte type, ArrayList<Byte> ssid, String connector,
                              ArrayList<Byte> cSignKey, ArrayList<Byte> netAccessKey,
                              int netAccessExpiry, String passphrase, ArrayList<Byte> psk) {
            logCallback("onDppConf");
            synchronized (mLock) {
                DppResult result = new DppResult();
                result.configEventType = type;
                result.ssid = NativeUtil.stringFromByteArrayList(ssid);
                result.connector = connector;
                result.cSignKey = NativeUtil.bytesToHexOrQuotedString(cSignKey);
                result.netAccessKey = NativeUtil.bytesToHexOrQuotedString(netAccessKey);
                result.netAccessKeyExpiry = netAccessExpiry;
                result.passphrase = passphrase;
                result.psk = NativeUtil.stringFromByteArrayList(psk);
                mWifiMonitor.broadcastDppEvent(mIfaceName, DPP_EVENT_CONF, result);
            }
        }

        @Override
        public void onDppNotCompatible(byte capab, boolean initiator) {}

        @Override
        public void onDppResponsePending() {}

        @Override
        public void onDppScanPeerQrCode(ArrayList<Byte> bootstrapData) {}

        @Override
        public void onDppMissingAuth(byte dppAuthParam) {}

        @Override
        public void onDppNetworkId(int netID) {}
        /* DPP Callbacks ends */
    }

    private class SupplicantStaIfaceHalCallback extends ISupplicantStaIfaceCallback.Stub {
        private String mIfaceName;
        private boolean mStateIsFourway = false; // Used to help check for PSK password mismatch

        SupplicantStaIfaceHalCallback(@NonNull String ifaceName) {
            mIfaceName = ifaceName;
        }

        /**
         * Parses the provided payload into an ANQP element.
         *
         * @param infoID  Element type.
         * @param payload Raw payload bytes.
         * @return AnqpElement instance on success, null on failure.
         */
        private ANQPElement parseAnqpElement(Constants.ANQPElementType infoID,
                                             ArrayList<Byte> payload) {
            synchronized (mLock) {
                try {
                    return Constants.getANQPElementID(infoID) != null
                            ? ANQPParser.parseElement(
                            infoID, ByteBuffer.wrap(NativeUtil.byteArrayFromArrayList(payload)))
                            : ANQPParser.parseHS20Element(
                            infoID, ByteBuffer.wrap(NativeUtil.byteArrayFromArrayList(payload)));
                } catch (IOException | BufferUnderflowException e) {
                    Log.e(TAG, "Failed parsing ANQP element payload: " + infoID, e);
                    return null;
                }
            }
        }

        /**
         * Parse the ANQP element data and add to the provided elements map if successful.
         *
         * @param elementsMap Map to add the parsed out element to.
         * @param infoID  Element type.
         * @param payload Raw payload bytes.
         */
        private void addAnqpElementToMap(Map<Constants.ANQPElementType, ANQPElement> elementsMap,
                                         Constants.ANQPElementType infoID,
                                         ArrayList<Byte> payload) {
            synchronized (mLock) {
                if (payload == null || payload.isEmpty()) return;
                ANQPElement element = parseAnqpElement(infoID, payload);
                if (element != null) {
                    elementsMap.put(infoID, element);
                }
            }
        }

        @Override
        public void onNetworkAdded(int id) {
            synchronized (mLock) {
                logCallback("onNetworkAdded");
            }
        }

        @Override
        public void onNetworkRemoved(int id) {
            synchronized (mLock) {
                logCallback("onNetworkRemoved");
                // Reset 4way handshake state since network has been removed.
                mStateIsFourway = false;
            }
        }

        @Override
        public void onStateChanged(int newState, byte[/* 6 */] bssid, int id,
                                   ArrayList<Byte> ssid) {
            synchronized (mLock) {
                logCallback("onStateChanged");
                SupplicantState newSupplicantState = supplicantHidlStateToFrameworkState(newState);
                WifiSsid wifiSsid = // wifigbk++
                        WifiGbk.createWifiSsidFromByteArray(NativeUtil.byteArrayFromArrayList(ssid));
                String bssidStr = NativeUtil.macAddressFromByteArray(bssid);
                mStateIsFourway = (newState == ISupplicantStaIfaceCallback.State.FOURWAY_HANDSHAKE);
                if (newSupplicantState == SupplicantState.COMPLETED) {
                    mWifiMonitor.broadcastNetworkConnectionEvent(
                            mIfaceName, getCurrentNetworkId(mIfaceName), bssidStr);
                }
                mWifiMonitor.broadcastSupplicantStateChangeEvent(
                        mIfaceName, getCurrentNetworkId(mIfaceName), wifiSsid,
                        bssidStr, newSupplicantState);
            }
        }

        @Override
        public void onAnqpQueryDone(byte[/* 6 */] bssid,
                                    ISupplicantStaIfaceCallback.AnqpData data,
                                    ISupplicantStaIfaceCallback.Hs20AnqpData hs20Data) {
            synchronized (mLock) {
                logCallback("onAnqpQueryDone");
                Map<Constants.ANQPElementType, ANQPElement> elementsMap = new HashMap<>();
                addAnqpElementToMap(elementsMap, ANQPVenueName, data.venueName);
                addAnqpElementToMap(elementsMap, ANQPRoamingConsortium, data.roamingConsortium);
                addAnqpElementToMap(
                        elementsMap, ANQPIPAddrAvailability, data.ipAddrTypeAvailability);
                addAnqpElementToMap(elementsMap, ANQPNAIRealm, data.naiRealm);
                addAnqpElementToMap(elementsMap, ANQP3GPPNetwork, data.anqp3gppCellularNetwork);
                addAnqpElementToMap(elementsMap, ANQPDomName, data.domainName);
                addAnqpElementToMap(elementsMap, HSFriendlyName, hs20Data.operatorFriendlyName);
                addAnqpElementToMap(elementsMap, HSWANMetrics, hs20Data.wanMetrics);
                addAnqpElementToMap(elementsMap, HSConnCapability, hs20Data.connectionCapability);
                addAnqpElementToMap(elementsMap, HSOSUProviders, hs20Data.osuProvidersList);
                mWifiMonitor.broadcastAnqpDoneEvent(
                        mIfaceName, new AnqpEvent(NativeUtil.macAddressToLong(bssid), elementsMap));
            }
        }

        @Override
        public void onHs20IconQueryDone(byte[/* 6 */] bssid, String fileName,
                                        ArrayList<Byte> data) {
            synchronized (mLock) {
                logCallback("onHs20IconQueryDone");
                mWifiMonitor.broadcastIconDoneEvent(
                        mIfaceName,
                        new IconEvent(NativeUtil.macAddressToLong(bssid), fileName, data.size(),
                                NativeUtil.byteArrayFromArrayList(data)));
            }
        }

        @Override
        public void onHs20SubscriptionRemediation(byte[/* 6 */] bssid, byte osuMethod, String url) {
            synchronized (mLock) {
                logCallback("onHs20SubscriptionRemediation");
                mWifiMonitor.broadcastWnmEvent(
                        mIfaceName,
                        new WnmData(NativeUtil.macAddressToLong(bssid), url, osuMethod));
            }
        }

        @Override
        public void onHs20DeauthImminentNotice(byte[/* 6 */] bssid, int reasonCode,
                                               int reAuthDelayInSec, String url) {
            synchronized (mLock) {
                logCallback("onHs20DeauthImminentNotice");
                mWifiMonitor.broadcastWnmEvent(
                        mIfaceName,
                        new WnmData(NativeUtil.macAddressToLong(bssid), url,
                                reasonCode == WnmData.ESS, reAuthDelayInSec));
            }
        }

        @Override
        public void onDisconnected(byte[/* 6 */] bssid, boolean locallyGenerated, int reasonCode) {
            synchronized (mLock) {
                logCallback("onDisconnected");
                if (mVerboseLoggingEnabled) {
                    Log.e(TAG, "onDisconnected 4way=" + mStateIsFourway
                            + " locallyGenerated=" + locallyGenerated
                            + " reasonCode=" + reasonCode);
                }
                if (mStateIsFourway
                        && (!locallyGenerated || reasonCode != ReasonCode.IE_IN_4WAY_DIFFERS)) {
                    mWifiMonitor.broadcastAuthenticationFailureEvent(
                            mIfaceName, WifiManager.ERROR_AUTH_FAILURE_WRONG_PSWD, -1);
                }
                mWifiMonitor.broadcastNetworkDisconnectionEvent(
                        mIfaceName, locallyGenerated ? 1 : 0, reasonCode,
                        NativeUtil.macAddressFromByteArray(bssid));
            }
        }

        @Override
        public void onAssociationRejected(byte[/* 6 */] bssid, int statusCode, boolean timedOut) {
            synchronized (mLock) {
                logCallback("onAssociationRejected");

                if (statusCode == StatusCode.UNSPECIFIED_FAILURE) {
                    WifiConfiguration curConfiguration = getCurrentNetworkLocalConfig(mIfaceName);

                    if (curConfiguration != null
                            && curConfiguration.allowedKeyManagement
                                    .get(WifiConfiguration.KeyMgmt.SAE)) {
                        // Special handling for WPA3-Personal networks. If the password is
                        // incorrect, the AP will send association rejection, with status code 1
                        // (unspecified failure). In SAE networks, the password authentication
                        // is not related to the 4-way handshake. In this case, we will send an
                        // authentication failure event up.
                        logCallback("SAE incorrect password");
                        mWifiMonitor.broadcastAuthenticationFailureEvent(
                                mIfaceName, WifiManager.ERROR_AUTH_FAILURE_WRONG_PSWD, -1);
                    }
                }

                // For WEP password error: not check status code to avoid IoT issues with AP.
                WifiConfiguration wificonfig = getCurrentNetworkLocalConfig(mIfaceName);
                if (wificonfig != null && WifiConfigurationUtil.isConfigForWepNetwork(wificonfig)) {
                    logCallback("WEP incorrect password");
                    mWifiMonitor.broadcastAuthenticationFailureEvent(
                        mIfaceName, WifiManager.ERROR_AUTH_FAILURE_WRONG_PSWD, -1);
                    // Not broadcast ASSOC REJECT to avoid bssid get blacklisted.
                    return;
                }

                mWifiMonitor.broadcastAssociationRejectionEvent(mIfaceName, statusCode, timedOut,
                        NativeUtil.macAddressFromByteArray(bssid));
            }
        }

        @Override
        public void onAuthenticationTimeout(byte[/* 6 */] bssid) {
            synchronized (mLock) {
                logCallback("onAuthenticationTimeout");
                mWifiMonitor.broadcastAuthenticationFailureEvent(
                        mIfaceName, WifiManager.ERROR_AUTH_FAILURE_TIMEOUT, -1);
            }
        }

        @Override
        public void onBssidChanged(byte reason, byte[/* 6 */] bssid) {
            synchronized (mLock) {
                logCallback("onBssidChanged");
                if (reason == BssidChangeReason.ASSOC_START) {
                    mWifiMonitor.broadcastTargetBssidEvent(
                            mIfaceName, NativeUtil.macAddressFromByteArray(bssid));
                } else if (reason == BssidChangeReason.ASSOC_COMPLETE) {
                    mWifiMonitor.broadcastAssociatedBssidEvent(
                            mIfaceName, NativeUtil.macAddressFromByteArray(bssid));
                }
            }
        }

        @Override
        public void onEapFailure() {
            synchronized (mLock) {
                logCallback("onEapFailure");
                mWifiMonitor.broadcastAuthenticationFailureEvent(
                        mIfaceName, WifiManager.ERROR_AUTH_FAILURE_EAP_FAILURE, -1);
            }
        }

        @Override
        public void onWpsEventSuccess() {
            logCallback("onWpsEventSuccess");
            synchronized (mLock) {
                mWifiMonitor.broadcastWpsSuccessEvent(mIfaceName);
            }
        }

        @Override
        public void onWpsEventFail(byte[/* 6 */] bssid, short configError, short errorInd) {
            synchronized (mLock) {
                logCallback("onWpsEventFail");
                if (configError == WpsConfigError.MSG_TIMEOUT
                        && errorInd == WpsErrorIndication.NO_ERROR) {
                    mWifiMonitor.broadcastWpsTimeoutEvent(mIfaceName);
                } else {
                    mWifiMonitor.broadcastWpsFailEvent(mIfaceName, configError, errorInd);
                }
            }
        }

        @Override
        public void onWpsEventPbcOverlap() {
            synchronized (mLock) {
                logCallback("onWpsEventPbcOverlap");
                mWifiMonitor.broadcastWpsOverlapEvent(mIfaceName);
            }
        }

        @Override
        public void onExtRadioWorkStart(int id) {
            synchronized (mLock) {
                logCallback("onExtRadioWorkStart");
            }
        }

        @Override
        public void onExtRadioWorkTimeout(int id) {
            synchronized (mLock) {
                logCallback("onExtRadioWorkTimeout");
            }
        }

        public void updateStateIsFourway(boolean stateIsFourway) {
            synchronized (mLock) {
                logCallback("updateStateIsFourway");
                mStateIsFourway = stateIsFourway;
            }
        }

    }

    private class SupplicantStaIfaceHalCallbackV1_1 extends
            android.hardware.wifi.supplicant.V1_1.ISupplicantStaIfaceCallback.Stub {
        private String mIfaceName;
        private SupplicantStaIfaceHalCallback mCallbackV1_0;

        SupplicantStaIfaceHalCallbackV1_1(@NonNull String ifaceName,
                @NonNull SupplicantStaIfaceHalCallback callback) {
            mIfaceName = ifaceName;
            mCallbackV1_0 = callback;
        }

        @Override
        public void onNetworkAdded(int id) {
            mCallbackV1_0.onNetworkAdded(id);
        }

        @Override
        public void onNetworkRemoved(int id) {
            mCallbackV1_0.onNetworkRemoved(id);
        }

        @Override
        public void onStateChanged(int newState, byte[/* 6 */] bssid, int id,
                                   ArrayList<Byte> ssid) {
            mCallbackV1_0.onStateChanged(newState, bssid, id, ssid);
        }

        @Override
        public void onAnqpQueryDone(byte[/* 6 */] bssid,
                                    ISupplicantStaIfaceCallback.AnqpData data,
                                    ISupplicantStaIfaceCallback.Hs20AnqpData hs20Data) {
            mCallbackV1_0.onAnqpQueryDone(bssid, data, hs20Data);
        }

        @Override
        public void onHs20IconQueryDone(byte[/* 6 */] bssid, String fileName,
                                        ArrayList<Byte> data) {
            mCallbackV1_0.onHs20IconQueryDone(bssid, fileName, data);
        }

        @Override
        public void onHs20SubscriptionRemediation(byte[/* 6 */] bssid,
                                                  byte osuMethod, String url) {
            mCallbackV1_0.onHs20SubscriptionRemediation(bssid, osuMethod, url);
        }

        @Override
        public void onHs20DeauthImminentNotice(byte[/* 6 */] bssid, int reasonCode,
                                               int reAuthDelayInSec, String url) {
            mCallbackV1_0.onHs20DeauthImminentNotice(bssid, reasonCode, reAuthDelayInSec, url);
        }

        @Override
        public void onDisconnected(byte[/* 6 */] bssid, boolean locallyGenerated,
                                   int reasonCode) {
            mCallbackV1_0.onDisconnected(bssid, locallyGenerated, reasonCode);
        }

        @Override
        public void onAssociationRejected(byte[/* 6 */] bssid, int statusCode,
                                          boolean timedOut) {
            mCallbackV1_0.onAssociationRejected(bssid, statusCode, timedOut);
        }

        @Override
        public void onAuthenticationTimeout(byte[/* 6 */] bssid) {
            mCallbackV1_0.onAuthenticationTimeout(bssid);
        }

        @Override
        public void onBssidChanged(byte reason, byte[/* 6 */] bssid) {
            mCallbackV1_0.onBssidChanged(reason, bssid);
        }

        @Override
        public void onEapFailure() {
            mCallbackV1_0.onEapFailure();
        }

        @Override
        public void onEapFailure_1_1(int code) {
            synchronized (mLock) {
                logCallback("onEapFailure_1_1");
                mWifiMonitor.broadcastAuthenticationFailureEvent(
                        mIfaceName, WifiManager.ERROR_AUTH_FAILURE_EAP_FAILURE, code);
            }
        }

        @Override
        public void onWpsEventSuccess() {
            mCallbackV1_0.onWpsEventSuccess();
        }

        @Override
        public void onWpsEventFail(byte[/* 6 */] bssid, short configError, short errorInd) {
            mCallbackV1_0.onWpsEventFail(bssid, configError, errorInd);
        }

        @Override
        public void onWpsEventPbcOverlap() {
            mCallbackV1_0.onWpsEventPbcOverlap();
        }

        @Override
        public void onExtRadioWorkStart(int id) {
            mCallbackV1_0.onExtRadioWorkStart(id);
        }

        @Override
        public void onExtRadioWorkTimeout(int id) {
            mCallbackV1_0.onExtRadioWorkTimeout(id);
        }
    }

    private class SupplicantStaIfaceHalCallbackV1_2 extends
            android.hardware.wifi.supplicant.V1_2.ISupplicantStaIfaceCallback.Stub {
        private SupplicantStaIfaceHalCallbackV1_1 mCallbackV1_1;

        SupplicantStaIfaceHalCallbackV1_2(
                @NonNull SupplicantStaIfaceHalCallbackV1_1 callback) {
            mCallbackV1_1 = callback;
        }

        @Override
        public void onNetworkAdded(int id) {
            mCallbackV1_1.onNetworkAdded(id);
        }

        @Override
        public void onNetworkRemoved(int id) {
            mCallbackV1_1.onNetworkRemoved(id);
        }

        @Override
        public void onStateChanged(int newState, byte[/* 6 */] bssid, int id,
                ArrayList<Byte> ssid) {
            mCallbackV1_1.onStateChanged(newState, bssid, id, ssid);
        }

        @Override
        public void onAnqpQueryDone(byte[/* 6 */] bssid,
                ISupplicantStaIfaceCallback.AnqpData data,
                ISupplicantStaIfaceCallback.Hs20AnqpData hs20Data) {
            mCallbackV1_1.onAnqpQueryDone(bssid, data, hs20Data);
        }

        @Override
        public void onHs20IconQueryDone(byte[/* 6 */] bssid, String fileName,
                ArrayList<Byte> data) {
            mCallbackV1_1.onHs20IconQueryDone(bssid, fileName, data);
        }

        @Override
        public void onHs20SubscriptionRemediation(byte[/* 6 */] bssid,
                byte osuMethod, String url) {
            mCallbackV1_1.onHs20SubscriptionRemediation(bssid, osuMethod, url);
        }

        @Override
        public void onHs20DeauthImminentNotice(byte[/* 6 */] bssid, int reasonCode,
                int reAuthDelayInSec, String url) {
            mCallbackV1_1.onHs20DeauthImminentNotice(bssid, reasonCode, reAuthDelayInSec, url);
        }

        @Override
        public void onDisconnected(byte[/* 6 */] bssid, boolean locallyGenerated,
                int reasonCode) {
            mCallbackV1_1.onDisconnected(bssid, locallyGenerated, reasonCode);
        }

        @Override
        public void onAssociationRejected(byte[/* 6 */] bssid, int statusCode,
                boolean timedOut) {
            mCallbackV1_1.onAssociationRejected(bssid, statusCode, timedOut);
        }

        @Override
        public void onAuthenticationTimeout(byte[/* 6 */] bssid) {
            mCallbackV1_1.onAuthenticationTimeout(bssid);
        }

        @Override
        public void onBssidChanged(byte reason, byte[/* 6 */] bssid) {
            mCallbackV1_1.onBssidChanged(reason, bssid);
        }

        @Override
        public void onEapFailure() {
            mCallbackV1_1.onEapFailure();
        }

        @Override
        public void onEapFailure_1_1(int code) {
            mCallbackV1_1.onEapFailure_1_1(code);
        }

        @Override
        public void onWpsEventSuccess() {
            mCallbackV1_1.onWpsEventSuccess();
        }

        @Override
        public void onWpsEventFail(byte[/* 6 */] bssid, short configError, short errorInd) {
            mCallbackV1_1.onWpsEventFail(bssid, configError, errorInd);
        }

        @Override
        public void onWpsEventPbcOverlap() {
            mCallbackV1_1.onWpsEventPbcOverlap();
        }

        @Override
        public void onExtRadioWorkStart(int id) {
            mCallbackV1_1.onExtRadioWorkStart(id);
        }

        @Override
        public void onExtRadioWorkTimeout(int id) {
            mCallbackV1_1.onExtRadioWorkTimeout(id);
        }

        @Override
        public void onDppSuccessConfigReceived(ArrayList<Byte> ssid, String password,
                byte[] psk, int securityAkm) {
            if (mDppCallback == null) {
                loge("onDppSuccessConfigReceived callback is null");
                return;
            }

            WifiConfiguration newWifiConfiguration = new WifiConfiguration();

            // Set up SSID
            WifiSsid wifiSsid =
                    WifiSsid.createFromByteArray(NativeUtil.byteArrayFromArrayList(ssid));

            newWifiConfiguration.SSID = "\"" + wifiSsid.toString() + "\"";

            // Set up password or PSK
            if (password != null) {
                newWifiConfiguration.preSharedKey = "\"" + password + "\"";
            } else if (psk != null) {
                newWifiConfiguration.preSharedKey = psk.toString();
            }

            // Set up key management: SAE or PSK
            if (securityAkm == DppAkm.SAE || securityAkm == DppAkm.PSK_SAE) {
                newWifiConfiguration.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.SAE);
                newWifiConfiguration.requirePMF = true;
            } else if (securityAkm == DppAkm.PSK) {
                newWifiConfiguration.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.WPA_PSK);
            } else {
                // No other AKMs are currently supported
                onDppFailure(DppFailureCode.NOT_SUPPORTED);
                return;
            }

            // Set up default values
            newWifiConfiguration.creatorName = mContext.getPackageManager()
                    .getNameForUid(Process.WIFI_UID);
            newWifiConfiguration.allowedAuthAlgorithms.set(WifiConfiguration.AuthAlgorithm.OPEN);
            newWifiConfiguration.allowedPairwiseCiphers.set(WifiConfiguration.PairwiseCipher.CCMP);
            newWifiConfiguration.allowedProtocols.set(WifiConfiguration.Protocol.RSN);
            newWifiConfiguration.status = WifiConfiguration.Status.ENABLED;

            mDppCallback.onSuccessConfigReceived(newWifiConfiguration);
        }

        @Override
        public void onDppSuccessConfigSent() {
            if (mDppCallback != null) {
                mDppCallback.onSuccessConfigSent();
            } else {
                loge("onSuccessConfigSent callback is null");
            }
        }

        @Override
        public void onDppProgress(int code) {
            if (mDppCallback != null) {
                mDppCallback.onProgress(code);
            } else {
                loge("onDppProgress callback is null");
            }
        }

        @Override
        public void onDppFailure(int code) {
            if (mDppCallback != null) {
                mDppCallback.onFailure(code);
            } else {
                loge("onDppFailure callback is null");
            }
        }
    }

    private static void logd(String s) {
        Log.d(TAG, s);
    }

    private static void logi(String s) {
        Log.i(TAG, s);
    }

    private static void loge(String s) {
        Log.e(TAG, s);
    }

    /**
     * Returns a bitmask of advanced key management capabilities: WPA3 SAE/SUITE B and OWE
     * Bitmask used is:
     * - WIFI_FEATURE_WPA3_SAE
     * - WIFI_FEATURE_WPA3_SUITE_B
     * - WIFI_FEATURE_OWE
     *
     *  This is a v1.2+ HAL feature.
     *  On error, or if these features are not supported, 0 is returned.
     */
    public long getAdvancedKeyMgmtCapabilities(@NonNull String ifaceName) {
        final String methodStr = "getAdvancedKeyMgmtCapabilities";

        long advancedCapabilities = 0;
        int keyMgmtCapabilities = getKeyMgmtCapabilities(ifaceName);

        if ((keyMgmtCapabilities & android.hardware.wifi.supplicant.V1_2.ISupplicantStaNetwork
                .KeyMgmtMask.SAE) != 0) {
            advancedCapabilities |= WIFI_FEATURE_WPA3_SAE;

            if (mVerboseLoggingEnabled) {
                Log.v(TAG, methodStr + ": SAE supported");
            }
        }

        if ((keyMgmtCapabilities & android.hardware.wifi.supplicant.V1_2.ISupplicantStaNetwork
                .KeyMgmtMask.SUITE_B_192) != 0) {
            advancedCapabilities |= WIFI_FEATURE_WPA3_SUITE_B;

            if (mVerboseLoggingEnabled) {
                Log.v(TAG, methodStr + ": SUITE_B supported");
            }
        }

        if ((keyMgmtCapabilities & android.hardware.wifi.supplicant.V1_2.ISupplicantStaNetwork
                .KeyMgmtMask.OWE) != 0) {
            advancedCapabilities |= WIFI_FEATURE_OWE;

            if (mVerboseLoggingEnabled) {
                Log.v(TAG, methodStr + ": OWE supported");
            }
        }

        if ((keyMgmtCapabilities & android.hardware.wifi.supplicant.V1_2.ISupplicantStaNetwork
                .KeyMgmtMask.DPP) != 0) {
            advancedCapabilities |= WIFI_FEATURE_DPP;

            if (mVerboseLoggingEnabled) {
                Log.v(TAG, methodStr + ": DPP supported");
            }
        }

        if (mVerboseLoggingEnabled) {
            Log.v(TAG, methodStr + ": Capability flags = " + keyMgmtCapabilities);
        }

        return advancedCapabilities;
    }

    private int getKeyMgmtCapabilities(@NonNull String ifaceName) {
        final String methodStr = "getKeyMgmtCapabilities";
        MutableBoolean status = new MutableBoolean(false);
        MutableInt keyMgmtMask = new MutableInt(0);

        if (isV1_2()) {
            ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) {
                return 0;
            }

            // Get a v1.2 supplicant STA Interface
            android.hardware.wifi.supplicant.V1_2.ISupplicantStaIface staIfaceV12 =
                    getStaIfaceMockableV1_2(iface);

            if (staIfaceV12 == null) {
                Log.e(TAG, methodStr
                        + ": ISupplicantStaIface is null, cannot get advanced capabilities");
                return 0;
            }

            try {
                // Support for new key management types; SAE, SUITE_B, OWE
                // Requires HAL v1.2 or higher
                staIfaceV12.getKeyMgmtCapabilities(
                        (SupplicantStatus statusInternal, int keyMgmtMaskInternal) -> {
                            status.value = statusInternal.code == SupplicantStatusCode.SUCCESS;
                            if (status.value) {
                                keyMgmtMask.value = keyMgmtMaskInternal;
                            }
                            checkStatusAndLogFailure(statusInternal, methodStr);
                        });
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
            }
        } else {
            Log.e(TAG, "Method " + methodStr + " is not supported in existing HAL");
        }

        // 0 is returned in case of an error
        return keyMgmtMask.value;
    }

    /**
     * Add the DPP bootstrap info obtained from QR code.
     *
     * @param ifaceName Name of the interface.
     * @param uri:The URI obtained from the QR code.
     *
     * @return: Handle to strored info else -1 on failure
     */
    public int dppAddBootstrapQrCode(@NonNull String ifaceName, String uri) {
        if (TextUtils.isEmpty(uri)) return -1;
        synchronized (mLock) {
            final String methodStr = "dppAddBootstrapQrCode";
            final MutableInt handle = new MutableInt(-1);
            ISupplicantVendorStaIface iface =
                   checkSupplicantVendorStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return -1;
            try {
                iface.dppAddBootstrapQrcode(uri,
                        (SupplicantStatus status, int hdl) -> {
                            if (checkVendorStatusAndLogFailure(status, methodStr)) {
                                handle.value = hdl;
                            }
                        });
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
            }
            return handle.value;
        }
    }

    /**
     * Generate bootstrap URI based on the passed arguments
     *
     * @param ifaceName Name of the interface.
     * @param config – bootstrap generate config
     *
     * @return: Handle to strored URI info else -1 on failure
     */
    public int dppBootstrapGenerate(@NonNull String ifaceName, WifiDppConfig config) {
        synchronized (mLock) {
            final String methodStr = "dppBootstrapGenerate";
            final MutableInt handle = new MutableInt(-1);

            String chan_list = (TextUtils.isEmpty(config.chan_list)) ? "" : config.chan_list;
            String mac_addr = (TextUtils.isEmpty(config.mac_addr)) ? "00:00:00:00:00:00" : config.mac_addr;
            String info = (TextUtils.isEmpty(config.info)) ? "" : config.info;
            String curve = (TextUtils.isEmpty(config.curve)) ? "" : config.curve;
            String key = (TextUtils.isEmpty(config.key)) ? "" : config.key;

            ISupplicantVendorStaIface iface =
                   checkSupplicantVendorStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return -1;
            try {
                iface.dppBootstrapGenerate(config.bootstrap_type,
                            chan_list, NativeUtil.macAddressToByteArray(mac_addr),
                            info, curve, key,
                        (SupplicantStatus status, int hdl) -> {
                            if (checkVendorStatusAndLogFailure(status, methodStr)) {
                                handle.value = hdl;
                            }
                        });
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
            }
            return handle.value;
        }
    }

    /**
     * Get bootstrap URI based on bootstrap ID
     *
     * @param ifaceName Name of the interface.
     * @param bootstrap_id: Stored bootstrap ID
     *
     * @return: URI string else -1 on failure
     */
    public String dppGetUri(@NonNull String ifaceName, int bootstrap_id) {
        synchronized (mLock) {
            final String methodStr = "dppGetUri";
            final Mutable<String> URI = new Mutable<>();
            ISupplicantVendorStaIface iface =
                   checkSupplicantVendorStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return "-1";
            try {
                iface.dppGetUri(bootstrap_id,
                        (SupplicantStatus status, String uri) -> {
                            if (checkVendorStatusAndLogFailure(status, methodStr)) {
                                URI.value = uri;
                            }
                        });
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
            }
            return URI.value;
        }
    }

    /**
     * Remove bootstrap URI based on bootstrap ID.
     *
     * @param ifaceName Name of the interface.
     * @param bootstrap_id: Stored bootstrap ID
     *
     * @return: 0 – Success or -1 on failure
     */
    public int dppBootstrapRemove(@NonNull String ifaceName, int bootstrap_id) {
        synchronized (mLock) {
            final String methodStr = "dppBootstrapRemove";
            final MutableInt handle = new MutableInt(-1);
            ISupplicantVendorStaIface iface =
                   checkSupplicantVendorStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return -1;
            try {
                iface.dppBootstrapRemove(bootstrap_id,
                        (SupplicantStatus status, int hdl) -> {
                            if (checkVendorStatusAndLogFailure(status, methodStr)) {
                                handle.value = hdl;
                            }
                        });
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
            }
            return handle.value;
        }
    }

    /**
     * start listen on the channel specified waiting to receive
     * the DPP Authentication request.
     *
     * @param ifaceName Name of the interface.
     * @param frequency: DPP listen frequency
     * @param dpp_role: Configurator/Enrollee role
     * @param qr_mutual: Mutual authentication required
     * @param netrole_ap: network role
     *
     * @return: Returns 0 if a DPP-listen work is successfully
     *  queued and -1 on failure.
     */
    public int dppListen(@NonNull String ifaceName, String frequency, int dpp_role,
                         boolean qr_mutual, boolean netrole_ap) {
        if (TextUtils.isEmpty(frequency)) return -1;
        synchronized (mLock) {
            final String methodStr = "dppListen";
            final MutableInt handle = new MutableInt(-1);
            ISupplicantVendorStaIface iface =
                   checkSupplicantVendorStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return -1;
            try {
                iface.dppStartListen(frequency, dpp_role,
                        qr_mutual, netrole_ap,
                        (SupplicantStatus status, int hdl) -> {
                            if (checkVendorStatusAndLogFailure(status, methodStr)) {
                                handle.value = hdl;
                            }
                        });
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
            }
            return handle.value;
        }
    }

    /**
     * stop ongoing dpp listen.
     *
     * @param ifaceName Name of the interface.
     * @return true if request is sent successfully, false otherwise.
     */
    public boolean dppStopListen(@NonNull String ifaceName) {
        synchronized (mLock) {
            final String methodStr = "dppStopListen";
            ISupplicantVendorStaIface iface =
                   checkSupplicantVendorStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return false;
            try {
                SupplicantStatus status = iface.dppStopListen();
                return checkVendorStatusAndLogFailure(status, methodStr);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
        }
    }

    /**
     * Adds the DPP configurator
     *
     * @param ifaceName Name of the interface.
     * @param curve curve used for dpp encryption
     * @param key private key
     * @param expiry timeout in seconds
     *
     * @return: Identifier of the added configurator or -1 on failure
     */
    public int dppConfiguratorAdd(@NonNull String ifaceName, String curve,
                                  String key, int expiry) {
        String curve_t = (TextUtils.isEmpty(curve)) ? "" : curve;
        String key_t = (TextUtils.isEmpty(key)) ? "" : key;
        synchronized (mLock) {
            final String methodStr = "dppConfiguratorAdd";
            final MutableInt handle = new MutableInt(-1);
            ISupplicantVendorStaIface iface =
                   checkSupplicantVendorStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return -1;
            try {
                iface.dppConfiguratorAdd(curve_t, key_t, expiry,
                        (SupplicantStatus status, int hdl) -> {
                            if (checkVendorStatusAndLogFailure(status, methodStr)) {
                                handle.value = hdl;
                            }
                        });
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
            }
            return handle.value;
        }
    }

    /**
     * Remove the added configurator through dppConfiguratorAdd.
     *
     * @param ifaceName Name of the interface.
     * @param config_id: DPP Configurator ID
     *
     * @return: Handle to strored info else -1 on failure
     */
    public int dppConfiguratorRemove(@NonNull String ifaceName, int config_id) {
        synchronized (mLock) {
            final String methodStr = "dppConfiguratorRemove";
            final MutableInt handle = new MutableInt(-1);
            ISupplicantVendorStaIface iface =
                   checkSupplicantVendorStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return -1;
            try {
                iface.dppConfiguratorRemove(config_id,
                        (SupplicantStatus status, int hdl) -> {
                            if (checkVendorStatusAndLogFailure(status, methodStr)) {
                                handle.value = hdl;
                            }
                        });
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
            }
            return handle.value;
        }
    }

    /**
     * Start DPP authentication and provisioning with the specified peer
     *
     * @param ifaceName Name of the interface.
     * @param config – dpp auth init config
     *
     * @return: 0 if DPP Authentication request was transmitted and -1 on failure
     */
    public int  dppStartAuth(@NonNull String ifaceName, WifiDppConfig config) {
        String ssid = (TextUtils.isEmpty(config.ssid)) ? "" : config.ssid;
        String passphrase = (TextUtils.isEmpty(config.passphrase)) ? "" : config.passphrase;
        synchronized (mLock) {
            final String methodStr = "dppStartAuth";
            final MutableInt Status = new MutableInt(-1);
            ISupplicantVendorStaIface iface =
                   checkSupplicantVendorStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return -1;
            try {
                iface.dppStartAuth(config.peer_bootstrap_id,
                            config.own_bootstrap_id, config.dpp_role,
                            ssid, passphrase, (config.isAp > 0) ? true : false,
                            (config.isDpp > 0) ? true: false, config.conf_id, config.expiry,
                        (SupplicantStatus status, int hdl) -> {
                            if (checkVendorStatusAndLogFailure(status, methodStr)) {
                                Status.value = hdl;
                            }
                        });
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
            }
            return Status.value;
        }
    }

    /**
     * Retrieve Private key to be used for configurator
     *
     * @param ifaceName Name of the interface.
     * @param id: id of configurator obj
     *
     * @return: Key string else -1 on failure
     */
    public String dppConfiguratorGetKey(@NonNull String ifaceName, int id) {
        synchronized (mLock) {
            final String methodStr = "dppConfiguratorGetKey";
            final Mutable<String> KEY = new Mutable<>();
            ISupplicantVendorStaIface iface =
                   checkSupplicantVendorStaIfaceAndLogFailure(ifaceName, methodStr);
            if (iface == null) return "-1";
            try {
                iface.dppConfiguratorGetKey(id,
                        (SupplicantStatus status, String key) -> {
                            if (checkVendorStatusAndLogFailure(status, methodStr)) {
                                KEY.value = key;
                            }
                        });
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
            }
            return KEY.value;
        }
    }

    /*
     * Adds a DPP peer URI to the URI list.
     *
     *  This is a v1.2+ HAL feature.
     *  Returns an ID to be used later to refer to this URI (>0).
     *  On error, or if these features are not supported, -1 is returned.
     */
    public int addDppPeerUri(@NonNull String ifaceName, @NonNull String uri) {
        final String methodStr = "addDppPeerUri";
        MutableBoolean status = new MutableBoolean(false);
        MutableInt bootstrapId = new MutableInt(-1);

        if (!isV1_2()) {
            Log.e(TAG, "Method " + methodStr + " is not supported in existing HAL");
            return -1;
        }

        ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
        if (iface == null) {
            return -1;
        }

        // Get a v1.2 supplicant STA Interface
        android.hardware.wifi.supplicant.V1_2.ISupplicantStaIface staIfaceV12 =
                getStaIfaceMockableV1_2(iface);

        if (staIfaceV12 == null) {
            Log.e(TAG, methodStr + ": ISupplicantStaIface is null");
            return -1;
        }

        try {
            // Support for DPP (Easy connect)
            // Requires HAL v1.2 or higher
            staIfaceV12.addDppPeerUri(uri,
                    (SupplicantStatus statusInternal, int bootstrapIdInternal) -> {
                        status.value = statusInternal.code == SupplicantStatusCode.SUCCESS;
                        if (status.value) {
                            bootstrapId.value = bootstrapIdInternal;
                        }
                        checkStatusAndLogFailure(statusInternal, methodStr);
                    });
        } catch (RemoteException e) {
            handleRemoteException(e, methodStr);
            return -1;
        }

        return bootstrapId.value;
    }

    /**
     * Removes a DPP URI to the URI list given an ID.
     *
     *  This is a v1.2+ HAL feature.
     *  Returns true when operation is successful
     *  On error, or if these features are not supported, false is returned.
     */
    public boolean removeDppUri(@NonNull String ifaceName, int bootstrapId)  {
        final String methodStr = "removeDppUri";

        if (!isV1_2()) {
            Log.e(TAG, "Method " + methodStr + " is not supported in existing HAL");
            return false;
        }

        ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
        if (iface == null) {
            return false;
        }

        // Get a v1.2 supplicant STA Interface
        android.hardware.wifi.supplicant.V1_2.ISupplicantStaIface staIfaceV12 =
                getStaIfaceMockableV1_2(iface);

        if (staIfaceV12 == null) {
            Log.e(TAG, methodStr + ": ISupplicantStaIface is null");
            return false;
        }

        try {
            // Support for DPP (Easy connect)
            // Requires HAL v1.2 or higher
            SupplicantStatus status = staIfaceV12.removeDppUri(bootstrapId);
            return checkStatusAndLogFailure(status, methodStr);
        } catch (RemoteException e) {
            handleRemoteException(e, methodStr);
        }

        return false;
    }

    /**
     * Stops/aborts DPP Initiator request
     *
     *  This is a v1.2+ HAL feature.
     *  Returns true when operation is successful
     *  On error, or if these features are not supported, false is returned.
     */
    public boolean stopDppInitiator(@NonNull String ifaceName)  {
        final String methodStr = "stopDppInitiator";

        if (!isV1_2()) {
            return false;
        }

        ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
        if (iface == null) {
            return false;
        }

        // Get a v1.2 supplicant STA Interface
        android.hardware.wifi.supplicant.V1_2.ISupplicantStaIface staIfaceV12 =
                getStaIfaceMockableV1_2(iface);

        if (staIfaceV12 == null) {
            Log.e(TAG, methodStr + ": ISupplicantStaIface is null");
            return false;
        }

        try {
            // Support for DPP (Easy connect)
            // Requires HAL v1.2 or higher
            SupplicantStatus status = staIfaceV12.stopDppInitiator();
            return checkStatusAndLogFailure(status, methodStr);
        } catch (RemoteException e) {
            handleRemoteException(e, methodStr);
        }

        return false;
    }

    /**
     * Starts DPP Configurator-Initiator request
     *
     *  This is a v1.2+ HAL feature.
     *  Returns true when operation is successful
     *  On error, or if these features are not supported, false is returned.
     */
    public boolean startDppConfiguratorInitiator(@NonNull String ifaceName, int peerBootstrapId,
            int ownBootstrapId, @NonNull String ssid, String password, String psk,
            int netRole, int securityAkm)  {
        final String methodStr = "startDppConfiguratorInitiator";

        if (!isV1_2()) {
            Log.e(TAG, "Method " + methodStr + " is not supported in existing HAL");
            return false;
        }

        ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
        if (iface == null) {
            return false;
        }

        // Get a v1.2 supplicant STA Interface
        android.hardware.wifi.supplicant.V1_2.ISupplicantStaIface staIfaceV12 =
                getStaIfaceMockableV1_2(iface);

        if (staIfaceV12 == null) {
            Log.e(TAG, methodStr + ": ISupplicantStaIface is null");
            return false;
        }

        try {
            // Support for DPP (Easy connect)
            // Requires HAL v1.2 or higher
            SupplicantStatus status = staIfaceV12.startDppConfiguratorInitiator(peerBootstrapId,
                    ownBootstrapId, ssid, password != null ? password : "", psk != null ? psk : "",
                    netRole, securityAkm);
            return checkStatusAndLogFailure(status, methodStr);
        } catch (RemoteException e) {
            handleRemoteException(e, methodStr);
        }

        return false;
    }

    /**
     * Starts DPP Enrollee-Initiator request
     *
     *  This is a v1.2+ HAL feature.
     *  Returns true when operation is successful
     *  On error, or if these features are not supported, false is returned.
     */
    public boolean startDppEnrolleeInitiator(@NonNull String ifaceName, int peerBootstrapId,
            int ownBootstrapId)  {
        final String methodStr = "startDppEnrolleeInitiator";

        if (!isV1_2()) {
            Log.e(TAG, "Method " + methodStr + " is not supported in existing HAL");
            return false;
        }

        ISupplicantStaIface iface = checkSupplicantStaIfaceAndLogFailure(ifaceName, methodStr);
        if (iface == null) {
            return false;
        }

        // Get a v1.2 supplicant STA Interface
        android.hardware.wifi.supplicant.V1_2.ISupplicantStaIface staIfaceV12 =
                getStaIfaceMockableV1_2(iface);

        if (staIfaceV12 == null) {
            Log.e(TAG, methodStr + ": ISupplicantStaIface is null");
            return false;
        }

        try {
            // Support for DPP (Easy connect)
            // Requires HAL v1.2 or higher
            SupplicantStatus status = staIfaceV12.startDppEnrolleeInitiator(peerBootstrapId,
                    ownBootstrapId);
            return checkStatusAndLogFailure(status, methodStr);
        } catch (RemoteException e) {
            handleRemoteException(e, methodStr);
        }

        return false;
    }

    /**
     * Register callbacks for DPP events.
     *
     * @param dppCallback DPP callback object.
     */
    public void registerDppCallback(DppEventCallback dppCallback) {
        mDppCallback = dppCallback;
    }

    protected vendor.qti.hardware.wifi.supplicant.V2_1.ISupplicantVendorStaIface
    getSupplicantVendorStaIfaceV2_1Mockable(ISupplicantVendorStaIface vendorIfaceV2_0) {
        if (vendorIfaceV2_0 == null) return null;
        return vendor.qti.hardware.wifi.supplicant.V2_1.ISupplicantVendorStaIface.castFrom(
                vendorIfaceV2_0);
    }

    public WifiNative.WifiGenerationStatus getWifiGenerationStatus(@NonNull String ifaceName) {
        synchronized (mLock) {
            final String methodStr = "getWifiGenerationStatus";
            final Mutable<WifiNative.WifiGenerationStatus> STATUS = new Mutable<>();
            ISupplicantVendorStaIface vendorIfaceV2_0 = getVendorStaIface(ifaceName);
            if (vendorIfaceV2_0 == null) {
                Log.e(TAG, "Can't call " + methodStr + ", ISupplicantVendorStaIface is null");
                return null;
            }

            vendor.qti.hardware.wifi.supplicant.V2_1.ISupplicantVendorStaIface vendorIfaceV2_1;
            vendorIfaceV2_1 = getSupplicantVendorStaIfaceV2_1Mockable(vendorIfaceV2_0);
            if (vendorIfaceV2_1 == null) {
                Log.e(TAG, "Can't call " + methodStr + ", V2_1.ISupplicantVendorStaIface is null");
                return null;
            }

            try {
                vendorIfaceV2_1.getWifiGenerationStatus((SupplicantStatus status, WifiGenerationStatus generationStatus) -> {
                            if (checkVendorStatusAndLogFailure(status, methodStr)) {
                                STATUS.value = new WifiNative.WifiGenerationStatus(generationStatus.generation,
                                                                                   generationStatus.vhtMax8SpatialStreamsSupport,
                                                                                   generationStatus.twtSupport);
                            }
                        });
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
            }
            return STATUS.value;
        }
    }

    public boolean updateLinkedNetworksIfCurrent(@NonNull String ifaceName, int networkId,
                           HashMap<String, WifiConfiguration> linkedConfigurations) {
        synchronized (mLock) {
            WifiConfiguration currentConfig = getCurrentNetworkLocalConfig(ifaceName);
            SupplicantStaNetworkHal currentHandle = getCurrentNetworkRemoteHandle(ifaceName);

            if (currentConfig == null || currentHandle == null) {
                Log.e(TAG, "updateLinkedNetworksIfCurrent failed, got null current network");
                return false;
            }

            if (currentConfig.networkId != networkId) {
                Log.e(TAG, "updateLinkedNetworksIfCurrent failed, current network id is not matched");
                return false;
            }

            if (!currentHandle.getId()) {
                Log.e(TAG, "updateLinkedNetworksIfCurrent getId failed");
                return false;
            }

            if (!removeLinkedNetworks(ifaceName, currentHandle.getNetworkId())) {
                Log.e(TAG, "updateLinkedNetworksIfCurrent failed, couldn't remove existing linked networks");
                return false;
            }

            mLinkedNetworkLocalAndRemoteConfigs.remove(ifaceName);

            if (linkedConfigurations == null || linkedConfigurations.size() == 0) {
                Log.e(TAG, "received zero linked networks");
                return true;
            }

            ArrayList<Pair<SupplicantStaNetworkHal, WifiConfiguration>> linkedNetworkHandles
                = new ArrayList<Pair<SupplicantStaNetworkHal, WifiConfiguration>>();

            linkedNetworkHandles.add(new Pair(mCurrentNetworkRemoteHandles.get(ifaceName),
                                              mCurrentNetworkLocalConfigs.get(ifaceName)));
            for (String linkedNetwork : linkedConfigurations.keySet()) {
                Log.e(TAG, "updateLinkedNetworksIfCurrent: add linked network: " + linkedNetwork);
                Pair<SupplicantStaNetworkHal, WifiConfiguration> pair =
                        addNetworkAndSaveConfig(ifaceName, linkedConfigurations.get(linkedNetwork));
                if (pair == null) {
                    Log.e(TAG, "updateLinkedNetworksIfCurrent failed to add/save linked network: "
                             + linkedNetwork);
                    return false;
                }
                pair.first.enable(true);
                linkedNetworkHandles.add(pair);
            }

            mLinkedNetworkLocalAndRemoteConfigs.put(ifaceName, linkedNetworkHandles);

            return true;
        }
    }

    /**
     * Remove linked networks from supplicant
     *
     * @param ifaceName Name of the interface.
     */
    private boolean removeLinkedNetworks(@NonNull String ifaceName, int currentNetworkId) {
        synchronized (mLock) {
            ArrayList<Integer> networks = listNetworks(ifaceName);
            if (networks == null) {
                Log.e(TAG, "removeLinkedNetworks failed, got null networks");
                return false;
            }
            for (int id : networks) {
                if (currentNetworkId == id) continue;
                if (!removeNetwork(ifaceName, id)) {
                    Log.e(TAG, "removeLinkedNetworks failed to remove network: " + id);
                    return false;
                }
            }
            return true;
        }
    }

    protected vendor.qti.hardware.wifi.supplicant.V2_2.ISupplicantVendorStaIface
        getSupplicantVendorStaIfaceV2_2Mockable(ISupplicantVendorStaIface vendorIfaceV2_0) {
        if (vendorIfaceV2_0 == null) return null;
        return vendor.qti.hardware.wifi.supplicant.V2_2.ISupplicantVendorStaIface.castFrom(
                vendorIfaceV2_0);
    }

    /**
     * run Driver command
     *
     * @param ifaceName Interface Name
     * @param command Driver Command
     */
    public String doDriverCmd(String ifaceName, String command)
    {
        synchronized (mLock) {
            final String methodStr = "doDriverCmd";
            final Mutable<String> reply = new Mutable<>();

            reply.value = "";
            ISupplicantVendorStaIface vendorIfaceV2_0 = getVendorStaIface(ifaceName);
            if (vendorIfaceV2_0 == null) {
                Log.e(TAG, "Can't call " + methodStr + ", ISupplicantVendorStaIface is null");
                return null;
            }

            vendor.qti.hardware.wifi.supplicant.V2_2.ISupplicantVendorStaIface vendorIfaceV2_2;
            vendorIfaceV2_2 = getSupplicantVendorStaIfaceV2_2Mockable(vendorIfaceV2_0);
            if (vendorIfaceV2_2 == null) {
                Log.e(TAG, "Can't call " + methodStr + ", V2_2.ISupplicantVendorStaIface is null");
                return null;
            }

            try {
                vendorIfaceV2_2.doDriverCmd(command,
                        (SupplicantStatus status, String rply) -> {
                        if(checkVendorStatusAndLogFailure(status, methodStr)) {
                            reply.value = rply;
                     }
                });
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
            }
            return reply.value;
         }
    }
}
