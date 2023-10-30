/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef YML_SCHEMACHK_TEST_ENV_H
#define YML_SCHEMACHK_TEST_ENV_H

#include "gtest/gtest.h"

extern std::string testoutput_folder;
extern std::string testinput_folder;

/**
 * @brief global test environment for all the test suites
*/
class YmlSchemaChkTestEnv : public ::testing::Environment {
public:
  void SetUp()    override;
  void TearDown() override;
};
#endif // YML_SCHEMACHK_TEST_ENV_H
