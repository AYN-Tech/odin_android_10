/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
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
import android.os.Bundle;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.PreferenceActivity;
import android.view.MenuItem;

import com.android.mms.R;

public class SMSPreferStoragePreferenceActivity extends PreferenceActivity {

    private ListPreference mSmsStorePref1;
    private ListPreference mSmsStorePref2;

    protected void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        createResource();
        ActionBar actionBar = getActionBar();
        actionBar.setDisplayOptions(ActionBar.DISPLAY_SHOW_TITLE
                | ActionBar.DISPLAY_HOME_AS_UP);
    }

    private void createResource() {
        addPreferencesFromResource(R.xml.sms_prefer_storage);
        mSmsStorePref1 = (ListPreference) findPreference("pref_key_sms_store_card1");
        mSmsStorePref2 = (ListPreference) findPreference("pref_key_sms_store_card2");
        setSmsPrefStorageSummary(MessageUtils.SUB1);
        setSmsPrefStorageSummary(MessageUtils.SUB2);
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
}
