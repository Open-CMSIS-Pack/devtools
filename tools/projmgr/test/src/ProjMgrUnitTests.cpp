/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgr.h"
#include "ProjMgrTestEnv.h"
#include "RteFsUtils.h"
#include "gtest/gtest.h"
#include <fstream>

using namespace std;

class ProjMgrUnitTests : public ProjMgr, public ::testing::Test {
protected:
  ProjMgrUnitTests() {}
  virtual ~ProjMgrUnitTests() {}

  void CompareFile(const string& file1, const string& file2);
  void CompareFileTree(const string& dir1, const string& dir2);
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

void ProjMgrUnitTests::CompareFileTree(const string& dir1, const string& dir2) {
  set<string> tree1, tree2;
  error_code ec;
  if (RteFsUtils::Exists(dir1)) {
    for (auto& p : fs::recursive_directory_iterator(dir1, ec)) {
      tree1.insert(p.path().filename().generic_string());
    }
  }
  if (RteFsUtils::Exists(dir2)) {
    for (auto& p : fs::recursive_directory_iterator(dir2, ec)) {
      tree2.insert(p.path().filename().generic_string());
    }
  }
  EXPECT_EQ(tree1, tree2);
}

TEST_F(ProjMgrUnitTests, RunProjMgr_EmptyOptions) {
  char* argv[1];
  // Empty options
  EXPECT_EQ(0, RunProjMgr(1, argv));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Help) {
  char* argv[2];

  // help
  argv[1] = (char*)"help";
  EXPECT_EQ(0, RunProjMgr(2, argv));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListPacks) {
  char* argv[5];
  StdStreamRedirect streamRedirect;

  // list packs
  argv[1] = (char*)"list";
  argv[2] = (char*)"packs";
  argv[3] = (char*)"--filter";
  argv[4] = (char*)"ARM::RteTest_DFP";
  EXPECT_EQ(0, RunProjMgr(5, argv));

  auto outStr = streamRedirect.GetOutString();
  EXPECT_STREQ(outStr.c_str(), "ARM::RteTest_DFP@0.1.1\nARM::RteTest_DFP@0.2.0\n");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListDevices) {
  char* argv[5];
  StdStreamRedirect streamRedirect;

  // list devices
  argv[1] = (char*)"list";
  argv[2] = (char*)"devices";
  argv[3] = (char*)"--filter";
  argv[4] = (char*)"RteTest_ARMCM4";
  EXPECT_EQ(0, RunProjMgr(5, argv));
  
  auto outStr = streamRedirect.GetOutString();
  EXPECT_STREQ(outStr.c_str(), "RteTest_ARMCM4_FP\nRteTest_ARMCM4_NOFP\n");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListComponents) {
  char* argv[3];

  // list components
  argv[1] = (char*)"list";
  argv[2] = (char*)"components";
  EXPECT_EQ(0, RunProjMgr(3, argv));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListDependencies) {
  char* argv[5];
  const std::string expected = "ARM::Device:Startup&RteTest Startup@2.0.3 require RteTest:CORE\n";
  StdStreamRedirect streamRedirect;

  // list dependencies
  const string& cproject = testinput_folder + "/TestProject/test-dependency.cproject.yml";
  argv[1] = (char*)"list";
  argv[2] = (char*)"dependencies";
  argv[3] = (char*)"-p";
  argv[4] = (char*)cproject.c_str();
  EXPECT_EQ(0, RunProjMgr(5, argv));

  auto outStr = streamRedirect.GetOutString();
  EXPECT_STREQ(outStr.c_str(), expected.c_str());
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ConvertProject) {
  char* argv[6];
  // convert -p cproject.yml
  const string& cproject = testinput_folder + "/TestProject/test.cproject.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-p";
  argv[3] = (char*)cproject.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv));

  // Check generated CPRJ
  CompareFile(testoutput_folder + "/test/test.cprj",
    testinput_folder + "/TestProject/test.cprj");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_With_Schema_Check) {
  char* argv[6];
  // convert -p cproject.yml
  const string& cproject = testinput_folder + "/TestProject/test.cproject_invalid_schema.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-p";
  argv[3] = (char*)cproject.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Skip_Schema_Check) {
  char* argv[7];
  // convert -p cproject.yml
  const string& cproject = testinput_folder + "/TestProject/test.cproject_invalid_schema.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-p";
  argv[3] = (char*)cproject.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"-n";
  EXPECT_EQ(0, RunProjMgr(7, argv));

  // Check generated CPRJ
  CompareFile(testoutput_folder + "/test/test.cprj",
    testinput_folder + "/TestProject/test.cprj");
}

