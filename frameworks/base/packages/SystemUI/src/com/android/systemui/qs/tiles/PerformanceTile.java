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

import android.os.SystemClock;
import java.io.BufferedWriter;
import java.io.FileWriter;
import android.widget.Toast;
import android.os.BatteryManager;

public class PerformanceTile extends QSTileImpl<BooleanState> {

	private static final String TAG = "SystemUI.PerformanceTile";
	private final PerformanceGetter mPerformanceGetter;
	private DetailAdapter mDetailAdapter;
	private List<String> mPerformanceModeTexts = new ArrayList<>();
	private MyBroadcastReceiver mBroadcastReceiver;
	
	private static final String GPU_FREQ_PATH = "/sys/class/kgsl/kgsl-3d0/devfreq/max_freq";
	private static final int MAX_RETRY = 100;
	private static final String GPU_FREQ_NORMAL = "710000000";
	private static final String GPU_FREQ_PREFORMANCE = "710000000";
	private static final String GPU_FREQ_HIGH_PREFORMANCE = "787000000";
	private boolean mLowBatteryLevel = false;
	private static final int BATTERY_LEVEL_LOW = 20;
	private static final int BATTERY_LEVEL_NORMAL = 25;
	private int mPerformanceMode = OdinSettings.PERFORMANCE_NORMAL;

    public PerformanceTile(QSHost host){       
        super(host);
		OdinSettings.init(mContext);
		initFanModeTexts();
		mPerformanceGetter = new PerformanceGetter(mContext);
		mDetailAdapter = (PerformanceDetailAdapter) createDetailAdapter();
		registerReceiver();
    }

	private void registerReceiver(){
		IntentFilter intentFilter = new IntentFilter();
        intentFilter.addAction(Intent.ACTION_LOCALE_CHANGED);
        //intentFilter.addAction(Intent.ACTION_BATTERY_CHANGED);
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
        return new PerformanceDetailAdapter();
    }

    @Override
    protected void handleClick() {
		int getValue=mPerformanceGetter.getValue();
		mPerformanceMode = getValue;
		if(mLowBatteryLevel){
			Log.d(TAG, "进入低电量模式，不能设置 getValue=" + getValue);
			Toast.makeText(mContext, mContext.getResources().getString(R.string.low_electricity_prompt), Toast.LENGTH_SHORT).show();
			return;
		}
		setGpuFreq(getValue);
    	OdinSettings.performanceMode.set(getValue);
		Log.d(TAG, "handleClick text = " + mPerformanceGetter.getPerformanceStateText() + "  isOpen = " + mPerformanceGetter.isHighPerformanceOpened());
		refreshState(mPerformanceGetter.isHighPerformanceOpened());
    }

    private void setGpuFreq(int value){
		int retry = 0;
        long start = SystemClock.uptimeMillis();
		String gpuFreq="";
		if(value==OdinSettings.PERFORMANCE_NORMAL){
			gpuFreq=GPU_FREQ_NORMAL;
		}else if(value==OdinSettings.PERFORMANCE_STANDARD){
			gpuFreq=GPU_FREQ_PREFORMANCE;
		}else if(value==OdinSettings.PERFORMANCE_HIGH){
			gpuFreq=GPU_FREQ_HIGH_PREFORMANCE;
		}else{
			gpuFreq=GPU_FREQ_NORMAL;
		}
		Log.w(TAG, "gpuFreq =" + gpuFreq);
        while (retry++ != MAX_RETRY) {
            try (BufferedWriter out = new BufferedWriter(new FileWriter(GPU_FREQ_PATH))) {
                out.write(gpuFreq);
				out.close();
            } catch (Exception e) {
				Log.w(TAG, "gpuFreq fail=" + e);
                continue;
            }
            break;
        }
        if (retry > 1) {
            Log.w(TAG, "Tried " + retry + " times.");
        }
	}

	@Override
    protected void handleSecondaryClick() {
    	showDetail(true);
    }

