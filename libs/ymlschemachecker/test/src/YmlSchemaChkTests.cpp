/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "YmlSchemaChecker.h"
#include "YmlSchemaChkTestEnv.h"
#include "YmlTree.h"

#include "gtest/gtest.h"

using namespace std;

class YmlSchemaChkTests : public ::testing::Test {
public:
  YmlSchemaChkTests() {}
  virtual ~YmlSchemaChkTests() {}
};

TEST_F(YmlSchemaChkTests, validate_clayer_yml_schema) {
  string datafile = testinput_folder + "/sample-data/clayer.yaml";
  string schemafile = testinput_folder + "/clayer.schema.json";

  vector<std::pair<int, int>> expectedErrPos = {
    //line, col
    {19, 11},   // additional property 'file_INVALID'
    {23, 15},   // additional property 'fileXXXX'
    { 9,  7},   // regex doesn't match
    { 2,  3}    // unexpected instance type
  };
  YmlSchemaChecker ymlSchemaChecker;
  EXPECT_FALSE(ymlSchemaChecker.ValidateFile(datafile, schemafile));
  auto& errList = ymlSchemaChecker.GetErrors();
  ASSERT_EQ(errList.size(), expectedErrPos.size());
  for (auto& errPos : expectedErrPos) {
    auto errItr = find_if(errList.begin(), errList.end(), [&](const RteError& err) {
      return err.m_line == errPos.first && err.m_col == errPos.second;
    });
    EXPECT_TRUE(errList.end() != errItr);
  }

  // same result if used via YmlTree
  YmlTree ymlTree;
  ymlTree.SetSchemaFileName(schemafile);
  ymlTree.SetSchemaChecker(&ymlSchemaChecker);
  ymlSchemaChecker.ClearErrors();
  EXPECT_FALSE(ymlTree.ParseFile(datafile));
  EXPECT_EQ(ymlTree.GetErrors(), errList.size());
}

TEST_F(YmlSchemaChkTests, validate_cproject_yml_schema) {
  string datafile = testinput_folder + "/sample-data/cproject.yaml";
  string schemafile = testinput_folder + "/cproject.schema.json";

  YmlSchemaChecker ymlSchemaChecker;
  EXPECT_TRUE(ymlSchemaChecker.ValidateFile(datafile, schemafile));
  EXPECT_EQ(ymlSchemaChecker.GetErrors().size(), 0);

  // same result if used via YmlTree
  YmlTree ymlTree;
  ymlTree.SetSchemaFileName(schemafile);
  ymlTree.SetSchemaChecker(&ymlSchemaChecker);
  ymlSchemaChecker.ClearErrors();
  EXPECT_TRUE(ymlTree.ParseFile(datafile));
  EXPECT_EQ(ymlTree.GetErrors(), 0);

}

TEST_F(YmlSchemaChkTests, validate_csolution_yml_schema) {
  string datafile = testinput_folder + "/sample-data/csolution.yaml";
  string schemafile = testinput_folder + "/csolution.schema.json";

  YmlSchemaChecker ymlSchemaChecker;
  EXPECT_TRUE(ymlSchemaChecker.ValidateFile(datafile, schemafile));
  EXPECT_EQ(ymlSchemaChecker.GetErrors().size(), 0);
}

TEST_F(YmlSchemaChkTests, Invalid_schema) {
  string datafile = testinput_folder + "/sample-data/clayer.yaml";
  string schemafile = testinput_folder + "/invalid-schema.json";

  YmlSchemaChecker ymlSchemaChecker;
  EXPECT_FALSE(ymlSchemaChecker.ValidateFile(datafile, schemafile));
  auto& errList = ymlSchemaChecker.GetErrors();

  ASSERT_EQ(errList.size(), 1);
  EXPECT_EQ(errList.begin()->m_file, schemafile);
  EXPECT_EQ(errList.begin()->m_line, 7);
  EXPECT_EQ(errList.begin()->m_col, 12);
}

TEST_F(YmlSchemaChkTests, Invalid_yml_file) {
  string datafile = testinput_folder + "/sample-data/invalid.yaml";
  string schemafile = testinput_folder + "/clayer.schema.json";

  YmlSchemaChecker ymlSchemaChecker;
  EXPECT_FALSE(ymlSchemaChecker.ValidateFile(datafile, schemafile));
  auto& errList = ymlSchemaChecker.GetErrors();
  ASSERT_EQ(errList.size(), 1);
  EXPECT_EQ(errList.begin()->m_file, datafile);
  EXPECT_EQ(errList.begin()->m_line, 2);
  EXPECT_EQ(errList.begin()->m_col, 3);
}

