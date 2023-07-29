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
package com.android.car.ui;

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.database.Cursor;
import android.graphics.drawable.Drawable;
import android.text.InputFilter;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.AdapterView;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.ListAdapter;
import android.widget.TextView;

import androidx.annotation.ArrayRes;
import androidx.annotation.AttrRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;

/**
 * Wrapper for AlertDialog.Builder
 */
public class AlertDialogBuilder {

    private AlertDialog.Builder mBuilder;
    private Context mContext;
    private boolean mPositiveButtonSet;
    private boolean mNeutralButtonSet;
    private boolean mNegativeButtonSet;
    private CharSequence mTitle;
    private CharSequence mSubtitle;
    private Drawable mIcon;

    public AlertDialogBuilder(Context context) {
        // Resource id specified as 0 uses the parent contexts resolved value for alertDialogTheme.
        this(context, /* themeResId= */0);
    }

    public AlertDialogBuilder(Context context, int themeResId) {
        mBuilder = new AlertDialog.Builder(context, themeResId);
        mContext = context;
    }

    public Context getContext() {
        return mBuilder.getContext();
    }

    /**
     * Set the title using the given resource id.
     *
     * @return This Builder object to allow for chaining of calls to set methods
     */
    public AlertDialogBuilder setTitle(@StringRes int titleId) {
        return setTitle(mContext.getText(titleId));
    }

    /**
     * Set the title displayed in the {@link Dialog}.
     *
     * @return This Builder object to allow for chaining of calls to set methods
     */
    public AlertDialogBuilder setTitle(CharSequence title) {
        mTitle = title;
        mBuilder.setTitle(title);
        return this;
    }

    /**
     * Sets a subtitle to be displayed in the {@link Dialog}.
     *
     * @return This Builder object to allow for chaining of calls to set methods
     */
    public AlertDialogBuilder setSubtitle(@StringRes int subtitle) {
        return setSubtitle(mContext.getString(subtitle));
    }

    /**
     * Sets a subtitle to be displayed in the {@link Dialog}.
     *
     * @return This Builder object to allow for chaining of calls to set methods
     */
    public AlertDialogBuilder setSubtitle(CharSequence subtitle) {
        mSubtitle = subtitle;
        return this;
    }

    /**
     * Set the message to display using the given resource id.
     *
     * @return This Builder object to allow for chaining of calls to set methods
     */
    public AlertDialogBuilder setMessage(@StringRes int messageId) {
        mBuilder.setMessage(messageId);
        return this;
    }

    /**
     * Set the message to display.
     *
     * @return This Builder object to allow for chaining of calls to set methods
     */
    public AlertDialogBuilder setMessage(CharSequence message) {
        mBuilder.setMessage(message);
        return this;
    }

    /**
     * Set the resource id of the {@link Drawable} to be used in the title.
     * <p>
     * Takes precedence over values set using {@link #setIcon(Drawable)}.
     *
     * @return This Builder object to allow for chaining of calls to set methods
     */
    public AlertDialogBuilder setIcon(@DrawableRes int iconId) {
        return setIcon(mContext.getDrawable(iconId));
    }

    /**
     * Set the {@link Drawable} to be used in the title.
     * <p>
     * <strong>Note:</strong> To ensure consistent styling, the drawable
     * should be inflated or constructed using the alert dialog's themed
     * context obtained via {@link #getContext()}.
     *
     * @return this Builder object to allow for chaining of calls to set
     * methods
     */
    public AlertDialogBuilder setIcon(Drawable icon) {
        mIcon = icon;
        mBuilder.setIcon(icon);
        return this;
    }

    /**
     * Set an icon as supplied by a theme attribute. e.g.
     * {@link android.R.attr#alertDialogIcon}.
     * <p>
     * Takes precedence over values set using {@link #setIcon(int)} or
     * {@link #setIcon(Drawable)}.
     *
     * @param attrId ID of a theme attribute that points to a drawable resource.
     */
    public AlertDialogBuilder setIconAttribute(@AttrRes int attrId) {
        mBuilder.setIconAttribute(attrId);
        return this;
    }

    /**
     * Set a listener to be invoked when the positive button of the dialog is pressed.
     *
     * @param textId The resource id of the text to display in the positive button
     * @param listener The {@link DialogInterface.OnClickListener} to use.
     * @return This Builder object to allow for chaining of calls to set methods
     */
    public AlertDialogBuilder setPositiveButton(@StringRes int textId,
            final DialogInterface.OnClickListener listener) {
        mBuilder.setPositiveButton(textId, listener);
        mPositiveButtonSet = true;
        return this;
    }

