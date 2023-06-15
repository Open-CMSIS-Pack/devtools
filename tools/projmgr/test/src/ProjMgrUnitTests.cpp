/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgr.h"
#include "ProjMgrTestEnv.h"
#include "ProjMgrYamlSchemaChecker.h"

#include "RteFsUtils.h"

#include "CrossPlatformUtils.h"

#include "gtest/gtest.h"
#include "yaml-cpp/yaml.h"

#include <fstream>
#include <regex>

using namespace std;

bool shouldHaveGeneratorForHostType(const string& hostType) {
  return hostType == "linux" || hostType == "win" || hostType == "mac";
}


class ProjMgrUnitTests : public ProjMgr, public ::testing::Test {
protected:
  ProjMgrUnitTests() {}
  virtual ~ProjMgrUnitTests() {}

  void SetUp() { m_context.clear(); };
  void GetFilesInTree(const string& dir, set<string>& files);
  void CompareFileTree(const string& dir1, const string& dir2);

  string UpdateTestSolutionFile(const string& projectFilePath);
};

string ProjMgrUnitTests::UpdateTestSolutionFile(const string& projectFilePath) {
  string csolutionFile = testinput_folder + "/TestSolution/test_validate_project.csolution.yml";
  YAML::Node root = YAML::LoadFile(csolutionFile);
  root["solution"]["projects"][0]["project"] = projectFilePath;
  std::ofstream fout(csolutionFile);
  fout << root;
  fout.close();
  return csolutionFile;
}

void ProjMgrUnitTests::GetFilesInTree(const string& dir, set<string>& files) {
  error_code ec;
  if (RteFsUtils::Exists(dir)) {
    for (auto& p : fs::recursive_directory_iterator(dir, ec)) {
      files.insert(p.path().filename().generic_string());
    }
  }
}

void ProjMgrUnitTests::CompareFileTree(const string& dir1, const string& dir2) {
  set<string> tree1, tree2;
  GetFilesInTree(dir1, tree1);
  GetFilesInTree(dir2, tree2);
  EXPECT_EQ(tree1, tree2);
}

