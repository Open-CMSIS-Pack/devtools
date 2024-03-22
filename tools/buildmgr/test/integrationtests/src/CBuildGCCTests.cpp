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
TEST_F(CBuildGCCTests, Build_GCC_Translation_Control_1) {
  TestParam param = { "GCC/TranslationControl/Project1", "Project" };

  RunCBuildScriptClean       (param);
  RunCBuildScript            (param);
  CheckCMakeLists            (param);
}

TEST_F(CBuildGCCTests, Build_GCC_Translation_Control_3) {
  TestParam param = { "GCC/TranslationControl/Project3", "Project" };

  RunCBuildScriptClean       (param);
  RunCBuildScript            (param);
  CheckCMakeLists            (param);

  // global options : optimize="size" debug="on" warnings="on" languageC="gnu99"
  CheckCompileCommand(param.name, "-Os");
  CheckCompileCommand(param.name, "-g3");
  CheckCompileCommand(param.name, "-std=gnu99");

  // component 'Device::Startup' options
  //   explicit : optimize="none"
  //   inherited : debug="on" warnings="on" languageC="gnu99"
  CheckCompileCommand(param.name, "-O0", "system_ARMCM3.c");
  CheckCompileCommand(param.name, "-g3", "system_ARMCM3.c");
  CheckCompileCommand(param.name, "-std=gnu99", "system_ARMCM3.c");

  // do some random tests
  // File_1.c options
  //   explicit : optimize="size" debug="off" languageC="c11"
  //   inherited : warnings="on"
  CheckCompileCommand(param.name, "-Os", "File_1.c");
  CheckCompileCommand(param.name, "-g0", "File_1.c");
  CheckCompileCommand(param.name, "-std=c11", "File_1.c");

  // File_3.s options
  // explicit : optimize="none" debug="off" warnings="all"
  CheckCompileCommand(param.name, "-O0", "File_3.s");
  CheckCompileCommand(param.name, "-g0", "File_3.s");
  CheckCompileCommand(param.name, "-Wall", "File_3.s");

  // File_4.cpp options
  // explicit : optimize="debug" debug="off" warnings="off" languageCpp="c++17"
  CheckCompileCommand(param.name, "-Og", "File_4.cpp");
  CheckCompileCommand(param.name, "-g0", "File_4.cpp");
  CheckCompileCommand(param.name, "-w", "File_4.cpp");
  CheckCompileCommand(param.name, "-std=c++17", "File_4.cpp");
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

// Validate compilation of library project with custom output
TEST_F(CBuildGCCTests, LibraryCustom) {
  TestParam param = { "GCC/LibraryCustom", "project" };
  RunCBuildScriptClean(param);
  RunCBuildScript(param);
  CheckCMakeLists(param);
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

// Validate project compilation having components sources with same filenames
TEST_F(CBuildGCCTests, DupFilename) {
  TestParam param = { "GCC/DupFilename", "Project" };

  RunCBuildScriptClean(param);
  RunCBuildScript(param);
  CheckCMakeLists(param);
}

// Validate project with preincluded header paths and their usage
TEST_F(CBuildGCCTests, PreInclude) {
  TestParam param = { "GCC/Pre Include", "Target", "", "", true };

  RunScript("preinclude.sh", testout_folder);
  RunCBuildScriptClean    (param);
  RunCBuildScriptWithArgs (param);
  CheckCMakeLists         (param);
}

// Validate RTE custom folder
TEST_F(CBuildGCCTests, Build_GCC_CustomRTE) {
  TestParam param = { "GCC/Build_GCC", "CustomRTE" };
  RunCBuildScriptClean(param);
  RunCBuildScript(param);
  CheckRteDir(param, "Custom/RTEDIR");
}

// Validate project compilation with linker script pre-processing
TEST_F(CBuildGCCTests, LinkerPreProcessing) {
  TestParam param = { "GCC/LinkerPreProcessing", "MyProject" };

  RunCBuildScriptClean(param);
  RunCBuildScript(param);
  CheckCMakeLists(param);
}

// Validate project compilation with linker script pre-processing defines
TEST_F(CBuildGCCTests, LinkerPreProcessingDefines) {
  TestParam param = { "GCC/LinkerPreProcessingDefines", "MyProject" };

  RunCBuildScriptClean(param);
  RunCBuildScript(param);
  CheckCMakeLists(param);
}

// Validate project compilation with linker script ".src" file
TEST_F(CBuildGCCTests, LinkerPreProcessingSrcFile) {
  TestParam param = { "GCC/LinkerPreProcessingSrcFile", "MyProject" };

  RunCBuildScriptClean(param);
  RunCBuildScript(param);
  CheckCMakeLists(param);
}

// Validate project compilation with standard library flags
TEST_F(CBuildGCCTests, StandardLibrary) {
  TestParam param = { "GCC/StandardLibrary", "MyProject" };

  RunCBuildScriptClean(param);
  RunCBuildScript(param);
  CheckCMakeLists(param);
}

// Validate Branch Protection
// TODO: Enable this test case after GCC 12.2.0 release and support completion
/*
TEST_F(CBuildAC6Tests, Build_AC6_BranchProtection) {
  TestParam param = { "AC6/BranchProtection", "Project" };
  RunCBuildScriptClean(param);
  RunCBuildScript(param);
  CheckCMakeLists(param);
  CheckCompileCommand(param.name, "-mbranch-protection=bti");
}
*/
