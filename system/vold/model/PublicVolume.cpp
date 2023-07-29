/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "PublicVolume.h"
#include "Utils.h"
#include "VolumeManager.h"
#include "fs/Exfat.h"
#include "fs/Vfat.h"
#include "fs/Ntfs.h"
#include "fs/Ext4.h"

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/stringprintf.h>
#include <cutils/fs.h>
#include <private/android_filesystem_config.h>
#include <utils/Timers.h>

#include <fcntl.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/wait.h>

using android::base::GetBoolProperty;
using android::base::StringPrintf;

namespace android {
namespace vold {

static const char* kFusePath = "/system/bin/sdcard";

static const char* kAsecPath = "/mnt/secure/asec";

PublicVolume::PublicVolume(dev_t device) : VolumeBase(Type::kPublic), mDevice(device), mFusePid(0) {
    setId(StringPrintf("public:%u,%u", major(device), minor(device)));
    mDevPath = StringPrintf("/dev/block/vold/%s", getId().c_str());
}

PublicVolume::~PublicVolume() {}

status_t PublicVolume::readMetadata() {
    status_t res = ReadMetadataUntrusted(mDevPath, &mFsType, &mFsUuid, &mFsLabel);

    auto listener = getListener();
    if (listener) listener->onVolumeMetadataChanged(getId(), mFsType, mFsUuid, mFsLabel);

    return res;
}

status_t PublicVolume::initAsecStage() {
    std::string legacyPath(mRawPath + "/android_secure");
    std::string securePath(mRawPath + "/.android_secure");

    // Recover legacy secure path
    if (!access(legacyPath.c_str(), R_OK | X_OK) && access(securePath.c_str(), R_OK | X_OK)) {
        if (rename(legacyPath.c_str(), securePath.c_str())) {
            PLOG(WARNING) << getId() << " failed to rename legacy ASEC dir";
        }
    }

    if (TEMP_FAILURE_RETRY(mkdir(securePath.c_str(), 0700))) {
        if (errno != EEXIST) {
            PLOG(WARNING) << getId() << " creating ASEC stage failed";
            return -errno;
        }
    }

    BindMount(securePath, kAsecPath);

    return OK;
}

status_t PublicVolume::doCreate() {
    return CreateDeviceNode(mDevPath, mDevice);
}

status_t PublicVolume::doDestroy() {
    return DestroyDeviceNode(mDevPath);
}
#if 0
bool PublicVolume::adjustIosched() {
	if (!writeStringToFile("100", "/sys/block/mmcblk0/queue/iosched/fifo_expire_async")) {
		PLOG(ERROR) << "adjustIosched failed to fifo_expire_async";
		return false;
	}
	if (!writeStringToFile("50", "/sys/block/mmcblk0/queue/iosched/fifo_expire_sync")) {
		PLOG(ERROR) << "adjustIosched failed to fifo_expire_sync";
		return false;
	}

	if (!writeStringToFile("8", "/sys/block/mmcblk0/queue/iosched/group_idle")) {
		PLOG(ERROR) << "adjustIosched failed to group_idle";
		return false;
	}

	if (!writeStringToFile("1", "/sys/block/mmcblk0/queue/iosched/low_latency")) {
		PLOG(ERROR) << "adjustIosched failed to low_latency";
		return false;
	}
	if (!writeStringToFile("4", "/sys/block/mmcblk0/queue/iosched/quantum")) {
		PLOG(ERROR) << "adjustIosched failed to quantum";
		return false;
	}
	if (!writeStringToFile("40", "/sys/block/mmcblk0/queue/iosched/slice_async")) {
		PLOG(ERROR) << "adjustIosched failed to slice_async";
		return false;
	}
	if (!writeStringToFile("8", "/sys/block/mmcblk0/queue/iosched/slice_sync")) {
		PLOG(ERROR) << "adjustIosched failed to slice_sync";
		return false;
	}
	if (!writeStringToFile("50", "/sys/block/mmcblk0/queue/iosched/target_latency")) {
		PLOG(ERROR) << "adjustIosched failed to target_latency";
		return false;
	}

	/*
	if (!writeStringToFile("0", "/sys/block/mmcblk0/queue/iosched/slice_idle")) {
		PLOG(ERROR) << "adjustIosched failed to slice_idle";
		return false;
	}

	if (!writeStringToFile("1", "/sys/block/mmcblk0/queue/iosched/low_latency")) {
		PLOG(ERROR) << "adjustIosched failed to low_latency";
		return false;
	}

	if (!writeStringToFile("4096", "/sys/block/mmcblk0/queue/discard_granularity")) {
		PLOG(ERROR) << "adjustIosched failed to discard_granularity";
		return false;
	}
	if (!writeStringToFile("34359734272", "/sys/block/mmcblk0/queue/discard_max_bytes")) {
		PLOG(ERROR) << "adjustIosched failed to discard_max_bytes";
		return false;
	}
	if (!writeStringToFile("34359734272", "/sys/block/mmcblk0/queue/discard_max_hw_bytes")) {
		PLOG(ERROR) << "adjustIosched failed to discard_max_hw_bytes";
		return false;
	}
	if (!writeStringToFile("0", "/sys/block/mmcblk0/queue/discard_zeroes_data")) {
		PLOG(ERROR) << "adjustIosched failed to discard_zeroes_data";
		return false;
	}
	if (!writeStringToFile("4096", "/sys/block/mmcblk0/queue/hw_sector_size")) {
		PLOG(ERROR) << "adjustIosched failed to hw_sector_size";
		return false;
	}
	if (!writeStringToFile("4096", "/sys/block/mmcblk0/queue/logical_block_size")) {
		PLOG(ERROR) << "adjustIosched failed to logical_block_size";
		return false;
	}
	if (!writeStringToFile("4096", "/sys/block/mmcblk0/queue/max_segment_size")) {
		PLOG(ERROR) << "adjustIosched failed to max_segment_size";
		return false;
	}
	if (!writeStringToFile("65536", "/sys/block/mmcblk0/queue/minimum_io_size")) {
		PLOG(ERROR) << "adjustIosched failed to minimum_io_size";
		return false;
	}
	if (!writeStringToFile("65536", "/sys/block/mmcblk0/queue/optimal_io_size")) {
		PLOG(ERROR) << "adjustIosched failed to optimal_io_size";
		return false;
	}
	if (!writeStringToFile("4096", "/sys/block/mmcblk0/queue/physical_block_size")) {
		PLOG(ERROR) << "adjustIosched failed to physical_block_size";
		return false;
	}
	*/
	if (!writeStringToFile("32", "/sys/block/mmcblk0/queue/nr_requests")) {
		PLOG(ERROR) << "adjustIosched failed to nr_requests";
		return false;
	}
	if (!writeStringToFile("128", "/sys/block/mmcblk0/queue/read_ahead_kb")) {
		PLOG(ERROR) << "adjustIosched failed to read_ahead_kb";
		return false;
	}
	return true;
}
#endif

status_t PublicVolume::doMount() {
    readMetadata();

	if (mFsType == "ext4") {
        int res = ext4::Check(mDevPath, mRawPath);
        if (res == 0 || res == 1) {
            LOG(DEBUG) << getId() << " passed filesystem check";
        } else {
            PLOG(ERROR) << getId() << " failed filesystem ext4 check";
            return -EIO;
        }
    } else if (mFsType == "vfat" && vfat::IsSupported()) {
        if (vfat::Check(mDevPath)) {
            LOG(ERROR) << getId() << " failed filesystem vfat check";
            return -EIO;
        }
    } else if (mFsType == "exfat") {
        if (exfat::Check(mDevPath)) {
            LOG(ERROR) << getId() << " failed filesystem exfat check";
            //return -EIO;
        }
    } else if (mFsType == "ntfs" && ntfs::IsSupported()) {
        if (ntfs::Check(mDevPath)) {
            LOG(ERROR) << getId() << "  ntfs failed check";
            return -EIO;
        }
    } else {
        LOG(ERROR) << getId() << " unsupported filesystem " << mFsType;
        return -EIO;
    }
	

    // Use UUID as stable name, if available
    std::string stableName = getId();
    if (!mFsUuid.empty()) {
        stableName = mFsUuid;
    }

    mRawPath = StringPrintf("/mnt/media_rw/%s", stableName.c_str());
	LOG(ERROR) << getId() << " filesystem " << mFsType << "/proc/mounts mRawPath=" << mRawPath;

    mFuseDefault = StringPrintf("/mnt/runtime/default/%s", stableName.c_str());
    mFuseRead = StringPrintf("/mnt/runtime/read/%s", stableName.c_str());
    mFuseWrite = StringPrintf("/mnt/runtime/write/%s", stableName.c_str());
    mFuseFull = StringPrintf("/mnt/runtime/full/%s", stableName.c_str());

    setInternalPath(mRawPath);
    if (getMountFlags() & MountFlags::kVisible) {
        setPath(StringPrintf("/storage/%s", stableName.c_str()));
    } else {
        setPath(mRawPath);
    }

    if (fs_prepare_dir(mRawPath.c_str(), 0700, AID_ROOT, AID_ROOT)) {
        PLOG(ERROR) << getId() << " failed to create mount points";
        return -errno;
    }

	if (mFsType == "ext4") {
		if (ext4::Mount(mDevPath, mRawPath, false, false, true)) {
            PLOG(ERROR) << getId() << " failed to mount";
            return -EIO;
        }
	} else if (mFsType == "vfat") {
        if (vfat::Mount(mDevPath, mRawPath, false, false, false, AID_MEDIA_RW, AID_MEDIA_RW, 0007,
                        true)) {
            PLOG(ERROR) << getId() << " failed to mount " << mDevPath;
            return -EIO;
        }
    } else if(mFsType == "ntfs") {
        if (ntfs::Mount(mDevPath, mRawPath, false, false, false,
                        AID_MEDIA_RW, AID_MEDIA_RW, 0007, true)) {
            PLOG(ERROR) << getId() << " failed to mount " << mDevPath;
            return -EIO;
        }
    } else if (mFsType == "exfat") {
        if (exfat::Mount(mDevPath, mRawPath, AID_MEDIA_RW, AID_MEDIA_RW, 0007)) {
            PLOG(ERROR) << getId() << " failed to mount " << mDevPath;
            return -EIO;
        }
    }

    if (getMountFlags() & MountFlags::kPrimary) {
        initAsecStage();
    }

    if (!(getMountFlags() & MountFlags::kVisible)) {
        // Not visible to apps, so no need to spin up FUSE
        //return OK;
		LOG(ERROR) << getId() << " filesystem " << mFsType << " mRawPath=" << mRawPath;
    }

    if (fs_prepare_dir(mFuseDefault.c_str(), 0700, AID_ROOT, AID_ROOT) ||
        fs_prepare_dir(mFuseRead.c_str(), 0700, AID_ROOT, AID_ROOT) ||
        fs_prepare_dir(mFuseWrite.c_str(), 0700, AID_ROOT, AID_ROOT) ||
        fs_prepare_dir(mFuseFull.c_str(), 0700, AID_ROOT, AID_ROOT)) {
        PLOG(ERROR) << getId() << " failed to create FUSE mount points";
        return -errno;
    }

    dev_t before = GetDevice(mFuseFull);

    if (!(mFusePid = fork())) {
        if (getMountFlags() & MountFlags::kPrimary) {
            // clang-format off
            if (execl(kFusePath, kFusePath,
                    "-u", "1023", // AID_MEDIA_RW
                    "-g", "1023", // AID_MEDIA_RW
                    "-U", std::to_string(getMountUserId()).c_str(),
                    "-w",
                    mRawPath.c_str(),
                    stableName.c_str(),
                    NULL)) {
                // clang-format on
                PLOG(ERROR) << "Failed to exec";
            }
        } else {
            // clang-format off
            if (execl(kFusePath, kFusePath,
                    "-u", "1023", // AID_MEDIA_RW
                    "-g", "1023", // AID_MEDIA_RW
                    "-U", std::to_string(getMountUserId()).c_str(),
                    mRawPath.c_str(),
                    stableName.c_str(),
                    NULL)) {
                // clang-format on
                PLOG(ERROR) << "Failed to exec";
            }
        }

        LOG(ERROR) << "FUSE exiting";
        _exit(1);
    }

    if (mFusePid == -1) {
        PLOG(ERROR) << getId() << " failed to fork";
        return -errno;
    }

    nsecs_t start = systemTime(SYSTEM_TIME_BOOTTIME);
    while (before == GetDevice(mFuseFull)) {
        LOG(DEBUG) << "Waiting for FUSE to spin up...";
        usleep(50000);  // 50ms

        nsecs_t now = systemTime(SYSTEM_TIME_BOOTTIME);
        if (nanoseconds_to_milliseconds(now - start) > 5000) {
            LOG(WARNING) << "Timed out while waiting for FUSE to spin up";
            return -ETIMEDOUT;
        }
    }
    /* sdcardfs will have exited already. FUSE will still be running */
    TEMP_FAILURE_RETRY(waitpid(mFusePid, nullptr, 0));
    mFusePid = 0;
	LOG(DEBUG) << "FUSE will still be running...";
	LOG(ERROR) << getId() << " filesystem " << mFsType << " mRawPath=" << mRawPath << " mDevPath=" << mDevPath;

	//adjustIosched();
    return OK;
}

status_t PublicVolume::doUnmount() {
    // Unmount the storage before we kill the FUSE process. If we kill
    // the FUSE process first, most file system operations will return
    // ENOTCONN until the unmount completes. This is an exotic and unusual
    // error code and might cause broken behaviour in applications.
    KillProcessesUsingPath(getPath());

    ForceUnmount(kAsecPath);

    ForceUnmount(mFuseDefault);
    ForceUnmount(mFuseRead);
    ForceUnmount(mFuseWrite);
    ForceUnmount(mFuseFull);
    ForceUnmount(mRawPath);

    rmdir(mFuseDefault.c_str());
    rmdir(mFuseRead.c_str());
    rmdir(mFuseWrite.c_str());
    rmdir(mFuseFull.c_str());
    rmdir(mRawPath.c_str());

    mFuseDefault.clear();
    mFuseRead.clear();
    mFuseWrite.clear();
    mFuseFull.clear();
    mRawPath.clear();

    return OK;
}

status_t PublicVolume::doFormat(const std::string& fsType) {
    bool useVfat = vfat::IsSupported();
    bool useExfat = exfat::IsSupported();
    status_t res = OK;

    // Resolve the target filesystem type
    if (fsType == "auto" && useVfat && useExfat) {
        uint64_t size = 0;

        res = GetBlockDevSize(mDevPath, &size);
        if (res != OK) {
            LOG(ERROR) << "Couldn't get device size " << mDevPath;
            return res;
        }

        // If both vfat & exfat are supported use exfat for SDXC (>~32GiB) cards
        if (size > 32896LL * 1024 * 1024) {
            useVfat = false;
        } else {
            useExfat = false;
        }
    } else if (fsType == "vfat") {
        useExfat = false;
    } else if (fsType == "exfat") {
        useVfat = false;
    } else if (fsType == "ext4") {
        // TODO: change reported mountpoint once we have better selinux support
		res = ext4::Format(mDevPath, 0, "/data");
    }
    if (!useVfat && !useExfat) {
        LOG(ERROR) << "Unsupported filesystem " << fsType;
        return -EINVAL;
    }

    if (WipeBlockDevice(mDevPath) != OK) {
        LOG(WARNING) << getId() << " failed to wipe";
    }

    if (useVfat) {
        res = vfat::Format(mDevPath, 0);
    } else if (useExfat) {
        res = exfat::Format(mDevPath);
    }

    if (res != OK) {
        LOG(ERROR) << getId() << " failed to format";
        res = -errno;
    }

    return res;
}

}  // namespace vold
}  // namespace android
