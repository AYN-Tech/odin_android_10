package com.android.settingslib.odinsettings;

public class OdinSettings extends SettingsBase {

    public static final String VIDEO_OUTPUT_MODE_HDMI = "hdmi";
    public static final String VIDEO_OUTPUT_MODE_DISPLAY_PORT = "display_port";
	
	public static final int PERFORMANCE_NORMAL =0;
    public static final int PERFORMANCE_STANDARD = 1;
    public static final int PERFORMANCE_HIGH = 2;

	public static final int FAN_DISABLED = 0;
    public static final int FAN_SPEED_1 = 1;
    public static final int FAN_SPEED_2 = 2;
    public static final int FAN_SPEED_3 = 3;
    public static final int FAN_SPEED_AUTO = 4;

    public static final BooleanSettingItem leftJoystickLightEnabled = new BooleanSettingItem("left_joystick_light_enabled", false);
    public static final BooleanSettingItem rightJoystickLightEnabled = new BooleanSettingItem("right_joystick_light_enabled", false);
    public static final BooleanSettingItem leftHandleLightEnabled = new BooleanSettingItem("left_handle_light_enabled", false);
    public static final BooleanSettingItem rightHandleLightEnabled = new BooleanSettingItem("right_handle_light_enabled", false);
    public static final BooleanSettingItem turnOffBuiltInScreenWhenOutputVideo = new BooleanSettingItem("turn_off_built_in_screen_when_output_video", true);
    public static final BooleanSettingItem noticeMeWhenUsbConnected = new BooleanSettingItem("notice_me_when_usb_connected", true);
	public static final IntSettingItem fanCloseScreenRecord = new IntSettingItem("fan_close_screen_record", FAN_DISABLED);

    public static final StringSettingItem videoOutputMode = new StringSettingItem("video_output_mode", VIDEO_OUTPUT_MODE_HDMI);

    public static final IntSettingItem performanceMode = new IntSettingItem("performance_mode", PERFORMANCE_NORMAL);

	public static final String PACKAGE_NAME = "com.retrostation.odinsettings";

	public static final IntSettingItem fanModeControl = new IntSettingItem("fan_mode", FAN_DISABLED);

	public static final BooleanSettingItem flipButtonLayout = new BooleanSettingItem("flip_button_layout", false);
}