    @Override
    protected void handleUpdateState(BooleanState state, Object arg) {
    	int value = OdinSettings.performanceMode.value();
		//Log.d(TAG, "handleUpdateState 1 text = " + mPerformanceGetter.getPerformanceStateText() + "  isOpen = " + mPerformanceGetter.isHighPerformanceOpened());
		mPerformanceGetter.flushState(value);
		//Log.d(TAG, "handleUpdateState 2 text = " + mPerformanceGetter.getPerformanceStateText() + "  isOpen = " + mPerformanceGetter.isHighPerformanceOpened());
        state.label = mPerformanceGetter.getPerformanceStateText();              
        state.value = mPerformanceGetter.isNormalPerformanceOpened();
		if(value==0){
			state.icon = ResourceIcon.get(R.drawable.ic_qs_fan_quiet);
		}else if(value==1){
			state.icon = ResourceIcon.get(R.drawable.ic_qs_performance_mode);
		}else if(value==2){
			state.icon = ResourceIcon.get(R.drawable.ic_qs_high_performance_mode);
		}
		//state.icon = state.value ? ResourceIcon.get(R.drawable.ic_qs_high_performance_mode) : ResourceIcon.get(R.drawable.ic_qs_performance_mode);
		state.dualTarget = true;
        state.state = state.value ? Tile.STATE_INACTIVE : Tile.STATE_ACTIVE;      
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
    	if(mPerformanceGetter != null){
			return mPerformanceGetter.getPerformanceStateText();
    	}
        return mContext.getString(R.string.quick_settings_performance_mode);
    }

    @Override
    public int getMetricsCategory() {
        return MetricsEvent.QS_PERFORMANCE;
    }

	private void initFanModeTexts(){
		mPerformanceModeTexts.add(mContext.getString(R.string.quick_settings_normal_mode));
		mPerformanceModeTexts.add(mContext.getString(R.string.quick_settings_performance_mode));
		mPerformanceModeTexts.add(mContext.getString(R.string.quick_settings_high_performance_mode));
	}

	private void refreshFanModeTexts(){
		mPerformanceModeTexts.clear();
		initFanModeTexts();
	}

    protected class PerformanceGetter{
        private final List<Integer> values = new ArrayList<>();
        private final Context mContext;
        private int mPoint;
        private String mPerformanceStateText;
        private boolean mIsHighPerformanceOpened;
		private boolean misNormalPerformanceOpened;

        public PerformanceGetter(Context context) {
            mContext = context;
            initValues();
            initState();
        }

        private void initState(){
            mPoint = 0;
            mPerformanceStateText = mContext.getString(R.string.quick_settings_normal_mode);
        }

        public void flushState(int value){
            mPoint = values.indexOf(value) + 1;
            setPerformanceState(value);
        }

        private void initValues(){
			values.add(OdinSettings.PERFORMANCE_NORMAL);
            values.add(OdinSettings.PERFORMANCE_STANDARD);
            values.add(OdinSettings.PERFORMANCE_HIGH);
        }

        public int getValue(){
            if(mPoint >= values.size()){
                mPoint = 0;
            }
            int value = values.get(mPoint++);
            setPerformanceState(value);
            return value;
        }

        private void setPerformanceState(int value){
            switch (value){
				case OdinSettings.PERFORMANCE_NORMAL:
                    mPerformanceStateText = mPerformanceModeTexts.get(0);
                    mIsHighPerformanceOpened = false;
					misNormalPerformanceOpened =true;
                    break;
                case OdinSettings.PERFORMANCE_STANDARD:
                    mPerformanceStateText = mPerformanceModeTexts.get(1);
                    mIsHighPerformanceOpened = false;
					misNormalPerformanceOpened =false;
                    break;
                case OdinSettings.PERFORMANCE_HIGH:
                    mPerformanceStateText = mPerformanceModeTexts.get(2);
                    mIsHighPerformanceOpened = true;
					misNormalPerformanceOpened =false;
                    break;
            }
        }

		public int findPerformanceModeValue(String modeName){
			if (modeName.equals(mPerformanceModeTexts.get(0))) {
				return values.get(0);
	        } else if (modeName.equals(mPerformanceModeTexts.get(1))) {
	        	return values.get(1);
	        }else if(modeName.equals(mPerformanceModeTexts.get(2))){
				return values.get(2);
			} else {
				return -1;
	        }
		}

        public String getPerformanceStateText() {
            return mPerformanceStateText;
        }

        public boolean isHighPerformanceOpened(){
            return mIsHighPerformanceOpened;
        }
		public boolean isNormalPerformanceOpened(){
            return misNormalPerformanceOpened;
        }
    }

   protected class PerformanceDetailAdapter implements DetailAdapter, QSDetailItems.Callback{

		private QSDetailItems mItems;
		private Item [] mItemArray = new Item[mPerformanceModeTexts.size()];

        @Override
        public CharSequence getTitle() {
            return mContext.getString(R.string.quick_settings_performance_mode);
        }

        @Override
        public Boolean getToggleState() {
            //return mPerformanceGetter.isHighPerformanceOpened();
            return null;
        }

