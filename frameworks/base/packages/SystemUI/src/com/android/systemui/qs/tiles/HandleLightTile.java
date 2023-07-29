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

public class HandleLightTile extends QSTileImpl<BooleanState> {

	private DetailAdapter mDetailAdapter;
	private List<String> mHandleLightTexts = new ArrayList<>();
	private boolean mLeftHandleLightState;
	private boolean mRightHandleLightState;
				
    public HandleLightTile(QSHost host){       
        super(host);
		OdinSettings.init(mContext);
		initHandleLightTexts();
		mDetailAdapter = (HandleLightDetailAdapter) createDetailAdapter();
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
        return new HandleLightDetailAdapter();
    }

    @Override
    protected void handleClick() {
    	mState.value = !mState.value;
    	setHandleLight(mState.value);
		//setJoystickLight(mState.value);
		refreshState(mState.value);
    }

	    @Override
    protected void handleSecondaryClick() {
    	showDetail(true);
    }

    @Override
    protected void handleUpdateState(BooleanState state, Object arg) {
        state.label = mContext.getString(R.string.quick_settings_handle_light);
        state.icon = ResourceIcon.get(R.drawable.ic_qs_handle_light);              
        state.value = getHandleLightState();  
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
        return mContext.getString(R.string.quick_settings_handle_light);
    }

    @Override
    public int getMetricsCategory() {
        return MetricsEvent.QS_HANDLE_LIGHT;
    }

	private void initHandleLightTexts(){
		mHandleLightTexts.add(mContext.getString(R.string.quick_settings_left_handle));
		mHandleLightTexts.add(mContext.getString(R.string.quick_settings_right_handle));
	}

	private void refreshAtmosphereLampTexts(){
		mHandleLightTexts.clear();
		initHandleLightTexts();
	}

	private void setHandleLight(boolean isOpen){
		OdinSettings.leftHandleLightEnabled.set(isOpen);
		OdinSettings.rightHandleLightEnabled.set(isOpen);
	}

	private boolean getHandleLightState(){
		boolean leftHandleLight = OdinSettings.leftHandleLightEnabled.value();
		boolean rightHandleLight = OdinSettings.rightHandleLightEnabled.value();
		if(!leftHandleLight && !rightHandleLight){
			return false;
		}else{
			return true;
		}
	}

	private void initLightState(){
		mLeftHandleLightState = getLightState(mContext.getString(R.string.quick_settings_left_handle));
		mRightHandleLightState = getLightState(mContext.getString(R.string.quick_settings_right_handle));
	}

	private boolean getLightState(String whatLight){
		if(whatLight.equals(mContext.getString(R.string.quick_settings_left_handle))){
			boolean leftHandleLight = OdinSettings.leftHandleLightEnabled.value();
			return leftHandleLight;
		}else if(whatLight.equals(mContext.getString(R.string.quick_settings_right_handle))){
			boolean rightHandleLight = OdinSettings.rightHandleLightEnabled.value();
			return rightHandleLight;
		}else{
			return false;
		}
	}

	private void setValueByHandleLightText(String        handleLightText, boolean value){
		if(handleLightText.equals(mContext.getString(R.string.quick_settings_left_handle))){
			OdinSettings.leftHandleLightEnabled.set(value);
		} else if(handleLightText.equals(mContext.getString(R.string.quick_settings_right_handle))){
			OdinSettings.rightHandleLightEnabled.set(value);
		}
	}
   protected class HandleLightDetailAdapter implements DetailAdapter, QSDetailItems.Callback{

		private QSDetailItems mItems;

        @Override
        public CharSequence getTitle() {
            return mContext.getString(R.string.quick_settings_handle_light);
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
            return MetricsEvent.QS_HANDLE_LIGHT;
        }

		@Override
        public void onDetailItemClick(Item item) {
        	String handleLightText = String.valueOf(item.line1);
        	Log.d("HandleLightTile", "onDetailItemClick = " + handleLightText);
			boolean whichState = setWhichStateChangeByHandleLightText(handleLightText);
			//1.改变单个灯开关
			setValueByHandleLightText(handleLightText, whichState);
			//2.改变细节面板UI
			mItems.refreshItemSwitchState(item, whichState);
			//3.刷新外部快捷按钮状态
			handleRefreshState(handleLightText);
        }

        @Override
        public void onDetailItemDisconnect(Item item) {
            // noop
        }

		private void initItems(){
			if(mItems != null){
				initLightState();
				Item [] itemArray = new Item[mHandleLightTexts.size()];
				for(int i = 0; i < mHandleLightTexts.size(); i++){
					final Item item = new Item();
					item.iconResId = findIconRes(mHandleLightTexts.get(i));
					item.line1 = mHandleLightTexts.get(i);
					item.isShowItemSwitch = true;
					item.switchState = getLightState(mHandleLightTexts.get(i));
					itemArray[i] = item;
				}
				mItems.setItems(itemArray);
			}
		}

		private int findIconRes(String handleLightText){
			if(handleLightText.equals(mHandleLightTexts.get(0))){
				return R.drawable.ic_qs_handle_light;
			} else if (handleLightText.equals(mHandleLightTexts.get(1))){
				return R.drawable.ic_qs_handle_light;
			} else {
				return -1;
			}
		}

		private boolean setWhichStateChangeByHandleLightText(String handleLightText){
			if(mContext.getString(R.string.quick_settings_left_handle).equals(handleLightText)){
				mLeftHandleLightState = !mLeftHandleLightState;
				return mLeftHandleLightState;
			} else if (mContext.getString(R.string.quick_settings_right_handle).equals(handleLightText)){
				mRightHandleLightState = !mRightHandleLightState;
				return mRightHandleLightState;
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




