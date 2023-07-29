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

import android.view.View;

import com.android.soundrecorder.filelist.listitem.BaseListItem;
import com.android.soundrecorder.util.FileUtils;
import com.android.soundrecorder.util.StorageUtils;
import com.android.soundrecorder.R;

public class FolderItemViewHolder extends BaseViewHolder {
    public FolderItemViewHolder(View itemView, int rootLayoutId) {
        super(itemView, rootLayoutId);
    }

    @Override
    public void setItem(BaseListItem item) {
        super.setItem(item);

        String title = FileUtils.getLastFileName(item.getPath(), false);
        if(title.equals(StorageUtils.FM_RECORDING_FOLDER_NAME)){
            setTitle(mRootView.getContext().getApplicationContext().getResources()
                    .getString(R.string.file_list_FM));
        }
        else if(title.equals(StorageUtils.CALL_RECORDING_FOLDER_NAME)){
            setTitle(mRootView.getContext().getApplicationContext().getResources()
                    .getString(R.string.file_list_call));
        }
        else{
            setTitle(title);
        }
    }
}
