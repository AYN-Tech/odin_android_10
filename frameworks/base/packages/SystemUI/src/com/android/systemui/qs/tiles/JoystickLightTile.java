package com.android.systemui.qs.tiles;

import android.util.Log;
import com.android.systemui.R;
import com.android.systemui.plugins.qs.QSTile.BooleanState;
import com.android.systemui.qs.QSHost;
import com.android.systemui.qs.tileimpl.QSTileImpl;
import android.content.Intent;
import android.content.Context;
import android.service.quicksettings.Tile;
import com.android.internal.logging.MetricsLogger;
import com.android.internal.logging.nano.MetricsProto.MetricsEvent;
import com.android.settingslib.odinsettings.OdinSettings;
import com.android.systemui.qs.QSDetailItems;
import com.android.systemui.qs.QSDetailItems.Item;
import java.util.ArrayList;
import java.util.List;
import android.view.View;
import android.view.ViewGroup;
import com.android.systemui.plugins.qs.DetailAdapter;
import android.content.IntentFilter;
import android.content.BroadcastReceiver;

public class JoystickLightTile extends QSTileImpl<BooleanState> {

	private DetailAdapter mDetailAdapter;
	private List<String> mJoystickLightTexts = new ArrayList<>();
	private boolean mLeftJoystickLightState;
	private boolean mRightJoystickLightState;
				
    public JoystickLightTile(QSHost host){       
        super(host);
		OdinSettings.init(mContext);
		initJoystickLightTexts();
		mDetailAdapter = (JoystickLightDetailAdapter) createDetailAdapter();
		registerReceiver();
    }

	private void registerReceiver(){
		IntentFilter intentFilter = new IntentFilter();
        intentFilter.addAction(Intent.ACTION_LOCALE_CHANGED);
		MyBroadcastReceiver mBroadcastReceiver = new MyBroadcastReceiver();
		mContext.registerReceiver(mBroadcastReceiver, intentFilter);
	}

    @Override
    public BooleanState newTileState() {
        return new BooleanState();
    }

    @Override
    public DetailAdapter getDetailAdapter() {
        return mDetailAdapter;
    }

    @Override
    protected DetailAdapter createDetailAdapter() {
        return new JoystickLightDetailAdapter();
    }

    @Override
    protected void handleClick() {
    	mState.value = !mState.value;
    	setJoystickLight(mState.value);
		refreshState(mState.value);
    }

	    @Override
    protected void handleSecondaryClick() {
    	showDetail(true);
    }

    @Override
    protected void handleUpdateState(BooleanState state, Object arg) {
        state.label = mContext.getString(R.string.quick_settings_joystick_light);
        state.icon = ResourceIcon.get(R.drawable.ic_qs_joystick_light);              
        state.value = getJoystickLightState();
		state.dualTarget = true;
        state.state = state.value ? Tile.STATE_ACTIVE : Tile.STATE_INACTIVE;      
    }

    @Override
    public Intent getLongClickIntent() {
        return new Intent();
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
        return mContext.getString(R.string.quick_settings_joystick_light);
    }

    @Override
    public int getMetricsCategory() {
        return MetricsEvent.QS_JOYSTICK_LIGHT;
    }

	private void initJoystickLightTexts(){
		mJoystickLightTexts.add(mContext.getString(R.string.quick_settings_left_joystick));
		mJoystickLightTexts.add(mContext.getString(R.string.quick_settings_right_joystick));
	}

	private void refreshAtmosphereLampTexts(){
		mJoystickLightTexts.clear();
		initJoystickLightTexts();
	}

	private void setJoystickLight(boolean isOpen){
		OdinSettings.leftJoystickLightEnabled.set(isOpen);
		OdinSettings.rightJoystickLightEnabled.set(isOpen);
	}

	private boolean getJoystickLightState(){
		boolean leftJoystickLight = OdinSettings.leftJoystickLightEnabled.value();
		boolean rightJoystickLight = OdinSettings.rightJoystickLightEnabled.value();
		if(!leftJoystickLight && !rightJoystickLight){
			return false;
		}else{
			return true;
		}
	}

	private void initLightState(){
		mLeftJoystickLightState = getLightState(mContext.getString(R.string.quick_settings_left_joystick));
		mRightJoystickLightState = getLightState(mContext.getString(R.string.quick_settings_right_joystick));
	}

