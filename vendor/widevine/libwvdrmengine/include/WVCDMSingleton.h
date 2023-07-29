//
// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//

#ifndef WV_CDM_SINGLETON_H_
#define WV_CDM_SINGLETON_H_

#include "utils/StrongPointer.h"

#include "wv_content_decryption_module.h"

namespace wvdrm {

android::sp<wvcdm::WvContentDecryptionModule> getCDM();

} // namespace wvdrm

#endif // WV_CDM_SINGLETON_H_
