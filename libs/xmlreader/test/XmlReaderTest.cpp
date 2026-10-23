/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gtest/gtest.h"
#include "XML_Reader.h"

#include <vector>

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

namespace {

class XmlReaderUTFTest : public testing::Test, public IErrConsumer {
protected:
  void SetUp() override {
    const auto log = ErrLog::Get();
    m_previousConsumer = log->SetErrConsumer(this);
    m_previousQuiet = log->IsQuietMode();
    m_previousErrors = log->GetErrCnt();
    m_previousWarnings = log->GetWarnCnt();
    log->SetQuietMode(false);
    log->ResetMsgCount();
  }

  void TearDown() override {
    const auto log = ErrLog::Get();
    log->SetErrConsumer(m_previousConsumer);
    log->SetQuietMode(m_previousQuiet);
    log->ResetMsgCount();
    for (int i = 0; i < m_previousErrors; ++i) {
      log->IncErrCnt();
    }
    for (int i = 0; i < m_previousWarnings; ++i) {
      log->IncWarnCnt();
    }
  }

  bool Consume(const PdscMsg& msg, const string&) override {
    m_messages.push_back(msg.GetMsgNum());
    if (msg.GetMsgNum() == "M412") {
      m_remainder = msg.GetSubstitute("STR");
    }
    return true;
  }

  vector<string> m_messages;
  string m_remainder;

private:
  XML_Reader m_reader{nullptr}; // Register XML diagnostic messages.
  IErrConsumer* m_previousConsumer = nullptr;
  bool m_previousQuiet = false;
  int m_previousErrors = 0;
  int m_previousWarnings = 0;
};

TEST_F(XmlReaderUTFTest, TruncatedPreamblesIgnoreRetainedStorage)
{
  const string prefixes[] = { "\xEF", "\xFE", "\xFF", "\xEF\xBB" };
  for (const auto& prefix : prefixes) {
    SCOPED_TRACE(testing::PrintToString(prefix));
    string text = prefix + "unused";
    text.resize(prefix.size()); // Retained bytes beyond the terminator are not input.
    m_messages.clear();

    EXPECT_EQ(XmlTypes::UTFCode::UTF_NULL, XML_Reader::CheckUTF(text));
    EXPECT_TRUE(m_messages.empty());
  }
}

TEST_F(XmlReaderUTFTest, PreservesPreambleAndRemainderDiagnostics)
{
  using UTFCode = XmlTypes::UTFCode;
  const struct {
    string text;
    UTFCode code;
    vector<string> messages;
    string remainder;
  } cases[] = {
    { "",                    UTFCode::UTF_NULL, {},                       "" },
    { "x",                   UTFCode::UTF_NULL, {},                       "" },
    { "\xEF\xBB\xBF",        UTFCode::UTF8,     {"M411"},                 "" },
    { "\xFE\xFF",            UTFCode::UTF16_BE, {"M411", "M413"},         "" },
    { "\xFF\xFE",            UTFCode::UTF16_LE, {"M411", "M413"},         "" },
    { "\xEF\xBB\xBF" "text", UTFCode::UTF8,     {"M411", "M412"},         "text" },
    { "\xFE\xFF" "text",     UTFCode::UTF16_BE, {"M411", "M413", "M412"}, "text" },
    { "\xFF\xFE" "text",     UTFCode::UTF16_LE, {"M411", "M413", "M412"}, "text" },
    { "\xEF!",               UTFCode::UTF_NULL, {},                       "" },
    { "\xEF\xBB!",           UTFCode::UTF_NULL, {},                       "" },
    { "\xFE!",               UTFCode::UTF_NULL, {},                       "" },
    { "\xFF!",               UTFCode::UTF_NULL, {},                       "" },
    { "\xEF!text",           UTFCode::UTF_NULL, {"M412"},                 "text" },
    { "\xEF\xBB!text",       UTFCode::UTF_NULL, {"M412"},                 "text" },
    { "\xFE!text",           UTFCode::UTF_NULL, {"M412"},                 "text" },
    { "\xFF!text",           UTFCode::UTF_NULL, {"M412"},                 "text" },
    { "xtext",               UTFCode::UTF_NULL, {"M412"},                 "text" },
  };
  for (const auto& test : cases) {
    SCOPED_TRACE(testing::PrintToString(test.text));
    m_messages.clear();
    m_remainder.clear();

    EXPECT_EQ(test.code, XML_Reader::CheckUTF(test.text));
    EXPECT_EQ(test.messages, m_messages);
    EXPECT_EQ(test.remainder, m_remainder);
  }
}

} // namespace

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
