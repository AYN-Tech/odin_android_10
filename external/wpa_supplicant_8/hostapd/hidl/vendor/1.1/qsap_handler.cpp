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

/* qsap interface for hostapd daemon */
#include "string"
#include "qsap_handler.h"
#include <qsap_api.h>

namespace vendor {
namespace qti {
namespace hardware {
namespace wifi {
namespace hostapd {
namespace V1_1 {
namespace implementation {
namespace qsap_handler {

int run_qsap_cmd(int argc, char** argv) {
	int ret = 0;

	if (argc < 3)
		return -1;

	if (!strcmp(argv[1], "qccmd")) {
		ret = qsap_hostd_exec(argc, argv);
	} else if (!strcmp(argv[1], "create") &&
		qsap_add_or_remove_interface(argv[2], 1)) {
	} else if (!strcmp(argv[1], "remove") &&
		qsap_add_or_remove_interface(argv[2], 0)) {
	} else if (!strcmp(argv[1], "bridge")) {
		ret = qsap_control_bridge(argc, argv);
	} else if (!strcmp(argv[1], "setsoftap")) {
		ret = qsapsetSoftap(argc, argv);
	} else {
		ret = -1;
	}

	return ret;
}

}  // namespace qsap_handler
}  // namespace implementation
}  // namespace V1_1
}  // namespace hostapd
}  // namespace wifi
}  // namespace hardware
}  // namespace qti
}  // namespace vendor
