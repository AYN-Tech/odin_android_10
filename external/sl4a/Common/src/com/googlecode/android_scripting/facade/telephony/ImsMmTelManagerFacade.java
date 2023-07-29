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

package com.googlecode.android_scripting.facade.telephony;

import android.telephony.ims.ImsMmTelManager;

import com.googlecode.android_scripting.facade.FacadeManager;
import com.googlecode.android_scripting.jsonrpc.RpcReceiver;
import com.googlecode.android_scripting.rpc.Rpc;
import com.googlecode.android_scripting.rpc.RpcParameter;

/**
 * Exposes ImsMmManager functionality
 */
public class ImsMmTelManagerFacade extends RpcReceiver {

    /**
     * Exposes ImsMmTelManager functionality
     */
    public ImsMmTelManagerFacade(FacadeManager manager) {
        super(manager);
    }

    /**
     * Get whether Advanced Calling is enabled for a subId
     *
     * @param subId The subscription ID of the sim you want to check
     */
    @Rpc(description = "Return True if Enhanced 4g Lte mode is enabled.")
    public boolean imsMmTelIsAdvancedCallingEnabled(@RpcParameter(name = "subId") Integer subId) {
        return ImsMmTelManager.createForSubscriptionId(subId).isAdvancedCallingSettingEnabled();
    }

    /**
     * Set whether Advanced Calling is enabled for a subId
     *
     * @param subId The subscription ID of the sim you want to check
     * @param isEnabled Whether the sim should have Enhanced 4g Lte on or off
     */
    @Rpc(description = "Set Enhanced 4g Lte mode")
    public void imsMmTelSetAdvancedCallingEnabled(
                        @RpcParameter(name = "subId") Integer subId,
                        @RpcParameter(name = "isEnabled") Boolean isEnabled) {
        ImsMmTelManager.createForSubscriptionId(subId).setAdvancedCallingSettingEnabled(isEnabled);
    }

    @Override
    public void shutdown() {

    }
}
