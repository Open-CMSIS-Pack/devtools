/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CBUILD_UNITTESTS_COMMON_H
#define CBUILD_UNITTESTS_COMMON_H

#include "Cbuild.h"
#include "CbuildCallback.h"
#include "CbuildKernel.h"
#include "CbuildLayer.h"
#include "CbuildModel.h"
#include "CbuildProject.h"
#include "CbuildUtils.h"

#include "ErrLog.h"
#include "BuildSystemGenerator.h"
#include "XmlFormatter.h"
#include "RteFsUtils.h"

#include "gtest/gtest.h"

#include <iostream>
#include <fstream>
#include <algorithm>

#ifndef PATH_MAX
#define PATH_MAX _MAX_PATH
#endif

struct TestParam {
  std::string name;            /* Example Name */
  std::string targetArg;       /* Application args*/
  std::string options;         /* Options */
  std::string command;         /* Command */
  bool expect;                 /* Expected Test Results*/
};

extern std::string testout_folder;
extern std::string testinput_folder;
extern std::string examples_folder;

void RemoveDir(fs::path path);

/**
 * @brief global test environment for all the test suites
*/
class CBuildUnitTestEnv : public ::testing::Environment {
public:
  void SetUp()    override;
  void TearDown() override;

  static const std::string workingDir;
};

#endif