TEST_F(ProjMgrUnitTests, RunProjMgr_EmptyOptions) {
  char* argv[1];
  // Empty options
  EXPECT_EQ(0, RunProjMgr(1, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Version) {
  char* argv[2];
  // version
  argv[1] = (char*)"--version";
  EXPECT_EQ(0, RunProjMgr(2, argv, 0));

  argv[1] = (char*)"-V";
  EXPECT_EQ(0, RunProjMgr(2, argv, 0));
}


TEST_F(ProjMgrUnitTests, RunProjMgr_ListPacks) {
  char* argv[7];
  map<std::pair<string, string>, string> testInputs = {
    {{"TestSolution/test.csolution.yml", "test1.Debug+CM0"},
      "ARM::RteTest_DFP@0.2.0 \\(.*\\)\n" },
      // packs are specified only with vendor
    {{"TestSolution/test_filtered_pack_selection.csolution.yml", "test1.Debug+CM0"},
      "ARM::RteTest@0.1.0 \\(.*\\)\nARM::RteTestBoard@0.1.0 \\(.*\\)\nARM::RteTestGenerator@0.1.0 \\(.*\\)\nARM::RteTest_DFP@0.2.0 \\(.*\\)\n"},
      // packs are specified with wildcards
    {{"TestSolution/test_filtered_pack_selection.csolution.yml", "test1.Release+CM0"},
      "ARM::RteTest_DFP@0.2.0 \\(.*\\)\n"},
      // packs are not specified
    {{"TestSolution/test_no_packs.csolution.yml", "test1.Debug+CM0"},
      "ARM::RteTest@0.1.0 \\(.*\\)\nARM::RteTestBoard@0.1.0 \\(.*\\)\nARM::RteTestGenerator@0.1.0 \\(.*\\)\nARM::RteTest_DFP@0.2.0 \\(.*\\)\n"},
      // packs are fully specified
    {{"TestSolution/test_pack_selection.csolution.yml", "test2.Debug+CM0"},
      "ARM::RteTest_DFP@0.2.0 \\(.*\\)\n"}
  };

  // positive tests
  argv[1] = (char*)"list";
  argv[2] = (char*)"packs";
  argv[3] = (char*)"--solution";
  for (const auto& [input, expected] : testInputs) {
    StdStreamRedirect streamRedirect;
    const string& csolution = testinput_folder + "/" + input.first;
    argv[4] = (char*)csolution.c_str();
    argv[5] = (char*)"-c";
    argv[6] = (char*)input.second.c_str();
    EXPECT_EQ(0, RunProjMgr(7, argv, 0));

    auto outStr = streamRedirect.GetOutString();
    EXPECT_TRUE(regex_match(outStr, regex(expected.c_str()))) << "error listing pack for " << csolution << endl;
  }

  map<std::pair<string, string>, string> testFalseInputs = {
    {{"TestSolution/test.csolution_unknown_file.yml", "test1.Debug+CM0"},
      "error csolution: csolution file was not found"},
    {{"TestSolution/test.csolution.yml", "invalid.context"},
      "following context(s) was not found:\n  invalid.context"}
  };
  // negative tests
  for (const auto& [input, expected] : testFalseInputs) {
    StdStreamRedirect streamRedirect;
    const string& csolution = testinput_folder + "/" + input.first;
    argv[4] = (char*)csolution.c_str();
    argv[5] = (char*)"-c";
    argv[6] = (char*)input.second.c_str();
    EXPECT_EQ(1, RunProjMgr(7, argv, 0));

    auto errStr = streamRedirect.GetErrorString();
    EXPECT_NE(string::npos, errStr.find(expected)) << "error listing pack for " << csolution << endl;
  }
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListPacks_1) {
  char* argv[3];
  StdStreamRedirect streamRedirect;
  const string& expected = "ARM::RteTest@0.1.0 \\(.*\\)\nARM::RteTestBoard@0.1.0 \\(.*\\)\nARM::RteTestGenerator@0.1.0 \\(.*\\)\nARM::RteTest_DFP@0.2.0 \\(.*\\)\n";
  // list packs
  argv[1] = (char*)"list";
  argv[2] = (char*)"packs";
  EXPECT_EQ(0, RunProjMgr(3, argv, 0));

  auto outStr = streamRedirect.GetOutString();
  EXPECT_TRUE(regex_match(outStr, regex(expected.c_str())));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListPacks_project) {
  char* argv[7];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestDefault/test.csolution.yml";
  const std::string rteFolder = testinput_folder + "/TestDefault/RTE";
  set<string> rteFilesBefore, rteFilesAfter;
  GetFilesInTree(rteFolder, rteFilesBefore);

  // list packs
  argv[1] = (char*)"list";
  argv[2] = (char*)"packs";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  argv[5] = (char*)"-c";
  argv[6] = (char*)"project.Debug+TEST_TARGET";
  EXPECT_EQ(0, RunProjMgr(7, argv, 0));

  GetFilesInTree(rteFolder, rteFilesAfter);
  EXPECT_EQ(rteFilesBefore, rteFilesAfter);

  auto outStr = streamRedirect.GetOutString();
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(regex_match(outStr, regex("ARM::RteTest_DFP@0.1.1 \\(.*\\)\n")));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListPacks_MultiContext) {
  char* argv[9];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestSolution/test_pack_selection.csolution.yml";
  // list packs
  argv[1] = (char*)"list";
  argv[2] = (char*)"packs";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  argv[5] = (char*)"-c";
  argv[6] = (char*)"test2.*";
  EXPECT_EQ(0, RunProjMgr(7, argv, 0));

  auto outStr = streamRedirect.GetOutString();
  EXPECT_TRUE(regex_match(outStr.c_str(), regex("ARM::RteTestGenerator@0.1.0 \\(.*\\)\nARM::RteTest_DFP@0.2.0 \\(.*\\)\n")));

  argv[7] = (char*)"-l";
  argv[8] = (char*)"latest";
  streamRedirect.ClearStringStreams();
  EXPECT_EQ(0, RunProjMgr(9, argv, 0));

  const string& expectedLatest = "\
ARM::RteTest@0.1.0 \\(.*\\)\n\
ARM::RteTestBoard@0.1.0 \\(.*\\)\n\
ARM::RteTestGenerator@0.1.0 \\(.*\\)\n\
ARM::RteTest_DFP@0.2.0 \\(.*\\)\n\
";

  outStr = streamRedirect.GetOutString();
  EXPECT_TRUE(regex_match(outStr, regex(expectedLatest)));

  argv[7] = (char*)"-l";
  argv[8] = (char*)"all";
  streamRedirect.ClearStringStreams();
  EXPECT_EQ(0, RunProjMgr(9, argv, 0));

  const string& expectedAll = "\
ARM::RteTest@0.1.0 \\(.*\\)\n\
ARM::RteTestBoard@0.1.0 \\(.*\\)\n\
ARM::RteTestGenerator@0.1.0 \\(.*\\)\n\
ARM::RteTest_DFP@0.1.1 \\(.*\\)\n\
ARM::RteTest_DFP@0.2.0 \\(.*\\)\n\
";

  outStr = streamRedirect.GetOutString();
  EXPECT_TRUE(regex_match(outStr, regex(expectedAll)));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListPacksAll) {
  char* argv[5];
  StdStreamRedirect streamRedirect;
  argv[1] = (char*)"list";
  argv[2] = (char*)"packs";
  argv[3] = (char*)"-l";
  argv[4] = (char*)"all";
  EXPECT_EQ(0, RunProjMgr(5, argv, 0));

  const string& expectedAll = "\
ARM::RteTest@0.1.0 \\(.*\\)\n\
ARM::RteTestBoard@0.1.0 \\(.*\\)\n\
ARM::RteTestGenerator@0.1.0 \\(.*\\)\n\
ARM::RteTest_DFP@0.1.1 \\(.*\\)\n\
ARM::RteTest_DFP@0.2.0 \\(.*\\)\n\
";

  auto outStr = streamRedirect.GetOutString();
  EXPECT_TRUE(regex_match(outStr, regex(expectedAll)));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListPacksMissing) {
  char* argv[8];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestSolution/pack_missing.csolution.yml";
  // list packs
  argv[1] = (char*)"list";
  argv[2] = (char*)"packs";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  argv[5] = (char*)"-c";
  argv[6] = (char*)"test1+CM0";
  argv[7] = (char*)"-m";
  EXPECT_EQ(0, RunProjMgr(8, argv, 0));

  auto outStr = streamRedirect.GetOutString();
  EXPECT_STREQ(outStr.c_str(), "ARM::Missing_DFP@0.0.9\n");
}

TEST_F(ProjMgrUnitTests, ListPacks_ProjectAndLayer) {
  char* argv[5];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestLayers/packs.csolution.yml";
  const string& expected = "ARM::RteTest@0.1.0 \\(.*\\)\nARM::RteTestBoard@0.1.0 \\(.*\\)\nARM::RteTest_DFP@0.2.0 \\(.*\\)\n";
  // list packs
  argv[1] = (char*)"list";
  argv[2] = (char*)"packs";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  EXPECT_EQ(0, RunProjMgr(5, argv, 0));

  auto outStr = streamRedirect.GetOutString();
  EXPECT_TRUE(regex_match(outStr, regex(expected.c_str())));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListBoards) {
  char* argv[5];
  StdStreamRedirect streamRedirect;

  // list boards
  argv[1] = (char*)"list";
  argv[2] = (char*)"boards";
  argv[3] = (char*)"--filter";
  argv[4] = (char*)"Dummy";
  EXPECT_EQ(0, RunProjMgr(5, argv, 0));

  auto outStr = streamRedirect.GetOutString();
  EXPECT_STREQ(outStr.c_str(), "Keil::RteTest Dummy board:1.2.3 (ARM::RteTest_DFP@0.2.0)\n");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListBoardsProjectFiltered) {
  char* argv[7];
  StdStreamRedirect streamRedirect;
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_board_and_device.cproject.yml");
  const std::string rteFolder = testinput_folder + "/TestProject/RTE";
  set<string> rteFilesBefore, rteFilesAfter;
  GetFilesInTree(rteFolder, rteFilesBefore);

  // list boards
  argv[1] = (char*)"list";
  argv[2] = (char*)"boards";
  argv[3] = (char*)"--filter";
  argv[4] = (char*)"Dummy";
  argv[5] = (char*)"--solution";
  argv[6] = (char*)csolutionFile.c_str();
  EXPECT_EQ(0, RunProjMgr(7, argv, 0));

  GetFilesInTree(rteFolder, rteFilesAfter);
  EXPECT_EQ(rteFilesBefore, rteFilesAfter);

  auto outStr = streamRedirect.GetOutString();
  EXPECT_STREQ(outStr.c_str(), "Keil::RteTest Dummy board:1.2.3 (ARM::RteTest_DFP@0.2.0)\n");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListDevices) {
  char* argv[5];
  StdStreamRedirect streamRedirect;

  // list devices
  argv[1] = (char*)"list";
  argv[2] = (char*)"devices";
  argv[3] = (char*)"--filter";
  argv[4] = (char*)"RteTest_ARMCM4";
  EXPECT_EQ(0, RunProjMgr(5, argv, 0));

  auto outStr = streamRedirect.GetOutString();
  EXPECT_STREQ(outStr.c_str(),"\
ARM::RteTest_ARMCM4 (ARM::RteTest_DFP@0.2.0)\n\
ARM::RteTest_ARMCM4_FP (ARM::RteTest_DFP@0.2.0)\n\
ARM::RteTest_ARMCM4_NOFP (ARM::RteTest_DFP@0.2.0)\n"
  );
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListComponents) {
  char* argv[3];

  // list components
  argv[1] = (char*)"list";
  argv[2] = (char*)"components";
  EXPECT_EQ(0, RunProjMgr(3, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListDependencies) {
  char* argv[6];
  const std::string expectedOut = "ARM::Device:Startup&RteTest Startup@2.0.3 require RteTest:CORE\n";
  const std::string expectedErr = "warning csolution: RTE Model reports:\n";
  StdStreamRedirect streamRedirect;
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test-dependency.cproject.yml");
  const std::string rteFolder = testinput_folder + "/TestProject/RTE";
  set<string> rteFilesBefore, rteFilesAfter;
  GetFilesInTree(rteFolder, rteFilesBefore);

  // list dependencies
  argv[1] = (char*)"list";
  argv[2] = (char*)"dependencies";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolutionFile.c_str();
  argv[5] = (char*)"-d";
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  GetFilesInTree(rteFolder, rteFilesAfter);
  EXPECT_EQ(rteFilesBefore, rteFilesAfter);

  auto outStr = streamRedirect.GetOutString();
  EXPECT_STREQ(outStr.c_str(), expectedOut.c_str());

  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(errStr.find(expectedErr), string::npos);
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ConvertProject) {
  char* argv[6];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test.cproject.yml");

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  // Check generated CPRJ
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test+TEST_TARGET.cprj",
    testinput_folder + "/TestSolution/TestProject4/test+TEST_TARGET.cprj");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_EnforcedComponent) {
  char* argv[6];
  string csolutionFile = testinput_folder + "/TestSolution/test_enforced_component.csolution.yml";

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  // Check generated CPRJ
  //ProjMgrTestEnv::CompareFile(testoutput_folder + "/test.cprj",
  //  testinput_folder + "/TestSolution/TestProject4/test.cprj");
}


TEST_F(ProjMgrUnitTests, RunProjMgr_LinkerScript) {
  char* argv[6];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_linker_script.cproject.yml");

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  // Check generated CPRJ
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test_linker_script+TEST_TARGET.cprj",
    testinput_folder + "/TestSolution/TestProject4/test_linker_script+TEST_TARGET.cprj");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_With_Schema_Check) {
  char* argv[6];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_invalid_schema.cproject.yml");
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Skip_Schema_Check) {
  char* argv[7];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_invalid_schema.cproject.yml");
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"-n";
  EXPECT_EQ(0, RunProjMgr(7, argv, 0));

  // Check generated CPRJ
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test+TEST_TARGET.cprj",
    testinput_folder + "/TestSolution/TestProject4/test+TEST_TARGET.cprj");
}

TEST_F(ProjMgrUnitTests, RunProjMgrContextSolution) {
  char* argv[7];
  StdStreamRedirect streamRedirect;

  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestSolution/test.csolution.yml";
  argv[1] = (char*)"list";
  argv[2] = (char*)"contexts";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  argv[5] = (char*)"--filter";
  argv[6] = (char*)"test1";
  EXPECT_EQ(0, RunProjMgr(7, argv, 0));

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
  EXPECT_EQ(1, RunProjMgr(5, argv, 0));

  // list empty arguments
  EXPECT_EQ(1, RunProjMgr(2, argv, 0));
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

  EXPECT_EQ(1, RunProjMgr(7, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution) {
  char* argv[7];

  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestSolution/test.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  // Check generated CPRJs
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test1.Debug+CM0.cprj",
    testinput_folder + "/TestSolution/ref/test1.Debug+CM0.cprj");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test1.Release+CM0.cprj",
    testinput_folder + "/TestSolution/ref/test1.Release+CM0.cprj");

 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test2.Debug+CM0.cprj",
    testinput_folder + "/TestSolution/ref/test2.Debug+CM0.cprj");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test2.Debug+CM3.cprj",
    testinput_folder + "/TestSolution/ref/test2.Debug+CM3.cprj");

  // Check generated cbuild YMLs
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test.cbuild-idx.yml",
    testinput_folder + "/TestSolution/ref/cbuild/test.cbuild-idx.yml");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test1.Debug+CM0.cbuild.yml",
    testinput_folder + "/TestSolution/ref/cbuild/test1.Debug+CM0.cbuild.yml");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test1.Release+CM0.cbuild.yml",
    testinput_folder + "/TestSolution/ref/cbuild/test1.Release+CM0.cbuild.yml");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test2.Debug+CM0.cbuild.yml",
    testinput_folder + "/TestSolution/ref/cbuild/test2.Debug+CM0.cbuild.yml");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test2.Debug+CM3.cbuild.yml",
    testinput_folder + "/TestSolution/ref/cbuild/test2.Debug+CM3.cbuild.yml");
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_PositionalArguments) {
  char* argv[6];
  const string& csolution = testinput_folder + "/TestSolution/test.csolution.yml";
  argv[1] = (char*)csolution.c_str();
  argv[2] = (char*)"list";
  argv[3] = (char*)"devices";
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  argv[1] = (char*)"list";
  argv[2] = (char*)"devices";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  argv[1] = (char*)"list";
  argv[2] = (char*)"devices";
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();
  argv[5] = (char*)csolution.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  argv[1] = (char*)"-o";
  argv[2] = (char*)testoutput_folder.c_str();
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"list";
  argv[5] = (char*)"devices";
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  argv[1] = (char*)"-o";
  argv[2] = (char*)testoutput_folder.c_str();
  argv[3] = (char*)"list";
  argv[4] = (char*)"devices";
  argv[5] = (char*)csolution.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolutionContext) {
  char* argv[8];
  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestSolution/test.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"-c";
  argv[7] = (char*)"test2.Debug+CM0";
  EXPECT_EQ(0, RunProjMgr(8, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolutionNonExistentContext) {
  char* argv[8];
  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestSolution/test.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"-c";
  argv[7] = (char*)"NON-EXISTENT-CONTEXT";
  EXPECT_EQ(1, RunProjMgr(8, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_InvalidLayerSchema) {
  char* argv[7];

  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestLayers/testlayers_invalid_layer.csolution.yml";
  const string& output = testoutput_folder + "/testlayers";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)output.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_UnknownLayer) {
  char* argv[7];
  const string& csolution = testinput_folder + "/TestLayers/testlayers_invalid_layer.csolution.yml";
  const string& output = testoutput_folder + "/testlayers";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)output.c_str();
  argv[6] = (char*)"-n";
  EXPECT_EQ(1, RunProjMgr(7, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgrLayers) {
  char* argv[7];

  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestLayers/testlayers.csolution.yml";
  const string& output = testoutput_folder + "/testlayers";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)output.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  // Check generated CPRJs
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/testlayers/testlayers.Debug+TEST_TARGET.cprj",
    testinput_folder + "/TestLayers/ref/testlayers/testlayers.Debug+TEST_TARGET.cprj");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/testlayers/testlayers.Release+TEST_TARGET.cprj",
    testinput_folder + "/TestLayers/ref/testlayers/testlayers.Release+TEST_TARGET.cprj");

  // Check creation of layers rte folders
  EXPECT_TRUE(RteFsUtils::Exists(testinput_folder + "/TestLayers/Layer2/RTE/Device/RteTest_ARMCM0"));
  EXPECT_TRUE(RteFsUtils::Exists(testinput_folder + "/TestLayers/Layer3/RTE/RteTest/MyDir"));
}

TEST_F(ProjMgrUnitTests, RunProjMgrLayers2) {
  char* argv[4];

  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestLayers/testlayers.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  EXPECT_EQ(0, RunProjMgr(4, argv, 0));

  // Check generated CPRJs
 ProjMgrTestEnv:: CompareFile(testinput_folder + "/TestLayers/testlayers.Debug+TEST_TARGET.cprj",
    testinput_folder + "/TestLayers/ref2/testlayers.Debug+TEST_TARGET.cprj");
 ProjMgrTestEnv:: CompareFile(testinput_folder + "/TestLayers/testlayers.Release+TEST_TARGET.cprj",
    testinput_folder + "/TestLayers/ref2/testlayers.Release+TEST_TARGET.cprj");
}

TEST_F(ProjMgrUnitTests, ListLayersAll) {
  StdStreamRedirect streamRedirect;
  char* argv[3];
  argv[1] = (char*)"list";
  argv[2] = (char*)"layers";
  EXPECT_EQ(0, RunProjMgr(3, argv, 0));

  const string& expected = "\
.*/ARM/RteTest_DFP/0.2.0/Layers/board1.clayer.yml \\(layer type: Board\\)\n\
.*/ARM/RteTest_DFP/0.2.0/Layers/board2.clayer.yml \\(layer type: Board\\)\n\
.*/ARM/RteTest_DFP/0.2.0/Layers/board3.clayer.yml \\(layer type: Board\\)\n\
.*/ARM/RteTest_DFP/0.2.0/Layers/config1.clayer.yml \\(layer type: Config1\\)\n\
.*/ARM/RteTest_DFP/0.2.0/Layers/config2.clayer.yml \\(layer type: Config2\\)\n\
.*/ARM/RteTest_DFP/0.2.0/Layers/incompatible.clayer.yml \\(layer type: Incompatible\\)\n\
.*/ARM/RteTest_DFP/0.2.0/Layers/pdsc-type-mismatch.clayer.yml \\(layer type: PdscType\\)\n\
.*/ARM/RteTest_DFP/0.2.0/Layers/testvariant.clayer.yml \\(layer type: TestVariant\\)\n\
";

  const string& outStr = streamRedirect.GetOutString();
  EXPECT_TRUE(regex_match(outStr, regex(expected)));
}

TEST_F(ProjMgrUnitTests, ListLayersCompatible) {
  StdStreamRedirect streamRedirect;
  char* argv[8];
  const string& csolution = testinput_folder + "/TestLayers/genericlayers.csolution.yml";
  const string& context = "genericlayers.CompatibleLayers+AnyBoard";
  argv[1] = (char*)"list";
  argv[2] = (char*)"layers";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  argv[5] = (char*)"-c";
  argv[6] = (char*)context.c_str();
  argv[7] = (char*)"-d";
  EXPECT_EQ(0, RunProjMgr(8, argv, 0));

  const string& expectedErrStr = "\
debug csolution: check for context 'genericlayers.CompatibleLayers\\+AnyBoard'\n\
\n\
check combined connections:\n\
  .*/TestLayers/genericlayers.cproject.yml\n\
    \\(Project Connections\\)\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/board1.clayer.yml \\(layer type: Board\\)\n\
    \\(Board1 Connections\\)\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/testvariant.clayer.yml \\(layer type: TestVariant\\)\n\
    \\(Test variant Connections\\)\n\
connections are valid\n\
\n\
check combined connections:\n\
  .*/TestLayers/genericlayers.cproject.yml\n\
    \\(Project Connections\\)\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/board2.clayer.yml \\(layer type: Board\\)\n\
    \\(Board2 Connections\\)\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/testvariant.clayer.yml \\(layer type: TestVariant\\)\n\
    \\(Test variant Connections\\)\n\
connections are valid\n\
\n\
check combined connections:\n\
  .*/TestLayers/genericlayers.cproject.yml\n\
    \\(Project Connections\\)\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/board3.clayer.yml \\(layer type: Board\\)\n\
    \\(Board3 Connections\\)\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/testvariant.clayer.yml \\(layer type: TestVariant\\)\n\
    \\(Test variant Connections\\)\n\
connections are valid\n\
\n\
multiple clayers match type 'Board':\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/board1.clayer.yml\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/board2.clayer.yml\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/board3.clayer.yml\n\
\n\
clayer of type 'TestVariant' was uniquely found:\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/testvariant.clayer.yml\n\
\n\
";

  const string& errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(regex_match(errStr, regex(expectedErrStr)));

  const string& expectedOutStr = "\
info csolution: valid configuration #1: \\(context 'genericlayers.CompatibleLayers\\+AnyBoard'\\)\n\
  .*/TestLayers/genericlayers.cproject.yml\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/board1.clayer.yml \\(layer type: Board\\)\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/testvariant.clayer.yml \\(layer type: TestVariant\\)\n\
\n\
info csolution: valid configuration #2: \\(context 'genericlayers.CompatibleLayers\\+AnyBoard'\\)\n\
  .*/TestLayers/genericlayers.cproject.yml\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/board2.clayer.yml \\(layer type: Board\\)\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/testvariant.clayer.yml \\(layer type: TestVariant\\)\n\
\n\
info csolution: valid configuration #3: \\(context 'genericlayers.CompatibleLayers\\+AnyBoard'\\)\n\
  .*/TestLayers/genericlayers.cproject.yml\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/board3.clayer.yml \\(layer type: Board\\)\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/testvariant.clayer.yml \\(layer type: TestVariant\\)\n\
\n\
.*/ARM/RteTest_DFP/0.2.0/Layers/board1.clayer.yml \\(layer type: Board\\)\n\
.*/ARM/RteTest_DFP/0.2.0/Layers/board2.clayer.yml \\(layer type: Board\\)\n\
.*/ARM/RteTest_DFP/0.2.0/Layers/board3.clayer.yml \\(layer type: Board\\)\n\
.*/ARM/RteTest_DFP/0.2.0/Layers/testvariant.clayer.yml \\(layer type: TestVariant\\)\n\
";

  const string& outStr = streamRedirect.GetOutString();
  EXPECT_TRUE(regex_match(outStr, regex(expectedOutStr)));
}

TEST_F(ProjMgrUnitTests, ListLayersConfigurations) {
  StdStreamRedirect streamRedirect;
  char* argv[6];
  const string& csolution = testinput_folder + "/TestLayers/config.csolution.yml";
  argv[1] = (char*)"list";
  argv[2] = (char*)"layers";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  argv[5] = (char*)"-v";
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  const string& expectedOutStr = "\
info csolution: valid configuration #1: \\(context 'config.CompatibleLayers\\+RteTest_ARMCM3'\\)\n\
  .*/TestLayers/config.clayer.yml\n\
    set: set1.select1 \\(connect R - set 1 select 1\\)\n\
  .*/TestLayers/config.cproject.yml\n\
    set: set1.select1 \\(project X - set 1 select 1\\)\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/config1.clayer.yml \\(layer type: Config1\\)\n\
    set: set1.select1 \\(connect A - set 1 select 1\\)\n\
    set: set2.select1 \\(connect C - set 2 select 1\\)\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/config2.clayer.yml \\(layer type: Config2\\)\n\
    set: set1.select1 \\(connect F - set 1 select 1\\)\n\
\n\
info csolution: valid configuration #2: \\(context 'config.CompatibleLayers\\+RteTest_ARMCM3'\\)\n\
  .*/TestLayers/config.clayer.yml\n\
    set: set1.select2 \\(connect S - set 1 select 2\\)\n\
  .*/TestLayers/config.cproject.yml\n\
    set: set1.select2 \\(project Y - set 1 select 2\\)\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/config1.clayer.yml \\(layer type: Config1\\)\n\
    set: set1.select2 \\(connect B - set 1 select 2\\)\n\
    set: set2.select2 \\(connect D - set 2 select 2\\)\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/config2.clayer.yml \\(layer type: Config2\\)\n\
    set: set1.select2 \\(connect G - set 1 select 2\\)\n\
\n\
.*/TestLayers/config.clayer.yml\n\
  set: set1.select1 \\(connect R - set 1 select 1\\)\n\
  set: set1.select2 \\(connect S - set 1 select 2\\)\n\
.*/ARM/RteTest_DFP/0.2.0/Layers/config1.clayer.yml \\(layer type: Config1\\)\n\
  set: set1.select1 \\(connect A - set 1 select 1\\)\n\
  set: set1.select2 \\(connect B - set 1 select 2\\)\n\
  set: set2.select1 \\(connect C - set 2 select 1\\)\n\
  set: set2.select2 \\(connect D - set 2 select 2\\)\n\
.*/ARM/RteTest_DFP/0.2.0/Layers/config2.clayer.yml \\(layer type: Config2\\)\n\
  set: set1.select1 \\(connect F - set 1 select 1\\)\n\
  set: set1.select2 \\(connect G - set 1 select 2\\)\n\
";

  const string& outStr = streamRedirect.GetOutString();
  EXPECT_TRUE(regex_match(outStr, regex(expectedOutStr)));
}

TEST_F(ProjMgrUnitTests, ListLayersMultipleSelect) {
  StdStreamRedirect streamRedirect;
  char* argv[6];
  const string& csolution = testinput_folder + "/TestLayers/select.csolution.yml";
  argv[1] = (char*)"list";
  argv[2] = (char*)"layers";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  argv[5] = (char*)"-v";
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  const string& expectedOutStr = "\
info csolution: valid configuration #1: \\(context 'select\\+RteTest_ARMCM3'\\)\n\
  .*/TestLayers/select.clayer.yml\n\
  .*/TestLayers/select.cproject.yml\n\
    set: set1.select1 \\(project X - set 1 select 1\\)\n\
\n\
info csolution: valid configuration #2: \\(context 'select\\+RteTest_ARMCM3'\\)\n\
  .*/TestLayers/select.clayer.yml\n\
  .*/TestLayers/select.cproject.yml\n\
    set: set1.select2 \\(project Y - set 1 select 2\\)\n\
    set: set1.select2 \\(project Z - set 1 select 2\\)\n\
\n\
.*/TestLayers/select.clayer.yml\n\
";

  const string& outStr = streamRedirect.GetOutString();
  EXPECT_TRUE(regex_match(outStr, regex(expectedOutStr)));
}

TEST_F(ProjMgrUnitTests, ListToolchains) {
  StdStreamRedirect streamRedirect;
  char* envp[4];
  string ac6 = "AC6_TOOLCHAIN_6_18_1=" + testinput_folder;
  string gcc = "GCC_TOOLCHAIN_11_3_1=" + testinput_folder;
  string iar = "IAR_TOOLCHAIN_9_32_5=" + testinput_folder;
  envp[0] = (char*)ac6.c_str();
  envp[1] = (char*)iar.c_str();
  envp[2] = (char*)gcc.c_str();
  envp[3] = (char*)'\0';
  char* argv[3];
  argv[1] = (char*)"list";
  argv[2] = (char*)"toolchains";
  EXPECT_EQ(0, RunProjMgr(3, argv, envp));

  const string& expectedOutStr = "\
AC6@6.18.1\n\
GCC@11.3.1\n\
IAR@9.32.5\n\
";

  const string& outStr = streamRedirect.GetOutString();
  EXPECT_TRUE(regex_match(outStr, regex(expectedOutStr)));
}

TEST_F(ProjMgrUnitTests, ListToolchainsVerbose) {
  StdStreamRedirect streamRedirect;
  char* envp[4];
  string ac6 = "AC6_TOOLCHAIN_6_18_1=" + testinput_folder;
  string gcc = "GCC_TOOLCHAIN_11_3_1=" + testinput_folder;
  string iar = "IAR_TOOLCHAIN_9_32_5=" + testinput_folder;
  envp[0] = (char*)ac6.c_str();
  envp[1] = (char*)iar.c_str();
  envp[2] = (char*)gcc.c_str();
  envp[3] = (char*)'\0';
  char* argv[4];
  argv[1] = (char*)"list";
  argv[2] = (char*)"toolchains";
  argv[3] = (char*)"-v";

  // Test listing registered toolchains in verbose mode
  EXPECT_EQ(0, RunProjMgr(4, argv, envp));

  const string& expectedOutStr = "\
AC6@6.18.1\n\
  Environment: AC6_TOOLCHAIN_6_18_1\n\
  Toolchain: .*/data\n\
  Configuration: .*/data/TestToolchains/AC6.6.18.0.cmake\n\
GCC@11.3.1\n\
  Environment: GCC_TOOLCHAIN_11_3_1\n\
  Toolchain: .*/data\n\
  Configuration: .*/data/TestToolchains/GCC.11.2.1.cmake\n\
IAR@9.32.5\n\
  Environment: IAR_TOOLCHAIN_9_32_5\n\
  Toolchain: .*/data\n\
  Configuration: .*/data/TestToolchains/IAR.8.50.6.cmake\n\
";

  const string& outStr = streamRedirect.GetOutString();
  EXPECT_TRUE(regex_match(outStr, regex(expectedOutStr)));
}

TEST_F(ProjMgrUnitTests, ListToolchainsSolution) {
  StdStreamRedirect streamRedirect;
  char* envp[3];
  string ac6 = "AC6_TOOLCHAIN_6_18_1=" + testinput_folder;
  string gcc = "GCC_TOOLCHAIN_11_3_1=" + testinput_folder;
  envp[0] = (char*)ac6.c_str();
  envp[1] = (char*)gcc.c_str();
  envp[2] = (char*)'\0';
  char* argv[5];
  const string& csolution = testinput_folder + "/TestSolution/toolchain.csolution.yml";
  argv[1] = (char*)"list";
  argv[2] = (char*)"toolchains";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();

  // Test listing required toolchains
  EXPECT_EQ(0, RunProjMgr(5, argv, envp));
  const string& expected = "AC6@>=0.0.0\nAC6@>=6.18.0\nGCC@11.3.1\n";
  const string& outStr = streamRedirect.GetOutString();
  EXPECT_EQ(outStr, expected);

  // Test with no registered toolchains (empty environment variables)
  streamRedirect.ClearStringStreams();
  EXPECT_EQ(1, RunProjMgr(5, argv, 0));
  const string& expected2 = "AC6@>=0.0.0\nAC6@>=6.18.0\nGCC@11.3.1\n";
  const string& outStr2 = streamRedirect.GetOutString();
  EXPECT_EQ(outStr2, expected2);
}

TEST_F(ProjMgrUnitTests, ListLayersUniquelyCompatibleBoard) {
  StdStreamRedirect streamRedirect;
  char* argv[8];
  const string& csolution = testinput_folder + "/TestLayers/genericlayers.csolution.yml";
  const string& context = "genericlayers.CompatibleLayers+Board3";
  argv[1] = (char*)"list";
  argv[2] = (char*)"layers";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  argv[5] = (char*)"-c";
  argv[6] = (char*)context.c_str();
  argv[7] = (char*)"-d";
  EXPECT_EQ(0, RunProjMgr(8, argv, 0));

  const string& expectedErrStr = "\
debug csolution: check for context 'genericlayers.CompatibleLayers\\+Board3'\n\
\n\
check combined connections:\n\
  .*/TestLayers/genericlayers.cproject.yml\n\
    \\(Project Connections\\)\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/board3.clayer.yml \\(layer type: Board\\)\n\
    \\(Board3 Connections\\)\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/testvariant.clayer.yml \\(layer type: TestVariant\\)\n\
    \\(Test variant Connections\\)\n\
connections are valid\n\
\n\
clayer of type 'Board' was uniquely found:\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/board3.clayer.yml\n\
\n\
clayer of type 'TestVariant' was uniquely found:\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/testvariant.clayer.yml\n\
\n\
";

  const string& errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(regex_match(errStr, regex(expectedErrStr)));

  const string& expectedOutStr = "\
info csolution: valid configuration #1: \\(context 'genericlayers.CompatibleLayers\\+Board3'\\)\n\
  .*/TestLayers/genericlayers.cproject.yml\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/board3.clayer.yml \\(layer type: Board\\)\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/testvariant.clayer.yml \\(layer type: TestVariant\\)\n\
\n\
.*/ARM/RteTest_DFP/0.2.0/Layers/board3.clayer.yml \\(layer type: Board\\)\n\
.*/ARM/RteTest_DFP/0.2.0/Layers/testvariant.clayer.yml \\(layer type: TestVariant\\)\n\
";

  const string& outStr = streamRedirect.GetOutString();
  EXPECT_TRUE(regex_match(outStr, regex(expectedOutStr)));
}

TEST_F(ProjMgrUnitTests, ListLayersIncompatible) {
  StdStreamRedirect streamRedirect;
  char* argv[8];
  const string& csolution = testinput_folder + "/TestLayers/genericlayers.csolution.yml";
  const string& context = "genericlayers.IncompatibleLayers+AnyBoard";
  argv[1] = (char*)"list";
  argv[2] = (char*)"layers";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  argv[5] = (char*)"-c";
  argv[6] = (char*)context.c_str();
  argv[7] = (char*)"-d";
  EXPECT_EQ(1, RunProjMgr(8, argv, 0));

  const string& expected = "\
debug csolution: check for context 'genericlayers.IncompatibleLayers\\+AnyBoard'\n\
no clayer matches type 'UnknownType'\n\
clayer type 'DifferentFromDescriptionInPdsc' does not match type 'PdscType' in pack description\n\
\n\
check combined connections:\n\
  .*/TestLayers/genericlayers.cproject.yml\n\
    \\(Project Connections\\)\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/board1.clayer.yml \\(layer type: Board\\)\n\
    \\(Board1 Connections\\)\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/incompatible.clayer.yml \\(layer type: Incompatible\\)\n\
    \\(Incompatible Connections\\)\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/pdsc-type-mismatch.clayer.yml \\(layer type: DifferentFromDescriptionInPdsc\\)\n\
connections provided with multiple different values:\n\
  MultipleProvidedNonIdentical0\n\
  MultipleProvidedNonIdentical1\n\
required connections not provided:\n\
  ProvidedDontMatch: -1\n\
  ProvidedEmpty: 123\n\
sum of required values exceed provided:\n\
  AddedValueHigherThanProvided: 100 > 99\n\
connections are invalid\n\
\n\
check combined connections:\n\
  .*/TestLayers/genericlayers.cproject.yml\n\
    \\(Project Connections\\)\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/board2.clayer.yml \\(layer type: Board\\)\n\
    \\(Board2 Connections\\)\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/incompatible.clayer.yml \\(layer type: Incompatible\\)\n\
    \\(Incompatible Connections\\)\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/pdsc-type-mismatch.clayer.yml \\(layer type: DifferentFromDescriptionInPdsc\\)\n\
connections provided with multiple different values:\n\
  MultipleProvidedNonIdentical0\n\
  MultipleProvidedNonIdentical1\n\
required connections not provided:\n\
  ProvidedDontMatch: -1\n\
  ProvidedEmpty: 123\n\
sum of required values exceed provided:\n\
  AddedValueHigherThanProvided: 100 > 99\n\
connections are invalid\n\
\n\
check combined connections:\n\
  .*/TestLayers/genericlayers.cproject.yml\n\
    \\(Project Connections\\)\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/board3.clayer.yml \\(layer type: Board\\)\n\
    \\(Board3 Connections\\)\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/incompatible.clayer.yml \\(layer type: Incompatible\\)\n\
    \\(Incompatible Connections\\)\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/pdsc-type-mismatch.clayer.yml \\(layer type: DifferentFromDescriptionInPdsc\\)\n\
connections provided with multiple different values:\n\
  MultipleProvidedNonIdentical0\n\
  MultipleProvidedNonIdentical1\n\
required connections not provided:\n\
  ProvidedDontMatch: -1\n\
  ProvidedEmpty: 123\n\
sum of required values exceed provided:\n\
  AddedValueHigherThanProvided: 100 > 99\n\
connections are invalid\n\
\n\
no valid combination of clayers was found\n\
\n\
";

  const string& errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(regex_match(errStr, regex(expected)));

  const string& expectedOutStr = "";

  const string& outStr = streamRedirect.GetOutString();
  EXPECT_TRUE(regex_match(outStr, regex(expectedOutStr)));
}

TEST_F(ProjMgrUnitTests, ListLayersInvalidContext) {
  char* argv[7];
  const string& csolution = testinput_folder + "/TestLayers/genericlayers.csolution.yml";
  const string& context = "*.InvalidContext";
  argv[1] = (char*)"list";
  argv[2] = (char*)"layers";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  argv[5] = (char*)"-c";
  argv[6] = (char*)context.c_str();
  EXPECT_EQ(1, RunProjMgr(7, argv, 0));
}

TEST_F(ProjMgrUnitTests, ListLayersAllContexts) {
  char* argv[5];
  const string& csolution = testinput_folder + "/TestLayers/genericlayers.csolution.yml";
  argv[1] = (char*)"list";
  argv[2] = (char*)"layers";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  EXPECT_EQ(1, RunProjMgr(5, argv, 0));
}

TEST_F(ProjMgrUnitTests, ListLayersSearchPath) {
  StdStreamRedirect streamRedirect;
  char* argv[8];
  const string& csolution = testinput_folder + "/TestLayers/searchpath.csolution.yml";
  const string& clayerSearchPath = testcmsispack_folder;
  argv[1] = (char*)"list";
  argv[2] = (char*)"layers";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  argv[5] = (char*)"--clayer-path";
  argv[6] = (char*)clayerSearchPath.c_str();
  argv[7] = (char*)"-d";
  EXPECT_EQ(1, RunProjMgr(8, argv, 0));

  const string& expectedErrStr = ".*\
check combined connections:\
  .*/TestLayers/searchpath.cproject.yml.*\
  .*/ARM/RteTest_DFP/0.2.0/Layers/testvariant.clayer.yml \\(layer type: TestVariant\\).*\
";

  string errStr = streamRedirect.GetErrorString();
  errStr.erase(std::remove(errStr.begin(), errStr.end(), '\n'), errStr.cend());

  EXPECT_TRUE(regex_match(errStr, regex(expectedErrStr)));

  // test invalid clayer path
  streamRedirect.ClearStringStreams();
  argv[6] = (char*)"invalid/clayer/path";
  EXPECT_EQ(1, RunProjMgr(8, argv, 0));

  errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(regex_match(errStr, regex(".*invalid/clayer/path - error csolution: clayer search path does not exist\n")));
}

TEST_F(ProjMgrUnitTests, LayerVariables) {
  char* argv[6];

  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestLayers/variables.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  // Check generated CPRJs
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/variables.BuildType1+TargetType1.cprj",
    testinput_folder + "/TestLayers/ref/variables/variables.BuildType1+TargetType1.cprj");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/variables.BuildType1+TargetType2.cprj",
    testinput_folder + "/TestLayers/ref/variables/variables.BuildType1+TargetType2.cprj");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/variables.BuildType2+TargetType1.cprj",
    testinput_folder + "/TestLayers/ref/variables/variables.BuildType2+TargetType1.cprj");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/variables.BuildType2+TargetType2.cprj",
    testinput_folder + "/TestLayers/ref/variables/variables.BuildType2+TargetType2.cprj");
}

