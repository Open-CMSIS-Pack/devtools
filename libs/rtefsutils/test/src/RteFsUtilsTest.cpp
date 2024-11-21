/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "gtest/gtest.h"
#include "RteUtils.h"
#include "RteFsUtils.h"
#include <fstream>

using namespace std;

class RteFsUtilsTest :public ::testing::Test {
protected:
  void SetUp()    override;
  void TearDown() override;
  void CreateInputFiles(const string& dir);
  bool CompareFileTree(const string& dir1, const string& dir2);
};

// Directories
const string dirnameBase = "RteFsUtilsTest";
const string dirnameDir = dirnameBase + "/dir";
const string dirnameSubdir = dirnameBase + "/dir/subdir";
const string dirnameSubdir2 = dirnameBase + "/dir/subdir2";
const string dirnameSubdirBackslash = RteUtils::SlashesToOsSlashes(dirnameBase + "\\dir\\subdir");
const string dirnameSubdirMixed = RteUtils::SlashesToOsSlashes(dirnameBase + "/dir\\subdir");
const string dirnameSubdirWithTrailing = dirnameBase + "/dir/subdir/";
const string dirnameBackslashWithTrailing = RteUtils::SlashesToOsSlashes(dirnameBase + "\\dir\\subdir\\");
const string dirnameMixedWithTrailing = RteUtils::SlashesToOsSlashes(dirnameBase + "/dir\\subdir/");
const string dirnameRegularCopy = dirnameBase + "/dir/copy";
const string dirnameBackslashCopy = RteUtils::SlashesToOsSlashes(dirnameBase + "\\dir\\copy");
const string dirnameMixedCopy = RteUtils::SlashesToOsSlashes(dirnameBase + "/dir\\copy");
const string dirnameDotSubdir = dirnameBase + "/dir/./subdir";
const string dirnameDotDotSubdir = dirnameBase + "/dir/subdir/../subdir";
const string dirnameEmpty = "";

// Filenames
const string filenameRegular = dirnameSubdir + "/file.txt";
const string filenameBackslash = RteUtils::SlashesToOsSlashes(dirnameSubdirBackslash + "\\file.txt");
const string filenameMixed = RteUtils::SlashesToOsSlashes(dirnameSubdirMixed + "/file.txt");
const string filenameRegularCopy = filenameRegular + ".copy";
const string filenameBackslashCopy = RteUtils::SlashesToOsSlashes(filenameBackslash + ".copy");
const string filenameMixedCopy = filenameMixed + ".copy";
const string filenameEmpty = "";
const string filenameBackup0 = RteUtils::SlashesToOsSlashes(filenameRegular + ".0000");
const string filenameBackup1 = RteUtils::SlashesToOsSlashes(filenameRegular + ".0001");
const string pathInvalid = dirnameSubdir + "/Invalid";

static set<string, VersionCmp::Greater> sortedFileSet = {
  "foo.h",
  "bar.h",
  "foo.bar.h",
  "foo.c",
  "bar.c",
  "foo.bar.c",
  "foo.sct",
  "bar.sct",
  "foo.s",
  "v1.0.0",
  "v2.0.0",
  "v10.0.0",
  "v2.0.0-beta"
};


// Buffers
const string bufferFoo = "\n\nbuild:\r\nfoo\r\n\n";
const string bufferBar = "\n\nbuild:\r\nbar\r\n\n";


void RteFsUtilsTest::SetUp() {
  // Remove test files and directories
  RteFsUtils::RemoveDir(dirnameBase);
}

void RteFsUtilsTest::TearDown() {
  // Remove test files and directories
  RteFsUtils::RemoveDir(dirnameBase);
}

void RteFsUtilsTest::CreateInputFiles(const string& dir) {
  // Create dummy test files
  for (auto fileName : sortedFileSet) {
    string filePath = dir + "/" + fileName;
    RteFsUtils::CreateTextFile(filePath, fileName);
  }
}


bool RteFsUtilsTest::CompareFileTree(const string& dir1, const string& dir2) {
  set<string> tree1, tree2;
  error_code ec;
  if (RteFsUtils::Exists(dir1)) {
    for (auto& p : fs::recursive_directory_iterator(dir1, ec)) {
      tree1.insert(p.path().filename().generic_string());
    }
  }
  if (RteFsUtils::Exists(dir2)) {
    for (auto& p : fs::recursive_directory_iterator(dir2, ec)) {
      tree2.insert(p.path().filename().generic_string());
    }
  }
  return (tree1 == tree2);
}


// Test Cases

TEST_F(RteFsUtilsTest, BackupFile) {
  string ret;
  error_code ec;

  RteFsUtils::CreateTextFile(filenameRegular, "foo");

  // Test filename with regular separators and multiple backup
  ret = RteUtils::SlashesToOsSlashes(RteFsUtils::BackupFile(filenameRegular));
  EXPECT_EQ(ret, filenameBackup0);
  EXPECT_EQ(RteFsUtils::Exists(filenameBackup0), true);
  ofstream fileStream(filenameRegular);
  fileStream << "bar";
  fileStream.flush();
  fileStream.close();
  ret = RteUtils::SlashesToOsSlashes(RteFsUtils::BackupFile(filenameRegular));
  EXPECT_EQ(ret, filenameBackup1);
  EXPECT_EQ(RteFsUtils::Exists(filenameBackup1), true);
  RteFsUtils::RemoveFile(filenameBackup0);
  RteFsUtils::RemoveFile(filenameBackup1);

  // Test filename with backslashes separators
  ret = RteFsUtils::BackupFile(filenameBackslash);
  EXPECT_EQ(ret, filenameBackup0);
  EXPECT_EQ(RteFsUtils::Exists(filenameBackup0), true);
  RteFsUtils::RemoveFile(filenameBackup0);

  // Test filename with mixed separators
  ret = RteUtils::SlashesToOsSlashes(RteFsUtils::BackupFile(filenameMixed));
  EXPECT_EQ(ret, filenameBackup0);
  EXPECT_EQ(RteFsUtils::Exists(filenameBackup0), true);
  RteFsUtils::RemoveFile(filenameBackup0);

  // Test filename empty
  ret = RteFsUtils::BackupFile(filenameEmpty);
  EXPECT_EQ(ret == RteUtils::EMPTY_STRING, true);
  EXPECT_EQ(RteFsUtils::Exists(filenameBackup0), false);

  // Test filename invalid
  ret = RteFsUtils::BackupFile(pathInvalid);
  EXPECT_EQ(ret == RteUtils::EMPTY_STRING, true);

  // Test deleteExisting argument
  ret = RteUtils::SlashesToOsSlashes(RteFsUtils::BackupFile(filenameRegular, true));
  EXPECT_EQ(ret, filenameBackup0);
  EXPECT_EQ(RteFsUtils::Exists(filenameBackup0), true);
  EXPECT_EQ(RteFsUtils::Exists(filenameRegular), false);
  RteFsUtils::RemoveFile(filenameBackup0);

  RteFsUtils::RemoveFile(filenameRegular);
}

