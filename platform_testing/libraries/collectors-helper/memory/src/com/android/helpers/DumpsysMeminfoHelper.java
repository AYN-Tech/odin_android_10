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
 * limitations under the License
 */

package com.android.helpers;

import android.support.test.uiautomator.UiDevice;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
import java.util.stream.Collectors;
import java.util.stream.Stream;

/**
 * This is a collector helper to use adb "dumpsys meminfo -a" command to get important memory
 * metrics like PSS, Shared Dirty, Private Dirty, e.t.c. for the specified packages
 */
public class DumpsysMeminfoHelper implements ICollectorHelper<Long> {

    private static final String TAG = DumpsysMeminfoHelper.class.getSimpleName();

    private static final String SEPARATOR = "\\s+";
    private static final String LINE_SEPARATOR = "\\n";

    private static final String DUMPSYS_MEMINFO_CMD = "dumpsys meminfo -a %s";
    private static final String PIDOF_CMD = "pidof %s";

    private static final String METRIC_SOURCE = "dumpsys";
    private static final String METRIC_UNIT = "kb";

    // Prefixes of the lines in the output of "dumpsys meminfo -a" command that will be parsed
    private static final String NATIVE_HEAP_PREFIX = "Native Heap";
    private static final String DALVIK_HEAP_PREFIX = "Dalvik Heap";
    private static final String TOTAL_PREFIX = "TOTAL";

    // The metric names corresponding to the columns in the output of "dumpsys meminfo -a" command
    private static final String PSS_TOTAL = "pss_total";
    private static final String SHARED_DIRTY = "shared_dirty";
    private static final String PRIVATE_DIRTY = "private_dirty";
    private static final String HEAP_SIZE = "heap_size";
    private static final String HEAP_ALLOC = "heap_alloc";
    private static final String PSS = "pss";

    // Mapping from prefixes of lines to metric category names
    private static final Map<String, String> CATEGORIES =
            Stream.of(
                    new String[][] {
                            {NATIVE_HEAP_PREFIX, "native"},
                            {DALVIK_HEAP_PREFIX, "dalvik"},
                            {TOTAL_PREFIX, "total"},
                    }).collect(Collectors.toMap(data -> data[0], data -> data[1]));

    // Mapping from metric keys to its column index (exclude prefix) in dumpsys meminfo output.
    // The index might change across different Android releases
    private static final Map<String, Integer> METRIC_POSITIONS =
            Stream.of(
                    new Object[][] {
                            {PSS_TOTAL, 0},
                            {SHARED_DIRTY, 2},
                            {PRIVATE_DIRTY, 3},
                            {HEAP_SIZE, 7},
                            {HEAP_ALLOC, 8},
                    })
            .collect(Collectors.toMap(data -> (String) data[0], data -> (Integer) data[1]));

    private String[] mProcessNames = {};
    private UiDevice mUiDevice;

    public void setUp(String... processNames) {
        if (processNames == null) {
            return;
        }
        mProcessNames = processNames;
    }

    @Override
    public boolean startCollecting() {
        mUiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        return true;
    }

    @Override
    public Map<String, Long> getMetrics() {
        Map<String, Long> metrics = new HashMap<>();
        for (String processName : mProcessNames) {
            String rawOutput = getRawDumpsysMeminfo(processName);
            metrics.putAll(parseMetrics(processName, rawOutput));
        }
        return metrics;
    }

    @Override
    public boolean stopCollecting() {
        return true;
    }

    private String getRawDumpsysMeminfo(String processName) {
        if (isEmpty(processName)) {
            return "";
        }
        try {
            String pidStr = mUiDevice.executeShellCommand(String.format(PIDOF_CMD, processName));
            if (isEmpty(pidStr)) {
                return "";
            }
            return mUiDevice.executeShellCommand(String.format(DUMPSYS_MEMINFO_CMD, pidStr));
        } catch (IOException e) {
            Log.e(TAG, String.format("Failed to execute command. %s", e));
            return "";
        }
    }

    private Map<String, Long> parseMetrics(String processName, String rawOutput) {
        String[] lines = rawOutput.split(LINE_SEPARATOR);
        Map<String, Long> metrics = new HashMap<>();
        for (String line : lines) {
            String[] tokens = line.trim().split(SEPARATOR);
            if (tokens.length < 2) {
                continue;
            }
            String firstToken = tokens[0];
            String firstTwoTokens = String.join(" ", tokens[0], tokens[1]);
            if (firstTwoTokens.equals(NATIVE_HEAP_PREFIX)
                    || firstTwoTokens.equals(DALVIK_HEAP_PREFIX)) {
                if (tokens.length < 11) {
                    continue;
                }
                int offset = 2;
                for (Map.Entry<String, Integer> metric : METRIC_POSITIONS.entrySet()) {
                    metrics.put(
                            MetricUtility.constructKey(
                                    METRIC_SOURCE,
                                    CATEGORIES.get(firstTwoTokens),
                                    metric.getKey(),
                                    METRIC_UNIT,
                                    processName),
                            Long.parseLong(tokens[offset + metric.getValue()]));
                }
            } else if (firstToken.equals(TOTAL_PREFIX)) {
                int offset = 1;
                metrics.put(
                        MetricUtility.constructKey(
                                METRIC_SOURCE,
                                CATEGORIES.get(firstToken),
                                PSS_TOTAL,
                                METRIC_UNIT,
                                processName),
                        Long.parseLong(tokens[offset + METRIC_POSITIONS.get(PSS_TOTAL)]));
            }
        }
        return metrics;
    }

    private boolean isEmpty(String input) {
        return input == null || input.isEmpty();
    }
}
