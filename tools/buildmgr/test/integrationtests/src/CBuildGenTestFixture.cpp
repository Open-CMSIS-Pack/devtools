/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CBuildGenTestFixture.h"
#include "CbuildUtils.h"

using namespace std;

void CBuildGenTestFixture::CheckCMakeLists(const TestParam& param) {
  int ret_val;
  ifstream f1, f2;
  string l1, l2;

  string filename1 = testout_folder + "/" + param.name + "/IntDir/CMakeLists.txt";
  f1.open(filename1);
  ret_val = f1.is_open();
  ASSERT_EQ(ret_val, true) << "Failed to open " << filename1;

  string filename2 = testout_folder + "/" + param.name + "/CMakeLists.txt.ref";
  f2.open(filename2);
  ret_val = f2.is_open();
  ASSERT_EQ(ret_val, true) << "Failed to open " << filename2;

  while (getline(f1, l1) && getline(f2, l2)) {
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

      FAIL() << filename1 << " is different from " << filename2 << endl << "[" << l1 << " is different from " << l2 << "]";
    }
  }

  f1.close();
  f2.close();
}

void CBuildGenTestFixture::RunCBuildGen(const TestParam& param, string projpath, bool env) {
  string cmd, workingDir;
  error_code ec;

  if (param.name != RteUtils::EMPTY_STRING) {
    if (projpath != examples_folder)
      workingDir = projpath + "/" + param.name;
    else {
      workingDir = testout_folder + "/" + param.name;

      if (fs::exists(workingDir, ec)) {
        RteFsUtils::RemoveDir(workingDir);
      }

      fs::create_directories(workingDir, ec);
      fs::copy(fs::path(examples_folder + "/" + param.name), fs::path(workingDir),
        fs::copy_options::recursive, ec);
    }
  }

  if (!env) {
    cmd = testout_folder + "/cbuild/bin/cbuildgen " +
      (param.targetArg.empty() ? "" : "\"" + workingDir + '/' + param.targetArg + ".cprj\" ") +
      param.command + (param.options.empty() ? "" : " ") + param.options;
  }
  else {
    cmd = SH + string(" \"source ") + testout_folder +
      "/cbuild/etc/setup && cbuildgen \"" + workingDir +
      '/' + param.targetArg + ".cprj\" " + param.command +
      (param.options.empty() ? "" : " ") + param.options + "\"";
  }
  const auto& ret_val = CrossPlatformUtils::ExecCommand(cmd.c_str());
  stdoutStr = ret_val.first;
  ASSERT_EQ(param.expect, (ret_val.second == 0) ? true : false);
}

void CBuildGenTestFixture::RunCBuildGenAux(const string& cmd, bool expect) {
  string command = SH + string(" \"source ") + testout_folder +
    "/cbuild/etc/setup && cbuildgen " + cmd + "\"";
  int ret_val = system(command.c_str());
  ASSERT_EQ(expect, (ret_val == 0) ? true : false);
}

