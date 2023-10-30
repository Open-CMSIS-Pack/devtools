/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "YmlSchemaChkTestEnv.h"
#include "RteFsUtils.h"

using namespace std;

string testinput_folder;
string testoutput_folder;

void YmlSchemaChkTestEnv::SetUp() {
  testinput_folder = string(TEST_FOLDER) + "data";
  testoutput_folder = RteFsUtils::GetCurrentFolder() + "output";

  if (RteFsUtils::Exists(testoutput_folder)) {
    RteFsUtils::RemoveDir(testoutput_folder);
  }

  RteFsUtils::CreateDirectories(testoutput_folder);
  testinput_folder = fs::canonical(testinput_folder).generic_string();
  testoutput_folder = fs::canonical(testoutput_folder).generic_string();
  ASSERT_FALSE(testinput_folder.empty());
  ASSERT_FALSE(testoutput_folder.empty());
}

void YmlSchemaChkTestEnv::TearDown() {
  // Reserved
}

int main(int argc, char **argv) {
  try {
    testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new YmlSchemaChkTestEnv);
    return RUN_ALL_TESTS();
  }
  catch (testing::internal::GoogleTestFailureException const& e) {
    cerr << "runtime_error: " << e.what();
    return 2;
  }
  catch (...) {
    cerr << "non-standard exception";
    return 2;
  }
}
// end of YmlSchemaChkTestEnv.cpp
