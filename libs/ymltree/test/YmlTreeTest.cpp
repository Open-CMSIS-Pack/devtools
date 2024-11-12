/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "gtest/gtest.h"
#include "YmlTree.h"

#include "RteUtils.h"
#include "RteFsUtils.h"
#include "XmlFormatter.h"
#include "YmlFormatter.h"

#include <vector>
#include <list>
#include <string>

using namespace std;

const string XML_HEADER = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?>\n";
// Directories
const string dirnameBase = "YmlTreeTest";
const string dirnameFiles = dirnameBase + "/files";

// Filenames
const string ymlIn = dirnameFiles + "/in.yml";
const string ymlOut = dirnameFiles + "/out.yml";

// constants
static string theYmlString = "build-idx:\n\
  generated-by: csolution version 0.0.0+g11955b66\n\
  cdefault: ${CMSIS_COMPILER_ROOT}/cdefault.yml\n\
  csolution: ../../data/TestDefault/empty.csolution.yml\n\
  cprojects:\n\
    - cproject: ../../data/TestDefault/project.cproject.yml\n\
  cbuilds:\n\
    - cbuild: project.Debug+TEST_TARGET.cbuild.yml\n\
      project: project\n\
      configuration: .Debug+TEST_TARGET\n\
    - cbuild: project.Release+TEST_TARGET.cbuild.yml\n\
      project: project\n\
      configuration: .Release+TEST_TARGET";

class YmlTreeTestF :public ::testing::Test {
protected:
  void SetUp()   override
  {
    RteFsUtils::DeleteTree(dirnameBase);
    RteFsUtils::CreateDirectories(dirnameFiles);
  }
  void TearDown() override
  {
    RteFsUtils::DeleteTree(dirnameBase);
  }

};

TEST(YmlTreeTest, InvalidInput) {
  const string yaml_input ="invalid: 1 : 2";

  YmlTree tree;
  EXPECT_FALSE(tree.ParseString(yaml_input));
  auto& errs = tree.GetErrorStrings();
  ASSERT_EQ(errs.size(), 1);
  EXPECT_EQ("(1,12):illegal map value", *errs.begin());
}

TEST(YmlTreeTest, InvalidFile) {
  YmlTree tree;
  EXPECT_FALSE(tree.ParseFile("BadFood"));
  auto& errs = tree.GetErrorStrings();
  ASSERT_EQ(errs.size(), 1);
  EXPECT_EQ((*errs.begin()).find("BadFood(0,0):bad file:"), 0);
}


TEST(YmlTreeTest, Sequence) {
  const string yaml_input =
"prime:\n\
  - two\n\
  - three\n\
  - seven";

  const string expected_output = XML_HEADER +
"<prime>\n\
  <->two</->\n\
  <->three</->\n\
  <->seven</->\n\
</prime>\n";

  YmlTree tree;
  EXPECT_TRUE(tree.ParseString(yaml_input));
  EXPECT_TRUE(tree.GetRoot());
  XMLTreeElement* root = tree.GetRoot() ? tree.GetRoot()->GetFirstChild() : nullptr;
  EXPECT_TRUE(root);

  XmlFormatter xmlFormatter(false);
  string xmlContent = xmlFormatter.FormatElement(root);
  EXPECT_EQ(xmlContent, expected_output);

  YmlFormatter ymlFormatter;
  string ymlContent = ymlFormatter.FormatElement(root);
  EXPECT_EQ(ymlContent, yaml_input);
}


TEST(YmlTreeTest, KeyVal) {
  const string yaml_input = "key: val";
  const string expected_output = XML_HEADER + "<key>val</key>\n";

  YmlTree tree;
  EXPECT_TRUE(tree.ParseString(yaml_input));
  EXPECT_TRUE(tree.GetRoot());
  XMLTreeElement* root = tree.GetRoot() ? tree.GetRoot()->GetFirstChild() : nullptr;
  EXPECT_TRUE(root);

  XmlFormatter xmlFormatter;
  string xmlContent = xmlFormatter.FormatElement(root);
  EXPECT_EQ(xmlContent, expected_output);

  YmlFormatter ymlFormatter;
  string ymlContent = ymlFormatter.FormatElement(root);
  EXPECT_EQ(ymlContent, yaml_input);

  const string yaml_input1 = "key1: val1";
  const string expected_output1 = XML_HEADER + "<key1>val1</key1>\n";

  tree.Clear();
  EXPECT_TRUE(tree.ParseString(yaml_input1));
  EXPECT_TRUE(tree.GetRoot());
  root = tree.GetRoot() ? tree.GetRoot()->GetFirstChild() : nullptr;
  EXPECT_TRUE(root);
  EXPECT_EQ(root->GetTag(), "key1");
  EXPECT_EQ(root->GetText(), "val1");

  xmlContent = xmlFormatter.FormatElement(root);
  EXPECT_EQ(xmlContent, expected_output1);
  string ymlContent1 = ymlFormatter.FormatElement(root);
  EXPECT_EQ(ymlContent1, yaml_input1);

  const string yaml_input2 = "key2: "; // null node type  (trailing space is intentional because added by emitter)
  const string expected_output2 = XML_HEADER + "<key2/>\n";

   tree.Clear();
  EXPECT_TRUE(tree.ParseString(yaml_input2));
  EXPECT_TRUE(tree.GetRoot());
  root = tree.GetRoot() ? tree.GetRoot()->GetFirstChild() : nullptr;
  EXPECT_TRUE(root);
  EXPECT_EQ(root->GetTag(), "key2");
  EXPECT_EQ(root->GetText(), "");

  xmlContent = xmlFormatter.FormatElement(root);
  EXPECT_EQ(xmlContent, expected_output2);
  string ymlContent2 = ymlFormatter.FormatElement(root);
  EXPECT_EQ(ymlContent2, yaml_input2);

}

