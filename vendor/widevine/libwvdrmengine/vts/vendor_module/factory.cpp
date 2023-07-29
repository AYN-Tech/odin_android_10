/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "drm_hal_vendor_module_api.h"
#include "vts_module.h"

namespace widevine_vts {
    extern "C" {
        DrmHalVTSVendorModule *vendorModuleFactory() {
            return new WidevineVTSVendorModule_V1();
        }
    };
}; // namespace widevine_vts
