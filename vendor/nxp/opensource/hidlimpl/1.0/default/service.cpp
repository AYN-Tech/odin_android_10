/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
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
 *
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

#define LOG_TAG "vendor.nxp.hardware.nfc@1.0-service"

#include <android/hardware/nfc/1.0/INfc.h>
#include <vendor/nxp/hardware/nfc/1.0/INqNfc.h>
#include <hwbinder/ProcessState.h>
#include <hidl/LegacySupport.h>

using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;
using android::hardware::nfc::V1_0::INfc;
using vendor::nxp::hardware::nfc::V1_0::INqNfc;
using android::hardware::registerPassthroughServiceImplementation;
using android::OK;

int main() {
    android::hardware::ProcessState::initWithMmapSize((size_t)(8192));
    configureRpcThreadpool(1, true /*callerWillJoin*/);
    android::status_t status;
    status = registerPassthroughServiceImplementation<INfc>();
    LOG_ALWAYS_FATAL_IF(status != OK, "Error while registering nfc AOSP service: %d", status);
    status = registerPassthroughServiceImplementation<INqNfc>();
    LOG_ALWAYS_FATAL_IF(status != OK, "Error while registering nqnfc vendor service: %d", status);
    joinRpcThreadpool();
    return status;
}
