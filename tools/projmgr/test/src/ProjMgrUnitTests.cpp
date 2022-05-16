/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgr.h"
#include "ProjMgrTestEnv.h"

#include "RteFsUtils.h"

#include "CrossPlatformUtils.h"

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
  char* argv[7];
  map<std::pair<string, string>, string> testInputs = {
    {{"TestSolution/test.csolution.yml", "test1.Debug+CM0"},
      "ARM::RteTest_DFP@0.2.0\n" },
    {{"TestSolution/test.csolution_filtered_pack_selection.yml", "test1.Debug+CM0"},
      "ARM::RteTest@0.1.0\nARM::RteTestBoard@0.1.0\nARM::RteTestGenerator@0.1.0\nARM::RteTest_DFP@0.2.0\n"},
    {{"TestSolution/test.csolution_no_packs.yml", "test1.Debug+CM0"},
      "ARM::RteTest@0.1.0\nARM::RteTestBoard@0.1.0\nARM::RteTestGenerator@0.1.0\nARM::RteTest_DFP@0.1.1\nARM::RteTest_DFP@0.2.0\n"},
    {{"TestSolution/test.csolution_pack_selection.yml", "test2.Debug+CM0"},
      "ARM::RteTestGenerator@0.1.0\nARM::RteTest_DFP@0.2.0\n"},
    {{"TestSolution/multicore.csolution.yml", "multicore+CM0"},
      "ARM::RteTest@0.1.0\nARM::RteTestBoard@0.1.0\nARM::RteTestGenerator@0.1.0\nARM::RteTest_DFP@0.1.1\nARM::RteTest_DFP@0.2.0\n"},
    {{"TestDefault/build-types.csolution.yml", "project.Debug"},
      "ARM::RteTest_DFP@0.1.1\n" }
  };

  // positive tests
  argv[1] = (char*)"list";
  argv[2] = (char*)"packs";
  argv[3] = (char*)"-s";
  for (const auto& [input, expected] : testInputs) {
    StdStreamRedirect streamRedirect;
    const string& csolution = testinput_folder + "/" + input.first;
    argv[4] = (char*)csolution.c_str();
    argv[5] = (char*)"-c";
    argv[6] = (char*)input.second.c_str();
    EXPECT_EQ(0, RunProjMgr(7, argv));

    auto outStr = streamRedirect.GetOutString();
    EXPECT_STREQ(outStr.c_str(), expected.c_str()) << "error listing pack for " << csolution << endl;
  }

  map<std::pair<string, string>, string> testFalseInputs = {
    {{"TestSolution/test.csolution_local_pack_path_not_found.yml", "test1.Debug+CM0"},
      "error csolution: pack path: ./SolutionSpecificPack/ARM does not exist\nerror csolution: processing pack list failed\n"},
    {{"TestSolution/test.csolution_local_pack_file_not_found.yml", "test1.Debug+CM0"},
      "error csolution: no pdsc file found under: ../SolutionSpecificPack/Device\nerror csolution: processing pack list failed\n"},
    {{"TestSolution/test.csolution_invalid_pack.yml", "test1.Debug+CM0"},
      "error csolution: required pack: ARM::RteTest_INVALID@0.2.0 not found\nerror csolution: processing pack list failed\n"},
    {{"TestSolution/test.csolution_unknown_file.yml", "test1.Debug+CM0"},
      "error csolution: csolution file was not found"},
    {{"TestSolution/test.csolution.yml", "invalid.context"},
      "error csolution: context 'invalid.context' was not found"}
  };
  // negative tests
  for (const auto& [input, expected] : testFalseInputs) {
    StdStreamRedirect streamRedirect;
    const string& csolution = testinput_folder + "/" + input.first;
    argv[4] = (char*)csolution.c_str();
    argv[5] = (char*)"-c";
    argv[6] = (char*)input.second.c_str();
    EXPECT_EQ(1, RunProjMgr(7, argv));

    auto errStr = streamRedirect.GetErrorString();
    EXPECT_NE(string::npos, errStr.find(expected)) << "error listing pack for " << csolution << endl;
  }
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListPacks_1) {
  char* argv[3];
  StdStreamRedirect streamRedirect;
  const string& expected = "ARM::RteTest@0.1.0\nARM::RteTestBoard@0.1.0\nARM::RteTestGenerator@0.1.0\nARM::RteTest_DFP@0.1.1\nARM::RteTest_DFP@0.2.0\n";
  // list packs
  argv[1] = (char*)"list";
  argv[2] = (char*)"packs";
  EXPECT_EQ(0, RunProjMgr(3, argv));

  auto outStr = streamRedirect.GetOutString();
  EXPECT_STREQ(outStr.c_str(), expected.c_str());
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListPacks_project) {
  char* argv[7];
  StdStreamRedirect streamRedirect;
  const string& cproject = testinput_folder + "/TestDefault/project.cproject.yml";
  // list packs
  argv[1] = (char*)"list";
  argv[2] = (char*)"packs";
  argv[3] = (char*)"-p";
  argv[4] = (char*)cproject.c_str();
  argv[5] = (char*)"-c";
  argv[6] = (char*)"project.Debug";
  EXPECT_EQ(0, RunProjMgr(7, argv));

  auto outStr = streamRedirect.GetOutString();
  EXPECT_STREQ(outStr.c_str(), "ARM::RteTest_DFP@0.1.1\n");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListBoards) {
  char* argv[5];
  StdStreamRedirect streamRedirect;

  // list boards
  argv[1] = (char*)"list";
  argv[2] = (char*)"boards";
  argv[3] = (char*)"--filter";
  argv[4] = (char*)"Dummy";
  EXPECT_EQ(0, RunProjMgr(5, argv));

  auto outStr = streamRedirect.GetOutString();
  EXPECT_STREQ(outStr.c_str(), "RteTest Dummy board\n");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListBoardsProjectFiltered) {
  char* argv[7];
  StdStreamRedirect streamRedirect;

  // list boards
  const string& cproject = testinput_folder + "/TestProject/test.cproject_board_and_device.yml";
  argv[1] = (char*)"list";
  argv[2] = (char*)"boards";
  argv[3] = (char*)"--filter";
  argv[4] = (char*)"Dummy";
  argv[5] = (char*)"-p";
  argv[6] = (char*)cproject.c_str();
  EXPECT_EQ(0, RunProjMgr(7, argv));

  auto outStr = streamRedirect.GetOutString();
  EXPECT_STREQ(outStr.c_str(), "RteTest Dummy board\n");
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
  EXPECT_STREQ(outStr.c_str(), "RteTest_ARMCM4\nRteTest_ARMCM4_FP\nRteTest_ARMCM4_NOFP\n");
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

TEST_F(ProjMgrUnitTests, RunProjMgr_LinkerScript) {
  char* argv[6];
  // convert -p cproject.yml
  const string& cproject = testinput_folder + "/TestProject/test_linker_script.cproject.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-p";
  argv[3] = (char*)cproject.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv));

  // Check generated CPRJ
  CompareFile(testoutput_folder + "/test_linker_script/test_linker_script.cprj",
    testinput_folder + "/TestProject/test_linker_script.cprj");
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
    testinput_folder + "/TestSolution/ref/test1.Debug+CM0/test1.Debug+CM0.cprj");
  CompareFile(testoutput_folder + "/test1.Release+CM0/test1.Release+CM0.cprj",
    testinput_folder + "/TestSolution/ref/test1.Release+CM0/test1.Release+CM0.cprj");

  CompareFile(testoutput_folder + "/test2.Debug+CM0/test2.Debug+CM0.cprj",
    testinput_folder + "/TestSolution/ref/test2.Debug+CM0/test2.Debug+CM0.cprj");
  CompareFile(testoutput_folder + "/test2.Debug+CM3/test2.Debug+CM3.cprj",
    testinput_folder + "/TestSolution/ref/test2.Debug+CM3/test2.Debug+CM3.cprj");
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