TEST_F(ProjMgrUnitTests, LayerVariablesRedefinition) {
  char* argv[6];
  StdStreamRedirect streamRedirect;
  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestLayers/variables-redefinition.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));
  const string& expected = "warning csolution: variable 'VariableName' redefined from 'FirstValue' to 'SecondValue'\n";
  const string& errStr = streamRedirect.GetErrorString();
  EXPECT_STREQ(errStr.c_str(), expected.c_str());
}

TEST_F(ProjMgrUnitTests, LayerVariablesNotDefined) {
  char* argv[8];
  StdStreamRedirect streamRedirect;
  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestLayers/variables-notdefined.csolution.yml";
  argv[1] = (char*)"list";
  argv[2] = (char*)"layers";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  argv[5] = (char*)"-o";
  argv[6] = (char*)testoutput_folder.c_str();
  argv[7] = (char*)"-d";
  EXPECT_EQ(1, RunProjMgr(8, argv, 0));

  const string& expectedErrStr = ".*\
\\$NotDefined\\$ - warning csolution: variable was not defined for context 'variables-notdefined.BuildType\\+TargetType'.*\
  .*/ARM/RteTest_DFP/0.2.0/Layers/board1.clayer.yml \\(layer type: Board\\).*\
  .*/ARM/RteTest_DFP/0.2.0/Layers/board2.clayer.yml \\(layer type: Board\\).*\
  .*/ARM/RteTest_DFP/0.2.0/Layers/board3.clayer.yml \\(layer type: Board\\).*\
no valid combination of clayers was found\
";

  string errStr = streamRedirect.GetErrorString();
  errStr.erase(std::remove(errStr.begin(), errStr.end(), '\n'), errStr.cend());

  EXPECT_TRUE(regex_match(errStr, regex(expectedErrStr)));
}

