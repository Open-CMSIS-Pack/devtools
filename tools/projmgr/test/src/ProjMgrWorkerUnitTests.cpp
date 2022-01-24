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
  EXPECT_EQ(true, parser.ParseCproject(filename, true));
  EXPECT_EQ(true, AddContexts(parser, descriptor, filename));
  ContextItem context = GetContexts().begin()->second;
  EXPECT_EQ(true, ProcessPrecedences(context));
  EXPECT_EQ(true, ProcessToolchain(context));
  EXPECT_EQ(expected.name, context.toolchain.name);
  EXPECT_EQ(expected.version, context.toolchain.version);
}

TEST_F(ProjMgrWorkerUnitTests, ProcessDevice) {
  map<string, string> expected = {
    {"Dclock", "10000000"},
    {"Dcore", "Cortex-M0"},
    {"DcoreVersion", "r0p0"},
    {"Dendian", "Configurable"},
    {"Dfpu", "NO_FPU"},
    {"Dmpu", "NO_MPU"},
    {"Dname", "RteTest_ARMCM0"},
    {"Dvendor", "ARM:82"},
  };
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder + "/TestProject/test.cproject.yml";
  EXPECT_EQ(true, parser.ParseCproject(filename, true));
  EXPECT_EQ(true, AddContexts(parser, descriptor, filename));
  ContextItem context = GetContexts().begin()->second;
  EXPECT_EQ(true, LoadPacks());
  EXPECT_EQ(true, ProcessPrecedences(context));
  EXPECT_EQ(true, ProcessDevice(context));
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
  EXPECT_EQ(true, parser.ParseCproject(filename, true));
  EXPECT_EQ(true, AddContexts(parser, descriptor, filename));
  ContextItem context = GetContexts().begin()->second;
  EXPECT_EQ(true, LoadPacks());
  EXPECT_EQ(true, ProcessPrecedences(context));
  EXPECT_EQ(true, ProcessDevice(context));
  EXPECT_EQ(true, SetTargetAttributes(context, context.targetAttributes));
  EXPECT_EQ(true, ProcessComponents(context));
  ASSERT_EQ(expected.size(), context.components.size());
  auto it = context.components.begin();
  for (const auto& expectedComponent : expected) {
    EXPECT_EQ(expectedComponent, (*it++).first);
  }
}

TEST_F(ProjMgrWorkerUnitTests, ProcessDependencies) {
  map<string, set<string>> expected = {{ "ARM::Device:Startup&RteTest Startup@2.0.3" , {"require RteTest:CORE"} }};
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder + "/TestProject/test-dependency.cproject.yml";
  EXPECT_EQ(true, parser.ParseCproject(filename, true));
  EXPECT_EQ(true, AddContexts(parser, descriptor, filename));
  ContextItem context = GetContexts().begin()->second;
  EXPECT_EQ(true, LoadPacks());
  EXPECT_EQ(true, ProcessPrecedences(context));
  EXPECT_EQ(true, ProcessDevice(context));
  EXPECT_EQ(true, SetTargetAttributes(context, context.targetAttributes));
  EXPECT_EQ(true, ProcessComponents(context));
  EXPECT_EQ(true, ProcessDependencies(context));
  ASSERT_EQ(expected.size(), context.dependencies.size());
  for (const auto& [expectedComponent, expectedDependencies] : expected) {
    EXPECT_TRUE(context.dependencies.find(expectedComponent) != context.dependencies.end());
    const auto& dependencies = context.dependencies[expectedComponent];
    for (const auto& expectedDependency : expectedDependencies) {
      EXPECT_TRUE(dependencies.find(expectedDependency) != dependencies.end());
    }
  }
}