TEST_F(RteFsUtilsTest, CmpFileMem) {
  bool ret;

  RteFsUtils::CreateTextFile(filenameRegular, bufferFoo);

  // Test filename with regular separators
  ret = RteFsUtils::CmpFileMem(filenameRegular, bufferFoo);
  EXPECT_EQ(ret, true);
  ret = RteFsUtils::CmpFileMem(filenameRegular, bufferBar);
  EXPECT_EQ(ret, false);

  // Test filename with backslashes separators
  ret = RteFsUtils::CmpFileMem(filenameBackslash, bufferFoo);
  EXPECT_EQ(ret, true);
  ret = RteFsUtils::CmpFileMem(filenameBackslash, bufferBar);
  EXPECT_EQ(ret, false);

  // Test filename with mixed separators
  ret = RteFsUtils::CmpFileMem(filenameMixed, bufferFoo);
  EXPECT_EQ(ret, true);
  ret = RteFsUtils::CmpFileMem(filenameMixed, bufferBar);
  EXPECT_EQ(ret, false);

  // Test filename empty
  ret = RteFsUtils::CmpFileMem("", bufferFoo);
  EXPECT_EQ(ret, false);

  // Test filename invalid
  ret = RteFsUtils::CmpFileMem(pathInvalid, bufferFoo);
  EXPECT_EQ(ret, false);

  // Test filename with buffer empty
  ret = RteFsUtils::CmpFileMem(filenameRegular, "");
  EXPECT_EQ(ret, false);
}

TEST_F(RteFsUtilsTest, CopyBufferToFile) {
  bool ret;
  error_code ec;

  // Test filename with regular separators
  ret = RteFsUtils::CopyBufferToFile(filenameRegular, bufferFoo, false);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(filenameRegular), true);
  EXPECT_EQ(RteFsUtils::CmpFileMem(filenameRegular, bufferFoo), true);
  RteFsUtils::RemoveFile(filenameRegular);

  // Test filename with backslashes separators
  ret = RteFsUtils::CopyBufferToFile(filenameBackslash, bufferFoo, false);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(filenameRegular), true);
  EXPECT_EQ(RteFsUtils::CmpFileMem(filenameRegular, bufferFoo), true);
  RteFsUtils::RemoveFile(filenameRegular);

  // Test filename with mixed separators
  ret = RteFsUtils::CopyBufferToFile(filenameMixed, bufferFoo, false);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(filenameRegular), true);
  EXPECT_EQ(RteFsUtils::CmpFileMem(filenameRegular, bufferFoo), true);
  RteFsUtils::RemoveFile(filenameRegular);

  // Test filename empty
  ret = RteFsUtils::CopyBufferToFile("", bufferFoo, false);
  EXPECT_EQ(ret, false);

  // Test filename with buffer empty
  ret = RteFsUtils::CopyBufferToFile(filenameRegular, "", false);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(filenameRegular), true);
  EXPECT_EQ(RteFsUtils::CmpFileMem(filenameRegular, ""), true);
  RteFsUtils::RemoveFile(filenameRegular);

  // Test existent file with same content
  RteFsUtils::CreateTextFile(filenameRegular, "foo");
  ret = RteFsUtils::CopyBufferToFile(filenameRegular, bufferFoo, false);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(filenameRegular), true);
  EXPECT_EQ(RteFsUtils::CmpFileMem(filenameRegular, bufferFoo), true);
  RteFsUtils::RemoveFile(filenameRegular);

  // Test existent file with different content and backup argument
  RteFsUtils::CreateTextFile(filenameRegular, bufferFoo);
  ret = RteFsUtils::CopyBufferToFile(filenameRegular, bufferBar, true);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(filenameRegular), true);
  EXPECT_EQ(RteFsUtils::Exists(filenameBackup0), true);
  EXPECT_EQ(RteFsUtils::CmpFileMem(filenameRegular, bufferBar), true);
  EXPECT_EQ(RteFsUtils::CmpFileMem(filenameBackup0, bufferFoo), true);
  RteFsUtils::RemoveFile(filenameRegular);
  RteFsUtils::RemoveFile(filenameBackup0);
}

TEST_F(RteFsUtilsTest, CopyCheckFile) {
  bool ret;
  error_code ec;

  RteFsUtils::CreateTextFile(filenameRegular, bufferFoo);

  // Test filename with regular separators
  ret = RteFsUtils::CopyCheckFile(filenameRegular, filenameRegularCopy, false);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(filenameRegularCopy), true);
  RteFsUtils::RemoveFile(filenameRegularCopy);

  // Test filename with backslashes separators
  ret = RteFsUtils::CopyCheckFile(filenameBackslash, filenameBackslashCopy, false);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(filenameRegularCopy), true);
  RteFsUtils::RemoveFile(filenameRegularCopy);

  // Test filename with mixed separators
  ret = RteFsUtils::CopyCheckFile(filenameMixed, filenameMixedCopy, false);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(filenameRegularCopy), true);
  RteFsUtils::RemoveFile(filenameRegularCopy);

  // Test source filename empty
  ret = RteFsUtils::CopyCheckFile("", filenameRegularCopy, false);
  EXPECT_EQ(ret, false);

  // Test destination filename empty
  ret = RteFsUtils::CopyCheckFile(filenameRegular, "", false);
  EXPECT_EQ(ret, false);

  // Test source equal to destination
  ret = RteFsUtils::CopyCheckFile(filenameRegular, filenameRegular, false);
  EXPECT_EQ(ret, false);

  // Test source filename invalid
  ret = RteFsUtils::CopyCheckFile(pathInvalid, filenameRegularCopy, false);
  EXPECT_EQ(ret, false);

  // Test backup argument
  fs::copy_file(filenameRegular, filenameRegularCopy, fs::copy_options::overwrite_existing, ec);
  ret = RteFsUtils::CopyCheckFile(filenameRegularCopy, filenameRegular, true);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(filenameRegular), true);
  EXPECT_EQ(RteFsUtils::Exists(filenameBackup0), true);
  RteFsUtils::RemoveFile(filenameRegularCopy);
  RteFsUtils::RemoveFile(filenameRegular);
  RteFsUtils::RemoveFile(filenameBackup0);
}

