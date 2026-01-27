/*
 * Copyright (c) 2020-2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgr.h"
#include "ProjMgrTestEnv.h"
#include "ProjMgrYamlSchemaChecker.h"
#include "ProjMgrLogger.h"

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

  void SetUp() {
    m_context.clear();
  };

  void TearDown() {
    // reserved
  }

  void GetFilesInTree(const string& dir, set<string>& files) {
    error_code ec;
    if (RteFsUtils::Exists(dir)) {
      for (auto& p : fs::recursive_directory_iterator(dir, ec)) {
        files.insert(p.path().filename().generic_string());
      }
    }
  }

  void CompareFileTree(const string& dir1, const string& dir2) {
    set<string> tree1, tree2;
    GetFilesInTree(dir1, tree1);
    GetFilesInTree(dir2, tree2);
    EXPECT_EQ(tree1, tree2);
  }

  void RemoveCbuildSetFile(const string& csolutionFile) {
    string fileName = fs::path(csolutionFile).filename().generic_string();
    fileName = RteUtils::ExtractPrefix(fileName, ".csolution.");
    const string& cbuildSetFile = fs::path(csolutionFile).parent_path().generic_string() +
      "/" + fileName + ".cbuild-set.yml";
    if (RteFsUtils::Exists(cbuildSetFile)) {
      RteFsUtils::RemoveFile(cbuildSetFile);
    }
  }

  string UpdateTestSolutionFile(const string& projectFilePath) {
    string csolutionFile = testinput_folder + "/TestSolution/test_validate_project.csolution.yml";
    RemoveCbuildSetFile(csolutionFile);

    YAML::Node root = YAML::LoadFile(csolutionFile);
    root["solution"]["projects"][0]["project"] = projectFilePath;
    std::ofstream fout(csolutionFile);
    fout << root;
    fout.close();
    return csolutionFile;
  }
};

TEST_F(ProjMgrUnitTests, Validate_Logger) {
  StdStreamRedirect streamRedirect;
  auto printLogMsgs = []() {
    ProjMgrLogger::Debug("debug-1 test message");
    ProjMgrLogger::Get().Warn("warning-1 test message");
    ProjMgrLogger::Get().Warn("warning-2 test message", "", "test.warn");
    ProjMgrLogger::Get().Warn("warning-3 test message", "", "test.warn", 1, 1);
    ProjMgrLogger::Get().Error("error-1 test message");
    ProjMgrLogger::Get().Error("error-2 test message", "", "test.err");
    ProjMgrLogger::Get().Error("error-3 test message", "", "test.err", 1, 1);
    ProjMgrLogger::Get().Info("info-1 test message");
    ProjMgrLogger::Get().Info("info-2 test message", "", "test.info");
    ProjMgrLogger::Get().Info("info-3 test message", "", "test.info", 1, 1);
    ProjMgrLogger::out() << "cout test message" << endl;
  };

  auto& ss = ProjMgrLogger::Get().GetStringStream();
  // Test quite mode
  ProjMgrLogger::m_quiet = true;
  string expErrMsg = "error csolution: error-1 test message\n\
test.err - error csolution: error-2 test message\n\
test.err:1:1 - error csolution: error-3 test message\n";
  string expOutMsg = "cout test message\n";

  printLogMsgs();
  auto outStr = streamRedirect.GetOutString();
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_STREQ(outStr.c_str(), expOutMsg.c_str());
  EXPECT_STREQ(errStr.c_str(), expErrMsg.c_str());
  EXPECT_TRUE(ss.str().empty());
  EXPECT_EQ(ProjMgrLogger::Get().GetWarnsForContext().size(), 3);
  EXPECT_EQ(ProjMgrLogger::Get().GetInfosForContext().size(), 3);
  EXPECT_EQ(ProjMgrLogger::Get().GetErrorsForContext().size(), 3);

  // Test non-quite mode
  ProjMgrLogger::Get().Clear();
  ProjMgrLogger::m_quiet = false;
  streamRedirect.ClearStringStreams();
  expErrMsg = "debug csolution: debug-1 test message\n\
warning csolution: warning-1 test message\n\
test.warn - warning csolution: warning-2 test message\n\
test.warn:1:1 - warning csolution: warning-3 test message\n\
error csolution: error-1 test message\n\
test.err - error csolution: error-2 test message\n\
test.err:1:1 - error csolution: error-3 test message\n";
  expOutMsg = "info csolution: info-1 test message\n\
test.info - info csolution: info-2 test message\n\
test.info:1:1 - info csolution: info-3 test message\n\
cout test message\n";

  printLogMsgs();
  outStr = streamRedirect.GetOutString();
  errStr = streamRedirect.GetErrorString();
  EXPECT_STREQ(outStr.c_str(), expOutMsg.c_str());
  EXPECT_STREQ(errStr.c_str(), expErrMsg.c_str());
  EXPECT_EQ(ProjMgrLogger::Get().GetWarnsForContext().size(), 3);
  EXPECT_EQ(ProjMgrLogger::Get().GetInfosForContext().size(), 3);
  EXPECT_EQ(ProjMgrLogger::Get().GetErrorsForContext().size(), 3);
  EXPECT_TRUE(ss.str().empty());

  // Test silent mode
  ProjMgrLogger::Get().Clear();
  ProjMgrLogger::m_silent = true;
  streamRedirect.ClearStringStreams();
  expErrMsg = "";
  expOutMsg = "";

  printLogMsgs();
  outStr = streamRedirect.GetOutString();
  errStr = streamRedirect.GetErrorString();
  EXPECT_STREQ(outStr.c_str(), expOutMsg.c_str());
  EXPECT_STREQ(errStr.c_str(), expErrMsg.c_str());
  EXPECT_STREQ(ss.str().c_str(), "cout test message\n");
  EXPECT_EQ(ProjMgrLogger::Get().GetWarnsForContext().size(), 3);
  EXPECT_EQ(ProjMgrLogger::Get().GetInfosForContext().size(), 3);
  EXPECT_EQ(ProjMgrLogger::Get().GetErrorsForContext().size(), 3);

  ProjMgrLogger::Get().Clear();
  EXPECT_EQ(ProjMgrLogger::Get().GetWarnsForContext().size(), 0);
  EXPECT_EQ(ProjMgrLogger::Get().GetInfosForContext().size(), 0);
  EXPECT_EQ(ProjMgrLogger::Get().GetErrorsForContext().size(), 0);
  EXPECT_TRUE(ss.str().empty());
  // return mode to normal to avoid affecting other tests
  ProjMgrLogger::m_silent = false;
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

TEST_F(ProjMgrUnitTests, RunProjMgr_Packs_Required_Warning) {
  StdStreamRedirect streamRedirect;
  const vector<string> warnings = {
    "pack 'ARM::RteTest_DFP@0.1.1:0.2.0' required by pack 'ARM::RteTest@0.1.0' is not specified",
    "pack 'ARM::RteTestRequiredRecursive@1.0.0:2.0.0' required by pack 'ARM::RteTestRequired@1.0.1-local' is not specified",
    "pack 'ARM::RteTest_DFP@0.1.1:0.2.0' required by pack 'ARM::RteTestRequired@1.0.1-local' is not specified"
  };

  const string csolution = testinput_folder + "/TestSolution/test_pack_requirements.csolution.yml";
  char* argv[9];

  //  list packs
  argv[1] = (char*)"list";
  argv[2] = (char*)"packs";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  EXPECT_EQ(0, RunProjMgr(5, argv, m_envp));
  // no warnings by default
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_FALSE(errStr.find(warnings[1]) != string::npos);
  streamRedirect.ClearStringStreams();

  argv[5] = (char*)"-d";
  EXPECT_EQ(0, RunProjMgr(6, argv, m_envp));
  // warnings in debug mode
  errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(errStr.find(warnings[1]) != string::npos);
  streamRedirect.ClearStringStreams();
// convert
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"-c";
  argv[7] = (char*)"test1.Debug+CM0";
  EXPECT_EQ(1, RunProjMgr(8, argv, m_envp)); //fails because DFP is not loaded => pack warnings (disabled)
  errStr = streamRedirect.GetErrorString();
  // pack warnings are not printed
  for(auto& w : warnings) {
    EXPECT_FALSE(errStr.find(w) != string::npos);
  }
  streamRedirect.ClearStringStreams();
  argv[8] = (char*)"-d";
  EXPECT_EQ(1, RunProjMgr(9, argv, m_envp)); //fails because DFP is not loaded => pack warnings (enabled)
  errStr = streamRedirect.GetErrorString();
  // pack warnings are printed
  for(auto& w : warnings) {
    EXPECT_TRUE(errStr.find(w) != string::npos);
  }
  streamRedirect.ClearStringStreams();
  argv[7] = (char*)"test1.Release+CM0";
  EXPECT_EQ(0, RunProjMgr(8, argv, m_envp)); // succeeds regardless missing pack requirement  => no pack warnings
  errStr = streamRedirect.GetErrorString();
  for(auto& w : warnings) {
    EXPECT_FALSE(errStr.find(w) != string::npos);
  }
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Incompatible_Packs_Required_Warning) {
  StdStreamRedirect streamRedirect;
  const string warning = {
    "pack 'ARM::RteTest_DFP@3.0.0' required by pack 'ARM::RteTestRequired@1.0.0' is not specified"
  };
  const string csolution = testinput_folder + "/TestSolution/PackRequirements/incompatible.csolution.yml";
  char* argv[3];
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  EXPECT_EQ(0, RunProjMgr(3, argv, m_envp));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(errStr.find(warning) != string::npos);
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListPacks) {
  map<std::pair<string, string>, string> testInputs = {
    {{"TestSolution/test.csolution.yml", "test1.Debug+CM0"}, "ARM::RteTest_DFP@0.2.0" },
      // packs are specified only with vendor
    {{"TestSolution/test_filtered_pack_selection.csolution.yml", "test1.Debug+CM0"},  "ARM::*"},
      // packs are specified with wildcards
    {{"TestSolution/test_filtered_pack_selection.csolution.yml", "test1.Release+CM0"}, "ARM::RteTest_DFP@0.2.0"},
      // packs are not specified
    {{"TestSolution/test_no_packs.csolution.yml", "test1.Debug+CM0"}, "*"},
      // packs are fully specified
    {{"TestSolution/test_pack_selection.csolution.yml", "test2.Debug+CM0"}, "ARM::RteTest_DFP@0.2.0"}
  };
  auto pdscFiles = ProjMgrTestEnv::GetEffectivePdscFiles(true);

  // positive tests
  char* argv[7];
  argv[1] = (char*)"list";
  argv[2] = (char*)"packs";
  argv[3] = (char*)"--solution";

  for (const auto& [input, ids] : testInputs) {
    StdStreamRedirect streamRedirect;
    const string& csolution = testinput_folder + "/" + input.first;
    argv[4] = (char*)csolution.c_str();
    argv[5] = (char*)"-c";
    argv[6] = (char*)input.second.c_str();
    EXPECT_EQ(0, RunProjMgr(7, argv, 0));

    auto outStr = streamRedirect.GetOutString();
    string expected = ProjMgrTestEnv::GetFilteredPacksString(pdscFiles, ids);

    EXPECT_TRUE(outStr == expected) << "error listing pack for " << csolution << endl;
  }

  map<std::pair<string, string>, string> testFalseInputs = {
    {{"TestSolution/test.csolution_unknown_file.yml", "test1.Debug+CM0"},
      "error csolution: csolution file was not found"},
    {{"TestSolution/test.csolution.yml", "invalid.context"},
      "no matching context found for option:\n  --context invalid.context"}
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
    auto outStr = streamRedirect.GetOutString();
    EXPECT_NE(string::npos, errStr.find(expected)) << "error listing pack for " << csolution << endl;
  }
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListPacks_1) {
  char* argv[3];
  StdStreamRedirect streamRedirect;
  auto pdscFiles = ProjMgrTestEnv::GetEffectivePdscFiles(true);
  string expected = ProjMgrTestEnv::GetFilteredPacksString(pdscFiles, "*");
  // list packs
  argv[1] = (char*)"list";
  argv[2] = (char*)"packs";
  EXPECT_EQ(0, RunProjMgr(3, argv, 0));

  auto outStr = streamRedirect.GetOutString();
  EXPECT_EQ(outStr, expected);
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

  auto pdscFiles = ProjMgrTestEnv::GetEffectivePdscFiles(true);
  string expected = ProjMgrTestEnv::GetFilteredPacksString(pdscFiles, "ARM::RteTestGenerator@0.1.0;ARM::RteTest_DFP@0.2.0");

  auto outStr = streamRedirect.GetOutString();
  EXPECT_EQ(outStr, expected);

  argv[7] = (char*)"-l";
  argv[8] = (char*)"latest";
  streamRedirect.ClearStringStreams();
  EXPECT_EQ(0, RunProjMgr(9, argv, 0));

  string expectedLatest = ProjMgrTestEnv::GetFilteredPacksString(pdscFiles, "*");

  outStr = streamRedirect.GetOutString();
  EXPECT_EQ(outStr, expectedLatest);

  argv[7] = (char*)"-l";
  argv[8] = (char*)"all";
  streamRedirect.ClearStringStreams();
  EXPECT_EQ(0, RunProjMgr(9, argv, 0));

  pdscFiles = ProjMgrTestEnv::GetEffectivePdscFiles(false);
  string expectedAll = ProjMgrTestEnv::GetFilteredPacksString(pdscFiles, "*");

  outStr = streamRedirect.GetOutString();
  EXPECT_EQ(outStr, expectedAll);
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListPacksAll) {
  char* argv[5];
  StdStreamRedirect streamRedirect;
  argv[1] = (char*)"list";
  argv[2] = (char*)"packs";
  argv[3] = (char*)"-l";
  argv[4] = (char*)"all";
  EXPECT_EQ(0, RunProjMgr(5, argv, 0));

  auto pdscFiles = ProjMgrTestEnv::GetEffectivePdscFiles(false);
  string expectedAll = ProjMgrTestEnv::GetFilteredPacksString(pdscFiles, "*");

  auto outStr = streamRedirect.GetOutString();
  EXPECT_EQ(outStr, expectedAll);
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListPacksMissing_1) {
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
  EXPECT_EQ(0, RunProjMgr(8, argv, 0)); // code should return success because of "-m" option

  auto outStr = streamRedirect.GetOutString();
  EXPECT_STREQ(outStr.c_str(), "ARM::Missing_DFP@0.0.9\n");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListPacksMissing_2) {
  char* argv[6];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestSolution/pack_missing_for_context.csolution.yml";
  // list packs
  argv[1] = (char*)"list";
  argv[2] = (char*)"packs";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  argv[5] = (char*)"-m";
  EXPECT_EQ(0, RunProjMgr(6, argv, 0)); // code should return success because of "-m" option

  auto outStr = streamRedirect.GetOutString();
  EXPECT_STREQ(outStr.c_str(), "ARM::Missing_DFP@0.0.9\n");
}

TEST_F(ProjMgrUnitTests, ListPacks_ProjectAndLayer) {
  char* argv[5];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestLayers/packs.csolution.yml";

  // list packs
  argv[1] = (char*)"list";
  argv[2] = (char*)"packs";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  EXPECT_EQ(0, RunProjMgr(5, argv, 0));

  auto pdscFiles = ProjMgrTestEnv::GetEffectivePdscFiles(false);
  string expected = ProjMgrTestEnv::GetFilteredPacksString(pdscFiles, "ARM::RteTest@0.1.0;ARM::RteTestBoard@0.1.0;ARM::RteTest_DFP@0.2.0");

  auto outStr = streamRedirect.GetOutString();
  EXPECT_EQ(outStr, expected);
}

TEST_F(ProjMgrUnitTests, ListPacks_Path) {
  char* argv[6];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestSolution/test_pack_path.csolution.yml";

  // list packs
  argv[1] = (char*)"list";
  argv[2] = (char*)"packs";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  argv[5] = (char*)"-R";
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  const string expected =
    "ARM::RteTest@0.1.0 (${CMSIS_PACK_ROOT}/ARM/RteTest/0.1.0/ARM.RteTest.pdsc)\n" \
    "ARM::RteTestRequired@1.1.0 (./Packs/RteTestRequired1/ARM.RteTestRequired.pdsc)\n" \
    "ARM::RteTestRequired@1.0.0 (./Packs/RteTestRequired/ARM.RteTestRequired.pdsc)\n" \
    "ARM::RteTest_DFP@0.2.0 (${CMSIS_PACK_ROOT}/ARM/RteTest_DFP/0.2.0/ARM.RteTest_DFP.pdsc)\n";
  auto outStr = streamRedirect.GetOutString();
  EXPECT_EQ(outStr, expected);
}


TEST_F(ProjMgrUnitTests, RunProjMgr_ListBoards) {
  char* argv[5];
  StdStreamRedirect streamRedirect;

  // list boards
  argv[1] = (char*)"list";
  argv[2] = (char*)"boards";
  argv[3] = (char*)"--filter";
  argv[4] = (char*)"DUMMY";
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
  argv[4] = (char*)"RTETest_ARMCM4";
  EXPECT_EQ(0, RunProjMgr(5, argv, 0));

  auto outStr = streamRedirect.GetOutString();
  EXPECT_STREQ(outStr.c_str(),"\
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
  EXPECT_EQ(0, RunProjMgr(6, argv, m_envp));

  GetFilesInTree(rteFolder, rteFilesAfter);
  EXPECT_EQ(rteFilesBefore, rteFilesAfter);

  auto outStr = streamRedirect.GetOutString();
  EXPECT_STREQ(outStr.c_str(), expectedOut.c_str());

  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(errStr.find(expectedErr), string::npos);
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ConvertProject_1) {
  char* argv[7];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test.cproject.yml");

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

  // Check generated CPRJ
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test+TEST_TARGET.cprj",
    testinput_folder + "/TestSolution/TestProject4/test+TEST_TARGET.cprj");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ConvertProject_2) {
  char* argv[7];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test.cproject.yml");

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-O";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

  // Check generated CPRJ
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/test+TEST_TARGET.cprj",
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
  EXPECT_EQ(0, RunProjMgr(6, argv, m_envp));

  // Check generated CPRJ
  //ProjMgrTestEnv::CompareFile(testoutput_folder + "/test.cprj",
  //  testinput_folder + "/TestSolution/TestProject4/test.cprj");
}


TEST_F(ProjMgrUnitTests, RunProjMgr_LinkerScript) {
  char* argv[7];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_linker_script.cproject.yml");

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

  // Check generated CPRJ
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test_linker_script+TEST_TARGET.cprj",
    testinput_folder + "/TestSolution/TestProject4/test_linker_script+TEST_TARGET.cprj");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_With_Schema_Check) {
  char* argv[7];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_invalid_schema.cproject.yml");
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(7, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Skip_Schema_Check) {
  char* argv[8];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_invalid_schema.cproject.yml");
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"-n";
  argv[7] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(8, argv, m_envp));

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
  argv[6] = (char*)"TEST1";
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
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

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

   // Check schema
   EXPECT_TRUE(ProjMgrYamlSchemaChecker().Validate(testoutput_folder + "/test.cbuild-idx.yml"));
   EXPECT_TRUE(ProjMgrYamlSchemaChecker().Validate(testoutput_folder + "/test1.Debug+CM0.cbuild.yml"));
   EXPECT_TRUE(ProjMgrYamlSchemaChecker().Validate(testoutput_folder + "/test1.Release+CM0.cbuild.yml"));
   EXPECT_TRUE(ProjMgrYamlSchemaChecker().Validate(testoutput_folder + "/test1.Debug+CM0.cbuild.yml"));
   EXPECT_TRUE(ProjMgrYamlSchemaChecker().Validate(testoutput_folder + "/test2.Debug+CM0.cbuild.yml"));
   EXPECT_TRUE(ProjMgrYamlSchemaChecker().Validate(testoutput_folder + "/test2.Debug+CM3.cbuild.yml"));

  // Check generated cbuild-pack file
  ProjMgrTestEnv::CompareFile(testinput_folder + "/TestSolution/test.cbuild-pack.yml",
    testinput_folder + "/TestSolution/ref/test.cbuild-pack.yml");
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
  char* argv[9];
  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestSolution/test.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"-c";
  argv[7] = (char*)"test2.Debug+CM0";
  argv[8] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(9, argv, m_envp));
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolutionNonExistentContext) {
  char* argv[9];
  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestSolution/test.csolution.yml";
  RemoveCbuildSetFile(csolution);

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"-c";
  argv[7] = (char*)"NON-EXISTENT-CONTEXT";
  argv[8] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(9, argv, 0));
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
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(7, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_UnknownLayer) {
  char* argv[8];
  const string& csolution = testinput_folder + "/TestLayers/testlayers_invalid_layer.csolution.yml";
  const string& output = testoutput_folder + "/testlayers";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)output.c_str();
  argv[6] = (char*)"-n";
  argv[7] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(8, argv, 0));
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
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

  // Check generated CPRJs
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/testlayers/testlayers.Debug+TEST_TARGET.cprj",
    testinput_folder + "/TestLayers/ref/testlayers/testlayers.Debug+TEST_TARGET.cprj");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/testlayers/testlayers.Release+TEST_TARGET.cprj",
    testinput_folder + "/TestLayers/ref/testlayers/testlayers.Release+TEST_TARGET.cprj");

  // Check creation of layers rte folders
  EXPECT_TRUE(RteFsUtils::Exists(testinput_folder + "/TestLayers/Layer2/RTE/Device/RteTest_ARMCM0"));
  EXPECT_TRUE(RteFsUtils::Exists(testinput_folder + "/TestLayers/Layer3/RTE/RteTest/MyDir"));
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_CbuildFailedToCreate) {
  char* argv[7];
  StdStreamRedirect streamRedirect;

  // convert --solution solution.yml
  const string csolution = testinput_folder + "/TestSolution/fail_create_cbuild.csolution.yml";
  const string output = testoutput_folder + "/testpacklock";
  const string cbuild = output + "/fail_create_cbuild+CM0.cbuild.yml";

  // Create a directory with same name where cbuild.yml will be attempted to be created
  EXPECT_TRUE(RteFsUtils::CreateDirectories(cbuild));

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)output.c_str();
  argv[6] = (char*)"--cbuildgen";

  EXPECT_EQ(1, RunProjMgr(7, argv, 0));
  EXPECT_TRUE(RteFsUtils::IsDirectory(cbuild));

  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(cbuild + " - error csolution: file cannot be written"));
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_LockPackVersion) {
  char* argv[7];

  // convert --solution solution.yml
  const string csolution = testinput_folder + "/TestSolution/PackLocking/lock_pack_version.csolution.yml";
  const string cbuildPack = testinput_folder + "/TestSolution/PackLocking/lock_pack_version.cbuild-pack.yml";
  const string cbuildPackBackup = RteFsUtils::BackupFile(cbuildPack);
  const string output = testoutput_folder + "/testpacklock";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)output.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

  // Check in the generated CPRJ that RteTest_DFP::0.1.1 is still used, even if 0.2.0 is available
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/testpacklock/project_with_dfp_components+CM0.cprj",
    testinput_folder + "/TestSolution/PackLocking/ref/project_with_dfp_components+CM0.cprj");

  // Check that the cbuild-pack file hasn't been modified by this operation
  ProjMgrTestEnv::CompareFile(cbuildPackBackup, cbuildPack);
  RteFsUtils::RemoveFile(cbuildPackBackup);
}


TEST_F(ProjMgrUnitTests, RunProjMgrSolution_LockPackVersionUpgrade) {
  char* argv[6];

  // convert --solution solution.yml
  const string csolution = testinput_folder + "/TestSolution/PackLocking/lock_pack_version_upgrade.csolution.yml";
  const string cbuildPack = testinput_folder + "/TestSolution/PackLocking/lock_pack_version_upgrade.cbuild-pack.yml";

  string buf1;
  RteFsUtils::ReadFile(cbuildPack, buf1);

  const string output = testoutput_folder + "/testpacklock";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)output.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, m_envp));

  string buf2;
  RteFsUtils::ReadFile(cbuildPack, buf2);
  RteUtils::ReplaceAll(buf2, "\r\n", "\n");
  // Check that the cbuild-pack file has been modified by this operation to reflect version change in csolution.yml
  EXPECT_NE(buf2, buf1); // expected 0.0.1 != 0.2.0

  //  replace buf1 versions with expected values
  RteUtils::ReplaceAll(buf1, "@0.1.1", "@0.2.0");
  EXPECT_EQ(buf2, buf1);
}


TEST_F(ProjMgrUnitTests, RunProjMgrSolution_MultiplePackEntries) {
  char* argv[3];
  const string csolution = testinput_folder + "/TestSolution/PackLocking/multiple_pack_entries.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  EXPECT_EQ(0, RunProjMgr(3, argv, m_envp));

  // Check the generated cbuild-pack file
  ProjMgrTestEnv::CompareFile(testinput_folder + "/TestSolution/PackLocking/multiple_pack_entries.cbuild-pack.yml",
    testinput_folder + "/TestSolution/PackLocking/ref/multiple_pack_entries.cbuild-pack.yml");
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_LockPackKeepExistingForContextSelections) {
  char* argv[9];
  string buf1, buf2, buf3;

  // convert --solution solution.yml
  const string csolution = testinput_folder + "/TestSolution/PackLocking/project_pack_lock_with_for_context.csolution.yml";
  const string cbuildPack = testinput_folder + "/TestSolution/PackLocking/project_pack_lock_with_for_context.cbuild-pack.yml";
  const string output = testoutput_folder + "/testpacklock";

  // Ensure clean state for the test case.
  RteFsUtils::RemoveFile(cbuildPack);

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)output.c_str();
  argv[6] = (char*)"--cbuildgen";
  argv[7] = (char*)"-c";

  // First create initial cbuild-pack.yml file without optional pack
  argv[8] = (char*)".withoutComponents";
  EXPECT_EQ(0, RunProjMgr(9, argv, m_envp));
  EXPECT_TRUE(RteFsUtils::Exists(cbuildPack));
  EXPECT_TRUE(RteFsUtils::ReadFile(cbuildPack, buf1));
  EXPECT_TRUE(buf1.find("- resolved-pack: ARM::RteTest_DFP@") != string::npos);
  EXPECT_FALSE(buf1.find("- resolved-pack: ARM::RteTest@") != string::npos); // Should not have been added yet

  // Update the cbuild-pack.yml to contain the optional pack
  argv[8] = (char*)".withComponents";
  EXPECT_EQ(0, RunProjMgr(9, argv, m_envp));
  EXPECT_TRUE(RteFsUtils::Exists(cbuildPack));
  EXPECT_TRUE(RteFsUtils::ReadFile(cbuildPack, buf2));
  EXPECT_TRUE(buf2.find("- resolved-pack: ARM::RteTest_DFP@") != string::npos);
  EXPECT_TRUE(buf2.find("- resolved-pack: ARM::RteTest@") != string::npos); // Should have been added.
  EXPECT_NE(buf1, buf2);

  // Re-run without the optional pack and ensure it's still present in the cbuild-pack.yml file
  argv[8] = (char*)".withoutComponents";
  EXPECT_EQ(0, RunProjMgr(9, argv, m_envp));
  EXPECT_TRUE(RteFsUtils::Exists(cbuildPack));
  EXPECT_TRUE(RteFsUtils::ReadFile(cbuildPack, buf3));
  EXPECT_TRUE(buf3.find("- resolved-pack: ARM::RteTest_DFP@") != string::npos);
  EXPECT_TRUE(buf3.find("- resolved-pack: ARM::RteTest@") != string::npos); // Should still be here even with -c flag.
  EXPECT_EQ(buf2, buf3);
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_LockPackCleanup) {
  char* argv[7];
  string buf1, buf2;

  // convert --solution solution.yml
  const string csolution = testinput_folder + "/TestSolution/PackLocking/project_pack_lock_cleanup.csolution.yml";
  const string cbuildPack = testinput_folder + "/TestSolution/PackLocking/project_pack_lock_cleanup.cbuild-pack.yml";
  const string output = testoutput_folder + "/testpacklock";

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)output.c_str();
  argv[6] = (char*)"--cbuildgen";

  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));
  EXPECT_TRUE(RteFsUtils::Exists(cbuildPack));
  EXPECT_TRUE(RteFsUtils::ReadFile(cbuildPack, buf1));
  EXPECT_FALSE(buf1.find("- resolved-pack: ARM::RteTest_DFP@0.1.1") != string::npos);
  EXPECT_TRUE(buf1.find("- resolved-pack: ARM::RteTest_DFP@0.2.0") != string::npos);
  EXPECT_TRUE(buf1.find("- ARM::RteTest_DFP") != string::npos);

  // 2nd run to verify that the cbuild-pack.yml content is stable
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));
  EXPECT_TRUE(RteFsUtils::Exists(cbuildPack));
  EXPECT_TRUE(RteFsUtils::ReadFile(cbuildPack, buf2));
  EXPECT_EQ(buf1, buf2);
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_LockPackNoPackList) {
  char* argv[7];

  // convert --solution solution.yml
  const string csolution = testinput_folder + "/TestSolution/PackLocking/project_pack_lock_no_pack_list.csolution.yml";
  const string cbuildPack = testinput_folder + "/TestSolution/PackLocking/ref/project_pack_lock_no_pack_list.cbuild-pack.yml";
  const string expectedCbuildPack = testinput_folder + "/TestSolution/PackLocking/ref/project_pack_lock_no_pack_list.cbuild-pack.yml";
  const string output = testoutput_folder + "/testpacklock";

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)output.c_str();
  argv[6] = (char*)"--cbuildgen";

  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));
  ProjMgrTestEnv::CompareFile(expectedCbuildPack, cbuildPack);

  // 2nd run to verify that the cbuild-pack.yml content is stable
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));
  ProjMgrTestEnv::CompareFile(expectedCbuildPack, cbuildPack);
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_LockPackFrozen) {
  char* argv[8];
  StdStreamRedirect streamRedirect;

  // convert --solution solution.yml
  const string csolution = testinput_folder + "/TestSolution/PackLocking/cbuild_pack_frozen.csolution.yml";
  const string cbuildPack = testinput_folder + "/TestSolution/PackLocking/cbuild_pack_frozen.cbuild-pack.yml";
  const string expectedCbuildPackRef = testinput_folder + "/TestSolution/PackLocking/ref/cbuild_pack_frozen.cbuild-pack.yml";
  const string rtePath = testinput_folder + "/TestSolution/PackLocking/RTE/";
  const string expectedCbuildPack = RteFsUtils::BackupFile(cbuildPack);
  const string output = testoutput_folder + "/testpacklock";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)output.c_str();
  argv[6] = (char*)"--cbuildgen";
  argv[7] = (char*)"--frozen-packs";
  // Ensure clean state when starting test
  ASSERT_TRUE(RteFsUtils::RemoveDir(rtePath));

  // 1st run to verify that the cbuild-pack.yml content is stable
  EXPECT_NE(0, RunProjMgr(8, argv, m_envp));
  EXPECT_NE(streamRedirect.GetErrorString().find(cbuildPack + " - error csolution: file not allowed to be updated"), string::npos);
  ProjMgrTestEnv::CompareFile(expectedCbuildPack, cbuildPack);
  EXPECT_FALSE(RteFsUtils::Exists(rtePath + "/Device"));

  // 2nd run to verify that the cbuild-pack.yml content is stable
  streamRedirect.ClearStringStreams();
  EXPECT_NE(0, RunProjMgr(8, argv, m_envp));
  EXPECT_NE(streamRedirect.GetErrorString().find(cbuildPack + " - error csolution: file not allowed to be updated"), string::npos);
  ProjMgrTestEnv::CompareFile(expectedCbuildPack, cbuildPack);
  EXPECT_FALSE(RteFsUtils::Exists(rtePath + "/Device"));

  // 3rd run without --frozen-packs to verify that the list can be updated
  streamRedirect.ClearStringStreams();
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));
  EXPECT_NE(streamRedirect.GetOutString().find(cbuildPack + " - info csolution: file generated successfully"), string::npos);
  ProjMgrTestEnv::CompareFile(expectedCbuildPackRef, cbuildPack);
  EXPECT_TRUE(RteFsUtils::Exists(rtePath + "/Device"));

  EXPECT_TRUE(RteFsUtils::Exists(testinput_folder + "/TestSolution/PackLocking/RTE/_CM3/RTE_Components.h"));
  EXPECT_TRUE(RteFsUtils::Exists(testinput_folder + "/TestSolution/PackLocking/RTE/Device/RteTest_ARMCM3/gcc_arm.ld"));
  EXPECT_FALSE(RteFsUtils::Exists(testinput_folder + "/TestSolution/PackLocking/RTE/Device/RteTest_ARMCM3/gcc_arm.ld.base@2.0.0")); // From Arm::RteTest_DFP@0.1.1
  EXPECT_TRUE(RteFsUtils::Exists(testinput_folder + "/TestSolution/PackLocking/RTE/Device/RteTest_ARMCM3/gcc_arm.ld.base@2.2.0"));
  EXPECT_TRUE(RteFsUtils::Exists(testinput_folder + "/TestSolution/PackLocking/RTE/Device/RteTest_ARMCM3/startup_ARMCM3.c"));
  EXPECT_TRUE(RteFsUtils::Exists(testinput_folder + "/TestSolution/PackLocking/RTE/Device/RteTest_ARMCM3/startup_ARMCM3.c.base@2.0.3"));
  EXPECT_TRUE(RteFsUtils::Exists(testinput_folder + "/TestSolution/PackLocking/RTE/Device/RteTest_ARMCM3/system_ARMCM3.c"));
  EXPECT_FALSE(RteFsUtils::Exists(testinput_folder + "/TestSolution/PackLocking/RTE/Device/RteTest_ARMCM3/system_ARMCM3.c.base@1.0.1")); // From Arm::RteTest_DFP@0.1.1
  EXPECT_TRUE(RteFsUtils::Exists(testinput_folder + "/TestSolution/PackLocking/RTE/Device/RteTest_ARMCM3/system_ARMCM3.c.base@1.2.2"));

  // 4th run with --frozen-packs to verify that RTE directory can be generated
  ASSERT_TRUE(RteFsUtils::RemoveDir(rtePath));
  streamRedirect.ClearStringStreams();
  EXPECT_EQ(0, RunProjMgr(8, argv, m_envp));
  EXPECT_NE(streamRedirect.GetOutString().find(cbuildPack + " - info csolution: file is already up-to-date"), string::npos);
  ProjMgrTestEnv::CompareFile(expectedCbuildPackRef, cbuildPack);

  EXPECT_TRUE(RteFsUtils::Exists(testinput_folder + "/TestSolution/PackLocking/RTE/_CM3/RTE_Components.h"));
  EXPECT_TRUE(RteFsUtils::Exists(testinput_folder + "/TestSolution/PackLocking/RTE/Device/RteTest_ARMCM3/gcc_arm.ld"));
  EXPECT_TRUE(RteFsUtils::Exists(testinput_folder + "/TestSolution/PackLocking/RTE/Device/RteTest_ARMCM3/gcc_arm.ld.base@2.2.0"));
  EXPECT_TRUE(RteFsUtils::Exists(testinput_folder + "/TestSolution/PackLocking/RTE/Device/RteTest_ARMCM3/startup_ARMCM3.c"));
  EXPECT_TRUE(RteFsUtils::Exists(testinput_folder + "/TestSolution/PackLocking/RTE/Device/RteTest_ARMCM3/startup_ARMCM3.c.base@2.0.3"));
  EXPECT_TRUE(RteFsUtils::Exists(testinput_folder + "/TestSolution/PackLocking/RTE/Device/RteTest_ARMCM3/system_ARMCM3.c"));
  EXPECT_TRUE(RteFsUtils::Exists(testinput_folder + "/TestSolution/PackLocking/RTE/Device/RteTest_ARMCM3/system_ARMCM3.c.base@1.2.2"));

  RteFsUtils::RemoveFile(expectedCbuildPack);
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_LockPackFrozenNoPackFile) {
  char* argv[8];

  // convert --solution solution.yml
  const string csolution = testinput_folder + "/TestSolution/PackLocking/cbuild_pack_frozen_no_pack_file.csolution.yml";
  const string output = testoutput_folder + "/testpacklock";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)output.c_str();
  argv[6] = (char*)"--frozen-packs";
  argv[7] = (char*)"--cbuildgen";

  EXPECT_NE(0, RunProjMgr(8, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_LockPackReselectSelectedByPack) {
  char* argv[7];

  // convert --solution solution.yml
  const string csolution = testinput_folder + "/TestSolution/PackLocking/project_pack_lock_reselect_selected-by-pack.csolution.yml";
  const string cbuildPack = testinput_folder + "/TestSolution/PackLocking/project_pack_lock_reselect_selected-by-pack.cbuild-pack.yml";
  const string expectedCbuildPack = testinput_folder + "/TestSolution/PackLocking/ref/project_pack_lock_reselect_selected-by-pack.cbuild-pack.yml";
  const string output = testoutput_folder + "/testpacklock";

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)output.c_str();
  argv[6] = (char*)"--cbuildgen";

  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));
  ProjMgrTestEnv::CompareFile(expectedCbuildPack, cbuildPack);

  // 2nd run to verify that the cbuild-pack.yml content is stable
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));
  ProjMgrTestEnv::CompareFile(expectedCbuildPack, cbuildPack);
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_LockPackLoadArgument) {
  error_code ec;
  char* argv[9];
  StdStreamRedirect streamRedirect;

  // convert --solution solution.yml
  const string csolution = testinput_folder + "/TestSolution/PackLocking/project_pack_lock_using_load_argument.csolution.yml";
  const string cbuildPack = testinput_folder + "/TestSolution/PackLocking/project_pack_lock_using_load_argument.cbuild-pack.yml";
  const string expectedCbuildPackAll = testinput_folder + "/TestSolution/PackLocking/ref/project_pack_lock_using_load_argument-all.cbuild-pack.yml";
  const string expectedCbuildPackLatest = testinput_folder + "/TestSolution/PackLocking/ref/project_pack_lock_using_load_argument-latest.cbuild-pack.yml";
  const string expectedCbuildPackRequired = testinput_folder + "/TestSolution/PackLocking/ref/project_pack_lock_using_load_argument-required.cbuild-pack.yml";
  const string output = testoutput_folder + "/testpacklock";

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)output.c_str();
  argv[6] = (char*)"--cbuildgen";
  argv[7] = (char*)"--load";

  // Test with --load all and without cbuild-pack.yml file
  argv[8] = (char*)"all";
  RteFsUtils::RemoveFile(cbuildPack);
  streamRedirect.ClearStringStreams();
  EXPECT_EQ(0, RunProjMgr(9, argv, m_envp));
  EXPECT_NE(streamRedirect.GetOutString().find(cbuildPack + " - info csolution: file generated successfully"), string::npos);
  ProjMgrTestEnv::CompareFile(expectedCbuildPackAll, cbuildPack);

  // Test with --load all and with cbuild-pack.yml file
  argv[8] = (char*)"all";
  fs::copy(fs::path(expectedCbuildPackRequired), fs::path(cbuildPack), fs::copy_options::overwrite_existing, ec);
  streamRedirect.ClearStringStreams();
  EXPECT_EQ(0, RunProjMgr(9, argv, m_envp));
  EXPECT_NE(streamRedirect.GetOutString().find(cbuildPack + " - info csolution: file generated successfully"), string::npos);
  ProjMgrTestEnv::CompareFile(expectedCbuildPackAll, cbuildPack);

  // Test with --load latest and without cbuild-pack.yml file
  argv[8] = (char*)"latest";
  RteFsUtils::RemoveFile(cbuildPack);
  streamRedirect.ClearStringStreams();
  EXPECT_EQ(0, RunProjMgr(9, argv, m_envp));
  EXPECT_NE(streamRedirect.GetOutString().find(cbuildPack + " - info csolution: file generated successfully"), string::npos);
  ProjMgrTestEnv::CompareFile(expectedCbuildPackLatest, cbuildPack);

  // Test with --load latest and with cbuild-pack.yml file
  argv[8] = (char*)"latest";
  fs::copy(fs::path(expectedCbuildPackRequired), fs::path(cbuildPack), fs::copy_options::overwrite_existing, ec);
  streamRedirect.ClearStringStreams();
  EXPECT_EQ(0, RunProjMgr(9, argv, m_envp));
  EXPECT_NE(streamRedirect.GetOutString().find(cbuildPack + " - info csolution: file generated successfully"), string::npos);
  ProjMgrTestEnv::CompareFile(expectedCbuildPackLatest, cbuildPack);

  // Test with --load required and without cbuild-pack.yml file
  argv[8] = (char*)"required";
  RteFsUtils::RemoveFile(cbuildPack);
  streamRedirect.ClearStringStreams();
  EXPECT_EQ(0, RunProjMgr(9, argv, m_envp));
  EXPECT_NE(streamRedirect.GetOutString().find(cbuildPack + " - info csolution: file generated successfully"), string::npos);
  ProjMgrTestEnv::CompareFile(expectedCbuildPackLatest, cbuildPack);

  // Test with --load required and with cbuild-pack.yml file
  argv[8] = (char*)"required";
  fs::copy(fs::path(expectedCbuildPackRequired), fs::path(cbuildPack), fs::copy_options::overwrite_existing, ec);
  streamRedirect.ClearStringStreams();
  EXPECT_EQ(0, RunProjMgr(9, argv, m_envp));
  EXPECT_NE(streamRedirect.GetOutString().find(cbuildPack + " - info csolution: file is already up-to-date"), string::npos);
  ProjMgrTestEnv::CompareFile(expectedCbuildPackRequired, cbuildPack);

  // Test without --load and without cbuild-pack.yml file
  RteFsUtils::RemoveFile(cbuildPack);
  streamRedirect.ClearStringStreams();
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));
  EXPECT_NE(streamRedirect.GetOutString().find(cbuildPack + " - info csolution: file generated successfully"), string::npos);
  ProjMgrTestEnv::CompareFile(expectedCbuildPackLatest, cbuildPack);

  // Test without --load but with cbuild-pack.yml file
  fs::copy(fs::path(expectedCbuildPackRequired), fs::path(cbuildPack), fs::copy_options::overwrite_existing, ec);
  streamRedirect.ClearStringStreams();
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));
  EXPECT_NE(streamRedirect.GetOutString().find(cbuildPack + " - info csolution: file is already up-to-date"), string::npos);
  ProjMgrTestEnv::CompareFile(expectedCbuildPackRequired, cbuildPack);
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_LockPackFindUnspecifiedPackUsingLoadArgument) {
  error_code ec;
  char* argv[9];
  StdStreamRedirect streamRedirect;

  // convert --solution solution.yml
  const string csolution = testinput_folder + "/TestSolution/PackLocking/project_pack_lock_find_unspecified_pack_using_load_argument.csolution.yml";
  const string cbuildPack = testinput_folder + "/TestSolution/PackLocking/project_pack_lock_find_unspecified_pack_using_load_argument.cbuild-pack.yml";
  const string expectedCbuildPackAll = testinput_folder + "/TestSolution/PackLocking/ref/project_pack_lock_find_unspecified_pack_using_load_argument-all.cbuild-pack.yml";
  const string expectedCbuildPackLatest = testinput_folder + "/TestSolution/PackLocking/ref/project_pack_lock_find_unspecified_pack_using_load_argument-latest.cbuild-pack.yml";
  const string expectedCbuildPackRequired = testinput_folder + "/TestSolution/PackLocking/ref/project_pack_lock_find_unspecified_pack_using_load_argument-required.cbuild-pack.yml";
  const string expectedCbuildPackRequiredUpdated = testinput_folder + "/TestSolution/PackLocking/ref/project_pack_lock_find_unspecified_pack_using_load_argument-required_updated.cbuild-pack.yml";
  const string output = testoutput_folder + "/testpacklock";

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)output.c_str();
  argv[6] = (char*)"--cbuildgen";
  argv[7] = (char*)"--load";

  // Test with --load all and without cbuild-pack.yml file
  argv[8] = (char*)"all";
  RteFsUtils::RemoveFile(cbuildPack);
  streamRedirect.ClearStringStreams();
  EXPECT_EQ(0, RunProjMgr(9, argv, m_envp));
  EXPECT_NE(streamRedirect.GetOutString().find(cbuildPack + " - info csolution: file generated successfully"), string::npos);
  ProjMgrTestEnv::CompareFile(expectedCbuildPackAll, cbuildPack);

  // Test with --load all and with cbuild-pack.yml file
  argv[8] = (char*)"all";
  fs::copy(fs::path(expectedCbuildPackRequired), fs::path(cbuildPack), fs::copy_options::overwrite_existing, ec);
  streamRedirect.ClearStringStreams();
  EXPECT_EQ(0, RunProjMgr(9, argv, m_envp));
  EXPECT_NE(streamRedirect.GetOutString().find(cbuildPack + " - info csolution: file generated successfully"), string::npos);
  ProjMgrTestEnv::CompareFile(expectedCbuildPackAll, cbuildPack);

  // Test with --load latest and without cbuild-pack.yml file
  argv[8] = (char*)"latest";
  RteFsUtils::RemoveFile(cbuildPack);
  streamRedirect.ClearStringStreams();
  EXPECT_EQ(0, RunProjMgr(9, argv, m_envp));
  EXPECT_NE(streamRedirect.GetOutString().find(cbuildPack + " - info csolution: file generated successfully"), string::npos);
  ProjMgrTestEnv::CompareFile(expectedCbuildPackLatest, cbuildPack);

  // Test with --load latest and with cbuild-pack.yml file
  argv[8] = (char*)"latest";
  fs::copy(fs::path(expectedCbuildPackRequired), fs::path(cbuildPack), fs::copy_options::overwrite_existing, ec);
  streamRedirect.ClearStringStreams();
  EXPECT_EQ(0, RunProjMgr(9, argv, m_envp));
  EXPECT_NE(streamRedirect.GetOutString().find(cbuildPack + " - info csolution: file generated successfully"), string::npos);
  ProjMgrTestEnv::CompareFile(expectedCbuildPackLatest, cbuildPack);

  // Test with --load required and without cbuild-pack.yml file
  argv[8] = (char*)"required";
  RteFsUtils::RemoveFile(cbuildPack);
  streamRedirect.ClearStringStreams();
  EXPECT_EQ(1, RunProjMgr(9, argv, m_envp));
  EXPECT_NE(streamRedirect.GetOutString().find(cbuildPack + " - info csolution: file generated successfully"), string::npos);
  EXPECT_NE(streamRedirect.GetErrorString().find("error csolution: component 'RteTest:ComponentLevel' not found in included packs"), string::npos);
  ProjMgrTestEnv::CompareFile(expectedCbuildPackRequiredUpdated, cbuildPack);

  // Test with --load required and with cbuild-pack.yml file
  argv[8] = (char*)"required";
  fs::copy(fs::path(expectedCbuildPackRequired), fs::path(cbuildPack), fs::copy_options::overwrite_existing, ec);
  streamRedirect.ClearStringStreams();
  EXPECT_EQ(1, RunProjMgr(9, argv, m_envp));
  EXPECT_NE(streamRedirect.GetOutString().find(cbuildPack + " - info csolution: file is already up-to-date"), string::npos);
  EXPECT_NE(streamRedirect.GetErrorString().find("error csolution: component 'RteTest:ComponentLevel' not found in included packs"), string::npos);
  ProjMgrTestEnv::CompareFile(expectedCbuildPackRequired, cbuildPack);

  // Test without --load and without cbuild-pack.yml file
  RteFsUtils::RemoveFile(cbuildPack);
  streamRedirect.ClearStringStreams();
  EXPECT_EQ(1, RunProjMgr(7, argv, m_envp));
  EXPECT_NE(streamRedirect.GetOutString().find(cbuildPack + " - info csolution: file generated successfully"), string::npos);
  EXPECT_NE(streamRedirect.GetErrorString().find("error csolution: component 'RteTest:ComponentLevel' not found in included packs"), string::npos);
  ProjMgrTestEnv::CompareFile(expectedCbuildPackRequiredUpdated, cbuildPack);

  // Test without --load but with cbuild-pack.yml file
  fs::copy(fs::path(expectedCbuildPackRequired), fs::path(cbuildPack), fs::copy_options::overwrite_existing, ec);
  streamRedirect.ClearStringStreams();
  EXPECT_EQ(1, RunProjMgr(7, argv, m_envp));
  EXPECT_NE(streamRedirect.GetOutString().find(cbuildPack + " - info csolution: file is already up-to-date"), string::npos);
  EXPECT_NE(streamRedirect.GetErrorString().find("error csolution: component 'RteTest:ComponentLevel' not found in included packs"), string::npos);
  ProjMgrTestEnv::CompareFile(expectedCbuildPackRequired, cbuildPack);
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_CbuildPackLocalPackIgnored) {
  char* argv[7];

  // convert --solution solution.yml
  const string csolution1 = testinput_folder + "/TestSolution/PackLocking/cbuild_pack_unused_local_pack_ignored.csolution.yml";
  const string cbuildPack1 = testinput_folder + "/TestSolution/PackLocking/cbuild_pack_unused_local_pack_ignored.cbuild-pack.yml";
  const string csolution2 = testinput_folder + "/TestSolution/PackLocking/cbuild_pack_used_local_pack_ignored.csolution.yml";
  const string cbuildPack2 = testinput_folder + "/TestSolution/PackLocking/cbuild_pack_used_local_pack_ignored.cbuild-pack.yml";
  const string output = testoutput_folder + "/testpacklock";

  EXPECT_FALSE(RteFsUtils::Exists(cbuildPack1));
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution1.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)output.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

  EXPECT_FALSE(RteFsUtils::Exists(cbuildPack2));
  argv[3] = (char*)csolution2.c_str();
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

  // Check that the cbuild-pack files contains the system wide pack but not the local
  string buf1;
  EXPECT_TRUE(RteFsUtils::ReadFile(cbuildPack1, buf1));
  EXPECT_TRUE(buf1.find("- resolved-pack: ARM::RteTest_DFP@") != string::npos);
  EXPECT_FALSE(buf1.find("- resolved-pack: ARM::RteTest@") != string::npos);
  string buf2;
  EXPECT_TRUE(RteFsUtils::ReadFile(cbuildPack2, buf2));
  EXPECT_TRUE(buf2.find("- resolved-pack: ARM::RteTest_DFP@") != string::npos);
  EXPECT_FALSE(buf2.find("- resolved-pack: ARM::RteTest@") != string::npos);
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_CbuildPackInvalidContent) {
  char* argv[8];
  StdStreamRedirect streamRedirect;

  // convert --solution solution.yml
  const string csolution = testinput_folder + "/TestSolution/PackLocking/cbuild_pack_invalid_content.csolution.yml";
  const string csolution2 = testinput_folder + "/TestSolution/PackLocking/cbuild_pack_invalid_content2.csolution.yml";
  const string output = testoutput_folder + "/testpacklock";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)output.c_str();
  argv[6] = (char*)"--cbuildgen";
  argv[7] = (char*)"--no-check-schema";
  EXPECT_NE(0, RunProjMgr(7, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(" error csolution: required property 'resolved-packs' not found in object"));

  streamRedirect.ClearStringStreams();
  argv[3] = (char*)csolution2.c_str();
  EXPECT_NE(0, RunProjMgr(7, argv, 0));
  errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(" error csolution: unexpected instance type"));

  streamRedirect.ClearStringStreams();
  EXPECT_NE(0, RunProjMgr(8, argv, 0));
  errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(" error csolution: operator[] call on a scalar (key: \"cbuild-pack\")"));
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_CbuildPackWithDisallowedField) {
  char* argv[8];
  StdStreamRedirect streamRedirect;

  // convert --solution solution.yml
  const string csolution = testinput_folder + "/TestSolution/PackLocking/cbuild_pack_with_disallowed_field.csolution.yml";
  const string csolution2 = testinput_folder + "/TestSolution/PackLocking/cbuild_pack_with_disallowed_field2.csolution.yml";
  const string output = testoutput_folder + "/testpacklock";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)output.c_str();
  argv[6] = (char*)"--cbuildgen";
  argv[7] = (char*)"--no-check-schema";

  // Run without "--no-check-schema"
  EXPECT_NE(0, RunProjMgr(7, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find("error csolution: schema check failed, verify syntax"));

  // Run with "--no-check-schema"
  streamRedirect.ClearStringStreams();
  EXPECT_NE(0, RunProjMgr(8, argv, 0));
  errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find("warning csolution: key 'misc' was not recognized"));
  EXPECT_NE(string::npos, errStr.find("error csolution: node 'misc' shall contain sequence elements"));

  streamRedirect.ClearStringStreams();
  argv[3] = (char*)csolution2.c_str();
  EXPECT_NE(0, RunProjMgr(8, argv, 0));
  errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find("warning csolution: key 'misc' was not recognized"));
  EXPECT_NE(string::npos, errStr.find("error csolution: node 'misc' shall contain sequence elements"));
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_CbuildPackWithUnmatchedVendor) {
  char* argv[7];

  // convert --solution solution.yml
  const string csolution = testinput_folder + "/TestSolution/PackLocking/cbuild_pack_with_unmatched_vendor.csolution.yml";
  const string output = testoutput_folder + "/testpacklock";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)output.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_CbuildPackWithoutUsedComponents) {
  char* argv[7];

  // convert --solution solution.yml
  const string csolution = testinput_folder + "/TestSolution/PackLocking/cbuild_pack_without_used_components.csolution.yml";
  const string cbuildPack = testinput_folder + "/TestSolution/PackLocking/cbuild_pack_without_used_components.cbuild-pack.yml";
  const string output = testoutput_folder + "/testpacklock";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)output.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

  string buf;
  EXPECT_TRUE(RteFsUtils::Exists(cbuildPack));
  EXPECT_TRUE(RteFsUtils::ReadFile(cbuildPack, buf));
  EXPECT_TRUE(buf.find("- resolved-pack: ARM::RteTest@0.1.0") != string::npos);
  EXPECT_TRUE(buf.find("- ARM::RteTest@0.1.0") != string::npos);
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_LockedPackVersionNotChangedByAddedPack) {
  char* argv[7];

  // convert --solution solution.yml
  const string csolution = testinput_folder + "/TestSolution/PackLocking/pack_lock_with_added_pack.csolution.yml";
  const string cbuildPack = testinput_folder + "/TestSolution/PackLocking/pack_lock_with_added_pack.cbuild-pack.yml";
  const string output = testoutput_folder + "/testpacklock";

  // Check that there is a newer version of the locked pack
  vector<string> packs;
  m_worker.SetLoadPacksPolicy(LoadPacksPolicy::ALL);
  EXPECT_TRUE(m_worker.ListPacks(packs, false, "ARM::RteTest_DFP@0.1.1"));
  EXPECT_TRUE(m_worker.ListPacks(packs, false, "ARM::RteTest_DFP@0.2.0"));

  // Check that the cbuild-pack file contains the first pack (locked to an old version) but not the second
  string buf;
  EXPECT_TRUE(RteFsUtils::ReadFile(cbuildPack, buf));
  EXPECT_TRUE(buf.find("- resolved-pack: ARM::RteTest_DFP@0.1.1") != string::npos);
  EXPECT_FALSE(buf.find("- resolved-pack: ARM::RteTest@") != string::npos);

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)output.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, m_envp));

  // Check that the cbuild-pack file contains both packs and that the first still has the same version
  EXPECT_TRUE(RteFsUtils::ReadFile(cbuildPack, buf));
  EXPECT_TRUE(buf.find("- resolved-pack: ARM::RteTest_DFP@0.1.1") != string::npos);
  EXPECT_TRUE(buf.find("- resolved-pack: ARM::RteTest@") != string::npos);
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_LockedProjectPackVersionNotChangedByAddedPack) {
  char* argv[7];

  // Same as previous test but with packs listed in project
  // convert --solution solution.yml
  const string csolution = testinput_folder + "/TestSolution/PackLocking/project_pack_lock_with_added_pack.csolution.yml";
  const string cbuildPack = testinput_folder + "/TestSolution/PackLocking/project_pack_lock_with_added_pack.cbuild-pack.yml";
  const string output = testoutput_folder + "/testpacklock";

  // Check that there is a newer version of the locked pack
  vector<string> packs;
  m_worker.SetLoadPacksPolicy(LoadPacksPolicy::ALL);
  EXPECT_TRUE(m_worker.ListPacks(packs, false, "ARM::RteTest_DFP@0.1.1"));
  EXPECT_TRUE(m_worker.ListPacks(packs, false, "ARM::RteTest_DFP@0.2.0"));

  // Check that the cbuild-pack file contains the first pack (locked to an old version) but not the second
  string buf;
  EXPECT_TRUE(RteFsUtils::ReadFile(cbuildPack, buf));
  EXPECT_TRUE(buf.find("- resolved-pack: ARM::RteTest_DFP@0.1.1") != string::npos);
  EXPECT_FALSE(buf.find("- resolved-pack: ARM::RteTest@") != string::npos);

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)output.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

  // Check that the cbuild-pack file contains both packs and that the first still has the same version
  EXPECT_TRUE(RteFsUtils::ReadFile(cbuildPack, buf));
  EXPECT_TRUE(buf.find("- resolved-pack: ARM::RteTest_DFP@0.1.1") != string::npos);
  EXPECT_TRUE(buf.find("- resolved-pack: ARM::RteTest@") != string::npos);
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_LockPackWithVersionRange) {
  char* argv[7];

  // convert --solution solution.yml
  const string csolution = testinput_folder + "/TestSolution/PackLocking/pack_lock_with_version_range.csolution.yml";
  const string output = testoutput_folder + "/testpacklock";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)output.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

  // Check the generated cbuild-pack file
  ProjMgrTestEnv::CompareFile(testinput_folder + "/TestSolution/PackLocking/pack_lock_with_version_range.cbuild-pack.yml",
    testinput_folder + "/TestSolution/PackLocking/ref/pack_lock_with_version_range.cbuild-pack.yml");
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_LockProjectPackWithVersionRange) {
  char* argv[7];

  // Same as previous test but with packs listed in project
  // convert --solution solution.yml
  const string csolution = testinput_folder + "/TestSolution/PackLocking/project_pack_lock_with_version_range.csolution.yml";
  const string output = testoutput_folder + "/testpacklock";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)output.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

  // Check the generated cbuild-pack file
  ProjMgrTestEnv::CompareFile(testinput_folder + "/TestSolution/PackLocking/project_pack_lock_with_version_range.cbuild-pack.yml",
    testinput_folder + "/TestSolution/PackLocking/ref/project_pack_lock_with_version_range.cbuild-pack.yml");
}

TEST_F(ProjMgrUnitTests, RunProjMgrLayers2) {
  char* argv[5];

  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestLayers/testlayers.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(5, argv, m_envp));

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
.*/ARM/RteTest_DFP/0.2.0/Layers/board-specific.clayer.yml \\(layer type: BoardSpecific\\)\n\
.*/ARM/RteTest_DFP/0.2.0/Layers/board1.clayer.yml \\(layer type: Board\\)\n\
.*/ARM/RteTest_DFP/0.2.0/Layers/board2.clayer.yml \\(layer type: Board\\)\n\
.*/ARM/RteTest_DFP/0.2.0/Layers/board3.clayer.yml \\(layer type: Board\\)\n\
.*/ARM/RteTest_DFP/0.2.0/Layers/config1.clayer.yml \\(layer type: Config1\\)\n\
.*/ARM/RteTest_DFP/0.2.0/Layers/config2.clayer.yml \\(layer type: Config2\\)\n\
.*/ARM/RteTest_DFP/0.2.0/Layers/config3.clayer.yml \\(layer type: Config2\\)\n\
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
  EXPECT_EQ(0, RunProjMgr(8, argv, m_envp));

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


