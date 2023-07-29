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

import java.util.ArrayList;

import android.app.ActionBar;
import android.app.AlertDialog;
import android.app.Dialog;
import android.app.DialogFragment;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.Parcelable;
import android.preference.EditTextPreference;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceClickListener;
import android.preference.PreferenceActivity;
import android.preference.PreferenceManager;
import android.preference.PreferenceScreen;
import android.preference.SwitchPreference;
import android.provider.Settings;
import android.telephony.TelephonyManager;
import android.telephony.SubscriptionManager;
import android.util.Log;
import android.view.MenuItem;
import android.view.WindowManager;
import android.widget.EditText;
import android.widget.Toast;

import com.android.mms.R;
import com.android.mms.MmsConfig;
import com.android.mms.transaction.TransactionService;
import com.android.mmswrapper.ConstantsWrapper;
import com.android.mmswrapper.SubscriptionManagerWrapper;

public class SmsPreferenceActivity extends PreferenceActivity {
    private static String TAG = "SmsPreferenceActivity";

    public static final String SMS_DELIVERY_REPORT_MODE = "pref_key_sms_delivery_reports";

    public static final String SMS_DELIVERY_REPORT_NO_MULTI
            = "pref_key_sms_delivery_reports_no_multi";

    public static final String SMS_DELIVERY_REPORT_SIM1 = "pref_key_sms_delivery_reports_sim1";

    public static final String SMS_DELIVERY_REPORT_SIM2 = "pref_key_sms_delivery_reports_sim2";

    public static final String SMS_TEMPLATE = "pref_key_message_template";

    public static final String SIM_SIGNATURE = "pref_key_enable_signature";

    public static final String SMS_EDIT_SIGNATURE = "pref_key_edit_signature";

    public static final String VALIDITY = "pref_key_sms_validity_period";

    public static final String SMS_CENTER_NUMBER = "pref_key_sms_center_number";

    public static final String SMS_CDMA_PRIORITY = "pref_key_sms_cdma_priority";

    public static final String WAP_PUSH = "pref_key_enable_wap_push";

    public static final String SMSC_DEFAULT = "pref_key_default_smsc";

    public static final String SMSC_CENTER_NUMBER = "pref_key_msm_center_number";

    public static final String SMSC_CENTER_NUMBER_SS ="pref_key_sms_center_number_no_multi";

    private static String SMS_CENTER_NUMBER_SIM1 = "0";

    private static String SMS_CENTER_NUMBER_SIM2 = "1";

    private static String SMS_DELIVERY_REPORTS = "sms_delivery_reports";

    private static String TYPE_SMS = "sms";

    private static String MSG_TYPE = "msg_type";

    private PreferenceScreen prefSmsSettings;
    private Preference mSmsTemplatePref;
    private Preference mSmsDeliveryReportPref;
    private Preference mSmsDeliveryReportNoMultiPref;
    private Preference mSmsCenterNumber;
    private Preference mSmsCenterNumberSS;
    private Preference mSmsCenterNumberSim1;
    private Preference mSmsCenterNumberSim2;
    private SwitchPreference mSmsDeliveryReportSim1Pref;
    private SwitchPreference mSmsDeliveryReportSim2Pref;
    private SwitchPreference mSmsSignaturePref;
    private EditTextPreference mSmsSignatureEditPref;
    private Preference mManageSimPref;
    private Preference mManageSimNoMultiPref;
    private Preference mManageSim1Pref;
    private Preference mManageSim2Pref;
    private Preference mSmsValidityPref;
    private ListPreference mSmsValidityNoMultiPref;
    private ListPreference mSmsValiditysim1Pref;
    private ListPreference mSmsValiditysim2Pref;
    private ListPreference mSmsStorePrefSingleSim;
    private Preference mSmsStorePrefDualSim;
    private ListPreference mSmsStorePref1;
    private ListPreference mSmsStorePref2;
    private SwitchPreference mWapPushPref;
    private ArrayList<Preference> mSmscPrefList = new ArrayList<Preference>();
    private boolean mIsSmsEnabled;
    private static final String SMSC_DIALOG_TITLE = "title";
    private static final String SMSC_DIALOG_NUMBER = "smsc";
    private static final String SMSC_DIALOG_SUB = "sub";
    private static final int EVENT_SET_SMSC_DONE = 0;
    private static final int EVENT_GET_SMSC_DONE = 1;
    private static final int EVENT_SET_SMSC_PREF_DONE = 2;
    private static SmscHandler mHandler = null;
    private BroadcastReceiver mReceiver = new BroadcastReceiver() {
        public void onReceive(Context context, Intent intent) {
            final String action = intent.getAction();
            if (Intent.ACTION_AIRPLANE_MODE_CHANGED.equals(action)) {
                updateSMSCPref();
            }
        }
    };

