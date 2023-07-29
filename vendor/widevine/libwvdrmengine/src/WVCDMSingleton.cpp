//
// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//

//#define LOG_NDEBUG 0
#define LOG_TAG "WVCdm"
#include <utils/Log.h>

#include "WVCDMSingleton.h"

#include "utils/Mutex.h"
#include "utils/RefBase.h"

namespace wvdrm {

using wvcdm::WvContentDecryptionModule;
using android::Mutex;
using android::sp;
using android::wp;

Mutex cdmLock;
// The strong pointers that keep this object alive live in the plugin objects.
// If all the plugins are deleted, the CDM will be deleted, and subsequent
// invocations of this code will construct a new CDM.
wp<WvContentDecryptionModule> sCdm;

sp<WvContentDecryptionModule> getCDM() {
  Mutex::Autolock lock(cdmLock);  // This function is a critical section.

  sp<WvContentDecryptionModule> cdm = sCdm.promote();

  if (cdm == NULL) {
    ALOGD("Instantiating CDM.");
    cdm = new WvContentDecryptionModule();
    sCdm = cdm;
  }

  return cdm;
}

} // namespace wvdrm
