//
// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//

#ifndef WV_TYPES_H_
#define WV_TYPES_H_

namespace wvdrm {

#define WVDRM_DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&) = delete;      \
  void operator=(const TypeName&) = delete;

#define WVDRM_DISALLOW_COPY_AND_ASSIGN_AND_NEW(TypeName) \
  TypeName() = delete;                     \
  TypeName(const TypeName&) = delete;      \
  void operator=(const TypeName&) = delete;

}  // namespace wvdrm
#endif // WV_TYPES_H_
