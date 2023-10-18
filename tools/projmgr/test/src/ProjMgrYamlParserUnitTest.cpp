/*
 * Copyright (c) 2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgr.h"
#include "ProjMgrTestEnv.h"
#include "ProjMgrYamlParser.h"

#include "gtest/gtest.h"

using namespace std;

class ProjMgrYamlParserUnitTests : public ProjMgrYamlParser, public ::testing::Test {
protected:
  ProjMgrYamlParserUnitTests() {}
  virtual ~ProjMgrYamlParserUnitTests() {}
};

TEST_F(ProjMgrYamlParserUnitTests, ParseCbuildSet) {
  string cbuildSetFile = testinput_folder + "/TestSolution/invalid_test.cbuild-set.yml";
  CbuildSetItem buildSetItem;
  EXPECT_FALSE(ParseCbuildSet(cbuildSetFile, buildSetItem, true));

  cbuildSetFile = testinput_folder + "/TestSolution/ref/cbuild/specific_contexts_test.cbuild-set.yml";
  EXPECT_TRUE(ParseCbuildSet(cbuildSetFile, buildSetItem, true));
  EXPECT_EQ(buildSetItem.contexts.size(), 2);
  EXPECT_EQ(buildSetItem.contexts[0], "test2.Debug+CM0");
  EXPECT_EQ(buildSetItem.contexts[1], "test1.Debug+CM0");
  EXPECT_EQ(buildSetItem.compiler, "GCC");

  EXPECT_FALSE(ParseCbuildSet("unkownfile.cbuild-set.yml", buildSetItem, true));
}

TEST_F(ProjMgrYamlParserUnitTests, ValidateCbuildSet) {
  string cbuildSetFile = testinput_folder + "/TestSolution/invalid_keys_test.cbuild-set.yml";
  YAML::Node root = YAML::LoadFile(cbuildSetFile);
  EXPECT_FALSE(ValidateCbuildSet(cbuildSetFile, root));

  YAML::Node invalidRoot;
  invalidRoot["processor"] = "invalid";
  EXPECT_FALSE(ValidateCbuildSet(cbuildSetFile, invalidRoot));
}
