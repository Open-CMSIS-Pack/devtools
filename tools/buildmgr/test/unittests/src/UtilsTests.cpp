/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CBuildUnitTestEnv.h"

#include "XMLTreeSlim.h"
#include "RteValueAdjuster.h"

using namespace std;

class CbuildUtilsTests :public ::testing::Test {
protected:
  static void SetUpTestSuite();
  static void TearDownTestSuite();
};

void CbuildUtilsTests::SetUpTestSuite() {
}

void CbuildUtilsTests::TearDownTestSuite() {
  error_code ec;
  fs::current_path(CBuildUnitTestEnv::workingDir, ec);
}

TEST_F(CbuildUtilsTests, GetFileType)
{
  RteFile::Category cat;
  vector<pair<string, RteFile::Category>> input =
  { {"Test.c", RteFile::Category::SOURCE_C }, {"Test.C",RteFile::Category::SOURCE_C},
  {"Test.cpp", RteFile::Category::SOURCE_CPP}, {"Test.c++", RteFile::Category::SOURCE_CPP},
  {"Test.C++", RteFile::Category::SOURCE_CPP}, {"Test.cxx",RteFile::Category::SOURCE_CPP},
  {"Test.cc",  RteFile::Category::SOURCE_CPP}, {"Test.CC", RteFile::Category::SOURCE_CPP},
  {"Test.asm", RteFile::Category::SOURCE_ASM}, {"Test.s", RteFile::Category::SOURCE_ASM },
  {"Test.S",RteFile::Category::SOURCE_ASM }, {"Test.txt",RteFile::Category::OTHER} };

  for (int elem = 0; elem <= (int)RteFile::Category::OTHER &&
    elem != (int)RteFile::Category::SOURCE; ++elem) {
     cat = CbuildUtils::GetFileType(static_cast<RteFile::Category>(elem), "");
     EXPECT_EQ(cat, static_cast<RteFile::Category>(elem));
  }

  for_each(input.begin(), input.end(), [&](pair<string, RteFile::Category> in) {
    cat = CbuildUtils::GetFileType(RteFile::Category::SOURCE, in.first);
    EXPECT_EQ(cat, in.second);
  });
}

TEST_F(CbuildUtilsTests, RemoveSlash) {
  string input = "/Arm";
  input = CbuildUtils::RemoveSlash(input);
  EXPECT_EQ("Arm", input);

  input = "\testinput";
  input = CbuildUtils::RemoveSlash(input);
  EXPECT_EQ("\testinput", input);
}

TEST_F(CbuildUtilsTests, ReplaceColon) {
  string input = "::Arm:";
  input = CbuildUtils::ReplaceColon(input);
  EXPECT_EQ("__Arm_", input);

  input = "test:input";
  input = CbuildUtils::ReplaceColon(input);
  EXPECT_EQ("test_input", input);
}

TEST_F(CbuildUtilsTests, GetItemByTagAndAttribute) {
  bool result;
  Collection<RteItem*> list;
  string compiler = "AC6", use = "armasm", name = "Test";
  RteItem compilerItem(nullptr);
  compilerItem.SetTag("cflags");
  compilerItem.SetAttribute("compiler", "AC6");

  RteItem asflagItem(nullptr);
  asflagItem.SetTag("asflags");
  asflagItem.SetAttribute("use", "armasm");

  RteItem outputItem(nullptr);
  outputItem.SetTag("output");
  outputItem.SetAttribute("name", "Test");

  list.push_back(&compilerItem);
  list.push_back(&asflagItem);
  list.push_back(&outputItem);

  /* Valid input */
  const RteItem*    cflags       = CbuildUtils::GetItemByTagAndAttribute(list, "cflags", "compiler", "AC6");
  const RteItem*    asUseflags   = CbuildUtils::GetItemByTagAndAttribute(list, "asflags", "use", "armasm");
  const RteItem*    asNameflags  = CbuildUtils::GetItemByTagAndAttribute(list, "output", "name", "Test");

  /* Invalid input*/
  const RteItem*    unknownflags = CbuildUtils::GetItemByTagAndAttribute(list, "Invalid", "compiler", "AC6");
  const RteItem*    InvalidTag   = CbuildUtils::GetItemByTagAndAttribute(list, "Invalid", "name", "Blinky");

  result = (cflags && asUseflags && asNameflags && !unknownflags && !InvalidTag);
  EXPECT_TRUE(result);
}

