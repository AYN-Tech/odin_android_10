/*
 * Copyright (C) 2019 The Android Open Source Project
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

package com.android.car.ui.paintbooth.dialogs;

import android.app.Activity;
import android.os.Bundle;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.Toast;

import androidx.annotation.NonNull;

import com.android.car.ui.AlertDialogBuilder;
import com.android.car.ui.paintbooth.R;
import com.android.car.ui.recyclerview.CarUiRecyclerView;

import java.util.ArrayList;
import java.util.List;

/**
 * Activity that shows different dialogs from the device default theme.
 */
public class DialogsActivity extends Activity {

    private final List<Pair<Integer, View.OnClickListener>> mButtons = new ArrayList<>();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.car_ui_recycler_view_activity);

        mButtons.add(Pair.create(R.string.dialog_show_dialog,
                v -> showDialog()));
        mButtons.add(Pair.create(R.string.dialog_show_dialog_icon,
                v -> showDialogWithIcon()));
        mButtons.add(Pair.create(R.string.dialog_show_dialog_edit,
                v -> showDialogWithTextBox()));
        mButtons.add(Pair.create(R.string.dialog_show_dialog_only_positive,
                v -> showDialogWithOnlyPositiveButton()));
        mButtons.add(Pair.create(R.string.dialog_show_dialog_no_button,
                v -> showDialogWithNoButtonProvided()));
        mButtons.add(Pair.create(R.string.dialog_show_dialog_checkbox,
                v -> showDialogWithCheckbox()));
        mButtons.add(Pair.create(R.string.dialog_show_dialog_no_title,
                v -> showDialogWithoutTitle()));
        mButtons.add(Pair.create(R.string.dialog_show_toast,
                v -> showToast()));
        mButtons.add(Pair.create(R.string.dialog_show_subtitle,
                v -> showDialogWithSubtitle()));
        mButtons.add(Pair.create(R.string.dialog_show_subtitle_and_icon,
                v -> showDialogWithSubtitleAndIcon()));

        CarUiRecyclerView recyclerView = requireViewById(R.id.list);
        recyclerView.setAdapter(mAdapter);
    }

    private void showDialog() {
        new AlertDialogBuilder(this)
                .setTitle("Standard Alert Dialog")
                .setMessage("With a message to show.")
                .setNeutralButton("NEUTRAL", (dialogInterface, which) -> {
                })
                .setPositiveButton("OK", (dialogInterface, which) -> {
                })
                .setNegativeButton("CANCEL", (dialogInterface, which) -> {
                })
                .show();
    }

    private void showDialogWithIcon() {
        new AlertDialogBuilder(this)
                .setTitle("Alert dialog with icon")
                .setMessage("The message body of the alert")
                .setIcon(R.drawable.ic_tracklist)
                .show();
    }

    private void showDialogWithNoButtonProvided() {
        new AlertDialogBuilder(this)
                .setTitle("Standard Alert Dialog")
                .show();
    }

    private void showDialogWithCheckbox() {
        new AlertDialogBuilder(this)
                .setTitle("Custom Dialog Box")
                .setMultiChoiceItems(
                        new CharSequence[]{"I am a checkbox"},
                        new boolean[]{false},
                        (dialog, which, isChecked) -> {
                        })
                .setPositiveButton("OK", (dialogInterface, which) -> {
                })
                .setNegativeButton("CANCEL", (dialogInterface, which) -> {
                })
                .show();
    }

    private void showDialogWithTextBox() {
        new AlertDialogBuilder(this)
                .setTitle("Standard Alert Dialog")
                .setEditBox("Edit me please", null, null)
                .setPositiveButton("OK", (dialogInterface, i) -> {
                })
                .show();
    }

    private void showDialogWithOnlyPositiveButton() {
        new AlertDialogBuilder(this)
                .setTitle("Standard Alert Dialog").setMessage("With a message to show.")
                .setPositiveButton("OK", (dialogInterface, i) -> {
                })
                .show();
    }

    private void showDialogWithoutTitle() {
        new AlertDialogBuilder(this)
                .setMessage("I dont have a title.")
                .setPositiveButton("OK", (dialogInterface, i) -> {
                })
                .setNegativeButton("CANCEL", (dialogInterface, which) -> {
                })
                .show();
    }

    private void showToast() {
        Toast.makeText(this, "Toast message looks like this", Toast.LENGTH_LONG).show();
    }

    private void showDialogWithSubtitle() {
        new AlertDialogBuilder(this)
                .setTitle("My Title!")
                .setSubtitle("My Subtitle!")
                .setMessage("My Message!")
                .show();
    }

    private void showDialogWithSubtitleAndIcon() {
        new AlertDialogBuilder(this)
                .setTitle("My Title!")
                .setSubtitle("My Subtitle!")
                .setMessage("My Message!")
                .setIcon(R.drawable.ic_tracklist)
                .show();
    }

    private static class ViewHolder extends CarUiRecyclerView.ViewHolder {

        private final Button mButton;

        ViewHolder(View itemView) {
            super(itemView);
            mButton = itemView.requireViewById(R.id.button);
        }

        public void bind(Integer title, View.OnClickListener listener) {
            mButton.setText(title);
            mButton.setOnClickListener(listener);
        }
    }

    private final CarUiRecyclerView.Adapter<ViewHolder> mAdapter =
            new CarUiRecyclerView.Adapter<ViewHolder>() {
                @Override
                public int getItemCount() {
                    return mButtons.size();
                }

                @Override
                public ViewHolder onCreateViewHolder(ViewGroup parent, int position) {
                    View item =
                            LayoutInflater.from(parent.getContext()).inflate(R.layout.list_item,
                                    parent, false);
                    return new ViewHolder(item);
                }

                @Override
                public void onBindViewHolder(@NonNull ViewHolder holder, int position) {
                    Pair<Integer, View.OnClickListener> pair = mButtons.get(position);
                    holder.bind(pair.first, pair.second);
                }
            };
}
