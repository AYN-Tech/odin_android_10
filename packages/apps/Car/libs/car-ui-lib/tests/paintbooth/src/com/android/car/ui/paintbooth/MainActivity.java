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

package com.android.car.ui.paintbooth;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import com.android.car.ui.paintbooth.caruirecyclerview.CarUiListItemActivity;
import com.android.car.ui.paintbooth.caruirecyclerview.CarUiRecyclerViewActivity;
import com.android.car.ui.paintbooth.caruirecyclerview.GridCarUiRecyclerViewActivity;
import com.android.car.ui.paintbooth.dialogs.DialogsActivity;
import com.android.car.ui.paintbooth.overlays.OverlayActivity;
import com.android.car.ui.paintbooth.preferences.PreferenceActivity;
import com.android.car.ui.paintbooth.toolbar.ToolbarActivity;
import com.android.car.ui.paintbooth.widgets.WidgetActivity;
import com.android.car.ui.recyclerview.CarUiRecyclerView;

import java.lang.reflect.Method;
import java.util.Arrays;
import java.util.List;

/**
 * Paint booth app
 */
public class MainActivity extends Activity {

    /**
     * List of all sample activities.
     */
    private final List<Pair<String, Class<? extends Activity>>> mActivities = Arrays.asList(
            Pair.create("Dialogs sample", DialogsActivity.class),
            Pair.create("List sample", CarUiRecyclerViewActivity.class),
            Pair.create("Grid sample", GridCarUiRecyclerViewActivity.class),
            Pair.create("Preferences sample", PreferenceActivity.class),
            Pair.create("Overlays", OverlayActivity.class),
            Pair.create("Toolbar sample", ToolbarActivity.class),
            Pair.create("Widget sample", WidgetActivity.class),
            Pair.create("ListItem sample", CarUiListItemActivity.class)
    );

    private class ViewHolder extends RecyclerView.ViewHolder {
        private Button mButton;

        ViewHolder(@NonNull View itemView) {
            super(itemView);
            mButton = itemView.findViewById(R.id.button);
        }

        void update(String title, Class<? extends Activity> activityClass) {
            mButton.setText(title);
            mButton.setOnClickListener(e -> {
                Intent intent = new Intent(mButton.getContext(), activityClass);
                startActivity(intent);
            });
        }
    }

    private final RecyclerView.Adapter<ViewHolder> mAdapter =
            new RecyclerView.Adapter<ViewHolder>() {
        @NonNull
        @Override
        public ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
            View item = LayoutInflater.from(parent.getContext()).inflate(R.layout.list_item, parent,
                    false);
            return new ViewHolder(item);
        }

        @Override
        public void onBindViewHolder(@NonNull ViewHolder holder, int position) {
            Pair<String, Class<? extends Activity>> item = mActivities.get(position);
            holder.update(item.first, item.second);
        }

        @Override
        public int getItemCount() {
            return mActivities.size();
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main_activity);

        CarUiRecyclerView prv = findViewById(R.id.activities);
        prv.setAdapter(mAdapter);

        initLeakCanary();
    }

    private void initLeakCanary() {
        // This sets LeakCanary to report errors after a single leak instead of 5, and to ask for
        // permission to use storage, which it needs to work.
        //
        // Equivalent to this non-reflection code:
        //
        // Config config = LeakCanary.INSTANCE.getConfig();
        // LeakCanary.INSTANCE.setConfig(config.copy(config.getDumpHeap(),
        //     config.getDumpHeapWhenDebugging(),
        //     1,
        //     config.getReferenceMatchers(),
        //     config.getObjectInspectors(),
        //     config.getOnHeapAnalyzedListener(),
        //     config.getMetatadaExtractor(),
        //     config.getComputeRetainedHeapSize(),
        //     config.getMaxStoredHeapDumps(),
        //     true,
        //     config.getUseExperimentalLeakFinders()));
        try {
            Class<?> canaryClass = Class.forName("leakcanary.LeakCanary");
            try {
                Class<?> onHeapAnalyzedListenerClass =
                        Class.forName("leakcanary.OnHeapAnalyzedListener");
                Class<?> metadataExtractorClass = Class.forName("shark.MetadataExtractor");
                Method getConfig = canaryClass.getMethod("getConfig");
                Class<?> configClass = getConfig.getReturnType();
                Method setConfig = canaryClass.getMethod("setConfig", configClass);
                Method copy = configClass.getMethod("copy", boolean.class, boolean.class,
                        int.class, List.class, List.class, onHeapAnalyzedListenerClass,
                        metadataExtractorClass, boolean.class, int.class, boolean.class,
                        boolean.class);

                Object canary = canaryClass.getField("INSTANCE").get(null);
                Object currentConfig = getConfig.invoke(canary);

                Boolean dumpHeap = (Boolean) configClass
                        .getMethod("getDumpHeap").invoke(currentConfig);
                Boolean dumpHeapWhenDebugging = (Boolean) configClass
                        .getMethod("getDumpHeapWhenDebugging").invoke(currentConfig);
                List<?> referenceMatchers = (List<?>) configClass
                        .getMethod("getReferenceMatchers").invoke(currentConfig);
                List<?> objectInspectors = (List<?>) configClass
                        .getMethod("getObjectInspectors").invoke(currentConfig);
                Object onHeapAnalyzedListener = configClass
                        .getMethod("getOnHeapAnalyzedListener").invoke(currentConfig);
                // Yes, LeakCanary misspelled metadata
                Object metadataExtractor = configClass
                        .getMethod("getMetatadaExtractor").invoke(currentConfig);
                Boolean computeRetainedHeapSize = (Boolean) configClass
                        .getMethod("getComputeRetainedHeapSize").invoke(currentConfig);
                Integer maxStoredHeapDumps = (Integer) configClass
                        .getMethod("getMaxStoredHeapDumps").invoke(currentConfig);
                Boolean useExperimentalLeakFinders = (Boolean) configClass
                        .getMethod("getUseExperimentalLeakFinders").invoke(currentConfig);

                setConfig.invoke(canary, copy.invoke(currentConfig,
                        dumpHeap,
                        dumpHeapWhenDebugging,
                        1,
                        referenceMatchers,
                        objectInspectors,
                        onHeapAnalyzedListener,
                        metadataExtractor,
                        computeRetainedHeapSize,
                        maxStoredHeapDumps,
                        true,
                        useExperimentalLeakFinders));

            } catch (ReflectiveOperationException e) {
                Log.e("paintbooth", "Error initializing LeakCanary", e);
                Toast.makeText(this, "Error initializing LeakCanary", Toast.LENGTH_LONG).show();
            }
        } catch (ClassNotFoundException e) {
            // LeakCanary is not used in this build, do nothing.
        }
    }
}
