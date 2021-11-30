/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
* Integration tests for cbuildgen
*
* The tests validate different configurations and
* features of cbuildgen
*
*/

#include "CBuildGenTestFixture.h"

using namespace std;

class CBuildGenTests : public CBuildGenTestFixture {

};


TEST_F(CBuildGenTests, GenCMake_Fixed_Cprj) {
  TestParam param = { "AC6/Build_AC6", "Simulation", "--update=Simulation.fixed.cprj", "cmake", true };

  RunCBuildGen           (param);
  CheckDescriptionFiles  (testout_folder + "/" + param.name + "/" + param.targetArg + ".fixed.cprj",
                          testout_folder + "/" + param.name + "/" + param.targetArg + ".fixed.cprj.ref");
}

// Validate cbuildgen without build environment set up
TEST_F(CBuildGenTests, RunWithoutEnvArgTest) {
  TestParam param = {
    "AC6/Build_AC6", "Simulation",
    "", "packlist", false };

  RunCBuildGen           (param, examples_folder, false);
}

// Validate cbuildgen with invalid command
TEST_F(CBuildGenTests, InvalidCommandTest) {
  TestParam param = {
    "AC6/Build_AC6", "Simulation",
    "", "Invalid", false
  };

  RunCBuildGen           (param);
}

// Validate generation of CMakeLists file required to build the project
TEST_F(CBuildGenTests, GenCMakeTest) {
  TestParam param = {
    "AC6/Build_AC6", "Simulation",
    "", "cmake", true
  };

  RunCBuildGen           (param);
  CheckCMakeLists        (param);
}

// Validate generation of output files under current output directory
TEST_F(CBuildGenTests, Gen_Output_In_SameDir) {
  vector<string> outDirs{ "OutDir", "./Build" };

  for (auto outdir = outDirs.begin(); outdir != outDirs.end(); ++outdir) {
    TestParam param = {
      "AC6/Build_AC6", "Simulation",
      "--outdir=" + (*outdir), "cmake", true
    };
    error_code ec;
    auto outPath = fs::current_path(ec).append(*outdir);

    if (fs::exists(outPath, ec)) {
      RemoveDir(outPath);
    }

    RunCBuildGen        (param);
    CheckOutputDir      (param, outPath.string());
  }
}

// Validate generation of output files under multiple level
// output directory paths
TEST_F(CBuildGenTests, GenCMake_Under_multipleLevel_OutDir_Test) {
  string outDir = "./Out1/Out2";
  string intDir = "./Int1/Int2";
  TestParam param = {
    "AC6/Build_AC6", "Simulation",
    "--outdir=" + outDir + " --intdir=" + intDir, "cmake", true
  };
  error_code ec;
  auto outPath = fs::current_path(ec).append(outDir);
  auto intPath = fs::current_path(ec).append(intDir);

  if (fs::exists(outPath, ec)) {
    RemoveDir(outPath);
  }

  if (fs::exists(intPath, ec)) {
    RemoveDir(intPath);
  }

  RunCBuildGen             (param);
  CheckOutputDir           (param, outPath.string());
  CheckCMakeIntermediateDir(param, intPath.string());
}

// Validate generation of output files under absolute
// output directory paths
TEST_F(CBuildGenTests, GenCMake_Output_At_Absolute_path) {
  string outDir = testout_folder + "/BuildOutput";
  string intDir = testout_folder + "/BuildIntermediate";
  TestParam param = {
    "AC6/Build_AC6", "Simulation",
    "--outdir=" + outDir + " --intdir=" + intDir, "cmake", true
  };
  error_code ec;
  auto outPath = fs::path(testout_folder + "/" + outDir);
  auto intPath = fs::path(testout_folder + "/" + intDir);

  if (fs::exists(outPath, ec)) {
    RemoveDir(outPath);
  }

  if (fs::exists(intPath, ec)) {
    RemoveDir(intPath);
  }

  RunCBuildGen             (param);
  CheckOutputDir           (param, outDir);
  CheckCMakeIntermediateDir(param, intDir);
}

