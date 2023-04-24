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
Components: 44\n\
From generic packs: 26\n\
From DFP: 18\n\
From BSP: 0\n\
\n\
Devices: 10\n\
Boards: 13\n\
\n\
completed\n";

  list<string> files;
  RteFsUtils::GetPackageDescriptionFiles(files, RteModelTestConfig::CMSIS_PACK_ROOT, 3);
  EXPECT_TRUE(files.size() > 0);

  stringstream ss;
  RteChk rteChk(ss);
  rteChk.SetFlag(RteChk::TIME, '-');
  rteChk.AddFileDir(RteModelTestConfig::CMSIS_PACK_ROOT);
  int res = rteChk.RunCheckRte();
  EXPECT_EQ(res, 0);
  EXPECT_EQ(rteChk.GetPackCount(), 5);
  EXPECT_EQ(rteChk.GetComponentCount(), 44);
  EXPECT_EQ(rteChk.GetDeviceCount(), 10);
  EXPECT_EQ(rteChk.GetBoardCount(), 13);

  string s = RteUtils::EnsureLf(ss.str());
  EXPECT_EQ(s, summary);
}
// end of RteChkTest.cpp
