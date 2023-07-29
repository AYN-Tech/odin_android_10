/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Not a Contribution.
 * Copyright (C) 2007-2008 Esmertec AG.
 * Copyright (C) 2007-2008 The Android Open Source Project
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

package com.android.mms.ui;

import android.R.drawable;
import android.app.ActionBar;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.BroadcastReceiver;
import android.content.ActivityNotFoundException;
import android.app.DialogFragment;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.Resources;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.media.Ringtone;
import android.media.RingtoneManager;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.os.Parcelable;
import android.os.Vibrator;
import android.os.Message;
import android.os.Handler;
import android.os.Bundle;
import android.os.Looper;
import android.preference.CheckBoxPreference;
import android.preference.EditTextPreference;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.Preference.OnPreferenceClickListener;
import android.preference.PreferenceActivity;
import android.preference.PreferenceCategory;
import android.preference.PreferenceManager;
import android.preference.PreferenceScreen;
import android.preference.RingtonePreference;
import android.preference.SwitchPreference;
import android.provider.MediaStore;
import android.provider.SearchRecentSuggestions;
import android.provider.Settings;
import android.provider.Telephony;
import android.provider.Telephony.Mms;
import android.provider.Telephony.Threads;
import android.telephony.SmsManager;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.EditText;
import android.widget.GridView;
import android.widget.ImageView;
import android.widget.SimpleAdapter;
import android.widget.SimpleAdapter.ViewBinder;
import android.widget.Toast;
import android.util.Log;

//import com.android.internal.telephony.OperatorSimInfo;
import com.android.internal.telephony.SmsApplication;
import com.android.internal.telephony.SmsApplication.SmsApplicationData;
import com.android.mms.MmsApp;
import com.android.mms.MmsConfig;
import com.android.mms.R;
import com.android.mms.transaction.TransactionService;
import com.android.mms.util.Recycler;
import com.android.mmswrapper.ConstantsWrapper;
import com.android.mmswrapper.SubscriptionManagerWrapper;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;

/**
 * With this activity, users can set preferences for MMS and SMS and
 * can access and manipulate SMS messages stored on the SIM.
 */
