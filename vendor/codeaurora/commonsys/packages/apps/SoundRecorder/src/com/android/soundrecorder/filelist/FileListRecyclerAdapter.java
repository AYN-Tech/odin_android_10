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

package com.android.soundrecorder.filelist;

import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.util.Log;

import com.android.soundrecorder.R;
import com.android.soundrecorder.filelist.listitem.BaseListItem;
import com.android.soundrecorder.filelist.listitem.FolderItem;
import com.android.soundrecorder.filelist.listitem.MediaItem;
import com.android.soundrecorder.filelist.viewholder.BaseViewHolder;
import com.android.soundrecorder.filelist.viewholder.FolderItemViewHolder;
import com.android.soundrecorder.filelist.viewholder.MediaItemViewHolder;
import com.android.soundrecorder.util.DatabaseUtils;
import com.android.soundrecorder.util.FileUtils;

import java.io.File;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;

public class FileListRecyclerAdapter extends RecyclerView.Adapter {
    private String[] mTargetFolderArray;
    private String[] mTargetSourceArray;
    private static final String TAG = "FileListRecyclerAdapter";

    private ContentResolver mContentResolver;

    // store all items
    private List<BaseListItem> mItemsList = new ArrayList<>();

    private boolean mInSelectionMode = false;

    public interface ItemListener {
        void openItem(BaseListItem item);
        void closeItem();
        MediaItem getPlayingItem();
        void updatePlayerItem(MediaItem item);
    }

    private ItemListener mItemListener;

    public void setItemListener(ItemListener listener) {
        mItemListener = listener;
    }

    public interface ActionModeListener {
        void showActionMode();
        void exitActionMode();
        void setSelectedCount(int selectedCount, int totalCount, List<BaseListItem> items);
    }

    private ActionModeListener mActionModeListener;

    public void setActionModeListener(ActionModeListener listener) {
        mActionModeListener = listener;
    }

    public FileListRecyclerAdapter(Context context, String[] targetFolderArray,
            String[] targetSourceArray) {
        mContentResolver = context.getContentResolver();
        mTargetFolderArray = targetFolderArray;
        mTargetSourceArray = targetSourceArray;
    }

    @Override
    public int getItemViewType(int position) {
        if (mItemsList != null && mItemsList.size() > position) {
            return mItemsList.get(position).getItemType();
        }
        return BaseListItem.TYPE_NONE;
    }

    @Override
    public RecyclerView.ViewHolder onCreateViewHolder(ViewGroup viewGroup, int viewType) {
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        if (viewType == BaseListItem.TYPE_FOLDER) {
            View folderItem = LayoutInflater.from(viewGroup.getContext()).inflate(
                    R.layout.file_list_item_folder, null);
            folderItem.setLayoutParams(lp);
            return new FolderItemViewHolder(folderItem, R.id.file_list_folder_layout);
        } else {
            View recordingItem = LayoutInflater.from(viewGroup.getContext()).inflate(
                    R.layout.file_list_item_recording, null);
            recordingItem.setLayoutParams(lp);
            return new MediaItemViewHolder(recordingItem, R.id.file_list_recording_layout);
        }
    }

    @Override
    public void onBindViewHolder(final RecyclerView.ViewHolder viewHolder, final int position) {
        if (viewHolder instanceof BaseViewHolder) {
            if (mItemsList != null && mItemsList.size() > position) {
                ((BaseViewHolder)viewHolder).setSelectionMode(mInSelectionMode);
                ((BaseViewHolder)viewHolder).setItem(mItemsList.get(position));
                ((BaseViewHolder)viewHolder)
                        .setViewHolderItemListener(new BaseViewHolder.ViewHolderItemListener() {
                            @Override
                            public void onItemChecked(BaseListItem item, boolean isChecked) {
                                changeSelectedState(viewHolder.getAdapterPosition(), isChecked);
                            }

                            @Override
                            public void onItemClick(BaseListItem item) {
                                if (mItemListener != null) {
                                    mItemListener.openItem(item);
                                }
                            }

                            @Override
                            public void onItemLongClick(BaseListItem item) {
                                enterSelectionMode(viewHolder.getAdapterPosition());
                            }
                        });

                setMediaItemPlayStatusListener(position);
            }
        }
    }

    @Override
    public int getItemCount() {
        return mItemsList == null ? 0 : mItemsList.size();
    }

    public int getSelectableItemCount() {
        int count = 0;
        for (BaseListItem item : mItemsList) {
            count = count + (item.isSelectable() ? 1 : 0);
        }
        return count;
    }

