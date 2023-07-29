package com.android.systemui.statusbar.phone.customview;

import android.content.Context;
import android.database.ContentObserver;
import android.graphics.Rect;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkRequest;
import android.net.TrafficStats;
import android.os.Handler;
import android.os.Message;
import android.provider.Settings;
import android.util.AttributeSet;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;
import android.content.res.Configuration;

import com.android.systemui.Dependency;
import com.android.systemui.R;
import com.android.systemui.plugins.DarkIconDispatcher;
import com.android.systemui.plugins.DarkIconDispatcher.DarkReceiver;
//import com.android.systemui.statusbar.policy.DarkIconDispatcher;
//import com.android.systemui.statusbar.policy.DarkIconDispatcher.DarkReceiver;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;

public class NetworkSpeedIndicatorView extends FrameLayout implements DarkReceiver {

    private static final String TAG = "NetworkSpeedTest";
    private static final String UNIT_KS = " K/s";
    private static final String UNIT_MS = " M/s";
    private static final String UNIT_KS_en = "KB/s";
    private static final String UNIT_MS_en = "MB/s";
    private static final String BIG_UP_TRIANGLE = " ▲ ";// \u25B2
    private static final String BIG_DOWN_TRIANGLE = " ▼ ";// \u25BC
    private static final String BIG_UP_HOLLOW_TRIANGLE = " △ ";// \u25B3
    private static final String BIG_DOWN_HOLLOW_TRIANGLE = " ▽ ";// \u25BD
    private static final int START_SHOW_NETWORK_SPEED = 1;
    private static final int STOP_SHOW_NETWORK_SPEED = 2;
    private static final int REFRESH_NETWORK_SPEED = 3;
    public static final String STATUS_BAR_NETWORK_SPEED = "status_bar_network_speed";
    private static final int REFRESH_INTERVAL = 2;

    private ScheduledExecutorService scheduledExecutor;
    private String unitKs, unitMs, speed0;
    private TextView mDownloadSpeedText, mPostSpeedText, mDownloadSpeedIconText, mPostSpeedIconText;
    private Context mContext;
    private boolean mIsConn;
    private SettingsValueChangeContentObserver mContentOb;
    private NetUtil mNetUtil;
    private long mLastRxTimeStamp, mLastTotalRxBytes, mLastTxTimeStamp, mLastTotalTxBytes;
    private List<Network> mNetworks = new ArrayList<>();

    private final Handler mHandler = new Handler(){
        @Override
        public void handleMessage(Message msg) {
            super.handleMessage(msg);
            if(msg.what == START_SHOW_NETWORK_SPEED){
                start();
            }else if(msg.what == STOP_SHOW_NETWORK_SPEED){
                stop();
            } else if(msg.what == REFRESH_NETWORK_SPEED){
                showNetSpeed();
            }
        }
    };

    public NetworkSpeedIndicatorView(Context context) {
        this(context, null);
    }

    public NetworkSpeedIndicatorView(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public NetworkSpeedIndicatorView(Context context, AttributeSet attrs, int defStyleAttr) {
        this(context, attrs, defStyleAttr, 0);
    }

    public NetworkSpeedIndicatorView(Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);
        mContext = context;
        init();
    }

    private void init() {
        initUnitText(getResources().getConfiguration().locale.getLanguage());

        initView();

        setTintWithBackground();

        checkNetConnInit();
    }

    private void initView(){
        LayoutInflater.from(mContext).inflate(R.layout.widget_network_speed_indicator_layout, this);
        mDownloadSpeedText = findViewById(R.id.download_tv);
        mPostSpeedText = findViewById(R.id.post_tv);
        mDownloadSpeedIconText = findViewById(R.id.download_icon_tv);
        mPostSpeedIconText = findViewById(R.id.post_icon_tv);
    }

    private void initUnitText(String language){
        if(language.equals(Locale.CHINESE.getLanguage())){
            unitKs = UNIT_KS;
            unitMs = UNIT_MS;
        }else{
            unitKs = UNIT_KS_en;
            unitMs = UNIT_MS_en;
        }
        speed0 = "0.0" + unitKs;
    }

    @Override
    public boolean hasOverlappingRendering() {
        return false;
    }

    private void setTintWithBackground(){
        Dependency.get(DarkIconDispatcher.class).addDarkReceiver(this);
    }

    @Override
    public void onDarkChanged(Rect area, float darkIntensity, int tint) {
        if (!DarkIconDispatcher.isInArea(area, this)) {
            return;
        }
        mPostSpeedText.setTextColor(tint);
        mDownloadSpeedText.setTextColor(tint);
        mDownloadSpeedIconText.setTextColor(tint);
        mPostSpeedIconText.setTextColor(tint);
    }

