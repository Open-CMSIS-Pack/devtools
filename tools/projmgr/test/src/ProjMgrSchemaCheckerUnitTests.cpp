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

#include <fstream>

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
  const string& filename = testinput_folder + "/TestProject/test_empty_object.cproject.yml";

  EXPECT_TRUE(Validate(filename, FileType::PROJECT));
  EXPECT_TRUE(GetErrors().empty());
}

TEST_F(ProjMgrSchemaCheckerUnitTests, SchemaCheck_Fail) {
  vector<std::pair<int, int>> expectedErrPos = {
    // line, col
    {  11  ,  3 }
  };

  const string& filename = testinput_folder +
    "/TestProject/test_schema_validation_failed.cproject.yml";
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
  const string& filename = testinput_folder + "/TestSolution/test_pack_selection.csolution.yml";

  EXPECT_TRUE(Validate(filename, FileType::SOLUTION));
  EXPECT_TRUE(GetErrors().empty());
}

TEST_F(ProjMgrSchemaCheckerUnitTests, CSolution_SchemaCheck_Fail) {
  const char* invalid_schema_Ex1 = "\
solution:\n\
  created-by: test1.0.0\n\
  created-for: test@1.2\n\
  target-types:\n\
    - type: Test_Target\n\
  projects:\n\
    - project: config.cproject.yml\n\
";

  const char* invalid_schema_Ex2 = "\
solution:\n\
  created-by: astro@1-0.0\n\
  created-for: test@1.2.0.4\n\
  target-types:\n\
    - type: Test_Target\n\
  projects:\n\
    - project: config.cproject.yml\n\
";

  const char* invalid_schema_Ex3 = "\
solution:\n\
  created-by: test\n\
  created-for: test@>=1\n\
  target-types:\n\
    - type: Test_Target\n\
  projects:\n\
    - project: config.cproject.yml\n\
";

  const char* valid_schema_Ex4 = "\
solution:\n\
  created-by: test@1.0.0\n\
  created-for: test@1.2.3+ed5dsd73ks\n\
  target-types:\n\
    - type: Test_Target\n\
  projects:\n\
    - project: config.cproject.yml\n\
";

  typedef std::pair<int, int> ErrInfo;

  // missing required properties
  const char* invalid_schema_Ex5 = "\
solution:\n\
  created-by: test@1.0.0\n\
  created-for: test@1.2.3+ed5dsd73ks\n\
";

  // invalid targettypes, buildtypes and project names
  // with "." character in the name
  const char* invalid_schema_Ex6 = "\
solution:\n\
  target-types:\n\
    # invalid targettype with .\n\
    - type: CM0.Plus\n\
  build-types:\n\
    # invalid buildtype with .\n\
    - type: Debug.Test\n\
  projects:\n\
    # invalid project name with .\n\
    - project: ./TestProject1/test.project.cproject.yml\n\
";

  // invalid targettypes, buildtypes and project names
  // with "+" character in the name
  const char* invalid_schema_Ex7 = "\
solution:\n\
  target-types:\n\
    # invalid targettype with +\n\
    - type: CM0+Plus\n\
  build-types:\n\
    # invalid buildtype with +\n\
    - type: Debug+Test\n\
  projects:\n\
    # invalid project name with +\n\
    - project: test+project.cproject.yml\n\
";

vector<ErrInfo> expectedErrPos = {
    // line, col
    {  2  ,  3 },
    {  3  ,  3 },
  };

  vector<std::tuple<const char*, bool, vector<ErrInfo>>> vecTestData = {
    // data, expectedRetVal, errorPos
    {invalid_schema_Ex1, false, expectedErrPos},
    {invalid_schema_Ex2, false, expectedErrPos },
    {invalid_schema_Ex3, false, expectedErrPos },
    {valid_schema_Ex4,   true,  vector<ErrInfo>{} },
    {invalid_schema_Ex5, false, vector<ErrInfo>{ {1,1}, {1,1}} },
    {invalid_schema_Ex6, false, vector<ErrInfo>{ {4,7}, {7,7}, {10,7}} },
    {invalid_schema_Ex7, false, vector<ErrInfo>{ {4,7}, {7,7}, {10,7}} }
  };

  auto writeFile = [](const string& filePath, const char* data) {
    ofstream fileStream(filePath);
    fileStream << data;
    fileStream << endl;
    fileStream << flush;
    fileStream.close();
  };

  const string& filename = testoutput_folder +
    "/test.csolution_schema_validation.yml";
  for (auto [data, expectRetVal, errorPos] : vecTestData) {
    writeFile(filename, data);
    EXPECT_EQ(expectRetVal, Validate(filename, FileType::SOLUTION)) << "failed for: " << data;

    // Check errors
    auto errList = GetErrors();
    EXPECT_EQ(errList.size(), expectRetVal ? 0 : errorPos.size()) << "failed for: " << data;
    for (auto& err : errList) {
      auto errItr = find_if(errorPos.begin(), errorPos.end(),
        [&](const std::pair<int,int>& errPos) {
          return err.m_line == errPos.first && err.m_col == errPos.second;
        });
      EXPECT_TRUE(errorPos.end() != errItr) << "failed for: " << data;
    }
  }
}

TEST_F(ProjMgrSchemaCheckerUnitTests, SchemaCheck_define) {
  vector<std::pair<int, int>> expectedErrPos = {
    // line, col
    {  10  ,  11 },
    {  11  ,  11 },
    {  12  ,  11 }
  };

  const string& filename = testinput_folder +
    "/TestSolution/test_validate_define_syntax.csolution.yml";
  EXPECT_FALSE(Validate(filename, FileType::SOLUTION));

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