TEST_F(ProjMgrUnitTests, RunProjMgrContextSolution) {
  char* argv[7];
  StdStreamRedirect streamRedirect;

  // convert -s solution.yml
  const string& csolution = testinput_folder + "/TestSolution/test.csolution.yml";
  argv[1] = (char*)"list";
  argv[2] = (char*)"contexts";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  argv[5] = (char*)"--filter";
  argv[6] = (char*)"test1";
  EXPECT_EQ(0, RunProjMgr(7, argv));

  auto outStr = streamRedirect.GetOutString();
  EXPECT_STREQ(outStr.c_str(), "test1.Debug+CM0\ntest1.Release+CM0\n");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_MissingSolutionFile) {
  char* argv[5];
  const string& csolution = testinput_folder + "/TestSolution/unknown.csolution.yml";
  argv[1] = (char*)"list";
  argv[2] = (char*)"contexts";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();

  // Missing Solution File
  EXPECT_EQ(1, RunProjMgr(5, argv));

  // list empty arguments
  EXPECT_EQ(1, RunProjMgr(2, argv));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_MissingProjectFile) {
  char* argv[6];
  // convert -p cproject.yml
  const string& cproject = testinput_folder + "/TestProject/unknown.cproject.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-p";
  argv[3] = (char*)cproject.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_InvalidArgs) {
  char* argv[7];
  const string& csolution = testinput_folder + "/TestSolution/test.csolution.yml";
  argv[1] = (char*)"list";
  argv[2] = (char*)"pack";
  argv[3] = (char*)"devices";
  argv[4] = (char*)"contexts";
  argv[5] = (char*)"--solution";
  argv[6] = (char*)csolution.c_str();

  EXPECT_EQ(1, RunProjMgr(7, argv));
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution) {
  char* argv[7];

  // convert -s solution.yml
  const string& csolution = testinput_folder + "/TestSolution/test.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-s";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv));

  // Check generated CPRJs
  CompareFile(testoutput_folder + "/test1.Debug+CM0/test1.Debug+CM0.cprj",
    testinput_folder + "/TestSolution/TestProject1/test1.Debug+CM0.cprj");
  CompareFile(testoutput_folder + "/test1.Release+CM0/test1.Release+CM0.cprj",
    testinput_folder + "/TestSolution/TestProject1/test1.Release+CM0.cprj");

  CompareFile(testoutput_folder + "/test2.Debug+CM0/test2.Debug+CM0.cprj",
    testinput_folder + "/TestSolution/TestProject2/test2.Debug+CM0.cprj");
  CompareFile(testoutput_folder + "/test2.Debug+CM3/test2.Debug+CM3.cprj",
    testinput_folder + "/TestSolution/TestProject2/test2.Debug+CM3.cprj");
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolutionContext) {
  char* argv[8];
  // convert -s solution.yml
  const string& csolution = testinput_folder + "/TestSolution/test.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-s";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"-c";
  argv[7] = (char*)"test2.Debug+CM0";
  EXPECT_EQ(0, RunProjMgr(8, argv));
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolutionNonExistentContext) {
  char* argv[8];
  // convert -s solution.yml
  const string& csolution = testinput_folder + "/TestSolution/test.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-s";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"-c";
  argv[7] = (char*)"NON-EXISTENT-CONTEXT";
  EXPECT_EQ(1, RunProjMgr(8, argv));
}

TEST_F(ProjMgrUnitTests, RunProjMgrLayers) {
  char* argv[7];

  // convert -s solution.yml
  const string& csolution = testinput_folder + "/TestLayers/testlayers.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-s";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv));

  // Check generated CPRJs
  CompareFile(testoutput_folder + "/testlayers.Debug/testlayers.Debug.cprj",
    testinput_folder + "/TestLayers/ref/testlayers.Debug/testlayers.Debug.cprj");
  CompareFile(testoutput_folder + "/testlayers.Release/testlayers.Release.cprj",
    testinput_folder + "/TestLayers/ref/testlayers.Release/testlayers.Release.cprj");

  // Check generated filetrees
  CompareFileTree(testoutput_folder + "/testlayers.Release",
    testinput_folder + "/TestLayers/ref/testlayers.Release");
  CompareFileTree(testoutput_folder + "/testlayers.Debug",
    testinput_folder + "/TestLayers/ref/testlayers.Debug");
}

