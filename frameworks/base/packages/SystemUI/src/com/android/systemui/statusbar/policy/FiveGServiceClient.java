/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *  * Neither the name of The Linux Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
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

package com.android.systemui.statusbar.policy;

import static android.telephony.CellSignalStrength.SIGNAL_STRENGTH_NONE_OR_UNKNOWN;
import static android.telephony.CellSignalStrength.SIGNAL_STRENGTH_POOR;
import static android.telephony.CellSignalStrength.SIGNAL_STRENGTH_MODERATE;
import static android.telephony.CellSignalStrength.SIGNAL_STRENGTH_GOOD;
import static android.telephony.CellSignalStrength.SIGNAL_STRENGTH_GREAT;

import android.content.ComponentName;
import android.content.Context;
import android.content.ServiceConnection;
import android.net.Uri;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.DeadObjectException;
import android.os.RemoteException;
import android.provider.Settings;
import android.telephony.CellInfo;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;
import android.util.Log;
import android.util.SparseArray;

import com.google.android.collect.Lists;
import com.android.internal.annotations.VisibleForTesting;

import java.lang.Exception;
import java.util.ArrayList;
import java.lang.ref.WeakReference;

import org.codeaurora.internal.Client;
import org.codeaurora.internal.IExtTelephony;
import org.codeaurora.internal.INetworkCallback;
import org.codeaurora.internal.NetworkCallbackBase;
import org.codeaurora.internal.NrConfigType;
import org.codeaurora.internal.NrIconType;
import org.codeaurora.internal.ServiceUtil;
import org.codeaurora.internal.SignalStrength;
import org.codeaurora.internal.Status;
import org.codeaurora.internal.Token;
import org.codeaurora.internal.NetworkCallbackBase;

import com.android.keyguard.KeyguardUpdateMonitorCallback;
import com.android.systemui.R;
import com.android.systemui.statusbar.policy.MobileSignalController.MobileIconGroup;

public class FiveGServiceClient {
    private static final String TAG = "FiveGServiceClient";
    private static final boolean DEBUG = Log.isLoggable(TAG, Log.DEBUG)||true;
    private static final int MESSAGE_REBIND = 1024;
    private static final int MESSAGE_REINIT = MESSAGE_REBIND+1;
    private static final int MESSAGE_NOTIFIY_MONITOR_CALLBACK = MESSAGE_REBIND+2;
    private static final int MAX_RETRY = 4;
    private static final int DELAY_MILLISECOND = 3000;
    private static final int DELAY_INCREMENT = 2000;

    /**
     * These threshold values are copied from CellSignalStrengthNr.
     */
    private static final int SIGNAL_GREAT_THRESHOLD = -95;
    private static final int SIGNAL_GOOD_THRESHOLD = -105;
    private static final int SIGNAL_MODERATE_THRESHOLD = -115;

    private static FiveGServiceClient sInstance;
    private final ArrayList<WeakReference<KeyguardUpdateMonitorCallback>>
            mKeyguardUpdateMonitorCallbacks = Lists.newArrayList();
    @VisibleForTesting
    final SparseArray<IFiveGStateListener> mStatesListeners = new SparseArray<>();
    private final SparseArray<FiveGServiceState> mCurrentServiceStates = new SparseArray<>();
    private final SparseArray<FiveGServiceState> mLastServiceStates = new SparseArray<>();

    private Context mContext;
    private boolean mServiceConnected;
    private IExtTelephony mNetworkService;
    private String mPackageName;
    private Client mClient;
    private int mBindRetryTimes = 0;
    private int mInitRetryTimes = 0;

    public static class FiveGServiceState{
        private int mLevel;
        private int mNrConfigType;
        private int mNrIconType;
        private MobileIconGroup mIconGroup;

        public FiveGServiceState(){
            mLevel = 0;
            mNrConfigType = NrConfigType.NSA_CONFIGURATION;
            mNrIconType = NrIconType.INVALID;
            mIconGroup = TelephonyIcons.UNKNOWN;
        }

