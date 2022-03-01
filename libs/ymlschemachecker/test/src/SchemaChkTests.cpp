/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "SchemaChecker.h"
#include "SchemaChkTestEnv.h"

#include "gtest/gtest.h"

using namespace std;

class SchemaChkTests : public ::testing::Test {
public:
  SchemaChkTests() {}
  virtual ~SchemaChkTests() {}
};

TEST_F(SchemaChkTests, validate_clayer_yml_schema) {
  string datafile = testinput_folder + "/sample-data/clayer.yaml";
  string schemafile = testinput_folder + "/clayer.schema.json";

  vector<std::pair<int, int>> expectedErrPos = {
    //line, col
    {23,  0},   // additional property 'file_INVALID'
    {19,  0},   // additional property 'fileXXXX'
    { 9, 16},   // regex doesn't match
    { 2,  8}    // unexpected instance type
  };

  SchemaErrors errList;
  EXPECT_FALSE(SchemaChecker::Validate(datafile, schemafile, errList));
  ASSERT_EQ(errList.size(), expectedErrPos.size());
  for (auto& errPos : expectedErrPos) {
    auto errItr = find_if(errList.begin(), errList.end(), [&](const SchemaError& err) {
      return err.m_line == errPos.first && err.m_col == errPos.second;
    });
    EXPECT_TRUE(errList.end() != errItr);
  }
}

TEST_F(SchemaChkTests, validate_cproject_yml_schema) {
  string datafile = testinput_folder + "/sample-data/cproject.yaml";
  string schemafile = testinput_folder + "/cproject.schema.json";

  SchemaErrors errList;
  EXPECT_TRUE(SchemaChecker::Validate(datafile, schemafile, errList));
  EXPECT_EQ(errList.size(), 0);
}

TEST_F(SchemaChkTests, validate_csolution_yml_schema) {
  string datafile = testinput_folder + "/sample-data/csolution.yaml";
  string schemafile = testinput_folder + "/csolution.schema.json";

  SchemaErrors errList;
  EXPECT_TRUE(SchemaChecker::Validate(datafile, schemafile, errList));
  EXPECT_EQ(errList.size(), 0);
}

TEST_F(SchemaChkTests, Invalid_schema) {
  string datafile = testinput_folder + "/sample-data/clayer.yaml";
  string schemafile = testinput_folder + "/invalid-schema.json";

  SchemaErrors errList;
  EXPECT_FALSE(SchemaChecker::Validate(datafile, schemafile, errList));
  ASSERT_EQ(errList.size(), 1);
  EXPECT_EQ(errList.begin()->m_file, schemafile);
  EXPECT_EQ(errList.begin()->m_line, 7);
  EXPECT_EQ(errList.begin()->m_col, 12);
}

TEST_F(SchemaChkTests, Invalid_yml_file) {
  string datafile = testinput_folder + "/sample-data/invalid.yaml";
  string schemafile = testinput_folder + "/clayer.schema.json";

  SchemaErrors errList;
  EXPECT_FALSE(SchemaChecker::Validate(datafile, schemafile, errList));
  ASSERT_EQ(errList.size(), 1);
  EXPECT_EQ(errList.begin()->m_file, datafile);
  EXPECT_EQ(errList.begin()->m_line, 2);
  EXPECT_EQ(errList.begin()->m_col, 3);
}

TEST_F(SchemaChkTests, Schema_Unavailable) {
  string datafile = testinput_folder + "/sample-data/clayer.yaml";
  string schemafile = testinput_folder + "/unavailable.json";

  SchemaErrors errList;
  EXPECT_FALSE(SchemaChecker::Validate(datafile, schemafile, errList));
  ASSERT_EQ(errList.size(), 1);
  EXPECT_EQ(errList.begin()->m_file, schemafile);
}

TEST_F(SchemaChkTests, Data_Unavailable) {
  string datafile = testinput_folder + "/sample-data/unavailable.yaml";
  string schemafile = testinput_folder + "/clayer.schema.json";

  SchemaErrors errList;
  EXPECT_FALSE(SchemaChecker::Validate(datafile, schemafile, errList));
  ASSERT_EQ(errList.size(), 1);
  EXPECT_EQ(errList.begin()->m_file, datafile);
}

TEST_F(SchemaChkTests, SchemaChk_Missing_Required_Property) {
  // Test with missing required properties
  string datafile   = testinput_folder + "/sample-data/missing_required_property.yaml";
  string schemafile = testinput_folder + "/clayer.schema.json";

  vector<std::pair<int, int>> expectedErrPos = {
    //line, col
    {1 , 0},    // required property 'temp' not found
    {2 , 2},    // required property 'component' not found
    {9 , 6}     // required property 'Link' not found
  };

  SchemaErrors errList;
  EXPECT_FALSE(SchemaChecker::Validate(datafile, schemafile, errList));
  ASSERT_EQ(errList.size(), expectedErrPos.size());
  for (auto& errPos : expectedErrPos) {
    auto errItr = find_if(errList.begin(), errList.end(), [&](const SchemaError& err) {
      return (err.m_line == errPos.first && err.m_col == errPos.second);
    });
    EXPECT_TRUE(errList.end() != errItr);
  }
}

TEST_F(SchemaChkTests, SchemaChk_Additional_Property) {
  // Test with additional properties
  string datafile   = testinput_folder + "/sample-data/additional_property.yaml";
  string schemafile = testinput_folder + "/clayer.schema.json";

  vector<std::pair<int, int>> expectedErrPos = {
    //line,col
    {8 , 0},    // additional property 'add-flag'
    {10, 0},    // additional property 'add-prop'
    {27, 0},    // additional property 'project'
    {30, 0}     // additional property 'solution'
  };

  SchemaErrors errList;
  EXPECT_FALSE(SchemaChecker::Validate(datafile, schemafile, errList));
  ASSERT_EQ(errList.size(), expectedErrPos.size());
  for (auto& errPos : expectedErrPos) {
    auto errItr = find_if(errList.begin(), errList.end(), [&](const SchemaError& err) {
      return (err.m_line == errPos.first && err.m_col == errPos.second);
      });
    EXPECT_TRUE(errList.end() != errItr);
  }
}

TEST_F(SchemaChkTests, Schema_Invalid) {
  string datafile = testinput_folder + "/sample-data/invalid_schema.yaml";
  string schemafile = testinput_folder + "/clayer.schema.json";

  vector<std::pair<int, int>> expectedErrPos = {
    //line,col
    {1, 0},     // required property 'layer'
    {1, 0},     // required property 'temp'
    {4, 0},     // additional property 'project'
    {2, 0}      // additional property 'solution'
  };

  SchemaErrors errList;
  EXPECT_FALSE(SchemaChecker::Validate(datafile, schemafile, errList));
  ASSERT_EQ(errList.size(), expectedErrPos.size());
  for (auto& errPos : expectedErrPos) {
    auto errItr = find_if(errList.begin(), errList.end(), [&](const SchemaError& err) {
      return (err.m_line == errPos.first && err.m_col == errPos.second);
      });
    EXPECT_TRUE(errList.end() != errItr);
  }
}