public class MessagingPreferenceActivity extends PreferenceActivity
            implements OnPreferenceChangeListener {
    private static final String TAG = "MessagingPreferenceActivity";
    // Symbolic names for the keys used for preference lookup
    public static final String MMS_DELIVERY_REPORT_MODE = "pref_key_mms_delivery_reports";
    public static final String EXPIRY_TIME              = "pref_key_mms_expiry";
    public static final String EXPIRY_TIME_SLOT1        = "pref_key_mms_expiry_slot1";
    public static final String EXPIRY_TIME_SLOT2        = "pref_key_mms_expiry_slot2";
    public static final String PRIORITY                 = "pref_key_mms_priority";
    public static final String READ_REPORT_MODE         = "pref_key_mms_read_reports";
    public static final String SMS_DELIVERY_REPORT_MODE = "pref_key_sms_delivery_reports";
    public static final String SMS_DELIVERY_REPORT_SUB1 = "pref_key_sms_delivery_reports_slot1";
    public static final String SMS_DELIVERY_REPORT_SUB2 = "pref_key_sms_delivery_reports_slot2";
    public static final String NOTIFICATION_ENABLED     = "pref_key_enable_notifications";
    public static final String NOTIFICATION_VIBRATE     = "pref_key_vibrate";
    public static final String NOTIFICATION_VIBRATE_WHEN= "pref_key_vibrateWhen";
    public static final String NOTIFICATION_RINGTONE    = "pref_key_ringtone";
    public static final String AUTO_RETRIEVAL           = "pref_key_mms_auto_retrieval";
    public static final String RETRIEVAL_DURING_ROAMING = "pref_key_mms_retrieval_during_roaming";
    public static final String AUTO_DELETE              = "pref_key_auto_delete";
    public static final String GROUP_MMS_MODE           = "pref_key_mms_group_mms";
    public static final String SMSC_DEFAULT             = "pref_key_default_smsc";
    public static final String SMS_VALIDITY_FOR_SIM1    = "pref_key_sms_validity_period_for_sim1";
    public static final String SMS_VALIDITY_SIM1        = "pref_key_sms_validity_period_sim1";
    public static final String SMS_VALIDITY_FOR_SIM2    = "pref_key_sms_validity_period_for_sim2";
    public static final String SMS_VALIDITY_SIM2        = "pref_key_sms_validity_period_sim2";
    public static final String SMS_VALIDITY_NO_MULTI    = "pref_key_sms_validity_period_no_multi";
    public static final String ZOOM_MESSAGE             = "pref_key_zoom_message";
    public static final String SEPERATE_NOTIFI_MSG      = "pref_key_seperate_notification_msg";
    public static final String ENABLE_GESTURE           = "pref_key_enable_gesture";
    public static final String ENABLE_SELECTABLE_COPY   = "pref_key_enable_selectable_copy";

    public static final boolean ENABLE_SELECTABLE_COPY_DEFAULT_VALUE = true;

    // ConfigurationClient
    public static final String OMACP_CONFIGURATION_CATEGORY =
            "pref_key_sms_omacp_configuration";
    public static final String CONFIGURATION_MESSAGE    = "pref_key_configuration_message";

    public static final String CHAT_WALLPAPER_SETTING   = "pref_key_chat_wallpaper";

    // Expiry of MMS
    private final static String EXPIRY_ONE_WEEK = "604800"; // 7 * 24 * 60 * 60
    private final static String EXPIRY_TWO_DAYS = "172800"; // 2 * 24 * 60 * 60

    //Chat wallpaper
    public static final String CHAT_WALLPAPER            = "chat_wallpaper";

    private static final int  PICK_FROM_CAMERA        = 0;
    private static final int  PICK_FROM_GALLERY       = 1;

    public static final String CELL_BROADCAST            = "pref_key_cell_broadcast";
    //Fontsize
    public static final String FONT_SIZE_SETTING         = "pref_key_message_font_size";

    // Menu entries
    private static final int MENU_RESTORE_DEFAULTS    = 1;

    // Preferences for enabling and disabling SMS
    private Preference mSmsDisabledPref;
    private Preference mSmsEnabledPref;

    private PreferenceCategory mStoragePrefCategory;
    private PreferenceCategory mSmsPrefCategory;
    private PreferenceCategory mMmsPrefCategory;
    private PreferenceCategory mNotificationPrefCategory;
    private PreferenceCategory mSmscPrefCate;

    private Preference mChatWallpaperPref;
    private Preference mSmsLimitPref;
    private Preference mSmsDeliveryReportPref;
    private Preference mSmsDeliveryReportPrefSub1;
    private Preference mSmsDeliveryReportPrefSub2;
    private Preference mMmsLimitPref;
    private Preference mMmsDeliveryReportPref;
    private Preference mMmsGroupMmsPref;
    private Preference mMmsReadReportPref;
    private Preference mManageSimPref;
    private Preference mManageSim1Pref;
    private Preference mManageSim2Pref;
    private Preference mCBsettingPref;
    private Preference mClearHistoryPref;
    private Preference mConfigurationmessage;
    private Preference mMmsSizeLimit;
    private Preference mMemoryStatusPref;
    private Preference mCellBroadcsatsPref;
    private SwitchPreference mVibratePref;
    private CheckBoxPreference mEnableNotificationsPref;
    private CheckBoxPreference mMmsAutoRetrievialPref;
    private ListPreference mFontSizePref;
    private ListPreference mMmsCreationModePref;
    private ListPreference mMmsExpiryPref;
    private ListPreference mMmsExpiryCard1Pref;
    private ListPreference mMmsExpiryCard2Pref;
    private RingtonePreference mRingtonePref;
    private ListPreference mSmsValidityPref;
    private ListPreference mSmsValidityCard1Pref;
    private ListPreference mSmsValidityCard2Pref;
    private Recycler mSmsRecycler;
    private Recycler mMmsRecycler;
    private Preference mSmsTemplate;
    private CheckBoxPreference mSmsSignaturePref;
    private EditTextPreference mSmsSignatureEditPref;
    private ArrayList<Preference> mSmscPrefList = new ArrayList<Preference>();
    private static final int CONFIRM_CLEAR_SEARCH_HISTORY_DIALOG = 3;

    private AsyncDialog mAsyncDialog;

    // Whether or not we are currently enabled for SMS. This field is updated in onResume to make
    // sure we notice if the user has changed the default SMS app.
    private boolean mIsSmsEnabled;
    private static final String SMSC_DIALOG_TITLE = "title";
    private static final String SMSC_DIALOG_NUMBER = "smsc";
    private static final String SMSC_DIALOG_SUB = "sub";
    private static final int EVENT_SET_SMSC_DONE = 0;
    private static final int EVENT_GET_SMSC_DONE = 1;
    private static final int EVENT_SET_SMSC_PREF_DONE = 2;
    private static SmscHandler mHandler = null;

    public static final String MESSAGE_FONT_SIZE = "message_font_size";

    public static final String MMS_CREATION_MODE = "pref_key_creation_mode";
    public static final int CREATION_MODE_RESTRICTED = 1;
    public static final int CREATION_MODE_WARNING = 2;
    public static final int CREATION_MODE_FREE = 3;

    // ConfigurationClient
    private static final String ACTION_CONFIGURE_MESSAGE =
            "org.codeaurora.CONFIGURE_MESSAGE";

    private static final int PREFERENCE_TYPE_DELIVERY_REPORT = 1;
    private static final int PREFERENCE_TYPE_MANAGE_SIM_CARD = 2;
    private static final int PREFERENCE_TYPE_SMS_VALIDITY = 3;
    private static final int PREFERENCE_TYPE_MMS_VALIDITY = 4;

    private BroadcastReceiver mReceiver = new BroadcastReceiver() {
        public void onReceive(Context context, Intent intent) {
            final String action = intent.getAction();
            PreferenceCategory smsCategory =
                    (PreferenceCategory) findPreference("pref_key_sms_settings");
            if (ConstantsWrapper.TelephonyIntent.ACTION_SIM_STATE_CHANGED.equals(action)) {
                if (smsCategory != null) {
                    updateSIMSMSPref();
                }
            } else if (Intent.ACTION_AIRPLANE_MODE_CHANGED.equals(action)) {
                updateSMSCPref();
            }
        }
    };

    @Override
    protected void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        if (MessageUtils.checkPermissionsIfNeeded(this)) {
            return;
        }
        mHandler = new SmscHandler(this);
        loadPrefs();

        ActionBar actionBar = getActionBar();
        actionBar.setDisplayHomeAsUpEnabled(true);
    }

    @Override
    protected void onResume() {
        super.onResume();
        boolean isSmsEnabled = MmsConfig.isSmsEnabled(this);
        if (isSmsEnabled != mIsSmsEnabled) {
            mIsSmsEnabled = isSmsEnabled;
            invalidateOptionsMenu();
        }

        // Since the enabled notifications pref can be changed outside of this activity,
        // we have to reload it whenever we resume.
        setEnabledNotificationsPref();
        // Initialize the sms signature
        updateSignatureStatus();
        registerListeners();
        updateSmsEnabledState();
        updateSMSCPref();
    }

    private void updateSmsEnabledState() {
        // Show the right pref (SMS Disabled or SMS Enabled)
        PreferenceScreen prefRoot = (PreferenceScreen)findPreference("pref_key_root");
        final PackageManager packageManager = getPackageManager();
        final String smsAppPackage = Telephony.Sms.getDefaultSmsPackage(this);
        ApplicationInfo smsAppInfo = null;
        String mSmsAppName = "Messaging";
        try {
            smsAppInfo = packageManager.getApplicationInfo(smsAppPackage, 0);
            mSmsAppName = smsAppInfo.loadLabel(packageManager).toString();
        } catch (NameNotFoundException e) {
        }

        if (!mIsSmsEnabled) {
            prefRoot.addPreference(mSmsDisabledPref);
            prefRoot.removePreference(mSmsEnabledPref);
            mSmsDisabledPref.setSummary(mSmsAppName);
        } else {
            prefRoot.removePreference(mSmsDisabledPref);
            prefRoot.addPreference(mSmsEnabledPref);
            mSmsEnabledPref.setSummary(mSmsAppName);
        }

        // Enable or Disable the settings as appropriate
        mStoragePrefCategory.setEnabled(mIsSmsEnabled);
        mSmsPrefCategory.setEnabled(mIsSmsEnabled);
        mMmsPrefCategory.setEnabled(mIsSmsEnabled);
    }

    @Override
    protected void onPause() {
        super.onPause();
        unregisterReceiver(mReceiver);

        if (mAsyncDialog != null) {
            mAsyncDialog.clearPendingProgressDialog();
        }
    }

    private void loadPrefs() {
        if (getResources().getBoolean(R.bool.def_custom_preferences_settings)) {
            addPreferencesFromResource(R.xml.custom_preferences);
        } else {
            addPreferencesFromResource(R.xml.preferences);
        }

        mSmsDisabledPref = findPreference("pref_key_sms_disabled");
        mSmsEnabledPref = findPreference("pref_key_sms_enabled");

        mStoragePrefCategory = (PreferenceCategory)findPreference("pref_key_storage_settings");
        mSmsPrefCategory = (PreferenceCategory)findPreference("pref_key_sms_settings");
        mMmsPrefCategory = (PreferenceCategory)findPreference("pref_key_mms_settings");
        mNotificationPrefCategory =
                (PreferenceCategory)findPreference("pref_key_notification_settings");

        mMemoryStatusPref = findPreference("pref_key_memory_status");
        mCellBroadcsatsPref = findPreference("pref_key_cell_broadcasts");
        mManageSimPref = findPreference("pref_key_manage_sim_messages");
        mManageSim1Pref = findPreference("pref_key_manage_sim_messages_slot1");
        mManageSim2Pref = findPreference("pref_key_manage_sim_messages_slot2");
        mSmsLimitPref = findPreference("pref_key_sms_delete_limit");
        mSmsDeliveryReportPref = findPreference("pref_key_sms_delivery_reports");
        mSmsDeliveryReportPrefSub1 = findPreference("pref_key_sms_delivery_reports_slot1");
        mSmsDeliveryReportPrefSub2 = findPreference("pref_key_sms_delivery_reports_slot2");
        mMmsDeliveryReportPref = findPreference("pref_key_mms_delivery_reports");
        mMmsGroupMmsPref = findPreference("pref_key_mms_group_mms");
        mMmsReadReportPref = findPreference("pref_key_mms_read_reports");
        mMmsLimitPref = findPreference("pref_key_mms_delete_limit");
        mClearHistoryPref = findPreference("pref_key_mms_clear_history");
        mEnableNotificationsPref = (CheckBoxPreference) findPreference(NOTIFICATION_ENABLED);
        mMmsAutoRetrievialPref = (CheckBoxPreference) findPreference(AUTO_RETRIEVAL);
        mMmsExpiryPref = (ListPreference) findPreference("pref_key_mms_expiry");
        mMmsExpiryCard1Pref = (ListPreference) findPreference("pref_key_mms_expiry_slot1");
        mMmsExpiryCard2Pref = (ListPreference) findPreference("pref_key_mms_expiry_slot2");
        mMmsCreationModePref = (ListPreference) findPreference("pref_key_creation_mode");
        mSmsSignaturePref = (CheckBoxPreference) findPreference("pref_key_enable_signature");
        mSmsSignatureEditPref = (EditTextPreference) findPreference("pref_key_edit_signature");
        mVibratePref = (SwitchPreference) findPreference(NOTIFICATION_VIBRATE);
        Vibrator vibrator = (Vibrator) getSystemService(Context.VIBRATOR_SERVICE);
        if (mVibratePref != null && (vibrator == null || !vibrator.hasVibrator())) {
            mNotificationPrefCategory.removePreference(mVibratePref);
            mVibratePref = null;
        }
        PreferenceScreen prefRoot = (PreferenceScreen) findPreference("pref_key_root");
        prefRoot.removePreference(mSmsPrefCategory);
        prefRoot.removePreference(mMmsPrefCategory);
        mNotificationPrefCategory.removePreference(mEnableNotificationsPref);
        mRingtonePref = (RingtonePreference) findPreference(NOTIFICATION_RINGTONE);
        mSmsTemplate = findPreference("pref_key_message_template");
        mSmsValidityPref = (ListPreference) findPreference("pref_key_sms_validity_period");
        mSmsValidityCard1Pref
            = (ListPreference) findPreference("pref_key_sms_validity_period_slot1");
        mSmsValidityCard2Pref
            = (ListPreference) findPreference("pref_key_sms_validity_period_slot2");
        // ConfigurationClient
        if((MmsConfig.isOMACPEnabled(MessagingPreferenceActivity.this))) {
            mConfigurationmessage = findPreference(CONFIGURATION_MESSAGE);
        } else {
            PreferenceCategory OMACPConCategory =
                    (PreferenceCategory) findPreference(OMACP_CONFIGURATION_CATEGORY);
            prefRoot.removePreference(OMACPConCategory);
        }

        mMmsSizeLimit = (Preference) findPreference("pref_key_mms_size_limit");

        if (getResources().getBoolean(R.bool.def_custom_preferences_settings)) {
            mCBsettingPref = findPreference(CELL_BROADCAST);
            mFontSizePref = (ListPreference) findPreference(FONT_SIZE_SETTING);
        }
        //Chat wallpaper
        if (getResources().getBoolean(R.bool.def_custom_preferences_settings)) {
            mChatWallpaperPref = findPreference(CHAT_WALLPAPER_SETTING);
        }

        setMessagePreferences();
        prefRoot.removePreference(mNotificationPrefCategory);
    }

    private void restoreDefaultPreferences() {
        PreferenceManager.getDefaultSharedPreferences(this).edit().clear().apply();
        setPreferenceScreen(null);
        // Reset the SMSC preference.
        mSmscPrefList.clear();
        mSmscPrefCate.removeAll();
        loadPrefs();
        updateSmsEnabledState();

        // NOTE: After restoring preferences, the auto delete function (i.e. message recycler)
        // will be turned off by default. However, we really want the default to be turned on.
        // Because all the prefs are cleared, that'll cause:
        // ConversationList.runOneTimeStorageLimitCheckForLegacyMessages to get executed the
        // next time the user runs the Messaging app and it will either turn on the setting
        // by default, or if the user is over the limits, encourage them to turn on the setting
        // manually.
    }

    private void setMessagePreferences() {
        updateSignatureStatus();

        mSmscPrefCate = (PreferenceCategory) findPreference("pref_key_smsc");
        showSmscPref();

        PreferenceScreen keyRoot = (PreferenceScreen)findPreference("pref_key_root");
        keyRoot.removePreference(mSmscPrefCate);
        // Set SIM card SMS management preference
        updateSIMSMSPref();

        if (!MmsConfig.getSMSDeliveryReportsEnabled()) {
            mSmsPrefCategory.removePreference(mSmsDeliveryReportPref);
            mSmsPrefCategory.removePreference(mSmsDeliveryReportPrefSub1);
            mSmsPrefCategory.removePreference(mSmsDeliveryReportPrefSub2);
            if (!MmsApp.getApplication().getTelephonyManager().hasIccCard()) {
                getPreferenceScreen().removePreference(mSmsPrefCategory);
            }
        } else {
            if (MessageUtils.isMultiSimEnabledMms()) {
                mSmsPrefCategory.removePreference(mSmsDeliveryReportPref);
                if (!MessageUtils.isIccCardActivated(MessageUtils.SUB1)) {
                    mSmsDeliveryReportPrefSub1.setEnabled(false);
                    // FIXME: Comment this framework dependency at bring up stage, will restore
                    //        back later.
                    //displaySimLabel(false, PREFERENCE_TYPE_DELIVERY_REPORT, MessageUtils.SUB1);
                }
                if (!MessageUtils.isIccCardActivated(MessageUtils.SUB2)) {
                    mSmsDeliveryReportPrefSub2.setEnabled(false);
                    //displaySimLabel(false, PREFERENCE_TYPE_DELIVERY_REPORT, MessageUtils.SUB2);
                }
                if (MessageUtils.isIccCardActivated(MessageUtils.SUB1)) {
                    //displaySimLabel(true, PREFERENCE_TYPE_DELIVERY_REPORT, MessageUtils.SUB1);
                }
                if (MessageUtils.isIccCardActivated(MessageUtils.SUB2)) {
                    //displaySimLabel(true, PREFERENCE_TYPE_DELIVERY_REPORT, MessageUtils.SUB2);
                }
            } else {
                mSmsPrefCategory.removePreference(mSmsDeliveryReportPrefSub1);
                mSmsPrefCategory.removePreference(mSmsDeliveryReportPrefSub2);
            }
        }

        setEnabledNotificationsPref();

        // If needed, migrate vibration setting from the previous tri-state setting stored in
        // NOTIFICATION_VIBRATE_WHEN to the boolean setting stored in NOTIFICATION_VIBRATE.
        SharedPreferences sharedPreferences = PreferenceManager.getDefaultSharedPreferences(this);
        if (sharedPreferences.contains(NOTIFICATION_VIBRATE_WHEN)) {
            String vibrateWhen = sharedPreferences.
                    getString(MessagingPreferenceActivity.NOTIFICATION_VIBRATE_WHEN, null);
            boolean vibrate = "always".equals(vibrateWhen);
            SharedPreferences.Editor prefsEditor = sharedPreferences.edit();
            prefsEditor.putBoolean(NOTIFICATION_VIBRATE, vibrate);
            prefsEditor.remove(NOTIFICATION_VIBRATE_WHEN);  // remove obsolete setting
            prefsEditor.apply();
            mVibratePref.setChecked(vibrate);
        }

        mSmsRecycler = Recycler.getSmsRecycler();
        mMmsRecycler = Recycler.getMmsRecycler();

        // Fix up the recycler's summary with the correct values
        setSmsDisplayLimit();
        setMmsDisplayLimit();

        String soundValue = sharedPreferences.getString(NOTIFICATION_RINGTONE, null);
        setRingtoneSummary(soundValue);
    }

    private void setMmsRelatedPref() {
        if (!MmsConfig.getMmsEnabled()) {
            // No Mms, remove all the mms-related preferences
            getPreferenceScreen().removePreference(mMmsPrefCategory);

            mStoragePrefCategory.removePreference(findPreference("pref_key_mms_delete_limit"));
        } else {
            if (!MmsConfig.getMMSDeliveryReportsEnabled()) {
                mMmsPrefCategory.removePreference(mMmsDeliveryReportPref);
            }
            if (!MmsConfig.getMMSReadReportsEnabled()) {
                mMmsPrefCategory.removePreference(mMmsReadReportPref);
            }
            // If the phone's SIM doesn't know it's own number, disable group mms.
            if (!MmsConfig.getGroupMmsEnabled() ||
                    TextUtils.isEmpty(MessageUtils.getLocalNumber())) {
                mMmsPrefCategory.removePreference(mMmsGroupMmsPref);
            }
            if (!MmsConfig.isCreationModeEnabled()) {
                mMmsPrefCategory.removePreference(mMmsCreationModePref);
            }
            if (!getResources().getBoolean(
                    R.bool.def_custom_preferences_settings)
                    && !(getResources().getBoolean(R.bool.def_show_mms_size))) {
                mMmsPrefCategory.removePreference(mMmsSizeLimit);
            } else {
                setMmsSizeSummary();
            }
        }

        if (MessageUtils.isMultiSimEnabledMms()) {
            if(MessageUtils.getActivatedIccCardCount() <
                    ConstantsWrapper.Phone.MAX_PHONE_COUNT_DUAL_SIM) {
                int subId = SmsManager.getDefault().getDefaultSmsSubscriptionId();
                int phoneId = SubscriptionManagerWrapper.getPhoneId(subId);
                mManageSimPref.setSummary(
                        getString(R.string.pref_summary_manage_sim_messages_slot,
                                phoneId + 1));
            } else {
                mManageSimPref.setSummary(
                        getString(R.string.pref_summary_manage_sim_messages));
            }
            mMmsPrefCategory.removePreference(mMmsExpiryPref);
        } else {
            mMmsPrefCategory.removePreference(mMmsExpiryCard1Pref);
            mMmsPrefCategory.removePreference(mMmsExpiryCard2Pref);
        }
    }

    private void setSmsValidityPeriodPref() {
        PreferenceCategory storageOptions =
                (PreferenceCategory)findPreference("pref_key_sms_settings");
        if (getResources().getBoolean(R.bool.config_sms_validity)) {
            if (MessageUtils.isMultiSimEnabledMms()) {
                storageOptions.removePreference(mSmsValidityPref);
                if (!MessageUtils.isIccCardActivated(MessageUtils.SUB1)) {
                    mSmsValidityCard1Pref.setEnabled(false);
                    // FIXME: Comment this framework dependency at bring up stage, will restore
                    //        back later.
                    //displaySimLabel(false, PREFERENCE_TYPE_SMS_VALIDITY, MessageUtils.SUB1);
                } else {
                    setSmsPreferValiditySummary(MessageUtils.SUB1);
                    //displaySimLabel(true, PREFERENCE_TYPE_SMS_VALIDITY, MessageUtils.SUB1);
                }
                if (!MessageUtils.isIccCardActivated(MessageUtils.SUB2)) {
                    mSmsValidityCard2Pref.setEnabled(false);
                    // displaySimLabel(false, PREFERENCE_TYPE_SMS_VALIDITY, MessageUtils.SUB2);
                } else {
                    setSmsPreferValiditySummary(MessageUtils.SUB2);
                    //displaySimLabel(true, PREFERENCE_TYPE_SMS_VALIDITY, MessageUtils.SUB2);
                }
            } else {
                storageOptions.removePreference(mSmsValidityCard1Pref);
                storageOptions.removePreference(mSmsValidityCard2Pref);
                setSmsPreferValiditySummary(MessageUtils.SUB_INVALID);
            }
        } else {
            storageOptions.removePreference(mSmsValidityPref);
            storageOptions.removePreference(mSmsValidityCard1Pref);
            storageOptions.removePreference(mSmsValidityCard2Pref);
        }
    }

    private void setRingtoneSummary(String soundValue) {
        Uri soundUri = TextUtils.isEmpty(soundValue) ? null : Uri.parse(soundValue);
        Ringtone tone = soundUri != null ? RingtoneManager.getRingtone(this, soundUri) : null;
        mRingtonePref.setSummary(tone != null ? tone.getTitle(this)
                : getResources().getString(R.string.silent_ringtone));
    }

    private void showSmscPref() {
        int count = MessageUtils.getPhoneCount();
        for (int i = 0; i < count; i++) {
            final Preference pref = new Preference(this);
            pref.setKey(String.valueOf(i));
            pref.setTitle(getSMSCDialogTitle(count, i));
            if (getResources().getBoolean(R.bool.show_edit_smsc)) {
                pref.setOnPreferenceClickListener(null);
            } else {
                pref.setOnPreferenceClickListener(new OnPreferenceClickListener() {
                    @Override
                    public boolean onPreferenceClick(Preference preference) {
                        MyEditDialogFragment dialog = MyEditDialogFragment.newInstance(
                            MessagingPreferenceActivity.this,
                            preference.getTitle(),
                            preference.getSummary(),
                            Integer.valueOf(preference.getKey()));
                        dialog.show(getFragmentManager(), "dialog");
                        return true;
                    }
                });
            }
            mSmscPrefCate.addPreference(pref);
            mSmscPrefList.add(pref);
        }
        updateSMSCPref();
    }

    private void updateSIMSMSPref() {
        if (MessageUtils.isMultiSimEnabledMms()) {
            if (!MessageUtils.isIccCardActivated(MessageUtils.SUB1)) {
                mSmsPrefCategory.removePreference(mManageSim1Pref);
                // FIXME: Comment this framework dependency at bring up stage, will restore
                //        back later.
                //displaySimLabel(false, PREFERENCE_TYPE_MANAGE_SIM_CARD, MessageUtils.SUB1);
            } else {
                mSmsPrefCategory.addPreference(mManageSim1Pref);
                //displaySimLabel(true, PREFERENCE_TYPE_MANAGE_SIM_CARD, MessageUtils.SUB1);
            }
            if (!MessageUtils.isIccCardActivated(MessageUtils.SUB2)) {
                mSmsPrefCategory.removePreference(mManageSim2Pref);
                //displaySimLabel(false, PREFERENCE_TYPE_MANAGE_SIM_CARD, MessageUtils.SUB2);
            } else {
                mSmsPrefCategory.addPreference(mManageSim2Pref);
                //displaySimLabel(true, PREFERENCE_TYPE_MANAGE_SIM_CARD, MessageUtils.SUB2);
            }
            mSmsPrefCategory.removePreference(mManageSimPref);
        } else {
            if (!MessageUtils.hasIccCard()) {
                mSmsPrefCategory.removePreference(mManageSimPref);
            } else {
                mSmsPrefCategory.addPreference(mManageSimPref);
            }
            mSmsPrefCategory.removePreference(mManageSim1Pref);
            mSmsPrefCategory.removePreference(mManageSim2Pref);
        }
    }

    private boolean isAirPlaneModeOn() {
        return Settings.System.getInt(getContentResolver(),
                Settings.System.AIRPLANE_MODE_ON, 0) != 0;
    }

    private String getSMSCDialogTitle(int count, int index) {
        String title = MessageUtils.isMultiSimEnabledMms()
                ? getString(R.string.pref_more_smcs, index + 1)
                : getString(R.string.pref_one_smcs);
        // check for operator custom label feature
        /*if (TelephonyManager.getDefault().isMultiSimEnabled()) {
            OperatorSimInfo operatorSimInfo = new OperatorSimInfo(getApplicationContext());
            if (operatorSimInfo.isOperatorFeatureEnabled()) {
                String simCustomLabel = checkForOperatorCustomLabel(index);
                if (simCustomLabel == null || simCustomLabel.equals("")) {
                    simCustomLabel = "SIM"+(index + 1);
                }
                title = getResources().getString(
                            R.string.pref_more_smcs, simCustomLabel);
            }
        }*/
        return title;
    }

    private void setSmsPreferValiditySummary(int subscription) {
        switch (subscription) {
            case MessageUtils.SUB_INVALID:
                mSmsValidityPref.setOnPreferenceChangeListener(
                        new Preference.OnPreferenceChangeListener() {
                    public boolean onPreferenceChange(Preference preference, Object newValue) {
                        final String summary = newValue.toString();
                        int index = mSmsValidityPref.findIndexOfValue(summary);
                        mSmsValidityPref.setSummary(mSmsValidityPref.getEntries()[index]);
                        mSmsValidityPref.setValue(summary);
                        return true;
                    }
                });
                mSmsValidityPref.setSummary(mSmsValidityPref.getEntry());
                break;
            case MessageUtils.SUB1:
                mSmsValidityCard1Pref.setOnPreferenceChangeListener(
                        new Preference.OnPreferenceChangeListener() {
                    public boolean onPreferenceChange(Preference preference, Object newValue) {
                        final String summary = newValue.toString();
                        int index = mSmsValidityCard1Pref.findIndexOfValue(summary);
                        mSmsValidityCard1Pref.setSummary(mSmsValidityCard1Pref.getEntries()[index]);
                        mSmsValidityCard1Pref.setValue(summary);
                        return true;
                    }
                });
                mSmsValidityCard1Pref.setSummary(mSmsValidityCard1Pref.getEntry());
                break;
            case MessageUtils.SUB2:
                mSmsValidityCard2Pref.setOnPreferenceChangeListener(
                        new Preference.OnPreferenceChangeListener() {
                    public boolean onPreferenceChange(Preference preference, Object newValue) {
                        final String summary = newValue.toString();
                        int index = mSmsValidityCard2Pref.findIndexOfValue(summary);
                        mSmsValidityCard2Pref.setSummary(mSmsValidityCard2Pref.getEntries()[index]);
                        mSmsValidityCard2Pref.setValue(summary);
                        return true;
                    }
                });
                mSmsValidityCard2Pref.setSummary(mSmsValidityCard2Pref.getEntry());
                break;
            default:
                break;
        }
    }

    private void setEnabledNotificationsPref() {
        // The "enable notifications" setting is really stored in our own prefs. Read the
        // current value and set the checkbox to match.
        mEnableNotificationsPref.setChecked(getNotificationEnabled(this));
    }

    private void setSmsDisplayLimit() {
        mSmsLimitPref.setSummary(
                getString(R.string.pref_summary_delete_limit,
                        mSmsRecycler.getMessageLimit(this)));
    }

    private void setMmsDisplayLimit() {
        mMmsLimitPref.setSummary(
                getString(R.string.pref_summary_delete_limit,
                        mMmsRecycler.getMessageLimit(this)));
    }

    private void setMmsExpiryPref() {
        PreferenceCategory mmsSettings =
                (PreferenceCategory)findPreference("pref_key_mms_settings");
        if (getResources().getBoolean(R.bool.config_mms_validity)) {
            if (MessageUtils.isMultiSimEnabledMms()) {
                mmsSettings.removePreference(mMmsExpiryPref);
                if (!MessageUtils.isIccCardActivated(MessageUtils.SUB1)) {
                    mMmsExpiryCard1Pref.setEnabled(false);
                    // FIXME: Comment this framework dependency at bring up stage, will restore
                    //        back later.
                    //displaySimLabel(false, PREFERENCE_TYPE_MMS_VALIDITY, MessageUtils.SUB1);
                } else {
                    setMmsExpirySummary(ConstantsWrapper.Phone.SUB1);
                    //displaySimLabel(true, PREFERENCE_TYPE_MMS_VALIDITY, MessageUtils.SUB1);
                }
                if (!MessageUtils.isIccCardActivated(MessageUtils.SUB2)) {
                    mMmsExpiryCard2Pref.setEnabled(false);
                    //displaySimLabel(false, PREFERENCE_TYPE_MMS_VALIDITY, MessageUtils.SUB2);
                } else {
                    setMmsExpirySummary(ConstantsWrapper.Phone.SUB2);
                    //displaySimLabel(true, PREFERENCE_TYPE_MMS_VALIDITY, MessageUtils.SUB2);
                }
            } else {
                mmsSettings.removePreference(mMmsExpiryCard1Pref);
                mmsSettings.removePreference(mMmsExpiryCard2Pref);
                setMmsExpirySummary(MessageUtils.SUB_INVALID);
            }
        } else {
            mmsSettings.removePreference(mMmsExpiryPref);
            mmsSettings.removePreference(mMmsExpiryCard1Pref);
            mmsSettings.removePreference(mMmsExpiryCard2Pref);
        }
    }

    private void setMmsExpirySummary(int subscription) {
        switch (subscription) {
            case MessageUtils.SUB_INVALID:
                mMmsExpiryPref.setOnPreferenceChangeListener(
                        new Preference.OnPreferenceChangeListener() {
                    public boolean onPreferenceChange(Preference preference, Object newValue) {
                        final String value = newValue.toString();
                        int index = mMmsExpiryPref.findIndexOfValue(value);
                        mMmsExpiryPref.setValue(value);
                        mMmsExpiryPref.setSummary(mMmsExpiryPref.getEntries()[index]);
                        return false;
                    }
                });
                mMmsExpiryPref.setSummary(mMmsExpiryPref.getEntry());
                break;
            case ConstantsWrapper.Phone.SUB1:
                mMmsExpiryCard1Pref.setOnPreferenceChangeListener(
                        new Preference.OnPreferenceChangeListener() {
                    public boolean onPreferenceChange(Preference preference, Object newValue) {
                        final String value = newValue.toString();
                        int index = mMmsExpiryCard1Pref.findIndexOfValue(value);
                        mMmsExpiryCard1Pref.setValue(value);
                        mMmsExpiryCard1Pref.setSummary(mMmsExpiryCard1Pref.getEntries()[index]);
                        return false;
                    }
                });
                mMmsExpiryCard1Pref.setSummary(mMmsExpiryCard1Pref.getEntry());
                break;
            case ConstantsWrapper.Phone.SUB2:
                mMmsExpiryCard2Pref.setOnPreferenceChangeListener(
                        new Preference.OnPreferenceChangeListener() {
                    public boolean onPreferenceChange(Preference preference, Object newValue) {
                        final String value = newValue.toString();
                        int index = mMmsExpiryCard2Pref.findIndexOfValue(value);
                        mMmsExpiryCard2Pref.setValue(value);
                        mMmsExpiryCard2Pref.setSummary(mMmsExpiryCard2Pref.getEntries()[index]);
                        return false;
                    }
                });
                mMmsExpiryCard2Pref.setSummary(mMmsExpiryCard2Pref.getEntry());
                break;
            default:
                break;
        }
    }

    private void updateSignatureStatus() {
        // If the signature CheckBox is checked, we should set the signature EditText
        // enable, and disable when it's not checked.
        boolean isChecked = mSmsSignaturePref.isChecked();
        mSmsSignatureEditPref.setEnabled(isChecked);
    }

    public boolean onCreateOptionsMenu(Menu menu) {
        super.onCreateOptionsMenu(menu);
        menu.clear();
        if (mIsSmsEnabled) {
            menu.add(0, MENU_RESTORE_DEFAULTS, 0, R.string.restore_default);
        }
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case MENU_RESTORE_DEFAULTS:
                restoreDefaultPreferences();
                return true;

            case android.R.id.home:
                // The user clicked on the Messaging icon in the action bar. Take them back from
                // wherever they came from
                finish();
                return true;
        }
        return false;
    }

    @Override
    public boolean onPreferenceTreeClick(PreferenceScreen preferenceScreen,
            Preference preference) {
        if (preference == mSmsLimitPref) {
            NumberPickerDialog dialog = new NumberPickerDialog(
                    MessagingPreferenceActivity.this,
                    mSmsLimitListener,
                    mSmsRecycler.getMessageLimit(this),
                    mSmsRecycler.getMessageMinLimit(),
                    mSmsRecycler.getMessageMaxLimit(),
                    R.string.pref_title_sms_delete);
            if (!MessagingPreferenceActivity.this.isFinishing()) {
                dialog.show();
            }
        } else if (preference == mMmsLimitPref) {
            NumberPickerDialog dialog = new NumberPickerDialog(
                    MessagingPreferenceActivity.this,
                    mMmsLimitListener,
                    mMmsRecycler.getMessageLimit(this),
                    mMmsRecycler.getMessageMinLimit(),
                    mMmsRecycler.getMessageMaxLimit(),
                    R.string.pref_title_mms_delete);
            if (!MessagingPreferenceActivity.this.isFinishing()) {
                dialog.show();
            }
        } else if (preference == mSmsTemplate) {
            startActivity(new Intent(this, MessageTemplate.class));
        } else if (preference == mManageSimPref) {
            startActivity(new Intent(this, ManageSimMessages.class));
        } else if (preference == mManageSim1Pref) {
            Intent intent = new Intent(this, ManageSimMessages.class);
            intent.putExtra(ConstantsWrapper.Phone.SLOT_KEY, ConstantsWrapper.Phone.SUB1);
            startActivity(intent);
        } else if (preference == mManageSim2Pref) {
            Intent intent = new Intent(this, ManageSimMessages.class);
            intent.putExtra(ConstantsWrapper.Phone.SLOT_KEY, ConstantsWrapper.Phone.SUB2);
            startActivity(intent);
        } else if (preference == mClearHistoryPref) {
            if (!MessagingPreferenceActivity.this.isFinishing()) {
                showDialog(CONFIRM_CLEAR_SEARCH_HISTORY_DIALOG);
            }
            return true;
        } else if (preference == mEnableNotificationsPref) {
            // Update the actual "enable notifications" value that is stored in secure settings.
            enableNotifications(mEnableNotificationsPref.isChecked(), this);
        } else if (preference == mSmsSignaturePref) {
            updateSignatureStatus();
        } else if (preference == mMmsAutoRetrievialPref) {
            if (mMmsAutoRetrievialPref.isChecked()) {
                startMmsDownload();
            }
        // ConfigurationClient
        } else if (preference == mConfigurationmessage) {
            try {
                Intent intent = new Intent(ACTION_CONFIGURE_MESSAGE);
                startActivity(intent);
            } catch (ActivityNotFoundException e) {
                Log.e(TAG,"Activity not found : "+e);
            }
        } else if (preference == mMemoryStatusPref) {
             MessageUtils.showMemoryStatusDialog(MessagingPreferenceActivity.this);
        } else if (preference == mCellBroadcsatsPref) {
            try {
                startActivity(MessageUtils.getCellBroadcastIntent());
            } catch (ActivityNotFoundException e) {
                Log.e(TAG,
                        "ActivityNotFoundException for CellBroadcastListActivity");
            }
        } else if (getResources().getBoolean(
                R.bool.def_custom_preferences_settings)
                && preference == mCBsettingPref) {
            try {
                startActivity(MessageUtils.getCellBroadcastIntent());
            } catch (ActivityNotFoundException e) {
                Log.e(TAG,
                        "ActivityNotFoundException for CellBroadcastListActivity");
            }
        } else if (getResources().getBoolean(
                R.bool.def_custom_preferences_settings)
                && preference == mChatWallpaperPref) {
            setChatWallpaper();
        } else if (preference == findPreference("pref_key_sms_text_settings")) {
            startActivity(new Intent(this, SmsPreferenceActivity.class));
        } else if (preference == findPreference("pref_key_mms_text_settings")) {
            startActivity(new Intent(this, MmsPreferenceActivity.class));
        }

        return super.onPreferenceTreeClick(preferenceScreen, preference);
    }

    /**
     * Trigger the TransactionService to download any outstanding messages.
     */
    private void startMmsDownload() {
        startService(new Intent(TransactionService.ACTION_ENABLE_AUTO_RETRIEVE, null, this,
                TransactionService.class));
    }

    NumberPickerDialog.OnNumberSetListener mSmsLimitListener =
        new NumberPickerDialog.OnNumberSetListener() {
            public void onNumberSet(int limit) {
                mSmsRecycler.setMessageLimit(MessagingPreferenceActivity.this, limit);
                setSmsDisplayLimit();
                getAsyncDialog().runAsync(new Runnable() {
                    @Override
                    public void run() {
                        if (mSmsRecycler
                                .checkForThreadsOverLimit(MessagingPreferenceActivity.this)) {
                            mSmsRecycler.deleteOldMessages(MessagingPreferenceActivity.this);
                        }
                    }
                }, null, R.string.pref_title_auto_delete);
            }
    };

    NumberPickerDialog.OnNumberSetListener mMmsLimitListener =
        new NumberPickerDialog.OnNumberSetListener() {
            public void onNumberSet(int limit) {
                mMmsRecycler.setMessageLimit(MessagingPreferenceActivity.this, limit);
                setMmsDisplayLimit();
                getAsyncDialog().runAsync(new Runnable() {
                    @Override
                    public void run() {
                        if (mMmsRecycler
                                .checkForThreadsOverLimit(MessagingPreferenceActivity.this)) {
                            mMmsRecycler.deleteOldMessages(MessagingPreferenceActivity.this);
                        }
                    }
                }, null, R.string.pref_title_auto_delete);
            }
    };

    AsyncDialog getAsyncDialog() {
        if (mAsyncDialog == null) {
            mAsyncDialog = new AsyncDialog(this);
        }
        return mAsyncDialog;
    }

    @Override
    protected Dialog onCreateDialog(int id) {
        switch (id) {
            case CONFIRM_CLEAR_SEARCH_HISTORY_DIALOG:
                return new AlertDialog.Builder(MessagingPreferenceActivity.this)
                    .setTitle(R.string.confirm_clear_search_title)
                    .setMessage(R.string.confirm_clear_search_text)
                    .setPositiveButton(android.R.string.ok, new AlertDialog.OnClickListener() {
                        public void onClick(DialogInterface dialog, int which) {
                            SearchRecentSuggestions recent =
                                ((MmsApp)getApplication()).getRecentSuggestions();
                            if (recent != null) {
                                recent.clearHistory();
                            }
                            dialog.dismiss();
                        }
                    })
                    .setNegativeButton(android.R.string.cancel, null)
                    .setIconAttribute(android.R.attr.alertDialogIcon)
                    .create();
        }
        return super.onCreateDialog(id);
    }

    public static boolean getNotificationEnabled(Context context) {
        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);
        boolean notificationsEnabled =
            prefs.getBoolean(MessagingPreferenceActivity.NOTIFICATION_ENABLED, true);
        return notificationsEnabled;
    }

    public static void enableNotifications(boolean enabled, Context context) {
        // Store the value of notifications in SharedPreferences
        SharedPreferences.Editor editor =
            PreferenceManager.getDefaultSharedPreferences(context).edit();

        editor.putBoolean(MessagingPreferenceActivity.NOTIFICATION_ENABLED, enabled);

        editor.apply();
    }

    private void registerListeners() {
        mRingtonePref.setOnPreferenceChangeListener(this);
        final IntentFilter intentFilter =
                new IntentFilter(ConstantsWrapper.TelephonyIntent.ACTION_SIM_STATE_CHANGED);
        intentFilter.addAction(Intent.ACTION_AIRPLANE_MODE_CHANGED);
        registerReceiver(mReceiver, intentFilter);
    }

    public boolean onPreferenceChange(Preference preference, Object newValue) {
        boolean result = false;
        if (preference == mRingtonePref) {
            setRingtoneSummary((String)newValue);
            result = true;
        }
        return result;
    }


    private void showToast(int id) {
        Toast.makeText(this, id, Toast.LENGTH_SHORT).show();
    }

    /**
     * Set the SMSC preference enable or disable.
     *
     * @param id  the subscription of the slot, if the value is ALL_SUB, update all the SMSC
     *            preference
     * @param airplaneModeIsOn  the state of the airplane mode
     */
    private void setSMSCPrefState(int id, boolean prefEnabled) {
        // We need update the preference summary.
        if (prefEnabled) {
            Log.d(TAG, "get SMSC from sub= " + id);
            if (getResources().getBoolean(R.bool.def_enable_reset_smsc)) {
                updateSmscFromPreference(id);
            } else {
                final Message callback = mHandler.obtainMessage(EVENT_GET_SMSC_DONE);
                Bundle userParams = new Bundle();
                userParams.putInt(ConstantsWrapper.Phone.SLOT_KEY, id);
                callback.obj = userParams;
                MessageUtils.getSmscFromSub(this, id, callback);
            }
        } else {
            mSmscPrefList.get(id).setSummary(null);
        }
        mSmscPrefList.get(id).setEnabled(prefEnabled);
    }

    private void updateSMSCPref() {
        if (mSmscPrefList == null || mSmscPrefList.size() == 0) {
            return;
        }
        int count = MessageUtils.getPhoneCount();
        for (int i = 0; i < count; i++) {
            boolean isCDMA = false;
            int subId[] = SubscriptionManagerWrapper.getSubId(i);
            if (subId != null && subId.length != 0) {
                isCDMA = MessageUtils.isCDMAPhone(subId[0]);
            }
            setSMSCPrefState(i, !isCDMA && !isAirPlaneModeOn()
                    && MessageUtils.hasActivatedIccCard(i));
        }
    }

    private void updateSmscFromBundle(Bundle bundle) {
        if (bundle != null) {
            int sub = bundle.getInt(ConstantsWrapper.Phone.SLOT_KEY, -1);
            if (sub != -1) {
                String summary = bundle.getString(MessageUtils.EXTRA_SMSC, null);
                if (summary == null) {
                    return;
                }
                Log.d(TAG, "Update SMSC: sub= " + sub + " SMSC= " + summary);
                int end = summary.lastIndexOf("\"");
                mSmscPrefList.get(sub).setSummary((end > 0)? summary.substring(1, end) : summary);
            }
        }
    }

    private static final class SmscHandler extends Handler {
        MessagingPreferenceActivity mOwner;
        public SmscHandler(MessagingPreferenceActivity owner) {
            super(Looper.getMainLooper());
            mOwner = owner;
        }
        @Override
        public void handleMessage(Message msg) {
            Bundle bundle = (Bundle) msg.obj;
            if (bundle == null) {
                return;
            }
            Throwable exception = (Throwable) bundle.getSerializable(MessageUtils.EXTRA_EXCEPTION);
            boolean result = bundle.getBoolean(MessageUtils.EXTRA_SMSC_RESULT, false);
            if (!result || exception != null) {
                Log.d(TAG, "Error: " + exception + " result:" + result);
                mOwner.showToast(R.string.set_smsc_error);
                return;
            }

            Bundle userParams = (Bundle)bundle.getParcelable(MessageUtils.EXTRA_USEROBJ);
            if (userParams == null) {
                Log.d(TAG, "userParams = null");
                return;
            }
            switch (msg.what) {
                case EVENT_SET_SMSC_DONE:
                    Log.d(TAG, "Set SMSC successfully");
                    mOwner.showToast(R.string.set_smsc_success);
                    mOwner.updateSmscFromBundle(userParams);
                    break;
                case EVENT_GET_SMSC_DONE:
                    Log.d(TAG, "Get SMSC successfully");
                    int sub = userParams.getInt(ConstantsWrapper.Phone.SLOT_KEY, -1);
                    if (sub != -1) {
                        bundle.putInt(ConstantsWrapper.Phone.SLOT_KEY, sub);
                        mOwner.updateSmscFromBundle(bundle);
                    }
                    break;
                case EVENT_SET_SMSC_PREF_DONE:
                    int key = userParams.getInt(ConstantsWrapper.Phone.SLOT_KEY, -1);
                    if (key != -1) {
                        mOwner.updateSmscFromPreference(key);
                    }
                    break;
            }
        }
    }

    public static class MyEditDialogFragment extends DialogFragment {
        private MessagingPreferenceActivity mActivity;

        public static MyEditDialogFragment newInstance(MessagingPreferenceActivity activity,
                CharSequence title, CharSequence smsc, int sub) {
            MyEditDialogFragment dialog = new MyEditDialogFragment();
            dialog.mActivity = activity;

            Bundle args = new Bundle();
            args.putCharSequence(SMSC_DIALOG_TITLE, title);
            args.putCharSequence(SMSC_DIALOG_NUMBER, smsc);
            args.putInt(SMSC_DIALOG_SUB, sub);
            dialog.setArguments(args);
            return dialog;
        }

        @Override
        public Dialog onCreateDialog(Bundle savedInstanceState) {
            final int sub = getArguments().getInt(SMSC_DIALOG_SUB);
            if (null == mActivity) {
                mActivity = (MessagingPreferenceActivity) getActivity();
                dismiss();
            }
            final EditText edit = new EditText(mActivity);
            edit.setPadding(15, 15, 15, 15);
            edit.setText(getArguments().getCharSequence(SMSC_DIALOG_NUMBER));

            Dialog alert = new AlertDialog.Builder(mActivity)
                    .setTitle(getArguments().getCharSequence(SMSC_DIALOG_TITLE))
                    .setView(edit)
                    .setPositiveButton(android.R.string.ok, new DialogInterface.OnClickListener() {
                        public void onClick(DialogInterface dialog, int whichButton) {
                            MyAlertDialogFragment newFragment = MyAlertDialogFragment.newInstance(
                                    mActivity, sub, edit.getText().toString());
                            newFragment.show(getFragmentManager(), "dialog");
                            dismiss();
                        }
                    })
                    .setNegativeButton(android.R.string.cancel, null)
                    .setCancelable(true)
                    .create();
            alert.getWindow().setSoftInputMode(
                    WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE);
            return alert;
        }
    }

    /**
     * All subclasses of Fragment must include a public empty constructor. The
     * framework will often re-instantiate a fragment class when needed, in
     * particular during state restore, and needs to be able to find this
     * constructor to instantiate it. If the empty constructor is not available,
     * a runtime exception will occur in some cases during state restore.
     */
    public static class MyAlertDialogFragment extends DialogFragment {
        private MessagingPreferenceActivity mActivity;

        public static MyAlertDialogFragment newInstance(MessagingPreferenceActivity activity,
                                                        int sub, String smsc) {
            MyAlertDialogFragment dialog = new MyAlertDialogFragment();
            dialog.mActivity = activity;

            Bundle args = new Bundle();
            args.putInt(SMSC_DIALOG_SUB, sub);
            args.putString(SMSC_DIALOG_NUMBER, smsc);
            dialog.setArguments(args);
            return dialog;
        }

        @Override
        public Dialog onCreateDialog(Bundle savedInstanceState) {
            final int sub = getArguments().getInt(SMSC_DIALOG_SUB);
            final String displayedSMSC = getArguments().getString(SMSC_DIALOG_NUMBER);

            // When framework re-instantiate this fragment by public empty
            // constructor and call onCreateDialog(Bundle savedInstanceState) ,
            // we should make sure mActivity not null.
            if (null == mActivity) {
                mActivity = (MessagingPreferenceActivity) getActivity();
            }

            final String actualSMSC = mActivity.adjustSMSC(displayedSMSC);

            return new AlertDialog.Builder(mActivity)
                    .setIcon(android.R.drawable.ic_dialog_alert).setMessage(
                            R.string.set_smsc_confirm_message)
                    .setPositiveButton(android.R.string.ok, new DialogInterface.OnClickListener() {
                        public void onClick(DialogInterface dialog, int whichButton) {
                           Log.d(TAG, "set SMSC from sub= " +sub + " SMSC= " + displayedSMSC);
                           Bundle userParams = new Bundle();
                           userParams.putInt(ConstantsWrapper.Phone.SLOT_KEY, sub);
                           if (getResources().getBoolean(R.bool.def_enable_reset_smsc)) {
                                final Message callbackMessage = mHandler
                                        .obtainMessage(EVENT_SET_SMSC_PREF_DONE);
                               callbackMessage.obj = userParams;
                               putSmscIntoPref(mActivity,sub,displayedSMSC,callbackMessage);
                           } else {
                               final Message callback = mHandler.obtainMessage(EVENT_SET_SMSC_DONE);
                               userParams.putString(MessageUtils.EXTRA_SMSC,actualSMSC);
                               callback.obj = userParams;
                               MessageUtils.setSmscForSub(mActivity, sub, actualSMSC, callback);
                           }
                        }
                    })
                    .setNegativeButton(android.R.string.cancel, null)
                    .setCancelable(true)
                    .create();
        }
    }

    private String adjustSMSC(String smsc) {
        String actualSMSC = "\"" + smsc + "\"";
        return actualSMSC;
    }

    // For the group mms feature to be enabled, the following must be true:
    //  1. the feature is enabled in mms_config.xml (currently on by default)
    //  2. the feature is enabled in the mms settings page
    //  3. the SIM knows its own phone number
    public static boolean getIsGroupMmsEnabled(Context context) {
        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);
        boolean groupMmsPrefOn = prefs.getBoolean(
                MessagingPreferenceActivity.GROUP_MMS_MODE, true);
        return MmsConfig.getGroupMmsEnabled() &&
                groupMmsPrefOn &&
                !TextUtils.isEmpty(MessageUtils.getLocalNumber());
    }

    private void setChatWallpaper() {
        final String[] mWallpaperText = getResources().getStringArray(
                R.array.chat_wallpaper_chooser_options);
        Drawable cameraAppIcon = null;
        Drawable galleryAppIcon = null;
        final PackageManager packageManager = getPackageManager();
        try {
            cameraAppIcon = packageManager.getApplicationIcon(getResources()
                    .getString(
                            R.string.camera_packagename));
            galleryAppIcon = packageManager.getApplicationIcon(getResources()
                    .getString(
                            R.string.gallery_packagename));
        } catch (NameNotFoundException e) {
            e.printStackTrace();
        }

        final Object[] mWallpaperImage = new Drawable[] {cameraAppIcon,
            galleryAppIcon};

        final String imageField = "imageField";
        final String textField = "textField";
        AlertDialog.Builder wallpaperDialogBuilder = new AlertDialog.Builder(
                this);

        ArrayList<HashMap<String, Object>> wallpaper = new ArrayList<HashMap<String, Object>>();
        for (int i = 0; i < 2; i++) {
            HashMap<String, Object> hashMap = new HashMap<String, Object>();
            hashMap.put(imageField, mWallpaperImage[i]);
            hashMap.put(textField, mWallpaperText[i]);
            wallpaper.add(hashMap);
        }

        SimpleAdapter wallpaperDialogAdapter = new SimpleAdapter(
                MessagingPreferenceActivity.this, wallpaper,
                R.layout.wallpaper_chooser_dialog_item, new String[] {
                        imageField, textField
                }, new int[] {
                        R.id.wallpaper_item_imageview,
                        R.id.wallpaper_item_textview
                });

        wallpaperDialogAdapter.setViewBinder(new ViewBinder() {

            public boolean setViewValue(View view, Object data,
                    String textRepresentation) {
                if ((view instanceof ImageView) && (data instanceof Drawable))
                {
                    ImageView imageView = (ImageView) view;
                    Drawable drawable = (Drawable) data;
                    imageView.setImageDrawable(drawable);
                    return true;
                }
                return false;
            }
        });

        LayoutInflater mInflater = (LayoutInflater) getSystemService(LAYOUT_INFLATER_SERVICE);
        View layout = mInflater.inflate(R.layout.wallpaper_chooser_dialog,
                (ViewGroup) findViewById(R.id.forwallpaperchooser));

        GridView mGridView = (GridView) layout
                .findViewById(R.id.wallpaperchooserdialog);
        mGridView.setAdapter(wallpaperDialogAdapter);

        final AlertDialog wallpaperChooser = wallpaperDialogBuilder
                .setTitle(
                        getResources().getString(
                                R.string.dialog_wallpaper_title))
                .setView(layout).create();
        wallpaperChooser.show();
        mGridView.setOnItemClickListener(new OnItemClickListener() {

            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position,
                    long id) {
                switch (position) {
                    case PICK_FROM_CAMERA:
                        chooseFromCamera();
                        wallpaperChooser.dismiss();
                        break;
                    case PICK_FROM_GALLERY:
                        chooseFromGallery();
                        wallpaperChooser.dismiss();
                        break;
                }

            }
        });
    }

    private void chooseFromGallery() {
        Intent intent = new Intent(Intent.ACTION_PICK);
        intent.setType("image/*");
        startActivityForResult(
                Intent.createChooser(intent, getString(R.string.intent_chooser_dialog_title)),
                PICK_FROM_GALLERY);
    }

    private void chooseFromCamera() {
        final Intent intent = new Intent(MediaStore.ACTION_IMAGE_CAPTURE);
        File file = new File(Environment.getExternalStorageDirectory()
                + "/DCIM/", "image" + new Date().getTime() + ".png");
        Uri imgUri = Uri.fromFile(file);
        if(imgUri != null) {
            intent.putExtra(MediaStore.EXTRA_OUTPUT, imgUri);
            startActivityForResult(intent, PICK_FROM_CAMERA);
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode == RESULT_OK) {
            switch (requestCode) {
                case PICK_FROM_CAMERA:
                    if (data.getExtras() != null) {
                        Bitmap photo = data.getExtras().getParcelable("data");
                        String picturePath = getAbsolutePath(getImageUri(this,
                                photo));
                        PreferenceManager.getDefaultSharedPreferences(this)
                                .edit()
                                .putString(CHAT_WALLPAPER, picturePath)
                                .commit();
                    }
                    break;
                case PICK_FROM_GALLERY:
                    if (data != null) {
                        Uri selectedImage = data.getData();
                        String picturePath = getAbsolutePath(selectedImage);
                        PreferenceManager.getDefaultSharedPreferences(this)
                                .edit()
                                .putString(CHAT_WALLPAPER, picturePath)
                                .commit();
                    }
                    break;
                default:
                    break;
            }
        }
    }

    public Uri getImageUri(Context inContext, Bitmap inImage) {

        String filepath = Environment.getExternalStorageDirectory().getPath();
        File file = new File(filepath, "temp");

        if (!file.exists()) {
            file.mkdirs();
        }
        String fileName = null;
        try {
            fileName = file.getAbsolutePath() + "/"
                    + System.currentTimeMillis() + ".png";

            FileOutputStream ostream = new FileOutputStream(fileName);
            inImage.compress(Bitmap.CompressFormat.PNG, 100, ostream);
            ostream.close();
        } catch (FileNotFoundException e) {
            e.printStackTrace();
        } catch (IOException e) {
            e.printStackTrace();
        }
        return Uri.parse(fileName);
    }

    public String getAbsolutePath(Uri uri) {
        String[] filePathColumn = {
                MediaStore.Images.Media.DATA
        };
        Cursor cursor = null;
        cursor = getContentResolver().query(uri,
                filePathColumn, null, null, null);
        try {
            if (cursor != null && cursor.moveToFirst()) {
                int columnIndex = cursor
                        .getColumnIndexOrThrow(filePathColumn[0]);
                String picturePath = cursor.getString(columnIndex);
                return picturePath;
            }
        } catch (IllegalArgumentException e) {
            e.printStackTrace();
        } finally {
            if (cursor != null) {
                cursor.close();
            }
        }
        return null;
    }
    private void setMmsSizeSummary() {
        String mSize = "";
        if (getResources().getBoolean(R.bool.def_custom_preferences_settings)) {
            mSize = MmsConfig
                    .getMaxMessageSize() / MmsConfig.KB_IN_BYTES
                    + MmsConfig.KILO_BYTE;
        } else if (getResources().getBoolean(R.bool.def_show_mms_size)) {
            mSize = MmsConfig.getMaxMessageSize()
                    / (MmsConfig.KB_IN_BYTES * MmsConfig.KB_IN_BYTES)
                    + MmsConfig.MEGA_BYTE;
        }
        mMmsSizeLimit.setSummary(mSize);
    }

    private void setSmsPreferFontSummary() {
        if (getResources().getBoolean(R.bool.def_custom_preferences_settings)
                && mFontSizePref != null) {
            mFontSizePref
                    .setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
                        public boolean onPreferenceChange(
                                Preference preference, Object newValue) {
                            final String summary = newValue.toString();
                            int index = mFontSizePref.findIndexOfValue(summary);
                            mFontSizePref.setSummary(mFontSizePref.getEntries()[index]);
                            mFontSizePref.setValue(summary);
                            return true;
                        }
                    });
            mFontSizePref.setSummary(mFontSizePref.getEntry());
        }
    }

    private void updateSmscFromPreference(int sub) {
        String smsc = PreferenceManager.getDefaultSharedPreferences(this)
                .getString(SMSC_DEFAULT,
                        ""/*SmsManager.getDefault().getSmscAddressFromIcc()*/);
        if (sub != -1) {
            mSmscPrefList.get(sub).setSummary(smsc);
        }
    }

    private static void putSmscIntoPref(Context context, int sub, String smsc,
            Message callback) {

        SharedPreferences.Editor editor = PreferenceManager
                .getDefaultSharedPreferences(context).edit();
        editor.putString(SMSC_DEFAULT, smsc);
        editor.apply();
        editor.commit();

        Bundle params = new Bundle();
        params.putInt(ConstantsWrapper.Phone.SLOT_KEY, sub);
        params.putString(MessageUtils.EXTRA_SMSC, smsc);

        params.putParcelable(MessageUtils.EXTRA_USEROBJ, (Parcelable) callback.obj);
        callback.obj = params;

        if (callback.getTarget() != null) {
            callback.sendToTarget();
        }
    }

    // FIXME: Comment this framework dependency at bring up stage, will restore
    //        back later.
    /*private String checkForOperatorCustomLabel(int slotIndex) {
        String simLabel = null;
        OperatorSimInfo operatorSimInfo = new OperatorSimInfo(getApplicationContext());
        boolean isCustomSimFeatureEnabled = operatorSimInfo.isOperatorFeatureEnabled();
        if (isCustomSimFeatureEnabled) {
            boolean isSimTypeOperator = operatorSimInfo.isSimTypeOperator(slotIndex);
            if (isSimTypeOperator) {
                simLabel = operatorSimInfo.getOperatorDisplayName();
            } else {
                int subId = SubscriptionManager.getSubId(slotIndex)[0];
                String operatorName = TelephonyManager.from(getApplicationContext()).
                        getSimOperatorNameForSubscription(subId);
                simLabel = operatorName;
            }
        }
        return simLabel;
    }*/

    /* If SIM1/SIM2 is absent, then show base SIM label
       else display custom SIM label.*/
    // FIXME: Comment this framework dependency at bring up stage, will restore
    //        back later.
    /* private void displaySimLabel(boolean isSIMEnabled, int preferenceType, int slot) {
        OperatorSimInfo operatorSimInfo = new OperatorSimInfo(getApplicationContext());
        boolean isCustomSimFeatureEnabled = operatorSimInfo.isOperatorFeatureEnabled();
        if (isCustomSimFeatureEnabled) {
            Preference currentPref = getPref(slot, preferenceType);
            String simLabel = getSimLabel(isSIMEnabled, preferenceType, slot);
            currentPref.setTitle(simLabel);
            if (preferenceType == PREFERENCE_TYPE_MMS_VALIDITY) {
                ((ListPreference)currentPref).setDialogTitle(simLabel);
            }
        }
    }

    private Preference getPref(int slot, int prefType) {
        Preference pref = null;
        switch (prefType) {
            case PREFERENCE_TYPE_DELIVERY_REPORT:
                pref = (slot == MessageUtils.SUB1)
                        ? mSmsDeliveryReportPrefSub1 : mSmsDeliveryReportPrefSub2;
                break;
            case PREFERENCE_TYPE_SMS_VALIDITY:
                pref = (slot == MessageUtils.SUB1)
                        ? mSmsValidityCard1Pref : mSmsValidityCard2Pref;
                break;
            case PREFERENCE_TYPE_MANAGE_SIM_CARD:
                pref = (slot == MessageUtils.SUB1)
                        ? mManageSim1Pref : mManageSim2Pref;
                break;
            case PREFERENCE_TYPE_MMS_VALIDITY:
                pref = (slot == MessageUtils.SUB1)
                        ? mMmsExpiryCard1Pref : mMmsExpiryCard2Pref;
                break;
            default:
                break;
        }
        return pref;
    }

    private String getSimLabel(boolean isSIMEnabled, int prefType, int slot) {
        String[] defaultSimLabelsArray = {"SIM1", "SIM2"};
        String simCustomLabel = isSIMEnabled ?
                checkForOperatorCustomLabel(slot) : defaultSimLabelsArray[slot];
        int[] deliverReportsArray = {R.string.pref_title_sms_delivery_reports_slot1,
                R.string.pref_title_sms_delivery_reports_slot2};
        int[] smsValidityArray = {R.string.pref_title_sms_validity_period_slot1,
                R.string.pref_title_sms_validity_period_slot2};
        int[] manageSimCardArray = {R.string.pref_title_manage_sim_messages_slot1,
                R.string.pref_title_manage_sim_messages_slot2};
        int[] mmsValidityArray = {R.string.pref_title_mms_save_time_slot1,
                R.string.pref_title_mms_save_time_slot2};
        String label = "";
        switch (prefType) {
            case PREFERENCE_TYPE_DELIVERY_REPORT:
                label = getResources().getString(
                        deliverReportsArray[slot], simCustomLabel);
                break;
            case PREFERENCE_TYPE_SMS_VALIDITY:
                label = getResources().getString(
                        smsValidityArray[slot], simCustomLabel);
                break;
            case PREFERENCE_TYPE_MANAGE_SIM_CARD:
                label = getResources().getString(
                        manageSimCardArray[slot], simCustomLabel);
                break;
            case PREFERENCE_TYPE_MMS_VALIDITY:
                label = getResources().getString(
                        mmsValidityArray[slot], simCustomLabel);
                break;
            default:
                break;
        }
        return label;
    }*/

    @Override
    protected void onDestroy() {
        super.onDestroy();
        MessageUtils.removeDialogs();
    }
}