    /**
     * Set a listener to be invoked when the positive button of the dialog is pressed.
     *
     * @param text The text to display in the positive button
     * @param listener The {@link DialogInterface.OnClickListener} to use.
     * @return This Builder object to allow for chaining of calls to set methods
     */
    public AlertDialogBuilder setPositiveButton(CharSequence text,
            final DialogInterface.OnClickListener listener) {
        mBuilder.setPositiveButton(text, listener);
        mPositiveButtonSet = true;
        return this;
    }

    /**
     * Set a listener to be invoked when the negative button of the dialog is pressed.
     *
     * @param textId The resource id of the text to display in the negative button
     * @param listener The {@link DialogInterface.OnClickListener} to use.
     * @return This Builder object to allow for chaining of calls to set methods
     */
    public AlertDialogBuilder setNegativeButton(@StringRes int textId,
            final DialogInterface.OnClickListener listener) {
        mBuilder.setNegativeButton(textId, listener);
        mNegativeButtonSet = true;
        return this;
    }

    /**
     * Set a listener to be invoked when the negative button of the dialog is pressed.
     *
     * @param text The text to display in the negative button
     * @param listener The {@link DialogInterface.OnClickListener} to use.
     * @return This Builder object to allow for chaining of calls to set methods
     */
    public AlertDialogBuilder setNegativeButton(CharSequence text,
            final DialogInterface.OnClickListener listener) {
        mBuilder.setNegativeButton(text, listener);
        mNegativeButtonSet = true;
        return this;
    }

    /**
     * Set a listener to be invoked when the neutral button of the dialog is pressed.
     *
     * @param textId The resource id of the text to display in the neutral button
     * @param listener The {@link DialogInterface.OnClickListener} to use.
     * @return This Builder object to allow for chaining of calls to set methods
     */
    public AlertDialogBuilder setNeutralButton(@StringRes int textId,
            final DialogInterface.OnClickListener listener) {
        mBuilder.setNeutralButton(textId, listener);
        mNeutralButtonSet = true;
        return this;
    }

    /**
     * Set a listener to be invoked when the neutral button of the dialog is pressed.
     *
     * @param text The text to display in the neutral button
     * @param listener The {@link DialogInterface.OnClickListener} to use.
     * @return This Builder object to allow for chaining of calls to set methods
     */
    public AlertDialogBuilder setNeutralButton(CharSequence text,
            final DialogInterface.OnClickListener listener) {
        mBuilder.setNeutralButton(text, listener);
        mNeutralButtonSet = true;
        return this;
    }

    /**
     * Sets whether the dialog is cancelable or not.  Default is true.
     *
     * @return This Builder object to allow for chaining of calls to set methods
     */
    public AlertDialogBuilder setCancelable(boolean cancelable) {
        mBuilder.setCancelable(cancelable);
        return this;
    }

    /**
     * Sets the callback that will be called if the dialog is canceled.
     *
     * <p>Even in a cancelable dialog, the dialog may be dismissed for reasons other than
     * being canceled or one of the supplied choices being selected.
     * If you are interested in listening for all cases where the dialog is dismissed
     * and not just when it is canceled, see
     * {@link #setOnDismissListener(android.content.DialogInterface.OnDismissListener)
     * setOnDismissListener}.</p>
     *
     * @return This Builder object to allow for chaining of calls to set methods
     * @see #setCancelable(boolean)
     * @see #setOnDismissListener(android.content.DialogInterface.OnDismissListener)
     */
    public AlertDialogBuilder setOnCancelListener(
            DialogInterface.OnCancelListener onCancelListener) {
        mBuilder.setOnCancelListener(onCancelListener);
        return this;
    }

    /**
     * Sets the callback that will be called when the dialog is dismissed for any reason.
     *
     * @return This Builder object to allow for chaining of calls to set methods
     */
    public AlertDialogBuilder setOnDismissListener(
            DialogInterface.OnDismissListener onDismissListener) {
        mBuilder.setOnDismissListener(onDismissListener);
        return this;
    }

    /**
     * Sets the callback that will be called if a key is dispatched to the dialog.
     *
     * @return This Builder object to allow for chaining of calls to set methods
     */
    public AlertDialogBuilder setOnKeyListener(DialogInterface.OnKeyListener onKeyListener) {
        mBuilder.setOnKeyListener(onKeyListener);
        return this;
    }

