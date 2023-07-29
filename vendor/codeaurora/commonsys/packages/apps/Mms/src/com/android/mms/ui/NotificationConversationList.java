/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

package com.android.mms.ui;

import android.app.ActionBar;
import android.database.sqlite.SQLiteException;
import android.database.sqlite.SqliteWrapper;
import android.os.Bundle;
import android.provider.Telephony.Threads;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.TextView;

import com.android.mms.R;
import com.android.mms.data.Conversation;
import com.android.mms.ui.MessageUtils;
import com.android.mmswrapper.TelephonyWrapper;

import java.util.HashSet;

public class NotificationConversationList extends ConversationList {
    private TextView mEmptyView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setTitle(R.string.notification_message_label);
        mEmptyView = (TextView)findViewById(R.id.empty);

        ActionBar actionBar = getActionBar();
        actionBar.setDisplayShowHomeEnabled(false);
        actionBar.setDisplayHomeAsUpEnabled(true);
        actionBar.setHomeButtonEnabled(true);
    }

    @Override
    public void startAsyncQuery() {
        try {
            mEmptyView.setText(R.string.loading_conversations);

            Conversation.startQuery(mQueryHandler, THREAD_LIST_QUERY_TOKEN,
                    TelephonyWrapper.NOTIFICATION + "=1" + " and " + NOT_OBSOLETE);
            Conversation.startQuery(mQueryHandler, UNREAD_THREADS_QUERY_TOKEN,
                    Threads.READ + "=0" + " and " + NOT_OBSOLETE);
        } catch (SQLiteException e) {
            SqliteWrapper.checkSQLiteException(this, e);
        }
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case android.R.id.home:
                finish();
                return true;
            case R.id.action_mark_as_read:
                MessageUtils.markAsReadForNotificationMessage(this);
                return true;
            default:
                return super.onOptionsItemSelected(item);
        }
    }

    @Override
    public boolean onPrepareOptionsMenu(Menu menu) {
        super.onPrepareOptionsMenu(menu);

        MenuItem item = menu.findItem(R.id.action_mark_as_read);
        if (item != null) {
            HashSet<Long> unreadIds = MessageUtils.getUnreadNotificationThreadIds(this);
            int unreadNum = unreadIds == null ? 0 : unreadIds.size();
            item.setVisible(unreadNum > 0 ? true : false);
        }

        item = menu.findItem(R.id.action_change_to_folder_mode);
        if (item != null) {
            item.setVisible(false);
        }

        item = menu.findItem(R.id.action_memory_status);
        if (item != null) {
            item.setVisible(false);
        }

        return true;
    }

    @Override
    protected void initCreateNewMessageButton() {
        return; // Would be hide floating button (create new conversation button).
    }

    @Override
    public void createNewMessage() {
        startActivity(ComposeMessageActivity.createIntent(this, 0,
                NotificationConversationList.class.getSimpleName()));
    }

    @Override
    public void openThread(long threadId) {
        startActivity(ComposeMessageActivity.createIntent(this, threadId,
                NotificationConversationList.class.getSimpleName()));
    }
}