        public boolean isNrIconTypeValid() {
            return mNrIconType != NrIconType.INVALID && mNrIconType != NrIconType.TYPE_NONE;
        }

        @VisibleForTesting
        public MobileIconGroup getIconGroup() {
            return mIconGroup;
        }

        @VisibleForTesting
        public int getSignalLevel() {
            return mLevel;
        }

        @VisibleForTesting
        int getNrConfigType() {
            return mNrConfigType;
        }

        @VisibleForTesting
        int getNrIconType() {
            return mNrIconType;
        }

        public void copyFrom(FiveGServiceState state) {
            this.mLevel = state.mLevel;
            this.mNrConfigType = state.mNrConfigType;
            this.mIconGroup = state.mIconGroup;
            this.mNrIconType = state.mNrIconType;
        }

        public boolean equals(FiveGServiceState state) {
            return this.mLevel == state.mLevel
                    && this.mNrConfigType == state.mNrConfigType
                    && this.mIconGroup == state.mIconGroup
                    && this.mNrIconType == state.mNrIconType;
        }
        @Override
        public String toString() {
            StringBuilder builder = new StringBuilder();
            builder.append("mLevel=").append(mLevel).append(", ").
                    append("mNrConfigType=").append(mNrConfigType).append(", ").
                    append("mNrIconType=").append(mNrIconType).append(", ").
                    append("mIconGroup=").append(mIconGroup);

            return builder.toString();
        }
    }

    public FiveGServiceClient(Context context) {
        mContext = context;
        mPackageName = mContext.getPackageName();
    }

    public static FiveGServiceClient getInstance(Context context) {
        if ( sInstance == null ) {
            sInstance = new FiveGServiceClient(context);
        }

        return sInstance;
    }

    public void registerCallback(KeyguardUpdateMonitorCallback callback) {
        mKeyguardUpdateMonitorCallbacks.add(
                new WeakReference<KeyguardUpdateMonitorCallback>(callback));
    }

    public void registerListener(int phoneId, IFiveGStateListener listener) {
        Log.d(TAG, "registerListener phoneId=" + phoneId);

        mStatesListeners.put(phoneId, listener);
        if ( !isServiceConnected() ) {
            binderService();
        }else{
            initFiveGServiceState(phoneId);
        }
    }

    public void unregisterListener(int phoneId) {
        Log.d(TAG, "unregisterListener phoneId=" + phoneId);
        mStatesListeners.remove(phoneId);
        mCurrentServiceStates.remove(phoneId);
        mLastServiceStates.remove(phoneId);
    }

    private void binderService() {
        boolean success = ServiceUtil.bindService(mContext, mServiceConnection);
        Log.d(TAG, " bind service " + success);
        if ( !success && mBindRetryTimes < MAX_RETRY && !mHandler.hasMessages(MESSAGE_REBIND)) {
            mHandler.sendEmptyMessageDelayed(MESSAGE_REBIND,
                    DELAY_MILLISECOND + mBindRetryTimes*DELAY_INCREMENT);
            mBindRetryTimes+=1;
        }
    }

    public boolean isServiceConnected() {
        return mServiceConnected;
    }

    @VisibleForTesting
    public FiveGServiceState getCurrentServiceState(int phoneId) {
        return getServiceState(phoneId, mCurrentServiceStates);
    }

    private FiveGServiceState getLastServiceState(int phoneId) {
        return getServiceState(phoneId, mLastServiceStates);
    }

    private static FiveGServiceState getServiceState(int key,
                                                     SparseArray<FiveGServiceState> array) {
        FiveGServiceState state = array.get(key);
        if ( state == null ) {
            state = new FiveGServiceState();
            array.put(key, state);
        }
        return state;
    }

    private void notifyListenersIfNecessary(int phoneId) {
        FiveGServiceState currentState = getCurrentServiceState(phoneId);
        FiveGServiceState lastState = getLastServiceState(phoneId);
        if ( !currentState.equals(lastState) ) {

            if ( DEBUG ) {
                Log.d(TAG, "phoneId(" + phoneId + ") Change in state from " + lastState + " \n"+
                        "\tto " + currentState);

            }

            lastState.copyFrom(currentState);
            IFiveGStateListener listener = mStatesListeners.get(phoneId);
            if (listener != null) {
                listener.onStateChanged(currentState);
            }

            mHandler.sendEmptyMessage(MESSAGE_NOTIFIY_MONITOR_CALLBACK);

        }
    }

