/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CBuildIntegTestEnv.h"

#include "RteFsUtils.h"

#include <algorithm>

using namespace std;

static const vector<fs::path> IARProjects = RteFsUtils::FindFiles(testinput_folder + "/MultiTargetIAR", ".cprj");

class MultiTargetIARTests : public ::testing::TestWithParam<fs::path> {
public:
  void RunCBuildScript      (const TestParam& param);
};

void MultiTargetIARTests::RunCBuildScript(const TestParam& param) {
  int ret_val;
  error_code ec;
  ASSERT_EQ(true, fs::exists(testout_folder + "/cbuild/bin/cbuild.sh", ec))
    << "error: cbuild.sh not found";

  const string clean = "cd " + testdata_folder + "/" + param.name + " && " +
    SH + " \"source " + testout_folder + "/cbuild/etc/setup && cbuild " +
    param.targetArg + " --clean\"";
  ret_val = system(clean.c_str());
  ASSERT_EQ(ret_val, 0);

  string cmd = "cd " + testdata_folder + "/" + param.name + " && " +
    SH + " \"source " + testout_folder + "/cbuild/etc/setup && cbuildgen cmake --update-rte " +
    param.targetArg + "\"";
  system(cmd.c_str());

  cmd = "cd " + testdata_folder + "/" + param.name + " && " +
    SH + " \"source " + testout_folder + "/cbuild/etc/setup && cbuild " +
    param.targetArg + "\"";
  ret_val = system(cmd.c_str());
  ASSERT_EQ(ret_val, 0);
}

// Value-Parameterized test

// Validate building project with multiple targets
TEST_P(MultiTargetIARTests, MultipleTarget) {
  TestParam param = { "MultiTargetIAR", GetParam().filename().generic_string() };

  RunCBuildScript(param);
}

INSTANTIATE_TEST_SUITE_P(
  MultiTargetIARTests,
  MultiTargetIARTests,
  ::testing::ValuesIn(IARProjects),
  [](const testing::TestParamInfo<MultiTargetIARTests::ParamType>& info) {
    // Specifying Names for Value-Parameterized Test
    string name = info.param.filename().generic_string();
    auto pos = name.find(".cprj");
    if (pos != string::npos) {
      name.erase(pos, strlen(".cprj"));
    }
    for_each(name.begin(), name.end(), [&](char& c) { if (!isalnum(c))  c = '_'; });
    return name;
});

