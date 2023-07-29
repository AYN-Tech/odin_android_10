/*
 * Copyright (C) 2008 Esmertec AG.
 * Copyright (C) 2008 The Android Open Source Project
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

package com.android.mms;

import android.app.Application;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.res.Configuration;
import android.database.ContentObserver;
import android.drm.DrmManagerClient;
import android.location.Country;
import android.location.CountryDetector;
import android.location.CountryListener;
import android.content.pm.PackageManager;
import android.content.ComponentName;
import android.net.Uri;
import android.os.StrictMode;
import android.preference.PreferenceManager;
import android.provider.SearchRecentSuggestions;
import android.provider.Settings;
import android.provider.Settings.SettingNotFoundException;
import android.telephony.TelephonyManager;
import android.util.Log;

import com.android.mms.data.Contact;
import com.android.mms.data.Conversation;
import com.android.mms.layout.LayoutManager;
import com.android.mms.transaction.MessagingNotification;
import com.android.mms.transaction.MmsNoConfirmationSendActivity;
import com.android.mms.transaction.MmsSystemEventReceiver;
import com.android.mms.transaction.SmsReceiver;
import com.android.mms.transaction.SmsReceiverService;
import com.android.mms.ui.MessageUtils;
import com.android.mms.ui.SmsStorageMonitor;
import com.android.mms.util.DownloadManager;
import com.android.mms.util.DraftCache;
import com.android.mms.util.PduLoaderManager;
import com.android.mms.util.RateController;
import com.android.mms.util.ThumbnailManager;
import com.android.mmswrapper.ConstantsWrapper;
import android.provider.Telephony.Mms;
import android.net.ConnectivityManager;
import com.android.mms.transaction.SmsRejectedReceiver;
import android.provider.Telephony.Sms;

public class MmsApp extends Application {
    public static final String LOG_TAG = LogTag.TAG;

    private SearchRecentSuggestions mRecentSuggestions;
    private TelephonyManager mTelephonyManager;
    private CountryDetector mCountryDetector;
    private CountryListener mCountryListener;
    private String mCountryIso;
    private static MmsApp sMmsApp = null;
    private PduLoaderManager mPduLoaderManager;
    private ThumbnailManager mThumbnailManager;
    private DrmManagerClient mDrmManagerClient;
    public static boolean hasPermissionRelated = false;
    private SmsStorageMonitor mSmsStorageMonitor = new SmsStorageMonitor();

    private MmsSystemEventReceiver mSystemEventReceiver = new MmsSystemEventReceiver();
    private SmsRejectedReceiver mSmsRejectedReceiver = new SmsRejectedReceiver();

    @Override
    public void onCreate() {
        super.onCreate();

        if (Log.isLoggable(LogTag.STRICT_MODE_TAG, Log.DEBUG)) {
            // Log tag for enabling/disabling StrictMode violation log. This will dump a stack
            // in the log that shows the StrictMode violator.
            // To enable: adb shell setprop log.tag.Mms:strictmode DEBUG
            StrictMode.setThreadPolicy(
                    new StrictMode.ThreadPolicy.Builder().detectAll().penaltyLog().build());
        }

        sMmsApp = this;

        // Load the default preference values
        if (getResources().getBoolean(R.bool.def_custom_preferences_settings)) {
            PreferenceManager.setDefaultValues(this, R.xml.custom_preferences,
                    false);
        } else {
            PreferenceManager.setDefaultValues(this, R.xml.preferences, false);
        }

        // Figure out the country *before* loading contacts and formatting numbers
        mCountryDetector = (CountryDetector) getSystemService(Context.COUNTRY_DETECTOR);
        mCountryListener = new CountryListener() {
            @Override
            public synchronized void onCountryDetected(Country country) {
                mCountryIso = country.getCountryIso();
            }
        };
        mCountryDetector.addCountryListener(mCountryListener, getMainLooper());

        Context context = getApplicationContext();
        mPduLoaderManager = new PduLoaderManager(context);
        mThumbnailManager = new ThumbnailManager(context);

        MmsConfig.init(this);
        DownloadManager.init(this);
        RateController.init(this);
        LayoutManager.init(this);
        MessagingNotification.init(this);
        if (MessageUtils.hasBasicPermissions()) {
            initPermissionRelated();
        }
        int enablePlugger = getResources().getBoolean(R.bool.enablePlugger) ?
                PackageManager.COMPONENT_ENABLED_STATE_ENABLED
                : PackageManager.COMPONENT_ENABLED_STATE_DEFAULT;
        PackageManager pm_plugger = getPackageManager();
        pm_plugger.setComponentEnabledSetting(new ComponentName(this,
                MmsNoConfirmationSendActivity.class), enablePlugger,
                PackageManager.DONT_KILL_APP);

        IntentFilter filter = new IntentFilter();
        filter.addAction(ConstantsWrapper.IntentConstants.ACTION_DEVICE_STORAGE_FULL);
        filter.addAction(ConstantsWrapper.IntentConstants.ACTION_DEVICE_STORAGE_NOT_FULL);
        registerReceiver(mSmsStorageMonitor, filter);

        registSystemReceiver();
        registSmsRejectedReceiver();
    }

    private void registSystemReceiver() {
        IntentFilter filter = new IntentFilter();
        filter.addAction(Mms.Intents.CONTENT_CHANGED_ACTION);
        filter.addAction(ConnectivityManager.CONNECTIVITY_ACTION);
        filter.addAction(Intent.ACTION_AIRPLANE_MODE_CHANGED);
        filter.addAction(ConstantsWrapper.TelephonyIntent.ACTION_SIM_STATE_CHANGED);
        registerReceiver(mSystemEventReceiver, filter);
    }

    private void registSmsRejectedReceiver() {
        IntentFilter filter = new IntentFilter();
        filter.addAction(Sms.Intents.SMS_REJECTED_ACTION);
        registerReceiver(mSmsRejectedReceiver, filter);
    }

    public void initPermissionRelated() {
        if (!hasPermissionRelated) {
            Contact.init(MmsApp.getApplication());
            DraftCache.init(MmsApp.getApplication());
            Conversation.init(MmsApp.getApplication());
            MmsApp.getApplication().activePendingMessages();
            registerMobileDataObserver();
            hasPermissionRelated = true;
        }
    }

    private void registerMobileDataObserver() {
        TelephonyManager tm = (TelephonyManager) getSystemService(Context.TELEPHONY_SERVICE);
        int phoneCount = tm.getPhoneCount();
        final int MOBILE_DATA_ON = 1;

        for( int i =0; i < phoneCount; i++) {
            Uri uri = Settings.Global.getUriFor(ConstantsWrapper.SettingsGlobal.MOBILE_DATA + i);
            getContentResolver().registerContentObserver(uri, false, new ContentObserver(null) {
                @Override
                public void onChange(boolean selfChange, Uri uri) {
                     Log.d(LOG_TAG, "MobileData UI changed = " + uri);
                     String uriLastSegment = uri.getLastPathSegment();
                     int uriLength = uriLastSegment.length();
                     int phoneId = Character.getNumericValue(uriLastSegment.charAt(uriLength - 1));

                     try {
                         int value = Settings.Global.getInt(getContentResolver(),
                                 ConstantsWrapper.SettingsGlobal.MOBILE_DATA + phoneId);
                         Log.d(LOG_TAG, "value = " + value);
                         if (value == MOBILE_DATA_ON) {
                             if (Log.isLoggable(LogTag.TRANSACTION, Log.VERBOSE)) {
                                 Log.d(LOG_TAG,
                                         "MobileData turned ON, trigger pending mms processing");
                             }
                             activePendingMessages();
                         }
                     } catch (SettingNotFoundException e) {
                         Log.e(LOG_TAG, "Exception in getting MobileData UI value. e = " + e);
                     }
                }
            });
        }
    }

    /**
     * Try to process all pending messages(which were interrupted by user, OOM, Mms crashing,
     * etc...) when Mms app is (re)launched.
     */
    private void activePendingMessages() {
        // For Mms: try to process all pending transactions if possible
        MmsSystemEventReceiver.wakeUpService(this);
        // For Sms: retry to send smses in outbox and queued box
        sendBroadcast(new Intent(SmsReceiverService.ACTION_SEND_INACTIVE_MESSAGE,
                null,
                this,
                SmsReceiver.class));
    }

    synchronized public static MmsApp getApplication() {
        return sMmsApp;
    }

    @Override
    public void onTerminate() {
        super.onTerminate();
        mCountryDetector.removeCountryListener(mCountryListener);
        unregisterReceiver(mSmsStorageMonitor);

        unregisterReceiver(mSystemEventReceiver);
        unregisterReceiver(mSmsRejectedReceiver);
    }

    @Override
    public void onLowMemory() {
        super.onLowMemory();

        mPduLoaderManager.onLowMemory();
        mThumbnailManager.onLowMemory();
    }

    public PduLoaderManager getPduLoaderManager() {
        return mPduLoaderManager;
    }

    public ThumbnailManager getThumbnailManager() {
        return mThumbnailManager;
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        LayoutManager.getInstance().onConfigurationChanged(newConfig);
    }

    /**
     * @return Returns the TelephonyManager.
     */
    public TelephonyManager getTelephonyManager() {
        if (mTelephonyManager == null) {
            mTelephonyManager = (TelephonyManager)getApplicationContext()
                    .getSystemService(Context.TELEPHONY_SERVICE);
        }
        return mTelephonyManager;
    }

    /**
     * Returns the content provider wrapper that allows access to recent searches.
     * @return Returns the content provider wrapper that allows access to recent searches.
     */
    public SearchRecentSuggestions getRecentSuggestions() {
        /*
        if (mRecentSuggestions == null) {
            mRecentSuggestions = new SearchRecentSuggestions(this,
                    SuggestionsProvider.AUTHORITY, SuggestionsProvider.MODE);
        }
        */
        return mRecentSuggestions;
    }

    // This function CAN return null.
    public String getCurrentCountryIso() {
        if (mCountryIso == null) {
            Country country = mCountryDetector.detectCountry();
            if (country != null) {
                mCountryIso = country.getCountryIso();
            }
        }
        return mCountryIso;
    }

    public DrmManagerClient getDrmManagerClient() {
        if (mDrmManagerClient == null) {
            mDrmManagerClient = new DrmManagerClient(getApplicationContext());
        }
        return mDrmManagerClient;
    }

}
