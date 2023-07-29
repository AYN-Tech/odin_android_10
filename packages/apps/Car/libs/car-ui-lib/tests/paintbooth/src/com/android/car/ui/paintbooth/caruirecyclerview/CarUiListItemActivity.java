/*
 * Copyright 2019 The Android Open Source Project
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

package com.android.car.ui.paintbooth.caruirecyclerview;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.widget.Toast;

import com.android.car.ui.paintbooth.R;
import com.android.car.ui.recyclerview.CarUiContentListItem;
import com.android.car.ui.recyclerview.CarUiHeaderListItem;
import com.android.car.ui.recyclerview.CarUiListItem;
import com.android.car.ui.recyclerview.CarUiListItemAdapter;
import com.android.car.ui.recyclerview.CarUiListItemLayoutManager;
import com.android.car.ui.recyclerview.CarUiRecyclerView;

import java.util.ArrayList;

/**
 * Activity that shows {@link CarUiRecyclerView} with dummy {@link CarUiContentListItem} entries
 */
public class CarUiListItemActivity extends Activity {

    private final ArrayList<CarUiListItem> mData = new ArrayList<>();
    private CarUiListItemAdapter mAdapter;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.car_ui_recycler_view_activity);
        CarUiRecyclerView recyclerView = findViewById(R.id.list);

        mAdapter = new CarUiListItemAdapter(generateDummyData());
        recyclerView.setAdapter(mAdapter);
        recyclerView.setLayoutManager(new CarUiListItemLayoutManager(this));
    }

    private ArrayList<CarUiListItem> generateDummyData() {
        Context context = this;

        CarUiHeaderListItem header = new CarUiHeaderListItem("First header");
        mData.add(header);

        CarUiContentListItem item = new CarUiContentListItem();
        item.setTitle("Test title");
        item.setBody("Test body");
        mData.add(item);

        item = new CarUiContentListItem();
        item.setTitle("Test title with no body");
        mData.add(item);

        header = new CarUiHeaderListItem("Random header", "with header body");
        mData.add(header);

        item = new CarUiContentListItem();
        item.setBody("Test body with no title");
        mData.add(item);

        item = new CarUiContentListItem();
        item.setTitle("Test Title");
        item.setIcon(getDrawable(R.drawable.ic_launcher));
        mData.add(item);

        item = new CarUiContentListItem();
        item.setTitle("Test Title");
        item.setBody("Test body text");
        item.setIcon(getDrawable(R.drawable.ic_launcher));
        mData.add(item);

        item = new CarUiContentListItem();
        item.setIcon(getDrawable(R.drawable.ic_launcher));
        item.setTitle("Title -- Item with checkbox");
        item.setBody("Will present toast on change of selection state.");
        item.setOnCheckedChangedListener(
                (listItem, isChecked) -> Toast.makeText(context,
                        "Item checked state is: " + isChecked, Toast.LENGTH_SHORT).show());
        item.setAction(CarUiContentListItem.Action.CHECK_BOX);
        mData.add(item);

        item = new CarUiContentListItem();
        item.setIcon(getDrawable(R.drawable.ic_launcher));
        item.setBody("Body -- Item with switch");
        item.setAction(CarUiContentListItem.Action.SWITCH);
        mData.add(item);

        item = new CarUiContentListItem();
        item.setIcon(getDrawable(R.drawable.ic_launcher));
        item.setTitle("Title -- Item with checkbox");
        item.setBody("Item is initially checked");
        item.setAction(CarUiContentListItem.Action.CHECK_BOX);
        item.setChecked(true);
        mData.add(item);

        CarUiContentListItem radioItem1 = new CarUiContentListItem();
        CarUiContentListItem radioItem2 = new CarUiContentListItem();

        radioItem1.setTitle("Title -- Item with radio button");
        radioItem1.setBody("Item is initially unchecked checked");
        radioItem1.setAction(CarUiContentListItem.Action.RADIO_BUTTON);
        radioItem1.setChecked(false);
        radioItem1.setOnCheckedChangedListener((listItem, isChecked) -> {
            if (isChecked) {
                radioItem2.setChecked(false);
                mAdapter.notifyItemChanged(mData.indexOf(radioItem2));
            }
        });
        mData.add(radioItem1);

        radioItem2.setIcon(getDrawable(R.drawable.ic_launcher));
        radioItem2.setTitle("Item is mutually exclusive with item above");
        radioItem2.setAction(CarUiContentListItem.Action.RADIO_BUTTON);
        radioItem2.setChecked(true);
        radioItem2.setOnCheckedChangedListener((listItem, isChecked) -> {
            if (isChecked) {
                radioItem1.setChecked(false);
                mAdapter.notifyItemChanged(mData.indexOf(radioItem1));
            }
        });
        mData.add(radioItem2);

        item = new CarUiContentListItem();
        item.setIcon(getDrawable(R.drawable.ic_launcher));
        item.setTitle("Title");
        item.setBody("Random body text -- with action divider");
        item.setActionDividerVisible(true);
        item.setSupplementalIcon(getDrawable(R.drawable.ic_launcher));
        item.setChecked(true);
        mData.add(item);

        item = new CarUiContentListItem();
        item.setIcon(getDrawable(R.drawable.ic_launcher));
        item.setTitle("Null supplemental icon");
        item.setAction(CarUiContentListItem.Action.ICON);
        item.setChecked(true);
        mData.add(item);

        item = new CarUiContentListItem();
        item.setTitle("Supplemental icon with listener");
        item.setSupplementalIcon(getDrawable(R.drawable.ic_launcher),
                v -> Toast.makeText(context, "Clicked supplemental icon",
                        Toast.LENGTH_SHORT).show());
        item.setChecked(true);
        mData.add(item);

        return mData;
    }
}
