/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CBuildIntegTestEnv.h"

#include <chrono>
#include <thread>

using namespace std;

class CPInitTests : public ::testing::Test {
protected:
  void RunCPInitScript(const TestParam& param, bool env = true);
  void CheckPackDir   (const TestParam& param);
};

void CPInitTests::RunCPInitScript(const TestParam& param, bool env) {
  string cmd;
  error_code ec;

  ASSERT_EQ(true, fs::exists(testout_folder + "/cbuild/bin/cp_init.sh", ec))
    << "error: cp_init.sh not found";

  if (!env) {
    cmd = "cd " + scripts_folder + " && " +
      SH + " \"source ./unsetenv && " + testout_folder + "/cbuild/bin/cp_init.sh " +
      param.targetArg + (param.command.empty() ? "" : " " + param.command) +
      (param.options.empty() ? "" : " " + param.options) + "\"";
  }
  else {
    cmd = "bash -c \"source " + testout_folder +
      "/cbuild/etc/setup && cp_init.sh " + param.targetArg +
      (param.command.empty() ? "" : " " + param.command) +
      (param.options.empty() ? "" : " " + param.options) + "\"";
  }

  auto ret_val = system(cmd.c_str());

  if (param.expect && (ret_val != 0)) {
    // Retry with progressive delay interval
    int retries = CPINIT_RETRY_CNT;
    int delay = 0;
    do {
      delay += CPINIT_RETRY_PROG_DELAY;
      std::cout << "Waiting " << delay << " seconds before retrying...\n";
      this_thread::sleep_for(std::chrono::milliseconds(delay*1000));
      ret_val = system(cmd.c_str());
    } while ((--retries > 0) && (ret_val != 0));
  }

  ASSERT_EQ(param.expect, (ret_val == 0) ? true : false);
}

void CPInitTests::CheckPackDir(const TestParam& param) {
  error_code ec;
  string dirpath = fs::canonical(param.targetArg, ec).generic_string();

  EXPECT_EQ(param.expect, fs::exists(dirpath + "/.Download", ec))
    << "Folder " << dirpath << "/.Download does" << (param.expect ? "not " : "") << "exist!";
  EXPECT_EQ(param.expect, fs::is_empty(dirpath + "/.Download", ec))
    << "Folder " << dirpath << "/.Download is" << (param.expect ? "not " : "") << "empty!";
  EXPECT_EQ(param.expect, fs::exists(dirpath + "/.Web", ec))
    << "Folder " << dirpath << "/.Web does" << (param.expect ? "not " : "") << "exist!";
  EXPECT_EQ(param.expect, fs::exists(dirpath + "/.Web/index.pidx", ec))
    << "File "<<dirpath <<"/.Web/index.pidx does " << (param.expect ? "not " : "") << "exist!";
}

// Test cp_init with multiple arguments
TEST_F(CPInitTests, MultipleArgTest) {
  TestParam param { "", testout_folder + "/MultiArgRepo", "extraArgs", "", false };

  RunCPInitScript        (param);
}

// Test cp_init with no arguments
TEST_F(CPInitTests, NoArgTest) {
  TestParam param { "", "", "", "", false };

  RunCPInitScript        (param);
}

// Validate setting up pack directory when directory already exists
TEST_F(CPInitTests, RepoExistTest) {
  error_code ec;
  TestParam param{ "", testout_folder + "/InstallRepo", "", "", false };
  RemoveDir              (param.targetArg);
  fs::create_directories (param.targetArg, ec);

  RunCPInitScript        (param);
}

// Test setup pack directory
TEST_F(CPInitTests, ValidInstallTest) {
  TestParam param { "", testout_folder + "/packrepo", "", "", true };
  RemoveDir              (param.targetArg);

  RunCPInitScript        (param);
  CheckPackDir           (param);
}

// Validate pack environment setup without
// build environment and without argument passed
TEST_F(CPInitTests, NoEnvNoArgTest) {
  TestParam param{ "", "", "", "", false };

  RunCPInitScript        (param, false);
}

// Setup pack environment without build environment
// setup but the installation folder exist
TEST_F(CPInitTests, NoEnvRepoExistTest) {
  TestParam param{ "", testout_folder + "/packrepo", "", "", false };

  RunCPInitScript        (param, false);
}

// Setup pack directory without build environment setup
TEST_F(CPInitTests, NoEnvValidArgTest) {
  TestParam param{ "", testout_folder + "/packrepo", "", "", true };

  RemoveDir              (param.targetArg);
  RunCPInitScript        (param, false);
}
