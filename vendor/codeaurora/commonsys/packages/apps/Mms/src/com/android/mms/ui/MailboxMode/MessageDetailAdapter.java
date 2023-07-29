/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
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
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

package com.android.mms.ui;

import java.io.File;
import java.io.FileInputStream;
import java.util.ArrayList;

import android.content.Context;
import android.database.Cursor;
import android.provider.Telephony.Sms;
import android.support.v4.view.PagerAdapter;
import android.support.v4.view.ViewPager;
import android.text.method.HideReturnsTransformationMethod;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;
import com.android.mms.R;

public class MessageDetailAdapter extends PagerAdapter {


    private Context mContext;
    private Cursor mCursor;
    private LayoutInflater mInflater;
    private float mBodyFontSize;
    private ArrayList<TextView> mScaleTextList;

    public MessageDetailAdapter(Context context, Cursor cursor) {
        mContext = context;
        mCursor = cursor;
        mInflater = LayoutInflater.from(context);
        mBodyFontSize = MessageUtils.getTextFontSize(context);
    }

    @Override
    public Object instantiateItem(ViewGroup view, int position) {
        mCursor.moveToPosition(position);
        View content = mInflater.inflate(R.layout.message_detail_content, view, false);

        final TextView bodyText = (TextView) content.findViewById(R.id.textViewBody);

        bodyText.setText(mCursor.getString(mCursor.getColumnIndexOrThrow(Sms.BODY)));
        bodyText.setTextSize(TypedValue.COMPLEX_UNIT_PX, mBodyFontSize);
        bodyText.setTransformationMethod(HideReturnsTransformationMethod.getInstance());
        bodyText.setTextIsSelectable(true);
        bodyText.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                MessageUtils.onMessageContentClick(mContext, bodyText);
            }
        });

        TextView detailsText = (TextView) content.findViewById(R.id.textViewDetails);
        detailsText.setText(MessageUtils.getTextMessageDetails(mContext, mCursor));
        view.addView(content);

        return content;
    }

    @Override
    public void destroyItem(ViewGroup container, int position, Object object) {
        ((ViewPager) container).removeView((View) object);
    }

    @Override
    public int getItemPosition(Object object) {
        return POSITION_NONE;
    }

    @Override
    public int getCount() {
        return mCursor != null ? mCursor.getCount() : 0;
    }

    @Override
    public boolean isViewFromObject(View view, Object object) {
        return view.equals(object);
    }

    @Override
    public void setPrimaryItem(View container, int position, Object object) {
        TextView currentBody = (TextView) container.findViewById(R.id.textViewBody);
        if (mScaleTextList.size() > 0) {
            mScaleTextList.clear();
        }
        mScaleTextList.add(currentBody);
    }

    public void setBodyFontSize(float currentFontSize) {
        mBodyFontSize = currentFontSize;
    }

    public void setScaleTextList(ArrayList<TextView> scaleTextList) {
        mScaleTextList = scaleTextList;
    }
}