TEST_F(ProjMgrUnitTests, ListLayersConfigurations_update_idx_pack_layer) {
  StdStreamRedirect streamRedirect;
  char* argv[6];
  const string& csolution = testinput_folder + "/TestLayers/config.csolution.yml";
  string expectedOutStr = ".*config.cbuild-idx.yml - info csolution: file generated successfully\\n";

  argv[1] = (char*)"list";
  argv[2] = (char*)"layers";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  argv[5] = (char*)"--update-idx";

  EXPECT_EQ(0, RunProjMgr(6, argv, m_envp));
  EXPECT_TRUE(regex_match(streamRedirect.GetOutString(), regex(expectedOutStr)));

  ProjMgrTestEnv::CompareFile(testinput_folder + "/TestLayers/ref/config.cbuild-idx.yml",
    testinput_folder + "/TestLayers/config.cbuild-idx.yml", ProjMgrTestEnv::StripAbsoluteFunc);
  EXPECT_TRUE(ProjMgrYamlSchemaChecker().Validate(testinput_folder + "/TestLayers/config.cbuild-idx.yml"));
}

TEST_F(ProjMgrUnitTests, ListLayersConfigurations_update_idx_local_layer) {
  StdStreamRedirect streamRedirect;
  char* argv[6];
  const string& csolution = testinput_folder + "/TestLayers/select.csolution.yml";
  string expectedOutStr = ".*select.cbuild-idx.yml - info csolution: file generated successfully\\n";

  argv[1] = (char*)"list";
  argv[2] = (char*)"layers";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  argv[5] = (char*)"--update-idx";

  EXPECT_EQ(0, RunProjMgr(6, argv, m_envp));
  EXPECT_TRUE(regex_match(streamRedirect.GetOutString(), regex(expectedOutStr)));
  ProjMgrTestEnv::CompareFile(testinput_folder + "/TestLayers/ref/select.cbuild-idx.yml",
    testinput_folder + "/TestLayers/select.cbuild-idx.yml");
  EXPECT_TRUE(ProjMgrYamlSchemaChecker().Validate(testinput_folder + "/TestLayers/select.cbuild-idx.yml"));
}