    /**
     * Set a list of items to be displayed in the dialog as the content, you will be notified of the
     * selected item via the supplied listener. This should be an array type i.e. R.array.foo
     *
     * @return This Builder object to allow for chaining of calls to set methods
     */
    public AlertDialogBuilder setItems(@ArrayRes int itemsId,
            final DialogInterface.OnClickListener listener) {
        mBuilder.setItems(itemsId, listener);
        return this;
    }

    /**
     * Set a list of items to be displayed in the dialog as the content, you will be notified of the
     * selected item via the supplied listener.
     *
     * @return This Builder object to allow for chaining of calls to set methods
     */
    public AlertDialogBuilder setItems(CharSequence[] items,
            final DialogInterface.OnClickListener listener) {
        mBuilder.setItems(items, listener);
        return this;
    }

    /**
     * Set a list of items, which are supplied by the given {@link ListAdapter}, to be
     * displayed in the dialog as the content, you will be notified of the
     * selected item via the supplied listener.
     *
     * @param adapter The {@link ListAdapter} to supply the list of items
     * @param listener The listener that will be called when an item is clicked.
     * @return This Builder object to allow for chaining of calls to set methods
     */
    public AlertDialogBuilder setAdapter(final ListAdapter adapter,
            final DialogInterface.OnClickListener listener) {
        mBuilder.setAdapter(adapter, listener);
        return this;
    }

    /**
     * Set a list of items, which are supplied by the given {@link Cursor}, to be
     * displayed in the dialog as the content, you will be notified of the
     * selected item via the supplied listener.
     *
     * @param cursor The {@link Cursor} to supply the list of items
     * @param listener The listener that will be called when an item is clicked.
     * @param labelColumn The column name on the cursor containing the string to display
     * in the label.
     * @return This Builder object to allow for chaining of calls to set methods
     */
    public AlertDialogBuilder setCursor(final Cursor cursor,
            final DialogInterface.OnClickListener listener,
            String labelColumn) {
        mBuilder.setCursor(cursor, listener, labelColumn);
        return this;
    }

    /**
     * Set a list of items to be displayed in the dialog as the content,
     * you will be notified of the selected item via the supplied listener.
     * This should be an array type, e.g. R.array.foo. The list will have
     * a check mark displayed to the right of the text for each checked
     * item. Clicking on an item in the list will not dismiss the dialog.
     * Clicking on a button will dismiss the dialog.
     *
     * @param itemsId the resource id of an array i.e. R.array.foo
     * @param checkedItems specifies which items are checked. It should be null in which case no
     * items are checked. If non null it must be exactly the same length as the array of
     * items.
     * @param listener notified when an item on the list is clicked. The dialog will not be
     * dismissed when an item is clicked. It will only be dismissed if clicked on a
     * button, if no buttons are supplied it's up to the user to dismiss the dialog.
     * @return This Builder object to allow for chaining of calls to set methods
     */
    public AlertDialogBuilder setMultiChoiceItems(@ArrayRes int itemsId, boolean[] checkedItems,
            final DialogInterface.OnMultiChoiceClickListener listener) {
        mBuilder.setMultiChoiceItems(itemsId, checkedItems, listener);
        return this;
    }

    /**
     * Set a list of items to be displayed in the dialog as the content,
     * you will be notified of the selected item via the supplied listener.
     * The list will have a check mark displayed to the right of the text
     * for each checked item. Clicking on an item in the list will not
     * dismiss the dialog. Clicking on a button will dismiss the dialog.
     *
     * @param items the text of the items to be displayed in the list.
     * @param checkedItems specifies which items are checked. It should be null in which case no
     * items are checked. If non null it must be exactly the same length as the array of
     * items.
     * @param listener notified when an item on the list is clicked. The dialog will not be
     * dismissed when an item is clicked. It will only be dismissed if clicked on a
     * button, if no buttons are supplied it's up to the user to dismiss the dialog.
     * @return This Builder object to allow for chaining of calls to set methods
     */
    public AlertDialogBuilder setMultiChoiceItems(CharSequence[] items, boolean[] checkedItems,
            final DialogInterface.OnMultiChoiceClickListener listener) {
        mBuilder.setMultiChoiceItems(items, checkedItems, listener);
        return this;
    }

