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
import android.provider.Settings;
import com.android.systemui.qs.QSDetailItems;
import com.android.systemui.qs.QSDetailItems.Item;
import java.util.ArrayList;
import java.util.List;
import android.view.View;
import android.view.ViewGroup;
import com.android.systemui.plugins.qs.DetailAdapter;
import android.content.IntentFilter;
import android.content.BroadcastReceiver;

import android.content.ContentResolver;
import android.database.ContentObserver;
import android.os.Handler;

public class FanTile extends QSTileImpl<BooleanState> {

	private final FanGetter mFanGetter;
	private DetailAdapter mDetailAdapter;
	private List<String> mFanModeTexts = new ArrayList<>();
	private MyBroadcastReceiver mBroadcastReceiver;
	
	private ContentObserver mPerformanceObserver = new ContentObserver(new Handler()) {
        @Override
        public void onChange(boolean selfChange) {
			// do something
			int value = OdinSettings.performanceMode.value();
			if(value==0){
				OdinSettings.fanModeControl.set(0);
			}else if(value==1){
				refreshFromPerformance();
			}else if(value==2){
				refreshFromHighPerformance();
			}
			refreshFanModeTexts();
			mFanGetter.freshValues();
			refreshState(mFanGetter.isFanOpen());
        }
    };
	
	private void refreshFromPerformance(){
		// first : change fan to quiet
		int fanValue = OdinSettings.fanModeControl.value();
		if(fanValue==OdinSettings.FAN_DISABLED){
			OdinSettings.fanModeControl.set(OdinSettings.FAN_SPEED_1);
		}	
	}
	
	private void refreshFromHighPerformance(){
		// first : change fan to quiet
		int fanValue = OdinSettings.fanModeControl.value();
		if(fanValue==OdinSettings.FAN_DISABLED || fanValue==OdinSettings.FAN_SPEED_1){
			OdinSettings.fanModeControl.set(OdinSettings.FAN_SPEED_2);
		}	
	}
				
    public FanTile(QSHost host){       
        super(host);
		OdinSettings.init(mContext);
		initFanModeTexts();
		mFanGetter = new FanGetter(mContext);
		mFanGetter.freshValues();
		mDetailAdapter = (FanDetailAdapter) createDetailAdapter();
		registerReceiver();
		registerPerformanceContentObserver();
    }
	
    private void registerPerformanceContentObserver() {
		 mContext.getContentResolver().registerContentObserver(Settings.System.getUriFor("performance_mode"), true, mPerformanceObserver);
    }

	private void registerReceiver(){
		IntentFilter intentFilter = new IntentFilter();
        intentFilter.addAction(Intent.ACTION_LOCALE_CHANGED);
		intentFilter.addAction(Intent.ACTION_SCREEN_ON);  
        intentFilter.addAction(Intent.ACTION_SCREEN_OFF);  
		intentFilter.addAction(Intent.ACTION_SHUTDOWN);
		mBroadcastReceiver = new MyBroadcastReceiver();
		mContext.registerReceiver(mBroadcastReceiver, intentFilter);
	}

    @Override
    public BooleanState newTileState() {
        return new BooleanState();
    }

	@Override
    public void setDetailListening(boolean listening) {
    }

    @Override
    public DetailAdapter getDetailAdapter() {
        return mDetailAdapter;
    }

    @Override
    protected DetailAdapter createDetailAdapter() {
        return new FanDetailAdapter();
    }

    @Override
    protected void handleClick() {
    	OdinSettings.fanModeControl.set(mFanGetter.getValue());
		//Log.d("FanTile", "handleClick text = " + mFanGetter.getFanStateText() + "  isOpen = " + mFanGetter.isFanOpen());
		refreshState(mFanGetter.isFanOpen());
    }

	    @Override
    protected void handleSecondaryClick() {
    	showDetail(true);
    }

    @Override
    protected void handleUpdateState(BooleanState state, Object arg) {
    	int value = OdinSettings.fanModeControl.value();
		//Log.d("FanTile", "handleUpdateState 1 text = " + mFanGetter.getFanStateText() + "  isOpen = " + mFanGetter.isFanOpen());
		mFanGetter.flushState(value);
		//Log.d("FanTile", "handleUpdateState 2 text = " + mFanGetter.getFanStateText() + "  isOpen = " + mFanGetter.isFanOpen());
        state.label = mFanGetter.getFanStateText();
        state.icon = ResourceIcon.get(R.drawable.ic_qs_fan);              
        state.value = mFanGetter.isFanOpen();  
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
    	if(mFanGetter != null){
			return mFanGetter.getFanStateText();
    	}
        return mContext.getString(R.string.quick_settings_fan_stop);
    }

    @Override
    public int getMetricsCategory() {
        return MetricsEvent.QS_FAN;
    }

