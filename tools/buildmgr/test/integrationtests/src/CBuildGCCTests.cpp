/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
* Integration tests for cbuild.sh
*
* The tests validate different configurations and
* features of cbuild.sh with GCC toolchain
*
*/

#include "CBuildTestFixture.h"

using namespace std;

class CBuildGCCTests : public CBuildTestFixture {
};

// Validate successful build of C example
// using latest pack versions available
TEST_F(CBuildGCCTests, Build_GCC_Translation_Control) {
  TestParam param = { "GCC/TranslationControl/Project1", "Project" };

  RunCBuildScriptClean       (param);
  RunCBuildScript            (param);
  CheckCMakeLists            (param);
}

// Validate successful build of C example
// using latest pack versions available
TEST_F(CBuildGCCTests, Build_GCC) {
  TestParam param = { "GCC/Build_GCC", "Simulation" };

  RunCBuildScriptClean       (param);
  RunCBuildScript            (param);
  CheckCMakeLists            (param);
}

// Validate successful compilation of C++ example
TEST_F(CBuildGCCTests, Build_GPP) {
  TestParam param = { "GCC/Build_GPP", "Simulation" };

  RunCBuildScriptClean       (param);
  RunCBuildScript            (param);
  CheckCMakeLists            (param);
}

// Validate assembling sources with legacy and default assemblers
// with and without preprocessing
TEST_F(CBuildGCCTests, Asm) {
  TestParam param = { "GCC/Asm", "Target" };
  RunCBuildScriptClean       (param);
  RunCBuildScript            (param);
  CheckCMakeLists            (param);
}

// Validate compilation of library project
TEST_F(CBuildGCCTests, Library) {
  TestParam param = { "GCC/Library", "project" };

  RunCBuildScriptClean        (param);
  RunCBuildScript             (param);
  CheckCMakeLists             (param);
}

// Validate usage of secure objects in non secure project
TEST_F(CBuildGCCTests, TrustZone_GCC) {
  TestParam param = { "GCC/TrustZone/CM33_s", "FVP_Simulation_Model" };

  RunCBuildScriptClean       (param);
  RunCBuildScript            (param);
  CheckCMakeLists            (param);

  param.name = "GCC/TrustZone/CM33_ns";
  RunCBuildScriptClean       (param);
  RunCBuildScript            (param);
  CheckCMakeLists            (param);
}

// Validate removal of output and intermediate directory
// using "--clean" option
TEST_F(CBuildGCCTests, CleanTest) {
  const string outDir = "OutDir";
  const string intDir = "IntDir";
  TestParam param = { "GCC/Build_GCC", "Simulation", "--outdir=" + outDir + " --intdir=" + intDir, "--clean", true };
  const string absOutDir = examples_folder + "/" + param.name + "/" + outDir;
  const string absIntDir = examples_folder + "/" + param.name + "/" + intDir;
  error_code ec;

  fs::create_directories(fs::path(absOutDir));
  fs::create_directories(fs::path(absIntDir));

  RunCBuildScriptWithArgs    (param);

  EXPECT_EQ(false, fs::exists(absOutDir, ec));
  EXPECT_EQ(false, fs::exists(absIntDir, ec));
}

// Verify cbuild fails on missing project file
TEST_F(CBuildGCCTests, MissingProjectFileTest) {
  TestParam param = { "GCC/Build_GCC", "MissingProject", "", "", false };

  RunCBuildScriptWithArgs    (param);
}

// Verify cbuild fails on invalid project file
TEST_F(CBuildGCCTests, InvalidProjectSchemaTest) {
  TestParam param = { "GCC/Build_GCC", "Invalid_Schema", "", "", false };

  RunCBuildScriptWithArgs    (param);
}

// Test cbuild with invalid option
TEST_F(CBuildGCCTests, InvalidOptionTest) {
  TestParam param = { "GCC/Build_GCC", "Simulation", "-h", "", true };

  RunCBuildScriptWithArgs    (param);
}

