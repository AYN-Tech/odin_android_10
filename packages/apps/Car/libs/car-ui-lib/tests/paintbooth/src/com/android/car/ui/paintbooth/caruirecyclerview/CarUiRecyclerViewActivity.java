/*
 * Copyright (C) 2019 The Android Open Source Project.
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
import android.os.Bundle;

import androidx.recyclerview.widget.LinearLayoutManager;

import com.android.car.ui.paintbooth.R;
import com.android.car.ui.recyclerview.CarUiRecyclerView;

import java.util.ArrayList;

/**
 * Activity that shows CarUiRecyclerView example with dummy data.
 */
public class CarUiRecyclerViewActivity extends Activity {
    private final ArrayList<String> mData = new ArrayList<>();
    private final int mDataToGenerate = 100;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.car_ui_recycler_view_activity);
        CarUiRecyclerView recyclerView = findViewById(R.id.list);
        recyclerView.setLayoutManager(new LinearLayoutManager(this));

        RecyclerViewAdapter adapter = new RecyclerViewAdapter(generateDummyData());
        recyclerView.setAdapter(adapter);
    }

    private ArrayList<String> generateDummyData() {
        for (int i = 0; i <= mDataToGenerate; i++) {
            mData.add("data" + i);
        }
        return mData;
    }
}