	private void initFanModeTexts(){
		int value = OdinSettings.performanceMode.value();
		if(value==2){
			mFanModeTexts.add(mContext.getString(R.string.quick_settings_fan_sport));
		    mFanModeTexts.add(mContext.getString(R.string.quick_settings_fan_smart));
		}else{
			mFanModeTexts.add(mContext.getString(R.string.quick_settings_fan_quiet));
			mFanModeTexts.add(mContext.getString(R.string.quick_settings_fan_sport));
			//mFanModeTexts.add(mContext.getString(R.string.quick_settings_fan_extreme));
			mFanModeTexts.add(mContext.getString(R.string.quick_settings_fan_smart));
		}
	}

	private void refreshFanModeTexts(){
		mFanModeTexts.clear();
		initFanModeTexts();
	}

    protected class FanGetter{
        private final List<Integer> values = new ArrayList<>();
        private final Context mContext;
        private int mPoint = 1;
        private String mFanStateText;
        private boolean mIsFanOpen;

        public FanGetter(Context context) {
            mContext = context;
            initValues();
            initState();
        }

        private void initState(){
            mPoint = 1;
            mFanStateText = mContext.getString(R.string.quick_settings_fan_quiet);
        }

        public void flushState(int value){
        	/*if(value == OdinSettings.FAN_SPEED_2){
				value = OdinSettings.FAN_SPEED_3;
        	}*/
            mPoint = values.indexOf(value) + 1;
            setFanState(value);
        }

        private void initValues(){
            values.add(OdinSettings.FAN_DISABLED);
            values.add(OdinSettings.FAN_SPEED_1);
			values.add(OdinSettings.FAN_SPEED_2);
            values.add(OdinSettings.FAN_SPEED_AUTO);
        }
		
		private void freshValues(){
			values.clear();
			int value = OdinSettings.performanceMode.value();
			if(value==1){
				values.add(OdinSettings.FAN_SPEED_1);
				values.add(OdinSettings.FAN_SPEED_2);
				values.add(OdinSettings.FAN_SPEED_AUTO);
			}else if(value==2){
				values.add(OdinSettings.FAN_SPEED_2);
				values.add(OdinSettings.FAN_SPEED_AUTO);
			}else {
				values.add(OdinSettings.FAN_DISABLED);
				values.add(OdinSettings.FAN_SPEED_1);
				values.add(OdinSettings.FAN_SPEED_2);
				values.add(OdinSettings.FAN_SPEED_AUTO);
			}
        }

        public int getValue(){
            if(mPoint >= values.size()){
                mPoint = 0;
            }
            int value = values.get(mPoint++);
            setFanState(value);
            return value;
        }

        private void setFanState(int value){
			int proformanValue = OdinSettings.performanceMode.value();
			if(proformanValue ==2){
				switch (value){
					case OdinSettings.FAN_SPEED_2:
						mFanStateText = mFanModeTexts.get(0);
						mIsFanOpen = true;
                    break;
					case OdinSettings.FAN_SPEED_AUTO:
						mFanStateText = mFanModeTexts.get(1);
						mIsFanOpen = true;
                    break;
				}
			}else {
            switch (value){
                case OdinSettings.FAN_DISABLED:
                    mFanStateText = mContext.getString(R.string.quick_settings_fan_stop);
                    mIsFanOpen = false;
                    break;
                case OdinSettings.FAN_SPEED_1:
                    mFanStateText = mFanModeTexts.get(0);
                    mIsFanOpen = true;
                    break;
				case OdinSettings.FAN_SPEED_2:
                    mFanStateText = mFanModeTexts.get(1);
                    mIsFanOpen = true;
                    break;
                /*case OdinSettings.FAN_SPEED_3:
                    mFanStateText = mFanModeTexts.get(2);
                    mIsFanOpen = true;
                    break;*/
                case OdinSettings.FAN_SPEED_AUTO:
                    mFanStateText = mFanModeTexts.get(2);
                    mIsFanOpen = true;
                    break;
            }
			}
        }

		public int findFanModeValue(String modeName){
			int value = OdinSettings.performanceMode.value();
			if(value==0){
				if (modeName.equals(mFanModeTexts.get(0))) {
					return values.get(1);
				} else if (modeName.equals(mFanModeTexts.get(1))) {
					return values.get(2);
				} else if (modeName.equals(mFanModeTexts.get(2))) {
					return values.get(3);
				} else{
					return values.get(0);
				}
			}else{
				for (int i=0;i<mFanModeTexts.size();i++){
					if(modeName.equals(mFanModeTexts.get(i))){
						return values.get(i);
					}
				}
				return values.get(0);
			}
		}

        public String getFanStateText() {
            return mFanStateText;
        }

        public boolean isFanOpen(){
            return mIsFanOpen;
        }
    }

   protected class FanDetailAdapter implements DetailAdapter, QSDetailItems.Callback{
 
		private QSDetailItems mItems;
		private Item [] mItemArray = new Item[mFanModeTexts.size()];

        @Override
        public CharSequence getTitle() {
            return mContext.getString(R.string.quick_settings_fan_stop);
        }

