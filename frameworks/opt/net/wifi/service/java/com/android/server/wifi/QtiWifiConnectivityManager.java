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

import static android.net.wifi.WifiConfiguration.NetworkSelectionStatus.DISABLED_NO_INTERNET_PERMANENT;
import static android.net.wifi.WifiConfiguration.NetworkSelectionStatus.DISABLED_NO_INTERNET_TEMPORARY;

import static com.android.internal.util.Preconditions.checkNotNull;
import static com.android.server.wifi.ClientModeImpl.WIFI_WORK_SOURCE;

import android.app.AlarmManager;
import android.content.Context;
import android.net.wifi.ScanResult;
import android.net.wifi.SupplicantState;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiManager.DeviceMobilityState;
import android.net.wifi.WifiScanner;
import android.net.wifi.p2p.WifiP2pManager;
import android.net.wifi.WifiScanner.PnoSettings;
import android.net.wifi.WifiScanner.ScanSettings;
import android.os.Handler;
import android.os.Looper;
import android.os.Process;
import android.os.WorkSource;
import android.util.LocalLog;
import android.util.Log;

import com.android.internal.R;
import com.android.internal.annotations.VisibleForTesting;
import com.android.server.wifi.util.ScanResultUtil;

import java.io.FileDescriptor;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * This class manages auto connection based on scan results.
 *
 * When the screen is turned on or off, WiFi is connected or disconnected,
 * or on-demand, a scan is initiatiated and the scan results are passed
 * to WifiNetworkSelector for it to make a recommendation on which network
 * to connect to.
 */
public class QtiWifiConnectivityManager {
    private static final long RESET_TIME_STAMP = Long.MIN_VALUE;
    // This is the time interval for the connection attempt rate calculation. Connection attempt
    // timestamps beyond this interval is evicted from the list.
    public static final int MAX_CONNECTION_ATTEMPTS_TIME_INTERVAL_MS = 4 * 60 * 1000; // 4 mins
    // Max number of connection attempts in the above time interval.
    public static final int MAX_CONNECTION_ATTEMPTS_RATE = 6;

    // ClientModeImpl has a bunch of states. From the
    // QtiWifiConnectivityManager's perspective it only cares
    // if it is in Connected state, Disconnected state or in
    // transition between these two states.
    public static final int WIFI_STATE_UNKNOWN = 0;
    public static final int WIFI_STATE_CONNECTED = 1;
    public static final int WIFI_STATE_DISCONNECTED = 2;
    public static final int WIFI_STATE_TRANSITIONING = 3;

    // Log tag for this class
    private static final String TAG = "QtiWifiConnectivityManager";

    private final QtiClientModeImpl mStateMachine;
    private final WifiInjector mWifiInjector;
    private final WifiConfigManager mConfigManager;
    private final WifiInfo mWifiInfo;
    private final WifiConnectivityHelper mConnectivityHelper;
    private final WifiNetworkSelector mNetworkSelector;
    private final Handler mEventHandler;
    private final Clock mClock;
    private final ScoringParams mScoringParams;
    private final LocalLog mLocalLog;
    private final LinkedList<Long> mConnectionAttemptTimeStamps;
    private WifiScanner mScanner;

    private boolean mDbg = false;
    private boolean DBG = false;
    private boolean mWifiEnabled = false;
    private boolean mRunning = false;
    private boolean mScreenOn = false;
    private int mMiracastMode = WifiP2pManager.MIRACAST_DISABLED;
    private int mWifiState = WIFI_STATE_UNKNOWN;
    private int mTotalConnectivityAttemptsRateLimited = 0;
    private String mLastConnectionAttemptBssid = null;
    // Device configs
    private boolean mWaitForFullBandScanResults = false;
    private final int mStaId;

    // BSSID blacklist
    @VisibleForTesting
    public static final int BSSID_BLACKLIST_THRESHOLD = 3;
    @VisibleForTesting
    public static final int BSSID_BLACKLIST_EXPIRE_TIME_MS = 5 * 60 * 1000;
    private static class BssidBlacklistStatus {
        // Number of times this BSSID has been rejected for association.
        public int counter;
        public boolean isBlacklisted;
        public long blacklistedTimeStamp = RESET_TIME_STAMP;
    }
    private Map<String, BssidBlacklistStatus> mBssidBlacklist =
            new HashMap<>();