TEST_F(ProjMgrUnitTests, ListLayersConfigurations_Error) {
  char* argv[8];
  const string& csolution = testinput_folder + "/TestLayers/variables-notdefined.csolution.yml";
  argv[1] = (char*)"list";
  argv[2] = (char*)"layers";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  argv[5] = (char*)"-o";
  argv[6] = (char*)testoutput_folder.c_str();
  argv[7] = (char*)"--update-idx";
  EXPECT_EQ(1, RunProjMgr(8, argv, m_envp));

  ProjMgrTestEnv::CompareFile(testinput_folder + "/TestLayers/ref/variables-notdefined.cbuild-idx.yml",
    testoutput_folder + "/variables-notdefined.cbuild-idx.yml");
}

TEST_F(ProjMgrUnitTests, ListLayersConfigurations) {
  StdStreamRedirect streamRedirect;
  char* argv[6];
  const string& csolution = testinput_folder + "/TestLayers/config.csolution.yml";
  argv[1] = (char*)"list";
  argv[2] = (char*)"layers";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  argv[5] = (char*)"-d";
  EXPECT_EQ(0, RunProjMgr(6, argv, m_envp));

  const string& outStr = streamRedirect.GetOutString();
  const string& errStr = streamRedirect.GetErrorString();
  EXPECT_EQ(200, ProjMgrTestEnv::CountOccurrences(errStr, "check combined connections"));
  EXPECT_EQ(4, ProjMgrTestEnv::CountOccurrences(outStr, "valid configuration #"));

  const string& expectedOutStr = "\
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
.*/ARM/RteTest_DFP/0.2.0/Layers/config3.clayer.yml \\(layer type: Config2\\)\n\
  set: set3.select1 \\(connect F - set 3 select 1\\)\n\
  set: set3.select2 \\(connect G - set 3 select 2\\)\n\
";
  EXPECT_TRUE(regex_search(outStr, regex(expectedOutStr)));
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
  EXPECT_EQ(0, RunProjMgr(6, argv, m_envp));

  const string& expectedOutStr = "\
info csolution: valid configuration #1: \\(context 'select\\+RteTest_ARMCM3'\\)\n\
  .*/TestLayers/select.cproject.yml\n\
    set: set1.select1 \\(project X - set 1 select 1\\)\n\
  .*/TestLayers/select.clayer.yml \\(layer type: Board\\)\n\
    set: set1.select1 \\(provided connections A and B - set 1 select 1\\)\n\
\n\
info csolution: valid configuration #2: \\(context 'select\\+RteTest_ARMCM3'\\)\n\
  .*/TestLayers/select.cproject.yml\n\
    set: set1.select2 \\(project Y - set 1 select 2\\)\n\
  .*/TestLayers/select.clayer.yml \\(layer type: Board\\)\n\
    set: set1.select2 \\(provided connections B and C - set 1 select 2\\)\n\
\n\
.*/TestLayers/select.clayer.yml \\(layer type: Board\\)\n\
  set: set1.select1 \\(provided connections A and B - set 1 select 1\\)\n\
  set: set1.select2 \\(provided connections B and C - set 1 select 2\\)\n\
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

  // Test with no registered toolchains (empty environment variables)
  streamRedirect.ClearStringStreams();
  EXPECT_EQ(1, RunProjMgr(3, argv, 0));
  const string& expectedWarn = "warning csolution: no compiler registered. Add path to compiler 'bin' directory with environment variable <name>_TOOLCHAIN_<major>_<minor>_<patch>. <name> is one of AC6, GCC, IAR, CLANG\n";
  const string& warnStr = streamRedirect.GetErrorString();
  EXPECT_EQ(warnStr, expectedWarn);
}

TEST_F(ProjMgrUnitTests, ListToolchainsNoToolchainRegistered) {
  StdStreamRedirect streamRedirect;
  char* argv[3];
  argv[1] = (char*)"list";
  argv[2] = (char*)"toolchains";
  EXPECT_EQ(1, RunProjMgr(3, argv, 0));

  const string& outStr = streamRedirect.GetOutString();
  const string& errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(outStr.empty());
  EXPECT_NE(std::string::npos, errStr.find("_TOOLCHAIN_<major>_<minor>_<patch>"));
}

TEST_F(ProjMgrUnitTests, ListToolchainsVerbose) {
  StdStreamRedirect streamRedirect;
  char* envp[4];
  string ac6 = "AC6_TOOLCHAIN_6_18_0=" + testinput_folder;
  string gcc = "GCC_TOOLCHAIN_11_2_1=" + testinput_folder;
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
AC6@6.18.0\n\
  Environment: AC6_TOOLCHAIN_6_18_0\n\
  Toolchain: .*/data\n\
  Configuration: .*/data/TestToolchains/AC6.6.18.0.cmake\n\
GCC@11.2.1\n\
  Environment: GCC_TOOLCHAIN_11_2_1\n\
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
  string ac6 = "AC6_TOOLCHAIN_6_18_0=" + testinput_folder;
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
  EXPECT_EQ(0, RunProjMgr(5, argv, 0));
  const string& expected2 = "AC6@>=0.0.0\nAC6@>=6.18.0\nGCC@11.3.1\n";
  const string& outStr2 = streamRedirect.GetOutString();
  EXPECT_EQ(outStr2, expected2);
}

TEST_F(ProjMgrUnitTests, ListToolchains_with_unknown_toolchain) {
  StdStreamRedirect streamRedirect;
  char* envp[4];
  string ac6 = "AC6_TOOLCHAIN_6_18_0=" + testinput_folder;
  string gcc = "GCC_TOOLCHAIN_11_3_1=" + testinput_folder;
  string unknown = "UNKNOWN_TOOLCHAIN_1_2_3=" + testinput_folder;
  envp[0] = (char*)ac6.c_str();
  envp[1] = (char*)gcc.c_str();
  envp[2] = (char*)unknown.c_str();
  envp[3] = (char*)'\0';
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
  const string& errStr = streamRedirect.GetErrorString();
  EXPECT_EQ(outStr, expected);
  EXPECT_TRUE(errStr.empty());

  // Test with no input solution
  streamRedirect.ClearStringStreams();
  EXPECT_EQ(1, RunProjMgr(3, argv, envp));
  const string& expected2 = "AC6@6.18.0\nGCC@11.3.1\n";
  const string& outStr2 = streamRedirect.GetOutString();
  const string& errStr2 = streamRedirect.GetErrorString();
  EXPECT_EQ(outStr2, expected2);
  EXPECT_TRUE(errStr2.find("error csolution: no toolchain cmake files found for 'UNKNOWN' in") != std::string::npos);

  // Test with converting the solution
  streamRedirect.ClearStringStreams();
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();

  // Test listing required toolchains
  EXPECT_EQ(0, RunProjMgr(4, argv, envp));
  const string& errStr3 = streamRedirect.GetErrorString();
  EXPECT_TRUE(errStr3.empty());
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
  EXPECT_EQ(0, RunProjMgr(8, argv, m_envp));

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
  EXPECT_EQ(1, RunProjMgr(8, argv, m_envp));

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
connections provided multiple times:\n\
  MultipleProvided\n\
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
connections provided multiple times:\n\
  MultipleProvided\n\
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
connections provided multiple times:\n\
  MultipleProvided\n\
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
error csolution: no compatible software layer found. Review required connections of the project\
\n\
";

  const string& errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(regex_match(errStr, regex(expected)));

  const string& expectedOutStr = "";

  const string& outStr = streamRedirect.GetOutString();
  EXPECT_TRUE(regex_match(outStr, regex(expectedOutStr)));
}

TEST_F(ProjMgrUnitTests, ListLayersIncompatibleNoLayerProvides) {
  StdStreamRedirect streamRedirect;
  char* argv[5];
  const string& csolution = testinput_folder + "/TestLayers/no-layer-provides.csolution.yml";
  argv[1] = (char*)"list";
  argv[2] = (char*)"layers";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-d";
  EXPECT_EQ(1, RunProjMgr(5, argv, m_envp));
  EXPECT_NE(string::npos, streamRedirect.GetErrorString().find("no provided connections from this layer are consumed"));
}

TEST_F(ProjMgrUnitTests, ListLayersOptionalLayerType) {
  StdStreamRedirect streamRedirect;
  char* argv[8];
  const string& csolution = testinput_folder + "/TestLayers/genericlayers.csolution.yml";
  const string& context = "genericlayers.OptionalLayerType+AnyBoard";
  argv[1] = (char*)"list";
  argv[2] = (char*)"layers";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  argv[5] = (char*)"-c";
  argv[6] = (char*)context.c_str();
  argv[7] = (char*)"-d";
  EXPECT_EQ(0, RunProjMgr(8, argv, m_envp));

  const string& expected ="\
check combined connections:\n\
  .*/TestLayers/genericlayers.cproject.yml\n\
    \\(Project Connections\\)\n\
connections are valid\n\
\n\
multiple clayers match type 'Board':\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/board1.clayer.yml\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/board2.clayer.yml\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/board3.clayer.yml\n\
";

  const string& errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(regex_search(errStr, regex(expected)));
}

TEST_F(ProjMgrUnitTests, ListLayersWithBoardSpecificPack) {
  StdStreamRedirect streamRedirect;
  char* argv[8];
  const string& csolution = testinput_folder + "/TestLayers/genericlayers.csolution.yml";
  const string& context = "genericlayers.OptionalLayerType+BoardSpecific";
  argv[1] = (char*)"list";
  argv[2] = (char*)"layers";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  argv[5] = (char*)"-c";
  argv[6] = (char*)context.c_str();
  argv[7] = (char*)"-d";
  EXPECT_EQ(0, RunProjMgr(8, argv, m_envp));

  const string& expected = "\
clayer of type 'BoardSpecific' was uniquely found:\n\
  .*/ARM/RteTest_DFP/0.2.0/Layers/board-specific.clayer.yml\n\
";

  const string& errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(regex_search(errStr, regex(expected)));
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
  EXPECT_EQ(1, RunProjMgr(8, argv, m_envp));

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
  EXPECT_EQ(1, RunProjMgr(8, argv, m_envp));

  errStr = streamRedirect.GetErrorString();
  string expectedStr = ".*invalid/clayer/path - error csolution: clayer search path does not exist\nerror csolution: no compatible software layer found. Review required connections of the project\n";
  EXPECT_TRUE(regex_match(errStr, regex(expectedStr)));
}

TEST_F(ProjMgrUnitTests, LayerVariables) {
  char* argv[7];

  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestLayers/variables.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

  // Check generated CPRJs
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/variables.BuildType1+TargetType1.cprj",
    testinput_folder + "/TestLayers/ref/variables/variables.BuildType1+TargetType1.cprj");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/variables.BuildType1+TargetType2.cprj",
    testinput_folder + "/TestLayers/ref/variables/variables.BuildType1+TargetType2.cprj");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/variables.BuildType2+TargetType1.cprj",
    testinput_folder + "/TestLayers/ref/variables/variables.BuildType2+TargetType1.cprj");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/variables.BuildType2+TargetType2.cprj",
    testinput_folder + "/TestLayers/ref/variables/variables.BuildType2+TargetType2.cprj");

   // Check generated cbuild-idx
   ProjMgrTestEnv::CompareFile(testoutput_folder + "/variables.cbuild-idx.yml",
     testinput_folder + "/TestLayers/ref/variables/variables.cbuild-idx.yml");
}

TEST_F(ProjMgrUnitTests, LayerVariablesRedefinition) {
  char* argv[7];
  StdStreamRedirect streamRedirect;
  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestLayers/variables-redefinition.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));
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
  EXPECT_EQ(1, RunProjMgr(8, argv, m_envp));

  string expectedErrStr = ".*\
error csolution: undefined variables in variables-notdefined.csolution.yml:.*\
  - \\$NotDefined\\$.*\
";

  string errStr = streamRedirect.GetErrorString();
  errStr.erase(std::remove(errStr.begin(), errStr.end(), '\n'), errStr.cend());
  EXPECT_TRUE(regex_match(errStr, regex(expectedErrStr)));

  // Validate --quiet mode output
  streamRedirect.ClearStringStreams();
  expectedErrStr = ".*\
error csolution: undefined variables in variables-notdefined.csolution.yml:.*\
  - \\$NotDefined\\$\
error csolution: no compatible software layer found. Review required connections of the project";

  argv[7] = (char*)"-q";
  EXPECT_EQ(1, RunProjMgr(8, argv, m_envp));
  errStr = streamRedirect.GetErrorString();
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
  EXPECT_EQ(1, RunProjMgr(8, argv, m_envp));

  const string& expectedErrStr = ".*\
error csolution: undefined variables in variables-notdefined.csolution.yml:.*\
  - \\$NotDefined\\$.*\
debug csolution: check for context \\'variables-notdefined\\.BuildType\\+TargetType\\'.*\
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
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

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
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

  // Check generated CPRJs
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test-access-sequences3.Debug+TEST_TARGET.cprj",
    testinput_folder + "/TestAccessSequences/ref/test-access-sequences3.Debug+TEST_TARGET.cprj");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test-access-sequences3.Release+TEST_TARGET.cprj",
    testinput_folder + "/TestAccessSequences/ref/test-access-sequences3.Release+TEST_TARGET.cprj");

  // Check generated cbuild-idx
 ProjMgrTestEnv::CompareFile(testoutput_folder + "/test-access-sequences2.cbuild-idx.yml",
   testinput_folder + "/TestAccessSequences/ref/test-access-sequences2.cbuild-idx.yml");
}

TEST_F(ProjMgrUnitTests, InvalidRefAccessSequences1) {
  char* argv[7];
  StdStreamRedirect streamRedirect;
  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestAccessSequences/test-not_exisitng-access-sequences1.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(7, argv, m_envp));
  const string& errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(errStr.find("context 'test-access-sequences-invalid' referenced by access sequence 'cmse-lib' does not exist or is not selected") != string::npos);
}

TEST_F(ProjMgrUnitTests, InvalidRefAccessSequences2) {
  char* argv[7];
  StdStreamRedirect streamRedirect;
  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestAccessSequences/test-not_exisitng-access-sequences2.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(7, argv, m_envp));
  const string& errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(errStr.find("context 'test-access-sequences5+CM3' referenced by access sequence 'elf' does not exist or is not selected") != string::npos);
}

TEST_F(ProjMgrUnitTests, PackAccessSequences) {
  char* argv[6];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestAccessSequences/pack-access-sequences.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();
  argv[5] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(6, argv, m_envp));

  // Check generated cbuild files
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/pack-access-sequences.cbuild-idx.yml",
    testinput_folder + "/TestAccessSequences/ref/pack-access-sequences.cbuild-idx.yml");
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/pack-access-sequences+CM4-Board.cbuild.yml",
    testinput_folder + "/TestAccessSequences/ref/pack-access-sequences+CM4-Board.cbuild.yml");

  // Check warnings
  const vector<string> expectedVec = {
    {"warning csolution: access sequence pack was not loaded: '$Pack(ARM::NotLoaded)$'"},
    {"warning csolution: access sequence '$Pack(Wrong.Format)' must have the format '$Pack(vendor::name)$'"}
  };
  const string& errStr = streamRedirect.GetErrorString();
  for (const auto& expected : expectedVec) {
    EXPECT_TRUE(errStr.find(expected) != string::npos) << "Missing Expected: " + expected;
  }
}

TEST_F(ProjMgrUnitTests, RunProjMgr_MalformedAccessSequences1) {
  char* argv[7];
  const string& csolution = testinput_folder + "/TestAccessSequences/test-malformed-access-sequences1.csolution.yml";

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(7, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_MalformedAccessSequences2) {
  char* argv[7];
  const string& csolution = testinput_folder + "/TestAccessSequences/malformed-access-sequences2.cproject.yml";

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(7, argv, 0));
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
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

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
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

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
  EXPECT_TRUE(ProjMgrYamlSchemaChecker().Validate(testoutput_folder + "/test-gpdsc.Debug+CM0.cbuild.yml"));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_GeneratorLayer) {
  char* argv[6];
  // convert solution
  const string& csolution = testinput_folder + "/TestGenerator/test-gpdsc-layer.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();
  argv[5] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(6, argv, m_envp));

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
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

  // Check generated CPRJs
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test_target_options+CM0.cprj",
              testinput_folder + "/TestSolution/ref/test_target_options+CM0.cprj");
}

TEST_F(ProjMgrUnitTests, ListPacks) {
  vector<string> packs;
  EXPECT_TRUE(m_worker.ParseContextSelection({}));
  m_worker.SetLoadPacksPolicy(LoadPacksPolicy::ALL);
  EXPECT_TRUE(m_worker.ListPacks(packs, false, "RTETest"));
  string allPacks;
  for (auto& pack : packs) {
    allPacks += pack + "\n";
  }

  auto pdscFiles = ProjMgrTestEnv::GetEffectivePdscFiles(false);
  string expected = ProjMgrTestEnv::GetFilteredPacksString(pdscFiles, "*RteTest*");
  EXPECT_EQ(allPacks, expected);
}

TEST_F(ProjMgrUnitTests, ListPacksLatest) {
  vector<string> packs;
  EXPECT_TRUE(m_worker.ParseContextSelection({}));
  m_worker.SetLoadPacksPolicy(LoadPacksPolicy::LATEST);
  EXPECT_TRUE(m_worker.ListPacks(packs, false, "RTETest"));
  string latestPacks;
  for (auto& pack : packs) {
    latestPacks += pack + "\n";
  }
  auto pdscFiles = ProjMgrTestEnv::GetEffectivePdscFiles(true);
  string expected = ProjMgrTestEnv::GetFilteredPacksString(pdscFiles, "*RteTest*");
  EXPECT_EQ(latestPacks, expected);
}

TEST_F(ProjMgrUnitTests, ListBoards) {
  set<string> expected = {
    "Keil::RteTest Dummy board:1.2.3 (ARM::RteTest_DFP@0.2.0)"
  };
  vector<string> devices;
  EXPECT_TRUE(m_worker.ParseContextSelection({}));
  EXPECT_TRUE(m_worker.ListBoards(devices, "DUMMY"));
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
  EXPECT_TRUE(m_worker.ListDevices(devices, "cm0"));
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
  EXPECT_TRUE(m_worker.ListDevices(devices, "cm3"));
  EXPECT_EQ(expected, set<string>(devices.begin(), devices.end()));
}

TEST_F(ProjMgrUnitTests, ListComponents) {
  set<string> expected = {
    "ARM::Device:Startup&RteTest Startup@2.0.3 (ARM::RteTest_DFP@0.2.0)",
  };
  vector<string> components;
  EXPECT_TRUE(m_worker.ParseContextSelection({}));
  EXPECT_TRUE(m_worker.ListComponents(components, "DEVICE:STARTUP"));
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
  EXPECT_TRUE(m_worker.ListDependencies(dependencies, "Core"));
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
  EXPECT_TRUE(m_parser.ParseCsolution(filenameInput, false, false));
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
  EXPECT_TRUE(m_parser.ParseCsolution(filenameInput, false, false));
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
  EXPECT_TRUE(m_parser.ParseCsolution(filenameInput, false, false));
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
  EXPECT_TRUE(m_parser.ParseCsolution(filenameInput, false, false));
  EXPECT_FALSE(m_worker.AddContexts(m_parser, descriptor, filenameInput));
}

TEST_F(ProjMgrUnitTests, GetInstalledPacks) {
  EXPECT_TRUE(m_worker.InitializeModel());
  auto kernel = ProjMgrKernel::Get();
  const string cmsisPackRoot = kernel->GetCmsisPackRoot();
  std::list<std::string> pdscFiles;

  // correct file, but no packs
  kernel->SetCmsisPackRoot(string(CMAKE_SOURCE_DIR) + "test/local");
  EXPECT_TRUE(kernel->GetEffectivePdscFiles(pdscFiles));
  EXPECT_TRUE(pdscFiles.empty());

  // incorrect file
  kernel->SetCmsisPackRoot(string(CMAKE_SOURCE_DIR) + "test/local-malformed");
  EXPECT_TRUE(kernel->GetEffectivePdscFiles(pdscFiles));
  EXPECT_TRUE(pdscFiles.empty());

  EXPECT_TRUE(m_worker.LoadAllRelevantPacks());

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
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

  // Check generated CPRJs
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test2.Debug+CM0.cprj",
    testinput_folder + "/TestSolution/ref/test2.Debug+CM0_pname.cprj");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test2.Debug+CM3.cprj",
    testinput_folder + "/TestSolution/ref/test2.Debug+CM3_pname.cprj");
}

TEST_F(ProjMgrUnitTests, RunProjMgrLayers_missing_project_file) {
  char* argv[7];
  StdStreamRedirect streamRedirect;
  const vector<string> expectedVec = {
{"unknown.cproject.yml - error csolution: cproject file was not found"}
  };
  const string& csolutionFile = testinput_folder + "/TestSolution/test_missing_project.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(7, argv, 0));
  const string& errStr = streamRedirect.GetErrorString();

  for (const auto& expected : expectedVec) {
    EXPECT_TRUE(errStr.find(expected) != string::npos) << "Missing Expected: " + expected;
  }
}

TEST_F(ProjMgrUnitTests, RunProjMgrLayers_pname) {
  char* argv[7];
  StdStreamRedirect streamRedirect;
  const string expected = "testlayers.cproject.yml - warning csolution: 'device: Dname' is deprecated at this level and accepted in *.csolution.yml only";
  const string& csolutionFile = testinput_folder + "/TestLayers/testlayers.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

  const string& errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(errStr.find(expected) != string::npos);
}

