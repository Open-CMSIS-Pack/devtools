/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgr.h"
#include "ProjMgrTestEnv.h"
#include "gtest/gtest.h"

using namespace std;

class ProjMgrWorkerUnitTests : public ProjMgrWorker, public ::testing::Test {
protected:
  ProjMgrWorkerUnitTests() {}
  virtual ~ProjMgrWorkerUnitTests() {}
};

TEST_F(ProjMgrWorkerUnitTests, ProcessToolchain) {
  ToolchainItem expected;
  expected.name = "AC6";
  expected.version = "6.16.0";
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder + "/TestProject/test.cproject.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem> contexts;
  GetContexts(contexts);
  ContextItem context = contexts.begin()->second;
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessToolchain(context));
  EXPECT_EQ(expected.name, context.toolchain.name);
  EXPECT_EQ(expected.version, context.toolchain.version);
}

TEST_F(ProjMgrWorkerUnitTests, ProcessToolchainOptions) {
  struct expectedOutput {
    bool result;
    string options, compiler, version;
  };

  std::map<std::string, expectedOutput> testInput =
  {
    { "", {false, "", "", ""}},
    { "TEST", {true, "", "TEST", "0.0.0"}},
    { "AC6", {true, "AC6", "ARMCC", "6.16.0"}}
  };

  for (auto input : testInput) {
    ContextItem context;
    context.compiler = input.first;

    EXPECT_EQ(input.second.result, ProcessToolchain(context));
    EXPECT_EQ(input.second.options, context.targetAttributes["Toptions"]);
    EXPECT_EQ(input.second.compiler, context.targetAttributes["Tcompiler"]);
    EXPECT_EQ(input.second.version, context.toolchain.version);
  }
}

TEST_F(ProjMgrWorkerUnitTests, ProcessDevice) {
  map<string, string> expected = {
    {"Dclock", "10000000"},
    {"Dcore", "Cortex-M0"},
    {"DcoreVersion", "r0p0"},
    {"Dendian", "Little-endian"},
    {"Dfpu", "NO_FPU"},
    {"Dmpu", "NO_MPU"},
    {"Dname", "RteTest_ARMCM0"},
    {"Dvendor", "ARM:82"},
    {"Dsecure", "Non-secure"}
  };
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder + "/TestProject/test.cproject.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem> contexts;
  GetContexts(contexts);
  ContextItem context = contexts.begin()->second;
  EXPECT_TRUE(LoadPacks());
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessDevice(context));
  for (const auto& expectedAttribute : expected) {
    EXPECT_EQ(expectedAttribute.second, context.targetAttributes[expectedAttribute.first]);
  }
}

TEST_F(ProjMgrWorkerUnitTests, ProcessComponents) {
  set<string> expected = {
    "ARM::Device:Startup&RteTest Startup@2.0.3",
    "ARM::RteTest:CORE@0.1.1",
  };
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder + "/TestProject/test.cproject.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem> contexts;
  GetContexts(contexts);
  ContextItem context = contexts.begin()->second;
  EXPECT_TRUE(LoadPacks());
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessDevice(context));
  EXPECT_TRUE(SetTargetAttributes(context, context.targetAttributes));
  EXPECT_TRUE(ProcessComponents(context));
  ASSERT_EQ(expected.size(), context.components.size());
  auto it = context.components.begin();
  for (const auto& expectedComponent : expected) {
    EXPECT_EQ(expectedComponent, (*it++).first);
  }
}

TEST_F(ProjMgrWorkerUnitTests, ProcessComponentsApi) {
  set<string> expectedComponents = {
    "ARM::Device:Startup&RteTest Startup@2.0.3",
    "ARM::RteTest:ApiExclusive:S1@0.9.9",
    "ARM::RteTest:CORE@0.1.1",
  };
  set<string> expectedPackages = {
    "ARM::RteTest@0.1.0",
    "ARM::RteTest_DFP@0.1.1",
  };
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder + "/TestProject/test-api.cproject.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem> contexts;
  GetContexts(contexts);
  ContextItem context = contexts.begin()->second;
  EXPECT_TRUE(LoadPacks());
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessDevice(context));
  EXPECT_TRUE(SetTargetAttributes(context, context.targetAttributes));
  EXPECT_TRUE(ProcessComponents(context));
  ASSERT_EQ(expectedComponents.size(), context.components.size());
  auto componentIt = context.components.begin();
  for (const auto& expectedComponent : expectedComponents) {
    EXPECT_EQ(expectedComponent, (*componentIt++).first);
  }
  ASSERT_EQ(expectedPackages.size(), context.packages.size());
  auto packageIt = context.packages.begin();
  for (const auto& expectedPackage : expectedPackages) {
    EXPECT_EQ(expectedPackage, (*packageIt++).first);
  }
}

