/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CBuildUnitTestEnv.h"
#include "BuildSystemGenerator.h"

using namespace std;

class BuildSystemGeneratorTests :
  public BuildSystemGenerator, public ::testing::Test {
public:
  BuildSystemGeneratorTests() : m_model(nullptr) {
    Init();
  }
  virtual ~BuildSystemGeneratorTests() {}
protected:
  void SetUp();
  void TearDown();

  void CheckBuildSystemGen_Collect(const TestParam& param,
    const string& inputDir = examples_folder);

private:
  const CbuildModel *m_model;
  void Init();
};

void BuildSystemGeneratorTests::Init() {
  m_outdir = testout_folder + "/";
  m_intdir = testout_folder + "/";
  m_projectName = "ValidTarget";
}

void BuildSystemGeneratorTests::SetUp() {
}

void BuildSystemGeneratorTests::TearDown() {
  error_code ec;
  CbuildKernel::Destroy();
  fs::current_path(CBuildUnitTestEnv::workingDir, ec);
}

void BuildSystemGeneratorTests::CheckBuildSystemGen_Collect(const TestParam& param, const string& inputDir) {
  string filename = inputDir + "/" + param.name;
  string toolchain, ext = CMEXT, novalue = "";
  string packlistDir, output, update;
  vector<string> envVars;
  bool ret_val, packMode = false, updateRte = false;
  error_code ec;

  fs::current_path(filename.c_str(), ec);

  filename = param.targetArg + ".cprj";
  ret_val = CreateRte({filename, novalue, novalue, toolchain, packlistDir, update, envVars, packMode, updateRte});
  ASSERT_EQ(ret_val, param.expect) << "CreateRte failed!";

  m_model = CbuildKernel::Get()->GetModel();

  /* Test */
  ret_val = Collect(filename, m_model, output, "", "");

  ASSERT_EQ(ret_val, param.expect) << "BuildSystemGenerator Collect failed!";
}


TEST_F(BuildSystemGeneratorTests, GetString) {
  set<string> input = {"ARM", "CMSIS", "OUT"};
  string expect = "ARM CMSIS OUT";

  EXPECT_EQ(expect, GetString(input));

  input.clear();
  EXPECT_EQ("", GetString(input));
}

TEST_F(BuildSystemGeneratorTests, StrConv) {
  string path, expected;

  path     = "/C/testdir\\new folder";
  expected = "/C/testdir/new folder";
  path     = StrConv(path);
  EXPECT_EQ(expected, path);

  path     = "/C/test\\dir\\Temp";
  expected = "/C/test/dir/Temp";
  path     = StrConv(path);
  EXPECT_EQ(expected, path);
}

TEST_F(BuildSystemGeneratorTests, StrNorm) {
  string path, expected;

  path     = ".\\mnt\\C\\test dir\\new folder\\";
  expected = "mnt/C/test dir/new folder";
  path     = StrNorm(path);
  EXPECT_EQ(expected, path);

  path     = "//network_path//test dir//doubleslash//";
  expected = "//network_path/test dir/doubleslash";
  path     = StrNorm(path);
  EXPECT_EQ(expected, path);

  path = "/c\\test dir//mixed slash\\/";
  expected = "/c/test dir/mixed slash";
  path = StrNorm(path);
  EXPECT_EQ(expected, path);
}

TEST_F(BuildSystemGeneratorTests, GenAuditFile) {
  error_code ec;
  string file = testout_folder + "/ValidTarget.clog";

  if (fs::exists(file, ec)) {
    fs::remove(file, ec);
  }

  EXPECT_TRUE(GenAuditFile());
  EXPECT_TRUE(fs::exists(file, ec));
}
