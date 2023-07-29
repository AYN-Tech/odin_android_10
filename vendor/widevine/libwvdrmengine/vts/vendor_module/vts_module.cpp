//
// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
#include "gtest/gtest.h"
#include "license_request.h"
#include "string_conversions.h"
#include "url_request.h"
#include "utils/Log.h"
#include "vts_module.h"

using std::array;
using std::map;
using std::string;
using std::vector;
using wvcdm::a2b_hex;
using wvcdm::b2a_hex;
using wvcdm::LicenseRequest;
using wvcdm::UrlRequest;

namespace widevine_vts {

    const int kHttpOk = 200;

    vector<uint8_t> WidevineVTSVendorModule_V1::getUUID() const {
        uint8_t uuid[16] = {
            0xED,0xEF,0x8B,0xA9,0x79,0xD6,0x4A,0xCE,
            0xA3,0xC8,0x27,0xDC,0xD5,0x1D,0x21,0xED
        };
        return vector<uint8_t>(uuid, uuid + sizeof(uuid));
    }

    void LogResponseError(const string& message, int http_status_code) {
        ALOGD("HTTP Status code = %d", http_status_code);
        ALOGD("HTTP response(%zd): %s", message.size(), b2a_hex(message).c_str());
    }

    vector<uint8_t> WidevineVTSVendorModule_V1::handleProvisioningRequest(
            const vector<uint8_t>& provisioning_request,
            const string& server_url) {

        // Use secure connection and chunk transfer coding.
        UrlRequest url_request(server_url);
        EXPECT_TRUE(url_request.is_connected()) << "Fail to connect to "
                                                << server_url;
        url_request.PostCertRequestInQueryString(toString(provisioning_request));
        string reply;
        EXPECT_TRUE(url_request.GetResponse(&reply));

        int http_status_code = url_request.GetStatusCode(reply);
        if (kHttpOk != http_status_code) {
            LogResponseError(reply, http_status_code);
        }
        EXPECT_EQ(kHttpOk, http_status_code);
        vector<uint8_t> result(reply.begin(), reply.end());
        return vector<uint8_t>(result);
    }

    vector<uint8_t> WidevineVTSVendorModule_V1::handleKeyRequest(
            const vector<uint8_t>&key_request, const string& server_url) {

        // Use secure connection and chunk transfer coding.
        UrlRequest url_request(server_url);
        EXPECT_TRUE(url_request.is_connected()) << "Fail to connect to "
                                               << server_url;
        url_request.PostRequest(toString(key_request));
        string reply;
        EXPECT_TRUE(url_request.GetResponse(&reply));

        int httpStatusCode = url_request.GetStatusCode(reply);
        if (httpStatusCode != kHttpOk) {
            LogResponseError(reply, httpStatusCode);
        }
        EXPECT_EQ(httpStatusCode, kHttpOk);

        string drm_msg;
        if (kHttpOk == httpStatusCode) {
            LicenseRequest lic_request;
            lic_request.GetDrmMessage(reply, drm_msg);
            ALOGV("HTTP response body: (%zd bytes)", drm_msg.size());
        }
        vector<uint8_t> result(drm_msg.begin(), drm_msg.end());
        return result;
    }

    vector<DrmHalVTSVendorModule_V1::ContentConfiguration>
    WidevineVTSVendorModule_V1::getContentConfigurations() const {

        vector<DrmHalVTSVendorModule_V1::ContentConfiguration> configurations;
        {
            const string serverUrl = "http://widevine-proxy.appspot.com/proxy";
            const vector<uint8_t> initData = a2b_hex(
                    "00000042"                          // blob size
                    "70737368"                          // "pssh"
                    "00000000"                          // flags
                    "edef8ba979d64acea3c827dcd51d21ed"  // Widevine system id
                    "00000022"                          // pssh data size
                    "08011a0d7769646576696e655f746573"  // pssh data...
                    "74220f73747265616d696e675f636c69"
                    "7031");
            const vector<uint8_t> keyId = a2b_hex("371EA35E1A985D75D198A7F41020DC23");
            const vector<uint8_t> keyValue = a2b_hex("371EA35E1A985D75D198A7F41020DC23");
            const vector<DrmHalVTSVendorModule_V1::ContentConfiguration::Key> keys = {
                {
                    .isSecure = false,
                    .keyId = keyId,
                    .clearContentKey = keyValue
                }
            };

            ContentConfiguration config = {
                .name = "streaming_clip1",
                .serverUrl = serverUrl,
                .initData = initData,
                .mimeType = "cenc",
                .optionalParameters = map<string, string>(),
                .policy.allowOffline = false,
                .keys = keys
            };
            configurations.push_back(config);
        }

        // Content Configuration #2 - Allows offline playback
        {
            const string serverUrl = "http://widevine-proxy.appspot.com/proxy";
            const vector<uint8_t> initData = a2b_hex(
                    "00000042"                          // blob size
                    "70737368"                          // "pssh"
                    "00000000"                          // flags
                    "edef8ba979d64acea3c827dcd51d21ed"  // Widevine system id
                    "00000020"                          // pssh data size
                    "08011a0d7769646576696e655f746573"  // pssh data...
                    "74220d6f66666c696e655f636c697033"
                    "7031");
            const vector<uint8_t> keyId = a2b_hex("3260f39e12ccf653529990168a3583ff");
            const vector<uint8_t> keyValue = a2b_hex("8040c019929b2cc116a2e8dac739eafa");
            const vector<DrmHalVTSVendorModule_V1::ContentConfiguration::Key> keys = {
                {
                    .isSecure = false,
                    .keyId = keyId,
                    .clearContentKey = keyValue
                }
            };

            ContentConfiguration config = {
                .name = "offline_clip3",
                .serverUrl = serverUrl,
                .initData = initData,
                .mimeType = "cenc",
                .optionalParameters = map<string, string>(),
                .policy.allowOffline = true,
                .keys = keys
            };
            configurations.push_back(config);
        };

        return configurations;
    }

}; // namespace widevine_vts