TEST_F(RteFsUtilsTest, ExpandFile) {
  bool ret;
  string buffer;

  RteFsUtils::CreateTextFile(filenameRegular, "%Instance%");

  // Test filename with regular separators
  ret = RteFsUtils::ExpandFile(filenameRegular, 1, buffer);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(buffer == "1", true);

  // Test filename with backslashes separators
  ret = RteFsUtils::ExpandFile(filenameBackslash, 1, buffer);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(buffer == "1", true);

  // Test filename with mixed separators
  ret = RteFsUtils::ExpandFile(filenameMixed, 1, buffer);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(buffer == "1", true);

  // Test filename empty
  ret = RteFsUtils::ExpandFile("", 1, buffer);
  EXPECT_EQ(ret, false);

  // Test filename invalid
  ret = RteFsUtils::ExpandFile(pathInvalid, 1, buffer);
  EXPECT_EQ(ret, false);

  RteFsUtils::RemoveFile(filenameRegular);
}

TEST_F(RteFsUtilsTest, CopyMergeFile) {
  bool ret;
  error_code ec;

  RteFsUtils::CreateTextFile(filenameRegular, "%Instance%");

  // Test filename with regular separators
  ret = RteFsUtils::CopyMergeFile(filenameRegular, filenameRegularCopy, 1, false);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(filenameRegularCopy), true);
  EXPECT_EQ(RteFsUtils::CmpFileMem(filenameRegularCopy, "1"), true);
  RteFsUtils::RemoveFile(filenameRegularCopy);

  // Test filename with backslashes separators
  ret = RteFsUtils::CopyMergeFile(filenameBackslash, filenameBackslashCopy, 1, false);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(filenameRegularCopy), true);
  EXPECT_EQ(RteFsUtils::CmpFileMem(filenameRegularCopy, "1"), true);
  RteFsUtils::RemoveFile(filenameRegularCopy);

  // Test filename with mixed separators
  ret = RteFsUtils::CopyMergeFile(filenameMixed, filenameMixedCopy, 1, false);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(filenameRegularCopy), true);
  EXPECT_EQ(RteFsUtils::CmpFileMem(filenameRegularCopy, "1"), true);
  RteFsUtils::RemoveFile(filenameRegularCopy);

  // Test source filename empty
  ret = RteFsUtils::CopyMergeFile("", filenameRegularCopy, 1, false);
  EXPECT_EQ(ret, false);

  // Test destination filename empty
  ret = RteFsUtils::CopyMergeFile(filenameRegular, "", 1, false);
  EXPECT_EQ(ret, false);

  // Test source filename invalid
  ret = RteFsUtils::CopyMergeFile(pathInvalid, filenameRegularCopy, 1, false);
  EXPECT_EQ(ret, false);

  // Test backup argument
  fs::copy_file(filenameRegular, filenameRegularCopy, fs::copy_options::overwrite_existing, ec);
  ret = RteFsUtils::CopyMergeFile(filenameRegularCopy, filenameRegular, 1, true);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::CmpFileMem(filenameRegular, "1"), true);
  EXPECT_EQ(RteFsUtils::Exists(filenameRegular), true);
  EXPECT_EQ(RteFsUtils::Exists(filenameBackup0), true);
  RteFsUtils::RemoveFile(filenameRegularCopy);
  RteFsUtils::RemoveFile(filenameRegular);
  RteFsUtils::RemoveFile(filenameBackup0);
}

TEST_F(RteFsUtilsTest, CopyTree) {
  bool ret;
  error_code ec;

  // use CountFilesInFolder if additionall test
  EXPECT_EQ(RteFsUtils::CountFilesInFolder(dirnameBase), 0);

  RteFsUtils::CreateTextFile(filenameRegular, "foo");
  RteFsUtils::CreateTextFile(filenameRegularCopy, "bar");

  EXPECT_EQ(RteFsUtils::CountFilesInFolder(dirnameBase), 2);

  // Test dirname with regular separators
  ret = RteFsUtils::CopyTree(dirnameSubdir, dirnameRegularCopy);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(dirnameRegularCopy), true);
  EXPECT_EQ(CompareFileTree(dirnameSubdir, dirnameRegularCopy), true);
  EXPECT_EQ(RteFsUtils::CountFilesInFolder(dirnameBase), 4);
  RteFsUtils::RemoveDir(dirnameRegularCopy);
  EXPECT_EQ(RteFsUtils::CountFilesInFolder(dirnameBase), 2);

  // Test dirname with backslashes separators
  ret = RteFsUtils::CopyTree(dirnameSubdirBackslash, dirnameBackslashCopy);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(dirnameRegularCopy), true);
  EXPECT_EQ(CompareFileTree(dirnameSubdir, dirnameRegularCopy), true);
  RteFsUtils::RemoveDir(dirnameRegularCopy);

  // Test dirname with mixed separators
  ret = RteFsUtils::CopyTree(dirnameSubdirMixed, dirnameMixedCopy);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(dirnameRegularCopy), true);
  EXPECT_EQ(CompareFileTree(dirnameSubdir, dirnameRegularCopy), true);
  RteFsUtils::RemoveDir(dirnameRegularCopy);

  // Test dirname with regular separators and trailing
  ret = RteFsUtils::CopyTree(dirnameSubdirWithTrailing, dirnameRegularCopy);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(dirnameRegularCopy), true);
  EXPECT_EQ(CompareFileTree(dirnameSubdir, dirnameRegularCopy), true);
  RteFsUtils::RemoveDir(dirnameRegularCopy);

  // Test dirname with backslashes separators and trailing
  ret = RteFsUtils::CopyTree(dirnameBackslashWithTrailing, dirnameBackslashCopy);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(dirnameRegularCopy), true);
  EXPECT_EQ(CompareFileTree(dirnameSubdir, dirnameRegularCopy), true);
  RteFsUtils::RemoveDir(dirnameRegularCopy);

  // Test dirname with mixed separators and trailing
  ret = RteFsUtils::CopyTree(dirnameMixedWithTrailing, dirnameMixedCopy);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(dirnameRegularCopy), true);
  EXPECT_EQ(CompareFileTree(dirnameSubdir, dirnameRegularCopy), true);
  RteFsUtils::RemoveDir(dirnameRegularCopy);

  // Test source dirname empty
  ret = RteFsUtils::CopyTree("", dirnameRegularCopy);
  EXPECT_EQ(ret, false);
  EXPECT_EQ(RteFsUtils::Exists(dirnameRegularCopy), false);

  // Test destination dirname empty
  ret = RteFsUtils::CopyTree(dirnameSubdir, "");
  EXPECT_EQ(ret, false);
  EXPECT_EQ(RteFsUtils::Exists(dirnameRegularCopy), false);

  // Test source not a directory
  ret = RteFsUtils::CopyTree(filenameRegular, dirnameRegularCopy);
  EXPECT_EQ(ret, false);
  EXPECT_EQ(RteFsUtils::Exists(dirnameRegularCopy), false);

  // Test source path invalid
  ret = RteFsUtils::CopyTree(pathInvalid, dirnameRegularCopy);
  EXPECT_EQ(ret, false);
  EXPECT_EQ(RteFsUtils::Exists(dirnameRegularCopy), false);

  RteFsUtils::RemoveFile(filenameRegularCopy);
  RteFsUtils::RemoveFile(filenameRegular);
  EXPECT_EQ(RteFsUtils::CountFilesInFolder(dirnameBase), 0);
}

