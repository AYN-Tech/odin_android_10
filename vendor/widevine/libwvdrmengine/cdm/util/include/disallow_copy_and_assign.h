// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#ifndef WVCDM_UTIL_DISALLOW_COPY_AND_ASSIGN_H_
#define WVCDM_UTIL_DISALLOW_COPY_AND_ASSIGN_H_

namespace wvcdm {

#define CORE_DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);                    \
  void operator=(const TypeName&)


}  // namespace wvcdm

#endif  //  WVCDM_UTIL_DISALLOW_COPY_AND_ASSIGN_H_
