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
import android.widget.TextView;

import com.android.soundrecorder.R;
import com.android.soundrecorder.filelist.listitem.BaseListItem;
import com.android.soundrecorder.filelist.listitem.MediaItem;
import com.android.soundrecorder.filelist.ui.WaveIndicator;
import com.android.soundrecorder.util.Utils;

import java.text.SimpleDateFormat;
import java.util.Date;

public class MediaItemViewHolder extends BaseViewHolder {
    private TextView mDateModifiedView;
    private TextView mDurationView;
    private WaveIndicator mWaveIndicator;
    private SimpleDateFormat mDateFormatter;

    public MediaItemViewHolder(View itemView, int rootLayoutId) {
        super(itemView, rootLayoutId);
        mDateModifiedView = (TextView)itemView.findViewById(R.id.list_item_date_modified);
        mDurationView = (TextView)itemView.findViewById(R.id.list_item_duration);
        mWaveIndicator = (WaveIndicator) itemView.findViewById(R.id.list_wave_indicator);

        mDateFormatter = new SimpleDateFormat(itemView.getResources().getString(
                R.string.list_item_date_modified_format), java.util.Locale.US);
    }

    @Override
    public void setItem(BaseListItem item) {
        super.setItem(item);
        if (item instanceof MediaItem) {
            long dateModified = ((MediaItem)item).getDateModified();
            Date date = new Date(dateModified);
            mDateModifiedView.setText(mDateFormatter.format(date));

            long duration = ((MediaItem)item).getDuration() / 1000; // millisecond to second
            mDurationView.setText(Utils.timeToString(mDurationView.getContext(), duration));

            updateWaveIndicator(((MediaItem) item).getPlayStatus());
        }
    }

    private void updateWaveIndicator(MediaItem.PlayStatus status) {
        if (status == MediaItem.PlayStatus.PLAYING) {
            mWaveIndicator.setVisibility(View.VISIBLE);
            mWaveIndicator.animate(true);
        } else if (status == MediaItem.PlayStatus.PAUSE) {
            mWaveIndicator.setVisibility(View.VISIBLE);
            mWaveIndicator.animate(false);
        } else {
            mWaveIndicator.setVisibility(View.GONE);
            mWaveIndicator.animate(false);
        }
    }
}
