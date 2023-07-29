/*
 * Copyright (C) 2016 The Android Open Source Project
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

package android.security.cts;

import com.android.compatibility.common.util.CrashUtils;
import com.android.ddmlib.NullOutputReceiver;
import com.android.tradefed.device.CollectingOutputReceiver;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.NativeDevice;
import com.android.tradefed.log.LogUtil.CLog;

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.concurrent.TimeoutException;
import java.util.List;
import java.util.regex.Pattern;
import java.util.concurrent.TimeUnit;
import java.util.Scanner;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.regex.Pattern;
import java.lang.Thread;
import static org.junit.Assert.*;
import junit.framework.Assert;

public class AdbUtils {

    /** Runs a commandline on the specified device
     *
     * @param command the command to be ran
     * @param device device for the command to be ran on
     * @return the console output from running the command
     */
    public static String runCommandLine(String command, ITestDevice device) throws Exception {
        if ("reboot".equals(command)) {
            throw new IllegalArgumentException(
                    "You called a forbidden command! Please fix your tests.");
        }
        return device.executeShellCommand(command);
    }

    /**
     * Pushes and runs a binary to the selected device
     *
     * @param pocName name of the poc binary
     * @param device device to be ran on
     * @return the console output from the binary
     */
    public static String runPoc(String pocName, ITestDevice device) throws Exception {
        device.executeShellCommand("chmod +x /data/local/tmp/" + pocName);
        return device.executeShellCommand("/data/local/tmp/" + pocName);
    }

    /**
     * Pushes and runs a binary to the selected device
     *
     * @param pocName name of the poc binary
     * @param device device to be ran on
     * @param timeout time to wait for output in seconds
     * @return the console output from the binary
     */
    public static String runPoc(String pocName, ITestDevice device, int timeout) throws Exception {
        return runPoc(pocName, device, timeout, null);
    }

    /**
     * Pushes and runs a binary to the selected device
     *
     * @param pocName name of the poc binary
     * @param device device to be ran on
     * @param timeout time to wait for output in seconds
     * @param arguments the input arguments for the poc
     * @return the console output from the binary
     */
    public static String runPoc(String pocName, ITestDevice device, int timeout, String arguments)
            throws Exception {
        device.executeShellCommand("chmod +x /data/local/tmp/" + pocName);
        CollectingOutputReceiver receiver = new CollectingOutputReceiver();
        if (arguments != null) {
            device.executeShellCommand("/data/local/tmp/" + pocName + " " + arguments, receiver,
                    timeout, TimeUnit.SECONDS, 0);
        } else {
            device.executeShellCommand("/data/local/tmp/" + pocName, receiver, timeout,
                    TimeUnit.SECONDS, 0);
        }
        String output = receiver.getOutput();
        return output;
    }

    /**
     * Pushes and runs a binary to the selected device and ignores any of its output.
     *
     * @param pocName name of the poc binary
     * @param device device to be ran on
     * @param timeout time to wait for output in seconds
     */
    public static void runPocNoOutput(String pocName, ITestDevice device, int timeout)
            throws Exception {
        runPocNoOutput(pocName, device, timeout, null);
    }

    /**
     * Pushes and runs a binary with arguments to the selected device and
     * ignores any of its output.
     *
     * @param pocName name of the poc binary
     * @param device device to be ran on
     * @param timeout time to wait for output in seconds
     * @param arguments input arguments for the poc
     */
    public static void runPocNoOutput(String pocName, ITestDevice device, int timeout,
            String arguments) throws Exception {
        device.executeShellCommand("chmod +x /data/local/tmp/" + pocName);
        NullOutputReceiver receiver = new NullOutputReceiver();
        if (arguments != null) {
            device.executeShellCommand("/data/local/tmp/" + pocName + " " + arguments, receiver,
                    timeout, TimeUnit.SECONDS, 0);
        } else {
            device.executeShellCommand("/data/local/tmp/" + pocName, receiver, timeout,
                    TimeUnit.SECONDS, 0);
        }
    }

    /**
     * Enables malloc debug on a given process.
     *
     * @param processName the name of the process to run with libc malloc debug
     * @param device the device to use
     * @return true if enabling malloc debug succeeded
     */
    public static boolean enableLibcMallocDebug(String processName, ITestDevice device) throws Exception {
        device.executeShellCommand("setprop libc.debug.malloc.program " + processName);
        device.executeShellCommand("setprop libc.debug.malloc.options \"backtrace guard\"");
        /**
         * The pidof command is being avoided because it does not exist on versions before M, and
         * it behaves differently between M and N.
         * Also considered was the ps -AoPID,CMDLINE command, but ps does not support options on
         * versions before O.
         * The [^]] prefix is being used for the grep command to avoid the case where the output of
         * ps includes the grep command itself.
         */
        String cmdOut = device.executeShellCommand("ps -A | grep '[^]]" + processName + "'");
        /**
         * .hasNextInt() checks if the next token can be parsed as an integer, not if any remaining
         * token is an integer.
         * Example command: $ ps | fgrep mediaserver
         * Out: media     269   1     77016  24416 binder_thr 00f35142ec S /system/bin/mediaserver
         * The second field of the output is the PID, which is needed to restart the process.
         */
        Scanner s = new Scanner(cmdOut).useDelimiter("\\D+");
        if(!s.hasNextInt()) {
            CLog.w("Could not find pid for process: " + processName);
            return false;
        }

        String result = device.executeShellCommand("kill -9 " + s.nextInt());
        if(!result.equals("")) {
            CLog.w("Could not restart process: " + processName);
            return false;
        }

        TimeUnit.SECONDS.sleep(1);
        return true;
    }

    /**
     * Pushes and installs an apk to the selected device
     *
     * @param pathToApk a string path to apk from the /res folder
     * @param device device to be ran on
     * @return the output from attempting to install the apk
     */
    public static String installApk(String pathToApk, ITestDevice device) throws Exception {

        String fullResourceName = pathToApk;
        File apkFile = File.createTempFile("apkFile", ".apk");
        try {
            apkFile = extractResource(fullResourceName, apkFile);
            return device.installPackage(apkFile, true);
        } finally {
            apkFile.delete();
        }
    }

    /**
     * Extracts a resource and pushes it to the device
     *
     * @param fullResourceName a string path to resource from the res folder
     * @param deviceFilePath the remote destination absolute file path
     * @param device device to be ran on
     */
    public static void pushResource(String fullResourceName, String deviceFilePath,
                                    ITestDevice device) throws Exception {
        File resFile = File.createTempFile("CTSResource", "");
        try {
            resFile = extractResource(fullResourceName, resFile);
            device.pushFile(resFile, deviceFilePath);
        } finally {
            resFile.delete();
        }
    }

   /**
     * Extracts the binary data from a resource and writes it to a temp file
     */
    private static File extractResource(String fullResourceName, File file) throws Exception {
        try (InputStream in = AdbUtils.class.getResourceAsStream(fullResourceName);
            OutputStream out = new BufferedOutputStream(new FileOutputStream(file))) {
            if (in == null) {
                throw new IllegalArgumentException("Resource not found: " + fullResourceName);
            }
            byte[] buf = new byte[65536];
            int chunkSize;
            while ((chunkSize = in.read(buf)) != -1) {
                out.write(buf, 0, chunkSize);
            }
            return file;
        }

    }
    /**
     * Utility function to help check the exit code of a shell command
     */
    public static int runCommandGetExitCode(String cmd, ITestDevice device) throws Exception {
        long time = System.currentTimeMillis();
        String exitStatus = runCommandLine(
                "(" + cmd + ") > /dev/null 2>&1; echo $?", device).trim();
        time = System.currentTimeMillis() - time;
        try {
            return Integer.parseInt(exitStatus);
        } catch (NumberFormatException e) {
            throw new IllegalArgumentException(String.format(
                    "Could not get the exit status (%s) for '%s' (%d ms).",
                    exitStatus, cmd, time));
        }
    }

    /**
     * Pushes and runs a binary to the selected device and checks exit code
     * Return code 113 is used to indicate the vulnerability
     *
     * @param pocName a string path to poc from the /res folder
     * @param device device to be ran on
     * @param timeout time to wait for output in seconds
     */
    @Deprecated
    public static boolean runPocCheckExitCode(String pocName, ITestDevice device,
                                              int timeout) throws Exception {

       //Refer to go/asdl-sts-guide Test section for knowing the significance of 113 code
       return runPocGetExitStatus(pocName, device, timeout) == 113;
    }

    /**
     * Pushes and runs a binary to the device and returns the exit status.
     * @param pocName a string path to poc from the /res folder
     * @param device device to be ran on
     * @param timeout time to wait for output in seconds

     */
    public static int runPocGetExitStatus(String pocName, ITestDevice device, int timeout)
            throws Exception {
        device.executeShellCommand("chmod +x /data/local/tmp/" + pocName);
        CollectingOutputReceiver receiver = new CollectingOutputReceiver();
        String cmd = "/data/local/tmp/" + pocName + " > /dev/null 2>&1; echo $?";
        long time = System.currentTimeMillis();
        device.executeShellCommand(cmd, receiver, timeout, TimeUnit.SECONDS, 0);
        time = System.currentTimeMillis() - time;
        String exitStatus = receiver.getOutput().trim();
        try {
            return Integer.parseInt(exitStatus);
        } catch (NumberFormatException e) {
            throw new IllegalArgumentException(String.format(
                    "Could not get the exit status (%s) for '%s' (%d ms).",
                    exitStatus, cmd, time));
        }
    }

    /**
     * Pushes and runs a binary and asserts that the exit status isn't 113: vulnerable.
     * @param pocName a string path to poc from the /res folder
     * @param device device to be ran on
     * @param timeout time to wait for output in seconds
     */
    public static void runPocAssertExitStatusNotVulnerable(
            String pocName, ITestDevice device, int timeout) throws Exception {
        assertTrue("PoC returned exit status 113: vulnerable",
                runPocGetExitStatus(pocName, device, timeout) != 113);
    }

    public static int runProxyAutoConfig(String pacName, ITestDevice device) throws Exception {
        runCommandLine("chmod +x /data/local/tmp/pacrunner", device);
        String targetPath = "/data/local/tmp/" + pacName + ".pac";
        AdbUtils.pushResource("/" + pacName + ".pac", targetPath, device);
        int code = runCommandGetExitCode("/data/local/tmp/pacrunner " + targetPath, device);
        runCommandLine("rm " + targetPath, device);
        return code;
    }

    /**
     * Runs the poc binary and asserts that there are no security crashes that match the expected
     * process pattern.
     * @param pocName a string path to poc from the /res folder
     * @param device device to be ran on
     * @param processPatternStrings a Pattern string to match the crash tombstone process
     */
    public static void runPocAssertNoCrashes(String pocName, ITestDevice device,
            String... processPatternStrings) throws Exception {
        AdbUtils.runCommandLine("logcat -c", device);
        AdbUtils.runPocNoOutput(pocName, device, SecurityTestCase.TIMEOUT_NONDETERMINISTIC);
        assertNoCrashes(device, processPatternStrings);
    }

    /**
     * Dumps logcat and asserts that there are no security crashes that match the expected process.
     * By default, checks min crash addresses
     * pattern. Ensure that adb logcat -c is called beforehand.
     * @param device device to be ran on
     * @param processPatternStrings a Pattern string to match the crash tombstone process
     */
    public static void assertNoCrashes(ITestDevice device, String... processPatternStrings)
            throws Exception {
        assertNoCrashes(device, true, processPatternStrings);
    }

    /**
     * Dumps logcat and asserts that there are no security crashes that match the expected process
     * pattern. Ensure that adb logcat -c is called beforehand.
     * @param device device to be ran on
     * @param checkMinAddress if the minimum fault address should be respected
     * @param processPatternStrings a Pattern string to match the crash tombstone process
     */
    public static void assertNoCrashes(ITestDevice device, boolean checkMinAddress,
            String... processPatternStrings) throws Exception {
        String logcat = AdbUtils.runCommandLine("logcat -d *:S DEBUG:V", device);

        Pattern[] processPatterns = new Pattern[processPatternStrings.length];
        for (int i = 0; i < processPatternStrings.length; i++) {
            processPatterns[i] = Pattern.compile(processPatternStrings[i]);
        }
        JSONArray crashes = CrashUtils.addAllCrashes(logcat, new JSONArray());
        JSONArray securityCrashes =
                CrashUtils.matchSecurityCrashes(crashes, checkMinAddress, processPatterns);

        if (securityCrashes.length() == 0) {
            return; // no security crashes detected
        }

        StringBuilder error = new StringBuilder();
        error.append("Security crash detected:\n");
        error.append("Process patterns:");
        for (String pattern : processPatternStrings) {
            error.append(String.format(" '%s'", pattern));
        }
        error.append("\nCrashes:\n");
        for (int i = 0; i < crashes.length(); i++) {
            try {
                JSONObject crash = crashes.getJSONObject(i);
                error.append(String.format("%s\n", crash));
            } catch (JSONException e) {}
        }
        fail(error.toString());
     }

    /**
     * Executes a given poc within a given timeout. Returns error if the
     * given poc doesnt complete its execution within timeout. It also deletes
     * the list of files provided.
     *
     * @param runner the thread which will be run
     * @param timeout the timeout within which the thread's execution should
     *        complete
     * @param device device to be ran on
     * @param inputFiles list of files to be deleted
     */
    public static void runWithTimeoutDeleteFiles(Runnable runner, int timeout, ITestDevice device,
            String[] inputFiles) throws Exception {
        Thread t = new Thread(runner);
        t.start();
        boolean test_failed = false;
        try {
            t.join(timeout);
        } catch (InterruptedException e) {
            test_failed = true;
        } finally {
            if (inputFiles != null) {
                for (String tempFile : inputFiles) {
                    AdbUtils.runCommandLine("rm /data/local/tmp/" + tempFile, device);
                }
            }
            if (test_failed) {
                fail("PoC was interrupted");
            }
        }
        if (t.isAlive()) {
            Assert.fail("PoC not completed within timeout of " + timeout + " ms");
        }
    }
}
