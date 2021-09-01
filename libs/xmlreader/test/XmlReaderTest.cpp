/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gtest/gtest.h"
#include "XML_Reader.h"

using namespace std;

static string utfInfo = "한€ह";
static string schemaFile = "CPRJ.xsd";
static string schemaVer = "0.0.9";
static string theXmlString = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?>\n"
"<cprj schemaVersion=\"" + schemaVer + "\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:noNamespaceSchemaLocation=\"" + schemaFile + "\">\n" +
"  <info>"+ utfInfo+"</info>\n" +
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


TEST(XmlReaderTest, Trim) {
  string expected = "Test String";

  map<string, string> input = {
    //input, expected
    { "  " + expected,        expected}, // leading whitespace
    { expected + "  ",        expected}, // trailing whitespace
    { "  " + expected + "  ", expected}  // leading + trailing whitespaces
  };

  std::for_each(input.begin(), input.end(),
    [](std::pair<std::string, string> element) {
      EXPECT_EQ(element.second.length(), XML_Reader::Trim(element.first));
      EXPECT_EQ(element.first, element.second);
  });
}

TEST(XmlReaderTest, ConvertSpecialChar)
{
  struct expected {
    bool result;
    char specialCh;
  };

  std::map<string, expected> input = {
    //input,  { result, specialCh}
    { "",      {false, '&'  }},
    { "amp",   {true,  '&'  }},
    { "lt",    {true,  '<'  }},
    { "gt",    {true,  '>'  }},
    { "apos",  {true,  '\'' }},
    { "quot",  {true,  '\"' }},
    { "#123",  {true,  '{'  }},
    { "#x123", {false, '&'  }},
    { "#x2b",  {true,  '+'  }},
    { "#xd",   {true,  '\r' }},
    { "#d",    {false, '&'  }},
  };

  std::for_each(input.begin(), input.end(),
    [&](std::pair<std::string, expected> element) {
      char found;
      EXPECT_EQ(element.second.result, XML_Reader::ConvertSpecialChar(element.first, found));
      EXPECT_EQ(found, element.second.specialCh);
  });
}

TEST(XmlReaderTest, CheckUTF)
{
  string utfStr;

  EXPECT_EQ(XmlTypes::UTFCode::UTF_NULL, XML_Reader::CheckUTF(""));

  utfStr += char(0xef); utfStr += char(0xbb); utfStr += char(0xbf);
  EXPECT_EQ(XmlTypes::UTFCode::UTF8, XML_Reader::CheckUTF(utfStr));

  utfStr.clear(); utfStr += char(0xfe); utfStr += char(0xff);
  EXPECT_EQ(XmlTypes::UTFCode::UTF16_BE, XML_Reader::CheckUTF(utfStr));

  utfStr.clear(); utfStr += char(0xff); utfStr += char(0xfe);
  EXPECT_EQ(XmlTypes::UTFCode::UTF16_LE, XML_Reader::CheckUTF(utfStr));
}

TEST(XmlReaderTest, ReadAttributes)
{
  XmlTypes::XmlNode_t node;
  XML_Reader reader(nullptr);
  reader.Init("", theXmlString);

  EXPECT_TRUE(reader.GetNextNode(node));
  EXPECT_TRUE(reader.HasAttributes());

  // Read attribute tags and data
  EXPECT_TRUE(reader.ReadNextAttribute(true));
  EXPECT_EQ("schemaVersion", reader.GetAttributeTag());
  EXPECT_EQ(schemaVer, reader.GetAttributeData());
  EXPECT_TRUE(reader.ReadNextAttribute(true));
  EXPECT_EQ("xsi", reader.GetAttributeTag());
  EXPECT_EQ("http://www.w3.org/2001/XMLSchema-instance", reader.GetAttributeData());
  EXPECT_TRUE(reader.ReadNextAttribute(false));
  EXPECT_EQ("xsi:noNamespaceSchemaLocation", reader.GetAttributeTag());
  EXPECT_EQ("CPRJ.xsd", reader.GetAttributeData());

  EXPECT_TRUE(reader.GetNextNode(node));
  EXPECT_TRUE(reader.GetNextNode(node));
  EXPECT_EQ(utfInfo, node.data);

  for (int indx = 0; indx < 10; ++indx) {
    EXPECT_TRUE(reader.GetNextNode(node));
  }

  EXPECT_EQ("subtext", node.tag);
  EXPECT_FALSE(reader.HasAttributes());
  EXPECT_FALSE(reader.ReadNextAttribute(true));
}
