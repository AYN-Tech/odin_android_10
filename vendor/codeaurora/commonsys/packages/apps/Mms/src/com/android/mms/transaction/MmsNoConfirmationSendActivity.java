 /*
 * Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *    * Neither the name of The Linux Foundation nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
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

package com.android.mms.transaction;

import com.android.mms.MmsConfig;
import com.android.mms.ui.ComposeMessageActivity;
import com.android.mms.ui.MessageUtils;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;

public class MmsNoConfirmationSendActivity extends Activity {

    private static final String LOG_TAG =
            MmsNoConfirmationSendActivity.class.getSimpleName();

    public void onCreate(final Bundle savedInstanceState) {
        // TODO Auto-generated method stub
        super.onCreate(savedInstanceState);
        if (MessageUtils.checkPermissionsIfNeeded(this)) {
            return;
        }
        Intent intent = this.getIntent();
        if (intent == null) {
            finish();
            return;
        }
        Bundle extras = intent.getExtras();
        Uri uri = (Uri) extras.getParcelable(Intent.EXTRA_STREAM);
        Intent serviceIntent = new Intent();
        serviceIntent.putExtra("address", MmsConfig.getMmsDestination());
        serviceIntent.setAction(intent.getAction());
        serviceIntent.setType(intent.getType());
        serviceIntent.setComponent(new ComponentName(getApplicationContext(),
                ComposeMessageActivity.class));
        if (uri != null) {
            serviceIntent.putExtra(Intent.EXTRA_STREAM, uri);
            this.startActivity(serviceIntent);
        } else {
            Log.d(LOG_TAG, "No URI --- Cannot send");
        }
        finish();
    }
}
