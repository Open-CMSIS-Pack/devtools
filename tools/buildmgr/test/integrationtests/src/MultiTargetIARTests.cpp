/*
 * Copyright (c) 2023 IAR Systems AB. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CBuildIntegTestEnv.h"
#include "CBuildTestFixture.h"

#include "RteFsUtils.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <map>

using namespace std;

// Read the content of the file that lists the expected outcome.
map<string, vector<string>> ListExpectedOutput() {
  ifstream commandslist;
  string line;
  map<string, vector<string>> results;

  // Read the content of the cmds which is the
  // commands that is expected to be present.
  commandslist.open(testinput_folder + "/MultiTargetIAR/expectedcommands.txt");
  if (!commandslist.is_open()) {
    return {};
  }

  vector<string> cmds;
  string filename;
  while (getline(commandslist, line)) {
    if (line.back() == ':') {
      if (!cmds.empty() && !filename.empty()) {
        results[filename] = cmds;
      }
      filename = line.substr(0, line.size()-1);
      cmds = {};
      continue;
    }
    cmds.push_back(line);
  }

  if (!cmds.empty() && !filename.empty()) {
    results[filename] = cmds;
  }

  return results;
};

vector<fs::path> IARProjects = RteFsUtils::FindFiles(testinput_folder + "/MultiTargetIAR", ".cprj");
map<string,vector<string>> IARExpectedCmds = ListExpectedOutput();

class MultiTargetIARTests : public CBuildTestFixture,
                            public ::testing::WithParamInterface<fs::path> {
public: 
  void SetUp() override;
};

void MultiTargetIARTests::SetUp() {
  string toolchainPath = CrossPlatformUtils::GetEnv("IAR_TOOLCHAIN_ROOT");
  if (toolchainPath.empty()) {
    GTEST_SKIP();
  }
}


// Value-Parameterized test

// Validate building project with multiple targets
TEST_P(MultiTargetIARTests, MultipleTarget) {
  TestParam param = {"../MultiTargetIAR", GetParam().filename().stem().generic_string()};

  RunCBuildScript(param);

  if(IARExpectedCmds.find(GetParam().filename().generic_string()) != IARExpectedCmds.end())
  {
    for(const string& cmd : IARExpectedCmds[GetParam().filename().generic_string()])
    {
        CheckCompileCommand("../MultiTargetIAR", cmd, "MyMain.c");
    }
  } 
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

