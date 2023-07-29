/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
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

package com.android.mms.ui;

import android.app.ActionBar;
import android.content.Intent;
import android.os.Bundle;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.PreferenceActivity;
import android.view.MenuItem;

import com.android.mms.R;
import com.android.mms.transaction.TransactionService;
import com.android.mmswrapper.ConstantsWrapper;

public class MessagingExpiryPreferenceActivity extends PreferenceActivity {
    private static String TAG = "MessagingExpiryPreferenceActivity";

    public static final String MMS_VALIDITY_FOR_SIM1 = "pref_key_mms_validity_period_for_sim1";

    public static final String MMS_VALIDITY_FOR_SIM2 = "pref_key_mms_validity_period_for_sim2";

    private static String TYPE_MMS = "mms";

    private static String TYPE_SMS = "sms";

    private static String MSG_TYPE = "msg_type";

    private ListPreference mSmsValidityCard1Pref;
    private ListPreference mSmsValidityCard2Pref;
    private ListPreference mMmsExpiryCard1Pref;
    private ListPreference mMmsExpiryCard2Pref;
    private String mMsgType = null;
    protected void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        Intent intent = getIntent();
        Bundle bundle = intent.getExtras();
        mMsgType = bundle.getString(MSG_TYPE);
        createResource();
        ActionBar actionBar = getActionBar();
        actionBar.setDisplayOptions(ActionBar.DISPLAY_SHOW_TITLE
                | ActionBar.DISPLAY_HOME_AS_UP);
    }

    private void createResource() {
        if (mMsgType.equals(TYPE_MMS)) {
            addPreferencesFromResource(R.xml.mms_validity_period);
            mMmsExpiryCard1Pref = (ListPreference)
                    findPreference("pref_key_mms_validity_period_for_sim1");
            mMmsExpiryCard2Pref = (ListPreference)
                    findPreference("pref_key_mms_validity_period_for_sim2");
            setMmsValidityPeriodPref();
        } else if (mMsgType.equals(TYPE_SMS)) {
            addPreferencesFromResource(R.xml.sms_validity_period);
            mSmsValidityCard1Pref = (ListPreference)
                    findPreference("pref_key_sms_validity_period_for_sim1");
            mSmsValidityCard2Pref = (ListPreference)
                    findPreference("pref_key_sms_validity_period_for_sim2");
            setSmsValidityPeriodPref();
        }
    }

    private void setMmsValidityPeriodPref() {
        if (getResources().getBoolean(R.bool.config_mms_validity)) {
            if (MessageUtils.isMultiSimEnabledMms()) {
                if (MessageUtils.isIccCardActivated(MessageUtils.SUB1)) {
                    setMmsExpirySummary(ConstantsWrapper.Phone.SUB1);
                }
                if (MessageUtils.isIccCardActivated(MessageUtils.SUB2)) {
                    setMmsExpirySummary(ConstantsWrapper.Phone.SUB2);
                }
            }
        }
    }

    private void setMmsExpirySummary(int subscription) {
        switch (subscription) {
            case ConstantsWrapper.Phone.SUB1:
                mMmsExpiryCard1Pref
                        .setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
                            public boolean onPreferenceChange(
                                    Preference preference, Object newValue) {
                                final String value = newValue.toString();
                                int index = mMmsExpiryCard1Pref
                                        .findIndexOfValue(value);
                                mMmsExpiryCard1Pref.setValue(value);
                                mMmsExpiryCard1Pref.setSummary(mMmsExpiryCard1Pref
                                        .getEntries()[index]);
                                return false;
                            }
                        });
                mMmsExpiryCard1Pref.setSummary(mMmsExpiryCard1Pref.getEntry());
                break;
            case ConstantsWrapper.Phone.SUB2:
                mMmsExpiryCard2Pref
                        .setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
                            public boolean onPreferenceChange(
                                    Preference preference, Object newValue) {
                                final String value = newValue.toString();
                                int index = mMmsExpiryCard2Pref
                                        .findIndexOfValue(value);
                                mMmsExpiryCard2Pref.setValue(value);
                                mMmsExpiryCard2Pref.setSummary(mMmsExpiryCard2Pref
                                        .getEntries()[index]);
                                return false;
                            }
                        });
                mMmsExpiryCard2Pref.setSummary(mMmsExpiryCard2Pref.getEntry());
                break;
            default:
                break;
        }
    }

    private void setSmsValidityPeriodPref() {
        if (getResources().getBoolean(R.bool.config_sms_validity)) {
            if (MessageUtils.isMultiSimEnabledMms()) {
                if (MessageUtils.isIccCardActivated(MessageUtils.SUB1)) {
                    setSmsPreferValiditySummary(MessageUtils.SUB1);
                }
                if (MessageUtils.isIccCardActivated(MessageUtils.SUB2)) {
                    setSmsPreferValiditySummary(MessageUtils.SUB2);
                }
            }
        }
    }

    private void setSmsPreferValiditySummary(int subscription) {
        switch (subscription) {
            case MessageUtils.SUB1:
                mSmsValidityCard1Pref
                        .setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
                            public boolean onPreferenceChange(
                                    Preference preference, Object newValue) {
                                final String summary = newValue.toString();
                                int index = mSmsValidityCard1Pref
                                        .findIndexOfValue(summary);
                                mSmsValidityCard1Pref
                                        .setSummary(mSmsValidityCard1Pref
                                                .getEntries()[index]);
                                mSmsValidityCard1Pref.setValue(summary);
                                return true;
                            }
                        });
                mSmsValidityCard1Pref.setSummary(mSmsValidityCard1Pref.getEntry());
                break;
            case MessageUtils.SUB2:
                mSmsValidityCard2Pref
                        .setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
                            public boolean onPreferenceChange(
                                    Preference preference, Object newValue) {
                                final String summary = newValue.toString();
                                int index = mSmsValidityCard2Pref
                                        .findIndexOfValue(summary);
                                mSmsValidityCard2Pref
                                        .setSummary(mSmsValidityCard2Pref
                                                .getEntries()[index]);
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
    protected void onResume() {
        super.onResume();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
    }
}
