/*
 * Copyright (c) 2014, 2016-2017, The Linux Foundation. All rights reserved.
 * Not a Contribution.
 *
 * Copyright (C) 2012 The Android Open Source Project.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.mms.ui;

import java.util.ArrayList;

import android.app.ActionBar;
import android.app.AlertDialog;
import android.app.ListActivity;
import android.app.SearchManager;
import android.app.SearchableInfo;
import android.content.ActivityNotFoundException;
import android.content.AsyncQueryHandler;
import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.database.Cursor;
import android.database.sqlite.SQLiteException;
import android.database.sqlite.SqliteWrapper;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.preference.PreferenceManager;
import android.provider.Telephony.Mms;
import android.provider.Telephony.MmsSms;
import android.provider.Telephony.Sms;
import android.telephony.SubscriptionManager;
import android.text.TextUtils;
import android.util.Log;
import android.util.SparseBooleanArray;
import android.widget.AdapterView.OnItemLongClickListener;
import android.widget.AdapterView;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.SearchView;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;
import android.view.ActionMode;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;

import com.android.mms.data.Contact;
import com.android.mms.data.Conversation;
import com.android.mms.LogTag;
import com.android.mms.R;
import com.android.mms.transaction.MessagingNotification;
import com.android.mms.transaction.Transaction;
import com.android.mms.transaction.TransactionBundle;
import com.android.mms.transaction.TransactionService;
import com.android.mms.ui.MessageListAdapter;
import com.android.mms.ui.MessageUtils;
import com.android.mms.ui.PopupList;
import com.android.mms.ui.SearchActivityExtend;
import com.android.mms.ui.SelectionMenu;
import com.android.mms.util.DownloadManager;
import com.android.mms.util.DraftCache;
import com.android.mms.widget.MmsWidgetProvider;
import com.android.mmswrapper.SubscriptionManagerWrapper;
import com.google.android.mms.pdu.PduHeaders;

import static com.android.mms.ui.MessageListAdapter.MAILBOX_PROJECTION;
import static com.android.mms.ui.MessageListAdapter.COLUMN_MSG_TYPE;
import static com.android.mms.ui.MessageListAdapter.COLUMN_ID;
import static com.android.mms.ui.MessageListAdapter.COLUMN_MMS_ERROR_TYPE;
import static com.android.mms.ui.MessageListAdapter.COLUMN_MMS_MESSAGE_TYPE;
import static com.android.mms.ui.MessageListAdapter.COLUMN_THREAD_ID;
import static com.android.mms.ui.MessageListAdapter.COLUMN_MMS_SUBJECT;
import static com.android.mms.ui.MessageListAdapter.COLUMN_MMS_SUBJECT_CHARSET;
import static com.android.mms.ui.MessageListAdapter.COLUMN_SMS_ADDRESS;
import static com.android.mms.ui.MessageListAdapter.COLUMN_SMS_BODY;
import static com.android.mms.ui.MessageListAdapter.COLUMN_SUB_ID;
import static com.android.mms.ui.MessageListAdapter.COLUMN_SMS_DATE;
import static com.android.mms.ui.MessageListAdapter.COLUMN_SMS_READ;
import static com.android.mms.ui.MessageListAdapter.COLUMN_SMS_TYPE;
import static com.android.mms.ui.MessageListAdapter.COLUMN_SMS_STATUS;
import static com.android.mms.ui.MessageListAdapter.COLUMN_SMS_DATE_SENT;
import static com.android.mms.ui.MessageListAdapter.COLUMN_MMS_READ;
import static com.android.mms.ui.MessageListAdapter.COLUMN_MMS_MESSAGE_BOX;
import static com.android.mms.ui.MessageListAdapter.COLUMN_MMS_DELIVERY_REPORT;
import static com.android.mms.ui.MessageListAdapter.COLUMN_SMS_LOCKED;
import static com.android.mms.ui.MessageListAdapter.COLUMN_MMS_LOCKED;

/**
 * This activity provides a list view of MailBox-Mode.
 */
