/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.server.wifi;

import com.android.server.wifi.util.NativeUtil;

import android.net.wifi.ScanResult;
import android.net.wifi.WifiSsid;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiConfiguration.KeyMgmt;
import android.util.Log;

import java.io.UnsupportedEncodingException;
import java.nio.ByteBuffer;
import java.nio.CharBuffer;
import java.nio.charset.CharacterCodingException;
import java.nio.charset.Charset;
import java.nio.charset.CharsetDecoder;
import java.nio.charset.UnsupportedCharsetException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Iterator;

/**
 * WifiGbk provides the valued addon for Gbk SSID support.
 */
public class WifiGbk {
    private static final String TAG = "WifiGbk";

    private static final boolean DBG = true;

    // BSSID Regix
    private static final String BSSID_REGIX = "(?:[0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}";

    // Max SSID Length
    public static final int MAX_SSID_LENGTH = 32;

    // Max UTF-8 SSID length: GBK SSID 32 bytes equals to UTF SSID 48 bytes
    public static final int MAX_SSID_UTF_LENGTH = 48;

    // BSS cache of 'Non-ASCII SSID' (e.g. 'Chinese SSID').
    private static final ArrayList<BssCache> mBssCacheList = new ArrayList<>();

    // BSS round robin Random record
    private static final HashMap<String, Integer> mBssRandom = new HashMap<>();

    // Expire BSS after number of scans
    private static final int SCAN_CACHE_EXPIRATION_COUNT = 2;

    private static Object mLock = new Object();

    /**
     * Log wrappers
     */
    protected static void loge(String s) {
        Log.e(TAG, s);
    }

    protected static void logi(String s) {
        Log.i(TAG, s);
    }

    protected static void logd(String s) {
        if (DBG) {
            Log.d(TAG, s);
        }
    }

    private static int getBssRandom(String SSID, int security) {
        synchronized (mLock) {
            String key = BssCache.bssToString(SSID, security);
            Integer rb = mBssRandom.get(key);
            if (rb == null) {
                mBssRandom.put(key, Integer.valueOf(0));
                return 0;
            }

            int rbInt = rb.intValue();
            rbInt ++;
            mBssRandom.put(key, Integer.valueOf(rbInt));

            return rbInt;
        }
    }

    /**
     * Checker - ScanResult
     */
    private static boolean isValid(ScanResult result) {
        if (result == null
               || result.wifiSsid == null
               || result.BSSID == null) {
            logi("Invalid ScanResult - BSSID=" + result.BSSID + " SSID=" + result.SSID);
            return false;
        }
        if (result.wifiSsid.isHidden()) {
            return false;
        }
        return true;
    }

    /**
     * Checker - BssCache
     */
    private static boolean isValid(BssCache bss) {
        if (bss == null
               || bss.SSID == null
               || bss.BSSID == null) {
            logi("Invalid BssCache - BSSID=" + bss.BSSID + " SSID=" + bss.SSID);
            return false;
        }
        return true;
    }

    /**
     * Get Bss Cache - BSSID and real ssid
     */
    private static BssCache getBssCache(String BSSID, byte[] ssidBytes) {
        for (BssCache bss : mBssCacheList) {
            if (bss.matches(BSSID, ssidBytes)) {
                return bss;
            }
        }
        return null;
    }

    /**
     * Get Bss Cache - BSSID and readable UTF SSID
     */
    private static BssCache getBssCache(String BSSID, String SSID) {
        for (BssCache bss : mBssCacheList) {
            if (bss.matches(BSSID, SSID)) {
                return bss;
            }
        }
        return null;
    }

    /**
     * Get preferred Bss Cache - readable UTF SSID and security
     */
    private static BssCache getPreferredBssCache(String SSID, int security) {
        int gbkCount = 0;
        int utfCount = 0;
        BssCache gbkBss = null;
        BssCache utfBss = null;

        // Detect the GBK and UTF Ap counters in air.
        for (BssCache bss : mBssCacheList) {
            if (bss.matches(SSID, security)) {
                if (bss.isGbk) {
                    gbkCount ++;
                    // gbkBss which has best RSSI
                    if (gbkBss != null) {
                        if (gbkBss.level > bss.level) {
                            gbkBss = bss;
                        }
                    } else {
                        gbkBss = bss;
                    }
                } else {
                    utfCount ++;
                    // utfBss which has best RSSI
                    if (utfBss != null) {
                        if (utfBss.level > bss.level) {
                            utfBss = bss;
                        }
                    } else {
                        utfBss = bss;
                    }
                }
            }
        }

        if (gbkCount != 0 && utfCount == 0) {
            logd("getPreferredBssCache - ssid=" + SSID
                 + " security=" + BssCache.securityToString(security)
                 + " gbk=" + gbkCount
                 + " utf=" + utfCount);
            return gbkBss;
        } else if (gbkCount != 0 && utfCount != 0) {
            int rand = getBssRandom(SSID, security);
            logd("getPreferredBssCache - ssid=" + SSID
                 + " security=" + BssCache.securityToString(security)
                 + " gbk=" + gbkCount
                 + " utf=" + utfCount
                 + " rand=" + rand);
            // Round robin between UTF and GBK.
            if (rand % 2 == 0) {
                return gbkBss;
            }
        }
        return utfBss;
    }

