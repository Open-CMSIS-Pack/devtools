/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgr.h"
#include "ProjMgrTestEnv.h"
#include "gtest/gtest.h"
#include <regex>

using namespace std;

class ProjMgrGeneratorUnitTests : public ProjMgrGenerator, public ::testing::Test {
protected:
  ProjMgrGeneratorUnitTests() {}
  virtual ~ProjMgrGeneratorUnitTests() {}
};

TEST_F(ProjMgrGeneratorUnitTests, GetStringFromVector) {
  string expected = "Word1 Word2 Word3";
  const vector<string> vec = {
   "Word1", "Word2", "Word3",
  };
  EXPECT_EQ(expected, ProjMgrGenerator().GetStringFromVector(vec, " "));
  
  const vector<string> emptyVec;
  EXPECT_EQ("", ProjMgrGenerator().GetStringFromVector(emptyVec, " "));
}

TEST_F(ProjMgrGeneratorUnitTests, GetLocalTimestamp) {
  string timestamp = ProjMgrGenerator().GetLocalTimestamp();
  EXPECT_EQ(true, regex_match(timestamp,
  std::regex("^([0-9]){4}(-([0-9]){2}){2}T([0-9]){2}(:([0-9]){2}){2}$")));
}
