/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CBuildUnitTestEnv.h"

#include "ErrOutputterSaveToStdoutOrFile.h"

using namespace std;

string testdata_folder;
string testinput_folder;
string examples_folder;
string testout_folder;

void RemoveDir(fs::path path)
{
  error_code ec;
  if (fs::is_directory(path, ec)) {
    // Remove Files
    for (auto& p : fs::recursive_directory_iterator(path)) {
      if (fs::is_regular_file(p, ec)) fs::remove(p, ec);
    }

    // Remove child dirs
    for (auto & dir : fs::directory_iterator(path)) {
      fs::remove(dir, ec);
    }

    // Remove parent
    fs::remove(path, ec);
  }
}

const string CBuildUnitTestEnv::workingDir = fs::current_path().generic_string();

void CBuildUnitTestEnv::SetUp() {
  if (!ErrLog::Get()->GetOutputter()) {
    ErrLog::Get()->SetOutputter(new ErrOutputterSaveToStdoutOrFile());
  }

  testdata_folder  = string(TEST_SRC_FOLDER) + "testinput";
  testinput_folder = string(TEST_BUILD_FOLDER) + "testinput";
  examples_folder  = testinput_folder + "/Examples";
  testout_folder   = string(TEST_BUILD_FOLDER) + "testoutput";
  testdata_folder = fs::canonical(testdata_folder).generic_string();
  ASSERT_TRUE(RteFsUtils::Exists(testdata_folder));

  if (RteFsUtils::Exists(testout_folder)) {
    RteFsUtils::RemoveDir(testout_folder);
  }
  error_code ec;
  fs::create_directories(testout_folder, ec);
  testout_folder = fs::canonical(testout_folder).generic_string();
  ASSERT_FALSE(testout_folder.empty());

  if (RteFsUtils::Exists(testinput_folder)) {
    RteFsUtils::RemoveDir(testinput_folder);
  }
  fs::create_directories(testinput_folder, ec);
  testinput_folder = fs::canonical(testinput_folder).generic_string();
  ASSERT_FALSE(testinput_folder.empty());
  // Copy test data from input test folder
  fs::copy(fs::path(testdata_folder), fs::path(testinput_folder), fs::copy_options::recursive, ec);

  examples_folder = fs::canonical(examples_folder).generic_string();
  ASSERT_FALSE(examples_folder.empty());

  // set quiet mode
  InitMessageTable();
  ErrLog::Get()->SetQuietMode();
}

void CBuildUnitTestEnv::TearDown() {
  // reserved
}

int main(int argc, char **argv) {
  try {
    testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new CBuildUnitTestEnv);
    return RUN_ALL_TESTS();
  }
  catch (testing::internal::GoogleTestFailureException const& e) {
    std::cout << "runtime_error: " << e.what();
    return 2;
  }
  catch (...) {
    std::cout << "non-standard exception";
    return 2;
  }
}