TEST_F(ProjMgrUnitTests, RunProjMgrLayers2) {
  char* argv[4];

  // convert -s solution.yml
  const string& csolution = testinput_folder + "/TestLayers/testlayers.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-s";
  argv[3] = (char*)csolution.c_str();
  EXPECT_EQ(0, RunProjMgr(4, argv));

  // Check generated CPRJs
  CompareFile(testinput_folder + "/TestLayers/testlayers.Debug.cprj",
    testinput_folder + "/TestLayers/ref2/testlayers.Debug.cprj");
  CompareFile(testinput_folder + "/TestLayers/testlayers.Release.cprj",
    testinput_folder + "/TestLayers/ref2/testlayers.Release.cprj");
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
    "ARM::RteTestBoard@0.1.0",
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
    "ARM::RteTest_DFP@0.1.1",
    "ARM::RteTest_DFP@0.2.0"
  };
  vector<string> packs;
  ContextDesc descriptor;
  const string& filenameInput = testinput_folder + "/TestProject/test.cproject.yml";
  EXPECT_TRUE(m_parser.ParseCproject(filenameInput, false, true));
  EXPECT_TRUE(m_worker.AddContexts(m_parser, descriptor, filenameInput));
  EXPECT_TRUE(m_worker.ListPacks(packs, "test", "RteTest_DFP"));
  EXPECT_EQ(expected, set<string>(packs.begin(), packs.end()));
}