TEST_F(ProjMgrUnitTests, LayerVariablesNotDefined_SearchPath) {
  StdStreamRedirect streamRedirect;
  char* argv[8];
  const string& csolution = testinput_folder + "/TestLayers/variables-notdefined.csolution.yml";
  const string& clayerSearchPath = testinput_folder + "/TestLayers/variables";
  argv[1] = (char*)"list";
  argv[2] = (char*)"layers";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  argv[5] = (char*)"--clayer-path";
  argv[6] = (char*)clayerSearchPath.c_str();
  argv[7] = (char*)"-d";
  EXPECT_EQ(0, RunProjMgr(8, argv, 0));

  const string& expectedErrStr = ".*\
clayer of type 'Board' was uniquely found:\
  .*/TestLayers/variables/target1.clayer.yml\
";

  string errStr = streamRedirect.GetErrorString();
  errStr.erase(std::remove(errStr.begin(), errStr.end(), '\n'), errStr.cend());

  EXPECT_TRUE(regex_match(errStr, regex(expectedErrStr)));
}

TEST_F(ProjMgrUnitTests, AccessSequences) {
  char* argv[7];

  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestAccessSequences/test-access-sequences.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  // Check generated CPRJs
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test-access-sequences1.Debug+CM0.cprj",
    testinput_folder + "/TestAccessSequences/ref/test-access-sequences1.Debug+CM0.cprj");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test-access-sequences1.Release+CM0.cprj",
    testinput_folder + "/TestAccessSequences/ref/test-access-sequences1.Release+CM0.cprj");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test-access-sequences2.Debug+CM0.cprj",
    testinput_folder + "/TestAccessSequences/ref/test-access-sequences2.Debug+CM0.cprj");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test-access-sequences2.Release+CM0.cprj",
    testinput_folder + "/TestAccessSequences/ref/test-access-sequences2.Release+CM0.cprj");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test-access-sequences1.Debug+CM3.cprj",
    testinput_folder + "/TestAccessSequences/ref/test-access-sequences1.Debug+CM3.cprj");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test-access-sequences1.Release+CM3.cprj",
    testinput_folder + "/TestAccessSequences/ref/test-access-sequences1.Release+CM3.cprj");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test-access-sequences2.Debug+CM3.cprj",
    testinput_folder + "/TestAccessSequences/ref/test-access-sequences2.Debug+CM3.cprj");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test-access-sequences2.Release+CM3.cprj",
    testinput_folder + "/TestAccessSequences/ref/test-access-sequences2.Release+CM3.cprj");
}

TEST_F(ProjMgrUnitTests, AccessSequences2) {
  char* argv[7];

  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestAccessSequences/test-access-sequences2.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  // Check generated CPRJs
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test-access-sequences3.Debug+TEST_TARGET.cprj",
    testinput_folder + "/TestAccessSequences/ref/test-access-sequences3.Debug+TEST_TARGET.cprj");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test-access-sequences3.Release+TEST_TARGET.cprj",
    testinput_folder + "/TestAccessSequences/ref/test-access-sequences3.Release+TEST_TARGET.cprj");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_MalformedAccessSequences1) {
  char* argv[6];
  const string& csolution = testinput_folder + "/TestAccessSequences/test-malformed-access-sequences1.csolution.yml";

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_MalformedAccessSequences2) {
  char* argv[6];
  const string& csolution = testinput_folder + "/TestAccessSequences/malformed-access-sequences2.cproject.yml";

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Multicore) {
  char* argv[7];
  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestSolution/multicore.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  // Check generated CPRJs
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/multicore+CM0.cprj",
    testinput_folder + "/TestSolution/ref/multicore+CM0.cprj");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Generator) {
  char* argv[7];
  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestGenerator/test-gpdsc.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  string RTE_Components_h = testinput_folder + "/TestGenerator/RTE/_Debug_CM0/RTE_Components.h";

  EXPECT_TRUE(RteFsUtils::Exists(RTE_Components_h));
  string buf;
  EXPECT_TRUE(RteFsUtils::ReadFile(RTE_Components_h, buf));
  EXPECT_TRUE(buf.find("#define RTE_TEST_GENERATOR_FROM_GPDSC_PRE_CHECK") != string::npos);

  // Check generated CPRJs
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test-gpdsc.Debug+CM0.cprj",
    testinput_folder + "/TestGenerator/ref/test-gpdsc.Debug+CM0.cprj");

  // Check generated cbuild YMLs
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test-gpdsc.cbuild-idx.yml",
    testinput_folder + "/TestGenerator/ref/test-gpdsc.cbuild-idx.yml");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test-gpdsc.Debug+CM0.cbuild.yml",
    testinput_folder + "/TestGenerator/ref/test-gpdsc.Debug+CM0.cbuild.yml");

  // Check cbuild.yml schema
  EXPECT_TRUE(ProjMgrYamlSchemaChecker().Validate(testoutput_folder + "/test-gpdsc.Debug+CM0.cbuild.yml", ProjMgrYamlSchemaChecker::FileType::BUILD));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_GeneratorLayer) {
  char* argv[5];
  // convert solution
  const string& csolution = testinput_folder + "/TestGenerator/test-gpdsc-layer.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(5, argv, 0));

  // Check generated cbuild YML
  ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test-gpdsc-layer.Debug+CM0.cbuild.yml",
    testinput_folder + "/TestGenerator/ref/test-gpdsc-layer.Debug+CM0.cbuild.yml");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_TargetOptions)
{
  char *argv[7];
  // convert --solution solution.yml
  const string &csolution = testinput_folder + "/TestSolution/test_target_options.csolution.yml";
  argv[1] = (char *)"convert";
  argv[2] = (char *)"--solution";
  argv[3] = (char *)csolution.c_str();
  argv[4] = (char *)"-o";
  argv[5] = (char *)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  // Check generated CPRJs
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test_target_options+CM0.cprj",
              testinput_folder + "/TestSolution/ref/test_target_options+CM0.cprj");
}

TEST_F(ProjMgrUnitTests, ListPacks) {
  set<string> expectedPacks = {
    "ARM::RteTest@0.1.0 \\(.*\\)",
    "ARM::RteTestBoard@0.1.0 \\(.*\\)",
    "ARM::RteTestGenerator@0.1.0 \\(.*\\)",
    "ARM::RteTest_DFP@0.1.1 \\(.*\\)",
    "ARM::RteTest_DFP@0.2.0 \\(.*\\)"
  };
  vector<string> packs;
  EXPECT_TRUE(m_worker.ParseContextSelection({}));
  m_worker.SetLoadPacksPolicy(LoadPacksPolicy::ALL);
  EXPECT_TRUE(m_worker.ListPacks(packs, false, "RteTest"));
  auto packIt = packs.begin();
  for (const auto& expected : expectedPacks) {
    EXPECT_TRUE(regex_match(*packIt++, regex(expected)));
  }
}

TEST_F(ProjMgrUnitTests, ListPacksLatest) {
  set<string> expectedPacks = {
    "ARM::RteTest@0.1.0 \\(.*\\)",
    "ARM::RteTestBoard@0.1.0 \\(.*\\)",
    "ARM::RteTestGenerator@0.1.0 \\(.*\\)",
    "ARM::RteTest_DFP@0.2.0 \\(.*\\)"
  };
  vector<string> packs;
  EXPECT_TRUE(m_worker.ParseContextSelection({}));
  m_worker.SetLoadPacksPolicy(LoadPacksPolicy::LATEST);
  EXPECT_TRUE(m_worker.ListPacks(packs, false, "RteTest"));
  auto packIt = packs.begin();
  for (const auto& expected : expectedPacks) {
    EXPECT_TRUE(regex_match(*packIt++, regex(expected)));
  }
}

TEST_F(ProjMgrUnitTests, ListBoards) {
  set<string> expected = {
    "Keil::RteTest Dummy board:1.2.3 (ARM::RteTest_DFP@0.2.0)"
  };
  vector<string> devices;
  EXPECT_TRUE(m_worker.ParseContextSelection({}));
  EXPECT_TRUE(m_worker.ListBoards(devices, "Dummy"));
  EXPECT_EQ(expected, set<string>(devices.begin(), devices.end()));
}

TEST_F(ProjMgrUnitTests, ListDevices) {
  set<string> expected = {
    "ARM::RteTestGen_ARMCM0 (ARM::RteTestGenerator@0.1.0)",
    "ARM::RteTest_ARMCM0 (ARM::RteTest_DFP@0.2.0)",
    "ARM::RteTest_ARMCM0_Dual:cm0_core0 (ARM::RteTest_DFP@0.2.0)",
    "ARM::RteTest_ARMCM0_Dual:cm0_core1 (ARM::RteTest_DFP@0.2.0)",
    "ARM::RteTest_ARMCM0_Single (ARM::RteTest_DFP@0.2.0)",
    "ARM::RteTest_ARMCM0_Test (ARM::RteTest_DFP@0.2.0)"
  };
  vector<string> devices;
  EXPECT_TRUE(m_worker.ParseContextSelection({}));
  EXPECT_TRUE(m_worker.ListDevices(devices, "CM0"));
  EXPECT_EQ(expected, set<string>(devices.begin(), devices.end()));
}

TEST_F(ProjMgrUnitTests, ListDevicesPackageFiltered) {
  set<string> expected = {
    "ARM::RteTest_ARMCM3 (ARM::RteTest_DFP@0.2.0)"
  };
  vector<string> devices;
  ContextDesc descriptor;
  const string& filenameInput = testinput_folder + "/TestProject/test.cproject.yml";
  EXPECT_TRUE(m_parser.ParseCproject(filenameInput, false, true));
  EXPECT_TRUE(m_worker.AddContexts(m_parser, descriptor, filenameInput));
  EXPECT_TRUE(m_worker.ParseContextSelection({ "test" }));
  EXPECT_TRUE(m_worker.ListDevices(devices, "CM3"));
  EXPECT_EQ(expected, set<string>(devices.begin(), devices.end()));
}

TEST_F(ProjMgrUnitTests, ListComponents) {
  set<string> expected = {
    "ARM::Device:Startup&RteTest Startup@2.0.3 (ARM::RteTest_DFP@0.2.0)",
  };
  vector<string> components;
  EXPECT_TRUE(m_worker.ParseContextSelection({}));
  EXPECT_TRUE(m_worker.ListComponents(components, "Device:Startup"));
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
  EXPECT_TRUE(m_worker.ParseContextSelection({ "test" }));
  EXPECT_TRUE(m_worker.ListComponents(components, "Device:Startup"));
  EXPECT_EQ(expected, set<string>(components.begin(), components.end()));
}

TEST_F(ProjMgrUnitTests, ListDependencies) {
  set<string> expected = {
     "ARM::Device:Startup&RteTest Startup@2.0.3 require RteTest:CORE",
  };
  vector<string> dependencies;
  ContextDesc descriptor;
  const string& filenameInput = testinput_folder + "/TestSolution/TestProject4/test-dependency.cproject.yml";
  EXPECT_TRUE(m_parser.ParseCproject(filenameInput, false, true));
  EXPECT_TRUE(m_worker.AddContexts(m_parser, descriptor, filenameInput));
  EXPECT_TRUE(m_worker.ParseContextSelection({ "test-dependency" }));
  EXPECT_TRUE(m_worker.ListDependencies(dependencies, "CORE"));
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

TEST_F(ProjMgrUnitTests, RunListContexts_Ordered) {
  set<string> expected = {
    "test2.Debug+CM0",
    "test2.Debug+CM3",
    "test1.Debug+CM0",
    "test1.Release+CM0"
  };
  const string& dirInput = testinput_folder + "/TestSolution/";
  const string& filenameInput = dirInput + "test_ordered.csolution.yml";
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
  EXPECT_TRUE(m_worker.ListContexts(contexts, RteUtils::EMPTY_STRING, true));
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
  const string& filenameInput = dirInput + "test_no_buildtypes.csolution.yml";
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
  const string& filenameInput = testinput_folder + "/TestSolution/test_missing_project.csolution.yml";
  EXPECT_TRUE(m_parser.ParseCsolution(filenameInput, false));
  EXPECT_FALSE(m_worker.AddContexts(m_parser, descriptor, filenameInput));
}

TEST_F(ProjMgrUnitTests, GetInstalledPacks) {
  EXPECT_TRUE(m_worker.InitializeModel());
  auto kernel = ProjMgrKernel::Get();
  const auto& cmsisPackRoot = kernel->GetCmsisPackRoot();
  std::list<std::string> pdscFiles;

  kernel->SetCmsisPackRoot(string(CMAKE_SOURCE_DIR) + "test/local");
  EXPECT_TRUE(kernel->GetInstalledPacks(pdscFiles));

  kernel->SetCmsisPackRoot(string(CMAKE_SOURCE_DIR) + "test/local-malformed");
  EXPECT_FALSE(kernel->GetInstalledPacks(pdscFiles));

  EXPECT_FALSE(m_worker.LoadAllRelevantPacks());

  kernel->SetCmsisPackRoot(cmsisPackRoot);
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_processor) {
  char* argv[7];

  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestSolution/test_pname.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  // Check generated CPRJs
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test2.Debug+CM0.cprj",
    testinput_folder + "/TestSolution/ref/test2.Debug+CM0_pname.cprj");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test2.Debug+CM3.cprj",
    testinput_folder + "/TestSolution/ref/test2.Debug+CM3_pname.cprj");
}

TEST_F(ProjMgrUnitTests, RunProjMgrLayers_missing_project_file) {
  char* argv[6];
  StdStreamRedirect streamRedirect;
  const string& expected = "./unknown.cproject.yml - error csolution: cproject file was not found\n";
  const string& csolutionFile = testinput_folder + "/TestSolution/test_missing_project.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv, 0));
  const string& errStr = streamRedirect.GetErrorString();
  EXPECT_STREQ(errStr.c_str(), expected.c_str());
}