TEST_F(ProjMgrUnitTests, AccessSequences) {
  char* argv[7];

  // convert -s solution.yml
  const string& csolution = testinput_folder + "/TestAccessSequences/test-access-sequences.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-s";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv));

  // Check generated CPRJs
  CompareFile(testoutput_folder + "/test-access-sequences1.Debug+CM0/test-access-sequences1.Debug+CM0.cprj",
    testinput_folder + "/TestAccessSequences/ref/test-access-sequences1.Debug+CM0/test-access-sequences1.Debug+CM0.cprj");
  CompareFile(testoutput_folder + "/test-access-sequences1.Release+CM0/test-access-sequences1.Release+CM0.cprj",
    testinput_folder + "/TestAccessSequences/ref/test-access-sequences1.Release+CM0/test-access-sequences1.Release+CM0.cprj");
  CompareFile(testoutput_folder + "/test-access-sequences2.Debug+CM0/test-access-sequences2.Debug+CM0.cprj",
    testinput_folder + "/TestAccessSequences/ref/test-access-sequences2.Debug+CM0/test-access-sequences2.Debug+CM0.cprj");
  CompareFile(testoutput_folder + "/test-access-sequences2.Release+CM0/test-access-sequences2.Release+CM0.cprj",
    testinput_folder + "/TestAccessSequences/ref/test-access-sequences2.Release+CM0/test-access-sequences2.Release+CM0.cprj");
  CompareFile(testoutput_folder + "/test-access-sequences1.Debug+CM3/test-access-sequences1.Debug+CM3.cprj",
    testinput_folder + "/TestAccessSequences/ref/test-access-sequences1.Debug+CM3/test-access-sequences1.Debug+CM3.cprj");
  CompareFile(testoutput_folder + "/test-access-sequences1.Release+CM3/test-access-sequences1.Release+CM3.cprj",
    testinput_folder + "/TestAccessSequences/ref/test-access-sequences1.Release+CM3/test-access-sequences1.Release+CM3.cprj");
  CompareFile(testoutput_folder + "/test-access-sequences2.Debug+CM3/test-access-sequences2.Debug+CM3.cprj",
    testinput_folder + "/TestAccessSequences/ref/test-access-sequences2.Debug+CM3/test-access-sequences2.Debug+CM3.cprj");
  CompareFile(testoutput_folder + "/test-access-sequences2.Release+CM3/test-access-sequences2.Release+CM3.cprj",
    testinput_folder + "/TestAccessSequences/ref/test-access-sequences2.Release+CM3/test-access-sequences2.Release+CM3.cprj");
}

TEST_F(ProjMgrUnitTests, AccessSequences2) {
  char* argv[7];

  // convert -s solution.yml
  const string& csolution = testinput_folder + "/TestAccessSequences/test-access-sequences2.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-s";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv));

  // Check generated CPRJs
  CompareFile(testoutput_folder + "/test-access-sequences3.Debug/test-access-sequences3.Debug.cprj",
    testinput_folder + "/TestAccessSequences/ref/test-access-sequences3.Debug/test-access-sequences3.Debug.cprj");
  CompareFile(testoutput_folder + "/test-access-sequences3.Release/test-access-sequences3.Release.cprj",
    testinput_folder + "/TestAccessSequences/ref/test-access-sequences3.Release/test-access-sequences3.Release.cprj");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_MalformedAccessSequences1) {
  char* argv[6];
  // convert -p cproject.yml
  const string& cproject = testinput_folder + "/TestAccessSequences/malformed-access-sequences1.cproject.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-p";
  argv[3] = (char*)cproject.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_MalformedAccessSequences2) {
  char* argv[6];
  // convert -p cproject.yml
  const string& cproject = testinput_folder + "/TestAccessSequences/malformed-access-sequences2.cproject.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-p";
  argv[3] = (char*)cproject.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Multicore) {
  char* argv[7];
  // convert -s solution.yml
  const string& csolution = testinput_folder + "/TestSolution/multicore.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-s";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv));

  // Check generated CPRJs
  CompareFile(testoutput_folder + "/multicore+CM0/multicore+CM0.cprj",
    testinput_folder + "/TestSolution/ref/multicore+CM0.cprj");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Generator) {
  char* argv[7];
  // convert -s solution.yml
  const string& csolution = testinput_folder + "/TestGenerator/test-gpdsc.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-s";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv));

  // Check generated CPRJs
  CompareFile(testoutput_folder + "/test-gpdsc.Debug+CM0/test-gpdsc.Debug+CM0.cprj",
    testinput_folder + "/TestGenerator/ref/test-gpdsc.Debug+CM0.cprj");
}

