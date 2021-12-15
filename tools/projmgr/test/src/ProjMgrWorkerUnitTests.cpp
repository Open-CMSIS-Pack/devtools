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
  ProjMgrProjectItem project;
  const string& filename = testinput_folder + "/TestProject/test.cproject.yml";
  EXPECT_EQ(true, ProjMgrParser().ParseCproject(filename, project.cproject));
  EXPECT_EQ(true, ProcessToolchain(project));
  EXPECT_EQ(expected.name, project.toolchain.name);
  EXPECT_EQ(expected.version, project.toolchain.version);
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
  ProjMgrProjectItem project;
  const string& filename = testinput_folder + "/TestProject/test.cproject.yml";
  EXPECT_EQ(true, ProjMgrParser().ParseCproject(filename, project.cproject));
  EXPECT_EQ(true, LoadPacks());
  EXPECT_EQ(true, ProcessDevice(project));
  for (const auto& expectedAttribute : expected) {
    EXPECT_EQ(expectedAttribute.second, project.targetAttributes[expectedAttribute.first]);
  }
}

TEST_F(ProjMgrWorkerUnitTests, ProcessComponents) {
  set<string> expected = {
    "ARM::Device.Startup(ARMCM0 RteTest):RteTest Startup:2.0.3[ARM.RteTest_DFP]",
    "ARM::RteTest.CORE(Cortex-M Device):0.1.1[ARM.RteTest_DFP]",
  };
  ProjMgrProjectItem project;
  const string& filename = testinput_folder + "/TestProject/test.cproject.yml";
  EXPECT_EQ(true, ProjMgrParser().ParseCproject(filename, project.cproject));
  EXPECT_EQ(true, LoadPacks());
  EXPECT_EQ(true, ProcessDevice(project));
  EXPECT_EQ(true, SetTargetAttributes(project, project.targetAttributes));
  EXPECT_EQ(true, ProcessComponents(project));
  ASSERT_EQ(expected.size(), project.components.size());
  auto it = project.components.begin();
  for (const auto& expectedComponent : expected) {
    EXPECT_EQ(expectedComponent, (*it++).first);
  }
}

TEST_F(ProjMgrWorkerUnitTests, ProcessDependencies) {
  set<string> expected = {
     "ARM::RteTest.CORE",
  };
  ProjMgrProjectItem project;
  const string& filename = testinput_folder + "/TestProject/test-dependency.cproject.yml";
  EXPECT_EQ(true, ProjMgrParser().ParseCproject(filename, project.cproject));
  EXPECT_EQ(true, LoadPacks());
  EXPECT_EQ(true, ProcessDevice(project));
  EXPECT_EQ(true, SetTargetAttributes(project, project.targetAttributes));
  EXPECT_EQ(true, ProcessComponents(project));
  EXPECT_EQ(true, ProcessDependencies(project));
  ASSERT_EQ(expected.size(), project.dependencies.size());
  auto it = project.dependencies.begin();
  for (const auto& expectedDependency : expected) {
    EXPECT_EQ(expectedDependency, (*it++).first);
  }
}