    @Override
    protected void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        Log.d(TAG, newConfig.getLocales().get(0).getCountry());
        String newLanguage = newConfig.getLocales().get(0).getLanguage();
        initUnitText(newLanguage);
    }

    private void checkNetConnInit() {
        if (mContentOb == null) {
            mContentOb = new SettingsValueChangeContentObserver();
            mContext.getContentResolver().registerContentObserver(Settings.System.getUriFor(STATUS_BAR_NETWORK_SPEED), true, mContentOb);
        }
        if(mNetUtil == null){
            mNetUtil = new NetUtil();
            NetworkRequest.Builder networkRequestBuilder = new NetworkRequest.Builder();
            NetworkRequest networkRequest = networkRequestBuilder.build();
            ConnectivityManager connectivityManager = mContext.getSystemService(ConnectivityManager.class);
            connectivityManager.registerNetworkCallback(networkRequest, mNetUtil);
        }
    }

    private void start() {
        mLastTotalRxBytes = TrafficStats.getTotalRxBytes();
        mLastRxTimeStamp = System.currentTimeMillis();
        mLastTotalTxBytes = TrafficStats.getTotalTxBytes();
        mLastTxTimeStamp = System.currentTimeMillis();
        updateAddOrSubtract();
        setVisibility(View.VISIBLE);
    }

    private void stop() {
        stopAddOrSubtract();
        setVisibility(View.GONE);
    }

    private void stopAddOrSubtract() {
        if (scheduledExecutor != null) {
            scheduledExecutor.shutdownNow();
            scheduledExecutor = null;
        }
    }

    private void updateAddOrSubtract() {
        if (scheduledExecutor == null) {
            scheduledExecutor = Executors.newSingleThreadScheduledExecutor();
            scheduledExecutor.scheduleWithFixedDelay(new Runnable() {
                @Override
                public void run() {
                    Message message = new Message();
                    message.what = REFRESH_NETWORK_SPEED;
                    mHandler.sendMessage(message);
                }
            }, 0, REFRESH_INTERVAL, TimeUnit.SECONDS);
        }
    }

    private void showNetSpeed() {
        String downLoadSpeed = getDownloadSpeed();
        String postSpeed = getPostSpeed();
        //Log.d(TAG, "showNetSpeed: downLoadSpeed = " + downLoadSpeed + "  postSpeed = " + postSpeed);

        mDownloadSpeedIconText.setText(downLoadSpeed.equals(speed0) ? BIG_DOWN_HOLLOW_TRIANGLE : BIG_DOWN_TRIANGLE);
        mPostSpeedIconText.setText(postSpeed.equals(speed0) ? BIG_UP_HOLLOW_TRIANGLE : BIG_UP_TRIANGLE);
        mDownloadSpeedText.setText(downLoadSpeed);
        mPostSpeedText.setText(postSpeed);
    }

    private String getDownloadSpeed() {
        String speed;
        long nowTotalRxBytes = TrafficStats.getTotalRxBytes();
        long nowRxTimeStamp = System.currentTimeMillis();
        long difTime = nowRxTimeStamp - mLastRxTimeStamp;
        speed = difTime <= 0 ? speed0 : calculateSpeed(nowTotalRxBytes, mLastTotalRxBytes, difTime);
        mLastRxTimeStamp = nowRxTimeStamp;
        mLastTotalRxBytes = nowTotalRxBytes;
        return speed;
    }

    private String getPostSpeed(){
        String speed;
        long nowTotalTxBytes = TrafficStats.getTotalTxBytes();
        long nowTxTimeStamp = System.currentTimeMillis();
        long difTime = nowTxTimeStamp - mLastTxTimeStamp;
        speed = difTime <= 0 ? speed0 : calculateSpeed(nowTotalTxBytes, mLastTotalTxBytes, difTime);
        mLastTxTimeStamp = nowTxTimeStamp;
        mLastTotalTxBytes = nowTotalTxBytes;
        return speed;
    }

    private String calculateSpeed(long nowBytes, long lastBytes, long time){
        String speed, endTotal;
        long speedTotal = (nowBytes - lastBytes) * 1000 / time;

        if (speedTotal == 0) {
            speed = speed0;
        } else if (speedTotal < 1000 * 1024) {
            endTotal = String.valueOf(speedTotal % 1024);
            speedTotal = speedTotal / 1024;
            if (endTotal.length() > 1) {
                endTotal = endTotal.substring(0, 1);
            }
            speed = speedTotal + "." + endTotal + unitKs;
        } else {
            endTotal = String.valueOf(speedTotal % (1024 * 1024));
            speedTotal = speedTotal / 1024 / 1024;
            if (endTotal.length() > 1) {
                endTotal = endTotal.substring(0, 1);
            }
            speed = speedTotal + "." + endTotal + unitMs;
        }
        return speed;
    }

    private class SettingsValueChangeContentObserver extends ContentObserver {
        public SettingsValueChangeContentObserver() {
            super(new Handler());
        }

        @Override
        public void onChange(boolean selfChange) {
            super.onChange(selfChange);
            if (mIsConn) {
                int state = Settings.System.getInt(mContext.getContentResolver(), STATUS_BAR_NETWORK_SPEED, 0);
                Message message = new Message();
                message.what = state == 1 ? START_SHOW_NETWORK_SPEED : STOP_SHOW_NETWORK_SPEED;
                mHandler.sendMessage(message);
            }
        }
    }

    private class NetUtil extends ConnectivityManager.NetworkCallback{

        @Override
        public void onAvailable(Network network) {
            super.onAvailable(network);
            mNetworks.add(network);
            if(mNetworks.size() < 2){
                mIsConn = true;
                Message message = new Message();
                boolean isOpen = Settings.System.getInt(getContext().getContentResolver(), STATUS_BAR_NETWORK_SPEED, 0) == 1;
                message.what = isOpen ? START_SHOW_NETWORK_SPEED : STOP_SHOW_NETWORK_SPEED;
                mHandler.sendMessage(message);
            }
        }

        @Override
        public void onLost(Network network) {
            super.onLost(network);
            mNetworks.remove(network);
            if(mNetworks.size() <= 0){
                mIsConn = false;
                Message message = new Message();
                message.what = STOP_SHOW_NETWORK_SPEED;
                mHandler.sendMessage(message);
            }
        }
    }
}
