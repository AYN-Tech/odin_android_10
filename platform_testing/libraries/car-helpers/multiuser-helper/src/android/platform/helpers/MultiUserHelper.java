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
// TODO(b/129771420): Use different package name for car-helper packages
package android.platform.helpers;

import android.annotation.Nullable;
import android.app.ActivityManager;
import android.app.UserSwitchObserver;
import android.car.userlib.CarUserManagerHelper;
import android.content.pm.UserInfo;
import android.os.RemoteException;
import android.os.SystemClock;

import androidx.test.InstrumentationRegistry;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Helper class that is used by integration test only. It is wrapping around {@link
 * CarUserManagerHelper} to expose user management functions. Unlike {@link CarUserManagerHelper} ,
 * some user management functions such as user switch will be synchronous calls in order to meet
 * testing requirements.
 */
public class MultiUserHelper {
    private static final String DEFAULT_GUEST_NAME = "Guest";

    private static final String TAG = MultiUserHelper.class.getSimpleName();

    /** For testing purpose we allow a wide range of switching time. */
    private static final int USER_SWITCH_TIMEOUT_SECOND = 300;

    private static MultiUserHelper sMultiUserHelper;
    private CarUserManagerHelper mUserManagerHelper;

    private MultiUserHelper() {
        mUserManagerHelper = new CarUserManagerHelper(InstrumentationRegistry.getTargetContext());
    }

    public enum UserType {
        GUEST,
        ADMIN,
        NON_ADMIN
    }

    /**
     * It will always be used as a singleton class
     *
     * @return MultiUserHelper instance
     */
    public static MultiUserHelper getInstance() {
        if (sMultiUserHelper == null) {
            sMultiUserHelper = new MultiUserHelper();
        }
        return sMultiUserHelper;
    }

    /**
     * Creates a user given the user name and type, e.g. guest, admin or non-admin
     *
     * @param name the name of the user
     * @param userType the type of user as defined by the helper
     * @return A {@link UserInfo} for newly created user or {@code null} if fail to create one
     */
    @Nullable
    public UserInfo createUser(String name, UserType userType) throws Exception {
        switch (userType) {
            case GUEST:
                return mUserManagerHelper.createNewOrFindExistingGuest(name);
            case ADMIN:
                return mUserManagerHelper.createNewAdminUser(name);
            case NON_ADMIN:
                return mUserManagerHelper.createNewNonAdminUser(name);
            default:
                throw new Exception("Unsupported user type: " + userType);
        }
    }

    /**
     * Switches to the target user at API level. Always waits until user switch complete. Besides,
     * it waits for an additional amount of time for switched user to become idle (stable)
     *
     * @param id user id
     * @param timeoutMs the time to wait (in msec) after user switch complete
     */
    public void switchAndWaitForStable(int id, long timeoutMs) throws Exception {
        switchToUserId(id);
        SystemClock.sleep(timeoutMs);
    }

    /**
     * Switches to the target user at API level. Always wait until user switch complete.
     *
     * <p>User switch complete only means the user ready at API level. It doesn't mean the UI is
     * completely ready for the target user. It doesn't include unlocking user data and loading car
     * launcher page
     *
     * @param id Id of the user to switch to
     */
    public void switchToUserId(int id) throws Exception {
        final CountDownLatch latch = new CountDownLatch(1);
        registerUserSwitchObserver(latch, id);
        if (!mUserManagerHelper.switchToUserId(id)) {
            throw new Exception(String.format("Failed to switch to user: %d", id));
        }
        if (!latch.await(USER_SWITCH_TIMEOUT_SECOND, TimeUnit.SECONDS)) {
            throw new Exception(
                    String.format(
                            "Timeout while switching to user %d after %d seconds",
                            id, USER_SWITCH_TIMEOUT_SECOND));
        }
    }

    /**
     * Removes the target user. For now it is a non-blocking call.
     *
     * @param userInfo info of the user to be removed
     * @return true if removed successfully
     */
    public boolean removeUser(UserInfo userInfo) {
        return mUserManagerHelper.removeUser(userInfo, DEFAULT_GUEST_NAME);
    }

    public UserInfo getCurrentForegroundUserInfo() {
        return mUserManagerHelper.getCurrentForegroundUserInfo();
    }

    public int getInitialUser() {
        return mUserManagerHelper.getInitialUser();
    }

    /**
     * Tries to find an existing user with the given name
     *
     * @param name the name of the user
     * @return A {@link UserInfo} if the user is found, or {@code null} if not found
     */
    @Nullable
    public UserInfo getUserByName(String name) {
        return mUserManagerHelper
                .getAllUsers()
                .stream()
                .filter(user -> user.name.equals(name))
                .findFirst()
                .orElse(null);
    }

    private void registerUserSwitchObserver(final CountDownLatch switchLatch, final int userId)
            throws RemoteException {
        ActivityManager.getService()
                .registerUserSwitchObserver(
                        new UserSwitchObserver() {
                            @Override
                            public void onUserSwitchComplete(int newUserId) {
                                if (switchLatch != null && userId == newUserId) {
                                    switchLatch.countDown();
                                }
                            }
                        },
                        TAG);
    }
}
