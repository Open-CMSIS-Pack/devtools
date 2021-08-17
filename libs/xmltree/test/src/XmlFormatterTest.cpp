/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "XmlFormatter.h"
#include "gtest/gtest.h"

using namespace std;

class XMLTreeDummy : public  XMLTree
{
public:
  XMLTreeDummy(IXmlItemBuilder* itemBuilder = NULL) : XMLTree(itemBuilder) {}
};



TEST(XMLFormatterTest, HeaderEndContentTest) {
  string schemaFile = "CPRJ.xsd";
  string schemaVer = "0.0.9";
  string resExpected = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?>\n"
    "<cprj schemaVersion=\"" + schemaVer + "\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:noNamespaceSchemaLocation=\"" + schemaFile + "\">\n" +
    "  <info>information</info>\n" +
    "\n" +
    "  <child number=\"1\">\n" +
    "    <subchild subnumber=\"11\"/>\n" +
    "    <subchild subnumber=\"12\"/>\n" +
    "  </child>\n" +
    "\n" +
    "  <child number=\"2\">\n" +
    "    <subchild subnumber=\"21\"/>\n" +
    "    <subchild subnumber=\"22\"/>\n" +
    "  </child>\n" +
    "</cprj>\n";


  auto tree = make_unique<XMLTreeDummy>();
  XMLTreeElement* cprj = tree->CreateElement("cprj");
  XMLTreeElement* info = cprj->CreateElement("info");
  info->SetText("information");
  for (int i = 1; i < 3; i++) {
    XMLTreeElement* child = cprj->CreateElement("child");
    child->AddAttribute("number", std::to_string(i));
    for (int j = 1; j < 3; j++) {
      XMLTreeElement* sub = child->CreateElement("subchild");
      sub->AddAttribute("subnumber", std::to_string(i * 10 + j));
    }
  }

  XmlFormatter xmlFormatter(tree.get(), schemaFile, schemaVer);
  string xmlContent = xmlFormatter.GetContent();

  EXPECT_EQ(resExpected, xmlContent);
}
