package com.android.settings;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.content.DialogInterface.OnDismissListener;
import android.content.Intent;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;

public class OutSpaceDialog extends Activity {

    private Button mManageApplication;
    private AlertDialog dialog;
    private DialogInterface.OnClickListener dialogOnClickListener;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // TODO Auto-generated method stub
        super.onCreate(savedInstanceState);

        init();
    }

    @Override
    protected void onResume() {
        // TODO Auto-generated method stub
        super.onResume();
        showMyDialog();
    }

    private void init() {
        // TODO Auto-generated method stub
        dialogOnClickListener = new DialogOnClickListener();
        createDialog();
        dialog.setOnDismissListener(new DialogOnDismissListener());
    }

    private void createDialog() {
        // TODO Auto-generated method stub
        AlertDialog.Builder dialogBuilder = new AlertDialog.Builder(this);
        dialogBuilder.setTitle(android.R.string.dialog_alert_title);
        dialogBuilder.setPositiveButton(R.string.okay,
                dialogOnClickListener);
        dialogBuilder.setNegativeButton(com.android.internal.R.string.cancel,
        dialogOnClickListener);
        dialogBuilder.setMessage(R.string.out_of_space_summary);
        dialog = dialogBuilder.create();
        dialog.setCanceledOnTouchOutside(false);
    }

    private void showMyDialog() {
        // TODO Auto-generated method stub
        if (dialog != null) {
            dialog.show();
        }
    }

    class DialogOnClickListener implements DialogInterface.OnClickListener {

        @Override
        public void onClick(DialogInterface dialog, int which) {
            // TODO Auto-generated method stub
            if (which == Dialog.BUTTON_POSITIVE) {
                Intent intent = new Intent(
                        "android.intent.action.MANAGE_PACKAGE_STORAGE");
                intent.setFlags(Intent.FLAG_ACTIVITY_NO_HISTORY);
                startActivity(intent);
                finish();
            } else {
                finish();
            }
        }
    }

    class DialogOnDismissListener implements DialogInterface.OnDismissListener {

        @Override
        public void onDismiss(DialogInterface dialog) {
            // TODO Auto-generated method stub
            finish();
        }
    }
}
