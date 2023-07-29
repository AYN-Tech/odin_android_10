/*
 * Copyright 2019 The Android Open Source Project
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

package com.android.car.ui.recyclerview;

import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * Definition of list items that can be inserted into {@link CarUiListItemAdapter}.
 */
public class CarUiContentListItem extends CarUiListItem {

    /**
     * Callback to be invoked when the checked state of a list item changed.
     */
    public interface OnCheckedChangedListener {
        /**
         * Called when the checked state of a list item has changed.
         *
         * @param item The item whose checked state changed.
         * @param isChecked new checked state of list item.
         */
        void onCheckedChanged(CarUiContentListItem item, boolean isChecked);
    }

    /**
     * Enum of secondary action types of a list item.
     */
    public enum Action {
        /**
         * For an action value of NONE, no action element is shown for a list item.
         */
        NONE,
        /**
         * For an action value of SWITCH, a switch is shown for the action element of the list item.
         */
        SWITCH,
        /**
         * For an action value of CHECK_BOX, a checkbox is shown for the action element of the list
         * item.
         */
        CHECK_BOX,
        /**
         * For an action value of CHECK_BOX, a radio button is shown for the action element of the
         * list item.
         */
        RADIO_BUTTON,
        /**
         * For an action value of ICON, an icon is shown for the action element of the list item.
         */
        ICON
    }

    private Drawable mIcon;
    @Nullable
    private Drawable mSupplementalIcon;
    private CharSequence mTitle;
    private CharSequence mBody;
    private Action mAction;
    private boolean mIsActionDividerVisible;
    private boolean mIsChecked;
    private OnCheckedChangedListener mOnCheckedChangedListener;
    private View.OnClickListener mSupplementalIconOnClickListener;

    public CarUiContentListItem() {
        mAction = Action.NONE;
    }

    /**
     * Returns the title of the item.
     */
    @Nullable
    public CharSequence getTitle() {
        return mTitle;
    }

    /**
     * Sets the title of the item.
     *
     * @param title text to display as title.
     */
    public void setTitle(@NonNull CharSequence title) {
        mTitle = title;
    }

    /**
     * Returns the body text of the item.
     */
    @Nullable
    public CharSequence getBody() {
        return mBody;
    }

    /**
     * Sets the body of the item.
     *
     * @param body text to display as body text.
     */
    public void setBody(@NonNull CharSequence body) {
        mBody = body;
    }

    /**
     * Returns the icon of the item.
     */
    @Nullable
    public Drawable getIcon() {
        return mIcon;
    }

    /**
     * Sets the icon of the item.
     *
     * @param icon the icon to display.
     */
    public void setIcon(@Nullable Drawable icon) {
        mIcon = icon;
    }

    /**
     * Returns {@code true} if the item is checked. Will always return {@code false} when the action
     * type for the item is {@code Action.NONE}.
     */
    public boolean isChecked() {
        return mIsChecked;
    }

    /**
     * Sets the checked state of the item.
     *
     * @param checked the checked state for the item.
     */
    public void setChecked(boolean checked) {
        // Checked state can only be set when action type is checkbox, radio button or switch.
        if (mAction == Action.CHECK_BOX || mAction == Action.SWITCH
                || mAction == Action.RADIO_BUTTON) {
            mIsChecked = checked;
        }
    }

    /**
     * Sets the visibility of the action divider.
     *
     * @param visible visibility of the action divider.
     */
    public void setActionDividerVisible(boolean visible) {
        mIsActionDividerVisible = visible;
    }

    /**
     * Returns {@code true} if the action divider is visible.
     */
    public boolean isActionDividerVisible() {
        return mIsActionDividerVisible;
    }

    /**
     * Returns the action type for the item.
     */
    public Action getAction() {
        return mAction;
    }

    /**
     * Sets the action type for the item.
     *
     * @param action the action type for the item.
     */
    public void setAction(Action action) {
        mAction = action;

        // Cannot have checked state be true when there action type is not checkbox, radio button or
        // switch.
        if (mAction != Action.CHECK_BOX && mAction != Action.SWITCH
                && mAction != Action.RADIO_BUTTON) {
            mIsChecked = false;
        }
    }

    /**
     * Returns the supplemental icon for the item.
     */
    @Nullable
    public Drawable getSupplementalIcon() {
        if (mAction != Action.ICON) {
            return null;
        }

        return mSupplementalIcon;
    }

    /**
     * Sets supplemental icon to be displayed in a list item.
     *
     * @param icon the Drawable to set as the icon, or null to clear the content.
     */
    public void setSupplementalIcon(@Nullable Drawable icon) {
        setSupplementalIcon(icon, null);
    }

    /**
     * Sets supplemental icon to be displayed in a list item.
     *
     * @param icon     the Drawable to set as the icon, or null to clear the content.
     * @param listener the callback that is invoked when the icon is clicked.
     */
    public void setSupplementalIcon(@Nullable Drawable icon,
            @Nullable View.OnClickListener listener) {
        mAction = Action.ICON;

        // Cannot have checked state when action type is {@code Action.ICON}.
        mIsChecked = false;

        mSupplementalIcon = icon;
        mSupplementalIconOnClickListener = listener;
    }

    View.OnClickListener getSupplementalIconOnClickListener() {
        return mSupplementalIconOnClickListener;
    }

    /**
     * Registers a callback to be invoked when the checked state of list item changes.
     *
     * <p>Checked state changes can take place when the action type is {@code Action.SWITCH} or
     * {@code Action.CHECK_BOX}.
     *
     * @param listener callback to be invoked when the checked state shown in the UI changes.
     */
    public void setOnCheckedChangedListener(
            @NonNull OnCheckedChangedListener listener) {
        mOnCheckedChangedListener = listener;
    }

    @Nullable
    OnCheckedChangedListener getOnCheckedChangedListener() {
        return mOnCheckedChangedListener;
    }
}