    private void initFiveGServiceState() {
        Log.d(TAG, "initFiveGServiceState size=" + mStatesListeners.size());
        for( int i=0; i < mStatesListeners.size(); ++i ) {
            int phoneId = mStatesListeners.keyAt(i);
            initFiveGServiceState(phoneId);
        }
    }

    private void initFiveGServiceState(int phoneId) {
        Log.d(TAG, "mNetworkService=" + mNetworkService + " mClient=" + mClient);
        if ( mNetworkService != null && mClient != null) {
            Log.d(TAG, "query 5G service state for phoneId " + phoneId);
            try {
                queryNrSignalStrength(phoneId);

                Token token = mNetworkService.query5gConfigInfo(phoneId, mClient);
                Log.d(TAG, "query5gConfigInfo result:" + token);

                token = mNetworkService.queryNrIconType(phoneId, mClient);
                Log.d(TAG, "queryNrIconType result:" + token);
            }catch(DeadObjectException e) {
                Log.e(TAG, "initFiveGServiceState: Exception = " + e);
                Log.d(TAG, "try to re-binder service");
                mInitRetryTimes = 0;
                mServiceConnected = false;
                mNetworkService = null;
                mClient = null;
                binderService();
            }catch (Exception e) {
                Log.d(TAG, "initFiveGServiceState: Exception = " + e);
                if ( mInitRetryTimes < MAX_RETRY && !mHandler.hasMessages(MESSAGE_REINIT) ) {
                    mHandler.sendEmptyMessageDelayed(MESSAGE_REINIT,
                            DELAY_MILLISECOND + mInitRetryTimes*DELAY_INCREMENT);
                    mInitRetryTimes +=1;
                }
            }
        }
    }

    public void queryNrSignalStrength(int phoneId) {
        if ( mNetworkService != null && mClient != null) {
            Log.d(TAG, "query queryNrSignalStrength for phoneId " + phoneId);
            try {
                Token token = mNetworkService.queryNrSignalStrength(phoneId, mClient);
                Log.d(TAG, "queryNrSignalStrength result:" + token);
            }catch (Exception e) {
                Log.e(TAG, "queryNrSignalStrength", e);
            }
        }else {
            Log.e(TAG, "query queryNrSignalStrength for phoneId " + phoneId);
        }
    }

    @VisibleForTesting
    void update5GIcon(FiveGServiceState state,int phoneId) {
        state.mIconGroup = getNrIconGroup(state.mNrIconType, phoneId);
    }

    private void updateLevel(int ssRsrp, FiveGServiceState state) {
        if (ssRsrp == CellInfo.UNAVAILABLE) {
            state.mLevel = SIGNAL_STRENGTH_NONE_OR_UNKNOWN;
        } else if (ssRsrp >= SIGNAL_GREAT_THRESHOLD) {
            state.mLevel = SIGNAL_STRENGTH_GREAT;
        } else if (ssRsrp >= SIGNAL_GOOD_THRESHOLD) {
            state.mLevel = SIGNAL_STRENGTH_GOOD;
        } else if (ssRsrp >= SIGNAL_MODERATE_THRESHOLD) {
            state.mLevel = SIGNAL_STRENGTH_MODERATE;
        } else {
            state.mLevel = SIGNAL_STRENGTH_POOR;
        }
    }

    private MobileIconGroup getNrIconGroup(int nrIconType , int phoneId) {
        MobileIconGroup iconGroup = TelephonyIcons.UNKNOWN;
        switch (nrIconType){
            case NrIconType.TYPE_5G_BASIC:
                iconGroup = TelephonyIcons.FIVE_G_BASIC;
                break;
            case NrIconType.TYPE_5G_UWB:
                iconGroup = TelephonyIcons.FIVE_G_UWB;
                break;
        }
        return iconGroup;
    }