TEST_F(RteFsUtilsTest, DeleteFileAutoRetry) {
  bool ret;
  error_code ec;

  // Test filename with regular separators
  RteFsUtils::CreateTextFile(filenameRegular, "foo");
  ret = RteFsUtils::DeleteFileAutoRetry(filenameRegular);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(filenameRegular), false);

  // Test filename with backslashes separators
  RteFsUtils::CreateTextFile(filenameRegular, "foo");
  ret = RteFsUtils::DeleteFileAutoRetry(filenameBackslash);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(filenameRegular), false);

  // Test filename with mixed separators
  RteFsUtils::CreateTextFile(filenameRegular, "foo");
  ret = RteFsUtils::DeleteFileAutoRetry(filenameMixed);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(filenameRegular), false);

  // Test filename empty
  ret = RteFsUtils::DeleteFileAutoRetry("");
  EXPECT_EQ(ret, true);

  // Test path non existent
  ret = RteFsUtils::DeleteFileAutoRetry(pathInvalid);
  EXPECT_EQ(ret, true);

  // Test delay argument equal to zero
  RteFsUtils::CreateTextFile(filenameRegular, "foo");
  ret = RteFsUtils::DeleteFileAutoRetry(filenameRegular, 5U, 0U);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(filenameRegular), false);

  // Test retry argument equal to zero
  RteFsUtils::CreateTextFile(filenameRegular, "foo");
  ret = RteFsUtils::DeleteFileAutoRetry(filenameRegular, 0U, 1U);
  EXPECT_EQ(ret, false);
  RteFsUtils::RemoveFile(filenameRegular);
}

TEST_F(RteFsUtilsTest, DeleteTree) {
  bool ret;
  error_code ec;

  // Test dirname with regular separators
  RteFsUtils::CreateTextFile(filenameRegular, "foo");
  ret = RteFsUtils::DeleteTree(dirnameSubdir);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(dirnameSubdir), false);

  // Test dirname with backslashes separators
  RteFsUtils::CreateTextFile(filenameRegular, "foo");
  ret = RteFsUtils::DeleteTree(dirnameSubdirBackslash);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(dirnameSubdir), false);

  // Test dirname with mixed separators
  RteFsUtils::CreateTextFile(filenameRegular, "foo");
  ret = RteFsUtils::DeleteTree(dirnameSubdirMixed);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(dirnameSubdir), false);

  // Test dirname with regular separators and trailing
  RteFsUtils::CreateTextFile(filenameRegular, "foo");
  ret = RteFsUtils::DeleteTree(dirnameSubdirWithTrailing);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(dirnameSubdir), false);

  // Test dirname with backslashes separators and trailing
  RteFsUtils::CreateTextFile(filenameRegular, "foo");
  ret = RteFsUtils::DeleteTree(dirnameBackslashWithTrailing);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(dirnameSubdir), false);

  // Test dirname with mixed separators and trailing
  RteFsUtils::CreateTextFile(filenameRegular, "foo");
  ret = RteFsUtils::DeleteTree(dirnameMixedWithTrailing);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(dirnameSubdir), false);

  // Test dirname empty
  ret = RteFsUtils::DeleteTree("");
  EXPECT_EQ(ret, true);

  // Test path non existent
  ret = RteFsUtils::DeleteTree(pathInvalid);
  EXPECT_EQ(ret, true);

  // Test source not a directory
  RteFsUtils::CreateTextFile(filenameRegular, "foo");
  ret = RteFsUtils::DeleteTree(filenameRegular);
  EXPECT_EQ(ret, false);
  RteFsUtils::RemoveFile(filenameRegular);
}

TEST_F(RteFsUtilsTest, MoveExistingFile) {
  bool ret;
  error_code ec;

  // Test filename with regular separators
  RteFsUtils::CreateTextFile(filenameRegular, "foo");
  ret = RteFsUtils::MoveExistingFile(filenameRegular, filenameRegularCopy);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(filenameRegular), false);
  EXPECT_EQ(RteFsUtils::Exists(filenameRegularCopy), true);
  RteFsUtils::RemoveFile(filenameRegularCopy);

  // Test filename with backslashes separators
  RteFsUtils::CreateTextFile(filenameRegular, "foo");
  ret = RteFsUtils::MoveExistingFile(filenameBackslash, filenameBackslashCopy);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(filenameRegular), false);
  EXPECT_EQ(RteFsUtils::Exists(filenameRegularCopy), true);
  RteFsUtils::RemoveFile(filenameRegularCopy);

  // Test filename with mixed separators
  RteFsUtils::CreateTextFile(filenameRegular, "foo");
  ret = RteFsUtils::MoveExistingFile(filenameMixed, filenameMixedCopy);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(filenameRegular), false);
  EXPECT_EQ(RteFsUtils::Exists(filenameRegularCopy), true);
  RteFsUtils::RemoveFile(filenameRegularCopy);

  // Test filename empty
  ret = RteFsUtils::MoveExistingFile("", filenameMixedCopy);
  EXPECT_EQ(ret, false);
  EXPECT_EQ(RteFsUtils::Exists(filenameRegularCopy), false);

  // Test path non existent
  ret = RteFsUtils::MoveExistingFile(pathInvalid, filenameMixedCopy);
  EXPECT_EQ(ret, false);
  EXPECT_EQ(RteFsUtils::Exists(filenameRegularCopy), false);
}

TEST_F(RteFsUtilsTest, MoveFileExAutoRetry) {
  bool ret;
  // Test filename with default retry and delay arguments
  RteFsUtils::CreateTextFile(filenameRegular, "foo");
  ret = RteFsUtils::MoveFileExAutoRetry(filenameRegular, filenameRegularCopy);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(filenameRegular), false);
  EXPECT_EQ(RteFsUtils::Exists(filenameRegularCopy), true);
  RteFsUtils::RemoveFile(filenameRegularCopy);

  // Test filename with delay argument equal to 0
  RteFsUtils::CreateTextFile(filenameRegular, "foo");
  ret = RteFsUtils::MoveFileExAutoRetry(filenameRegular, filenameRegularCopy, 5U, 0U);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(filenameRegular), false);
  EXPECT_EQ(RteFsUtils::Exists(filenameRegularCopy), true);
  RteFsUtils::RemoveFile(filenameRegularCopy);

  // Test filename with retry argument equal to 0
  RteFsUtils::CreateTextFile(filenameRegular, "foo");
  ret = RteFsUtils::MoveFileExAutoRetry(filenameRegular, filenameRegularCopy, 0U, 1U);
  EXPECT_EQ(ret, false);
  RteFsUtils::RemoveFile(filenameRegular);

  // try to move non-existing file
  ret = RteFsUtils::MoveFileExAutoRetry(filenameRegular, filenameRegularCopy, 0U, 1U);
  EXPECT_FALSE(ret);
  EXPECT_FALSE(RteFsUtils::Exists(filenameRegular));
  EXPECT_FALSE(RteFsUtils::Exists(filenameRegularCopy));
}


