/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgr.h"
#include "ProjMgrTestEnv.h"
#include "gtest/gtest.h"
#include <fstream>

using namespace std;

class ProjMgrUnitTests : public ProjMgr, public ::testing::Test {
protected:
  ProjMgrUnitTests() {}
  virtual ~ProjMgrUnitTests() {}
  void CompareFile(const string& file1, const string& file2);
};

void ProjMgrUnitTests::CompareFile(const string& file1, const string& file2) {
  ifstream f1, f2;
  string l1, l2;
  bool ret_val;

  f1.open(file1);
  ret_val = f1.is_open();
  ASSERT_EQ(ret_val, true) << "Failed to open " << file1;

  f2.open(file2);
  ret_val = f2.is_open();
  ASSERT_EQ(ret_val, true) << "Failed to open " << file2;

  while (getline(f1, l1) && getline(f2, l2)) {
    if (!l1.empty() && l1.rfind('\r') == l1.length() - 1) {
      l1.pop_back();
    }

    if (!l2.empty() && l2.rfind('\r') == l2.length() - 1) {
      l2.pop_back();
    }

    if (l1 != l2) {
      // ignore 'timestamp'
      if ((!l1.empty() && (l1.find("timestamp=") != string::npos)) && (!l2.empty() && (l2.find("timestamp=") != string::npos))) {
        continue;
      }

      FAIL() << "error: " << file1 << " is different from " << file2;
    }
  }

  f1.close();
  f2.close();
}

TEST_F(ProjMgrUnitTests, RunProjMgr) {
  char *argv[7];

  // Empty options
  EXPECT_EQ(0, RunProjMgr(1, argv));

  // help
  argv[1] = (char*)"help";
  EXPECT_EQ(0, RunProjMgr(2, argv));

  // list packs
  argv[1] = (char*)"list";
  argv[2] = (char*)"packs";
  EXPECT_EQ(0, RunProjMgr(3, argv));

  // list devices
  argv[1] = (char*)"list";
  argv[2] = (char*)"devices";
  EXPECT_EQ(0, RunProjMgr(3, argv));

  // list components
  argv[1] = (char*)"list";
  argv[2] = (char*)"components";
  EXPECT_EQ(0, RunProjMgr(3, argv));

  // convert -p cproject.yml
  const string& cproject = testinput_folder + "/TestProject/test.cproject.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-p";
  argv[3] = (char*)cproject.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv));

  // Check generated CPRJ
  CompareFile(testoutput_folder + "/TestProject.cprj",
    testinput_folder + "/TestProject/TestProject.cprj");
}

TEST_F(ProjMgrUnitTests, ListPacks) {
  set<string> expected = {
    "ARM.RteTest.0.1.0",
    "ARM.RteTest_DFP.0.1.1"
  };
  set<string> packs;
  EXPECT_EQ(true, m_worker.ListPacks("RteTest", packs));
  EXPECT_EQ(expected, packs);
}

TEST_F(ProjMgrUnitTests, ListDevices) {
  set<string> expected = {
    "RteTest_ARMCM4_FP",
    "RteTest_ARMCM4_NOFP",
    "RteTest_ARMCM4_FP",
    "RteTest_ARMCM4_NOFP"
  };
  ProjMgrProjectItem project;
  set<string> devices;
  EXPECT_EQ(true, m_worker.ListDevices(project, "CM4", devices));
  EXPECT_EQ(expected, devices);
}

TEST_F(ProjMgrUnitTests, ListComponents) {
  set<string> expected = {
    "ARM::Device.Startup(ARMCM0 RteTest):RteTest Startup:2.0.3[ARM.RteTest_DFP]",
    "ARM::Device.Startup(ARMCM3 RteTest):RteTest Startup:2.0.3[ARM.RteTest_DFP]",
    "ARM::Device.Startup(ARMCM4 RteTest):RteTest Startup:2.0.3[ARM.RteTest_DFP]"
  };
  ProjMgrProjectItem project;
  set<string> components;
  EXPECT_EQ(true, m_worker.ListComponents(project, "Startup", components));
  EXPECT_EQ(expected, components);
}

TEST_F(ProjMgrUnitTests, ListComponentsDeviceFiltered) {
  set<string> expected = {
    "ARM::Device.Startup(ARMCM0 RteTest):RteTest Startup:2.0.3[ARM.RteTest_DFP]"
  };
  ProjMgrProjectItem project;
  set<string> components;
  const string& filename = testinput_folder + "/TestProject/test.cproject.yml";
  EXPECT_EQ(true, m_parser.ParseCproject(filename, project.cproject));
  EXPECT_EQ(true, m_worker.ListComponents(project, "Startup", components));
  EXPECT_EQ(expected, components);
}

TEST_F(ProjMgrUnitTests, ListDependencies) {
  set<string> expected = {
     "ARM::RteTest.CORE",
  };
  ProjMgrProjectItem project;
  set<string> dependencies;
  const string& filename = testinput_folder + "/TestProject/test-dependency.cproject.yml";
  EXPECT_EQ(true, m_parser.ParseCproject(filename, project.cproject));
  EXPECT_EQ(true, m_worker.ListDependencies(project, "CORE", dependencies));
  EXPECT_EQ(expected, dependencies);
}

TEST_F(ProjMgrUnitTests, GenerateCprj) {
  ProjMgrProjectItem project;
  const string& filenameInput = testinput_folder + "/TestProject/test.cproject.yml";
  const string& filenameOutput = testoutput_folder + "/GenerateCprjTest.cprj";
  EXPECT_EQ(true, m_parser.ParseCproject(filenameInput, project.cproject));
  EXPECT_EQ(true, m_worker.ProcessProject(project, true));
  EXPECT_EQ(true, m_generator.GenerateCprj(project, filenameOutput));

  CompareFile(testoutput_folder + "/GenerateCprjTest.cprj",
    testinput_folder + "/TestProject/TestProject.cprj");
}
