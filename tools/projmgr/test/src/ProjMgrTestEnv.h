/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROJMGRTESTENV_H
#define PROJMGRTESTENV_H

#include "gtest/gtest.h"

#include "RtePackage.h"

#include <list>

extern std::string testoutput_folder;
extern std::string testinput_folder;
extern std::string testcmsispack_folder;
extern std::string testcmsiscompiler_folder;
extern std::string schema_folder;
extern std::string etc_folder;
extern std::string bin_folder;
/**
 * @brief direct console output to string
*/
class StdStreamRedirect {
public:
  StdStreamRedirect();
  ~StdStreamRedirect();

  std::string GetOutString();
  std::string GetErrorString();
  void ClearStringStreams();

private:
  std::stringstream m_outbuffer;
  std::stringstream m_cerrbuffer;
  std::streambuf*   m_stdoutStreamBuf;
  std::streambuf*   m_stdcerrStreamBuf;
};

class TempSwitchCwd {
public:
  TempSwitchCwd(const std::string &path);
  ~TempSwitchCwd();
private:
  std::string m_oldPath;
};

typedef std::string (*LineReplaceFunc_t)(const std::string&);

/**
 * @brief global test environment for all the test suites
*/
class ProjMgrTestEnv : public ::testing::Environment {
public:
  void SetUp() override;
  void TearDown() override;
  static void CompareFile(const std::string& file1, const std::string& file2, LineReplaceFunc_t file2LineReplaceFunc = nullptr);
  static const std::string& GetCmsisPackRoot();
  static std::map<std::string, std::string, RtePackageComparator> GetEffectivePdscFiles(bool bLatestsOnly = false);
  static std::string GetFilteredPacksString(const std::map<std::string, std::string, RtePackageComparator>& pdscMap,
    const std::string& includeIds);
  static bool FilterId(const std::string& id, const std::string& includeIds);
  static bool IsFileInCbuildFilesList(const std::vector<std::map<std::string, std::string>> files, const std::string file);
  static int CountOccurrences(const std::string input, const std::string substring);
};

#endif  // PROJMGRTESTENV_H
