/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "RteModelTestConfig.h"
#include "RteFsUtils.h"

const std::string RteModelTestConfig::CMSIS_PACK_ROOT = std::string(GLOBAL_TEST_DIR) + std::string("/packs");
const std::string RteModelTestConfig::LOCAL_REPO_DIR = std::string(GLOBAL_TEST_DIR) + std::string("/local");
const std::string RteModelTestConfig::PROJECTS_DIR = std::string(GLOBAL_TEST_DIR) + std::string("/projects");
const std::string RteModelTestConfig::M3_CPRJ = PROJECTS_DIR + std::string("/RteTestM3/RteTestM3.cprj");

// define project and header file names with relative paths
const std::string RteModelTestConfig::prjsDir = "RteModelTestProjects";
const std::string RteModelTestConfig::localRepoDir = "RteModelLocalRepo";
const std::string RteModelTestConfig::RteTestM3 = "/RteTestM3";
const std::string RteModelTestConfig::RteTestM3_cprj = prjsDir + RteTestM3 + "/RteTestM3.cprj";
const std::string RteModelTestConfig::RteTestM3_ConfigFolder_cprj = prjsDir + RteTestM3 + "/RteTestM3_ConfigFolder.cprj";
const std::string RteModelTestConfig::RteTestM3_PackPath_cprj = prjsDir + RteTestM3 + "/RteTestM3_PackPath.cprj";
const std::string RteModelTestConfig::RteTestM3_PackPath_MultiplePdscs_cprj = prjsDir + RteTestM3 + "/RteTestM3_PackPath_MultiplePdscs.cprj";
const std::string RteModelTestConfig::RteTestM3_PackPath_NoPdsc_cprj = prjsDir + RteTestM3 + "/RteTestM3_PackPath_NoPdsc.cprj";
const std::string RteModelTestConfig::RteTestM3_PackPath_Invalid_cprj = prjsDir + RteTestM3 + "/RteTestM3_PackPath_Invalid.cprj";
const std::string RteModelTestConfig::RteTestM3_PrjPackPath = prjsDir + RteTestM3 + "/packs";

const std::string RteModelTestConfig::RteTestM4 = "/RteTestM4";
const std::string RteModelTestConfig::RteTestM4_cprj = prjsDir + RteTestM4 + "/RteTestM4.cprj";
const std::string RteModelTestConfig::RteTestM4_CompDep_cprj = prjsDir + RteTestM4 + "/RteTestM4_CompDep.cprj";



RteModelTestConfig::RteModelTestConfig() {
  /* Reserved */
}

void RteModelTestConfig::SetUp()
{
  RteFsUtils::DeleteTree(prjsDir);
  RteFsUtils::CopyTree(RteModelTestConfig::PROJECTS_DIR, prjsDir);
  RteFsUtils::CopyTree(RteModelTestConfig::LOCAL_REPO_DIR, localRepoDir);
}

void RteModelTestConfig::TearDown()
{
  RteFsUtils::DeleteTree(prjsDir);
}
// end of RteModelTestConfig