TEST_F(ProjMgrUnitTests, ListBoards) {
  set<string> expected = {
    "RteTest Dummy board"
  };
  vector<string> devices;
  EXPECT_TRUE(m_worker.ListBoards(devices, "", "Dummy"));
  EXPECT_EQ(expected, set<string>(devices.begin(), devices.end()));
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
    "RteTest_ARMCM3"
  };
  vector<string> devices;
  ContextDesc descriptor;
  const string& filenameInput = testinput_folder + "/TestProject/test.cproject.yml";
  EXPECT_TRUE(m_parser.ParseCproject(filenameInput, false, true));
  EXPECT_TRUE(m_worker.AddContexts(m_parser, descriptor, filenameInput));
  EXPECT_TRUE(m_worker.ListDevices(devices, "test", "CM3"));
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
    testinput_folder + "/TestProject/GenerateCprjTest.cprj");
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
    testinput_folder + "/TestSolution/ref/test2.Debug+CM0/test2.Debug+CM0_pname.cprj");
  CompareFile(testoutput_folder + "/test2.Debug+CM3/test2.Debug+CM3.cprj",
    testinput_folder + "/TestSolution/ref/test2.Debug+CM3/test2.Debug+CM3_pname.cprj");
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

  // Check generated CPRJ
  CompareFile(testoutput_folder + "/test/test.cprj",
    testinput_folder + "/TestProject/test_only_board.cprj");
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
  const string& expected = "specified device 'RteTest_ARM_UNKNOWN' was not found";
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

  const string& expected = "specified device 'RteTest_ARMCM0' was not found";
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
  const string& expected = "specified device 'RteTest_ARMCM7' was not found";
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
  const string& expected = "specified device 'RteTest_ARMCM_Unknown' was not found";
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

TEST_F(ProjMgrUnitTests, RunProjMgr_Board_Device_Variant) {
  // Test Project with only board info and single mounted device with single variant
  char* argv[6];
  const string& cproject = testinput_folder +
    "/TestProject/test.cproject_board_device_variant.yml";

  argv[1] = (char*)"convert";
  argv[2] = (char*)"-p";
  argv[3] = (char*)cproject.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Board_Multi_Variants_And_Device) {
  // Test Project with device variant and board info and mounted device with multiple variants
  char* argv[6];
  const string& cproject = testinput_folder +
    "/TestProject/test.cproject_board_multi_variant_and_device.yml";

  argv[1] = (char*)"convert";
  argv[2] = (char*)"-p";
  argv[3] = (char*)cproject.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Board_Multi_Variants) {
  // Test Project with only board info and mounted device with multiple variants
  char* argv[6];
  const string& cproject = testinput_folder +
    "/TestProject/test.cproject_board_multi_variant.yml";
  const string& expected = "found multiple device variants";
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
  // Test Project with board mounted device different than selected devices
  char* argv[6];
  const string& cproject = testinput_folder +
    "/TestProject/test.cproject_mounted_device_differs_selected_device.yml";
  const string& expected = "warning csolution: specified device 'RteTest_ARMCM0' and board mounted device 'RteTest_ARMCM0_Dual' are different";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"-p";
  argv[3] = (char*)cproject.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv));
  auto warnStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, warnStr.find(expected));
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

  const string& hostType = CrossPlatformUtils::GetHostType();
  if (hostType == "linux" || hostType == "win") {
    EXPECT_EQ(0, RunProjMgr(8, argv));
  } else {
    EXPECT_EQ(1, RunProjMgr(8, argv));
  }
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ExecuteGeneratorEmptyContext) {
  char* argv[6];
  const string& csolution = testinput_folder + "/TestGenerator/test-gpdsc.csolution.yml";
  argv[1] = (char*)"run";
  argv[2] = (char*)"-g";
  argv[3] = (char*)"RteTestGeneratorIdentifier";
  argv[4] = (char*)"-s";
  argv[5] = (char*)csolution.c_str();

  const string& hostType = CrossPlatformUtils::GetHostType();
  if (hostType == "linux" || hostType == "win") {
    EXPECT_EQ(0, RunProjMgr(6, argv));
  } else {
    EXPECT_EQ(1, RunProjMgr(6, argv));
  }
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

  const string& hostType = CrossPlatformUtils::GetHostType();
  if (hostType == "linux" || hostType == "win") {
    EXPECT_TRUE(m_worker.ExecuteGenerator(m_context, m_codeGenerator));
  } else {
    EXPECT_FALSE(m_worker.ExecuteGenerator(m_context, m_codeGenerator));
  }
}


