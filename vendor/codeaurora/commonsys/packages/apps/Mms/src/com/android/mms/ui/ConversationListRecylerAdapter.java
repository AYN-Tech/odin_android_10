/*
* Copyright (c) 2016, The Linux Foundation. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
* * Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
* * Redistributions in binary form must reproduce the above
* copyright notice, this list of conditions and the following
* disclaimer in the documentation and/or other materials provided
* with the distribution.
* * Neither the name of The Linux Foundation nor the names of its
* contributors may be used to endorse or promote products derived
* from this software without specific prior written permission.
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

import android.content.Context;
import android.database.Cursor;
import android.support.v7.widget.RecyclerView;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import com.android.mms.R;
import com.android.mms.data.Conversation;

public class ConversationListRecylerAdapter extends CursorRecyclerAdapter
        <ConversationListRecylerAdapter.ConversationListViewHolder> {
    private static final String TAG = "CovRecylerAdapter";
    private final View.OnClickListener mViewClickListener;
    private final View.OnLongClickListener mViewLongClickListener;

    public ConversationListRecylerAdapter(final Context context, final Cursor cursor,
            final View.OnClickListener viewClickListener,
            final View.OnLongClickListener longClickListener) {
        super(context, cursor, 0);
        mViewClickListener = viewClickListener;
        mViewLongClickListener = longClickListener;
        setHasStableIds(true);
    }

    @Override
    public void bindViewHolder(ConversationListViewHolder holder, Context context, Cursor cursor) {
        if (!(holder.mView instanceof ConversationListItem) ||
                cursor == null || cursor.getPosition() < 0) {
            Log.e(TAG, "Unexpected bindViewHolder: " + holder.mView);
            return;
        }
        ConversationListItem headerView = (ConversationListItem) holder.mView;
        Conversation conv = Conversation.from(context, cursor);
        headerView.bind(context, conv);
    }

    @Override
    public ConversationListViewHolder createViewHolder(Context context, ViewGroup parent,
            int viewType) {
        final LayoutInflater layoutInflater = LayoutInflater.from(context);
        final ConversationListItem itemView =
                (ConversationListItem) layoutInflater.inflate(
                        R.layout.conversation_list_item, parent, false);
        return new ConversationListViewHolder(itemView,
                mViewClickListener, mViewLongClickListener);
    }

    /**
     * ViewHolder that holds a ConversationListItem.
     */
    public static class ConversationListViewHolder extends RecyclerView.ViewHolder {
        final ConversationListItem mView;

        public ConversationListViewHolder(final ConversationListItem itemView,
                final View.OnClickListener viewClickListener,
                final View.OnLongClickListener viewLongClickListener) {
            super(itemView);
            mView = itemView;
            mView.setOnClickListener(viewClickListener);
            mView.setOnLongClickListener(viewLongClickListener);
        }
    }

}