TEST_F(ProjMgrUnitTests, RunProjMgrLayers_no_device_found) {
  char* argv[7];
  const string& csolution = testinput_folder + "/TestLayers/testlayers_no_device_name.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(7, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_No_Device_Name) {
  char* argv[5];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestSolution/test_no_device_name.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"--cbuildgen";

  EXPECT_EQ(1, RunProjMgr(5, argv, 0));
  EXPECT_NE(string::npos, streamRedirect.GetErrorString().find("error csolution: processor name 'cm0_core0' was not found"));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_No_Board_No_Device) {
  // Test with no board no device info
  char* argv[7];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_no_board_no_device.cproject.yml");
  const string& expected = "missing device and/or board info";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(7, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Invalid_Board_Name) {
  // Test project with invalid board name
  char* argv[7];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_board_name_invalid.cproject.yml");
  const string& expected = "board 'Keil::RteTest_unknown' was not found";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(7, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Invalid_Board_Vendor) {
  // Test project with invalid vendor name
  char* argv[7];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_board_vendor_invalid.cproject.yml");
  const string& expected = "board 'UNKNOWN::RteTest Dummy board' was not found";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(7, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Only_Board_Info) {
  // Test project with only board info and missing device info
  char* argv[7];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_only_board.cproject.yml");
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

  // Check generated CPRJ
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test_only_board+TEST_TARGET.cprj",
    testinput_folder + "/TestSolution/TestProject4/test_only_board+TEST_TARGET.cprj");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Only_Board_No_Pname) {
  // Test project with only board info and no pname
  char* argv[7];
  const string& csolution = testinput_folder + "/TestProject/test_only_board_no_pname.cproject.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(7, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Device_Unknown) {
  // Test project with unknown device
  char* argv[7];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_device_unknown.cproject.yml");
  const string& expectedErrStr = R"(error csolution: specified device 'RteTest_ARM_UNKNOWN' not found in the installed packs. Use:
  cpackget add Vendor::PackName)";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(7, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expectedErrStr));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Device_Unknown_Vendor) {
  // Test project with unknown device vendor
  char* argv[7];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_device_unknown_vendor.cproject.yml");
  const string& expectedErrStr = R"(error csolution: specified device 'TEST::RteTest_ARMCM0' not found in the installed packs. Use:
  cpackget add Vendor::PackName)";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(7, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expectedErrStr));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Device_Unknown_Processor) {
  // Test project with unknown processor
  char* argv[7];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_device_unknown_processor.cproject.yml");
  const string& expected = "processor name 'NOT_AVAILABLE' was not found";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(7, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Device_Unavailable_In_Board) {
  // Test project with device different from the board's mounted device
  char* argv[7];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_device_unavailable_in_board.cproject.yml");
  const string& expectedErrStr = R"(error csolution: specified device 'RteTest_ARMCM7' not found in the installed packs. Use:
  cpackget add Vendor::PackName)";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(7, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expectedErrStr));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Device_Pname_Unavailable_In_Board) {
  // Test project with device processor name different from the board's mounted device's processors
  char* argv[7];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_device_pname_unavailable_in_board.cproject.yml");
  const string& expected = "processor name 'cm0_core7' was not found";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(7, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Only_Device_Info) {
  // Test project with only device info
  char* argv[7];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test.cproject.yml");
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Board_And_Device_Info) {
  // Test project with valid board and device info
  char* argv[7];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_board_and_device.cproject.yml");
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

  // Check generated cbuild YML
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/test_board_and_device+TEST_TARGET.cbuild.yml",
    testinput_folder + "/TestSolution/TestProject4/test_board_and_device+TEST_TARGET.cbuild.yml");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Correct_Board_Wrong_Device_Info) {
  // Test project with correct board info but wrong device info
  char* argv[7];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_correct_board_wrong_device.cproject.yml");
  const string& expectedErrStr = R"(error csolution: specified device 'ARM::RteTest_ARMCM_Unknown' not found in the installed packs. Use:
  cpackget add Vendor::PackName)";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(7, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expectedErrStr));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Correct_Device_Wrong_Board_Info) {
  // Test project with correct device info but wrong board info
  char* argv[7];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_correct_device_wrong_board.cproject.yml");
  const string& expected = "board 'Keil::RteTest unknown board' was not found";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(7, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Board_Multi_Mounted_Devices) {
  // Test Project with board with multiple mounted devices
  char* argv[8];
  const string& csolution = testinput_folder + "/TestSolution/board-devices.csolution.yml";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();
  argv[5] = (char*)"--cbuildgen";
  argv[6] = (char*)"-c";

  // test with only 'board' specified
  argv[7] = (char*)"+Only_Board";
  EXPECT_EQ(1, RunProjMgr(8, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find("error csolution: found multiple mounted devices, one of the following must be specified:\nRteTest_ARMCM3\nRteTest_ARMCM0_Dual"));

  // test with 'board' and 'device' specified
  argv[7] = (char*)"+Board_And_Device";
  EXPECT_EQ(0, RunProjMgr(8, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Board_Device_Variant) {
  // Test Project with only board info and single mounted device with single variant
  char* argv[7];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_board_device_variant.cproject.yml");

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Board_Multi_Variants_And_Device) {
  // Test Project with device variant and board info and mounted device with multiple variants
  char* argv[7];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_board_multi_variant_and_device.cproject.yml");

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Board_Multi_Variants) {
  // Test Project with only board info and mounted device with multiple variants
  char* argv[7];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_board_multi_variant.cproject.yml");
  const string& expected = "found multiple device variants";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(7, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Board_No_Mounted_Devices) {
  // Test Project with only board info and no mounted devices
  char* argv[7];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_board_no_mounted_device.cproject.yml");
  const string& expected = "found no mounted device";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(7, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Board_Device_Info) {
  // Test Project with board mounted device different than selected devices
  char* argv[7];
  string csolutionFile = UpdateTestSolutionFile("./TestProject4/test_mounted_device_differs_selected_device.cproject.yml");
  const string& expected = "warning csolution: specified device 'RteTest_ARMCM0' is not among board mounted devices";
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));
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
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

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
  EXPECT_EQ(0, RunProjMgr(5, argv, m_envp));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListGeneratorsEmptyContextMultipleTypes) {
  char* argv[5];
  const string& csolution = testinput_folder + "/TestGenerator/test-gpdsc-multiple-types.csolution.yml";
  argv[1] = (char*)"list";
  argv[2] = (char*)"generators";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  EXPECT_EQ(0, RunProjMgr(5, argv, m_envp));
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
    EXPECT_EQ(0, RunProjMgr(8, argv, m_envp));
  } else {
    EXPECT_EQ(1, RunProjMgr(8, argv, m_envp));
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
    EXPECT_EQ(0, RunProjMgr(6, argv, m_envp));
  } else {
    EXPECT_EQ(1, RunProjMgr(6, argv, m_envp));
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
  EXPECT_EQ(1, RunProjMgr(6, argv, m_envp));

  // specify a single context
  argv[6] = (char*)"-c";
  argv[7] = (char*)"test-gpdsc.Debug+CM0";
  const string& hostType = CrossPlatformUtils::GetHostType();
  if (shouldHaveGeneratorForHostType(hostType)) {
    EXPECT_EQ(0, RunProjMgr(8, argv, m_envp));
  } else {
    EXPECT_EQ(1, RunProjMgr(8, argv, m_envp));
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
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

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
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

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
  string genFolder = testcmsispack_folder + "/ARM/RteTestGenerator/0.1.0/Generator with spaces";
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
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));
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
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

  // Check generated CPRJs
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test2.Debug+CM0.cprj",
    testinput_folder + "/TestSolution/ref/test2.Debug+CM0_pack_selection.cprj");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test2.Debug+TestGen.cprj",
    testinput_folder + "/TestSolution/ref/test2.Debug+TestGen.cprj");

  // Check generated cbuild-pack file
  ProjMgrTestEnv::CompareFile(testinput_folder + "/TestSolution/test_pack_selection.cbuild-pack.yml",
    testinput_folder + "/TestSolution/ref/test_pack_selection.cbuild-pack.yml");
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
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));
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
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(7, argv, 0));

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
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

  // Check generated CPRJ
  ProjMgrTestEnv:: CompareFile(testoutput_folder + "/pack_path+CM0.cprj",
    testinput_folder + "/TestSolution/ref/pack_path+CM0.cprj");
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_Local_Pack_Invalid) {
  StdStreamRedirect streamRedirect;
  char* argv[7];

  // verify schema check
  const string& csolution = testinput_folder + "/TestSolution/pack_path_invalid.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();
  argv[5] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(6, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find("error csolution: schema check failed, verify syntax"));

  // run with schema check disabled
  streamRedirect.ClearStringStreams();
  argv[6] = (char*)"-n";
  EXPECT_EQ(1, RunProjMgr(7, argv, 0));
  errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find("error csolution: pack 'ARM::RteTest_DFP' specified with 'path' must not have a version"));
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
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

  // remove additionally added file
  RteFsUtils::RemoveFile(destPackFile);
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_Local_Pack_Path_Not_Found) {
  char* argv[7];
  StdStreamRedirect streamRedirect;
  const string errExpected = "/SolutionSpecificPack/ARM does not exist";
  const string& csolution = testinput_folder + "/TestSolution/test_local_pack_path_not_found.csolution.yml";

  // convert --solution solution.yml
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(7, argv, 0));

  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(errExpected));
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_Local_Pack_File_Not_Found) {
  char* argv[7];
  StdStreamRedirect streamRedirect;
  const string errExpected = "pdsc file was not found in: .*/SolutionSpecificPack/Device";
  const string& csolution = testinput_folder + "/TestSolution/test_local_pack_file_not_found.csolution.yml";

  // convert --solution solution.yml
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(7, argv, 0));

  auto errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(regex_search(errStr, regex(errExpected)));
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
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

  // Check generated CPRJs
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test1.Debug+CM0.cprj",
    testinput_folder + "/TestSolution/ref/test1.Debug+CM0_board_package.cprj");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test1.Release+CM0.cprj",
    testinput_folder + "/TestSolution/ref/test1.Release+CM0_board_package.cprj");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_LoadPacksPolicy_Required) {
  char* argv[7];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestSolution/test_no_packs.csolution.yml";
  const string& cbuildPack = testinput_folder + "/TestSolution/test_no_packs.cbuild-pack.yml";
  EXPECT_TRUE(RteFsUtils::RemoveFile(cbuildPack));
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-l";
  argv[5] = (char*)"required";
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(7, argv, 0));
  auto errorStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(errorStr.find("error csolution: required packs must be specified") != string::npos);
}

TEST_F(ProjMgrUnitTests, RunProjMgr_LoadPacksPolicy_Invalid) {
  char* argv[7];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestSolution/test_no_packs.csolution.yml";
  const string& cbuildPack = testinput_folder + "/TestSolution/test_no_packs.cbuild-pack.yml";
  EXPECT_TRUE(RteFsUtils::RemoveFile(cbuildPack));
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-l";
  argv[5] = (char*)"invalid";
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(7, argv, 0));
  auto errorStr = streamRedirect.GetErrorString();
  EXPECT_EQ(0, errorStr.find("error csolution: unknown load option: 'invalid', it must be 'latest', 'all' or 'required'\n"));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_LoadPacksPolicy_Latest) {
  char* argv[7];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestSolution/test_no_packs.csolution.yml";
  const string& cbuildPack = testinput_folder + "/TestSolution/test_no_packs.cbuild-pack.yml";
  EXPECT_TRUE(RteFsUtils::RemoveFile(cbuildPack));
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-l";
  argv[5] = (char*)"latest";
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_LoadPacksPolicy_All) {
  char* argv[7];
  const string& csolution = testinput_folder + "/TestSolution/test_no_packs.csolution.yml";
  const string& cbuildPack = testinput_folder + "/TestSolution/test_no_packs.cbuild-pack.yml";
  EXPECT_TRUE(RteFsUtils::RemoveFile(cbuildPack));
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-l";
  argv[5] = (char*)"all";
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

  const string& csolution2 = testinput_folder + "/TestSolution/test_pack_selection.csolution.yml";
  argv[3] = (char*)csolution2.c_str();
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_GetCdefaultFile1) {
  // Valid file search
  const string& testdir = testoutput_folder + "/FindFileRegEx";
  const string& fileName = testdir + "/cdefault.yml";
  RteFsUtils::CreateDirectories(testdir);
  RteFsUtils::CreateTextFile(fileName, "");
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
  char* argv[7];
  // convert --solution solution.yml
  // csolution is empty, all information from cdefault.yml is taken
  const string& csolution = testinput_folder + "/TestDefault/empty.csolution.yml";
  const string& output = testoutput_folder + "/empty";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)output.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

  // Check generated CPRJs
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/empty/project.Debug+TEST_TARGET.cprj",
    testinput_folder + "/TestDefault/ref/empty/project.Debug+TEST_TARGET.cprj");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/empty/project.Release+TEST_TARGET.cprj",
    testinput_folder + "/TestDefault/ref/empty/project.Release+TEST_TARGET.cprj");
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_DefaultFile2) {
  char* argv[7];
  // convert --solution solution.yml
  // csolution.yml is complete, no information from cdefault.yml is taken except from misc
  const string& csolution = testinput_folder + "/TestDefault/full.csolution.yml";
  const string& output = testoutput_folder + "/full";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)output.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

  // Check generated CPRJs
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/full/project.Debug+TEST_TARGET.cprj",
    testinput_folder + "/TestDefault/ref/full/project.Debug+TEST_TARGET.cprj");
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/full/project.Release+TEST_TARGET.cprj",
    testinput_folder + "/TestDefault/ref/full/project.Release+TEST_TARGET.cprj");
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_DefaultFileInCompilerRoot) {
  char* argv[7];
  const string cdefault = testinput_folder + "/TestDefault/cdefault.yml";
  const string cdefaultInCompilerRoot = testcmsiscompiler_folder + "/cdefault.yml";
  RteFsUtils::MoveExistingFile(cdefaultInCompilerRoot, cdefaultInCompilerRoot + ".bak");
  RteFsUtils::MoveExistingFile(cdefault, cdefaultInCompilerRoot);
  const string& csolution = testinput_folder + "/TestDefault/empty.csolution.yml";
  RteFsUtils::RemoveFile(testinput_folder + "/TestDefault/empty.cbuild-pack.yml");
  RteFsUtils::RemoveDir(testoutput_folder);
  const string& output = testoutput_folder + "/empty";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)output.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

  // Check generated cbuild YMLs
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/empty/empty.cbuild-idx.yml",
    testinput_folder + "/TestDefault/ref/empty/empty.cbuild-idx.yml");
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/empty/project.Debug+TEST_TARGET.cbuild.yml",
    testinput_folder + "/TestDefault/ref/empty/project.Debug+TEST_TARGET.cbuild.yml");
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/empty/project.Release+TEST_TARGET.cbuild.yml",
    testinput_folder + "/TestDefault/ref/empty/project.Release+TEST_TARGET.cbuild.yml");

  // Check cbuild-idx.yml schema
  EXPECT_TRUE(ProjMgrYamlSchemaChecker().Validate(testoutput_folder + "/empty/empty.cbuild-idx.yml"));

  RteFsUtils::MoveExistingFile(cdefaultInCompilerRoot, cdefault);
  RteFsUtils::MoveExistingFile(cdefaultInCompilerRoot + ".bak", cdefaultInCompilerRoot);
}

TEST_F(ProjMgrUnitTests, RunProjMgr_NoUpdateRTEFiles) {
  char* argv[8];
  const string csolutionFile = UpdateTestSolutionFile("./TestProject4/test.cproject.yml");
  const string rteFolder = RteFsUtils::ParentPath(csolutionFile) + "/TestProject4/RTE";
  set<string> rteFiles;
  RteFsUtils::RemoveDir(rteFolder);
  StdStreamRedirect streamRedirect;

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--no-update-rte";
  argv[7] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(8, argv, m_envp));

  EXPECT_NE(streamRedirect.GetOutString().find("RTE/_TEST_TARGET/RTE_Components.h was recreated"), string::npos);

  // Only constructed files are created in the RTE folder
  GetFilesInTree(rteFolder, rteFiles);
  const set<string> expected = { "RTE_Components.h", "_TEST_TARGET" };
  EXPECT_EQ(expected, rteFiles);

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
    "ARM::Device:RteTest Generated Component:RteTestNoDryRun@1.1.0 (ARM::RteTestGenerator@0.1.0)",
    "ARM::Device:RteTest Generated Component:RteTestSimple@1.1.0 (ARM::RteTestGenerator@0.1.0)",
    "ARM::Device:RteTest Generated Component:RteTestWithKey@1.1.0 (ARM::RteTestGenerator@0.1.0)",
    "ARM::Device:RteTest Generated Component:RteTestNoExe@1.1.0 (ARM::RteTestGenerator@0.1.0)",
    "ARM::Device:RteTest Generated Component:RteTestOverlap@1.1.0 (ARM::RteTestGenerator@0.1.0)",
    "ARM::RteTestGenerator:Check Global Generator@0.9.0 (ARM::RteTestGenerator@0.1.0)"
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
  char* argv[7];
  const string& csolution = testinput_folder + "/Validation/dependencies.csolution.yml";
  RemoveCbuildSetFile(csolution);

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"--cbuildgen";
  argv[5] = (char*)"-c";

  map<string, string> testData = {
    {"conflict+CM0", "warning csolution: dependency validation for context 'conflict+CM0' failed:\n\
CONFLICT RteTest:ApiExclusive@1.0.0\n\
  ARM::RteTest:ApiExclusive:S1\n\
  ARM::RteTest:ApiExclusive:S2" },
    {"incompatible+CM0", "warning csolution: dependency validation for context 'incompatible+CM0' failed:\n\
INCOMPATIBLE ARM::RteTest:Check:Incompatible@0.9.9\n\
  deny RteTest:Dependency:Incompatible_component\n\
  ARM::RteTest:Dependency:Incompatible_component" },
    {"incompatible-variant+CM0", "warning csolution: dependency validation for context 'incompatible-variant+CM0' failed:\n\
INCOMPATIBLE_VARIANT ARM::RteTest:Check:IncompatibleVariant@0.9.9\n\
  require RteTest:Dependency:Variant&Compatible\n\
  ARM::RteTest:Dependency:Variant" },
    {"missing+CM0", "warning csolution: dependency validation for context 'missing+CM0' failed:\n\
MISSING ARM::RteTest:Check:Missing@0.9.9\n\
  require RteTest:Dependency:Missing" },
    {"selectable+CM0", "warning csolution: dependency validation for context 'selectable+CM0' failed:\n\
SELECTABLE ARM::Device:Startup&RteTest Startup@2.0.3\n\
  require RteTest:CORE\n\
  ARM::RteTest:CORE" }
  };

  for (const auto& [context, expected] : testData) {
    RemoveCbuildSetFile(csolution);
    StdStreamRedirect streamRedirect;
    argv[6] = (char*)context.c_str();
    EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));
    auto errorStr = streamRedirect.GetErrorString();
    EXPECT_NE(string::npos, errorStr.find(expected));
  }
}

TEST_F(ProjMgrUnitTests, Convert_ValidationResults_Filtering) {
  char* argv[7];
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[4] = (char*)"--cbuildgen";
  argv[5] = (char*)"-c";

  vector<tuple<string, int, string>> testData = {
    {"recursive", 1, "\
warning csolution: RTE Model reports:\n\
ARM::RteTestRecursive@0.1.0: condition 'Recursive': error #503: direct or indirect recursion detected\n\
error csolution: component 'RteTest:Check:Recursive' not found in included packs\n"},
    {"missing-condition", 0, "\
warning csolution: RTE Model reports:\n\
ARM::RteTestMissingCondition@0.1.0: component 'ARM::RteTest:Check:MissingCondition@0.9.9(MissingCondition)[]': error #501: error(s) in component definition:\n\
 condition 'MissingCondition' not found\n"},
  };

  for (const auto& [project, expectedReturn, expectedMessage] : testData) {
    StdStreamRedirect streamRedirect;
    const string& csolution = testinput_folder + "/Validation/" + project + ".csolution.yml";
    const string& context = project + "+CM0";
    argv[3] = (char*)csolution.c_str();
    argv[6] = (char*)context.c_str();
    EXPECT_EQ(expectedReturn, RunProjMgr(7, argv, m_envp));
    auto errorStr = streamRedirect.GetErrorString();
    EXPECT_EQ(0, errorStr.find(expectedMessage));
  }
}

TEST_F(ProjMgrUnitTests, Convert_ValidationResults_Quiet_Mode) {
  char* argv[8];
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[4] = (char*)"-c";

  string expectedMsg = "error csolution: component 'RteTest:Check:Recursive' not found in included packs\nerror csolution: processing context 'recursive+CM0' failed\n";

  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/Validation/recursive.csolution.yml";
  const string& context = "recursive+CM0";
  argv[3] = (char*)csolution.c_str();
  argv[5] = (char*)context.c_str();
  argv[6] = (char*)"-q";
  argv[7] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(8, argv, m_envp));
  const string& errStr = streamRedirect.GetErrorString();
  EXPECT_EQ(string::npos, errStr.find("warning csolution"));
  EXPECT_EQ(string::npos, errStr.find("debug csolution"));
  EXPECT_STREQ(errStr.c_str(), expectedMsg.c_str());
}

TEST_F(ProjMgrUnitTests, OutputDirs) {
  char* argv[5];
  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestSolution/outdirs.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(5, argv, m_envp));

  // Check generated CPRJs
 ProjMgrTestEnv:: CompareFile(testinput_folder + "/TestSolution/AC6/test1.Debug+TypeA.cprj",
    testinput_folder + "/TestSolution/ref/AC6/test1.Debug+TypeA.cprj");
 ProjMgrTestEnv:: CompareFile(testinput_folder + "/TestSolution/AC6/test1.Debug+TypeB.cprj",
    testinput_folder + "/TestSolution/ref/AC6/test1.Debug+TypeB.cprj");
 ProjMgrTestEnv:: CompareFile(testinput_folder + "/TestSolution/GCC/test1.Release+TypeA.cprj",
    testinput_folder + "/TestSolution/ref/GCC/test1.Release+TypeA.cprj");
 ProjMgrTestEnv:: CompareFile(testinput_folder + "/TestSolution/GCC/test1.Release+TypeB.cprj",
    testinput_folder + "/TestSolution/ref/GCC/test1.Release+TypeB.cprj");

   // Check custom tmp directory
   const YAML::Node& cbuild = YAML::LoadFile(testinput_folder + "/TestSolution/outdirs.cbuild-idx.yml");
   EXPECT_EQ("custom/tmp/path", cbuild["build-idx"]["tmpdir"].as<string>());
}

TEST_F(ProjMgrUnitTests, OutputDirsTmpdirAccessSequence) {
  StdStreamRedirect streamRedirect;
  char* argv[4];
  const string& csolution = testinput_folder + "/TestSolution/tmpdir-as.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(4, argv, m_envp));

  auto errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(regex_search(errStr, regex("warning csolution: 'tmpdir' does not support access sequences and must be relative to csolution.yml")));
}

TEST_F(ProjMgrUnitTests, OutputDirsAbsolutePath) {
  StdStreamRedirect streamRedirect;
  char* argv[5];
  const string& csolution = testinput_folder + "/TestSolution/outdirs-absolute.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(5, argv, m_envp));

  auto errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(regex_search(errStr, regex("warning csolution: absolute path .* is not portable, use relative path instead")));
}

TEST_F(ProjMgrUnitTests, ProjectSetup) {
  char* argv[5];
  // convert setup-test.solution.yml
  const string& csolution = testinput_folder + "/TestProjectSetup/setup-test.csolution.yml";
  const string& output = testoutput_folder;
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-o";
  argv[4] = (char*)output.c_str();
  EXPECT_EQ(0, RunProjMgr(5, argv, m_envp));

  // check generated cbuild.yml files
  ProjMgrTestEnv:: CompareFile(testoutput_folder + "/out/setup-test/TEST_TARGET/Build_AC6/setup-test.Build_AC6+TEST_TARGET.cbuild.yml",
    testinput_folder + "/TestProjectSetup/ref/setup-test.Build_AC6+TEST_TARGET.cbuild.yml");
  ProjMgrTestEnv:: CompareFile(testoutput_folder + "/out/setup-test/TEST_TARGET/Build_GCC/setup-test.Build_GCC+TEST_TARGET.cbuild.yml",
    testinput_folder + "/TestProjectSetup/ref/setup-test.Build_GCC+TEST_TARGET.cbuild.yml");

  if (CrossPlatformUtils::GetHostType() == "win") {
    // check if windows absolute add-path is tolerated and persists correctly
    const YAML::Node& cbuild = YAML::LoadFile(testinput_folder + "/TestProjectSetup/ref/setup-test.AbsolutePath+TEST_TARGET.cbuild.yml");
    EXPECT_EQ("C:/Absolute/Path", cbuild["build"]["add-path"][3].as<string>());
  }
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
  argv[2] = (char*)"target-sets";
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
  char* argv[11];

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
  argv[10] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(11, argv, m_envp));

  // Check generated CPRJs
 ProjMgrTestEnv:: CompareFile(testoutput_folder + "/test2.Debug+TestGen_export.cprj",
    testinput_folder + "/TestSolution/ref/test2.Debug+TestGen_export.cprj");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_WriteCprjFail) {
  char* argv[11];

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
  argv[10] = (char*)"--cbuildgen";

  // Fail to write export cprj file
  RteFsUtils::CreateTextFile(outputFolder + "/test2.Debug+CM0_export.cprj", "");
  RteFsUtils::SetTreeReadOnly(outputFolder);
  EXPECT_EQ(1, RunProjMgr(11, argv, 0));

  // Fail to write cprj file
  RteFsUtils::SetTreeReadOnly(outputFolder);
  EXPECT_EQ(1, RunProjMgr(11, argv, 0));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_PreInclude) {
  char* argv[6];
  // update-rte pre-include.solution.yml
  const string& csolution = testinput_folder + "/TestSolution/pre-include.csolution.yml";
  argv[1] = (char*)csolution.c_str();
  argv[2] = (char*)"update-rte";
  EXPECT_EQ(0, RunProjMgr(3, argv, m_envp));
  // convert pre-include.solution.yml
  argv[2] = (char*)"convert";
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();
  argv[5] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(6, argv, m_envp));

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
  EXPECT_TRUE(m_parser.ParseCsolution(filenameInput, false, false));
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

TEST_F(ProjMgrUnitTests, RunCheckContextProcessing) {
  char* argv[8];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestSolution/contexts.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-c";
  argv[4] = (char*)"contexts.B1+T1";
  argv[5] = (char*)"-o";
  argv[6] = (char*)testoutput_folder.c_str();
  argv[7] = (char*)"--cbuildgen";
  EXPECT_EQ(2, RunProjMgr(8, argv, 0));

  // Check error for processed context
  const string expected = "error csolution: undefined variables in contexts.csolution.yml:\n  - $LayerVar$\n\n";
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(errStr.find(expected) != string::npos);

  // Contexts not being processed don't trigger warnings
  const vector<string> absenceExpectedList = { "'contexts.B1+T2'", "'contexts.B2+T1'", "'contexts.B2+T2'" };
  for (const auto& absenceExpected : absenceExpectedList) {
    EXPECT_TRUE(errStr.find(absenceExpected) == string::npos);
  }
}

TEST_F(ProjMgrUnitTests, RunProjMgrOutputFiles) {
  char* argv[6];
  StdStreamRedirect streamRedirect;

  // convert solution.yml
  const string& csolution = testinput_folder + "/TestSolution/outputFiles.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();
  argv[5] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(6, argv, m_envp));

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
  EXPECT_TRUE(regex_search(errStr, regex(expected)));
}

TEST_F(ProjMgrUnitTests, SelectToolchains) {
  const string& AC6_6_6_5 = testinput_folder + "/TestToolchains/AC6.6.6.5.cmake";
  RteFsUtils::CreateTextFile(AC6_6_6_5, "");
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
  char* argv[9];
  const string& csolution = testinput_folder + "/TestSolution/toolchain-selection.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"-t";

  argv[7] = (char*)"AC6@6.20.0";
  argv[8] = (char*)"--cbuildgen";
  RemoveCbuildSetFile(csolution);
  EXPECT_EQ(0, RunProjMgr(9, argv, envp));
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/toolchain.Debug+Target.cprj",
    testinput_folder + "/TestSolution/ref/toolchains/toolchain.Debug+Target.cprj.ac6_6_20_0");

  argv[7] = (char*)"AC6@6.16.1";
  RemoveCbuildSetFile(csolution);
  EXPECT_EQ(0, RunProjMgr(9, argv, envp));
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/toolchain.Debug+Target.cprj",
    testinput_folder + "/TestSolution/ref/toolchains/toolchain.Debug+Target.cprj.ac6_6_16_1");

  argv[7] = (char*)"AC6@6.6.5";
  RemoveCbuildSetFile(csolution);
  EXPECT_EQ(0, RunProjMgr(9, argv, envp));
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/toolchain.Debug+Target.cprj",
    testinput_folder + "/TestSolution/ref/toolchains/toolchain.Debug+Target.cprj.ac6_6_6_5");

  argv[7] = (char*)"GCC";
  RemoveCbuildSetFile(csolution);
  EXPECT_EQ(0, RunProjMgr(9, argv, envp));
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/toolchain.Debug+Target.cprj",
    testinput_folder + "/TestSolution/ref/toolchains/toolchain.Debug+Target.cprj.gcc");

  argv[7] = (char*)"IAR";
  RemoveCbuildSetFile(csolution);
  EXPECT_EQ(0, RunProjMgr(9, argv, envp));
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/toolchain.Debug+Target.cprj",
    testinput_folder + "/TestSolution/ref/toolchains/toolchain.Debug+Target.cprj.iar");

  argv[7] = (char*)"AC6@6.0.0";
  RemoveCbuildSetFile(csolution);
  EXPECT_EQ(1, RunProjMgr(9, argv, envp));

  RteFsUtils::RemoveFile(AC6_6_6_5);
}

TEST_F(ProjMgrUnitTests, ToolchainRedefinition) {
  StdStreamRedirect streamRedirect;
  char* argv[10];
  const string& csolution = testinput_folder + "/TestSolution/toolchain-redefinition.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();
  argv[5] = (char*)"--cbuildgen";
  argv[6] = (char*)"-c";
  argv[7] = (char*)".Error";
  EXPECT_EQ(1, RunProjMgr(8, argv, m_envp));
  const string err = streamRedirect.GetErrorString();
  const string expectedErr =\
    "error csolution: redefinition from 'AC6' into 'GCC' is not allowed\n"\
    "error csolution: processing context 'toolchain.Error+RteTest_ARMCM3' failed\n";
  EXPECT_EQ(err, expectedErr);

  streamRedirect.ClearStringStreams();
  argv[7] = (char*)".Warning";
  argv[8] = (char*)"-t";
  argv[9] = (char*)"GCC";
  EXPECT_EQ(0, RunProjMgr(10, argv, m_envp));
  const string warn = streamRedirect.GetErrorString();
  const string expectedWarn =\
    "warning csolution: redefinition from 'AC6' into 'GCC'\n";
  EXPECT_EQ(warn, expectedWarn);

  const YAML::Node& cbuild = YAML::LoadFile(testoutput_folder + "/toolchain.Warning+RteTest_ARMCM3.cbuild.yml");
  EXPECT_EQ(cbuild["build"]["compiler"].as<string>(), "GCC");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_LinkerOptions) {
  char* argv[8];
  const string& csolution = testinput_folder + "/TestSolution/LinkerOptions/linker.csolution.yml";
  RemoveCbuildSetFile(csolution);

  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-c";
  argv[4] = (char*)"linker.Debug_*+RteTest_ARMCM3";
  argv[5] = (char*)"-o";
  argv[6] = (char*)testoutput_folder.c_str();
  argv[7] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(8, argv, m_envp));

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

TEST_F(ProjMgrUnitTests, RunProjMgr_MissingLinkerScript) {
  char* argv[7];
  const string& csolution = testinput_folder + "/TestSolution/LinkerOptions/linker.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();
  argv[5] = (char*)"-c";
  argv[6] = (char*)"linker.Missing+RteTest_ARMCM3";
  EXPECT_EQ(1, RunProjMgr(7, argv, m_envp));
  const string& expected = "file '.*/TestSolution/LinkerOptions/unknown.sct' was not found";
  const YAML::Node& cbuild = YAML::LoadFile(testoutput_folder + "/linker.cbuild-idx.yml");
  EXPECT_TRUE(regex_search(cbuild["build-idx"]["cbuilds"][0]["messages"]["errors"][0].as<string>(), regex(expected)));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_LinkerOptions_Auto) {
  char* argv[8];
  const string& csolution = testinput_folder + "/TestSolution/LinkerOptions/linker.csolution.yml";
  RemoveCbuildSetFile(csolution);

  // linker script from component
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-c";
  argv[4] = (char*)"linker.FromComponent+RteTest_ARMCM3";
  argv[5] = (char*)"-o";
  argv[6] = (char*)testoutput_folder.c_str();
  argv[7] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(8, argv, m_envp));

  // check generated files
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/linker.FromComponent+RteTest_ARMCM3.cprj",
    testinput_folder + "/TestSolution/LinkerOptions/ref/linker.FromComponent+RteTest_ARMCM3.cprj");
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/linker.FromComponent+RteTest_ARMCM3.cbuild.yml",
    testinput_folder + "/TestSolution/LinkerOptions/ref/linker.FromComponent+RteTest_ARMCM3.cbuild.yml");

  RteFsUtils::RemoveDir(testinput_folder + "/TestSolution/LinkerOptions/RTE");
  // 'auto' enabled
  argv[4] = (char*)"linker.AutoGen+RteTest_ARMCM3";
  EXPECT_EQ(0, RunProjMgr(8, argv, m_envp));

  // check generated files
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/linker.AutoGen+RteTest_ARMCM3.cprj",
    testinput_folder + "/TestSolution/LinkerOptions/ref/linker.AutoGen+RteTest_ARMCM3.cprj");
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/linker.AutoGen+RteTest_ARMCM3.cbuild.yml",
    testinput_folder + "/TestSolution/LinkerOptions/ref/linker.AutoGen+RteTest_ARMCM3.cbuild.yml");
  ProjMgrTestEnv::CompareFile(testinput_folder + "/TestSolution/LinkerOptions/RTE/Device/RteTest_ARMCM3/regions_RteTest_ARMCM3.h",
    testinput_folder + "/TestSolution/LinkerOptions/ref/regions_RteTest_ARMCM3.h");

  RteFsUtils::RemoveDir(testinput_folder + "/TestSolution/LinkerOptions/RTE");
  // 'auto' enabled for board
  argv[4] = (char*)"linker.AutoGen+RteTest_Board";
  EXPECT_EQ(0, RunProjMgr(8, argv, m_envp));

  // check generated files
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/linker.AutoGen+RteTest_Board.cprj",
    testinput_folder + "/TestSolution/LinkerOptions/ref/linker.AutoGen+RteTest_Board.cprj");
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/linker.AutoGen+RteTest_Board.cbuild.yml",
    testinput_folder + "/TestSolution/LinkerOptions/ref/linker.AutoGen+RteTest_Board.cbuild.yml");
  ProjMgrTestEnv::CompareFile(testinput_folder + "/TestSolution/LinkerOptions/RTE/Device/RteTest_ARMCM3/regions_RteTest-Test-board_With.Memory.h",
    testinput_folder + "/TestSolution/LinkerOptions/ref/regions_RteTest-Test-board_With.Memory.h");

  RteFsUtils::RemoveDir(testinput_folder + "/TestSolution/LinkerOptions/RTE");
  // 'auto' enabled warning
  StdStreamRedirect streamRedirect;
  argv[4] = (char*)"linker.AutoGenWarning+RteTest_ARMCM3";
  EXPECT_EQ(0, RunProjMgr(8, argv, m_envp));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(errStr.find("warning csolution: conflict: automatic linker script generation overrules specified script\
 '../data/TestSolution/LinkerOptions/layer/linkerScript.ld'") != string::npos);
}

