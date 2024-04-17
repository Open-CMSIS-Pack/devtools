/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CBuildTestFixture.h"

using namespace std;

void CBuildTestFixture::RunCBuildScript(const TestParam& param) {
  int ret_val;
  error_code ec;
  ASSERT_EQ(true, fs::exists(testout_folder + "/cbuild/bin/cbuild.sh", ec))
    << "error: cbuild.sh not found";

  string cmd = "cd " + examples_folder + "/" + param.name + " && " +
    SH + " \"source " + testout_folder + "/cbuild/etc/setup && cbuildgen cmake --update-rte " +
    param.targetArg + ".cprj\"";
  system(cmd.c_str());

  cmd = "cd " + examples_folder + "/" + param.name + " && " +
    SH + " \"source " + testout_folder + "/cbuild/etc/setup && cbuild " +
    param.targetArg + ".cprj\"";
  ret_val = system(cmd.c_str());
  ASSERT_EQ(ret_val, 0);
}

void CBuildTestFixture::RunCBuildScriptClean(const TestParam& param) {
  int ret_val;
  error_code ec;
  string cmd = "cd \"" + examples_folder + "/" + param.name + "\" && " +
    SH + " \"source " + testout_folder + "/cbuild/etc/setup && cbuild " +
    param.targetArg + ".cprj --clean\"";
  ret_val = system(cmd.c_str());
  ASSERT_EQ(ret_val, 0);

  fs::remove(fs::path(examples_folder + "/" + param.name + "/" +
    param.targetArg + "CMakeLists.txt"), ec);
  fs::remove(fs::path(examples_folder + "/" + param.name + "/" +
    param.targetArg + ".clog"), ec);
}

void CBuildTestFixture::RunCBuildScriptWithArgs(const TestParam& param) {
  int ret_val;
  string cmd = "cd \"" + examples_folder + "/" + param.name + "\" && " +
    SH + " \"source " + testout_folder + "/cbuild/etc/setup && cbuildgen cmake --update-rte " +
    param.targetArg + ".cprj\"";
  system(cmd.c_str());

  cmd = "cd \"" + examples_folder + "/" + param.name + "\" && " +
    SH + " \"source " + testout_folder + "/cbuild/etc/setup && cbuild" +
    (param.targetArg.empty() ? "" : " " + param.targetArg + ".cprj") +
    (param.command.empty() ? "" : " " + param.command) +
    (param.options.empty() ? "" : " " + param.options) + "\"";
  ret_val = system(cmd.c_str());
  ASSERT_EQ(param.expect, (ret_val == 0) ? true : false);
}

void CBuildTestFixture::CheckCMakeLists(const TestParam& param) {
  int ret_val;
  ifstream f1, f2;
  string l1,l2;

  string filename1 = examples_folder + "/" + param.name + "/IntDir/CMakeLists.txt";
  f1.open(filename1);
  ret_val = f1.is_open();
  ASSERT_EQ(ret_val, true) << "Failed to open " << filename1;

  string filename2 = examples_folder + "/" + param.name + "/CMakeLists.txt.ref";
  f2.open(filename2);
  ret_val = f2.is_open();
  ASSERT_EQ(ret_val, true) << "Failed to open " << filename2;

  while (getline(f1,l1) && getline(f2,l2)) {
    if (!l1.empty() && l1.rfind('\r') == l1.length() - 1) {
      l1.pop_back(); // remove eol cr
    }

    if (!l2.empty() && l2.rfind('\r') == l2.length() - 1) {
      l2.pop_back();
    }

    if (l1 != l2) {
      // ignore commented lines
      if ((!l1.empty() && l1.at(0) == '#') && (!l2.empty() && l2.at(0) == '#')) {
        continue;
      }

      // ignore lines containing forward slashes (paths)
      if ((!l1.empty() && (l1.find("/") != string::npos)) && (!l2.empty() && (l2.find("/") != string::npos))) {
        continue;
      }

      FAIL() << "error: " << filename1 << " is different from " << filename2 << "\nLine1: " << l1 << "\nLine2: " << l2;
    }
  }

  f1.close();
  f2.close();
}

void CBuildTestFixture::CheckMapFile(const TestParam& param) {
  int ret_val;
  ifstream f1, f2;
  string l1,l2;

  string filename1 = examples_folder + "/" + param.name + "/OutDir/" +
    fs::path(param.name).filename().generic_string() + ".map";
  f1.open(filename1);
  ret_val = f1.is_open();
  ASSERT_EQ(ret_val, true) << "Failed to open " << filename1;

  string filename2 = examples_folder + "/" + param.name + "/" +
    fs::path(param.name).filename().generic_string() + ".map.ref";
  f2.open(filename2);
  ret_val = f2.is_open();
  ASSERT_EQ(ret_val, true) << "Failed to open " << filename2;

  f1.close();
  f2.close();
}

void CBuildTestFixture::CheckOutputDir(const TestParam& param, const string& outdir) {
  error_code ec;
  EXPECT_EQ(param.expect, fs::exists(outdir + "/" + param.targetArg + ".clog", ec))
    << "File " << param.targetArg << ".clog does " << (param.expect ? "not " : "") << "exist!";
}

void CBuildTestFixture::CheckRteDir(const TestParam& param, const string& rtedir) {
  error_code ec;
  string rteDir = examples_folder + "/" + param.name + rtedir;
  EXPECT_EQ(param.expect, fs::exists(rteDir, ec))
    << "Folder " << rteDir << ".does " << (param.expect ? "not " : "") << "exist!";
}

void CBuildTestFixture::CheckCMakeIntermediateDir(const TestParam& param, const string& intdir) {
  error_code ec;
  EXPECT_EQ(param.expect, fs::exists(intdir + "/CMakeLists.txt", ec))
    << "File CMakeLists.txt does " << (param.expect ? "not " : "") << "exist!";
}

void CBuildTestFixture::CleanOutputDir(const TestParam& param) {
  string path = fs::path(examples_folder + "/" + param.name).string();
  error_code ec;

  fs::path file(path + "/" + param.targetArg + ".clog");
  if (fs::exists(file, ec)) {
    fs::remove(file, ec);
  }

  file = fs::path(path + "/CMakeLists.txt");
  if (fs::exists(file, ec)) {
    fs::remove(file, ec);
  }
}

void CBuildTestFixture::CheckCompileCommand(const string& projectName, const string& cmdOption, const string& srcFile) {
  int ret_val;
  ifstream compileCommands;
  string line;

  string compileCommandsFilename = examples_folder + "/" + projectName + "/IntDir/compile_commands.json";
  compileCommands.open(compileCommandsFilename);
  ret_val = compileCommands.is_open();
  ASSERT_TRUE(ret_val) << "Failed to open " << compileCommandsFilename;

  bool found = false;
  while (getline(compileCommands, line)) {
    if ((line.find("\"command\":") != string::npos) &&
      (line.find(cmdOption) != string::npos)) {
      if (srcFile.empty() || 
        (getline(compileCommands, line) &&
        (line.find("\"file\":") != string::npos) &&
        (line.find(srcFile) != string::npos))) {
        found = true;
        break;
      }
    }
  }

  ASSERT_TRUE(found) << "Compiler option '" << cmdOption << (srcFile.empty()? "" : "' for file '" + srcFile) << "' was not found";
  compileCommands.close();
}
