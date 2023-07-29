package com.android.settingslib.odinsettings;


import android.content.ContentResolver;
import android.content.Context;
import android.provider.Settings;

public class SettingsBase {

    private static ContentResolver cr;

    public static void init(Context context) {
        cr = context.getApplicationContext().getContentResolver();
    }

    private abstract static class SettingItem {

        protected final String key;

        public SettingItem(String key) {
            this.key = key;
        }

        public String key() {
            return key;
        }

    }

    public static class BooleanSettingItem extends SettingItem {

        private final boolean defaultValue;

        public BooleanSettingItem(String key, boolean defaultValue) {
            super(key);
            this.defaultValue = defaultValue;
        }

        public boolean defaultValue() {
            return defaultValue;
        }

        public boolean value() {
            return Settings.System.getInt(cr, key, defaultValue ? 1 : 0) != 0;
        }

        public void set(boolean value) {
            Settings.System.putInt(cr, key, value ? 1 : 0);
        }
    }

    public static class IntSettingItem extends SettingItem {

        private final int defaultValue;

        public IntSettingItem(String key, int defaultValue) {
            super(key);
            this.defaultValue = defaultValue;
        }

        public int defaultValue() {
            return defaultValue;
        }

        public int value() {
            return Settings.System.getInt(cr, key, defaultValue);
        }

        public void set(int value) {
            Settings.System.putInt(cr, key, value);
        }
    }

    public static class FloatSettingItem extends SettingItem {

        private final float defaultValue;

        public FloatSettingItem(String key, float defaultValue) {
            super(key);
            this.defaultValue = defaultValue;
        }

        public float defaultValue() {
            return defaultValue;
        }

        public float value() {
            return Settings.System.getFloat(cr, key, defaultValue);
        }

        public void set(float value) {
            Settings.System.putFloat(cr, key, value);
        }
    }

    public static class LongSettingItem extends SettingItem {

        private final long defaultValue;

        public LongSettingItem(String key, long defaultValue) {
            super(key);
            this.defaultValue = defaultValue;
        }

        public long defaultValue() {
            return defaultValue;
        }

        public long value() {
            return Settings.System.getLong(cr, key, defaultValue);
        }

        public void set(long value) {
            Settings.System.putLong(cr, key, value);
        }
    }

    public static class StringSettingItem extends SettingItem {

        private final String defaultValue;

        public StringSettingItem(String key, String defaultValue) {
            super(key);
            this.defaultValue = defaultValue;
        }

        public String defaultValue() {
            return defaultValue;
        }

        public String value() {
            String v = Settings.System.getString(cr, key);
            return v == null ? defaultValue : v;
        }

        public void set(String value) {
            Settings.System.putString(cr, key, value);
        }
    }
}