TEST_F(ProjMgrUnitTests, ExecuteGeneratorWithKey) {
  m_csolutionFile = testinput_folder + "/TestGenerator/test-gpdsc_with_key.csolution.yml";
  error_code ec;
  m_rootDir = fs::path(m_csolutionFile).parent_path().generic_string();
  m_context = "test-gpdsc_with_key.Debug+CM0";
  m_codeGenerator = "RteTestGeneratorWithKey";
  EXPECT_TRUE(PopulateContexts());

  const string& hostType = CrossPlatformUtils::GetHostType();
  string genFolder = testcmsispack_folder + "/ARM/RteTestGenerator/0.1.0/Generator";
  // we use environment variable to test on all pl since it is reliable
  CrossPlatformUtils::SetEnv("RTE_GENERATOR_WITH_KEY", genFolder);
  if (hostType == "linux" || hostType == "win") {
    EXPECT_TRUE(m_worker.ExecuteGenerator(m_context, m_codeGenerator));
  } else {
    EXPECT_FALSE(m_worker.ExecuteGenerator(m_context, m_codeGenerator));
  }
}


TEST_F(ProjMgrUnitTests, RunProjMgrSolution_Filtered_Pack_Selection) {
  char* argv[7];

  // convert -s solution.yml
  const string& csolution = testinput_folder + "/TestSolution/test.csolution_filtered_pack_selection.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-s";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv));
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_Pack_Selection) {
  char* argv[7];

  // convert -s solution.yml
  const string& csolution = testinput_folder + "/TestSolution/test.csolution_pack_selection.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-s";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv));

  // Check generated CPRJs
  CompareFile(testoutput_folder + "/test2.Debug+CM0/test2.Debug+CM0.cprj",
    testinput_folder + "/TestSolution/ref/test2.Debug+CM0/test2.Debug+CM0_pack_selection.cprj");
  CompareFile(testoutput_folder + "/test2.Debug+TestGen/test2.Debug+TestGen.cprj",
    testinput_folder + "/TestSolution/ref/test2.Debug+TestGen/test2.Debug+TestGen.cprj");
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_No_Packs) {
  char* argv[7];

  // convert -s solution.yml
  const string& csolution = testinput_folder + "/TestSolution/test.csolution_no_packs.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-s";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv));
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_Invalid_Packs) {
  char* argv[7];
  StdStreamRedirect streamRedirect;
  std::string errExpected = "required pack: ARM::RteTest_INVALID@0.2.0 not found";

  // convert -s solution.yml
  const string& csolution = testinput_folder + "/TestSolution/test.csolution_invalid_pack.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-s";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv));

  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(errExpected));
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_Local_Pack) {
  char* argv[7];
  const string& csolution = testinput_folder + "/TestSolution/pack_path.csolution.yml";

  // convert -s solution.yml
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-s";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv));

  // Check generated CPRJ
  CompareFile(testoutput_folder + "/pack_path+CM0/pack_path+CM0.cprj",
    testinput_folder + "/TestSolution/ref/pack_path+CM0.cprj");

}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_Local_Multiple_Pack_Files) {
  char* argv[7];
  StdStreamRedirect streamRedirect;
  const string warnExpected = "no pack loaded as multiple pdsc files found under: ../SolutionSpecificPack";
  const string& csolution = testinput_folder + "/TestSolution/test.csolution_local_pack_path.yml";

  // copy an additional pack file
  string srcPackFile, destPackFile;
  srcPackFile = testinput_folder + "/SolutionSpecificPack/ARM.RteTest_DFP.pdsc";
  destPackFile = testinput_folder + "/SolutionSpecificPack/ARM.RteTest_DFP_2.pdsc";
  if (RteFsUtils::Exists(destPackFile)) {
    RteFsUtils::RemoveFile(destPackFile);
  }
  RteFsUtils::CopyCheckFile(srcPackFile, destPackFile, false);

  // convert -s solution.yml
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-s";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv));

  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(warnExpected));

  // remove additionally added file
  RteFsUtils::RemoveFile(destPackFile);
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_Local_Pack_Path_Not_Found) {
  char* argv[7];
  StdStreamRedirect streamRedirect;
  const string errExpected = "pack path: ./SolutionSpecificPack/ARM does not exist";
  const string& csolution = testinput_folder + "/TestSolution/test.csolution_local_pack_path_not_found.yml";

  // convert -s solution.yml
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-s";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv));

  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(errExpected));
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_Local_Pack_File_Not_Found) {
  char* argv[7];
  StdStreamRedirect streamRedirect;
  const string errExpected = "no pdsc file found under: ../SolutionSpecificPack/Device";
  const string& csolution = testinput_folder + "/TestSolution/test.csolution_local_pack_file_not_found.yml";

  // convert -s solution.yml
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-s";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv));

  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(errExpected));
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_List_Board_Pack) {
  char* argv[7];
  const string& csolution = testinput_folder + "/TestSolution/test.csolution_list_board_package.yml";

  // convert -s solution.yml
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-s";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv));

  // Check generated CPRJs
  CompareFile(testoutput_folder + "/test1.Debug+CM0/test1.Debug+CM0.cprj",
    testinput_folder + "/TestSolution/ref/test1.Debug+CM0/test1.Debug+CM0_board_package.cprj");
  CompareFile(testoutput_folder + "/test1.Release+CM0/test1.Release+CM0.cprj",
    testinput_folder + "/TestSolution/ref/test1.Release+CM0/test1.Release+CM0_board_package.cprj");
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_GetCdefaultFile1) {
  // Valid file search
  const string& testdir = testoutput_folder + "/FindFileRegEx";
  const string& fileName = testdir + "/test.cdefault.yml";
  RteFsUtils::CreateDirectories(testdir);
  RteFsUtils::CreateFile(fileName, "");
  m_rootDir = testdir;
  m_cdefaultFile.clear();
  EXPECT_TRUE(GetCdefaultFile());
  EXPECT_EQ(fileName, m_cdefaultFile);
  RteFsUtils::RemoveDir(testdir);
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_GetCdefaultFile2) {
  // Multiple files in the search path
  const string& testdir = testinput_folder + "/TestDefault/multiple";
  m_rootDir = testdir;
  m_cdefaultFile.clear();
  EXPECT_FALSE(GetCdefaultFile());
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_GetCdefaultFile3) {
  // No cdefault file in the search path
  const string& testdir = testinput_folder + "/TestDefault/empty";
  m_rootDir = testdir;
  m_cdefaultFile.clear();
  EXPECT_FALSE(GetCdefaultFile());
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_ParseCdefault1) {
  // Valid cdefault file
  const string& validCdefaultFile = testinput_folder + "/TestDefault/.cdefault.yml";
  EXPECT_TRUE(m_parser.ParseCdefault(validCdefaultFile, true));
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_ParseCdefault2) {
  // Wrong cdefault file and schema check enabled
  const string& wrongCdefaultFile = testinput_folder + "/TestDefault/wrong/.cdefault.yml";
  EXPECT_FALSE(m_parser.ParseCdefault(wrongCdefaultFile, true));
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_ParseCdefault3) {
  // Wrong cdefault file and schema check disabled
  const string& wrongCdefaultFile = testinput_folder + "/TestDefault/wrong/.cdefault.yml";
  EXPECT_TRUE(m_parser.ParseCdefault(wrongCdefaultFile, false));
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_DefaultFile1) {
  char* argv[6];
  // convert -s solution.yml
  // csolution is empty, all information from cdefault.yml is taken
  const string& csolution = testinput_folder + "/TestDefault/empty.csolution.yml";
  const string& output = testoutput_folder + "/empty";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-s";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)output.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv));

  // Check generated CPRJs
  CompareFile(testoutput_folder + "/empty/project.Debug/project.Debug.cprj",
    testinput_folder + "/TestDefault/ref/empty/project.Debug/project.Debug.cprj");
  CompareFile(testoutput_folder + "/empty/project.Release/project.Release.cprj",
    testinput_folder + "/TestDefault/ref/empty/project.Release/project.Release.cprj");
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_DefaultFile2) {
  char* argv[6];
  // convert -s solution.yml
  // csolution.yml is complete, no information from cdefault.yml is taken
  const string& csolution = testinput_folder + "/TestDefault/full.csolution.yml";
  const string& output = testoutput_folder + "/full";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-s";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)output.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv));

  // Check generated CPRJs
  CompareFile(testoutput_folder + "/full/project.Debug/project.Debug.cprj",
    testinput_folder + "/TestDefault/ref/full/project.Debug/project.Debug.cprj");
  CompareFile(testoutput_folder + "/full/project.Release/project.Release.cprj",
    testinput_folder + "/TestDefault/ref/full/project.Release/project.Release.cprj");
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_DefaultFile3) {
  char* argv[6];
  // convert -s solution.yml
  // default build-types and default compiler are taken when applicable
  const string& csolution = testinput_folder + "/TestDefault/build-types.csolution.yml";
  const string& output = testoutput_folder + "/build-types";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-s";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)output.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv));

  // Check generated CPRJs
  CompareFile(testoutput_folder + "/build-types/project.Debug/project.Debug.cprj",
    testinput_folder + "/TestDefault/ref/build-types/project.Debug/project.Debug.cprj");
  CompareFile(testoutput_folder + "/build-types/project.Release/project.Release.cprj",
    testinput_folder + "/TestDefault/ref/build-types/project.Release/project.Release.cprj");
  CompareFile(testoutput_folder + "/build-types/project.AC6/project.AC6.cprj",
    testinput_folder + "/TestDefault/ref/build-types/project.AC6/project.AC6.cprj");
  CompareFile(testoutput_folder + "/build-types/project.IAR/project.IAR.cprj",
    testinput_folder + "/TestDefault/ref/build-types/project.IAR/project.IAR.cprj");
}

