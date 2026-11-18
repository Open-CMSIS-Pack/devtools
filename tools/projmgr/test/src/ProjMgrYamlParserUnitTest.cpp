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

TEST_F(ProjMgrYamlParserUnitTests, ParseWestDefines) {
  const auto node = YAML::Load(R"(
- FLAG
- FIRST: one
  SECOND: two
  for-compiler: GCC
)");
  vector<WestDefineItem> entries;
  ASSERT_TRUE(ParseWestDefines(node, entries));
  ASSERT_EQ(2, entries.size());
  EXPECT_EQ(vector<string>({ "FLAG" }), entries[0].defines);
  EXPECT_TRUE(entries[0].forCompiler.empty());
  EXPECT_EQ(vector<string>({ "FIRST=one", "SECOND=two" }), entries[1].defines);
  EXPECT_EQ(vector<string>({ "GCC" }), entries[1].forCompiler);
  EXPECT_TRUE(node[1]["for-compiler"]);
  EXPECT_FALSE(ParseWestDefines(YAML::Load("- INVALID: 'bad\"quote'"), entries));
}

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

TEST_F(ProjMgrYamlParserUnitTests, ParseStdioAliases) {
  for (const auto& key : { "stdio", "telnet" }) {
    const auto node = YAML::Load(string("debugger:\n  ") + key + R"(:
    - mode: file
      port: 4444
      pname: Core0
      file: legacy.txt
      file-in: explicit.in
      file-out: explicit.out
    - mode: file
      file-out: explicit.out
    - mode: file
      file: capture.txt
)");
    DebuggerItem debugger;
    ParseDebugger(node, "test.csolution.yml", debugger);
    ASSERT_EQ(3, debugger.stdio.size());
    EXPECT_EQ("file", debugger.stdio[0].mode);
    EXPECT_EQ("4444", debugger.stdio[0].port);
    EXPECT_EQ("Core0", debugger.stdio[0].pname);
    EXPECT_EQ("legacy.txt", debugger.stdio[0].file);
    EXPECT_EQ("explicit.in", debugger.stdio[0].fileIn);
    EXPECT_EQ("explicit.out", debugger.stdio[0].fileOut);
    EXPECT_TRUE(debugger.stdio[1].fileIn.empty());
    EXPECT_EQ("explicit.out", debugger.stdio[1].fileOut);
    EXPECT_EQ("capture.txt", debugger.stdio[2].file);
    EXPECT_TRUE(debugger.stdio[2].fileIn.empty());
    EXPECT_TRUE(debugger.stdio[2].fileOut.empty());
    EXPECT_TRUE(debugger.custom.map.empty());

    DebugAdapterDefaultsItem defaults;
    ParseDebugDefaults(YAML::Load(string("defaults:\n  ") + key + R"(:
    active:
    port: 4444
    mode: file
)"), "debug-adapters.yml", defaults);
    EXPECT_TRUE(defaults.stdio.active);
    EXPECT_EQ("4444", defaults.stdio.port);
    EXPECT_EQ("file", defaults.stdio.mode);
    EXPECT_TRUE(defaults.custom.map.empty());
  }
}

TEST_F(ProjMgrYamlParserUnitTests, ParseStdioPrecedence) {
  const auto node = YAML::Load(R"(
debugger:
  stdio:
    - mode: console
  telnet:
    - mode: file
)");
  DebuggerItem debugger;
  ParseDebugger(node, "test.csolution.yml", debugger);
  ASSERT_EQ(1, debugger.stdio.size());
  EXPECT_EQ("console", debugger.stdio[0].mode);
  EXPECT_TRUE(debugger.custom.map.empty());

  DebugAdapterDefaultsItem defaults;
  ParseDebugDefaults(YAML::Load(R"(
defaults:
  stdio:
    mode: console
    port: 5555
  telnet:
    active:
    mode: file
    port: 4444
)"), "debug-adapters.yml", defaults);
  EXPECT_EQ("console", defaults.stdio.mode);
  EXPECT_EQ("5555", defaults.stdio.port);
  EXPECT_FALSE(defaults.stdio.active);
  EXPECT_TRUE(defaults.custom.map.empty());
}