TEST_F(ProjMgrUnitTests, RunProjMgr_LinkerPriority) {
  char* argv[8];
  const string& csolution = testinput_folder + "/TestSolution/LinkerOptions/linker.csolution.yml";
  RemoveCbuildSetFile(csolution);

  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-c";
  argv[4] = (char*)"linker.Priority*+RteTest_ARMCM3";
  argv[5] = (char*)"-o";
  argv[6] = (char*)testoutput_folder.c_str();
  argv[7] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(8, argv, m_envp));

  // Check generated cbuild YMLs
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/linker.PriorityRegions+RteTest_ARMCM3.cbuild.yml",
    testinput_folder + "/TestSolution/LinkerOptions/ref/linker.PriorityRegions+RteTest_ARMCM3.cbuild.yml");
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/linker.PriorityDefines+RteTest_ARMCM3.cbuild.yml",
    testinput_folder + "/TestSolution/LinkerOptions/ref/linker.PriorityDefines+RteTest_ARMCM3.cbuild.yml");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_LinkerOptions_Redefinition) {
  char* argv[8];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestSolution/LinkerOptions/linker.csolution.yml";
  RemoveCbuildSetFile(csolution);

  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-c";
  argv[4] = (char*)"linker.Redefinition+RteTest_ARMCM3";
  argv[5] = (char*)"-o";
  argv[6] = (char*)testoutput_folder.c_str();
  argv[7] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(8, argv, 0));

  // Check error messages
  const vector<string> expectedVec = {
{"error csolution: redefinition from '.*/linkerScript.ld' into '.*/linkerScript2.ld' is not allowed"},
{"error csolution: processing context 'linker.Redefinition\\+RteTest_ARMCM3' failed"},
{"warning csolution: '.*/userLinkerScript.ld' this linker script is ignored; multiple linker scripts defined"}
  };

  auto errStr = streamRedirect.GetErrorString();
  for (const auto& expected : expectedVec) {
    EXPECT_TRUE(regex_search(errStr, regex(expected))) << "Missing Expected: " + expected;
  }
}

TEST_F(ProjMgrUnitTests, RunProjMgr_StandardLibrary) {
  char* argv[8];
  const string& csolution = testinput_folder + "/TestSolution/StandardLibrary/library.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-a";
  argv[4] = (char*)"";
  argv[5] = (char*)"-o";
  argv[6] = (char*)testoutput_folder.c_str();
  argv[7] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(8, argv, m_envp));

  // Check generated CPRJs
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/library.Debug+RteTest_ARMCM3.cprj",
    testinput_folder + "/TestSolution/StandardLibrary/ref/library.Debug+RteTest_ARMCM3.cprj");

  // Check generated cbuild YMLs
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/library.Debug+RteTest_ARMCM3.cbuild.yml",
    testinput_folder + "/TestSolution/StandardLibrary/ref/library.Debug+RteTest_ARMCM3.cbuild.yml");

  // Check there is no cbuild-run (target-set has only lib contexts)
  const YAML::Node& cbuildIdx = YAML::LoadFile(testoutput_folder + "/library.cbuild-idx.yml");
  EXPECT_FALSE(cbuildIdx["build-idx"]["cbuild-run"].IsDefined());
}

TEST_F(ProjMgrUnitTests, RunProjMgr_MultipleProject_SameFolder) {
  char* argv[6];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestSolution/multiple.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();
  argv[5] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(6, argv, 0));

  // Check warning message
  const string expected = ".*/TestSolution/multiple.csolution.yml - warning csolution: cproject.yml files should be placed in separate sub-directories";

  auto errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(regex_search(errStr, regex(expected)));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_MultipleProject_SameFilename) {
  char* argv[6];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestSolution/multiple2.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();
  argv[5] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(6, argv, 0));

  // Check error message
  const string expected = ".*/TestSolution/multiple2.csolution.yml - error csolution: cproject.yml filenames must be unique";

  auto errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(regex_search(errStr, regex(expected)));
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

  string localCompilerPath, defaultPackRoot;
  streamRedirect.ClearStringStreams();
  EXPECT_EQ(0, RunProjMgr(3, argv, 0));
  outStr = streamRedirect.GetOutString();
  localCompilerPath = string(PROJMGRUNITTESTS_BIN_PATH) + "/../etc";
  localCompilerPath = RteFsUtils::MakePathCanonical(localCompilerPath);
  defaultPackRoot = CrossPlatformUtils::GetDefaultCMSISPackRootDir();
  defaultPackRoot = RteFsUtils::MakePathCanonical(defaultPackRoot);
  EXPECT_EQ(defaultPackRoot, GetValue(outStr, "CMSIS_PACK_ROOT="));
  EXPECT_EQ(localCompilerPath, GetValue(outStr, "CMSIS_COMPILER_ROOT="));

  // Restore env variables
  CrossPlatformUtils::SetEnv("CMSIS_PACK_ROOT", pack_Root);
  CrossPlatformUtils::SetEnv("CMSIS_COMPILER_ROOT", compiler_Root);
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ContextMap) {
  char* argv[8];
  const string& csolution = testinput_folder + "/TestSolution/ContextMap/context-map.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-c";
  argv[4] = (char*)"*";
  argv[5] = (char*)"-o";
  argv[6] = (char*)testoutput_folder.c_str();
  argv[7] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(8, argv, m_envp));

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
  const string rteDir = testinput_folder + "/TestSolution/TestProject1/RTE/";
  const string configFile = rteDir + "Device/RteTest_ARMCM0/startup_ARMCM0.c";
  const string baseFile = configFile + ".base@2.0.1";
  const string& testdir = testoutput_folder + "/OutputDir";
  RteFsUtils::RemoveDir(rteDir);
  RteFsUtils::RemoveDir(testdir);
  RteFsUtils::CreateTextFile(configFile, "// config file");
  RteFsUtils::CreateTextFile(baseFile, "// config file@base");

  StdStreamRedirect streamRedirect;
  string csolutionFile = testinput_folder + "/TestSolution/test.csolution.yml";
  RemoveCbuildSetFile(csolutionFile);

  char* argv[11];
  argv[0] = (char*)"";
  argv[1] = (char*)"update-rte";
  argv[2] = (char*)csolutionFile.c_str();
  argv[3] = (char*)"-c";
  argv[4] = (char*)"test1.Debug+CM0";
  argv[5] = (char*)"-o";
  argv[6] = (char*)testdir.c_str();
  argv[7] = (char*)"-v";
  EXPECT_EQ(0, RunProjMgr(8, argv, m_envp));
  EXPECT_TRUE(RteFsUtils::Exists(rteDir + "/_Debug_CM0/RTE_Components.h"));

  // Check info message
  const string expected = "\
info csolution: config files for each component:\n\
  :\n\
    - .*/TestSolution/.cmsis/test\\+CM0.dbgconf \\(base@0.0.2\\)\n\
  ARM::Device:Startup&RteTest Startup@2.0.3:\n\
    - .*/TestSolution/TestProject1/RTE/Device/RteTest_ARMCM0/ARMCM0_ac6.sct \\(base@1.0.0\\)\n\
    - .*/TestSolution/TestProject1/RTE/Device/RteTest_ARMCM0/startup_ARMCM0.c \\(base@2.0.1\\) \\(update@2.0.3\\)\n\
    - .*/TestSolution/TestProject1/RTE/Device/RteTest_ARMCM0/system_ARMCM0.c \\(base@1.0.0\\)\n\
";

  auto outStr = streamRedirect.GetOutString();
  EXPECT_TRUE(regex_match(outStr, regex(expected))) <<
    "Expected regex: \n" << expected << "\nActual:\n" << outStr;

  streamRedirect.ClearStringStreams();
  argv[1] = (char*)"list";
  argv[2] = (char*)"configs";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-c";
  argv[5] = (char*)"test1.Debug+CM0";
  argv[6] = (char*)"-o";
  argv[7] = (char*)testoutput_folder.c_str();
  argv[8] = (char*)"-v";
  EXPECT_EQ(0, RunProjMgr(9, argv, m_envp));

  outStr = streamRedirect.GetOutString();
  const string expected1 =
 "../.cmsis/test+CM0.dbgconf@0.0.2 (up to date)\n"
 "../TestProject1/RTE/Device/RteTest_ARMCM0/ARMCM0_ac6.sct@1.0.0 (up to date) from ARM::Device:Startup&RteTest Startup@2.0.3\n"\
 "../TestProject1/RTE/Device/RteTest_ARMCM0/startup_ARMCM0.c@2.0.1 (update@2.0.3) from ARM::Device:Startup&RteTest Startup@2.0.3\n"\
 "../TestProject1/RTE/Device/RteTest_ARMCM0/system_ARMCM0.c@1.0.0 (up to date) from ARM::Device:Startup&RteTest Startup@2.0.3\n";
  EXPECT_EQ(outStr, expected1);

  streamRedirect.ClearStringStreams();
  argv[9] = (char*)"-f";
  argv[10] = (char*)"DBGCONF";
  EXPECT_EQ(0, RunProjMgr(11, argv, m_envp));
  outStr = streamRedirect.GetOutString();
  const string expected2 = "../.cmsis/test+CM0.dbgconf@0.0.2 (up to date)\n";
  EXPECT_EQ(outStr, expected2);
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListConfigsWithoutInput) {
  StdStreamRedirect streamRedirect;
  char* argv[3];
  argv[1] = (char*)"list";
  argv[2] = (char*)"configs";
  EXPECT_EQ(1, RunProjMgr(3, argv, m_envp));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find("input yml files were not specified"));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_No_target_Types) {
  StdStreamRedirect streamRedirect;
  char* argv[8];
  const string& csolutionFile = testinput_folder + "/TestSolution/missing_target_types.csolution.yml";

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"-n";
  argv[7] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(8, argv, 0));

  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find("target-types not found"));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_No_Projects) {
  StdStreamRedirect streamRedirect;
  char* argv[8];
  const string& csolutionFile = testinput_folder + "/TestSolution/missing_projects.csolution.yml";

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"-n";
  argv[7] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(8, argv, 0));

  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find("projects not found"));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_RteDir) {
  char* argv[6];
  const string& csolutionFile = testinput_folder + "/TestSolution/rtedir.csolution.yml";

  argv[1] = (char*)"convert";
  argv[2] = (char*)csolutionFile.c_str();
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();
  argv[5] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(6, argv, m_envp));

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
  char* argv[11];

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
  argv[10] = (char*)"--cbuildgen";

  EXPECT_EQ(1, RunProjMgr(11, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find("no matching context found for option:\n  --context test3*"));
}

TEST_F(ProjMgrUnitTests, RunProjMgrCovertMultipleContext) {
  StdStreamRedirect streamRedirect;
  char* argv[11];

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
  argv[10] = (char*)"--cbuildgen";

  EXPECT_EQ(0, RunProjMgr(11, argv, m_envp));
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
  const string& cproject1 = testinput_folder + "/TestSolution/FilenameCase/filename.cproject.yml";
  const string& cproject2 = testinput_folder + "/TestSolution/FilenameCase/Filename.cproject.yml";

  bool cprojectsExist = fs::exists(fs::path(cproject1)) && fs::exists(fs::path(cproject2));
  const string expectedErrMsg = cprojectsExist ?
    "warning csolution: 'filename.cproject.yml' has case inconsistency, use 'Filename.cproject.yml' instead" :
    "error csolution: cproject file was not found";

  char* argv[6];
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();
  argv[5] = (char*)"--cbuildgen";

  EXPECT_EQ(cprojectsExist ? 0 : 1, RunProjMgr(6, argv, m_envp));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expectedErrMsg)) << "errStr: " + errStr;
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Reverse_Context_Syntax) {
  StdStreamRedirect streamRedirect;
  char* argv[11];

  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestSolution/test.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"-c";
  argv[7] = (char*)"test2+CM0.Debug";
  argv[8] = (char*)"-c";
  argv[9] = (char*)"test1+CM0.Release";
  argv[10] = (char*)"--cbuildgen";

  EXPECT_EQ(0, RunProjMgr(11, argv, m_envp));
  auto outStr = streamRedirect.GetOutString();
  EXPECT_NE(string::npos, outStr.find("test2.Debug+CM0.cprj - info csolution: file generated successfully"));
  EXPECT_NE(string::npos, outStr.find("test1.Release+CM0.cprj - info csolution: file generated successfully"));
  EXPECT_EQ(string::npos, outStr.find("test1.Debug+CM0.cprj"));
  EXPECT_EQ(string::npos, outStr.find("test2.Debug+CM3.cprj"));
}

TEST_F(ProjMgrUnitTests, FileLanguageAndScope) {
  const string& csolution = testinput_folder + "/TestSolution/LanguageAndScope/lang-scope.csolution.yml";

  char* argv[6];
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();
  argv[5] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(6, argv, m_envp));

  // Check generated cbuild YMLs
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/lang-scope.Debug_AC6+RteTest_ARMCM3.cbuild.yml",
    testinput_folder + "/TestSolution/LanguageAndScope/ref/lang-scope.Debug_AC6+RteTest_ARMCM3.cbuild.yml");
}

TEST_F(ProjMgrUnitTests, EnsurePortability) {
  const string host = CrossPlatformUtils::GetHostType();
  const string cproject1 = testinput_folder + "/TestSolution/Portability/case/case.cproject.yml";
  const string cproject2 = testinput_folder + "/TestSolution/Portability/CASE/CASE.cproject.yml";
  bool cprojectsExist = fs::exists(fs::path(cproject1)) && fs::exists(fs::path(cproject2));

  // WSL is identified as 'linux' host but with case insensitive file system
  if (cprojectsExist && (host == "linux")) {
    GTEST_SKIP() << "Skip portability test in WSL";
  }

  StdStreamRedirect streamRedirect;
  char* argv[7];
  const string csolution = testinput_folder + "/TestSolution/Portability/portability.csolution.yml";
  const string csolution2 = testinput_folder + "/TestSolution/Portability/portability2.csolution.yml";
  argv[1] = (char*)"convert";
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();
  argv[5] = (char*)"-n";
  argv[6] = (char*)"--cbuildgen";

  const vector<string> expectedSeparator = {
{"portability.csolution.yml:20:13 - warning csolution: '..\\Portability' contains non-portable backslash, use forward slash instead"},
{"portability.csolution.yml:14:7 - warning csolution: '..\\Portability' contains non-portable backslash, use forward slash instead"},
{"portability.csolution.yml:26:13 - warning csolution: '..\\..\\SolutionSpecificPack' contains non-portable backslash, use forward slash instead"},
{"bs/bs.cproject.yml:10:16 - warning csolution: '..\\artifact.elf' contains non-portable backslash, use forward slash instead"},
{"bs/bs.cproject.yml:7:14 - warning csolution: '..\\layer.clayer.yml' contains non-portable backslash, use forward slash instead"},
{"bs/bs.cproject.yml:4:15 - warning csolution: '..\\linker_script.ld' contains non-portable backslash, use forward slash instead"},
{"bs/bs.cproject.yml:13:15 - warning csolution: '..\\..\\Portability' contains non-portable backslash, use forward slash instead"},
{"bs/bs.cproject.yml:16:15 - warning csolution: '..\\..\\Portability' contains non-portable backslash, use forward slash instead"},
{"portability2.csolution.yml:10:16 - warning csolution: '.\\bs\\bs.cproject.yml' contains non-portable backslash, use forward slash instead"},
  };

  const vector<string> expectedCase = {
{"portability.csolution.yml:19:13 - warning csolution: '../PortAbility' has case inconsistency, use '.' instead"},
{"portability.csolution.yml:13:7 - warning csolution: '../PortAbility' has case inconsistency, use '.' instead"},
{"portability.csolution.yml:24:13 - warning csolution: '../../solutionspecificpack' has case inconsistency, use '../../SolutionSpecificPack' instead"},
{"case/case.cproject.yml:10:16 - warning csolution: '../Artifact.elf' has case inconsistency, use '../artifact.elf' instead"},
{"case/case.cproject.yml:7:14 - warning csolution: '../laYer.clayer.yml' has case inconsistency, use '../layer.clayer.yml' instead"},
{"case/case.cproject.yml:4:15 - warning csolution: '../linker_Script.ld' has case inconsistency, use '../linker_script.ld' instead"},
{"case/case.cproject.yml:13:15 - warning csolution: '../../PortAbility' has case inconsistency, use '..' instead"},
{"case/case.cproject.yml:16:15 - warning csolution: '../../PortAbility' has case inconsistency, use '..' instead"},
{"portability2.csolution.yml:9:16 - warning csolution: './Case/caSe.cproject.yml' has case inconsistency, use 'case/case.cproject.yml' instead"},
  };

  const vector<string> expectedAbsPathWin = {
{"portability.csolution.yml:16:7 - warning csolution: absolute path 'C:/absolute/path/win' is not portable, use relative path instead"},
  };

  const vector<string> expectedAbsPathUnix = {
{"portability.csolution.yml:15:7 - warning csolution: absolute path '/absolute/path/unix' is not portable, use relative path instead"},
  };

  vector<string> expectedVec = expectedSeparator;
  if (host == "linux") {
    expectedVec.insert(expectedVec.end(), expectedAbsPathUnix.begin(), expectedAbsPathUnix.end());
  } else if (host == "win") {
    expectedVec.insert(expectedVec.end(), expectedCase.begin(), expectedCase.end());
    expectedVec.insert(expectedVec.end(), expectedAbsPathWin.begin(), expectedAbsPathWin.end());
  } else if (host == "darwin") {
    expectedVec.insert(expectedVec.end(), expectedCase.begin(), expectedCase.end());
    expectedVec.insert(expectedVec.end(), expectedAbsPathUnix.begin(), expectedAbsPathUnix.end());
  }
  argv[2] = (char*)csolution.c_str();
  EXPECT_EQ(host != "win" ? 1 : 0, RunProjMgr(7, argv, m_envp));
  argv[2] = (char*)csolution2.c_str();
  EXPECT_EQ(host != "win" ? 1 : 0, RunProjMgr(7, argv, m_envp));

  string errStr = streamRedirect.GetErrorString();
  for (const auto& expected : expectedVec) {
    EXPECT_TRUE(errStr.find(expected) != string::npos) << " Missing Expected: " + expected;
  }
}

TEST_F(ProjMgrUnitTests, RunProjMgr_NonUniqueMapKeys) {
  StdStreamRedirect streamRedirect;
  char* argv[7];
  const string& csolutionFile = testinput_folder + "/TestSolution/non-unique-map-keys.csolution.yml";

  // verify schema check
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolutionFile.c_str();
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();
  argv[5] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(6, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find("error csolution: schema check failed, verify syntax"));

  // run with schema check disabled
  streamRedirect.ClearStringStreams();
  argv[6] = (char*)"-n";
  EXPECT_EQ(1, RunProjMgr(7, argv, 0));
  errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find("error csolution: map keys must be unique"));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_MissingFilters) {
  char* argv[7];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestSolution/typefilters.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();
  argv[5] = (char*)"-n";
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

  const string expected = "\
warning csolution: build-type '.MappedTarget' does not exist in solution, did you mean '+MappedTarget'?\n\
warning csolution: build-type '.Target' does not exist in solution, did you mean '+Target'?\n\
warning csolution: build-type '.UnknownBuild' does not exist in solution\n\
warning csolution: target-type '+Debug' does not exist in solution, did you mean '.Debug'?\n\
warning csolution: target-type '+MappedDebug' does not exist in solution, did you mean '.MappedDebug'?\n\
warning csolution: target-type '+UnknownTarget' does not exist in solution\n\
warning csolution: compiler 'Ac6' is not supported\n\
";
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(errStr.find(expected) != string::npos);
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_cbuildset_file) {
  char* argv[14];

  const string& csolutionDir  = testinput_folder + "/TestSolution";
  const string& csolution     = csolutionDir + "/test.csolution.yml";
  const string& outputDir     = testoutput_folder + "/cbuildset";
  const string& cbuildSetFile = outputDir + "/test.cbuild-set.yml";

  auto CleanUp = [&]() {
    // Clean residual (if any)
    if (RteFsUtils::Exists(outputDir)) {
      RteFsUtils::RemoveDir(outputDir);
    }
    if (RteFsUtils::Exists(cbuildSetFile)) {
      RteFsUtils::RemoveFile(cbuildSetFile);
    }
  };

  {
    CleanUp();
    // Test 1: Run without contexts specified (no existing cbuild-set file without -S)
    // Expectations: All available contexts should be processed and cbuild-set
    //               file not generated.
    argv[1] = (char*)"convert";
    argv[2] = (char*)"--solution";
    argv[3] = (char*)csolution.c_str();
    argv[4] = (char*)"-o";
    argv[5] = (char*)outputDir.c_str();
    argv[6] = (char*)"--cbuildgen";

    EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));
    EXPECT_FALSE(RteFsUtils::Exists(cbuildSetFile));
    EXPECT_TRUE(RteFsUtils::Exists(outputDir + "/test2.Debug+CM0.cbuild.yml"));
    EXPECT_TRUE(RteFsUtils::Exists(outputDir + "/test1.Debug+CM0.cbuild.yml"));
    EXPECT_TRUE(RteFsUtils::Exists(outputDir + "/test2.Debug+CM3.cbuild.yml"));
    EXPECT_TRUE(RteFsUtils::Exists(outputDir + "/test1.Release+CM0.cbuild.yml"));
  }

  {
    CleanUp();
    // Test 2: Run without contexts specified (no existing cbuild-set file with -S)
    // Expectation: *.cbuild-set.yml file should be generated and only first build and
    //              target types should be selected with first project
    argv[1] = (char*)"convert";
    argv[2] = (char*)"--solution";
    argv[3] = (char*)csolution.c_str();
    argv[4] = (char*)"-o";
    argv[5] = (char*)outputDir.c_str();
    argv[6] = (char*)"-S";
    argv[7] = (char*)"--cbuildgen";

    EXPECT_EQ(0, RunProjMgr(8, argv, m_envp));
    EXPECT_TRUE(RteFsUtils::Exists(cbuildSetFile));
    ProjMgrTestEnv::CompareFile(cbuildSetFile, testinput_folder + "/TestSolution/ref/cbuild/first_build_target.cbuild.set.yml");
    EXPECT_TRUE(RteFsUtils::Exists(outputDir + "/test2.Debug+CM0.cbuild.yml"));
    EXPECT_FALSE(RteFsUtils::Exists(outputDir + "/test1.Debug+CM0.cbuild.yml"));
    EXPECT_FALSE(RteFsUtils::Exists(outputDir + "/test2.Debug+CM3.cbuild.yml"));
    EXPECT_FALSE(RteFsUtils::Exists(outputDir + "/test1.Release+CM0.cbuild.yml"));
  }

  {
    // Test 3: Run with only specified contexts and no cbuild-set file but with -S
    // Expectation: *.cbuild-set.yml file should be generated and only
    //               specified contexts should be processed
    CleanUp();
    argv[1] = (char*)"convert";
    argv[2] = (char*)"--solution";
    argv[3] = (char*)csolution.c_str();
    argv[4] = (char*)"-c";
    argv[5] = (char*)"test2.Debug+CM0";
    argv[6] = (char*)"-c";
    argv[7] = (char*)"test1.Debug+CM0";
    argv[8] = (char*)"-o";
    argv[9] = (char*)outputDir.c_str();
    argv[10] = (char*)"-t";
    argv[11] = (char*)"GCC";
    argv[12] = (char*)"-S";
    argv[13] = (char*)"--cbuildgen";

    EXPECT_EQ(0, RunProjMgr(14, argv, m_envp));
    EXPECT_TRUE(RteFsUtils::Exists(cbuildSetFile));
    ProjMgrTestEnv::CompareFile(cbuildSetFile, testinput_folder + "/TestSolution/ref/cbuild/specific_contexts_test.cbuild-set.yml");
    EXPECT_TRUE(RteFsUtils::Exists(outputDir + "/test2.Debug+CM0.cbuild.yml"));
    EXPECT_TRUE(RteFsUtils::Exists(outputDir + "/test1.Debug+CM0.cbuild.yml"));
  }

  {
    // Test 4: Run with contexts specified (with existing cbuild-set file)
    // Expectations: All specified contexts should be processed and cbuild-set
    //               file remains unchanged.
    StdStreamRedirect streamRedirect;
    argv[1] = (char*)"convert";
    argv[2] = (char*)"--solution";
    argv[3] = (char*)csolution.c_str();
    argv[4] = (char*)"-o";
    argv[5] = (char*)outputDir.c_str();
    argv[6] = (char*)"-c";
    argv[7] = (char*)"test1.Release+CM0";
    argv[8] = (char*)"--cbuildgen";

    EXPECT_EQ(0, RunProjMgr(9, argv, m_envp));
    auto outStr = streamRedirect.GetOutString();
    EXPECT_NE(outStr.find("test1.Release+CM0.cprj - info csolution: file generated successfully"), string::npos);
    EXPECT_NE(outStr.find("test1.Release+CM0.cbuild.yml - info csolution: file generated successfully"), string::npos);
    EXPECT_TRUE(RteFsUtils::Exists(cbuildSetFile));
    ProjMgrTestEnv::CompareFile(cbuildSetFile, testinput_folder + "/TestSolution/ref/cbuild/specific_contexts_test.cbuild-set.yml");
  }

  {
    // Test 5: Run without contexts specified (with existing cbuild-set file with -S)
    // Expectations: Contexts from cbuild-set should be processed
    argv[1] = (char*)"convert";
    argv[2] = (char*)"--solution";
    argv[3] = (char*)csolution.c_str();
    argv[4] = (char*)"-o";
    argv[5] = (char*)outputDir.c_str();
    argv[6] = (char*)"-S";
    argv[7] = (char*)"--cbuildgen";

    EXPECT_EQ(0, RunProjMgr(8, argv, m_envp));
    EXPECT_TRUE(RteFsUtils::Exists(cbuildSetFile));
    ProjMgrTestEnv::CompareFile(cbuildSetFile, testinput_folder + "/TestSolution/ref/cbuild/specific_contexts_test.cbuild-set.yml");
  }

  {
    // Test 6: Run with no contexts but toolchain AC6 specified (with existing cbuild-set file with -S)
    // Expectations: Contexts from cbuild-set should be processed with selected toolchain
    argv[1] = (char*)"convert";
    argv[2] = (char*)"--solution";
    argv[3] = (char*)csolution.c_str();
    argv[4] = (char*)"-o";
    argv[5] = (char*)outputDir.c_str();
    argv[6] = (char*)"-S";
    argv[7] = (char*)"-t";
    argv[8] = (char*)"AC6";
    argv[9] = (char*)"--cbuildgen";

    EXPECT_EQ(0, RunProjMgr(10, argv, m_envp));
    EXPECT_TRUE(RteFsUtils::Exists(cbuildSetFile));
    ProjMgrTestEnv::CompareFile(cbuildSetFile, testinput_folder + "/TestSolution/ref/cbuild/specific_contexts_test_AC6.cbuild-set.yml");
  }

  {
    // Test 7: Run with no contexts but toolchain GCC specified (with existing cbuild-set file with -S)
    // Expectations: Contexts from cbuild-set should be processed with selected toolchain
    argv[1] = (char*)"convert";
    argv[2] = (char*)"--solution";
    argv[3] = (char*)csolution.c_str();
    argv[4] = (char*)"-o";
    argv[5] = (char*)outputDir.c_str();
    argv[6] = (char*)"-S";
    argv[7] = (char*)"-t";
    argv[8] = (char*)"GCC";
    argv[9] = (char*)"--cbuildgen";

    EXPECT_EQ(0, RunProjMgr(10, argv, m_envp));
    EXPECT_TRUE(RteFsUtils::Exists(cbuildSetFile));
    ProjMgrTestEnv::CompareFile(cbuildSetFile, testinput_folder + "/TestSolution/ref/cbuild/specific_contexts_test.cbuild-set.yml");
  }

  {
    // Test 8: Run with no contexts no toolchain specified (with existing cbuild-set file with -S)
    // Expectations: Contexts from cbuild-set should be processed with toolchain specified in cbuild-set
    argv[1] = (char*)"convert";
    argv[2] = (char*)"--solution";
    argv[3] = (char*)csolution.c_str();
    argv[4] = (char*)"-o";
    argv[5] = (char*)outputDir.c_str();
    argv[6] = (char*)"-S";
    argv[7] = (char*)"--cbuildgen";

    EXPECT_EQ(0, RunProjMgr(8, argv, m_envp));
    EXPECT_TRUE(RteFsUtils::Exists(cbuildSetFile));
    ProjMgrTestEnv::CompareFile(cbuildSetFile, testinput_folder + "/TestSolution/ref/cbuild/specific_contexts_test.cbuild-set.yml");
  }

  {
    // Test 9: Run with invalid conversion with -S
    // Expectations: Conversion should fail and cbuild-set file should not be generated
    const string& csolutionFile = testinput_folder + "/TestSolution/novalid_context.csolution.yml";
    argv[1] = (char*)"convert";
    argv[2] = (char*)csolutionFile.c_str();
    argv[3] = (char*)"-o";
    argv[4] = (char*)outputDir.c_str();
    argv[5] = (char*)"-S";
    argv[6] = (char*)"--cbuildgen";

    EXPECT_EQ(1, RunProjMgr(7, argv, m_envp));
    EXPECT_FALSE(RteFsUtils::Exists(outputDir + "/novalid_context.cbuild-set.yml"));
  }
}

