/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrTestEnv.h"
#include "RteKernelSlim.h"
#include "RteFsUtils.h"

#include "CrossPlatformUtils.h"

#include <fstream>
#include <sstream>

using namespace std;

string testinput_folder;
string testoutput_folder;
string testcmsispack_folder;
string testcmsiscompiler_folder;
string schema_folder;
string templates_folder;
string etc_folder;
string bin_folder;

StdStreamRedirect::StdStreamRedirect() :
  m_outbuffer(""), m_cerrbuffer(""),
  m_stdoutStreamBuf(std::cout.rdbuf(m_outbuffer.rdbuf())),
  m_stdcerrStreamBuf(std::cerr.rdbuf(m_cerrbuffer.rdbuf()))
{
}

std::string StdStreamRedirect::GetOutString() {
  return m_outbuffer.str();
}

std::string StdStreamRedirect::GetErrorString() {
  return m_cerrbuffer.str();
}

void StdStreamRedirect::ClearStringStreams() {
  m_outbuffer.str(string());
  m_cerrbuffer.str(string());
}

StdStreamRedirect::~StdStreamRedirect() {
  // reverse redirect
  std::cout.rdbuf(m_stdoutStreamBuf);
  std::cerr.rdbuf(m_stdcerrStreamBuf);
}

TempSwitchCwd::TempSwitchCwd(const string& path) {
  m_oldPath = RteFsUtils::GetCurrentFolder();
  RteFsUtils::SetCurrentFolder(path);
}

TempSwitchCwd::~TempSwitchCwd() {
  RteFsUtils::SetCurrentFolder(m_oldPath);
}

void ProjMgrTestEnv::SetUp() {
  error_code ec;
  schema_folder        = string(TEST_FOLDER) + "../schemas";
  templates_folder     = string(TEST_FOLDER) + "../templates";
  testinput_folder     = RteFsUtils::GetCurrentFolder() + "data";
  testoutput_folder    = RteFsUtils::GetCurrentFolder() + "output";
  testcmsispack_folder = string(CMAKE_SOURCE_DIR) + "test/packs";
  testcmsiscompiler_folder = testinput_folder + "/TestToolchains";
  etc_folder = string(PROJMGRUNITTESTS_BIN_PATH) + "/../etc";
  RteFsUtils::NormalizePath(etc_folder);
  bin_folder = string(PROJMGRUNITTESTS_BIN_PATH) + "/../bin";
  RteFsUtils::NormalizePath(bin_folder);

  if (RteFsUtils::Exists(testoutput_folder)) {
    RteFsUtils::RemoveDir(testoutput_folder);
  }
  RteFsUtils::CreateDirectories(testoutput_folder);
  std::string testdata_folder = string(TEST_FOLDER) + "data";
  testdata_folder   = fs::canonical(testdata_folder).generic_string();
  testoutput_folder = fs::canonical(testoutput_folder).generic_string();
  schema_folder     = fs::canonical(schema_folder).generic_string();
  ASSERT_FALSE(testdata_folder.empty());
  ASSERT_FALSE(testoutput_folder.empty());
  ASSERT_FALSE(schema_folder.empty());

  // copy schemas
  if (RteFsUtils::Exists(etc_folder)) {
    RteFsUtils::RemoveDir(etc_folder);
  }
  RteFsUtils::CreateDirectories(etc_folder);
  fs::copy(fs::path(schema_folder), fs::path(etc_folder), fs::copy_options::recursive, ec);

  //copy test input data
  if (RteFsUtils::Exists(testinput_folder)) {
    RteFsUtils::RemoveDir(testinput_folder);
  }
  RteFsUtils::CreateDirectories(testinput_folder);
  fs::copy(fs::path(testdata_folder), fs::path(testinput_folder), fs::copy_options::recursive, ec);

  if (RteFsUtils::Exists(bin_folder)) {
    RteFsUtils::RemoveDir(bin_folder);
  }
  RteFsUtils::CreateDirectories(bin_folder);
  // add dummy manifest file
  string manifestFile = string(PROJMGRUNITTESTS_BIN_PATH) + "/../manifest_0.0.0.yml";
  if (RteFsUtils::Exists(manifestFile)) {
    RteFsUtils::RemoveFile(manifestFile);
  }
  RteFsUtils::CreateTextFile(manifestFile, RteUtils::EMPTY_STRING);

  // copy local pack
  string srcPackPath, destPackPath;
  srcPackPath  = testcmsispack_folder + "/ARM/RteTest_DFP/0.2.0";
  destPackPath = testinput_folder + "/SolutionSpecificPack";
  if (RteFsUtils::Exists(destPackPath)) {
    RteFsUtils::RemoveDir(destPackPath);
  }
  RteFsUtils::CreateDirectories(destPackPath);
  fs::copy(fs::path(srcPackPath), fs::path(destPackPath), fs::copy_options::recursive, ec);
  srcPackPath  = testcmsispack_folder + "/ARM/RteTest/0.1.0";
  destPackPath = testinput_folder + "/SolutionSpecificPack2";
  if (RteFsUtils::Exists(destPackPath)) {
    RteFsUtils::RemoveDir(destPackPath);
  }
  RteFsUtils::CreateDirectories(destPackPath);
  fs::copy(fs::path(srcPackPath), fs::path(destPackPath), fs::copy_options::recursive, ec);

  // copy invalid packs
  string srcInvalidPacks, destInvalidPacks;
  srcInvalidPacks = testcmsispack_folder + "-invalid";
  destInvalidPacks = testinput_folder + "/InvalidPacks";
  if (RteFsUtils::Exists(destInvalidPacks)) {
    RteFsUtils::RemoveDir(destInvalidPacks);
  }
  RteFsUtils::CreateDirectories(destInvalidPacks);
  fs::copy(fs::path(srcInvalidPacks), fs::path(destInvalidPacks), fs::copy_options::recursive, ec);

  CrossPlatformUtils::SetEnv("CMSIS_PACK_ROOT", testcmsispack_folder);

  // create dummy cmsis compiler root
  RteFsUtils::CreateDirectories(testcmsiscompiler_folder);
  RteFsUtils::CreateTextFile(testcmsiscompiler_folder + "/AC6.6.18.0.cmake", "");
  RteFsUtils::CreateTextFile(testcmsiscompiler_folder + "/GCC.11.2.1.cmake", "");
  RteFsUtils::CreateTextFile(testcmsiscompiler_folder + "/IAR.8.50.6.cmake", "");
  CrossPlatformUtils::SetEnv("CMSIS_COMPILER_ROOT", testcmsiscompiler_folder);

  // copy linker script template files
  fs::copy(fs::path(templates_folder), fs::path(testcmsiscompiler_folder), fs::copy_options::recursive, ec);
}

