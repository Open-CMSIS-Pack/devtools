/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CBuildUnitTestEnv.h"

#include "ErrOutputterSaveToStdoutOrFile.h"

using namespace std;

string testinput_folder = string(TEST_FOLDER) + "testinput";
string examples_folder  = string(TEST_FOLDER) + "testinput/Examples";
string testout_folder   = fs::current_path().append("testoutput").generic_string();

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

  error_code ec;
  fs::create_directories(testout_folder, ec);

  testinput_folder = fs::canonical(testinput_folder, ec).generic_string();
  examples_folder  = fs::canonical(examples_folder, ec).generic_string();

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
