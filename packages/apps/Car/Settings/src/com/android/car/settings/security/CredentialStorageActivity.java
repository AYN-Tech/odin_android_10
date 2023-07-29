/*
 * Copyright 2019 The Android Open Source Project
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

package com.android.car.settings.security;

import android.app.Activity;
import android.car.userlib.CarUserManagerHelper;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.UserInfo;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Process;
import android.os.RemoteException;
import android.os.UserHandle;
import android.os.UserManager;
import android.security.Credentials;
import android.security.KeyChain;
import android.security.KeyStore;
import android.text.TextUtils;
import android.widget.Toast;

import androidx.annotation.Nullable;
import androidx.fragment.app.FragmentActivity;

import com.android.car.settings.R;
import com.android.car.settings.common.ConfirmationDialogFragment;
import com.android.car.settings.common.Logger;
import com.android.internal.widget.LockPatternUtils;

import java.lang.ref.WeakReference;

/**
 * Handles resetting and installing keys into {@link KeyStore}. Only Settings and the CertInstaller
 * application are permitted to perform the install action.
 */
public class CredentialStorageActivity extends FragmentActivity {

    private static final Logger LOG = new Logger(CredentialStorageActivity.class);

    private static final String ACTION_INSTALL = "com.android.credentials.INSTALL";
    private static final String ACTION_RESET = "com.android.credentials.RESET";

    private static final String DIALOG_TAG = "com.android.car.settings.security.RESET_CREDENTIALS";
    private static final String CERT_INSTALLER_PKG = "com.android.certinstaller";

    private static final int CONFIRM_CLEAR_SYSTEM_CREDENTIAL_REQUEST = 1;

    private final KeyStore mKeyStore = KeyStore.getInstance();