    // Association failure reason codes
    @VisibleForTesting
    public static final int REASON_CODE_AP_UNABLE_TO_HANDLE_NEW_STA = 17;

    // A helper to log debugging information in the local log buffer, which can
    // be retrieved in bugreport.
    private void localLog(String log) {
        mLocalLog.log(log);
        if (DBG)
            Log.d(TAG, log);
    }

    /**
     * Enable verbose logging QtiWifiConnectivityManager.
     */
    public void enableVerboseLogging(int verbose) {
        if (verbose > 0) {
            DBG = true;
        } else {
            DBG = false;
        }
    }

    /**
     * Handles 'onResult' callbacks for the Periodic, Single & Pno ScanListener.
     * Executes selection of potential network candidates, initiation of connection attempt to that
     * network.
     *
     * @return true - if a candidate is selected by WifiNetworkSelector
     *         false - if no candidate is selected by WifiNetworkSelector
     */
    private boolean handleScanResults(List<ScanDetail> scanDetails, String listenerName) {
        // Check if any blacklisted BSSIDs can be freed.
        refreshBssidBlacklist();

        if (mStateMachine.isSupplicantTransientState()) {
            localLog(listenerName
                    + " onResults: No network selection because supplicantTransientState is "
                    + mStateMachine.isSupplicantTransientState());
            return false;
        }

        localLog(listenerName + " onResults: start network selection");

        List<ScanDetail> filteredScans = mStateMachine.qtiGetFilteredScan(scanDetails);

        WifiConfiguration candidate =
                mNetworkSelector.selectNetwork(filteredScans, buildBssidBlacklist(), mWifiInfo,
                mStateMachine.isConnected(), mStateMachine.isDisconnected(),
                true);
        if (candidate != null) {
            localLog(listenerName + ":  WNS candidate-" + candidate.SSID);
            connectToNetwork(candidate);
            return true;
        } else {
            return false;
        }
    }

    // All single scan results listener.
    //
    // Note: This is the listener for all the available single scan results,
    //       including the ones initiated by QtiWifiConnectivityManager and
    //       other modules.
    private class AllSingleScanListener implements WifiScanner.ScanListener {
        private List<ScanDetail> mScanDetails = new ArrayList<ScanDetail>();

        public void clearScanDetails() {
            mScanDetails.clear();
        }

        @Override
        public void onSuccess() {
        }

        @Override
        public void onFailure(int reason, String description) {
            localLog("registerScanListener onFailure:"
                      + " reason: " + reason + " description: " + description);
        }

        @Override
        public void onPeriodChanged(int periodInMs) {
        }

        @Override
        public void onResults(WifiScanner.ScanData[] results) {
            if (!mWifiEnabled) {
                clearScanDetails();
                mWaitForFullBandScanResults = false;
                return;
            }

            // We treat any full band scans (with DFS or not) as "full".
            boolean isFullBandScanResults =
                    results[0].getBandScanned() == WifiScanner.WIFI_BAND_BOTH_WITH_DFS
                            || results[0].getBandScanned() == WifiScanner.WIFI_BAND_BOTH;
            // Full band scan results only.
            if (mWaitForFullBandScanResults) {
                if (!isFullBandScanResults) {
                    localLog("AllSingleScanListener waiting for full band scan results.");
                    clearScanDetails();
                    return;
                } else {
                    mWaitForFullBandScanResults = false;
                }
            }
            boolean wasConnectAttempted = handleScanResults(mScanDetails, "AllSingleScanListener");
            clearScanDetails();
        }

        @Override
        public void onFullResult(ScanResult fullScanResult) {
            if (!mWifiEnabled) {
                return;
            }

            if (mDbg) {
                localLog("AllSingleScanListener onFullResult: " + fullScanResult.SSID
                        + " capabilities " + fullScanResult.capabilities);
            }

            mScanDetails.add(ScanResultUtil.toScanDetail(fullScanResult));
        }
    }

