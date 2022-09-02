/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROJMGRTESTENV_H
#define PROJMGRTESTENV_H

#include "gtest/gtest.h"

extern std::string testoutput_folder;
extern std::string testinput_folder;
extern std::string testcmsispack_folder;
extern std::string schema_folder;

/**
 * @brief direct console output to string
*/
class StdStreamRedirect {
public:
  StdStreamRedirect();
  ~StdStreamRedirect();

  std::string GetOutString();
  std::string GetErrorString();

private:
  std::stringstream m_outbuffer;
  std::stringstream m_cerrbuffer;
  std::streambuf*   m_stdoutStreamBuf;
  std::streambuf*   m_stdcerrStreamBuf;
};

/**
 * @brief global test environment for all the test suites
*/
class ProjMgrTestEnv : public ::testing::Environment {
public:
  void SetUp() override;
  void TearDown() override;
};

#endif  // PROJMGRTESTENV_H
