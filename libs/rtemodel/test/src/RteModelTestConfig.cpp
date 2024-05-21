/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "RteModelTestConfig.h"
#include "RteFsUtils.h"
#include "XmlFormatter.h"

#include <iostream>
#include <fstream>

using namespace std;

const std::string RteModelTestConfig::CMSIS_PACK_ROOT = std::string(GLOBAL_TEST_DIR) + std::string("/packs");
const std::string RteModelTestConfig::LOCAL_REPO_DIR = std::string(GLOBAL_TEST_DIR) + std::string("/local");
const std::string RteModelTestConfig::LOCAL_PACK_DIR = std::string(GLOBAL_TEST_DIR) + std::string("/local_packs");
const std::string RteModelTestConfig::PROJECTS_DIR = std::string(GLOBAL_TEST_DIR) + std::string("/projects");
const std::string RteModelTestConfig::M3_CPRJ = PROJECTS_DIR + std::string("/RteTestM3/RteTestM3.cprj");

// define project and header file names with relative paths
const std::string RteModelTestConfig::prjsDir = "RteModelTestProjects";
const std::string RteModelTestConfig::packsDir = "RteModelTestPacks";
const std::string RteModelTestConfig::localPacks = "local_packs";
const std::string RteModelTestConfig::RteTestM3 = "/RteTestM3";
const std::string RteModelTestConfig::RteTestM3_cprj = prjsDir + RteTestM3 + "/RteTestM3.cprj";
const std::string RteModelTestConfig::RteTestM3NoComponents_cprj = prjsDir + RteTestM3 + "/RteTestM3NoComponents.cprj";
const std::string RteModelTestConfig::RteTestM3PackReq_cprj = prjsDir + RteTestM3 + "/RteTestM3_PackReq.cprj";
const std::string RteModelTestConfig::RteTestM3_ConfigFolder_cprj = prjsDir + RteTestM3 + "/RteTestM3_ConfigFolder.cprj";
const std::string RteModelTestConfig::RteTestM3_PackPath_cprj = prjsDir + RteTestM3 + "/RteTestM3_PackPath.cprj";
const std::string RteModelTestConfig::RteTestM3_PackPath_MultiplePdscs_cprj = prjsDir + RteTestM3 + "/RteTestM3_PackPath_MultiplePdscs.cprj";
const std::string RteModelTestConfig::RteTestM3_PackPath_NoPdsc_cprj = prjsDir + RteTestM3 + "/RteTestM3_PackPath_NoPdsc.cprj";
const std::string RteModelTestConfig::RteTestM3_PackPath_Invalid_cprj = prjsDir + RteTestM3 + "/RteTestM3_PackPath_Invalid.cprj";
const std::string RteModelTestConfig::RteTestM3_PrjPackPath = prjsDir + RteTestM3 + "/packs";
const std::string RteModelTestConfig::RteTestM3_UpdateHeader_cprj = prjsDir + RteTestM3 + "/RteTestM3_Rte_Update_Header.cprj";

const std::string RteModelTestConfig::RteTestM4 = "/RteTestM4";
const std::string RteModelTestConfig::RteTestM4_cprj = prjsDir + RteTestM4 + "/RteTestM4.cprj";
const std::string RteModelTestConfig::RteTestM4_Board_cprj = prjsDir + RteTestM4 + "/RteTestM4_Board.cprj";
const std::string RteModelTestConfig::RteTestM4_CompDep_cprj = prjsDir + RteTestM4 + "/RteTestM4_CompDep.cprj";



RteModelTestConfig::RteModelTestConfig() {
  /* Reserved */
}

void RteModelTestConfig::SetUp()
{
  RteFsUtils::DeleteTree(prjsDir);
  RteFsUtils::CopyTree(RteModelTestConfig::PROJECTS_DIR, prjsDir);
  RteFsUtils::CopyTree(RteModelTestConfig::CMSIS_PACK_ROOT, packsDir);
  RteFsUtils::CopyTree(RteModelTestConfig::LOCAL_PACK_DIR, localPacks);
}

void RteModelTestConfig::TearDown()
{
  RteFsUtils::DeleteTree(prjsDir);
  RteFsUtils::DeleteTree(packsDir);
  RteFsUtils::DeleteTree(localPacks);
}

void RteModelTestConfig::compareFile(const string& newFile, const string& refFile,
  const std::unordered_map<string, string>& expectedChangedFlags, const string& toolchain) const {
  ifstream streamNewFile, streamRefFile;
  string newLine, refLine;

  streamNewFile.open(newFile);
  EXPECT_TRUE(streamNewFile.is_open());

  streamRefFile.open(refFile);
  EXPECT_TRUE(streamRefFile.is_open());

  auto nextline = [](ifstream& f, string& line, bool wait)
  {
    if (wait)
      return true;
    line.clear();
    while (getline(f, line)) {
      if (!line.empty())
        return true;
    }
    return false;
  };

  auto compareLine = [](const string& key, const string& flags, const string& line)
  {
    string res;
    size_t pos, pos2 = string::npos;
    if (line.find(key) != string::npos) {
      pos = line.find("\"");
      if (pos != string::npos)
        pos2 = line.find("\"", pos + 1);
      if (pos != string::npos && pos2 != string::npos) {
        res = line;
        string convFlags = XmlFormatter::ConvertSpecialChars(flags);
        res.replace(pos + 1, pos2 - pos - 1, convFlags);
        EXPECT_EQ(res.compare(line), 0);
      }
    }
  };

  auto getTag = [&toolchain](const string& line) {
    string compiler = "compiler=\"" + toolchain + "\"";
    if (line.find(compiler) != string::npos) {
      size_t pos1 = line.find("<");
      if (pos1 != string::npos) {
        size_t pos2 = line.find(" ", pos1);
        if (pos2 != string::npos)
          return line.substr(pos1, pos2 - pos1);
      }
    }
    return RteUtils::EMPTY_STRING;
  };

  // compare lines of files
  bool wait = false;
  bool diff = false;
  while (nextline(streamNewFile, newLine, false) && nextline(streamRefFile, refLine, wait)) {
    if (newLine != refLine) {
      auto iter = expectedChangedFlags.find(getTag(newLine));
      if (iter != expectedChangedFlags.end()) {
        compareLine(iter->first, iter->second, newLine);
        if (!wait) {
          iter = expectedChangedFlags.find(getTag(refLine));
          if (iter == expectedChangedFlags.end())
            wait = true;      // wait until checking buildflags in updated file is done
        }
      } else {
        diff = true;
        break;
      }
    } else {
      wait = false;
    }
  }

  streamNewFile.close();
  streamRefFile.close();
  if (diff) {
    FAIL() << newFile << " is different from " << refFile;
  }
}

// end of RteModelTestConfig
