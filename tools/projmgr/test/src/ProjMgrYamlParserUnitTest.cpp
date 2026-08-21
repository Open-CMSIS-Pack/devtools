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

TEST_F(ProjMgrYamlParserUnitTests, ParseCompilerAlias) {
  const string csolutionFile = testinput_folder + "/TestSolution/test.csolution.yml";
  CsolutionItem csolution;

  EXPECT_TRUE(ParseCsolution(csolutionFile, csolution, true, false));
  ASSERT_EQ(csolution.compilerAlias.size(), 1);
  EXPECT_EQ(csolution.compilerAlias.front(), "CLANG");

  YAML::Node solutionNode;
  solutionNode[YAML_COMPILER_ALIAS].push_back("AC6");
  solutionNode[YAML_COMPILER_ALIAS].push_back("GCC");
  vector<string> compilerAliases;
  ParseVectorOrString(solutionNode, YAML_COMPILER_ALIAS, compilerAliases);
  ASSERT_EQ(compilerAliases.size(), 2);
  EXPECT_EQ(compilerAliases[0], "AC6");
  EXPECT_EQ(compilerAliases[1], "GCC");
}

TEST_F(ProjMgrYamlParserUnitTests, RejectMultipleProjectDescriptors) {
  const vector<vector<string>> descriptorCombinations = {
    { YAML_PROJECT, YAML_WEST },
    { YAML_PROJECT, YAML_CMAKE },
    { YAML_WEST, YAML_CMAKE },
    { YAML_PROJECT, YAML_WEST, YAML_CMAKE },
  };

  for (const auto& descriptors : descriptorCombinations) {
    YAML::Node root;
    YAML::Node project;
    for (const auto& descriptor : descriptors) {
      project[descriptor] = YAML::Node(YAML::NodeType::Map);
    }
    root[YAML_PROJECTS].push_back(project);

    CsolutionItem csolution;
    csolution.path = "multiple-descriptors.csolution.yml";
    EXPECT_FALSE(ParseContexts(root, csolution));
    EXPECT_TRUE(csolution.contexts.empty());
    EXPECT_TRUE(csolution.cprojects.empty());
    EXPECT_TRUE(csolution.westApps.empty());
    EXPECT_TRUE(csolution.cmakeApps.empty());
  }
}

TEST_F(ProjMgrYamlParserUnitTests, ValidateCbuildSet) {
  string cbuildSetFile = testinput_folder + "/TestSolution/invalid_keys_test.cbuild-set.yml";
  YAML::Node root = YAML::LoadFile(cbuildSetFile);
  EXPECT_FALSE(ValidateCbuildSet(cbuildSetFile, root));

  YAML::Node invalidRoot;
  invalidRoot["processor"] = "invalid";
  EXPECT_FALSE(ValidateCbuildSet(cbuildSetFile, invalidRoot));
}
