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

package com.android.soundrecorder.filelist;

import android.app.AlertDialog;
import android.app.Fragment;
import android.app.FragmentTransaction;
import android.content.DialogInterface;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.ActionMode;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.RelativeLayout;
import android.widget.Toast;

import com.android.soundrecorder.RenameDialogBuilder;
import com.android.soundrecorder.actionmode.ActionModeHandler;
import com.android.soundrecorder.R;
import com.android.soundrecorder.filelist.listitem.BaseListItem;
import com.android.soundrecorder.filelist.listitem.MediaItem;
import com.android.soundrecorder.filelist.player.Player;
import com.android.soundrecorder.filelist.player.PlayerPanel;
import com.android.soundrecorder.util.DatabaseUtils;
import com.android.soundrecorder.util.FileUtils;
import com.android.soundrecorder.util.PermissionUtils;
import com.android.soundrecorder.util.StorageUtils;

import java.io.File;
import java.util.ArrayList;
import java.util.List;
import android.util.Log;
import android.os.Build;

public class FileListFragment extends Fragment {
    private static final String TAG = "FileListFragment";

    private static final int PERMISSION_REQUEST_CODE = 1;
    public static final String FRAGMENT_TAG = "FileListFragment";
    public static final String FOLDER_PATH = "folder_path";
    public static final String MIME_TYPE_AUDIO = "audio/*";
    private FileListRecyclerView mRecyclerView;
    private FileListRecyclerAdapter mAdapter;

    private boolean mIsRootPage = true;
    private String mArgumentPath;

