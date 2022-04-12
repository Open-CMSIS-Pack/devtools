/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgr.h"
#include "ProjMgrTestEnv.h"
#include "ProjMgrYamlSchemaChecker.h"

#include "CrossPlatformUtils.h"
#include "RteFsUtils.h"
#include "gtest/gtest.h"

using namespace std;

class ProjMgrSchemaCheckerUnitTests :
  public ProjMgrYamlSchemaChecker, public ::testing::Test {
protected:
  ProjMgrSchemaCheckerUnitTests() {}
  virtual ~ProjMgrSchemaCheckerUnitTests() {}
};

TEST_F(ProjMgrSchemaCheckerUnitTests, SchemaCheck_Pass) {
  const string& filename = testinput_folder + "/TestProject/test.cproject.yml";

  EXPECT_TRUE(Validate(filename, FileType::PROJECT));
  EXPECT_TRUE(GetErrors().empty());
}

TEST_F(ProjMgrSchemaCheckerUnitTests, SchemaCheck_Empty_Object) {
  const string& filename = testinput_folder + "/TestProject/test.cproject_empty_object.yml";

  EXPECT_TRUE(Validate(filename, FileType::PROJECT));
  EXPECT_TRUE(GetErrors().empty());
}

TEST_F(ProjMgrSchemaCheckerUnitTests, SchemaCheck_Fail) {
  vector<std::pair<int, int>> expectedErrPos = {
    // line, col
    {  5  ,  0 }
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

TEST_F(ProjMgrSchemaCheckerUnitTests, Schema_Not_Available) {
  string schemaSrcFile  = string(PROJMGRUNITTESTS_BIN_PATH) + "/../etc/cproject.schema.json";
  string schemaDestFile = string(PROJMGRUNITTESTS_BIN_PATH) + "/../etc/cproject.schema.json.bak";
  RteFsUtils::MoveExistingFile(schemaSrcFile, schemaDestFile);

  // Test schema check
  StdStreamRedirect streamRedirect;
  const string& expected = "yaml schemas were not found, file cannot be validated";
  const string& filename = testinput_folder + "/TestProject/test.cproject.yml";
  EXPECT_TRUE(Validate(filename, FileType::PROJECT));
  auto outStr = streamRedirect.GetOutString();
  EXPECT_NE(0, outStr.find(expected));

  // restoring schema file
  RteFsUtils::MoveExistingFile(schemaDestFile, schemaSrcFile);
}

TEST_F(ProjMgrSchemaCheckerUnitTests, Schema_Search_Path_1) {
  // Use schemas from current directory
  string schemaSrcDir = string(PROJMGRUNITTESTS_BIN_PATH) + "/../etc/";
  string schemaDestDir = string(PROJMGRUNITTESTS_BIN_PATH) + "/";
  RteFsUtils::MoveExistingFile(schemaSrcDir + "cproject.schema.json", schemaDestDir + "cproject.schema.json");
  RteFsUtils::MoveExistingFile(schemaSrcDir + "common.schema.json", schemaDestDir + "common.schema.json");

  // Test schema check
  StdStreamRedirect streamRedirect;
  const string& filename = testinput_folder + "/TestProject/test.cproject.yml";
  EXPECT_TRUE(Validate(filename, FileType::PROJECT));
  EXPECT_EQ(GetErrors().size(), 0);

  // restoring schema file
  RteFsUtils::MoveExistingFile(schemaDestDir + "cproject.schema.json", schemaSrcDir + "cproject.schema.json");
  RteFsUtils::MoveExistingFile(schemaDestDir + "common.schema.json", schemaSrcDir + "common.schema.json");
}

TEST_F(ProjMgrSchemaCheckerUnitTests, Schema_Search_Path_2) {
  // Use schema from ../../etc path
  string schemaSrcDir = string(PROJMGRUNITTESTS_BIN_PATH) + "/../etc/";
  string schemaDestDir = string(PROJMGRUNITTESTS_BIN_PATH) + "/../../etc";
  RteFsUtils::MoveExistingFile(schemaSrcDir + "cproject.schema.json", schemaDestDir + "/cproject.schema.json");
  RteFsUtils::MoveExistingFile(schemaSrcDir + "common.schema.json", schemaDestDir + "/common.schema.json");

  // Test schema check
  StdStreamRedirect streamRedirect;
  const string& filename = testinput_folder + "/TestProject/test.cproject.yml";
  EXPECT_TRUE(Validate(filename, FileType::PROJECT));
  EXPECT_EQ(GetErrors().size(), 0);

  // restoring schema file
  RteFsUtils::MoveExistingFile(schemaDestDir + "/cproject.schema.json", schemaSrcDir + "cproject.schema.json");
  RteFsUtils::MoveExistingFile(schemaDestDir + "/common.schema.json", schemaSrcDir + "common.schema.json");
  RteFsUtils::RemoveDir(schemaDestDir);
}

TEST_F(ProjMgrSchemaCheckerUnitTests, SchemaCheck_Pack_Selection) {
  const string& filename = testinput_folder + "/TestSolution/test.csolution_pack_selection.yml";

  EXPECT_TRUE(Validate(filename, FileType::SOLUTION));
  EXPECT_TRUE(GetErrors().empty());
}
