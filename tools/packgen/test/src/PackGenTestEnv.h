/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PACKGENTESTENV_H
#define PACKGENTESTENV_H

#include "gtest/gtest.h"

extern std::string testoutput_folder;
extern std::string testinput_folder;

/**
 * @brief global test environment for all the test suites
*/
class PackGenTestEnv : public ::testing::Environment {
public:
  void SetUp()    override;
  void TearDown() override;
};

#endif  // PACKGENTESTENV_H
