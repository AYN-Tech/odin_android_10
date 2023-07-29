package com.android.systemui.qs.settings;

import android.content.ContentResolver;
import android.content.Context;
import android.provider.Settings;

/**
                已弃用
**/
public class CustomSettings {

    public static final String KEY_FAN_MODE = "fan_mode";
    public static final String KEY_LEFT_JOYSTICK_LIGHT = "left_joystick_light";
    public static final String KEY_RIGHT_JOYSTICK_LIGHT = "right_joystick_light";
    public static final String KEY_LEFT_HANDLE_LIGHT = "key_left_handle_light";
    public static final String KEY_RIGHT_HANDLE_LIGHT = "key_right_handle_light";
    public static final String KEY_TURN_OFF_SCREEN_WHEN_HDMI_CONNECTED = "turn_off_screen_when_hdmi_connected";
    public static final String KEY_VIDEO_OUTPUT_MODE = "video_output_mode";
    public static final String KEY_PERFORMANCE_MODE = "performance_mode";

    public static final int FAN_DISABLED = 0;
    public static final int FAN_SPEED_1 = 1;
    public static final int FAN_SPEED_2 = 2;
    public static final int FAN_SPEED_3 = 3;
    public static final int FAN_SPEED_AUTO = 4;

    public static final int PERFORMANCE_DISABLED = 0;
    //public static final int PERFORMANCE_COOL = 1;
    public static final int PERFORMANCE_HIGH = 1;

    public static final String VIDEO_OUTPUT_MODE_HDMI = "hdmi";
    public static final String VIDEO_OUTPUT_MODE_DISPLAY_PORT = "display_port";

    private static ContentResolver resolver;

    public static void init(Context context) {
        resolver = context.getApplicationContext().getContentResolver();
    }

    public static void setInt(String key, int value) {
        Settings.System.putInt(resolver, key, value);
    }

    public static int getInt(String key, int def) {
        return Settings.System.getInt(resolver, key, def);
    }

    public static void setBoolean(String key, boolean value) {
        Settings.System.putInt(resolver, key, value ? 1 : 0);
    }

    public static boolean getBoolean(String key, boolean def) {
        return Settings.System.getInt(resolver, key, def ? 1 : 0) != 0;
    }

    public static void setString(String key, String value) {
        Settings.System.putString(resolver, key, value);
    }

    public static String getString(String key) {
        return Settings.System.getString(resolver, key);
    }
}
