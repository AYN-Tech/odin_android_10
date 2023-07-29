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

package android.platform.helpers;

import android.graphics.Rect;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.Direction;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject2;
import androidx.test.InstrumentationRegistry;

/**
 * This interface is intended to be inherited by AppHelper classes to add scrolling functionlity.
 */
public interface Scrollable {
    /**
     * Setup expectations: None
     *
     * <p>return true is the corresponding app is in foreground, and false otherwise.
     */
    boolean isAppInForeground();

    /**
     * Setup expectations: None.
     *
     * <p>Scroll up from the bottom of the scrollable region to top of the scrollable region (i.e.
     * by one page) in <code>durationMs</code> milliseconds only if corresponding app is open.
     *
     * @param durationMs The duration in milliseconds to perform the scrolling gesture.
     */
    public default void scrollUpOnePage(long durationMs) {
        scrollUp(100f, durationMs);
    }

    /**
     * Setup expectations: None.
     *
     * <p>Scroll up from the bottom of the scrollable region towards the top of the scrollable
     * region by <code>percent</code> percent of the whole scrollable region in <code>durationMs
     * </code> milliseconds only if corresponding app is open.
     *
     * @param percent The percentage of the whole scrollable region by which to scroll up, ranging
     *     from 0 - 100. For instance, percent = 50 would scroll up by half of the screen.
     * @param durationMs The duration in milliseconds to perform the scrolling gesture.
     */
    public default void scrollUp(float percent, long durationMs) {
        scroll(Direction.UP, percent, durationMs);
    }

    /**
     * Setup expectations: None.
     *
     * <p>Scroll down from the top of the scrollable region to bottom of the scrollable region (i.e.
     * by one page) in <code>durationMs</code> milliseconds only if corresponding app is open.
     *
     * @param durationMs The duration in milliseconds to perform the scrolling gesture.
     */
    public default void scrollDownOnePage(long durationMs) {
        scrollDown(100f, durationMs);
    }

    /**
     * Setup expectations: None.
     *
     * <p>Scroll down from the top of the scrollable region towards the bottom of the scrollable
     * region by <code>percent</code> percent of the whole scrollable region in <code>durationMs
     * </code> milliseconds only if corresponding app is open.
     *
     * @param percent The percentage of the whole scrollable region by which to scroll down, ranging
     *     from 0 - 100. For instance, percent = 50 would scroll down by half of the screen.
     * @param durationMs The duration in milliseconds to perform the scrolling gesture.
     */
    public default void scrollDown(float percent, long durationMs) {
        scroll(Direction.DOWN, percent, durationMs);
    }

    /**
     * This is not part of the public interface. For internal use only.
     *
     * <p>Scroll in <code>direction</code> direction by <code>percent</code> percent of the whole
     * scrollable region in <code>durationMs </code> milliseconds only if corresponding app is open.
     *
     * @param direction The direction in which to perform scrolling, it's either up or down.
     * @param percent The percentage of the whole scrollable region by which to scroll, ranging from
     *     0 - 100. For instance, percent = 50 would scroll up/down by half of the screen.
     * @param durationMs The duration in milliseconds to perform the scrolling gesture.
     */
    default void scroll(Direction direction, float percent, long durationMs) {
        if (isAppInForeground()) {
            UiDevice device = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
            UiObject2 scrollable = device.findObject(By.scrollable(true));

            if (scrollable != null) {
                int scrollSpeed = calcScrollSpeed(scrollable, durationMs);
                scrollable.scroll(direction, percent / 100, scrollSpeed);
            } else {
                throw new RuntimeException("There is nothing that can scroll.");
            }
        } else {
            throw new RuntimeException("App is not open.");
        }
    }

    /**
     * This is not part of the public interface. For internal use only.
     *
     * <p>Return the scroll speed such that it takes <code>durationMs</code> milliseconds for the
     * device to scroll through the whole scrollable region(i.e. from the top of the scrollable
     * region to bottom).
     *
     * @param scrollable The given scrollable object to scroll through.
     * @param durationMs The duration in milliseconds to perform the scrolling gesture.
     */
    default int calcScrollSpeed(UiObject2 scrollable, long durationMs) {
        Rect bounds = scrollable.getVisibleBounds();
        double durationSeconds = (double) durationMs / 1000;
        int scrollSpeed = (int) (bounds.height() / durationSeconds);
        return scrollSpeed;
    }
}
