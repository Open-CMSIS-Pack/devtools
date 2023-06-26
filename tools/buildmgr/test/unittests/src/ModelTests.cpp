/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CBuildUnitTestEnv.h"

#include "CbuildKernel.h"
#include "ErrLog.h"
#include "BuildSystemGenerator.h"
#include "RteValueAdjuster.h"
#include "XMLTreeSlim.h"
#include "XmlFormatter.h"

using namespace std;

class ModelTests :public ::testing::Test {
protected:
  void SetUp();
  void TearDown();

  void CheckCreateRte (const TestParam& param, const string& inputDir = examples_folder);
  void CheckRteResults(const TestParam& param, const string& inputDir = examples_folder);
};

void ModelTests::SetUp() {
}

void ModelTests::TearDown() {
  error_code ec;
  CbuildKernel::Destroy();
  fs::current_path(CBuildUnitTestEnv::workingDir, ec);
}

void ModelTests::CheckCreateRte(const TestParam& param, const string& inputDir) {
  error_code ec;
  string filename = inputDir + "/" + param.name;
  string toolchain = "", novalue = "";
  bool ret_val, packMode = false, updateRte = false;
  vector<string> envVars;

  fs::current_path(filename.c_str(), ec);
  filename = param.targetArg + ".cprj";

  ret_val = CreateRte({filename, novalue, novalue, toolchain, novalue, novalue, envVars, packMode, updateRte });
  EXPECT_EQ(ret_val, param.expect) << "CreateRte failed!";
}

void ModelTests::CheckRteResults(const TestParam& param, const string& inputDir) {
  int ret_val;
  ifstream f1, f2;
  string l1, l2;
  string prefix = param.name;
  std::replace(prefix.begin(), prefix.end(), '/', '_');

  string filename1 = testout_folder + "/" + prefix + "_" + param.targetArg + "_Rte_result.txt";
  f1.open(filename1);
  ret_val = f1.is_open();
  ASSERT_EQ(ret_val, true) << "Failed to open " << filename1;

  string filename2 = inputDir + "/" + param.name + "/" + param.targetArg + "_Rte_result.txt.ref";
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
      FAIL() << filename1 << " is different from " << filename2;
    }
  }

  f1.close();
  f2.close();
}

TEST_F(ModelTests, CreateRte_NoPackage) {
  TestParam param = { "", "NoPackage", "", "", false };
  CheckCreateRte    (param, testinput_folder);
}

TEST_F(ModelTests, CreateRte_NoCompiler) {
  TestParam param = { "", "NoCompiler", "", "", false };
  CheckCreateRte    (param, testinput_folder);
}

TEST_F(ModelTests, CreateRte_MultipleCompiler) {
  TestParam param = { "", "MultipleCompiler", "", "", false };
  CheckCreateRte    (param, testinput_folder);
}

TEST_F(ModelTests, CreateRte_UnknownToolChainConfig) {
  TestParam param = { "", "UnknowlToolchainConfig", "", "", false };
  CheckCreateRte(param, testinput_folder);
}

TEST_F(ModelTests, CreateRte_MissingTargetInfo) {
  TestParam param = { "", "MissingTargetInfo", "", "", false };
  CheckCreateRte    (param, testinput_folder);
}

TEST_F(ModelTests, CreateRte_MissingDeviceName) {
  TestParam param = { "", "MissingDeviceName", "", "", false };
  CheckCreateRte    (param, testinput_folder);
}

TEST_F(ModelTests, CheckPackListLocalRepo) {
  string const toolchain = "", novalue = "";
  string const filename = testinput_folder + "/PacklistLocal.cprj";
  string const rtePath = string(CMAKE_SOURCE_DIR) + "/test/local";
  vector<string> envVars;
  EXPECT_TRUE(CreateRte({ filename, rtePath, novalue, toolchain, novalue, novalue, envVars, true, false }));
}

TEST_F(ModelTests, OldCprjSchema) {
  TestParam param = { "", "OldSchema", "", "", false };
  CheckCreateRte    (param, testinput_folder);
}