TEST_F(ProjMgrUnitTests, ListPacks) {
  set<string> expected = {
    "ARM::RteTest@0.1.0",
    "ARM::RteTestGenerator@0.1.0",
    "ARM::RteTest_DFP@0.1.1",
    "ARM::RteTest_DFP@0.2.0"
  };
  vector<string> packs;
  EXPECT_TRUE(m_worker.ListPacks(packs, "", "RteTest"));
  EXPECT_EQ(expected, set<string>(packs.begin(), packs.end()));
}

TEST_F(ProjMgrUnitTests, ListPacksPackageFiltered) {
  set<string> expected = {
    "ARM::RteTest_DFP@0.1.1"
  };
  vector<string> packs;
  ContextDesc descriptor;
  const string& filenameInput = testinput_folder + "/TestProject/test.cproject_exact_package.yml";
  EXPECT_TRUE(m_parser.ParseCproject(filenameInput, false, true));
  EXPECT_TRUE(m_worker.AddContexts(m_parser, descriptor, filenameInput));
  EXPECT_TRUE(m_worker.ListPacks(packs, "test", "RteTest"));
  EXPECT_EQ(expected, set<string>(packs.begin(), packs.end()));
}

TEST_F(ProjMgrUnitTests, ListDevices) {
  set<string> expected = {
    "RteTestGen_ARMCM0",
    "RteTest_ARMCM0",
    "RteTest_ARMCM0_Dual:cm0_core0",
    "RteTest_ARMCM0_Dual:cm0_core1",
    "RteTest_ARMCM0_Single",
    "RteTest_ARMCM0_Test"
  };
  vector<string> devices;
  EXPECT_TRUE(m_worker.ListDevices(devices, "", "CM0"));
  EXPECT_EQ(expected, set<string>(devices.begin(), devices.end()));
}

TEST_F(ProjMgrUnitTests, ListDevicesPackageFiltered) {
  set<string> expected = {
    "RteTest_ARMCM0"
  };
  vector<string> devices;
  ContextDesc descriptor;
  const string& filenameInput = testinput_folder + "/TestProject/test.cproject_exact_package.yml";
  EXPECT_TRUE(m_parser.ParseCproject(filenameInput, false, true));
  EXPECT_TRUE(m_worker.AddContexts(m_parser, descriptor, filenameInput));
  EXPECT_TRUE(m_worker.ListDevices(devices, "test", "CM0"));
  EXPECT_EQ(expected, set<string>(devices.begin(), devices.end()));
}

TEST_F(ProjMgrUnitTests, ListComponents) {
  set<string> expected = {
    "ARM::Device:Startup&RteTest Startup@2.0.3 (ARM::RteTest_DFP@0.2.0)",
  };
  vector<string> components;
  EXPECT_TRUE(m_worker.ListComponents(components, "", "Startup"));
  EXPECT_EQ(expected, set<string>(components.begin(), components.end()));
}

TEST_F(ProjMgrUnitTests, ListComponentsDeviceFiltered) {
  set<string> expected = {
    "ARM::Device:Startup&RteTest Startup@2.0.3 (ARM::RteTest_DFP@0.2.0)"
  };
  vector<string> components;
  ContextDesc descriptor;
  const string& filenameInput = testinput_folder + "/TestProject/test.cproject.yml";
  EXPECT_TRUE(m_parser.ParseCproject(filenameInput, false, true));
  EXPECT_TRUE(m_worker.AddContexts(m_parser, descriptor, filenameInput));
  EXPECT_TRUE(m_worker.ListComponents(components, "test", "Startup"));
  EXPECT_EQ(expected, set<string>(components.begin(), components.end()));
}

TEST_F(ProjMgrUnitTests, ListDependencies) {
  set<string> expected = {
     "ARM::Device:Startup&RteTest Startup@2.0.3 require RteTest:CORE",
  };
  vector<string> dependencies;
  ContextDesc descriptor;
  const string& filenameInput = testinput_folder + "/TestProject/test-dependency.cproject.yml";
  EXPECT_TRUE(m_parser.ParseCproject(filenameInput, false, true));
  EXPECT_TRUE(m_worker.AddContexts(m_parser, descriptor, filenameInput));
  EXPECT_TRUE(m_worker.ListDependencies(dependencies, "test-dependency", "CORE"));
  EXPECT_EQ(expected, set<string>(dependencies.begin(), dependencies.end()));
}

