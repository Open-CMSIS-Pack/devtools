/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrTestEnv.h"
#include "RteFsUtils.h"
#include "CrossPlatformUtils.h"

using namespace std;

string testinput_folder;
string testoutput_folder;
string testcmsispack_folder;

void ProjMgrTestEnv::SetUp() {
  testinput_folder = string(TEST_FOLDER) + "data";
  testoutput_folder = RteFsUtils::GetCurrentFolder() + "output";
  testcmsispack_folder = string(CMAKE_SOURCE_DIR) + "test/packs";
  if (RteFsUtils::Exists(testoutput_folder)) {
    RteFsUtils::RemoveDir(testoutput_folder);
  }
  RteFsUtils::CreateDirectories(testoutput_folder);
  testinput_folder = fs::canonical(testinput_folder).generic_string();
  testoutput_folder = fs::canonical(testoutput_folder).generic_string();
  ASSERT_FALSE(testinput_folder.empty());
  ASSERT_FALSE(testoutput_folder.empty());
  CrossPlatformUtils::SetEnv("CMSIS_PACK_ROOT", testcmsispack_folder);
}

void ProjMgrTestEnv::TearDown() {
  // Reserved
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
