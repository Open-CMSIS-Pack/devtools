/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrTestEnv.h"
#include "RteKernel.h"
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

void ProjMgrTestEnv::SetUp() {
  error_code ec;
  schema_folder        = string(TEST_FOLDER) + "../schemas";
  templates_folder     = string(TEST_FOLDER) + "../templates";
  testinput_folder     = RteFsUtils::GetCurrentFolder() + "data";
  testoutput_folder    = RteFsUtils::GetCurrentFolder() + "output";
  testcmsispack_folder = string(CMAKE_SOURCE_DIR) + "test/packs";
  testcmsiscompiler_folder = testinput_folder + "/TestToolchains";
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
  std::string schemaDestDir = string(PROJMGRUNITTESTS_BIN_PATH) + "/../etc";
  if (RteFsUtils::Exists(schemaDestDir)) {
    RteFsUtils::RemoveDir(schemaDestDir);
  }
  RteFsUtils::CreateDirectories(schemaDestDir);
  fs::copy(fs::path(schema_folder), fs::path(schemaDestDir), fs::copy_options::recursive, ec);

  //copy test input data
  if (RteFsUtils::Exists(testinput_folder)) {
    RteFsUtils::RemoveDir(testinput_folder);
  }
  RteFsUtils::CreateDirectories(testinput_folder);
  fs::copy(fs::path(testdata_folder), fs::path(testinput_folder), fs::copy_options::recursive, ec);

  // copy local pack
  string srcPackPath, destPackPath;
  srcPackPath  = testcmsispack_folder + "/ARM/RteTest_DFP/0.2.0";
  destPackPath = testinput_folder + "/SolutionSpecificPack";
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
  RteFsUtils::CreateFile(testcmsiscompiler_folder + "/AC6.6.18.0.cmake", "");
  RteFsUtils::CreateFile(testcmsiscompiler_folder + "/GCC.11.2.1.cmake", "");
  RteFsUtils::CreateFile(testcmsiscompiler_folder + "/IAR.8.50.6.cmake", "");
  CrossPlatformUtils::SetEnv("CMSIS_COMPILER_ROOT", testcmsiscompiler_folder);

  // copy linker script template files
  fs::copy(fs::path(templates_folder), fs::path(testcmsiscompiler_folder), fs::copy_options::recursive, ec);
}

void ProjMgrTestEnv::TearDown() {
  // Reserved
}

void ProjMgrTestEnv::CompareFile(const string& file1, const string& file2) {
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

    if (l1 != l2) {
      // ignore 'timestamp'
      if ((!l1.empty() && (l1.find("timestamp=") != string::npos)) && (!l2.empty() && (l2.find("timestamp=") != string::npos))) {
        continue;
      }
      if ((!l1.empty() && (l1.find("generated-by") != string::npos)) && (!l2.empty() && (l2.find("generated-by") != string::npos))) {
        continue;
      }
      FAIL() << "error: " << file1 << " is different from " << file2;
    }
  }

  f1.close();
  f2.close();
}

const std::string& ProjMgrTestEnv::GetCmsisPackRoot()
{
  return testcmsispack_folder;
}

std::list<std::string> ProjMgrTestEnv::GetInstalledPdscFiles( bool bLatestsOnly)
{
  list<string> files;
  RteKernel::GetInstalledPdscFiles(files, GetCmsisPackRoot(), bLatestsOnly);
  return files;
}

std::string ProjMgrTestEnv::GetFilteredPacksString(const std::list<std::string>& pdscFiles, const std::string& includeIds)
{
  stringstream ss;
  for (auto& f : pdscFiles) {
    string id = RtePackage::PackIdFromPath(f);
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