TEST_F(ProjMgrUnitTests, RunListContexts) {
  set<string> expected = {
    "test1.Debug+CM0",
    "test1.Release+CM0",
    "test2.Debug+CM0",
    "test2.Debug+CM3"
  };
  const string& dirInput = testinput_folder + "/TestSolution/";
  const string& filenameInput = dirInput + "test.csolution.yml";
  error_code ec;
  EXPECT_TRUE(m_parser.ParseCsolution(filenameInput, false));
  for (const auto& cproject : m_parser.GetCsolution().cprojects) {
    string const& cprojectFile = fs::canonical(dirInput + cproject, ec).generic_string();
    EXPECT_TRUE(m_parser.ParseCproject(cprojectFile, false, false));
  }
  for (auto& descriptor : m_parser.GetCsolution().contexts) {
    const string& cprojectFile = fs::canonical(dirInput + descriptor.cproject, ec).generic_string();
    EXPECT_TRUE(m_worker.AddContexts(m_parser, descriptor, cprojectFile));
  }
  vector<string> contexts;
  EXPECT_TRUE(m_worker.ListContexts(contexts));
  EXPECT_EQ(expected, set<string>(contexts.begin(), contexts.end()));
}

TEST_F(ProjMgrUnitTests, RunListContexts_Without_BuildTypes) {
  set<string> expected = {
    "test1+CM0",
    "test2+CM0",
    "test2+CM3",
    "test2+Debug",
    "test2+Release"
  };
  const string& dirInput = testinput_folder + "/TestSolution/";
  const string& filenameInput = dirInput + "test.csolution_no_buildtypes.yml";
  error_code ec;
  EXPECT_TRUE(m_parser.ParseCsolution(filenameInput, false));
  for (const auto& cproject : m_parser.GetCsolution().cprojects) {
    string const& cprojectFile = fs::canonical(dirInput + cproject, ec).generic_string();
    EXPECT_TRUE(m_parser.ParseCproject(cprojectFile, false, false));
  }
  for (auto& descriptor : m_parser.GetCsolution().contexts) {
    const string& cprojectFile = fs::canonical(dirInput + descriptor.cproject, ec).generic_string();
    EXPECT_TRUE(m_worker.AddContexts(m_parser, descriptor, cprojectFile));
  }
  vector<string> contexts;
  EXPECT_TRUE(m_worker.ListContexts(contexts));
  EXPECT_EQ(expected, set<string>(contexts.begin(), contexts.end()));
}

TEST_F(ProjMgrUnitTests, AddContextFailed) {
  ContextDesc descriptor;
  const string& filenameInput = testinput_folder + "/TestSolution/test.csolution_missing_project.yml";
  EXPECT_TRUE(m_parser.ParseCsolution(filenameInput, false));
  EXPECT_FALSE(m_worker.AddContexts(m_parser, descriptor, filenameInput));
}

TEST_F(ProjMgrUnitTests, GenerateCprj) {
  const string& filenameInput = testinput_folder + "/TestProject/test.cproject.yml";
  const string& filenameOutput = testoutput_folder + "/GenerateCprjTest.cprj";
  ContextDesc descriptor;
  EXPECT_TRUE(m_parser.ParseCproject(filenameInput, false, true));
  EXPECT_TRUE(m_worker.AddContexts(m_parser, descriptor, filenameInput));
  map<string, ContextItem>* contexts = nullptr;
  m_worker.GetContexts(contexts);
  EXPECT_TRUE(m_worker.ProcessContext(contexts->begin()->second, true));
  EXPECT_TRUE(m_generator.GenerateCprj(contexts->begin()->second, filenameOutput));

  CompareFile(testoutput_folder + "/GenerateCprjTest.cprj",
    testinput_folder + "/TestProject/test.cprj");
}

TEST_F(ProjMgrUnitTests, GetInstalledPacks) {
  auto kernel = ProjMgrKernel::Get();
  const auto& cmsisPackRoot = kernel->GetCmsisPackRoot();
  std::set<std::string> pdscFiles;

  kernel->SetCmsisPackRoot(string(CMAKE_SOURCE_DIR) + "test/local");
  EXPECT_TRUE(kernel->GetInstalledPacks(pdscFiles));

  kernel->SetCmsisPackRoot(string(CMAKE_SOURCE_DIR) + "test/local-malformed");
  EXPECT_FALSE(kernel->GetInstalledPacks(pdscFiles));

  kernel->SetCmsisPackRoot(cmsisPackRoot);
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_processor) {
  char* argv[7];

  // convert -s solution.yml
  const string& csolution = testinput_folder + "/TestSolution/test.csolution_pname.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-s";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv));

  // Check generated CPRJs
  CompareFile(testoutput_folder + "/test2.Debug+CM0/test2.Debug+CM0.cprj",
    testinput_folder + "/TestSolution/TestProject2/test2.Debug+CM0_pname.cprj");
  CompareFile(testoutput_folder + "/test2.Debug+CM3/test2.Debug+CM3.cprj",
    testinput_folder + "/TestSolution/TestProject2/test2.Debug+CM3_pname.cprj");
}

