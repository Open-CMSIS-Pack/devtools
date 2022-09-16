/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SvdConvIntegTestEnv_H
#define SvdConvIntegTestEnv_H

#include "RteFsUtils.h"

#include "gtest/gtest.h"

/**
 * @brief global test environment for all the test suites
*/
class SvdConvIntegTestEnv : public ::testing::Environment {
public:
  void SetUp()    override;
  void TearDown() override;

  static std::string localtestdata_dir;
  static std::string testoutput_dir;
};

#endif // SvdConvIntegTestEnv_H