TEST_F(ProjMgrUnitTests, RunProjMgrLayers_pname) {
  char* argv[6];
  const string& csolutionFile = testinput_folder + "/TestLayers/testlayers.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgrLayers_no_device_found) {
  char* argv[6];
  const string& csolution = testinput_folder + "/TestLayers/testlayers_no_device_name.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_No_Device_Name) {
  char* argv[4];
  const string& csolution = testinput_folder + "/TestSolution/test_no_device_name.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  EXPECT_EQ(1, RunProjMgr(4, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_No_Board_No_Device) {
  // Test with no board no device info
  char* argv[6];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_no_board_no_device.cproject.yml");
  const string& expected = "missing device and/or board info";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Invalid_Board_Name) {
  // Test project with invalid board name
  char* argv[6];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_board_name_invalid.cproject.yml");
  const string& expected = "board 'Keil::RteTest_unknown' was not found";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Invalid_Board_Vendor) {
  // Test project with invalid vendor name
  char* argv[6];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_board_vendor_invalid.cproject.yml");
  const string& expected = "board 'UNKNOWN::RteTest Dummy board' was not found";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Only_Board_Info) {
  // Test project with only board info and missing device info
  char* argv[6];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_only_board.cproject.yml");
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  // Check generated CPRJ
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test_only_board+TEST_TARGET.cprj",
    testinput_folder + "/TestSolution/TestProject4/test_only_board+TEST_TARGET.cprj");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Only_Board_No_Pname) {
  // Test project with only board info and no pname
  char* argv[6];
  const string& csolution = testinput_folder + "/TestProject/test_only_board_no_pname.cproject.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Device_Unknown) {
  // Test project with unknown device
  char* argv[6];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_device_unknown.cproject.yml");
  const string& expectedErrStr = R"(error csolution: specified device 'RteTest_ARM_UNKNOWN' was not found among the installed packs.
use 'cpackget' utility to install software packs.
  cpackget add Vendor.PackName --pack-root ./Path/Packs)";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expectedErrStr));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Device_Unknown_Vendor) {
  // Test project with unknown device vendor
  char* argv[6];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_device_unknown_vendor.cproject.yml");
  const string& expectedErrStr = R"(error csolution: specified device 'RteTest_ARMCM0' was not found among the installed packs.
use 'cpackget' utility to install software packs.
  cpackget add Vendor.PackName --pack-root ./Path/Packs)";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expectedErrStr));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Device_Unknown_Processor) {
  // Test project with unknown processor
  char* argv[6];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_device_unknown_processor.cproject.yml");
  const string& expected = "processor name 'NOT_AVAILABLE' was not found";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Device_Unavailable_In_Board) {
  // Test project with device different from the board's mounted device
  char* argv[6];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_device_unavailable_in_board.cproject.yml");
  const string& expectedErrStr = R"(error csolution: specified device 'RteTest_ARMCM7' was not found among the installed packs.
use 'cpackget' utility to install software packs.
  cpackget add Vendor.PackName --pack-root ./Path/Packs)";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expectedErrStr));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Device_Pname_Unavailable_In_Board) {
  // Test project with device processor name different from the board's mounted device's processors
  char* argv[6];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_device_pname_unavailable_in_board.cproject.yml");
  const string& expected = "processor name 'cm0_core7' was not found";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Only_Device_Info) {
  // Test project with only device info
  char* argv[6];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test.cproject.yml");
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Board_And_Device_Info) {
  // Test project with valid board and device info
  char* argv[6];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_board_and_device.cproject.yml");
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Correct_Board_Wrong_Device_Info) {
  // Test project with correct board info but wrong device info
  char* argv[6];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_correct_board_wrong_device.cproject.yml");
  const string& expectedErrStr = R"(error csolution: specified device 'RteTest_ARMCM_Unknown' was not found among the installed packs.
use 'cpackget' utility to install software packs.
  cpackget add Vendor.PackName --pack-root ./Path/Packs)";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expectedErrStr));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Correct_Device_Wrong_Board_Info) {
  // Test project with correct device info but wrong board info
  char* argv[6];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_correct_device_wrong_board.cproject.yml");
  const string& expected = "board 'Keil::RteTest unknown board' was not found";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Board_Multi_Mounted_Devices) {
  // Test Project with only board info and multiple mounted devices
  char* argv[6];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_board_multi_mounted_device.cproject.yml");
  const string& expected = "found multiple mounted devices";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Board_Device_Variant) {
  // Test Project with only board info and single mounted device with single variant
  char* argv[6];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_board_device_variant.cproject.yml");

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Board_Multi_Variants_And_Device) {
  // Test Project with device variant and board info and mounted device with multiple variants
  char* argv[6];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_board_multi_variant_and_device.cproject.yml");

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Board_Multi_Variants) {
  // Test Project with only board info and mounted device with multiple variants
  char* argv[6];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_board_multi_variant.cproject.yml");
  const string& expected = "found multiple device variants";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Board_No_Mounted_Devices) {
  // Test Project with only board info and no mounted devices
  char* argv[6];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_board_no_mounted_device.cproject.yml");
  const string& expected = "found no mounted device";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Board_Device_Info) {
  // Test Project with board mounted device different than selected devices
  char* argv[6];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_mounted_device_differs_selected_device.cproject.yml");
  const string& expected = "warning csolution: specified device 'RteTest_ARMCM0' and board mounted device 'RteTest_ARMCM0_Dual' are different";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));
  auto warnStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, warnStr.find(expected));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListGenerators) {
  const std::string rteFolder = testinput_folder + "/TestGenerator/RTE";
  set<string> rteFilesBefore, rteFilesAfter;
  GetFilesInTree(rteFolder, rteFilesBefore);

  char* argv[7];
  const string& csolution = testinput_folder + "/TestGenerator/test-gpdsc.csolution.yml";
  argv[1] = (char*)"list";
  argv[2] = (char*)"generators";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  argv[5] = (char*)"-c";
  argv[6] = (char*)"test-gpdsc.Debug+CM0";
  EXPECT_EQ(0, RunProjMgr(7, argv, 0));

  GetFilesInTree(rteFolder, rteFilesAfter);
  EXPECT_EQ(rteFilesBefore, rteFilesAfter);
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListGeneratorsEmptyContext) {
  char* argv[5];
  const string& csolution = testinput_folder + "/TestGenerator/test-gpdsc.csolution.yml";
  argv[1] = (char*)"list";
  argv[2] = (char*)"generators";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  EXPECT_EQ(0, RunProjMgr(5, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListGeneratorsEmptyContextMultipleTypes) {
  char* argv[5];
  const string& csolution = testinput_folder + "/TestGenerator/test-gpdsc-multiple-types.csolution.yml";
  argv[1] = (char*)"list";
  argv[2] = (char*)"generators";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  EXPECT_EQ(0, RunProjMgr(5, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListGeneratorsNonExistentContext) {
  char* argv[7];
  const string& csolution = testinput_folder + "/TestGenerator/test-gpdsc.csolution.yml";
  argv[1] = (char*)"list";
  argv[2] = (char*)"generators";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  argv[5] = (char*)"-c";
  argv[6] = (char*)"NON-EXISTENT-CONTEXT";
  EXPECT_EQ(1, RunProjMgr(7, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListGeneratorsNonExistentSolution) {
  char* argv[5];
  const string& csolution = testinput_folder + "/TestGenerator/NON-EXISTENT.csolution.yml";
  argv[1] = (char*)"list";
  argv[2] = (char*)"generators";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  EXPECT_EQ(1, RunProjMgr(5, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ExecuteGenerator) {
  char* argv[8];
  const string& csolution = testinput_folder + "/TestGenerator/test-gpdsc.csolution.yml";
  argv[1] = (char*)"run";
  argv[2] = (char*)"-g";
  argv[3] = (char*)"RteTestGeneratorIdentifier";
  argv[4] = (char*)"--solution";
  argv[5] = (char*)csolution.c_str();
  argv[6] = (char*)"-c";
  argv[7] = (char*)"test-gpdsc.Debug+CM0";

  const string& hostType = CrossPlatformUtils::GetHostType();
  if (shouldHaveGeneratorForHostType(hostType)) {
    EXPECT_EQ(0, RunProjMgr(8, argv, 0));
  } else {
    EXPECT_EQ(1, RunProjMgr(8, argv, 0));
  }
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ExecuteGeneratorEmptyContext) {
  char* argv[6];
  const string& csolution = testinput_folder + "/TestGenerator/test-gpdsc.csolution.yml";
  argv[1] = (char*)"run";
  argv[2] = (char*)"-g";
  argv[3] = (char*)"RteTestGeneratorIdentifier";
  argv[4] = (char*)"--solution";
  argv[5] = (char*)csolution.c_str();

  const string& hostType = CrossPlatformUtils::GetHostType();
  if (shouldHaveGeneratorForHostType(hostType)) {
    EXPECT_EQ(0, RunProjMgr(6, argv, 0));
  } else {
    EXPECT_EQ(1, RunProjMgr(6, argv, 0));
  }
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ExecuteGeneratorEmptyContextMultipleTypes) {
  char* argv[8];
  const string& csolution = testinput_folder + "/TestGenerator/test-gpdsc-multiple-types.csolution.yml";
  argv[1] = (char*)"run";
  argv[2] = (char*)"-g";
  argv[3] = (char*)"RteTestGeneratorIdentifier";
  argv[4] = (char*)"--solution";
  argv[5] = (char*)csolution.c_str();
  // the project has multiple contexts but none is specified
  EXPECT_EQ(1, RunProjMgr(6, argv, 0));

  // specify a single context
  argv[6] = (char*)"-c";
  argv[7] = (char*)"test-gpdsc.Debug+CM0";
  const string& hostType = CrossPlatformUtils::GetHostType();
  if (shouldHaveGeneratorForHostType(hostType)) {
    EXPECT_EQ(0, RunProjMgr(8, argv, 0));
  } else {
    EXPECT_EQ(1, RunProjMgr(8, argv, 0));
  }
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ExecuteGeneratorNonExistentContext) {
  char* argv[8];
  const string& csolution = testinput_folder + "/TestGenerator/test-gpdsc.csolution.yml";
  argv[1] = (char*)"run";
  argv[2] = (char*)"-g";
  argv[3] = (char*)"RteTestGeneratorIdentifier";
  argv[4] = (char*)"--solution";
  argv[5] = (char*)csolution.c_str();
  argv[6] = (char*)"-c";
  argv[7] = (char*)"NON-EXISTENT-CONTEXT";
  EXPECT_EQ(1, RunProjMgr(8, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ExecuteGeneratorNonExistentSolution) {
  char* argv[6];
  const string& csolution = testinput_folder + "/TestGenerator/NON-EXISTENT.csolution.yml";
  argv[1] = (char*)"run";
  argv[2] = (char*)"-g";
  argv[3] = (char*)"RteTestGeneratorIdentifier";
  argv[4] = (char*)"--solution";
  argv[5] = (char*)csolution.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv, 0));
}

TEST_F(ProjMgrUnitTests, ListGenerators) {
  set<string> expected = {
     "RteTestGeneratorIdentifier (RteTest Generator Description)",
  };
  vector<string> generators;
  m_csolutionFile = testinput_folder + "/TestGenerator/test-gpdsc.csolution.yml";
  error_code ec;
  m_rootDir = fs::path(m_csolutionFile).parent_path().generic_string();
  m_context.push_back("test-gpdsc.Debug+CM0");
  EXPECT_TRUE(PopulateContexts());
  EXPECT_TRUE(m_worker.ParseContextSelection(m_context));
  EXPECT_TRUE(m_worker.ListGenerators(generators));
  EXPECT_EQ(expected, set<string>(generators.begin(), generators.end()));
}

TEST_F(ProjMgrUnitTests, ListMultipleGenerators) {
  set<string> expected = {
     "RteTestGeneratorIdentifier (RteTest Generator Description)",
     "RteTestGeneratorWithKey (RteTest Generator with Key Description)",
  };
  vector<string> generators;
  m_csolutionFile = testinput_folder + "/TestGenerator/test-gpdsc-multiple-generators.csolution.yml";
  error_code ec;
  m_rootDir = fs::path(m_csolutionFile).parent_path().generic_string();
  m_context.push_back("test-gpdsc-multiple-generators.Debug+CM0");
  EXPECT_TRUE(PopulateContexts());
  EXPECT_TRUE(m_worker.ParseContextSelection(m_context));
  EXPECT_TRUE(m_worker.ListGenerators(generators));
  EXPECT_EQ(expected, set<string>(generators.begin(), generators.end()));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_MultipleGenerators) {
  char* argv[7];
  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestGenerator/test-gpdsc-multiple-generators.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  // Check generated cbuild YML
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/test-gpdsc-multiple-generators.Debug+CM0.cbuild.yml",
    testinput_folder + "/TestGenerator/ref/test-gpdsc-multiple-generators.Debug+CM0.cbuild.yml");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_MultipleGeneratedComponents) {
  char* argv[7];
  // convert -s solution.yml
  const string& csolution = testinput_folder + "/TestGenerator/multiple-components.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"-s";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  // Check generated cbuild YML
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/multiple-components.Debug+CM0.cbuild.yml",
    testinput_folder + "/TestGenerator/ref/multiple-components.Debug+CM0.cbuild.yml");
  // Check generated CPRJ
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/multiple-components.Debug+CM0.cprj",
    testinput_folder + "/TestGenerator/ref/multiple-components.Debug+CM0.cprj");
}

TEST_F(ProjMgrUnitTests, ExecuteGenerator) {
  m_csolutionFile = testinput_folder + "/TestGenerator/test-gpdsc.csolution.yml";
  error_code ec;
  m_rootDir = fs::path(m_csolutionFile).parent_path().generic_string();
  m_context.push_back("test-gpdsc.Debug+CM0");
  m_codeGenerator = "RteTestGeneratorIdentifier";
  EXPECT_TRUE(PopulateContexts());
  EXPECT_TRUE(m_worker.ParseContextSelection(m_context));
  const string& hostType = CrossPlatformUtils::GetHostType();
  if (shouldHaveGeneratorForHostType(hostType)) {
    EXPECT_TRUE(m_worker.ExecuteGenerator(m_codeGenerator));
  } else {
    EXPECT_FALSE(m_worker.ExecuteGenerator(m_codeGenerator));
  }
}


TEST_F(ProjMgrUnitTests, ExecuteGeneratorWithKey) {
  m_csolutionFile = testinput_folder + "/TestGenerator/test-gpdsc_with_key.csolution.yml";
  error_code ec;
  m_rootDir = fs::path(m_csolutionFile).parent_path().generic_string();
  m_context.push_back("test-gpdsc_with_key.Debug+CM0");
  m_codeGenerator = "RteTestGeneratorWithKey";
  EXPECT_TRUE(PopulateContexts());
  EXPECT_TRUE(m_worker.ParseContextSelection(m_context));

  const string& hostType = CrossPlatformUtils::GetHostType();
  string genFolder = testcmsispack_folder + "/ARM/RteTestGenerator/0.1.0/Generator";
  // we use environment variable to test on all pl since it is reliable
  CrossPlatformUtils::SetEnv("RTE_GENERATOR_WITH_KEY", genFolder);
  if (shouldHaveGeneratorForHostType(hostType)) {
    EXPECT_TRUE(m_worker.ExecuteGenerator(m_codeGenerator));
  } else {
    EXPECT_FALSE(m_worker.ExecuteGenerator(m_codeGenerator));
  }
}


TEST_F(ProjMgrUnitTests, RunProjMgrSolution_Filtered_Pack_Selection) {
  char* argv[7];

  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestSolution/test_filtered_pack_selection.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_Pack_Selection) {
  char* argv[7];

  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestSolution/test_pack_selection.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  // Check generated CPRJs
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test2.Debug+CM0.cprj",
    testinput_folder + "/TestSolution/ref/test2.Debug+CM0_pack_selection.cprj");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test2.Debug+TestGen.cprj",
    testinput_folder + "/TestSolution/ref/test2.Debug+TestGen.cprj");
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_No_Packs) {
  char* argv[7];

  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestSolution/test_no_packs.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_Invalid_Packs) {
  char* argv[7];
  StdStreamRedirect streamRedirect;
  std::string errExpected = "required pack: ARM::RteTest_INVALID@0.2.0 not installed";

  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestSolution/test_invalid_pack.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv, 0));

  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(errExpected));
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_Local_Pack) {
  char* argv[7];
  const string& csolution = testinput_folder + "/TestSolution/pack_path.csolution.yml";

  // convert --solution solution.yml
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  // Check generated CPRJ
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/pack_path+CM0.cprj",
    testinput_folder + "/TestSolution/ref/pack_path+CM0.cprj");

}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_Local_Multiple_Pack_Files) {
  char* argv[7];
  const string& csolution = testinput_folder + "/TestSolution/test_local_pack_path.csolution.yml";

  // copy an additional pack file
  string srcPackFile, destPackFile;
  srcPackFile = testinput_folder + "/SolutionSpecificPack/ARM.RteTest_DFP.pdsc";
  destPackFile = testinput_folder + "/SolutionSpecificPack/ARM.RteTest_DFP_2.pdsc";
  if (RteFsUtils::Exists(destPackFile)) {
    RteFsUtils::RemoveFile(destPackFile);
  }
  RteFsUtils::CopyCheckFile(srcPackFile, destPackFile, false);

  // convert --solution solution.yml
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  // remove additionally added file
  RteFsUtils::RemoveFile(destPackFile);
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_Local_Pack_Path_Not_Found) {
  char* argv[7];
  StdStreamRedirect streamRedirect;
  const string errExpected = "pack path: ./SolutionSpecificPack/ARM does not exist";
  const string& csolution = testinput_folder + "/TestSolution/test_local_pack_path_not_found.csolution.yml";

  // convert --solution solution.yml
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv, 0));

  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(errExpected));
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_Local_Pack_File_Not_Found) {
  char* argv[7];
  StdStreamRedirect streamRedirect;
  const string errExpected = "pdsc file was not found in: ../SolutionSpecificPack/Device";
  const string& csolution = testinput_folder + "/TestSolution/test_local_pack_file_not_found.csolution.yml";

  // convert --solution solution.yml
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv, 0));

  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(errExpected));
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_List_Board_Pack) {
  char* argv[7];
  const string& csolution = testinput_folder + "/TestSolution/test_list_board_package.csolution.yml";

  // convert --solution solution.yml
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  // Check generated CPRJs
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test1.Debug+CM0.cprj",
    testinput_folder + "/TestSolution/ref/test1.Debug+CM0_board_package.cprj");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test1.Release+CM0.cprj",
    testinput_folder + "/TestSolution/ref/test1.Release+CM0_board_package.cprj");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_LoadPacksPolicy_Required) {
  char* argv[6];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestSolution/test_no_packs.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-l";
  argv[5] = (char*)"required";
  EXPECT_EQ(1, RunProjMgr(6, argv, 0));
  auto errorStr = streamRedirect.GetErrorString();
  EXPECT_EQ(0, errorStr.find("error csolution: required packs must be specified\n"));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_LoadPacksPolicy_Invalid) {
  char* argv[6];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestSolution/test_no_packs.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-l";
  argv[5] = (char*)"invalid";
  EXPECT_EQ(1, RunProjMgr(6, argv, 0));
  auto errorStr = streamRedirect.GetErrorString();
  EXPECT_EQ(0, errorStr.find("error csolution: unknown load option: 'invalid', it must be 'latest', 'all' or 'required'\n"));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_LoadPacksPolicy_Latest) {
  char* argv[6];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestSolution/test_no_packs.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-l";
  argv[5] = (char*)"latest";
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_LoadPacksPolicy_All) {
  char* argv[6];
  const string& csolution = testinput_folder + "/TestSolution/test_no_packs.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-l";
  argv[5] = (char*)"all";
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  const string& csolution2 = testinput_folder + "/TestSolution/test_pack_selection.csolution.yml";
  argv[3] = (char*)csolution2.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_GetCdefaultFile1) {
  // Valid file search
  const string& testdir = testoutput_folder + "/FindFileRegEx";
  const string& fileName = testdir + "/cdefault.yml";
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
  const string& cdefaultInCompilerRoot = testcmsiscompiler_folder + "/cdefault.yml";
  m_rootDir = testdir;
  m_cdefaultFile.clear();
  RteFsUtils::MoveExistingFile(cdefaultInCompilerRoot, cdefaultInCompilerRoot + ".bak");
  EXPECT_FALSE(GetCdefaultFile());
  RteFsUtils::MoveExistingFile(cdefaultInCompilerRoot + ".bak", cdefaultInCompilerRoot);
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_ParseCdefault1) {
  // Valid cdefault file
  const string& validCdefaultFile = testinput_folder + "/TestDefault/cdefault.yml";
  EXPECT_TRUE(m_parser.ParseCdefault(validCdefaultFile, true));
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_ParseCdefault2) {
  // Wrong cdefault file and schema check enabled
  const string& wrongCdefaultFile = testinput_folder + "/TestDefault/wrong/cdefault.yml";
  EXPECT_FALSE(m_parser.ParseCdefault(wrongCdefaultFile, true));
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_ParseCdefault3) {
  // Wrong cdefault file and schema check disabled
  const string& wrongCdefaultFile = testinput_folder + "/TestDefault/wrong/cdefault.yml";
  EXPECT_TRUE(m_parser.ParseCdefault(wrongCdefaultFile, false));
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_DefaultFile1) {
  char* argv[6];
  // convert --solution solution.yml
  // csolution is empty, all information from cdefault.yml is taken
  const string& csolution = testinput_folder + "/TestDefault/empty.csolution.yml";
  const string& output = testoutput_folder + "/empty";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)output.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  // Check generated CPRJs
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/empty/project.Debug+TEST_TARGET.cprj",
    testinput_folder + "/TestDefault/ref/empty/project.Debug+TEST_TARGET.cprj");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/empty/project.Release+TEST_TARGET.cprj",
    testinput_folder + "/TestDefault/ref/empty/project.Release+TEST_TARGET.cprj");
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_DefaultFile2) {
  char* argv[6];
  // convert --solution solution.yml
  // csolution.yml is complete, no information from cdefault.yml is taken except from misc
  const string& csolution = testinput_folder + "/TestDefault/full.csolution.yml";
  const string& output = testoutput_folder + "/full";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)output.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  // Check generated CPRJs
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/full/project.Debug+TEST_TARGET.cprj",
    testinput_folder + "/TestDefault/ref/full/project.Debug+TEST_TARGET.cprj");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/full/project.Release+TEST_TARGET.cprj",
    testinput_folder + "/TestDefault/ref/full/project.Release+TEST_TARGET.cprj");
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_DefaultFileInCompilerRoot) {
  char* argv[6];
  const string cdefault = testinput_folder + "/TestDefault/cdefault.yml";
  const string cdefaultInCompilerRoot = testcmsiscompiler_folder + "/cdefault.yml";
  RteFsUtils::MoveExistingFile(cdefaultInCompilerRoot, cdefaultInCompilerRoot + ".bak");
  RteFsUtils::MoveExistingFile(cdefault, cdefaultInCompilerRoot);
  const string& csolution = testinput_folder + "/TestDefault/empty.csolution.yml";
  const string& output = testoutput_folder + "/empty";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)output.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  // Check generated cbuild YMLs
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/empty/empty.cbuild-idx.yml",
    testinput_folder + "/TestDefault/ref/empty/empty.cbuild-idx.yml");
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/empty/project.Debug+TEST_TARGET.cbuild.yml",
    testinput_folder + "/TestDefault/ref/empty/project.Debug+TEST_TARGET.cbuild.yml");
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/empty/project.Release+TEST_TARGET.cbuild.yml",
    testinput_folder + "/TestDefault/ref/empty/project.Release+TEST_TARGET.cbuild.yml");

  RteFsUtils::MoveExistingFile(cdefaultInCompilerRoot, cdefault);
  RteFsUtils::MoveExistingFile(cdefaultInCompilerRoot + ".bak", cdefaultInCompilerRoot);
}