    private final AllSingleScanListener mAllSingleScanListener = new AllSingleScanListener();

    /**
     * QtiWifiConnectivityManager constructor
     */
    QtiWifiConnectivityManager(Context context, ScoringParams scoringParams,
            QtiClientModeImpl stateMachine,
            WifiInjector injector, WifiConfigManager configManager,
            WifiNetworkSelector networkSelector, WifiConnectivityHelper connectivityHelper,
            Looper looper, Clock clock, LocalLog localLog, int identity) {
        mStateMachine = stateMachine;
        mWifiInjector = injector;
        mConfigManager = configManager;
        mWifiInfo = mStateMachine.getWifiInfo();
        mNetworkSelector = networkSelector;
        mConnectivityHelper = connectivityHelper;
        mLocalLog = localLog;
        mEventHandler = new Handler(looper);
        mClock = clock;
        mScoringParams = scoringParams;
        mConnectionAttemptTimeStamps = new LinkedList<>();
        mStaId = identity;
    }

    /**
     * This checks the connection attempt rate and recommends whether the connection attempt
     * should be skipped or not. This attempts to rate limit the rate of connections to
     * prevent us from flapping between networks and draining battery rapidly.
     */
    private boolean shouldSkipConnectionAttempt(Long timeMillis) {
        Iterator<Long> attemptIter = mConnectionAttemptTimeStamps.iterator();
        // First evict old entries from the queue.
        while (attemptIter.hasNext()) {
            Long connectionAttemptTimeMillis = attemptIter.next();
            if ((timeMillis - connectionAttemptTimeMillis)
                    > MAX_CONNECTION_ATTEMPTS_TIME_INTERVAL_MS) {
                attemptIter.remove();
            } else {
                // This list is sorted by timestamps, so we can skip any more checks
                break;
            }
        }
        // If we've reached the max connection attempt rate, skip this connection attempt
        return (mConnectionAttemptTimeStamps.size() >= MAX_CONNECTION_ATTEMPTS_RATE);
    }

    /**
     * Add the current connection attempt timestamp to our queue of connection attempts.
     */
    private void noteConnectionAttempt(Long timeMillis) {
        mConnectionAttemptTimeStamps.addLast(timeMillis);
    }

    /**
     * This is used to clear the connection attempt rate limiter. This is done when the user
     * explicitly tries to connect to a specified network.
     */
    private void clearConnectionAttemptTimeStamps() {
        mConnectionAttemptTimeStamps.clear();
    }