TEST_F(RteFsUtilsTest, CopyFileExAutoRetry) {
  // try to move non-existing file first
  bool ret = RteFsUtils::MoveFileExAutoRetry(filenameRegular, filenameRegularCopy, 0U, 1U);
  EXPECT_FALSE(ret);
  EXPECT_FALSE(RteFsUtils::Exists(filenameRegular));
  EXPECT_FALSE(RteFsUtils::Exists(filenameRegularCopy));

  // Test filename with default retry and delay arguments
  RteFsUtils::CreateTextFile(filenameRegular, "foo");
  ret = RteFsUtils::CopyFileExAutoRetry(filenameRegular, filenameRegularCopy);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(filenameRegular), true);
  EXPECT_EQ(RteFsUtils::Exists(filenameRegularCopy), true);
  RteFsUtils::RemoveFile(filenameRegularCopy);

  // Test filename with delay argument equal to 0
  ret = RteFsUtils::CopyFileExAutoRetry(filenameRegular, filenameRegularCopy, 5U, 0U);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(filenameRegular), true);
  EXPECT_EQ(RteFsUtils::Exists(filenameRegularCopy), true);
  RteFsUtils::RemoveFile(filenameRegularCopy);
}

TEST_F(RteFsUtilsTest, RemoveDirectoryAutoRetry) {
  bool ret;
  error_code ec;

  // Test dirname with regular separators
  RteFsUtils::CreateDirectories(dirnameSubdir);
  ret = RteFsUtils::RemoveDirectoryAutoRetry(dirnameSubdir);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(dirnameSubdir), false);

  // Test dirname with backslashes separators
  RteFsUtils::CreateDirectories(dirnameSubdir);
  ret = RteFsUtils::RemoveDirectoryAutoRetry(dirnameSubdirBackslash);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(dirnameSubdir), false);

  // Test dirname with mixed separators
  RteFsUtils::CreateDirectories(dirnameSubdir);
  ret = RteFsUtils::RemoveDirectoryAutoRetry(dirnameSubdirMixed);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(dirnameSubdir), false);

  // Test dirname with regular separators and trailing
  RteFsUtils::CreateDirectories(dirnameSubdir);
  ret = RteFsUtils::RemoveDirectoryAutoRetry(dirnameSubdirWithTrailing);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(dirnameSubdir), false);

  // Test dirname with backslashes separators and trailing
  RteFsUtils::CreateDirectories(dirnameSubdir);
  ret = RteFsUtils::RemoveDirectoryAutoRetry(dirnameBackslashWithTrailing);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(dirnameSubdir), false);

  // Test dirname with mixed separators and trailing
  RteFsUtils::CreateDirectories(dirnameSubdir);
  ret = RteFsUtils::RemoveDirectoryAutoRetry(dirnameMixedWithTrailing);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(dirnameSubdir), false);

  // Test dirname empty
  ret = RteFsUtils::RemoveDirectoryAutoRetry("");
  EXPECT_EQ(ret, true);

  // Test path non existent
  ret = RteFsUtils::RemoveDirectoryAutoRetry(pathInvalid);
  EXPECT_EQ(ret, true);

  // Test path is not a directory
  RteFsUtils::CreateTextFile(filenameRegular, "foo");
  ret = RteFsUtils::RemoveDirectoryAutoRetry(filenameRegular);
  EXPECT_EQ(ret, false);
  RteFsUtils::RemoveFile(filenameRegular);

  // Test filename with delay argument equal to 0
  RteFsUtils::CreateDirectories(dirnameSubdir);
  ret = RteFsUtils::RemoveDirectoryAutoRetry(dirnameSubdir, 5U, 0U);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(RteFsUtils::Exists(dirnameSubdir), false);

  // Test filename with retry argument equal to 0
  RteFsUtils::CreateDirectories(dirnameSubdir);
  ret = RteFsUtils::RemoveDirectoryAutoRetry(dirnameSubdir, 0U, 1U);
  EXPECT_EQ(ret, false);
  RteFsUtils::RemoveDir(dirnameSubdir);
}

TEST_F(RteFsUtilsTest, SetFileReadOnly) {
  bool ret;
  error_code ec;
  constexpr fs::perms write_mask = fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write;

  RteFsUtils::CreateTextFile(filenameRegular, "foo");
  const fs::perms initial_perm = fs::status(filenameRegular, ec).permissions();

  // Test filename with regular separators
  ret = RteFsUtils::SetFileReadOnly(filenameRegular, true);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(fs::status(filenameRegular, ec).permissions(), initial_perm & ~write_mask);
  ret = RteFsUtils::SetFileReadOnly(filenameRegular, false);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(fs::status(filenameRegular, ec).permissions(), initial_perm);

  // Test filename with backslashes separators
  ret = RteFsUtils::SetFileReadOnly(filenameBackslash, true);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(fs::status(filenameRegular, ec).permissions(), initial_perm & ~write_mask);
  ret = RteFsUtils::SetFileReadOnly(filenameBackslash, false);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(fs::status(filenameRegular, ec).permissions(), initial_perm);

  // Test filename with mixed separators
  ret = RteFsUtils::SetFileReadOnly(filenameMixed, true);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(fs::status(filenameRegular, ec).permissions(), initial_perm & ~write_mask);
  ret = RteFsUtils::SetFileReadOnly(filenameMixed, false);
  EXPECT_EQ(ret, true);
  EXPECT_EQ(fs::status(filenameRegular, ec).permissions(), initial_perm);

  // Test filename empty
  ret = RteFsUtils::SetFileReadOnly("", true);
  EXPECT_EQ(ret, false);
  EXPECT_EQ(fs::status(filenameRegular, ec).permissions(), initial_perm);

  // Test path non existent
  ret = RteFsUtils::SetFileReadOnly(pathInvalid, true);
  EXPECT_EQ(ret, false);
  EXPECT_EQ(fs::status(filenameRegular, ec).permissions(), initial_perm);

  RteFsUtils::RemoveDir(dirnameSubdir);
}