TEST_F(ProjMgrUnitTests, RunProjMgr_NoUpdateRTEFiles) {
  char* argv[7];
  const string csolutionFile = UpdateTestSolutionFile("./TestProject4/test.cproject.yml");
  const string rteFolder = RteFsUtils::ParentPath(csolutionFile) + "/TestProject4/RTE";
  set<string> rteFilesBefore, rteFilesAfter;
  RteFsUtils::RemoveDir(rteFolder);
  GetFilesInTree(rteFolder, rteFilesBefore);

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--no-update-rte";
  EXPECT_EQ(0, RunProjMgr(7, argv, 0));

  // The RTE folder should be left untouched
  GetFilesInTree(rteFolder, rteFilesAfter);
  EXPECT_EQ(rteFilesBefore, rteFilesAfter);

  // CPRJ should still be generated
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test+TEST_TARGET.cprj",
    testinput_folder + "/TestSolution/TestProject4/test+TEST_TARGET.cprj");
}

TEST_F(ProjMgrUnitTests, LoadPacks_MultiplePackSelection) {
  m_csolutionFile = testinput_folder + "/TestSolution/pack_contexts.csolution.yml";
  m_rootDir = fs::path(m_csolutionFile).parent_path().generic_string();
  EXPECT_TRUE(PopulateContexts());
  map<string, ContextItem>* contexts = nullptr;
  m_worker.GetContexts(contexts);
  for (auto& [contextName, contextItem] : *contexts) {
    EXPECT_TRUE(m_worker.ProcessContext(contextItem, false));
  }
}

TEST_F(ProjMgrUnitTests, LoadPacks_MissingPackSelection) {
  m_csolutionFile = testinput_folder + "/TestSolution/test_local_pack_path_not_found.csolution.yml";
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
    "ARM::RteTest_ARMCM0 (ARM::RteTest_DFP@0.2.0)",
    "ARM::RteTest_ARMCM0_Dual:cm0_core0 (ARM::RteTest_DFP@0.2.0)",
    "ARM::RteTest_ARMCM0_Dual:cm0_core1 (ARM::RteTest_DFP@0.2.0)",
    "ARM::RteTest_ARMCM0_Single (ARM::RteTest_DFP@0.2.0)",
    "ARM::RteTest_ARMCM0_Test (ARM::RteTest_DFP@0.2.0)"
  };
  set<string> expected_Gen = {
    "ARM::RteTestGen_ARMCM0 (ARM::RteTestGenerator@0.1.0)"
  };
  vector<string> devices;
  m_csolutionFile = testinput_folder + "/TestSolution/pack_contexts.csolution.yml";
  m_rootDir = fs::path(m_csolutionFile).parent_path().generic_string();
  EXPECT_TRUE(PopulateContexts());
  EXPECT_TRUE(m_worker.InitializeModel());
  EXPECT_TRUE(m_worker.LoadAllRelevantPacks());
  EXPECT_TRUE(m_worker.ParseContextSelection({ "pack_contexts+CM0" }));
  EXPECT_TRUE(m_worker.ListDevices(devices, "CM0"));
  EXPECT_EQ(expected_CM0, set<string>(devices.begin(), devices.end()));
  devices.clear();
  EXPECT_TRUE(m_worker.ParseContextSelection({ "pack_contexts+Gen" }));
  EXPECT_TRUE(m_worker.ListDevices(devices, "CM0"));
  EXPECT_EQ(expected_Gen, set<string>(devices.begin(), devices.end()));
}

TEST_F(ProjMgrUnitTests, ListComponents_MultiplePackSelection) {
  set<string> expected_CM0 = {
    "ARM::Device:Startup&RteTest Startup@2.0.3 (ARM::RteTest_DFP@0.2.0)"
  };
  set<string> expected_Gen = {
    "ARM::Device&RteTestBundle:RteTest Generated Component@1.1.0 (ARM::RteTestGenerator@0.1.0)",
    "ARM::Device&RteTestBundle:Startup@1.1.0 (ARM::RteTestGenerator@0.1.0)",
    "ARM::Device:RteTest Generated Component:RteTest@1.1.0 (ARM::RteTestGenerator@0.1.0)",
    "ARM::Device:RteTest Generated Component:RteTestGenFiles@1.1.0 (ARM::RteTestGenerator@0.1.0)",
    "ARM::Device:RteTest Generated Component:RteTestSimple@1.1.0 (ARM::RteTestGenerator@0.1.0)",
    "ARM::Device:RteTest Generated Component:RteTestWithKey@1.1.0 (ARM::RteTestGenerator@0.1.0)",
    "ARM::Device:RteTest Generated Component:RteTestNoExe@1.1.0 (ARM::RteTestGenerator@0.1.0)"
  };
  vector<string> components;
  m_csolutionFile = testinput_folder + "/TestSolution/pack_contexts.csolution.yml";
  m_rootDir = fs::path(m_csolutionFile).parent_path().generic_string();
  EXPECT_TRUE(PopulateContexts());
  EXPECT_TRUE(m_worker.InitializeModel());
  EXPECT_TRUE(m_worker.LoadAllRelevantPacks());
  EXPECT_TRUE(m_worker.ParseContextSelection({ "pack_contexts+CM0" }));
  EXPECT_TRUE(m_worker.ListComponents(components, "Device:Startup"));
  EXPECT_EQ(expected_CM0, set<string>(components.begin(), components.end()));
  components.clear();
  EXPECT_TRUE(m_worker.ParseContextSelection({ "pack_contexts+Gen" }));
  EXPECT_TRUE(m_worker.ListComponents(components));
  EXPECT_EQ(expected_Gen, set<string>(components.begin(), components.end()));
}