    /**
     * Set a list of items to be displayed in the dialog as the content,
     * you will be notified of the selected item via the supplied listener.
     * The list will have a check mark displayed to the right of the text
     * for each checked item. Clicking on an item in the list will not
     * dismiss the dialog. Clicking on a button will dismiss the dialog.
     *
     * @param cursor the cursor used to provide the items.
     * @param isCheckedColumn specifies the column name on the cursor to use to determine
     * whether a checkbox is checked or not. It must return an integer value where 1
     * means checked and 0 means unchecked.
     * @param labelColumn The column name on the cursor containing the string to display in the
     * label.
     * @param listener notified when an item on the list is clicked. The dialog will not be
     * dismissed when an item is clicked. It will only be dismissed if clicked on a
     * button, if no buttons are supplied it's up to the user to dismiss the dialog.
     * @return This Builder object to allow for chaining of calls to set methods
     */
    public AlertDialogBuilder setMultiChoiceItems(Cursor cursor, String isCheckedColumn,
            String labelColumn,
            final DialogInterface.OnMultiChoiceClickListener listener) {
        mBuilder.setMultiChoiceItems(cursor, isCheckedColumn, labelColumn, listener);
        return this;
    }

    /**
     * Set a list of items to be displayed in the dialog as the content, you will be notified of
     * the selected item via the supplied listener. This should be an array type i.e.
     * R.array.foo The list will have a check mark displayed to the right of the text for the
     * checked item. Clicking on an item in the list will not dismiss the dialog. Clicking on a
     * button will dismiss the dialog.
     *
     * @param itemsId the resource id of an array i.e. R.array.foo
     * @param checkedItem specifies which item is checked. If -1 no items are checked.
     * @param listener notified when an item on the list is clicked. The dialog will not be
     * dismissed when an item is clicked. It will only be dismissed if clicked on a
     * button, if no buttons are supplied it's up to the user to dismiss the dialog.
     * @return This Builder object to allow for chaining of calls to set methods
     */
    public AlertDialogBuilder setSingleChoiceItems(@ArrayRes int itemsId, int checkedItem,
            final DialogInterface.OnClickListener listener) {
        mBuilder.setSingleChoiceItems(itemsId, checkedItem, listener);
        return this;
    }

    /**
     * Set a list of items to be displayed in the dialog as the content, you will be notified of
     * the selected item via the supplied listener. The list will have a check mark displayed to
     * the right of the text for the checked item. Clicking on an item in the list will not
     * dismiss the dialog. Clicking on a button will dismiss the dialog.
     *
     * @param cursor the cursor to retrieve the items from.
     * @param checkedItem specifies which item is checked. If -1 no items are checked.
     * @param labelColumn The column name on the cursor containing the string to display in the
     * label.
     * @param listener notified when an item on the list is clicked. The dialog will not be
     * dismissed when an item is clicked. It will only be dismissed if clicked on a
     * button, if no buttons are supplied it's up to the user to dismiss the dialog.
     * @return This Builder object to allow for chaining of calls to set methods
     */
    public AlertDialogBuilder setSingleChoiceItems(Cursor cursor, int checkedItem,
            String labelColumn,
            final DialogInterface.OnClickListener listener) {
        mBuilder.setSingleChoiceItems(cursor, checkedItem, labelColumn, listener);
        return this;
    }

    /**
     * Set a list of items to be displayed in the dialog as the content, you will be notified of
     * the selected item via the supplied listener. The list will have a check mark displayed to
     * the right of the text for the checked item. Clicking on an item in the list will not
     * dismiss the dialog. Clicking on a button will dismiss the dialog.
     *
     * @param items the items to be displayed.
     * @param checkedItem specifies which item is checked. If -1 no items are checked.
     * @param listener notified when an item on the list is clicked. The dialog will not be
     * dismissed when an item is clicked. It will only be dismissed if clicked on a
     * button, if no buttons are supplied it's up to the user to dismiss the dialog.
     * @return This Builder object to allow for chaining of calls to set methods
     */
    public AlertDialogBuilder setSingleChoiceItems(CharSequence[] items, int checkedItem,
            final DialogInterface.OnClickListener listener) {
        mBuilder.setSingleChoiceItems(items, checkedItem, listener);
        return this;
    }

    /**
     * Set a list of items to be displayed in the dialog as the content, you will be notified of
     * the selected item via the supplied listener. The list will have a check mark displayed to
     * the right of the text for the checked item. Clicking on an item in the list will not
     * dismiss the dialog. Clicking on a button will dismiss the dialog.
     *
     * @param adapter The {@link ListAdapter} to supply the list of items
     * @param checkedItem specifies which item is checked. If -1 no items are checked.
     * @param listener notified when an item on the list is clicked. The dialog will not be
     * dismissed when an item is clicked. It will only be dismissed if clicked on a
     * button, if no buttons are supplied it's up to the user to dismiss the dialog.
     * @return This Builder object to allow for chaining of calls to set methods
     */
    public AlertDialogBuilder setSingleChoiceItems(ListAdapter adapter, int checkedItem,
            final DialogInterface.OnClickListener listener) {
        mBuilder.setSingleChoiceItems(adapter, checkedItem, listener);
        return this;
    }

