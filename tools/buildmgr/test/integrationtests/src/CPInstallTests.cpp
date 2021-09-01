/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CBuildIntegTestEnv.h"

using namespace std;

class CPInstallTests : public ::testing::Test {
protected:
  void RunCPInstallScript(const TestParam& param, bool env = true);
};

void CPInstallTests::RunCPInstallScript(const TestParam& param, bool env) {
  string cmd;
  error_code ec;

  ASSERT_EQ(true, fs::exists(testout_folder + "/cbuild/bin/cp_install.sh", ec))
    << "error: cp_install.sh not found";

  if (!env) {
    cmd = "cd " + testdata_folder + " && "
      SH + " \"source " + testout_folder + "/unsetenv && " +
      testout_folder + "/cbuild/bin/cp_install.sh " + param.targetArg + "\"";
  }
  else {
    cmd = "cd " + testdata_folder + " && "
      SH + " \"source " + testout_folder +
      "/cbuild/etc/setup && cp_install.sh " + param.targetArg + "\"";
  }
  auto ret_val = system(cmd.c_str());
  ASSERT_EQ(param.expect, (ret_val == 0) ? true : false);
}

// Verify pack installer with no arguments
TEST_F(CPInstallTests, NoArgTest) {
  TestParam param = { "", "", "", "", false };

  RunCPInstallScript     (param);
}

// Verify pack installer with multiple unknown arguments
TEST_F(CPInstallTests, ExtraArgTest) {
  TestParam param = { "", "pack ExtraArgs", "", "", false };

  RunCPInstallScript     (param);
}

// Verify pack installer with invalid arguments
TEST_F(CPInstallTests, InvalidArgTest) {
  TestParam param = { "", "packinstall", "", "", false };

  RunCPInstallScript     (param);
}

// Verify pack installer with missing input packlist file
TEST_F(CPInstallTests, FileNotAvailableTest) {
  TestParam param = { "", "package.cpinstall", "", "", false };

  RunCPInstallScript     (param);
}

// Verify pack installer with missing input packlist file
TEST_F(CPInstallTests, ValidFileArgTest) {
  TestParam param = { "", "pack.cpinstall", "", "", true };

  RunCPInstallScript     (param);
}

// Test valid installtion of packs
TEST_F(CPInstallTests, PackInstallationTest) {
  TestParam param = { "", "Testpack.cpinstall", "", "", true };

  RunScript              ("prepackinstall.sh", testout_folder);
  RunCPInstallScript     (param);
  RunScript              ("postpackinstall.sh", testout_folder);
}

// Install pack with missing build environment
TEST_F(CPInstallTests, NoEnvValidArgTest) {
  TestParam param = { "", "pack.cpinstall", "", "", false };

  RunCPInstallScript     (param, false);
}

// Install pack with missing build environment and invalid argument
TEST_F(CPInstallTests, NoEnvInvalidArgTest) {
  TestParam param = { "", "InvalidArg", "", "", false };

  RunCPInstallScript     (param, false);
}

// Verify pack installer with invalid packlist file
TEST_F(CPInstallTests, InvalidPackFileTest) {
  TestParam param = { "", "Invalid.cpinstall", "", "", false };

  RunCPInstallScript     (param);
}

// Install already installed pack
TEST_F(CPInstallTests, AlreadyInstalledPackTest) {
  TestParam param = { "", "pack.cpinstall", "", "", true };

  RunCPInstallScript     (param);
}

// Test when installer fails to download pack
TEST_F(CPInstallTests, PackDownloadFailTest) {
  TestParam param = { "", "local.cpinstall", "", "", false };

  RunCPInstallScript     (param);
}