TEST_F(ProjMgrUnitTests, Convert_ValidationResults_Dependencies) {
  char* argv[6];
  const string& csolution = testinput_folder + "/Validation/dependencies.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-c";

  map<string, string> testData = {
    {"selectable+CM0",           "warning csolution: dependency validation for context 'selectable+CM0' failed:\nSELECTABLE ARM::Device:Startup&RteTest Startup@2.0.3\n  require RteTest:CORE" },
    {"missing+CM0",              "warning csolution: dependency validation for context 'missing+CM0' failed:\nMISSING ARM::RteTest:Check:Missing@0.9.9\n  require RteTest:Dependency:Missing" },
    {"conflict+CM0",             "warning csolution: dependency validation for context 'conflict+CM0' failed:\nCONFLICT RteTest:ApiExclusive@1.0.0\n  ARM::RteTest:ApiExclusive:S1\n  ARM::RteTest:ApiExclusive:S2" },
    {"incompatible+CM0",         "warning csolution: dependency validation for context 'incompatible+CM0' failed:\nINCOMPATIBLE ARM::RteTest:Check:Incompatible@0.9.9\n  deny RteTest:Dependency:Incompatible_component" },
    {"incompatible-variant+CM0", "warning csolution: dependency validation for context 'incompatible-variant+CM0' failed:\nINCOMPATIBLE_VARIANT ARM::RteTest:Check:IncompatibleVariant@0.9.9\n  require RteTest:Dependency:Variant&Compatible" },
  };

  for (const auto& [context, expected] : testData) {
    StdStreamRedirect streamRedirect;
    argv[5] = (char*)context.c_str();
    EXPECT_EQ(0, RunProjMgr(6, argv, 0));
    auto errorStr = streamRedirect.GetErrorString();
    EXPECT_NE(string::npos, errorStr.find(expected));
  }
}

TEST_F(ProjMgrUnitTests, Convert_ValidationResults_Filtering) {
  char* argv[6];
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[4] = (char*)"-c";

  vector<tuple<string, int, string>> testData = {
    {"recursive", 1, "\
warning csolution: RTE Model reports:\n\
ARM.RteTestRecursive.0.1.0: condition 'Recursive': error #503: direct or indirect recursion detected\n\
error csolution: no component was found with identifier 'RteTest:Check:Recursive'\n"},
    {"missing-condition", 0, "\
warning csolution: RTE Model reports:\n\
ARM.RteTestMissingCondition.0.1.0: component 'ARM::RteTest.Check.MissingCondition(MissingCondition):0.9.9[]': error #501: error(s) in component definition:\n\
 condition 'MissingCondition' not found\n"},
  };

  for (const auto& [project, expectedReturn, expectedMessage] : testData) {
    StdStreamRedirect streamRedirect;
    const string& csolution = testinput_folder + "/Validation/" + project + ".csolution.yml";
    const string& context = project + "+CM0";
    argv[3] = (char*)csolution.c_str();
    argv[5] = (char*)context.c_str();
    EXPECT_EQ(expectedReturn, RunProjMgr(6, argv, 0));
    auto errorStr = streamRedirect.GetErrorString();
    EXPECT_EQ(0, errorStr.find(expectedMessage));
  }
}

TEST_F(ProjMgrUnitTests, OutputDirs) {
  char* argv[4];
  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestSolution/outdirs.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  EXPECT_EQ(0, RunProjMgr(4, argv, 0));

  // Check generated CPRJs
 ProjMgrTestEnv:: CompareFile(testinput_folder + "/TestSolution/AC6/test1.Debug+TypeA.cprj",
    testinput_folder + "/TestSolution/ref/AC6/test1.Debug+TypeA.cprj");
 ProjMgrTestEnv:: CompareFile(testinput_folder + "/TestSolution/AC6/test1.Debug+TypeB.cprj",
    testinput_folder + "/TestSolution/ref/AC6/test1.Debug+TypeB.cprj");
 ProjMgrTestEnv:: CompareFile(testinput_folder + "/TestSolution/GCC/test1.Release+TypeA.cprj",
    testinput_folder + "/TestSolution/ref/GCC/test1.Release+TypeA.cprj");
 ProjMgrTestEnv:: CompareFile(testinput_folder + "/TestSolution/GCC/test1.Release+TypeB.cprj",
    testinput_folder + "/TestSolution/ref/GCC/test1.Release+TypeB.cprj");
}

TEST_F(ProjMgrUnitTests, OutputDirsAbsolutePath) {
  StdStreamRedirect streamRedirect;
  char* argv[4];
  const string& csolution = testinput_folder + "/TestSolution/outdirs-absolute.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  EXPECT_EQ(0, RunProjMgr(4, argv, 0));

  auto errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(regex_match(errStr, regex("warning csolution: custom .* is not a relative path\n")));
}

TEST_F(ProjMgrUnitTests, ProjectSetup) {
  char* argv[6];
  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestProjectSetup/setup-test.csolution.yml";
  const string& output = testoutput_folder;
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)output.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  // Check generated CPRJs
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/setup-test.Build AC6+TEST_TARGET.cprj",
    testinput_folder + "/TestProjectSetup/ref/setup-test.Build AC6+TEST_TARGET.cprj");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/setup-test.Build GCC+TEST_TARGET.cprj",
    testinput_folder + "/TestProjectSetup/ref/setup-test.Build GCC+TEST_TARGET.cprj");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_help) {
  char* argv[4];

  argv[1] = (char*)"-h";
  EXPECT_EQ(0, RunProjMgr(2, argv, 0));

  argv[1] = (char*)"--help";
  EXPECT_EQ(0, RunProjMgr(2, argv, 0));

  argv[1] = (char*)"run";
  argv[2] = (char*)"-h";
  EXPECT_EQ(0, RunProjMgr(3, argv, 0));

  argv[1] = (char*)"convert";
  argv[2] = (char*)"-h";
  EXPECT_EQ(0, RunProjMgr(3, argv, 0));

  argv[1] = (char*)"list";
  argv[2] = (char*)"packs";
  argv[3] = (char*)"-h";
  EXPECT_EQ(0, RunProjMgr(4, argv, 0));

  argv[1] = (char*)"list";
  argv[2] = (char*)"boards";
  argv[3] = (char*)"-h";
  EXPECT_EQ(0, RunProjMgr(4, argv, 0));

  argv[1] = (char*)"list";
  argv[2] = (char*)"devices";
  argv[3] = (char*)"-h";
  EXPECT_EQ(0, RunProjMgr(4, argv, 0));

  argv[1] = (char*)"list";
  argv[2] = (char*)"components";
  argv[3] = (char*)"-h";
  EXPECT_EQ(0, RunProjMgr(4, argv, 0));

  argv[1] = (char*)"list";
  argv[2] = (char*)"dependencies";
  argv[3] = (char*)"-h";
  EXPECT_EQ(0, RunProjMgr(4, argv, 0));

  argv[1] = (char*)"list";
  argv[2] = (char*)"contexts";
  argv[3] = (char*)"-h";
  EXPECT_EQ(0, RunProjMgr(4, argv, 0));

  argv[1] = (char*)"list";
  argv[2] = (char*)"generators";
  argv[3] = (char*)"-h";
  EXPECT_EQ(0, RunProjMgr(4, argv, 0));

  argv[1] = (char*)"list";
  argv[2] = (char*)"layers";
  argv[3] = (char*)"-h";
  EXPECT_EQ(0, RunProjMgr(4, argv, 0));

  argv[1] = (char*)"list";
  argv[2] = (char*)"toolchains";
  argv[3] = (char*)"-h";
  EXPECT_EQ(0, RunProjMgr(4, argv, 0));

  // invalid command
  argv[1] = (char*)"list";
  argv[2] = (char*)"invalid";
  argv[3] = (char*)"-h";
  EXPECT_EQ(1, RunProjMgr(4, argv, 0));

  // invalid command
  argv[1] = (char*)"test";
  argv[2] = (char*)"-h";
  EXPECT_EQ(1, RunProjMgr(3, argv, 0));

  // invalid command
  argv[1] = (char*)"--helped";
  EXPECT_EQ(1, RunProjMgr(2, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ExportNonLockedCprj) {
  char* argv[10];

  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestSolution/test_pack_selection.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"-c";
  argv[7] = (char*)"test2.Debug+TestGen";
  argv[8] = (char*)"-e";
  argv[9] = (char*)"_export";
  EXPECT_EQ(0, RunProjMgr(10, argv, 0));

  // Check generated CPRJs
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test2.Debug+TestGen_export.cprj",
    testinput_folder + "/TestSolution/ref/test2.Debug+TestGen_export.cprj");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_WriteCprjFail) {
  char* argv[10];

  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestSolution/test_pack_selection.csolution.yml";
  const string& outputFolder = testoutput_folder + "/outputFolder";

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)outputFolder.c_str();
  argv[6] = (char*)"-c";
  argv[7] = (char*)"test2.Debug+CM0";
  argv[8] = (char*)"-e";
  argv[9] = (char*)"_export";

  // Fail to write export cprj file
  RteFsUtils::CreateFile(outputFolder + "/test2.Debug+CM0_export.cprj", "");
  RteFsUtils::SetTreeReadOnly(outputFolder);
  EXPECT_EQ(1, RunProjMgr(10, argv, 0));

  // Fail to write cprj file
  RteFsUtils::SetTreeReadOnly(outputFolder);
  EXPECT_EQ(1, RunProjMgr(10, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_PreInclude) {
  char* argv[6];

  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestSolution/pre-include.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  // Check generated CPRJs
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/pre-include+CM0.cprj",
    testinput_folder + "/TestSolution/ref/pre-include+CM0.cprj");

  // Check generated cbuild YMLs
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/pre-include.cbuild-idx.yml",
    testinput_folder + "/TestSolution/ref/pre-include.cbuild-idx.yml");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/pre-include+CM0.cbuild.yml",
    testinput_folder + "/TestSolution/ref/pre-include+CM0.cbuild.yml");
}

TEST_F(ProjMgrUnitTests, RunCheckForContext) {
  const string& filenameInput = testinput_folder + "/TestSolution/contexts.csolution.yml";
  EXPECT_TRUE(m_parser.ParseCsolution(filenameInput, false));
  const CsolutionItem csolutionItem = m_parser.GetCsolution();
  const auto& contexts = csolutionItem.contexts;
  const string& cproject = "contexts.cproject.yml";
  const vector<ContextDesc> expectedVec = {
    {cproject, { {{"B1","T1"}}, { } }},
    {cproject, { { }, {{"B1","T2"}} }},
    {cproject, { {{"B2","T1"}}, { } }},
    {cproject, { { }, {{"B2","T2"}} }},
  };
  auto it = contexts.begin();
  for (const auto& expected : expectedVec) {
    ASSERT_EQ(expected.type.include.size(), (*it).type.include.size());
    ASSERT_EQ(expected.type.exclude.size(), (*it).type.exclude.size());
    if (!expected.type.include.empty()) {
      EXPECT_EQ(expected.type.include.front().build, (*it).type.include.front().build);
      EXPECT_EQ(expected.type.include.front().target, (*it).type.include.front().target);
    }
    if (!expected.type.exclude.empty()) {
      EXPECT_EQ(expected.type.exclude.front().build, (*it).type.exclude.front().build);
      EXPECT_EQ(expected.type.exclude.front().target, (*it).type.exclude.front().target);
    }
    it++;
  }
}

TEST_F(ProjMgrUnitTests, RunProjMgrOutputFiles) {
  char* argv[5];
  StdStreamRedirect streamRedirect;

  // convert solution.yml
  const string& csolution = testinput_folder + "/TestSolution/outputFiles.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(5, argv, 0));

  // Check generated CPRJs
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/outputFiles.Debug+Target.cprj",
    testinput_folder + "/TestSolution/ref/outputFiles.Debug+Target.cprj");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/outputFiles.Library+Target.cprj",
    testinput_folder + "/TestSolution/ref/outputFiles.Library+Target.cprj");

  // Check generated cbuild YMLs
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/outputFiles.Debug+Target.cbuild.yml",
    testinput_folder + "/TestSolution/ref/outputFiles.Debug+Target.cbuild.yml");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/outputFiles.Library+Target.cbuild.yml",
    testinput_folder + "/TestSolution/ref/outputFiles.Library+Target.cbuild.yml");

  // Check error messages
  const string expected = "\
error csolution: redefinition from 'conflict' into 'renaming_conflict' is not allowed\n\
error csolution: processing context 'outputFiles.BaseNameConflict\\+Target' failed\n\
error csolution: output 'lib' is incompatible with other output types\n\
error csolution: processing context 'outputFiles.TypeConflict\\+Target' failed\n\
";

  auto errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(regex_match(errStr, regex(expected)));
}

TEST_F(ProjMgrUnitTests, SelectToolchains) {
  const string& AC6_6_6_5 = testinput_folder + "/TestToolchains/AC6.6.6.5.cmake";
  RteFsUtils::CreateFile(AC6_6_6_5, "");
  char* envp[6];
  string ac6_0 = "AC6_TOOLCHAIN_6_20_0=" + testinput_folder;
  string ac6_1 = "AC6_TOOLCHAIN_6_16_1=" + testinput_folder;
  string ac6_2 = "AC6_TOOLCHAIN_6_6_5=" + testinput_folder;
  string gcc = "GCC_TOOLCHAIN_11_2_1=" + testinput_folder;
  string iar = "IAR_TOOLCHAIN_9_32_1=" + testinput_folder;
  envp[0] = (char*)ac6_0.c_str();
  envp[1] = (char*)ac6_1.c_str();
  envp[2] = (char*)ac6_2.c_str();
  envp[3] = (char*)gcc.c_str();
  envp[4] = (char*)iar.c_str();
  envp[5] = (char*)'\0';
  char* argv[8];
  const string& csolution = testinput_folder + "/TestSolution/toolchain-selection.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"-t";

  argv[7] = (char*)"AC6@6.20.0";
  EXPECT_EQ(0, RunProjMgr(8, argv, envp));
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/toolchain.Debug+Target.cprj",
    testinput_folder + "/TestSolution/ref/toolchains/toolchain.Debug+Target.cprj.ac6_6_20_0");

  argv[7] = (char*)"AC6@6.16.1";
  EXPECT_EQ(0, RunProjMgr(8, argv, envp));
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/toolchain.Debug+Target.cprj",
    testinput_folder + "/TestSolution/ref/toolchains/toolchain.Debug+Target.cprj.ac6_6_16_1");

  argv[7] = (char*)"AC6@6.6.5";
  EXPECT_EQ(0, RunProjMgr(8, argv, envp));
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/toolchain.Debug+Target.cprj",
    testinput_folder + "/TestSolution/ref/toolchains/toolchain.Debug+Target.cprj.ac6_6_6_5");

  argv[7] = (char*)"GCC";
  EXPECT_EQ(0, RunProjMgr(8, argv, envp));
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/toolchain.Debug+Target.cprj",
    testinput_folder + "/TestSolution/ref/toolchains/toolchain.Debug+Target.cprj.gcc");

  argv[7] = (char*)"IAR";
  EXPECT_EQ(0, RunProjMgr(8, argv, envp));
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/toolchain.Debug+Target.cprj",
    testinput_folder + "/TestSolution/ref/toolchains/toolchain.Debug+Target.cprj.iar");

  RteFsUtils::RemoveFile(AC6_6_6_5);
}