	private boolean getLightState(String whatLight){
		if(whatLight.equals(mContext.getString(R.string.quick_settings_left_joystick))){
			boolean leftJoystickLight = OdinSettings.leftJoystickLightEnabled.value();
			return leftJoystickLight;
		}else if(whatLight.equals(mContext.getString(R.string.quick_settings_right_joystick))){
			boolean rightJoystickLight = OdinSettings.rightJoystickLightEnabled.value();
			return rightJoystickLight;
		}else{
			return false;
		}
	}

	private void setValueByJoystickLightText(String joystickLightText, boolean value){
		if(joystickLightText.equals(mContext.getString(R.string.quick_settings_left_joystick))){
			OdinSettings.leftJoystickLightEnabled.set(value);
		} else if(joystickLightText.equals(mContext.getString(R.string.quick_settings_right_joystick))){
			OdinSettings.rightJoystickLightEnabled.set(value);
		}
	}
   protected class JoystickLightDetailAdapter implements DetailAdapter, QSDetailItems.Callback{

		private QSDetailItems mItems;

        @Override
        public CharSequence getTitle() {
            return mContext.getString(R.string.quick_settings_joystick_light);
        }

        @Override
        public Boolean getToggleState() {
            return null;
        }

        @Override
        public View createDetailView(Context context, View convertView, ViewGroup parent) {
        	Log.d(TAG, "FanTile: createDetailView convertView=" + (convertView != null));
			mItems = QSDetailItems.convertOrInflate(context, convertView, parent);
			initItems();
            mItems.setTagSuffix(mContext.getString(R.string.quick_settings_joystick_light));
            mItems.setCallback(this);
            return mItems;
        }

        @Override
        public Intent getSettingsIntent() {
            return null;
        }

        @Override
        public void setToggleState(boolean state) {
        	/*Log.d("FanTile", "setToggleState state: " + state);
        	MetricsLogger.action(mContext, MetricsEvent.QS_ATMOSPHERE_LAMP, state);
			setHandleLight(state);
			setJoystickLight(state);
			handleRefreshState("");
			for(int i = 0; i < mItems.getItemsSize(); i++){
				mItems.refreshItemSwitchState(i, state, R.drawable.ic_fan_mode_selected);
			}*/
        }

        @Override
        public int getMetricsCategory() {
            return MetricsEvent.QS_JOYSTICK_LIGHT;
        }

		@Override
        public void onDetailItemClick(Item item) {
        	String joystickLighttext = String.valueOf(item.line1);
        	//Log.d("JoystickLightTile", "joystickLighttext = " + joystickLighttext);
			boolean whichState = setWhichStateChangeByJoystickLightText(joystickLighttext);
			//1.改变单个灯开关
			setValueByJoystickLightText(joystickLighttext, whichState);
			//2.改变细节面板UI
			mItems.refreshItemSwitchState(item, whichState);
			//3.刷新外部快捷按钮状态
			handleRefreshState(joystickLighttext);
        }

        @Override
        public void onDetailItemDisconnect(Item item) {
            // noop
        }

		private void initItems(){
			if(mItems != null){
				initLightState();
				Item [] itemArray = new Item[mJoystickLightTexts.size()];
				for(int i = 0; i < mJoystickLightTexts.size(); i++){
					final Item item = new Item();
					item.iconResId = findIconRes(mJoystickLightTexts.get(i));
					item.line1 = mJoystickLightTexts.get(i);
					item.isShowItemSwitch = true;
					item.switchState = getLightState(mJoystickLightTexts.get(i));
					itemArray[i] = item;
				}
				mItems.setItems(itemArray);
			}
		}

		private int findIconRes(String joystickLightText){
			if(joystickLightText.equals(mJoystickLightTexts.get(0))){
				return R.drawable.ic_qs_joystick_light;
			} else if (joystickLightText.equals(mJoystickLightTexts.get(1))){
				return R.drawable.ic_qs_joystick_light;
			} else {
				return -1;
			}
		}

		private boolean setWhichStateChangeByJoystickLightText(String joystickLightText){
			if(mContext.getString(R.string.quick_settings_left_joystick).equals(joystickLightText)){
				mLeftJoystickLightState = !mLeftJoystickLightState;
				return mLeftJoystickLightState;
			} else if (mContext.getString(R.string.quick_settings_right_joystick).equals(joystickLightText)){
				mRightJoystickLightState = !mRightJoystickLightState;
				return mRightJoystickLightState;
			}
			return false;
		}
    }

    private class MyBroadcastReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            if(intent.getAction().equals(Intent.ACTION_LOCALE_CHANGED)){
                refreshAtmosphereLampTexts();
            }
        }
    }

}





