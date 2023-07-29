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
import android.content.Intent;
import android.os.Bundle;
import android.preference.PreferenceActivity;
import android.preference.PreferenceScreen;
import android.preference.SwitchPreference;
import android.view.MenuItem;

import com.android.mms.R;
import com.android.mms.transaction.TransactionService;

public class MessagingReportsPreferenceActivity extends PreferenceActivity {
    private static String TAG = "MessagingReportsPreferenceActivity";

    private static String MMS_READ_REPORTS = "mms_read_reports";

    private static String MMS_DELIVERY_REPORTS = "mms_delivery_reports";

    private static String SMS_DELIVERY_REPORTS = "sms_delivery_reports";

    private static String MSG_TYPE = "msg_type";

    public static final String SMS_DELIVERY_REPORT_SUB1 = "pref_key_sms_delivery_reports_slot1";

    public static final String SMS_DELIVERY_REPORT_SUB2 = "pref_key_sms_delivery_reports_slot2";

    private String mMsgType = null;

    protected void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        Intent intent = getIntent();
        Bundle bundler = intent.getExtras();
        mMsgType = bundler.getString(MSG_TYPE);
        createResource();
        ActionBar actionBar = getActionBar();
        actionBar.setDisplayOptions(ActionBar.DISPLAY_SHOW_TITLE
                | ActionBar.DISPLAY_HOME_AS_UP);
    }

    private void createResource() {
        if (mMsgType.equals(MMS_READ_REPORTS)) {
            addPreferencesFromResource(R.xml.mms_read_reports);
        } else if (mMsgType.equals(MMS_DELIVERY_REPORTS)) {
            addPreferencesFromResource(R.xml.mms_delivery_reports);
        } else if (mMsgType.equals(SMS_DELIVERY_REPORTS)) {
            addPreferencesFromResource(R.xml.sms_delivery_reports);
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
    protected void onDestroy() {
        super.onDestroy();
    }
}