// Validate generation of output files under relative
// output directory paths
TEST_F(CBuildGenTests, GenCMake_Output_At_Relative_path) {
  error_code ec;
  auto workingDir = fs::current_path(ec);

  // Change working dir
  fs::current_path(testout_folder, ec);

  string outDir = "../RelativeOutput";
  string intDir = "../RelativeIntermediate";

  TestParam param = {
    "AC6/Build_AC6", "Simulation",
    "--outdir=" + outDir + " --intdir=" + intDir, "cmake", true
  };
  auto outPath = fs::current_path(ec).append(outDir);
  auto intPath = fs::current_path(ec).append(intDir);

  if (fs::exists(outPath, ec)) {
    RemoveDir(outPath);
  }

  if (fs::exists(intPath, ec)) {
    RemoveDir(intPath);
  }

  RunCBuildGen             (param);
  CheckOutputDir           (param, outPath.string());
  CheckCMakeIntermediateDir(param, intPath.string());

  // Restore working dir
  fs::current_path(workingDir, ec);
}

// Validate cbuildgen with no arguments
TEST_F(CBuildGenTests, NoArgTest) {
  TestParam param = {
    "AC6/Build_AC6", "Simulation",
    "", "", false };

  RunCBuildGen          (param);
}

// Validate cbuildgen with multiple commands
TEST_F(CBuildGenTests, MultipleCommandsTest) {
  TestParam param = {
    "AC6/Build_AC6", "Simulation",
    "", "packlist cmake", false
  };

  RunCBuildGen          (param);
}

// Validate cbuildgen with no target argument
TEST_F(CBuildGenTests, NoTargetArgTest) {
  TestParam param = {
    "GCC/Build_GCC", "",
    "", "cmake", false
  };

  RunCBuildGen          (param);
}

// Validate cbuildgen with invalid argument
TEST_F(CBuildGenTests, InvalidArgTest) {
  TestParam param = {
    "AC6/Build_AC6", "Simulation",
    "--Test", "cmake", false
  };

  RunCBuildGen          (param);
}

// Validate listing of missing packs in a packlist file
TEST_F(CBuildGenTests, GeneratePackListTest) {
  TestParam param = {
    "ModelTest", "PacklistProject",
    "", "packlist", true
  };

  auto outFile = fs::path(
    testdata_folder + "/" + param.name + "/" + param.targetArg + ".cpinstall");
  error_code ec;

  if (fs::exists(outFile, ec)) {
    fs::remove(outFile, ec);
  }

  RunCBuildGen          (param, testdata_folder);
  CheckCPInstallFile    (param);
}

// Validate listing of missing packs in a packlist file
TEST_F(CBuildGenTests, GeneratePackListDirTest) {
  TestParam param = {
    "ModelTest", "PacklistProject",
    "--intdir=" + testdata_folder + "/" + param.name , "packlist", true
  };
  auto outFile = fs::path(
    testdata_folder + "/" + param.name + "/" + param.targetArg + ".cpinstall");
  error_code ec;

  if (fs::exists(outFile, ec)) {
    fs::remove(outFile, ec);
  }

  RunCBuildGen          (param, testdata_folder);
  CheckCPInstallFile    (param);
}

// Validate CMakelists generation for project with duplicated source filenames
TEST_F(CBuildGenTests, GenCMake_DuplicatedSourceFilename) {
  TestParam param = {
    "Mixed/Minimal_DupSrc", "MyProject",
    "", "cmake", false
  };

  RunCBuildGen          (param);
}

// Validate extraction of layers from project
TEST_F(CBuildGenTests, Layer_Extract) {
  TestParam param = { "Layers/Layers_Extract", "Simulation" };

  RunLayerCommand       (1/*L_EXTRACT*/, param);
  CheckLayerFiles       (param, "RTE");
}

// Validate creation of new projects from layers
TEST_F(CBuildGenTests, Layer_Compose) {
  TestParam param = { "Layers/Layers_Compose", "Simulation" };

  RunLayerCommand       (2/*L_COMPOSE*/, param);
  CheckProjectFiles     (param, "RTE");
}