TEST_F(ProjMgrWorkerUnitTests, ProcessDependencies) {
  map<string, set<string>> expected = {{ "ARM::Device:Startup&RteTest Startup@2.0.3" , {"require RteTest:CORE"} }};
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder + "/TestProject/test-dependency.cproject.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem> contexts;
  GetContexts(contexts);
  ContextItem context = contexts.begin()->second;
  EXPECT_TRUE(LoadPacks());
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessDevice(context));
  EXPECT_TRUE(SetTargetAttributes(context, context.targetAttributes));
  EXPECT_TRUE(ProcessComponents(context));
  EXPECT_TRUE(ProcessDependencies(context));
  ASSERT_EQ(expected.size(), context.dependencies.size());
  for (const auto& [expectedComponent, expectedDependencies] : expected) {
    EXPECT_TRUE(context.dependencies.find(expectedComponent) != context.dependencies.end());
    const auto& dependencies = context.dependencies[expectedComponent];
    for (const auto& expectedDependency : expectedDependencies) {
      EXPECT_TRUE(dependencies.find(expectedDependency) != dependencies.end());
    }
  }
}

TEST_F(ProjMgrWorkerUnitTests, ProcessDeviceFailed) {
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder + "/TestProject/test-unavailable-processor.cproject.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem> contexts;
  GetContexts(contexts);
  ContextItem context = contexts.begin()->second;
  EXPECT_TRUE(LoadPacks());
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_FALSE(ProcessDevice(context));
}

TEST_F(ProjMgrWorkerUnitTests, LoadUnknownPacks) {
  ProjMgrParser parser;
  ContextDesc descriptor;

  string filename = testinput_folder + "/TestProject/test.cproject_unknown_package.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  EXPECT_FALSE(LoadPacks());

  // No pack is loaded
  EXPECT_EQ(0, m_installedPacks.size());
}

TEST_F(ProjMgrWorkerUnitTests, LoadDuplicatePacks) {
  ProjMgrParser parser;
  ContextDesc descriptor;

  string filename = testinput_folder + "/TestProject/test.cproject_duplicate_package.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  EXPECT_TRUE(LoadPacks());

  // Check if only one pack is loaded
  ASSERT_EQ(1, m_installedPacks.size());
  EXPECT_EQ("ARM.RteTest_DFP", (*m_installedPacks.begin())->GetCommonID());
}

TEST_F(ProjMgrWorkerUnitTests, LoadRequiredPacks) {
  ProjMgrParser parser;
  ContextDesc descriptor;

  string filename = testinput_folder + "/TestProject/test.cproject.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  EXPECT_TRUE(LoadPacks());

  // Check if only one pack is loaded
  ASSERT_EQ(1, m_installedPacks.size());
  EXPECT_EQ("ARM.RteTest_DFP", (*m_installedPacks.begin())->GetCommonID());
}

TEST_F(ProjMgrWorkerUnitTests, LoadPacksNoPackage) {
  ProjMgrParser parser;
  ContextDesc descriptor;

  string filename = testinput_folder + "/TestProject/test.cproject_no_packages.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  EXPECT_TRUE(LoadPacks());

  // get list of available packs
  vector<string> availablePacks;
  EXPECT_TRUE(ListPacks("", availablePacks));

  // by default all packs available should be loaded
  EXPECT_EQ(availablePacks.size(), m_installedPacks.size());
}

TEST_F(ProjMgrWorkerUnitTests, GetAccessSequence) {
  string src, sequence;
  size_t offset = 0;

  src = "Option=$Dname$ - $Dboard$";
  EXPECT_TRUE(GetAccessSequence(offset, src, sequence, '$', '$'));
  EXPECT_EQ(offset, 14);
  EXPECT_EQ(sequence, "Dname");
  EXPECT_TRUE(GetAccessSequence(offset, src, sequence, '$', '$'));
  EXPECT_EQ(offset, 25);
  EXPECT_EQ(sequence, "Dboard");
  EXPECT_TRUE(GetAccessSequence(offset, src, sequence, '$', '$'));
  EXPECT_EQ(offset, string::npos);

  src = "DEF=$Output(project)$";
  offset = 0;
  EXPECT_TRUE(GetAccessSequence(offset, src, sequence, '$', '$'));
  EXPECT_EQ(offset, 21);
  EXPECT_EQ(sequence, "Output(project)");
  offset = 0;
  EXPECT_TRUE(GetAccessSequence(offset, sequence, sequence, '(', ')'));
  EXPECT_EQ(offset, 15);
  EXPECT_EQ(sequence, "project");

  src = "Option=$Dname";
  offset = 0;
  EXPECT_FALSE(GetAccessSequence(offset, src, sequence, '$', '$'));
}
