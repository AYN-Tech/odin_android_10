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

public class AtmosphereLampTile extends QSTileImpl<BooleanState> {

    private DetailAdapter mDetailAdapter;
    private List<String> mAtmosphereLampTexts = new ArrayList<>();
    private boolean mLampSwitch;
    private boolean mHandleLightState;
    private boolean mJoystickLightState;
    private boolean mRightJoystickLightState;
    private boolean mLeftJoystickLightState;
    private boolean mRightHandleLightState;
    private boolean mLeftHandleLightState;
	private MyBroadcastReceiver mBroadcastReceiver;

    public AtmosphereLampTile(QSHost host){
        super(host);
        OdinSettings.init(mContext);
        initAtmosphereLampTexts();
        mDetailAdapter = (AtmosphereLampDetailAdapter) createDetailAdapter();
        registerReceiver();
    }

    private void registerReceiver(){
        IntentFilter intentFilter = new IntentFilter();
        intentFilter.addAction(Intent.ACTION_LOCALE_CHANGED);
        mBroadcastReceiver = new MyBroadcastReceiver();
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
        return new AtmosphereLampDetailAdapter();
    }

    @Override
    protected void handleClick() {
        mState.value = !mState.value;
        setHandleLight(mState.value);
        setJoystickLight(mState.value);
        refreshState(mState.value);
    }

    @Override
    protected void handleSecondaryClick() {
        showDetail(true);
    }

    @Override
    protected void handleUpdateState(BooleanState state, Object arg) {
        state.label = mContext.getString(R.string.quick_settings_atmosphere_lamp);
        state.icon = ResourceIcon.get(R.drawable.ic_qs_lights);
        state.value = getLampState();
        state.dualTarget = true;
        state.state = state.value ? Tile.STATE_ACTIVE : Tile.STATE_INACTIVE;
    }

    @Override
    public Intent getLongClickIntent() {
		//return mContext.getPackageManager().getLaunchIntentForPackage(OdinSettings.PACKAGE_NAME);
		return new Intent();
    }

    @Override
    public void handleSetListening(boolean listening) {
    }

	@Override
	public void onTileDestroy(){
		if(mBroadcastReceiver != null){
			mContext.unregisterReceiver(mBroadcastReceiver);
		}
	}

    @Override
    public boolean isAvailable() {
        return true;
    }

    @Override
    public CharSequence getTileLabel() {
        return mContext.getString(R.string.quick_settings_atmosphere_lamp);
    }

    @Override
    public int getMetricsCategory() {
        return MetricsEvent.QS_ATMOSPHERE_LAMP;
    }

    private void initAtmosphereLampTexts(){
    	mAtmosphereLampTexts.add(mContext.getString(R.string.quick_settings_left_joystick));
        mAtmosphereLampTexts.add(mContext.getString(R.string.quick_settings_right_joystick));
        mAtmosphereLampTexts.add(mContext.getString(R.string.quick_settings_left_handle));
        mAtmosphereLampTexts.add(mContext.getString(R.string.quick_settings_right_handle));
    }

    private void refreshAtmosphereLampTexts(){
        mAtmosphereLampTexts.clear();
        initAtmosphereLampTexts();
    }

    private void setJoystickLight(boolean isOpen){
        setRightJoystickLight(isOpen);
        setLeftJoystickLight(isOpen);

    }

    private void setRightJoystickLight(boolean isOpen){
        OdinSettings.rightJoystickLightEnabled.set(isOpen);
    }

    private void setLeftJoystickLight(boolean isOpen){
        OdinSettings.leftJoystickLightEnabled.set(isOpen);
    }

    private void setHandleLight(boolean isOpen){
        setRightHandleLight(isOpen);
        setLeftHandleLight(isOpen);
    }

    private void setRightHandleLight(boolean isOpen){
        OdinSettings.rightHandleLightEnabled.set(isOpen);
    }

    private void setLeftHandleLight(boolean isOpen){
        OdinSettings.leftHandleLightEnabled.set(isOpen);
    }

    private boolean getLampState(){
        boolean leftJoystickLight = OdinSettings.leftJoystickLightEnabled.value();
        boolean rightJoystickLight = OdinSettings.rightJoystickLightEnabled.value();
        boolean leftHandleLight = OdinSettings.leftHandleLightEnabled.value();
        boolean rightHandleLight = OdinSettings.rightHandleLightEnabled.value();
        return leftJoystickLight || rightJoystickLight || leftHandleLight || rightHandleLight;
    }

    private void initLightState(){
        mRightJoystickLightState = getLightState(mContext.getString(R.string.quick_settings_right_joystick));
        mLeftJoystickLightState = getLightState(mContext.getString(R.string.quick_settings_left_joystick));
        mRightHandleLightState = getLightState(mContext.getString(R.string.quick_settings_right_handle));
        mLeftHandleLightState = getLightState(mContext.getString(R.string.quick_settings_left_handle));
    }