// Set output and intermediate directory path and verify
// all files get generated under respective directories
TEST_F(CBuildGCCTests, OutDirGenTest) {
  const string outDir = "OutDir";
  const string intDir = "IntDir";
  TestParam param = { "GCC/Build_GCC", "Simulation", "--outdir=" + outDir + " --intdir=" + intDir, "", true };
  const string absOutDir = examples_folder + "/" + param.name + "/" + outDir;
  const string absIntDir = examples_folder + "/" + param.name + "/" + intDir;
  error_code ec;

  if (fs::exists(absOutDir, ec)) {
    RteFsUtils::RemoveDir(absOutDir);
  }

  if (fs::exists(absIntDir, ec)) {
    RteFsUtils::RemoveDir(absIntDir);
  }

  RunCBuildScriptWithArgs    (param);
  CheckOutputDir             (param, absOutDir);
  CheckCMakeIntermediateDir  (param, absIntDir);
}

// Set output and intermediate directory path with whitespaces and verify
// all files get generated under respective directories
TEST_F(CBuildGCCTests, OutDirGenTestWhitespace) {
  const string outDir = "\\\"Out Dir\\\"";
  const string intDir = "\\\"Int Dir\\\"";
  TestParam param = { "GCC/Build_GCC", "Simulation", "--outdir=" + outDir + " --intdir=" + intDir, "", true };
  const string absOutDir = examples_folder + "/" + param.name + "/Out Dir";
  const string absIntDir = examples_folder + "/" + param.name + "/Int Dir";
  error_code ec;

  if (fs::exists(absOutDir, ec)) {
    RteFsUtils::RemoveDir(absOutDir);
  }

  if (fs::exists(absIntDir, ec)) {
    RteFsUtils::RemoveDir(absIntDir);
  }

  RunCBuildScriptWithArgs    (param);
  CheckOutputDir             (param, absOutDir);
  CheckCMakeIntermediateDir  (param, absIntDir);
}

// Validate project compilation with inclusion of
// files or directory paths having whitespaces
TEST_F(CBuildGCCTests, Whitespace) {
  TestParam param = { "GCC/Whitespace", "Target_Name" };

  RunCBuildScriptClean    (param);
  RunCBuildScript         (param);
  CheckCMakeLists         (param);
}

// Validate project with usage of nested groups and
// inheritance of flags in each group
TEST_F(CBuildGCCTests, NestedGroups) {
  TestParam param = { "GCC/NestedGroups", "Project" };

  RunCBuildScriptClean    (param);
  RunCBuildScript         (param);
  CheckCMakeLists         (param);
}

// Validate project compilation having flag definitions and their usage
TEST_F(CBuildGCCTests, Flags) {
  TestParam param = { "GCC/Flags", "Target" };

  RunCBuildScriptClean    (param);
  RunCBuildScript         (param);
  CheckCMakeLists         (param);
}

// Validate project compilation having specific flag order requirements
TEST_F(CBuildGCCTests, FlagOrder) {
  TestParam param = { "GCC/FlagOrder", "MyProject" };

  RunCBuildScriptClean    (param);
  RunCBuildScript         (param);
  CheckCMakeLists         (param);
}

// Test project with directory inclusion with relative paths
TEST_F(CBuildGCCTests, RelativePath) {
  TestParam param = { "GCC/RelativePath", "Project" };

  RunCBuildScriptClean    (param);
  RunCBuildScript         (param);
  CheckCMakeLists         (param);
}

// Validate project compilation having minimal configurations
TEST_F(CBuildGCCTests, Minimal) {
  TestParam param = { "GCC/Minimal", "MyProject" };

  RunCBuildScriptClean    (param);
  RunCBuildScript         (param);
  CheckCMakeLists         (param);
}

// Validate project with preincluded header paths and their usage
TEST_F(CBuildGCCTests, PreInclude) {
  TestParam param = { "GCC/Pre Include", "Target", "", "", true };

  RunScript("preinclude.sh", testout_folder);
  RunCBuildScriptClean    (param);
  RunCBuildScriptWithArgs (param);
  CheckCMakeLists         (param);
}

