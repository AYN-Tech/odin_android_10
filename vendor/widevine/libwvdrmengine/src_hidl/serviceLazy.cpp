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
#define LOG_TAG "WidevineHidlService"

#include <android-base/logging.h>
#include <binder/ProcessState.h>
#include <hidl/HidlLazyUtils.h>
#include <hidl/HidlTransportSupport.h>

#include "WVCryptoFactory.h"
#include "WVDrmFactory.h"

using wvdrm::hardware::drm::V1_2::widevine::WVCryptoFactory;
using wvdrm::hardware::drm::V1_2::widevine::WVDrmFactory;
using android::hardware::LazyServiceRegistrar;

int main(int /* argc */, char** /* argv */) {
    sp<IDrmFactory> drmFactory = new WVDrmFactory;
    sp<ICryptoFactory> cryptoFactory = new WVCryptoFactory;

    configureRpcThreadpool(8, true /* callerWillJoin */);

    // Setup hwbinder service
    LazyServiceRegistrar serviceRegistrar;
    CHECK_EQ(serviceRegistrar.registerService(drmFactory, "widevine"), android::NO_ERROR)
        << "Failed to register Widevine Factory HAL";
    CHECK_EQ(serviceRegistrar.registerService(cryptoFactory, "widevine"), android::NO_ERROR)
        << "Failed to register Widevine Crypto  HAL";

    joinRpcThreadpool();
}
