/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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
 *
 */

/* vendor hidl interface for hostapd daemon */

#ifndef HOSTAPD_VENDOR_HIDL_SUPPLICANT_H
#define HOSTAPD_VENDOR_HIDL_SUPPLICANT_H

#include <string>

#include <android-base/macros.h>

#include <android/hardware/wifi/hostapd/1.0/IHostapd.h>
#include <vendor/qti/hardware/wifi/hostapd/1.1/IHostapdVendor.h>
#include <vendor/qti/hardware/wifi/hostapd/1.1/IHostapdVendorIfaceCallback.h>

extern "C"
{
#include "utils/common.h"
#include "utils/includes.h"
#include "utils/wpa_debug.h"
#include "ap/hostapd.h"
}

namespace vendor {
namespace qti {
namespace hardware {
namespace wifi {
namespace hostapd {
namespace V1_1 {
namespace implementation {

using namespace android::hardware;
using namespace android::hardware::wifi::hostapd::V1_0;

typedef IHostapd::IfaceParams IfaceParams;
typedef IHostapd::NetworkParams NetworkParams;

/**
 * Implementation of the hostapd hidl object. This hidl
 * object is used core for global control operations on
 * hostapd.
 */
class HostapdVendor : public V1_1::IHostapdVendor
{
public:
	HostapdVendor(hapd_interfaces* interfaces);
	HostapdVendor() = default;
	~HostapdVendor() override = default;

	// Vendor Hidl methods exposed.
	Return<void> addVendorAccessPoint(
	    const V1_0::IHostapdVendor::VendorIfaceParams& iface_params, const NetworkParams& nw_params,
	    addVendorAccessPoint_cb _hidl_cb) override;
	Return<void> addVendorAccessPoint_1_1(
	    const VendorIfaceParams& iface_params, const NetworkParams& nw_params,
	    addVendorAccessPoint_cb _hidl_cb) override;
	Return<void> removeVendorAccessPoint(
	    const hidl_string& iface_name,
	    removeVendorAccessPoint_cb _hidl_cb) override;
	Return<void> setHostapdParams(
	    const hidl_string& cmd,
	    setHostapdParams_cb _hidl_cb) override;
	Return<void> registerVendorCallback(
	    const hidl_string& cmd,
	    const android::sp<V1_0::IHostapdVendorIfaceCallback>& callback,
	    registerVendorCallback_cb _hidl_cb) override;
	Return<void> registerVendorCallback_1_1(
	    const hidl_string& iface_name,
	    const android::sp<IHostapdVendorIfaceCallback>& callback,
	    registerVendorCallback_cb _hidl_cb) override;
	Return<void> setDebugParams(
	    IHostapdVendor::DebugLevel level, bool show_timestamp, bool show_keys,
	    setDebugParams_cb _hidl_cb) override;
	Return<IHostapdVendor::DebugLevel> getDebugLevel() override;
	int addVendorIfaceCallbackHidlObject(
	    const std::string &ifname,
	    const android::sp<IHostapdVendorIfaceCallback> &callback);
	void invalidate();

private:
	// Corresponding worker functions for the HIDL methods.
	HostapdStatus addVendorAccessPointInternal(
	    const V1_0::IHostapdVendor::VendorIfaceParams& iface_params, const NetworkParams& nw_params);
	HostapdStatus __addVendorAccessPointInternal_1_1(
	    const VendorIfaceParams& iface_params, const NetworkParams& nw_params);
	HostapdStatus addVendorAccessPointInternal_1_1(
	    const VendorIfaceParams& iface_params, const NetworkParams& nw_params);
	HostapdStatus removeVendorAccessPointInternal(const std::string& iface_name);
	HostapdStatus setHostapdParamsInternal(const std::string& cmd);
	HostapdStatus registerCallbackInternal(
	    const std::string& iface_name,
	    const android::sp<V1_0::IHostapdVendorIfaceCallback>& callback);
	HostapdStatus registerCallbackInternal_1_1(
	    const std::string& iface_name,
	    const android::sp<IHostapdVendorIfaceCallback>& callback);
	HostapdStatus setDebugParamsInternal(
	    IHostapdVendor::DebugLevel level, bool show_timestamp, bool show_keys);
	void callWithEachHostapdIfaceCallback(
	    const std::string &ifname,
	    const std::function<android::hardware::Return<void>(
		android::sp<IHostapdVendorIfaceCallback>)> &method);
	void setIfacename(const std::string &ifname);
	// Raw pointer to the global structure maintained by the core.
	struct hapd_interfaces* interfaces_;
        std::map<
	    const std::string,
	    std::vector<android::sp<IHostapdVendorIfaceCallback>>>
	    vendor_hostapd_callbacks_map_;

	DISALLOW_COPY_AND_ASSIGN(HostapdVendor);
};
}  // namespace implementation
}  // namespace V1_1
}  // namespace hostapd
}  // namespace wifi
}  // namespace hardware
}  // namespace qti
}  // namespace vendor

#endif  // HOSTAPD_VENDOR_HIDL_SUPPLICANT_H
