/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.

 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

package com.android.soundrecorder.actionmode;

import android.app.Activity;
import android.view.ActionMode;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.LinearLayout;
import android.widget.PopupMenu;
import android.widget.Spinner;

import com.android.soundrecorder.R;
import com.android.soundrecorder.filelist.FileListRecyclerAdapter;
import com.android.soundrecorder.filelist.listitem.BaseListItem;

import java.util.ArrayList;
import java.util.List;

public class ActionModeHandler implements FileListRecyclerAdapter.ActionModeListener {
    private Activity mActivity;
    private View mCustomView;
    private ButtonWithPopupMenu mSelectionButton;
    private String mSelectedFormat;
    private ActionMode mActionMode;
    private ActionMode.Callback mCallback;

    private Spinner mSelectionSpinner;
    private String mSelectedAllString;
    private String mDeSelectedAllString;
    private static final int SPINNER_COUNT_INDEX = 0;
    private static final int SPINNER_SELECT_ALL_INDEX = 1;

    public ActionModeHandler(Activity activity, int customViewId, ActionMode.Callback callback) {
        mActivity = activity;
        mCallback = callback;
        mCustomView = LayoutInflater.from(activity).inflate(customViewId, null);
        mSelectedAllString = mCustomView.getResources().getString(R.string.action_mode_select_all);
        mDeSelectedAllString = mCustomView.getResources()
                .getString(R.string.action_mode_deselect_all);
        mSelectionButton = (ButtonWithPopupMenu) mCustomView.findViewById(R.id.selection_button);
        mSelectionButton.loadPopupMenu(R.menu.select_all_popup_menu, mOnSelectAllClickListener);
        mSelectedFormat = mCustomView.getResources().getString(
                R.string.action_mode_selected);

        mSelectionSpinner = (Spinner) mCustomView.findViewById(R.id.selection_spinner);

        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.MATCH_PARENT);
        mCustomView.setLayoutParams(lp);
    }

    @Override
    public void showActionMode() {
        mActionMode = mActivity.startActionMode(mCallback);
        if (mActionMode != null) {
            mActionMode.setCustomView(mCustomView);
        }
    }

    @Override
    public void exitActionMode() {
        if (mActionMode != null) {
            mActionMode.finish();
        }
    }

    @Override
    public void setSelectedCount(int selectedCount, int totalCount, List<BaseListItem> items) {
        updateSelectionMenu(selectedCount, totalCount);
        updateActionModeOperations(selectedCount, items);
    }

    private void updateSelectionMenu(int selectedCount, int totalCount) {
        List<String> list = new ArrayList<String>();
        list.add(String.format(mSelectedFormat, selectedCount));
        list.add(selectedCount >= totalCount ? mDeSelectedAllString : mSelectedAllString);
        ArrayAdapter adapter = new ArrayAdapter<>(mSelectionSpinner.getContext(),
                R.layout.spinner_dropdown_item, list);
        mSelectionSpinner.setAdapter(adapter);
        mSelectionSpinner.setOnItemSelectedListener(new Spinner.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                if (position == SPINNER_SELECT_ALL_INDEX) {
                    if (mCallback != null && mActionMode != null) {
                        MenuItem item = mActionMode.getMenu().findItem(R.id.action_select_all);
                        mCallback.onActionItemClicked(mActionMode, item);
                    }
                }
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {
            }
        });
        mSelectionSpinner.setSelection(SPINNER_COUNT_INDEX, true);

        mSelectionButton.setText(String.format(mSelectedFormat, selectedCount));
        MenuItem menuItem = mSelectionButton.findPopupMenuItem(R.id.action_select_all);
        menuItem.setTitle(selectedCount >= totalCount ? mDeSelectedAllString : mSelectedAllString);
    }

    private void updateActionModeOperations(int selectedCount, List<BaseListItem> items) {
        if (mActionMode != null) {
            int operation = BaseListItem.SUPPORT_ALL;
            for (BaseListItem item : items) {
                operation &= item.getSupportedOperation();
            }

            if (selectedCount == 0) {
                operation = BaseListItem.SUPPORT_NONE;
            } else if (selectedCount > 1) {
                operation &= ~BaseListItem.SUPPORT_EDIT;
            }
            updateMenuOperation(mActionMode.getMenu(), operation);
        }
    }

    private void updateMenuOperation(Menu menu, int supported) {
        boolean supportDelete = (supported & BaseListItem.SUPPORT_DELETE) != 0;
        boolean supportShare = (supported & BaseListItem.SUPPORT_SHARE) != 0;
        boolean supportEdit = (supported & BaseListItem.SUPPORT_EDIT) != 0;
        setMenuItemVisible(menu, R.id.action_delete, supportDelete);
        setMenuItemVisible(menu, R.id.action_share, supportShare);
        setMenuItemVisible(menu, R.id.action_edit, supportEdit);
    }

    private void setMenuItemVisible(Menu menu, int itemId, boolean visible) {
        MenuItem item = menu.findItem(itemId);
        if (item != null) item.setVisible(visible);
    }

    private PopupMenu.OnMenuItemClickListener mOnSelectAllClickListener =
            new PopupMenu.OnMenuItemClickListener() {
                @Override
                public boolean onMenuItemClick(MenuItem item) {
                    if (mCallback != null && mActionMode != null) {
                        return mCallback.onActionItemClicked(mActionMode, item);
                    } else {
                        return false;
                    }
                }
            };
}