TEST_F(ProjMgrUnitTests, RunProjMgr_LinkerOptions) {
  char* argv[7];
  const string& csolution = testinput_folder + "/TestSolution/LinkerOptions/linker.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-c";
  argv[4] = (char*)"linker.Debug_*+RteTest_ARMCM3";
  argv[5] = (char*)"-o";
  argv[6] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(7, argv, 0));

  // Check generated CPRJs
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/linker.Debug_AC6+RteTest_ARMCM3.cprj",
    testinput_folder + "/TestSolution/LinkerOptions/ref/linker.Debug_AC6+RteTest_ARMCM3.cprj");
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/linker.Debug_GCC+RteTest_ARMCM3.cprj",
    testinput_folder + "/TestSolution/LinkerOptions/ref/linker.Debug_GCC+RteTest_ARMCM3.cprj");

  // Check generated cbuild YMLs
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/linker.Debug_AC6+RteTest_ARMCM3.cbuild.yml",
    testinput_folder + "/TestSolution/LinkerOptions/ref/linker.Debug_AC6+RteTest_ARMCM3.cbuild.yml");
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/linker.Debug_GCC+RteTest_ARMCM3.cbuild.yml",
    testinput_folder + "/TestSolution/LinkerOptions/ref/linker.Debug_GCC+RteTest_ARMCM3.cbuild.yml");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_LinkerPriority) {
  char* argv[7];
  const string& csolution = testinput_folder + "/TestSolution/LinkerOptions/linker.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-c";
  argv[4] = (char*)"linker.Priority*+RteTest_ARMCM3";
  argv[5] = (char*)"-o";
  argv[6] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(7, argv, 0));

  // Check generated cbuild YMLs
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/linker.PriorityRegions+RteTest_ARMCM3.cbuild.yml",
    testinput_folder + "/TestSolution/LinkerOptions/ref/linker.PriorityRegions+RteTest_ARMCM3.cbuild.yml");
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/linker.PriorityDefines+RteTest_ARMCM3.cbuild.yml",
    testinput_folder + "/TestSolution/LinkerOptions/ref/linker.PriorityDefines+RteTest_ARMCM3.cbuild.yml");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_LinkerOptions_Redefinition) {
  char* argv[7];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestSolution/LinkerOptions/linker.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-c";
  argv[4] = (char*)"linker.Redefinition+RteTest_ARMCM3";
  argv[5] = (char*)"-o";
  argv[6] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(7, argv, 0));

  // Check error messages
  const string expected = "\
error csolution: redefinition from '.*/linkerScript.ld' into '.*/linkerScript2.ld' is not allowed\n\
error csolution: processing context 'linker.Redefinition\\+RteTest_ARMCM3' failed\n\
";

  auto errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(regex_match(errStr, regex(expected)));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_StandardLibrary) {
  char* argv[7];
  const string& csolution = testinput_folder + "/TestSolution/StandardLibrary/library.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-c";
  argv[4] = (char*)"library.Debug+RteTest_ARMCM3";
  argv[5] = (char*)"-o";
  argv[6] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(7, argv, 0));

  // Check generated CPRJs
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/library.Debug+RteTest_ARMCM3.cprj",
    testinput_folder + "/TestSolution/StandardLibrary/ref/library.Debug+RteTest_ARMCM3.cprj");

  // Check generated cbuild YMLs
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/library.Debug+RteTest_ARMCM3.cbuild.yml",
    testinput_folder + "/TestSolution/StandardLibrary/ref/library.Debug+RteTest_ARMCM3.cbuild.yml");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_MultipleProject_SameFolder) {
  char* argv[5];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestSolution/multiple.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(5, argv, 0));

  // Check warning message
  const string expected = "\
.*/TestSolution/multiple.csolution.yml - warning csolution: cproject.yml files should be placed in separate sub-directories\n\
.*\n";

  auto errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(regex_match(errStr, regex(expected)));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListEnvironment) {
  auto GetValue = [](const string& inputStr, const string& subStr) -> string{
    auto startPos = inputStr.find(subStr, 0);
    if (startPos == string::npos) {
      return RteUtils::EMPTY_STRING;
    }
    startPos += subStr.length();
    auto endPos = inputStr.find_first_of("\n", startPos);
    if (endPos == string::npos) {
      return RteUtils::EMPTY_STRING;
    }
    return inputStr.substr(startPos, endPos - startPos);
  };

  //backup env variables
  const auto pack_Root = CrossPlatformUtils::GetEnv("CMSIS_PACK_ROOT");
  const auto compiler_Root = CrossPlatformUtils::GetEnv("CMSIS_COMPILER_ROOT");

  char* argv[3];
  StdStreamRedirect streamRedirect;
  // list environment
  argv[1] = (char*)"list";
  argv[2] = (char*)"environment";
  EXPECT_EQ(0, RunProjMgr(3, argv, 0));

  // Test 1
  auto outStr = streamRedirect.GetOutString();
  EXPECT_EQ(pack_Root, GetValue(outStr, "CMSIS_PACK_ROOT="));
  EXPECT_EQ(compiler_Root, GetValue(outStr, "CMSIS_COMPILER_ROOT="));

  // Test 2 :set CMSIS_PACK_ROOT & CMSIS_COMPILER_ROOT empty
  CrossPlatformUtils::SetEnv("CMSIS_PACK_ROOT", RteUtils::EMPTY_STRING);
  CrossPlatformUtils::SetEnv("CMSIS_COMPILER_ROOT", RteUtils::EMPTY_STRING);

  error_code ec;
  string localCompilerPath, defaultPackRoot;
  streamRedirect.ClearStringStreams();
  EXPECT_EQ(0, RunProjMgr(3, argv, 0));
  outStr = streamRedirect.GetOutString();
  localCompilerPath = string(PROJMGRUNITTESTS_BIN_PATH) + "/../etc";
  localCompilerPath = fs::weakly_canonical(fs::path(localCompilerPath), ec).generic_string();
  defaultPackRoot = CrossPlatformUtils::GetDefaultCMSISPackRootDir();
  defaultPackRoot = fs::weakly_canonical(fs::path(defaultPackRoot), ec).generic_string();
  EXPECT_EQ(defaultPackRoot, GetValue(outStr, "CMSIS_PACK_ROOT="));
  EXPECT_EQ(localCompilerPath, GetValue(outStr, "CMSIS_COMPILER_ROOT="));

  // Restore env variables
  CrossPlatformUtils::SetEnv("CMSIS_PACK_ROOT", pack_Root);
  CrossPlatformUtils::SetEnv("CMSIS_COMPILER_ROOT", compiler_Root);
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ContextMap) {
  char* argv[7];
  const string& csolution = testinput_folder + "/TestSolution/ContextMap/context-map.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-c";
  argv[4] = (char*)"*";
  argv[5] = (char*)"-o";
  argv[6] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(7, argv, 0));

  // Check generated cbuild YMLs
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/project1.Debug+RteTest_ARMCM3.cbuild.yml",
    testinput_folder + "/TestSolution/ContextMap/ref/project1.Debug+RteTest_ARMCM3.cbuild.yml");
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/project1.Release+RteTest_ARMCM3.cbuild.yml",
    testinput_folder + "/TestSolution/ContextMap/ref/project1.Release+RteTest_ARMCM3.cbuild.yml");
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/project2.Debug+RteTest_ARMCM3.cbuild.yml",
    testinput_folder + "/TestSolution/ContextMap/ref/project2.Debug+RteTest_ARMCM3.cbuild.yml");
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/project2.Release+RteTest_ARMCM3.cbuild.yml",
    testinput_folder + "/TestSolution/ContextMap/ref/project2.Release+RteTest_ARMCM3.cbuild.yml");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_UpdateRte) {
  char* argv[8];
  const string rteDir = testinput_folder + "/TestSolution/TestProject1/RTE/";
  const string configFile = rteDir + "Device/RteTest_ARMCM0/startup_ARMCM0.c";
  const string baseFile = configFile + ".base@1.1.1";
  RteFsUtils::RemoveDir(rteDir);
  RteFsUtils::CreateFile(configFile, "");
  RteFsUtils::CreateFile(baseFile, "");

  StdStreamRedirect streamRedirect;
  string csolutionFile = testinput_folder + "/TestSolution/test.csolution.yml";
  argv[1] = (char*)"update-rte";
  argv[2] = (char*)csolutionFile.c_str();
  argv[3] = (char*)"-c";
  argv[4] = (char*)"test1.Debug+CM0";
  argv[5] = (char*)"-o";
  argv[6] = (char*)testoutput_folder.c_str();
  argv[7] = (char*)"-v";
  EXPECT_EQ(0, RunProjMgr(8, argv, 0));
  EXPECT_TRUE(RteFsUtils::Exists(rteDir + "/_Debug_CM0/RTE_Components.h"));

  // Check info message
  const string expected = "\
info csolution: config files for each component:\n\
  ARM::Device:Startup&RteTest Startup@2.0.3:\n\
    - .*/TestSolution/TestProject1/RTE/Device/RteTest_ARMCM0/ARMCM0_ac6.sct \\(base@1.0.0\\)\n\
    - .*/TestSolution/TestProject1/RTE/Device/RteTest_ARMCM0/startup_ARMCM0.c \\(base@1.1.1\\) \\(update@2.0.3\\)\n\
    - .*/TestSolution/TestProject1/RTE/Device/RteTest_ARMCM0/system_ARMCM0.c \\(base@1.0.0\\)\n\
";

  auto outStr = streamRedirect.GetOutString();
  EXPECT_TRUE(regex_match(outStr, regex(expected)));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_No_target_Types) {
  StdStreamRedirect streamRedirect;
  char* argv[7];
  const string& csolutionFile = testinput_folder + "/TestSolution/missing_target_types.csolution.yml";

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"-n";
  EXPECT_EQ(1, RunProjMgr(7, argv, 0));

  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find("target-types not found"));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_No_Projects) {
  StdStreamRedirect streamRedirect;
  char* argv[7];
  const string& csolutionFile = testinput_folder + "/TestSolution/missing_projects.csolution.yml";

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"-n";
  EXPECT_EQ(1, RunProjMgr(7, argv, 0));

  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find("projects not found"));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_RteDir) {
  char* argv[5];
  const string& csolutionFile = testinput_folder + "/TestSolution/rtedir.csolution.yml";

  argv[1] = (char*)"convert";
  argv[2] = (char*)csolutionFile.c_str();
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(5, argv, 0));

  // Check generated cbuild YMLs
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/rtedir.Debug+CM0.cbuild.yml",
    testinput_folder + "/TestSolution/ref/rtedir.Debug+CM0.cbuild.yml");
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/rtedir.Release+CM0.cbuild.yml",
    testinput_folder + "/TestSolution/ref/rtedir.Release+CM0.cbuild.yml");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Invalid_Target_Build_Type) {
  const char* invalid_target_types_Ex1 = "\
solution:\n\
  target-types:\n\
    - type: CM0+Test\n\
    - type: CM3.Test\n\
  build-types:\n\
    - type: Debug\n\
    - type: Release\n\
  projects:\n\
    - project: ./TestProject2/test2.cproject.yml\n\
    - project: ./TestProject1/test1.cproject.yml\n\
";

  const char* invalid_build_types_Ex2 = "\
solution:\n\
  target-types:\n\
    - type: CM0\n\
    - type: CM3\n\
  build-types:\n\
    - type: Debug.Test\n\
    - type: Release+Test\n\
  projects:\n\
    - project: ./TestProject2/test2.cproject.yml\n\
    - project: ./TestProject1/test1.cproject.yml\n\
";

  auto writeFile = [](const string& filePath, const char* data) {
    ofstream fileStream(filePath);
    fileStream << data;
    fileStream << endl;
    fileStream << flush;
    fileStream.close();
  };

  vector<std::tuple<const char*, int, string>> vecTestData = {
    // data, expectedRetVal, errorMsg
    {invalid_target_types_Ex1,  1, "invalid target type(s)"},
    {invalid_build_types_Ex2, 1, "invalid build type(s)"},
  };

  const string& csolutionFile = testinput_folder +
    "/TestSolution/test_invalid_target_type.csolution.yml";

  for (auto [data, expectRetVal, expectedErrMsg] : vecTestData) {
    writeFile(csolutionFile, data);

    StdStreamRedirect streamRedirect;
    char* argv[5];
    argv[1] = (char*)"list";
    argv[2] = (char*)"contexts";
    argv[3] = (char*)csolutionFile.c_str();
    argv[4] = (char*)"-n";

    EXPECT_EQ(expectRetVal, RunProjMgr(5, argv, 0));
    auto errStr = streamRedirect.GetErrorString();
    EXPECT_NE(string::npos, errStr.find(expectedErrMsg));
  }
}

TEST_F(ProjMgrUnitTests, RunProjMgrInvalidContext) {
  StdStreamRedirect streamRedirect;
  char* argv[10];

  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestSolution/test.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"-c";
  argv[7] = (char*)"test2.Debug+CM0";
  argv[8] = (char*)"-c";
  argv[9] = (char*)"test3*";

  EXPECT_EQ(1, RunProjMgr(10, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find("following context(s) was not found:\n  test3*"));
}

TEST_F(ProjMgrUnitTests, RunProjMgrCovertMultipleContext) {
  StdStreamRedirect streamRedirect;
  char* argv[10];

  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestSolution/test.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"-c";
  argv[7] = (char*)"test2.Debug+CM0";
  argv[8] = (char*)"-c";
  argv[9] = (char*)"test1.Release+CM0";

  EXPECT_EQ(0, RunProjMgr(10, argv, 0));
  auto outStr = streamRedirect.GetOutString();
  EXPECT_NE(string::npos, outStr.find("test2.Debug+CM0.cprj"));
  EXPECT_NE(string::npos, outStr.find("test1.Release+CM0.cprj"));
  EXPECT_EQ(string::npos, outStr.find("test1.Debug+CM0.cprj"));
  EXPECT_EQ(string::npos, outStr.find("test2.Debug+CM3.cprj"));
}

/*
* This test case runs the ProjMgrYamlEmitter with a solution
* that references a project using different file name case.
*/
TEST_F(ProjMgrUnitTests, RunProjMgr_YamlEmitterFileCaseIssue) {
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestSolution/FilenameCase/filename.csolution.yml";
  const string& expectedErrMsg = "cproject filename has case inconsistency";

  char* argv[5];
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();

  EXPECT_EQ(1, RunProjMgr(5, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expectedErrMsg));
}
