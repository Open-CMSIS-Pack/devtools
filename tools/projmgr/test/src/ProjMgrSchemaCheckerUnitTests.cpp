/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgr.h"
#include "ProjMgrTestEnv.h"
#include "ProjMgrYamlSchemaChecker.h"

#include "CrossPlatformUtils.h"
#include "gtest/gtest.h"

using namespace std;

class ProjMgrSchemaCheckerUnitTests :
  public ProjMgrYamlSchemaChecker, public ::testing::Test {
protected:
  ProjMgrSchemaCheckerUnitTests() {}
  virtual ~ProjMgrSchemaCheckerUnitTests() {}

  void SetUp();
};

void ProjMgrSchemaCheckerUnitTests::SetUp() {
  m_schemaPath = schema_folder;
}

TEST_F(ProjMgrSchemaCheckerUnitTests, SchemaCheck_Pass) {
  const string& filename = testinput_folder + "/TestProject/test.cproject.yml";

  EXPECT_TRUE(Validate(filename, FileType::PROJECT));
  EXPECT_TRUE(GetErrors().empty());
}

TEST_F(ProjMgrSchemaCheckerUnitTests, SchemaCheck_Fail) {
  vector<std::pair<int, int>> expectedErrPos = {
    // line, col
    {  5  ,  3 }
  };

  const string& filename = testinput_folder +
    "/TestProject/test.cproject_schema_validation_failed.yml";
  EXPECT_FALSE(Validate(filename, FileType::PROJECT));

  // Check errors
  auto errList = GetErrors();
  EXPECT_EQ(errList.size(), expectedErrPos.size());
  for (auto& errPos : expectedErrPos) {
    auto errItr = find_if(errList.begin(), errList.end(),
      [&](const SchemaError& err) {
        return err.m_line == errPos.first && err.m_col == errPos.second;
    });
    EXPECT_TRUE(errList.end() != errItr);
  }
}

TEST_F(ProjMgrSchemaCheckerUnitTests, SchemaCheck_Yaml_File_Not_Found) {
  EXPECT_FALSE(Validate("UNKNOWN.yml", FileType::PROJECT));
}

TEST_F(ProjMgrSchemaCheckerUnitTests, Schemas_Not_Available) {
  m_schemaPath = "INVALID_PATH";
  CoutRedirect coutRedirect;
  const string& expected = "yaml schemas were not found, file cannot be validated";

  const string& filename = testinput_folder + "/TestProject/test.cproject.yml";
  EXPECT_TRUE(Validate(filename, FileType::PROJECT));

  auto outStr = coutRedirect.GetString();
  EXPECT_NE(0, outStr.find(expected));
}
