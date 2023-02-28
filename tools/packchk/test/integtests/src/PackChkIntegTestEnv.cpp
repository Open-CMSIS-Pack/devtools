/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "PackChkIntegTestEnv.h"

using namespace std;

string PackChkIntegTestEnv::localtestdata_dir;
string PackChkIntegTestEnv::globaltestdata_dir;
string PackChkIntegTestEnv::testoutput_dir;

void PackChkIntegTestEnv::SetUp() {
  error_code ec;
  localtestdata_dir  = string(TEST_FOLDER) + "data";
  globaltestdata_dir = string(BUILD_FOLDER) + "../test";
  testoutput_dir     = string(BUILD_FOLDER) + "testoutput";

  ASSERT_TRUE(RteFsUtils::Exists(localtestdata_dir));
  ASSERT_TRUE(RteFsUtils::Exists(globaltestdata_dir));

  if (RteFsUtils::Exists(testoutput_dir)) {
    RteFsUtils::RemoveDir(testoutput_dir);
  }

  ASSERT_TRUE(RteFsUtils::CreateDirectories(testoutput_dir));

  localtestdata_dir  = fs::canonical(localtestdata_dir, ec).generic_string();
  globaltestdata_dir = fs::canonical(globaltestdata_dir, ec).generic_string();
  testoutput_dir     = fs::canonical(testoutput_dir, ec).generic_string();
  ASSERT_FALSE(localtestdata_dir.empty());
  ASSERT_FALSE(globaltestdata_dir.empty());
  ASSERT_FALSE(testoutput_dir.empty());

  // Copy Pack.xsd
  const string packXsd = string(PACKXSD_FOLDER) + "/PACK.xsd";
  const string schemaDestDir = string(PROJMGRUNITTESTS_BIN_PATH) + "/../etc";
  if (RteFsUtils::Exists(schemaDestDir)) {
    RteFsUtils::RemoveDir(schemaDestDir);
  }
  RteFsUtils::CreateDirectories(schemaDestDir);
  fs::copy(fs::path(packXsd), fs::path(schemaDestDir + "/PACK.xsd"), ec);
}

void PackChkIntegTestEnv::TearDown() {
}

int main(int argc, char **argv) {
  try {
    testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new PackChkIntegTestEnv);
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
