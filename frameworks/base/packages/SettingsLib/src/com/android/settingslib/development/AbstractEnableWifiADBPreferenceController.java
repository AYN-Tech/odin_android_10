package com.android.settingslib.development;

import android.app.ActivityManager;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.os.UserManager;
import android.provider.Settings;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;
import androidx.localbroadcastmanager.content.LocalBroadcastManager;
import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;
import androidx.preference.SwitchPreference;
import androidx.preference.TwoStatePreference;
import android.os.SystemProperties;

import javax.security.auth.login.LoginException;
import android.util.Log;

import com.android.settingslib.core.ConfirmationDialogController;
public abstract class AbstractEnableWifiADBPreferenceController extends
        DeveloperOptionsPreferenceController implements ConfirmationDialogController{
    private static final String KEY_WIFI_ENABLE_ADB = "enable_wifi_adb";
    private static final String GLOBAL_WIFI_ADB_KEY = "GLOBAL_ENABLE_WIFI_ADB";
    public static final int ADB_WIFI_SETTING_ON = 1;
    public static final int ADB_WIFI_SETTING_OFF = 0;
    public static boolean firstBoot = true;
    public static boolean state =  false;
    public String TAG = "AbstractEnableWifiADBPreferenceController";

    protected SwitchPreference mPreference;

    public AbstractEnableWifiADBPreferenceController(Context context) {
        super(context);
    }

    @Override
    public void displayPreference(PreferenceScreen screen) {
        super.displayPreference(screen);
        if (isAvailable()) {
            mPreference = (SwitchPreference) screen.findPreference(KEY_WIFI_ENABLE_ADB);
        }
    }

    @Override
    public boolean isAvailable() {
        return mContext.getSystemService(UserManager.class).isAdminUser();
    }

    @Override
    public String getPreferenceKey() {
        return KEY_WIFI_ENABLE_ADB;
    }

    private boolean isWifiAdbEnabled() {
        final ContentResolver cr = mContext.getContentResolver();
        if(firstBoot){
            writeWifiAdbSetting(Settings.Global.getInt(cr, GLOBAL_WIFI_ADB_KEY, ADB_WIFI_SETTING_OFF)
            != ADB_WIFI_SETTING_OFF);
            firstBoot = false;
        }
        return Settings.Global.getInt(cr, GLOBAL_WIFI_ADB_KEY, ADB_WIFI_SETTING_OFF)
                != ADB_WIFI_SETTING_OFF;
    }

    @Override
    public void updateState(Preference preference) {
        ((TwoStatePreference) preference).setChecked(isWifiAdbEnabled());
    }

    public void enablePreference(boolean enabled) {
        if (isAvailable()) {
            mPreference.setEnabled(enabled);
        }
    }

    public void resetPreference() {
        if (mPreference.isChecked()) {
            mPreference.setChecked(false);
            handlePreferenceTreeClick(mPreference);
        }
    }

    public boolean haveDebugSettings() {
        return isWifiAdbEnabled();
    }

    @Override
    public boolean handlePreferenceTreeClick(Preference preference) {
        Log.e(TAG,"oncethings  handlePreferenceTreeClick");
        if (isUserAMonkey()) {
            return false;
        }
        
        if (TextUtils.equals(KEY_WIFI_ENABLE_ADB, preference.getKey())) {
            if (!isWifiAdbEnabled()) {
                writeWifiAdbSetting(true);
                //android.os.SystemProperties.set("sys.connect.adb.wifi", "1");
            } else {
                writeWifiAdbSetting(false);
                //android.os.SystemProperties.set("sys.connect.adb.wifi", "0");
            }
            return true;
        } else {
            return false;
        }
    }

    protected void writeWifiAdbSetting(boolean enabled) {
        //state = enabled;
        Settings.Global.putInt(mContext.getContentResolver(),
            GLOBAL_WIFI_ADB_KEY, enabled ? ADB_WIFI_SETTING_ON : ADB_WIFI_SETTING_OFF);
        enableWifiAdb(enabled);
    }

    private void enableWifiAdb(boolean enabled){
        if(enabled){
            android.os.SystemProperties.set("sys.connect.adb.wifi", "1");
        }else{
            android.os.SystemProperties.set("sys.connect.adb.wifi", "0");
        }
    }

    @VisibleForTesting
    boolean isUserAMonkey() {
        return ActivityManager.isUserAMonkey();
    }

}
