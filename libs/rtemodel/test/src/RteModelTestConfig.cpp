/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "RteModelTestConfig.h"

const std::string RteModelTestConfig::CMSIS_PACK_ROOT = std::string(GLOBAL_TEST_DIR) + std::string("/packs");
const std::string RteModelTestConfig::LOCAL_REPO_DIR = std::string(GLOBAL_TEST_DIR) + std::string("/local");
const std::string RteModelTestConfig::PROJECTS_DIR = std::string(GLOBAL_TEST_DIR) + std::string("/projects");
const std::string RteModelTestConfig::M3_CPRJ = PROJECTS_DIR + std::string("/RteTestM3/RteTestM3.cprj");

RteModelTestConfig::RteModelTestConfig() {
  /* Reserved */
}
