package com.android.systemui.qs.tiles;

import android.content.Intent;
import android.provider.MediaStore;
import android.service.quicksettings.Tile;
import com.android.systemui.R;
import com.android.systemui.plugins.qs.QSTile;
import com.android.systemui.qs.QSHost;
import com.android.systemui.qs.tileimpl.QSTileImpl;
import com.android.systemui.screenshot.GlobalScreenshot;
import android.os.Message;
import android.os.Messenger;
import android.os.Handler;
import android.os.RemoteException;
import com.android.systemui.plugins.ActivityStarter;
import com.android.systemui.Dependency;
import com.android.internal.logging.nano.MetricsProto.MetricsEvent;
import java.util.function.Consumer;
import android.util.Log;
import android.net.Uri;

public class ScreenshotTile extends QSTileImpl<QSTile.BooleanState> {

	private static final int SCREEN_SHOT_MESSAGE = 10000;
	private final ActivityStarter mActivityStarter;

	private Handler mHandler = new Handler() {
		public void handleMessage(Message msg) {
			switch (msg.what) {  
			case SCREEN_SHOT_MESSAGE:  
				final Messenger callback = msg.replyTo;  
				GlobalScreenshot screenshot = new GlobalScreenshot(mContext); 

				screenshot.takeScreenshot(new Consumer<Uri>() {
		            @Override
		            public void accept(Uri uri) {
		            	//Log.d("ScreenshotTile", "accept uri = " + uri);
		            }
		        }, msg.arg1 > 0, msg.arg2 > 0);  
				break;						
			default:  
				break;	
			}  
		}  
	};

    public ScreenshotTile(QSHost host) {
        super(host);
		mActivityStarter = Dependency.get(ActivityStarter.class);
    }
    @Override
    public BooleanState newTileState() {
        return new BooleanState();
    }
    @Override
    protected void handleClick() {
		mActivityStarter.postStartActivityDismissingKeyguard(new Intent(), 0);
		Message msg = mHandler.obtainMessage(SCREEN_SHOT_MESSAGE);  
        mHandler.sendMessageDelayed(msg,1000);
    }
    @Override
    protected void handleUpdateState(BooleanState state, Object arg) {
        state.label = mContext.getString(R.string.quick_settings_screenshot_label);
        state.icon = ResourceIcon.get(R.drawable.ic_qs_screenshot);
		state.state = Tile.STATE_INACTIVE;
    }
    @Override
    public int getMetricsCategory() {
        return MetricsEvent.QS_SCREENSHOT;
    }
    @Override
    public Intent getLongClickIntent() {
        Intent intent =  new Intent();
		intent.setAction(Intent.ACTION_MAIN); 
        intent.addCategory(Intent.CATEGORY_APP_GALLERY);
		return intent;
    }
    @Override
    protected void handleSetListening(boolean listening) {
    }
    @Override
    public CharSequence getTileLabel() {
        return mContext.getString(R.string.quick_settings_screenshot_label);
    }
}

