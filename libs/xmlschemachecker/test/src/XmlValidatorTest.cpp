/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gtest/gtest.h"
#include "XmlChecker.h"

#include <list>
#include <string>

using namespace std;

const string testDataFolder = string(TEST_FOLDER) + "data";

class XmlValidatorTests : public ::testing::Test {
protected:
    virtual void SetUp() {}
    virtual void TearDown() {}
};

// Test case for valid XML file
TEST_F(XmlValidatorTests, validate_pdsc) {
  string packXsd = string(PACKXSD_FOLDER) + "/PACK.xsd";
  string pdscFile = testDataFolder + "/valid.pdsc";

  // Validate XML file against schema
  EXPECT_TRUE(XmlChecker::Validate(pdscFile, packXsd));
}

// Test case for valid XML file
TEST_F(XmlValidatorTests, invalidate_pdsc) {
  string packXsd = string(PACKXSD_FOLDER) + "/PACK.xsd";
  string pdscFile = testDataFolder + "/invalid.pdsc";

  // Validate XML file against schema
  EXPECT_FALSE(XmlChecker::Validate(pdscFile, packXsd));
}
