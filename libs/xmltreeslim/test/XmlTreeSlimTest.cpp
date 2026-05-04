/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "gtest/gtest.h"
#include "XMLTreeSlim.h"
#include "XMLTreeSlimString.h"

#include "XmlFormatter.h"

#include "RteFsUtils.h"

#include <vector>
#include <list>
#include <string>

using namespace std;

// Directories
const string dirnameBase = "XmlTreeSlimTest";
const string dirnameFiles = dirnameBase + "/files";

// Filenames
const string xmlIn = dirnameFiles + "/in.xml";
const string xmlOut = dirnameFiles + "/out.xml";

// constants
static string schemaFile = "CPRJ.xsd";
static string schemaVer = "0.0.9";
static string theXmlString = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?>\n"
"<cprj schemaVersion=\"" + schemaVer + "\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:noNamespaceSchemaLocation=\"" + schemaFile + "\">\n" +
"  <info>information</info>\n" +
"\n" +
"  <child number=\"1\">\n" +
"    <subchild subnumber=\"11\">text11</subchild>\n" +
"    <subchild subnumber=\"12\">text12</subchild>\n" +
"    <subtext>subtext1</subtext>\n" +
"  </child>\n" +
"\n" +
"  <child number=\"2\">\n" +
"    <subchild subnumber=\"21\">text21</subchild>\n" +
"    <subchild subnumber=\"22\">text22</subchild>\n" +
"    <subtext>subtext2</subtext>\n" +
"  </child>\n" +
"\n" +
"  <special_chars amp=\"&amp;\" apos=\"&apos;\" gt=\"&gt;\" lt=\"&lt;\" quot=\"&quot;\"/>\n" +
"\n" +
"  <special_chars_in_text>amp=&amp; apos=&apos; gt=&gt; lt=&lt; quot=&quot;</special_chars_in_text>\n" +
"</cprj>\n";


class XmlTreeSlimTest :public ::testing::Test {
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

TEST_F(XmlTreeSlimTest, ReadFileDefault) {

  bool success = RteFsUtils::CopyBufferToFile(xmlIn, theXmlString, false);
  EXPECT_TRUE(success);

  XMLTreeSlim tree;
  success = tree.ParseFile(xmlIn);
  EXPECT_TRUE(success);
  EXPECT_TRUE(tree.GetRoot());
  XMLTreeElement* root = tree.GetRoot() ? tree.GetRoot()->GetFirstChild() : nullptr;
  EXPECT_TRUE(root);
  // currently XmlParser removes xmlns and other prefixes by default, tests it
  string xsi = root ?  root->GetAttribute("xsi") : XmlItem::EMPTY_STRING;
  string xsd = root ?  root->GetAttribute("noNamespaceSchemaLocation") : XmlItem::EMPTY_STRING;

   EXPECT_EQ(xsi,"http://www.w3.org/2001/XMLSchema-instance");
   EXPECT_EQ(xsd, schemaFile);

   if (root) {
     root->RemoveAttribute("xsi");
     root->RemoveAttribute("noNamespaceSchemaLocation");
   }

  XmlFormatter xmlFormatter(&tree, schemaFile, schemaVer);
  string xmlContent = xmlFormatter.GetContent();
  RteFsUtils::CopyBufferToFile(xmlOut, xmlContent, false);
  EXPECT_EQ(theXmlString, xmlContent);
}

TEST_F(XmlTreeSlimTest, ReadFileXmlns) {

  bool success = RteFsUtils::CopyBufferToFile(xmlIn, theXmlString, false);
  EXPECT_TRUE(success);

  XMLTreeSlim tree(nullptr, true, false); // default builder, redirect errors, do not ignore attribute prefixes
  success = tree.ParseFile(xmlIn);
  EXPECT_TRUE(success);
  EXPECT_TRUE(tree.GetRoot());
  XMLTreeElement* root = tree.GetRoot() ? tree.GetRoot()->GetFirstChild() : nullptr;
  EXPECT_TRUE(root);

  EXPECT_TRUE(root && !root->HasAttribute("xsi"));
  EXPECT_TRUE(root && !root->HasAttribute("noNamespaceSchemaLocation"));

  XmlFormatter xmlFormatter(&tree, schemaFile, schemaVer);
  string xmlContent = xmlFormatter.GetContent();
  RteFsUtils::CopyBufferToFile(xmlOut, xmlContent, false);
  EXPECT_EQ(theXmlString, xmlContent);
}


TEST_F(XmlTreeSlimTest, ReadFileStringXmlns) {

  XMLTreeSlim tree(nullptr, true, false); // default builder, redirect errors, do not ignore attribute prefixes
  bool success = tree.ParseString(theXmlString);
  EXPECT_TRUE(success);
  EXPECT_TRUE(tree.GetRoot());
  XMLTreeElement* root = tree.GetRoot() ? tree.GetRoot()->GetFirstChild() : nullptr;
  EXPECT_TRUE(root);

  EXPECT_TRUE(root && !root->HasAttribute("xsi"));
  EXPECT_TRUE(root && !root->HasAttribute("noNamespaceSchemaLocation"));

  XmlFormatter xmlFormatter(&tree, schemaFile, schemaVer);
  string xmlContent = xmlFormatter.GetContent();
  RteFsUtils::CopyBufferToFile(xmlOut, xmlContent, false);
  EXPECT_EQ(theXmlString, xmlContent);
}

TEST_F(XmlTreeSlimTest, ReadStringXmlns) {

  XMLTreeSlimString tree(nullptr, true, false); // default builder, redirect errors, do not ignore attribute prefixes
  bool success = tree.ParseString(theXmlString);
  EXPECT_TRUE(success);
  EXPECT_TRUE(tree.GetRoot());
  XMLTreeElement* root = tree.GetRoot() ? tree.GetRoot()->GetFirstChild() : nullptr;
  EXPECT_TRUE(root);

  EXPECT_TRUE(root && !root->HasAttribute("xsi"));
  EXPECT_TRUE(root && !root->HasAttribute("noNamespaceSchemaLocation"));

  XmlFormatter xmlFormatter(&tree, schemaFile, schemaVer);
  string xmlContent = xmlFormatter.GetContent();
  RteFsUtils::CopyBufferToFile(xmlOut, xmlContent, false);
  EXPECT_EQ(theXmlString, xmlContent);
}

TEST_F(XmlTreeSlimTest, ReadFileStringFail) {

  bool success = RteFsUtils::CopyBufferToFile(xmlIn, theXmlString, false);
  EXPECT_TRUE(success);

  XMLTreeSlimString tree(nullptr, true, false);
  success = tree.ParseFile(xmlIn);
  EXPECT_FALSE(success);
  string msg;
  auto& errors = tree.GetErrorStrings();
  if (errors.size() > 0) {
    msg = *(errors.begin());
  }
  EXPECT_EQ(msg, "XmlTreeSlimTest/files/in.xml: error M422 : Error reading file 'XmlTreeSlimTest/files/in.xml'");
}