void CBuildGenTestFixture::RunLayerCommand(int cmdNum, const TestParam& param) {
  int ret_val;
  string cmdName, layers, output;
  error_code ec;

  string workingDir = testout_folder + "/" + param.name;
  if (fs::exists(workingDir, ec)) {
    RteFsUtils::RemoveDir(workingDir);
  }

  fs::create_directories(workingDir, ec);
  fs::copy(fs::path(examples_folder + "/" + param.name), fs::path(workingDir),
    fs::copy_options::recursive, ec);

  // clean test case working directory
  workingDir += "/Project";
  if (fs::exists(workingDir, ec) && fs::is_directory(workingDir, ec)) {
    for (auto& p : fs::recursive_directory_iterator(workingDir, ec)) {
      if (fs::is_regular_file(p, ec)) fs::remove(p, ec);
    }
  }
  else {
    fs::create_directories(workingDir, ec);
  }

  // set parameters and copy files
  switch (cmdNum) {
  case 1:
    cmdName = "extract";
    fs::copy(fs::path(workingDir + "/../Project_Full"), fs::path(workingDir), fs::copy_options::recursive, ec);
    output = " --outdir=" + workingDir + "/Layer";
    break;
  case 2:
    cmdName = "compose";
    layers = workingDir + "/../Layer_Ref/application/application.clayer " + workingDir + "/../Layer_Ref/device/device.clayer" +
      " --name=ProjectName --description=\\\"Project Description\\\"";
    break;
  case 3:
    cmdName = "add";
    layers = workingDir + "/../Layer_Ref/device/device.clayer";
    fs::copy(fs::path(workingDir + "/../Project_Partial"), fs::path(workingDir), fs::copy_options::recursive, ec);
    break;
  case 4:
    cmdName = "remove";
    layers = "--layer=device";
    fs::copy(fs::path(workingDir + "/../Project_Full"), fs::path(workingDir), fs::copy_options::recursive, ec);
    break;
  }

  string cmd = "bash -c \"source " + testout_folder + "/cbuild/etc/setup && cbuildgen " + workingDir + "/" + param.targetArg + ".cprj " +
    cmdName + " " + layers + output + "\"";
  ret_val = system(cmd.c_str());
  ASSERT_EQ(ret_val, 0) << "cmd: " << cmd << endl;
}

void CBuildGenTestFixture::CheckDescriptionFiles(const string& filename1, const string& filename2) {
  int ret_val;
  ifstream f1, f2;
  string l1, l2;

  f1.open(filename1);
  ret_val = f1.is_open();
  ASSERT_EQ(ret_val, true) << "Failed to open " << filename1;

  f2.open(filename2);
  ret_val = f2.is_open();
  ASSERT_EQ(ret_val, true) << "Failed to open " << filename2;

  string f1Name = RteUtils::ExtractFileName(filename1);
  string f2Name = RteUtils::ExtractFileName(filename2);

  bool equal = true;
  size_t lineNumber = 0;
  while (getline(f1, l1) && getline(f2, l2)) {
    lineNumber++;
    // remove eol cr
    if (!l1.empty() && l1.rfind('\r') == l1.length() - 1) {
      l1.pop_back();
    }

    if (!l2.empty() && l2.rfind('\r') == l2.length() - 1) {
      l2.pop_back();
    }

    if (l1 != l2) {
      // ignore 'timestamp'
      if ((!l1.empty() && (l1.find("timestamp=") != string::npos)) && (!l2.empty() && (l2.find("timestamp=") != string::npos))) {
        continue;
      }

      // ignore 'used file'
      if ((!l1.empty() && (l1.find("used file=") != string::npos)) && (!l2.empty() && (l2.find("used file=") != string::npos))) {
        continue;
      }
      equal = false;
      EXPECT_TRUE(equal)
        << f1Name << "(" << lineNumber << "):" << endl
        << l1 << endl
        << " is different from " << endl <<
        f2Name << "(" << lineNumber << "):" << endl
        << l2 << endl;

    }
  }
  if (!equal) {
    FAIL() << filename1 << endl
      << " is different from " << endl
      << filename2 << endl;
  }
  f1.close();
  f2.close();
}


void CBuildGenTestFixture::GetDirectoryItems(const string& inPath, set<string> &result, const string& ignoreDir) {
  string itemPath;
  for (auto& item : fs::directory_iterator(inPath)) {
    if (fs::is_directory(fs::status(item))) {
      if (item.path().filename().compare(ignoreDir) == 0) {
        continue;
      }
      GetDirectoryItems(item.path().generic_string(), result, ignoreDir);
    }
    itemPath = item.path().generic_string();
    result.insert(itemPath.substr(inPath.length() + 1, itemPath.length()));
  }
}