public class MailBoxMessageList extends ListActivity implements
        MailBoxMessageListAdapter.OnListContentChangedListener{
    private static final String TAG = "MailBoxMessageList";
    private static final String MAILBOX_URI = "content://mms-sms/mailbox/";
    private static final Uri SEARCH_URI = Uri.parse("content://mms-sms/search-message");
    private static final int MESSAGE_LIST_QUERY_TOKEN = 9001;
    private static final int MESSAGE_SEARCH_LIST_QUERY_TOKEN = 9002;

    // IDs of the spinner items for the box type, the values are based on original database.
    public static final int TYPE_INVALID = -1;
    public static final int TYPE_INBOX = 1;
    public static final int TYPE_SENTBOX = 2;
    public static final int TYPE_DRAFTBOX = 3;
    public static final int TYPE_OUTBOX = 4;
    // IDs of the spinner items for the slot type in DSDS
    private static final int TYPE_ALL_SLOT = 0;
    private static final int TYPE_SLOT_ONE = 1;
    private static final int TYPE_SLOT_TWO = 2;

    private static final String ORIGIN_SUB_ID = "origin_sub_id";
    private static final String NONE_SELECTED = "0";
    private static final String BOX_SPINNER_TYPE = "box_spinner_type";
    private static final String SLOT_SPINNER_TYPE = "slot_spinner_type";
    private final static String THREAD_ID = "thread_id";
    private final static String MESSAGE_ID = "message_id";
    private final static String MESSAGE_TYPE = "message_type";
    private final static String MESSAGE_BODY = "message_body";
    private final static String MESSAGE_SUBJECT = "message_subject";
    private final static String MESSAGE_SUBJECT_CHARSET = "message_subject_charset";
    private final static String NEED_RESEND = "needResend";
    private final static int DELAY_TIME = 500;
    private final static int MAIL_BOX_ID_INVALID = -1;
    private final static String COUNT_TEXT_DECOLLATOR_1 = "";
    private final static String COUNT_TEXT_DECOLLATOR_2 = "/";

    private boolean mIsPause = false;
    private boolean mQueryDone = true;
    private int mQueryBoxType = TYPE_INBOX;
    private int mQuerySlotType = TYPE_ALL_SLOT;
    private BoxMsgListQueryHandler mQueryHandler;
    private long mThreadId;
    private String mSmsWhereDelete = "";
    private String mMmsWhereDelete = "";
    private boolean mHasLockedMessage = false;
    private ArrayList<Long> mThreadIds = new ArrayList<Long>();

    private MailBoxMessageListAdapter mListAdapter = null;
    private Cursor mCursor;
    private final Object mCursorLock = new Object();
    private ListView mListView;
    private TextView mCountTextView;
    private TextView mMessageTitle;
    private View mSpinners;
    private Spinner mSlotSpinner = null;
    private Spinner mBoxSpinner;
    private ModeCallback mModeCallback = null;
    // mark whether comes into MultiChoiceMode or not.
    private boolean mMultiChoiceMode = false;
    private MenuItem mSearchItem;
    private SearchView mSearchView;
    private CharSequence mQueryText;
    private Handler mHandler;

    private CharSequence mSearchTitle;

    // add for obtain parameters from SearchActivityExtend
    private int mSearchModePosition = MessageUtils.SEARCH_MODE_CONTENT;
    private String mSearchKeyStr = "";
    private String mSearchDisplayStr = "";
    private int mMatchWhole = MessageUtils.MATCH_BY_ADDRESS;
    private boolean mIsInSearchMode;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (MessageUtils.checkPermissionsIfNeeded(this)) {
            return;
        }
        mQueryHandler = new BoxMsgListQueryHandler(getContentResolver());
        setContentView(R.layout.mailbox_list_screen);
        mSpinners = (View) findViewById(R.id.spinners);
        initSpinner();

        mListView = getListView();
        getListView().setItemsCanFocus(true);
        mModeCallback = new ModeCallback();
        ActionBar actionBar = getActionBar();
        actionBar.setDisplayHomeAsUpEnabled(false);
        setupActionBar();

        mHandler = new Handler();
        handleIntent(getIntent());
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        handleIntent(intent);
    }

    @Override
    public boolean onSearchRequested() {
        if (getResources().getBoolean(R.bool.config_classify_search)) {
            // block search entirely (by simply returning false).
            return false;
        }

        // if comes into multiChoiceMode,do not continue to enter search mode ;
        if (mSearchItem != null && !mMultiChoiceMode) {
            mSearchItem.expandActionView();
        }
        return true;
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        // We override this method to avoid restarting the entire
        // activity when the keyboard is opened (declared in
        // AndroidManifest.xml). Because the only translatable text
        // in this activity is "New Message", which has the full width
        // of phone to work with, localization shouldn't be a problem:
        // no abbreviated alternate words should be needed even in
        // 'wide' languages like German or Russian.
        closeContextMenu();
        super.onConfigurationChanged(newConfig);
    }

    @Override
    public void onListItemClick(ListView l, View v, int position, long id) {
        if (!mQueryDone) {
            return;
        }
        Cursor c = (Cursor) l.getAdapter().getItem(position);
        if (c == null) {
            return;
        }

        try {
            String type = c.getString(COLUMN_MSG_TYPE);
            long msgId = c.getLong(COLUMN_ID);
            long threadId = c.getLong(COLUMN_THREAD_ID);
            boolean isDraft = "sms".equals(type)
                    && (c.getInt(COLUMN_SMS_TYPE) == Sms.MESSAGE_TYPE_DRAFT);
            isDraft |= "mms".equals(type)
                    && (c.getInt(MessageListAdapter.COLUMN_MMS_MESSAGE_BOX)
                            == Mms.MESSAGE_BOX_DRAFTS);

            // If it's draft, launch ComposeMessageActivity.
            if (isDraft) {
                startActivity(ComposeMessageActivity.createIntent(this, threadId));
                return;
            } else if ("sms".equals(type)) {
                // If the message is a failed one, clicking it should reload it in the compose view,
                // regardless of whether it has links in it
                if (c.getInt(COLUMN_SMS_TYPE) == Sms.MESSAGE_TYPE_FAILED) {
                    Intent intent = new Intent(this, ComposeMessageActivity.class);
                    intent.putExtra(THREAD_ID, threadId);
                    intent.putExtra(MESSAGE_ID, msgId);
                    intent.putExtra(MESSAGE_TYPE, type);
                    intent.putExtra(MESSAGE_BODY, c.getString(COLUMN_SMS_BODY));
                    intent.putExtra(NEED_RESEND, true);
                    startActivity(intent);
                    return;
                }
                MessageUtils.showSmsMessageContent(
                        MailBoxMessageList.this, c.getLong(MessageListAdapter.COLUMN_ID));
            } else {
                int errorType = c.getInt(COLUMN_MMS_ERROR_TYPE);
                // Assuming the current message is a failed one, reload it into the compose view so
                // the user can resend it.
                if ( c.getInt(COLUMN_MMS_MESSAGE_BOX) == Mms.MESSAGE_BOX_OUTBOX
                        && (errorType >= MmsSms.ERR_TYPE_GENERIC_PERMANENT)) {
                    Intent intent = new Intent(this, ComposeMessageActivity.class);
                    intent.putExtra(THREAD_ID, threadId);
                    intent.putExtra(MESSAGE_ID, msgId);
                    intent.putExtra(MESSAGE_TYPE, type);
                    intent.putExtra(MESSAGE_SUBJECT, c.getString(COLUMN_MMS_SUBJECT));
                    intent.putExtra(MESSAGE_SUBJECT_CHARSET, c.getInt(COLUMN_MMS_SUBJECT_CHARSET));
                    intent.putExtra(NEED_RESEND, true);
                    startActivity(intent);
                    return;
                }

                Uri msgUri = ContentUris.withAppendedId(Mms.CONTENT_URI, msgId);
                int subId = c.getInt(COLUMN_SUB_ID);
                int mmsStatus = c.getInt(MessageListAdapter.COLUMN_MMS_STATUS);
                int downloadStatus = MessageUtils.getMmsDownloadStatus(mmsStatus);
                if (PduHeaders.MESSAGE_TYPE_NOTIFICATION_IND == c.getInt(COLUMN_MMS_MESSAGE_TYPE)) {
                    switch (downloadStatus) {
                        case DownloadManager.STATE_PRE_DOWNLOADING:
                        case DownloadManager.STATE_DOWNLOADING:
                            Toast.makeText(MailBoxMessageList.this, getString(R.string.downloading),
                                    Toast.LENGTH_LONG).show();
                            break;
                        case DownloadManager.STATE_UNKNOWN:
                        case DownloadManager.STATE_UNSTARTED:
                        case DownloadManager.STATE_TRANSIENT_FAILURE:
                        case DownloadManager.STATE_PERMANENT_FAILURE:
                        default:
                            showDownloadDialog(msgUri, subId);
                            break;
                    }
                } else {
                    MessageUtils.viewMmsMessageAttachment(MailBoxMessageList.this, msgUri, null,
                            new AsyncDialog(MailBoxMessageList.this));
                    int hasRead = c.getInt(COLUMN_MMS_READ);
                    if (hasRead == 0) {
                        markAsRead(msgUri);
                    }
                }
            }
        } catch (Exception e) {
            Log.e(TAG, "Open message error", e);
        }
    }

    private void showDownloadDialog(final Uri uri, final int subId) {
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setTitle(R.string.download);
        builder.setIconAttribute(android.R.attr.alertDialogIcon);
        builder.setCancelable(true);
        builder.setMessage(R.string.download_dialog_title);
        builder.setPositiveButton(R.string.yes, new OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        startDownloadAttachment(uri, subId);
                    }
                });
        builder.setNegativeButton(R.string.no, null);
        builder.show();
    }

    private void startDownloadAttachment(Uri uri, int subId) {
        Intent intent = new Intent(this, TransactionService.class);
        intent.putExtra(TransactionBundle.URI, uri.toString());
        intent.putExtra(TransactionBundle.TRANSACTION_TYPE,
                Transaction.RETRIEVE_TRANSACTION);
        intent.putExtra(Mms.SUBSCRIPTION_ID, subId); //destination subId
        intent.putExtra(ORIGIN_SUB_ID,
                SubscriptionManager.getDefaultDataSubscriptionId());
        startService(intent);

        DownloadManager.getInstance().markState(uri, DownloadManager.STATE_PRE_DOWNLOADING);
        Toast.makeText(MailBoxMessageList.this, getString(R.string.downloading),
                Toast.LENGTH_LONG).show();
    }

    private void markAsRead(final Uri msgUri) {
        new Thread(new Runnable() {
            public void run() {
                try {
                    ContentValues values = new ContentValues(2);
                    values.put(Mms.READ, MessageUtils.MESSAGE_READ);
                    values.put(Mms.SEEN, MessageUtils.MESSAGE_SEEN);
                    SqliteWrapper.update(MailBoxMessageList.this, getContentResolver(),
                            msgUri, values, null, null);
                    MessagingNotification.nonBlockingUpdateNewMessageIndicator(
                            MailBoxMessageList.this, MessagingNotification.THREAD_NONE,
                            false);
                } catch (Exception e) {
                    Log.e(TAG, "Update Read Error", e);
                }
            }
        }).start();
    }

    private void handleIntent(Intent intent) {
        mIsInSearchMode = intent.getBooleanExtra(MessageUtils.SEARCH_KEY, false);
        if (mIsInSearchMode) {
            mSearchTitle = intent.getStringExtra(MessageUtils.SEARCH_KEY_TITLE);
            mSearchModePosition = intent.getIntExtra(MessageUtils.SEARCH_KEY_MODE_POSITION,
                    MessageUtils.SEARCH_MODE_CONTENT);
            mSearchKeyStr = intent.getStringExtra(MessageUtils.SEARCH_KEY_KEY_STRING);
            mSearchDisplayStr = intent.getStringExtra(MessageUtils.SEARCH_KEY_DISPLAY_STRING);
            mMatchWhole = intent.getIntExtra(MessageUtils.SEARCH_KEY_MATCH_WHOLE,
                    MessageUtils.MATCH_BY_ADDRESS);
        } else {
            mQueryBoxType = intent.getIntExtra(MessageUtils.MAIL_BOX_ID,
                    PreferenceManager.getDefaultSharedPreferences(this).getInt(
                            BOX_SPINNER_TYPE, TYPE_INBOX));
            mBoxSpinner.setSelection(getSelectBoxIndex(mQueryBoxType));
        }

        if (mIsInSearchMode && mSearchTitle != null) {
            mMessageTitle.setText(mSearchTitle);
            mSpinners.setVisibility(View.GONE);
        } else {
            if (mQueryBoxType == TYPE_DRAFTBOX) {
                mSlotSpinner.setSelection(TYPE_ALL_SLOT);
                mSlotSpinner.setEnabled(false);
            }
            mListView.setMultiChoiceModeListener(mModeCallback);
        }

        // Cancel failed notification.
        MessageUtils.cancelFailedToDeliverNotification(intent, this);
        MessageUtils.cancelFailedDownloadNotification(intent, this);

        Conversation.markAllConversationsAsSeen(MailBoxMessageList.this);
    }

    @Override
    public void onResume() {
        super.onResume();
        mIsPause = false;
        startAsyncQuery();
        if (!mIsInSearchMode) {
            mListView.setChoiceMode(ListView.CHOICE_MODE_MULTIPLE_MODAL);
        }

        getListView().invalidateViews();
    }

    @Override
    public void onPause() {
        super.onPause();
        mIsPause = true;
        MessagingNotification.setCurrentlyDisplayedMsgType(TYPE_INVALID);
    }

    private int getSelectBoxIndex(int mailBoxId) {
        // Because box index starts from 0, while mailBoxId starts from 1.
        // We need to "-1" on mailBoxId to get the right mail box index.
        if(mailBoxId > TYPE_OUTBOX) {
            return TYPE_OUTBOX -1;
        }
        return mailBoxId - 1;
    }

    private void initSpinner() {
        final SharedPreferences sp = PreferenceManager.getDefaultSharedPreferences(this);
        mBoxSpinner = (Spinner) findViewById(R.id.box_spinner);
        mSlotSpinner = (Spinner) findViewById(R.id.slot_spinner);
        mBoxSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                int oldQueryType = mQueryBoxType;
                // position 0-3 means box: inbox, sent, draft, outbox
                mQueryBoxType = position + 1;
                if (mQueryBoxType > TYPE_OUTBOX) {
                    mQueryBoxType = TYPE_OUTBOX;
                }
                if (oldQueryType != mQueryBoxType) {
                    if (mQueryBoxType == TYPE_DRAFTBOX) {
                        mQuerySlotType = TYPE_ALL_SLOT;
                        mSlotSpinner.setSelection(TYPE_ALL_SLOT);
                        mSlotSpinner.setEnabled(false);
                    } else {
                        mSlotSpinner.setEnabled(true);
                    }
                    startAsyncQuery();
                    getListView().invalidateViews();
                }
                sp.edit().putInt(BOX_SPINNER_TYPE, mQueryBoxType).commit();
            }

            @Override
            public void onNothingSelected(AdapterView<?> arg0) {
                // do nothing
            }
        });

        if (MessageUtils.isMsimIccCardActive()) {
            mSlotSpinner.setPrompt(getResources().getString(R.string.slot_type_select));
            ArrayAdapter<CharSequence> slotAdapter = ArrayAdapter.createFromResource(this,
                    R.array.slot_type, android.R.layout.simple_spinner_item);
            slotAdapter.setDropDownViewResource(R.layout.simple_spinner_dropdown_item);
            mSlotSpinner.setAdapter(slotAdapter);
            mSlotSpinner.setSelection(sp.getInt(SLOT_SPINNER_TYPE, TYPE_ALL_SLOT));
            mSlotSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
                @Override
                public void onItemSelected(AdapterView<?> parent,
                                           View view, int position, long id) {
                    sp.edit().putInt(SLOT_SPINNER_TYPE, position).commit();
                    // position 0-2 means slotType: slot_all, slot_one, slot_two
                    int oldQuerySlotType = mQuerySlotType;
                    if (position > TYPE_SLOT_TWO) {
                        position = TYPE_ALL_SLOT;
                    }
                    mQuerySlotType = position;
                    if (oldQuerySlotType != mQuerySlotType) {
                        startAsyncQuery();
                        getListView().invalidateViews();
                    }
                }

                @Override
                public void onNothingSelected(AdapterView<?> arg0) {
                    // do nothing
                }
            });
        } else {
            mSlotSpinner.setVisibility(View.GONE);
        }

    }

    private void startAsyncQuery() {
        try {
            synchronized (mCursorLock) {
                setProgressBarIndeterminateVisibility(true);
                // FIXME: I have to pass the mQueryToken as cookie since the
                // AsyncQueryHandler.onQueryComplete() method doesn't provide
                // the same token as what I input here.
                mQueryDone = false;
                String selStr = null;
                if (mQuerySlotType == TYPE_SLOT_ONE) {
                    int subId = SubscriptionManagerWrapper.getSubId(MessageUtils.SUB1)[0];
                    selStr = "sub_id = " + subId;
                } else if (mQuerySlotType == TYPE_SLOT_TWO) {
                    int subId = SubscriptionManagerWrapper.getSubId(MessageUtils.SUB2)[0];
                    selStr = "sub_id = " + subId;
                }
                if (mIsInSearchMode) {
                    Uri queryUri = SEARCH_URI.buildUpon().appendQueryParameter(
                            "search_mode", Integer.toString(mSearchModePosition)).build()
                            .buildUpon().appendQueryParameter("key_str", mSearchKeyStr).build()
                            .buildUpon().appendQueryParameter("match_whole",
                                    Integer.toString(mMatchWhole)).build();

                    mQueryHandler.startQuery(MESSAGE_SEARCH_LIST_QUERY_TOKEN, 0,
                        queryUri, null, null, null, null);
                } else {
                    String mailboxUri = MAILBOX_URI + mQueryBoxType;

                    mQueryHandler.startQuery(MESSAGE_LIST_QUERY_TOKEN, 0,
                        Uri.parse(mailboxUri),
                        MAILBOX_PROJECTION, selStr,
                        null, "normalized_date DESC");
                }
            }
        } catch (SQLiteException e) {
            mQueryDone = true;
            SqliteWrapper.checkSQLiteException(this, e);
            mListView.setVisibility(View.VISIBLE);
        }
    }

    private void resetSearchView() {
        mHandler.removeCallbacks(resetSearchRunnable);
        mHandler.postDelayed(resetSearchRunnable, DELAY_TIME);
    }

    private Runnable resetSearchRunnable = new Runnable() {
        @Override
        public void run() {
            mSearchView.setQuery(mSearchView.getQuery(), false);
        }
    };

    private void showMessageCount(Cursor cursor) {
        mCountTextView.setVisibility(View.VISIBLE);
        if (mQueryBoxType == TYPE_INBOX) {
            mCountTextView.setText(COUNT_TEXT_DECOLLATOR_1
                    + getUnReadMessageCount(cursor)
                    + COUNT_TEXT_DECOLLATOR_2 + cursor.getCount());
        } else {
            mCountTextView.setText("" + cursor.getCount());
        }
    }

    private final class BoxMsgListQueryHandler extends AsyncQueryHandler {
        public BoxMsgListQueryHandler(ContentResolver contentResolver) {
            super(contentResolver);
        }

        @Override
        protected void onQueryComplete(int token, Object cookie, Cursor cursor) {
            synchronized (mCursorLock) {
                if (cursor != null) {
                    if (mCursor != null) {
                        mCursor.close();
                    }
                    mCursor = cursor;
                    TextView emptyView = (TextView) findViewById(R.id.emptyview);
                    if (mListAdapter == null) {
                        mListAdapter = new MailBoxMessageListAdapter(MailBoxMessageList.this,
                                MailBoxMessageList.this, cursor);
                        invalidateOptionsMenu();
                        MailBoxMessageList.this.setListAdapter(mListAdapter);
                        if (mIsInSearchMode) {
                            int count = cursor.getCount();
                            setMessageTitle(count);
                            if (count == 0) {
                                emptyView.setText(getString(R.string.search_empty));
                             }
                         } else if (cursor.getCount() == 0) {
                             mListView.setEmptyView(emptyView);
                         } else if (needShowCountNum(cursor)) {
                            showMessageCount(cursor);
                         }
                     } else {
                        mListAdapter.changeCursor(mCursor);
                        if (!getResources().getBoolean(R.bool.config_classify_search)) {
                            if (mSearchView != null && !mSearchView.isIconified()) {
                                resetSearchView();
                            }
                        }
                        if (needShowCountNum(cursor)) {
                            showMessageCount(cursor);
                        } else if (mIsInSearchMode) {
                            setMessageTitle(cursor.getCount());
                        } else {
                            mListView.setEmptyView(emptyView);
                            mCountTextView.setVisibility(View.INVISIBLE);
                        }
                    }
                } else {
                    if (LogTag.VERBOSE || Log.isLoggable(LogTag.APP, Log.VERBOSE)) {
                        Log.e(TAG, "Cannot init the cursor for the thread list.");
                    }
                    finish();
                }
            }
            setProgressBarIndeterminateVisibility(false);
            mQueryDone = true;
            MessagingNotification.setCurrentlyDisplayedMsgType(mQueryBoxType);
        }
    }

    private int getUnReadMessageCount(Cursor cursor) {
        int count = 0;
        while (cursor.moveToNext()) {
            if (cursor.getInt(COLUMN_SMS_READ) == 0
                    || cursor.getInt(COLUMN_MMS_READ) == 0) {
                count++;
            }
        }
        return count;
    }

    private boolean needShowCountNum(Cursor cursor) {
        return (cursor.getCount() > 0 && !mIsInSearchMode);
    }

    private void setMessageTitle(int count) {
        if (count > 0) {
            mMessageTitle.setText(getResources().getQuantityString(
                    R.plurals.search_results_title, count, count,
                    mSearchDisplayStr));
        } else {
            mMessageTitle.setText(getResources().getQuantityString(
                    R.plurals.search_results_title, 0, 0, mSearchDisplayStr));
        }
    }

    SearchView.OnQueryTextListener mQueryTextListener = new SearchView.OnQueryTextListener() {
        @Override
        public boolean onQueryTextSubmit(String query) {
            Intent intent = new Intent();
            intent.setClass(MailBoxMessageList.this, SearchActivity.class);
            intent.putExtra(SearchManager.QUERY, query);
            startActivity(intent);
            mSearchItem.collapseActionView();
            return true;
        }

        @Override
        public boolean onQueryTextChange(String newText) {
            return false;
        }
    };

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        if (mIsInSearchMode) {
            return true;
        }

        getMenuInflater().inflate(R.menu.conversation_list_menu, menu);
        mSearchItem = menu.findItem(R.id.search);
        mSearchView = (SearchView) mSearchItem.getActionView();
        if (mSearchView != null) {
            mSearchView.setOnQueryTextListener(mQueryTextListener);
            mSearchView.setQueryHint(getString(R.string.search_hint));
            mSearchView.setIconifiedByDefault(true);
            SearchManager searchManager = (SearchManager) getSystemService(Context.SEARCH_SERVICE);

            if (searchManager != null) {
                SearchableInfo info = searchManager.getSearchableInfo(getComponentName());
                mSearchView.setSearchableInfo(info);
            }
        }
        MenuItem item = menu.findItem(R.id.action_change_to_folder_mode);
        if (item != null) {
            item.setVisible(false);
        }

        return true;
    }

    @Override
    public boolean onPrepareOptionsMenu(Menu menu) {
        MenuItem item = menu.findItem(R.id.action_delete_all);
        if (item != null) {
            item.setVisible(false);
        }
        item = menu.findItem(R.id.action_compose_new);
        if (item != null) {
            item.setVisible(true);
        }
        // if mQueryText is not null,so restore it.
        if (mQueryText != null && mSearchView != null) {
            mSearchView.setQuery(mQueryText, false);
        }

        if (!LogTag.DEBUG_DUMP) {
            item = menu.findItem(R.id.action_debug_dump);
            if (item != null) {
                item.setVisible(false);
            }
        }
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.search:
                if (getResources().getBoolean(R.bool.config_classify_search)) {
                    Intent searchintent = new Intent(this, SearchActivityExtend.class);
                    startActivityIfNeeded(searchintent, -1);
                    break;
                }
                return true;
            case R.id.action_compose_new:
                startActivity(ComposeMessageActivity.createIntent(this, 0));
                break;
            case R.id.action_settings:
                Intent intent = new Intent(this, MessagingPreferenceActivity.class);
                startActivityIfNeeded(intent, -1);
                break;
            case R.id.action_change_to_conversation_mode:
                Intent modeIntent = new Intent(this, ConversationList.class);
                startActivityIfNeeded(modeIntent, -1);
                MessageUtils.setMailboxMode(false);
                finish();
                break;
            case R.id.action_memory_status:
                MessageUtils.showMemoryStatusDialog(this);
                break;
            case R.id.action_cell_broadcasts:
                try {
                    startActivity(MessageUtils.getCellBroadcastIntent());
                } catch (ActivityNotFoundException e) {
                    Log.e(TAG, "ActivityNotFoundException for CellBroadcastListActivity");
                }
                break;
            default:
                return true;
        }
        return true;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        mHasLockedMessage = false;
        mIsPause = true;
        if (mCursor != null) {
            mCursor.close();
        }
        if (mListAdapter != null) {
            mListAdapter.changeCursor(null);
        }
        MessageUtils.removeDialogs();
        Contact.clearListener();
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_SEARCH && !mMultiChoiceMode) {
            if (getResources().getBoolean(R.bool.config_classify_search)) {
                Intent searchintent = new Intent(this, SearchActivityExtend.class);
                startActivityIfNeeded(searchintent, -1);
            } else if (mSearchView != null) {
                mSearchView.setIconified(false);
            }
            return true;
        }

        return super.onKeyDown(keyCode, event);
    }

    private void confirmDeleteMessages() {
        calcuteSelect();
        DeleteMessagesListener l = new DeleteMessagesListener();
        confirmDeleteDialog(l, mHasLockedMessage);
    }

    private class DeleteMessagesListener implements OnClickListener {
        private boolean mDeleteLockedMessages;

        public void setDeleteLockedMessage(boolean deleteLockedMessage) {
            mDeleteLockedMessages = deleteLockedMessage;
        }

        @Override
        public void onClick(DialogInterface dialog, int whichButton) {
            deleteMessages(mDeleteLockedMessages);
        }
    }

    private void confirmDeleteDialog(final DeleteMessagesListener listener, boolean locked) {
        View contents = View.inflate(this, R.layout.delete_thread_dialog_view, null);
        TextView msg = (TextView) contents.findViewById(R.id.message);
        msg.setText(getString(R.string.confirm_delete_selected_messages));
        final CheckBox checkbox = (CheckBox) contents.findViewById(R.id.delete_locked);
        checkbox.setChecked(false);
        if (!mHasLockedMessage) {
            checkbox.setVisibility(View.GONE);
        } else {
            listener.setDeleteLockedMessage(checkbox.isChecked());
            checkbox.setOnClickListener(new View.OnClickListener() {
                public void onClick(View v) {
                    listener.setDeleteLockedMessage(checkbox.isChecked());
                }
            });
        }

        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setTitle(R.string.confirm_dialog_title);
        builder.setIconAttribute(android.R.attr.alertDialogIcon);
        builder.setCancelable(true);
        builder.setView(contents);
        builder.setPositiveButton(R.string.yes, listener);
        builder.setNegativeButton(R.string.no, null);
        builder.show();
    }

    private void deleteMessages(boolean deleteLocked) {
        String whereClause;
        String smsWhereDelete = mSmsWhereDelete;
        String mmsWhereDelete = mMmsWhereDelete;

        if (!TextUtils.isEmpty(mSmsWhereDelete)) {
            smsWhereDelete = smsWhereDelete.substring(0, smsWhereDelete.length() - 1);
            smsWhereDelete = "_id in (" + smsWhereDelete + ")";
            if (!deleteLocked) {
                whereClause = smsWhereDelete == null ? " locked=0 " : smsWhereDelete
                        + " AND locked=0 ";
            } else {
                whereClause = smsWhereDelete;
            }

            if (!TextUtils.isEmpty(whereClause)) {
                int delSmsCount = SqliteWrapper.delete(this, getContentResolver(),
                        Uri.parse("content://sms"), whereClause, null);
                if (delSmsCount > 0) {
                    if (mQueryBoxType == TYPE_DRAFTBOX) {
                        updateDraftCache();
                    }
                    Toast.makeText(MailBoxMessageList.this, getString(R.string.operate_success),
                            Toast.LENGTH_LONG).show();
                }
            }
        }

        if (!TextUtils.isEmpty(mmsWhereDelete)) {
            mmsWhereDelete = mmsWhereDelete.substring(0, mmsWhereDelete.length() - 1);
            mmsWhereDelete = "_id in (" + mmsWhereDelete + ")";
            if (!deleteLocked) {
                whereClause = mmsWhereDelete == null ? " locked=0 " : mmsWhereDelete
                        + " AND locked=0 ";
            } else {
                whereClause = mmsWhereDelete;
            }

            if (!TextUtils.isEmpty(whereClause)) {
                int delMmsCount = SqliteWrapper.delete(this, getContentResolver(),
                        Uri.parse("content://mms"), whereClause, null);
                if (delMmsCount > 0) {
                    if (mQueryBoxType == TYPE_DRAFTBOX) {
                        updateDraftCache();
                    }
                    Toast.makeText(MailBoxMessageList.this, getString(R.string.operate_success),
                            Toast.LENGTH_LONG).show();
                }
            }
        }
        MmsWidgetProvider.notifyDatasetChanged(getApplicationContext());
        if (mThreadIds.size() > 0) {
            Conversation.updateThreads(mThreadIds);
            mThreadIds.clear();
        }
    }

    private void calcuteSelect() {
        int count = mListAdapter.getCount();
        SparseBooleanArray booleanArray = mListView.getCheckedItemPositions();
        int size = booleanArray.size();

        if (count == 0 || size == 0) {
            return;
        }
        String smsWhereDelete = "";
        String mmsWhereDelete = "";
        mThreadIds.clear();
        boolean hasLocked = false;

        for (int j = 0; j < size; j++) {
            int position = booleanArray.keyAt(j);

            if (!mListView.isItemChecked(position)) {
                continue;
            }
            Cursor c = (Cursor) mListAdapter.getItem(position);
            if (c == null) {
                return;
            }

            String msgtype = "sms";
            try {
                msgtype = c.getString(COLUMN_MSG_TYPE);
            } catch (Exception ex) {
                continue;
            }
            if (msgtype.equals("sms")) {
                String msgId = c.getString(COLUMN_ID);
                int lockValue = c.getInt(COLUMN_SMS_LOCKED);
                if (lockValue == 1) {
                    hasLocked = true;
                }
                smsWhereDelete += msgId + ",";
                mThreadIds.add(c.getLong(COLUMN_THREAD_ID));
            } else if (msgtype.equals("mms")) {
                int lockValue = c.getInt(COLUMN_MMS_LOCKED);
                if (lockValue == 1) {
                    hasLocked = true;
                }
                String msgId = c.getString(COLUMN_ID);
                mmsWhereDelete += msgId + ",";
            }
            mThreadId = c.getLong(COLUMN_THREAD_ID);
        }
        mSmsWhereDelete = smsWhereDelete;
        mMmsWhereDelete = mmsWhereDelete;
        mHasLockedMessage = hasLocked;
    }

    private void updateDraftCache() {
        if (mThreadId <= 0)
            return;
        DraftCache.getInstance().setDraftState(mThreadId, false);
    }

    public void onListContentChanged() {
        if (!mIsPause) {
            startAsyncQuery();
        }
    }

    public void checkAll() {
        int count = getListView().getCount();
        for (int i = 0; i < count; i++) {
            getListView().setItemChecked(i, true);
        }
        mListAdapter.notifyDataSetChanged();
    }

    public void unCheckAll() {
        int count = getListView().getCount();
        for (int i = 0; i < count; i++) {
            getListView().setItemChecked(i, false);
        }
        if (mListAdapter != null) {
            mListAdapter.notifyDataSetChanged();
        }
    }

    private void setupActionBar() {
        ActionBar actionBar = getActionBar();

        ViewGroup v = (ViewGroup) LayoutInflater.from(this).inflate(
                R.layout.mailbox_list_actionbar, null);
        actionBar.setDisplayOptions(ActionBar.DISPLAY_SHOW_CUSTOM, ActionBar.DISPLAY_SHOW_CUSTOM);
        actionBar.setCustomView(v, new ActionBar.LayoutParams(ActionBar.LayoutParams.WRAP_CONTENT,
                ActionBar.LayoutParams.WRAP_CONTENT, Gravity.CENTER_VERTICAL | Gravity.RIGHT));

        mCountTextView = (TextView) v.findViewById(R.id.message_count);
        mMessageTitle = (TextView) v.findViewById(R.id.message_title);
    }

    private class ModeCallback implements ListView.MultiChoiceModeListener {
        private View mMultiSelectActionBarView;
        private TextView mSelectedConvCount;
        private ImageView mSelectedAll;
        //used in MultiChoiceMode
        private boolean mHasSelectAll = false;
        private SelectionMenu mSelectionMenu;

        public boolean onCreateActionMode(ActionMode mode, Menu menu) {
            // comes into MultiChoiceMode
            mMultiChoiceMode = true;
            mSpinners.setVisibility(View.GONE);
            MenuInflater inflater = getMenuInflater();
            inflater.inflate(R.menu.conversation_multi_select_menu, menu);

            if (mMultiSelectActionBarView == null) {
                mMultiSelectActionBarView = (ViewGroup) LayoutInflater
                        .from(MailBoxMessageList.this).inflate(R.layout.action_mode, null);
            }
            mode.setCustomView(mMultiSelectActionBarView);
            mSelectionMenu = new SelectionMenu(getApplicationContext(),
                    (Button) mMultiSelectActionBarView.findViewById(R.id.selection_menu),
                    new PopupList.OnPopupItemClickListener() {
                        @Override
                        public boolean onPopupItemClick(int itemId) {
                            if (itemId == SelectionMenu.SELECT_OR_DESELECT) {
                                if (mHasSelectAll) {
                                    unCheckAll();
                                    mHasSelectAll = false;
                                } else {
                                    checkAll();
                                    mHasSelectAll = true;
                                }
                                mSelectionMenu.updateSelectAllMode(mHasSelectAll);
                            }
                            return true;
                        }
                    });
            return true;
        }

        public boolean onPrepareActionMode(ActionMode mode, Menu menu) {
            if (mSelectionMenu != null) {
                mSelectionMenu.setTitle(getApplicationContext().getString(R.string.selected_count,
                        getListView().getCheckedItemCount()));
            }
            return true;
        }

        public boolean onActionItemClicked(ActionMode mode, MenuItem item) {
            ListView listView = getListView();
            final int checkedCount = listView.getCheckedItemCount();
            switch (item.getItemId()) {
                case R.id.delete:
                    confirmDeleteMessages();
                    break;
                default:
                    break;
            }
            mode.finish();
            return true;
        }

        public void onDestroyActionMode(ActionMode mode) {
            // leave MultiChoiceMode
            mMultiChoiceMode = false;
            getListView().clearChoices();
            mListAdapter.notifyDataSetChanged();
            mSpinners.setVisibility(View.VISIBLE);
            mSelectionMenu.dismiss();

        }

        public void onItemCheckedStateChanged(ActionMode mode, int position, long id,
                boolean checked) {
            ListView listView = getListView();
            int checkedCount = listView.getCheckedItemCount();
            mSelectionMenu.setTitle(getApplicationContext().getString(R.string.selected_count,
                    checkedCount));
            if (checkedCount == getListAdapter().getCount()) {
                mHasSelectAll = true;
            } else {
                mHasSelectAll = false;
            }
            mSelectionMenu.updateSelectAllMode(mHasSelectAll);
            mListAdapter.updateItemBackgroud(position);
            mSelectionMenu.updateCheckedCount();
        }
    }
}
