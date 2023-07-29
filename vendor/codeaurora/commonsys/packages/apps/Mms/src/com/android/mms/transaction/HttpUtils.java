/*
 * Copyright (C) 2008 Esmertec AG.
 * Copyright (C) 2008 The Android Open Source Project
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

package com.android.mms.transaction;

import java.io.DataInputStream;
import java.io.IOException;
import java.net.SocketException;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.Locale;

import org.apache.http.HttpEntity;
import org.apache.http.HttpHost;
import org.apache.http.HttpRequest;
import org.apache.http.HttpResponse;
import org.apache.http.StatusLine;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.client.methods.HttpRequestBase;
import org.apache.http.conn.params.ConnRouteParams;
import org.apache.http.params.HttpConnectionParams;
import org.apache.http.params.HttpParams;
import org.apache.http.params.HttpProtocolParams;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.SystemClock;
import android.telephony.TelephonyManager;
import android.text.TextUtils;
import android.util.Config;
import android.util.Log;

import com.android.mms.LogTag;
import com.android.mms.MmsConfig;
import java.net.HttpURLConnection;

import java.net.Proxy;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.net.InetSocketAddress;
import java.net.URL;
import java.util.Map;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.List;
import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.ByteArrayOutputStream;
import java.net.MalformedURLException;
import java.net.ProtocolException;
import java.net.HttpURLConnection;
import com.android.i18n.phonenumbers.NumberParseException;
import com.android.i18n.phonenumbers.PhoneNumberUtil;
import com.android.i18n.phonenumbers.Phonenumber;
import android.telephony.SubscriptionInfo;
import android.telephony.SubscriptionManager;
import android.util.Base64;
import java.io.UnsupportedEncodingException;
import com.android.mmswrapper.TelephonyManagerWrapper;
import com.android.mmswrapper.SubscriptionManagerWrapper;
import com.google.android.mms.MmsException;

public class HttpUtils {
    private static final String TAG = LogTag.TRANSACTION;

    private static final boolean DEBUG = false;
    private static final boolean LOCAL_LOGV = DEBUG ? Config.LOGD : Config.LOGV;

    public static final int HTTP_POST_METHOD = 1;
    public static final int HTTP_GET_METHOD = 2;

    private static final int MMS_READ_BUFFER = 4096;

    // This is the value to use for the "Accept-Language" header.
    // Once it becomes possible for the user to change the locale
    // setting, this should no longer be static.  We should call
    // getHttpAcceptLanguage instead.
    private static final String HDR_VALUE_ACCEPT_LANGUAGE;

    static {
        HDR_VALUE_ACCEPT_LANGUAGE = getCurrentAcceptLanguage(Locale.getDefault());
    }

    // Definition for necessary HTTP headers.
    private static final String HDR_KEY_ACCEPT = "Accept";
    private static final String HDR_KEY_ACCEPT_LANGUAGE = "Accept-Language";

    private static final String HEADER_USER_AGENT = "User-Agent";
    private static final String HDR_VALUE_ACCEPT =
        "*/*, application/vnd.wap.mms-message, application/vnd.wap.sic";

    private static final String INTENT_HTTP_TIMEOUT_ALARM = "org.codeaurora.mms.http_timeout";
    private static final String INTENT_EXTRA_TAG = "mmstag";
    // 3min default timeout
    private static final int HTTP_TIMEOUT = 3 * 60 * 1000;
    private static long sAlarmTag = 0;
    private static HttpRequestBase sHttpReq = null;
    private static PendingIntent sAlarmIntent = null;
    private static IntentFilter sIntentFilter = new IntentFilter(INTENT_HTTP_TIMEOUT_ALARM);

    private static final Pattern MACRO_P = Pattern.compile("##(\\S+)##");
    private static final String MACRO_LINE1 = "LINE1";
    private static final String MACRO_LINE1NOCOUNTRYCODE = "LINE1NOCOUNTRYCODE";
    private static final String MACRO_NAI = "NAI";
    private static final String HEADER_CONTENT_TYPE = "Content-Type";
    private static final String HEADER_VALUE_CONTENT_TYPE_WITHOUT_CHARSET =
        "application/vnd.wap.mms-message";
    // The "Content-Type" header value
    private static final String HEADER_VALUE_CONTENT_TYPE_WITH_CHARSET =
        "application/vnd.wap.mms-message; charset=utf-8";

    private static final String METHOD_POST = "POST";
    private static final String METHOD_GET = "GET";

    private static HttpURLConnection mConnection = null;

    private static BroadcastReceiver sIntentReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (INTENT_HTTP_TIMEOUT_ALARM.equals(intent.getAction())) {
                long tag = intent.getLongExtra(INTENT_EXTRA_TAG, -1);
                Log.d(TAG, "[HttpUtils] HTTP timeout alarm. tag = " + tag
                        + " sAlarmTag = " + sAlarmTag);
                abortHttpRequest(tag);
            }
        }
    };

    private HttpUtils() {
        // To forbidden instantiate this class.
    }

    private static final String ACCEPT_LANG_FOR_US_LOCALE = "en-US";

    /**
     * Return the Accept-Language header.  Use the current locale plus
     * US if we are in a different locale than US.
     * This code copied from the browser's WebSettings.java
     * @return Current AcceptLanguage String.
     */
    public static String getCurrentAcceptLanguage(Locale locale) {
        StringBuilder buffer = new StringBuilder();
        addLocaleToHttpAcceptLanguage(buffer, locale);

        if (!Locale.US.equals(locale)) {
            if (buffer.length() > 0) {
                buffer.append(", ");
            }
            buffer.append(ACCEPT_LANG_FOR_US_LOCALE);
        }

        return buffer.toString();
    }

    /**
     * Convert obsolete language codes, including Hebrew/Indonesian/Yiddish,
     * to new standard.
     */
    private static String convertObsoleteLanguageCodeToNew(String langCode) {
        if (langCode == null) {
            return null;
        }
        if ("iw".equals(langCode)) {
            // Hebrew
            return "he";
        } else if ("in".equals(langCode)) {
            // Indonesian
            return "id";
        } else if ("ji".equals(langCode)) {
            // Yiddish
            return "yi";
        }
        return langCode;
    }

    private static void addLocaleToHttpAcceptLanguage(StringBuilder builder,
                                                      Locale locale) {
        String language = convertObsoleteLanguageCodeToNew(locale.getLanguage());
        if (language != null) {
            builder.append(language);
            String country = locale.getCountry();
            if (country != null) {
                builder.append("-");
                builder.append(country);
            }
        }
    }

    private static synchronized void startAlarmForTimeout(Context context) {
        if (Log.isLoggable(LogTag.TRANSACTION, Log.VERBOSE)) {
            Log.d(TAG, "[HttpUtils] startAlarmForTimeout for " + HTTP_TIMEOUT);
        }
        Intent intent = new Intent(INTENT_HTTP_TIMEOUT_ALARM);
        intent.addFlags(Intent.FLAG_RECEIVER_FOREGROUND);
        sAlarmTag++;
        intent.putExtra(INTENT_EXTRA_TAG, sAlarmTag);
        sAlarmIntent = PendingIntent.getBroadcast (context, 0,
                intent, PendingIntent.FLAG_UPDATE_CURRENT);
        AlarmManager am = (AlarmManager) context.getSystemService(Context.ALARM_SERVICE);
        am.setExact(AlarmManager.ELAPSED_REALTIME_WAKEUP,
                SystemClock.elapsedRealtime() + HTTP_TIMEOUT, sAlarmIntent);
        context.registerReceiver(sIntentReceiver, sIntentFilter);

    }

    private static synchronized void cancelTimeoutAlarm(Context context) {
        if (Log.isLoggable(LogTag.TRANSACTION, Log.VERBOSE)) {
            Log.d(TAG, "HttpUtils: cancelTimeoutAlarm");
        }
        if (sAlarmIntent != null) {
            context.unregisterReceiver(sIntentReceiver);
            AlarmManager am = (AlarmManager) context.getSystemService(Context.ALARM_SERVICE);
            am.cancel(sAlarmIntent);
            sAlarmIntent = null;
        }
    }

    private static void abortHttpRequest(final long tag) {
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                synchronized(HttpUtils.class) {
                    Log.d(TAG, "[HttpUtils] abortHttpRequest. tag = " + tag
                            + " sAlarmTag = " + sAlarmTag);
                    if (tag == sAlarmTag && mConnection != null) {
                        try {
                            mConnection.disconnect();
                        } catch (Exception e) {
                            Log.w(TAG, "[HttpUtils] abortHttpRequest: Ex:" + e);
                        }
                    } else {
                        Log.d(TAG, "[HttpUtils] Alarm tags not matching or no req. Ignore!");
                    }
                }
            }
        };
        Thread abortThread = new Thread(runnable);
        abortThread.start();
    }

    /**
     * A helper method to send or retrieve data through HTTP protocol.
     *
     * @param token The token to identify the sending progress.
     * @param url The URL used in a GET request. Null when the method is
     *         HTTP_POST_METHOD.
     * @param pdu The data to be POST. Null when the method is HTTP_GET_METHOD.
     * @param method HTTP_POST_METHOD or HTTP_GET_METHOD.
     * @return A byte array which contains the response data.
     *         If an HTTP error code is returned, an IOException will be thrown.
     * @throws IOException if any error occurred on network interface or
     *         an HTTP error code(&gt;=400) returned from the server.
     */
    protected static byte[] httpConnection(Context context, long token,
            String urlString, byte[] pdu, int method, boolean isProxySet,
            String proxyHost, int proxyPort, int subId) throws IOException {
        if (urlString == null) {
            throw new IllegalArgumentException("URL must not be null.");
        }

        if (Log.isLoggable(LogTag.TRANSACTION, Log.VERBOSE)) {
            Log.v(TAG, "httpConnection: params list");
            Log.v(TAG, "\ttoken\t\t= " + token);
            Log.v(TAG, "\turl\t\t= " + urlString);
            Log.v(TAG, "\tmethod\t\t= "
                    + ((method == HTTP_POST_METHOD) ? "POST"
                            : ((method == HTTP_GET_METHOD) ? "GET" : "UNKNOWN")));
            Log.v(TAG, "\tisProxySet\t= " + isProxySet);
            Log.v(TAG, "\tproxyHost\t= " + proxyHost);
            Log.v(TAG, "\tproxyPort\t= " + proxyPort);
            // TODO Print out binary data more readable.
            //Log.v(TAG, "\tpdu\t\t= " + Arrays.toString(pdu));
        }

        HttpURLConnection connection = null;
        try {
            startAlarmForTimeout(context);
            checkMethod(method);
            Proxy proxy = Proxy.NO_PROXY;
            if (isProxySet) {
                proxy = new Proxy(Proxy.Type.HTTP,
                    new InetSocketAddress(proxyHost, proxyPort));
            }
            final URL url = new URL(urlString);

            // Now get the connection
            connection = (HttpURLConnection) url.openConnection(proxy);
            connection.setDoInput(true);
            connection.setConnectTimeout(HTTP_TIMEOUT);
            connection.setReadTimeout(MmsConfig.getHttpSocketTimeout());

            // ------- COMMON HEADERS ---------
            // Header: Accept
            connection.setRequestProperty(HDR_KEY_ACCEPT, HDR_VALUE_ACCEPT);
            // Header: Accept-Language
            connection.setRequestProperty(HDR_KEY_ACCEPT_LANGUAGE,
                HDR_VALUE_ACCEPT_LANGUAGE);
            // Header: User-Agent
            Log.i(TAG, "HTTP: User-Agent=" + MmsConfig.getUserAgent());
            connection.setRequestProperty(HEADER_USER_AGENT, MmsConfig.getUserAgent());
            // Header: x-wap-profile
            String xWapProfileTagName = MmsConfig.getUaProfTagName();
            String xWapProfileUrl = MmsConfig.getUaProfUrl();
            if (xWapProfileUrl != null) {
                Log.i(TAG, "HTTP: xWapProfileUrl=" + xWapProfileUrl
                    + ";xWapProfileTagName=" + xWapProfileTagName);
                connection.setRequestProperty(xWapProfileTagName, xWapProfileUrl);
            }

            // Add extra headers specified by mms_config.xml's httpparams
            addExtraHeaders(context, connection, subId);

            // Different stuff for GET and POST
            if (HTTP_POST_METHOD == method) {
                if (pdu == null || pdu.length < 1) {
                    Log.e(TAG, "HTTP: empty pdu");
                    throw new IllegalArgumentException("HTTP: empty pdu");
                }
                connection.setDoOutput(true);
                connection.setRequestMethod(METHOD_POST);
                connection.setRequestProperty(HEADER_CONTENT_TYPE,
                    HEADER_VALUE_CONTENT_TYPE_WITHOUT_CHARSET);

                if (Log.isLoggable(LogTag.TRANSACTION, Log.VERBOSE)) {
                    logHttpHeaders(connection.getRequestProperties());
                }
                connection.setFixedLengthStreamingMode(pdu.length);
                // Sending request body
                final OutputStream out = new BufferedOutputStream(connection.getOutputStream());
                out.write(pdu);
                out.flush();
                out.close();
            } else if (HTTP_GET_METHOD == method) {
                if (Log.isLoggable(LogTag.TRANSACTION, Log.VERBOSE)) {
                    logHttpHeaders(connection.getRequestProperties());
                }
                connection.setRequestMethod(METHOD_GET);
            }

            synchronized (HttpUtils.class) {
                mConnection = connection;
            }

            // Get response
            final int responseCode = connection.getResponseCode();
            final String responseMessage = connection.getResponseMessage();
            Log.d(TAG, "HTTP: " + responseCode + " responseMessage " + responseMessage);
            if (Log.isLoggable(LogTag.TRANSACTION, Log.VERBOSE)) {
                logHttpHeaders(connection.getHeaderFields());
            }
            if (responseCode / 100 != 2) {
                throw new IOException("HTTP error: responseCode:"+ responseCode + responseMessage);
            }
            final InputStream in = new BufferedInputStream(connection.getInputStream());
            final ByteArrayOutputStream byteOut = new ByteArrayOutputStream();
            final byte[] buf = new byte[4096];
            int count = 0;
            while ((count = in.read(buf)) > 0) {
                byteOut.write(buf, 0, count);
            }
            in.close();
            final byte[] responseBody = byteOut.toByteArray();
            Log.d(TAG, "HTTP: response size=" + (responseBody != null ? responseBody.length : 0));
            return responseBody;
        } catch (MalformedURLException e) {
            handleHttpConnectionException(e, urlString);
        } catch (ProtocolException e) {
            handleHttpConnectionException(e, urlString);
        } catch (SocketException e) {
            handleHttpConnectionException(e, urlString);
        } catch (IOException e) {
            handleHttpConnectionException(e, urlString);
        } catch (MmsException e) {
            handleHttpConnectionException(e, urlString);
        } finally {
            if (connection != null) {
                connection.disconnect();
            }
            cancelTimeoutAlarm(context);
        }
        return null;
    }

    private static void checkMethod(int method) throws MmsException {
        if (HTTP_POST_METHOD != method && HTTP_GET_METHOD != method) {
            throw new MmsException("Invalid method " + method);
        }
    }

    private static void handleHttpConnectionException(Exception exception, String url)
            throws IOException {
        // Inner exception should be logged to make life easier.
        Log.e(TAG, "Url: " + url + "\n" + exception.getMessage());
        IOException e = new IOException(exception.getMessage());
        e.initCause(exception);
        throw e;
    }

    private static void addExtraHeaders(Context context,
            HttpURLConnection connection, int subId) {
        final String extraHttpParams = MmsConfig.getHttpParams();

        if (!TextUtils.isEmpty(extraHttpParams)) {
            // Parse the parameter list
            String paramList[] = extraHttpParams.split("\\|");
            for (String paramPair : paramList) {
                String splitPair[] = paramPair.split(":", 2);
                if (splitPair.length == 2) {
                    final String name = splitPair[0].trim();
                    final String value = resolveMacro(context, splitPair[1].trim(), subId);
                    if (!TextUtils.isEmpty(name) && !TextUtils.isEmpty(value)) {
                        // Add the header if the param is valid
                       connection.setRequestProperty(name, value);
                    }
                }
            }
        }
    }

    private static String resolveMacro(Context context, String value, int subId) {
        if (TextUtils.isEmpty(value)) {
            return value;
        }
        final Matcher matcher = MACRO_P.matcher(value);
        int nextStart = 0;
        StringBuilder replaced = null;
        while (matcher.find()) {
            if (replaced == null) {
                replaced = new StringBuilder();
            }
            final int matchedStart = matcher.start();
            if (matchedStart > nextStart) {
                replaced.append(value.substring(nextStart, matchedStart));
            }
            final String macro = matcher.group(1);
            final String macroValue = getHttpParamMacro(context, macro, subId);
            if (macroValue != null) {
                replaced.append(macroValue);
            }
            nextStart = matcher.end();
        }
        if (replaced != null && nextStart < value.length()) {
            replaced.append(value.substring(nextStart));
        }
        return replaced == null ? value : replaced.toString();
    }

    /**
     * Return the HTTP param macro value. Example: LINE1 returns the phone number,
     * etc.
     *
     * @param macro
     *            The macro name
     * @param mmsConfig
     *            The carrier configuration values
     * @return The value of the defined macro
    */
    private static String getHttpParamMacro(Context context, final String macro, int subId) {
        if (MACRO_LINE1.equals(macro)) {
            return getSelfNumber(context, subId);
        } else if (MACRO_LINE1NOCOUNTRYCODE.equals(macro)) {
            return getLine1NoCountryCode(context, subId);
        } else if (MACRO_NAI.equals(macro)) {
            return getEncodedNai(context, subId);
        }
        return null;
    }

    private static String getLine1NoCountryCode(Context context, int subId) {
        final TelephonyManager telephonyManager = (TelephonyManager) context
            .getSystemService(Context.TELEPHONY_SERVICE);
        return getNationalNumber(telephonyManager, subId,
            TelephonyManagerWrapper.getLine1Number(telephonyManager, subId));
    }

    public static String getNationalNumber(TelephonyManager telephonyManager,
            int subId, String phoneText) {
        final String country = getSimOrDefaultLocaleCountry(telephonyManager, subId);
        final PhoneNumberUtil phoneNumberUtil = PhoneNumberUtil.getInstance();
        final Phonenumber.PhoneNumber parsed = getParsedNumber(phoneNumberUtil, phoneText, country);
        if (parsed == null) {
            return phoneText;
        }
        return phoneNumberUtil.format(parsed,
            PhoneNumberUtil.PhoneNumberFormat.NATIONAL).replaceAll("\\D", "");
    }

    private static Phonenumber.PhoneNumber getParsedNumber(PhoneNumberUtil phoneNumberUtil,
            String phoneText, String country) {
        try {
            final Phonenumber.PhoneNumber phoneNumber =
                phoneNumberUtil.parse(phoneText, country);
            if (phoneNumberUtil.isValidNumber(phoneNumber)) {
                return phoneNumber;
            } else {
                Log.e(TAG, "getParsedNumber: not a valid phone number"
                + " for country " + country);
                return null;
            }
        } catch (final NumberParseException e) {
            Log.e(TAG, "getParsedNumber: Not able to parse phone number", e);
            return null;
        }
    }

    private static String getSimOrDefaultLocaleCountry(TelephonyManager telephonyManager,
            int subId) {
        String country = getSimCountry(telephonyManager, subId);
        if (TextUtils.isEmpty(country)) {
            country = Locale.getDefault().getCountry();
        }

        return country;
    }

    // Get country/region from SIM ID
    private static String getSimCountry(TelephonyManager telephonyManager, int subId) {
        final String country = TelephonyManagerWrapper.getSimCountryIso(telephonyManager, subId);
        if (TextUtils.isEmpty(country)) {
            return null;
        }
        return country.toUpperCase();
    }

    private static String getEncodedNai(Context context, int subId) {
        String nai = TelephonyManagerWrapper.getNai(context,
            SubscriptionManagerWrapper.getSlotId(subId));
        if (Log.isLoggable(LogTag.TRANSACTION, Log.VERBOSE)) {
            Log.v(TAG, "getNai: nai=" + nai);
        }

        if (!TextUtils.isEmpty(nai)) {
            byte[] encoded = null;
            try {
                encoded = Base64.encode(nai.getBytes("UTF-8"), Base64.NO_WRAP);
            } catch (UnsupportedEncodingException e) {
                encoded = Base64.encode(nai.getBytes(), Base64.NO_WRAP);
            }
            try {
                nai = new String(encoded, "UTF-8");
            } catch (UnsupportedEncodingException e) {
                nai = new String(encoded);
            }
         }
         return nai;
    }

    /**
     * Get the device phone number
     *
	 * @return the phone number text
    */
    private static String getSelfNumber(Context context, int subId) {
        final TelephonyManager telephonyManager = (TelephonyManager) context
            .getSystemService(Context.TELEPHONY_SERVICE);
        return TelephonyManagerWrapper.getLine1Number(telephonyManager, subId);
    }

    private static void logHttpHeaders(Map<String, List<String>> headers) {
        final StringBuilder sb = new StringBuilder();
        if (headers != null) {
            for (Map.Entry<String, List<String>> entry : headers.entrySet()) {
                final String key = entry.getKey();
                final List<String> values = entry.getValue();
                if (values != null) {
                    for (String value : values) {
                        sb.append(key).append('=').append(value).append('\n');
                    }
                }
            }
            Log.v(TAG, "HTTP: headers\n" + sb.toString());
        }
    }

    private static String redactUrlForNonVerbose(String urlString) {
        if (Log.isLoggable(LogTag.TRANSACTION, Log.VERBOSE)) {
            // Don't redact for VERBOSE level logging
            return urlString;
        }
        if (TextUtils.isEmpty(urlString)) {
            return urlString;
        }
        String protocol = "http";
        String host = "";
        try {
            final URL url = new URL(urlString);
            protocol = url.getProtocol();
            host = url.getHost();
        } catch (MalformedURLException e) {
            // Ignore
        }
        // Print "http://host[length]"
        final StringBuilder sb = new StringBuilder();
        sb.append(protocol)
          .append("://")
          .append(host)
          .append("[")
          .append(urlString.length())
          .append("]");
        return sb.toString();
    }
}