TEST_F(ProjMgrUnitTests, ExternalGenerator) {

  const string& srcGlobalGenerator = testinput_folder + "/ExternalGenerator/global.generator.yml";
  const string& dstGlobalGenerator = etc_folder + "/global.generator.yml";
  RteFsUtils::CopyCheckFile(srcGlobalGenerator, dstGlobalGenerator, false);

  const string& srcBridgeTool = testinput_folder + "/ExternalGenerator/bridge tool.sh";
  const string& dstBridgeTool = bin_folder + "/bridge tool.sh";
  RteFsUtils::CopyCheckFile(srcBridgeTool, dstBridgeTool, false);

  // list generators
  char* argv[7];
  const string& csolution = testinput_folder + "/ExternalGenerator/extgen.csolution.yml";
  argv[1] = (char*)csolution.c_str();
  argv[2] = (char*)"list";
  argv[3] = (char*)"generators";
  EXPECT_EQ(0, RunProjMgr(4, argv, m_envp));

  // run multi-core
  argv[1] = (char*)csolution.c_str();
  argv[2] = (char*)"run";
  argv[3] = (char*)"-g";
  argv[4] = (char*)"RteTestExternalGenerator";
  argv[5] = (char*)"-c";
  argv[6] = (char*)"core0.Debug+MultiCore";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

  ProjMgrTestEnv::CompareFile(testinput_folder + "/ExternalGenerator/ref/MultiCore/extgen.cbuild-gen-idx.yml",
    testinput_folder + "/ExternalGenerator/tmp/extgen.cbuild-gen-idx.yml", ProjMgrTestEnv::StripAbsoluteFunc);
  ProjMgrTestEnv::CompareFile(testinput_folder + "/ExternalGenerator/ref/MultiCore/core0.Debug+MultiCore.cbuild-gen.yml",
    testinput_folder + "/ExternalGenerator/tmp/core0.Debug+MultiCore.cbuild-gen.yml", ProjMgrTestEnv::StripAbsoluteFunc);
  ProjMgrTestEnv::CompareFile(testinput_folder + "/ExternalGenerator/ref/MultiCore/core1.Debug+MultiCore.cbuild-gen.yml",
    testinput_folder + "/ExternalGenerator/tmp/core1.Debug+MultiCore.cbuild-gen.yml", ProjMgrTestEnv::StripAbsoluteFunc);

  // run single-core
  argv[6] = (char*)"single-core.Debug+CM0";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

  ProjMgrTestEnv::CompareFile(testinput_folder + "/ExternalGenerator/ref/SingleCore/extgen.cbuild-gen-idx.yml",
    testinput_folder + "/ExternalGenerator/tmp/extgen.cbuild-gen-idx.yml", ProjMgrTestEnv::StripAbsoluteFunc);
  ProjMgrTestEnv::CompareFile(testinput_folder + "/ExternalGenerator/ref/SingleCore/single-core.Debug+CM0.cbuild-gen.yml",
    testinput_folder + "/ExternalGenerator/tmp/single-core.Debug+CM0.cbuild-gen.yml", ProjMgrTestEnv::StripAbsoluteFunc);

  // run trustzone
  argv[6] = (char*)"ns.Debug+CM0";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

  ProjMgrTestEnv::CompareFile(testinput_folder + "/ExternalGenerator/ref/TrustZone/extgen.cbuild-gen-idx.yml",
    testinput_folder + "/ExternalGenerator/tmp/extgen.cbuild-gen-idx.yml", ProjMgrTestEnv::StripAbsoluteFunc);
  ProjMgrTestEnv::CompareFile(testinput_folder + "/ExternalGenerator/ref/TrustZone/ns.Debug+CM0.cbuild-gen.yml",
    testinput_folder + "/ExternalGenerator/tmp/ns.Debug+CM0.cbuild-gen.yml", ProjMgrTestEnv::StripAbsoluteFunc);
  ProjMgrTestEnv::CompareFile(testinput_folder + "/ExternalGenerator/ref/TrustZone/s.Debug+CM0.cbuild-gen.yml",
    testinput_folder + "/ExternalGenerator/tmp/s.Debug+CM0.cbuild-gen.yml", ProjMgrTestEnv::StripAbsoluteFunc);

  // convert single core
  argv[2] = (char*)"convert";
  argv[3] = (char*)"-c";
  argv[4] = (char*)"single-core.Debug+CM0";
  EXPECT_EQ(0, RunProjMgr(5, argv, m_envp));

  ProjMgrTestEnv::CompareFile(testinput_folder + "/ExternalGenerator/out/single-core/CM0/Debug/single-core.Debug+CM0.cbuild.yml",
    testinput_folder + "/ExternalGenerator/ref/SingleCore/single-core.Debug+CM0.cbuild.yml");

  RteFsUtils::RemoveFile(dstGlobalGenerator);
  RteFsUtils::RemoveFile(dstBridgeTool);
}

TEST_F(ProjMgrUnitTests, ExternalGenerator_NotRegistered) {
  StdStreamRedirect streamRedirect;
  char* argv[7];
  const string& csolution = testinput_folder + "/ExternalGenerator/extgen.csolution.yml";
  argv[1] = (char*)csolution.c_str();
  argv[2] = (char*)"run";
  argv[3] = (char*)"-g";
  argv[4] = (char*)"RteTestExternalGenerator";
  argv[5] = (char*)"-c";
  argv[6] = (char*)"single-core.Debug+CM0";
  EXPECT_EQ(1, RunProjMgr(7, argv, 0));

  const string expected = "\
error csolution: generator 'RteTestExternalGenerator' required by component 'ARM::RteTestGenerator:Check Global Generator@0.9.0' was not found in global register\n\
";
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(errStr.find(expected) != string::npos);
}

TEST_F(ProjMgrUnitTests, ExternalGenerator_WrongGenDir) {
  const string& srcGlobalGenerator = testinput_folder + "/ExternalGenerator/wrong-gendir.generator.yml";
  const string& dstGlobalGenerator = etc_folder + "/global.generator.yml";
  RteFsUtils::CopyCheckFile(srcGlobalGenerator, dstGlobalGenerator, false);

  const string& srcBridgeTool = testinput_folder + "/ExternalGenerator/bridge tool.sh";
  const string& dstBridgeTool = bin_folder + "/bridge tool.sh";
  RteFsUtils::CopyCheckFile(srcBridgeTool, dstBridgeTool, false);

  StdStreamRedirect streamRedirect;
  char* argv[8];
  const string& csolution = testinput_folder + "/ExternalGenerator/extgen.csolution.yml";
  argv[1] = (char*)csolution.c_str();
  argv[2] = (char*)"run";
  argv[3] = (char*)"-g";
  argv[4] = (char*)"RteTestExternalGenerator";
  argv[5] = (char*)"-c";
  argv[6] = (char*)"core0.Debug+MultiCore";
  argv[7] = (char*)"-n";
  EXPECT_EQ(1, RunProjMgr(8, argv, m_envp));

  const string expected = "\
error csolution: unknown access sequence: 'UnknownAccessSequence()'\n\
";
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(errStr.find(expected) != string::npos);

  RteFsUtils::RemoveFile(dstGlobalGenerator);
  RteFsUtils::RemoveFile(dstBridgeTool);
}

TEST_F(ProjMgrUnitTests, ExternalGenerator_NoGenDir) {
  const string& srcGlobalGenerator = testinput_folder + "/ExternalGenerator/no-gendir.generator.yml";
  const string& dstGlobalGenerator = etc_folder + "/global.generator.yml";
  RteFsUtils::CopyCheckFile(srcGlobalGenerator, dstGlobalGenerator, false);

  StdStreamRedirect streamRedirect;
  const string expected = "\
error csolution: generator output directory was not set\n\
";
  char* argv[8];
  const string& csolution = testinput_folder + "/ExternalGenerator/extgen.csolution.yml";
  argv[1] = (char*)csolution.c_str();
  argv[2] = (char*)"-c";
  argv[3] = (char*)"core0.Debug+MultiCore";
  argv[4] = (char*)"-n";
  argv[5] = (char*)"run";
  argv[6] = (char*)"-g";
  argv[7] = (char*)"RteTestExternalGenerator";
  EXPECT_EQ(1, RunProjMgr(8, argv, 0));

  auto errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(errStr.find(expected) != string::npos);

  RteFsUtils::RemoveFile(dstGlobalGenerator);
}

TEST_F(ProjMgrUnitTests, ExternalGenerator_MultipleContexts) {
  const string& srcGlobalGenerator = testinput_folder + "/ExternalGenerator/global.generator.yml";
  const string& dstGlobalGenerator = etc_folder + "/global.generator.yml";
  RteFsUtils::CopyCheckFile(srcGlobalGenerator, dstGlobalGenerator, false);

  const string& srcBridgeTool = testinput_folder + "/ExternalGenerator/bridge tool.sh";
  const string& dstBridgeTool = bin_folder + "/bridge tool.sh";
  RteFsUtils::CopyCheckFile(srcBridgeTool, dstBridgeTool, false);

  StdStreamRedirect streamRedirect;
  char* argv[7];
  const string& csolution = testinput_folder + "/ExternalGenerator/extgen.csolution.yml";
  argv[1] = (char*)csolution.c_str();
  argv[2] = (char*)"run";
  argv[3] = (char*)"-g";
  argv[4] = (char*)"RteTestExternalGenerator";
  argv[5] = (char*)"-c";

  // multiple context selection is accepted when all selected contexts are siblings (same generator-id and gendir)
  argv[6] = (char*)"+MultiCore";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

  // multiple context selection is rejected when the selected contexts are unrelated (require distinct generator run calls)
  argv[6] = (char*)"+CM0";
  EXPECT_EQ(1, RunProjMgr(7, argv, m_envp));
  const string expected = "\
one or more selected contexts are unrelated, redefine the '--context arg [...]' option\n\
";
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(errStr.find(expected) != string::npos);

  // use cbuild-set with siblings selection
  argv[5] = (char*)"--context-set";
  EXPECT_EQ(0, RunProjMgr(6, argv, m_envp));

  RteFsUtils::RemoveFile(dstGlobalGenerator);
  RteFsUtils::RemoveFile(dstBridgeTool);
}

TEST_F(ProjMgrUnitTests, ExternalGenerator_WrongGeneratedData) {
  const string& srcGlobalGenerator = testinput_folder + "/ExternalGenerator/global.generator.yml";
  const string& dstGlobalGenerator = etc_folder + "/global.generator.yml";
  RteFsUtils::CopyCheckFile(srcGlobalGenerator, dstGlobalGenerator, false);

  StdStreamRedirect streamRedirect;
  char* argv[5];
  const string& csolution = testinput_folder + "/ExternalGenerator/wrong.csolution.yml";
  argv[1] = (char*)csolution.c_str();
  argv[2] = (char*)"convert";
  argv[3] = (char*)"-c";
  argv[4] = (char*)"wrong.WrongPack+CM0";
  EXPECT_EQ(1, RunProjMgr(5, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(errStr.find("error csolution: required pack: UnknownVendor::UnknownPack not installed") != string::npos);

  streamRedirect.ClearStringStreams();
  argv[4] = (char*)"wrong.WrongComponent+CM0";
  EXPECT_EQ(1, RunProjMgr(5, argv, 0));
  errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(errStr.find("error csolution: component 'UnknownVendor:UnknownComponent' not found in included packs") != string::npos);

  streamRedirect.ClearStringStreams();
  argv[4] = (char*)"wrong.WrongGroup+CM0";
  EXPECT_EQ(1, RunProjMgr(5, argv, 0));
  errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(errStr.find("error csolution: conflict: group 'sources' is declared multiple times") != string::npos);

  streamRedirect.ClearStringStreams();
  argv[4] = (char*)"wrong.Debug+WrongDevice";
  EXPECT_EQ(1, RunProjMgr(5, argv, 0));
  errStr = streamRedirect.GetErrorString();

  RteFsUtils::RemoveFile(dstGlobalGenerator);
}

TEST_F(ProjMgrUnitTests, ExternalGenerator_NoCgenFile) {
  const string& srcGlobalGenerator = testinput_folder + "/ExternalGenerator/global.generator.yml";
  const string& dstGlobalGenerator = etc_folder + "/global.generator.yml";
  RteFsUtils::CopyCheckFile(srcGlobalGenerator, dstGlobalGenerator, false);

  const string genDir = m_extGenerator.GetGlobalGenDir("RteTestExternalGenerator");
  if (!genDir.empty()) {
    RteFsUtils::RemoveDir(genDir);
  }

  StdStreamRedirect streamRedirect;
  char* argv[5];
  const string& csolution = testinput_folder + "/ExternalGenerator/extgen.csolution.yml";
  argv[1] = (char*)csolution.c_str();
  argv[2] = (char*)"convert";
  argv[3] = (char*)"-c";
  argv[4] = (char*)"core0.Debug+MultiCore";
  EXPECT_EQ(1, RunProjMgr(5, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(errStr.find("error csolution: cgen file was not found, run generator 'RteTestExternalGenerator' for context 'core0.Debug+MultiCore'") != string::npos);

  RteFsUtils::RemoveFile(dstGlobalGenerator);
}

TEST_F(ProjMgrUnitTests, ExternalGeneratorBoard) {
  const string& srcGlobalGenerator = testinput_folder + "/ExternalGenerator/global.generator.yml";
  const string& dstGlobalGenerator = etc_folder + "/global.generator.yml";
  RteFsUtils::CopyCheckFile(srcGlobalGenerator, dstGlobalGenerator, false);

  char* argv[3];
  const string& csolution = testinput_folder + "/ExternalGenerator/board.csolution.yml";
  argv[1] = (char*)csolution.c_str();
  argv[2] = (char*)"convert";
  EXPECT_EQ(0, RunProjMgr(3, argv, m_envp));

  ProjMgrTestEnv::CompareFile(testinput_folder + "/ExternalGenerator/out/single-core/Board/Debug/single-core.Debug+Board.cbuild.yml",
    testinput_folder + "/ExternalGenerator/ref/SingleCore/single-core.Debug+Board.cbuild.yml");

  RteFsUtils::RemoveFile(dstGlobalGenerator);
}

TEST_F(ProjMgrUnitTests, ExternalGeneratorListVerbose) {
  const string& srcGlobalGenerator = testinput_folder + "/ExternalGenerator/global.generator.yml";
  const string& dstGlobalGenerator = etc_folder + "/global.generator.yml";
  RteFsUtils::CopyCheckFile(srcGlobalGenerator, dstGlobalGenerator, false);
  StdStreamRedirect streamRedirect;

  char* argv[5];
  const string& csolution = testinput_folder + "/ExternalGenerator/extgen.csolution.yml";
  argv[1] = (char*)csolution.c_str();
  argv[2] = (char*)"list";
  argv[3] = (char*)"generators";
  argv[4] = (char*)"-v";
  EXPECT_EQ(0, RunProjMgr(5, argv, m_envp));

  const string expected = "\
RteTestExternalGenerator (Global Registered Generator)\n\
  base-dir: generated/CM0\n\
    cgen-file: generated/CM0/ns.cgen.yml\n\
      context: ns.Debug+CM0\n\
      context: ns.Release+CM0\n\
    cgen-file: generated/CM0/s.cgen.yml\n\
      context: s.Debug+CM0\n\
      context: s.Release+CM0\n\
  base-dir: generated/MultiCore\n\
    cgen-file: generated/MultiCore/MyConf.cgen.yml\n\
      context: boot.Debug+MultiCore\n\
      context: boot.Release+MultiCore\n\
    cgen-file: generated/MultiCore/core0.cgen.yml\n\
      context: core0.Debug+MultiCore\n\
      context: core0.Release+MultiCore\n\
    cgen-file: generated/MultiCore/core1.cgen.yml\n\
      context: core1.Debug+MultiCore\n\
      context: core1.Release+MultiCore\n\
  base-dir: single/generated\n\
    cgen-file: single/generated/single-core.cgen.yml\n\
      context: single-core.Debug+CM0\n\
      context: single-core.Release+CM0\n\
";
  auto outStr = streamRedirect.GetOutString();
  EXPECT_TRUE(outStr.find(expected) != string::npos);

  RteFsUtils::RemoveFile(dstGlobalGenerator);
}

TEST_F(ProjMgrUnitTests, ClassicGeneratorListVerbose) {
  StdStreamRedirect streamRedirect;

  char* argv[5];
  const string& csolution = testinput_folder + "/TestGenerator/test-gpdsc-multiple-generators.csolution.yml";
  argv[1] = (char*)"list";
  argv[2] = (char*)"generators";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-v";
  EXPECT_EQ(0, RunProjMgr(5, argv, m_envp));

  const string expected = "\
RteTestGeneratorIdentifier (RteTest Generator Description)\n\
  base-dir: GeneratedFiles/RteTestGeneratorIdentifier\n\
    cgen-file: GeneratedFiles/RteTestGeneratorIdentifier/RteTestGen_ARMCM0/RteTest.gpdsc\n\
      context: test-gpdsc-multiple-generators.Debug+CM0\n\
RteTestGeneratorWithKey (RteTest Generator with Key Description)\n\
  base-dir: GeneratedFiles/RteTestGeneratorWithKey\n\
    cgen-file: GeneratedFiles/RteTestGeneratorWithKey/RteTestGen_ARMCM0/RteTest.gpdsc\n\
      context: test-gpdsc-multiple-generators.Debug+CM0\n\
";
  auto outStr = streamRedirect.GetOutString();
  EXPECT_TRUE(outStr.find(expected) != string::npos);
}

TEST_F(ProjMgrUnitTests, DeviceAttributes) {
  const map<string, vector<string>> projects = {
    {"fpu", {"+fpu-dp","+fpu-sp", "+no-fpu"}},
    {"dsp", {"+dsp", "+no-dsp"}},
    {"mve", {"+mve-fp", "+mve-int", "+no-mve"}},
    {"endian", {"+big", "+little"}},
    {"trustzone", {"+secure", "+secure-only", "+non-secure", "+tz-disabled"}},
    {"branch-protection", {"+bti","+bti-signret", "+no-bp"}}
  };
  char* argv[8];
  const string& csolution = testinput_folder + "/TestSolution/DeviceAttributes/solution.csolution.yml";
  argv[1] = (char*)csolution.c_str();
  argv[2] = (char*)"convert";
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();
  argv[5] = (char*)"--cbuildgen";
  argv[6] = (char*)"-c";

  // Test user selectable device attributes
  for (const auto& [project, targetTypes] : projects) {
    const string& context = project + ".Debug";
    argv[7] = (char*)context.c_str();
    EXPECT_EQ(0, RunProjMgr(8, argv, m_envp));

    // Check generated files
    for (const auto& targetType : targetTypes) {
      ProjMgrTestEnv::CompareFile(testoutput_folder + "/" + project + ".Debug" + targetType + ".cbuild.yml",
        testinput_folder + "/TestSolution/DeviceAttributes/ref/" + project + ".Debug" + targetType + ".cbuild.yml");
      ProjMgrTestEnv::CompareFile(testoutput_folder + "/" + project + ".Debug" + targetType + ".cprj",
        testinput_folder + "/TestSolution/DeviceAttributes/ref/" + project + ".Debug" + targetType + ".cprj");
    }
  }

  // Test attribute redefinition
  for (const auto& [project, targetTypes] : projects) {
    StdStreamRedirect streamRedirect;
    streamRedirect.ClearStringStreams();
    const string& context = project + ".Fail";
    argv[7] = (char*)context.c_str();
    EXPECT_EQ(1, RunProjMgr(8, argv, 0));

    auto errStr = streamRedirect.GetErrorString();
    EXPECT_TRUE(regex_search(errStr, regex("error csolution: redefinition from .* into .* is not allowed")));
  }
}

TEST_F(ProjMgrUnitTests, RunProjMgr_GpdscWithoutComponents) {
  char* argv[7];
  const string& csolution = testinput_folder + "/TestGenerator/test-gpdsc-without-components.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_GpdscWithProjectFiles) {
  char* argv[7];
  const string& csolution = testinput_folder + "/TestGenerator/test-gpdsc-project-files.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

  // Check generated cbuild YML
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/test-gpdsc-project-files.Debug+CM0.cbuild.yml",
    testinput_folder + "/TestGenerator/ref/test-gpdsc-project-files.Debug+CM0.cbuild.yml");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ValidateContextSpecificPacksMissing) {
  char* argv[5];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestSolution/pack_missing.csolution.yml";
  string expectedErr1 = "error csolution: required pack: ARM::Missing_DFP@0.0.9 not installed";
  string expectedErr2 = "error csolution: required pack: ARM::Missing_PACK@0.0.1 not installed";

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(5, argv, 0));
  string errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expectedErr1));
  EXPECT_NE(string::npos, errStr.find(expectedErr2));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_cbuild_files_with_errors_node) {
  char* argv[8];
  StdStreamRedirect streamRedirect;
  string expectedErr = "error csolution: processor name 'cm0_core0' was not found";
  const string& csolution = testinput_folder + "/TestSolution/test_no_device_name.csolution.yml";
  RteFsUtils::RemoveFile(testinput_folder + "/TestSolution/test_no_device_name.cbuild-pack.yml");
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();
  argv[5] = (char*)"-c";
  argv[6] = (char*)"test1.Debug+CM0";
  argv[7] = (char*)"--cbuildgen";

  EXPECT_EQ(1, RunProjMgr(8, argv, m_envp));
  EXPECT_NE(string::npos, streamRedirect.GetErrorString().find(expectedErr));
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/test_no_device_name.cbuild-idx.yml",
    testinput_folder + "/TestSolution/TestProject1/ref/test_no_device_name.cbuild-idx.yml");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_cbuild_files_with_packs_missing) {
  char* argv[6];
  StdStreamRedirect streamRedirect;
  string expectedErr1 = "error csolution: required pack: ARM::Missing_DFP@0.0.9 not installed";
  string expectedErr2 = "error csolution: required pack: ARM::Missing_PACK@0.0.1 not installed";
  const string& csolution = testinput_folder + "/TestSolution/PackMissing/missing_pack.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();
  argv[5] = (char*)"--cbuildgen";

  EXPECT_EQ(1, RunProjMgr(6, argv, m_envp));
  auto err = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, streamRedirect.GetErrorString().find(expectedErr1));
  EXPECT_NE(string::npos, streamRedirect.GetErrorString().find(expectedErr2));
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/project+CM0.cbuild.yml",
    testinput_folder + "/TestSolution/PackMissing/ref/project+CM0.cbuild.yml");
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/project+Gen.cbuild.yml",
    testinput_folder + "/TestSolution/PackMissing/ref/project+Gen.cbuild.yml");
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/missing_pack.cbuild-idx.yml",
    testinput_folder + "/TestSolution/PackMissing/ref/missing_pack.cbuild-idx.yml");
  EXPECT_TRUE(ProjMgrYamlSchemaChecker().Validate(testoutput_folder + "/missing_pack.cbuild-idx.yml"));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_cbuild_files_with_packs_missing_specific_context) {
  char* argv[7];
  StdStreamRedirect streamRedirect;
  string expectedErr1 = "error csolution: required pack: ARM::Missing_DFP@0.0.9 not installed";
  string expectedErr2 = "error csolution: required pack: ARM::Missing_PACK@0.0.1 not installed";
  const string& csolution = testinput_folder + "/TestSolution/PackMissing/missing_pack.csolution.yml";
  const string& cbuildidx = testoutput_folder + "/missing_pack.cbuild-idx.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();
  argv[5] = (char*)"-c";
  argv[6] = (char*)"project+CM0";
  RteFsUtils::RemoveFile(cbuildidx);
  EXPECT_EQ(1, RunProjMgr(7, argv, m_envp));
  auto err = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, streamRedirect.GetErrorString().find(expectedErr1));
  EXPECT_EQ(string::npos, streamRedirect.GetErrorString().find(expectedErr2));
  ProjMgrTestEnv::CompareFile(cbuildidx,
    testinput_folder + "/TestSolution/PackMissing/ref/missing_pack_specific_context.cbuild-idx.yml");
}

TEST_F(ProjMgrUnitTests, ComponentInstances) {
  char* argv[9];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestSolution/Instances/instances.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"-c";
  argv[7] = (char*)".Debug";
  argv[8] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(9, argv, m_envp));

  // Check generated files
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/instances.Debug+RteTest_ARMCM3.cprj",
    testinput_folder + "/TestSolution/Instances/ref/instances.Debug+RteTest_ARMCM3.cprj");
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/instances.Debug+RteTest_ARMCM3.cbuild.yml",
    testinput_folder + "/TestSolution/Instances/ref/instances.Debug+RteTest_ARMCM3.cbuild.yml");

  // Check error message
  argv[6] = (char*)"-c";
  argv[7] = (char*)".Error";
  EXPECT_EQ(1, RunProjMgr(9, argv, m_envp));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find("error csolution: component 'Device:Startup&RteTest Startup' does not accept more than 1 instance(s)"));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_Cbuild_Template_API_Node) {
  char* argv[7];
  StdStreamRedirect streamRedirect;
  const string& csolutionFile = testinput_folder + "/TestSolution/TemplateAndApi/template_api.csolution.yml";

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

  // Check generated cbuild yml files
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/template_api.Debug+RteTest_ARMCM3.cbuild.yml",
    testinput_folder + "/TestSolution/TemplateAndApi/ref/template_api.Debug+RteTest_ARMCM3.cbuild.yml");
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_Config_Base_Update_File) {
  char* argv[7];
  StdStreamRedirect streamRedirect;

  const string& csolution = testinput_folder + "/TestSolution/TestBaseUpdate/test.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

  EXPECT_FALSE(regex_match(streamRedirect.GetOutString(), regex("Multiple(.*)files detected(.*)")));
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/project.Debug+CM0.cbuild.yml",
    testinput_folder + "/TestSolution/TestBaseUpdate/ref/project.Debug+CM0.cbuild.yml");
}

TEST_F(ProjMgrUnitTests, CheckPackMetadata) {
  char* argv[9];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestSolution/PackMetadata/metadata.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"-c";
  argv[7] = (char*)".Debug";
  argv[8] = (char*)"--cbuildgen";
  EXPECT_EQ(0, RunProjMgr(9, argv, m_envp));

  // Check generated files
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/metadata.Debug+RteTest_ARMCM3.cprj",
    testinput_folder + "/TestSolution/PackMetadata/ref/metadata.Debug+RteTest_ARMCM3.cprj");
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/metadata.Debug+RteTest_ARMCM3.cbuild.yml",
    testinput_folder + "/TestSolution/PackMetadata/ref/metadata.Debug+RteTest_ARMCM3.cbuild.yml");
  ProjMgrTestEnv::CompareFile(testinput_folder + "/TestSolution/PackMetadata/metadata.cbuild-pack.yml",
    testinput_folder + "/TestSolution/PackMetadata/ref/metadata.cbuild-pack.yml");

  // Check warning message
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find("warning csolution: loaded pack 'ARM::RteTest_DFP0.1.1+metadata' does not match specified metadata 'user_metadata'"));

  // Check matching metadata, no warning should be issued
  streamRedirect.ClearStringStreams();
  argv[6] = (char*)"-c";
  argv[7] = (char*)".Match";
  EXPECT_EQ(0, RunProjMgr(9, argv, m_envp));
  EXPECT_TRUE(streamRedirect.GetErrorString().empty());
}

TEST_F(ProjMgrUnitTests, CbuildPackSelectBy) {
  // Accept "selected-by" for backward compatibility
  char* argv[6];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestSolution/PackLocking/selected-by.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(6, argv, m_envp));
  EXPECT_TRUE(streamRedirect.GetErrorString().empty());
}

