/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.

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

package com.android.soundrecorder;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;

import com.android.soundrecorder.util.FileUtils;

import java.io.File;

public class RenameDialogBuilder extends AlertDialog.Builder {
    public interface OnPositiveListener {
        public void onClick(DialogInterface dialog, int which, String extra);
    }

    private static final String EMPTY_EXTENSION = "";
    private static final int FILENAME_CHAR_LIMIT = 255;
    private String mFolderPath;
    private String mFileName;
    private String mFileExtension;
    private View mCustomLayout;
    private EditText mEditText;
    private TextView mInfoText;
    private View mInfoLayout;


    public RenameDialogBuilder(Context context, File origFile) {
        super(context);
        if (origFile != null) {
            mFolderPath = origFile.getParent();
            mFileName = origFile.getName(); // with extension
            mFileExtension = FileUtils.getFileExtension(origFile, true);
        }
        if (mFileExtension == null) {
            mFileExtension = EMPTY_EXTENSION;
        }
        initCustomLayout();
    }

    @Override
    public AlertDialog create() {
        AlertDialog dialog = super.create();

        dialog.setCanceledOnTouchOutside(false);

        //dialog.getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_VISIBLE);
		dialog.getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_PAN);
        dialog.setView(mCustomLayout);

        return dialog;
    }

    @Override
    public AlertDialog show() {
        final AlertDialog dialog = super.show();

        Button negativeButton = dialog.getButton(DialogInterface.BUTTON_NEGATIVE);
        if (negativeButton != null) {
            negativeButton.setTextColor(getContext().getColor(R.color.negative_button_color));
        }

        if (mEditText != null) {
            mEditText.addTextChangedListener(new TextWatcher() {
                @Override
                public void beforeTextChanged(CharSequence s, int start, int count, int after) {
                }

                @Override
                public void onTextChanged(CharSequence s, int start, int before, int count) {
                }

                @Override
                public void afterTextChanged(Editable s) {
                    checkFileName(dialog, s);
                }
            });
            mEditText.requestFocusFromTouch();
        }
        return dialog;
    }

    /** check file name is valid or not. */
    private void checkFileName(AlertDialog dialog, Editable editable) {
        boolean errorEmptyLength = (editable.length() == 0);
        boolean errorFileExists = false;

        int fileExtensionLength = mFileExtension == null ? 0 : mFileExtension.length();
        boolean errorFileNameLengthLimit =
                (editable.length() + fileExtensionLength) >= FILENAME_CHAR_LIMIT;

        if (mFolderPath != null) {
            File newFile = new File(mFolderPath + File.separator + editable.toString()
                    + mFileExtension);
            // if input name is same as original name, ignore it.
            if (mFileName != null && !mFileName.equals(editable.toString() + mFileExtension)) {
                errorFileExists = FileUtils.exists(newFile);
            }
        }

        if (errorEmptyLength || errorFileExists || errorFileNameLengthLimit) {
            dialog.getButton(DialogInterface.BUTTON_POSITIVE).setEnabled(false);
            mInfoLayout.setVisibility(View.VISIBLE);
            if (errorEmptyLength) {
                mInfoText.setText(R.string.rename_dialog_info_null);
            }
            if (errorFileNameLengthLimit) {
                mInfoText.setText(R.string.rename_dialog_info_length_limit);
            }
            if (errorFileExists) {
                mInfoText.setText(R.string.rename_dialog_info_exists);
            }
        } else {
            dialog.getButton(DialogInterface.BUTTON_POSITIVE).setEnabled(true);
            mInfoLayout.setVisibility(View.GONE);
        }
    }

    private void initCustomLayout() {
        mCustomLayout = View.inflate(getContext(), R.layout.rename_dialog, null);
        mEditText = (EditText) mCustomLayout.findViewById(R.id.rename_edit_text);
        mInfoText = (TextView) mCustomLayout.findViewById(R.id.rename_info_text);
        mInfoLayout = mCustomLayout.findViewById(R.id.rename_info_layout);
    }

    public void setEditTextContent(CharSequence content) {
        if (mEditText != null) {
            mEditText.setText(content);
            mEditText.setSelection(mEditText.getText().length());
        }
    }

    public RenameDialogBuilder setPositiveButton(int textId,
            final OnPositiveListener onPositiveListener) {
        this.setPositiveButton(textId, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                onClickPositiveButton(onPositiveListener, dialog, which);
            }
        });
        return this;
    }

    public RenameDialogBuilder setPositiveButton(CharSequence text,
            final OnPositiveListener onPositiveListener) {
        this.setPositiveButton(text, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                onClickPositiveButton(onPositiveListener, dialog, which);
            }
        });
        return this;
    }

    private void onClickPositiveButton(final OnPositiveListener onPositiveListener,
            DialogInterface dialog, int which) {
        if (onPositiveListener != null) {
            onPositiveListener.onClick(dialog, which,
                    mEditText == null ? null : mEditText.getText().toString());
        }
    }
}