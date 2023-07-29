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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.AbstractMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/** Unit tests for {@link TestResultHistory} */
@RunWith(JUnit4.class)
public class TestResultHistoryTest {

    private static final String TEST_NAME = "Test";
    private static final long START_TIME = 1000000000011L;
    private static final long END_TIME = 1000000000012L;
    private static final String HEADER_XML =
            "<?xml version='1.0' encoding='UTF-8' standalone='yes' ?>";
    private static final String RUN_HISTORIES_XML =
            HEADER_XML
                    + "\r\n"
                    + "<RunHistory>\r\n"
                    + "  <Run start=\"1000000000011\" end=\"1000000000012\" />\r\n"
                    + "</RunHistory>";

    private TestResultHistory mTestResultHistory;
    private Set<Map.Entry> mDurations;

    @Before
    public void setUp() throws Exception {
        mDurations = new HashSet<>();
    }

    @Test
    public void testSerialize_null() throws Exception {
        try {
            TestResultHistory.serialize(null, null);
            fail("Expected IllegalArgumentException when serializing an empty test result history");
        } catch (IllegalArgumentException e) {
            // Expected
        }
    }

    @Test
    public void testSerialize_full() throws Exception {
        mDurations.add(new AbstractMap.SimpleEntry<>(START_TIME, END_TIME));
        mTestResultHistory = new TestResultHistory(TEST_NAME, mDurations);
        assertEquals(RUN_HISTORIES_XML, TestResultHistory.serialize(mTestResultHistory, TEST_NAME));
    }
}
