/*
 * Copyright (c) 2020-2024 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "XmlFormatter.h"
#include "JsonFormatter.h"

#include "gtest/gtest.h"

using namespace std;

class XMLTreeDummy : public  XMLTree
{
public:
  XMLTreeDummy(IXmlItemBuilder* itemBuilder = NULL) : XMLTree(itemBuilder) {}
protected:
  XMLTreeParserInterface* CreateParserInterface() override { return nullptr; }
};

static const string xmlSchemaFile = "CPRJ.xsd";
static const string xmlSchemaVer = "0.0.9";

static const string xmlResExpected = string("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?>\n") +
"<cprj schemaVersion=\"" + xmlSchemaVer + "\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:noNamespaceSchemaLocation=\"" + xmlSchemaFile + "\">\n" +
"  <info>information</info>\n" +
"\n" +
"  <info attr1=\"someAttr\" attr2=\"anotherAttr\">information with attributes</info>\n" +
"\n" +
"  <descr>Some description</descr>\n" +
"\n" +
"  <child number=\"1\">\n" +
"    <childInfo info=\"100\"/>\n" +
"    <subchild subnumber=\"11\"/>\n" +
"    <subchild subnumber=\"12\"/>\n" +
"  </child>\n" +
"\n" +
"  <child number=\"2\">\n" +
"    <subchild subnumber=\"21\"/>\n" +
"    <subchild subnumber=\"22\"/>\n" +
"  </child>\n" +
"</cprj>\n";


static const string jsonResExpected = string("{\n") +
"  \"cprj\": \n" +
"  {\n" +
"    \"info\": [\n" +
"      \"information\",\n" +
"      {\n" +
"        \"-attr1\": \"someAttr\",\n" +
"        \"-attr2\": \"anotherAttr\",\n" +
"        \"#text\": \"information with attributes\"\n" +
"      }\n" +
"    ],\n" +
"      \"descr\": \"Some description\",\n" +
"    \"child\": [\n" +
"      {\n" +
"        \"-number\": \"1\",\n" +
"          \"childInfo\": \n" +
"          {\n" +
"            \"-info\": \"100\"\n" +
"          },\n" +
"        \"subchild\": [\n" +
"          {\n" +
"            \"-subnumber\": \"11\"\n" +
"          },\n" +
"          {\n" +
"            \"-subnumber\": \"12\"\n" +
"          }\n" +
"        ]\n" +
"      },\n" +
"      {\n" +
"        \"-number\": \"2\",\n" +
"        \"subchild\": [\n" +
"          {\n" +
"            \"-subnumber\": \"21\"\n" +
"          },\n" +
"          {\n" +
"            \"-subnumber\": \"22\"\n" +
"          }\n" +
"        ]\n" +
"      }\n" +
"    ]\n" +
"  }\n" +
"}\n";


TEST(XMLTree, CannotParseWithoutInterface) {
  auto tree = make_unique<XMLTreeDummy>();
  EXPECT_FALSE(tree->ParseString(xmlResExpected));
}


TEST(XMLFormatterTest, HeaderAndContentTest)
{
  auto tree = make_unique<XMLTreeDummy>();
  XMLTreeElement* cprj = tree->CreateElement("cprj");
  XMLTreeElement* info = cprj->CreateElement("info");
  info->SetText("information");
  info = cprj->CreateElement("info");
  info->SetText("information with attributes");
  info->AddAttribute("attr1", "someAttr");
  info->AddAttribute("attr2", "anotherAttr");

  XMLTreeElement* descr = cprj->CreateElement("descr");
  descr->SetText("Some description");

  for (int i = 1; i < 3; i++) {
    XMLTreeElement* child = cprj->CreateElement("child");
    child->AddAttribute("number", std::to_string(i));
    if (i == 1) {
      XMLTreeElement* childInfo = child->CreateElement("childInfo");
      childInfo->AddAttribute("info", std::to_string(i * 100));
    }
    for (int j = 1; j < 3; j++) {
      XMLTreeElement* sub = child->CreateElement("subchild");
      sub->AddAttribute("subnumber", std::to_string(i * 10 + j));
    }
  }

  JsonFormatter jsonFormatter;
  string jsonContent = jsonFormatter.Format(tree.get(), XmlItem::EMPTY_STRING, XmlItem::EMPTY_STRING);
  EXPECT_EQ(jsonResExpected, jsonContent);

  XmlFormatter xmlFormatter(tree.get(), xmlSchemaFile, xmlSchemaVer);
  string xmlContent = xmlFormatter.GetContent();
  EXPECT_EQ(xmlResExpected, xmlContent);
}