void CBuildGenTestFixture::CheckLayerFiles(const TestParam& param, const string& rteFolder) {
  string layerDir = testout_folder + "/" + param.name + "/Project/Layer";
  string layerRef = testout_folder + "/" + param.name + "/Layer_Ref";
  error_code ec;
  set<string> dir, ref, clayers, clayers_ref;

  GetDirectoryItems(layerDir, dir, rteFolder);
  GetDirectoryItems(layerRef, ref, rteFolder);

  for (auto& p : fs::recursive_directory_iterator(layerDir, ec)) {
    if (p.path().extension() == ".clayer") {
      clayers.insert(p.path().generic_string());
    }
  }

  for (auto& p : fs::recursive_directory_iterator(layerRef, ec)) {
    if (p.path().extension() == ".clayer") {
      clayers_ref.insert(p.path().generic_string());
    }
  }

  ASSERT_EQ(dir == ref, true) << "Layer directory '" << layerDir
    << "' filetree is different from '" << layerRef << "' reference";

  auto clayer = clayers.begin();
  auto clayer_ref = clayers_ref.begin();

  for (; clayer != clayers.end() && clayer_ref != clayers_ref.end(); clayer++, clayer_ref++) {
    CheckDescriptionFiles(*clayer, *clayer_ref);
  }
}

void CBuildGenTestFixture::CheckProjectFiles(const TestParam& param, const string& rteFolder) {
  string projectDir = testout_folder + "/" + param.name + "/Project";
  string projectRef = testout_folder + "/" + param.name + "/Project_Ref";
  error_code ec;
  set<string> dir, ref;

  GetDirectoryItems(projectDir, dir, rteFolder);
  GetDirectoryItems(projectRef, ref, rteFolder);

  ASSERT_EQ(dir == ref, true) << "Project directory '" << projectDir
    << "' filetree is different from '" << projectRef << "' reference";

  CheckDescriptionFiles(projectDir + "/" + param.targetArg + ".cprj", projectRef + "/" + param.targetArg + ".cprj");
}

void CBuildGenTestFixture::CheckOutputDir(const TestParam& param, const string& outdir) {
  error_code ec;
  EXPECT_EQ(param.expect, fs::exists(outdir + "/" + param.targetArg + ".clog", ec))
    << "File " << param.targetArg << ".clog does " << (param.expect ? "not " : "") << "exist!";
}

void CBuildGenTestFixture::CheckCMakeIntermediateDir(const TestParam& param, const string& intdir) {
  error_code ec;
  EXPECT_EQ(param.expect, fs::exists(intdir + "/CMakeLists.txt", ec))
    << "File CMakeLists.txt does " << (param.expect ? "not " : "") << "exist!";
}

void CBuildGenTestFixture::CheckCPInstallFile(const TestParam& param, bool json) {
  int ret_val;
  ifstream f1, f2;
  string l1, l2;

  string filename1 = testdata_folder + "/" + param.name + "/" + param.targetArg + ".cpinstall" + (json ? ".json" : "");
  f1.open(filename1);
  ret_val = f1.is_open();
  ASSERT_EQ(ret_val, true) << "Failed to open " << filename1;

  string filename2 = filename1 + ".ref";
  f2.open(filename2);
  ret_val = f2.is_open();
  ASSERT_EQ(ret_val, true) << "Failed to open " << filename2;

  while (getline(f1, l1) && getline(f2, l2)) {
    if (!l1.empty() && l1.rfind('\r') == l1.length() - 1) {
      l1.pop_back(); // remove eol cr
    }

    if (!l2.empty() && l2.rfind('\r') == l2.length() - 1) {
      l2.pop_back();
    }

    size_t s;
    if ((s = l1.find("https", 0)) != string::npos) {
      l1.replace(s, 5, "http"); // replace 'https' by 'http'
    }

    if ((s = l2.find("https", 0)) != string::npos) {
      l2.replace(s, 5, "http");
    }

    if (l1 != l2) {
      FAIL() << filename1 << " is different from " << filename2;
    }
  }

  f1.close();
  f2.close();
}
