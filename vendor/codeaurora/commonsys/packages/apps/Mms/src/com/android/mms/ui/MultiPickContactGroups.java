/**
 * Copyright (C) 2013, 2016-2017, The Linux Foundation. All Rights Reserved.
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

import java.lang.ref.WeakReference;
import android.app.ListActivity;
import android.content.AsyncQueryHandler;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.provider.ContactsContract.CommonDataKinds.GroupMembership;
import android.provider.ContactsContract.CommonDataKinds.Phone;
import android.provider.ContactsContract.Data;
import android.provider.ContactsContract.Groups;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CursorAdapter;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import com.android.mms.R;

public class MultiPickContactGroups extends ListActivity implements
        View.OnClickListener {
    private final static String TAG = "MultiPickContactGroups";
    private final static boolean DEBUG = true;

    public static final String RESULT_KEY = "result";
    public final static String MAX_CONTACT_COUNT = "max_contact_count";
    private static final int LOCAL_GROUP_QUERY_TOKEN = 55;
    private static final int PHONE_CONTACT_QUERY_TOKEN = 56;

    private static final String[] CONTACTS_GROUP_PROJECTION = new String[] {
        Groups._ID,
        Groups.TITLE,
        Groups.SUMMARY_COUNT,
    };

    private static final int CONTACTS_GROUP_ID_INDEX = 0;
    private static final int CONTACTS_GROUP_TITLE_INDEX = 1;
    private static final int CONTACTS_GROUP_COUNT_INDEX = 2;

    private static final String[] PHONE_CONTACTS_PROJECTION = new String[] {
        Phone._ID, // 0
        Phone.NUMBER, // 1
    };
    private static final int PHONE_CONTACTS_ID_INDEX = 0;
    private static final int PHONE_CONTACTS_NUMBER_INDEX = 1;

    private static final String DATA_JOIN_MIMETYPES = "data "
            + "JOIN mimetypes ON (data.mimetype_id = mimetypes._id)";
    private static final String QUERY_PHONE_ID_IN_LOCAL_GROUP = Data.MIMETYPE
            + "='" + Phone.CONTENT_ITEM_TYPE + "'"
            + " AND "
            + " raw_contact_id"
            + " IN "
            + "(SELECT "
            + "data.raw_contact_id"
            + " FROM "
            + DATA_JOIN_MIMETYPES
            + " WHERE "
            + Data.MIMETYPE
            + "='"
            + GroupMembership.CONTENT_ITEM_TYPE
            + "'";// need add group id

    public static final int PHONE_CONTACTS_COLUMN_ID = 0;
    public static final int PHONE_CONTACTS_COLUMN_NUMBER = 1;

    private LocalGroupListAdapter mAdapter;
    private QueryHandler mQueryHandler;
    private Bundle mChoiceSet;
    private Bundle mBackupChoiceSet;
    private Button mOKButton;
    private Button mCancelButton;
    private CheckBox mSelectAllCheckBox;
    private TextView mSelectAllLabel;

    private Context mContext;
    private Intent mIntent;
    private int mMaxContactCount = 0;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (MessageUtils.checkPermissionsIfNeeded(this)) {
            return;
        }
        Intent intent = getIntent();
        mMaxContactCount = intent.getIntExtra(MAX_CONTACT_COUNT, 0);
        setContentView(R.layout.pick_contact_group);
        mChoiceSet = new Bundle();
        mAdapter = new LocalGroupListAdapter(this);
        getListView().setAdapter(mAdapter);
        mQueryHandler = new QueryHandler(this);
        mContext = getApplicationContext();
        initResource();
        startQuery(LOCAL_GROUP_QUERY_TOKEN);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Let the system ignore the menu key when the activity is foreground.
        return false;
    }

    private void initResource() {
        mOKButton = (Button) findViewById(R.id.btn_ok);
        mOKButton.setOnClickListener(this);
        mOKButton.setText(getOKString());
        mCancelButton = (Button) findViewById(R.id.btn_cancel);
        mCancelButton.setOnClickListener(this);
        mSelectAllCheckBox = (CheckBox) findViewById(R.id.select_all_check);
        mSelectAllCheckBox.setOnClickListener(this);
        mSelectAllLabel = (TextView) findViewById(R.id.select_all_label);
    }

    @Override
    protected void onListItemClick(ListView l, View v, int position, long id) {
        CheckBox checkBox = (CheckBox) v.findViewById(R.id.pick_group_check);
        boolean isChecked = checkBox.isChecked() ? false : true;
        checkBox.setChecked(isChecked);
        if (isChecked) {
            String[] value = null;
            ContactItemCache cache = (ContactItemCache) v.getTag();
            value = new String[] { String.valueOf(cache.group_id),
                    cache.group_title, cache.group_count };
            mChoiceSet.putStringArray(String.valueOf(id), value);
            if(mChoiceSet.size() == l.getChildCount() && !mSelectAllCheckBox.isChecked()) {
               mSelectAllCheckBox.setChecked(true);
            }
        } else {
            mChoiceSet.remove(String.valueOf(id));
            mSelectAllCheckBox.setChecked(false);
        }
        mOKButton.setText(getOKString());
    }

    private String getOKString() {
        int selectedCount = 0;
        Cursor cursor = mAdapter.getCursor();
        if (cursor == null) {
            Log.w(TAG, "cursor is null.");
        } else {
            cursor.moveToPosition(-1);
            while (cursor.moveToNext()) {
                String id = String.valueOf(cursor.getLong(CONTACTS_GROUP_ID_INDEX));
                if (mChoiceSet.containsKey(id)) {
                    selectedCount += cursor.getLong(CONTACTS_GROUP_COUNT_INDEX);
                }
            }
        }

        mOKButton.setEnabled(selectedCount != 0);

        return getString(R.string.yes) + "(" + selectedCount + ")";
    }

    private void backupChoiceSet() {
        mBackupChoiceSet = (Bundle) mChoiceSet.clone();
    }

    private void restoreChoiceSet() {
        mChoiceSet = mBackupChoiceSet;
    }

    public void onClick(View v) {
        int id = v.getId();
        switch (id) {
            case R.id.btn_ok:
                startQuery(PHONE_CONTACT_QUERY_TOKEN);
                break;
            case R.id.btn_cancel:
                this.setResult(this.RESULT_CANCELED);
                finish();
                break;
            case R.id.select_all_check:
                selectAll(mSelectAllCheckBox.isChecked());
                break;
        }
    }

    @Override
    public void onDestroy() {
        mQueryHandler.removeCallbacksAndMessages(LOCAL_GROUP_QUERY_TOKEN);
        mQueryHandler.removeCallbacksAndMessages(PHONE_CONTACT_QUERY_TOKEN);
        if (mAdapter.getCursor() != null) {
            mAdapter.getCursor().close();
        }

        if(preFilterCursor != null) {
            preFilterCursor.close();
        }
        super.onDestroy();
    }

    private Uri getUriToQuery(int type) {
        switch (type) {
            case LOCAL_GROUP_QUERY_TOKEN:
                return Groups.CONTENT_SUMMARY_URI;
            case PHONE_CONTACT_QUERY_TOKEN:
                return Phone.CONTENT_URI;
        }
        return null;
    }

    public String[] getProjectionForQuery(int type) {
        switch (type) {
            case LOCAL_GROUP_QUERY_TOKEN:
                return CONTACTS_GROUP_PROJECTION;
            case PHONE_CONTACT_QUERY_TOKEN:
                return PHONE_CONTACTS_PROJECTION;
        }
        return null;
    }

    public String getSelectionForQuery(int type) {
        switch (type) {
            case PHONE_CONTACT_QUERY_TOKEN:
                if (mChoiceSet != null) {
                    StringBuilder buf = new StringBuilder();
                    int i = 0;
                    buf.append(QUERY_PHONE_ID_IN_LOCAL_GROUP).append(" AND (");
                    for (String groupId : mChoiceSet.keySet()) {
                        if (i > 0) {
                            buf.append(" OR ");
                        }
                        buf.append(GroupMembership.GROUP_ROW_ID).append("=?");
                        i ++;
                    }
                    return buf.append("))").toString();
                }
        }
        return null;
    }

    public String[] getSelectionArgsForQuery(int type) {
        switch (type) {
            case PHONE_CONTACT_QUERY_TOKEN:
                if (mChoiceSet != null) {
                    String[] selectionArgs = new String[mChoiceSet.size()];
                    int i = 0;
                    for (String groupId : mChoiceSet.keySet()) {
                        selectionArgs[i++] = groupId;
                    }
                    return selectionArgs;
                }
                break;
        }
        return null;
    }

    private String getSortOrder(int type) {
        switch (type) {
            case LOCAL_GROUP_QUERY_TOKEN:
                return CONTACTS_GROUP_PROJECTION[1] + " asc";
            case PHONE_CONTACT_QUERY_TOKEN:
                return PHONE_CONTACTS_PROJECTION[0] + " asc";
        }
        return null;
    }

    public void startQuery(int type) {
        Uri uri = getUriToQuery(type);
        String[] projection = getProjectionForQuery(type);
        String selection = getSelectionForQuery(type);
        String[] selectionArgs = getSelectionArgsForQuery(type);
        mQueryHandler.startQuery(type, null, uri, projection, selection,
                selectionArgs, getSortOrder(type));
    }

    public void updateContent() {
        startQuery(PHONE_CONTACT_QUERY_TOKEN);
    }

    private void selectAll(boolean isSelected) {
        // update mChoiceSet.
        // TODO: make it more efficient
        Cursor cursor = mAdapter.getCursor();
        if (cursor == null) {
            if (DEBUG) Log.w(TAG, "cursor is null.");
            return;
        }

        cursor.moveToPosition(-1);
        while (cursor.moveToNext()) {
            String[] value = null;
            String group_id = cursor.getString(CONTACTS_GROUP_ID_INDEX);
            String group_title = cursor.getString(CONTACTS_GROUP_TITLE_INDEX);
            String group_count = cursor.getInt(CONTACTS_GROUP_COUNT_INDEX) + "";
            value = new String[] {group_id, group_title, group_count};
            if (isSelected) {
                mChoiceSet.putStringArray(group_id, value);
            } else {
                mChoiceSet.remove(group_id);
            }
        }

        // update UI items.
        mOKButton.setText(getOKString());
        ListView list = getListView();
        int count = list.getChildCount();
        for (int i = 0; i < count; i++) {
            View v = list.getChildAt(i);
            CheckBox checkBox = (CheckBox) v.findViewById(R.id.pick_group_check);
            checkBox.setChecked(isSelected);
        }
    }

    private Cursor preFilterCursor = null;
    private class QueryHandler extends AsyncQueryHandler {
        protected  WeakReference<MultiPickContactGroups> mActivity;

        public QueryHandler(Context context) {
            super(context.getContentResolver());
            mActivity = new WeakReference<MultiPickContactGroups>(
                    (MultiPickContactGroups) context);
        }

        @Override
        protected void onQueryComplete(int token, Object cookie, Cursor cursor) {
            if(preFilterCursor != null) {
                preFilterCursor.close();
                preFilterCursor = null;
            }

            preFilterCursor = cursor;
            if (cursor == null || cursor.getCount() == 0) {
                Toast.makeText(mContext, R.string.no_contact_group,
                        Toast.LENGTH_SHORT).show();
            }

            // In the case of low memory, the WeakReference object may be
            // recycled.
            if (mActivity == null || mActivity.get() == null) {
                mActivity = new WeakReference<MultiPickContactGroups>(
                        MultiPickContactGroups.this);
            }
            final MultiPickContactGroups activity = mActivity.get();
            if (token == LOCAL_GROUP_QUERY_TOKEN) {
                activity.mAdapter.changeCursor(cursor);
            } else if (token == PHONE_CONTACT_QUERY_TOKEN && cursor != null) {
                Intent intent = new Intent();
                Bundle bundle = new Bundle();
                Bundle result = new Bundle();
                cursor.moveToPosition(-1);
                Log.e(TAG, "query phone count" + cursor.getCount());
                while (cursor.moveToNext()) {
                    String id = String.valueOf(
                                cursor.getLong(PHONE_CONTACTS_ID_INDEX));
                    String number = String.valueOf(
                                cursor.getLong(PHONE_CONTACTS_NUMBER_INDEX));
                    result.putString(id, number);
                }
                bundle.putBundle(RESULT_KEY, result);
                intent.putExtras(bundle);
                activity.setResult(RESULT_OK, intent);
                finish();
            }
        }
    }

    private final class ContactItemCache {
        long group_id;
        String group_title;
        String group_count;
    }

    private final class LocalGroupListAdapter extends CursorAdapter {
        Context mContext;
        protected LayoutInflater mInflater;

        public LocalGroupListAdapter(Context context) {
            super(context, null, false);

            mContext = context;
            mInflater = (LayoutInflater) context
                    .getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        }

        @Override
        public void bindView(View view, Context context, Cursor cursor) {
            ContactItemCache cache = (ContactItemCache) view.getTag();
            cache.group_id = cursor.getLong(CONTACTS_GROUP_ID_INDEX);
            cache.group_title = cursor.getString(CONTACTS_GROUP_TITLE_INDEX);
            cache.group_count = cursor.getInt(CONTACTS_GROUP_COUNT_INDEX) + "";
            ((TextView) view.findViewById(R.id.pick_group_name))
                    .setText(cache.group_title);
            ((TextView) view.findViewById(R.id.pick_group_memnber_count))
                    .setText("(" + cache.group_count + ")");
            CheckBox checkBox = (CheckBox) view
                    .findViewById(R.id.pick_group_check);
            if (mChoiceSet.containsKey(String.valueOf(cache.group_id))) {
                checkBox.setChecked(true);
            } else {
                checkBox.setChecked(false);
            }
        }

        @Override
        public View newView(Context context, Cursor cursor, ViewGroup parent) {
            View v = null;
            v = mInflater.inflate(R.layout.pick_contact_group_item, parent, false);
            ContactItemCache cache = new ContactItemCache();
            v.setTag(cache);
            return v;
        }

        public View getView(int position, View convertView, ViewGroup parent) {
            View v;
            Cursor cursor = getCursor();
            if (null != cursor && !cursor.moveToPosition(position)) {
                throw new IllegalStateException(
                        "couldn't move cursor to position " + position);
            }
            if (convertView != null && convertView.getTag() != null) {
                v = convertView;
            } else {
                v = newView(mContext, cursor, parent);
            }
            bindView(v, mContext, cursor);
            return v;
        }

        @Override
        protected void onContentChanged() {
            startQuery(LOCAL_GROUP_QUERY_TOKEN);
        }

        @Override
        public void changeCursor(Cursor cursor) {
            super.changeCursor(cursor);
            if (cursor == null || cursor.getCount() == 0) {
                mSelectAllCheckBox.setChecked(false);
                mSelectAllLabel.setTextColor(Color.GRAY);
                mSelectAllCheckBox.setClickable(false);
            } else if (cursor.getCount() > mChoiceSet.size()) {
                mSelectAllCheckBox.setChecked(false);
            } else {
                mSelectAllCheckBox.setChecked(false);
            }
        }
    }

    protected static void log(String msg) {
        if (DEBUG)  Log.d(TAG, msg);
    }
}
