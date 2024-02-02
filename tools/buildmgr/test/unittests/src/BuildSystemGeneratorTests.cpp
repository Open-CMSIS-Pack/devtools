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
  void CheckBuildArtifacts(const string& outPath,
    const vector<string>& testBuildArtifactNames, const bool& exist = false);

  void CreateBuildArtifacts(const string& outPath,
    const vector<string>& testBuildArtifactNames);

private:
  const CbuildModel *m_model;
  void Init();
};

void BuildSystemGeneratorTests::CheckBuildArtifacts(const string& outPath,
  const vector<string>& testBuildArtifactNames, const bool& exist)
{
  for (const auto& fileName : testBuildArtifactNames) {
    string filePath = outPath + "/" + fileName;
    EXPECT_EQ(exist, fs::exists(filePath))
      << "file: '" + filePath + "' does" + (exist ? "" : "not") + "exist";
  }
}

void BuildSystemGeneratorTests::CreateBuildArtifacts(const string& outPath,
  const vector<string>& testBuildArtifactNames)
{
  error_code ec;
  if (fs::exists(outPath, ec)) {
    RteFsUtils::RemoveDir(outPath);
  }

  RteFsUtils::CreateDirectories(outPath);
  for (const auto& fileName : testBuildArtifactNames) {
    string filePath = outPath + "/" + fileName;
    ASSERT_TRUE(RteFsUtils::CreateTextFile(filePath, RteUtils::EMPTY_STRING)) <<
      "unable to create file '" + filePath + "'";
  }
}

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

TEST_F(BuildSystemGeneratorTests, GenAuditFile_WithOut_Existing_Audit_File) {
  error_code ec;
  string file = testout_folder + "/ValidTarget.clog";
  string targetName = m_targetName;

  m_targetName = "ValidTarget";
  vector<string> testBuildArtifactNames{
  m_targetName + ".axf", m_targetName + ".axf.map",
  m_targetName + ".bin", m_targetName + ".hex",
  m_targetName + ".html", "libValidTarget.lib"
  };

  CreateBuildArtifacts(testout_folder, testBuildArtifactNames);
  EXPECT_TRUE(GenAuditFile());
  EXPECT_TRUE(fs::exists(file, ec));
  CheckBuildArtifacts(testout_folder, testBuildArtifactNames);

  // restore target name
  m_targetName = targetName;
}

TEST_F(BuildSystemGeneratorTests, GenAuditFile_With_Existing_Audit_File) {
  error_code ec;
  string file = testout_folder + "/ValidTarget.clog";
  string targetName = m_targetName;

  m_targetName = "ValidTarget";
  vector<string> testBuildArtifactNames{
    m_targetName + ".axf", m_targetName + ".axf.map",
    m_targetName + ".bin", m_targetName + ".hex",
    m_targetName + ".html", "libValidTarget.lib"
  };

  CreateBuildArtifacts(testout_folder, testBuildArtifactNames);
  RteFsUtils::CreateTextFile(file, RteUtils::EMPTY_STRING);

  EXPECT_TRUE(GenAuditFile());
  EXPECT_TRUE(fs::exists(file, ec));
  CheckBuildArtifacts(testout_folder, testBuildArtifactNames, true);

  // restore target name
  m_targetName = targetName;
}
