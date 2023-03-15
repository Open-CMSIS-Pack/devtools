/*
 * Copyright (c) 2023 IAR Systems AB. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
* Integration tests for cbuild.sh
*
* The tests validate different configurations and
* features of cbuild.sh with IAR toolchain
*
*/

#include "CBuildTestFixture.h"

using namespace std;

class CBuildIARTests : public CBuildTestFixture {
public:
  void SetUp();
};

void CBuildIARTests::SetUp() {
  string toolchainPath = CrossPlatformUtils::GetEnv("IAR_TOOLCHAIN_ROOT");
  if (!fs::exists(toolchainPath)) {
    GTEST_SKIP();
  }
}

// Validate successful build of C example
// using latest pack versions available
TEST_F(CBuildIARTests, Build_IAR) {
  TestParam param = { "IAR/Minimal", "MyProject" };

  RunCBuildScriptClean       (param);
  RunCBuildScript            (param);
  CheckCMakeLists            (param);
}

TEST_F(CBuildIARTests, Build_IAR_ASM) {
  TestParam param = { "IAR/Asm", "Asm" };
  RunCBuildScriptClean       (param);
  RunCBuildScript            (param);
}

TEST_F(CBuildIARTests, Build_IAR_MIXED_SOURCE) {
  TestParam param = { "IAR/MixedSource", "Project" };
  RunCBuildScriptClean       (param);
  RunCBuildScript            (param);

  CheckCompileCommand("IAR/MixedSource", "--c++", "test_cxx.cpp");
}

TEST_F(CBuildIARTests, Build_IAR_STATIC_LIB) {
  TestParam param = { "IAR/Library", "Library" };
  RunCBuildScriptClean       (param);
  RunCBuildScript            (param);

  CheckCompileCommand("IAR/Library", "--c++", "calc.cpp");
  CheckCompileCommand("IAR/Library", "-DTest=1", "calc.cpp");
}
