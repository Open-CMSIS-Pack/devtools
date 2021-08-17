/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CBuildIntegTestEnv.h"

#include "RteFsUtils.h"

#include <algorithm>

using namespace std;

vector<fs::path> AC6Projects = RteFsUtils::FindFiles(testinput_folder + "/MultiTargetAC6", ".cprj");

class MultiTargetAC6Tests : public ::testing::TestWithParam<fs::path> {
public:
  void SetUp                ();
  void RunCBuildScript      (const TestParam& param);
  void RunCBuildScriptCMake (const TestParam& param);
};

void MultiTargetAC6Tests::SetUp() {
  string toolchainPath = CrossPlatformUtils::GetEnv("CI_ARMCC6_TOOLCHAIN_ROOT");
  if (toolchainPath.empty() && !fs::exists(CBuildIntegTestEnv::ac6_toolchain_path)) {
    GTEST_SKIP();
  }
}

void MultiTargetAC6Tests::RunCBuildScript(const TestParam& param) {
  int ret_val;
  error_code ec;
  ASSERT_EQ(true, fs::exists(testout_folder + "/cbuild/bin/cbuild.sh", ec))
    << "error: cbuild.sh not found";

  const string clean = "cd " + testdata_folder + "/" + param.name + " && " +
    SH + " \"source " + testout_folder + "/cbuild/etc/setup && cbuild.sh " +
    param.targetArg + " clean\"";
  ret_val = system(clean.c_str());
  ASSERT_EQ(ret_val, 0);

  const string cmd = "cd " + testdata_folder + "/" + param.name + " && " +
    SH + " \"source " + testout_folder + "/cbuild/etc/setup && cbuild.sh " +
    param.targetArg + "\"";
  ret_val = system(cmd.c_str());
  ASSERT_EQ(ret_val, 0);
}

void MultiTargetAC6Tests::RunCBuildScriptCMake(const TestParam& param) {
  int ret_val;
  error_code ec;
  ASSERT_EQ(true, fs::exists(testout_folder + "/cbuild/bin/cbuild.sh", ec))
    << "error: cbuild.sh not found";

  const string clean = "cd " + testdata_folder + "/" + param.name + " && " +
    SH + " \"source " + testout_folder + "/cbuild/etc/setup && cbuild.sh " +
    param.targetArg + " --cmake clean\"";
  ret_val = system(clean.c_str());
  ASSERT_EQ(ret_val, 0);

  const string cmd = "cd " + testdata_folder + "/" + param.name + " && " +
    SH + " \"source " + testout_folder + "/cbuild/etc/setup && cbuild.sh " +
    param.targetArg + " --cmake\"";
  ret_val = system(cmd.c_str());
  ASSERT_EQ(ret_val, 0);
}

// Value-Parameterized test

// Validate building project with multiple targets
TEST_P(MultiTargetAC6Tests, MultipleTarget) {
  TestParam param = { "MultiTargetAC6", GetParam().filename().generic_string() };

  RunCBuildScript(param);
}

// Validate building multiple target AC6 projects with CMakelists
TEST_P(MultiTargetAC6Tests, CMake) {
  TestParam param = { "MultiTargetAC6", GetParam().filename().generic_string() };

  RunCBuildScriptCMake(param);
}

INSTANTIATE_TEST_SUITE_P(
  MultiTargetAC6Tests,
  MultiTargetAC6Tests,
  ::testing::ValuesIn(AC6Projects),
  [](const testing::TestParamInfo<MultiTargetAC6Tests::ParamType>& info) {
    // Specifying Names for Value-Parameterized Test
    string name = info.param.filename().generic_string();
    auto pos = name.find(".cprj");
    if (pos != string::npos) {
      name.erase(pos, strlen(".cprj"));
    }
    for_each(name.begin(), name.end(), [&](char& c) { if (!isalnum(c))  c = '_'; });
    return name;
});

