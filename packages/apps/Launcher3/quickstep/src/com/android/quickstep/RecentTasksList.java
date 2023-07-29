/*
 * Copyright (C) 2014 The Android Open Source Project
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

package com.android.quickstep;

import static com.android.launcher3.util.Executors.UI_HELPER_EXECUTOR;

import android.annotation.TargetApi;
import android.app.ActivityManager;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.Build;
import android.os.Process;
import android.service.wallpaper.WallpaperService;
import android.util.SparseBooleanArray;

import androidx.annotation.VisibleForTesting;

import com.android.launcher3.util.LooperExecutor;
import com.android.systemui.shared.recents.model.Task;
import com.android.systemui.shared.system.ActivityManagerWrapper;
import com.android.systemui.shared.system.KeyguardManagerCompat;
import com.android.systemui.shared.system.TaskStackChangeListener;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.function.Consumer;
import android.app.AppGlobals;
import android.util.Log;
/**
 * Manages the recent task list from the system, caching it as necessary.
 */
@TargetApi(Build.VERSION_CODES.P)
public class RecentTasksList extends TaskStackChangeListener {
    private static final String TAG = RecentTasksList.class.getSimpleName();

    private final KeyguardManagerCompat mKeyguardManager;
    private final LooperExecutor mMainThreadExecutor;
    private final ActivityManagerWrapper mActivityManagerWrapper;

    // The list change id, increments as the task list changes in the system
    private int mChangeId;
    // The last change id when the list was last loaded completely, must be <= the list change id
    private int mLastLoadedId;
    // The last change id was loaded with keysOnly  = true
    private boolean mLastLoadHadKeysOnly;

    ArrayList<Task> mTasks = new ArrayList<>();

    private String[] filterTaskForPkg = {
            AppGlobals.getInitialApplication().getPackageName(),
//            "com.android.wallpaper.livepicker",
        };
    private List<String> mNotCreateTaskForPkg = new ArrayList<>();

    public RecentTasksList(LooperExecutor mainThreadExecutor,
            KeyguardManagerCompat keyguardManager, ActivityManagerWrapper activityManagerWrapper) {
        mMainThreadExecutor = mainThreadExecutor;
        mKeyguardManager = keyguardManager;
        mChangeId = 1;
        mActivityManagerWrapper = activityManagerWrapper;
        mActivityManagerWrapper.registerTaskStackListener(this);
        for (String pkg : filterTaskForPkg) {
            mNotCreateTaskForPkg.add(pkg);
        }
    }

    private List<String> mInvalidThirdLivePickerPkg = Arrays.asList(new String[]{"com.tencent.android.qqdownloader"});
    private void filterThirdLivePickerPkg() {
        PackageManager pm = AppGlobals.getInitialApplication().getPackageManager();
        List<ResolveInfo> liveServiceList = pm.queryIntentServices(new Intent(WallpaperService.SERVICE_INTERFACE), PackageManager.GET_META_DATA);
        for (ResolveInfo ri : liveServiceList) {
            String pkgName = ri.serviceInfo.packageName;
            boolean isContains = mNotCreateTaskForPkg.contains(pkgName) || mInvalidThirdLivePickerPkg.contains(pkgName);
            Log.d(TAG, "filterThirdLivePickerPkg: pkgName = " + pkgName + ",,isContains = " + isContains);
            if (!isContains) mNotCreateTaskForPkg.add(pkgName);
        }
    }

    /**
     * Fetches the task keys skipping any local cache.
     */
    public void getTaskKeys(int numTasks, Consumer<ArrayList<Task>> callback) {
        // Kick off task loading in the background
        UI_HELPER_EXECUTOR.execute(() -> {
            ArrayList<Task> tasks = loadTasksInBackground(numTasks, true /* loadKeysOnly */);
            mMainThreadExecutor.execute(() -> callback.accept(tasks));
        });
    }

