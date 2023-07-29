/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */

package android.security.cts;

import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import android.platform.test.annotations.SecurityTest;
import java.util.regex.Pattern;

@SecurityTest
public class TestMedia extends SecurityTestCase {

    final static int TIMEOUT_SEC = 9 * 60;
    final static String RESOURCE_ROOT = "/";
    final static String TMP_FILE_PATH = "/data/local/tmp/";

    /****************************************************************
     * To prevent merge conflicts, add tests for N below this comment,
     * before any existing test methods
     ****************************************************************/


    /****************************************************************
     * To prevent merge conflicts, add tests for O below this comment,
     * before any existing test methods
     ****************************************************************/


    /****************************************************************
     * To prevent merge conflicts, add tests for P below this comment,
     * before any existing test methods
     ****************************************************************/

    /**
     * Pushes input files, runs the PoC and checks for crash and hang
     *
     * @param binaryName name of the binary
     * @param inputFiles files required as input
     * @param arguments arguments for running the binary
     * @param device device to be run on
     * @param errPattern error patterns to be checked for
     */
    public static void runMediaTest(String binaryName,
            String inputFiles[], String arguments, ITestDevice device,
            String processPatternStrings[]) throws Exception {
        if (inputFiles != null) {
            for (String tempFile : inputFiles) {
                AdbUtils.pushResource(RESOURCE_ROOT + tempFile,
                        TMP_FILE_PATH + tempFile, device);
            }
        }
        AdbUtils.runCommandLine("logcat -c", device);
        AdbUtils.runWithTimeoutDeleteFiles(new Runnable() {
            @Override
            public void run() {
                try {
                    AdbUtils.runPocNoOutput(binaryName, device,
                            TIMEOUT_SEC + 30, arguments);
                } catch (Exception e) {
                    CLog.w("Exception: " + e.getMessage());
                }
            }
        }, TIMEOUT_SEC * 1000, device, inputFiles);

        AdbUtils.assertNoCrashes(device, binaryName);
        if (processPatternStrings != null) {
            AdbUtils.assertNoCrashes(device, processPatternStrings);
        }
    }
}