TEST_F(CbuildUtilsTests, StrPathConv) {
  string path, expected;

  path     = "/C/testdir\\new folder";
  expected = "/C/testdir/new folder";
  path     = CbuildUtils::StrPathConv(path);
  EXPECT_EQ(expected, path);

  path     = "/C/test\\dir\\Temp";
  expected = "/C/test/dir/Temp";
  path     = CbuildUtils::StrPathConv(path);
  EXPECT_EQ(expected, path);
}

TEST_F(CbuildUtilsTests, StrPathAbsolute) {
  string path, base, expected, cwd;
  error_code ec;

  cwd = fs::current_path(ec).generic_string();
  string testDir = string(TEST_BUILD_FOLDER);
  fs::create_directories(testDir + "/UtilsTest/relative/path", ec);

  path     = "./UtilsTest/relative/path";
  base     = testDir;
  expected = "\"" + testDir + "UtilsTest/relative/path\"";
  path     = CbuildUtils::StrPathAbsolute(path, base);
  EXPECT_EQ(expected, path);

  path     = ".\\UtilsTest\\relative\\path";
  base     = testDir;
  expected = "\"" + testDir + "UtilsTest/relative/path\"";
  path     = CbuildUtils::StrPathAbsolute(path, base);
  EXPECT_EQ(expected, path);

  path     = "--relpath_flag=./UtilsTest/relative/path";
  base     = testDir;
  expected = "--relpath_flag=\"" + testDir + "UtilsTest/relative/path\"";
  path     = CbuildUtils::StrPathAbsolute(path, base);
  EXPECT_EQ(expected, path);

  path     = "--relpath_flag=.\\UtilsTest\\relative\\path";
  base     = testDir;
  expected = "--relpath_flag=\"" + testDir + "UtilsTest/relative/path\"";
  path     = CbuildUtils::StrPathAbsolute(path, base);
  EXPECT_EQ(expected, path);

  // Move CWD
  fs::current_path(testDir + "/UtilsTest", ec);

  path     = "../UtilsTest/relative/path";
  base     = fs::current_path(ec).generic_string() + "/";
  expected = "\"" + testDir + "UtilsTest/relative/path\"";
  path     = CbuildUtils::StrPathAbsolute(path, base);
  EXPECT_EQ(expected, path);

  path     = "..\\UtilsTest\\relative\\path";
  base     = fs::current_path(ec).generic_string() + "/";
  expected = "\"" + testDir + "UtilsTest/relative/path\"";
  path     = CbuildUtils::StrPathAbsolute(path, base);
  EXPECT_EQ(expected, path);

  // Restore CWD
  fs::current_path(cwd, ec);
}
TEST_F(CbuildUtilsTests, EscapeQuotes) {
  string result = CbuildUtils::EscapeQuotes("-DFILE=\"config.h\"");
  EXPECT_EQ(result, "-DFILE=\\\"config.h\\\"");
  result = CbuildUtils::EscapeQuotes("-DFILE=\\\"config.h\\\"");
  EXPECT_EQ(result, "-DFILE=\\\\\\\"config.h\\\\\\\"");
}

TEST_F(CbuildUtilsTests, NormalizePath) {
  error_code ec;
  string path, base;
  path = "./testinput//.//Test1/../Test1/Test2";
  base = string(TEST_BUILD_FOLDER);
  fs::create_directories(base + "testinput/Test1/Test2", ec);

  EXPECT_TRUE(CbuildUtils::NormalizePath(path, base));
  EXPECT_EQ(path, base + "testinput/Test1/Test2");

  path = "./unknown/../path";
  EXPECT_FALSE(CbuildUtils::NormalizePath(path, base));
  EXPECT_EQ(path, path);

  RemoveDir(base + "testinput/Test1");
}
