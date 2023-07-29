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

package com.android.soundrecorder.filelist.viewholder;

import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.TextView;

import com.android.soundrecorder.R;
import com.android.soundrecorder.filelist.listitem.BaseListItem;

public class BaseViewHolder extends RecyclerView.ViewHolder implements View.OnClickListener,
        View.OnLongClickListener {
    protected View mRootView;
    protected TextView mTitleView;
    protected CheckBox mCheckBox;
    protected boolean mInSelectionMode = false;
    protected BaseListItem mItem;

    public interface ViewHolderItemListener {
        void onItemChecked(BaseListItem item, boolean isChecked);
        void onItemClick(BaseListItem item);
        void onItemLongClick(BaseListItem item);
    }

    protected ViewHolderItemListener mItemListener;

    public void setViewHolderItemListener(ViewHolderItemListener listener) {
        mItemListener = listener;
    }

    public BaseViewHolder(View itemView, int rootLayoutId) {
        super(itemView);
        mRootView = itemView.findViewById(rootLayoutId);
        if (mRootView != null) {
            mRootView.setOnClickListener(this);
            mRootView.setOnLongClickListener(this);
        }
        mTitleView = (TextView)itemView.findViewById(R.id.list_item_title);
        mCheckBox = (CheckBox)itemView.findViewById(R.id.list_check_box);
        if (mCheckBox != null) {
            mCheckBox.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
                @Override
                public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                    if (mItemListener != null) {
                        mItemListener.onItemChecked(mItem, isChecked);
                    }
                }
            });
        }
    }

    public void setItem(BaseListItem item) {
        mItem = item;
        setTitle(item.getTitle());
        if (mItem.isSelectable()) {
            setSelected(mInSelectionMode && item.isChecked());
        } else {
            if (mCheckBox != null) {
                mCheckBox.setVisibility(View.INVISIBLE);
            }
        }
    }

    @Override
    public void onClick(View v) {
        if (mInSelectionMode) {
            setSelected(!isSelected());
        } else {
            if (mItemListener != null) {
                mItemListener.onItemClick(mItem);
            }
        }
    }

    @Override
    public boolean onLongClick(View v) {
        if (!mItem.isSelectable() || mInSelectionMode) {
            return false;
        }
        if (mItemListener != null) {
            mItemListener.onItemLongClick(mItem);
            return true;
        }
        return false;
    }

    public void setTitle(String title) {
        if (mTitleView != null) {
            mTitleView.setText(title);
        }
    }

    public void setSelectionMode(boolean inSelection) {
        mInSelectionMode = inSelection;
        if (mCheckBox == null) return;
        mCheckBox.setVisibility(mInSelectionMode ? View.VISIBLE : View.INVISIBLE);
    }

    public void setSelected(boolean selected) {
        if (mCheckBox == null) return;
        mCheckBox.setChecked(selected);
    }

    public boolean isSelected() {
        return mCheckBox != null && mCheckBox.isChecked();
    }
}
