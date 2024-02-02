/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gmock/gmock.h"

#include "PackChk.h"
#include "CheckFiles.h"
#include "RteFsUtils.h"
#include "ErrLog.h"

#include <string>
#include <list>
#include <sstream>

using namespace std;
using namespace ::testing;

class TestCheckFiles : public Test {
public:
  TestCheckFiles() : checkFiles(), errLog(*ErrLog::Get()), packChk() { }

protected:
  CheckFiles checkFiles;
  ErrLog& errLog;

  void SetUp() override {
    ErrOutputter* oldErr = errLog.SetOutputter(new StdoutOutputter());
    if (oldErr) { delete oldErr; }
    packChk.InitMessageTable();

    checkFiles.SetPackagePath(BUILD_FOLDER);
    checkFiles.SetPackageName("TestCheckFiles");
  };

  void TearDown() override {
    errLog.ClearLogMessages();
    RteFsUtils::RemoveDir(string(BUILD_FOLDER) + "/testdata");
  }

private:
  class StdoutOutputter : public ErrOutputter
  {
  public:
    StdoutOutputter() : ErrOutputter(), newline(true) { }

    void MsgOut(const std::string& msg) override {
      ErrOutputter::MsgOut(msg);
      if (newline) { cout << "[  ErrLog  ] "; }

      list<string> lines;
      auto cnt = RteUtils::SplitString(lines, msg, '\n');
      for (auto& l : lines) {
        cout << l;
        cnt--;
        if (cnt > 0) { cout << endl << "[  ErrLog  ] ";  }
      }
      newline = (*msg.rbegin() == '\n');
      if (newline) { cout << endl; }
    }

    void Clear() override {
      ErrOutputter::Clear();
      if (!newline) { cout << endl; }
      newline = true;
    }
  private:
    bool newline;
  };

  class PackChkProxy : private PackChk
  {
  public:
    void InitMessageTable() { ASSERT_TRUE(PackChk::InitMessageTable()); }
  } packChk;
};

TEST_F(TestCheckFiles, CheckFileExtension_Include) {
  // GIVEN an existing directory
  const string checkPath = "CheckFileExtension/TheIncludeDir/";
  ASSERT_TRUE(RteFsUtils::CreateDirectories(string(BUILD_FOLDER)+checkPath));

  // ... AND an item refering to this directory as category include
  RteItem item(NULL);
  item.SetAttribute("name", checkPath.c_str());
  item.SetAttribute("category", "include");

  // WHEN calling CheckFileExtension for this item
  const auto res = checkFiles.CheckFileExtension(&item);

  // THEN the check is expected to pass
  EXPECT_TRUE(res);
}

TEST_F(TestCheckFiles, CheckFileExtension_IncludeNoSlash) {
  // GIVEN an existing directory without trailing slash
  const string checkPath = "CheckFileExtension/TheIncludeDir";
  ASSERT_TRUE(RteFsUtils::CreateDirectories(string(BUILD_FOLDER) + checkPath));

  // ... AND an item refering to this directory as category include
  RteItem item(NULL);
  item.SetAttribute("name", checkPath.c_str());
  item.SetAttribute("category", "include");
  item.SetLineNumber(4711);

  // WHEN calling CheckFileExtension for this item
  const auto res = checkFiles.CheckFileExtension(&item);

  // THEN the check is expected to fail with error M340
  EXPECT_FALSE(res);
  EXPECT_THAT(errLog.GetLogMessages(), Contains(HasSubstr("M340")));
  EXPECT_THAT(errLog.GetLogMessages(), Contains(HasSubstr(checkPath)));
  EXPECT_THAT(errLog.GetLogMessages(), Contains(HasSubstr("4711")));
}

TEST_F(TestCheckFiles, CheckFileExtension_IncludeNotADir) {
  // GIVEN an existing header file
  const string checkFile = "CheckFileExtension/TheIncludeDir/header.h";
  ASSERT_TRUE(RteFsUtils::CreateTextFile(string(BUILD_FOLDER)+checkFile, ""));

  // ... AND an item refering to this file as category include
  RteItem item(NULL);
  item.SetAttribute("name", checkFile.c_str());
  item.SetAttribute("category", "include");
  item.SetLineNumber(4711);

  // WHEN calling CheckFileExtension for this item
  const auto res = checkFiles.CheckFileExtension(&item);

  // THEN the check is expected to fail error M339
  EXPECT_FALSE(res);
  EXPECT_THAT(errLog.GetLogMessages(), Contains(HasSubstr("M339")));
  EXPECT_THAT(errLog.GetLogMessages(), Contains(HasSubstr(checkFile)));
  EXPECT_THAT(errLog.GetLogMessages(), Contains(HasSubstr("4711")));
}

TEST_F(TestCheckFiles, CheckFileExtension_NullItem) {
  // GIVEN a nullptr
  RteItem* item = nullptr;

  // WHEN calling CheckFileExtension for this item
  const auto res = checkFiles.CheckFileExtension(item);

  // THEN the check is expected to pass
  EXPECT_TRUE(res);
}

TEST_F(TestCheckFiles, CheckCaseSense)
{
  // test setup
  string packPath = checkFiles.GetPackagePath();
  string testDataFolder = packPath + "/testdata";
  const string& testApiFolder = testDataFolder + "/Api";
  if (RteFsUtils::Exists(testDataFolder)) {
    RteFsUtils::RemoveDir(testDataFolder);
  }
  ASSERT_TRUE(RteFsUtils::CreateDirectories(testApiFolder));
  ASSERT_TRUE(RteFsUtils::CreateTextFile(
    testApiFolder + "/Exclusive.h", RteUtils::EMPTY_STRING));
  ASSERT_TRUE(RteFsUtils::CreateDirectories(testDataFolder + "/.test1"));
  ASSERT_TRUE(RteFsUtils::CreateTextFile(
    testDataFolder + "/.test1/NonExclusive.h", RteUtils::EMPTY_STRING));
  checkFiles.SetPackagePath(testDataFolder);

  // test
  map<string, bool> testInputs = {
    // FilePath, expectedResults
    { RteUtils::EMPTY_STRING,        true},
    { "Api\\Exclusive.h",            true},
    { "Api/Exclusive.h",             true},
    { "./Api/Exclusive.h",           true},
    { "././././Api/Exclusive.h",     true},
    { ".test1/NonExclusive.h",       true},
    { ".test1/../Api/Exclusive.h",   true},
    { "../testdata/Api/Exclusive.h", true},
    { "../Invalid/Path/Exclusive.h", true},   // result is true now because relative paths are currently not checked
    { "api\\exclusive.h",            false},
    { "api/exclusive.h",             false}
  };

  for (const auto& [filePath, result] : testInputs) {
    EXPECT_EQ(result, checkFiles.CheckCaseSense(filePath, 1)) <<
      "error: failed for input \"" << filePath << "\"" << endl;
  }

  // cleanup
  RteFsUtils::RemoveDir(testDataFolder);
  checkFiles.SetPackagePath(packPath);
}


TEST_F(TestCheckFiles, CheckForSpaces)
{
  map<string, bool> testInputs = {
    // FilePath, expectedResults
    { RteUtils::EMPTY_STRING,        true},
    { "TestFile.h",                  true},
    { "Test File.h",                 false},
  };

  for (const auto& [fileName, result] : testInputs) {
    EXPECT_EQ(result, checkFiles.CheckForSpaces(fileName, 1)) <<
      "error: failed for input \"" << fileName << "\"" << endl;
  }
}