TEST_F(ProjMgrUnitTests, RunProjMgrLayers_pname) {
  char* argv[6];
  const string& cproject = testinput_folder + "/TestLayers/testlayers.cproject_pname.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-p";
  argv[3] = (char*)cproject.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv));
}

TEST_F(ProjMgrUnitTests, RunProjMgrLayers_no_device_found) {
  char* argv[6];
  const string& cproject = testinput_folder + "/TestLayers/testlayers.cproject_no_device_name.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-p";
  argv[3] = (char*)cproject.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv));
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_No_Device_Name) {
  char* argv[4];
  const string& csolution = testinput_folder + "/TestSolution/test.csolution_no_device_name.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-s";
  argv[3] = (char*)csolution.c_str();
  EXPECT_EQ(1, RunProjMgr(4, argv));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_No_Board_No_Device) {
  // Test with no board no device info
  char* argv[6];
  const string& cproject = testinput_folder +
    "/TestProject/test.cproject_no_board_no_device.yml";
  const string& expected = "missing device and/or board info";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"-p";
  argv[3] = (char*)cproject.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Invalid_Board_Name) {
  // Test project with invalid board name
  char* argv[6];
  const string& cproject = testinput_folder + "/TestProject/test.cproject_board_name_invalid.yml";
  const string& expected = "board 'Keil::RteTest_unknown' was not found";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"-p";
  argv[3] = (char*)cproject.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Invalid_Board_Vendor) {
  // Test project with invalid vendor name
  char* argv[6];
  const string& cproject = testinput_folder + "/TestProject/test.cproject_board_vendor_invalid.yml";
  const string& expected = "board 'UNKNOWN::RteTest Dummy board' was not found";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"-p";
  argv[3] = (char*)cproject.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Only_Board_Info) {
  // Test project with only board info and missing device info
  char* argv[6];
  const string& cproject = testinput_folder + "/TestProject/test.cproject_only_board.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-p";
  argv[3] = (char*)cproject.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Only_Board_No_Pname) {
  // Test project with only board info and no pname
  char* argv[6];
  const string& cproject = testinput_folder + "/TestProject/test.cproject_only_board_no_pname.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-p";
  argv[3] = (char*)cproject.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Device_Unknown) {
  // Test project with unknown device
  char* argv[6];
  const string& cproject = testinput_folder +
    "/TestProject/test.cproject_device_unknown.yml";
  const string& expected = "processor device 'RteTest_ARM_UNKNOWN' was not found";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"-p";
  argv[3] = (char*)cproject.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Device_Unknown_Vendor) {
  // Test project with unknown device vendor
  char* argv[6];
  const string& cproject = testinput_folder +
    "/TestProject/test.cproject_device_unknown_vendor.yml";

  const string& expected = "processor device 'RteTest_ARMCM0' was not found";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"-p";
  argv[3] = (char*)cproject.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Device_Unknown_Processor) {
  // Test project with unknown processor
  char* argv[6];
  const string& cproject = testinput_folder +
    "/TestProject/test.cproject_device_unknown_processor.yml";
  const string& expected = "processor name 'NOT_AVAILABLE' was not found";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"-p";
  argv[3] = (char*)cproject.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Device_Unavailable_In_Board) {
  // Test project with device different from the board's mounted device
  char* argv[6];
  const string& cproject = testinput_folder +
    "/TestProject/test.cproject_device_unavailable_in_board.yml";
  const string& expected = "processor device 'RteTest_ARMCM7' was not found";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"-p";
  argv[3] = (char*)cproject.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Device_Pname_Unavailable_In_Board) {
  // Test project with device processor name different from the board's mounted device's processors
  char* argv[6];
  const string& cproject = testinput_folder +
    "/TestProject/test.cproject_device_pname_unavailable_in_board.yml";
  const string& expected = "processor name 'cm0_core7' was not found";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"-p";
  argv[3] = (char*)cproject.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Only_Device_Info) {
  // Test project with only device info
  char* argv[6];
  const string& cproject = testinput_folder + "/TestProject/test.cproject.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-p";
  argv[3] = (char*)cproject.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Board_And_Device_Info) {
  // Test project with valid board and device info
  char* argv[6];
  const string& cproject = testinput_folder +
    "/TestProject/test.cproject_board_and_device.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-p";
  argv[3] = (char*)cproject.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Correct_Board_Wrong_Device_Info) {
  // Test project with correct board info but wrong device info
  char* argv[6];
  const string& cproject = testinput_folder +
    "/TestProject/test.cproject_correct_board_wrong_device.yml";
  const string& expected = "processor device 'RteTest_ARMCM_Unknown' was not found";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"-p";
  argv[3] = (char*)cproject.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Correct_Device_Wrong_Board_Info) {
  // Test project with correct device info but wrong board info
  char* argv[6];
  const string& cproject = testinput_folder +
    "/TestProject/test.cproject_correct_device_wrong_board.yml";
  const string& expected = "board 'Keil::RteTest unknown board' was not found";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"-p";
  argv[3] = (char*)cproject.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Board_Multi_Mounted_Devices) {
  // Test Project with only board info and multiple mounted devices
  char* argv[6];
  const string& cproject = testinput_folder +
    "/TestProject/test.cproject_board_multi_mounted_device.yml";
  const string& expected = "found multiple mounted devices";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"-p";
  argv[3] = (char*)cproject.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Board_No_Mounted_Devices) {
  // Test Project with only board info and no mounted devices
  char* argv[6];
  const string& cproject = testinput_folder +
    "/TestProject/test.cproject_board_no_mounted_device.yml";
  const string& expected = "found no mounted device";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"-p";
  argv[3] = (char*)cproject.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Board_Device_Info) {
  // Test Project with board mounted device different than available devices
  char* argv[6];
  const string& cproject = testinput_folder +
    "/TestProject/test.cproject_mounted_device_differs_avaialble_device.yml";
  const string& expected = "specified device 'RteTest_ARMCM0_Dual' and board mounted device 'RteTest_ARMCM0_Test' are different";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"-p";
  argv[3] = (char*)cproject.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListGenerators) {
  char* argv[7];
  const string& csolution = testinput_folder + "/TestGenerator/test-gpdsc.csolution.yml";
  argv[1] = (char*)"list";
  argv[2] = (char*)"generators";
  argv[3] = (char*)"-s";
  argv[4] = (char*)csolution.c_str();
  argv[5] = (char*)"-c";
  argv[6] = (char*)"test-gpdsc.Debug+CM0";
  EXPECT_EQ(0, RunProjMgr(7, argv));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListGeneratorsEmptyContext) {
  char* argv[5];
  const string& csolution = testinput_folder + "/TestGenerator/test-gpdsc.csolution.yml";
  argv[1] = (char*)"list";
  argv[2] = (char*)"generators";
  argv[3] = (char*)"-s";
  argv[4] = (char*)csolution.c_str();
  EXPECT_EQ(0, RunProjMgr(5, argv));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListGeneratorsEmptyContextMultipleTypes) {
  char* argv[5];
  const string& csolution = testinput_folder + "/TestGenerator/test-gpdsc-multiple-types.csolution.yml";
  argv[1] = (char*)"list";
  argv[2] = (char*)"generators";
  argv[3] = (char*)"-s";
  argv[4] = (char*)csolution.c_str();
  EXPECT_EQ(1, RunProjMgr(5, argv));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListGeneratorsNonExistentContext) {
  char* argv[7];
  const string& csolution = testinput_folder + "/TestGenerator/test-gpdsc.csolution.yml";
  argv[1] = (char*)"list";
  argv[2] = (char*)"generators";
  argv[3] = (char*)"-s";
  argv[4] = (char*)csolution.c_str();
  argv[5] = (char*)"-c";
  argv[6] = (char*)"NON-EXISTENT-CONTEXT";
  EXPECT_EQ(1, RunProjMgr(7, argv));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListGeneratorsNonExistentSolution) {
  char* argv[5];
  const string& csolution = testinput_folder + "/TestGenerator/NON-EXISTENT.csolution.yml";
  argv[1] = (char*)"list";
  argv[2] = (char*)"generators";
  argv[3] = (char*)"-s";
  argv[4] = (char*)csolution.c_str();
  EXPECT_EQ(1, RunProjMgr(5, argv));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ExecuteGenerator) {
  char* argv[8];
  const string& csolution = testinput_folder + "/TestGenerator/test-gpdsc.csolution.yml";
  argv[1] = (char*)"run";
  argv[2] = (char*)"-g";
  argv[3] = (char*)"RteTestGeneratorIdentifier";
  argv[4] = (char*)"-s";
  argv[5] = (char*)csolution.c_str();
  argv[6] = (char*)"-c";
  argv[7] = (char*)"test-gpdsc.Debug+CM0";

  //TODO: Enable ExecuteGenerator tests for all platforms once it is implemented
#ifdef _WIN32
  EXPECT_EQ(0, RunProjMgr(8, argv));
#else
  EXPECT_EQ(1, RunProjMgr(8, argv));
#endif
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ExecuteGeneratorEmptyContext) {
  char* argv[6];
  const string& csolution = testinput_folder + "/TestGenerator/test-gpdsc.csolution.yml";
  argv[1] = (char*)"run";
  argv[2] = (char*)"-g";
  argv[3] = (char*)"RteTestGeneratorIdentifier";
  argv[4] = (char*)"-s";
  argv[5] = (char*)csolution.c_str();

  //TODO: Enable ExecuteGenerator tests for all platforms once it is implemented
#ifdef _WIN32
  EXPECT_EQ(0, RunProjMgr(6, argv));
#else
  EXPECT_EQ(1, RunProjMgr(6, argv));
#endif
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ExecuteGeneratorEmptyContextMultipleTypes) {
  char* argv[6];
  const string& csolution = testinput_folder + "/TestGenerator/test-gpdsc-multiple-types.csolution.yml";
  argv[1] = (char*)"run";
  argv[2] = (char*)"-g";
  argv[3] = (char*)"RteTestGeneratorIdentifier";
  argv[4] = (char*)"-s";
  argv[5] = (char*)csolution.c_str();

  EXPECT_EQ(1, RunProjMgr(6, argv));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ExecuteGeneratorNonExistentContext) {
  char* argv[8];
  const string& csolution = testinput_folder + "/TestGenerator/test-gpdsc.csolution.yml";
  argv[1] = (char*)"run";
  argv[2] = (char*)"-g";
  argv[3] = (char*)"RteTestGeneratorIdentifier";
  argv[4] = (char*)"-s";
  argv[5] = (char*)csolution.c_str();
  argv[6] = (char*)"-c";
  argv[7] = (char*)"NON-EXISTENT-CONTEXT";
  EXPECT_EQ(1, RunProjMgr(8, argv));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ExecuteGeneratorNonExistentSolution) {
  char* argv[6];
  const string& csolution = testinput_folder + "/TestGenerator/NON-EXISTENT.csolution.yml";
  argv[1] = (char*)"run";
  argv[2] = (char*)"-g";
  argv[3] = (char*)"RteTestGeneratorIdentifier";
  argv[4] = (char*)"-s";
  argv[5] = (char*)csolution.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv));
}

