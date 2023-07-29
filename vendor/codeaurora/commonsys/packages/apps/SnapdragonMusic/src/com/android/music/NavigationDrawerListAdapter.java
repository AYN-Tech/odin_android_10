/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.

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

package com.android.music;

import java.util.ArrayList;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.ImageView;
import android.widget.RelativeLayout;
import android.widget.TextView;

public class NavigationDrawerListAdapter extends BaseAdapter {

    private int curTab = 0;
    Context mContext;
    ArrayList<String> mListItemNames = new ArrayList<String>();
    ArrayList<Integer> mListItemIcons = new ArrayList<Integer>();

    public NavigationDrawerListAdapter(Context context) {
        mContext = context;
        mListItemNames.add(context.getResources().getString(
                R.string.artists_title));
        mListItemNames.add(context.getResources().getString(
                R.string.albums_title));
        mListItemNames.add(context.getResources().getString(
                R.string.tracks_title));

        if (MusicUtils.isGroupByFolder()) {
            mListItemNames.add(context.getResources().getString(
                R.string.folders_title));
        }
        mListItemNames.add(context.getResources().getString(
                R.string.playlists_title));

        mListItemIcons.add(R.drawable.artists);
        mListItemIcons.add(R.drawable.albums);
        mListItemIcons.add(R.drawable.songs);

        if (MusicUtils.isGroupByFolder()) {
            mListItemIcons.add(R.drawable.ic_folder);
        }
        mListItemIcons.add(R.drawable.playlist);
    }

    @Override
    public int getCount() {
        return mListItemNames.size();
    }

    @Override
    public Object getItem(int position) {
        return mListItemNames.get(position);
    }

    @Override
    public long getItemId(int position) {
        return 0;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        View view;

        if (convertView == null) {
            LayoutInflater inflater = (LayoutInflater) mContext
                    .getSystemService(Context.LAYOUT_INFLATER_SERVICE);
            view = inflater.inflate(
                    com.android.music.R.layout.drawer_list_item, null);
        } else {
            view = convertView;
        }

        TextView titleView = (TextView) view.findViewById(R.id.title);
        ImageView iconView = (ImageView) view.findViewById(R.id.icon);
        RelativeLayout layout = (RelativeLayout) view.findViewById(R.id.layout);

        titleView.setText(mListItemNames.get(position));
        iconView.setImageResource(mListItemIcons.get(position));

        if (curTab == position) {
            view.setBackgroundResource(R.drawable.back);
        } else {
            view.setBackgroundColor(android.R.color.transparent);
        }

        return view;
    }

    public void setClickPosition(int position) {
        // TODO Auto-generated method stub
        curTab = position;
    }
}