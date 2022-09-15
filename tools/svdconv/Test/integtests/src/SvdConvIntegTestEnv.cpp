/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "SvdConvIntegTestEnv.h"

using namespace std;

string SvdConvIntegTestEnv::localtestdata_dir;
string SvdConvIntegTestEnv::testoutput_dir;

void SvdConvIntegTestEnv::SetUp() {
  error_code ec;
  localtestdata_dir  = string(TEST_FOLDER) + "data";
  testoutput_dir     = string(BUILD_FOLDER) + "testoutput";

  ASSERT_TRUE(RteFsUtils::Exists(localtestdata_dir));

  if (RteFsUtils::Exists(testoutput_dir)) {
    RteFsUtils::RemoveDir(testoutput_dir);
  }

  ASSERT_TRUE(RteFsUtils::CreateDirectories(testoutput_dir));

  localtestdata_dir  = fs::canonical(localtestdata_dir, ec).generic_string();
  testoutput_dir     = fs::canonical(testoutput_dir, ec).generic_string();
  ASSERT_FALSE(localtestdata_dir.empty());
  ASSERT_FALSE(testoutput_dir.empty());
}

void SvdConvIntegTestEnv::TearDown() {
}

int main(int argc, char **argv) {
  try {
    testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new SvdConvIntegTestEnv);
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
