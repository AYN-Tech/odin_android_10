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

package com.android.server.telecom.tests;

import static junit.framework.Assert.fail;

import android.telecom.Logging.Session;
import android.test.suitebuilder.annotation.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/**
 * Unit tests for android.telecom.Logging.Session
 */

@RunWith(JUnit4.class)
public class SessionTest {

    /**
     * Ensure creating two sessions that are parent/child of each other does not lead to a crash
     * or infinite recursion.
     */
    @SmallTest
    @Test
    public void testRecursion() {
        Session parentSession =  createTestSession("parent", "p");
        Session childSession =  createTestSession("child", "c");
        parentSession.addChild(childSession);
        parentSession.setParentSession(childSession);
        childSession.addChild(parentSession);
        childSession.setParentSession(parentSession);

        // Make sure calling these methods does not result in a crash
        try {
            parentSession.printFullSessionTree();
            childSession.printFullSessionTree();
            parentSession.getFullMethodPath(false);
            childSession.getFullMethodPath(false);
            parentSession.toString();
            childSession.toString();
            Session.Info.getInfo(parentSession);
            Session.Info.getInfo(childSession);
        } catch (Exception e) {
            fail();
        }
    }

    private Session createTestSession(String name, String methodName) {
        return new Session(name, methodName, 0, false, null);
    }
}