    private CarUserManagerHelper mCarUserManagerHelper;
    private LockPatternUtils mUtils;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mCarUserManagerHelper = new CarUserManagerHelper(this);
        mUtils = new LockPatternUtils(this);
    }

    @Override
    protected void onResume() {
        super.onResume();

        if (isFinishing()) {
            return;
        }

        if (mCarUserManagerHelper.isCurrentProcessUserHasRestriction(
                UserManager.DISALLOW_CONFIG_CREDENTIALS)) {
            finish();
            return;
        }

        Intent intent = getIntent();
        String action = intent.getAction();
        if (ACTION_RESET.equals(action)) {
            showResetConfirmationDialog();
        } else if (ACTION_INSTALL.equals(action) && checkCallerIsCertInstallerOrSelfInProfile()) {
            Bundle installBundle = intent.getExtras();
            boolean allTasksComplete = installIfAvailable(installBundle);
            if (allTasksComplete) {
                finish();
            }
        }
    }

    private void showResetConfirmationDialog() {
        ConfirmationDialogFragment dialog = new ConfirmationDialogFragment.Builder(this)
                .setTitle(R.string.credentials_reset)
                .setMessage(R.string.credentials_reset_hint)
                .setPositiveButton(android.R.string.ok, arguments -> onResetConfirmed())
                .setNegativeButton(android.R.string.cancel, arguments -> finish())
                .build();
        dialog.show(getSupportFragmentManager(), DIALOG_TAG);
    }

    private void onResetConfirmed() {
        if (!mUtils.isSecure(mCarUserManagerHelper.getCurrentProcessUserId())) {
            new ResetKeyStoreAndKeyChain(this).execute();
        } else {
            startActivityForResult(new Intent(this, CheckLockActivity.class),
                    CONFIRM_CLEAR_SYSTEM_CREDENTIAL_REQUEST);
        }
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == CONFIRM_CLEAR_SYSTEM_CREDENTIAL_REQUEST) {
            if (resultCode == Activity.RESULT_OK) {
                new ResetKeyStoreAndKeyChain(this).execute();
            } else {
                finish();
            }
        }
    }

    /**
     * Check that the caller is either CertInstaller or Settings running in a profile of this user.
     */
    private boolean checkCallerIsCertInstallerOrSelfInProfile() {
        if (TextUtils.equals(CERT_INSTALLER_PKG, getCallingPackage())) {
            // CertInstaller is allowed to install credentials if it has the same signature as
            // Settings package.
            return getPackageManager().checkSignatures(getCallingPackage(), getPackageName())
                    == PackageManager.SIGNATURE_MATCH;
        }

        int launchedFromUserId;
        try {
            int launchedFromUid = android.app.ActivityManager.getService().getLaunchedFromUid(
                    getActivityToken());
            if (launchedFromUid == -1) {
                LOG.e(ACTION_INSTALL + " must be started with startActivityForResult");
                return false;
            }
            if (!UserHandle.isSameApp(launchedFromUid, Process.myUid())) {
                return false;
            }
            launchedFromUserId = UserHandle.getUserId(launchedFromUid);
        } catch (RemoteException e) {
            LOG.w("Unable to verify calling identity. ActivityManager is down.", e);
            return false;
        }

        UserManager userManager = (UserManager) getSystemService(Context.USER_SERVICE);
        UserInfo parentInfo = userManager.getProfileParent(launchedFromUserId);
        // Caller is running in a profile of this user
        return ((parentInfo != null) && (parentInfo.id
                == mCarUserManagerHelper.getCurrentProcessUserId()));
    }

    /**
     * Install credentials if available, otherwise do nothing.
     *
     * @return {@code true} if the installation is done and the activity should be finished, {@code
     * false} if an asynchronous task is pending and will finish the activity when it's done.
     */
    private boolean installIfAvailable(Bundle installBundle) {
        if (installBundle == null || installBundle.isEmpty()) {
            return true;
        }

        int uid = installBundle.getInt(Credentials.EXTRA_INSTALL_AS_UID, KeyStore.UID_SELF);

        if (uid != KeyStore.UID_SELF && !UserHandle.isSameUser(uid, Process.myUid())) {
            int dstUserId = UserHandle.getUserId(uid);

            // Restrict install target to the wifi uid.
            if (uid != Process.WIFI_UID) {
                LOG.e("Failed to install credentials as uid " + uid
                        + ": cross-user installs may only target wifi uids");
                return true;
            }

            Intent installIntent = new Intent(ACTION_INSTALL)
                    .setFlags(Intent.FLAG_ACTIVITY_FORWARD_RESULT)
                    .putExtras(installBundle);
            startActivityAsUser(installIntent, new UserHandle(dstUserId));
            return true;
        }

        boolean shouldFinish = true;
        if (installBundle.containsKey(Credentials.EXTRA_USER_PRIVATE_KEY_NAME)) {
            String key = installBundle.getString(Credentials.EXTRA_USER_PRIVATE_KEY_NAME);
            byte[] value = installBundle.getByteArray(Credentials.EXTRA_USER_PRIVATE_KEY_DATA);

            if (!mKeyStore.importKey(key, value, uid, KeyStore.FLAG_NONE)) {
                LOG.e("Failed to install " + key + " as uid " + uid);
                return true;
            }
            // The key was prepended USER_PRIVATE_KEY by the CredentialHelper. However,
            // KeyChain internally uses the raw alias name and only prepends USER_PRIVATE_KEY
            // to the key name when interfacing with KeyStore.
            // This is generally a symptom of CredentialStorage and CredentialHelper relying
            // on internal implementation details of KeyChain and imitating its functionality
            // rather than delegating to KeyChain for the certificate installation.
            if (uid == Process.SYSTEM_UID || uid == KeyStore.UID_SELF) {
                new MarkKeyAsUserSelectable(this,
                        key.replaceFirst("^" + Credentials.USER_PRIVATE_KEY, "")).execute();
                shouldFinish = false;
            }
        }

        int flags = KeyStore.FLAG_NONE;

        if (installBundle.containsKey(Credentials.EXTRA_USER_CERTIFICATE_NAME)) {
            String certName = installBundle.getString(Credentials.EXTRA_USER_CERTIFICATE_NAME);
            byte[] certData = installBundle.getByteArray(Credentials.EXTRA_USER_CERTIFICATE_DATA);

            if (!mKeyStore.put(certName, certData, uid, flags)) {
                LOG.e("Failed to install " + certName + " as uid " + uid);
                return shouldFinish;
            }
        }

        if (installBundle.containsKey(Credentials.EXTRA_CA_CERTIFICATES_NAME)) {
            String caListName = installBundle.getString(Credentials.EXTRA_CA_CERTIFICATES_NAME);
            byte[] caListData = installBundle.getByteArray(Credentials.EXTRA_CA_CERTIFICATES_DATA);

            if (!mKeyStore.put(caListName, caListData, uid, flags)) {
                LOG.e("Failed to install " + caListName + " as uid " + uid);
                return shouldFinish;
            }
        }

        sendBroadcast(new Intent(KeyChain.ACTION_KEYCHAIN_CHANGED));
        setResult(RESULT_OK);
        return shouldFinish;
    }

    /**
     * Background task to handle reset of both {@link KeyStore} and user installed CAs.
     */
    private static class ResetKeyStoreAndKeyChain extends AsyncTask<Void, Void, Boolean> {

        private final WeakReference<CredentialStorageActivity> mCredentialStorage;

        ResetKeyStoreAndKeyChain(CredentialStorageActivity credentialStorage) {
            mCredentialStorage = new WeakReference<>(credentialStorage);
        }

        @Override
        protected Boolean doInBackground(Void... unused) {
            CredentialStorageActivity credentialStorage = mCredentialStorage.get();
            if (credentialStorage == null || credentialStorage.isFinishing()
                    || credentialStorage.isDestroyed()) {
                return false;
            }

            credentialStorage.mUtils.resetKeyStore(
                    credentialStorage.mCarUserManagerHelper.getCurrentProcessUserId());

            try {
                KeyChain.KeyChainConnection keyChainConnection = KeyChain.bind(credentialStorage);
                try {
                    return keyChainConnection.getService().reset();
                } catch (RemoteException e) {
                    LOG.w("Failed to reset KeyChain", e);
                    return false;
                } finally {
                    keyChainConnection.close();
                }
            } catch (InterruptedException e) {
                LOG.w("Failed to reset KeyChain", e);
                Thread.currentThread().interrupt();
                return false;
            }
        }

        @Override
        protected void onPostExecute(Boolean success) {
            CredentialStorageActivity credentialStorage = mCredentialStorage.get();
            if (credentialStorage == null || credentialStorage.isFinishing()
                    || credentialStorage.isDestroyed()) {
                return;
            }
            if (success) {
                Toast.makeText(credentialStorage, R.string.credentials_erased,
                        Toast.LENGTH_SHORT).show();
            } else {
                Toast.makeText(credentialStorage, R.string.credentials_not_erased,
                        Toast.LENGTH_SHORT).show();
            }
            credentialStorage.finish();
        }
    }

    /**
     * Background task to mark a given key alias as user-selectable so that it can be selected by
     * users from the Certificate Selection prompt.
     */
    private static class MarkKeyAsUserSelectable extends AsyncTask<Void, Void, Boolean> {

        private final WeakReference<CredentialStorageActivity> mCredentialStorage;
        private final String mAlias;

        MarkKeyAsUserSelectable(CredentialStorageActivity credentialStorage, String alias) {
            mCredentialStorage = new WeakReference<>(credentialStorage);
            mAlias = alias;
        }

        @Override
        protected Boolean doInBackground(Void... unused) {
            CredentialStorageActivity credentialStorage = mCredentialStorage.get();
            if (credentialStorage == null || credentialStorage.isFinishing()
                    || credentialStorage.isDestroyed()) {
                return false;
            }
            try (KeyChain.KeyChainConnection keyChainConnection = KeyChain.bind(
                    credentialStorage)) {
                keyChainConnection.getService().setUserSelectable(mAlias, true);
                return true;
            } catch (RemoteException e) {
                LOG.w("Failed to mark key " + mAlias + " as user-selectable.", e);
                return false;
            } catch (InterruptedException e) {
                LOG.w("Failed to mark key " + mAlias + " as user-selectable.", e);
                Thread.currentThread().interrupt();
                return false;
            }
        }

        @Override
        protected void onPostExecute(Boolean result) {
            LOG.i(String.format("Marked alias %s as selectable, success? %s", mAlias, result));
            CredentialStorageActivity credentialStorage = mCredentialStorage.get();
            if (credentialStorage == null || credentialStorage.isFinishing()
                    || credentialStorage.isDestroyed()) {
                return;
            }
            credentialStorage.finish();
        }
    }
}
