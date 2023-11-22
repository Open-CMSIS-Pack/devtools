/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CBuildIntegTestEnv.h"

#include <chrono>
#include <thread>

using namespace std;

class CPackGetTests : public ::testing::Test {
protected:
  void RunPackAdd(const TestParam& param, bool env = true);
  void RunInit(const TestParam& param, bool env = true);
  void CheckPackDir(const string dirpath, const bool expect = true);
};

void RunCPackGet(string subcommand, const TestParam& param, bool env, int *rc) {
  string cmd;
  error_code ec;
  string cpackget = "cpackget";
  string precmd;

#ifdef _WIN32
  cpackget += ".exe";
#endif

  ASSERT_EQ(true, fs::exists(testout_folder + "/cbuild/bin/" + cpackget, ec)) << "error: " + cpackget  + " not found";

  precmd = "cd " + testdata_folder;
  precmd += " && ";
  precmd += SH " \"source " + (env ? testout_folder + "/cbuild/etc/setup" : scripts_folder + "/unsetenv");
  precmd += " && ";
  precmd += env ? "" : testout_folder + "/cbuild/bin/";
  cmd = precmd + cpackget + " -v " + subcommand + param.targetArg + param.options + "\"";

  *rc = system(cmd.c_str());
}

void CPackGetTests::RunPackAdd(const TestParam& param, bool env) {
  int ret_val;
  // "-a" means "agree with embedded license"
  // "-f" means "filename with pack list"
  RunCPackGet(" add -a -f ", param, env, &ret_val);
  ASSERT_EQ(param.expect, (ret_val == 0) ? true : false);
}

void CPackGetTests::RunInit(const TestParam& param, bool env) {
  string subcommand = " init https://www.keil.com/pack/index.pidx ";
  int ret_val;
  RunCPackGet(subcommand, param, env, &ret_val);

  if (param.expect && (ret_val != 0)) {
    // Retry with progressive delay interval
    int retries = CPINIT_RETRY_CNT;
    int delay = 0;
    do {
      delay += CPINIT_RETRY_PROG_DELAY;
      std::cout << "Waiting " << delay << " seconds before retrying...\n";
      this_thread::sleep_for(std::chrono::milliseconds(delay*1000));
      RunCPackGet(subcommand, param, env, &ret_val);
    } while ((--retries > 0) && (ret_val != 0));
  }

  ASSERT_EQ(param.expect, (ret_val == 0) ? true : false);
}

void CPackGetTests::CheckPackDir(const string dirpath, const bool expect) {
  error_code ec;

  EXPECT_EQ(expect, fs::exists(dirpath + "/.Download", ec))
    << "Folder " << dirpath << "/.Download does" << (expect ? "not " : "") << "exist!";
  EXPECT_EQ(expect, fs::is_empty(dirpath + "/.Download", ec))
    << "Folder " << dirpath << "/.Download is" << (expect ? "not " : "") << "empty!";
  EXPECT_EQ(expect, fs::exists(dirpath + "/.Web", ec))
    << "Folder " << dirpath << "/.Web does" << (expect ? "not " : "") << "exist!";
  EXPECT_EQ(expect, fs::exists(dirpath + "/.Web/index.pidx", ec))
    << "File "<<dirpath <<"/.Web/index.pidx does " << (expect ? "not " : "") << "exist!";
}

// Verify pack installer with no arguments
TEST_F(CPackGetTests, PackAddNoArgTest) {
  TestParam param = { "", "", "", "", false };

  RunPackAdd(param);
}

// Verify pack installer with multiple unknown arguments
TEST_F(CPackGetTests, PackAddExtraArgTest) {
  TestParam param = { "", "pack ExtraArgs", "", "", false };

  RunPackAdd(param);
}

// Verify pack installer with invalid arguments
TEST_F(CPackGetTests, PackAddInvalidArgTest) {
  TestParam param = { "", "packinstall", "", "", false };

  RunPackAdd(param);
}

