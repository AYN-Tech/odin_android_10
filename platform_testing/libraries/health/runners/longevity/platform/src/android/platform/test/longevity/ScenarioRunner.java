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

import android.os.Bundle;
import android.platform.test.longevity.proto.Configuration.Scenario;
import android.platform.test.longevity.proto.Configuration.Scenario.ExtraArg;
import androidx.annotation.VisibleForTesting;
import androidx.test.InstrumentationRegistry;

import org.junit.runner.notification.RunNotifier;
import org.junit.runners.BlockJUnit4ClassRunner;
import org.junit.runners.model.FrameworkMethod;
import org.junit.runners.model.InitializationError;

/** A {@link BlockJUnit4ClassRunner} that runs a test class with profile-specified options. */
public class ScenarioRunner extends LongevityClassRunner {
    private final Scenario mScenario;
    private final Bundle mArguments;

    public ScenarioRunner(Class<?> klass, Scenario scenario) throws InitializationError {
        this(klass, scenario, InstrumentationRegistry.getArguments());
    }

    @VisibleForTesting
    ScenarioRunner(Class<?> klass, Scenario scenario, Bundle arguments) throws InitializationError {
        super(klass, arguments);
        mScenario = scenario;
        mArguments = arguments;
    }

    @Override
    protected void runChild(final FrameworkMethod method, RunNotifier notifier) {
        // Keep a copy of the bundle arguments for restoring later.
        Bundle modifiedArguments = mArguments.deepCopy();
        for (ExtraArg argPair : mScenario.getExtrasList()) {
            if (argPair.getKey() == null || argPair.getValue() == null) {
                throw new IllegalArgumentException(
                        String.format(
                                "Each extra arg entry in scenario must have both a key and a value,"
                                        + " but scenario is %s.",
                                mScenario.toString()));
            }
            modifiedArguments.putString(argPair.getKey(), argPair.getValue());
        }
        // Swap the arguments, run the scenario, and then restore arguments.
        InstrumentationRegistry.registerInstance(
                InstrumentationRegistry.getInstrumentation(), modifiedArguments);
        super.runChild(method, notifier);
        InstrumentationRegistry.registerInstance(
                InstrumentationRegistry.getInstrumentation(), mArguments);
    }
}
