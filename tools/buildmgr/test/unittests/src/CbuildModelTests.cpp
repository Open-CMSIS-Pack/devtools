/*
 * Copyright (c) 2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CBuildUnitTestEnv.h"

#include "CbuildModel.h"

using namespace std;

class CbuildModelTests : public CbuildModel, public ::testing::Test {
};

TEST_F(CbuildModelTests, GetAccessSequence) {
  string src, sequence;
  size_t offset = 0;

  src = "$Bpack$";
  EXPECT_TRUE(GetAccessSequence(offset, src, sequence, '$', '$'));
  EXPECT_EQ(offset, 7);
  EXPECT_EQ(sequence, "Bpack");
  EXPECT_TRUE(GetAccessSequence(offset, src, sequence, '$', '$'));
  EXPECT_EQ(offset, string::npos);

  src = "$Pack(vendor.name)$";
  offset = 0;
  EXPECT_TRUE(GetAccessSequence(offset, src, sequence, '$', '$'));
  EXPECT_EQ(offset, 19);
  EXPECT_EQ(sequence, "Pack(vendor.name)");
  offset = 0;
  EXPECT_TRUE(GetAccessSequence(offset, sequence, sequence, '(', ')'));
  EXPECT_EQ(offset, 17);
  EXPECT_EQ(sequence, "vendor.name");

  src = "$Dpack";
  offset = 0;
  EXPECT_FALSE(GetAccessSequence(offset, src, sequence, '$', '$'));
}