TEST_F(ProjMgrUnitTests, ListGenerators) {
  set<string> expected = {
     "RteTestGeneratorIdentifier (RteTest Generator Description)",
  };
  vector<string> generators;
  m_csolutionFile = testinput_folder + "/TestGenerator/test-gpdsc.csolution.yml";
  error_code ec;
  m_rootDir = fs::path(m_csolutionFile).parent_path().generic_string();
  m_context = "test-gpdsc.Debug+CM0";
  EXPECT_TRUE(PopulateContexts());
  EXPECT_TRUE(m_worker.ListGenerators(m_context, generators));
  EXPECT_EQ(expected, set<string>(generators.begin(), generators.end()));
}

TEST_F(ProjMgrUnitTests, ExecuteGenerator) {
  m_csolutionFile = testinput_folder + "/TestGenerator/test-gpdsc.csolution.yml";
  error_code ec;
  m_rootDir = fs::path(m_csolutionFile).parent_path().generic_string();
  m_context = "test-gpdsc.Debug+CM0";
  m_codeGenerator = "RteTestGeneratorIdentifier";
  EXPECT_TRUE(PopulateContexts());

  //TODO: Enable ExecuteGenerator tests for all platforms once it is implemented
#ifdef _WIN32
  EXPECT_TRUE(m_worker.ExecuteGenerator(m_context, m_codeGenerator));
#else
  EXPECT_FALSE(m_worker.ExecuteGenerator(m_context, m_codeGenerator));
#endif
}