        @Override
        public Boolean getToggleState() {
			if(OdinSettings.performanceMode.value()==OdinSettings.PERFORMANCE_STANDARD || 
					OdinSettings.performanceMode.value()==OdinSettings.PERFORMANCE_HIGH){
				return null;
			}
            return mFanGetter.isFanOpen();
        }

        @Override
        public View createDetailView(Context context, View convertView, ViewGroup parent) {
			mItems = QSDetailItems.convertOrInflate(context, convertView, parent);
			initItems();
            mItems.setTagSuffix(mContext.getString(R.string.quick_settings_fan_stop));
            mItems.setCallback(this);
			if(!mState.value){
				mItems.setItems(null);
				mItems.setEmptyState(R.drawable.ic_qs_fan, R.string.fan_is_off);
			}
            return mItems;
        }

        @Override
        public Intent getSettingsIntent() {
            return null;
        }

        @Override
        public void setToggleState(boolean state) {
			//1.刷新外部快捷按钮状态
        	MetricsLogger.action(mContext, MetricsEvent.QS_FAN, state);
			OdinSettings.fanModeControl.set(state ? OdinSettings.FAN_SPEED_1 : OdinSettings.FAN_DISABLED);
			handleRefreshState("");
			//2.更新细节面板UI
			if(state){
				initItemsWithSwitchOpen();
				mItems.setItems(mItemArray);
				//mItems.refreshItemsChoiceState(0, R.drawable.ic_fan_mode_selected);
			}else{
				mItems.setItems(null);
				mItems.setEmptyState(R.drawable.ic_qs_fan, R.string.fan_is_off);
			}
        }

        @Override
        public int getMetricsCategory() {
            return MetricsEvent.QS_FAN;
        }

		@Override
        public void onDetailItemClick(Item item) {
        	String modeName = String.valueOf(item.line1);
        	Log.d("FanTile", "onDetailItemClick = " + modeName);
			//1.改变风扇模式
			OdinSettings.fanModeControl.set(mFanGetter.findFanModeValue(modeName));
			//2.改变细节面板UI
			mItems.refreshItemsChoiceState(item, R.drawable.ic_fan_mode_selected);
			//3.刷新外部快捷按钮状态
			handleRefreshState(modeName);
        }

        @Override
        public void onDetailItemDisconnect(Item item) {
            // noop
        }

		private void initItems(){
			if(mItems != null){
				mItemArray = new Item[mFanModeTexts.size()];
				for(int i = 0; i < mFanModeTexts.size(); i++){
					final Item item = new Item();
					item.iconResId = findIconRes(mFanModeTexts.get(i));
					item.line1 = mFanModeTexts.get(i);
					if(mFanGetter.getFanStateText().equals(mFanModeTexts.get(i))){
						item.icon2 = R.drawable.ic_fan_mode_selected;
					}
					mItemArray[i] = item;
				}
				mItems.setItems(mItemArray);
			}
		}

		private void initItemsWithSwitchOpen(){
			if(mItemArray == null){
				return;
			}
			for(int i = 0; i < mItemArray.length; i++){
				mItemArray[i].icon2 = -1;
			}
			mItemArray[0].icon2 = R.drawable.ic_fan_mode_selected;
		}

		private int findIconRes(String modeName){
			if(modeName.equals(mFanModeTexts.get(0))){
				return R.drawable.ic_qs_fan_quiet;
			} else if (modeName.equals(mFanModeTexts.get(1))){
				return R.drawable.ic_qs_fan_sport;
			} else if (modeName.equals(mFanModeTexts.get(2))){
				return R.drawable.ic_qs_fan_smart;
			}else {
				return -1;
			}
		}
		
    }

    private class MyBroadcastReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            if(intent.getAction().equals(Intent.ACTION_LOCALE_CHANGED)){
                refreshFanModeTexts();
            } else if (intent.getAction().equals(Intent.ACTION_SCREEN_ON)){
            	//Log.d("FanTile", "onReceive ACTION_SCREEN_ON");
				//int value = mFanGetter.findFanModeValue(mFanGetter.getFanStateText());
				int value = OdinSettings.fanCloseScreenRecord.value();
				//Log.d("FanTile", "value = " + value);
				if (value != -1){
					OdinSettings.fanModeControl.set(value);
				}
				OdinSettings.fanCloseScreenRecord.set(-1);
            } else if (intent.getAction().equals(Intent.ACTION_SCREEN_OFF)){
				//Log.d("FanTile", "onReceive ACTION_SCREEN_OFF");
				OdinSettings.fanCloseScreenRecord.set(OdinSettings.fanModeControl.value());
				OdinSettings.fanModeControl.set(OdinSettings.FAN_DISABLED);
            } else if (intent.getAction().equals(Intent.ACTION_SHUTDOWN)){
				OdinSettings.fanCloseScreenRecord.set(-1);
			}
        }
    }

}


