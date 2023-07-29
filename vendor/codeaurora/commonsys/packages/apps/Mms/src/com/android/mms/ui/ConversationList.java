/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Not a Contribution.
 * Copyright (C) 2008 Esmertec AG.
 * Copyright (C) 2008 The Android Open Source Project
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

import android.app.ActionBar;
import android.app.AlertDialog;
import android.app.ListActivity;
import android.app.ProgressDialog;
import android.content.ActivityNotFoundException;
import android.content.AsyncQueryHandler;
import android.content.ContentResolver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.Configuration;
import android.database.Cursor;
import android.database.sqlite.SQLiteException;
import android.database.sqlite.SqliteWrapper;
import android.graphics.drawable.Drawable;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.preference.PreferenceManager;
import android.provider.ContactsContract;
import android.provider.ContactsContract.Contacts;
import android.provider.Settings;
import android.provider.Telephony;
import android.provider.Telephony.Threads;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.TextUtils;
import android.util.Log;
import android.view.ActionMode;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnCreateContextMenuListener;
import android.view.View.OnKeyListener;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import com.android.mms.LogTag;
import com.android.mms.MmsConfig;
import com.android.mms.R;
import com.android.mms.data.Contact;
import com.android.mms.data.ContactList;
import com.android.mms.data.Conversation;
import com.android.mms.data.Conversation.ConversationQueryHandler;
import com.android.mms.data.RecipientIdCache;
import com.android.mms.data.WorkingMessage;
import com.android.mms.transaction.MessagingNotification;
import com.android.mms.transaction.SmsReceiverService;
import com.android.mms.transaction.SmsRejectedReceiver;
import com.android.mms.ui.MailBoxMessageList;
import com.android.mms.ui.PopupList;
import com.android.mms.ui.SelectionMenu;
import com.android.mms.util.ContactRecipientEntryUtils;
import com.android.mms.util.DraftCache;
import com.android.mms.util.Recycler;
import com.android.mms.widget.MmsWidgetProvider;
import com.android.mmswrapper.ConstantsWrapper;
import com.android.mmswrapper.TelephonyWrapper;
import com.google.android.mms.pdu.PduHeaders;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashSet;
import java.util.Iterator;

/**
 * This activity provides a list view of existing conversations.
 */
public class ConversationList extends ListActivity implements DraftCache.OnDraftChangedListener {
    private static final String TAG = LogTag.TAG;
    private static final boolean DEBUG = false;
    private static final boolean DEBUGCLEANUP = true;

    public static final int THREAD_LIST_QUERY_TOKEN                 = 1701;
    public static final int UNREAD_THREADS_QUERY_TOKEN              = 1702;
    private static final int NOTIFICATION_THREAD_QUERY_TOKEN        = 1703;
    private static final int UNREAD_NOTIFICATION_THREAD_QUERY_TOKEN = 1704;
    public static final int DELETE_CONVERSATION_TOKEN               = 1801;
    public static final int HAVE_LOCKED_MESSAGES_TOKEN              = 1802;
    private static final int DELETE_OBSOLETE_THREADS_TOKEN          = 1803;

    // IDs of the context menu items for the list of conversations.
    public static final int MENU_DELETE               = 0;
    public static final int MENU_VIEW                 = 1;
    public static final int MENU_VIEW_CONTACT         = 2;
    public static final int MENU_ADD_TO_CONTACTS      = 3;

    public static final String NOT_OBSOLETE = "_id IN (SELECT DISTINCT thread_id FROM sms where "+
            "thread_id NOT NULL UNION SELECT DISTINCT thread_id FROM pdu where "+
            "thread_id NOT NULL)";

    public static final int COLUMN_ID = 0;

    public ThreadListQueryHandler mQueryHandler;
    private ConversationListAdapter mListAdapter;
    private SharedPreferences mPrefs;
    private Handler mHandler;
    private boolean mDoOnceAfterFirstQuery;
    private TextView mUnreadConvCount;
    private View mSmsPromoBannerView;
    private int mSavedFirstVisiblePosition = AdapterView.INVALID_POSITION;
    private int mSavedFirstItemOffset;
    private ProgressDialog mProgressDialog;
    private ImageButton mComposeMessageButton;
    private TextView mEmptyView;

    // keys for extras and icicles
    private final static String LAST_LIST_POS = "last_list_pos";
    private final static String LAST_LIST_OFFSET = "last_list_offset";

    static private final String CHECKED_MESSAGE_LIMITS = "checked_message_limits";
    private final static int DELAY_TIME = 500;

    // Whether or not we are currently enabled for SMS. This field is updated in onResume to make
    // sure we notice if the user has changed the default SMS app.
    private boolean mIsSmsEnabled;
    private Toast mComposeDisabledToast;
    private static long mLastDeletedThread = -1;
    private boolean mMultiChoiceMode = false;
    private boolean mHasNormalThreads = false;
    private boolean mHasNotificationThreads = false;

    private ViewGroup mListViewHeader;
    private ViewGroup mNotificationView;
    private ViewGroup mNotificationContent;
    private ImageView mNotificationIcon;
    private TextView mNotificationLabel;
    private TextView mNotificationDate;
    private TextView mNotificationSubject;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (MessageUtils.checkPermissionsIfNeeded(this)) {
            return;
        }
        // Cache recipients information in a background thread in advance.
        RecipientIdCache.init(getApplication());

        setContentView(R.layout.conversation_list_screen);

        LayoutInflater inflate = (LayoutInflater)
                getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        mListViewHeader = (ViewGroup) inflate.inflate(
                R.layout.conversation_list_header, null);
        getListView().addHeaderView(mListViewHeader);

        getWindow().addFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
        getWindow().setStatusBarColor(getResources().getColor(R.color.primary_color_dark));
        if (MessageUtils.isMailboxMode()) {
            Intent modeIntent = new Intent(this, MailBoxMessageList.class);
            startActivityIfNeeded(modeIntent, -1);
            finish();
            return;
        }

        mSmsPromoBannerView = findViewById(R.id.banner_sms_promo);
        initCreateNewMessageButton();

        mQueryHandler = new ThreadListQueryHandler(getContentResolver());

        ListView listView = getListView();
        mEmptyView = (TextView)findViewById(R.id.empty);
        listView.setOnCreateContextMenuListener(mConvListOnCreateContextMenuListener);
        listView.setOnKeyListener(mThreadListKeyListener);
        listView.setChoiceMode(ListView.CHOICE_MODE_MULTIPLE_MODAL);
        listView.setMultiChoiceModeListener(new ModeCallback());

        initListAdapter();

        setupActionBar();

        mProgressDialog = createProgressDialog();

        setTitle(R.string.app_label);

        mHandler = new Handler();
        mPrefs = PreferenceManager.getDefaultSharedPreferences(this);
        boolean checkedMessageLimits = mPrefs.getBoolean(CHECKED_MESSAGE_LIMITS, false);
        if (DEBUG) Log.v(TAG, "checkedMessageLimits: " + checkedMessageLimits);
        if (!checkedMessageLimits) {
            runOneTimeStorageLimitCheckForLegacyMessages();
        }

