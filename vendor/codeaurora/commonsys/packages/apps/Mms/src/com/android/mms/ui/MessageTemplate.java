/*
 *
 * Copyright (c) 2013-2014, 2016, The Linux Foundation. All rights reserved.
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

import java.util.HashMap;
import java.util.Map;

import com.android.mms.R;
import android.widget.Adapter;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ListAdapter;
import android.widget.ListView;
import android.widget.SimpleCursorAdapter;

import com.android.mms.transaction.MessagingNotification;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.graphics.Color;

import android.app.ActionBar;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.AlertDialog.Builder;
import android.app.Dialog;

import android.content.AsyncQueryHandler;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.DialogInterface.OnClickListener;
import android.content.SharedPreferences.Editor;

import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.provider.ContactsContract.PhoneLookup;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.Log;
import android.view.ContextMenu;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MenuItem.OnMenuItemClickListener;
import android.view.View;
import android.view.View.OnLongClickListener;
import android.view.inputmethod.InputMethodManager;
import android.view.Window;
import android.view.ContextMenu.ContextMenuInfo;
import android.widget.TextView;
import android.widget.Toast;
import android.util.Log;

/**
 * new, edit or delete message template.
 */
public class MessageTemplate extends Activity {

    final private static String TAG = "MessageTemplate";
    final private boolean DBG = true;
    final static String strSmsTempUri =
            "content://com.android.mms.MessageTemplateProvider/messages";

    private ListView mLvContent = null;
    private SimpleCursorAdapter mAdapterContent = null;
    private AlertDialog mEditDlg = null;
    private AlertDialog mNewDlg = null;
    private int position = -1;
    private final int TEMPLATE_MAX_LENGTH = 1000;

    private OnMenuItemClickListener menuItemClickListener = new OnMenuItemClickListener() {

        @Override
        public boolean onMenuItemClick(MenuItem item) {
            AdapterView.AdapterContextMenuInfo info =
                    (AdapterView.AdapterContextMenuInfo)item.getMenuInfo();
            int  _id = (int)info.id;
            if ( _id < 0 || _id >=  mLvContent.getCount()) {
                if (DBG)
                    Log.d(TAG, "_id error. _id:"+_id+" count:"+mLvContent.getCount());
                return false;
            }

            Uri uri = Uri.parse(strSmsTempUri+"/"+_id);
            int result = getContentResolver().delete(uri, null, null);
            if (result == 0) {
                uri = Uri.parse(strSmsTempUri);
                Cursor cur = MessageTemplate.this.managedQuery(uri, null, null, null, null);
                if (cur!=null)
                    mAdapterContent.changeCursor(cur);
            }
            return false;
        }
    };

    private AdapterView.OnItemClickListener editSMSTempClick
        = new AdapterView.OnItemClickListener() {
        private String title = null;

        @Override
        public void onItemClick(AdapterView<?> arg0, View arg1, int arg2, long arg3) {
             Cursor cur = mAdapterContent.getCursor();
             cur.moveToPosition(arg2);
             title = cur.getString(cur.getColumnIndex("message"));
             position = arg2;
             createEditMessageDialog(title);
        }
    };

    private OnClickListener editSmsTempClickOK = new OnClickListener() {
        @Override
        public void onClick(DialogInterface dialog, int which) {
            EditText et = (EditText)((Dialog)dialog).findViewById(R.id.edit_tempsms_editor);
            String newSMSTemp = et.getText().toString();

            if (position < 0 || position >= mLvContent.getCount()) {
                if (DBG)
                    Log.d(TAG, "postion error. position:"+position+" count:"+mLvContent.getCount());
                return;
            }

            // delete
            if (newSMSTemp.trim().length() == 0 ) {
                Uri uri = Uri.parse(strSmsTempUri+"/"+position);
                int result = getContentResolver().delete(uri, null, null);

                if (result == 0) {
                    uri = Uri.parse(strSmsTempUri);
                    Cursor cur = MessageTemplate.this.managedQuery(uri, null, null, null, null);
                    if (cur!=null)
                        mAdapterContent.changeCursor(cur);
                }
                return;
            }
            // update
            ContentValues values = new ContentValues();
            values.put("message", newSMSTemp);
            values.put("_id", position);
            Uri uri = Uri.parse(strSmsTempUri);
            int result = getContentResolver().update(uri, values, null, null);

            if (result == 0) {
                Cursor cur = MessageTemplate.this.managedQuery(uri, null, null, null, null);
                if (cur!=null)
                    mAdapterContent.changeCursor(cur);
            }
        }
    };

    // new template message handle
    private OnItemClickListener mNewSmsTemp = new OnItemClickListener() {
        // send new con
        @Override
        public void onItemClick(AdapterView<?> arg0, View arg1, int arg2, long arg3) {
            createNewMessageDialog(null);
        }

    };

    public DialogInterface.OnClickListener newSmsTempClickOK =
            new DialogInterface.OnClickListener() {
        public void onClick(DialogInterface dialog, int whichButton) {
            EditText et = (EditText)((Dialog)dialog).findViewById(R.id.edit_tempsms_editor);
            String newSMSTemp = et.getText().toString();

            ContentValues values = new ContentValues();
            if (newSMSTemp.trim().length()==0)
                return;
            values.put("message", newSMSTemp);
            Uri uri = Uri.parse(strSmsTempUri);
            Uri result = getContentResolver().insert(uri, values);
            if (result!=null) {
                Cursor cur = MessageTemplate.this.managedQuery(uri, null, null, null, null);
                if (cur!=null)
                    mAdapterContent.changeCursor(cur);
            }
        }
    };