    /**
     * Add or Update scan cache
     */
    private static boolean addOrUpdateBssCache(ScanResult result) {
        synchronized (mLock) {
            byte[] ssidBytes = result.wifiSsid.getOctets();
            BssCache bss = getBssCache(result.BSSID, ssidBytes);
            if (bss == null) {
                bss = new BssCache(result);
                if (isValid(bss)) {
                    mBssCacheList.add(bss);
                    logd("adding bss - " + bss);
                }
            } else {
                bss.update(result);
            }
        }
        return true;
    }


    /**
     * Age out bss cache, which shall be called when receive scan result.
     */
    public static void ageBssCache() {
        synchronized (mLock) {
            Iterator<BssCache> it = mBssCacheList.iterator();
            while (it.hasNext()) {
                BssCache bss = it.next();
                bss.expire_count --;
                if (bss.expire_count <= 0) {
                    it.remove();
                    logd("removing bss - " + bss);
                }
            }
        }
        return;
    }

    /**
     * Clear Bss Cache
     */
    public static void clearBssCache() {
        synchronized (mLock) {
            mBssCacheList.clear();
            mBssRandom.clear();
        }
        return;
    }


    /**
     * Process Scan results whose SSID is non-ASCII.
     * Save it to BssCache, with conversion from gbkBytes to readable UTF String
     */
    public static boolean processScanResult(ScanResult result) {
        if (!isValid(result)) {
            return false;
        }

        byte[] ssidBytes = result.wifiSsid.getOctets();
        if (isAllAscii(ssidBytes)) {
            return false;
        }
        return addOrUpdateBssCache(result);
    }


    /**
     * If bssid set in WifiConfiguration, match the bssid and get real SSID.
     * else match the ssid and security, and get preferred SSID.
     */
    public static String getRealSsid(WifiConfiguration config) {
        String bssid = config.getNetworkSelectionStatus().getNetworkSelectionBSSID();
        boolean setbssid = (bssid != null && bssid.matches(BSSID_REGIX));
        BssCache bss = null;

        synchronized (mLock) {
            if (setbssid) {
                bss = getBssCache(bssid, config.SSID);
            } else {
                bss = getPreferredBssCache(config.SSID, BssCache.getSecurity(config));
            }

            if (bss != null) {
                logi("getRealSsid - BSSID=" + bssid + " - " + bss);
                if (bss.isGbk) {
                    String SSID2 = NativeUtil.hexStringFromByteArray(bss.ssidBytes);
                    return SSID2;
                }
            }
        }

        return config.SSID;
    }


    /**
     * Get random UTF or GBK bytes for SSID.
     * throws IllegalArgumentException e
     */
    public static byte[] getRandUtfOrGbkBytes(String SSID)
                throws IllegalArgumentException {
        byte [] utfBytes;
        byte [] gbkBytes;
        boolean utfSsidValid = false;
        boolean gbkSsidValid = false;

        // utfSsid
        utfBytes = NativeUtil.byteArrayFromArrayList(NativeUtil.decodeSsid(SSID));
        if (utfBytes == null || (utfBytes.length > MAX_SSID_UTF_LENGTH)) {
            // Important! check if ssidBytes exceed max length
            throw new IllegalArgumentException("Exceed max length " +
                    MAX_SSID_UTF_LENGTH +  ", ssid=" + SSID);
        }
        if (utfBytes != null && (utfBytes.length <= MAX_SSID_LENGTH)) {
            utfSsidValid = true;
        }

        // gbkSsid
        gbkBytes = isAllAscii(utfBytes) ? null : getSsidBytes(SSID, "GBK");
        if (gbkBytes != null && gbkBytes.length <= MAX_SSID_LENGTH) {
            gbkSsidValid = true;
        }

        if ((utfSsidValid == false) && (gbkSsidValid == true)) {
            return gbkBytes;
        } else if ((utfSsidValid == true) && (gbkSsidValid == false)){
            return utfBytes;
        } else if ((utfSsidValid == true) && (gbkSsidValid == true)) {
            // random pick
            int rand = getBssRandom(SSID, BssCache.SECURITY_NONE);
            logd("getRandUtfOrGbkBytes - ssid=" + SSID + " rand=" + rand);
            return (rand % 2 == 0) ? gbkBytes : utfBytes;
        }

        throw new IllegalArgumentException("No valid utfBytes or " +
                "gbkBytes for ssid=" + SSID);
    }