// Validate addition of new layers to the project
TEST_F(CBuildGenTests, Layer_Add) {
  TestParam param = { "Layers/Layers_Add", "Simulation" };

  RunLayerCommand       (3/*L_ADD*/, param);
  CheckProjectFiles     (param, "RTE");
}

// Validate removal of layers from project
TEST_F(CBuildGenTests, Layer_Remove) {
  TestParam param = { "Layers/Layers_Remove", "Simulation" };

  RunLayerCommand       (4/*L_REMOVE*/, param);
  CheckProjectFiles     (param, "RTE");
}

// Test creation of new directory(s)
TEST_F(CBuildGenTests, MkdirCmdTest)
{
  vector<string> directories = {
    testout_folder + "/AuxCmdTest/0",
    testout_folder + "/AuxCmdTest/1",
    testout_folder + "/AuxCmdTest/2/22",
  };
  string cmd = "mkdir " + directories[0] + " " + directories[1] +
    " " + directories[2];

  RunCBuildGenAux       (cmd, true);

  // Assert that all directories exist
  for (string dir : directories) {
    string cmd = SH + string(" \"[ -d ") + dir + string(" ]\"");
    int ret_val = system(cmd.c_str());
    ASSERT_EQ(ret_val, 0);
  }
}

// Test removal of directory(s)
TEST_F(CBuildGenTests, RmdirCmdTest)
{
  vector<string> directories = {
    testout_folder + "/AuxCmdTest/0",
    testout_folder + "/AuxCmdTest/1",
    testout_folder + "/AuxCmdTest/2/",
    testout_folder + "/AuxCmdTest/2/22/222",
  };
  string except = testout_folder + "/AuxCmdTest/0/00";

  // Create directories
  string cmd = "mkdir " + directories[0] + " " + directories[1] + " " +
    directories[2] + " " + directories[3] + " " + except;
  RunCBuildGenAux(cmd, true);

  // Remove all base 'directories' but 'except'
  cmd = "rmdir " + directories[0] + " " + directories[1] + " " +
    directories[2] + " --except=" + except;

  RunCBuildGenAux       (cmd, true);

  // Assert 'except' directory exist
  cmd = SH + string(" \"[ -d ") + except + string(" ]\"");
  int ret_val = system(cmd.c_str());
  ASSERT_EQ(ret_val, 0);

  // Assert other directories were removed
  cmd = SH + string(" \"[ ! -d ") + directories[1] + string(" ]\"");
  ret_val = system(cmd.c_str());
  ASSERT_EQ(ret_val, 0);

  cmd = SH + string(" \"[ ! -d ") + directories[2] + string(" ]\"");
  ret_val = system(cmd.c_str());
  ASSERT_EQ(ret_val, 0);
}

// Verify create, change and modify timestamps of file using cbuildgen touch
TEST_F(CBuildGenTests, TouchCmdTest)
{
  string file = testout_folder + "/AuxCmdTest/Timestamp.txt";

  // Initialize: remove file if present, create directories if necessary
  error_code ec;
  fs::remove(file, ec);
  fs::create_directories(fs::path(file).parent_path());

  // Assert file does not exist
  string cmd = SH + string(" \"[ ! -f ") + file + string(" ]\"");
  int ret_val = system(cmd.c_str());
  ASSERT_EQ(ret_val, 0);

  // Create file
  cmd = "touch " + file;
  RunCBuildGenAux       (cmd, true);

  // Assert file exists
  cmd = SH + string(" \"[ -f ") + file + string(" ]\"");
  ret_val = system(cmd.c_str());
  ASSERT_EQ(ret_val, 0);

  // Get timestamp1
  auto timestamp1 = fs::last_write_time(file, ec);

  // Touch file
  cmd = "touch " + file;
  RunCBuildGenAux       (cmd, true);

  // Get timestamp2
  auto timestamp2 = fs::last_write_time(file, ec);

  // Assert timestamp was updated
  ASSERT_EQ(timestamp2 != timestamp1, true);
}

// Validate cbuildgen with multiple commands
TEST_F(CBuildGenTests, MultipleAuxCmdTest)
{
  // Multiple commands in the same command line are not accepted
  string cmd = "mkdir rmdir touch";

  RunCBuildGenAux       (cmd, false);
}

