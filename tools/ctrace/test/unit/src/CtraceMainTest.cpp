/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gtest/gtest.h"

#include "CtraceMain.h"

TEST(CtraceMainTest, PrintsExecutableName) {
  testing::internal::CaptureStdout();

  EXPECT_EQ(0, CtraceMain());

  EXPECT_EQ("ctrace Executable\n", testing::internal::GetCapturedStdout());
}