void ProjMgrTestEnv::TearDown() {
  // Reserved
}

void ProjMgrTestEnv::CompareFile(const string& file1, const string& file2, LineReplaceFunc_t file2LineReplaceFunc) {
  ifstream f1, f2;
  string l1, l2;
  bool ret_val;

  f1.open(file1);
  ret_val = f1.is_open();
  ASSERT_EQ(ret_val, true) << "Failed to open " << file1;

  f2.open(file2);
  ret_val = f2.is_open();
  ASSERT_EQ(ret_val, true) << "Failed to open " << file2;

  while (true) {
    if (getline(f1, l1)) {
      if (!getline(f2, l2)) {
        FAIL() << "error: " << file1 << " is longer than " << file2 << "\nLine not in " << file2 << ":" << l1;
      }
    } else if (getline(f2, l2)) {
        FAIL() << "error: " << file1 << " is shorter than " << file2 << "\nLine not in " << file1 << ": " << l2;
    } else {
      break;
    }
    if (!l1.empty() && l1.rfind('\r') == l1.length() - 1) {
      l1.pop_back();
    }

    if (!l2.empty() && l2.rfind('\r') == l2.length() - 1) {
      l2.pop_back();
    }

    if (file2LineReplaceFunc) {
      l2 = file2LineReplaceFunc(l2);
    }

    if (l1 != l2) {
      // ignore 'timestamp'
      if ((!l1.empty() && (l1.find("timestamp=") != string::npos)) && (!l2.empty() && (l2.find("timestamp=") != string::npos))) {
        continue;
      }
      if ((!l1.empty() && (l1.find("generated-by") != string::npos)) && (!l2.empty() && (l2.find("generated-by") != string::npos))) {
        continue;
      }
      FAIL() << "error: " << file1 << " is different from " << file2 << "\nLine1: " << l1 << "\nLine2: " << l2;
    }
  }

  f1.close();
  f2.close();
}

const std::string& ProjMgrTestEnv::GetCmsisPackRoot()
{
  return testcmsispack_folder;
}

std::map<std::string, std::string, RtePackageComparator> ProjMgrTestEnv::GetEffectivePdscFiles( bool bLatestsOnly)
{
  std::map<std::string, std::string, RtePackageComparator> pdscMap;
  RteKernelSlim rteKernel;
  rteKernel.SetCmsisPackRoot(GetCmsisPackRoot());
  rteKernel.GetEffectivePdscFilesAsMap(pdscMap,  bLatestsOnly);
  return pdscMap;
}

std::string ProjMgrTestEnv::GetFilteredPacksString(const std::map<std::string, std::string, RtePackageComparator>& pdscMap,
  const std::string& includeIds)
{
  stringstream ss;
  for (auto& [id, f] : pdscMap) {
    if(FilterId(id, includeIds)) {
      ss << id << " (" << f << ")\n";
    }
  }
  return ss.str();
}

bool ProjMgrTestEnv::FilterId(const std::string& id, const std::string& includeIds)
{
  if (!includeIds.empty()) {
    list<string> segments;
    RteUtils::SplitString(segments, includeIds, ';');
    for (auto& s : segments) {
      if (WildCards::Match(id, s)) {
        return true;
      }
    }
    return false;
  }
  return true;
}

bool ProjMgrTestEnv::IsFileInCbuildFilesList(const std::vector<std::map<std::string, std::string>> files, const std::string file)
{
  for (const auto& entry : files) {
    for (const auto& [key, value] : entry) {
      if (key == "file" && value == file) {
        return true;
      }
    }
  }
  return false;
}

int ProjMgrTestEnv::CountOccurrences(const std::string input, const std::string substring)
{
  if (substring.empty()) {
    return 0; // Avoid infinite loop if substring is empty
  }
  int occurrences = 0;
  size_t pos = 0;
  while ((pos = input.find(substring, pos)) != std::string::npos) {
    occurrences++;
    pos += substring.length();
  }
  return occurrences;
}

int main(int argc, char **argv) {
  try {
    testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new ProjMgrTestEnv);
    return RUN_ALL_TESTS();
  }
  catch (testing::internal::GoogleTestFailureException const& e) {
    cout << "runtime_error: " << e.what();
    return 2;
  }
  catch (...) {
    cout << "non-standard exception";
    return 2;
  }
}
