/*
 Copyright (c) 2014, 2016, The Linux Foundation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
package com.android.mms.ui;

import com.android.mms.ui.PopupList;
import com.android.mms.R;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;

public class SelectionMenu implements View.OnClickListener {
    private final Context mContext;
    private final Button mButton;
    private ImageView mExpandImageView;
    private String mCheckedCount;
    private final PopupList mPopupList;
    public static final int NUMBER_SELECTED = 0;
    public static final int SELECT_OR_DESELECT = 1;
    public Drawable mExpandLessDrawable;

    public SelectionMenu(Context context, Button button,
            PopupList.OnPopupItemClickListener listener,
            ImageView expandImageView) {
        mContext = context;
        mButton = button;
        mExpandImageView = expandImageView;
        mPopupList = new PopupList(context, mButton, mExpandImageView);
        mPopupList.addItem(NUMBER_SELECTED, mCheckedCount);
        mPopupList.addItem(SELECT_OR_DESELECT, context.getString(R.string.selected_all));
        mPopupList.setOnPopupItemClickListener(listener);
        mButton.setOnClickListener(this);
        mExpandLessDrawable = mContext.getResources().getDrawable(R.drawable.expand_less);
    }

    public SelectionMenu(Context context, Button button,
            PopupList.OnPopupItemClickListener listener) {
        this(context, button, listener, null);
    }

    @Override
    public void onClick(View v) {
        if (mExpandImageView != null) {
            mExpandImageView.setImageDrawable(mExpandLessDrawable);
        }
        mPopupList.show();
    }

    public void dismiss() {
        mPopupList.dismiss();
    }

    public void updateSelectAllMode(boolean inSelectAllMode) {
        PopupList.Item item = mPopupList.findItem(SELECT_OR_DESELECT);
        if (item != null) {
            item.setTitle(mContext.getString(
                    inSelectAllMode ? R.string.deselected_all : R.string.selected_all));
        }
    }

    public void setTitle(CharSequence title) {
        mButton.setText(title);
        mCheckedCount = title.toString();
    }

    public void updateCheckedCount() {
        PopupList.Item item = mPopupList.findItem(NUMBER_SELECTED);
        if (item != null) {
            item.setTitle(mCheckedCount);
        }
    }
}
