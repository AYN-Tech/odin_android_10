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

package com.codeaurora.music.lyric;

import android.content.Context;
import android.os.RemoteException;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.Button;

import com.android.music.IMediaPlaybackService;
import com.android.music.MediaPlaybackActivity;
import com.android.music.R;

import java.util.List;

public class LyricAdapter extends BaseAdapter {
    public static final String TAG = "LyricAdapter";
    private List<Sentence> mSentences;
    private boolean mShouldInvalidate = false;
    private int mSelection = -1;
    private int mCurrentSelection = -1;
    private Context mContext = null;

    public LyricAdapter(Context context) {
        mContext = context;
    }

    public void setLyric(Lyric lyric) {
        if (lyric != null) {
            mSentences = lyric.mSentenceList;
        }
        mSelection = -1;
        mCurrentSelection = -1;
        mShouldInvalidate = true;
        notifyDataSetChanged();
    }

    @Override
    public int getCount() {
        return mSentences == null ? 0 : mSentences.size();
    }

    @Override
    public Object getItem(int position) {
        return mSentences.get(position);
    }

    @Override
    public long getItemId(int position) {
        return position;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        ViewHolder viewHolder;
        if (convertView == null) {
            convertView = LayoutInflater.from(mContext).inflate(R.layout.item_lyric, null);
            viewHolder = new ViewHolder();
            viewHolder.mSentenceTv =
                    (Button) convertView.findViewById(R.id.btn_focused_lyric_sentence);
            viewHolder.mSentenceBtn = (Button) convertView.findViewById(R.id.btn_lyric_sentence);
            viewHolder.mSentenceBtn.setOnClickListener(mClickListener);
            convertView.setTag(viewHolder);
        } else {
            viewHolder = (ViewHolder) convertView.getTag();
        }

        Sentence sentence = mSentences.get(position);

        viewHolder.mSentenceTv.setText(sentence.getContent());
        viewHolder.mSentenceBtn.setText(sentence.getContent());

        if (position == mSelection) {
            viewHolder.mSentenceBtn.setVisibility(View.GONE);
            viewHolder.mSentenceTv.setVisibility(View.VISIBLE);
            mCurrentSelection = mSelection;
        } else {
            viewHolder.mSentenceBtn.setVisibility(View.VISIBLE);
            viewHolder.mSentenceTv.setVisibility(View.GONE);
        }

        viewHolder.mSentenceBtn.setTag(sentence);

        return convertView;
    }

    public void setTime(long time) {
        if (mSentences == null || mSentences.size() == 0) {
            return;
        }

        for (int i = 0; i < mSentences.size(); i++) {
            Sentence sentence = mSentences.get(i);
            if (sentence.isInTime(time)) {
                if (i != mSelection) {
                    mSelection = i;
                    notifyDataSetChanged();
                }
                return;
            }
        }
    }

    static class ViewHolder {
        Button mSentenceTv;
        Button mSentenceBtn;
    }

    public int getSelection() {
        return mSelection;
    }

    public void release() {
        mSentences = null;
    }

    private View.OnClickListener mClickListener = new View.OnClickListener() {
        @Override
        public void onClick(View v) {
            Sentence sentence = (Sentence) v.getTag();
            if (sentence != null) {
                if (mContext instanceof MediaPlaybackActivity) {
                    IMediaPlaybackService service =
                            ((MediaPlaybackActivity) mContext).getService();
                    try {
                        if (service != null) {
                            service.seek(sentence.getFromTime());
                        }
                    } catch (RemoteException e) {
                    }
                }
                mShouldInvalidate = true;
            }
        }
    };

    public void setInvalidate() {
        mShouldInvalidate = true;
    }

    public boolean shouldInvalidate() {
        boolean ret = false;
        if (mCurrentSelection != mSelection) {
            ret = true;
        } else if (mShouldInvalidate) {
            ret = true;
        }
        return ret;
    }

    public void invalidateComplete() {
        mShouldInvalidate = false;
    }
}