TEST(YmlTreeTest, Map) {
  const string yaml_input =
 "map:\n\
  nul: \n\
  one: 1\n\
  two: 2";

  const string json_input = "map: {nul:, one: 1, two: 2}\n";

  const string expected_output = XML_HEADER + "<map nul=\"\" one=\"1\" two=\"2\"/>\n";

  YmlTree tree;
  EXPECT_TRUE(tree.ParseString(yaml_input));
  EXPECT_TRUE(tree.GetRoot());
  XMLTreeElement* root = tree.GetRoot() ? tree.GetRoot()->GetFirstChild() : nullptr;
  EXPECT_TRUE(root);

  XmlFormatter xmlFormatter(false);
  string xmlContent = xmlFormatter.FormatElement(root);
  EXPECT_EQ(xmlContent, expected_output);
  tree.Clear();

  EXPECT_TRUE(tree.ParseString(json_input));
  EXPECT_TRUE(tree.GetRoot());
  root = tree.GetRoot() ? tree.GetRoot()->GetFirstChild() : nullptr;
  EXPECT_TRUE(root);

  xmlContent = xmlFormatter.FormatElement(root);
  EXPECT_EQ(xmlContent, expected_output);

  YmlFormatter ymlFormatter;
  string ymlContent = ymlFormatter.FormatElement(root);
  EXPECT_EQ(ymlContent, yaml_input);
}

TEST(YmlTreeTest, Nested) {
  const string yaml_input =
 "nested:\n\
  one: 1\n\
  two: 2\n\
  three:\n\
    s_null: \n\
    s_one: 3.1\n\
    s_two: 3.2\n\
  four:\n\
    - 4.1\n\
    - 4.2.a: a\n\
      4.2.b: b";
  const string expected_output = XML_HEADER +
    "<nested one=\"1\" two=\"2\">\n\
  <three s_null=\"\" s_one=\"3.1\" s_two=\"3.2\"/>\n\
  <four>\n\
    <->4.1</->\n\
    <- 4.2.a=\"a\" 4.2.b=\"b\"/>\n\
  </four>\n\
</nested>\n";

  YmlTree tree;
  EXPECT_TRUE(tree.ParseString(yaml_input));
  EXPECT_TRUE(tree.GetRoot());
  XMLTreeElement* root = tree.GetRoot() ? tree.GetRoot()->GetFirstChild() : nullptr;
  EXPECT_TRUE(root);

  XmlFormatter xmlFormatter(false);
  string xmlContent = xmlFormatter.FormatElement(root);
  EXPECT_EQ(xmlContent, expected_output);

  YmlFormatter ymlFormatter;
  string ymlContent = ymlFormatter.FormatElement(root);
  EXPECT_EQ(ymlContent, yaml_input);
}


TEST_F(YmlTreeTestF, ReadFileDefault) {

  bool success = RteFsUtils::CopyBufferToFile(ymlIn, theYmlString, false);
  EXPECT_TRUE(success);

  YmlTree tree;
  success = tree.ParseFile(ymlIn);

  EXPECT_TRUE(success);
  EXPECT_TRUE(tree.GetRoot());
  XMLTreeElement* root = tree.GetRoot() ? tree.GetRoot()->GetFirstChild() : nullptr;
  ASSERT_TRUE(root);
  EXPECT_EQ(root->GetRootFileName(), ymlIn);

  XmlFormatter xmlFormatter;
  string xmlContent = xmlFormatter.FormatElement(root);

  tree.Clear();
}

// end of YmlTreeTest.cpp
