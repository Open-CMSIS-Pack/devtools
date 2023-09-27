/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
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

class YmlTreeTest :public ::testing::Test {
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

TEST_F(YmlTreeTest, Sequence) {
  const string input =
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
  EXPECT_TRUE(tree.ParseXmlString(input));
  EXPECT_TRUE(tree.GetRoot());
  XMLTreeElement* root = tree.GetRoot() ? tree.GetRoot()->GetFirstChild() : nullptr;
  EXPECT_TRUE(root);

  XmlFormatter xmlFormatter(false);
  string xmlContent = xmlFormatter.FormatElement(root);
  EXPECT_EQ(xmlContent, expected_output);

  YmlFormatter ymlFormatter;
  string ymlContent = ymlFormatter.FormatElement(root);
  EXPECT_EQ(ymlContent, input);
}


TEST_F(YmlTreeTest, KeyVal) {
  const string input = "key: val";
  const string expected_output = XML_HEADER + "<key>val</key>\n";

  YmlTree tree;
  EXPECT_TRUE(tree.ParseXmlString("key: val"));
  EXPECT_TRUE(tree.GetRoot());
  XMLTreeElement* root = tree.GetRoot() ? tree.GetRoot()->GetFirstChild() : nullptr;
  EXPECT_TRUE(root);

  XmlFormatter xmlFormatter;
  string xmlContent = xmlFormatter.FormatElement(root);
  EXPECT_EQ(xmlContent, expected_output);

  YmlFormatter ymlFormatter;
  string ymlContent = ymlFormatter.FormatElement(root);
  EXPECT_EQ(ymlContent, input);
}

TEST_F(YmlTreeTest, Map) {
  const string yaml_input =
  "map:\n\
   one: 1\n\
   two: 2";

  const string json_input = "map: {one: 1, two: 2}\n";

  const string expected_output = XML_HEADER + "<map one=\"1\" two=\"2\"/>\n";

  YmlTree tree;
  EXPECT_TRUE(tree.ParseXmlString(yaml_input));
  EXPECT_TRUE(tree.GetRoot());
  XMLTreeElement* root = tree.GetRoot() ? tree.GetRoot()->GetFirstChild() : nullptr;
  EXPECT_TRUE(root);

  XmlFormatter xmlFormatter(false);
  string xmlContent = xmlFormatter.FormatElement(root);
  EXPECT_EQ(xmlContent, expected_output);
  tree.Clear();

  EXPECT_TRUE(tree.ParseXmlString(json_input));
  EXPECT_TRUE(tree.GetRoot());
  root = tree.GetRoot() ? tree.GetRoot()->GetFirstChild() : nullptr;
  EXPECT_TRUE(root);

  xmlContent = xmlFormatter.FormatElement(root);
  EXPECT_EQ(xmlContent, expected_output);

  YmlFormatter ymlFormatter;
  string ymlContent = ymlFormatter.FormatElement(root);
  EXPECT_EQ(ymlContent, yaml_input);
}

TEST_F(YmlTreeTest, Nested) {
  const string yaml_input =
  "nested:\n\
   one: 1\n\
   two:\n\
     s_one: 2.0\n\
     s_two: 2.1\n\
   three: 3\n\
   four:\n\
    - 4.1\n\
    - 4.2.a: a\n\
      4.2.b: b";

  const string expected_output = XML_HEADER +
"<nested one=\"1\" three=\"3\">\n\
  <two s_one=\"2.0\" s_two=\"2.1\"/>\n\
  <four>\n\
    <->4.1</->\n\
    <- 4.2.a=\"a\" 4.2.b=\"b\"/>\n\
  </four>\n\
</nested>\n";

  YmlTree tree;
  EXPECT_TRUE(tree.ParseXmlString(yaml_input));
  EXPECT_TRUE(tree.GetRoot());
  XMLTreeElement* root = tree.GetRoot() ? tree.GetRoot()->GetFirstChild() : nullptr;
  EXPECT_TRUE(root);

  XmlFormatter xmlFormatter(false);
  string xmlContent = xmlFormatter.FormatElement(root);
  EXPECT_EQ(xmlContent, expected_output);
}
TEST_F(YmlTreeTest, ReadFileDefault) {

  bool success = RteFsUtils::CopyBufferToFile(ymlIn, theYmlString, false);
  EXPECT_TRUE(success);

  YmlTree tree;
  success = tree.ParseFile(ymlIn);

  EXPECT_TRUE(success);
  EXPECT_TRUE(tree.GetRoot());
  XMLTreeElement* root = tree.GetRoot() ? tree.GetRoot()->GetFirstChild() : nullptr;
  EXPECT_TRUE(root);

  XmlFormatter xmlFormatter;
  string xmlContent = xmlFormatter.FormatElement(root);

  tree.Clear();

}

// end of YmlTreeTest.cpp