    private boolean getLightState(String whatLight){
        if(whatLight.equals(mContext.getString(R.string.quick_settings_right_joystick))){
            return OdinSettings.rightJoystickLightEnabled.value();
        } else if (whatLight.equals(mContext.getString(R.string.quick_settings_left_joystick))){
            return OdinSettings.leftJoystickLightEnabled.value();
        } else if (whatLight.equals(mContext.getString(R.string.quick_settings_right_handle))){
            return OdinSettings.rightHandleLightEnabled.value();
        } else if (whatLight.equals(mContext.getString(R.string.quick_settings_left_handle))){
            return OdinSettings.leftHandleLightEnabled.value();
        } else {
            return false;
        }
    }

    private void setValueByAtmosphereLampText(String atmosphereLampText, boolean state){
        if (atmosphereLampText.equals(mContext.getString(R.string.quick_settings_right_joystick))){
            setRightJoystickLight(state);
        } else if (atmosphereLampText.equals(mContext.getString(R.string.quick_settings_left_joystick))){
            setLeftJoystickLight(state);
        } else if (atmosphereLampText.equals(mContext.getString(R.string.quick_settings_right_handle))){
            setRightHandleLight(state);
        } else if (atmosphereLampText.equals(mContext.getString(R.string.quick_settings_left_handle))){
            setLeftHandleLight(state);
        }
    }

    protected class AtmosphereLampDetailAdapter implements DetailAdapter, QSDetailItems.Callback{

        private QSDetailItems mItems;

        @Override
        public CharSequence getTitle() {
            return mContext.getString(R.string.quick_settings_atmosphere_lamp);
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
            mItems.setTagSuffix(mContext.getString(R.string.quick_settings_atmosphere_lamp));
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
            return MetricsEvent.QS_ATMOSPHERE_LAMP;
        }

        @Override
        public void onDetailItemClick(Item item) {
            String atmosphereLampText = String.valueOf(item.line1);
            //Log.d("AtmosphereLampTile", "onDetailItemClick = " + atmosphereLampText);
            boolean whichState = setWhichStateChangeByAtmosphereLampText(atmosphereLampText);
            //1.改变单个灯开关
            setValueByAtmosphereLampText(atmosphereLampText, whichState);
            //2.改变细节面板UI
            mItems.refreshItemSwitchState(item, whichState);
            //3.刷新外部快捷按钮状态
            handleRefreshState(atmosphereLampText);
        }

        @Override
        public void onDetailItemDisconnect(Item item) {
            // noop
        }

        private void initItems(){
            if(mItems != null){
                initLightState();
                Item [] itemArray = new Item[mAtmosphereLampTexts.size()];
                for(int i = 0; i < mAtmosphereLampTexts.size(); i++){
                    final Item item = new Item();
                    item.iconResId = findIconRes(mAtmosphereLampTexts.get(i));
                    item.line1 = mAtmosphereLampTexts.get(i);
                    item.isShowItemSwitch = true;
                    item.switchState = getLightState(mAtmosphereLampTexts.get(i));
                    itemArray[i] = item;
                }
                mItems.setItems(itemArray);
            }
        }

        private int findIconRes(String atmosphereLampText){
            if(atmosphereLampText.equals(mAtmosphereLampTexts.get(0)) || atmosphereLampText.equals(mAtmosphereLampTexts.get(1))){
                return R.drawable.ic_qs_joystick_light;
            } else if (atmosphereLampText.equals(mAtmosphereLampTexts.get(2)) || atmosphereLampText.equals(mAtmosphereLampTexts.get(3))){
                return R.drawable.ic_qs_handle_light;
            } else {
                return -1;
            }
        }

        private boolean setWhichStateChangeByAtmosphereLampText(String atmosphereLampText){
            if (mContext.getString(R.string.quick_settings_right_joystick).equals(atmosphereLampText)){
                mRightJoystickLightState = !mRightJoystickLightState;
                return mRightJoystickLightState;
            } else if (mContext.getString(R.string.quick_settings_left_joystick).equals(atmosphereLampText)){
                mLeftJoystickLightState = !mLeftJoystickLightState;
                return mLeftJoystickLightState;
            } else if (mContext.getString(R.string.quick_settings_right_handle).equals(atmosphereLampText)){
                mRightHandleLightState = !mRightHandleLightState;
                return mRightHandleLightState;
            } else if (mContext.getString(R.string.quick_settings_left_handle).equals(atmosphereLampText)){
                mLeftHandleLightState = !mLeftHandleLightState;
                return mLeftHandleLightState;
            } else {
                return false;
            }
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



