package com.android.systemui.qs.tiles;

import android.util.Log;
import com.android.systemui.R;
import com.android.systemui.plugins.qs.QSTile.BooleanState;
import com.android.systemui.qs.QSHost;
import com.android.systemui.qs.tileimpl.QSTileImpl;
import android.content.Intent;
import android.provider.Settings;
import android.service.quicksettings.Tile;
import android.widget.Switch;
import com.android.internal.logging.nano.MetricsProto.MetricsEvent;
import com.android.systemui.settings.BrightnessController;
import android.os.SystemProperties;

import static android.provider.Settings.System.SCREEN_BRIGHTNESS_MODE;
import static android.provider.Settings.System.SCREEN_BRIGHTNESS_MODE_AUTOMATIC;
import static android.provider.Settings.System.SCREEN_BRIGHTNESS_MODE_MANUAL;

public class AutoBrightnessTile extends QSTileImpl<BooleanState> {

    private final Icon mIcon = ResourceIcon.get(R.drawable.ic_auto_brightness2);
    private boolean isAuto;
	
    public AutoBrightnessTile(QSHost host){       
        super(host);    
    }

    @Override
    public BooleanState newTileState() {
        return new BooleanState();
    }

    @Override
    protected void handleClick() {
        isAuto = !mState.value;
        Settings.System.putInt(mContext.getContentResolver(), 
            SCREEN_BRIGHTNESS_MODE,(isAuto ? 1 : 0) );
        refreshState(isAuto);
    }

    @Override
    protected void handleUpdateState(BooleanState state, Object arg) {
        if (state.slash == null) {
            state.slash = new SlashState();
        }
        isAuto = Settings.System.getInt(mContext.getContentResolver(),
            SCREEN_BRIGHTNESS_MODE, 0) == 1;
        state.label = mContext.getString(R.string.quick_settings_auto_brightness);
        state.icon = mIcon;              
        state.value = isAuto;
        state.slash.isSlashed = !state.value;             
        state.state = state.value ? Tile.STATE_ACTIVE : Tile.STATE_INACTIVE;      
    }

    @Override
    protected void handleUserSwitch(int newUserId) {
    }

    @Override
    public Intent getLongClickIntent() {
        return new Intent(Settings.ACTION_DISPLAY_SETTINGS);
    }

    @Override
    public void handleSetListening(boolean listening) {
    }

    @Override
    public boolean isAvailable() {       
        return true;
    }

    @Override
    public CharSequence getTileLabel() {
        return mContext.getString(R.string.quick_settings_auto_brightness);
    }

    @Override
    public int getMetricsCategory() {
        return MetricsEvent.ACTION_BRIGHTNESS_AUTO;
    }
}