// Verify pack installer with missing input packlist file
TEST_F(CPackGetTests, PackAddFileNotAvailableTest) {
  TestParam param = { "", "package.cpinstall", "", "", false };

  RunPackAdd(param);
}

// Verify pack installer with valid arguments
TEST_F(CPackGetTests, PackAddValidFileArgTest) {
  // Create a directory for this test only
  string localTestingDir = testout_folder + "/packrepo-valid-arg";
  TestParam paramInit = { "", "", " -R " + localTestingDir, "", true };
  RunInit(paramInit);

  TestParam param = { "", "Testpack.cpinstall", " -R " + localTestingDir, "", true };
  RunPackAdd(param);
  RteFsUtils::RemoveDir(localTestingDir);
}

// Test valid installation of packs
TEST_F(CPackGetTests, PackAddPackInstallationTest) {
  TestParam param = { "", "Testpack.cpinstall", "", "", true };

  RunScript("prepackinstall.sh", testout_folder);
  RunPackAdd(param);
  RunScript("postpackinstall.sh", testout_folder);

  // Try to re-install already installed pack
  // Since cpackget 0.4.0 re-installing an already installed pack does not raise error
  RunPackAdd(param);
}

// Install pack with missing build environment
TEST_F(CPackGetTests, PackAddNoEnvValidArgTest) {
  TestParam param = { "", "pack.cpinstall", "", "", true };

  RunPackAdd(param, false);
}

// Install pack with missing build environment and invalid argument
TEST_F(CPackGetTests, PackAddNoEnvInvalidArgTest) {
  TestParam param = { "", "InvalidArg", "", "", false };

  RunPackAdd(param, false);
}

// Verify pack installer with invalid packlist file
TEST_F(CPackGetTests, PackAddInvalidPackFileTest) {
  TestParam param = { "", "Invalid.cpinstall", "", "", false };

  RunPackAdd(param);
}

// Test when installer fails to download pack
TEST_F(CPackGetTests, PackAddDownloadFailTest) {
  TestParam param = { "", "local.cpinstall", "", "", false };

  RunPackAdd(param);
}

// Test cpackget init

// Test init with multiple arguments
TEST_F(CPackGetTests, InitMultipleArgTest) {
  TestParam param = { "", testout_folder + "/MultiArgRepo", "extraArgs", "", false };

  RunInit(param);
}

// Test init with no arguments
TEST_F(CPackGetTests, InitNoArgTest) {
  TestParam param = { "", "", "", "", true };

  RunInit(param);
}

// Validate setting up pack directory when directory already exists
TEST_F(CPackGetTests, InitRepoExistTest) {
  error_code ec;
  TestParam param = { "", testout_folder + "/InstallRepo", "", "", false };
  RteFsUtils::RemoveDir(param.targetArg);
  fs::create_directories(param.targetArg, ec);

  RunInit(param);
}

// Test setup pack directory
TEST_F(CPackGetTests, InitValidInstallTest) {
  string localTestingDir = testout_folder + "/packrepo";
  TestParam param = { "", "", " -R " + localTestingDir, "", true };

  RteFsUtils::RemoveDir(localTestingDir);
  RunInit(param);
  CheckPackDir(localTestingDir);
}

// Validate pack environment setup without
// build environment and without argument passed
TEST_F(CPackGetTests, InitNoEnvNoArgTest) {
  TestParam param = { "", "", "", "", true };

  RunInit(param, false);
}

// Setup pack environment without build environment
// setup but the installation folder exist
TEST_F(CPackGetTests, InitNoEnvRepoExistTest) {
  TestParam param = { "", testout_folder + "/packrepo", "", "", false };

  RunInit(param, false);
}

// Setup pack directory without build environment setup
TEST_F(CPackGetTests, InitNoEnvValidArgTest) {
  TestParam param = { "", "", " -R " + testout_folder + "/packrepo", "", true };

  RteFsUtils::RemoveDir(param.targetArg);
  RunInit(param, false);
}
