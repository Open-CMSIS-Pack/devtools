/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgr.h"
#include "ProjMgrTestEnv.h"
#include "RteFsUtils.h"
#include "gtest/gtest.h"
#include <regex>
#include <filesystem>

using namespace std;

class ProjMgrGeneratorUnitTests : public ProjMgrGenerator, public ::testing::Test {
protected:
  ProjMgrGeneratorUnitTests() {}
  virtual ~ProjMgrGeneratorUnitTests() {}
};

TEST_F(ProjMgrGeneratorUnitTests, GetStringFromVector) {
  string expected = "Word1 Word2 Word3";
  const vector<string> vec = {
   "Word1", "Word2", "Word3",
  };
  EXPECT_EQ(expected, ProjMgrGenerator().GetStringFromVector(vec, " "));

  const vector<string> emptyVec;
  EXPECT_EQ("", ProjMgrGenerator().GetStringFromVector(emptyVec, " "));
}

TEST_F(ProjMgrGeneratorUnitTests, GetLocalTimestamp) {
  string timestamp = ProjMgrGenerator().GetLocalTimestamp();
  EXPECT_TRUE(regex_match(timestamp,
  std::regex("^([0-9]){4}(-([0-9]){2}){2}T([0-9]){2}(:([0-9]){2}){2}$")));
}

TEST_F(ProjMgrGeneratorUnitTests, EmptyCprjElements) {
  ContextItem context;
  CprojectItem cproject;
  context.cproject = &cproject;
  context.groups = { GroupNode() };
  const string cprj = testoutput_folder + "/empty.cprj";
  EXPECT_TRUE(ProjMgrGenerator().GenerateCprj(context, cprj, true));
  string content;
  EXPECT_TRUE(RteFsUtils::ReadFile(cprj, content));
  EXPECT_TRUE(content.find("component") == string::npos);
  EXPECT_TRUE(content.find("files") == string::npos);
  EXPECT_TRUE(content.find("group") == string::npos);
}

TEST_F(ProjMgrGeneratorUnitTests, GenDir) {
  char *argv[6], *envp[2];
  string gcc = "GCC_TOOLCHAIN_11_2_1=" + testinput_folder;
  envp[0] = (char*)gcc.c_str();
  envp[1] = (char*)'\0';

  const string& csolution = testinput_folder + "/TestSolution/gendir.csolution.yml";
  argv[1] = (char*)"run";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-g";
  argv[5] = (char*)"RteTestGeneratorIdentifier";

  EXPECT_EQ(0, ProjMgr::RunProjMgr(6, argv, envp));

  const string generatorInputFile = testinput_folder + "/TestSolution/tmp/TestProject3.Debug+TypeA.cbuild-gen.yml";
  const string generatedGPDSC = testinput_folder + "/TestSolution/TestProject3/gendir/RteTestGen_ARMCM0/RteTest.gpdsc";

  EXPECT_EQ(true, std::filesystem::exists(generatorInputFile));
  EXPECT_EQ(true, std::filesystem::exists(generatedGPDSC));
}

TEST_F(ProjMgrGeneratorUnitTests, GenFiles) {
  char *argv[6], *envp[2];
  string gcc = "GCC_TOOLCHAIN_11_2_1=" + testinput_folder;
  envp[0] = (char*)gcc.c_str();
  envp[1] = (char*)'\0';

  const string& csolution = testinput_folder + "/TestSolution/genfiles.csolution.yml";
  argv[1] = (char*)"run";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-g";
  argv[5] = (char*)"RteTestGeneratorIdentifier";

  EXPECT_EQ(0, ProjMgr::RunProjMgr(6, argv, envp));

  const string generatorInputFile = testinput_folder + "/TestSolution/tmp/TestProject3_1.Debug+TypeA.cbuild-gen.yml";
  const string generatedGPDSC = testinput_folder + "/TestSolution/TestProject3_1/gendir/RteTestGen_ARMCM0/RteTest.gpdsc";

  auto stripAbsoluteFunc = [](const std::string& in) {
    std::string str = in;
    RteUtils::ReplaceAll(str, testinput_folder, "${DEVTOOLS(data)}");
    RteUtils::ReplaceAll(str, testcmsispack_folder, "${DEVTOOLS(packs)}");
    return str;
  };

  ProjMgrTestEnv::CompareFile(testinput_folder + "/TestSolution/ref/TestProject3_1.Debug+TypeA.cbuild-gen.yml",  generatorInputFile, stripAbsoluteFunc);

  EXPECT_EQ(true, std::filesystem::exists(generatorInputFile));
  EXPECT_EQ(true, std::filesystem::exists(generatedGPDSC));
}

TEST_F(ProjMgrGeneratorUnitTests, FailCreatingDirectories) {
  char* argv[6];
  StdStreamRedirect streamRedirect;

  const string& root = testinput_folder + "/TestSolution";
  const string& csolution = root + "/test_fail_creating_directories.csolution.yml";
  argv[1] = (char*)"run";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-g";
  argv[5] = (char*)"RteTestGeneratorIdentifier";

  RteFsUtils::RemoveDir(root + "/tmp");
  // Create a file with same name as parent directory where cbuild-gen.yml will be attempted to be created
  EXPECT_TRUE(RteFsUtils::CreateTextFile(root + "/tmp", ""));
  EXPECT_EQ(1, ProjMgr::RunProjMgr(6, argv, 0));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find("destination directory cannot be created"));
  RteFsUtils::RemoveFile(root + "/tmp");
}