TEST_F(YmlSchemaChkTests, Schema_Unavailable) {
  string datafile = testinput_folder + "/sample-data/clayer.yaml";
  string schemafile = testinput_folder + "/unavailable.json";

  YmlSchemaChecker ymlSchemaChecker;
  EXPECT_FALSE(ymlSchemaChecker.ValidateFile(datafile, schemafile));
  auto& errList = ymlSchemaChecker.GetErrors();
  ASSERT_EQ(errList.size(), 1);
  EXPECT_EQ(errList.begin()->m_file, schemafile);
}

TEST_F(YmlSchemaChkTests, Data_Unavailable) {
  string datafile = testinput_folder + "/sample-data/unavailable.yaml";
  string schemafile = testinput_folder + "/clayer.schema.json";

  YmlSchemaChecker ymlSchemaChecker;
  EXPECT_FALSE(ymlSchemaChecker.ValidateFile(datafile, schemafile));
  auto& errList = ymlSchemaChecker.GetErrors();
  ASSERT_EQ(errList.size(), 1);
  EXPECT_EQ(errList.begin()->m_file, datafile);
}

TEST_F(YmlSchemaChkTests, SchemaChk_Missing_Required_Property) {
  // Test with missing required properties
  string datafile   = testinput_folder + "/sample-data/missing_required_property.yaml";
  string schemafile = testinput_folder + "/clayer.schema.json";

  vector<std::pair<int, int>> expectedErrPos = {
    //line, col
    {1 , 1},    // required property 'temp' not found
    {1 , 1},    // required property 'components' not found
    {9 , 7}     // required property 'Link' not found
  };

  YmlSchemaChecker ymlSchemaChecker;
  EXPECT_FALSE(ymlSchemaChecker.ValidateFile(datafile, schemafile));
  auto& errList = ymlSchemaChecker.GetErrors();
  ASSERT_EQ(errList.size(), expectedErrPos.size());
  for (auto& errPos : expectedErrPos) {
    auto errItr = find_if(errList.begin(), errList.end(), [&](const RteError& err) {
      return (err.m_line == errPos.first && err.m_col == errPos.second);
    });
    EXPECT_TRUE(errList.end() != errItr);
  }
}

TEST_F(YmlSchemaChkTests, SchemaChk_Additional_Property) {
  // Test with additional properties
  string datafile   = testinput_folder + "/sample-data/additional_property.yaml";
  string schemafile = testinput_folder + "/clayer.schema.json";

  vector<std::pair<int, int>> expectedErrPos = {
    //line,col
    {8 , 3},    // additional property 'add-flag'
    {9,  3},    // additional property 'add-prop'
    {27, 1},    // additional property 'project'
    {29, 1}     // additional property 'solution'
  };

  YmlSchemaChecker ymlSchemaChecker;
  EXPECT_FALSE(ymlSchemaChecker.ValidateFile(datafile, schemafile));
  auto& errList = ymlSchemaChecker.GetErrors();
  ASSERT_EQ(errList.size(), expectedErrPos.size());
  for (auto& errPos : expectedErrPos) {
    auto errItr = find_if(errList.begin(), errList.end(), [&](const RteError& err) {
      return (err.m_line == errPos.first && err.m_col == errPos.second);
      });
    EXPECT_TRUE(errList.end() != errItr);
  }
}

TEST_F(YmlSchemaChkTests, Schema_Invalid) {
  string datafile = testinput_folder + "/sample-data/invalid_schema.yaml";
  string schemafile = testinput_folder + "/clayer.schema.json";

  vector<std::pair<int, int>> expectedErrPos = {
    //line,col
    {1, 1},     // required property 'layer'
    {1, 1},     // required property 'temp'
    {3, 1},     // additional property 'project'
    {1, 1}      // additional property 'solution'
  };

  YmlSchemaChecker ymlSchemaChecker;
  EXPECT_FALSE(ymlSchemaChecker.ValidateFile(datafile, schemafile));
  auto& errList = ymlSchemaChecker.GetErrors();
  ASSERT_EQ(errList.size(), expectedErrPos.size());
  for (auto& errPos : expectedErrPos) {
    auto errItr = find_if(errList.begin(), errList.end(), [&](const RteError& err) {
      return (err.m_line == errPos.first && err.m_col == errPos.second);
      });
    EXPECT_TRUE(errList.end() != errItr);
  }
}
// end of YmlSchemaChkTests.cpp
