// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

// Use in place of the gtest_main in order to initialize the WvCdmTestBase using
// command line parameters.

#include <stdio.h>

#include "log.h"
#include "test_base.h"

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (!wvcdm::WvCdmTestBase::Initialize(argc, argv)) return 0;
  return RUN_ALL_TESTS();
}