    /**
     * Attempt to connect to a network candidate.
     *
     * Based on the currently connected network, this menthod determines whether we should
     * connect or roam to the network candidate recommended by WifiNetworkSelector.
     */
    private void connectToNetwork(WifiConfiguration candidate) {
        ScanResult scanResultCandidate = candidate.getNetworkSelectionStatus().getCandidate();
        if (scanResultCandidate == null) {
            localLog("connectToNetwork: bad candidate - "  + candidate
                    + " scanResult: " + scanResultCandidate);
            return;
        }

        String targetBssid = scanResultCandidate.BSSID;
        String targetAssociationId = candidate.SSID + " : " + targetBssid;

        // Check if we are already connected or in the process of connecting to the target
        // BSSID. mWifiInfo.mBSSID tracks the currently connected BSSID. This is checked just
        // in case the firmware automatically roamed to a BSSID different from what
        // WifiNetworkSelector selected.
        if (targetBssid != null
                && (targetBssid.equals(mLastConnectionAttemptBssid)
                    || targetBssid.equals(mWifiInfo.getBSSID()))
                && SupplicantState.isConnecting(mWifiInfo.getSupplicantState())) {
            localLog("connectToNetwork: Either already connected "
                    + "or is connecting to " + targetAssociationId);
            return;
        }

        if (candidate.BSSID != null
                && !candidate.BSSID.equals(ClientModeImpl.SUPPLICANT_BSSID_ANY)
                && !candidate.BSSID.equals(targetBssid)) {
            localLog("connecToNetwork: target BSSID " + targetBssid + " does not match the "
                    + "config specified BSSID " + candidate.BSSID + ". Drop it!");
            return;
        }

        long elapsedTimeMillis = mClock.getElapsedSinceBootMillis();
        if (!mScreenOn && shouldSkipConnectionAttempt(elapsedTimeMillis)) {
            localLog("connectToNetwork: Too many connection attempts. Skipping this attempt!");
            mTotalConnectivityAttemptsRateLimited++;
            return;
        }
        noteConnectionAttempt(elapsedTimeMillis);

        mLastConnectionAttemptBssid = targetBssid;

        WifiConfiguration currentConnectedNetwork = mConfigManager
                .getConfiguredNetwork(mWifiInfo.getNetworkId());
        String currentAssociationId = (currentConnectedNetwork == null) ? "Disconnected" :
                (mWifiInfo.getSSID() + " : " + mWifiInfo.getBSSID());

        if (currentConnectedNetwork != null
                && (currentConnectedNetwork.networkId == candidate.networkId
                //TODO(b/36788683): re-enable linked configuration check
                /* || currentConnectedNetwork.isLinked(candidate) */)) {
            // Framework initiates roaming only if firmware doesn't support
            // {@link android.net.wifi.WifiManager#WIFI_FEATURE_CONTROL_ROAMING}.
            if (mConnectivityHelper.isFirmwareRoamingSupported()) {
                // Keep this logging here for now to validate the firmware roaming behavior.
                localLog("connectToNetwork: Roaming candidate - " + targetAssociationId + "."
                        + " The actual roaming target is up to the firmware.");
            } else {
                localLog("connectToNetwork: Roaming to " + targetAssociationId + " from "
                        + currentAssociationId);
                mStateMachine.startRoamToNetwork(candidate.networkId, scanResultCandidate);
            }
        } else {
            // Framework specifies the connection target BSSID if firmware doesn't support
            // {@link android.net.wifi.WifiManager#WIFI_FEATURE_CONTROL_ROAMING} or the
            // candidate configuration contains a specified BSSID.
            if (!mStateMachine.isActiveDualMode() && mConnectivityHelper.isFirmwareRoamingSupported()
                && (candidate.BSSID == null || candidate.BSSID.equals(ClientModeImpl.SUPPLICANT_BSSID_ANY))) {
                targetBssid = ClientModeImpl.SUPPLICANT_BSSID_ANY;
                localLog("connectToNetwork: Connect to " + candidate.SSID + ":" + targetBssid
                        + " from " + currentAssociationId);
            } else {
                localLog("connectToNetwork: Connect to " + targetAssociationId + " from "
                        + currentAssociationId);
            }
            mStateMachine.startConnectToNetwork(candidate.networkId, Process.WIFI_UID, targetBssid);
        }
    }

    /**
     * Handler for screen state (on/off) changes
     */
    public void handleScreenStateChanged(boolean screenOn) {
        localLog("handleScreenStateChanged: screenOn=" + screenOn);
        mScreenOn = screenOn;
    }

    /**
     * Save current miracast mode, it will be used to ignore
     * connectivity scan during the time when miracast is enabled.
     */
    public void saveMiracastMode(int mode) {
        Log.d(TAG,"saveMiracastMode: mode=" + mode);
        mMiracastMode = mode;
    }

    /**
     * Helper function that converts the WIFI_STATE_XXX constants to string
     */
    private static String stateToString(int state) {
        switch (state) {
            case WIFI_STATE_CONNECTED:
                return "connected";
            case WIFI_STATE_DISCONNECTED:
                return "disconnected";
            case WIFI_STATE_TRANSITIONING:
                return "transitioning";
            default:
                return "unknown";
        }
    }

    /**
     * Handler for WiFi state (connected/disconnected) changes
     */
    public void handleConnectionStateChanged(int state) {
        localLog("handleConnectionStateChanged: state=" + stateToString(state));

        mWifiState = state;

        // Reset BSSID of last connection attempt and kick off
        // the watchdog timer if entering disconnected state.
        if (mWifiState == WIFI_STATE_DISCONNECTED) {
            mLastConnectionAttemptBssid = null;
        }
    }

