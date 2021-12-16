/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gtest/gtest.h"

extern std::string testoutput_folder;
extern std::string testinput_folder;
extern std::string testcmsispack_folder;

/**
 * @brief global test environment for all the test suites
*/
class ProjMgrTestEnv : public ::testing::Environment {
public:
  void SetUp() override;
  void TearDown() override;
};