    /**
     * Sets a listener to be invoked when an item in the list is selected.
     *
     * @param listener the listener to be invoked
     * @return this Builder object to allow for chaining of calls to set methods
     * @see AdapterView#setOnItemSelectedListener(android.widget.AdapterView.OnItemSelectedListener)
     */
    public AlertDialogBuilder setOnItemSelectedListener(
            final AdapterView.OnItemSelectedListener listener) {
        mBuilder.setOnItemSelectedListener(listener);
        return this;
    }

    /**
     * Sets a custom edit text box within the alert dialog.
     *
     * @param prompt the string that will be set on the edit text view
     * @param textChangedListener textWatcher whose methods are called whenever this TextView's text
     * changes {@link null} otherwise.
     * @param inputFilters list of input filters, {@link null} if no filter is needed
     * @param inputType See {@link EditText#setInputType(int)}, except
     *                  {@link android.text.InputType#TYPE_NULL} will not be set.
     * @return this Builder object to allow for chaining of calls to set methods
     */
    public AlertDialogBuilder setEditBox(String prompt, TextWatcher textChangedListener,
            InputFilter[] inputFilters, int inputType) {
        View contentView = LayoutInflater.from(mContext).inflate(
                R.layout.car_ui_alert_dialog_edit_text, null);

        EditText editText = contentView.requireViewById(R.id.textbox);
        editText.setText(prompt);

        if (textChangedListener != null) {
            editText.addTextChangedListener(textChangedListener);
        }

        if (inputFilters != null) {
            editText.setFilters(inputFilters);
        }

        if (inputType != 0) {
            editText.setInputType(inputType);
        }

        mBuilder.setView(contentView);
        return this;
    }

    /**
     * Sets a custom edit text box within the alert dialog.
     *
     * @param prompt the string that will be set on the edit text view
     * @param textChangedListener textWatcher whose methods are called whenever this TextView's text
     * changes {@link null} otherwise.
     * @param inputFilters list of input filters, {@link null} if no filter is needed
     * @return this Builder object to allow for chaining of calls to set methods
     */
    public AlertDialogBuilder setEditBox(String prompt, TextWatcher textChangedListener,
            InputFilter[] inputFilters) {
        return setEditBox(prompt, textChangedListener, inputFilters, 0);
    }


    /** Final steps common to both {@link #create()} and {@link #show()} */
    private void prepareDialog() {
        if (mSubtitle != null) {

            View customTitle = LayoutInflater.from(mContext).inflate(
                    R.layout.car_ui_alert_dialog_title_with_subtitle, null);

            TextView mTitleView = customTitle.requireViewById(R.id.alertTitle);
            TextView mSubtitleView = customTitle.requireViewById(R.id.alertSubtitle);
            ImageView mIconView = customTitle.requireViewById(R.id.icon);

            mTitleView.setText(mTitle);
            mSubtitleView.setText(mSubtitle);
            mIconView.setImageDrawable(mIcon);
            mIconView.setVisibility(mIcon != null ? View.VISIBLE : View.GONE);
            mBuilder.setCustomTitle(customTitle);
        }

        if (!mNeutralButtonSet && !mNegativeButtonSet && !mPositiveButtonSet) {
            String mDefaultButtonText = mContext.getString(
                    R.string.car_ui_alert_dialog_default_button);
            mBuilder.setNegativeButton(mDefaultButtonText, (dialog, which) -> {
            });
        }
    }

    /**
     * Creates an {@link AlertDialog} with the arguments supplied to this
     * builder.
     * <p>
     * Calling this method does not display the dialog. If no additional
     * processing is needed, {@link #show()} may be called instead to both
     * create and display the dialog.
     */
    public AlertDialog create() {
        prepareDialog();
        return mBuilder.create();
    }

    /**
     * Creates an {@link AlertDialog} with the arguments supplied to this
     * builder and immediately displays the dialog.
     * <p>
     * Calling this method is functionally identical to:
     * <pre>
     *     AlertDialog dialog = builder.create();
     *     dialog.show();
     * </pre>
     */
    public AlertDialog show() {
        prepareDialog();
        return mBuilder.show();
    }
}