    /**
     * Handler when user specifies a particular network to connect to
     */
    public void setUserConnectChoice(int netId) {
        localLog("setUserConnectChoice: netId=" + netId);

        mNetworkSelector.setUserConnectChoice(netId);
    }

    /**
     * Handler to prepare for connection to a user or app specified network
     */
    public void prepareForForcedConnection(int netId) {
        localLog("prepareForForcedConnection: netId=" + netId);

        clearConnectionAttemptTimeStamps();
        clearBssidBlacklist();
    }

    /**
     * Handler for on-demand connectivity scan
     */
    public void forceConnectivityScan(WorkSource workSource) {
        localLog("forceConnectivityScan in request of " + workSource);

        mWaitForFullBandScanResults = true;
    }

    /**
     * Update the BSSID blacklist when a BSSID is enabled or disabled
     *
     * @param bssid the bssid to be enabled/disabled
     * @param enable -- true enable the bssid
     *               -- false disable the bssid
     * @param reasonCode enable/disable reason code
     * @return true if blacklist is updated; false otherwise
     */
    private boolean updateBssidBlacklist(String bssid, boolean enable, int reasonCode) {
        // Remove the bssid from blacklist when it is enabled.
        if (enable) {
            return mBssidBlacklist.remove(bssid) != null;
        }

        // Update the bssid's blacklist status when it is disabled because of
        // association rejection.
        BssidBlacklistStatus status = mBssidBlacklist.get(bssid);
        if (status == null) {
            // First time for this BSSID
            status = new BssidBlacklistStatus();
            mBssidBlacklist.put(bssid, status);
        }

        status.blacklistedTimeStamp = mClock.getElapsedSinceBootMillis();
        status.counter++;
        if (!status.isBlacklisted) {
            if (status.counter >= BSSID_BLACKLIST_THRESHOLD
                    || reasonCode == REASON_CODE_AP_UNABLE_TO_HANDLE_NEW_STA) {
                status.isBlacklisted = true;
                return true;
            }
        }
        return false;
    }

    /**
     * Track whether a BSSID should be enabled or disabled for WifiNetworkSelector
     *
     * @param bssid the bssid to be enabled/disabled
     * @param enable -- true enable the bssid
     *               -- false disable the bssid
     * @param reasonCode enable/disable reason code
     * @return true if blacklist is updated; false otherwise
     */
    public boolean trackBssid(String bssid, boolean enable, int reasonCode) {
        localLog("trackBssid: " + (enable ? "enable " : "disable ") + bssid + " reason code "
                + reasonCode);

        if (bssid == null) {
            return false;
        }

        if (!updateBssidBlacklist(bssid, enable, reasonCode)) {
            return false;
        }

        // Blacklist was updated, so update firmware roaming configuration.
        updateFirmwareRoamingConfiguration();
        return true;
    }

    /**
     * Check whether a bssid is disabled
     */
    @VisibleForTesting
    public boolean isBssidDisabled(String bssid) {
        BssidBlacklistStatus status = mBssidBlacklist.get(bssid);
        return status == null ? false : status.isBlacklisted;
    }

    /**
     * Compile and return a hashset of the blacklisted BSSIDs
     */
    private HashSet<String> buildBssidBlacklist() {
        HashSet<String> blacklistedBssids = new HashSet<String>();
        for (String bssid : mBssidBlacklist.keySet()) {
            if (isBssidDisabled(bssid)) {
                blacklistedBssids.add(bssid);
            }
        }

        return blacklistedBssids;
    }