TEST_F(RteFsUtilsTest, SetTreeReadOnly) {
  bool ret;
  error_code ec;
  constexpr fs::perms write_mask = fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write;

  const string validationDir = dirnameSubdir + "/foo/bar";
  RteFsUtils::CreateTextFile(validationDir + "/baz.txt", "foo");
  const fs::perms initial_perm = fs::status(validationDir, ec).permissions();

  // Set parent directory read-only for the reminder of the test
  const fs::perms parent_perm = fs::status(dirnameDir).permissions();
  fs::permissions(dirnameDir, write_mask, fs::perm_options::remove | fs::perm_options::nofollow);

  // Test dirname with regular separators
  ret = RteFsUtils::SetTreeReadOnly(dirnameSubdir, true);
  EXPECT_EQ(ret, true);
  EXPECT_EQ((fs::status(validationDir, ec).permissions()), initial_perm & ~write_mask);
  ret = RteFsUtils::SetTreeReadOnly(dirnameSubdir, false);
  EXPECT_EQ(ret, true);
  EXPECT_EQ((fs::status(validationDir, ec).permissions()), initial_perm);

  // Test dirname with backslashes separators
  ret = RteFsUtils::SetTreeReadOnly(dirnameSubdirBackslash, true);
  EXPECT_EQ(ret, true);
  EXPECT_EQ((fs::status(validationDir, ec).permissions()), initial_perm & ~write_mask);
  ret = RteFsUtils::SetTreeReadOnly(dirnameSubdirBackslash, false);
  EXPECT_EQ(ret, true);
  EXPECT_EQ((fs::status(validationDir, ec).permissions()), initial_perm);

  // Test dirname with mixed separators
  ret = RteFsUtils::SetTreeReadOnly(dirnameSubdirMixed, true);
  EXPECT_EQ(ret, true);
  EXPECT_EQ((fs::status(validationDir, ec).permissions()), initial_perm & ~write_mask);
  ret = RteFsUtils::SetTreeReadOnly(dirnameSubdirMixed, false);
  EXPECT_EQ(ret, true);
  EXPECT_EQ((fs::status(validationDir, ec).permissions()), initial_perm);

  // Test dirname empty
  ret = RteFsUtils::SetTreeReadOnly("", true);
  EXPECT_EQ(ret, false);
  EXPECT_EQ((fs::status(validationDir, ec).permissions()), initial_perm);

  // Test path non existent
  ret = RteFsUtils::SetTreeReadOnly(pathInvalid, true);
  EXPECT_EQ(ret, false);
  EXPECT_EQ((fs::status(validationDir, ec).permissions()), initial_perm);

  // Restore parent directory permission
  fs::permissions(dirnameDir, parent_perm, fs::perm_options::replace | fs::perm_options::nofollow);

  RteFsUtils::RemoveDir(dirnameSubdir);
}

TEST_F(RteFsUtilsTest, MakePathCanonical) {
  string ret;
  error_code ec;
  const string filenameCanonical = fs::current_path(ec).append(filenameRegular).generic_string();
  const string dirnameCanonical = fs::current_path(ec).append(dirnameSubdir).generic_string();

  // create file and with parent directories for reliability of the tests
  RteFsUtils::CreateTextFile(filenameRegular, "foo");

  // Test filename with regular separators, file exists
  ret = RteFsUtils::MakePathCanonical(filenameRegular);
  EXPECT_EQ(ret, filenameCanonical);

  // remove the file (not the parent directories)
  RteFsUtils::DeleteFileAutoRetry(filenameRegular);

  // it is still possible to get canonical name
  ret = RteFsUtils::MakePathCanonical(filenameRegular);
  EXPECT_EQ(ret, filenameCanonical);

  // Even longer path can get canonical name
  string nonExistingFileRel = dirnameSubdir + "/non/existing/path/../file.txt";
  string nonExistingFileAbs = dirnameCanonical + "/non/existing/file.txt";
  ret = RteFsUtils::MakePathCanonical(nonExistingFileRel);
  EXPECT_EQ(ret, nonExistingFileAbs);

  // Test filename with backslashes separators
  ret = RteFsUtils::MakePathCanonical(filenameBackslash);
  EXPECT_EQ(ret, filenameCanonical);

  // Test filename with mixed separators
  ret = RteFsUtils::MakePathCanonical(filenameBackslash);
  EXPECT_EQ(ret, filenameCanonical);

  // Test dirname with regular separators and trailing
  ret = RteFsUtils::MakePathCanonical(dirnameSubdirWithTrailing);
  EXPECT_EQ(ret, dirnameCanonical);

  // Test dirname with backslashes separators and trailing
  ret = RteFsUtils::MakePathCanonical(dirnameBackslashWithTrailing);
  EXPECT_EQ(ret, dirnameCanonical);

  // Test dirname with mixed separators and trailing
  ret = RteFsUtils::MakePathCanonical(dirnameMixedWithTrailing);
  EXPECT_EQ(ret, dirnameCanonical);

  // Test path with dot inside
  ret = RteFsUtils::MakePathCanonical(dirnameDotSubdir);
  EXPECT_EQ(ret, dirnameCanonical);

  // Test path with two dots inside
  ret = RteFsUtils::MakePathCanonical(dirnameDotDotSubdir);
  EXPECT_EQ(ret, dirnameCanonical);

  RteFsUtils::RemoveDir(dirnameSubdir);
}

TEST_F(RteFsUtilsTest, GetCurrentFolder) {
  error_code ec;
  string currDir;
  const string expectedDir = fs::current_path(ec).append(dirnameSubdir).generic_string() + "/";
  string curDir = RteFsUtils::GetCurrentFolder();

  RteFsUtils::CreateTextFile(filenameRegular, "");
  fs::current_path(dirnameSubdir, ec);
  currDir = RteFsUtils::GetCurrentFolder();
  EXPECT_EQ(expectedDir, currDir);

  fs::current_path(curDir, ec);
  RteFsUtils::RemoveDir(dirnameSubdir);
}

TEST_F(RteFsUtilsTest, MakeSureFilePath) {
  error_code ec;
  bool result;
  string filePath, dirPath;

  filePath = dirnameBase + "/Test/Temp.txt";
  dirPath = fs::path(filePath).remove_filename().generic_string();
  if (RteFsUtils::Exists(dirPath)) {
    fs::remove(dirPath, ec);
  }

  result = RteFsUtils::MakeSureFilePath(filePath);
  result &= RteFsUtils::Exists(dirPath);
  EXPECT_TRUE(result);
}

TEST_F(RteFsUtilsTest, ParentPath) {
  string filePath, dirPath;

  dirPath = RteFsUtils::ParentPath(dirnameSubdir);
  EXPECT_EQ(dirPath, dirnameDir);

  filePath = RteFsUtils::ParentPath(filenameRegular);
  EXPECT_EQ(filePath, dirnameSubdir);
}

TEST_F(RteFsUtilsTest, LexicallyNormal) {
  string dirPath;

  dirPath = RteFsUtils::LexicallyNormal(dirnameDotDotSubdir);
  EXPECT_EQ(dirPath, dirnameSubdir);

  dirPath = RteFsUtils::LexicallyNormal(dirnameDotSubdir);
  EXPECT_EQ(dirPath, dirnameSubdir);

  dirPath = RteFsUtils::LexicallyNormal(dirnameSubdirWithTrailing);
  EXPECT_EQ(dirPath, dirnameSubdir);
}