    private void notifyMonitorCallback() {
        for (int i = 0; i < mKeyguardUpdateMonitorCallbacks.size(); i++) {
            KeyguardUpdateMonitorCallback cb = mKeyguardUpdateMonitorCallbacks.get(i).get();
            if (cb != null) {
                cb.onRefreshCarrierInfo();
            }
        }
    }

    private Handler mHandler = new Handler() {
        public void handleMessage(Message msg) {
            int what = msg.what;
            switch ( msg.what ) {
                case MESSAGE_REBIND:
                    binderService();
                    break;

                case MESSAGE_REINIT:
                    initFiveGServiceState();
                    break;

                case MESSAGE_NOTIFIY_MONITOR_CALLBACK:
                    notifyMonitorCallback();
                    break;
            }

        }
    };

    private ServiceConnection mServiceConnection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            Log.d(TAG, "onServiceConnected:" + service);

            try {
                mNetworkService = IExtTelephony.Stub.asInterface(service);
                mClient = mNetworkService.registerCallback(mPackageName, mCallback);
                mServiceConnected = true;
                initFiveGServiceState();
                Log.d(TAG, "Client = " + mClient);
            } catch (Exception e) {
                Log.d(TAG, "onServiceConnected: Exception = " + e);
            }
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            Log.d(TAG, "onServiceDisconnected:" + name);
            cleanup();
        }

        @Override
        public void onBindingDied(ComponentName name) {
            Log.d(TAG, "onBindingDied:" + name);
            cleanup();
            if ( mBindRetryTimes < MAX_RETRY ) {
                Log.d(TAG, "try to re-bind");
                mHandler.sendEmptyMessageDelayed(MESSAGE_REBIND,
                        DELAY_MILLISECOND+mBindRetryTimes*DELAY_INCREMENT);
            }
        }

        private void cleanup() {
            Log.d(TAG, "cleanup");
            mServiceConnected = false;
            mNetworkService = null;
            mClient = null;
        }
    };


    @VisibleForTesting
    protected INetworkCallback mCallback = new NetworkCallbackBase() {
        @Override
        public void onSignalStrength(int slotId, Token token, Status status,
                                     org.codeaurora.internal.SignalStrength
                                             signalStrength) throws RemoteException {
            if ( DEBUG ) {
                Log.d(TAG, "onSignalStrength: slotId=" + slotId + " token=" + token
                        + " status=" + status + " signalStrength=" + signalStrength);
            }

            if (status.get() == Status.SUCCESS && signalStrength != null) {
                FiveGServiceState state = getCurrentServiceState(slotId);
                updateLevel(signalStrength.getRsrp(), state);
                notifyListenersIfNecessary(slotId);
            }
        }

        @Override
        public void on5gConfigInfo(int slotId, Token token, Status status, NrConfigType
                nrConfigType) throws RemoteException {
            Log.d(TAG,
                    "on5gConfigInfo: slotId = " + slotId + " token = " + token + " " + "status"
                            + status + " NrConfigType = " + nrConfigType);
            if (status.get() == Status.SUCCESS) {
                FiveGServiceState state = getCurrentServiceState(slotId);
                state.mNrConfigType = nrConfigType.get();
                notifyListenersIfNecessary(slotId);
            }
        }

        @Override
        public void onNrIconType(int slotId, Token token, Status status, NrIconType
                nrIconType) throws RemoteException {
            Log.d(TAG,
                    "onNrIconType: slotId = " + slotId + " token = " + token + " " + "status"
                            + status + " NrIconType = " + nrIconType);
            if (status.get() == Status.SUCCESS) {
                FiveGServiceState state = getCurrentServiceState(slotId);
                state.mNrIconType = nrIconType.get();
                update5GIcon(state, slotId);
                notifyListenersIfNecessary(slotId);
            }
        }
    };

    public interface IFiveGStateListener {
        public void onStateChanged(FiveGServiceState state);
    }
}