    private int getSelectedItemCount() {
        return getSelectedItems().size();
    }

    public List<BaseListItem> getSelectedItems() {
        List<BaseListItem> items = new ArrayList<>();
        for (BaseListItem item : mItemsList) {
            if (item.isChecked()) {
                items.add(item);
            }
        }
        return items;
    }

    public void reload() {
        List<BaseListItem> resultList = new ArrayList<>();

        Cursor cursor = DatabaseUtils.getFolderCursor(mContentResolver);
        if (cursor != null) {
            int len = cursor.getCount();
            for (int i = 0; i < len; i++) {
                cursor.moveToNext();
                WeakReference<Cursor> cursorWeakReference = new WeakReference<>(cursor);
                MediaItem item = new MediaItem(cursorWeakReference.get());
                resultList.add(item);
            }
            cursor.close();
        }

        // update list item to mItemsList.
        for (BaseListItem item : resultList) {
            int index = mItemsList.indexOf(item);
            if (index >= 0) {
                item.copyFrom(mItemsList.get(index));
            }
        }
        mItemsList.clear();
        mItemsList.addAll(resultList);

        updatePlayerItem();

        updateSelectedCountInActionMode();

        notifyDataSetChanged();
    }

    private void updatePlayerItem() {
        if (mItemListener == null) return;
        MediaItem playingItem = mItemListener.getPlayingItem();
        if (playingItem == null || !FileUtils.exists(new File(playingItem.getPath()))) {
            mItemListener.closeItem();
        } else {
            int index = mItemsList.indexOf(playingItem);
            if (index >= 0) {
                BaseListItem item = mItemsList.get(index);
                if (item instanceof MediaItem) {
                    mItemListener.updatePlayerItem((MediaItem) item);
                }
            }
        }
    }

    private void enterSelectionMode(int startPosition) {
        if (mInSelectionMode) return;

        if (mItemListener != null) {
            mItemListener.closeItem();
        }

        if (mActionModeListener != null) {
            mActionModeListener.showActionMode();
        }
        mInSelectionMode = true;
        changeSelectedState(startPosition, true);
        notifyDataSetChanged();
    }

    public void leaveSelectionMode() {
        mInSelectionMode = false;
        int count = getItemCount();
        for (int i = 0; i < count; i++) {
            changeSelectedState(i, false);
        }
        notifyDataSetChanged();
    }

    private void updateSelectedCountInActionMode() {
        if (mActionModeListener != null) {
            int selectedCount = getSelectedItemCount();
            if (selectedCount == 0 || getSelectableItemCount() == 0) {
                mActionModeListener.exitActionMode();
            } else {
                mActionModeListener.setSelectedCount(selectedCount, getSelectableItemCount(),
                        getSelectedItems());
            }
        }
    }

    public void toggleSelectAllState() {
        if (!mInSelectionMode) return;
        boolean isAll = getSelectedItemCount() >= getSelectableItemCount();

        int count = getItemCount();
        for (int i = 0; i < count; i++) {
            changeSelectedState(i, !isAll);
        }
        notifyDataSetChanged();
    }

    private void changeSelectedState(int position, boolean checked) {
        BaseListItem item = mItemsList.get(position);
        item.setChecked(checked);

        if (mInSelectionMode) {
            updateSelectedCountInActionMode();
        }
    }

    public void notifyItemChanged(BaseListItem item) {
        if (mItemsList.contains(item)) {
            int position = mItemsList.indexOf(item);
            notifyItemChanged(position);
        }
    }

    public void removeItem(BaseListItem item) {
        if (mItemsList.contains(item)) {
            int position = mItemsList.indexOf(item);
            mItemsList.remove(item);
            notifyItemRemoved(position);
        }

        if (mInSelectionMode) {
            updateSelectedCountInActionMode();
        }
    }

    private void setMediaItemPlayStatusListener(final int position) {
        if (BaseListItem.TYPE_MEDIA_ITEM != getItemViewType(position)) {
            return;
        }

        BaseListItem baseItem = mItemsList.get(position);
        if ((baseItem == null) || !(baseItem instanceof MediaItem)) {
            return;
        }

        ((MediaItem) baseItem).setItemPlayStatusListener(
                new MediaItem.ItemPlayStatusListener() {

                    @Override
                    public void onPlayStatusChanged(MediaItem.PlayStatus status) {
                        FileListRecyclerAdapter.this.notifyItemChanged(position);
                    }
                }
        );
    }
}