TEST_F(RteFsUtilsTest, RelativePath) {
  string relPath;
  string absSubdir = RteFsUtils::AbsolutePath(dirnameSubdir).generic_string();
  string absSubdir2 = RteFsUtils::AbsolutePath(dirnameSubdir2).generic_string();
  string absBase = RteFsUtils::AbsolutePath(dirnameBase).generic_string();

  relPath = RteFsUtils::RelativePath(absSubdir, absBase);
  EXPECT_EQ(relPath, "dir/subdir");

  relPath = RteFsUtils::RelativePath(absSubdir, absBase, true);
  EXPECT_EQ(relPath, "./dir/subdir");

  relPath = RteFsUtils::RelativePath(absSubdir, absSubdir2, true);
  EXPECT_EQ(relPath, "../subdir");

  relPath = RteFsUtils::RelativePath(absSubdir, "");
  EXPECT_EQ(relPath, "");

  relPath = RteFsUtils::RelativePath("", absBase);
  EXPECT_EQ(relPath, "");
}

TEST_F(RteFsUtilsTest, CreateDirectories) {
  error_code ec;
  bool result;
  string dirPath;

  dirPath = dirnameBase + "/Test/";
  if (RteFsUtils::Exists(dirPath)) {
    fs::remove(dirPath, ec);
  }

  result = RteFsUtils::CreateDirectories(dirPath);
  result &= RteFsUtils::Exists(dirPath);
  EXPECT_TRUE(result);
}

TEST_F(RteFsUtilsTest, FindFiles) {
  RteFsUtils::PathVec files;

  CreateInputFiles(dirnameSubdir);

  files = RteFsUtils::FindFiles(dirnameSubdir, ".h");
  EXPECT_EQ(files.size(), 3);

  files = RteFsUtils::FindFiles(dirnameSubdir, ".c");
  EXPECT_EQ(files.size(), 3);

  files = RteFsUtils::FindFiles(dirnameSubdir, ".sct");
  EXPECT_EQ(files.size(), 2);

  files = RteFsUtils::FindFiles(dirnameSubdir, ".s");
  EXPECT_EQ(files.size(), 1);

  RteFsUtils::RemoveDir(dirnameSubdir);
}

TEST_F(RteFsUtilsTest, NormalizePath) {
  string path, base;
  path = "Test/.//foo/bar/../baz.h";
  base = dirnameBase + "/";

  RteFsUtils::NormalizePath(path, base);
  EXPECT_EQ(path, base + "Test/foo/baz.h");
}

TEST_F(RteFsUtilsTest, FindFirstFileWithExt) {

  CreateInputFiles(dirnameSubdir);

  string file = RteFsUtils::FindFirstFileWithExt(dirnameSubdir, ".h");
  EXPECT_EQ(file, "foo.h");

  file = RteFsUtils::FindFirstFileWithExt(dirnameSubdir, ".c");
  EXPECT_EQ(file, "foo.c");

  file = RteFsUtils::FindFirstFileWithExt(dirnameSubdir, nullptr);
  EXPECT_EQ(file, "");

  file = RteFsUtils::FindFirstFileWithExt(dirnameSubdir, "");
  EXPECT_EQ(file, "");

  file = RteFsUtils::FindFirstFileWithExt(dirnameSubdir, ".unknown");
  EXPECT_EQ(file, "");

  file = RteFsUtils::FindFirstFileWithExt(dirnameSubdir, "test");
  EXPECT_EQ(file, "");

  RteFsUtils::RemoveDir(dirnameSubdir);
}

TEST_F(RteFsUtilsTest, GetMatchingFiles) {

  CreateInputFiles(dirnameDir);
  CreateInputFiles(dirnameSubdir);
  CreateInputFiles(dirnameSubdir+"/1");
  CreateInputFiles(dirnameSubdir + "/2");
  CreateInputFiles(dirnameSubdir + "/.WithDot"); // should be ignored

  string curDir = RteUtils::SlashesToOsSlashes(RteFsUtils::GetCurrentFolder());

  list<string> files;
  RteFsUtils::GetMatchingFiles(files, ".sct", dirnameDir, 0, true);
  EXPECT_EQ(files.size(), 2);

  for (const string& f : files) {
    const string path = RteUtils::SlashesToOsSlashes(f);
    EXPECT_EQ(path.find(curDir), 0); // starts with current directory => absolute
  }

  files.clear();
  RteFsUtils::GetMatchingFiles(files, ".sct", dirnameDir, 1, true);
  EXPECT_EQ(files.size(), 4);

  files.clear();
  RteFsUtils::GetMatchingFiles(files, ".sct", dirnameDir, 2, true);
  EXPECT_EQ(files.size(), 8);

  files.clear();
  RteFsUtils::GetMatchingFiles(files, ".sct", dirnameDir, 3, true);
  EXPECT_EQ(files.size(), 8);


  files.clear();
  RteFsUtils::GetMatchingFiles(files, ".h", dirnameBase, 3, true);
  EXPECT_EQ(files.size(), 12);

  files.clear();
  RteFsUtils::GetMatchingFiles(files, ".h", dirnameDir, 3, false);
  EXPECT_EQ(files.size(), 3);

  files.clear();
  RteFsUtils::GetMatchingFiles(files, ".bar.h", dirnameDir, 3, true);
  EXPECT_EQ(files.size(), 4);

  RteFsUtils::RemoveDir(dirnameDir);
}

TEST_F(RteFsUtilsTest, GetFilesSorted) {

  CreateInputFiles(dirnameSubdir);

  set<string, VersionCmp::Greater> files;
  RteFsUtils::GetFilesSorted("//invalid_path", files);
  EXPECT_TRUE(files.empty());

  RteFsUtils::GetFilesSorted(dirnameSubdir, files);
  EXPECT_EQ(files, sortedFileSet);

  RteFsUtils::RemoveDir(dirnameSubdir);
}

TEST_F(RteFsUtilsTest, CreateExtendedName) {

  RteFsUtils::CreateTextFile(filenameRegular, bufferFoo);

  string backup = RteFsUtils::CreateExtendedName(filenameRegular, "backup");
  EXPECT_EQ(backup, filenameRegular + "_backup_0");
  RteFsUtils::CreateTextFile(backup, "0");

  backup = RteFsUtils::CreateExtendedName(filenameRegular, "backup");
  EXPECT_EQ(backup, filenameRegular + "_backup_1");
  RteFsUtils::CreateTextFile(backup, "1");

  backup = RteFsUtils::CreateExtendedName(filenameRegular, "backup");
  EXPECT_EQ(backup, filenameRegular + "_backup_2");

  RteFsUtils::RemoveDir(dirnameSubdir);
}