    /**
     * For Utf ssidBytes, it equals to WifiSsid.createFromByteArray().
     * For Gbk ssidBytes, it will convert to utfBytes, then create WifiSsid.
     */
    public static WifiSsid createWifiSsidFromByteArray(byte[] ssidBytes) {
        if (isGbk(ssidBytes)) {
            byte[] utfBytes = toUtf(ssidBytes);
            if (utfBytes != null) {
                return WifiSsid.createFromByteArray(utfBytes);
            }
        }
        return WifiSsid.createFromByteArray(ssidBytes);
    }

    /**
     * Helper method - check ssidBytes if all Ascii.
     */
    public static boolean isAllAscii(byte[] ssidBytes) {
        if (ssidBytes == null) {
            return false;
        }
        int length = ssidBytes.length;
        for (int i = 0; i < length; i ++) {
            if (ssidBytes[i] < 0) {
                return false;
            }
        }
        return true;
    }


    /**
     * Helper method - check ssidBytes if GBK.
     * @return true only it matches GBK rule but not match UTF rule.
     */
    public static boolean isGbk(byte[] ssidBytes) {
        // Return false if matches UTF rule.
        String ssid = encodeSsid(ssidBytes, "UTF-8");
        if (ssid != null) {
            return false;
        }

        // Return false if not matches Gbk rule
        ssid = encodeSsid(ssidBytes, "GBK");
        if (ssid == null) {
            return false;
        }

        return true;
    }

    /**
     * Helper method - GbkBytes to utfBytes
     */
    public static byte[] toUtf(byte[] gbkBytes) {
        String ssid = encodeSsid(gbkBytes, "GBK");
        if (ssid == null) {
            return null;
        }
        return getSsidBytes(ssid, "UTF-8");
    }

    /**
     * Helper method - utfBytes to GbkBytes
     */
    public static byte[] toGbk(byte[] utfBytes) {
        String ssid = encodeSsid(utfBytes, "UTF-8");
        if (ssid == null) {
            return null;
        }
        return getSsidBytes(ssid, "GBK");
    }

    /**
     * Helper method - get ssid bytes from Quoted String.
     */
    public static byte[] getSsidBytes(String ssid, String charsetName) {
        if (ssid == null) {
            return null;
        }

        String bareSsid = NativeUtil.removeEnclosingQuotes(ssid);
        byte ssidBytes[] = null;
        try {
            ssidBytes = bareSsid.getBytes(charsetName);
        } catch (UnsupportedEncodingException cce) {
           // Unsupported
        }

        int maxlen = "UTF-8".equals(charsetName) ?
                MAX_SSID_UTF_LENGTH : MAX_SSID_LENGTH;
        if (ssidBytes.length > maxlen) {
            loge("getSsidBytes - converted SSID exceed max length " +
                    maxlen + ", ssid=" + ssid);
            ssidBytes = null;
        }
        return ssidBytes;
    }

    /**
     * Helper method - get Quoted String from ssid bytes based on Charset.
     */
    public static String encodeSsid(byte[] ssidBytes, String name) {
        String ssid = null;
        try {
            Charset charset = Charset.forName(name);
            CharsetDecoder decoder = charset.newDecoder();
            CharBuffer decoded = decoder.decode(ByteBuffer.wrap(ssidBytes));
            ssid = "\"" + decoded.toString() + "\"";
        } catch (UnsupportedCharsetException cce) {
        } catch (CharacterCodingException cce) {
        }

        int maxlen = "UTF-8".equals(name) ?
                MAX_SSID_UTF_LENGTH : MAX_SSID_LENGTH;
        if (ssid != null && ssid.length() > (maxlen + 2)) {
            loge("encodeSsid - converted SSID exceed max length " +
                    maxlen +  ", ssid=" + ssid);
            ssid = null;
        }

        return ssid;
    }

    /**
     * BssCache reprents Bss with Chinese SSIDs, including UTF-8 and GBK.
     */
    private static class BssCache {
        // Real SSID in ocets.
        public byte[] ssidBytes;

