/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "gtest/gtest.h"

#include "RteModelTestConfig.h"

#include "RteChk.h"

#include "RteFsUtils.h"
using namespace std;


TEST(RteChkTest, Summary) {

const string summary = "Collecting pdsc files 5 files found\n\
Parsing XML passed\n\
\n\
Constructing Model passed\n\
\n\
Cleaning XML data\n\
\n\
Validating Model passed\n\
\n\
Summary:\n\
Packs: 5\n\
Generic: 1\n\
DFP: 3\n\
BSP: 1\n\
\n\
Components: 40\n\
From generic packs: 24\n\
From DFP: 16\n\
From BSP: 0\n\
\n\
Devices: 10\n\
Boards: 11\n\
\n\
completed\n";

  list<string> files;
  RteFsUtils::GetPackageDescriptionFiles(files, RteModelTestConfig::CMSIS_PACK_ROOT, 3);
  ASSERT_TRUE(files.size() > 0);

  stringstream ss;
  RteChk rteChk(ss);
  rteChk.SetFlag(RteChk::TIME, '-');
  rteChk.AddFileDir(RteModelTestConfig::CMSIS_PACK_ROOT);
  int res = rteChk.RunCheckRte();
  ASSERT_EQ(res, 0);
  ASSERT_EQ(rteChk.GetPackCount(), 5);
  ASSERT_EQ(rteChk.GetComponentCount(), 40);
  ASSERT_EQ(rteChk.GetDeviceCount(), 10);
  ASSERT_EQ(rteChk.GetBoardCount(), 11);

  string s = RteUtils::EnsureLf(ss.str());
  ASSERT_EQ(s, summary);
}
// end of RteChkTest.cpp