TEST_F(ProjMgrUnitTests, LoadPacks_MultiplePackSelection) {
  m_csolutionFile = testinput_folder + "/TestSolution/pack_contexts.csolution.yml";
  m_rootDir = fs::path(m_csolutionFile).parent_path().generic_string();
  EXPECT_TRUE(PopulateContexts());
  map<string, ContextItem>* contexts = nullptr;
  m_worker.GetContexts(contexts);
  for (auto& [contextName, contextItem] : *contexts) {
    EXPECT_TRUE(m_worker.ProcessContext(contextItem));
  }
}

TEST_F(ProjMgrUnitTests, LoadPacks_MissingPackSelection) {
  m_csolutionFile = testinput_folder + "/TestSolution/test.csolution_local_pack_path_not_found.yml";
  m_rootDir = fs::path(m_csolutionFile).parent_path().generic_string();
  EXPECT_TRUE(PopulateContexts());
  map<string, ContextItem>* contexts = nullptr;
  m_worker.GetContexts(contexts);
  for (auto& [contextName, contextItem] : *contexts) {
    EXPECT_FALSE(m_worker.ProcessContext(contextItem));
  }
}

TEST_F(ProjMgrUnitTests, ListDevices_MultiplePackSelection) {
  set<string> expected_CM0 = {
    "RteTest_ARMCM0",
    "RteTest_ARMCM0_Dual:cm0_core0",
    "RteTest_ARMCM0_Dual:cm0_core1",
    "RteTest_ARMCM0_Single",
    "RteTest_ARMCM0_Test"
  };
  set<string> expected_Gen = {
    "RteTestGen_ARMCM0"
  };
  vector<string> devices;
  m_csolutionFile = testinput_folder + "/TestSolution/pack_contexts.csolution.yml";
  m_rootDir = fs::path(m_csolutionFile).parent_path().generic_string();
  EXPECT_TRUE(PopulateContexts());
  EXPECT_TRUE(m_worker.ListDevices(devices, "pack_contexts+CM0", "CM0"));
  EXPECT_EQ(expected_CM0, set<string>(devices.begin(), devices.end()));
  devices.clear();
  EXPECT_TRUE(m_worker.ListDevices(devices, "pack_contexts+Gen", "CM0"));
  EXPECT_EQ(expected_Gen, set<string>(devices.begin(), devices.end()));
}

