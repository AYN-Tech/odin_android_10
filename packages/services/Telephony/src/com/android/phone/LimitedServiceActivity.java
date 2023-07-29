/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
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

package com.android.phone;

import android.app.AlertDialog;
import android.app.Dialog;
import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.support.annotation.NonNull;
import android.telephony.ims.ImsMmTelManager;
import android.telephony.SubscriptionManager;
import android.util.Log;
import android.view.View;
import android.widget.CheckBox;
import androidx.fragment.app.DialogFragment;
import androidx.fragment.app.FragmentActivity;
import com.android.internal.telephony.Phone;
import com.android.internal.telephony.PhoneConstants;
import com.android.internal.telephony.PhoneFactory;

public class LimitedServiceActivity extends FragmentActivity {

    private static final String LOG_TAG = "LimitedServiceActivity";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Log.d(LOG_TAG, "Started LimitedServiceActivity");
        LimitedServiceAlertDialogFragment newFragment = LimitedServiceAlertDialogFragment.
                newInstance(getIntent());
        newFragment.show(getSupportFragmentManager(), null);
    }

    public static class LimitedServiceAlertDialogFragment extends DialogFragment {
        private static final String TAG = "LimitedServiceAlertDialog";
        public static final int NOTIFICATION_EMERGENCY_NETWORK = 1001;
        private static int mPhoneId = SubscriptionManager.INVALID_PHONE_INDEX;

        public static LimitedServiceAlertDialogFragment newInstance(Intent intent) {
            LimitedServiceAlertDialogFragment frag = new LimitedServiceAlertDialogFragment();
            mPhoneId = intent.getExtras().getInt(PhoneConstants.PHONE_KEY);
            Log.i(TAG, "LimitedServiceAlertDialog for phoneId:" + mPhoneId);
            return frag;
        }

        @Override
        public Dialog onCreateDialog(Bundle bundle) {
            if (!SubscriptionManager.isValidPhoneId(mPhoneId)) return null;
            super.onCreateDialog(bundle);
            View dialogView = View.inflate(getActivity(),
                    R.layout.frag_limited_service_alert_dialog, null);
            CheckBox alertCheckBox = dialogView.findViewById(R.id.do_not_show);
            SharedPreferences pref = PreferenceManager.getDefaultSharedPreferences(
                    PhoneFactory.getPhone(mPhoneId).getContext());
            Log.i(TAG, "onCreateDialog " + Phone.KEY_DO_NOT_SHOW_LIMITED_SERVICE_ALERT +
                    PhoneFactory.getPhone(mPhoneId).getSubId()  + ":" + pref.getBoolean
                    (Phone.KEY_DO_NOT_SHOW_LIMITED_SERVICE_ALERT + PhoneFactory.getPhone
                    (mPhoneId).getSubId(), false));

            AlertDialog alertDialog =
                new AlertDialog.Builder(getActivity())
                    .setView(dialogView)
                    .setNegativeButton(
                        android.R.string.cancel,
                        (dialog, which) -> onNegativeButtonClicked())
                    .setPositiveButton(
                        android.R.string.ok,
                        (dialog, which) -> onPositiveButtonClicked(pref, alertCheckBox.isChecked()))
                    .create();
            this.setCancelable(false);
            return alertDialog;
        }

        private void onNegativeButtonClicked() {
            Log.d(TAG, "onNegativeButtonClicked");
            SubscriptionManager subscriptionManager = (SubscriptionManager) getContext().
                    getSystemService(Context.TELEPHONY_SUBSCRIPTION_SERVICE);
            int[] subIds = subscriptionManager.getSubscriptionIds(mPhoneId);
            if (subIds != null && subIds.length > 0 && SubscriptionManager.
                    isValidSubscriptionId(subIds[0])) {
                ImsMmTelManager imsMmTelMgr = ImsMmTelManager.
                        createForSubscriptionId(subIds[0]);
                Log.i(TAG, "Disabling WFC setting");
                imsMmTelMgr.setVoWiFiSettingEnabled(false);
            }
            dismiss();
            getActivity().finish();
        }

        private void onPositiveButtonClicked(@NonNull SharedPreferences preferences,
                boolean isChecked) {
            SharedPreferences.Editor editor = preferences.edit();
            editor.putBoolean(Phone.KEY_DO_NOT_SHOW_LIMITED_SERVICE_ALERT + PhoneFactory.
                    getPhone(mPhoneId).getSubId(), isChecked);
            editor.commit();
            Log.i(TAG, "onPositiveButtonClicked isChecked:" + isChecked + " phoneId:" + mPhoneId
                    + " do not show preference:" + preferences.getBoolean
                    (Phone.KEY_DO_NOT_SHOW_LIMITED_SERVICE_ALERT +
                    PhoneFactory.getPhone(mPhoneId).getSubId(), false));
            if (isChecked) {
                NotificationManager sNotificationManager = (NotificationManager) getContext().
                        getSystemService(NOTIFICATION_SERVICE);
                sNotificationManager.cancel(NOTIFICATION_EMERGENCY_NETWORK);
            }
            dismiss();
            getActivity().finish();
        }
  }
}
