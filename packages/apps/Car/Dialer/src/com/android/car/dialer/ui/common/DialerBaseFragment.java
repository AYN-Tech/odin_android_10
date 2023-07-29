/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.car.dialer.ui.common;

import android.app.Activity;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;
import androidx.lifecycle.ViewModelProviders;

import com.android.car.dialer.R;
import com.android.car.dialer.ui.TelecomActivity;
import com.android.car.dialer.ui.TelecomActivityViewModel;
import com.android.car.ui.toolbar.Toolbar;

/**
 * The base class for top level dialer content {@link Fragment}s.
 */
public abstract class DialerBaseFragment extends Fragment {

    private MutableLiveData<Integer> mToolbarHeight;

    /**
     * Interface for Dialer top level fragment's parent to implement.
     */
    public interface DialerFragmentParent {

        /**
         * Push a fragment to the back stack. Update action bar accordingly.
         */
        void pushContentFragment(Fragment fragment, String fragmentTag);
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mToolbarHeight = new MutableLiveData<>();
    }

    @Override
    public void onStart() {
        super.onStart();
        mToolbarHeight.observe(this, this::onToolbarHeightChange);
    }

    /**
     * Customizes the tool bar. Can be overridden in subclasses.
     */
    public void setupToolbar(@NonNull Toolbar toolbar) {
        TelecomActivityViewModel viewModel = ViewModelProviders.of(getActivity()).get(
                TelecomActivityViewModel.class);
        LiveData<String> toolbarTitleLiveData = viewModel.getToolbarTitle();
        toolbarTitleLiveData.observe(this,
                toolbarTitle -> toolbar.setTitle(toolbarTitle));

        toolbar.setState(getToolbarState());
        toolbar.setLogo(getToolbarState() == Toolbar.State.HOME ? getActivity().getDrawable(
                R.drawable.ic_app_icon) : null);

        toolbar.setMenuItems(R.xml.menuitems);

        setShowToolbarBackground(true);

        setToolbarHeight(toolbar.getHeight());
    }

    /**
     * Push a fragment to the back stack. Update action bar accordingly.
     */
    protected void pushContentFragment(@NonNull Fragment fragment, String fragmentTag) {
        Activity parentActivity = getActivity();
        if (parentActivity instanceof DialerFragmentParent) {
            ((DialerFragmentParent) parentActivity).pushContentFragment(fragment, fragmentTag);
        }
    }

    protected Toolbar.State getToolbarState() {
        return Toolbar.State.HOME;
    }

    protected final void setShowToolbarBackground(boolean showBackground) {
        Activity activity = getActivity();
        if (activity instanceof TelecomActivity) {
            ((TelecomActivity) activity).setShowToolbarBackground(showBackground);
        }
    }

    /**
     * Sets the toolbar height.
     */
    public final void setToolbarHeight(int toolbarHeight) {
        mToolbarHeight.setValue(toolbarHeight);
    }

    /**
     * Handles the toolbar height.
     */
    public abstract void onToolbarHeightChange(int toolbarHeight);
}
