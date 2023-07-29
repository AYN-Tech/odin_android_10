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
package android.platform.test.longevity;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import android.os.Bundle;
import android.platform.test.longevity.proto.Configuration.Scenario;
import android.platform.test.longevity.proto.Configuration.Scenario.ExtraArg;
import androidx.test.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runner.notification.Failure;
import org.junit.runner.notification.RunNotifier;
import org.junit.runners.JUnit4;
import org.junit.runners.model.InitializationError;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.exceptions.base.MockitoAssertionError;

import java.util.HashSet;
import java.util.List;

/** Unit tests for the {@link ScenarioRunner} runner. */
@RunWith(JUnit4.class)
public class ScenarioRunnerTest {

    @Mock private RunNotifier mRunNotifier;

    private static final String ASSERTION_FAILURE_MESSAGE = "Test assertion failed";

    public static class ArgumentTest {
        public static final String TEST_ARG = "test-arg-test-only";
        public static final String TEST_ARG_DEFAULT = "default";
        public static final String TEST_ARG_OVERRIDE = "not default";

        @Before
        public void setUp() {
            // The actual argument testing happens here as this is where instrumentation args are
            // parsed in the CUJs.
            String argValue =
                    InstrumentationRegistry.getArguments().getString(TEST_ARG, TEST_ARG_DEFAULT);
            Assert.assertEquals(ASSERTION_FAILURE_MESSAGE, argValue, TEST_ARG_OVERRIDE);
        }

        @Test
        public void dummyTest() {
            // Does nothing; always passes.
        }
    }

    // Holds the state of the instrumentation args before each test for restoring after, as one test
    // might affect the state of another otherwise.
    // TODO(b/124239142): Avoid manipulating the instrumentation args here.
    private Bundle mArgumentsBeforeTest;

    @Before
    public void setUpSuite() throws InitializationError {
        initMocks(this);
        mArgumentsBeforeTest = InstrumentationRegistry.getArguments();
    }

    @After
    public void restoreSuite() {
        InstrumentationRegistry.registerInstance(
                InstrumentationRegistry.getInstrumentation(), mArgumentsBeforeTest);
    }

    /** Test that the "extras" in a scenario is properly registered before the test. */
    @Test
    public void testExtraArgs_registeredBeforeTest() throws Throwable {
        Scenario testScenario =
                Scenario.newBuilder()
                        .setIndex(1)
                        .setJourney(ArgumentTest.class.getName())
                        .addExtras(
                                ExtraArg.newBuilder()
                                        .setKey(ArgumentTest.TEST_ARG)
                                        .setValue(ArgumentTest.TEST_ARG_OVERRIDE))
                        .build();
        ScenarioRunner runner = spy(new ScenarioRunner(ArgumentTest.class, testScenario));
        runner.run(mRunNotifier);
        verifyForAssertionFailures(mRunNotifier);
    }

    /** Test that the "extras" in a scenario is properly un-registered after the test. */
    @Test
    public void testExtraArgs_unregisteredAfterTest() throws Throwable {
        Bundle argsBeforeTest = InstrumentationRegistry.getArguments();
        Scenario testScenario =
                Scenario.newBuilder()
                        .setIndex(1)
                        .setJourney(ArgumentTest.class.getName())
                        .addExtras(
                                ExtraArg.newBuilder()
                                        .setKey(ArgumentTest.TEST_ARG)
                                        .setValue(ArgumentTest.TEST_ARG_OVERRIDE))
                        .build();
        ScenarioRunner runner = spy(new ScenarioRunner(ArgumentTest.class, testScenario));
        runner.run(mRunNotifier);
        Bundle argsAfterTest = InstrumentationRegistry.getArguments();
        Assert.assertTrue(bundlesContainSameStringKeyValuePairs(argsBeforeTest, argsAfterTest));
    }

    /**
     * Verify that no test failure is fired because of an assertion failure in the stubbed methods.
     * If the verification fails, check whether it's due the injected assertions failing. If yes,
     * throw that exception out; otherwise, throw the first exception.
     */
    private void verifyForAssertionFailures(final RunNotifier notifier) throws Throwable {
        try {
            verify(notifier, never()).fireTestFailure(any());
        } catch (MockitoAssertionError e) {
            ArgumentCaptor<Failure> failureCaptor = ArgumentCaptor.forClass(Failure.class);
            verify(notifier, atLeastOnce()).fireTestFailure(failureCaptor.capture());
            List<Failure> failures = failureCaptor.getAllValues();
            // Go through the failures, look for an known failure case from the above exceptions
            // and throw the exception in the first one out if any.
            for (Failure failure : failures) {
                if (failure.getException().getMessage().contains(ASSERTION_FAILURE_MESSAGE)) {
                    throw failure.getException();
                }
            }
            // Otherwise, throw the exception from the first failure reported.
            throw failures.get(0).getException();
        }
    }

    /**
     * Helper method to check whether two {@link Bundle}s are equal since the built-in {@code
     * equals} is not properly overridden.
     */
    private boolean bundlesContainSameStringKeyValuePairs(Bundle b1, Bundle b2) {
        if (b1.size() != b2.size()) {
            return false;
        }
        HashSet<String> allKeys = new HashSet<String>(b1.keySet());
        allKeys.addAll(b2.keySet());
        for (String key : allKeys) {
            if (b1.getString(key) != null) {
                // If key is in b1 and corresponds to a string, check whether this key corresponds
                // to the same value in b2.
                if (!b1.getString(key).equals(b2.getString(key))) {
                    return false;
                }
            } else if (b2.getString(key) != null) {
                // Otherwise if b2 has a string at this key, return false since we know that b1 does
                // not have a string at this key.
                return false;
            }
        }
        return true;
    }
}
