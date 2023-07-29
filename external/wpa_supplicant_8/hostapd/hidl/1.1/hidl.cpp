/*
 * hidl interface for wpa_supplicant daemon
 * Copyright (c) 2004-2018, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2004-2018, Roshan Pius <rpius@google.com>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include <hwbinder/IPCThreadState.h>
#include <hidl/HidlTransportSupport.h>
#include <hwbinder/ProcessState.h>
#include <cutils/properties.h>

#include "hostapd.h"
#ifdef CONFIG_USE_VENDOR_HIDL
#include "hostapd_vendor.h"
#endif /* CONFIG_USE_VENDOR_HIDL */

extern "C"
{
#include "hidl.h"
#include "utils/common.h"
#include "utils/eloop.h"
#include "utils/includes.h"
}

using android::hardware::configureRpcThreadpool;
using android::hardware::IPCThreadState;
using android::hardware::wifi::hostapd::V1_1::IHostapd;
using android::hardware::wifi::hostapd::V1_1::implementation::Hostapd;

// This file is a bridge between the hostapd code written in 'C' and the HIDL
// interface in C++. So, using "C" style static globals here!
static int hidl_fd = -1;
static android::sp<IHostapd> service;

#ifdef CONFIG_USE_VENDOR_HIDL
using vendor::qti::hardware::wifi::hostapd::V1_1::IHostapdVendor;
using vendor::qti::hardware::wifi::hostapd::V1_1::implementation::HostapdVendor;

static android::sp<HostapdVendor> vendor_service;
#endif /* CONFIG_USE_VENDOR_HIDL */

void hostapd_hidl_sock_handler(
    int /* sock */, void * /* eloop_ctx */, void * /* sock_ctx */)
{
	IPCThreadState::self()->handlePolledCommands();
}

#ifdef ARCH_ARM_32
#define DEFAULT_WIFISUPP_HW_BINDER_SIZE_KB 4
size_t getHWBinderMmapSize() {
	size_t value = 0;
	value = property_get_int32("persist.vendor.wifi.supplicant.hw.binder.size", DEFAULT_WIFISUPP_HW_BINDER_SIZE_KB);
	if (!value) value = DEFAULT_WIFISUPP_HW_BINDER_SIZE_KB; // deafult to 1 page of 4 Kb

	return 1024 * value;
}
#endif /* ARCH_ARM_32 */

int hostapd_hidl_init(struct hapd_interfaces *interfaces)
{
	wpa_printf(MSG_DEBUG, "Initing hidl control");
#ifdef ARCH_ARM_32
	android::hardware::ProcessState::initWithMmapSize(getHWBinderMmapSize());
#endif /* ARCH_ARM_32 */

	IPCThreadState::self()->setupPolling(&hidl_fd);
	if (hidl_fd < 0)
		goto err;

	wpa_printf(MSG_INFO, "Processing hidl events on FD %d", hidl_fd);
	// Look for read events from the hidl socket in the eloop.
	if (eloop_register_read_sock(
		hidl_fd, hostapd_hidl_sock_handler, interfaces, NULL) < 0)
		goto err;
	service = new Hostapd(interfaces);
	if (!service)
		goto err;
	if (interfaces->hidl_service_name) {
		wpa_printf(MSG_DEBUG, "Override HIDL service name: %s",
			   interfaces->hidl_service_name);
		if (service->registerAsService(interfaces->hidl_service_name)
		    != android::NO_ERROR)
			goto err;
	} else {
		wpa_printf(MSG_DEBUG, "Using default HIDL service name");
		if (service->registerAsService() != android::NO_ERROR)
			goto err;
	}

#ifdef CONFIG_USE_VENDOR_HIDL
	vendor_service = new HostapdVendor(interfaces);
	if (!vendor_service)
		goto err;
	if (interfaces->hidl_service_name) {
		wpa_printf(MSG_DEBUG, "Override Vendor HIDL service name: %s",
			   interfaces->hidl_service_name);
		if (vendor_service->registerAsService(interfaces->hidl_service_name)
		    != android::NO_ERROR)
			goto err;
	} else {
		wpa_printf(MSG_DEBUG, "Using default Vendor HIDL service name");
		if (vendor_service->registerAsService() != android::NO_ERROR)
			goto err;
	}
#endif /* CONFIG_USE_VENDOR_HIDL */

	return 0;
err:
	hostapd_hidl_deinit(interfaces);
	return -1;
}

void hostapd_hidl_deinit(struct hapd_interfaces *interfaces)
{
	wpa_printf(MSG_DEBUG, "Deiniting hidl control");
	eloop_unregister_read_sock(hidl_fd);
	IPCThreadState::shutdown();
	hidl_fd = -1;
	service.clear();
	os_free(interfaces->hidl_service_name);
	interfaces->hidl_service_name = NULL;
#ifdef CONFIG_USE_VENDOR_HIDL
	vendor_service->invalidate();
	vendor_service.clear();
#endif /* CONFIG_USE_VENDOR_HIDL */
}
