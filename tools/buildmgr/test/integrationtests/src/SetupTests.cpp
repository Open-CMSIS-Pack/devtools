/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CBuildIntegTestEnv.h"

// Check build environment post setup
TEST(SetupTests, SetEnvTest) {
  RunScript("postsetupcheck.sh", testout_folder);
}
