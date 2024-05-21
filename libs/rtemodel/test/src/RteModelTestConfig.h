#ifndef RteModelTestConfig_H
#define RteModelTestConfig_H
/******************************************************************************/
/* RteModelTestConfig - Common Test Configurations */
/******************************************************************************/
/** @file RteModelTestConfig.h
  * @brief CMSIS environment configurations

*/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 /******************************************************************************/
#include "gtest/gtest.h"

#include <string>
#include <unordered_map>

class RteModelTestConfig : public ::testing::Test
{
public:
  RteModelTestConfig();

  void compareFile(const std::string& newFile, const std::string& refFile,
    const std::unordered_map<std::string, std::string>& expectedChangedFlags, const std::string& toolchain) const;

protected:
  void SetUp() override;
  void TearDown() override;

public:
  static const std::string CMSIS_PACK_ROOT;
  static const std::string LOCAL_REPO_DIR;
  static const std::string LOCAL_PACK_DIR;
  static const std::string PROJECTS_DIR;
  static const std::string M3_CPRJ;

  static const std::string prjsDir;
  static const std::string packsDir;
  static const std::string localPacks;
  static const std::string RteTestM3;
  static const std::string RteTestM3_cprj;
  static const std::string RteTestM3NoComponents_cprj;
  static const std::string RteTestM3PackReq_cprj;
  static const std::string RteTestM3_ConfigFolder_cprj;
  static const std::string RteTestM3_PackPath_cprj;
  static const std::string RteTestM3_PackPath_MultiplePdscs_cprj;
  static const std::string RteTestM3_PackPath_NoPdsc_cprj;
  static const std::string RteTestM3_PackPath_Invalid_cprj;
  static const std::string RteTestM3_PrjPackPath;
  static const std::string RteTestM3_UpdateHeader_cprj;

  static const std::string RteTestM4;
  static const std::string RteTestM4_cprj;
  static const std::string RteTestM4_Board_cprj;
  static const std::string RteTestM4_CompDep_cprj;
};

#endif // RteModelTestConfig_H