    /**
     * Asynchronously fetches the list of recent tasks, reusing cached list if available.
     *
     * @param loadKeysOnly Whether to load other associated task data, or just the key
     * @param callback The callback to receive the list of recent tasks
     * @return The change id of the current task list
     */
    public synchronized int getTasks(boolean loadKeysOnly, Consumer<ArrayList<Task>> callback) {
        final int requestLoadId = mChangeId;
        Runnable resultCallback = callback == null
                ? () -> { }
                : () -> callback.accept(copyOf(mTasks));

        if (mLastLoadedId == mChangeId && (!mLastLoadHadKeysOnly || loadKeysOnly)) {
            // The list is up to date, send the callback on the next frame,
            // so that requestID can be returned first.
            mMainThreadExecutor.post(resultCallback);
            return requestLoadId;
        }

        // Kick off task loading in the background
        UI_HELPER_EXECUTOR.execute(() -> {
            ArrayList<Task> tasks = loadTasksInBackground(Integer.MAX_VALUE, loadKeysOnly);

            mMainThreadExecutor.execute(() -> {
                mTasks = tasks;
                mLastLoadedId = requestLoadId;
                mLastLoadHadKeysOnly = loadKeysOnly;
                resultCallback.run();
            });
        });

        return requestLoadId;
    }

    /**
     * @return Whether the provided {@param changeId} is the latest recent tasks list id.
     */
    public synchronized boolean isTaskListValid(int changeId) {
        return mChangeId == changeId;
    }

    @Override
    public synchronized void onTaskStackChanged() {
        mChangeId++;
    }

    @Override
    public void onTaskRemoved(int taskId) {
        mTasks = loadTasksInBackground(Integer.MAX_VALUE, false);
    }

    @Override
    public synchronized void onActivityPinned(String packageName, int userId, int taskId,
            int stackId) {
        mChangeId++;
    }

    @Override
    public synchronized void onActivityUnpinned() {
        mChangeId++;
    }

    /**
     * Loads and creates a list of all the recent tasks.
     */
    @VisibleForTesting
    ArrayList<Task> loadTasksInBackground(int numTasks,
            boolean loadKeysOnly) {
        int currentUserId = Process.myUserHandle().getIdentifier();
        ArrayList<Task> allTasks = new ArrayList<>();
        List<ActivityManager.RecentTaskInfo> rawTasks =
                mActivityManagerWrapper.getRecentTasks(numTasks, currentUserId);
        // The raw tasks are given in most-recent to least-recent order, we need to reverse it
        Collections.reverse(rawTasks);

        SparseBooleanArray tmpLockedUsers = new SparseBooleanArray() {
            @Override
            public boolean get(int key) {
                if (indexOfKey(key) < 0) {
                    // Fill the cached locked state as we fetch
                    put(key, mKeyguardManager.isDeviceLocked(key));
                }
                return super.get(key);
            }
        };

        filterThirdLivePickerPkg();

        int taskCount = rawTasks.size();
        for (int i = 0; i < taskCount; i++) {
            ActivityManager.RecentTaskInfo rawTask = rawTasks.get(i);
            Task.TaskKey taskKey = new Task.TaskKey(rawTask);

            boolean isContains = mNotCreateTaskForPkg.contains(taskKey.sourceComponent.getPackageName());
            // Log.d(TAG, "loadTasksInBackground: taskKey.pkgName = " + taskKey.sourceComponent.getPackageName() + ",,isContains = " + isContains);
            if (isContains) {
                continue;
            }
            Task task;
            if (!loadKeysOnly) {
                boolean isLocked = tmpLockedUsers.get(taskKey.userId);
                task = Task.from(taskKey, rawTask, isLocked);
            } else {
                task = new Task(taskKey);
            }
            allTasks.add(task);
        }

        return allTasks;
    }

    private ArrayList<Task> copyOf(ArrayList<Task> tasks) {
        ArrayList<Task> newTasks = new ArrayList<>();
        for (int i = 0; i < tasks.size(); i++) {
            Task t = tasks.get(i);
            newTasks.add(new Task(t.key, t.colorPrimary, t.colorBackground, t.isDockable,
                    t.isLocked, t.taskDescription, t.topActivity));
        }
        return newTasks;
    }
}