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
import java.util.List;
import java.util.Map;

import com.android.mms.MmsApp;
import com.android.mms.MmsConfig;
import com.android.mms.R;

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
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.Parcelable;
import android.os.RemoteException;
import android.preference.CheckBoxPreference;
import android.preference.EditTextPreference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.Preference.OnPreferenceClickListener;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.PreferenceActivity;
import android.preference.PreferenceCategory;
import android.preference.PreferenceManager;
import android.provider.Settings;
import android.telephony.TelephonyManager;
import android.telephony.SubscriptionManager;

import android.util.Log;
import android.view.MenuItem;
import android.view.WindowManager;
import android.widget.EditText;
import android.widget.Toast;

import com.android.mms.transaction.MmsMessageSender;
import com.android.mms.transaction.SmsMessageSender;
import com.android.mms.transaction.TransactionService;
import com.android.mmswrapper.ConstantsWrapper;
import com.android.mmswrapper.SubscriptionManagerWrapper;

import android.content.DialogInterface.OnCancelListener;
import android.database.Cursor;

public class SMSCPreferenceActivity extends PreferenceActivity {
    private static String TAG = "SMSCPreferenceActivity";

    public static final String SMSC_DEFAULT = "pref_key_default_smsc";

    private PreferenceCategory mSmscPrefCate;
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
        addPreferencesFromResource(R.xml.sms_center_number);
        mSmscPrefCate = (PreferenceCategory) findPreference("pref_key_message_smsc");
        showSmscPref();
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
                        MyEditDialogFragment dialog = MyEditDialogFragment
                                .newInstance(SMSCPreferenceActivity.this,
                                        preference.getTitle(),
                                        preference.getSummary(),
                                        Integer.valueOf(preference.getKey()));
                        dialog.show(getFragmentManager(), "dialog");
                        return true;
                    }
                });
            }
            mSmscPrefList.add(pref);
            for (int j = 0; j < count; j++) {
                boolean isCDMA = false;
                int subId[] = SubscriptionManagerWrapper.getSubId(j);
                if (subId != null && subId.length != 0) {
                    isCDMA = MessageUtils.isCDMAPhone(subId[0]);
                }
                if (!isCDMA && !isAirPlaneModeOn() && MessageUtils.hasActivatedIccCard(i)) {
                    mSmscPrefCate.addPreference(pref);
                } else {
                    mSmscPrefCate.removePreference(pref);
                }
            }
        }
        updateSMSCPref();
    }

    private String getSMSCDialogTitle(int count, int index) {
        String title = MessageUtils.isMultiSimEnabledMms() ? getString(
                R.string.pref_more_message_smcs, index + 1)
                : getString(R.string.pref_one_smcs);
        return title;
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
            setSMSCPrefState(i,
                    !isCDMA && !isAirPlaneModeOn() && MessageUtils.hasActivatedIccCard(i));
        }

    }

    private void setSMSCPrefState(int id, boolean prefEnabled) {
        // We need update the preference summary.
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

    private void updateSmscFromPreference(int sub) {
        String smsc = PreferenceManager.getDefaultSharedPreferences(this)
                .getString(SMSC_DEFAULT, ""/*
                                            * SmsManager.getDefault(). getSmscAddressFromIcc()
                                            */);
        if (sub != -1) {
            mSmscPrefList.get(sub).setSummary(smsc);
        }
    }

    /**
     * All subclasses of Fragment must include a public empty constructor. The framework will often
     * re-instantiate a fragment class when needed, in particular during state restore, and needs to
     * be able to find this constructor to instantiate it. If the empty constructor is not
     * available, a runtime exception will occur in some cases during state restore.
     */
    public static class MyAlertDialogFragment extends DialogFragment {
        private SMSCPreferenceActivity mActivity;

        public static MyAlertDialogFragment newInstance(
                SMSCPreferenceActivity activity, int sub, String smsc) {
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
                mActivity = (SMSCPreferenceActivity) getActivity();
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
        private SMSCPreferenceActivity mActivity;

        public static MyEditDialogFragment newInstance(
                SMSCPreferenceActivity activity, CharSequence title,
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
                mActivity = (SMSCPreferenceActivity) getActivity();
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
        SMSCPreferenceActivity mOwner;

        public SmscHandler(SMSCPreferenceActivity owner) {
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
                mSmscPrefList.get(sub).setSummary((end > 0)? summary.substring(1, end) : summary);
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
        updateSMSCPref();
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
    protected void onDestroy() {
        unregisterReceiver(mReceiver);
        super.onDestroy();
    }
}
