/*
 * Copyright (c) 2014, 2016, The Linux Foundation. All rights reserved.
 * Not a Contribution.
 * Copyright (C) 2007 The Android Open Source Project.
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

import android.app.ExpandableListActivity;
import android.content.AsyncQueryHandler;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.ContactsContract;
import android.provider.ContactsContract.CommonDataKinds.Email;
import android.provider.ContactsContract.CommonDataKinds.Phone;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.ExpandableListView;
import android.widget.ImageView;
import android.widget.RadioButton;
import android.widget.SectionIndexer;
import android.widget.SimpleCursorTreeAdapter;
import android.widget.TextView;

import com.android.mms.R;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map.Entry;
import java.util.Set;

public class MultiPickContactsActivity extends ExpandableListActivity implements OnClickListener,
        TextWatcher {
    private static final String TAG = "MultiPickContacts";
    private static final boolean DEBUG = false;

    public static final String MODE = "mode";
    public static final int MODE_INFO = 1;
    public static final int MODE_VCARD = 2;

    public static final String EXTRA_INFO = "info";
    public static final String EXTRA_VCARD = "vcard";

    private int mMode = -1;

    private ExpandableListView mList = null;
    private ContactsAdapter mAdapter = null;
    private Cursor mContactCursor = null;

    private MySelected mSelected;

    private Button mOkBtn;
    private Button mCancelBtn;
    private EditText mSearchText;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (MessageUtils.checkPermissionsIfNeeded(this)) {
            return;
        }
        // get the mode from the intent or saved instance.
        if (savedInstanceState != null) {
            mMode = savedInstanceState.getInt(MODE);
        } else {
            mMode = getIntent().getIntExtra(MODE, MODE_INFO);
        }

        setContentView(R.layout.pick_contact);
        switch (mMode) {
            case MODE_INFO:
                setTitle(R.string.attach_add_contact_as_text);
                break;
            case MODE_VCARD:
                setTitle(R.string.attach_add_contact_as_vcard);
                break;
            default:
                Log.e(TAG, "shouldn't be here.");
        }

        mSelected = new MySelected();

        mOkBtn = (Button) findViewById(R.id.btn_ok);
        mOkBtn.setOnClickListener(this);
        setOKButtonState();
        mCancelBtn = (Button) findViewById(R.id.btn_cancel);
        mCancelBtn.setOnClickListener(this);
        mSearchText = (EditText) findViewById(R.id.search_field);
        mSearchText.addTextChangedListener(this);

        mList = getExpandableListView();

        if (mAdapter == null) {
            mAdapter = new ContactsAdapter(getApplication(), this, null,
                    R.layout.pick_contact_item, new String[] {}, new int[] {},
                    R.layout.pick_contact_item, new String[] {}, new int[] {});
            setListAdapter(mAdapter);
            getContactsCursor(mAdapter.getQueryHandler(), null);
        }
    }

    @Override
    protected void onDestroy() {
        if (mAdapter != null) {
            mAdapter.changeCursor(null);
            setListAdapter(null);
            mAdapter = null;
        }
        super.onDestroy();
    }

    @Override
    public boolean onChildClick(ExpandableListView parent, View v, int groupPosition,
            int childPosition, long id) {
        // We set the children's check box clickable as false.
        // So we need deal with the click action, and change the status.
        if (mMode == MODE_VCARD)
            return false;
        CheckBox cb = (CheckBox) v.findViewById(R.id.pick_item_checkbox);
        MyChildData data = (MyChildData) cb.getTag();
        if (DEBUG)
            Log.i(TAG, "The child click, and contactId:" + data.mContactId
                    + " , childPosition:" + data.mChildPosition);

        if (data.mChildPosition != childPosition) {
            Log.w(TAG, "The child click, the groupPosition(" + groupPosition
                    + ") ,and the childPosition(" + childPosition
                    + ") is not same as saved childPosition("
                    + data.mChildPosition + ")");
            return super.onChildClick(parent, v, groupPosition, childPosition, id);
        }

        boolean checked = cb.isChecked();
        if (checked) {
            cb.setChecked(false);
            mSelected.removeSelected(data);
        } else {
            cb.setChecked(true);
            mSelected.addSelected(data);
        }
        setOKButtonState();
        return super.onChildClick(parent, v, groupPosition, childPosition, id);
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.btn_ok:
                Intent intent = new Intent();
                if (getSelectedResult()) {
                    putExtraWithContact(intent);
                    setResult(RESULT_OK, intent);
                } else {
                    setResult(RESULT_CANCELED);
                }
                finish();
                break;
            case R.id.btn_cancel:
                setResult(RESULT_CANCELED);
                finish();
                break;
            case R.id.pick_item_checkbox:
                /**
                 * When we get the check box or radio button onClick event, the
                 * status of the check box had been changed. So we won't set the
                 * checked status, but only modify the selected content.
                 */
                CheckBox cb = (CheckBox) v;
                long contactId = (Long) cb.getTag();
                boolean cbCecked = cb.isChecked();
                if (cbCecked) {
                    mSelected.addSelected(contactId);
                } else {
                    mSelected.removeSelected(contactId);
                }
                setOKButtonState();
                break;
            case R.id.pick_item_radiobutton:
                RadioButton rb = (RadioButton) v;
                String lookupKey = (String) rb.getTag();
                boolean rbChecked = rb.isChecked();
                if (rbChecked) {
                    mSelected.addSelected(lookupKey);
                }
                setOKButtonState();
                break;
        }
    }

    private boolean getSelectedResult() {
        return ((mMode == MODE_INFO && !TextUtils.isEmpty(mSelected.getSelectedAsText()))
            || (mMode == MODE_VCARD && null != mSelected.getSelectedAsVcard()));
    }

    private void putExtraWithContact(Intent intent) {
        if (mMode == MODE_INFO) {
            intent.putExtra(EXTRA_INFO, mSelected.getSelectedAsText());
        } else if (mMode == MODE_VCARD) {
            intent.putExtra(EXTRA_VCARD, mSelected.getSelectedAsVcard().toString());
        }
    }

    @Override
    public void afterTextChanged(Editable s) {
        if (mAdapter != null) {
            Cursor cursor = mAdapter.runQueryOnBackgroundThread(s);
            init(cursor);
        }
    }

    @Override
    public void beforeTextChanged(CharSequence s, int start, int count, int after) {
        // do nothing
    }

    @Override
    public void onTextChanged(CharSequence s, int start, int before, int count) {
        // do nothing
    }

    private void setOKButtonState() {
        if (mMode == MODE_INFO) {
            if (mSelected.mSelectedMap != null) {
                if (mSelected.mSelectedMap.size() > 0) {
                    mOkBtn.setEnabled(true);
                } else {
                    mOkBtn.setEnabled(false);
                }
            } else {
                mOkBtn.setEnabled(false);
            }
        } else if (mMode == MODE_VCARD) {
            if (mSelected.mSelectedContactLookupKey != null) {
                mOkBtn.setEnabled(true);
            } else {
                mOkBtn.setEnabled(false);
            }
        }
    }

    private void init(Cursor c) {
        if (mAdapter == null) {
            return;
        }
        mAdapter.changeCursor(c);
    }

    private Cursor getContactsCursor(AsyncQueryHandler async, String filter) {
        String[] cols = new String[] {
                ContactsContract.Contacts._ID,
                ContactsContract.Contacts.LOOKUP_KEY,
                ContactsContract.Contacts.DISPLAY_NAME,
                ContactsContract.Contacts.SORT_KEY_PRIMARY
        };

        Uri uri = ContactsContract.Contacts.CONTENT_URI;
        if (!TextUtils.isEmpty(filter)) {
            uri = Uri.withAppendedPath(ContactsContract.Contacts.CONTENT_FILTER_URI,
                    Uri.encode(filter.toString()));
        }

        Cursor ret = null;
        if (async != null) {
            async.startQuery(0, null, uri,
                    cols, null, null, ContactsContract.Contacts.SORT_KEY_PRIMARY);
        } else {
            ret = getContentResolver().query(uri, cols, null, null,
                    ContactsContract.Contacts.SORT_KEY_PRIMARY);
        }
        return ret;
    }

    private Cursor getContactsDetailCursor(String contactId) {
        StringBuilder selection = new StringBuilder();
        selection.append(ContactsContract.Data.CONTACT_ID + "=" + contactId)
                .append(" AND (")
                .append(ContactsContract.Data.MIMETYPE + "='" + Phone.CONTENT_ITEM_TYPE + "'")
                .append(" OR ")
                .append(ContactsContract.Data.MIMETYPE + "='" + Email.CONTENT_ITEM_TYPE + "')");

        Cursor cursor = getContentResolver().query(
                ContactsContract.Data.CONTENT_URI, null, selection.toString(), null, null);

        return cursor;
    }

    private class ContactsAdapter extends SimpleCursorTreeAdapter {
        private MultiPickContactsActivity mActivity;
        private AsyncQueryHandler mQueryHandler;
        private String mConstraint = null;
        private boolean mConstraintIsValid = false;

        private int mContactIdIndex = -1;
        private int mContactLookupIndex = -1;
        private int mDisplayNameIndex = -1;
        private int mSortedIndex = -1;

        class QueryHandler extends AsyncQueryHandler {

            public QueryHandler(ContentResolver res) {
                super(res);
            }

            @Override
            protected void onQueryComplete(int token, Object cookie, Cursor cursor) {
                if (null == mActivity || mActivity.isFinishing()) {
                    if (null != cursor) {
                        cursor.close();
                    }
                    Log.w(TAG, "onQueryComplete activity finished.");
                    return;
                }
                mActivity.init(cursor);
            }
        }

        public ContactsAdapter(Context context, MultiPickContactsActivity activity, Cursor cursor,
                int expandedGroupLayout, String[] groupFrom, int[] groupTo, int childLayout,
                String[] childFrom, int[] childTo) {
            super(context, cursor, expandedGroupLayout, groupFrom, groupTo,
                    childLayout, childFrom, childTo);
            mActivity = activity;
            mQueryHandler = new QueryHandler(context.getContentResolver());
            getColumnIndex(cursor);
        }

        public AsyncQueryHandler getQueryHandler() {
            return mQueryHandler;
        }

        @Override
        protected void bindGroupView(View view, Context context, Cursor cursor,
                boolean isExpanded) {
            final int groupPosition = cursor.getPosition();

            TextView tv = (TextView) view.findViewById(R.id.pick_item_detail);
            String displayName = cursor.getString(mDisplayNameIndex);
            tv.setText(displayName);
            tv.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    boolean expand = mList.isGroupExpanded(groupPosition);
                    if (expand) {
                        mList.collapseGroup(groupPosition);
                    } else {
                        mList.expandGroup(groupPosition);
                    }
                }
            });

            ImageView iv = (ImageView) view.findViewById(R.id.pick_item_type);
            iv.setVisibility(View.GONE);

            CheckBox cb = (CheckBox) view.findViewById(R.id.pick_item_checkbox);
            RadioButton rb = (RadioButton) view.findViewById(R.id.pick_item_radiobutton);
            if (mMode == MODE_INFO) {
                rb.setVisibility(View.GONE);

                long contactId = cursor.getLong(mContactIdIndex);
                cb.setTag(contactId);
                cb.setChecked(mSelected.isSelected(contactId));
                cb.setOnClickListener(mActivity);
            } else {
                cb.setVisibility(View.GONE);

                String lookupKey = cursor.getString(mContactLookupIndex);
                rb.setTag(lookupKey);
                rb.setChecked(mSelected.isSelected(lookupKey));
                rb.setOnClickListener(mActivity);
            }
        }

        @Override
        protected void bindChildView(View view, Context context, Cursor cursor,
                boolean isLastChild) {
            ImageView iv = (ImageView) view.findViewById(R.id.pick_item_type);
            TextView tv = (TextView) view.findViewById(R.id.pick_item_detail);

            int typeResId = -1;
            String detail = null;
            String mimetype = cursor.getString(cursor
                    .getColumnIndexOrThrow(ContactsContract.Data.MIMETYPE));
            if (mimetype == null) {
                Log.e(TAG, "bindChildView, the mimetype is null.");
                return;
            } else if (mimetype.equals(Phone.CONTENT_ITEM_TYPE)) {
                // phone
                typeResId = android.R.drawable.ic_menu_call;
                detail = cursor.getString(cursor.getColumnIndexOrThrow(Phone.NUMBER));
            } else if (mimetype.equals(Email.CONTENT_ITEM_TYPE)) {
                // email
                typeResId = R.drawable.ic_list_email;
                detail = cursor.getString(cursor.getColumnIndexOrThrow(Email.ADDRESS));
            }
            iv.setImageResource(typeResId);
            tv.setText(detail);

            CheckBox cb = (CheckBox) view.findViewById(R.id.pick_item_checkbox);
            RadioButton rb = (RadioButton) view.findViewById(R.id.pick_item_radiobutton);
            rb.setVisibility(View.GONE);
            if (mActivity.mMode == MODE_VCARD) {
                // For the vCard mode, we will export all the info to the vCard.
                // So we needn't show the check box.
                cb.setVisibility(View.GONE);
            } else {
                int count = cursor.getCount();
                long contactId = cursor.getLong(cursor
                        .getColumnIndexOrThrow(ContactsContract.Data.CONTACT_ID));
                int childPosition = cursor.getPosition();

                MyChildData data = new MyChildData(contactId, childPosition);
                data.mCount = count;
                data.mMimeType = mimetype;
                data.mValue = detail;
                cb.setTag(data);

                cb.setChecked(mSelected.isSelected(data));
                cb.setClickable(false);
                cb.setFocusable(false);
            }
        }

        @Override
        protected Cursor getChildrenCursor(Cursor groupCursor) {
            String contactId = groupCursor.getString(mContactIdIndex);
            return getContactsDetailCursor(contactId);
        }

        @Override
        public void changeCursor(Cursor cursor) {
            if (mActivity.isFinishing() && cursor != null) {
                cursor.close();
                cursor = null;
            }
            if (cursor != mActivity.mContactCursor) {
                mActivity.mContactCursor = cursor;
                getColumnIndex(cursor);
                super.changeCursor(cursor);
            }
        }

        @Override
        public Cursor runQueryOnBackgroundThread(CharSequence constraint) {
            String s = constraint.toString();
            if (mConstraintIsValid && (
                    (s == null && mConstraint == null) ||
                    (s != null && s.equals(mConstraint)))) {
                return getCursor();
            }
            Cursor c = mActivity.getContactsCursor(null, s);
            mConstraint = s;
            mConstraintIsValid = true;
            return c;
        }

        private void getColumnIndex(Cursor cursor) {
            if (cursor == null) {
                Log.w(TAG, "getColumnsIndex, the cursor is null, couldn't get the index.");
                return;
            }
            mContactIdIndex = cursor.getColumnIndexOrThrow(ContactsContract.Contacts._ID);
            mContactLookupIndex = cursor
                    .getColumnIndexOrThrow(ContactsContract.Contacts.LOOKUP_KEY);
            mDisplayNameIndex = cursor
                    .getColumnIndexOrThrow(ContactsContract.Contacts.DISPLAY_NAME);
            mSortedIndex = cursor.getColumnIndexOrThrow(ContactsContract.Contacts.SORT_KEY_PRIMARY);

        }
    }

    private class MyChildData {
        long mContactId;
        int mChildPosition;

        int mCount;
        String mMimeType;
        String mValue;

        public MyChildData(long contactId, int childPosition) {
            mContactId = contactId;
            mChildPosition = childPosition;
        }

        @Override
        public boolean equals(Object o) {
            if (o instanceof MyChildData) {
                MyChildData temp = (MyChildData) o;
                if (temp.mContactId == this.mContactId
                        && temp.mChildPosition == this.mChildPosition) {
                    return true;
                }
            }
            return false;
        }
    }

    private class MySelected {
        HashMap<String, ArrayList<MyChildData>> mSelectedMap = null;
        String mSelectedContactLookupKey;

        private static final int PARTLY_SELECTED = 1;
        private static final int ALL_SELECTED = -1;

        private static final String KEY_SEP = ",";
        private static final String ITEM_SEP = ", ";
        private static final String CONTACT_SEP_LEFT = "[";
        private static final String CONTACT_SEP_RIGHT = "]";

        public MySelected() {
            if (mMode == MODE_INFO) {
                mSelectedMap = new HashMap<String, ArrayList<MyChildData>>();
            }
        }

        /**
         * Get the selected status of this contact.
         *
         * @param contactId The contact's id.
         * @return The current selected status of this contact.
         */
        public boolean isSelected(long contactId) {
            if (mMode == MODE_INFO) {
                return mSelectedMap.containsKey(getKeyString(contactId, ALL_SELECTED));
            } else {
                if (DEBUG)
                    Log.w(TAG, "vcard mode, isSelected from group, shouldn't be here.");
                return false;
            }
        }

        /**
         * Get the selected status of this contact.
         *
         * @param lookupKey The contact's lookupKey.
         * @return The current selected status of this contact.
         */
        public boolean isSelected(String lookupKey) {
            if (mMode == MODE_VCARD && !TextUtils.isEmpty(lookupKey)) {
                if (lookupKey.equals(mSelectedContactLookupKey)) {
                    return true;
                }
            }
            return false;
        }

        /**
         * Get the selected status of this contact's detail info. This function
         * should only called when the mode is info mode.
         *
         * @param data The child data which must contains the contactId, and the
         *            childPosition.
         * @return The current selected status of this detail.
         */
        public boolean isSelected(MyChildData data) {
            if (mMode == MODE_INFO) {
                if (mSelectedMap.containsKey(getKeyString(data.mContactId, ALL_SELECTED))) {
                    return true;
                } else {
                    String partKey = getKeyString(data.mContactId, PARTLY_SELECTED);
                    ArrayList<MyChildData> list = mSelectedMap.get(partKey);
                    if (list != null) {
                        // the selected map contains partKey
                        return list.contains(data);
                    } else {
                        return false;
                    }
                }
            } else {
                if (DEBUG)
                    Log.w(TAG, "vcard mode, isSelected from children, shouldn't be here.");
                return false;
            }
        }

        /**
         * Add the selected for group checked under info mode.
         *
         * @param contactId The contactId of the checked contact.
         */
        public void addSelected(long contactId) {
            if (mMode == MODE_INFO) {
                mSelectedMap.put(getKeyString(contactId, ALL_SELECTED), null);
                mSelectedMap.remove(getKeyString(contactId, PARTLY_SELECTED));
                mAdapter.notifyDataSetChanged();
            } else {
                if (DEBUG)
                    Log.w(TAG, "vcard mode, addSelected from group, shouldn't be here.");
            }
        }

        /**
         * Add the selected for group checked under vCard mode.
         *
         * @param lookupKey The lookupKey of the checked contact.
         */
        public void addSelected(String lookupKey) {
            if (mMode == MODE_VCARD) {
                if (!TextUtils.isEmpty(lookupKey)) {
                    mSelectedContactLookupKey = lookupKey;
                    mAdapter.notifyDataSetChanged();
                }
            } else {
                if (DEBUG)
                    Log.w(TAG, "info mode, addSelected from group, shouldn't be here.");
            }
        }

        /**
         * Add the selected for child checked under info mode.
         *
         * @param data The child data which contains the child info.
         */
        public void addSelected(MyChildData data) {
            if (mMode == MODE_INFO) {
                if (data.mChildPosition < 0) {
                    Log.e(TAG, "the selected child position is wrong:" + data.mChildPosition);
                    return;
                }

                String partKey = getKeyString(data.mContactId, PARTLY_SELECTED);
                ArrayList<MyChildData> list = mSelectedMap.get(partKey);
                boolean newList = false;
                if (list == null) {
                    list = new ArrayList<MyChildData>();
                    newList = true;
                }
                if (list.size() + 1 == data.mCount) {
                    /**
                     * It means all the children have been selected. So we need
                     * remove the partSelected and add the allSelected.
                     */
                    mSelectedMap.remove(partKey);
                    mSelectedMap.put(getKeyString(data.mContactId, ALL_SELECTED), null);
                } else if (list.size() + 1 < data.mCount) {
                    list.add(data);
                    if (newList) {
                        // if the list is new, we need put the list to the map.
                        mSelectedMap.put(partKey, list);
                    }
                } else {
                    Log.w(TAG,
                            "addSelected, contactId(" + data.mContactId + "), childPosition("
                                    + data.mChildPosition + "), count("
                                    + data.mCount
                                    + "), shouldn't be here caused by the selected list size("
                                    + (list.size() + 1)
                                    + ") would be larger than the count.");
                }

                mAdapter.notifyDataSetChanged();
            } else {
                if (DEBUG)
                    Log.w(TAG, "vcard mode, addSelected from children, shouldn't be here.");
            }
        }

        /**
         * Remove the selected from the group unchecked.
         *
         * @param contactId The contactId of the unchecked contact.
         */
        public void removeSelected(long contactId) {
            if (mMode == MODE_INFO) {
                mSelectedMap.remove(getKeyString(contactId, ALL_SELECTED));
                mSelectedMap.remove(getKeyString(contactId, PARTLY_SELECTED));
                mAdapter.notifyDataSetChanged();
            } else {
                if (DEBUG)
                    Log.w(TAG, "vcard mode, removeSelected from group, shouldn't be here.");
            }
        }

        /**
         * Remove the selected for child unchecked under info mode.
         *
         * @param data The child data which contains the child info.
         */
        public void removeSelected(MyChildData data) {
            if (mMode == MODE_INFO) {
                String partkey = getKeyString(data.mContactId, PARTLY_SELECTED);
                String allkey = getKeyString(data.mContactId, ALL_SELECTED);
                ArrayList<MyChildData> list = mSelectedMap.get(partkey);
                if (list != null) {
                    // The list isn't null, the map must contains partly
                    // selected.
                    if (list.size() - 1 == 0) {
                        /**
                         * The selected list only contains one child, and the
                         * user removed one selected, it means there isn't any
                         * child selected. So remove the partly selected from
                         * the map.
                         */
                        mSelectedMap.remove(partkey);
                    } else {
                        list.remove(data);
                    }
                } else {
                    /**
                     * The list is null, so it means the map contains all
                     * selected. So we need remove the all selected from the
                     * map.
                     */
                    mSelectedMap.remove(allkey);
                    if (data.mCount > 1) {
                        /**
                         * The count of this contact's detail info is more than
                         * one. And the map saved as all selected for the last
                         * status, so we need add the partly selected to the map
                         * and add the other children to the list.
                         */
                        list = new ArrayList<MyChildData>();
                        for (int i = 0; i < data.mCount; i++) {
                            if (i == data.mChildPosition)
                                continue;
                            list.add(new MyChildData(data.mContactId, i));
                        }
                        mSelectedMap.put(partkey, list);
                    }
                }
                mAdapter.notifyDataSetChanged();
            } else {
                if (DEBUG)
                    Log.w(TAG, "vcard mode, removeSelected from children, shouldn't be here.");
            }
        }

        /**
         * Get the selected as string, and this is used under info mode.
         *
         * @return The string contains the selected contact info.
         */
        public String getSelectedAsText() {
            StringBuilder result = new StringBuilder();

            Set<Entry<String, ArrayList<MyChildData>>> set = mSelectedMap.entrySet();
            for (Entry<String, ArrayList<MyChildData>> entry : set) {
                String key = entry.getKey();
                String[] items = key.split(KEY_SEP);
                String contactId = items[0];
                String marked = items[1];

                result.append(CONTACT_SEP_LEFT);
                // append the name string.
                result.append(getString(R.string.contact_info_text_as_name));
                Cursor contactCursor = getContentResolver().query(
                        ContactsContract.Contacts.CONTENT_URI, new String[] {
                            ContactsContract.Contacts.DISPLAY_NAME
                        },
                        ContactsContract.Contacts._ID + "=" + contactId, null, null);
                try {
                    if (contactCursor != null && contactCursor.moveToFirst()) {
                        result.append(contactCursor.getString(0));
                    }
                } finally {
                    if (contactCursor != null) {
                        contactCursor.close();
                        contactCursor = null;
                    }
                }

                Cursor cursor = getContactsDetailCursor(contactId);
                try {
                    ArrayList<MyChildData> list = entry.getValue();
                    if (cursor != null) {
                        int mimeIndex = cursor
                                .getColumnIndexOrThrow(ContactsContract.Data.MIMETYPE);
                        int phoneIndex = cursor.getColumnIndexOrThrow(Phone.NUMBER);
                        int emailIndex = cursor.getColumnIndexOrThrow(Email.ADDRESS);

                        if (list == null && marked.equals(Integer.toString(ALL_SELECTED))) {
                            // all selected, append the detail string.
                            while (cursor.moveToNext()) {
                                result.append(ITEM_SEP);
                                String mimeType = cursor.getString(mimeIndex);
                                if (mimeType.equals(Phone.CONTENT_ITEM_TYPE)) {
                                    result.append(getString(R.string.contact_info_text_as_phone));
                                    result.append(cursor.getString(phoneIndex));
                                } else if (mimeType.equals(Email.CONTENT_ITEM_TYPE)) {
                                    result.append(getString(R.string.contact_info_text_as_email));
                                    result.append(cursor.getString(emailIndex));
                                }
                            }
                        } else if (marked.equals(Integer.toString(PARTLY_SELECTED))) {
                            // partly selected, append the detail string.
                            for (MyChildData data : list) {
                                result.append(ITEM_SEP);
                                if (TextUtils.isEmpty(data.mMimeType)
                                        || TextUtils.isEmpty(data.mValue)) {
                                    // we need get the value from the database
                                    if (cursor.moveToPosition(data.mChildPosition)) {
                                        String mimeType = cursor.getString(mimeIndex);
                                        if (mimeType.equals(Phone.CONTENT_ITEM_TYPE)) {
                                            result.append(getString(
                                                R.string.contact_info_text_as_phone));
                                            result.append(cursor.getString(phoneIndex));
                                        } else if (mimeType.equals(Email.CONTENT_ITEM_TYPE)) {
                                            result.append(getString(
                                                R.string.contact_info_text_as_email));
                                            result.append(cursor.getString(emailIndex));
                                        }
                                    }
                                } else {
                                    if (data.mMimeType.equals(Phone.CONTENT_ITEM_TYPE)) {
                                        result.append(getString(
                                            R.string.contact_info_text_as_phone));
                                    } else if (data.mMimeType.equals(Email.CONTENT_ITEM_TYPE)) {
                                        result.append(getString(
                                            R.string.contact_info_text_as_email));
                                    }
                                    result.append(data.mValue);
                                }
                            }
                        }
                    }
                } finally {
                    if (cursor != null) {
                        cursor.close();
                        cursor = null;
                    }
                }

                result.append(CONTACT_SEP_RIGHT);
            }
            return result.toString();
        }

        /**
         * Get the uri as lookup uri which point to the selected contact.
         *
         * @return The lookup uri.
         */
        public Uri getSelectedAsVcard() {
            if (mMode == MODE_VCARD && !TextUtils.isEmpty(mSelectedContactLookupKey)) {
                return Uri.withAppendedPath(ContactsContract.Contacts.CONTENT_VCARD_URI,
                        mSelectedContactLookupKey);
            }
            return null;
        }

        private String getKeyString(long contactId, int childPosition) {
            StringBuilder selected = new StringBuilder(3);
            selected.append(Long.toString(contactId)).append(KEY_SEP)
                    .append(Long.toString(childPosition));
            return selected.toString();
        }

    }
}
