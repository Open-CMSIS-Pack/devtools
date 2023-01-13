/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgr.h"
#include "ProjMgrTestEnv.h"
#include "gtest/gtest.h"
#include <regex>
#include <filesystem>

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
  EXPECT_TRUE(regex_match(timestamp,
  std::regex("^([0-9]){4}(-([0-9]){2}){2}T([0-9]){2}(:([0-9]){2}){2}$")));
}

TEST_F(ProjMgrGeneratorUnitTests, GenDir) {
  char* argv[6];

  const string& csolution = testinput_folder + "/TestSolution/gendir.csolution.yml";
  argv[1] = (char*)"run";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-g";
  argv[5] = (char*)"RteTestGeneratorIdentifier";

  EXPECT_EQ(0, ProjMgr::RunProjMgr(6, argv));

  const string generatorInputFile = testinput_folder + "/TestSolution/TestProject3/TestProject3.Debug+TypeA.cbuild.yml";
  const string generatedGPDSC = testinput_folder + "/TestSolution/TestProject3/gendir/RteTestGen_ARMCM0/RteTest.gpdsc";

  EXPECT_EQ(true, std::filesystem::exists(generatorInputFile));
  EXPECT_EQ(true, std::filesystem::exists(generatedGPDSC));
}