TEST_F(ProjMgrUnitTests, ListComponents_MultiplePackSelection) {
  set<string> expected_CM0 = {
    "ARM::Device:Startup&RteTest Startup@2.0.3 (ARM::RteTest_DFP@0.2.0)"
  };
  set<string> expected_Gen = {
    "ARM::Device:RteTest Generated Component:RteTest@1.1.0 (ARM::RteTestGenerator@0.1.0)",
    "ARM::Device:RteTest Generated Component:RteTestWithKey@1.1.0 (ARM::RteTestGenerator@0.1.0)"
  };
  vector<string> components;
  m_csolutionFile = testinput_folder + "/TestSolution/pack_contexts.csolution.yml";
  m_rootDir = fs::path(m_csolutionFile).parent_path().generic_string();
  EXPECT_TRUE(PopulateContexts());
  EXPECT_TRUE(m_worker.ListComponents(components, "pack_contexts+CM0", "Startup"));
  EXPECT_EQ(expected_CM0, set<string>(components.begin(), components.end()));
  components.clear();
  EXPECT_TRUE(m_worker.ListComponents(components, "pack_contexts+Gen"));
  EXPECT_EQ(expected_Gen, set<string>(components.begin(), components.end()));
}

TEST_F(ProjMgrUnitTests, Convert_ValidationResults_Dependencies) {
  char* argv[6];
  const string& csolution = testinput_folder + "/Validation/dependencies.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-s";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-c";

  map<string, string> testData = {
    {"selectable+CM0",           "warning csolution: dependency validation failed:\nSELECTABLE ARM::Device:Startup&RteTest Startup@2.0.3\n  require RteTest:CORE" },
    {"missing+CM0",              "warning csolution: dependency validation failed:\nMISSING ARM::RteTest:Check:Missing@0.9.9\n  require RteTest:Dependency:Missing" },
    {"conflict+CM0",             "warning csolution: dependency validation failed:\nCONFLICT RteTest:ApiExclusive@1.0.0\n  ARM::RteTest:ApiExclusive:S1\n  ARM::RteTest:ApiExclusive:S2" },
    {"incompatible+CM0",         "warning csolution: dependency validation failed:\nINCOMPATIBLE ARM::RteTest:Check:Incompatible@0.9.9\n  deny RteTest:Dependency:Incompatible_component" },
    {"incompatible-variant+CM0", "warning csolution: dependency validation failed:\nINCOMPATIBLE_VARIANT ARM::RteTest:Check:IncompatibleVariant@0.9.9\n  require RteTest:Dependency:Variant&Compatible" },
  };

  for (const auto& [context, expected] : testData) {
    StdStreamRedirect streamRedirect;
    argv[5] = (char*)context.c_str();
    EXPECT_EQ(0, RunProjMgr(6, argv));
    auto errorStr = streamRedirect.GetErrorString();
    EXPECT_EQ(0, errorStr.find(expected));
  }
}