    @Override
    public void onCreateContextMenu(ContextMenu menu,View view,ContextMenuInfo menuInfo){
        menu.setHeaderTitle(R.string.message_template);
        menu.add(0,0,0,R.string.delete_template_message)
            .setOnMenuItemClickListener(menuItemClickListener);
    }

    @Override
    protected void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        if (MessageUtils.checkPermissionsIfNeeded(this)) {
            return;
        }
        setContentView(R.layout.message_template_list);

        mLvContent = (ListView)findViewById(R.id.listViewContent);
        Uri uri = Uri.parse(strSmsTempUri);
        Cursor cur = managedQuery(uri, null, null, null, null);
        if (cur!=null) {
            mAdapterContent = new SimpleCursorAdapter(this,
                    android.R.layout.simple_list_item_1,
                    cur,
                    new String[] {"message"},
                    new int[] {android.R.id.text1});
            mLvContent.setAdapter(mAdapterContent);
            mLvContent.setOnItemClickListener(editSMSTempClick);
        }
        else {
            if (DBG)
                Log.d(TAG, "Get message teamplate from content provider failed!");
        }

        // resume previous dialog since this activity may be rotated and re-created.
        resumePreviousDialog(icicle);

        ActionBar actionBar = getActionBar();
        actionBar.setDisplayHomeAsUpEnabled(true);
    }

    @Override
    public void onResume() {
        super.onResume();
        registerForContextMenu(mLvContent);
    }

    @Override
    public void onPause() {
        super.onPause();
        this.unregisterForContextMenu(mLvContent);
    }

    @Override
    protected void onStop() {
        super.onStop();
        // Hide the keyboard when stop it.
        hideDialogSoftKeyboard(mNewDlg);
        hideDialogSoftKeyboard(mEditDlg);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu){
        getMenuInflater().inflate(R.menu.template_menu, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.add:
                createNewMessageDialog(null);
                return true;
            case android.R.id.home:
                finish();
                return true;
        }
        return false;
    }

    private void hideDialogSoftKeyboard(Dialog dialog) {
        if (dialog != null && dialog.isShowing()) {
            EditText et = (EditText) dialog.findViewById(R.id.edit_tempsms_editor);
            InputMethodManager inputMethodManager =
                    (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
            inputMethodManager.hideSoftInputFromWindow(et.getWindowToken(), 0);
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (mEditDlg!=null)
            mEditDlg.dismiss();
        mEditDlg = null;
        if (mNewDlg!=null)
            mNewDlg.dismiss();
        mNewDlg = null;
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        if (mNewDlg!=null && mNewDlg.isShowing()) {
            EditText et = (EditText)mNewDlg.findViewById(R.id.edit_tempsms_editor);
            String message = et.getText().toString();
            outState.putString("new_message", message);
        }
        if (mEditDlg!=null && mEditDlg.isShowing()) {
            EditText et = (EditText)mEditDlg.findViewById(R.id.edit_tempsms_editor);
            String message = et.getText().toString();
            outState.putString("edit_message", message);
            outState.putInt("edit_pos", position);
        }
    }

    private void resumePreviousDialog(Bundle icicle) {
        if (icicle == null)
            return;

        if (icicle.containsKey("new_message")) {
            createNewMessageDialog(icicle.getString("new_message"));
        }

        if (icicle.containsKey("edit_pos")) {
            position = icicle.getInt("edit_pos");
            createEditMessageDialog(icicle.getString("edit_message"));
        }
    }

    private final TextWatcher mTemplateWatcher = new TextWatcher() {
        @Override
        public void beforeTextChanged(CharSequence s, int start, int count, int after) {
        }

        @Override
        public void onTextChanged(CharSequence s, int start, int before, int count) {
            if (s.toString().length() >= TEMPLATE_MAX_LENGTH) {
                Toast.makeText(MessageTemplate.this, R.string.template_full, Toast.LENGTH_SHORT)
                        .show();
            }
        }

        @Override
        public void afterTextChanged(Editable s) {}

    };

    private void createNewMessageDialog(String message) {
        LayoutInflater factory = LayoutInflater.from(MessageTemplate.this);
        final View view = factory.inflate(R.layout.dialog_edit_template_message, null);
        EditText et = (EditText)view.findViewById(R.id.edit_tempsms_editor);
        et.addTextChangedListener(mTemplateWatcher);
        if (message!=null) {
            et.setText(message);

            // Set the cursor of EditText to real position user wants.
            et.setSelection(message.length());
        }
        mNewDlg = new AlertDialog.Builder(MessageTemplate.this)
            .setTitle(getText(R.string.dialog_createSMSTemplate_title))
            .setView(view)
            .setPositiveButton(android.R.string.ok,newSmsTempClickOK)
            .setNegativeButton(android.R.string.cancel,
                new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int whichButton) {}
            })
            .create();
        mNewDlg.show();
    }

    private void createEditMessageDialog(String message) {
        LayoutInflater factory = LayoutInflater.from(MessageTemplate.this);
        final View view = factory.inflate(R.layout.dialog_edit_template_message, null);
        EditText et = (EditText) view.findViewById(R.id.edit_tempsms_editor);
        et.setText(message);
        et.addTextChangedListener(mTemplateWatcher);
        mEditDlg = new AlertDialog.Builder(MessageTemplate.this)
            .setTitle(getText(R.string.dialog_editSMSTemplate_title))
            .setView(view)
            .setPositiveButton(R.string.yes,editSmsTempClickOK)
            .setNegativeButton(R.string.no,
                new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int whichButton) {}
                })
            .create();
        mEditDlg.show();
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if(keyCode == KeyEvent.KEYCODE_MENU) {
            //Do nothing.
            return true;
        }
        return super.onKeyUp(keyCode, event);
    }
}
