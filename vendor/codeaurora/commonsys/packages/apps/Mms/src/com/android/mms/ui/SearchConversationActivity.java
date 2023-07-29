/*
* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

import android.content.ContentResolver;
import android.content.Intent;
import android.database.Cursor;
import android.database.sqlite.SQLiteException;
import android.net.Uri;
import android.os.Bundle;
import android.app.Activity;
import android.os.Handler;
import android.os.Message;
import android.support.v4.content.ContextCompat;
import android.widget.Toolbar;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.util.Log;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;

import java.util.Set;

import com.android.mms.R;
import com.android.mms.data.Conversation;
import com.android.mms.data.Conversation.ConversationQueryHandler;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import com.google.android.mms.pdu.PduHeaders;

public class SearchConversationActivity extends Activity implements View.OnClickListener {
    private static final String TAG = "SearchConvActivity";
    private static final boolean DEBUG = false;
    private static final int EVENT_QUERY_ADDRESS_DONE = 1;
    public  static final int SEARCH_LIST_QUERY_TOKEN = 2001;
    private static final int SEARCH_CONVERSATION_ID_QUERY_TOKEN = 2002;
    private static final Uri SEARCH_URI = Uri.parse("content://mms-sms/search-message");
    private static final Uri ADDRESS_URI = Uri.parse("content://mms-sms/address");


    private Toolbar mToolbar;
    private EditText mSearchView;
    private View mClearSearchView;
    private View mBackButton;
    private String mQueryString;

    private RecyclerView mRecyclerView;
    private RecyclerView.LayoutManager mLayoutManager;
    private ConversationListRecylerAdapter mAdapter;

    public ThreadListQueryHandler mQueryHandler;
    private MyHandler mHandler = new MyHandler();
    private QueryAddressTask mQueryTask = null;
    private static final long RESULT_FOR_ID_NOT_FOUND = -1L;
    // Escape character
    private static final char SEARCH_ESCAPE_CHARACTER = '!';

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.search_conversation_screen);
        initViews();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (mAdapter != null) {
            mAdapter.changeCursor(null);
        }
        if (mQueryTask != null) {
            mQueryTask.cancelQuery();
            mQueryTask = null;
        }
    }

    @Override
    public void onBackPressed() {
        final InputMethodManager imm = (InputMethodManager) getSystemService(INPUT_METHOD_SERVICE);
        final View currentFocus = getCurrentFocus();
        if (imm != null && currentFocus != null) {
            imm.hideSoftInputFromWindow(currentFocus.getWindowToken(), 0);
        }
        super.onBackPressed();
    }

    @Override
    public void onClick(View view) {
        switch (view.getId()) {
            case R.id.search_close_button:
                setQueryString(null);
                break;
            case R.id.search_back_button:
                onBackPressed();
                break;
            default:
                break;
        }

    }

    @Override
    protected void onRestart(){
        super.onRestart();
        updateViews(mQueryString, true);
    }

    private void initViews() {
        final int normalStatusBarColor = ContextCompat.getColor(
                this, R.color.primary_color_dark);
        this.getWindow().setStatusBarColor(normalStatusBarColor);
        mToolbar = (Toolbar) findViewById(R.id.toolbar);
        setActionBar(mToolbar);
        mToolbar.setBackgroundColor(getResources().getColor(
                R.color.searchbox_background_color));
        mAdapter = new ConversationListRecylerAdapter(this, null,
                // Sets the item click listener on the Recycler item views.
                new View.OnClickListener() {
                    @Override
                    public void onClick(final View v) {
                        final ConversationListItem messageView = (ConversationListItem) v;
                        handleMessageClick(messageView);
                    }
                },
                new View.OnLongClickListener() {
                    @Override
                    public boolean onLongClick(final View view) {
                        final ConversationListItem messageView = (ConversationListItem) view;
                        handleMessageClick(messageView);
                        return true;
                    }
                });
        mRecyclerView = (RecyclerView) findViewById(R.id.list);
        mRecyclerView.setHasFixedSize(true);
        mLayoutManager = new LinearLayoutManager(this);
        mRecyclerView.setLayoutManager(mLayoutManager);
        mRecyclerView.setAdapter(mAdapter);

        mClearSearchView = findViewById(R.id.search_close_button);
        mClearSearchView.setVisibility(View.GONE);
        mClearSearchView.setOnClickListener(this);

        mQueryTask = new QueryAddressTask(null);
        mSearchView = (EditText) findViewById(R.id.search_view);
        mSearchView.setHint(getString(R.string.search_hint));
        mQueryHandler = new ThreadListQueryHandler(getContentResolver());
        mSearchView.addTextChangedListener(new TextWatcher() {
            @Override
            public void onTextChanged(final CharSequence s, final int start, final int before,
                                      final int count) {
                updateViews(s, false);
            }

            @Override
            public void beforeTextChanged(final CharSequence s, final int start, final int count,
                                          final int after) {
            }

            @Override
            public void afterTextChanged(final Editable s) {
            }
        });
        mBackButton = findViewById(R.id.search_back_button);
        mBackButton.setOnClickListener(this);
    }

    private void handleMessageClick(final ConversationListItem itemView) {
        Conversation conversation = itemView.getConversation();
        long threadId = conversation.getThreadId();
        logD(TAG, "handleMessageClick:" + threadId);
        Intent intent = ComposeMessageActivity.createIntent(this, threadId,
                SearchConversationActivity.class.getSimpleName());
        startActivity(intent);
    }

    private void updateViews(CharSequence queryString, boolean isRestart) {
        logD(TAG, "updateViews:" + queryString + "| isRestart="+isRestart);
        if (null == queryString || TextUtils.isEmpty(queryString)) {
            if (null != mClearSearchView) {
                mClearSearchView.setVisibility(View.GONE);
            }
            if (null != mAdapter) {
                mAdapter.changeCursor(null);
            }
        } else {
            if (null != mClearSearchView) {
                mClearSearchView.setVisibility(View.VISIBLE);
            }
            if(!isRestart){
                if (queryString.equals(mQueryString)) {
                    return;
                }
                mQueryString = queryString.toString();
            }
            if (queryString.length() > 0) {
                if (null == mQueryTask) {
                    mQueryTask = new QueryAddressTask(mQueryString);
                }
                mQueryTask.requestQuery(mQueryString);
            }
        }
    }

    public void setQueryString(String query) {
        mQueryString = query;
        if (mSearchView != null) {
            mSearchView.setText(query);
            mSearchView.setSelection(mSearchView.getText() == null ?
                    0 : mSearchView.getText().length());
        }
    }

    private class MyHandler extends Handler {
        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case EVENT_QUERY_ADDRESS_DONE:
                    if (null != mQueryTask) {
                        String address = (String) msg.obj;
                        mQueryTask.preHandleQuery(address);
                    } else {
                        Log.d(TAG, "!!error: handleMessage null mQueryTask");
                        mQueryTask = new QueryAddressTask(mQueryString);
                        mQueryTask.requestQuery(mQueryString);
                    }
                    break;
                case SEARCH_CONVERSATION_ID_QUERY_TOKEN:
                    String threadIds = (String)msg.obj;
                    logD(TAG, "SEARCH_CONVERSATION_ID_QUERY_TOKEN threadIds:" + threadIds
                            +"; searchString=" + mQueryString);
                    Uri queryUri = SEARCH_URI.buildUpon().appendQueryParameter(
                            "thread_ids", threadIds).build()
                            .buildUpon().appendQueryParameter(
                                    "key_str", addEscapeCharacter(mQueryString)).build();
                    Conversation.startConversationQuery(mQueryHandler,
                            SEARCH_LIST_QUERY_TOKEN, null, queryUri);
                    break;
                default:
                    break;
            }
        }
    }

    private String addEscapeCharacter(String keyStr) {
        if (keyStr == null) {
            return keyStr;
        }
        if (keyStr.contains("%") ||
                keyStr.contains(String.valueOf(SEARCH_ESCAPE_CHARACTER))) {
            StringBuilder searchKeyStrBuilder = new StringBuilder();
            int keyStrLen = keyStr.length();
            for (int i = 0; i < keyStrLen; i++) {
                if (keyStr.charAt(i) == '%' ||
                        keyStr.charAt(i) == SEARCH_ESCAPE_CHARACTER) {
                    searchKeyStrBuilder.append(SEARCH_ESCAPE_CHARACTER);
                    searchKeyStrBuilder.append(keyStr.charAt(i));
                    continue;
                }
                searchKeyStrBuilder.append(keyStr.charAt(i));
            }
            return searchKeyStrBuilder.toString();
        }
        return keyStr;
    }

    private class QueryAddressTask {
        private static final int STATUS_INIT = 1;
        private static final int STATUS_QUERYING = 2;
        private static final int STATUS_DONE = 3;
        private static final int STATUS_ERR = 0;
        private int mStatus = STATUS_ERR;
        private boolean isNewQuery = false;
        private boolean mCancelQuery = false;
        private String mQuery = null;
        private String mNewQuery = null;

        QueryAddressTask(String query) {
            logD(TAG, "init QueryAddressTask query=" + query);
            mStatus = STATUS_INIT;
            mQuery = query;
            mNewQuery = query;
        }

        public void requestQuery(String query) {
            logD(TAG, "requestQuery:" + query + " |status=" + mStatus);
            if (query == null) {
                return;
            }
            mNewQuery = query;
            isNewQuery = true;
            switch (mStatus) {
                case STATUS_INIT:
                    handleQuery();
                    break;
                case STATUS_QUERYING:
                    break;
                case STATUS_DONE:
                    handleQuery();
                    break;
                default:
                    break;
            }
        }

        /**
         * preHandleQuery should only be called by handler
         *
         * @param address contact address
         */
        public void preHandleQuery(String address) {
            logD(TAG, "preHandleQuery status:" + mStatus + " |address:" + address);
            switch (mStatus) {
                case STATUS_INIT:
                    handleQuery();
                    break;
                case STATUS_QUERYING:
                    break;
                case STATUS_DONE:
                    logD(TAG, "preHandleQuery mQuery=" + mQuery
                            + " |mNewQuery=" + mNewQuery + " |isNewQuery=" + isNewQuery
                            + " |address=" + address);
                    if (isNewQuery && !TextUtils.isEmpty(mNewQuery)
                            && !TextUtils.equals(mQuery, mNewQuery)) {
                        handleQuery();
                    } else if (null != mQuery && !TextUtils.isEmpty(mQuery.trim())){
                        startAsyncQuery(address);
                    }
                    break;
                case STATUS_ERR:
                    logD(TAG, "Query status error");
                    break;
                default:
                    break;
            }
        }

        private void handleQuery() {
            isNewQuery = false;
            mQuery = mNewQuery;
            logD(TAG, "handleQuery isNewQuery:" + isNewQuery + " |mQuery:" + mQuery);
            new Thread() {
                @Override
                public void run() {
                    if (mCancelQuery) {
                        return;
                    }
                    mStatus = STATUS_QUERYING;
                    String contactAddress = MessageUtils.getAddressByName(
                            getApplicationContext(), mQuery);
                    mStatus = STATUS_DONE;
                    if (!mCancelQuery) {
                        Message msg = mHandler.obtainMessage(EVENT_QUERY_ADDRESS_DONE);
                        msg.obj = contactAddress;
                        mHandler.sendMessage(msg);
                    } else {
                        Log.d(TAG, "handleQuery cancelled!");
                    }
                }
            }.start();
        }

        public void cancelQuery() {
            logD(TAG, "cancelQuery");
            mCancelQuery = true;
        }

        private void startAsyncQuery(String contactAddress) {
            if (mCancelQuery) {
                return;
            }
            try {
                int searchMode = MessageUtils.SEARCH_MODE_CONTENT;
                mQueryHandler.cancelOperation(SEARCH_LIST_QUERY_TOKEN);

                if (!TextUtils.isEmpty(contactAddress)) {
                    searchMode = MessageUtils.SEARCH_MODE_NAME;
                }
                logD(TAG, "startAsyncQuery searchMode=" + searchMode + " |mQuery:"
                        + mQuery + " |contactAddress=" + contactAddress);
                Uri queryUri = SEARCH_URI.buildUpon().appendQueryParameter(
                        "search_mode", Integer.toString(searchMode)).build()
                        .buildUpon().appendQueryParameter(
                                "key_str", mQuery).build()
                        .buildUpon().appendQueryParameter(
                                "contact_addr", contactAddress).build();
                queryThreadId(contactAddress);
            } catch (SQLiteException e) {
                Log.e(TAG, "startAsyncQuery Error:", e);
            }
        }
    }

    private static final String DEFAULT_STRING_ZERO = "0";

    private void queryThreadId(String contactAddress) {
        new Thread(new Runnable(){
            public void run(){
                if (null != contactAddress) {
                    String threadId = getThreadIdByAddress(contactAddress);
                    Message msg = mHandler.obtainMessage(SEARCH_CONVERSATION_ID_QUERY_TOKEN);
                    msg.obj = threadId;
                    mHandler.sendMessage(msg);
                }
            }
        }).start();
    }
    private String getThreadIdByAddress(String contactAddress) {
        long[] threadIdSet = getSortedSet(getThreadIdsByAddressList(contactAddress.split(",")));
        String threadIdString = getCommaSeparatedId(threadIdSet);
        if (TextUtils.isEmpty(threadIdString)) {
            threadIdString = DEFAULT_STRING_ZERO;
        }
        logD(TAG, "getThreadIdByAddress=" + threadIdString);
        return threadIdString;
    }

    private long[] getSortedSet(Set<Long> numbers) {
        int size = numbers.size();
        long[] result = new long[size];
        int i = 0;

        for (Long number : numbers) {
            result[i++] = number;
        }

        if (size > 1) {
            Arrays.sort(result);
        }

        return result;
    }

    private Set<Long> getThreadIdsByAddressList(String[] addresses) {
        int count = addresses.length;
        Set<Long> result = new HashSet<Long>(count);

        for (int i = 0; i < count; i++) {
            String address = addresses[i];
            if (address != null && !address.equals(PduHeaders.FROM_INSERT_ADDRESS_TOKEN_STR)) {
                long id = getSingleThreadId(address);
                if (id != RESULT_FOR_ID_NOT_FOUND) {
                    result.add(id);
                } else {
                    Log.e(TAG, "Address ID not found for: " + address);
                }
            }
        }
        return result;
    }

    private String getCommaSeparatedId(long[] threadIds) {
        int size = threadIds.length;
        StringBuilder buffer = new StringBuilder();

        for (int i = 0; i < size; i++) {
            if (i != 0) {
                buffer.append(',');
            }
            buffer.append(String.valueOf(threadIds[i]));
        }
        return buffer.toString();
    }

    private long getSingleThreadId(String address) {
        Uri uri = ADDRESS_URI.buildUpon().appendQueryParameter(
                "address_info", address).build();

        logD(TAG, "getSingleThreadId uri="+uri);
        Cursor cursor = null;
        try {
            cursor = getContentResolver().query(uri, null, null, null, null);
            if (cursor == null || cursor.getCount() == 0) {
                return 0;
            }
            if (cursor.moveToFirst()) {
                long threadId = cursor.getLong(0);
                logD(TAG, "getSingleThreadId threadId="+threadId);
                return (threadId >= 0) ? threadId : 0;
            }
            return 0;
        } finally {
           if (cursor != null) {
               cursor.close();
           }
        }
    }

    private final class ThreadListQueryHandler extends ConversationQueryHandler {
        public ThreadListQueryHandler(ContentResolver contentResolver) {
            super(contentResolver);
        }

        @Override
        protected void onQueryComplete(int token, Object cookie, Cursor cursor) {
            if (SearchConversationActivity.this.isFinishing()) {
                Log.w(TAG, "onQueryComplete activity finished.");
                if (null != cursor) {
                    cursor.close();
                    return;
                }
            }
            logD(TAG, "onQueryComplete:" + token);
            switch (token) {
                case SEARCH_LIST_QUERY_TOKEN:
                    if (cursor != null) {
                        logD(TAG, "cursor count: " + cursor.getCount());
                    }
                    mAdapter.changeCursor(cursor);
                    break;
                default:
                    logD(TAG, "onQueryComplete unknown token: " + token);
                    if (null != cursor) {
                        cursor.close();
                    }
                    break;
            }
        }
    }


    private void logD(String tag, String msg) {
        if (DEBUG) {
            Log.d(tag, msg);
        }
    }

}
