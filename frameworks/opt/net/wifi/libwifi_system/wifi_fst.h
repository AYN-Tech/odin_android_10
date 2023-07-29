/*
 * Copyright (c) 2015,2017, The Linux Foundation. All rights reserved.
 *
 * Not a Contribution.
 * Copyright (C) 2008 The Android Open Source Project
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

#ifndef _WIFI_FST_H
#define _WIFI_FST_H

namespace android {
namespace wifi_system {

/**
 * returns whether FST is enabled or not according to
 * persist.vendor.fst.rate.upgrade.en system property
 *
 * @return 1 if FST enabled.
 */
int is_fst_enabled();

/**
* returns whether FST Soft AP is enabled or not according to
* persist.vendor.fst.softap.en system property
*
* @return 1 if FST Soft AP enabled.
*/
int is_fst_softap_enabled();

/**
 * Start FST Manager.
 *
 * @return 0 on success, < 0 on failure.
 */
int wifi_start_fstman(int softap_mode);

/**
 * Stop FST Manager.
 *
 * @return 0 on success, < 0 on failure.
 */
int wifi_stop_fstman(int softap_mode);

}  // namespace wifi_system
}  // namespace android

#endif  // _WIFI_FST_H
