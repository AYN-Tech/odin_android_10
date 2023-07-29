/*
 * Copyright (C) 2019 The Android Open Source Project
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

package com.android.compatibility.common.util;

import java.io.File;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.math.BigInteger;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

/** Contains helper functions and shared constants for crash parsing. */
public class CrashUtils {
    // used to only detect actual addresses instead of nullptr and other unlikely values
    public static final BigInteger MIN_CRASH_ADDR = new BigInteger("8000", 16);
    // Matches the end of a crash
    public static final Pattern sEndofCrashPattern =
            Pattern.compile("DEBUG\\s+?:\\s+?backtrace:");
    public static final String DEVICE_PATH = "/data/local/tmp/CrashParserResults/";
    public static final String LOCK_FILENAME = "lockFile.loc";
    public static final String UPLOAD_REQUEST = "Please upload a result file to stagefright";
    public static final Pattern sUploadRequestPattern =
            Pattern.compile(UPLOAD_REQUEST);
    public static final String NEW_TEST_ALERT = "New test starting with name: ";
    public static final Pattern sNewTestPattern =
            Pattern.compile(NEW_TEST_ALERT + "(\\w+?)\\(.*?\\)");
    public static final String SIGNAL = "signal";
    public static final String NAME = "name";
    public static final String PROCESS = "process";
    public static final String PID = "pid";
    public static final String TID = "tid";
    public static final String FAULT_ADDRESS = "faultaddress";
    // Matches the smallest blob that has the appropriate header and footer
    private static final Pattern sCrashBlobPattern =
            Pattern.compile("DEBUG\\s+?:( [*]{3})+?.*?DEBUG\\s+?:\\s+?backtrace:", Pattern.DOTALL);
    // Matches process id and name line and captures them
    private static final Pattern sPidtidNamePattern =
            Pattern.compile("pid: (\\d+?), tid: (\\d+?), name: ([^\\s]+?\\s+?)*?>>> (.*?) <<<");
    // Matches fault address and signal type line
    private static final Pattern sFaultLinePattern =
            Pattern.compile(
                    "\\w+? \\d+? \\((.*?)\\), code -*?\\d+? \\(.*?\\), fault addr "
                            + "(?:0x(\\p{XDigit}+)|-+)");
    // Matches the abort message line if it contains CHECK_
    private static Pattern sAbortMessageCheckPattern =
            Pattern.compile("(?i)Abort message.*?CHECK_");

    /**
     * returns true if the signal is a segmentation fault or bus error.
     */
    public static boolean isSecuritySignal(JSONObject crash) throws JSONException {
        return crash.getString(SIGNAL).toLowerCase().matches("sig(segv|bus)");
    }

    /**
     * returns the filename of the process.
     * e.g. "/system/bin/mediaserver" returns "mediaserver"
     */
    public static String getProcessFileName(JSONObject crash) throws JSONException {
        return new File(crash.getString(PROCESS)).getName();
    }

    /**
     * Determines if the given input has a {@link com.android.compatibility.common.util.Crash} that
     * should fail an sts test
     *
     * @param processPatterns list of patterns that match applicable process names
     * @param checkMinAddr if the minimum fault address should be respected
     * @param crashes list of crashes to check
     * @return if a crash is serious enough to fail an sts test
     */
    public static boolean securityCrashDetected(
            JSONArray crashes, boolean checkMinAddr, Pattern... processPatterns) {
        return matchSecurityCrashes(crashes, checkMinAddr, processPatterns).length() > 0;
    }

    public static BigInteger getBigInteger(JSONObject source, String name) throws JSONException {
        if (source.isNull(name)) {
            return null;
        }
        String intString = source.getString(name);
        BigInteger value = null;
        try {
            value = new BigInteger(intString, 16);
        } catch (NumberFormatException e) {}
        return value;
    }

    /**
     * Determines which given inputs have a {@link com.android.compatibility.common.util.Crash} that
     * should fail an sts test
     *
     * @param processPatterns list of patterns that match applicable process names
     * @param checkMinAddr if the minimum fault address should be respected
     * @param crashes list of crashes to check
     * @return the list of crashes serious enough to fail an sts test
     */
    public static JSONArray matchSecurityCrashes(
            JSONArray crashes, boolean checkMinAddr, Pattern... processPatterns) {
        JSONArray securityCrashes = new JSONArray();
        for (int i = 0; i < crashes.length(); i++) {
            try {
                JSONObject crash = crashes.getJSONObject(i);
                if (!matchesAny(getProcessFileName(crash), processPatterns)) {
                    continue;
                }
                if (!isSecuritySignal(crash)) {
                    continue;
                }
                BigInteger faultAddress = getBigInteger(crash, FAULT_ADDRESS);
                if (checkMinAddr && faultAddress != null
                        && faultAddress.compareTo(MIN_CRASH_ADDR) < 0) {
                    continue;
                }
                securityCrashes.put(crash);
            } catch (JSONException | NullPointerException e) {}
        }
        return securityCrashes;
    }

    /**
     * returns true if the input matches any of the patterns.
     */
    private static boolean matchesAny(String input, Pattern... patterns) {
        for (Pattern p : patterns) {
            if (p.matcher(input).matches()) {
                return true;
            }
        }
        return false;
    }

    /** Adds all crashes found in the input as JSONObjects to the given JSONArray */
    public static JSONArray addAllCrashes(String input, JSONArray crashes) {
        Matcher crashBlobFinder = sCrashBlobPattern.matcher(input);
        while (crashBlobFinder.find()) {
            String crashStr = crashBlobFinder.group(0);
            int tid = 0;
            int pid = 0;
            BigInteger faultAddress = null;
            String name = null;
            String process = null;
            String signal = null;

            Matcher pidtidNameMatcher = sPidtidNamePattern.matcher(crashStr);
            if (pidtidNameMatcher.find()) {
                try {
                    pid = Integer.parseInt(pidtidNameMatcher.group(1));
                } catch (NumberFormatException e) {}
                try {
                    tid = Integer.parseInt(pidtidNameMatcher.group(2));
                } catch (NumberFormatException e) {}
                name = pidtidNameMatcher.group(3).trim();
                process = pidtidNameMatcher.group(4).trim();
            }

            Matcher faultLineMatcher = sFaultLinePattern.matcher(crashStr);
            if (faultLineMatcher.find()) {
                signal = faultLineMatcher.group(1);
                String faultAddrMatch = faultLineMatcher.group(2);
                if (faultAddrMatch != null) {
                    try {
                        faultAddress = new BigInteger(faultAddrMatch, 16);
                    } catch (NumberFormatException e) {}
                }
            }
            if (!sAbortMessageCheckPattern.matcher(crashStr).find()) {
                try {
                    JSONObject crash = new JSONObject();
                    crash.put(PID, pid);
                    crash.put(TID, tid);
                    crash.put(NAME, name);
                    crash.put(PROCESS, process);
                    crash.put(FAULT_ADDRESS,
                            faultAddress == null ? null : faultAddress.toString(16));
                    crash.put(SIGNAL, signal);
                    crashes.put(crash);
                } catch (JSONException e) {}
            }
        }
        return crashes;
    }
}