    protected void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        mHandler = new SmscHandler(this);
        createResource();
        ActionBar actionBar = getActionBar();
        actionBar.setDisplayOptions(ActionBar.DISPLAY_SHOW_TITLE
                | ActionBar.DISPLAY_HOME_AS_UP);
    }

    private void createResource() {
        addPreferencesFromResource(R.xml.sms_preferences);
        prefSmsSettings = (PreferenceScreen) findPreference("pref_key_sms_settings");
        mSmsDeliveryReportPref = findPreference("pref_key_sms_delivery_reports");
        mSmsDeliveryReportNoMultiPref = (SwitchPreference)
                findPreference("pref_key_sms_delivery_reports_no_multi");
        mSmsDeliveryReportSim1Pref = (SwitchPreference)
                findPreference("pref_key_sms_delivery_reports_sim1");
        mSmsDeliveryReportSim2Pref = (SwitchPreference)
                findPreference("pref_key_sms_delivery_reports_sim2");
        mSmsTemplatePref = findPreference("pref_key_message_template");
        mSmsSignaturePref = (SwitchPreference) findPreference("pref_key_enable_signature");
        mSmsSignatureEditPref = (EditTextPreference) findPreference("pref_key_edit_signature");
        mManageSimPref = findPreference("pref_key_manage_sim_messages");
        mManageSimNoMultiPref = findPreference("pref_key_manage_sim_messages_no_multi");
        mManageSim1Pref = findPreference("pref_key_manage_sim_messages_slot1");
        mManageSim2Pref = findPreference("pref_key_manage_sim_messages_slot2");
        mSmsValidityPref = (Preference) findPreference("pref_key_sms_validity_period");
        mSmsValiditysim1Pref = (ListPreference) findPreference("pref_key_sms_validity_period_sim1");
        mSmsValiditysim2Pref = (ListPreference) findPreference("pref_key_sms_validity_period_sim2");
        mSmsValidityNoMultiPref = (ListPreference)
                findPreference("pref_key_sms_validity_period_no_multi");
        mWapPushPref = (SwitchPreference) findPreference("pref_key_enable_wap_push");
        mSmsCenterNumber = findPreference("pref_key_sms_center_number");
        mSmsCenterNumberSS = findPreference(SMSC_CENTER_NUMBER_SS);
        mSmsCenterNumberSim1 = findPreference(SMS_CENTER_NUMBER_SIM1);
        mSmsCenterNumberSim2 = findPreference(SMS_CENTER_NUMBER_SIM2);
        mSmsStorePrefSingleSim = (ListPreference) findPreference("pref_key_sms_store_single");
        mSmsStorePrefDualSim = (Preference) findPreference("pref_key_sms_store_dual");
        mSmsStorePref1 = (ListPreference) findPreference("pref_key_sms_store_card1");
        mSmsStorePref2 = (ListPreference) findPreference("pref_key_sms_store_card2");
        setSmsValidityPeriodPref();
        setMessagePriorityPref();
        showSmscPref();
        if (MessageUtils.isMultiSimEnabledMms()) {
            prefSmsSettings.removePreference(mManageSimNoMultiPref);
            if (MessageUtils.isMsimIccCardActive()) {
                prefSmsSettings.removePreference(mManageSim1Pref);
                prefSmsSettings.removePreference(mManageSim2Pref);
            } else if (MessageUtils.isIccCardActivated(MessageUtils.SUB1)
                    && !MessageUtils.isIccCardActivated(MessageUtils.SUB2)) {
                prefSmsSettings.removePreference(mManageSim2Pref);
                prefSmsSettings.removePreference(mManageSimPref);
            } else if (!MessageUtils.isIccCardActivated(MessageUtils.SUB1)
                    && MessageUtils.isIccCardActivated(MessageUtils.SUB2)) {
                prefSmsSettings.removePreference(mManageSim1Pref);
                prefSmsSettings.removePreference(mManageSimPref);
            } else {
                prefSmsSettings.removePreference(mManageSim1Pref);
                prefSmsSettings.removePreference(mManageSim2Pref);
                mManageSimPref.setEnabled(false);
            }
        } else {
            prefSmsSettings.removePreference(mManageSim1Pref);
            prefSmsSettings.removePreference(mManageSim2Pref);
            prefSmsSettings.removePreference(mManageSimPref);
            if (MessageUtils.hasIccCard()) {
                mManageSimNoMultiPref.setEnabled(true);
            } else {
                mManageSimNoMultiPref.setEnabled(false);
            }
        }
        if (!MmsConfig.getSMSDeliveryReportsEnabled()) {
            prefSmsSettings.removePreference(mSmsDeliveryReportSim1Pref);
            prefSmsSettings.removePreference(mSmsDeliveryReportSim2Pref);
            prefSmsSettings.removePreference(mSmsDeliveryReportPref);
            prefSmsSettings.removePreference(mSmsDeliveryReportNoMultiPref);
        } else {
            if (MessageUtils.isMultiSimEnabledMms()) {
                prefSmsSettings.removePreference(mSmsDeliveryReportNoMultiPref);
                if (!MessageUtils.isIccCardActivated(MessageUtils.SUB1)) {
                    prefSmsSettings
                            .removePreference(mSmsDeliveryReportSim1Pref);
                    prefSmsSettings.removePreference(mSmsDeliveryReportPref);
                }
                if (!MessageUtils.isIccCardActivated(MessageUtils.SUB2)) {
                    prefSmsSettings
                            .removePreference(mSmsDeliveryReportSim2Pref);
                    prefSmsSettings.removePreference(mSmsDeliveryReportPref);
                }
                if (MessageUtils.isMsimIccCardActive()) {
                    prefSmsSettings
                            .removePreference(mSmsDeliveryReportSim1Pref);
                    prefSmsSettings
                            .removePreference(mSmsDeliveryReportSim2Pref);
                }
                if (!MessageUtils.isIccCardActivated(MessageUtils.SUB1)
                        && !MessageUtils.isIccCardActivated(MessageUtils.SUB2)) {
                    prefSmsSettings
                            .removePreference(mSmsDeliveryReportSim1Pref);
                    prefSmsSettings
                            .removePreference(mSmsDeliveryReportSim2Pref);
                    prefSmsSettings.addPreference(mSmsDeliveryReportPref);
                    mSmsDeliveryReportPref.setEnabled(false);
                }
            } else {
                prefSmsSettings.removePreference(mSmsDeliveryReportSim1Pref);
                prefSmsSettings.removePreference(mSmsDeliveryReportSim2Pref);
                prefSmsSettings.removePreference(mSmsDeliveryReportPref);
                if (!MessageUtils.hasIccCard()) {
                    mSmsDeliveryReportNoMultiPref.setEnabled(false);
                }
            }
        }
        if (getResources().getBoolean(R.bool.config_savelocation)) {
            if (MessageUtils.isMultiSimEnabledMms()) {
                prefSmsSettings.removePreference(mSmsStorePrefSingleSim);
                if (!MessageUtils.hasIccCard(MessageUtils.SUB1) &&
                        !MessageUtils.hasIccCard(MessageUtils.SUB2)) {
                    mSmsStorePrefDualSim.setEnabled(false);
                    prefSmsSettings.removePreference(mSmsStorePref1);
                    prefSmsSettings.removePreference(mSmsStorePref2);
                } else if (MessageUtils.hasIccCard(MessageUtils.SUB1) &&
                        MessageUtils.hasIccCard(MessageUtils.SUB2)) {
                    prefSmsSettings.removePreference(mSmsStorePref1);
                    prefSmsSettings.removePreference(mSmsStorePref2);
                } else if (MessageUtils.hasIccCard(MessageUtils.SUB1) &&
                        !MessageUtils.hasIccCard(MessageUtils.SUB2)) {
                    prefSmsSettings.removePreference(mSmsStorePrefDualSim);
                    prefSmsSettings.removePreference(mSmsStorePref2);
                    setSmsPrefStorageSummary(MessageUtils.SUB1);
                } else if (!MessageUtils.hasIccCard(MessageUtils.SUB1) &&
                        MessageUtils.hasIccCard(MessageUtils.SUB2)) {
                    prefSmsSettings.removePreference(mSmsStorePrefDualSim);
                    prefSmsSettings.removePreference(mSmsStorePref1);
                    setSmsPrefStorageSummary(MessageUtils.SUB2);
                }
            } else {
                prefSmsSettings.removePreference(mSmsStorePref1);
                prefSmsSettings.removePreference(mSmsStorePref2);
                prefSmsSettings.removePreference(mSmsStorePrefDualSim);
                if (!MessageUtils.hasIccCard()) {
                    mSmsStorePrefSingleSim.setEnabled(false);
                } else {
                    setSmsPrefStorageSummary(MessageUtils.SUB_INVALID);
                }
            }
        } else {
            prefSmsSettings.removePreference(mSmsStorePrefSingleSim);
            prefSmsSettings.removePreference(mSmsStorePrefDualSim);
            prefSmsSettings.removePreference(mSmsStorePref1);
            prefSmsSettings.removePreference(mSmsStorePref2);
        }
    }

    private void setSmsPrefStorageSummary(int subscription) {
        switch (subscription) {
            case MessageUtils.SUB1:
                mSmsStorePref1
                        .setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
                            public boolean onPreferenceChange(
                                    Preference preference, Object newValue) {
                                final String summary = newValue.toString();
                                int index = mSmsStorePref1
                                        .findIndexOfValue(summary);
                                mSmsStorePref1
                                        .setSummary(mSmsStorePref1
                                                .getEntries()[index]);
                                mSmsStorePref1.setValue(summary);
                                return true;
                            }
                        });
                mSmsStorePref1.setSummary(mSmsStorePref1.getEntry());
                break;
            case MessageUtils.SUB2:
                mSmsStorePref2
                        .setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
                            public boolean onPreferenceChange(
                                    Preference preference, Object newValue) {
                                final String summary = newValue.toString();
                                int index = mSmsStorePref2
                                        .findIndexOfValue(summary);
                                mSmsStorePref2
                                        .setSummary(mSmsStorePref2
                                                .getEntries()[index]);
                                mSmsStorePref2.setValue(summary);
                                return true;
                            }
                        });
                mSmsStorePref2.setSummary(mSmsStorePref2.getEntry());
                break;
            case MessageUtils.SUB_INVALID:
                mSmsStorePrefSingleSim
                        .setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
                            public boolean onPreferenceChange(
                                    Preference preference, Object newValue) {
                                final String summary = newValue.toString();
                                int index = mSmsStorePrefSingleSim
                                        .findIndexOfValue(summary);
                                mSmsStorePrefSingleSim
                                        .setSummary(mSmsStorePrefSingleSim
                                                .getEntries()[index]);
                                mSmsStorePrefSingleSim.setValue(summary);
                                return true;
                            }
                        });
                mSmsStorePrefSingleSim.setSummary(mSmsStorePrefSingleSim.getEntry());
            default:
                break;
        }
    }

    private void updateSignatureStatus() {
        // If the signature CheckBox is checked, we should set the signature
        // EditText
        // enable, and disable when it's not checked.
        boolean isChecked = mSmsSignaturePref.isChecked();
        mSmsSignatureEditPref.setEnabled(isChecked);
    }

    private OnPreferenceClickListener getSMSCPrefOnClickListener(final int subId) {
        return  new OnPreferenceClickListener() {
            @Override
            public boolean onPreferenceClick(Preference preference) {
                MyEditDialogFragment dialog = MyEditDialogFragment
                        .newInstance(SmsPreferenceActivity.this,
                                preference.getTitle(),
                                preference.getSummary(),
                                subId);
                dialog.show(getFragmentManager(), "dialog");
                return true;
            }
        };
    }

    private void showSmscPref() {
        if (!MessageUtils.isMultiSimEnabledMms()) {
            //remove all multi
            prefSmsSettings.removePreference(mSmsCenterNumber);
            prefSmsSettings.removePreference(mSmsCenterNumberSim1);
            prefSmsSettings.removePreference(mSmsCenterNumberSim2);
            if (getResources().getBoolean(R.bool.show_edit_smsc)) {
                mSmsCenterNumberSS.setOnPreferenceClickListener(null);
            } else {
                mSmsCenterNumberSS.setOnPreferenceClickListener(getSMSCPrefOnClickListener(0));
            }
            boolean  isCDMA = MessageUtils.isCDMAPhone(0);
            if (!isCDMA && !isAirPlaneModeOn() && MessageUtils.isIccCardActivated()) {
                mSmsCenterNumberSS.setEnabled(true);
            } else {
                mSmsCenterNumberSS.setEnabled(false);
            }
        } else {
            prefSmsSettings.removePreference(mSmsCenterNumberSS);
            prefSmsSettings.removePreference(mSmsCenterNumber);
            int count = MessageUtils.getPhoneCount();
            for (int i = 0; i < count; i++) {
                final Preference pref = findPreference(String.valueOf(i));
                pref.setTitle(getSMSCDialogTitle(count, i));
                if (getResources().getBoolean(R.bool.show_edit_smsc)) {
                    pref.setOnPreferenceClickListener(null);
                } else {
                   pref.setOnPreferenceClickListener(getSMSCPrefOnClickListener(Integer.valueOf(pref.getKey())));
                }

                mSmscPrefList.add(pref);
                boolean isCDMA = false;
                int subId[] = SubscriptionManagerWrapper.getSubId(i);
                if (subId != null && subId.length != 0) {
                    isCDMA = MessageUtils.isCDMAPhone(subId[0]);
                }
                if (!isCDMA && !isAirPlaneModeOn() && MessageUtils.hasActivatedIccCard(i)) {
                    prefSmsSettings.addPreference(pref);
                } else {
                    prefSmsSettings.removePreference(pref);
                }
            }
        }

        updateSMSCPref();
    }

    private String getSMSCDialogTitle(int count, int index) {
        String title = MessageUtils.isMultiSimEnabledMms()
                ? getString(R.string.pref_more_smcs) : getString(R.string.pref_one_smcs);
        return title;
    }

    private void updateSMSCPref() {
        if (!MessageUtils.isMultiSimEnabledMms()) {
            boolean isCDMA =  MessageUtils.isCDMAPhone(0);
            if (!isCDMA && !isAirPlaneModeOn() && MessageUtils.isIccCardActivated()) {
                mSmsCenterNumberSS.setEnabled(true);
                setSMSCPrefState(0, true);
            } else {
                mSmsCenterNumberSS.setEnabled(false);
            }

        } else {
            if (mSmscPrefList == null || mSmscPrefList.size() == 0) {
                return;
            }
            int count = MessageUtils.getPhoneCount();
            int iccCardActivited = 0;
            for (int i = 0; i < count; i++) {
                boolean isCDMA = false;
                int subId[] = SubscriptionManagerWrapper.getSubId(i);
                if (subId != null && subId.length != 0) {
                    isCDMA = MessageUtils.isCDMAPhone(subId[0]);
                }
                if (!isCDMA && !isAirPlaneModeOn()
                        && MessageUtils.hasActivatedIccCard(i)) {
                    iccCardActivited++;
                }
                setSMSCPrefState(i, !isCDMA && !isAirPlaneModeOn()
                        && MessageUtils.hasActivatedIccCard(i));
            }
            if (iccCardActivited == 0) {
                prefSmsSettings.addPreference(mSmsCenterNumber);
                mSmsCenterNumber.setEnabled(false);
            } else if (iccCardActivited == 2) {
                prefSmsSettings.addPreference(mSmsCenterNumber);
                prefSmsSettings.removePreference(mSmsCenterNumberSim1);
                prefSmsSettings.removePreference(mSmsCenterNumberSim2);
                mSmsCenterNumber.setEnabled(true);
            } else {
                prefSmsSettings.removePreference(mSmsCenterNumber);
            }
        }
    }

    private void setSMSCPrefState(int id, boolean prefEnabled) {
        // We need update the preference summary.
        if (!MessageUtils.isMultiSimEnabledMms()) {
            if (prefEnabled) {
                if (getResources().getBoolean(R.bool.def_enable_reset_smsc)) {
                    updateSmscFromPreference(id);
                } else {
                    final Message callback = mHandler
                            .obtainMessage(EVENT_GET_SMSC_DONE);
                    Bundle userParams = new Bundle();
                    userParams.putInt(ConstantsWrapper.Phone.SLOT_KEY, id);
                    callback.obj = userParams;
                    MessageUtils.getSmscFromSub(this, id, callback);
                }
            }
        } else {
            if (prefEnabled) {
                Log.d(TAG, "get SMSC from sub= " + id);
                if (getResources().getBoolean(R.bool.def_enable_reset_smsc)) {
                    updateSmscFromPreference(id);
                } else {
                    final Message callback = mHandler
                            .obtainMessage(EVENT_GET_SMSC_DONE);
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
    }

    private void updateSmscFromPreference(int sub) {
        String smsc = PreferenceManager.getDefaultSharedPreferences(this)
                .getString(SMSC_DEFAULT, ""/*
                                            * SmsManager.getDefault(). getSmscAddressFromIcc()
                                            */);
        if (!MessageUtils.isMultiSimEnabledMms()) {
            mSmsCenterNumberSS.setSummary(smsc);
        } else {
            if (sub != -1) {
                mSmscPrefList.get(sub).setSummary(smsc);
            }
        }
    }

    private void setSmsValidityPeriodPref() {
        if (getResources().getBoolean(R.bool.config_sms_validity)) {
            if (MessageUtils.isMultiSimEnabledMms()) {
                prefSmsSettings.removePreference(mSmsValidityNoMultiPref);
                if (MessageUtils.isMsimIccCardActive()) {
                    prefSmsSettings.removePreference(mSmsValiditysim1Pref);
                    prefSmsSettings.removePreference(mSmsValiditysim2Pref);
                } else if (MessageUtils.isIccCardActivated(MessageUtils.SUB1)
                        && !MessageUtils.isIccCardActivated(MessageUtils.SUB2)) {
                    prefSmsSettings.removePreference(mSmsValiditysim2Pref);
                    prefSmsSettings.removePreference(mSmsValidityPref);
                    setSmsPreferValiditySummary(MessageUtils.SUB1);
                } else if (!MessageUtils.isIccCardActivated(MessageUtils.SUB1)
                        && MessageUtils.isIccCardActivated(MessageUtils.SUB2)) {
                    prefSmsSettings.removePreference(mSmsValiditysim1Pref);
                    prefSmsSettings.removePreference(mSmsValidityPref);
                    setSmsPreferValiditySummary(MessageUtils.SUB2);
                } else {
                    prefSmsSettings.removePreference(mSmsValiditysim1Pref);
                    prefSmsSettings.removePreference(mSmsValiditysim2Pref);
                    mSmsValidityPref.setEnabled(false);
                }
            } else {
                prefSmsSettings.removePreference(mSmsValiditysim1Pref);
                prefSmsSettings.removePreference(mSmsValiditysim2Pref);
                prefSmsSettings.removePreference(mSmsValidityPref);
                setSmsPreferValiditySummary(MessageUtils.SUB_INVALID);
            }
        } else {
            prefSmsSettings.removePreference(mSmsValiditysim1Pref);
            prefSmsSettings.removePreference(mSmsValiditysim2Pref);
            prefSmsSettings.removePreference(mSmsValidityPref);
            prefSmsSettings.removePreference(mSmsValidityNoMultiPref);
        }
    }

    private void setSmsPreferValiditySummary(int subscription) {
        switch (subscription) {
            case MessageUtils.SUB_INVALID:
                mSmsValidityNoMultiPref
                        .setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
                            public boolean onPreferenceChange(
                                    Preference preference, Object newValue) {
                                final String summary = newValue.toString();
                                int index = mSmsValidityNoMultiPref
                                        .findIndexOfValue(summary);
                                mSmsValidityNoMultiPref
                                        .setSummary(mSmsValidityNoMultiPref
                                                .getEntries()[index]);
                                mSmsValidityNoMultiPref.setValue(summary);
                                return true;
                            }
                        });
                mSmsValidityNoMultiPref.setSummary(mSmsValidityNoMultiPref
                        .getEntry());
                break;
            case MessageUtils.SUB1:
                mSmsValiditysim1Pref
                        .setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
                            public boolean onPreferenceChange(
                                    Preference preference, Object newValue) {
                                final String summary = newValue.toString();
                                int index = mSmsValiditysim1Pref
                                        .findIndexOfValue(summary);
                                mSmsValiditysim1Pref
                                        .setSummary(mSmsValiditysim1Pref
                                                .getEntries()[index]);
                                mSmsValiditysim1Pref.setValue(summary);
                                return true;
                            }
                        });
                mSmsValiditysim1Pref.setSummary(mSmsValiditysim1Pref.getEntry());
                break;
            case MessageUtils.SUB2:
                mSmsValiditysim2Pref
                        .setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
                            public boolean onPreferenceChange(
                                    Preference preference, Object newValue) {
                                final String summary = newValue.toString();
                                int index = mSmsValiditysim2Pref
                                        .findIndexOfValue(summary);
                                mSmsValiditysim2Pref
                                        .setSummary(mSmsValiditysim2Pref
                                                .getEntries()[index]);
                                mSmsValiditysim2Pref.setValue(summary);
                                return true;
                            }
                        });
                mSmsValiditysim2Pref.setSummary(mSmsValiditysim2Pref.getEntry());
                break;
            default:
                break;
        }
    }

    private void setMessagePriorityPref() {
        if (!getResources().getBoolean(R.bool.support_sms_priority)) {
            Preference priorotySettings = findPreference(SMS_CDMA_PRIORITY);
            PreferenceScreen prefSet = getPreferenceScreen();
            prefSet.removePreference(priorotySettings);
        }
    }

    /**
     * All subclasses of Fragment must include a public empty constructor. The framework will often
     * re-instantiate a fragment class when needed, in particular during state restore, and needs to
     * be able to find this constructor to instantiate it. If the empty constructor is not
     * available, a runtime exception will occur in some cases during state restore.
     */
    public static class MyAlertDialogFragment extends DialogFragment {
        private SmsPreferenceActivity mActivity;

        public static MyAlertDialogFragment newInstance(
                SmsPreferenceActivity activity, int sub, String smsc) {
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
            final String displayedSMSC = getArguments().getString(
                    SMSC_DIALOG_NUMBER);

            // When framework re-instantiate this fragment by public empty
            // constructor and call onCreateDialog(Bundle savedInstanceState) ,
            // we should make sure mActivity not null.
            if (null == mActivity) {
                mActivity = (SmsPreferenceActivity) getActivity();
            }

            final String actualSMSC = mActivity.adjustSMSC(displayedSMSC);

            return new AlertDialog.Builder(mActivity)
                    .setIcon(android.R.drawable.ic_dialog_alert)
                    .setMessage(R.string.set_smsc_confirm_message)
                    .setPositiveButton(android.R.string.ok,
                            new DialogInterface.OnClickListener() {
                                public void onClick(DialogInterface dialog,
                                        int whichButton) {
                                    Log.d(TAG, "set SMSC from sub= " + sub
                                            + " SMSC= " + displayedSMSC);
                                    Bundle userParams = new Bundle();
                                    userParams.putInt(ConstantsWrapper.Phone.SLOT_KEY,
                                            sub);
                                    if (getResources().getBoolean(
                                            R.bool.def_enable_reset_smsc)) {
                                        final Message callbackMessage = mHandler
                                                .obtainMessage(EVENT_SET_SMSC_PREF_DONE);
                                        callbackMessage.obj = userParams;
                                        putSmscIntoPref(mActivity, sub,
                                                displayedSMSC, callbackMessage);
                                    } else {
                                        final Message callback = mHandler
                                                .obtainMessage(EVENT_SET_SMSC_DONE);
                                        userParams.putString(
                                                MessageUtils.EXTRA_SMSC,
                                                actualSMSC);
                                        callback.obj = userParams;
                                        MessageUtils.setSmscForSub(mActivity,
                                                sub, actualSMSC, callback);
                                    }
                                }
                            }).setNegativeButton(android.R.string.cancel, null)
                    .setCancelable(true).create();
        }
    }

    private String adjustSMSC(String smsc) {
        String actualSMSC = "\"" + smsc + "\"";
        return actualSMSC;
    }

    public static class MyEditDialogFragment extends DialogFragment {
        private SmsPreferenceActivity mActivity;

        public static MyEditDialogFragment newInstance(
                SmsPreferenceActivity activity, CharSequence title,
                CharSequence smsc, int sub) {
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
                mActivity = (SmsPreferenceActivity) getActivity();
                dismiss();
            }
            final EditText edit = new EditText(mActivity);
            edit.setPadding(15, 15, 15, 15);
            edit.setText(getArguments().getCharSequence(SMSC_DIALOG_NUMBER));

            Dialog alert = new AlertDialog.Builder(mActivity)
                    .setTitle(getArguments().getCharSequence(SMSC_DIALOG_TITLE))
                    .setView(edit)
                    .setPositiveButton(android.R.string.ok,
                            new DialogInterface.OnClickListener() {
                                public void onClick(DialogInterface dialog,
                                        int whichButton) {
                                    MyAlertDialogFragment newFragment = MyAlertDialogFragment
                                            .newInstance(mActivity, sub, edit
                                                    .getText().toString());
                                    newFragment.show(getFragmentManager(),
                                            "dialog");
                                    dismiss();
                                }
                            }).setNegativeButton(android.R.string.cancel, null)
                    .setCancelable(true).create();
            alert.getWindow().setSoftInputMode(
                    WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE);
            return alert;
        }
    }

    private static final class SmscHandler extends Handler {
        SmsPreferenceActivity mOwner;

        public SmscHandler(SmsPreferenceActivity owner) {
            super(Looper.getMainLooper());
            mOwner = owner;
        }

        @Override
        public void handleMessage(Message msg) {
            Bundle bundle = (Bundle) msg.obj;
            if (bundle == null) {
                return;
            }
            Throwable exception = (Throwable) bundle
                    .getSerializable(MessageUtils.EXTRA_EXCEPTION);
            boolean result = bundle.getBoolean(MessageUtils.EXTRA_SMSC_RESULT, false);
            if (!result || exception != null) {
                Log.d(TAG, "Error: " + exception + " result:" + result);
                mOwner.showToast(R.string.set_smsc_error);
                return;
            }

            Bundle userParams = (Bundle) bundle.getParcelable(MessageUtils.EXTRA_USEROBJ);
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

    private void updateSmscFromBundle(Bundle bundle) {
        if (bundle != null) {
            int sub = bundle.getInt(ConstantsWrapper.Phone.SLOT_KEY, -1);
            if (sub != -1) {
                String summary = bundle
                        .getString(MessageUtils.EXTRA_SMSC, null);
                if (summary == null) {
                    return;
                }
                Log.d(TAG, "Update SMSC: sub= " + sub + " SMSC= " + summary);
                int end = summary.lastIndexOf("\"");
                if (!MessageUtils.isMultiSimEnabledMms()) {
                    mSmsCenterNumberSS.setSummary((end > 0)? summary.substring(1, end) : summary);
                } else {
                    mSmscPrefList.get(sub).setSummary(
                            (end > 0)? summary.substring(1, end) : summary);
                }
            }
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

    private boolean isAirPlaneModeOn() {
        return Settings.System.getInt(getContentResolver(),
                Settings.System.AIRPLANE_MODE_ON, 0) != 0;
    }

    private void showToast(int id) {
        Toast.makeText(this, id, Toast.LENGTH_SHORT).show();
    }

    @Override
    protected void onResume() {
        // Initialize the sms signature
        updateSignatureStatus();
        registerListeners();
        super.onResume();
    }

    private void registerListeners() {
        final IntentFilter intentFilter = new IntentFilter(
                ConstantsWrapper.TelephonyIntent.ACTION_SIM_STATE_CHANGED);
        intentFilter.addAction(Intent.ACTION_AIRPLANE_MODE_CHANGED);
        registerReceiver(mReceiver, intentFilter);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case android.R.id.home:
                // The user clicked on the Messaging icon in the action bar.
                // Take them back from
                // wherever they came from
                finish();
                return true;
        }
        return false;
    }

    @Override
    public boolean onPreferenceTreeClick(PreferenceScreen preferenceScreen,
            Preference preference) {
        if (preference == mSmsDeliveryReportPref) {
            Intent intent = new Intent(this, MessagingReportsPreferenceActivity.class);
            intent.putExtra(MSG_TYPE, SMS_DELIVERY_REPORTS);
            startActivity(intent);
        } else if (preference == mSmsTemplatePref) {
            startActivity(new Intent(this, MessageTemplate.class));
        } else if (preference == mSmsSignaturePref) {
            updateSignatureStatus();
        } else if (preference == mManageSimPref) {
            startActivity(new Intent(this, ManageSimSMSPreferenceActivity.class));
        } else if (preference == mManageSimNoMultiPref) {
            startActivity(new Intent(this, ManageSimMessages.class));
        } else if (preference == mManageSim1Pref) {
            Intent intent = new Intent(this, ManageSimMessages.class);
            intent.putExtra(ConstantsWrapper.Phone.SLOT_KEY, ConstantsWrapper.Phone.SUB1);
            startActivity(intent);
        } else if (preference == mManageSim2Pref) {
            Intent intent = new Intent(this, ManageSimMessages.class);
            intent.putExtra(ConstantsWrapper.Phone.SLOT_KEY, ConstantsWrapper.Phone.SUB2);
            startActivity(intent);
        } else if (preference == mSmsValidityPref) {
            Intent intent = new Intent(this, MessagingExpiryPreferenceActivity.class);
            intent.putExtra(MSG_TYPE, TYPE_SMS);
            startActivity(intent);
        } else if (preference == mSmsCenterNumber) {
            startActivity(new Intent(this, SMSCPreferenceActivity.class));
        } else if (preference == mSmsStorePrefDualSim) {
            startActivity(new Intent(this, SMSPreferStoragePreferenceActivity.class));
        }
        return super.onPreferenceTreeClick(preferenceScreen, preference);
    }

    /**
     * Trigger the TransactionService to download any outstanding messages.
     */
    private void startMmsDownload() {
        startService(new Intent(TransactionService.ACTION_ENABLE_AUTO_RETRIEVE,
                null, this, TransactionService.class));
    }

    @Override
    protected void onDestroy() {
        unregisterReceiver(mReceiver);
        super.onDestroy();
    }
}