        if (savedInstanceState != null) {
            mSavedFirstVisiblePosition = savedInstanceState.getInt(LAST_LIST_POS,
                    AdapterView.INVALID_POSITION);
            mSavedFirstItemOffset = savedInstanceState.getInt(LAST_LIST_OFFSET, 0);
        } else {
            mSavedFirstVisiblePosition = AdapterView.INVALID_POSITION;
            mSavedFirstItemOffset = 0;
        }
        try {
            if (MessageUtils.isWfcUnavailable(this)) {
                MessageUtils.pupConnectWifiNotification(this);
            }
        } catch (Settings.SettingNotFoundException e) {
            Log.w(TAG, "SettingNotFoundException wfc");
        }
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);

        outState.putInt(LAST_LIST_POS, mSavedFirstVisiblePosition);
        outState.putInt(LAST_LIST_OFFSET, mSavedFirstItemOffset);
    }

    @Override
    public void onPause() {
        super.onPause();

        // Don't listen for changes while we're paused.
        if (null != mListAdapter) {
            mListAdapter.setOnContentChangedListener(null);
        }

        // Remember where the list is scrolled to so we can restore the scroll position
        // when we come back to this activity and *after* we complete querying for the
        // conversations.
        ListView listView = getListView();
        if (null != listView) {
            mSavedFirstVisiblePosition = listView.getFirstVisiblePosition();
            View firstChild = listView.getChildAt(0);
            mSavedFirstItemOffset = (firstChild == null) ? 0 : firstChild.getTop();
        }
    }

    /**
     * Enable Notification Group when quit Action Mode.
     */
    private void enableNotificationGroup() {
        if (mNotificationView == null || mNotificationIcon == null) {
            return;
        }
        mNotificationView.setEnabled(true);
        mNotificationView.setAlpha(1.0f);
        mNotificationIcon.setImageResource(R.drawable.notification_main);
    }

    /**
     * Disable Notification Group when into Action Mode.
     */
    private void disableNotificationGroup() {
        if (mNotificationView == null || mNotificationIcon == null) {
            return;
        }
        mNotificationView.setEnabled(false);
        mNotificationView.setAlpha(0.25f);
        mNotificationIcon.setImageResource(R.drawable.notification_disable);
    }

    /**
     * Gone Notification Group
     */
    private void invisibleNotificationGroup() {
        if (mNotificationContent != null) {
            mNotificationContent.setVisibility(View.GONE);
        }
        if (mNotificationIcon != null) {
            mNotificationIcon.setVisibility(View.GONE);
        }
    }

    /**
     * Visible Notification Group
     */
    private void visibleNotificationGroup() {
        if (mNotificationContent != null) {
            mNotificationContent.setVisibility(View.VISIBLE);
        }
        if (mNotificationIcon != null) {
            mNotificationIcon.setVisibility(View.VISIBLE);
        }
    }

    /**
     * Control Notification Group if need visible.
     * @param visible True is Visible, false is Gone.
     */
    private void toggleNotificationGroup(boolean visible) {
        String currentClassName = null;
        Intent intent = getIntent();
        if (intent != null) {
            currentClassName = intent.getComponent().getClassName();
        }
        SharedPreferences sp = PreferenceManager.getDefaultSharedPreferences(this);
        if (visible && sp.getBoolean(MessagingPreferenceActivity.SEPERATE_NOTIFI_MSG, true)
                && ConversationList.class.getName().equals(currentClassName)) {
            if (mNotificationView == null) {
                ViewStub stub = (ViewStub) mListViewHeader
                        .findViewById(R.id.view_stub_notification);
                if (stub != null) {
                    stub.inflate();
                }
                mNotificationView = (ViewGroup) mListViewHeader.
                        findViewById(R.id.notification_header);
                mNotificationView.setOnClickListener(new View.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        Intent intent = new Intent(ConversationList.this,
                                NotificationConversationList.class);
                        startActivity(intent);
                    }
                });
                mNotificationContent = (ViewGroup) mNotificationView.
                        findViewById(R.id.notification_content);
                mNotificationIcon = (ImageView) mNotificationView.
                        findViewById(R.id.notification_icon);
                mNotificationLabel = (TextView) mNotificationView.
                        findViewById(R.id.notification_label);
                mNotificationDate = (TextView) mNotificationView.
                        findViewById(R.id.date);
                mNotificationSubject = (TextView) mNotificationView.
                        findViewById(R.id.subject);
            }
            visibleNotificationGroup();
        } else {
            invisibleNotificationGroup();
        }
    }

    @Override
    protected void onResume() {
        super.onResume();

        boolean isSmsEnabled = MmsConfig.isSmsEnabled(this);
        if (isSmsEnabled != mIsSmsEnabled) {
            mIsSmsEnabled = isSmsEnabled;
            invalidateOptionsMenu();
        }

        // Multi-select is used to delete conversations. It is disabled if we are not the sms app.
        ListView listView = getListView();
        if (mIsSmsEnabled) {
            listView.setChoiceMode(ListView.CHOICE_MODE_MULTIPLE_MODAL);
        } else {
            listView.setChoiceMode(ListView.CHOICE_MODE_NONE);
        }

        // Show or hide the SMS promo banner
        if (mIsSmsEnabled || MmsConfig.isSmsPromoDismissed(this)) {
            mSmsPromoBannerView.setVisibility(View.GONE);
        } else {
            initSmsPromoBanner();
            mSmsPromoBannerView.setVisibility(View.VISIBLE);
        }
        mListAdapter.setOnContentChangedListener(mContentChangedListener);
        if (!mDoOnceAfterFirstQuery) {
            startAsyncQuery();
        }
    }

    private void setupActionBar() {
        ActionBar actionBar = getActionBar();

        ViewGroup v = (ViewGroup)LayoutInflater.from(this)
            .inflate(R.layout.conversation_list_actionbar, null);
        actionBar.setDisplayOptions(ActionBar.DISPLAY_SHOW_CUSTOM,
                ActionBar.DISPLAY_SHOW_CUSTOM);
        actionBar.setCustomView(v,
                new ActionBar.LayoutParams(ActionBar.LayoutParams.WRAP_CONTENT,
                        ActionBar.LayoutParams.WRAP_CONTENT,
                        Gravity.CENTER_VERTICAL | Gravity.END));

        mUnreadConvCount = (TextView)v.findViewById(R.id.unread_conv_count);
    }

    private final ConversationListAdapter.OnContentChangedListener mContentChangedListener =
        new ConversationListAdapter.OnContentChangedListener() {
        @Override
        public void onContentChanged(ConversationListAdapter adapter) {
            startAsyncQuery();
        }
    };

    private void initListAdapter() {
        mListAdapter = new ConversationListAdapter(this, null);
        mListAdapter.setOnContentChangedListener(mContentChangedListener);
        setListAdapter(mListAdapter);
        getListView().setRecyclerListener(mListAdapter);
    }

    private void initSmsPromoBanner() {
        final PackageManager packageManager = getPackageManager();
        final String smsAppPackage = Telephony.Sms.getDefaultSmsPackage(this);

        // Get all the data we need about the default app to properly render the promo banner. We
        // try to show the icon and name of the user's selected SMS app and have the banner link
        // to that app. If we can't read that information for any reason we leave the fallback
        // text that links to Messaging settings where the user can change the default.
        Drawable smsAppIcon = null;
        ApplicationInfo smsAppInfo = null;
        try {
            smsAppIcon = packageManager.getApplicationIcon(smsAppPackage);
            smsAppInfo = packageManager.getApplicationInfo(smsAppPackage, 0);
        } catch (NameNotFoundException e) {
        }
        final Intent smsAppIntent = packageManager.getLaunchIntentForPackage(smsAppPackage);

        // If we got all the info we needed
        if (smsAppIcon != null && smsAppInfo != null && smsAppIntent != null) {
            ImageView defaultSmsAppIconImageView =
                    (ImageView)mSmsPromoBannerView.findViewById(R.id.banner_sms_default_app_icon);
            defaultSmsAppIconImageView.setImageDrawable(smsAppIcon);
            TextView smsPromoBannerTitle =
                    (TextView)mSmsPromoBannerView.findViewById(R.id.banner_sms_promo_title);
            String message = getResources().getString(R.string.banner_sms_promo_title_application,
                    smsAppInfo.loadLabel(packageManager));
            smsPromoBannerTitle.setText(message);

            mSmsPromoBannerView.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    startActivity(smsAppIntent);
                }
            });
        } else {
            // Otherwise the banner will be left alone and will launch settings
            mSmsPromoBannerView.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    // Launch settings
                    Intent settingsIntent = new Intent(ConversationList.this,
                            MessagingPreferenceActivity.class);
                    startActivityIfNeeded(settingsIntent, -1);
                }
            });
        }
    }

    /**
     * Checks to see if the number of MMS and SMS messages are under the limits for the
     * recycler. If so, it will automatically turn on the recycler setting. If not, it
     * will prompt the user with a message and point them to the setting to manually
     * turn on the recycler.
     */
    public synchronized void runOneTimeStorageLimitCheckForLegacyMessages() {
        if (Recycler.isAutoDeleteEnabled(this)) {
            if (DEBUG) Log.v(TAG, "recycler is already turned on");
            // The recycler is already turned on. We don't need to check anything or warn
            // the user, just remember that we've made the check.
            markCheckedMessageLimit();
            return;
        }
        new Thread(new Runnable() {
            @Override
            public void run() {
                if (Recycler.checkForThreadsOverLimit(ConversationList.this)) {
                    if (DEBUG) Log.v(TAG, "checkForThreadsOverLimit TRUE");
                    // Dang, one or more of the threads are over the limit. Show an activity
                    // that'll encourage the user to manually turn on the setting. Delay showing
                    // this activity until a couple of seconds after the conversation list appears.
                    mHandler.postDelayed(new Runnable() {
                        @Override
                        public void run() {
                            Intent intent = new Intent(ConversationList.this,
                                    WarnOfStorageLimitsActivity.class);
                            startActivity(intent);
                        }
                    }, 2000);
                } else {
                    if (DEBUG) Log.v(TAG, "checkForThreadsOverLimit silently turning on recycler");
                    // No threads were over the limit. Turn on the recycler by default.
                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            SharedPreferences.Editor editor = mPrefs.edit();
                            editor.putBoolean(MessagingPreferenceActivity.AUTO_DELETE, true);
                            editor.apply();
                        }
                    });
                }
                // Remember that we don't have to do the check anymore when starting MMS.
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        markCheckedMessageLimit();
                    }
                });
            }
        }, "ConversationList.runOneTimeStorageLimitCheckForLegacyMessages").start();
    }

    /**
     * Mark in preferences that we've checked the user's message limits. Once checked, we'll
     * never check them again, unless the user wipe-data or resets the device.
     */
    private void markCheckedMessageLimit() {
        if (DEBUG) Log.v(TAG, "markCheckedMessageLimit");
        SharedPreferences.Editor editor = mPrefs.edit();
        editor.putBoolean(CHECKED_MESSAGE_LIMITS, true);
        editor.apply();
    }

    @Override
    protected void onNewIntent(Intent intent) {
        // Handle intents that occur after the activity has already been created.
        startAsyncQuery();
    }

    @Override
    protected void onStart() {
        super.onStart();

        MessagingNotification.cancelNotification(getApplicationContext(),
                SmsRejectedReceiver.SMS_REJECTED_NOTIFICATION_ID);

        DraftCache.getInstance().addOnDraftChangedListener(this);

        mDoOnceAfterFirstQuery = true;

        startAsyncQuery();

        // We used to refresh the DraftCache here, but
        // refreshing the DraftCache each time we go to the ConversationList seems overly
        // aggressive. We already update the DraftCache when leaving CMA in onStop() and
        // onNewIntent(), and when we delete threads or delete all in CMA or this activity.
        // I hope we don't have to do such a heavy operation each time we enter here.

        // we invalidate the contact cache here because we want to get updated presence
        // and any contact changes. We don't invalidate the cache by observing presence and contact
        // changes (since that's too untargeted), so as a tradeoff we do it here.
        // If we're in the middle of the app initialization where we're loading the conversation
        // threads, don't invalidate the cache because we're in the process of building it.
        // TODO: think of a better way to invalidate cache more surgically or based on actual
        // TODO: changes we care about
        if (!Conversation.loadingThreads()) {
            Contact.invalidateCache();
        }
    }

    @Override
    protected void onStop() {
        super.onStop();

        stopAsyncQuery();

        DraftCache.getInstance().removeOnDraftChangedListener(this);

        unbindListeners(null);
        // Simply setting the choice mode causes the previous choice mode to finish and we exit
        // multi-select mode (if we're in it) and remove all the selections.
        getListView().setChoiceMode(ListView.CHOICE_MODE_MULTIPLE_MODAL);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (mListAdapter != null) {
            mListAdapter.changeCursor(null);
        }
        if (mHandler != null) {
            mHandler.removeCallbacks(mShowProgressDialogRunnable);
        }
        if (mProgressDialog != null && mProgressDialog.isShowing()) {
            LogTag.debugD("dismiss progress dialog");
            mProgressDialog.dismiss();
        }
        if (getResources().getBoolean(R.bool.config_mailbox_enable)) {
            MessageUtils.removeDialogs();
        }
        Contact.clearListener();
    }

    private void unbindListeners(final Collection<Long> threadIds) {
        for (int i = 0; i < getListView().getChildCount(); i++) {
            View view = getListView().getChildAt(i);
            if (view instanceof ConversationListItem) {
                ConversationListItem item = (ConversationListItem)view;
                if (threadIds == null) {
                    item.unbind();
                } else if (item.getConversation() != null
                        && threadIds.contains(item.getConversation().getThreadId())) {
                    item.unbind();
                }
            }
        }
    }

    @Override
    public void onDraftChanged(final long threadId, final boolean hasDraft) {
        // Run notifyDataSetChanged() on the main thread.
        mQueryHandler.post(new Runnable() {
            @Override
            public void run() {
                if (Log.isLoggable(LogTag.APP, Log.VERBOSE)) {
                    log("onDraftChanged: threadId=" + threadId + ", hasDraft=" + hasDraft);
                }
                mListAdapter.notifyDataSetChanged();
            }
        });
    }

    public void startAsyncQuery() {
        try {
            if (null == mListAdapter) {
                Log.e(TAG, "startAsyncQuery null adapter");
                ConversationList.this.finish();
                return;
            }
            if (null != mEmptyView) {
                mEmptyView.setText(R.string.loading_conversations);
            }
            if (null == mQueryHandler) {
                mQueryHandler = new ThreadListQueryHandler(getContentResolver());
            }

            SharedPreferences sp = PreferenceManager.getDefaultSharedPreferences(this);
            if (sp.getBoolean(MessagingPreferenceActivity.SEPERATE_NOTIFI_MSG, true)) {
                Conversation.startQuery(mQueryHandler, THREAD_LIST_QUERY_TOKEN,
                        TelephonyWrapper.NOTIFICATION + "=0" + " and " + NOT_OBSOLETE);
                Conversation.startQuery(mQueryHandler, NOTIFICATION_THREAD_QUERY_TOKEN,
                        TelephonyWrapper.NOTIFICATION + "=1" + " and " + NOT_OBSOLETE);
            } else {
                Conversation.startQuery(mQueryHandler, THREAD_LIST_QUERY_TOKEN, NOT_OBSOLETE);
                toggleNotificationGroup(false);
            }
            Conversation.startQuery(mQueryHandler, UNREAD_THREADS_QUERY_TOKEN, Threads.READ + "=0"
                    + " and " + NOT_OBSOLETE);
        } catch (SQLiteException e) {
            SqliteWrapper.checkSQLiteException(this, e);
        }
    }

    private void stopAsyncQuery() {
        if (mQueryHandler != null) {
            mQueryHandler.cancelOperation(THREAD_LIST_QUERY_TOKEN);
            mQueryHandler.cancelOperation(UNREAD_THREADS_QUERY_TOKEN);
        }
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        //In folder mode, it will jump to MailBoxMessageList,finish current
        //activity, no need create option menu.
        if (MessageUtils.isMailboxMode()) {
            return true;
        }
        getMenuInflater().inflate(R.menu.conversation_list_menu, menu);

        MenuItem item = menu.findItem(R.id.action_change_to_conversation_mode);
        if (item != null) {
            item.setVisible(false);
        }

        MenuItem cellBroadcastItem = menu.findItem(R.id.action_cell_broadcasts);
        cellBroadcastItem.setVisible(false);
        if (cellBroadcastItem != null) {
            // Enable link to Cell broadcast activity depending on the value in config.xml.
            boolean isCellBroadcastAppLinkEnabled = ConstantsWrapper.getConfigCellBroadcastAppLinks(this);
            try {
                if (isCellBroadcastAppLinkEnabled) {
                    PackageManager pm = getPackageManager();
                    if (pm.getApplicationEnabledSetting("com.android.cellbroadcastreceiver")
                            == PackageManager.COMPONENT_ENABLED_STATE_DISABLED) {
                        isCellBroadcastAppLinkEnabled = false;  // CMAS app disabled
                    }
                }
            } catch (IllegalArgumentException ignored) {
                isCellBroadcastAppLinkEnabled = false;  // CMAS app not installed
            }
            if (!isCellBroadcastAppLinkEnabled) {
                cellBroadcastItem.setVisible(false);
            }
        }

        return true;
    }

    @Override
    public boolean onPrepareOptionsMenu(Menu menu) {
        //In folder mode, it will jump to MailBoxMessageList,finish current
        //activity, no need prepare option menu.
        if (MessageUtils.isMailboxMode()) {
            return true;
        }
        MenuItem item = menu.findItem(R.id.action_delete_all);
        if (item != null) {
            item.setVisible(false);
        }

        if (!getResources().getBoolean(R.bool.config_mailbox_enable)) {
            item = menu.findItem(R.id.action_change_to_folder_mode);
            if (item != null) {
                item.setVisible(false);
            }
            item = menu.findItem(R.id.action_memory_status);
            if (item != null) {
                item.setVisible(false);
            }
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
    public boolean onSearchRequested() {
        if (getResources().getBoolean(R.bool.config_classify_search)) {
            // block search entirely (by simply returning false).
            return false;
        }
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch(item.getItemId()) {
            case R.id.search:
                Intent searchintent = new Intent(this, SearchConversationActivity.class);
                startActivity(searchintent);
                return true;
            case R.id.action_delete_all:
                // The invalid threadId of -1 means all threads here.
                confirmDeleteThread(-1L, mQueryHandler);
                break;
            case R.id.action_settings:
                Intent intent = new Intent(this, MessagingPreferenceActivity.class);
                startActivityIfNeeded(intent, -1);
                break;
            case R.id.action_change_to_folder_mode:
                Intent modeIntent = new Intent(this, MailBoxMessageList.class);
                startActivityIfNeeded(modeIntent, -1);
                MessageUtils.setMailboxMode(true);
                finish();
                break;
            case R.id.action_debug_dump:
                LogTag.dumpInternalTables(this);
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
                return true;
            default:
                return true;
        }
        return false;
    }

    @Override
    protected void onListItemClick(ListView l, View v, int position, long id) {
        // Note: don't read the thread id data from the ConversationListItem view passed in.
        // It's unreliable to read the cached data stored in the view because the ListItem
        // can be recycled, and the same view could be assigned to a different position
        // if you click the list item fast enough. Instead, get the cursor at the position
        // clicked and load the data from the cursor.
        // (ConversationListAdapter extends CursorAdapter, so getItemAtPosition() should
        // return the cursor object, which is moved to the position passed in)
        Cursor cursor  = (Cursor) getListView().getItemAtPosition(position);
        if (cursor == null || cursor.getPosition() < 0) {
            return;
        }
        Conversation conv = Conversation.from(this, cursor);
        long tid = conv.getThreadId();

        if (LogTag.VERBOSE) {
            Log.d(TAG, "onListItemClick: pos=" + position + ", view=" + v + ", tid=" + tid);
        }
        openThread(tid);
    }

    protected void initCreateNewMessageButton() {
        findViewById(R.id.floating_button_view_stub).setVisibility(View.VISIBLE);
        mComposeMessageButton = (ImageButton) findViewById(R.id.create);
        mComposeMessageButton.setClickable(true);
        mComposeMessageButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                if (mIsSmsEnabled) {
                    createNewMessage();
                } else {
                    // Display a toast letting the user know they can not compose.
                    if (mComposeDisabledToast == null) {
                        mComposeDisabledToast = Toast.makeText(ConversationList.this,
                                R.string.compose_disabled_toast, Toast.LENGTH_SHORT);
                    }
                    mComposeDisabledToast.show();
                }
            }
        });
    }

    public void createNewMessage() {
        startActivity(ComposeMessageActivity.createIntent(this, 0));
    }

    public void openThread(long threadId) {
        startActivity(ComposeMessageActivity.createIntent(this, threadId));
    }

    public static Intent createAddContactIntent(String address) {
        // address must be a single recipient
        Intent intent = new Intent(Intent.ACTION_INSERT_OR_EDIT);
        intent.setType(Contacts.CONTENT_ITEM_TYPE);
        if (ContactRecipientEntryUtils.isEmailAddress(address)) {
            intent.putExtra(ContactsContract.Intents.Insert.EMAIL, address);
        } else {
            intent.putExtra(ContactsContract.Intents.Insert.PHONE, address);
            intent.putExtra(ContactsContract.Intents.Insert.PHONE_TYPE,
                    ContactsContract.CommonDataKinds.Phone.TYPE_MOBILE);
        }
        intent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_WHEN_TASK_RESET);

        return intent;
    }

    private final OnCreateContextMenuListener mConvListOnCreateContextMenuListener =
        new OnCreateContextMenuListener() {
        @Override
        public void onCreateContextMenu(ContextMenu menu, View v,
                ContextMenuInfo menuInfo) {
            Cursor cursor = mListAdapter.getCursor();
            if (cursor == null || cursor.getPosition() < 0) {
                return;
            }
            Conversation conv = Conversation.from(ConversationList.this, cursor);
            ContactList recipients = conv.getRecipients();
            menu.setHeaderTitle(recipients.formatNames(","));

            AdapterView.AdapterContextMenuInfo info =
                (AdapterView.AdapterContextMenuInfo) menuInfo;
            menu.add(0, MENU_VIEW, 0, R.string.menu_view);

            // Only show if there's a single recipient
            if (recipients.size() == 1) {
                // do we have this recipient in contacts?
                if (recipients.get(0).existsInDatabase()) {
                    menu.add(0, MENU_VIEW_CONTACT, 0, R.string.menu_view_contact);
                } else {
                    menu.add(0, MENU_ADD_TO_CONTACTS, 0, R.string.menu_add_to_contacts);
                }
            }
            if (mIsSmsEnabled) {
                menu.add(0, MENU_DELETE, 0, R.string.menu_delete);
            }
        }
    };

    @Override
    public boolean onContextItemSelected(MenuItem item) {
        Cursor cursor = mListAdapter.getCursor();
        if (cursor != null && cursor.getPosition() >= 0) {
            Conversation conv = Conversation.from(ConversationList.this, cursor);
            long threadId = conv.getThreadId();
            switch (item.getItemId()) {
            case MENU_DELETE: {
                confirmDeleteThread(threadId, mQueryHandler);
                break;
            }
            case MENU_VIEW: {
                openThread(threadId);
                break;
            }
            case MENU_VIEW_CONTACT: {
                Contact contact = conv.getRecipients().get(0);
                Intent intent = new Intent(Intent.ACTION_VIEW, contact.getUri());
                intent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_WHEN_TASK_RESET);
                startActivity(intent);
                break;
            }
            case MENU_ADD_TO_CONTACTS: {
                String address = conv.getRecipients().get(0).getNumber();
                startActivity(createAddContactIntent(address));
                break;
            }
            default:
                break;
            }
        }
        return super.onContextItemSelected(item);
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        // We override this method to avoid restarting the entire
        // activity when the keyboard is opened (declared in
        // AndroidManifest.xml).  Because the only translatable text
        // in this activity is "New Message", which has the full width
        // of phone to work with, localization shouldn't be a problem:
        // no abbreviated alternate words should be needed even in
        // 'wide' languages like German or Russian.

        super.onConfigurationChanged(newConfig);
        if (DEBUG) Log.v(TAG, "onConfigurationChanged: " + newConfig);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_SEARCH && !mMultiChoiceMode) {
            if (getResources().getBoolean(R.bool.config_classify_search)) {
                Intent searchintent = new Intent(this, SearchActivityExtend.class);
                startActivityIfNeeded(searchintent, -1);
            }
            return true;
        }

        return super.onKeyDown(keyCode, event);
    }

    /**
     * Start the process of putting up a dialog to confirm deleting a thread,
     * but first start a background query to see if any of the threads or thread
     * contain locked messages so we'll know how detailed of a UI to display.
     * @param threadId id of the thread to delete or -1 for all threads
     * @param handler query handler to do the background locked query
     */
    public static void confirmDeleteThread(long threadId, AsyncQueryHandler handler) {
        ArrayList<Long> threadIds = null;
        if (threadId != -1) {
            threadIds = new ArrayList<Long>();
            threadIds.add(threadId);
        }
        confirmDeleteThreads(threadIds, handler);
    }

    /**
     * Start the process of putting up a dialog to confirm deleting threads,
     * but first start a background query to see if any of the threads
     * contain locked messages so we'll know how detailed of a UI to display.
     * @param threadIds list of threadIds to delete or null for all threads
     * @param handler query handler to do the background locked query
     */
    public static void confirmDeleteThreads(Collection<Long> threadIds, AsyncQueryHandler handler) {
        Conversation.startQueryHaveLockedMessages(handler, threadIds,
                HAVE_LOCKED_MESSAGES_TOKEN);
    }

    /**
     * Build and show the proper delete thread dialog. The UI is slightly different
     * depending on whether there are locked messages in the thread(s) and whether we're
     * deleting single/multiple threads or all threads.
     * @param listener gets called when the delete button is pressed
     * @param threadIds the thread IDs to be deleted (pass null for all threads)
     * @param hasLockedMessages whether the thread(s) contain locked messages
     * @param context used to load the various UI elements
     */
    public static void confirmDeleteThreadDialog(final DeleteThreadListener listener,
            Collection<Long> threadIds,
            boolean hasLockedMessages,
            Context context) {
        View contents = View.inflate(context, R.layout.delete_thread_dialog_view, null);
        TextView msg = (TextView)contents.findViewById(R.id.message);

        if (threadIds == null) {
            msg.setText(R.string.confirm_delete_all_conversations);
        } else {
            // Show the number of threads getting deleted in the confirmation dialog.
            int cnt = threadIds.size();
            msg.setText(context.getResources().getQuantityString(
                R.plurals.confirm_delete_conversation, cnt, cnt));
        }

        final CheckBox checkbox = (CheckBox)contents.findViewById(R.id.delete_locked);
        if (!hasLockedMessages) {
            checkbox.setVisibility(View.GONE);
        } else {
            listener.setDeleteLockedMessage(checkbox.isChecked());
            checkbox.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    listener.setDeleteLockedMessage(checkbox.isChecked());
                }
            });
        }

        AlertDialog.Builder builder = new AlertDialog.Builder(context);
        builder.setTitle(R.string.confirm_dialog_title)
            .setIconAttribute(android.R.attr.alertDialogIcon)
            .setCancelable(true)
            .setPositiveButton(R.string.delete, listener)
            .setNegativeButton(R.string.no, null)
            .setView(contents)
            .show();
    }

    private final OnKeyListener mThreadListKeyListener = new OnKeyListener() {
        @Override
        public boolean onKey(View v, int keyCode, KeyEvent event) {
            if (event.getAction() == KeyEvent.ACTION_DOWN) {
                switch (keyCode) {
                    case KeyEvent.KEYCODE_DEL: {
                        long id = getListView().getSelectedItemId();
                        if (id > 0) {
                            confirmDeleteThread(id, mQueryHandler);
                        }
                        return true;
                    }
                }
            }
            return false;
        }
    };

    public static class DeleteThreadListener implements OnClickListener {
        private final Collection<Long> mThreadIds;
        private final ConversationQueryHandler mHandler;
        private final Context mContext;
        private boolean mDeleteLockedMessages;
        private final Runnable mCallBack;

        public DeleteThreadListener(Collection<Long> threadIds, ConversationQueryHandler handler,
                Runnable callBack, Context context) {
            mThreadIds = threadIds;
            mHandler = handler;
            mContext = context;
            mCallBack = callBack;
        }

        public void setDeleteLockedMessage(boolean deleteLockedMessages) {
            mDeleteLockedMessages = deleteLockedMessages;
        }

        @Override
        public void onClick(DialogInterface dialog, final int whichButton) {
            MessageUtils.handleReadReport(mContext, mThreadIds,
                    PduHeaders.READ_STATUS__DELETED_WITHOUT_BEING_READ, new Runnable() {
                @Override
                public void run() {
                    int token = DELETE_CONVERSATION_TOKEN;
                    if (mCallBack != null) {
                        mCallBack.run();
                    }
                    if (mContext instanceof ConversationList) {
                        ((ConversationList)mContext).unbindListeners(mThreadIds);
                    }
                    if (mThreadIds == null) {
                        Conversation.startDeleteAll(mHandler, token, mDeleteLockedMessages);
                        DraftCache.getInstance().refresh();
                    } else {
                        int size = mThreadIds.size();
                        if (size > 0 && mCallBack != null) {
                            // Save the last thread id.
                            // And cancel deleting dialog after this thread been deleted.
                            mLastDeletedThread = (mThreadIds.toArray(new Long[size]))[size - 1];
                        }
                        Conversation.startDelete(mHandler, token, mDeleteLockedMessages,
                                mThreadIds);
                    }
                }
            });
            dialog.dismiss();
        }
    }

    private final Runnable mDeleteObsoleteThreadsRunnable = new Runnable() {
        @Override
        public void run() {
            if (Log.isLoggable(LogTag.APP, Log.VERBOSE)) {
                LogTag.debug("mDeleteObsoleteThreadsRunnable getSavingDraft(): " +
                        DraftCache.getInstance().getSavingDraft());
            }
            if (DraftCache.getInstance().getSavingDraft()) {
                // We're still saving a draft. Try again in a second. We don't want to delete
                // any threads out from under the draft.
                if (DEBUGCLEANUP) {
                    LogTag.debug("mDeleteObsoleteThreadsRunnable saving draft, trying again");
                }
                mHandler.postDelayed(mDeleteObsoleteThreadsRunnable, 1000);
            } else if (SmsReceiverService.getSavingMessage()) {
                LogTag.debug("mDeleteObsoleteThreadsRunnable saving new message, trying again");
                mHandler.postDelayed(mDeleteObsoleteThreadsRunnable, 1000);
            } else if (WorkingMessage.getSendingStatus()) {
                LogTag.debug("mDeleteObsoleteThreadsRunnable is sending new message, trying again");
                mHandler.postDelayed(mDeleteObsoleteThreadsRunnable, 1000);
            } else {
                if (DEBUGCLEANUP) {
                    LogTag.debug("mDeleteObsoleteThreadsRunnable calling " +
                            "asyncDeleteObsoleteThreads");
                }
                Conversation.asyncDeleteObsoleteThreads(mQueryHandler,
                        DELETE_OBSOLETE_THREADS_TOKEN);
            }
        }
    };

    private final class ThreadListQueryHandler extends ConversationQueryHandler {
        public ThreadListQueryHandler(ContentResolver contentResolver) {
            super(contentResolver);
        }

        // Test code used for various scenarios where its desirable to insert a delay in
        // responding to query complete. To use, uncomment out the block below and then
        // comment out the @Override and onQueryComplete line.
//        @Override
//        protected void onQueryComplete(final int token, final Object cookie, final Cursor cursor) {
//            mHandler.postDelayed(new Runnable() {
//                public void run() {
//                    myonQueryComplete(token, cookie, cursor);
//                    }
//            }, 2000);
//        }
//
//        protected void myonQueryComplete(int token, Object cookie, Cursor cursor) {

        @Override
        protected void onQueryComplete(int token, Object cookie, Cursor cursor) {
            if (ConversationList.this.isFinishing()) {
                Log.w(TAG, "ConversationList is finished, do nothing ");
                if (cursor != null) {
                    cursor.close();
                }
                return;
            }

            switch (token) {
            case THREAD_LIST_QUERY_TOKEN:
                if (null == mListAdapter) {
                    if (null != cursor) {
                        cursor.close();
                    }
                    Log.e(TAG, "onQueryComplete null adapter");
                    ConversationList.this.finish();
                    return;
                }
                mListAdapter.changeCursor(cursor);

                if (mListAdapter.getCount() == 0) {
                    mEmptyView.setText(R.string.no_conversations);
                    mHasNormalThreads = false;
                } else {
                    mHasNormalThreads = true;
                }
                updateEmptyView();

                if (mDoOnceAfterFirstQuery) {
                    mDoOnceAfterFirstQuery = false;
                    // Delay doing a couple of DB operations until we've initially queried the DB
                    // for the list of conversations to display. We don't want to slow down showing
                    // the initial UI.

                    // 1. Delete any obsolete threads. Obsolete threads are threads that aren't
                    // referenced by at least one message in the pdu or sms tables.
                    mHandler.post(mDeleteObsoleteThreadsRunnable);

                    // 2. Mark all the conversations as seen.
                    Conversation.markAllConversationsAsSeen(getApplicationContext());
                }
                if (mSavedFirstVisiblePosition != AdapterView.INVALID_POSITION) {
                    // Restore the list to its previous position.
                    getListView().setSelectionFromTop(mSavedFirstVisiblePosition,
                            mSavedFirstItemOffset);
                    mSavedFirstVisiblePosition = AdapterView.INVALID_POSITION;
                }
                break;

            case UNREAD_THREADS_QUERY_TOKEN:
                int count = 0;
                if (cursor != null) {
                    count = cursor.getCount();
                    cursor.close();
                }
                mUnreadConvCount.setText(count > 0 ? Integer.toString(count) : null);
                break;

            case NOTIFICATION_THREAD_QUERY_TOKEN:
                if (cursor != null && cursor.getCount() > 0) {
                    mHasNotificationThreads = true;
                } else {
                    mHasNotificationThreads = false;
                }
                updateEmptyView();
                if (cursor != null) {
                    int notificationCount = 0;
                    notificationCount = cursor.getCount();
                    if (notificationCount > 0) {
                        if (cursor.moveToFirst()) {
                            updateNotificationView(cursor);
                        } else {
                            Log.e(TAG,"Cursor couldn't move to first");
                        }
                    } else {
                        toggleNotificationGroup(false);
                    }
                }
                break;

            case UNREAD_NOTIFICATION_THREAD_QUERY_TOKEN:
                int unreadCount = 0;
                if (cursor != null) {
                    unreadCount = cursor.getCount();
                    cursor.close();
                }
                mNotificationLabel.setText(unreadCount > 0 ?
                        getString(R.string.unread_notification_message_label, unreadCount) :
                        getString(R.string.notification_message_label));
                break;

            case HAVE_LOCKED_MESSAGES_TOKEN:
                @SuppressWarnings("unchecked")
                Collection<Long> threadIds = (Collection<Long>)cookie;
                confirmDeleteThreadDialog(new DeleteThreadListener(threadIds, mQueryHandler,
                        mDeletingRunnable, ConversationList.this), threadIds,
                        cursor != null && cursor.getCount() > 0,
                        ConversationList.this);
                if (cursor != null) {
                    cursor.close();
                }
                break;

            default:
                Log.e(TAG, "onQueryComplete called with unknown token " + token);
            }
        }

        @Override
        protected void onDeleteComplete(int token, Object cookie, int result) {
            super.onDeleteComplete(token, cookie, result);
            switch (token) {
            case DELETE_CONVERSATION_TOKEN:
                long threadId = cookie != null ? (Long)cookie : -1;     // default to all threads
                if (threadId < 0 || threadId == mLastDeletedThread) {
                    mHandler.removeCallbacks(mShowProgressDialogRunnable);
                    if (mProgressDialog != null && mProgressDialog.isShowing()) {
                        mProgressDialog.dismiss();
                    }
                    mLastDeletedThread = -1;
                }
                if (threadId == -1) {
                    // Rebuild the contacts cache now that all threads and their associated unique
                    // recipients have been deleted.
                    Contact.init(getApplication());
                } else {
                    // Remove any recipients referenced by this single thread from the
                    // contacts cache. It's possible for two or more threads to reference
                    // the same contact. That's ok if we remove it. We'll recreate that contact
                    // when we init all Conversations below.
                    Conversation conv = Conversation.get(ConversationList.this, threadId, false);
                    if (conv != null) {
                        ContactList recipients = conv.getRecipients();
                        for (Contact contact : recipients) {
                            contact.removeFromCache();
                        }
                    }
                }
                // Make sure the conversation cache reflects the threads in the DB.
                Conversation.init(getApplicationContext());

                // Update the notification for new messages since they
                // may be deleted.
                MessagingNotification.nonBlockingUpdateNewMessageIndicator(ConversationList.this,
                        MessagingNotification.THREAD_NONE, false);
                // Update the notification for failed messages since they
                // may be deleted.
                MessagingNotification.nonBlockingUpdateSendFailedNotification(ConversationList.this);

                // Make sure the list reflects the delete
                startAsyncQuery();

                MmsWidgetProvider.notifyDatasetChanged(getApplicationContext());
                break;

            case DELETE_OBSOLETE_THREADS_TOKEN:
                if (DEBUGCLEANUP) {
                    LogTag.debug("onQueryComplete finished DELETE_OBSOLETE_THREADS_TOKEN");
                }

                if (result > 0) {
                    startAsyncQuery();
                }
                break;
            }
        }
    }

    private void updateNotificationView(Cursor cursor) {
        new AsyncTask<Cursor, Void, Long>() {
            private int mUnreadCount = 0;

            @Override
            protected void onPreExecute() {
                toggleNotificationGroup(true);
            }

            @Override
            protected Long doInBackground(Cursor... params) {
                long firstUnReadThreadId = Conversation.INVALID_THREAD_ID;
                long firstReadThreadId = Conversation.INVALID_THREAD_ID;
                do {
                    Conversation conv = Conversation.
                            from(getApplicationContext(), params[0]);
                    if (conv.hasUnreadMessages()) {
                        if (firstUnReadThreadId == Conversation.INVALID_THREAD_ID) {
                            firstUnReadThreadId = conv.getThreadId();
                        }
                        mUnreadCount++;
                    } else {
                        if (firstReadThreadId == Conversation.INVALID_THREAD_ID) {
                            firstReadThreadId = conv.getThreadId();
                        }
                    }
                } while (params[0].moveToNext());

                params[0].close();

                return firstUnReadThreadId != Conversation.INVALID_THREAD_ID ?
                        firstUnReadThreadId : firstReadThreadId;
            }

            @Override
            protected void onPostExecute(Long integer) {
                if (integer == -1) {
                    return;
                }
                Conversation conv = Conversation.get(ConversationList.this,
                        integer, false);
                if (conv.hasUnreadMessages()) {
                    String data = MessageUtils.formatTimeStampString(
                            getApplicationContext(), conv.getDate());
                    mNotificationDate.setText(formatTextToBold(data));

                    String snippet = conv.getSnippet();
                    if (!TextUtils.isEmpty(snippet)) {
                        mNotificationSubject.setSingleLine(false);
                        mNotificationSubject.setMaxLines(getResources().
                                getInteger(R.integer.
                                        max_unread_message_lines));
                        mNotificationSubject.setText(formatTextToBold(snippet));
                    } else {
                        mNotificationSubject.setVisibility(View.GONE);
                    }

                    String label = getString(R.string.
                                    unread_notification_message_label,
                            mUnreadCount);
                    mNotificationLabel.setText(formatTextToBold(label));
                    mNotificationView.setBackground(getResources().
                            getDrawable(R.drawable.
                                    conversation_item_background_unread));
                } else {
                    mNotificationDate.setText(MessageUtils.
                            formatTimeStampString(getApplicationContext(),
                                    conv.getDate()));

                    String snippet = conv.getSnippet();
                    if (!TextUtils.isEmpty(snippet)) {
                        mNotificationSubject.setSingleLine(true);
                        mNotificationSubject.setText(snippet);
                        mNotificationSubject.setVisibility(View.VISIBLE);
                    } else {
                        mNotificationSubject.setVisibility(View.GONE);
                    }

                    mNotificationLabel.setText(
                            getString(R.string.notification_message_label));
                    mNotificationView.setBackground(getResources().
                            getDrawable(R.drawable.
                                    conversation_item_background_read));
                }
            }
        }.execute(cursor);
    }

    private CharSequence formatTextToBold(String content) {
        SpannableStringBuilder buf = null;
        if (content != null) {
            buf = new SpannableStringBuilder(content);
            buf.setSpan(ConversationListItem.STYLE_BOLD, 0, buf.length(),
                    Spannable.SPAN_INCLUSIVE_EXCLUSIVE);
        }
        return buf;
    }

    private void updateEmptyView() {
        SharedPreferences sp =
                PreferenceManager.getDefaultSharedPreferences(getApplicationContext());
        if (sp.getBoolean(MessagingPreferenceActivity.SEPERATE_NOTIFI_MSG, true)) {
            if (mHasNotificationThreads || mHasNormalThreads) {
                mEmptyView.setVisibility(View.GONE);
            } else {
                mEmptyView.setVisibility(View.VISIBLE);
            }
        } else {
            if (mHasNormalThreads) {
                mEmptyView.setVisibility(View.GONE);
            } else {
                mEmptyView.setVisibility(View.VISIBLE);
            }
        }
    }

    private ProgressDialog createProgressDialog() {
        ProgressDialog dialog = new ProgressDialog(this);
        dialog.setIndeterminate(true);
        dialog.setProgressStyle(ProgressDialog.STYLE_SPINNER);
        dialog.setCanceledOnTouchOutside(false);
        dialog.setCancelable(false);
        dialog.setMessage(getText(R.string.deleting_threads));
        return dialog;
    }

    private Runnable mDeletingRunnable = new Runnable() {
        @Override
        public void run() {
            mHandler.postDelayed(mShowProgressDialogRunnable, DELAY_TIME);
        }
    };

    private Runnable mShowProgressDialogRunnable = new Runnable() {
        @Override
        public void run() {
            if (mProgressDialog != null) {
                mProgressDialog.show();
            }
        }
    };

    public void checkAll() {
        int count = getListAdapter().getCount();

        for (int i = 1; i <= count; i++) {
            getListView().setItemChecked(i, true);
        }
        mListAdapter.notifyDataSetChanged();
    }

    public void unCheckAll() {
        int count = getListAdapter().getCount();

        for (int i = 1; i <= count; i++) {
            getListView().setItemChecked(i, false);
        }
        mListAdapter.notifyDataSetChanged();
    }

    private class ModeCallback implements ListView.MultiChoiceModeListener {
        private View mMultiSelectActionBarView;
        private TextView mSelectedConvCount;
        private ImageView mSelectedAll;
        private boolean mHasSelectAll = false;
        private HashSet<Long> mSelectedThreadIds;
        // build action bar with a spinner
        private SelectionMenu mSelectionMenu;
        private Menu mMenu;

        private void updateMenu(ActionMode mode, boolean isCheck) {
            ListView listView = getListView();
            int count = listView.getCount();
            if (isCheck) {
                mSelectionMenu.setTitle(getString(R.string.selected_count, count));
                mHasSelectAll = true;
                mSelectionMenu.updateSelectAllMode(mHasSelectAll);
            } else {
                mHasSelectAll = false;
            }
        }

        @Override
        public boolean onCreateActionMode(final ActionMode mode, Menu menu) {
            View floatingBtnContainer = findViewById(R.id.floating_action_button_container);
            if(null != floatingBtnContainer) {
                floatingBtnContainer.setVisibility(View.GONE);
            }
            disableNotificationGroup();
            getWindow().setStatusBarColor(
                    getResources().getColor(R.color.statubar_select_background));
            MenuInflater inflater = getMenuInflater();
            mSelectedThreadIds = new HashSet<Long>();
            inflater.inflate(R.menu.conversation_multi_select_menu, menu);
            mMenu = menu;
            mMultiChoiceMode = true;

            if (mMultiSelectActionBarView == null) {
                mMultiSelectActionBarView = LayoutInflater.from(ConversationList.this).inflate(
                        R.layout.action_mode, null);
            }
            mode.setCustomView(mMultiSelectActionBarView);
            mSelectionMenu = new SelectionMenu(ConversationList.this,
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
                    },
                    (ImageView)mMultiSelectActionBarView.findViewById(R.id.expand));
            return true;
        }

        @Override
        public boolean onPrepareActionMode(ActionMode mode, Menu menu) {
            prepareActionMode(mode);
            return true;
        }

        @Override
        public boolean onActionItemClicked(ActionMode mode, MenuItem item) {
            switch (item.getItemId()) {
                case R.id.delete:
                    if (mSelectedThreadIds.size() > 0) {
                        confirmDeleteThreads(mSelectedThreadIds, mQueryHandler);
                    }
                    mode.finish();
                    break;
                default:
                    break;
            }
            return true;
        }

        private void prepareActionMode(ActionMode mode) {
            if (mMultiSelectActionBarView == null) {
                ViewGroup v = (ViewGroup)LayoutInflater.from(ConversationList.this)
                    .inflate(R.layout.conversation_list_multi_select_actionbar, null);
                mode.setCustomView(v);

                mSelectedConvCount = (TextView)v.findViewById(R.id.selected_conv_count);
            }
        }

        @Override
        public void onDestroyActionMode(ActionMode mode) {
            View floatingBtnContainer = findViewById(R.id.floating_action_button_container);
            if(null != floatingBtnContainer) {
                floatingBtnContainer.setVisibility(View.VISIBLE);
            }
            enableNotificationGroup();
            getWindow().setStatusBarColor(getResources().getColor(R.color.primary_color_dark));
            Iterator<Long> it = mSelectedThreadIds.iterator();
            while (it.hasNext()) {
                Conversation.get(getApplicationContext(), it.next(), false).setIsChecked(false);
            }
            mSelectedThreadIds = null;
            mSelectionMenu.dismiss();
            mMultiChoiceMode = false;
        }

        @Override
        public void onItemCheckedStateChanged(ActionMode mode,
                int position, long id, boolean checked) {
            ListView listView = getListView();
            if (position < listView.getHeaderViewsCount()) {
                return;
            }
            final int checkedCount = listView.getCheckedItemCount();
            mSelectionMenu.setTitle(getApplicationContext()
                    .getString(R.string.selected_count, checkedCount));
            if (getListAdapter().getCount() == checkedCount) {
                mHasSelectAll = true;
            } else {
                mHasSelectAll = false;
            }
            mSelectionMenu.updateSelectAllMode(mHasSelectAll);
            mSelectionMenu.updateCheckedCount();

            Cursor cursor  = (Cursor)listView.getItemAtPosition(position);
            Conversation conv = Conversation.from(ConversationList.this, cursor);
            conv.setIsChecked(checked);
            long threadId = conv.getThreadId();
            if (checked) {
                mSelectedThreadIds.add(threadId);
            } else {
                mSelectedThreadIds.remove(threadId);
            }
            mListAdapter.notifyDataSetChanged();
        }

    }

    private void log(String format, Object... args) {
        String s = String.format(format, args);
        Log.d(TAG, "[" + Thread.currentThread().getId() + "] " + s);
    }
}
