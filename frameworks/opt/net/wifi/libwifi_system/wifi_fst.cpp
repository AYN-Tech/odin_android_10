/*
 * Copyright (c) 2015,2017, The Linux Foundation. All rights reserved.
 *
 * Not a Contribution.
 * Copyright 2008, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include <android-base/logging.h>
#define LOG_TAG "WifiFST"
#include <android-base/file.h>
#include <android-base/stringprintf.h>
#include "cutils/properties.h"

#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>
#include <private/android_filesystem_config.h>

using android::base::StringPrintf;
using android::base::WriteStringToFile;
using std::string;

static const char FSTMAN_IFNAME[] = "wlan0";
static const char FSTMAN_NAME[] = "fstman";
static const char FST_RATE_UPGRADE_ENABLED_PROP_NAME[] = "persist.vendor.fst.rate.upgrade.en";
static const char FST_SOFTAP_ENABLED_PROP_NAME[] = "persist.vendor.fst.softap.en";

namespace android {
namespace wifi_system {

int is_fst_enabled()
{
    char prop_value[PROPERTY_VALUE_MAX] = { '\0' };

    if (property_get(FST_RATE_UPGRADE_ENABLED_PROP_NAME, prop_value, NULL) &&
        strcmp(prop_value, "1") == 0) {
        return 1;
    }

    return 0;
}

int is_fst_softap_enabled() {
    char prop_value[PROPERTY_VALUE_MAX] = { '\0' };

    if (is_fst_enabled() &&
        property_get(FST_SOFTAP_ENABLED_PROP_NAME, prop_value, NULL) &&
        strcmp(prop_value, "1") == 0) {
        return 1;
    }

    return 0;
}

static void get_fstman_props(int softap_mode,
			     char *fstman_svc_name, int fstman_svc_name_len,
			     char *fstman_init_prop, int fstman_init_prop_len)
{
    if (softap_mode)
        strlcpy(fstman_svc_name, FSTMAN_NAME, fstman_svc_name_len);
    else
        snprintf(fstman_svc_name, fstman_svc_name_len, "%s_%s",
                 FSTMAN_NAME, FSTMAN_IFNAME);
    snprintf(fstman_init_prop, fstman_init_prop_len, "init.svc.%s",
             fstman_svc_name);
}

int wifi_start_fstman(int softap_mode)
{
    char fstman_status[PROPERTY_VALUE_MAX] = { '\0' };
    char fstman_svc_name[PROPERTY_VALUE_MAX] = { '\0' };
    char fstman_init_prop[PROPERTY_VALUE_MAX] = { '\0' };
    int count = 50; /* wait at most 5 seconds for completion */

    if (!is_fst_enabled() ||
        (softap_mode && !is_fst_softap_enabled())) {
        return 0;
    }

    get_fstman_props(softap_mode, fstman_svc_name, sizeof(fstman_svc_name),
                     fstman_init_prop, sizeof(fstman_init_prop));

    /* Check whether already running */
    if (property_get(fstman_init_prop, fstman_status, NULL) &&
        strcmp(fstman_status, "running") == 0)
        return 0;

    LOG(DEBUG) << "Starting FST Manager";
    property_set("ctl.start", fstman_svc_name);
    sched_yield();

    while (count-- > 0) {
        if (property_get(fstman_init_prop, fstman_status, NULL) &&
            strcmp(fstman_status, "running") == 0)
                return 0;
        usleep(100000);
    }

    LOG(ERROR) << "Failed to start FST Manager";
    return -1;
}

int wifi_stop_fstman(int softap_mode)
{
    char fstman_status[PROPERTY_VALUE_MAX] = { '\0' };
    char fstman_svc_name[PROPERTY_VALUE_MAX] = { '\0' };
    char fstman_init_prop[PROPERTY_VALUE_MAX] = { '\0' };
    int count = 50; /* wait at most 5 seconds for completion */

    if (!is_fst_enabled() ||
        (softap_mode && !is_fst_softap_enabled())) {
        return 0;
    }

    get_fstman_props(softap_mode, fstman_svc_name, sizeof(fstman_svc_name),
                     fstman_init_prop, sizeof(fstman_init_prop));

    /* Check whether already stopped */
    if (property_get(fstman_init_prop, fstman_status, NULL) &&
        strcmp(fstman_status, "stopped") == 0)
        return 0;

    LOG(DEBUG) << "Stopping FST Manager";
    property_set("ctl.stop", fstman_svc_name);
    sched_yield();

    while (count-- > 0) {
        if (property_get(fstman_init_prop, fstman_status, NULL) &&
            strcmp(fstman_status, "stopped") == 0)
                return 0;
        usleep(100000);
    }

    LOG(ERROR) << "Failed to stop fstman";
    return -1;
}

}  // namespace wifi_system
}  // namespace android
