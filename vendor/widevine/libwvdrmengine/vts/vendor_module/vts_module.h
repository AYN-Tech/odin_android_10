//
// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
#ifndef WIDEVINE_VENDOR_VTS_MODULE
#define WIDEVINE_VENDOR_VTS_MODULE

#include <utils/Log.h>

#include "drm_hal_vendor_module_api.h"

using std::vector;

/**
 * Define the DrmHalVTSVendorModule_V1 for Widevine. Refer to
 * drm_hal_vendor_module_api.h for more details.
 */
namespace widevine_vts {

    class WidevineVTSVendorModule_V1 : public DrmHalVTSVendorModule_V1 {
    public:
        WidevineVTSVendorModule_V1() {}
        virtual ~WidevineVTSVendorModule_V1() {}

        virtual vector<uint8_t> getUUID() const;

        virtual vector<uint8_t> handleProvisioningRequest(const vector<uint8_t>&
                provisioningRequest, const std::string& url);

        virtual vector<uint8_t> handleKeyRequest(const vector<uint8_t>&
                keyRequest, const std::string& serverUrl);

        virtual std::vector<ContentConfiguration>
                getContentConfigurations() const;

        virtual std::string getServiceName() const {return "widevine";}

    private:
        WidevineVTSVendorModule_V1(const WidevineVTSVendorModule_V1&) = delete;
        void operator=(const WidevineVTSVendorModule_V1&) = delete;

        std::string toString(std::vector<uint8_t> vector) {
            return std::string(vector.begin(), vector.end());
        }
    };
}; // namespace widevine_vts

#endif //WIDEVINE_VENDOR_VTS_MODULE
