/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "wifi_system/hostapd_manager.h"

#include <android-base/logging.h>
#include <cutils/properties.h>
#include "wifi_fst.h"

namespace android {
namespace wifi_system {
const char kHostapdServiceName[] = "hostapd";
const char kHostapdFSTServiceName[] = "hostapd_fst";
const char kHostapdFullServiceName[] = "init.svc.hostapd";
const char kHostapdFSTFullServiceName[] = "init.svc.hostapd_fst";

bool HostapdManager::StartHostapd() {

  // Check if hostapd already started
  char hostapd_status[PROPERTY_VALUE_MAX];
  property_get(is_fst_softap_enabled() ? kHostapdFSTFullServiceName :
    kHostapdFullServiceName, hostapd_status, "");

  if (strcmp(hostapd_status, "running") == 0) {
    LOG(DEBUG) << "SoftAP already started. Skip another start";
    return true;
  }

  if (wifi_start_fstman(1)) {
    return false;
  }

  if (property_set("ctl.start",
                   is_fst_softap_enabled() ?
                     kHostapdFSTServiceName : kHostapdServiceName) != 0) {
    LOG(ERROR) << "Failed to start SoftAP";
    wifi_stop_fstman(1);
    return false;
  }

  LOG(DEBUG) << "SoftAP started successfully";
  return true;
}

bool HostapdManager::StopHostapd() {
  LOG(DEBUG) << "Stopping the SoftAP service...";

  // Check if hostapd already stopped
  char hostapd_status[PROPERTY_VALUE_MAX];
  property_get(is_fst_softap_enabled() ? kHostapdFSTFullServiceName :
    kHostapdFullServiceName, hostapd_status, "");

  if (!strlen(hostapd_status) || strcmp(hostapd_status, "stopped") == 0) {
    LOG(DEBUG) << "SoftAP already stopped. Skip another stop";
    wifi_stop_fstman(1);
    return true;
  }

  if (property_set("ctl.stop",
                   is_fst_softap_enabled() ?
                     kHostapdFSTServiceName : kHostapdServiceName) < 0) {
    LOG(ERROR) << "Failed to stop hostapd service!";
    wifi_stop_fstman(1);
    return false;
  }

  wifi_stop_fstman(1);
  LOG(DEBUG) << "SoftAP stopped successfully";
  return true;
}
}  // namespace wifi_system
}  // namespace android