    //fix the rename failure when reload adapter during renaming.
    private AlertDialog mRenameDialog;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        initArguments(getArguments());
        initAdapter();
    }

    @Override
    public void onStart() {
        super.onStart();
        // FileListActivity will receive the result.
        if (PermissionUtils.checkPermissions(getActivity(), PermissionUtils.PermissionType.PLAY,
                PERMISSION_REQUEST_CODE)) {
            if (mRenameDialog != null && mRenameDialog.isShowing()) {
                mRenameDialog.dismiss();
                mRenameDialog = null;
            }
            reloadAdapter();
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        resetActionBarTitle();
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {
        View rootView = inflater.inflate(R.layout.file_list_fragment, container, false);

        final View recyclerView = rootView.findViewById(R.id.recycler_view);
        mRecyclerView = (FileListRecyclerView) recyclerView;
        mRecyclerView.setEmptyView(rootView.findViewById(R.id.list_empty));

        RecyclerView.LayoutManager layoutManager = new LinearLayoutManager(getContext());
        mRecyclerView.setLayoutManager(layoutManager);

        mRecyclerView.setAdapter(mAdapter);

        if (getPlayer() != null) {
            getPlayer().setPlayerPanelLayoutListener(new PlayerPanel.LayoutChangedListener() {
                @Override
                public void onLayoutChanged(View view) {
                    updateRecyclerViewLayout(view);
                }
            });

            updateRecyclerViewLayout(getPlayer().getPlayerPanel());
        }

        return rootView;
    }

    private void updateRecyclerViewLayout(View anchor) {
        final RelativeLayout.LayoutParams params = (RelativeLayout.LayoutParams) mRecyclerView
                .getLayoutParams();
        boolean isPlayerShown = false;

        if (getPlayer() != null) {
            isPlayerShown = getPlayer().isPlayShown();
        }

        if (isPlayerShown && anchor != null) {
            params.bottomMargin = anchor.getMeasuredHeight();
        } else {
            params.bottomMargin = 0;
        }
        mRecyclerView.post(new Runnable() {
            @Override
            public void run() {
                mRecyclerView.setLayoutParams(params);
            }
        });
    }

    private Player getPlayer() {
        return ((FileListActivity) getActivity()).getPlayer();
    }

    private void initArguments(Bundle arguments) {
        if (arguments != null) {
            mArgumentPath = arguments.getString(FOLDER_PATH);
        }
        mIsRootPage = (mArgumentPath == null);
    }

    private void initAdapter() {
        String[] folderArray = null;
        String[] rootArray = null;
        if (mIsRootPage) {
            folderArray = new String[] {
                    StorageUtils.getCallRecordingStoragePath(),
                    StorageUtils.getFmRecordingStoragePath()
            };
            rootArray = new String[] {
                    StorageUtils.getPhoneStoragePath()
            };
        } else {
            rootArray = new String[] {
                    mArgumentPath
            };
        }
        mAdapter = new FileListRecyclerAdapter(getContext(), folderArray, rootArray);
        mAdapter.setItemListener(new FileListRecyclerAdapter.ItemListener() {
            @Override
            public void openItem(BaseListItem item) {
                if (item.getItemType() == BaseListItem.TYPE_FOLDER) {
                    startFragment(item.getPath());
                } else {
                    if (getPlayer() != null) {
                        getPlayer().setItem((MediaItem) item);
                        getPlayer().startPlayer();
                    }
                }
            }

            @Override
            public void closeItem() {
                if (getPlayer() != null) {
                    getPlayer().destroyPlayer();
                }
            }

            @Override
            public MediaItem getPlayingItem() {
                if (getPlayer() != null) {
                    return getPlayer().getMediaItem();
                }
                return null;
            }

            @Override
            public void updatePlayerItem(MediaItem item) {
                if (getPlayer() != null) {
                    getPlayer().onItemChanged(item);
                }
            }
        });
        mAdapter.setActionModeListener(
                new ActionModeHandler(getActivity(), R.layout.file_list_action_mode,
                        mActionModeCallback));
    }

    public void reloadAdapter() {
        if (mAdapter != null) {
            mAdapter.reload();
        }
    }

    private void resetActionBarTitle() {
        if (mIsRootPage) {
            getActivity().setTitle(R.string.file_list_activity_label);
        } else {
            if (mArgumentPath != null) {
                String title = FileUtils.getLastFileName(mArgumentPath, false);
                if(title.equals(StorageUtils.FM_RECORDING_FOLDER_NAME)){
                    getActivity().setTitle(R.string.file_list_FM);
                }
                else if(title.equals(StorageUtils.CALL_RECORDING_FOLDER_NAME)){
                    getActivity().setTitle(R.string.file_list_call);
                }
                else{
                    getActivity().setTitle(title);
                }
            }
        }
    }

    private void startFragment(String path) {
        FileListFragment fragment = new FileListFragment();
        FragmentTransaction transaction = getFragmentManager().beginTransaction();
        transaction.replace(android.R.id.content, fragment);
        transaction.addToBackStack(path);
        Bundle bundle = new Bundle();
        bundle.putString(FOLDER_PATH, path);
        fragment.setArguments(bundle);
        transaction.commit();
    }

    private ActionMode.Callback mActionModeCallback = new ActionMode.Callback() {
        @Override
        public boolean onPrepareActionMode(ActionMode actionMode, Menu menu) {
            return false;
        }

        @Override
        public void onDestroyActionMode(ActionMode actionMode) {
            if (mAdapter != null) {
                mAdapter.leaveSelectionMode();
            }
        }

        @Override
        public boolean onCreateActionMode(ActionMode actionMode, Menu menu) {
            actionMode.getMenuInflater().inflate(R.menu.list_action_mode_menu, menu);
            return true;
        }

        @Override
        public boolean onActionItemClicked(ActionMode actionMode, MenuItem menuItem) {
            switch (menuItem.getItemId()) {
                case R.id.action_edit:
                    if (mAdapter != null) {
                        List<BaseListItem> items = mAdapter.getSelectedItems();
                        if (items.size() > 0) {
                            renameItem(items.get(0));
                        }
                    }
                    return true;
                case R.id.action_share:
                    if (mAdapter != null) {
                        shareItems(mAdapter.getSelectedItems());
                    }
                    return true;
                case R.id.action_delete:
                    if (mAdapter != null) {
                        deleteItems(mAdapter.getSelectedItems());
                    }
                    return true;
                case R.id.action_select_all:
                    if (mAdapter != null) {
                        mAdapter.toggleSelectAllState();
                    }
                    return true;
                default:
                    break;
            }
            return false;
        }
    };

    private void renameItem(final BaseListItem item) {
        final File file = new File(item.getPath());
        RenameDialogBuilder builder = new RenameDialogBuilder(getContext(), file);
        builder.setTitle(R.string.rename_dialog_title);
        builder.setNegativeButton(R.string.discard, null);
        builder.setPositiveButton(R.string.rename_dialog_save,
                new RenameDialogBuilder.OnPositiveListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which, String newName) {
                        if (!FileUtils.exists(file)) {
                            Toast.makeText(getContext(), R.string.file_deleted, Toast.LENGTH_SHORT)
                                    .show();
                            return;
                        }
                        // rename file.
                        File newFile = FileUtils.renameFile(file, newName);
                        // update database.
                        DatabaseUtils.rename(getContext(), file, newFile);

                        item.setPath(newFile.getAbsolutePath());
                        // update list item.
                        item.setTitle(newName);
                        if (mAdapter != null) {
                            mAdapter.notifyItemChanged(item);
                        }
                        if (getPlayer() != null && getPlayer().isItemUsing(item)) {
                            getPlayer().onItemChanged((MediaItem) item);
                        }
                    }
                });
        builder.setEditTextContent(FileUtils.getLastFileName(file, false));
        mRenameDialog = builder.show();
    }

    private void deleteItems(final List<BaseListItem> items) {
        String message = getResources().getQuantityString(R.plurals.delete_selection_message,
                items.size());

        AlertDialog dialog = new AlertDialog.Builder(getContext())
                .setMessage(message)
                .setNegativeButton(android.R.string.cancel, null)
                .setPositiveButton(R.string.delete_dialog_confirm,
                        new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                for (BaseListItem item : items) {
                                    deleteItem(item);
                                    if (getPlayer() != null && getPlayer().isItemUsing(item)) {
                                        getPlayer().onItemDeleted();
                                    }
                                }
                            }
                        })
                .create();
        dialog.show();
        Button negativeButton = dialog.getButton(DialogInterface.BUTTON_NEGATIVE);
        if (negativeButton != null) {
            negativeButton.setTextColor(getContext().getColor(R.color.negative_button_color));
        }
    }

    private void deleteItem(final BaseListItem item) {
        File file = new File(item.getPath());
        boolean itemRemoved = FileUtils.deleteFile(file, getContext());

        if (itemRemoved && mAdapter != null) {
            mAdapter.removeItem(item);
        }
    }

    private void shareItems(final List<BaseListItem> items) {
        // generate uris
        final ArrayList<Uri> uris = new ArrayList<Uri>();
        for (BaseListItem item : items) {
            File file = new File(item.getPath());
            if (file.isDirectory()) {
                uris.addAll(FileUtils.urisFromFolder(file, getContext()));
            } else {
                Uri uri = null;
                if (Build.VERSION.SDK_INT >= 24) {
                    uri = FileUtils.uriFromFile(file, getContext());
                } else {
                    uri = Uri.fromFile(file);
                }
                uris.add(uri);
            }
        }
        // generate intent
        final Intent shareIntent = new Intent();

        int size = uris.size();
        if (size == 0) return;
        if (size > 1) {
            shareIntent.setAction(Intent.ACTION_SEND_MULTIPLE).setType(MIME_TYPE_AUDIO);
            shareIntent.putParcelableArrayListExtra(Intent.EXTRA_STREAM, uris);
        } else {
            shareIntent.setAction(Intent.ACTION_SEND).setType(MIME_TYPE_AUDIO);
            shareIntent.putExtra(Intent.EXTRA_STREAM, uris.get(0));
        }
        shareIntent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);

        String shareTitle = getResources().getString(R.string.share_title);
        getActivity().startActivity(Intent.createChooser(shareIntent, shareTitle));
    }
}