TEST_F(ProjMgrUnitTests, Convert_ValidationResults_Filtering) {
  char* argv[6];
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-s";
  argv[4] = (char*)"-c";

  vector<tuple<string, int, string>> testData = {
    {"recursive", 1, "\
warning csolution: ARM.RteTestRecursive.0.1.0: condition 'Recursive': error #503: direct or indirect recursion detected\n\
error csolution: no component was found with identifier 'RteTest:Check:Recursive'\n"},
    {"missing-condition", 0, "\
warning csolution: ARM.RteTestMissingCondition.0.1.0: component 'ARM::RteTest.Check.MissingCondition(MissingCondition):0.9.9[]': error #501: error(s) in component definition:\n\
warning csolution:  condition 'MissingCondition' not found\n"},
  };

  for (const auto& [project, expectedReturn, expectedMessage] : testData) {
    StdStreamRedirect streamRedirect;
    const string& csolution = testinput_folder + "/Validation/" + project + ".csolution.yml";
    const string& context = project + "+CM0";
    argv[3] = (char*)csolution.c_str();
    argv[5] = (char*)context.c_str();
    EXPECT_EQ(expectedReturn, RunProjMgr(6, argv));
    auto errorStr = streamRedirect.GetErrorString();
    EXPECT_EQ(0, errorStr.find(expectedMessage));
  }
}