TEST_F(ProjMgrGeneratorUnitTests, NoExeFiles) {
  char* argv[6];

  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestSolution/gen_noexe.csolution.yml";
  argv[1] = (char*)"run";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-g";
  argv[5] = (char*)"RteTestGeneratorNoExe";

  // execution fails
  EXPECT_EQ(1, ProjMgr::RunProjMgr(6, argv, 0));

  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find("permissions"));
  // still cbuild.yaml file got created
  const string generatorInputFile = testinput_folder + "/TestSolution/tmp/TestProject3_2.Debug+TypeA.cbuild-gen.yml";
  EXPECT_TRUE(std::filesystem::exists(generatorInputFile));
  const string generatedGPDSC = testinput_folder + "/TestSolution/TestProject3_2/gendir/RteTestGen_ARMCM0/RteTest.gpdsc";
  // but not gpdsc
  EXPECT_FALSE(std::filesystem::exists(generatedGPDSC));
}

TEST_F(ProjMgrGeneratorUnitTests, DryRunIncapableGenerator) {
  char* argv[7];

  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestSolution/gen_nodryrun.csolution.yml";
  argv[1] = (char*)"run";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-g";
  argv[5] = (char*)"RteTestGeneratorNoDryRun";
  argv[6] = (char*)"--dry-run";

  EXPECT_NE(0, ProjMgr::RunProjMgr(7, argv, 0));
  auto outStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, outStr.find("is not dry-run capable"));
}

TEST_F(ProjMgrGeneratorUnitTests, DryRun) {
  char* argv[7], *envp[2];
  string gcc = "GCC_TOOLCHAIN_11_2_1=" + testinput_folder;
  envp[0] = (char*)gcc.c_str();
  envp[1] = (char*)'\0';

  StdStreamRedirect streamRedirect;
  const string& csolution = testinput_folder + "/TestSolution/genfiles.csolution.yml";
  argv[1] = (char*)"run";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-g";
  argv[5] = (char*)"RteTestGeneratorIdentifier";
  argv[6] = (char*)"--dry-run";

  const string generatorInputFile = testinput_folder + "/TestSolution/tmp/TestProject3_1.Debug+TypeA.cbuild-gen.yml";
  const string generatorDestination = testinput_folder + "/TestSolution/TestProject3_1/gendir";
  const string targetGPDSC = generatorDestination + "/RteTestGen_ARMCM0/RteTest.gpdsc";
  const string rteDir = testinput_folder + "/TestSolution/TestProject3_1/RTE";

  RteFsUtils::RemoveDir(generatorDestination);
  RteFsUtils::RemoveDir(rteDir);

  EXPECT_EQ(0, ProjMgr::RunProjMgr(7, argv, envp));

  auto stripAbsoluteFunc = [](const std::string& in) {
    std::string str = in;
    RteUtils::ReplaceAll(str, testinput_folder, "${DEVTOOLS(data)}");
    RteUtils::ReplaceAll(str, testcmsispack_folder, "${DEVTOOLS(packs)}");
    return str;
  };

  ProjMgrTestEnv::CompareFile(testinput_folder + "/TestSolution/ref/TestProject3_1.Debug+TypeA.cbuild-gen.yml",  generatorInputFile, stripAbsoluteFunc);

  EXPECT_EQ(true, std::filesystem::exists(generatorInputFile));
  EXPECT_EQ(false, std::filesystem::exists(rteDir));
  EXPECT_EQ(false, std::filesystem::exists(targetGPDSC));
  EXPECT_EQ(false, std::filesystem::exists(generatorDestination));

  // Expect that the GPDSC content was printed to stdout, enclosed within the begin and end marks
  auto outStr = streamRedirect.GetOutString();
  string beginGpdscMark = "-----BEGIN GPDSC-----\n";
  string endGpdscMark = "-----END GPDSC-----\n";
  auto beginGpdscMarkIndex = outStr.find(beginGpdscMark);
  auto endGpdscMarkIndex = outStr.find(endGpdscMark);
  EXPECT_NE(string::npos, beginGpdscMarkIndex);
  EXPECT_NE(string::npos, endGpdscMarkIndex);
  auto gpdscContentIndex = beginGpdscMarkIndex + beginGpdscMark.size();
  auto contentLength = endGpdscMarkIndex - gpdscContentIndex;
  string gpdscContent = outStr.substr(gpdscContentIndex, contentLength);

  // Check that the GPDSC content seems OK (the full reference GPDSC file is not easily available from the test for comparison)
  EXPECT_EQ(0, gpdscContent.find("<?xml"));
  EXPECT_NE(string::npos, gpdscContent.find("<component generator=\"RteTestGeneratorIdentifier\""));
}

TEST_F(ProjMgrGeneratorUnitTests, PdscAndGpdscWithSameComponentName) {
  char* argv[6], * envp[2];
  string gcc = "GCC_TOOLCHAIN_11_2_1=" + testinput_folder;
  envp[0] = (char*)gcc.c_str();
  envp[1] = (char*)'\0';

  const string& csolution = testinput_folder + "/TestSolution/genfiles.csolution.yml";
  argv[1] = (char*)"run";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();
  argv[4] = (char*)"-g";
  argv[5] = (char*)"RteTestGeneratorIdentifier";

  const string targetGPDSC = testinput_folder + "/TestSolution/TestProject3_1/gendir/RteTestGen_ARMCM0/RteTest.gpdsc";

  RteFsUtils::RemoveFile(targetGPDSC);

  EXPECT_EQ(0, ProjMgr::RunProjMgr(6, argv, envp));

  argv[1] = (char*)"convert";
  argv[2] = (char*)"--solution";
  argv[3] = (char*)csolution.c_str();

  EXPECT_EQ(0, ProjMgr::RunProjMgr(4, argv, envp));
}