    /**
     * Update firmware roaming configuration if the firmware roaming feature is supported.
     * Compile and write the BSSID blacklist only. TODO(b/36488259): SSID whitelist is always
     * empty for now.
     */
    private void updateFirmwareRoamingConfiguration() {
        if (!mConnectivityHelper.isFirmwareRoamingSupported()) {
            return;
        }

        int maxBlacklistSize = mConnectivityHelper.getMaxNumBlacklistBssid();
        if (maxBlacklistSize <= 0) {
            Log.wtf(TAG, "Invalid max BSSID blacklist size:  " + maxBlacklistSize);
            return;
        }

        ArrayList<String> blacklistedBssids = new ArrayList<String>(buildBssidBlacklist());
        int blacklistSize = blacklistedBssids.size();

        if (blacklistSize > maxBlacklistSize) {
            Log.wtf(TAG, "Attempt to write " + blacklistSize + " blacklisted BSSIDs, max size is "
                    + maxBlacklistSize);

            blacklistedBssids = new ArrayList<String>(blacklistedBssids.subList(0,
                    maxBlacklistSize));
            localLog("Trim down BSSID blacklist size from " + blacklistSize + " to "
                    + blacklistedBssids.size());
        }

        if (!mConnectivityHelper.setFirmwareRoamingConfiguration(blacklistedBssids,
                new ArrayList<String>())) {  // TODO(b/36488259): SSID whitelist management.
            localLog("Failed to set firmware roaming configuration.");
        }
    }

    /**
     * Refresh the BSSID blacklist
     *
     * Go through the BSSID blacklist and check if a BSSID has been blacklisted for
     * BSSID_BLACKLIST_EXPIRE_TIME_MS. If yes, re-enable it.
     */
    private void refreshBssidBlacklist() {
        if (mBssidBlacklist.isEmpty()) {
            return;
        }

        boolean updated = false;
        Iterator<BssidBlacklistStatus> iter = mBssidBlacklist.values().iterator();
        Long currentTimeStamp = mClock.getElapsedSinceBootMillis();

        while (iter.hasNext()) {
            BssidBlacklistStatus status = iter.next();
            if (status.isBlacklisted && ((currentTimeStamp - status.blacklistedTimeStamp)
                    >= BSSID_BLACKLIST_EXPIRE_TIME_MS)) {
                iter.remove();
                updated = true;
            }
        }

        if (updated) {
            updateFirmwareRoamingConfiguration();
        }
    }

    /**
     * Helper method to populate WifiScanner handle. This is done lazily because
     * WifiScanningService is started after WifiService.
     */
    private void retrieveWifiScanner() {
        if (mScanner != null) return;
        mScanner = mWifiInjector.getWifiScanner();
        checkNotNull(mScanner);
        // Register for all single scan results
        mScanner.registerScanListener(mAllSingleScanListener);
    }


    /**
     * Clear the BSSID blacklist
     */
    private void clearBssidBlacklist() {
        mBssidBlacklist.clear();
        updateFirmwareRoamingConfiguration();
    }

    /**
     * Start QtiWifiConnectivityManager
     */
    private void start() {
        if (mRunning) return;
        retrieveWifiScanner();
        mConnectivityHelper.setInterfaceName(mStateMachine.getInterfaceName());
        mConnectivityHelper.getFirmwareRoamingInfo();
        clearBssidBlacklist();
        mRunning = true;
    }

    /**
     * Stop and reset QtiWifiConnectivityManager
     */
    private void stop() {
        if (!mRunning) return;
        mRunning = false;
        clearBssidBlacklist();
        mLastConnectionAttemptBssid = null;
        mWaitForFullBandScanResults = false;
    }

    /**
     * Update QtiWifiConnectivityManager running state
     *
     * Start QtiWifiConnectivityManager only if both Wifi and QtiWifiConnectivityManager
     * are enabled, otherwise stop it.
     */
    private void updateRunningState() {
        if (mWifiEnabled) {
            localLog("Starting up QtiWifiConnectivityManager");
            start();
        } else {
            localLog("Stopping QtiWifiConnectivityManager");
            stop();
        }
    }

    /**
     * Inform WiFi is enabled for connection or not
     */
    public void setWifiEnabled(boolean enable) {
        localLog("Set WiFi " + (enable ? "enabled" : "disabled"));

        mWifiEnabled = enable;
        updateRunningState();
    }

    /**
     * Dump the local logs.
     */
    public void dump(FileDescriptor fd, PrintWriter pw, String[] args) {
        pw.println("Dump of QtiWifiConnectivityManager");
        pw.println("QtiWifiConnectivityManager - Log Begin ----");
        mLocalLog.dump(fd, pw, args);
        pw.println("QtiWifiConnectivityManager - Log End ----");
    }
}
