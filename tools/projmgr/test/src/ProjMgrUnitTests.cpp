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

  static void SetUpTestSuite() {
    error_code ec;
    std::string schemaSrcDir, schemaDestDir;
    schemaSrcDir = string(TEST_FOLDER) + "../schemas";
    schemaDestDir = string(PROJMGRUNITTESTS_BIN_PATH) + "/../etc";

    if (!RteFsUtils::Exists(schemaSrcDir)) {
      GTEST_SKIP();
    }

    if (RteFsUtils::Exists(schemaDestDir)) {
      RteFsUtils::RemoveDir(schemaDestDir);
    }
    RteFsUtils::CreateDirectories(schemaDestDir);

    fs::copy(fs::path(schemaSrcDir), fs::path(schemaDestDir), fs::copy_options::recursive, ec);
    if (ec) {
      GTEST_SKIP();
    }
  }

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
  CoutRedirect coutRedirect;

  // list packs
  argv[1] = (char*)"list";
  argv[2] = (char*)"packs";
  argv[3] = (char*)"--filter";
  argv[4] = (char*)"ARM::RteTest_DFP";
  EXPECT_EQ(0, RunProjMgr(5, argv));

  auto outStr = coutRedirect.GetString();
  EXPECT_STREQ(outStr.c_str(), "ARM::RteTest_DFP@0.1.1\n");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListDevices) {
  char* argv[5];
  CoutRedirect coutRedirect;

  // list devices
  argv[1] = (char*)"list";
  argv[2] = (char*)"devices";
  argv[3] = (char*)"--filter";
  argv[4] = (char*)"RteTest_ARMCM4";
  EXPECT_EQ(0, RunProjMgr(5, argv));
  
  auto outStr = coutRedirect.GetString();
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
  CoutRedirect coutRedirect;

  // list dependencies
  const string& cproject = testinput_folder + "/TestProject/test-dependency.cproject.yml";
  argv[1] = (char*)"list";
  argv[2] = (char*)"dependencies";
  argv[3] = (char*)"-p";
  argv[4] = (char*)cproject.c_str();
  EXPECT_EQ(0, RunProjMgr(5, argv));

  auto outStr = coutRedirect.GetString();
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
  CoutRedirect coutRedirect;

  // convert -s solution.yml
  const string& csolution = testinput_folder + "/TestSolution/test.csolution.yml";
  argv[1] = (char*)"list";
  argv[2] = (char*)"contexts";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  argv[5] = (char*)"--filter";
  argv[6] = (char*)"test1";
  EXPECT_EQ(0, RunProjMgr(7, argv));

  auto outStr = coutRedirect.GetString();
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

TEST_F(ProjMgrUnitTests, ListPacks) {
  set<string> expected = {
    "ARM::RteTest@0.1.0",
    "ARM::RteTest_DFP@0.1.1"
  };
  vector<string> packs;
  EXPECT_TRUE(m_worker.ListPacks("RteTest", packs));
  EXPECT_EQ(expected, set<string>(packs.begin(), packs.end()));
}

TEST_F(ProjMgrUnitTests, ListDevices) {
  set<string> expected = {
    "RteTest_ARMCM4_FP",
    "RteTest_ARMCM4_NOFP",
    "RteTest_ARMCM4_FP",
    "RteTest_ARMCM4_NOFP"
  };
  vector<string> devices;
  EXPECT_TRUE(m_worker.ListDevices("CM4", devices));
  EXPECT_EQ(expected, set<string>(devices.begin(), devices.end()));
}

TEST_F(ProjMgrUnitTests, ListComponents) {
  set<string> expected = {
    "ARM::Device:Startup&RteTest Startup@2.0.3 (ARM::RteTest_DFP@0.1.1)",
  };
  vector<string> components;
  EXPECT_TRUE(m_worker.ListComponents("Startup", components));
  EXPECT_EQ(expected, set<string>(components.begin(), components.end()));
}

TEST_F(ProjMgrUnitTests, ListComponentsDeviceFiltered) {
  set<string> expected = {
    "ARM::Device:Startup&RteTest Startup@2.0.3 (ARM::RteTest_DFP@0.1.1)"
  };
  vector<string> components;
  ContextDesc descriptor;
  const string& filenameInput = testinput_folder + "/TestProject/test.cproject.yml";
  EXPECT_TRUE(m_parser.ParseCproject(filenameInput, false, true));
  EXPECT_TRUE(m_worker.AddContexts(m_parser, descriptor, filenameInput));
  EXPECT_TRUE(m_worker.ListComponents("Startup", components));
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
  EXPECT_TRUE(m_worker.ListDependencies("CORE", dependencies));
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
  EXPECT_TRUE(m_worker.ListContexts("", contexts));
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
  EXPECT_TRUE(m_worker.ListContexts("", contexts));
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
  std::map<std::string, ContextItem> contexts;
  m_worker.GetContexts(contexts);
  EXPECT_TRUE(m_worker.ProcessContext(contexts.begin()->second, true));
  EXPECT_TRUE(m_generator.GenerateCprj(contexts.begin()->second, filenameOutput));

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
