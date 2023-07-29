package com.android.settings.development;
import android.content.Context;

import androidx.annotation.Nullable;
import androidx.preference.Preference;

import com.android.settings.core.PreferenceControllerMixin;
import com.android.settingslib.development.AbstractEnableWifiADBPreferenceController;

public class WifiAdbPreferenceController extends AbstractEnableWifiADBPreferenceController implements
        PreferenceControllerMixin {
            
    private final DevelopmentSettingsDashboardFragment mFragment;

    public WifiAdbPreferenceController(Context context, DevelopmentSettingsDashboardFragment fragment) {
        super(context);
        mFragment = fragment;
    }

    public void onWifiAdbDialogConfirmed() {
        writeWifiAdbSetting(true);
    }

    public void onWifiAdbDialogDismissed() {
        updateState(mPreference);
    }

    @Override
    public void showConfirmationDialog(@Nullable Preference preference) {
        // EnableAdbWarningDialog.show(mFragment);
    }

    @Override
    public void dismissConfirmationDialog() {
        // intentional no-op
    }

    @Override
    public boolean isConfirmationDialogShowing() {
        // intentional no-op
        return false;
    }

    @Override
    protected void onDeveloperOptionsSwitchDisabled() {
        super.onDeveloperOptionsSwitchDisabled();
        writeWifiAdbSetting(false);
        mPreference.setChecked(false);
    }
}
