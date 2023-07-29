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

package android.checkpoint.cts;

import com.android.compatibility.common.util.ApiLevelUtil;
import com.android.compatibility.common.util.CtsDownstreamingTest;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.testtype.DeviceTestCase;
import junit.framework.Assert;


/**
 * Test to validate that the checkpoint failures in b/138952436 are properly patched
 */
public class CheckpointHostTest extends DeviceTestCase {
    private static final String TAG = "CheckpointHostTest";

    @CtsDownstreamingTest
    public void testLogEntries() throws Exception {
        // This test is build also as a part of GTS, which runs also on older releases.
        if (ApiLevelUtil.isBefore(getDevice(), "Q")) return;

        // Clear buffer to make it easier to find new logs
        getDevice().executeShellCommand("logcat --clear");

        // reboot device
        getDevice().rebootUntilOnline();
        waitForBootCompleted();

        // wait for logs to post
        Thread.sleep(10000);

        final String amLog = getDevice().executeShellCommand("logcat -d -s ActivityManager");
        int counterNameIndex = amLog.indexOf("ActivityManager: About to commit checkpoint");
        Assert.assertTrue("did not find commit checkpoint in boot logs", counterNameIndex != -1);

        final String checkpointLog = getDevice().executeShellCommand("logcat -d -s Checkpoint");
        counterNameIndex = checkpointLog.indexOf(
            "Checkpoint: cp_prepareCheckpoint called");
        Assert.assertTrue("did not find prepare checkpoint in boot logs", counterNameIndex != -1);
    }

    private boolean isBootCompleted() throws Exception {
        return "1".equals(getDevice().executeShellCommand("getprop sys.boot_completed").trim());
    }

    private void waitForBootCompleted() throws Exception {
        for (int i = 0; i < 45; i++) {
            if (isBootCompleted()) {
                return;
            }
            Thread.sleep(1000);
        }
        throw new AssertionError("System failed to become ready!");
    }
}