        // Real SSID is Gbk?
        boolean isGbk;

        // Readable SSID for framework, UTF encoded.
        public String SSID;

        // BSSID
        public String BSSID;

        // Security
        public int security;

        public static final int SECURITY_NONE = 0;
        public static final int SECURITY_WEP = 1;
        public static final int SECURITY_PSK = 2;
        public static final int SECURITY_EAP = 3;

        // RSSI
        public int level;

        // Freqency
        public int frequency;

        // Expire count
        public int expire_count;

        public BssCache() {}

        public BssCache(ScanResult result) {
            this.ssidBytes = result.wifiSsid.getOctets();
            this.isGbk = isGbk(ssidBytes);
            // Add Quotes to result.SSID as it is not quoted.
            this.SSID = NativeUtil.addEnclosingQuotes(result.SSID);
            this.BSSID = result.BSSID;
            this.security = BssCache.getSecurity(result);
            this.level = result.level;
            this.frequency = result.frequency;
            this.expire_count = SCAN_CACHE_EXPIRATION_COUNT;

            if (isGbk) {
                // Override BSSCache's SSID
                String ssid = encodeSsid(ssidBytes, "GBK");
                this.SSID = ssid;

                // Override ScanResult's SSID and wifiSsid
                replaceSSIDinScanResult(result);
            }
        }

        public void update(ScanResult result) {
            // TODO: remove this protect?
            byte[] ssidBytes = result.wifiSsid.getOctets();
            if (!this.matches(result.BSSID, ssidBytes)) {
                return;
            }

            this.security = BssCache.getSecurity(result);
            this.level = result.level;
            this.frequency = result.frequency;
            this.expire_count = SCAN_CACHE_EXPIRATION_COUNT;

            if (this.isGbk) {
                // Override ScanResult's SSID and wifiSsid
                replaceSSIDinScanResult(result);
            }
        }

        private boolean replaceSSIDinScanResult(ScanResult result) {
            byte[] utfBytes = getSsidBytes(this.SSID, "UTF-8");
            if (this.SSID == null || utfBytes == null) {
                loge("replaceSSIDinScanResult fail - result=" + result);
                return false;
            } else {
                result.SSID = NativeUtil.removeEnclosingQuotes(this.SSID);
                result.wifiSsid = WifiSsid.createFromByteArray(utfBytes);
            }
            return true;
        }

        public boolean matches(String BSSID, byte[] ssidBytes) {
            if (!this.BSSID.equals(BSSID)) {
                return false;
            }
            return Arrays.equals(this.ssidBytes, ssidBytes);
        }

        public boolean matches(String BSSID, String SSID) {
            if (!this.BSSID.equals(BSSID)) {
                return false;
            }
            return this.SSID.equals(SSID);
        }

        public boolean matches(String SSID, int security) {
            if (!this.SSID.equals(SSID)) {
                return false;
            }
            return (this.security == security);
        }

        public static int getSecurity(ScanResult result) {
            if (result.capabilities.contains("WEP")) {
                return SECURITY_WEP;
            } else if (result.capabilities.contains("PSK")) {
                return SECURITY_PSK;
            } else if (result.capabilities.contains("EAP")) {
                return SECURITY_EAP;
            }
            return SECURITY_NONE;
        }

        public static int getSecurity(WifiConfiguration config) {
            if (config.allowedKeyManagement.get(KeyMgmt.WPA_PSK)) {
                return SECURITY_PSK;
            }
            if (config.allowedKeyManagement.get(KeyMgmt.WPA_EAP) ||
                    config.allowedKeyManagement.get(KeyMgmt.IEEE8021X)) {
                return SECURITY_EAP;
            }
            return (config.wepKeys[0] != null) ? SECURITY_WEP : SECURITY_NONE;
        }

        public static String securityToString(int security) {
            switch(security) {
                case SECURITY_NONE:
                    return "NONE";
                case SECURITY_WEP:
                    return "WEP";
                case SECURITY_PSK:
                    return "PSK";
                case SECURITY_EAP:
                    return "EAP";
                default:
                    return "?";
            }
        }

        public static String bssToString(String SSID, int security) {
            return SSID + securityToString(security);
        }

        @Override
        public String toString() {
            StringBuilder builder = new StringBuilder().append("Bss(")
                .append(SSID);

            if (BSSID != null) {
                builder.append(":").append(BSSID);
            }
            builder.append(", isGbk=").append(isGbk);
            builder.append(", security=").append(securityToString(security));
            builder.append(", level=").append(level);
            builder.append(", frequency=").append(frequency);

            return builder.append(')').toString();
        }
    }
}

