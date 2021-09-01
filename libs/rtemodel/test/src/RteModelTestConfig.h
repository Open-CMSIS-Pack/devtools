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

#include <string>

class RteModelTestConfig
{
private:
  RteModelTestConfig();

public:
  static const std::string CMSIS_PACK_ROOT;
  static const std::string PROJECTS_DIR;
  static const std::string M3_CPRJ;
};

#endif // RteModelTestConfig_H