TEST_F(ProjMgrUnitTests, Executes) {
  char* argv[6];
  const string& csolution = testinput_folder + "/TestSolution/Executes/solution.csolution.yml";
  const string& cbuildidx = testoutput_folder + "/solution.cbuild-idx.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  RteFsUtils::RemoveFile(cbuildidx);
  EXPECT_EQ(0, RunProjMgr(6, argv, m_envp));

  // Check generated files
  ProjMgrTestEnv::CompareFile(cbuildidx,
    testinput_folder + "/TestSolution/Executes/ref/solution.cbuild-idx.yml");

  // Check error message
  StdStreamRedirect streamRedirect;
  const string& csolutionError = testinput_folder + "/TestSolution/Executes/error.csolution.yml";
  argv[3] = (char*)csolutionError.c_str();
  RteFsUtils::RemoveFile(cbuildidx);
  EXPECT_EQ(1, RunProjMgr(6, argv, m_envp));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find("error csolution: context 'unknown.Debug+RteTest_ARMCM3' referenced by access sequence 'elf' is not compatible"));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_GeneratorError) {
  char* argv[6];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestGenerator/test-gpdsc-error.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find("error csolution: redefinition from 'balanced' into 'none' is not allowed"));
}

TEST_F(ProjMgrUnitTests, TestRelativeOutputOption) {
  char* argv[5];
  const string& csolution = testinput_folder + "/TestSolution/Executes/solution.csolution.yml";
  RteFsUtils::RemoveFile(testinput_folder + "/TestSolution/Executes/solution.cbuild-pack.yml");
  const string& testFolder = RteFsUtils::ParentPath(testoutput_folder);
  const string& outputFolder = testFolder + "/outputFolder";

  // Ensure output folder does not exist
  RteFsUtils::RemoveDir(outputFolder);
  ASSERT_FALSE(RteFsUtils::Exists(outputFolder));

  const string& currentFolder = RteFsUtils::GetCurrentFolder();
  error_code ec;
  fs::current_path(testFolder, ec);
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"--output";
  argv[4] = (char*)"outputFolder";
  EXPECT_EQ(0, RunProjMgr(5, argv, m_envp));
  fs::current_path(currentFolder, ec);

  // Check generated cbuild-idx
  ProjMgrTestEnv::CompareFile(outputFolder + "/solution.cbuild-idx.yml",
    testinput_folder + "/TestSolution/Executes/ref/solution.cbuild-idx.yml");
}

TEST_F(ProjMgrUnitTests, TestRestrictedContextsWithContextSet_Failed_Read_From_CbuildSet) {
  char* argv[4];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestSolution/test_restricted_contexts.csolution.yml";
  const string& expectedErrMsg = "\
error csolution: invalid combination of contexts specified in test_restricted_contexts.cbuild-set.yml:\n\
  target-type does not match for 'test1.Debug+CM3' and 'test1.Debug+CM0'";

  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-S";

  EXPECT_EQ(1, RunProjMgr(4, argv, 0));
  auto errMsg = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errMsg.find(expectedErrMsg));
}

TEST_F(ProjMgrUnitTests, TestRestrictedContextsWithContextSet_Failed1) {
  char* argv[14];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestSolution/test.csolution.yml";
  const string& expectedErrMsg = "\
error csolution: invalid combination of contexts specified in command line:\n\
  target-type does not match for 'test2.Debug+CM3' and 'test1.Debug+CM0'";

  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-c";
  argv[4] = (char*)"test1.Debug+CM0";
  argv[5] = (char*)"-c";
  argv[6] = (char*)"test1.Release+CM0";
  argv[7] = (char*)"-c";
  argv[8] = (char*)"test2.Debug+CM0";
  argv[9] = (char*)"-c";
  argv[10] = (char*)"test2.Debug+CM3";
  argv[11] = (char*)"--output";
  argv[12] = (char*)testoutput_folder.c_str();
  argv[13] = (char*)"-S";

  EXPECT_EQ(1, RunProjMgr(14, argv, 0));
  auto errMsg = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errMsg.find(expectedErrMsg));
}

TEST_F(ProjMgrUnitTests, TestRestrictedContextsWithContextSet_Failed2) {
  char* argv[12];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestSolution/test.csolution.yml";
  const string& expectedErrMsg = "\
error csolution: invalid combination of contexts specified in command line:\n\
  build-type is not unique in 'test1.Release+CM0' and 'test1.Debug+CM0'";

  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-c";
  argv[4] = (char*)"test1.Debug+CM0";
  argv[5] = (char*)"-c";
  argv[6] = (char*)"test1.Release+CM0";
  argv[7] = (char*)"-c";
  argv[8] = (char*)"test2.Debug+CM0";
  argv[9] = (char*)"--output";
  argv[10] = (char*)testoutput_folder.c_str();
  argv[11] = (char*)"-S";

  EXPECT_EQ(1, RunProjMgr(12, argv, 0));
  auto errMsg = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errMsg.find(expectedErrMsg));
}

TEST_F(ProjMgrUnitTests, TestRestrictedContextsWithContextSet_Pass) {
  char* argv[10];
  const string& csolution = testinput_folder + "/TestSolution/test.csolution.yml";

  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-c";
  argv[4] = (char*)"test1.Debug+CM0";
  argv[5] = (char*)"-c";
  argv[6] = (char*)"test2.Debug+CM0";
  argv[7] = (char*)"--output";
  argv[8] = (char*)testoutput_folder.c_str();
  argv[9] = (char*)"-S";

  EXPECT_EQ(0, RunProjMgr(10, argv, m_envp));
}

TEST_F(ProjMgrUnitTests, ValidateCreatedFor) {
  const vector<tuple<string, bool, string, bool>> testData = {
    { "CMSIS-Toolbox@9.9.9", true , "warning", true  },
    { "CMSIS-Toolbox@9.9.9", false, "error"  , false },
    { "CMSIS-Tooling@9.9.9", false, "warning", true  },
    { "CMSIS-Toolbox@0.0.0", false, ""       , true  },
    { ""                   , false, ""       , true  },
    { "Unknown"            , false, "warning", true  },
  };
  StdStreamRedirect streamRedirect;
  for (const auto& [createdFor, rpcMode, expectedMsg, expectedReturn] : testData) {
    m_rpcMode = rpcMode;
    streamRedirect.ClearStringStreams();
    EXPECT_EQ(expectedReturn, ValidateCreatedFor(createdFor));
    auto errMsg = streamRedirect.GetErrorString();
    if (expectedMsg.empty()) {
      EXPECT_EQ(RteUtils::EMPTY_STRING, errMsg);
    } else {
      EXPECT_NE(string::npos, errMsg.find(expectedMsg));
    }
  }
}

TEST_F(ProjMgrUnitTests, FailCreatedFor) {
  char* argv[5];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestSolution/created-for.csolution.yml";
  const string& expectedErrMsg = "error csolution: 'created-for' in file .*created-for\\.csolution\\.yml\
 specifies a minimum version 9\\.9\\.9 \\(current version .*\\)\n";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"--output";
  argv[4] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(5, argv, 0));
  auto errMsg = streamRedirect.GetErrorString();
  EXPECT_TRUE(regex_match(errMsg, regex(expectedErrMsg)));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_FailedConvertShouldCreateRteDirInProjectFolder) {
  char* argv[4];
  const string& app = testoutput_folder + "/app";
  const string& csolution = app + "/app.csolution.yml";
  const string& work = testoutput_folder + "/work";

  ASSERT_TRUE(RteFsUtils::CreateDirectories(work));

  ASSERT_TRUE(RteFsUtils::CreateTextFile(csolution, "# yaml-language-server: $schema=https://raw.githubusercontent.com/Open-CMSIS-Pack/devtools/schemas/projmgr/2.4.0/tools/projmgr/schemas/csolution.schema.json\n"
    "solution:\n"
    "  build-types:\n"
    "    - type: debug\n"
    "  target-types:\n"
    "    - type: main\n"
    "  projects:\n"
    "    - project: test.cproject.yml\n"));

  ASSERT_TRUE(RteFsUtils::CreateTextFile(app + "/test.cproject.yml", "# yaml-language-server: $schema=https://raw.githubusercontent.com/Open-CMSIS-Pack/devtools/schemas/projmgr/2.4.0/tools/projmgr/schemas/cproject.schema.json\n"
    "project:\n"));

  TempSwitchCwd cwdSwitcher(work);

  ASSERT_FALSE(RteFsUtils::IsDirectory(work + "/RTE"));
  ASSERT_FALSE(RteFsUtils::IsDirectory(app + "/RTE"));

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  EXPECT_EQ(1, RunProjMgr(4, argv, 0));

  ASSERT_FALSE(RteFsUtils::IsDirectory(work + "/RTE"));
  ASSERT_FALSE(RteFsUtils::IsDirectory(app + "/RTE"));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_CprjFilesShouldBePlacedInProjectTree) {
  char* argv[5];
  const string& app = testoutput_folder + "/app";
  const string& csolution = app + "/app.csolution.yml";
  const string& cprjdir = app + "/foo/baz";

  ASSERT_TRUE(RteFsUtils::CreateTextFile(csolution, "# yaml-language-server: $schema=https://raw.githubusercontent.com/Open-CMSIS-Pack/devtools/schemas/projmgr/2.4.0/tools/projmgr/schemas/csolution.schema.json\n"
    "solution:\n"
    "  output-dirs:\n"
    "    intdir: $ProjectDir()$/build/$BuildType$\n"
    "    outdir: $ProjectDir()$/build/$BuildType$\n"
    "    cprjdir: $ProjectDir()$/baz\n"
    "  generators:\n"
    "    base-dir: $ProjectDir()$/generated\n"
    "  build-types:\n"
    "    - type: debug\n"
    "      compiler: GCC\n"
    "  target-types:\n"
    "    - type: main\n"
    "  projects:\n"
    "    - project: foo/test.cproject.yml\n"));

  ASSERT_TRUE(RteFsUtils::CreateTextFile(app + "/foo/test.cproject.yml", "# yaml-language-server: $schema=https://raw.githubusercontent.com/Open-CMSIS-Pack/devtools/schemas/projmgr/2.4.0/tools/projmgr/schemas/cproject.schema.json\n"
    "project:\n"));

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(5, argv, 0));

  EXPECT_TRUE(RteFsUtils::Exists(cprjdir + "/test.debug+main.cprj"));
  EXPECT_TRUE(RteFsUtils::Exists(cprjdir + "/test.debug+main.cbuild.yml"));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ProjectDirShouldBeExpanded) {
  char* argv[5];
  StdStreamRedirect streamRedirect;
  const string& app = testoutput_folder + "/app";
  const string& csolution = app + "/app.csolution.yml";
  const string& cprjdir = app + "/foo/baz";

  ASSERT_TRUE(RteFsUtils::CreateTextFile(csolution, "# yaml-language-server: $schema=https://raw.githubusercontent.com/Open-CMSIS-Pack/devtools/schemas/projmgr/2.4.0/tools/projmgr/schemas/csolution.schema.json\n"
    "solution:\n"
    "  output-dirs:\n"
    "    intdir: $ProjectDir()$/build/$BuildType$\n"
    "    outdir: $ProjectDir()$/build/$BuildType$\n"
    "    cprjdir: $ProjectDir()$/baz\n"
    "  generators:\n"
    "    base-dir: $ProjectDir()$/generated\n"
    "  build-types:\n"
    "    - type: debug\n"
    "  target-types:\n"
    "    - type: main\n"
    "  packs:\n"
    "    - pack: does-not-exist\n"
    "  projects:\n"
    "    - project: foo/test.cproject.yml\n"));

  ASSERT_TRUE(RteFsUtils::CreateTextFile(app + "/foo/test.cproject.yml", "# yaml-language-server: $schema=https://raw.githubusercontent.com/Open-CMSIS-Pack/devtools/schemas/projmgr/2.4.0/tools/projmgr/schemas/cproject.schema.json\n"
    "project:\n"));

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(5, argv, 0));

  EXPECT_EQ(string::npos, streamRedirect.GetOutString().find("$ProjectDir()$"))
    << "stdout:\n" << streamRedirect.GetOutString()
    << "stderr:\n" << streamRedirect.GetErrorString();

  EXPECT_TRUE(RteFsUtils::Exists(cprjdir + "/test.debug+main.cprj"));
  EXPECT_TRUE(RteFsUtils::Exists(cprjdir + "/test.debug+main.cbuild.yml"));
}

TEST_F(ProjMgrUnitTests, SelectableToolchains) {
  StdStreamRedirect streamRedirect;
  char* argv[6];
  const string& csolution = testinput_folder + "/TestSolution/SelectableToolchains/select-compiler.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();
  argv[5] = (char*)"--cbuildgen";
  EXPECT_EQ(ErrorCode::COMPILER_NOT_DEFINED, RunProjMgr(6, argv, m_envp));
  const string err = streamRedirect.GetErrorString();
  const string expectedErr = \
    "error csolution: compiler undefined, use '--toolchain' option or add 'compiler: <value>' to yml input, selectable values can be found in cbuild-idx.yml";
  EXPECT_NE(string::npos, err.find(expectedErr));

  ProjMgrTestEnv::CompareFile(testoutput_folder + "/select-compiler.cbuild-idx.yml",
    testinput_folder + "/TestSolution/SelectableToolchains/ref/select-compiler.cbuild-idx.yml");
}

TEST_F(ProjMgrUnitTests, SourcesAddedByMultipleComponents) {
  StdStreamRedirect streamRedirect;
  char* argv[5];
  const string& csolution = testinput_folder + "/TestSolution/ComponentSources/components.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();;
  EXPECT_EQ(0, RunProjMgr(5, argv, m_envp));

  const string& expected = "\
warning csolution: source modules added by multiple components, duplicate ignored:\n\
  filename: .*/ARM/RteTest/0.1.0/Dummy/dummy.c\n\
    - component: ARM::RteTest:DupFilename@0.9.9\n\
      from-pack: ARM::RteTest@0.1.0\n\
    - component: ARM::RteTest:TemplateFile@0.9.9\n\
      from-pack: ARM::RteTest@0.1.0\n\
";

  string errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(regex_search(errStr, regex(expected)));

  ProjMgrTestEnv::CompareFile(testoutput_folder + "/out/components/RteTest_ARMCM3/Debug/components.Debug+RteTest_ARMCM3.cbuild.yml",
    testinput_folder + "/TestSolution/ComponentSources/ref/components.Debug+RteTest_ARMCM3.cbuild.yml");
}

TEST_F(ProjMgrUnitTests, AccessSequencesMixedBuildTypes) {
  char* argv[9];
  const string& csolution = testinput_folder + "/TestAccessSequences/mixed-build-type.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();
  argv[5] = (char*)"-c";
  argv[6] = (char*)"ns.Release";
  argv[7] = (char*)"-c";
  argv[8] = (char*)"s.Debug";
  EXPECT_EQ(0, RunProjMgr(9, argv, m_envp));

  const YAML::Node& cbuild1 = YAML::LoadFile(testoutput_folder + "/out/ns/CM0/Release/ns.Release+CM0.cbuild.yml");
  EXPECT_EQ(cbuild1["build"]["groups"][0]["files"][0]["file"].as<string>(), "../../../s/CM0/Debug/s_CMSE_Lib.o");
  const YAML::Node& cbuild2 = YAML::LoadFile(testoutput_folder + "/out/ns/CM3/Release/ns.Release+CM3.cbuild.yml");
  EXPECT_EQ(cbuild2["build"]["groups"][0]["files"][0]["file"].as<string>(), "../../../s/CM3/Debug/s_CMSE_Lib.o");
}

TEST_F(ProjMgrUnitTests, ForContextRegex) {
  char* argv[3];
  const string& csolution = testinput_folder + "/TestSolution/ForContextRegex/regex.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  EXPECT_EQ(0, RunProjMgr(3, argv, m_envp));

  const vector<string> expectedfiles = {
    "../../../../CM0.c", "../../../../CM3.c", "../../../../Debug_CM0_CM3.c",
    "../../../../Release.c", "../../../../Debug.c", "../../../../Debug_Release_CM0.c"
  };
  const vector<tuple<string, string, vector<bool>>> testData = {
    // expected files: CM0.c | CM3.c | Debug_CM0_CM3.c | Release.c | Debug.c | Debug_Release_CM0.c
    {"Debug", "CM0",   { true  , false , true            , false     , true    , true             }},
    {"Debug", "CM3",   { false , true  , true            , false     , true    , false            }},
    {"Release", "CM0", { true  , false , false           , true      , false   , true             }},
    {"Release", "CM3", { false , true  , false           , true      , false   , false            }},
  };

  for (const auto& [build, target, expected] : testData) {
    const YAML::Node& node = YAML::LoadFile(testinput_folder +
      "/TestSolution/ForContextRegex/out/regex/" + target + "/" + build + "/regex" + "." + build + "+" + target + ".cbuild.yml");
    const auto& files = node["build"]["groups"][0]["files"].as<vector<map<string, string>>>();
    int index = 0;
    for (const auto& expectedfile : expectedfiles) {
      EXPECT_EQ(expected[index++], ProjMgrTestEnv::IsFileInCbuildFilesList(files, expectedfile))
        << "failed for context \"" << ("." + build + "+" + target) << "\" and expected file \"" << expectedfile << "\"";
    }
  }
}

TEST_F(ProjMgrUnitTests, ForContextRegexFail) {
  StdStreamRedirect streamRedirect;
  char* argv[3];
  const string& csolution = testinput_folder + "/TestSolution/ForContextRegex/regex-fail.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  EXPECT_EQ(1, RunProjMgr(3, argv, m_envp));

  auto errMsg = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errMsg.find("error csolution: invalid pattern '^.Debug+(CM0'"));
}

TEST_F(ProjMgrUnitTests, RebuildConditions) {
  char* argv[5];
  const string& csolution = testinput_folder + "/TestSolution/RebuildConditions/rebuild.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  EXPECT_EQ(0, RunProjMgr(3, argv, m_envp));
  const YAML::Node& cbuild1 = YAML::LoadFile(testinput_folder + "/TestSolution/RebuildConditions/rebuild.cbuild-idx.yml");
  // rebuild at solution level due to change in context selection
  EXPECT_TRUE(cbuild1["build-idx"]["rebuild"].as<bool>());

  argv[3] = (char*)"--toolchain";
  argv[4] = (char*)"GCC";
  EXPECT_EQ(0, RunProjMgr(5, argv, m_envp));
  const YAML::Node& cbuild2 = YAML::LoadFile(testinput_folder + "/TestSolution/RebuildConditions/rebuild.cbuild-idx.yml");
  // rebuild at context level due to change in compiler selection
  EXPECT_TRUE(cbuild2["build-idx"]["cbuilds"][0]["rebuild"].as<bool>());
}

TEST_F(ProjMgrUnitTests, RunProjMgr_MultiVariantComponent) {
  StdStreamRedirect streamRedirect;
  char* argv[6];
  string csolutionFile = testinput_folder + "/TestSolution/test_use_multiple_variant_component.csolution.yml";

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolutionFile.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(1, RunProjMgr(6, argv, m_envp));

  auto errMsg = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos,
    errMsg.find("multiple variants of the same component are specified:\n  - Device:Test variant\n  - Device:Test variant&Variant name"));
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListPacks_ContextSet) {
  char* argv[6];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestSolution/contexts.csolution.yml";

  // list packs
  argv[1] = (char*)"list";
  argv[2] = (char*)"packs";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  argv[5] = (char*)"-S";
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  auto outStr = streamRedirect.GetOutString();
  EXPECT_NE(outStr.find("ARM::RteTest_DFP@0.2.0"), string::npos);
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListBoards_ContextSet) {
  char* argv[6];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestSolution/contexts.csolution.yml";

  argv[1] = (char*)"list";
  argv[2] = (char*)"boards";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  argv[5] = (char*)"-S";
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  auto outStr = streamRedirect.GetOutString();
  EXPECT_NE(outStr.find("Keil::RteTest Dummy board:1.2.3 (ARM::RteTest_DFP@0.2.0)"), string::npos);
}

TEST_F(ProjMgrUnitTests, RunProjMgr_ListDevices_ContextSet) {
  char* argv[6];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestSolution/contexts.csolution.yml";

  argv[1] = (char*)"list";
  argv[2] = (char*)"devices";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  argv[5] = (char*)"-S";
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  auto outStr = streamRedirect.GetOutString();
  EXPECT_NE(outStr.find("ARM::RteTest_ARMCM0 (ARM::RteTest_DFP@0.2.0)"), string::npos);
}

TEST_F(ProjMgrUnitTests, ConvertEmptyLayer) {
  char* argv[5];
  const string& csolution = testinput_folder + "/TestLayers/empty-layer.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(5, argv, m_envp));

  ProjMgrTestEnv::CompareFile(testoutput_folder + "/empty-layer.cbuild-idx.yml",
    testinput_folder + "/TestLayers/ref/empty-layer.cbuild-idx.yml");
}

TEST_F(ProjMgrUnitTests, RunProjMgr_conflict_cbuild_set) {
  char* argv[7];
  StdStreamRedirect streamRedirect;

  // convert --solution solution.yml
  const string& csolution = testinput_folder + "/TestSolution/test.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-c";
  argv[5] = (char*)"test1+CM0";
  argv[6] = (char*)"-S";
  EXPECT_EQ(1, RunProjMgr(7, argv, 0));

  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(errStr.find("build-type is not unique in 'test1.Release+CM0' and 'test1.Debug+CM0'"), string::npos);
}

TEST_F(ProjMgrUnitTests, ListLayers_update_idx_with_no_compiler_selected) {
  StdStreamRedirect streamRedirect;
  char* argv[6];
  const string& csolution = testinput_folder + "/TestLayers/no_compiler.csolution.yml";
  string expectedOutStr = ".*no_compiler.cbuild-idx.yml - info csolution: file generated successfully\\n";

  argv[1] = (char*)"list";
  argv[2] = (char*)"layers";
  argv[3] = (char*)"--solution";
  argv[4] = (char*)csolution.c_str();
  argv[5] = (char*)"--update-idx";

  EXPECT_EQ(0, RunProjMgr(6, argv, m_envp));
  EXPECT_TRUE(regex_match(streamRedirect.GetOutString(), regex(expectedOutStr)));

  ProjMgrTestEnv::CompareFile(testinput_folder + "/TestLayers/ref/no_compiler.cbuild-idx.yml",
    testinput_folder + "/TestLayers/no_compiler.cbuild-idx.yml");
  EXPECT_TRUE(ProjMgrYamlSchemaChecker().Validate(testinput_folder + "/TestLayers/no_compiler.cbuild-idx.yml"));
}

TEST_F(ProjMgrUnitTests, ConfigFilesUpdate) {
  StdStreamRedirect streamRedirect;
  char* argv[6];
  const string& csolution = testinput_folder + "/TestSolution/ConfigFilesUpdate/config.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();

  // --no-update-rte
  std::vector<std::tuple<std::string, int, std::string, std::string>> testDataVector1 = {
    { "BaseUnknown", 0, "warning csolution: file '.*/startup_ARMCM3.c.base' not found; base version unknown", "missing base"       },
    { "Patch",       0, "warning csolution: file '.*/startup_ARMCM3.c' update suggested; use --update-rte",   "update suggested"   },
    { "Minor",       0, "warning csolution: file '.*/startup_ARMCM3.c' update recommended; use --update-rte", "update recommended" },
    { "Major",       1, "error csolution: file '.*/startup_ARMCM3.c' update required; use --update-rte",      "update required"    },
    { "Missing",     1, "error csolution: file '.*/startup_ARMCM3.c' not found; use --update-rte",            "missing file"       },
  };

  for (const auto& [build, errCode, errMsg, status] : testDataVector1) {
    const string contextOption = "." + build;
    streamRedirect.ClearStringStreams();
    argv[3] = (char*)"--no-update-rte";
    argv[4] = (char*)"-c";
    argv[5] = (char*)contextOption.c_str();
    EXPECT_EQ(errCode, RunProjMgr(6, argv, m_envp));
    auto errStr = streamRedirect.GetErrorString();
    EXPECT_TRUE(regex_search(errStr, regex(errMsg)));
    const YAML::Node& cbuild = YAML::LoadFile(testinput_folder +
      "/TestSolution/ConfigFilesUpdate/out/config/RteTest_ARMCM3/" + build + "/config." + build + "+RteTest_ARMCM3.cbuild.yml");
    EXPECT_EQ(status, cbuild["build"]["components"][0]["files"][3]["status"].as<string>());
  }

  // --update-rte
  std::vector<std::tuple<std::string, int, std::string>> testDataVector2 = {
    { "BaseUnknown", 0, "" },
    { "Patch",       0, "warning csolution: file '.*/startup_ARMCM3.c' update suggested; merge content from update file, rename update file to base file and remove previous base file" },
    { "Minor",       0, "warning csolution: file '.*/startup_ARMCM3.c' update recommended; merge content from update file, rename update file to base file and remove previous base file" },
    { "Major",       1, "error csolution: file '.*/startup_ARMCM3.c' update required; merge content from update file, rename update file to base file and remove previous base file" },
    { "Missing",     0, "" },
  };

  for (const auto& [build, errCode, errMsg] : testDataVector2) {
    const string contextOption = "." + build;
    streamRedirect.ClearStringStreams();
    argv[3] = (char*)"-c";
    argv[4] = (char*)contextOption.c_str();
    EXPECT_EQ(errCode, RunProjMgr(5, argv, m_envp));
    auto errStr = streamRedirect.GetErrorString();
    EXPECT_TRUE(regex_search(errStr, regex(errMsg)));
  }
}

TEST_F(ProjMgrUnitTests, RegionsFileGeneration) {
  char* argv[3];
  const string& csolution = testinput_folder + "/TestMemoryRegions/regions.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  EXPECT_EQ(0, RunProjMgr(3, argv, m_envp));

  ProjMgrTestEnv::CompareFile(testinput_folder + "/TestMemoryRegions/ref/RteTestDevice0/regions_RteTestBoard0.h",
    testinput_folder + "/TestMemoryRegions/RTE/Device/RteTestDevice0/regions_RteTestBoard0.h");
  ProjMgrTestEnv::CompareFile(testinput_folder + "/TestMemoryRegions/ref/RteTestDevice1/regions_RteTestBoard1.h",
    testinput_folder + "/TestMemoryRegions/RTE/Device/RteTestDevice1/regions_RteTestBoard1.h");
  ProjMgrTestEnv::CompareFile(testinput_folder + "/TestMemoryRegions/ref/RteTestDevice_Dual_cm0_core1/regions_RteTestDevice_Dual_cm0_core1.h",
    testinput_folder + "/TestMemoryRegions/RTE/Device/RteTestDevice_Dual_cm0_core1/regions_RteTestDevice_Dual_cm0_core1.h");
}

TEST_F(ProjMgrUnitTests, MissingFile) {
  StdStreamRedirect streamRedirect;
  char* argv[3];
  const string& csolution = testinput_folder + "/TestSolution/missing.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  EXPECT_EQ(1, RunProjMgr(3, argv, m_envp));
  const auto& errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find("missing.c' was not found"));
  EXPECT_NE(string::npos, errStr.find("regions.h' was not found"));
  EXPECT_EQ(string::npos, errStr.find("generated.h' was not found"));
  EXPECT_EQ(string::npos, errStr.find("generated.c' was not found"));
}

TEST_F(ProjMgrUnitTests, RunProjMgrSolution_pack_version_not_available) {
  char* argv[7];
  StdStreamRedirect streamRedirect;
  std::string errExpected = "required pack: ARM::RteTest_DFP@0.1.0 not installed, version fixed in *.cbuild-pack.yml file";

  const string& csolution = testinput_folder + "/TestSolution/PackLocking/pack_version_not_available.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-o";
  argv[5] = (char*)testoutput_folder.c_str();
  argv[6] = (char*)"--cbuildgen";
  EXPECT_EQ(1, RunProjMgr(7, argv, 0));

  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(errExpected));
}

TEST_F(ProjMgrUnitTests, ReportPacksUnused) {
  char* argv[5];
  const string& csolution = testinput_folder + "/TestSolution/PacksUnused/packs.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-c";
  argv[4] = (char*)"+CM0";
  EXPECT_EQ(0, RunProjMgr(5, argv, 0));
  const YAML::Node& cbuild1 = YAML::LoadFile(testinput_folder + "/TestSolution/PacksUnused/packs.cbuild-idx.yml");
  EXPECT_EQ(2, cbuild1["build-idx"]["cbuilds"][0]["packs-unused"].size());
  EXPECT_EQ("ARM::RteTestBoard@0.1.0", cbuild1["build-idx"]["cbuilds"][0]["packs-unused"][0]["pack"].as<string>());
  EXPECT_EQ("ARM::RteTestGenerator@0.1.0", cbuild1["build-idx"]["cbuilds"][0]["packs-unused"][1]["pack"].as<string>());

  argv[4] = (char*)"+Board";
  EXPECT_EQ(0, RunProjMgr(5, argv, 0));
  const YAML::Node& cbuild2 = YAML::LoadFile(testinput_folder + "/TestSolution/PacksUnused/packs.cbuild-idx.yml");
  EXPECT_EQ(1, cbuild2["build-idx"]["cbuilds"][0]["packs-unused"].size());
  EXPECT_EQ("ARM::RteTestGenerator@0.1.0", cbuild2["build-idx"]["cbuilds"][0]["packs-unused"][0]["pack"].as<string>());
}

TEST_F(ProjMgrUnitTests, GetToolboxVersion) {
  const string& testdir = testoutput_folder + "/toolbox_version";
  string fileName = "manifest_1.test2.3.yml";
  string filePath = testdir + "/" + fileName;
  RteFsUtils::CreateDirectories(testdir);
  RteFsUtils::CreateTextFile(filePath, "");

  StdStreamRedirect streamRedirect;
  EXPECT_EQ("", GetToolboxVersion(testdir));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(errStr.find("manifest file does not exist") != string::npos);

  streamRedirect.ClearStringStreams();
  fileName = "manifest_1.2.3.yml";
  filePath = testdir + "/" + fileName;
  RteFsUtils::CreateDirectories(testdir);
  RteFsUtils::CreateTextFile(filePath, "");
  EXPECT_EQ("1.2.3", GetToolboxVersion(testdir));

  RteFsUtils::RemoveDir(testdir);
}