        @Override
        public View createDetailView(Context context, View convertView, ViewGroup parent) {
        	Log.d(TAG, "FanTile: createDetailView convertView=" + (convertView != null));
			mItems = QSDetailItems.convertOrInflate(context, convertView, parent);
			initItems();
            mItems.setTagSuffix(mContext.getString(R.string.quick_settings_performance_mode));
            mItems.setCallback(this);
			/*if(!mState.value){
				mItems.setItems(null);
				mItems.setEmptyState(R.drawable.ic_qs_performance_mode, R.string.quick_settings_performance_mode);
			}*/
            return mItems;
        }

        @Override
        public Intent getSettingsIntent() {
            return null;
        }

        @Override
        public void setToggleState(boolean state) {		
        	MetricsLogger.action(mContext, MetricsEvent.QS_FAN, state);
			OdinSettings.performanceMode.set(state ? OdinSettings.PERFORMANCE_HIGH : OdinSettings.PERFORMANCE_STANDARD);
			handleRefreshState("");
			if(state){
				initItemsWithSwitchOpen();
				mItems.setItems(mItemArray);
				//mItems.refreshItemsChoiceState(0, R.drawable.ic_fan_mode_selected);
			}else{
				mItems.setItems(null);
				mItems.setEmptyState(R.drawable.ic_qs_performance_mode, R.string.quick_settings_performance_mode);
			}
        }

        @Override
        public int getMetricsCategory() {
            return MetricsEvent.QS_PERFORMANCE;
        }

		@Override
        public void onDetailItemClick(Item item) {
        	String modeName = String.valueOf(item.line1);
			int mode= mPerformanceGetter.findPerformanceModeValue(modeName);
        	Log.d(TAG, "onDetailItemClick = " + modeName + ", mode=" + mode);
			mPerformanceMode = mode;
            if(mLowBatteryLevel){
                Log.d(TAG, "进入低电量模式，不能设置 modeName=" + modeName);
                Toast.makeText(mContext, mContext.getResources().getString(R.string.low_electricity_prompt), Toast.LENGTH_SHORT).show();
                return;
            }
			setGpuFreq(mode);
			OdinSettings.performanceMode.set(mode);
			mItems.refreshItemsChoiceState(item, R.drawable.ic_fan_mode_selected);
			handleRefreshState(modeName);
        }

        @Override
        public void onDetailItemDisconnect(Item item) {
            // noop
        }

		private void initItems(){
			if(mItems != null){
				mItemArray = new Item[mPerformanceModeTexts.size()];
				for(int i = 0; i < mPerformanceModeTexts.size(); i++){
					final Item item = new Item();
					item.iconResId = findIconRes(mPerformanceModeTexts.get(i));
					item.line1 = mPerformanceModeTexts.get(i);
					if(mPerformanceGetter.getPerformanceStateText().equals(mPerformanceModeTexts.get(i))){
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
			if(modeName.equals(mPerformanceModeTexts.get(0))){
				return R.drawable.ic_qs_fan_quiet;
			} else if (modeName.equals(mPerformanceModeTexts.get(1))){
				return R.drawable.ic_qs_performance_mode;
			} else if(modeName.equals(mPerformanceModeTexts.get(2))){
				return R.drawable.ic_qs_high_performance_mode;
			}else{
				return -1;
			}
		}
		
    }

    private class MyBroadcastReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if(action.equals(Intent.ACTION_LOCALE_CHANGED)){
                refreshFanModeTexts();
            } else if (Intent.ACTION_BATTERY_CHANGED.equals(action)) {
                int batteryLevel = intent.getIntExtra(BatteryManager.EXTRA_LEVEL, 100);
                if(batteryLevel <= BATTERY_LEVEL_LOW){
                    if(!mLowBatteryLevel){
                        Log.d(TAG, "onReceive() mPerformanceMode=" + mPerformanceMode);
                        setGpuFreq(OdinSettings.PERFORMANCE_NORMAL);
                        OdinSettings.performanceMode.set(OdinSettings.PERFORMANCE_NORMAL);
                    }
                    mLowBatteryLevel = true;
                } if(batteryLevel >= BATTERY_LEVEL_NORMAL){
                    if(mLowBatteryLevel){
                        Log.d(TAG, "onReceive() mPerformanceMode=" + mPerformanceMode);
                        setGpuFreq(mPerformanceMode);
                        OdinSettings.performanceMode.set(mPerformanceMode);
                    }
                    mLowBatteryLevel = false;
                }
            }
        }
    }

}



