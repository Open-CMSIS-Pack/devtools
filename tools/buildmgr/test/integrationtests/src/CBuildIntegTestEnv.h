/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CBUILDINTEGTESTENV_H
#define CBUILDINTEGTESTENV_H

#include "gtest/gtest.h"

#include "CrossPlatformUtils.h"
#include "RteFsUtils.h"

#include <string>
#include <iostream>
#include <fstream>


#ifndef SH
#define SH "bash -c"
#endif

#ifndef PATH_MAX
#define PATH_MAX _MAX_PATH
#endif

#define CPINIT_RETRY_CNT 3
#define CPINIT_RETRY_PROG_DELAY 10

struct example_struct {
  std::string name;
  std::string target;
};

struct TestParam {
  std::string name;            /* Example Name */
  std::string targetArg;       /* Application args*/
  std::string options;         /* Options */
  std::string command;         /* Command */
  bool expect;                 /* Expected Test Results*/
};

extern std::string examples_folder;
extern std::string scripts_folder;
extern std::string testout_folder;
extern std::string testinput_folder;
extern std::string testdata_folder;

void RunScript(const std::string& script, std::string arg = "");

/**
 * @brief global test environment for all the test suites
*/
class CBuildIntegTestEnv : public ::testing::Environment {
public:
  void SetUp()    override;
  void TearDown() override;

  static std::string ci_installer_path;
  static std::string ac6_toolchain_path;
};

#endif  // CBUILDINTEGTESTENV_H