TEST_F(RteFsUtilsTest, AbsolutePath) {
  fs::path path;
  error_code ec;
  const string absFilePath = fs::current_path(ec).append(filenameRegular).generic_string();

  RteFsUtils::CreateTextFile(filenameRegular, "foo");

  // Test empty path
  path = RteFsUtils::AbsolutePath("");
  EXPECT_TRUE(path.generic_string().empty());

  // Test with absolute file path
  path = RteFsUtils::AbsolutePath(absFilePath);
  EXPECT_TRUE(path.is_absolute());

  // Test relative file path
  path = RteFsUtils::AbsolutePath("./" + filenameRegular);
  EXPECT_TRUE(path.is_absolute());
}

TEST_F(RteFsUtilsTest, FindFileRegEx) {
  const string& testdir = dirnameBase + "/FindFileRegEx";
  const string& fileName = testdir + "/test.cdefault.yml";
  RteFsUtils::CreateDirectories(testdir);
  RteFsUtils::CreateTextFile(fileName, "");
  string discoveredFile;
  vector<string> searchPaths = { testdir };
  EXPECT_EQ(true, RteFsUtils::FindFileRegEx(searchPaths, ".*\\.cdefault\\.yml", discoveredFile));
  EXPECT_EQ(fileName, discoveredFile);
  RteFsUtils::RemoveDir(testdir);
}

TEST_F(RteFsUtilsTest, FindFileRegEx_MultipleMatches) {
  const string& testdir = dirnameBase + "/FindFileRegEx";
  const string& fileName1 = testdir + "/test1.cdefault.yml";
  const string& fileName2 = testdir + "/test2.cdefault.yml";
  RteFsUtils::CreateDirectories(testdir);
  RteFsUtils::CreateTextFile(fileName1, "");
  RteFsUtils::CreateTextFile(fileName2, "");
  string finding;
  vector<string> searchPaths = { testdir };
  EXPECT_EQ(false, RteFsUtils::FindFileRegEx(searchPaths, ".*\\.cdefault\\.yml", finding));
  RteFsUtils::RemoveDir(testdir);
}

TEST_F(RteFsUtilsTest, FindFileRegEx_NoMatch) {
  const string& testdir = dirnameBase + "/FindFileRegEx";
  RteFsUtils::CreateDirectories(testdir);
  string finding;
  vector<string> searchPaths = { testdir };
  EXPECT_EQ(false, RteFsUtils::FindFileRegEx(searchPaths, ".*\\.cdefault\\.yml", finding));
  RteFsUtils::RemoveDir(testdir);
}

TEST_F(RteFsUtilsTest, FileCategoryFromExtension) {
  map<string, vector<string>> testDataVec = {
    {"sourceC",      {"sourceFile.c", "sourceFile.C"}},
    {"sourceCpp",    {"sourceFile.cpp", "sourceFile.c++", "sourceFile.C++", "sourceFile.cxx", "sourceFile.cc", "sourceFile.CC"}},
    {"sourceAsm",    {"sourceFile.asm", "sourceFile.s", "sourceFile.S"}},
    {"header",       {"headerFile.h", "headerFile.hpp"}},
    {"library",      {"libraryFile.a", "libraryFile.lib"}},
    {"object",       {"objectFile.o"}},
    {"linkerScript", {"linkerFile.sct", "linkerFile.scf", "linkerFile.ld", "linkerFile.icf"}},
    {"doc",          {"documentFile.txt", "documentFile.md", "documentFile.pdf", "documentFile.htm", "documentFile.html"}},
  };
  for (const auto& [expected, files] : testDataVec) {
    for (const auto& file : files) {
      EXPECT_EQ(RteFsUtils::FileCategoryFromExtension(file), expected);
    }
  }
}

TEST_F(RteFsUtilsTest, GetAbsPathFromLocalUrl) {

  // Absolute dummy path
#ifdef _WIN32
  const string absoluteFilename = "C:/path/to/file.txt";
#else
  const string absoluteFilename = "/path/to/file.txt";
#endif
  vector<string> files = {absoluteFilename, "relative/file", "./relative/file", "../relative/file"};
  for(auto fileName : files) {
    // file itself always passes: relative or absolute
    EXPECT_EQ(fileName, RteFsUtils::GetAbsPathFromLocalUrl(fileName));
  }

  //other support cases only  absolute file names
  const string testUrlLocalHost = "file://localhost/" + absoluteFilename;
  EXPECT_EQ(absoluteFilename, RteFsUtils::GetAbsPathFromLocalUrl(testUrlLocalHost));

  // Empty host
  const string testUrlEmptyHost = "file:///" + absoluteFilename;
  EXPECT_EQ(absoluteFilename, RteFsUtils::GetAbsPathFromLocalUrl(testUrlEmptyHost));

  // Omitted host
  const string testUrlOmittedHost = "file:/" + absoluteFilename;
  EXPECT_EQ(absoluteFilename, RteFsUtils::GetAbsPathFromLocalUrl(testUrlOmittedHost));
}

TEST_F(RteFsUtilsTest, FindFileWithPattern) {
  const string& testdir = dirnameBase + "/FindFileWithPattern";
  const string& fileName = "manifest_1.2.3.yml";
  const string& filePath = testdir + "/" + fileName;
  RteFsUtils::CreateDirectories(testdir);
  RteFsUtils::CreateTextFile(filePath, "");
  string discoveredFile;
  EXPECT_EQ(true, RteFsUtils::FindFileWithPattern(
    testdir, "manifest_(\\d+\\.\\d+\\.\\d+)(.*).yml", discoveredFile));
  EXPECT_EQ(fileName, discoveredFile);
  RteFsUtils::RemoveDir(testdir);
}

TEST_F(RteFsUtilsTest, FindFileWithPattern_NoMatch) {
  const string& testdir = dirnameBase + "/FindFileWithPattern";
  RteFsUtils::CreateDirectories(testdir);
  string discoveredFile;

  EXPECT_EQ(false, RteFsUtils::FindFileWithPattern(testdir, "manifest_(\\d+\\.\\d+\\.\\d+)(.*).yml", discoveredFile));
  RteFsUtils::RemoveDir(testdir);
  EXPECT_TRUE(discoveredFile.empty());
}

TEST_F(RteFsUtilsTest, FindFileWithPattern_InvalidSearchPath) {
  const string& testdir = dirnameBase + "/FindFileWithPattern";
  string discoveredFile;

  EXPECT_EQ(false, RteFsUtils::FindFileWithPattern(testdir, "manifest_(\\d+\\.\\d+\\.\\d+)(.*).yml", discoveredFile));
  RteFsUtils::RemoveDir(testdir);
  EXPECT_TRUE(discoveredFile.empty());
}
// end of RteFsUtilsTest.cpp