TEST_F(ProjMgrUnitTests, PackCaseInsensitive) {
  // pack identifiers are now case insensitive regardless of platform and file system
  char* argv[3];
  const string& csolution = testinput_folder + "/TestSolution/pack_case_insensitive.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  EXPECT_EQ(0, RunProjMgr(3, argv, 0));

  // check cbuild-pack.yml has a resolved pack selected by pack identifiers with mixed letter-cases
  const YAML::Node& cbuild = YAML::LoadFile(testinput_folder + "/TestSolution/pack_case_insensitive.cbuild-pack.yml");
  const auto& resolvedPack = cbuild["cbuild-pack"]["resolved-packs"][0];
  EXPECT_EQ("ARM::RteTest_DFP@0.2.0", resolvedPack["resolved-pack"].as<string>());
  EXPECT_EQ("ARM::RteTest_DFP", resolvedPack["selected-by-pack"][0].as<string>());
  EXPECT_EQ("Arm::RteTest_DFP", resolvedPack["selected-by-pack"][1].as<string>());
}

TEST_F(ProjMgrUnitTests, InvalidContextSet) {
  StdStreamRedirect streamRedirect;
  char* argv[4];
  const string& csolution = testinput_folder + "/TestSolution/invalid-context-set.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"--context-set";
  EXPECT_EQ(1, RunProjMgr(4, argv, m_envp));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find("unknown selected context(s):\n  unknown1.debug+target\n  unknown2.release+target"));
}

TEST_F(ProjMgrUnitTests, TestRunDebug) {
  char* argv[7];
  const string& csolution = testinput_folder + "/TestRunDebug/run-debug.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();
  argv[5] = (char*)"--active";
  argv[6] = (char*)"TestHW";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/out/run-debug+TestHW.cbuild-run.yml",
    testinput_folder + "/TestRunDebug/ref/run-debug+TestHW.cbuild-run.yml");
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/out/run-debug/TestHW/run-debug+TestHW.cbuild.yml",
    testinput_folder + "/TestRunDebug/ref/run-debug+TestHW.cbuild.yml");
  const YAML::Node& cbuildIdx = YAML::LoadFile(testoutput_folder + "/run-debug.cbuild-idx.yml");
  EXPECT_EQ("out/run-debug+TestHW.cbuild-run.yml", cbuildIdx["build-idx"]["cbuild-run"].as<string>());

  argv[6] = (char*)"TestHW2";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/out/run-debug+TestHW2.cbuild-run.yml",
    testinput_folder + "/TestRunDebug/ref/run-debug+TestHW2.cbuild-run.yml");
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/out/run-debug/TestHW2/run-debug+TestHW2.cbuild.yml",
    testinput_folder + "/TestRunDebug/ref/run-debug+TestHW2.cbuild.yml");

  // Check cbuild-run file generated by csolution 2.10.0 was removed
  EXPECT_FALSE(RteFsUtils::Exists(testinput_folder + "/TestRunDebug/run-debug+TestHW.cbuild-run.yml"));
}

TEST_F(ProjMgrUnitTests, TestRunDebugCustom) {
  const string& debugAdaptersPath = etc_folder + "/debug-adapters.yml";
  const string& backup = RteFsUtils::BackupFile(debugAdaptersPath);
  YAML::Node debugAdapters = YAML::LoadFile(debugAdaptersPath);
  YAML::Node testAdapter;
  testAdapter["name"] = "Test Custom Adapter";
  testAdapter["defaults"]["custom-adapter-key"] = "custom adapter value";
  testAdapter["defaults"]["custom-key-overwrite"] = "custom adapter key overwrite";
  testAdapter["defaults"]["custom-map"]["adapter-key"] = "adapter value";
  testAdapter["defaults"]["custom-array"][0] = "adapter item";
  testAdapter["defaults"]["custom-array-map"][0]["adapter-key"] = "adapter value";
  debugAdapters["debug-adapters"].push_back(testAdapter);
  ofstream debugAdaptersFile;
  debugAdaptersFile.open(debugAdaptersPath, fstream::trunc);
  debugAdaptersFile << debugAdapters << std::endl;
  debugAdaptersFile.close();

  char* argv[7];
  const string& csolution = testinput_folder + "/TestRunDebug/custom.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();
  argv[5] = (char*)"--active";
  argv[6] = (char*)"TestHW";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/out/custom+TestHW.cbuild-run.yml",
    testinput_folder + "/TestRunDebug/ref/custom+TestHW.cbuild-run.yml");

  error_code ec;
  fs::copy(fs::path(backup), fs::path(debugAdaptersPath), fs::copy_options::overwrite_existing, ec);
  RteFsUtils::RemoveFile(backup);
}

TEST_F(ProjMgrUnitTests, TestNoDbgconf) {
  char* argv[7];
  const string& csolution = testinput_folder + "/TestRunDebug/no-dbgconf.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();
  argv[5] = (char*)"--active";
  argv[6] = (char*)"ARMCM3";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));

  const YAML::Node& cbuild = YAML::LoadFile(testoutput_folder + "/out/run-debug/ARMCM3/run-debug+ARMCM3.cbuild.yml");
  EXPECT_FALSE(cbuild["build"]["dbgconf"].IsDefined());
  const YAML::Node& cbuildrun = YAML::LoadFile(testoutput_folder + "/out/no-dbgconf+ARMCM3.cbuild-run.yml");
  EXPECT_FALSE(cbuildrun["cbuild-run"]["debugger"]["dbgconf"].IsDefined());
}

TEST_F(ProjMgrUnitTests, MissingDbgconf) {
  const string csolutionFile = testinput_folder + "/TestSolution/test.csolution.yml";
  const string dbgconf = testinput_folder + "/TestSolution/.cmsis/test+CM0.dbgconf";
  char* argv[6];
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolutionFile.c_str();
  argv[3] = (char*)"-a";
  argv[4] = (char*)"CM0";
  EXPECT_EQ(0, RunProjMgr(5, argv, m_envp));

  // remove dbgconf file and convert again but with --no-update-rte
  // the missing dbgconf is just a warning, the convert must succeed
  StdStreamRedirect streamRedirect;
  EXPECT_TRUE(RteFsUtils::RemoveFile(dbgconf));
  EXPECT_FALSE(RteFsUtils::Exists(dbgconf));
  argv[5] = (char*)"--no-update-rte";
  EXPECT_EQ(0, RunProjMgr(6, argv, m_envp));
  auto errStr = streamRedirect.GetErrorString();
  auto expected = "warning csolution: file '" + dbgconf + "' not found; use --update-rte";
  EXPECT_TRUE(errStr.find(expected) != string::npos);
}

TEST_F(ProjMgrUnitTests, TestRunDebugMulticore) {
  char* argv[7];
  const string& csolution = testinput_folder + "/TestRunDebug/run-debug.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();
  argv[5] = (char*)"--active";
  argv[6] = (char*)"TestHW3";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));
  ProjMgrTestEnv::CompareFile(testoutput_folder + "/out/run-debug+TestHW3.cbuild-run.yml",
    testinput_folder + "/TestRunDebug/ref/run-debug+TestHW3.cbuild-run.yml");
}

TEST_F(ProjMgrUnitTests, TestRunDebugTelnet) {
  char* argv[7];
  const string& csolution = testinput_folder + "/TestRunDebug/telnet.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();
  argv[5] = (char*)"--active";

  // single core without port, with file mode
  argv[6] = (char*)"SingleCore";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));
  stringstream sstream0;
  const YAML::Node& cbuildrun0 = YAML::LoadFile(testoutput_folder + "/out/telnet+SingleCore.cbuild-run.yml");
  sstream0 << cbuildrun0["cbuild-run"]["debugger"]["telnet"];
  EXPECT_EQ(
R"(- mode: file
  port: 4444
  file: telnet+SingleCore)", sstream0.str());

  // dual core without ports
  argv[6] = (char*)"DualCore";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));
  const YAML::Node& cbuildrun1 = YAML::LoadFile(testoutput_folder + "/out/telnet+DualCore.cbuild-run.yml");
  stringstream sstream1;
  sstream1 << cbuildrun1["cbuild-run"]["debugger"]["telnet"];
  EXPECT_EQ(
R"(- mode: server
  pname: cm0_core0
  port: 4445
- mode: console
  pname: cm0_core1
  port: 4444)", sstream1.str());

  // dual core with start port and file mode
  argv[6] = (char*)"DualCore@TelnetFile";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));
  const YAML::Node& cbuildrun2 = YAML::LoadFile(testoutput_folder + "/out/telnet+DualCore.cbuild-run.yml");
  stringstream sstream2;
  sstream2 << cbuildrun2["cbuild-run"]["debugger"]["telnet"];
  EXPECT_EQ(
R"(- mode: monitor
  pname: cm0_core0
  port: 5556
- mode: file
  pname: cm0_core1
  port: 5555
  file: telnet+DualCore.cm0_core1)", sstream2.str());

  // dual core with jlink and no telnet
  argv[6] = (char*)"DualCore@JLinkNoTelnet";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));
  const YAML::Node& cbuildrun3 = YAML::LoadFile(testoutput_folder + "/out/telnet+DualCore.cbuild-run.yml");
  stringstream sstream3;
  sstream3 << cbuildrun3["cbuild-run"]["debugger"]["telnet"];
  EXPECT_EQ(
R"(- mode: off
  pname: cm0_core0
  port: 4445
- mode: off
  pname: cm0_core1
  port: 4444)", sstream3.str());

  // dual core with custom port numbers
  argv[6] = (char*)"DualCore@CustomPorts";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));
  const YAML::Node& cbuildrun4 = YAML::LoadFile(testoutput_folder + "/out/telnet+DualCore.cbuild-run.yml");
  stringstream sstream4;
  sstream4 << cbuildrun4["cbuild-run"]["debugger"]["telnet"];
  EXPECT_EQ(
    R"(- mode: monitor
  pname: cm0_core0
  port: 5678
- mode: monitor
  pname: cm0_core1
  port: 1234)", sstream4.str());

  // warnings
  StdStreamRedirect streamRedirect;
  argv[6] = (char*)"DualCore@Warnings";
  EXPECT_EQ(0, RunProjMgr(7, argv, m_envp));
  string expected = "\
warning csolution: \\'telnet:\\' pname is required \\(multicore device\\)\n\
warning csolution: pname \\'unknown\\' does not match any device pname\n\
";
  string errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(regex_search(errStr, regex(expected)));
}

TEST_F(ProjMgrUnitTests, Test_Check_Define_Value_With_Quotes) {
  StdStreamRedirect streamRedirect;
  char* argv[6];
  const string& csolution = testinput_folder + "/TestSolution/test_invalid_defines.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();

  // Test1: Check schema error
  string expected = "\
.*test_invalid_defines.csolution.yml:33:7 - error csolution: schema check failed, verify syntax\n\
.*test_invalid_defines.csolution.yml:34:7 - error csolution: schema check failed, verify syntax\n\
";
  EXPECT_EQ(1, RunProjMgr(5, argv, m_envp));
  string errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(regex_search(errStr, regex(expected)));

  //Test2: Check Parsing errors
  streamRedirect.ClearStringStreams();
  expected = "\
error csolution: invalid define: \\\"No_ending_escape_quotes, improper quotes\n\
error csolution: invalid define: Escape_quotes_in_\\\"middle\\\", improper quotes\n\
error csolution: invalid define: \\\"Invalid_ending\"\\, improper quotes\n\
error csolution: invalid define: \\\"No_ending_escape_quotes, improper quotes\n\
error csolution: invalid define: \\\"sam.h\\, improper quotes\n\
error csolution: invalid define: \\\"Invalid_ending\"\\, improper quotes\n\
error csolution: invalid define: No_Starting_escaped_quotes\\\", improper quotes\n\
error csolution: invalid define: \\\"Mixed_quotes\", improper quotes\n\
";
  argv[5] = (char*)"-n";
  EXPECT_EQ(1, RunProjMgr(6, argv, m_envp));
  errStr = streamRedirect.GetErrorString();
  EXPECT_EQ(errStr, expected);
}

TEST_F(ProjMgrUnitTests, ComponentVersions) {
  char* argv[5];
  const string& csolution = testinput_folder + "/TestSolution/ComponentSources/versions.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"-o";
  argv[4] = (char*)testoutput_folder.c_str();
  EXPECT_EQ(0, RunProjMgr(5, argv, m_envp));

  ProjMgrTestEnv::CompareFile(testoutput_folder + "/out/versions/RteTest_ARMCM3/Debug/versions.Debug+RteTest_ARMCM3.cbuild.yml",
    testinput_folder + "/TestSolution/ComponentSources/ref/versions.Debug+RteTest_ARMCM3.cbuild.yml");
}

TEST_F(ProjMgrUnitTests, ListTargetSets) {
  char* argv[6];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestTargetSet/solution.csolution.yml";
  argv[1] = (char*)"list";
  argv[2] = (char*)"target-sets";
  argv[3] = (char*)csolution.c_str();
  EXPECT_EQ(0, RunProjMgr(4, argv, 0));

  auto outStr = streamRedirect.GetOutString();
  EXPECT_STREQ(outStr.c_str(), "Type1\nType1@Custom2\nType1@Custom3\nType2@Default2\n");

  streamRedirect.ClearStringStreams();
  argv[4] = (char*)"--filter";
  argv[5] = (char*)"TYPE2";
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));

  outStr = streamRedirect.GetOutString();
  EXPECT_STREQ(outStr.c_str(), "Type2@Default2\n");

  streamRedirect.ClearStringStreams();
  argv[4] = (char*)"--filter";
  argv[5] = (char*)"Unknown";
  EXPECT_EQ(1, RunProjMgr(6, argv, 0));

  auto errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(errStr.find("no target-set was found with filter 'Unknown'") != string::npos);
}

TEST_F(ProjMgrUnitTests, ListTargetSetsImageOnly) {
  char* argv[6];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/ImageOnly/image-only.csolution.yml";
  argv[1] = (char*)"list";
  argv[2] = (char*)"target-sets";
  argv[3] = (char*)csolution.c_str();
  EXPECT_EQ(0, RunProjMgr(4, argv, 0));

  auto outStr = streamRedirect.GetOutString();
  EXPECT_STREQ(outStr.c_str(), "CM0\n");
}

TEST_F(ProjMgrUnitTests, ListExamples) {
  char* argv[9];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/Examples/solution.csolution.yml";

  // test with board
  argv[1] = (char*)"list";
  argv[2] = (char*)"examples";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"--active";
  argv[5] = (char*)"TestBoard";
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));
  auto outStr = streamRedirect.GetOutString();
  EXPECT_STREQ(outStr.c_str(), "\
PreInclude@1.0.0 (ARM::RteTest@0.1.0)\n\
PreIncludeEnvFolder@1.0.0 (ARM::RteTest@0.1.0)\n\
");

  // test with verbose flag
  streamRedirect.ClearStringStreams();
  argv[6] = (char*)"--verbose";
  EXPECT_EQ(0, RunProjMgr(7, argv, 0));
  outStr = streamRedirect.GetOutString();
  EXPECT_TRUE(regex_search(outStr, regex("\
PreInclude@1.0.0 \\(ARM::RteTest@0.1.0\\)\n\
  description: PreInclude Test Application\n\
  doc: .*/ARM/RteTest/0.1.0/Examples/PreInclude/Abstract.txt\n\
  environment: uv\n\
    load: .*/ARM/RteTest/0.1.0/Examples/PreInclude/PreInclude.uvprojx\n\
    folder: .*/ARM/RteTest/0.1.0/Examples/PreInclude\n\
  boards:\n\
    Keil::RteTest Dummy board\n\
PreIncludeEnvFolder@1.0.0 \\(ARM::RteTest@0.1.0\\)\n\
  description: PreInclude Test Application with different folder description\n\
  doc: .*/ARM/RteTest/0.1.0/Examples/PreInclude/Abstract.txt\n\
  environment: uv\n\
    load: .*/ARM/RteTest/0.1.0/Examples/PreInclude.uvprojx\n\
    folder: .*/ARM/RteTest/0.1.0/Examples/PreInclude\n\
  boards:\n\
    Keil::RteTest Dummy board\n\
")));

  // test with compatible device
  streamRedirect.ClearStringStreams();
  argv[5] = (char*)"CM0_Dual";
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));
  outStr = streamRedirect.GetOutString();
  EXPECT_STREQ(outStr.c_str(), "\
PreInclude@1.0.0 (ARM::RteTest@0.1.0)\n\
PreIncludeEnvFolder@1.0.0 (ARM::RteTest@0.1.0)\n\
");

  // test with filter option
  streamRedirect.ClearStringStreams();
  argv[6] = (char*)"--filter";
  argv[7] = (char*)"ENVFOLDER";
  EXPECT_EQ(0, RunProjMgr(8, argv, 0));
  outStr = streamRedirect.GetOutString();
  EXPECT_STREQ(outStr.c_str(), "\
PreIncludeEnvFolder@1.0.0 (ARM::RteTest@0.1.0)\n\
");

  // test with filter option (description)
  streamRedirect.ClearStringStreams();
  argv[7] = (char*)"different folder description";
  argv[8] = (char*)"--verbose";
  EXPECT_EQ(0, RunProjMgr(9, argv, 0));
  outStr = streamRedirect.GetOutString();
  EXPECT_TRUE(regex_search(outStr, regex("\
PreIncludeEnvFolder@1.0.0 \\(ARM::RteTest@0.1.0\\)\n\
  description: PreInclude Test Application with different folder description\n\
")));

  // test with non-compatible device
  streamRedirect.ClearStringStreams();
  argv[5] = (char*)"CM0";
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));
  outStr = streamRedirect.GetOutString();
  EXPECT_TRUE(outStr.empty());
}


TEST_F(ProjMgrUnitTests, ListTemplates) {
  char* argv[7];
  StdStreamRedirect streamRedirect;

  // list all templates
  argv[1] = (char*)"list";
  argv[2] = (char*)"templates";
  EXPECT_EQ(0, RunProjMgr(3, argv, 0));
  auto outStr = streamRedirect.GetOutString();
  EXPECT_STREQ(outStr.c_str(), "\
Board1Template (ARM::RteTest_DFP@0.2.0)\n\
Board2 (ARM::RteTest_DFP@0.2.0)\n\
Board3 (ARM::RteTest_DFP@0.2.0)\n\
");

  // test filter
  argv[3] = (char*)"--filter";
  argv[4] = (char*)"BOARD1";
  streamRedirect.ClearStringStreams();
  EXPECT_EQ(0, RunProjMgr(5, argv, 0));
  outStr = streamRedirect.GetOutString();
  EXPECT_STREQ(outStr.c_str(), "\
Board1Template (ARM::RteTest_DFP@0.2.0)\n\
");

  // test filter (description)
  argv[4] = (char*)"Template one";
  argv[5] = (char*)"--verbose";
  streamRedirect.ClearStringStreams();
  EXPECT_EQ(0, RunProjMgr(6, argv, 0));
  outStr = streamRedirect.GetOutString();
  EXPECT_TRUE(regex_search(outStr, regex("\
Board1Template \\(ARM::RteTest_DFP@0.2.0\\)\n\
  description: \"Test board Template one\"\n\
")));

  // list board's compatible template
  const string& csolution = testinput_folder + "/Examples/solution.csolution.yml";
  const string expected =
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"--active";
  argv[5] = (char*)"TestBoard";
  argv[6] = (char*)"--verbose";
  streamRedirect.ClearStringStreams();
  EXPECT_EQ(0, RunProjMgr(7, argv, 0));
  outStr = streamRedirect.GetOutString();
  EXPECT_TRUE(regex_search(outStr, regex("\
Board3 \\(ARM::RteTest_DFP@0.2.0\\)\n\
  description: \"Test board Template three\"\n\
  path: .*/ARM/RteTest_DFP/0.2.0/Templates\n\
  file: .*/ARM/RteTest_DFP/0.2.0/Templates/board3.csolution.yml\n\
  copy-to: Template3\n\
")));
}

TEST_F(ProjMgrUnitTests, ConvertActiveTargetSet) {
  char* argv[6];
  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestTargetSet/solution.csolution.yml";
  argv[1] = (char*)csolution.c_str();
  argv[2] = (char*)"convert";
  argv[3] = (char*)"--active";
  argv[4] = (char*)"Type1@Custom2";
  EXPECT_EQ(0, RunProjMgr(5, argv, 0));
  const YAML::Node& cbuildRun1 = YAML::LoadFile(testinput_folder + "/TestTargetSet/out/solution+Type1.cbuild-run.yml");
  EXPECT_EQ("Custom2", cbuildRun1["cbuild-run"]["target-set"].as<string>());

  argv[4] = (char*)"Type1";
  EXPECT_EQ(0, RunProjMgr(5, argv, 0));
  const YAML::Node& cbuildRun2 = YAML::LoadFile(testinput_folder + "/TestTargetSet/out/solution+Type1.cbuild-run.yml");
  EXPECT_EQ("<default>", cbuildRun2["cbuild-run"]["target-set"].as<string>());

  argv[4] = (char*)"";
  EXPECT_EQ(0, RunProjMgr(5, argv, 0));
  const YAML::Node& cbuildRun3 = YAML::LoadFile(testinput_folder + "/TestTargetSet/out/solution+Type1.cbuild-run.yml");
  EXPECT_EQ("Type1", cbuildRun3["cbuild-run"]["target-type"].as<string>());
  EXPECT_EQ("<default>", cbuildRun3["cbuild-run"]["target-set"].as<string>());

  argv[4] = (char*)"Type2";
  EXPECT_EQ(0, RunProjMgr(5, argv, 0));
  const YAML::Node& cbuildRun4 = YAML::LoadFile(testinput_folder + "/TestTargetSet/out/solution+Type2.cbuild-run.yml");
  EXPECT_EQ("Type2", cbuildRun4["cbuild-run"]["target-type"].as<string>());
  EXPECT_EQ("Default2", cbuildRun4["cbuild-run"]["target-set"].as<string>());

  streamRedirect.ClearStringStreams();
  argv[4] = (char*)"Type1@Unknown";
  EXPECT_EQ(1, RunProjMgr(5, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_STREQ(errStr.c_str(), "error csolution: 'Type1@Unknown' is not selectable as active target-set\n");

  streamRedirect.ClearStringStreams();
  argv[4] = (char*)"TypeUnknown";
  EXPECT_EQ(1, RunProjMgr(5, argv, 0));
  errStr = streamRedirect.GetErrorString();
  EXPECT_STREQ(errStr.c_str(), "error csolution: 'TypeUnknown' is not selectable as active target-set\n");

  streamRedirect.ClearStringStreams();
  argv[4] = (char*)"Type1";
  argv[5] = (char*)"--context-set";
  EXPECT_EQ(1, RunProjMgr(6, argv, 0));
  errStr = streamRedirect.GetErrorString();
  EXPECT_STREQ(errStr.c_str(), "error csolution: invalid arguments: '-a' option cannot be used in combination with '-S'\n");

  streamRedirect.ClearStringStreams();
  argv[4] = (char*)"Type1@Custom3";
  EXPECT_EQ(1, RunProjMgr(5, argv, 0));
  errStr = streamRedirect.GetErrorString();
  EXPECT_STREQ(errStr.c_str(), "error csolution: unknown selected context(s):\n  UnknownContext+Type1\n");
}

TEST_F(ProjMgrUnitTests, LinkTimeOptimize) {
  char* argv[3];
  const string& csolution = testinput_folder + "/TestLTO/solution.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  EXPECT_EQ(0, RunProjMgr(3, argv, m_envp));
  const YAML::Node& cbuild = YAML::LoadFile(testinput_folder + "/TestLTO/out/project/CM0/project+CM0.cbuild.yml");
  EXPECT_TRUE(cbuild["build"]["link-time-optimize"].IsDefined());
  EXPECT_TRUE(cbuild["build"]["components"][0]["link-time-optimize"].IsDefined());
  EXPECT_TRUE(cbuild["build"]["groups"][0]["files"][0]["link-time-optimize"].IsDefined());
}

TEST_F(ProjMgrUnitTests, LinkWholeArchive) {
  char* argv[3];
  const string& csolution = testinput_folder + "/TestLinkLib/solution.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  EXPECT_EQ(0, RunProjMgr(3, argv, m_envp));
  const YAML::Node& cbuild = YAML::LoadFile(testinput_folder + "/TestLinkLib/out/project/CM0/project+CM0.cbuild.yml");
  EXPECT_EQ("library", cbuild["build"]["groups"][0]["files"][0]["category"].as<string>());
  EXPECT_EQ("whole-archive", cbuild["build"]["groups"][0]["files"][0]["link"].as<string>());
}

TEST_F(ProjMgrUnitTests, ImageOnly) {
  char* argv[5];
  const string& csolution = testinput_folder + "/ImageOnly/image-only.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"--active";
  argv[4] = (char*)"CM0";
  EXPECT_EQ(0, RunProjMgr(5, argv, m_envp));

  ProjMgrTestEnv::CompareFile(testinput_folder + "/ImageOnly/image-only.cbuild-idx.yml",
    testinput_folder + "/ImageOnly/ref/image-only.cbuild-idx.yml");
  ProjMgrTestEnv::CompareFile(testinput_folder + "/ImageOnly/out/image-only/CM0/image-only+CM0.cbuild.yml",
    testinput_folder + "/ImageOnly/ref/image-only+CM0.cbuild.yml");
  ProjMgrTestEnv::CompareFile(testinput_folder + "/ImageOnly/out/image-only+CM0.cbuild-run.yml",
    testinput_folder + "/ImageOnly/ref/image-only+CM0.cbuild-run.yml");
}

TEST_F(ProjMgrUnitTests, ImageOnlyMulticore) {
  char* argv[5];
  const string& csolution = testinput_folder + "/ImageOnly/image-only-multicore.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"--active";
  argv[4] = (char*)"CM0";
  EXPECT_EQ(0, RunProjMgr(5, argv, m_envp));

  const YAML::Node& cbuildRun = YAML::LoadFile(testinput_folder + "/ImageOnly/out/image-only-multicore+CM0.cbuild-run.yml");
  EXPECT_EQ("cm0_core0", cbuildRun["cbuild-run"]["output"][0]["pname"].as<string>());
  EXPECT_EQ("cm0_core1", cbuildRun["cbuild-run"]["output"][1]["pname"].as<string>());
}

TEST_F(ProjMgrUnitTests, ListDebuggers) {
  char* argv[6];
  StdStreamRedirect streamRedirect;
  argv[1] = (char*)"list";
  argv[2] = (char*)"debuggers";
  EXPECT_EQ(0, RunProjMgr(3, argv, m_envp));
  auto outStr = streamRedirect.GetOutString();
  EXPECT_TRUE(regex_search(outStr, regex("\
CMSIS-DAP@pyOCD\n\
ULINKplus@pyOCD\n\
MCU-Link@pyOCD\n\
Nu-Link@pyOCD\n\
PICkit@pyOCD\n\
KitProg3@pyOCD\n\
RPiDebugProbe@pyOCD\n\
ST-Link@pyOCD\n\
J-Link Server\n\
CMSIS-DAP@Arm-Debugger\n\
ST-Link@Arm-Debugger\n\
Arm-FVP\n\
Keil uVision\n\
")));

  streamRedirect.ClearStringStreams();
  argv[3] = (char*)"--verbose";
  argv[4] = (char*)"--filter";
  argv[5] = (char*)"Cmsis-Dap";
  EXPECT_EQ(0, RunProjMgr(6, argv, m_envp));
  outStr = streamRedirect.GetOutString();
  EXPECT_TRUE(regex_search(outStr, regex("\
CMSIS-DAP@pyOCD\n\
  CMSIS-DAP\n\
  DAP-Link\n\
CMSIS-DAP@Arm-Debugger\n\
  CMSIS-DAP@armdbg\n\
")));
}

TEST_F(ProjMgrUnitTests, WestSupport) {
  char* argv[5];
  const string& csolution = testinput_folder + "/WestSupport/solution.csolution.yml";
  argv[1] = (char*)"convert";
  argv[2] = (char*)csolution.c_str();
  argv[3] = (char*)"--active";
  argv[4] = (char*)"CM0";
  EXPECT_EQ(0, RunProjMgr(5, argv, m_envp));

  ProjMgrTestEnv::CompareFile(testinput_folder + "/WestSupport/solution.cbuild-idx.yml",
    testinput_folder + "/WestSupport/ref/solution.cbuild-idx.yml");
  ProjMgrTestEnv::CompareFile(testinput_folder + "/WestSupport/out/solution+CM0.cbuild-run.yml",
    testinput_folder + "/WestSupport/ref/solution+CM0.cbuild-run.yml");
  ProjMgrTestEnv::CompareFile(testinput_folder + "/WestSupport/out/core0/CM0/Debug/core0.Debug+CM0.cbuild.yml",
    testinput_folder + "/WestSupport/ref/core0.Debug+CM0.cbuild.yml");
  ProjMgrTestEnv::CompareFile(testinput_folder + "/WestSupport/out/core1/CM0/Debug/core1.Debug+CM0.cbuild.yml",
    testinput_folder + "/WestSupport/ref/core1.Debug+CM0.cbuild.yml");
}

