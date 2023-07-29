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

package com.android.helpers;

import android.support.test.uiautomator.UiDevice;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * FreeMemHelper is a helper to parse the free memory based on total memory available from
 * proc/meminfo, private dirty and private clean information of the cached processes from dumpsys
 * meminfo.
 *
 * Example Usage:
 * freeMemHelper.startCollecting();
 * freeMemHelper.getMetrics();
 * freeMemHelper.stopCollecting();
 */
public class FreeMemHelper implements ICollectorHelper<Long> {
    private static final String TAG = FreeMemHelper.class.getSimpleName();
    private static final String SEPARATOR = "\\s+";
    private static final String DUMPSYS_MEMIFNO = "dumpsys meminfo";
    private static final String PROC_MEMINFO = "cat /proc/meminfo";
    private static final String LINE_SEPARATOR = "\\n";
    private static final String MEM_AVAILABLE_PATTERN = "^MemAvailable.*";
    private static final Pattern CACHE_PROC_START_PATTERN = Pattern.compile(".*: Cached$");
    private static final Pattern PID_PATTERN = Pattern.compile("^.*pid(?<processid> [0-9]*).*$");
    private static final String DUMPSYS_PROCESS = "dumpsys meminfo %s";
    private static final String MEM_TOTAL = "^\\s+TOTAL\\s+.*";
    private static final String PROCESS_ID = "processid";
    public static final String MEM_AVAILABLE_CACHE_PROC_DIRTY = "MemAvailable_CacheProcDirty_bytes";
    public static final String PROC_MEMINFO_MEM_AVAILABLE= "proc_meminfo_memavailable_bytes";
    public static final String DUMPSYS_CACHED_PROC_MEMORY= "dumpsys_cached_procs_memory_bytes";

    private UiDevice mUiDevice;

    @Override
    public boolean startCollecting() {
        mUiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        return true;
    }

    @Override
    public boolean stopCollecting() {
        return true;
    }

    @Override
    public Map<String, Long> getMetrics() {
        String memInfo;
        try {
            memInfo = mUiDevice.executeShellCommand(PROC_MEMINFO);
        } catch (IOException ioe) {
            Log.e(TAG, "Failed to read " + PROC_MEMINFO + ".", ioe);
            return null;
        }

        Pattern memAvailablePattern = Pattern.compile(MEM_AVAILABLE_PATTERN, Pattern.MULTILINE);
        Matcher memAvailableMatcher = memAvailablePattern.matcher(memInfo);

        String[] memAvailable = null;
        if (memAvailableMatcher.find()) {
            memAvailable = memAvailableMatcher.group(0).split(SEPARATOR);
        }

        if (memAvailable == null) {
            Log.e(TAG, "MemAvailable is null.");
            return null;
        }
        Map<String, Long> results = new HashMap<>();
        long memAvailableProc = Long.parseLong(memAvailable[1]);
        results.put(PROC_MEMINFO_MEM_AVAILABLE, (memAvailableProc * 1024));

        long cacheProcDirty = memAvailableProc;
        byte[] dumpsysMemInfoBytes = MetricUtility.executeCommandBlocking(DUMPSYS_MEMIFNO,
                InstrumentationRegistry.getInstrumentation());
        List<String> cachedProcList = getCachedProcesses(dumpsysMemInfoBytes);
        Long cachedProcMemory = 0L;

        for (String process : cachedProcList) {
            Log.i(TAG, "Cached Process" + process);
            Matcher match;
            if (((match = matches(PID_PATTERN, process))) != null) {
                String processId = match.group(PROCESS_ID);
                String processDumpSysMemInfo = String.format(DUMPSYS_PROCESS, processId);
                String processInfoStr;
                Log.i(TAG, "Process Id of the cached process" + processId);
                try {
                    processInfoStr = mUiDevice.executeShellCommand(processDumpSysMemInfo);
                } catch (IOException ioe) {
                    Log.e(TAG, "Failed to get " + processDumpSysMemInfo + ".", ioe);
                    return null;
                }

                Pattern memTotalPattern = Pattern.compile(MEM_TOTAL, Pattern.MULTILINE);
                Matcher memTotalMatcher = memTotalPattern.matcher(processInfoStr);

                String[] processInfo = null;
                if (memTotalMatcher.find()) {
                    processInfo = memTotalMatcher.group(0).split(LINE_SEPARATOR);
                }
                if (processInfo != null && processInfo.length > 0) {
                    String[] procDetails = processInfo[0].trim().split(SEPARATOR);
                    int privateDirty = Integer.parseInt(procDetails[2].trim());
                    int privateClean = Integer.parseInt(procDetails[3].trim());
                    cachedProcMemory = cachedProcMemory + privateDirty + privateClean;
                    cacheProcDirty = cacheProcDirty + privateDirty + privateClean;
                    Log.i(TAG, "Cached process: " + process + " Private Dirty: "
                            + (privateDirty * 1024) + " Private Clean: " + (privateClean * 1024));
                }
            }
        }

        // Sum of all the cached process memory.
        results.put(DUMPSYS_CACHED_PROC_MEMORY, (cachedProcMemory * 1024));

        // Mem available cache proc dirty (memavailable + cachedProcMemory)
        results.put(MEM_AVAILABLE_CACHE_PROC_DIRTY, (cacheProcDirty * 1024));
        return results;
    }

    /**
     * Checks whether {@code line} matches the given {@link Pattern}.
     *
     * @return The resulting {@link Matcher} obtained by matching the {@code line} against {@code
     * pattern}, or null if the {@code line} does not match.
     */
    private static Matcher matches(Pattern pattern, String line) {
        Matcher ret = pattern.matcher(line);
        return ret.matches() ? ret : null;
    }

    /**
     * Get cached process information from dumpsys meminfo.
     *
     * @param dumpsysMemInfoBytes
     * @return list of cached processes.
     */
    List<String> getCachedProcesses(byte[] dumpsysMemInfoBytes) {
        List<String> cachedProcessList = new ArrayList<String>();
        InputStream inputStream = new ByteArrayInputStream(dumpsysMemInfoBytes);
        boolean isCacheProcSection = false;
        try (BufferedReader bfReader = new BufferedReader(new InputStreamReader(inputStream))) {
            String currLine = null;
            while ((currLine = bfReader.readLine()) != null) {
                Log.i(TAG, currLine);
                Matcher match;
                if (!isCacheProcSection
                        && ((match = matches(CACHE_PROC_START_PATTERN, currLine))) == null) {
                    // Continue untill the start of cache proc section.
                    continue;
                } else {
                    if (isCacheProcSection) {
                        // If empty we encountered the end of cached process logging.
                        if (!currLine.isEmpty()) {
                            cachedProcessList.add(currLine.trim());
                        } else {
                            break;
                        }
                    }
                    isCacheProcSection = true;
                }
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
        return cachedProcessList;
    }
}
